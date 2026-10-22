$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Passed = 0
$script:Failed = 0
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$packer = Join-Path $scriptRoot 'embed-assets.ps1'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('acrux-atlas-tests-' + [guid]::NewGuid().ToString('N'))

Add-Type -AssemblyName System.Drawing

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    if ($Expected -ne $Actual) {
        throw "$Message (expected: $Expected, actual: $Actual)"
    }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Pattern, [string]$Message)
    try { & $Action }
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
    }
}

function New-FixtureBitmap {
    param(
        [string]$Path,
        [int]$Width,
        [int]$Height,
        [System.Drawing.Color]$Fill
    )
    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try { $graphics.Clear($Fill) } finally { $graphics.Dispose() }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $bitmap.Dispose() }
}

New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $transparent = [System.Drawing.ColorTranslator]::FromHtml('#000000')
    $index1 = [System.Drawing.ColorTranslator]::FromHtml('#070B18')
    $index31 = [System.Drawing.ColorTranslator]::FromHtml('#35B8C2')
    $unknown = [System.Drawing.ColorTranslator]::FromHtml('#FF00FF')

    $player = Join-Path $tempRoot 'player.png'
    $world = Join-Path $tempRoot 'world.png'
    $creatures = Join-Path $tempRoot 'creatures.png'
    New-FixtureBitmap $player 256 256 $transparent
    New-FixtureBitmap $world 256 256 $transparent
    New-FixtureBitmap $creatures 256 128 $transparent

    $playerBitmap = [System.Drawing.Bitmap]::new($player)
    try {
        $playerBitmap.SetPixel(0, 0, $index1)
        $playerBitmap.SetPixel(1, 0, $index31)
        $rewritten = Join-Path $tempRoot 'player-rewritten.png'
        $playerBitmap.Save($rewritten, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $playerBitmap.Dispose() }
    Move-Item -LiteralPath $rewritten -Destination $player -Force

    $payload = Join-Path $tempRoot 'atlas.5bpp'
    $include = Join-Path $tempRoot 'atlas_data.inc'
    $validArgs = @{
        PlayerPath = $player
        WorldPath = $world
        CreaturesPath = $creatures
        BinaryOutput = $payload
        IncludeOutput = $include
    }

    Invoke-Test 'real sheets pack exact size and 5bpp MSB-first order' {
        & $packer @validArgs
        $bytes = [IO.File]::ReadAllBytes($payload)
        Assert-Equal 102400 $bytes.Length 'combined payload size'
        Assert-Equal 0x0F $bytes[0] 'indices 1 and 31 must start with 00001111'
        Assert-Equal 0xC0 $bytes[1] 'index 31 tail and zero index must continue across bytes'
    }

    Invoke-Test 'wrong sheet dimensions fail closed' {
        $wrong = Join-Path $tempRoot 'wrong-player.png'
        New-FixtureBitmap $wrong 255 256 $transparent
        $caseParameters = $validArgs.Clone()
        $caseParameters.PlayerPath = $wrong
        Assert-Throws { & $packer @caseParameters } 'dimension|256' 'wrong size must fail'
    }

    Invoke-Test 'unknown source color fails closed' {
        $invalid = Join-Path $tempRoot 'invalid-player.png'
        New-FixtureBitmap $invalid 256 256 $transparent
        $invalidBitmap = [System.Drawing.Bitmap]::new($invalid)
        try {
            $invalidBitmap.SetPixel(0, 0, $unknown)
            $rewrittenInvalid = Join-Path $tempRoot 'invalid-player-rewritten.png'
            $invalidBitmap.Save($rewrittenInvalid, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $invalidBitmap.Dispose() }
        Move-Item -LiteralPath $rewrittenInvalid -Destination $invalid -Force
        $caseParameters = $validArgs.Clone()
        $caseParameters.PlayerPath = $invalid
        Assert-Throws { & $packer @caseParameters } 'palette|color' 'unknown color must fail'
    }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

Write-Host "$script:Passed passed, $script:Failed failed"
if ($script:Failed -ne 0) { exit 1 }
