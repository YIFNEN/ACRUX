[CmdletBinding()]
param(
    [ValidateSet('Test', 'Game')]
    [string]$Target = 'Test',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x86', 'x64')]
    [string]$Arch = 'x86',

    [switch]$EndingPreview
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$vsPath = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if (-not $vsPath) {
    throw 'A Visual Studio C++ toolchain was not found.'
}

$vsDevCmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "VsDevCmd.bat was not found at $vsDevCmd"
}

$environmentLines = & $env:ComSpec /d /s /c "`"$vsDevCmd`" -no_logo -arch=$Arch -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw "Visual Studio environment setup failed for $Arch."
}

foreach ($line in $environmentLines) {
    $equals = $line.IndexOf('=')
    if ($equals -gt 0) {
        $name = $line.Substring(0, $equals)
        $value = $line.Substring($equals + 1)
        # Some hosts expose both PATH and Path. Keep the VsDevCmd value rather than
        # allowing the inherited mixed-case entry to overwrite the compiler paths.
        if ($name -ieq 'Path' -and -not $value.Contains((Join-Path $vsPath 'VC\Tools'))) {
            continue
        }
        Set-Item -LiteralPath "Env:$name" -Value $value
    }
}

$cl = (Get-Command cl.exe -ErrorAction Stop).Source
$outName = "{0}-{1}-{2}" -f $Target.ToLowerInvariant(), $Configuration.ToLowerInvariant(), $Arch
if ($EndingPreview) {
    if ($Target -ne 'Game' -or $Configuration -ne 'Debug') {
        throw 'EndingPreview is only supported for the Debug Game target.'
    }
    $outName = 'game-ending-preview-debug-{0}' -f $Arch
}
$outDir = Join-Path $repoRoot ("out\{0}" -f $outName)
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$common = @('/nologo', '/std:c++17', '/utf-8', '/W4', '/WX', '/permissive-', '/GR-', '/FC')
if ($Configuration -eq 'Release') {
    $common += @('/MT', '/O1', '/GL', '/Gw', '/DNDEBUG')
} else {
    $common += @('/MTd', '/Od', '/Zi', '/D_DEBUG')
}

if ($Target -eq 'Test') {
    $sources = @(Join-Path $repoRoot 'tests\game_core_tests.cpp')
    $coreSource = Join-Path $repoRoot 'src\game_core.cpp'
    if (Test-Path -LiteralPath $coreSource) {
        $sources += $coreSource
    }
    $contentSource = Join-Path $repoRoot 'src\content_data.cpp'
    if (Test-Path -LiteralPath $contentSource) {
        $sources += $contentSource
    }
    $audioSource = Join-Path $repoRoot 'src\acrux_synth.cpp'
    if (Test-Path -LiteralPath $audioSource) {
        $sources += $audioSource
    }
    $epilogueAudioSource = Join-Path $repoRoot 'src\acrux_epilogue.cpp'
    if (Test-Path -LiteralPath $epilogueAudioSource) {
        $sources += $epilogueAudioSource
    }
    $exe = Join-Path $outDir 'game_core_tests.exe'
    $arguments = $common + @('/EHsc', '/DLC_TEST', "/Fe:$exe") + $sources + @(
        '/link',
        '/SUBSYSTEM:CONSOLE',
        '/STACK:8388608'
    )
} else {
    $sources = @(
        (Join-Path $repoRoot 'src\win32_main.cpp'),
        (Join-Path $repoRoot 'src\atlas_data.cpp'),
        (Join-Path $repoRoot 'src\production_assets.cpp'),
        (Join-Path $repoRoot 'src\cutscene_assets.cpp'),
        (Join-Path $repoRoot 'src\title_assets.cpp'),
        (Join-Path $repoRoot 'src\game_core.cpp'),
        (Join-Path $repoRoot 'src\content_data.cpp'),
        (Join-Path $repoRoot 'src\acrux_epilogue.cpp'),
        (Join-Path $repoRoot 'src\acrux_synth.cpp'),
        (Join-Path $repoRoot 'src\acrux_waveout.cpp')
    )
    foreach ($source in $sources) {
        if (-not (Test-Path -LiteralPath $source)) {
            throw "Required game source is missing: $source"
        }
    }
    $exeName = if ($EndingPreview) { 'ACRUX_ENDING_PREVIEW.exe' } else { 'ACRUX.exe' }
    $exe = Join-Path $outDir $exeName
    if ($EndingPreview) {
        $common += '/DACRUX_ENDING_PREVIEW_BUILD'
    }
    $arguments = $common + @('/EHsc-', '/DUNICODE', '/D_UNICODE', "/Fe:$exe") + $sources + @(
        '/link',
        '/SUBSYSTEM:WINDOWS',
        '/INCREMENTAL:NO',
        '/STACK:8388608',
        '/OPT:REF',
        '/OPT:ICF',
        'user32.lib',
        'gdi32.lib',
        'winmm.lib'
    )
    if ($Configuration -eq 'Release') {
        $arguments += '/LTCG'
    }
}

Push-Location $outDir
try {
    & $cl @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed with exit code $LASTEXITCODE."
    }
} finally {
    Pop-Location
}

Get-Item -LiteralPath $exe | Select-Object FullName, Length, LastWriteTime

if ($Target -eq 'Test') {
    & $exe
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed with exit code $LASTEXITCODE."
    }
}
