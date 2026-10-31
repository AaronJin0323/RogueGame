# Restart proved in the package, off-screen (package.md R-PKG-10, T-PKG-10). Needs Tools\Package.ps1 first. Opens no window.
#   & Tools\PackageRestartCheck.ps1 [-Seed 9107] [-OtherSeed 2203] [-Seconds 90] [-RestartStep 1800] [-Timeout 300]
# Development proof build, -RenderOffScreen (the hooks are compiled out of Shipping, R-PKG-02):
#   A  a fresh bot run on -Seed.
#   B  a bot run on -OtherSeed that restarts through the menu's path onto -Seed: input_hash, steps and CSV match A, the old file reads
#      `restart`, the new file is new, and nothing is missing from the cook on either side of the reload.
#   C  -DTInputCheck: real key events, a real confirm on the pause screen, a fresh seed from the wall clock.
#   D  the language and the volume changed through the menu in play, then a restart: the settings file holds them, the new run_start reads
#      the language, the old one does not, and the settings subsystem did not start again; a frame after the reload passes image_check.
# Shipping (no hook reaches a restart there):
#   E  a fresh bot run on -Seed: the CSV must equal the restarted Development run's, the bridge between the two builds.
#   F  the settings file from D, no -culture: the run reads its language in Shipping.
#   Both configs must hold the same cook: every .utoc and .ucas hash matches. The .pak holds no cooked asset and differs by Development's developer plugin descriptors.
# Prints "PKGRESTART: PASS (...)" or "PKGRESTART: FAIL (...)"; exit 0 / 1, 2 when there is no package. Logs: Saved\Logs\PkgRestart_<tag>.log.
param(
	[int]$Seed = 9107,
	[int]$OtherSeed = 2203,
	[int]$Seconds = 90,
	[int]$RestartStep = 1800,
	[int]$Timeout = 300
)

trap { Write-Host "PKGRESTART: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$PackageRoot = Join-Path $Root "Saved\Package"
$DevExe = Join-Path $PackageRoot "Development\Windows\Dusktracer\Binaries\Win64\Dusktracer.exe"
$ShipExe = Join-Path $PackageRoot "Shipping\Windows\Dusktracer\Binaries\Win64\Dusktracer-Win64-Shipping.exe"
$DevRuns = Join-Path $PackageRoot "Development\Windows\Dusktracer\Saved\Playtests"
$ShipRuns = Join-Path $env:LOCALAPPDATA "Dusktracer\Saved\Playtests"
$LogDir = Join-Path $Root "Saved\Logs"
$WorkDir = Join-Path $Root "Saved\PkgRestart"
$Map = "/Game/DT/Maps/L_Arena_Test"
$SettingsSeed = 770101
$MenuSeed = 770102
New-Item -ItemType Directory -Force $LogDir, $WorkDir | Out-Null
foreach ($File in $DevExe, $ShipExe) {
	if (-not (Test-Path $File)) { Write-Host "PKGRESTART: NOT RUN (no $File; Tools\Package.ps1)"; exit 2 }
}

$script:Failures = @()
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; $script:Failures += $Text }

function Remove-Runs {
	foreach ($Dir in $DevRuns, $ShipRuns) {
		if (Test-Path $Dir) { foreach ($S in $Seed, $OtherSeed, $SettingsSeed, $MenuSeed) { Get-ChildItem $Dir -Filter "${S}_*" -File | Remove-Item -Force } }
	}
}

function Invoke-Game([string]$Exe, [string]$Tag, [string[]]$RunArgs) {
	$ArgList = @($Map, "-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-unattended", "-nopause") + $RunArgs
	$Watch = [Diagnostics.Stopwatch]::StartNew()
	$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		Note-Fail "launch $Tag timed out"
	}
	Write-Host ("pkgrestart: {0} done in {1:n0} s" -f $Tag, $Watch.Elapsed.TotalSeconds)
}

