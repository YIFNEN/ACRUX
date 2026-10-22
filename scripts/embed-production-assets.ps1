[CmdletBinding()]
param(
    [string]$RuntimeRoot = (Join-Path $PSScriptRoot '..\assets\runtime\production'),
    [string]$IncludeOutput = (Join-Path $PSScriptRoot '..\src\generated\production_assets.inc')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$assets = @(
    @('world','W01_ForestGround_1',24,24,12,18,0), @('world','W01_ForestGround_2',24,24,12,18,0),
    @('world','W01_ForestGround_3',24,24,12,18,0), @('world','W01_ForestGround_4',24,24,12,18,0),
    @('world','W02_ForestRock_1',24,24,12,18,0), @('world','W02_ForestRock_2',24,24,12,18,0),
    @('world','W02_ForestRock_3',24,24,12,18,0), @('world','W03_ForestBush_1',24,24,12,18,0),
    @('world','W03_ForestBush_2',24,24,12,18,0), @('world','W03_ForestBush_3',24,24,12,18,0),
    @('world','W04_Path_Uncharged',24,24,12,18,0), @('world','W04_Path_Charged',24,24,12,18,0),
    @('world','W04_Path_Broken',24,24,12,18,0), @('world','W07_Food',24,24,12,18,0),
    @('world','W07_Water',24,24,12,18,0), @('world','W07_PathMaterial',24,24,12,18,0),
    @('world','W05_Base_Observatory',64,64,32,44,0), @('world','W06_Relay_1',80,80,40,72,0),
    @('world','W06_Relay_2',80,96,40,88,0), @('world','W06_Relay_3',96,88,48,80,0),
    @('world','W06_Relay_1_Connected',80,80,40,72,0), @('world','W06_Relay_2_Connected',80,96,40,88,0),
    @('world','W06_Relay_3_Connected',96,88,48,80,0),
    @('world','W08_Rustle_Ready',24,24,12,18,0), @('world','W08_Rustle_Triggered',24,24,12,18,0),
    @('world','W08_Snare_Ready',24,24,12,18,0), @('world','W08_Snare_Triggered',24,24,12,18,0),
    @('moth','C01_Moth_IdleA_32c_prod',32,32,16,20,1), @('moth','C01_Moth_IdleB_32c_prod',32,32,16,20,1),
    @('moth','C01_Moth_Alert_32c_prod',32,32,16,20,1), @('moth','C01_Moth_Charge_32c_prod',32,32,16,20,1),
    @('moth','C01_Moth_Stunned_32c_prod',32,32,16,20,1),
    @('boar','C02_Boar_BaseA_32c_prod',48,32,24,24,2), @('boar','C02_Boar_BaseB_32c_prod',48,32,24,24,2),
    @('boar','C02_Boar_ChargePrep1_32c_prod',48,32,24,24,2), @('boar','C02_Boar_ChargePrep2_32c_prod',48,32,24,24,2),
    @('boar','C02_Boar_ChargeDash_32c_prod',48,32,24,24,2), @('boar','C02_Boar_StunnedDrop_32c_prod',48,32,24,24,2),
    @('boar','C02_Boar_StunnedLoop_32c_prod',48,32,24,24,2),
    @('world','W05_BaseInteraction_01',12,10,6,13,0), @('world','W05_BaseInteraction_02',12,10,6,13,0),
    @('world','W07_PathScrap',24,24,12,18,0),
    @('fx','FX_Charge_Manual',72,24,2,22,0), @('fx','FX_Boar_PathImpact',72,24,12,12,0),
    @('fx','FX_Boar_StunLoop',80,24,20,18,0),
    @('ui','B4_food_critical',24,24,0,0,0), @('ui','B4_water_critical',24,24,0,0,0),
    @('title','ACRUX_TITLE_LOGO_192x64',192,64,0,0,0),
    @('title','ACRUX_LANDMARKS_3x64x64',192,64,0,0,0)
)

$moth = @('#000000','#070B18','#11182D','#1D2742','#334665','#416C6A','#747C9A','#A2A7BF','#D7D9EB','#F3F2FA','#9B7067','#D8A692','#774D45','#E6A34A','#238B9A','#53E6EA','#1A1228','#2C1B42','#44305E','#5C4675','#75618B','#907AA3','#B2A1BE','#D0C5D7','#3B1118','#6E171A','#9B231C','#C9361E','#E65020','#F36A22','#FF922D','#FFC05A')
$boar = @('#000000','#070B18','#11182D','#202736','#343A45','#4A5058','#60666C','#767B7F','#8C9092','#A2A4A3','#B8B8B4','#CDCCC5','#E3E0D5','#55503F','#6E6647','#87794A','#2A323D','#3C4651','#515C67','#68737D','#808991','#99A0A5','#B1B6B8','#C9CCCA','#4C3D18','#66521B','#806921','#9B812B','#B69A39','#CEB34B','#E4CA66','#F3DF8B')
$worldRgb = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'world\world32.rgb'))
if ($worldRgb.Length -ne 96) { throw 'world32.rgb must be exactly 96 bytes.' }
$epilogueRgb = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'epilogue\ACRUX_EPILOGUE32.rgb'))
if ($epilogueRgb.Length -ne 96) { throw 'ACRUX_EPILOGUE32.rgb must be exactly 96 bytes.' }
$panelRgb = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'ui\ACRUX_PANEL32.rgb'))
if ($panelRgb.Length -ne 96) { throw 'ACRUX_PANEL32.rgb must be exactly 96 bytes.' }
$cursorRgb = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'ui\ACRUX_CURSOR32.rgb'))
if ($cursorRgb.Length -ne 96) { throw 'ACRUX_CURSOR32.rgb must be exactly 96 bytes.' }
$storageRgb = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'ui\ACRUX_STORAGE32.rgb'))
if ($storageRgb.Length -ne 96) { throw 'ACRUX_STORAGE32.rgb must be exactly 96 bytes.' }

