[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Logic', 'Playable', 'Size', 'Candidate')]
    [string]$Profile,

    [switch]$CreateCandidate,

    [string]$CandidateName
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptRoot
$modulePath = Join-Path $scriptRoot 'ReleaseHarness.psm1'
$buildScript = Join-Path $scriptRoot 'build.ps1'
$harnessTests = Join-Path $scriptRoot 'test-release-harness.ps1'
$verifyRelease = Join-Path $scriptRoot 'verify-release.ps1'
$policyPath = Join-Path $scriptRoot 'release-policy.json'
$releaseExe = Join-Path $repoRoot 'out\game-release-x86\ACRUX.exe'

Import-Module $modulePath -Force
$route = Get-IterationProfile -Profile $Profile -CreateCandidate:$CreateCandidate
$beforeBytes = if (Test-Path -LiteralPath $releaseExe -PathType Leaf) {
    [int64](Get-Item -LiteralPath $releaseExe).Length
} else {
    $null
}

function Invoke-Build {
    param([string]$Target, [string]$Configuration)
    & $buildScript -Target $Target -Configuration $Configuration -Arch x86
}

function Find-X86Dumpbin {
    $vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'Visual Studio Installer vswhere.exe was not found.'
    }
    $vsPath = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
    if (-not $vsPath) { throw 'A Visual Studio C++ toolchain was not found.' }
    $toolRoots = @(Get-ChildItem -LiteralPath (Join-Path $vsPath 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending)
    foreach ($toolRoot in $toolRoots) {
        $candidate = Join-Path $toolRoot.FullName 'bin\Hostx64\x86\dumpbin.exe'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw 'An x86 dumpbin.exe was not found in the selected Visual Studio installation.'
}

foreach ($action in $route.Actions) {
    switch ($action) {
        'HarnessTests' { & $harnessTests }
        'CoreDebug'    { Invoke-Build -Target Test -Configuration Debug }
        'CoreRelease'  { Invoke-Build -Target Test -Configuration Release }
        'GameDebug'    { Invoke-Build -Target Game -Configuration Debug }
        'GameRelease'  { Invoke-Build -Target Game -Configuration Release }
        'MeasureSize'  {
            $policy = Get-Content -Raw -LiteralPath $policyPath | ConvertFrom-Json
            $afterBytes = [int64](Get-Item -LiteralPath $releaseExe).Length
            $size = Test-ReleaseSize -Bytes $afterBytes -Policy $policy
            if (-not $size.InternalPass) {
                throw "Release build exceeds the internal limit by $(-$size.InternalRemaining) bytes."
            }
            $deltaText = if ($null -eq $beforeBytes) { 'n/a (no previous Release)' } else { '{0:+#;-#;0} bytes' -f ($afterBytes - $beforeBytes) }
            Write-Host "PASS size profile: $afterBytes / $($policy.internalLimitBytes) bytes"
            Write-Host "Delta: $deltaText; remaining: $($size.InternalRemaining) bytes"
        }
        'CreateCandidate' {
            $parameters = @{
                RepositoryRoot = $repoRoot
                PolicyPath     = $policyPath
                DumpbinPath    = Find-X86Dumpbin
            }
            if (-not [string]::IsNullOrWhiteSpace($CandidateName)) {
                $parameters.CandidateName = $CandidateName
            }
            & $verifyRelease @parameters
        }
        default { throw "Unknown iteration action: $action" }
    }
}

Write-Host "PASS $Profile iteration profile"
