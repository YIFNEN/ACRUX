[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PlayerPath,
    [Parameter(Mandatory)][string]$WorldPath,
    [Parameter(Mandatory)][string]$CreaturesPath,
    [Parameter(Mandatory)][string]$BinaryOutput,
    [Parameter(Mandatory)][string]$IncludeOutput
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing

$worldPaletteHex = @(
    '#000000', '#070B18', '#11182D', '#1D2742',
    '#334665', '#416C6A', '#747C9A', '#A2A7BF',
    '#D7D9EB', '#F3F2FA', '#9B7067', '#D8A692',
    '#774D45', '#E6A34A', '#238B9A', '#53E6EA',
    '#0B2230', '#173747', '#28576A', '#4F7F78',
    '#72A58F', '#4C5474', '#676F94', '#8D96B5',
    '#BEC6D9', '#FFF7D6', '#5A3038', '#A65D4E',
    '#CC805B', '#FFD278', '#D96A73', '#A5FFFF'
)
$playerPaletteHex = @(
    '#000000', '#070B18', '#11182D', '#1D2742',
    '#334665', '#416C6A', '#747C9A', '#A2A7BF',
    '#D7D9EB', '#F3F2FA', '#B7847C', '#E8C5BA',
    '#774D45', '#E6A34A', '#238B9A', '#53E6EA',
    '#0C1224', '#17223A', '#263550', '#405271',
    '#526787', '#647898', '#8994AF', '#B8BED3',
    '#C8CCE0', '#E5E6F1', '#FAF9FD', '#A86F69',
    '#C9978E', '#F0D4CB', '#1A6573', '#35B8C2'
)

function New-PaletteLookup {
    param([string[]]$Palette)
    $lookup = @{}
    for ($index = 0; $index -lt $Palette.Count; $index++) {
        $color = [System.Drawing.ColorTranslator]::FromHtml($Palette[$index])
        $key = '{0:X2}{1:X2}{2:X2}' -f $color.R, $color.G, $color.B
        $lookup[$key] = [byte]$index
    }
    return $lookup
}
$playerPaletteByRgb = New-PaletteLookup $playerPaletteHex
$worldPaletteByRgb = New-PaletteLookup $worldPaletteHex

$sheets = @(
    [pscustomobject]@{ Name = 'player'; Path = $PlayerPath; Width = 256; Height = 256; PixelOffset = 0 },
    [pscustomobject]@{ Name = 'world'; Path = $WorldPath; Width = 256; Height = 256; PixelOffset = 65536 },
    [pscustomobject]@{ Name = 'creatures'; Path = $CreaturesPath; Width = 256; Height = 128; PixelOffset = 131072 }
)

function Get-PaletteIndex {
    param([System.Drawing.Color]$Color, [string]$SheetName, [int]$X, [int]$Y, [hashtable]$PaletteByRgb)
    $key = '{0:X2}{1:X2}{2:X2}' -f $Color.R, $Color.G, $Color.B
    if (-not $paletteByRgb.ContainsKey($key)) {
        throw "Sheet '$SheetName' contains color #$key outside the approved palette at ($X,$Y)."
    }
    return [byte]$paletteByRgb[$key]
}

$indices = [System.Collections.Generic.List[byte]]::new(163840)
foreach ($sheet in $sheets) {
    if (-not (Test-Path -LiteralPath $sheet.Path -PathType Leaf)) {
        throw "Atlas source sheet is missing: $($sheet.Path)"
    }
    $bitmap = [System.Drawing.Bitmap]::new([IO.Path]::GetFullPath($sheet.Path))
    try {
        if ($bitmap.Width -ne $sheet.Width -or $bitmap.Height -ne $sheet.Height) {
            throw "Sheet '$($sheet.Name)' dimension must be $($sheet.Width)x$($sheet.Height), got $($bitmap.Width)x$($bitmap.Height)."
        }
        if ($indices.Count -ne $sheet.PixelOffset) {
            throw "Internal atlas pixel offset mismatch for '$($sheet.Name)': expected $($sheet.PixelOffset), got $($indices.Count)."
        }
        for ($y = 0; $y -lt $bitmap.Height; $y++) {
            for ($x = 0; $x -lt $bitmap.Width; $x++) {
            $lookup = if ($sheet.Name -eq 'player') { $playerPaletteByRgb } else { $worldPaletteByRgb }
            $indices.Add((Get-PaletteIndex $bitmap.GetPixel($x, $y) $sheet.Name $x $y $lookup))
            }
        }
    }
    finally { $bitmap.Dispose() }
}

if ($indices.Count -ne 163840) {
    throw "Combined atlas must contain 163840 pixels, got $($indices.Count)."
}

$payload = [System.Collections.Generic.List[byte]]::new(102400)
$currentByte = 0
$bitsInByte = 0
foreach ($index in $indices) {
    for ($shift = 4; $shift -ge 0; $shift--) {
        $currentByte = ($currentByte -shl 1) -bor (($index -shr $shift) -band 1)
        $bitsInByte++
        if ($bitsInByte -eq 8) {
            $payload.Add([byte]$currentByte)
            $currentByte = 0
            $bitsInByte = 0
        }
    }
}
if ($bitsInByte -ne 0 -or $payload.Count -ne 102400) {
    throw "Combined 5bpp atlas payload must be 102400 bytes, got $($payload.Count) with $bitsInByte trailing bits."
}

function Ensure-ParentDirectory {
    param([string]$Path)
    $parent = Split-Path -Parent ([IO.Path]::GetFullPath($Path))
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Write-AtomicBytes {
    param([string]$Path, [byte[]]$Bytes)
    Ensure-ParentDirectory $Path
    $fullPath = [IO.Path]::GetFullPath($Path)
    $temporary = "$fullPath.tmp-$([guid]::NewGuid().ToString('N'))"
    try {
        [IO.File]::WriteAllBytes($temporary, $Bytes)
        Move-Item -LiteralPath $temporary -Destination $fullPath -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Write-AtomicText {
    param([string]$Path, [string]$Value)
    Ensure-ParentDirectory $Path
    $fullPath = [IO.Path]::GetFullPath($Path)
    $temporary = "$fullPath.tmp-$([guid]::NewGuid().ToString('N'))"
    try {
        [IO.File]::WriteAllText($temporary, $Value, [Text.UTF8Encoding]::new($false))
        Move-Item -LiteralPath $temporary -Destination $fullPath -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

Write-AtomicBytes $BinaryOutput $payload.ToArray()

$builder = [Text.StringBuilder]::new()
[void]$builder.Append("static constexpr std::uint32_t kPlayerPalette[32] = {`n    ")
$paletteValues = foreach ($hex in $playerPaletteHex) { '0x00{0}u' -f $hex.Substring(1).ToUpperInvariant() }
[void]$builder.Append(($paletteValues -join ', '))
[void]$builder.Append("`n};`n`nstatic constexpr std::uint8_t kAtlasBytes[102400] = {`n")
for ($offset = 0; $offset -lt $payload.Count; $offset += 32) {
    $count = [Math]::Min(32, $payload.Count - $offset)
    $line = for ($i = 0; $i -lt $count; $i++) { '0x{0:X2}' -f $payload[$offset + $i] }
    [void]$builder.Append('    ')
    [void]$builder.Append(($line -join ', '))
    [void]$builder.Append(",`n")
}
[void]$builder.Append("};`n`nstatic constexpr SpriteFrame kSpriteFrames[16] = {`n")
$frameNames = @(
    'PlayerFrontIdle', 'PlayerFrontWalk1', 'PlayerFrontWalk2', 'PlayerFrontWalk3',
    'PlayerBackIdle', 'PlayerBackWalk1', 'PlayerBackWalk2', 'PlayerBackWalk3',
    'PlayerSideIdle', 'PlayerSideWalk1', 'PlayerSideWalk2', 'PlayerSideWalk3',
    'PlayerBlink1', 'PlayerBlink2', 'PlayerTake1', 'PlayerTake2'
)
for ($index = 0; $index -lt $frameNames.Count; $index++) {
    $x = ($index % 4) * 64
    $y = [Math]::Floor($index / 4) * 64
    [void]$builder.Append("    { 0u, 256u, $($x)u, $($y)u, 64u, 64u, 32, 58 }, // $($frameNames[$index])`n")
}
[void]$builder.Append("};`n")
Write-AtomicText $IncludeOutput $builder.ToString()

Write-Host "PASS atlas pack: $($payload.Count) bytes"