function Invoke-Dev([string]$Tag, [string[]]$RunArgs) {
	$Log = Join-Path $LogDir "PkgRestart_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	Invoke-Game $DevExe $Tag ($RunArgs + @("-ABSLOG=`"$Log`""))
	return $Log
}

function Read-Record([string]$Path) {
	$Record = @{}
	if (Test-Path "$Path.txt") {
		foreach ($Line in [IO.File]::ReadAllLines("$Path.txt")) {
			$Pair = $Line -split "=", 2
			if ($Pair.Count -eq 2) { $Record[$Pair[0]] = $Pair[1] }
		}
	}
	return $Record
}

function Get-RunFile([string]$Dir, [int]$RunSeed, [string]$Extension) {
	return Get-ChildItem $Dir -Filter "${RunSeed}_*.$Extension" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
}

function Get-Text($File) { if ($File) { return [IO.File]::ReadAllText($File.FullName) } return "" }

function Get-LastRecord($File) {
	if (-not $File) { return "" }
	return @([IO.File]::ReadAllLines($File.FullName)) | Select-Object -Last 1
}

function Get-Stamp([string]$Line) {
	if ($Line -match '^\[(\d{4})\.(\d{2})\.(\d{2})-(\d{2})\.(\d{2})\.(\d{2}):(\d{3})\]') {
		return New-Object DateTime ([int]$Matches[1]), ([int]$Matches[2]), ([int]$Matches[3]), ([int]$Matches[4]), ([int]$Matches[5]), ([int]$Matches[6]), ([int]$Matches[7])
	}
	return $null
}

# What a reload costs, by the log's clock, and what the log says is broken: the cook on both sides of the reload.
function Read-Log([string]$Tag, [string]$Log) {
	$Lines = if (Test-Path $Log) { @([IO.File]::ReadAllLines($Log)) } else { @() }
	$Result = @{ ReloadMs = -1.0; ColdS = ""; ReloadS = ""; Starts = @($Lines | Where-Object { $_ -match "LogDT: Run start seed=[1-9]" }).Count; SettingsReads = @($Lines | Where-Object { $_ -match "LogDT: Settings: " }).Count }
	$Crash = @($Lines | Where-Object { $_ -match "Fatal error|=== Critical error:? ===|Unhandled Exception|Ensure condition failed|LogDT: (Error|Fatal)" })
	if ($Crash.Count) { Note-Fail "$Tag crash marker: $($Crash[0])" }
	# A missing asset, table or shader is a warning or an error in a Development log; a clean log has none at all.
	$Cook = @($Lines | Where-Object { $_ -cmatch ": (Warning|Error): " })
	if ($Cook.Count) { Note-Fail "$Tag log warning or error ($($Cook.Count)): $($Cook[0])" }
	$Maps = @($Lines | ForEach-Object { if ($_ -match "Took ([0-9.]+) seconds to LoadMap") { $Matches[1] } })
	if ($Maps.Count) { $Result.ColdS = $Maps[0]; $Result.ReloadS = $Maps[$Maps.Count - 1] }
	$Index = -1
	for ($I = 0; $I -lt $Lines.Count; $I++) { if ($Lines[$I] -match "LogDT: Menu: restart on seed") { $Index = $I; break } }
	if ($Index -ge 0) {
		$From = Get-Stamp $Lines[$Index]
		for ($I = $Index + 1; $I -lt $Lines.Count; $I++) {
			if ($Lines[$I] -match "LogDT: Run start seed=[1-9]") {
				$To = Get-Stamp $Lines[$I]
				if ($From -and $To) { $Result.ReloadMs = ($To - $From).TotalMilliseconds }
				break
			}
		}
	}
	return $Result
}

function Expect-Restart([string]$Tag, $Old, $New) {
	if (-not $Old) { Note-Fail "$Tag no file for the old run"; return }
	if (-not $New) { Note-Fail "$Tag no file for the new run"; return }
	if ($Old.FullName -eq $New.FullName) { Note-Fail "$Tag the new run wrote the old file" }
	$OldEnd = Get-LastRecord $Old
	if ($OldEnd -notmatch '"kind":"run_end"' -or $OldEnd -notmatch '"reason":"restart"') { Note-Fail "$Tag old run ends: $OldEnd" }
	$NewStart = @([IO.File]::ReadAllLines($New.FullName)) | Select-Object -First 1
	if ($NewStart -notmatch '"kind":"run_start"') { Note-Fail "$Tag new file does not open on run_start: $NewStart" }
}

