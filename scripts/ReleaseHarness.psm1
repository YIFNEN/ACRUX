Set-StrictMode -Version Latest

function ConvertTo-NormalizedRelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return $Path.Replace('\', '/').TrimStart('/')
}

function Get-PackageInventory {
    param([Parameter(Mandatory = $true)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        throw "Package root does not exist: $Root"
    }

    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    $items = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $rootFull -File -Recurse | Sort-Object FullName)) {
        $relative = ConvertTo-NormalizedRelativePath $file.FullName.Substring($prefix.Length)
        $items += [pscustomobject]@{
            Path   = $relative
            Length = [int64]$file.Length
            Sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    return @($items)
}

function Test-ReleaseSize {
    param(
        [Parameter(Mandatory = $true)][int64]$Bytes,
        [Parameter(Mandatory = $true)]$Policy
    )

    return [pscustomobject]@{
        Bytes             = $Bytes
        InternalLimit     = [int64]$Policy.internalLimitBytes
        OfficialLimit     = [int64]$Policy.officialLimitBytes
        InternalPass      = $Bytes -le [int64]$Policy.internalLimitBytes
        OfficialPass      = $Bytes -le [int64]$Policy.officialLimitBytes
        InternalRemaining = [int64]$Policy.internalLimitBytes - $Bytes
        OfficialRemaining = [int64]$Policy.officialLimitBytes - $Bytes
    }
}

function Test-PackageWhitelist {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string[]]$AllowedPaths
    )

    $expected = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $AllowedPaths) {
        [void]$expected.Add((ConvertTo-NormalizedRelativePath $path))
    }

    $actual = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($item in @(Get-PackageInventory -Root $Root)) {
        [void]$actual.Add($item.Path)
    }

    $missing = @($expected | Where-Object { -not $actual.Contains($_) } | Sort-Object)
    $extra = @($actual | Where-Object { -not $expected.Contains($_) } | Sort-Object)
    return [pscustomobject]@{
        Pass    = $missing.Count -eq 0 -and $extra.Count -eq 0
        Missing = $missing
        Extra   = $extra
    }
}

function Compare-PackageTrees {
    param(
        [Parameter(Mandatory = $true)][string]$ExpectedRoot,
        [Parameter(Mandatory = $true)][string]$ActualRoot
    )

    $expected = @{}
    foreach ($item in @(Get-PackageInventory -Root $ExpectedRoot)) { $expected[$item.Path.ToLowerInvariant()] = $item }
    $actual = @{}
    foreach ($item in @(Get-PackageInventory -Root $ActualRoot)) { $actual[$item.Path.ToLowerInvariant()] = $item }

    $missing = @($expected.Keys | Where-Object { -not $actual.ContainsKey($_) } | ForEach-Object { $expected[$_].Path } | Sort-Object)
    $extra = @($actual.Keys | Where-Object { -not $expected.ContainsKey($_) } | ForEach-Object { $actual[$_].Path } | Sort-Object)
    $changed = @($expected.Keys | Where-Object {
        $actual.ContainsKey($_) -and (
            $expected[$_].Length -ne $actual[$_].Length -or
            $expected[$_].Sha256 -ne $actual[$_].Sha256
        )
    } | ForEach-Object { $expected[$_].Path } | Sort-Object)

    return [pscustomobject]@{
        Pass    = $missing.Count -eq 0 -and $extra.Count -eq 0 -and $changed.Count -eq 0
        Missing = $missing
        Extra   = $extra
        Changed = $changed
    }
}

function Test-PeImportText {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [Parameter(Mandatory = $true)]$Policy
    )

    $imports = @([regex]::Matches($Text, '(?im)^\s*([A-Za-z0-9._-]+\.dll)\s*$') | ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } | Sort-Object -Unique)
    $allowed = @($Policy.allowedImports | ForEach-Object { ([string]$_).ToUpperInvariant() })
    $forbiddenNames = @($Policy.forbiddenNetworkImports | ForEach-Object { ([string]$_).ToUpperInvariant() })
    $forbidden = @($imports | Where-Object { $forbiddenNames -contains $_ } | Sort-Object -Unique)
    $unknown = @($imports | Where-Object { $allowed -notcontains $_ -and $forbiddenNames -notcontains $_ } | Sort-Object -Unique)

    return [pscustomobject]@{
        Pass      = $imports.Count -gt 0 -and $forbidden.Count -eq 0 -and $unknown.Count -eq 0
        Imports   = $imports
        Forbidden = $forbidden
        Unknown   = $unknown
    }
}

