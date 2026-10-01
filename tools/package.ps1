# Assemble WetFunction NG: dist\WetFunction NG\ (Data layout), deploy it to the MO2 mod folder and pack a .7z.
# Run after: xmake build WetFunctionNG, tools\build_papyrus.ps1, python tools\build_esp.py
#
# Run: powershell -ExecutionPolicy Bypass -File tools\package.ps1 [-NoArchive]

param([switch]$NoArchive)

$ErrorActionPreference = 'Stop'

$root    = Split-Path -Parent $PSScriptRoot
$version = '1.2.0'
$stage   = Join-Path $root 'dist\WetFunction NG'
$archive = Join-Path $root "dist\WetFunction NG $version.7z"
$deploy  = if ($env:WFNG_DEPLOY_DIR) { $env:WFNG_DEPLOY_DIR } else { Join-Path $root 'dist\deploy\WetFunction NG' }  # set WFNG_DEPLOY_DIR to your MO2 mod folder

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force "$stage\SKSE\Plugins", "$stage\SKSE\CustomConsole", "$stage\Scripts\Source" | Out-Null

Copy-Item (Join-Path $root 'plugin\WetFunctionNG.esp') $stage
Copy-Item (Join-Path $root 'build\windows\x64\releasedbg\WetFunctionNG.dll') "$stage\SKSE\Plugins\"
Copy-Item (Join-Path $root 'dist_template\SKSE\Plugins\WetFunctionNG.ini') "$stage\SKSE\Plugins\"
Copy-Item (Join-Path $root 'dist_template\SKSE\CustomConsole\WetFunctionNG.yaml') "$stage\SKSE\CustomConsole\"
Copy-Item (Join-Path $root 'assets\textures') $stage -Recurse

foreach ($s in 'WetFunctionNG', 'WetFunctionNGMCM', 'WetFunctionNGPlayerAlias') {
    $pex = Join-Path $root "papyrus\out\$s.pex"
    $psc = Join-Path $root "papyrus\source\$s.psc"
    if ((Get-Item $pex).LastWriteTime -lt (Get-Item $psc).LastWriteTime) { throw "$s.pex is older than its source - run tools\build_papyrus.ps1" }
    Copy-Item $pex "$stage\Scripts\"
    Copy-Item $psc "$stage\Scripts\Source\"
}

# verification: plugin form versions, current scripts and expected textures
$esp = [IO.File]::ReadAllBytes("$stage\WetFunctionNG.esp")
$off = 0; $bad = 0
while ($off + 24 -le $esp.Length) {
    $type = [Text.Encoding]::ASCII.GetString($esp, $off, 4)
    $size = [BitConverter]::ToUInt32($esp, $off + 4)
    if ($type -eq 'GRUP') { $off += 24; continue }
    if ([BitConverter]::ToUInt16($esp, $off + 20) -ne 44) { $bad++ }
    $off += 24 + $size
}
if ($bad) { throw "$bad record(s) in WetFunctionNG.esp are not form 44" }

$textures = @(
    'wet_001_s', 'wet_010_s', 'wet_011_s', 'wet_100_s', 'wet_101_s', 'wet_110_s', 'wet_111_s', 'wethand_s', 'wethead_s', 'wetschlong_s',
    'male_wet_01_s', 'male_wet_10_s', 'male_wet_11_s', 'male_wethand_s', 'male_wethead_s', 'male_wetschlong_s',
    'wet_001_UBE_n', 'wet_010_UBE_n', 'wet_011_UBE_n', 'wet_100_UBE_n', 'wet_101_UBE_n', 'wet_110_UBE_n', 'wet_111_UBE_n', 'wethand_UBE_n', 'wethead_UBE_n',
    'male_wet_01_UBE_n', 'male_wet_10_UBE_n', 'male_wet_11_UBE_n', 'male_wethand_UBE_n', 'male_wethead_UBE_n', 'male_wetschlong_UBE_n'
)
$missing = $textures | Where-Object { -not (Test-Path "$stage\textures\actors\character\WetFunction\$_.dds") }
if ($missing) { throw "missing textures: $($missing -join ', ')" }

$files = Get-ChildItem $stage -Recurse -File
$expected = 1 + 1 + 1 + 1 + 3 + 3 + $textures.Count
if ($files.Count -ne $expected) { throw "expected $expected files, staged $($files.Count)" }
$files | Where-Object { $_.Extension -ne '.dds' } | Select-Object @{ n = 'file'; e = { $_.FullName.Substring($stage.Length) } }, Length | Format-Table -AutoSize | Out-String -Width 200
'{0} textures, {1:N0} MB total' -f $textures.Count, (($files | Measure-Object Length -Sum).Sum / 1MB)

# deploy to MO2 (keeps meta.ini and the settings of an existing mod folder)
$userIni = Join-Path $deploy 'SKSE\Plugins\WetFunctionNG.ini'
$keptIni = if (Test-Path $userIni) { [IO.File]::ReadAllBytes($userIni) }
New-Item -ItemType Directory -Force $deploy | Out-Null
Get-ChildItem $deploy -Force | Where-Object { $_.Name -ne 'meta.ini' } | Remove-Item -Recurse -Force
Copy-Item "$stage\*" $deploy -Recurse
if ($keptIni) { [IO.File]::WriteAllBytes($userIni, $keptIni) }
Copy-Item (Join-Path $root 'build\windows\x64\releasedbg\WetFunctionNG.pdb') "$deploy\SKSE\Plugins\"
"deployed to $deploy"

if ($NoArchive) { return }
$sevenZip = @('C:\Program Files\7-Zip\7z.exe', 'C:\Program Files (x86)\7-Zip\7z.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sevenZip) { Write-Host "7-Zip not found - staged folder only: $stage"; return }
if (Test-Path $archive) { Remove-Item $archive -Force }
Push-Location $stage
try { & $sevenZip a -t7z -mx=5 $archive * | Select-Object -Last 2 } finally { Pop-Location }
Get-Item $archive | Select-Object FullName, Length