function Get-Culture($File) {
	$First = if ($File) { @([IO.File]::ReadAllLines($File.FullName)) | Select-Object -First 1 } else { "" }
	if ($First -match '"culture":"([^"]*)"') { return $Matches[1] }
	return ""
}

Remove-Runs

# --- the same cook in both configs ---------------------------------------------------------------------------------------
$Hash = @{}
foreach ($Config in "Development", "Shipping") {
	$Paks = Join-Path $PackageRoot "$Config\Windows\Dusktracer\Content\Paks"
	$Hash[$Config] = @(Get-ChildItem $Paks -File | Where-Object { $_.Extension -in ".utoc", ".ucas" } | Sort-Object Name | ForEach-Object { "{0} {1} {2}" -f $_.Name, $_.Length, (Get-FileHash $_.FullName -Algorithm SHA256).Hash })
}
$SameCook = ($Hash.Development.Count -gt 0) -and (($Hash.Development -join "`n") -ceq ($Hash.Shipping -join "`n"))
if (-not $SameCook) { Note-Fail "the configs hold different cooks" }
Write-Host ("pkgrestart: {0} container file(s), same cook={1}" -f $Hash.Development.Count, $SameCook)

# --- A, B: determinism and telemetry across the reload --------------------------------------------------------------------
$Bot = @("-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=0.6", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotFast=60", "-DTBotQuit", "-DTBotMaxSeconds=$Seconds")
$OutA = Join-Path $WorkDir "fresh.csv"
$OutB = Join-Path $WorkDir "restarted.csv"
$OutE = Join-Path $WorkDir "shipping.csv"
foreach ($Out in $OutA, $OutB, $OutE) { foreach ($File in $Out, "$Out.txt") { if (Test-Path $File) { Remove-Item $File -Force } } }

$LogA = Invoke-Dev "a" ($Bot + @("-culture=en", "-DTSeed=$Seed", "-DTBotOut=`"$OutA`""))
$CsvFresh = Get-Text (Get-RunFile $DevRuns $Seed "csv")
$A = Read-Log "A" $LogA
Get-ChildItem $DevRuns -Filter "${Seed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force

$LogB = Invoke-Dev "b" ($Bot + @("-culture=en", "-DTSeed=$OtherSeed", "-DTRestartAtStep=$RestartStep", "-DTRestartSeed=$Seed", "-DTBotOut=`"$OutB`""))
$B = Read-Log "B" $LogB
$OldB = Get-RunFile $DevRuns $OtherSeed "jsonl"
$NewB = Get-RunFile $DevRuns $Seed "jsonl"
Expect-Restart "B" $OldB $NewB
$CsvRestarted = Get-Text (Get-RunFile $DevRuns $Seed "csv")
$Fresh = Read-Record $OutA
$Restarted = Read-Record $OutB
$HashOk = $Fresh["input_hash"] -and ($Fresh["input_hash"] -eq $Restarted["input_hash"])
$StepsOk = $Fresh["steps"] -and ($Fresh["steps"] -eq $Restarted["steps"])
$CsvOk = ($CsvFresh.Length -gt 0) -and ($CsvFresh -ceq $CsvRestarted)
if (-not $HashOk) { Note-Fail "input_hash $($Fresh['input_hash']) fresh, $($Restarted['input_hash']) restarted" }
if (-not $StepsOk) { Note-Fail "steps $($Fresh['steps']) fresh, $($Restarted['steps']) restarted" }
if (-not $CsvOk) { Note-Fail "the restarted CSV is not the fresh CSV" }
if ($B.Starts -ne 2) { Note-Fail "B has $($B.Starts) Run start line(s), expected 2" }
Write-Host ("    A/B input_hash {0} / {1}, steps {2} / {3}, csv same={4}, reload {5:n0} ms" -f $Fresh["input_hash"], $Restarted["input_hash"], $Fresh["steps"], $Restarted["steps"], $CsvOk, $B.ReloadMs)

# --- C: the real confirm, real keys, a seed from the wall clock ----------------------------------------------------------
$Known = @(if (Test-Path $DevRuns) { Get-ChildItem $DevRuns -Filter *.jsonl | ForEach-Object { $_.Name } })
$LogC = Invoke-Dev "c" @("-culture=en", "-DTSeed=$MenuSeed", "-DTNoDirector", "-DTInputCheck")
$C = Read-Log "C" $LogC
$InputLines = @(Select-String -Path $LogC -Pattern "INPUTCHECK" | ForEach-Object { $_.Line -replace "^.*LogDT: ", "" })
$InputLines | ForEach-Object { "    $_" }
if (-not ($InputLines | Where-Object { $_ -match "^INPUTCHECK: PASS" })) { Note-Fail "the input check did not pass" }
$OldC = Get-RunFile $DevRuns $MenuSeed "jsonl"
$NewC = Get-ChildItem $DevRuns -Filter *.jsonl | Where-Object { $Known -notcontains $_.Name -and $_.Name -notlike "${MenuSeed}_*" } | Sort-Object LastWriteTime | Select-Object -Last 1
Expect-Restart "C" $OldC $NewC
Write-Host ("    C new file {0}, reload {1:n0} ms" -f $(if ($NewC) { $NewC.Name } else { "(none)" }), $C.ReloadMs)
if ($NewC) { $NewC | Remove-Item -Force; Get-ChildItem $DevRuns -Filter ($NewC.BaseName + ".*") | Remove-Item -Force }

# --- D: settings changed in play survive the reload ------------------------------------------------------------------------
$SettingsFile = Join-Path $WorkDir "DTSettings.ini"
$Shot = Join-Path $WorkDir "after_reload.png"
foreach ($File in $SettingsFile, $Shot) { if (Test-Path $File) { Remove-Item $File -Force } }
$Menu = "DT.UI.Pause,DT.UI.Key Down,DT.UI.Key Accept,DT.UI.Key Right,DT.UI.Key Down,DT.UI.Key Left,DT.UI.Key Back,DT.UI.Key Back"
# Real time, not -DTBotFast: the shot's timer runs on world time and starts over with each world, so a first run shorter than the wait puts the frame in the restarted run.
$ShotAfter = 12
$BotReal = @("-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=0.6", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotMaxSeconds=$Seconds")
$LogD = Invoke-Dev "d" ($BotReal + @("-DTSeed=$SettingsSeed", "-DTRestartAtStep=300", "-DTRestartSeed=$OtherSeed", "-DTSettingsFile=`"$SettingsFile`"", "-ExecCmds=`"$Menu`"", "-DTShot=`"$Shot`"", "-DTShotAfter=$ShotAfter"))
$D = Read-Log "D" $LogD
$OldD = Get-RunFile $DevRuns $SettingsSeed "jsonl"
$NewD = Get-RunFile $DevRuns $OtherSeed "jsonl"
if ($NewD -and $OldB -and $NewD.Name -eq $OldB.Name) { $NewD = $null }
Expect-Restart "D" $OldD $NewD
$CultureOld = Get-Culture $OldD
$CultureNew = Get-Culture $NewD
$Saved = if (Test-Path $SettingsFile) { [IO.File]::ReadAllText($SettingsFile) } else { "" }
if ($CultureOld -ne "en") { Note-Fail "D first run reads culture '$CultureOld', expected the launch's en" }
if ($CultureNew -ne "zh-Hans") { Note-Fail "D restarted run reads culture '$CultureNew', expected the menu's zh-Hans" }
if ($Saved -notmatch "Language=zh-Hans") { Note-Fail "D settings file lost the language: $Saved" }
if ($Saved -notmatch "Volume=90\b") { Note-Fail "D settings file lost the volume: $Saved" }
if ($D.SettingsReads -ne 1) { Note-Fail "D read the settings $($D.SettingsReads) time(s), expected once" }
$ImgLine = "no shot"
if (Test-Path $Shot) {
	$Img = & python -I (Join-Path $Root "Tools\image_check.py") $Shot 2>&1
	$ImgRc = $LASTEXITCODE
	$ImgLine = ($Img | Select-String "^IMAGE:" | Select-Object -Last 1).Line
	if ($ImgRc -ne 0) { Note-Fail "D frame after the reload: $ImgLine" }
}
else { Note-Fail "D no frame after the reload" }
Write-Host ("    D culture {0} -> {1}, {2}, settings reads {3}, reload {4:n0} ms, {5}" -f $CultureOld, $CultureNew, (($Saved -split "`r?`n" | Where-Object { $_ -match "^(Language|Volume)=" }) -join " "), $D.SettingsReads, $D.ReloadMs, $ImgLine)

# --- E, F: Shipping --------------------------------------------------------------------------------------------------------
Invoke-Game $ShipExe "e" ($Bot + @("-culture=en", "-DTSeed=$Seed", "-DTBotOut=`"$OutE`""))
$CsvShipping = Get-Text (Get-RunFile $ShipRuns $Seed "csv")
$Shipping = Read-Record $OutE
$ShipEnd = Get-LastRecord (Get-RunFile $ShipRuns $Seed "jsonl")
$BridgeOk = ($CsvShipping.Length -gt 0) -and ($CsvShipping -ceq $CsvRestarted)
if (-not $BridgeOk) { Note-Fail "the Shipping fresh CSV is not the restarted Development CSV" }
# T-TEC-15: both are game targets, /fp:fast. The editor's hash is not comparable here (07_Technical R-TEC-15).
if (-not $Shipping["input_hash"] -or $Shipping["input_hash"] -ne $Restarted["input_hash"]) { Note-Fail "Shipping input_hash $($Shipping['input_hash']), Development $($Restarted['input_hash'])" }
if ($ShipEnd -notmatch '"reason":"bot_timeout"') { Note-Fail "E ends: $ShipEnd" }
Write-Host ("    E Shipping input_hash {0}, steps {1}, csv same as restarted Development={2}" -f $Shipping["input_hash"], $Shipping["steps"], $BridgeOk)

Get-ChildItem $ShipRuns -Filter "${SettingsSeed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force
Invoke-Game $ShipExe "f" @("-DTSeed=$SettingsSeed", "-DTOfferAutoPick", "-DTBot=Steered", "-DTBotMaxSeconds=15", "-DTBotQuit", "-DTSettingsFile=`"$SettingsFile`"")
$ShipCulture = Get-Culture (Get-RunFile $ShipRuns $SettingsSeed "jsonl")
if ($ShipCulture -ne "zh-Hans") { Note-Fail "F Shipping reads culture '$ShipCulture' from the settings file" }
Write-Host "    F Shipping culture $ShipCulture from the settings file"

Remove-Runs

$Reloads = @($B.ReloadMs, $C.ReloadMs, $D.ReloadMs) | Where-Object { $_ -ge 0 }
if ($Reloads.Count -ne 3) { Note-Fail "only $($Reloads.Count) of 3 reloads timed" }
$Summary = "same cook={0}, hash {1}, csv same={2}, shipping bridge={3}, reloads ms {4}, LoadMap s first {5} reload {6}" -f $SameCook, $Restarted["input_hash"], $CsvOk, $BridgeOk, (($Reloads | ForEach-Object { "{0:n0}" -f $_ }) -join " "), $B.ColdS, $B.ReloadS
if ($script:Failures.Count) {
	Write-Host "PKGRESTART: FAIL ($($script:Failures.Count) failure(s), first: $($script:Failures[0]); $Summary)"
	exit 1
}
Write-Host "PKGRESTART: PASS ($Summary)"
exit 0
