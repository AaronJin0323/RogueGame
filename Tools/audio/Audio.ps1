# The audio import (02_Systems/audio.md R-AUD-04, R-AUD-19, R-AUD-22; Validation/S4_audio_wave1.md §4.1, §8.2): one heavy entry script.
#   & Tools\audio\Audio.ps1
# 1. select   python -I Tools\audio\audio_select.py: every WAV the accepted rows need, against Tools\audio\sound_manifest.csv. It never
#             synthesises: a missing or stale file fails here and names python -I Tools\audio\audio_build.py, which writes them.
# 2. import   Tools\EditorPython.bat Tools\audio\import_audio.py (a commandlet: no window, no renderer, no sound): the SoundWaves, the class
#             tree and SMX_Duck under /Game/DT/Audio. A second run on an unchanged catalog saves nothing.
# 3. tests    Tools\RunTests.ps1 -Filter Dusktracer.Audio.Assets, then Dusktracer.Audio.Classes (T-AUD-19, T-AUD-04), inside this run's slot.
# Prints "AUDIO IMPORT: PASS (...)" or "AUDIO IMPORT: FAIL (...)"; exit 0 / 1. No step plays a sound: every launch is unattended.
param()

trap { Write-Host "AUDIO IMPORT: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "..\Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "Audio"  # D-055 item 7, R-TEC-18: one machine slot for the commandlet and the tests; the children inherit it

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Tools = Join-Path $Root "Tools"

$Select = & python -I (Join-Path $PSScriptRoot "audio_select.py") 2>&1
$Line = ($Select | Select-String "^SELECT:" | Select-Object -Last 1).Line
if ($LASTEXITCODE -ne 0) {
	$Select | Select-String "^error:" | ForEach-Object { Write-Host "    $($_.Line)" }
	Write-Host "AUDIO IMPORT: FAIL (select: $Line; run python -I Tools\audio\audio_build.py)"
	exit 1
}

$Import = & (Join-Path $Tools "EditorPython.bat") (Join-Path $PSScriptRoot "import_audio.py") 2>&1
$Saved = ($Import | Select-String "import_audio: \d+ wave" | Select-Object -Last 1).Line
if ($LASTEXITCODE -ne 0) {
	$Import | Select-Object -Last 12 | ForEach-Object { Write-Host "    $_" }
	Write-Host "AUDIO IMPORT: FAIL (import; see Saved\Logs\EditorPython_import_audio.log)"
	exit 1
}

$Verdicts = @()
foreach ($Test in "Dusktracer.Audio.Assets", "Dusktracer.Audio.Classes") {
	$Out = & (Join-Path $Tools "RunTests.ps1") -Filter $Test -Log (Join-Path $Root "Saved\Logs\Audio_$($Test.Split('.')[-1]).log") *>&1 | ForEach-Object { "$_" }
	$Tests = ($Out | Select-String "^TESTS:" | Select-Object -Last 1).Line
	if ($LASTEXITCODE -ne 0 -or $Tests -notmatch "^TESTS: PASS \(1 run of 1 found") {
		$Out | Select-String "^  ! " | ForEach-Object { Write-Host $_.Line }
		Write-Host "AUDIO IMPORT: FAIL ($Test`: $Tests)"
		exit 1
	}
	$Verdicts += "$($Test.Split('.')[-1]) PASS"
}

Write-Host "AUDIO IMPORT: PASS ($Line; $($Saved -replace '^.*import_audio: ', ''); $($Verdicts -join ', '))"
exit 0
