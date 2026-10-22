$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$modulePath = Join-Path $scriptRoot 'ReleaseHarness.psm1'
Import-Module $modulePath -Force

$script:Passed = 0
$script:Failed = 0

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    if ($Expected -ne $Actual) {
        throw "$Message (expected: $Expected, actual: $Actual)"
    }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Pattern, [string]$Message)
    try {
        & $Action
    }
    catch {
        if ($_.Exception.Message -match $Pattern) { return }
        throw "$Message (unexpected error: $($_.Exception.Message))"
    }
    throw "$Message (no error was thrown)"
}

function Invoke-Test {
    param([string]$Name, [scriptblock]$Body)
    try {
        & $Body
        $script:Passed++
        Write-Host "PASS $Name"
    }
    catch {
        $script:Failed++
        Write-Host "FAIL $Name :: $($_.Exception.Message)" -ForegroundColor Red
        if ($_.ScriptStackTrace) { Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray }
    }
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("acrux-harness-tests-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$unicodeSegment = -join ([char[]]@(0xC6D0, 0xBCF8))

try {
    Invoke-Test 'iteration profiles select only their intended verification work' {
        Assert-Equal 'CoreDebug' ((Get-IterationProfile -Profile Logic).Actions -join ',') 'Logic must run only Debug core tests'
        Assert-Equal 'GameDebug' ((Get-IterationProfile -Profile Playable).Actions -join ',') 'Playable must run only the Debug game build'
        Assert-Equal 'GameRelease,MeasureSize' ((Get-IterationProfile -Profile Size).Actions -join ',') 'Size must build once and measure bytes'
        Assert-Throws { Get-IterationProfile -Profile Candidate } 'CreateCandidate' 'Candidate creation must require explicit intent'
        $candidate = Get-IterationProfile -Profile Candidate -CreateCandidate
        Assert-Equal 'HarnessTests,CoreRelease,GameRelease,CreateCandidate' ($candidate.Actions -join ',') 'Candidate must run the complete release path'
        Assert-True $candidate.CreatesCandidate 'Candidate must report that it creates an immutable RC'
        Assert-Throws { Get-IterationProfile -Profile Logic -CreateCandidate } 'only with the Candidate' 'Other profiles must reject candidate creation'
        Assert-Throws { Get-IterationProfile -Profile Unknown } 'ValidateSet|valid values' 'Unknown profile names must be rejected'
    }

    Invoke-Test 'size boundary reports internal and official limits independently' {
        $policy = [pscustomobject]@{ internalLimitBytes = 1350000; officialLimitBytes = 1474560 }
        $atInternal = Test-ReleaseSize -Bytes 1350000 -Policy $policy
        Assert-True $atInternal.InternalPass 'The exact internal limit must pass'
        Assert-True $atInternal.OfficialPass 'The exact internal limit must also pass the official limit'

        $overInternal = Test-ReleaseSize -Bytes 1350001 -Policy $policy
        Assert-True (-not $overInternal.InternalPass) 'One byte over the internal limit must fail internally'
        Assert-True $overInternal.OfficialPass 'One byte over the internal limit is still below the official limit'

        $overOfficial = Test-ReleaseSize -Bytes 1474561 -Policy $policy
        Assert-True (-not $overOfficial.InternalPass) 'An official overflow must also fail internally'
        Assert-True (-not $overOfficial.OfficialPass) 'One byte over the official limit must fail officially'
    }

    Invoke-Test 'package whitelist rejects missing and extra files' {
        $package = Join-Path $tempRoot 'whitelist package'
        New-Item -ItemType Directory -Path $package | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $package 'ACRUX.exe'), [byte[]](1, 2, 3))
        $allowed = @('ACRUX.exe')
        $valid = Test-PackageWhitelist -Root $package -AllowedPaths $allowed
        Assert-True $valid.Pass 'The exact whitelist must pass'

        [IO.File]::WriteAllText((Join-Path $package 'debug.pdb'), 'debug')
        $extra = Test-PackageWhitelist -Root $package -AllowedPaths $allowed
        Assert-True (-not $extra.Pass) 'An unlisted file must fail'
        Assert-True ($extra.Extra -contains 'debug.pdb') 'The extra file must be reported'

        Remove-Item -LiteralPath (Join-Path $package 'debug.pdb')
        Remove-Item -LiteralPath (Join-Path $package 'ACRUX.exe')
        $missing = Test-PackageWhitelist -Root $package -AllowedPaths $allowed
        Assert-True (-not $missing.Pass) 'A required file must fail when absent'
        Assert-True ($missing.Missing -contains 'ACRUX.exe') 'The missing file must be reported'
    }

    Invoke-Test 'directory comparison detects round-trip tampering' {
        $left = Join-Path $tempRoot "$unicodeSegment package"
        $right = Join-Path $tempRoot 'round trip'
        New-Item -ItemType Directory -Path $left, $right | Out-Null
        [IO.File]::WriteAllText((Join-Path $left 'ACRUX.exe'), 'same bytes')
        [IO.File]::WriteAllText((Join-Path $right 'ACRUX.exe'), 'same bytes')
        Assert-True (Compare-PackageTrees -ExpectedRoot $left -ActualRoot $right).Pass 'Identical trees must pass'

        [IO.File]::WriteAllText((Join-Path $right 'ACRUX.exe'), 'changed bytes')
        $changed = Compare-PackageTrees -ExpectedRoot $left -ActualRoot $right
        Assert-True (-not $changed.Pass) 'Changed file content must fail'
        Assert-True ($changed.Changed -contains 'ACRUX.exe') 'The changed file must be reported'
    }

    Invoke-Test 'PE import audit rejects network and unknown DLLs' {
        $policy = [pscustomobject]@{
            allowedImports = @('KERNEL32.dll', 'USER32.dll', 'GDI32.dll', 'WINMM.dll')
            forbiddenNetworkImports = @('WINHTTP.dll', 'WININET.dll', 'WS2_32.dll', 'URLMON.dll')
        }
        $safe = Test-PeImportText -Text "    KERNEL32.dll`r`n    USER32.dll`r`n" -Policy $policy
        Assert-True $safe.Pass 'Expected Windows imports must pass'

        $network = Test-PeImportText -Text "    KERNEL32.dll`r`n    WINHTTP.dll`r`n" -Policy $policy
        Assert-True (-not $network.Pass) 'A network import must fail'
        Assert-True ($network.Forbidden -contains 'WINHTTP.dll') 'The forbidden DLL must be reported'

        $runtime = Test-PeImportText -Text "    KERNEL32.dll`r`n    VCRUNTIME140.dll`r`n" -Policy $policy
        Assert-True (-not $runtime.Pass) 'An unapproved dynamic runtime must fail'
        Assert-True ($runtime.Unknown -contains 'VCRUNTIME140.dll') 'The unknown DLL must be reported'
    }

    Invoke-Test 'asset ledger fails closed for missing entries and licenses' {
        $assetRoot = Join-Path $tempRoot 'assets source'
        $ledger = Join-Path $tempRoot 'asset-ledger.csv'
        New-Item -ItemType Directory -Path $assetRoot | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $assetRoot 'player.png'), [byte[]](7, 8, 9))
        'path,kind,creator,source,license,modified,embedded_in' | Set-Content -LiteralPath $ledger -Encoding utf8
        $missing = Test-AssetLedger -AssetRoot $assetRoot -LedgerPath $ledger
        Assert-True (-not $missing.Pass) 'An asset without a ledger row must fail'

        @(
            'path,kind,creator,source,license,modified,embedded_in',
            'player.png,original,ACRUX,project,,no,atlas.bin'
        ) | Set-Content -LiteralPath $ledger -Encoding utf8
        $blank = Test-AssetLedger -AssetRoot $assetRoot -LedgerPath $ledger
        Assert-True (-not $blank.Pass) 'A blank license must fail'

        @(
            'path,kind,creator,source,license,modified,embedded_in',
            'player.png,original,ACRUX,project,Project original,no,atlas.bin'
        ) | Set-Content -LiteralPath $ledger -Encoding utf8
        Assert-True (Test-AssetLedger -AssetRoot $assetRoot -LedgerPath $ledger).Pass 'A complete row must pass'
    }

    Invoke-Test 'dependency ledger rejects blank licenses' {
        $ledger = Join-Path $tempRoot 'dependency-ledger.csv'
        @(
            'kind,name,version,source,license,redistributed,notice_required',
            'library,example,1.0,local,,false,false'
        ) | Set-Content -LiteralPath $ledger -Encoding utf8
        Assert-True (-not (Test-DependencyLedger -LedgerPath $ledger).Pass) 'A blank dependency license must fail'
    }

    Invoke-Test 'dependency ledger requires a packaged notice when redistribution terms require it' {
        $ledger = Join-Path $tempRoot 'notice-dependency-ledger.csv'
        @(
            'kind,name,version,source,license,redistributed,notice_required',
            'library,third-party,1.0,https://example.invalid,Example License,true,true'
        ) | Set-Content -LiteralPath $ledger -Encoding utf8
        $missingNotice = Test-DependencyLedger -LedgerPath $ledger -PackagePaths @('ACRUX.exe')
        Assert-True (-not $missingNotice.Pass) 'A required notice missing from the package must fail'
        Assert-True ($missingNotice.MissingNotice -contains 'third-party') 'The affected dependency must be reported'

        $withNotice = Test-DependencyLedger -LedgerPath $ledger -PackagePaths @('ACRUX.exe', 'NOTICE.txt')
        Assert-True $withNotice.Pass 'An explicit packaged notice must satisfy the ledger requirement'
    }

    Invoke-Test 'release candidate path cannot be overwritten' {
        $candidate = Join-Path $tempRoot 'RC-existing'
        New-Item -ItemType Directory -Path $candidate | Out-Null
        Assert-Throws { Assert-NewReleaseCandidatePath -Path $candidate } 'already exists' 'An existing RC path must be rejected'
    }

    Invoke-Test 'release archive survives ZIP round-trip and records hashes' {
        $staging = Join-Path $tempRoot "$unicodeSegment staging"
        $candidateRoot = Join-Path $tempRoot 'release candidates'
        New-Item -ItemType Directory -Path $staging, $candidateRoot | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $staging 'ACRUX.exe'), [byte[]](10, 20, 30, 40, 50))
        $policy = [pscustomobject]@{
            internalLimitBytes = 1350000
            officialLimitBytes = 1474560
            packageFiles = @([pscustomobject]@{ destination = 'ACRUX.exe' })
        }

        $result = New-ReleaseArchive -StagingRoot $staging -CandidateRoot $candidateRoot -Policy $policy -CandidateName 'RC-test'
        Assert-True (Test-Path -LiteralPath $result.CandidatePath -PathType Container) 'The RC directory must be preserved'
        Assert-True (Test-Path -LiteralPath $result.ZipPath -PathType Leaf) 'The submission ZIP must be preserved'
        Assert-True (Test-Path -LiteralPath (Join-Path $result.CandidatePath 'SHA256SUMS.txt') -PathType Leaf) 'SHA256SUMS must be written'
        Assert-True (Test-Path -LiteralPath (Join-Path $result.CandidatePath 'verification.json') -PathType Leaf) 'The JSON report must be written'
        Assert-True (Compare-PackageTrees -ExpectedRoot $staging -ActualRoot (Join-Path $result.CandidatePath 'unpacked')).Pass 'The preserved unpacked tree must match staging'
        Assert-Equal 5 $result.TotalBytes 'The report must use unpacked file bytes'
    }

    Invoke-Test 'release archive rejects an internal overflow without preserving an RC' {
        $staging = Join-Path $tempRoot 'oversized staging'
        $candidateRoot = Join-Path $tempRoot 'rejected candidates'
        New-Item -ItemType Directory -Path $staging, $candidateRoot | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $staging 'ACRUX.exe'), [byte[]](1, 2))
        $policy = [pscustomobject]@{
            internalLimitBytes = 1
            officialLimitBytes = 2
            packageFiles = @([pscustomobject]@{ destination = 'ACRUX.exe' })
        }
        Assert-Throws {
            New-ReleaseArchive -StagingRoot $staging -CandidateRoot $candidateRoot -Policy $policy -CandidateName 'RC-must-not-exist'
        } 'internal byte limit' 'An internal overflow must stop RC creation'
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $candidateRoot 'RC-must-not-exist'))) 'A rejected RC must not leave a candidate directory'
    }

    Invoke-Test 'verification command stages explicit files and creates a validated RC' {
        $repo = Join-Path $tempRoot "$unicodeSegment repository"
        $exeDir = Join-Path $repo 'build/release'
        $assetRoot = Join-Path $repo 'assets/source'
        $assetDocs = Join-Path $repo 'docs/assets'
        $releaseDocs = Join-Path $repo 'docs/release'
        New-Item -ItemType Directory -Path $exeDir, $assetRoot, $assetDocs, $releaseDocs | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $exeDir 'ACRUX.exe'), [byte[]](77, 90, 0, 1))
        'path,kind,creator,source,license,modified,embedded_in' | Set-Content -LiteralPath (Join-Path $assetDocs 'asset-ledger.csv') -Encoding utf8
        @(
            'kind,name,version,source,license,redistributed,notice_required',
            'toolchain,MSVC,selected by build script,Microsoft,Microsoft Software License Terms,false,false'
        ) | Set-Content -LiteralPath (Join-Path $releaseDocs 'dependency-ledger.csv') -Encoding utf8

        $policy = [ordered]@{
            schemaVersion = 1
            internalLimitBytes = 1350000
            officialLimitBytes = 1474560
            packageFiles = @([ordered]@{ source = 'build/release/ACRUX.exe'; destination = 'ACRUX.exe' })
            allowedImports = @('KERNEL32.dll', 'USER32.dll')
            forbiddenNetworkImports = @('WINHTTP.dll', 'WININET.dll', 'WS2_32.dll', 'URLMON.dll')
            assetRoot = 'assets/source'
            assetLedger = 'docs/assets/asset-ledger.csv'
            dependencyLedger = 'docs/release/dependency-ledger.csv'
            candidateRoot = 'artifacts/rc'
        }
        $policyPath = Join-Path $repo 'release-policy.json'
        [IO.File]::WriteAllText($policyPath, ($policy | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))

        $fakeDumpbin = Join-Path $repo 'fake-dumpbin.cmd'
        @('@echo off', 'echo     KERNEL32.dll', 'echo     USER32.dll', 'exit /b 0') | Set-Content -LiteralPath $fakeDumpbin -Encoding ascii
        $verifyScript = Join-Path $scriptRoot 'verify-release.ps1'
        & $verifyScript -RepositoryRoot $repo -PolicyPath $policyPath -DumpbinPath $fakeDumpbin -CandidateName 'RC-command-test' | Out-Null

        $candidate = Join-Path $repo 'artifacts/rc/RC-command-test'
        Assert-True (Test-Path -LiteralPath (Join-Path $candidate 'ACRUX.zip') -PathType Leaf) 'The command must produce a validated ACRUX ZIP'
        Assert-True (Test-Path -LiteralPath (Join-Path $candidate 'verification.json') -PathType Leaf) 'The command must preserve its report'
    }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        $resolvedTemp = [IO.Path]::GetFullPath($tempRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if (-not $resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a test path outside the system temp directory: $resolvedTemp"
        }
        Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    }
}

Write-Host "Harness tests: $script:Passed passed, $script:Failed failed"
if ($script:Failed -ne 0) { exit 1 }
