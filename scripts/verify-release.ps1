param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)),
    [string]$PolicyPath,
    [string]$DumpbinPath,
    [string]$CandidateName
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Import-Module (Join-Path $scriptRoot 'ReleaseHarness.psm1') -Force

function Resolve-RepositoryPath {
    param([string]$Root, [string]$Path, [switch]$AllowMissing)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    if ([IO.Path]::IsPathRooted($Path)) { $full = [IO.Path]::GetFullPath($Path) }
    else { $full = [IO.Path]::GetFullPath((Join-Path $rootFull $Path)) }
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    if ($full -ne $rootFull -and -not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes the repository root: $Path"
    }
    if (-not $AllowMissing -and -not (Test-Path -LiteralPath $full)) {
        throw "Required path does not exist: $full"
    }
    return $full
}

function Find-Dumpbin {
    param([string]$RequestedPath)
    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        if (-not (Test-Path -LiteralPath $RequestedPath -PathType Leaf)) { throw "dumpbin tool does not exist: $RequestedPath" }
        return [IO.Path]::GetFullPath($RequestedPath)
    }
    $command = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw 'dumpbin.exe was not found. Run from an MSVC developer shell or pass -DumpbinPath.'
}

$repoFull = [IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $repoFull -PathType Container)) { throw "Repository root does not exist: $repoFull" }
if ([string]::IsNullOrWhiteSpace($PolicyPath)) { $PolicyPath = Join-Path $scriptRoot 'release-policy.json' }
if (-not (Test-Path -LiteralPath $PolicyPath -PathType Leaf)) { throw "Release policy does not exist: $PolicyPath" }
$policy = Get-Content -Raw -LiteralPath $PolicyPath | ConvertFrom-Json

$requiredPolicyFields = @(
    'schemaVersion', 'internalLimitBytes', 'officialLimitBytes', 'packageFiles',
    'allowedImports', 'forbiddenNetworkImports', 'assetRoot', 'assetLedger',
    'dependencyLedger', 'candidateRoot'
)
foreach ($field in $requiredPolicyFields) {
    if (-not $policy.PSObject.Properties[$field]) { throw "Release policy is missing '$field'." }
}
if ([int]$policy.schemaVersion -ne 1) { throw "Unsupported release policy schema: $($policy.schemaVersion)" }
if ([int64]$policy.internalLimitBytes -gt [int64]$policy.officialLimitBytes) {
    throw 'The internal byte limit must not exceed the official limit.'
}
$packageFiles = @($policy.packageFiles)
if ($packageFiles.Count -eq 0) { throw 'Release policy must list at least one package file.' }

$assetRoot = Resolve-RepositoryPath -Root $repoFull -Path ([string]$policy.assetRoot) -AllowMissing
$assetLedger = Resolve-RepositoryPath -Root $repoFull -Path ([string]$policy.assetLedger)
$dependencyLedger = Resolve-RepositoryPath -Root $repoFull -Path ([string]$policy.dependencyLedger)
$assetAudit = Test-AssetLedger -AssetRoot $assetRoot -LedgerPath $assetLedger
if (-not $assetAudit.Pass) {
    throw "Asset ledger audit failed. Missing: $($assetAudit.Missing -join ', '); invalid: $($assetAudit.Invalid -join ', ')"
}
$packageDestinations = @($packageFiles | ForEach-Object { [string]$_.destination })
$dependencyAudit = Test-DependencyLedger -LedgerPath $dependencyLedger -PackagePaths $packageDestinations
if (-not $dependencyAudit.Pass) {
    throw "Dependency ledger audit failed. Invalid: $($dependencyAudit.Invalid -join ', '); missing notices: $($dependencyAudit.MissingNotice -join ', ')"
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("acrux-staging-" + [guid]::NewGuid().ToString('N'))
$stagingRoot = Join-Path $tempRoot 'package'
New-Item -ItemType Directory -Path $stagingRoot | Out-Null

try {
    foreach ($entry in $packageFiles) {
        if (-not $entry.PSObject.Properties['source'] -or -not $entry.PSObject.Properties['destination']) {
            throw 'Every package file needs source and destination fields.'
        }
        $source = Resolve-RepositoryPath -Root $repoFull -Path ([string]$entry.source)
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Package source must be a file: $source" }

        $destinationText = ([string]$entry.destination).Replace('/', [IO.Path]::DirectorySeparatorChar)
        if ([IO.Path]::IsPathRooted($destinationText)) { throw "Package destination must be relative: $destinationText" }
        $destination = [IO.Path]::GetFullPath((Join-Path $stagingRoot $destinationText))
        $stagingPrefix = [IO.Path]::GetFullPath($stagingRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        if (-not $destination.StartsWith($stagingPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Package destination escapes staging: $destinationText"
        }
        $destinationParent = Split-Path -Parent $destination
        if (-not (Test-Path -LiteralPath $destinationParent -PathType Container)) {
            New-Item -ItemType Directory -Path $destinationParent | Out-Null
        }
        Copy-Item -LiteralPath $source -Destination $destination
    }

    $exeEntries = @($packageFiles | Where-Object { ([string]$_.destination).EndsWith('.exe', [StringComparison]::OrdinalIgnoreCase) })
    if ($exeEntries.Count -ne 1) { throw 'The release policy must contain exactly one executable.' }
    $stagedExe = Join-Path $stagingRoot (([string]$exeEntries[0].destination).Replace('/', [IO.Path]::DirectorySeparatorChar))

    $dumpbin = Find-Dumpbin -RequestedPath $DumpbinPath
    $importOutput = @(& $dumpbin /IMPORTS $stagedExe 2>&1)
    if ($LASTEXITCODE -ne 0) { throw "dumpbin failed with exit code $LASTEXITCODE.`n$($importOutput -join [Environment]::NewLine)" }
    $importAudit = Test-PeImportText -Text ($importOutput -join [Environment]::NewLine) -Policy $policy
    if (-not $importAudit.Pass) {
        throw "PE import audit failed. Forbidden: $($importAudit.Forbidden -join ', '); unknown: $($importAudit.Unknown -join ', ')"
    }

    if ([string]::IsNullOrWhiteSpace($CandidateName)) {
        $exeHash = (Get-FileHash -LiteralPath $stagedExe -Algorithm SHA256).Hash.ToLowerInvariant()
        $CandidateName = 'RC-{0}-{1}' -f ([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')), $exeHash.Substring(0, 8)
    }
    $candidateRoot = Resolve-RepositoryPath -Root $repoFull -Path ([string]$policy.candidateRoot) -AllowMissing
    $result = New-ReleaseArchive -StagingRoot $stagingRoot -CandidateRoot $candidateRoot -Policy $policy -CandidateName $CandidateName

    $reportPath = Join-Path $result.CandidatePath 'verification.json'
    $report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
    $report | Add-Member -NotePropertyName peImports -NotePropertyValue @($importAudit.Imports)
    $report | Add-Member -NotePropertyName assetLedgerAudit -NotePropertyValue 'pass'
    $report | Add-Member -NotePropertyName dependencyLedgerAudit -NotePropertyValue 'pass'
    [IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 7), (New-Object Text.UTF8Encoding($false)))

    Write-Host "PASS release candidate: $($result.CandidatePath)"
    Write-Host "Unpacked bytes: $($result.TotalBytes) / $($policy.internalLimitBytes) internal / $($policy.officialLimitBytes) official"
    Write-Host "ZIP SHA-256: $($result.ZipSha256)"
    return $result
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if (-not $resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a staging path outside the system temp directory: $resolvedTemp"
        }
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    }
}