$bytes = [Collections.Generic.List[byte]]::new()
$frames = [Collections.Generic.List[string]]::new()
$assetOffsets = @{}
$pixelOffset = 0
$script:packBuffer = 0
$script:packBits = 0
function Get-PackedIndex([byte[]]$Data, [int]$PixelIndex) {
    $bitIndex = $PixelIndex * 5
    $byteIndex = [Math]::Floor($bitIndex / 8)
    $bitOffset = $bitIndex - $byteIndex * 8
    $next = if ($byteIndex + 1 -lt $Data.Length) { $Data[$byteIndex + 1] } else { 0 }
    $window = ([int]$Data[$byteIndex] -shl 8) -bor $next
    return ($window -shr (11 - $bitOffset)) -band 31
}
function Add-ProductionIndex([int]$Index) {
    $script:packBuffer = ($script:packBuffer -shl 5) -bor ($Index -band 31)
    $script:packBits += 5
    while ($script:packBits -ge 8) {
        $script:packBits -= 8
        $bytes.Add([byte](($script:packBuffer -shr $script:packBits) -band 255))
        $script:packBuffer = if ($script:packBits -eq 0) { 0 } else {
            $script:packBuffer -band ((1 -shl $script:packBits) - 1)
        }
    }
}
function Flush-ProductionBits() {
    if ($script:packBits -ne 0) {
        $bytes.Add([byte](($script:packBuffer -shl (8 - $script:packBits)) -band 255))
        $script:packBuffer = 0
        $script:packBits = 0
    }
}
foreach ($asset in $assets) {
    $path = Join-Path $RuntimeRoot ("{0}\{1}.5bpp" -f $asset[0],$asset[1])
    [byte[]]$raw = [IO.File]::ReadAllBytes($path)
    $pixels = [int]$asset[2] * [int]$asset[3]
    $expected = [Math]::Ceiling($pixels * 5 / 8)
    if ($raw.Length -ne $expected) { throw "$path must be $expected bytes, got $($raw.Length)." }
    for ($pixel = 0; $pixel -lt $pixels; ++$pixel) {
        Add-ProductionIndex (Get-PackedIndex $raw $pixel)
    }
    $assetOffsets[$asset[1]] = $pixelOffset
    $frames.Add("    { $($pixelOffset)u, $($asset[2])u, $($asset[3])u, $($asset[4]), $($asset[5]), $($asset[6])u }, // $($asset[1])")
    $pixelOffset += $pixels
}

[byte[]]$allBytes = $bytes.ToArray()
[byte[]]$mothSheet = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'moth\C01_Moth_32c_animation_sheet.5bpp'))
[byte[]]$boarSheet = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'boar\C02_Boar_32c_animation_sheet.5bpp'))
[byte[]]$hudSheet = [IO.File]::ReadAllBytes((Join-Path $RuntimeRoot 'hud\ACRUX_HUD_Icons_32c_prod_v4_192x24.5bpp'))
if ($hudSheet.Length -ne 2880) { throw 'HUD icon sheet must be exactly 2880 bytes.' }
for ($frame = 0; $frame -lt 5; $frame++) {
    for ($y = 0; $y -lt 32; $y++) { for ($x = 0; $x -lt 32; $x++) {
        $packed = Get-PackedIndex $allBytes ($assetOffsets['C01_Moth_IdleA_32c_prod'] + $frame * 1024 + $y * 32 + $x)
        $sheet = Get-PackedIndex $mothSheet ($y * 160 + $frame * 32 + $x)
        if ($packed -ne $sheet) { throw "MOTH frame $frame differs from its animation sheet at ($x,$y)." }
    }}
}
for ($frame = 0; $frame -lt 7; $frame++) {
    $sheetColumn = $frame % 4
    $sheetRow = [Math]::Floor($frame / 4)
    for ($y = 0; $y -lt 32; $y++) { for ($x = 0; $x -lt 48; $x++) {
        $packed = Get-PackedIndex $allBytes ($assetOffsets['C02_Boar_BaseA_32c_prod'] + $frame * 1536 + $y * 48 + $x)
        $sheet = Get-PackedIndex $boarSheet (($sheetRow * 32 + $y) * 192 + $sheetColumn * 48 + $x)
        if ($packed -ne $sheet) { throw "BOAR frame $frame differs from its animation sheet at ($x,$y)." }
    }}
}