function Test-AssetLedger {
    param(
        [Parameter(Mandatory = $true)][string]$AssetRoot,
        [Parameter(Mandatory = $true)][string]$LedgerPath
    )

    if (-not (Test-Path -LiteralPath $LedgerPath -PathType Leaf)) {
        return [pscustomobject]@{ Pass = $false; Missing = @(); Invalid = @('ledger missing') }
    }

    $rows = @(Import-Csv -LiteralPath $LedgerPath)
    $byPath = @{}
    $invalid = @()
    foreach ($row in $rows) {
        $required = @('path', 'kind', 'creator', 'source', 'license', 'modified', 'embedded_in')
        $rowInvalid = $false
        foreach ($field in $required) {
            if (-not $row.PSObject.Properties[$field] -or [string]::IsNullOrWhiteSpace([string]$row.$field)) {
                $rowInvalid = $true
            }
        }
        if ($rowInvalid) {
            $invalid += [string]$row.path
            continue
        }
        $key = (ConvertTo-NormalizedRelativePath ([string]$row.path)).ToLowerInvariant()
        if ($byPath.ContainsKey($key)) {
            $invalid += [string]$row.path
            continue
        }
        $byPath[$key] = $row
    }

    $missing = @()
    if (Test-Path -LiteralPath $AssetRoot -PathType Container) {
        $rootFull = [IO.Path]::GetFullPath($AssetRoot).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
        $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
        foreach ($file in @(Get-ChildItem -LiteralPath $rootFull -File -Recurse)) {
            $relative = ConvertTo-NormalizedRelativePath $file.FullName.Substring($prefix.Length)
            if (-not $byPath.ContainsKey($relative.ToLowerInvariant())) { $missing += $relative }
        }
    }

    return [pscustomobject]@{
        Pass    = $missing.Count -eq 0 -and $invalid.Count -eq 0
        Missing = @($missing | Sort-Object)
        Invalid = @($invalid | Sort-Object)
    }
}

function Test-DependencyLedger {
    param(
        [Parameter(Mandatory = $true)][string]$LedgerPath,
        [string[]]$PackagePaths = @()
    )

    if (-not (Test-Path -LiteralPath $LedgerPath -PathType Leaf)) {
        return [pscustomobject]@{ Pass = $false; Invalid = @('ledger missing'); MissingNotice = @() }
    }
    $rows = @(Import-Csv -LiteralPath $LedgerPath)
    $invalid = @()
    $missingNotice = @()
    $normalizedPackagePaths = @($PackagePaths | ForEach-Object { (ConvertTo-NormalizedRelativePath $_).ToLowerInvariant() })
    foreach ($row in $rows) {
        $required = @('kind', 'name', 'version', 'source', 'license', 'redistributed', 'notice_required')
        $rowInvalid = $false
        foreach ($field in $required) {
            if (-not $row.PSObject.Properties[$field] -or [string]::IsNullOrWhiteSpace([string]$row.$field)) {
                $rowInvalid = $true
            }
        }
        if ($rowInvalid) {
            $invalid += [string]$row.name
            continue
        }
        $redistributed = ([string]$row.redistributed).Trim().ToLowerInvariant()
        $noticeRequired = ([string]$row.notice_required).Trim().ToLowerInvariant()
        if (@('true', 'false') -notcontains $redistributed -or @('true', 'false') -notcontains $noticeRequired) {
            $invalid += [string]$row.name
            continue
        }
        if ($redistributed -eq 'true' -and $noticeRequired -eq 'true' -and $normalizedPackagePaths -notcontains 'notice.txt') {
            $missingNotice += [string]$row.name
        }
    }
    return [pscustomobject]@{
        Pass          = $invalid.Count -eq 0 -and $missingNotice.Count -eq 0
        Invalid       = @($invalid | Sort-Object)
        MissingNotice = @($missingNotice | Sort-Object)
    }
}

function Assert-NewReleaseCandidatePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (Test-Path -LiteralPath $Path) {
        throw "Release candidate path already exists: $Path"
    }
}