$hudNames = @('PERSONAL_LIGHT','BASE_ENERGY','RELAY','BLINK','FOOD','WATER','PATH_TILES','NIGHT_CYCLE')
for ($frame = 0; $frame -lt 8; $frame++) {
    for ($y = 0; $y -lt 24; $y++) {
        for ($x = 0; $x -lt 24; $x++) {
            $index = Get-PackedIndex $hudSheet ($y * 192 + $frame * 24 + $x)
            Add-ProductionIndex $index
        }
    }
    $frames.Add("    { $($pixelOffset)u, 24u, 24u, 0, 0, 0u }, // HUD_$($hudNames[$frame])")
    $pixelOffset += 24 * 24
}

$extraAssets = @(
    @('world','W04_Path_Stabilized',24,24,12,18,0),
    @('world','W04_Path_Break_FX_Amber',72,24,12,12,0),
    @('epilogue','puddle_normal_40x40',40,40,20,20,3),
    @('epilogue','puddle_ripple_variants_3x40x40',120,40,20,20,3),
    @('epilogue','southern_cross_reflection_overlay_40x40',40,40,20,20,3),
    @('epilogue','southern_cross_reflection_ripple_3x40x40',120,40,20,20,3),
    @('epilogue','winter_particles_12x12x8',96,12,6,6,3),
    @('epilogue','spring_particles_12x12x8',96,12,6,6,3),
    @('epilogue','summer_particles_12x12x8',96,12,6,6,3),
    @('epilogue','autumn_particles_12x12x8',96,12,6,6,3),
    @('epilogue','season_micro_ornaments_4x8x8',32,8,4,4,3),
    @('ui','base_context_panel_80x64_white',80,64,0,0,4),
    @('ui','base_context_cursor_2x8x8_refined',16,8,0,0,5),
    @('ui','base_storage_window_176x104_refinedwood',176,104,0,0,6)
)
foreach ($asset in $extraAssets) {
    $path = Join-Path $RuntimeRoot ("{0}\{1}.5bpp" -f $asset[0],$asset[1])
    [byte[]]$raw = [IO.File]::ReadAllBytes($path)
    $pixels = [int]$asset[2] * [int]$asset[3]
    $expected = [Math]::Ceiling($pixels * 5 / 8)
    if ($raw.Length -ne $expected) { throw "$path must be $expected bytes, got $($raw.Length)." }
    for ($pixel = 0; $pixel -lt $pixels; ++$pixel) {
        Add-ProductionIndex (Get-PackedIndex $raw $pixel)
    }
    $frames.Add("    { $($pixelOffset)u, $($asset[2])u, $($asset[3])u, $($asset[4]), $($asset[5]), $($asset[6])u }, // $($asset[1])")
    $pixelOffset += $pixels
}
Flush-ProductionBits

function Palette-Line([byte[]]$rgb) {
    $values = for($i=0;$i -lt 32;$i++){ '0x00{0:X2}{1:X2}{2:X2}u' -f $rgb[$i*3],$rgb[$i*3+1],$rgb[$i*3+2] }
    return '    { ' + ($values -join ', ') + ' }'
}
function Hex-Palette-Line([string[]]$hexes) {
    return '    { ' + (($hexes | ForEach-Object { '0x00{0}u' -f $_.Substring(1) }) -join ', ') + ' }'
}

$builder=[Text.StringBuilder]::new()
[void]$builder.Append("static constexpr std::uint32_t kProductionPalettes[7][32] = {`n")
[void]$builder.Append((Palette-Line $worldRgb)+",`n"+(Hex-Palette-Line $moth)+",`n"+(Hex-Palette-Line $boar)+",`n"+(Palette-Line $epilogueRgb)+",`n"+(Palette-Line $panelRgb)+",`n"+(Palette-Line $cursorRgb)+",`n"+(Palette-Line $storageRgb)+"`n};`n`n")
[void]$builder.Append("static constexpr std::uint8_t kProductionBytes[$($bytes.Count)] = {`n")
for($offset=0;$offset -lt $bytes.Count;$offset+=32){ $count=[Math]::Min(32,$bytes.Count-$offset); $line=for($i=0;$i -lt $count;$i++){'0x{0:X2}' -f $bytes[$offset+$i]}; [void]$builder.Append('    '+($line -join ', ')+",`n") }
[void]$builder.Append("};`n`nstatic constexpr ProductionFrame kProductionFrames[$($frames.Count)] = {`n"+($frames -join "`n")+"`n};`n")
$parent=Split-Path -Parent ([IO.Path]::GetFullPath($IncludeOutput)); if(-not(Test-Path $parent)){New-Item -ItemType Directory -Path $parent | Out-Null}
[IO.File]::WriteAllText([IO.Path]::GetFullPath($IncludeOutput),$builder.ToString(),[Text.UTF8Encoding]::new($false))
Write-Host "PASS production assets: $($frames.Count) frames, $($bytes.Count) bytes"