function New-ReleaseArchive {
    param(
        [Parameter(Mandatory = $true)][string]$StagingRoot,
        [Parameter(Mandatory = $true)][string]$CandidateRoot,
        [Parameter(Mandatory = $true)]$Policy,
        [Parameter(Mandatory = $true)][string]$CandidateName
    )

    if ([string]::IsNullOrWhiteSpace($CandidateName) -or $CandidateName.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) {
        throw "Invalid release candidate name: $CandidateName"
    }
    if ($CandidateName.Contains('/') -or $CandidateName.Contains('\')) {
        throw "Release candidate name must not contain a path: $CandidateName"
    }

    $stagingFull = [IO.Path]::GetFullPath($StagingRoot)
    $candidateRootFull = [IO.Path]::GetFullPath($CandidateRoot)
    if (-not (Test-Path -LiteralPath $candidateRootFull -PathType Container)) {
        New-Item -ItemType Directory -Path $candidateRootFull | Out-Null
    }
    $candidatePath = Join-Path $candidateRootFull $CandidateName
    Assert-NewReleaseCandidatePath -Path $candidatePath

    $allowedPaths = @($Policy.packageFiles | ForEach-Object { [string]$_.destination })
    $whitelist = Test-PackageWhitelist -Root $stagingFull -AllowedPaths $allowedPaths
    if (-not $whitelist.Pass) {
        throw "Package whitelist failed. Missing: $($whitelist.Missing -join ', '); extra: $($whitelist.Extra -join ', ')"
    }

    $inventory = @(Get-PackageInventory -Root $stagingFull)
    $totalBytes = [int64](($inventory | Measure-Object -Property Length -Sum).Sum)
    $size = Test-ReleaseSize -Bytes $totalBytes -Policy $Policy
    if (-not $size.InternalPass) {
        throw "Package exceeds the internal byte limit by $(-$size.InternalRemaining) bytes."
    }
    if (-not $size.OfficialPass) {
        throw "Package exceeds the official byte limit by $(-$size.OfficialRemaining) bytes."
    }

    $tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("acrux-rc-" + [guid]::NewGuid().ToString('N'))
    $zipPath = Join-Path $tempRoot 'ACRUX.zip'
    $unpackedPath = Join-Path $tempRoot 'unpacked'
    New-Item -ItemType Directory -Path $tempRoot, $unpackedPath | Out-Null

    try {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [IO.Compression.ZipFile]::CreateFromDirectory(
            $stagingFull,
            $zipPath,
            [IO.Compression.CompressionLevel]::Optimal,
            $false
        )
        [IO.Compression.ZipFile]::ExtractToDirectory($zipPath, $unpackedPath)

        $roundTrip = Compare-PackageTrees -ExpectedRoot $stagingFull -ActualRoot $unpackedPath
        if (-not $roundTrip.Pass) {
            throw "ZIP round-trip mismatch. Missing: $($roundTrip.Missing -join ', '); extra: $($roundTrip.Extra -join ', '); changed: $($roundTrip.Changed -join ', ')"
        }

        $zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $hashLines = @("$zipHash  ACRUX.zip")
        foreach ($item in $inventory) {
            $hashLines += "$($item.Sha256)  unpacked/$($item.Path)"
        }
        $hashFile = Join-Path $tempRoot 'SHA256SUMS.txt'
        [IO.File]::WriteAllLines($hashFile, $hashLines, (New-Object Text.UTF8Encoding($false)))

        $report = [ordered]@{
            schemaVersion      = 1
            verifiedAtUtc      = [DateTime]::UtcNow.ToString('o')
            candidateName      = $CandidateName
            totalUnpackedBytes = $totalBytes
            internalLimitBytes = [int64]$Policy.internalLimitBytes
            officialLimitBytes = [int64]$Policy.officialLimitBytes
            internalRemaining  = [int64]$size.InternalRemaining
            officialRemaining  = [int64]$size.OfficialRemaining
            zipSha256          = $zipHash
            files              = $inventory
        }
        $reportPath = Join-Path $tempRoot 'verification.json'
        [IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 6), (New-Object Text.UTF8Encoding($false)))

        New-Item -ItemType Directory -Path $candidatePath | Out-Null
        Copy-Item -LiteralPath $zipPath -Destination (Join-Path $candidatePath 'ACRUX.zip')
        Copy-Item -LiteralPath $unpackedPath -Destination (Join-Path $candidatePath 'unpacked') -Recurse
        Copy-Item -LiteralPath $hashFile -Destination (Join-Path $candidatePath 'SHA256SUMS.txt')
        Copy-Item -LiteralPath $reportPath -Destination (Join-Path $candidatePath 'verification.json')

        return [pscustomobject]@{
            CandidatePath = $candidatePath
            ZipPath       = Join-Path $candidatePath 'ACRUX.zip'
            TotalBytes    = $totalBytes
            ZipSha256     = $zipHash
        }
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
            $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
            if (-not $resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove an RC temp path outside the system temp directory: $resolvedTemp"
            }
            Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
        }
    }
}

function Get-IterationProfile {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [ValidateSet('Logic', 'Playable', 'Size', 'Candidate')]
        [string]$Profile,

        [switch]$CreateCandidate
    )

    if ($Profile -eq 'Candidate' -and -not $CreateCandidate) {
        throw "Candidate verification requires the explicit -CreateCandidate switch."
    }
    if ($Profile -ne 'Candidate' -and $CreateCandidate) {
        throw "-CreateCandidate is valid only with the Candidate profile."
    }

    $actions = switch ($Profile) {
        'Logic'     { @('CoreDebug') }
        'Playable'  { @('GameDebug') }
        'Size'      { @('GameRelease', 'MeasureSize') }
        'Candidate' { @('HarnessTests', 'CoreRelease', 'GameRelease', 'CreateCandidate') }
    }

    return [pscustomobject]@{
        Profile          = $Profile
        Actions          = [string[]]$actions
        CreatesCandidate = ($Profile -eq 'Candidate')
    }
}

Export-ModuleMember -Function @(
    'Get-PackageInventory',
    'Test-ReleaseSize',
    'Test-PackageWhitelist',
    'Compare-PackageTrees',
    'Test-PeImportText',
    'Test-AssetLedger',
    'Test-DependencyLedger',
    'Assert-NewReleaseCandidatePath',
    'New-ReleaseArchive',
    'Get-IterationProfile'
)
