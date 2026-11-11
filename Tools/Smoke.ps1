# Smoke test (gate step `smoke`, 11_Quality s1): a rendered run on the stress arena, judged from its log and a screenshot.
#   & Tools\Smoke.ps1 [-Culture en|zh-Hans] [-Cmds "DT.Spawn.Keep 400,DT.Bullets 6000"] [-Seconds <s>] [-Name arena_empty]
#                     [-Map /Game/DT/Maps/L_Arena_Test] [-Director] [-Update] [-Contrast] [-DormantLine]
# Launches the game (UnrealEditor -game -RenderOffScreen at 1280x720, on the hidden desktop of Tools/HiddenLaunch.ps1) in the given culture, waits for "Run start", runs -Cmds as
# -ExecCmds, takes a screenshot -Seconds of sim time after run start (default C.SMOKE.SECONDS; ADTGameMode -DTShot hook: the sim
# is held on that step and shot once the still frame has settled, so neither a hitch nor the frame rate moves what it shows), and the
# game quits itself once the file is written; a game still alive after the timeout is killed by its own PID only.
# The run director is off (-DTNoDirector, director.md) so the goldens stay deterministic; -Director turns it on (pair it
# with a -Name that has no golden, e.g. -Director -Seconds 60 -Name arena_director: the image check then fails by design).
# Verdict: no crash, no ensure, no Error/Fatal line from LogDT, "Run start" seen, the drawn Test.LocRoundTrip equals
# ST_UI.csv for the culture, the drawn Dash and Intercession labels carry a key (R-CMB-13), screenshot present, and
# Tools/image_check.py PASS against Docs/Validation/goldens/<Name>_<Culture>.png (-Update re-baselines the golden).
# -Contrast judges the shot with Tools/contrast_check.py (T-PRE-05) instead of a golden; the hero is read from the arena_empty
# shot of the same culture, where nothing stands on it. -DormantLine judges it with contrast_check.py --dormant-line instead:
# the HUD's dormant line on its panel, as rendered (R-MOD-08); pass -Cmds that stage one.
# -Burst is the rest of T-PRE-07: first Tools/render_settings_check.py reads the arena's post-process volume in a headless commandlet (RENDER: line),
# then the shot of -Cmds (a burst of fire) is judged by Tools/image_check.py --exposure against the arena_empty shot of the same culture: no exposure change.
# Before the launch, T-PRE-07's config half: the render settings 04_Presentation s4 lists hold their values in Config/DefaultEngine.ini.
# The game's process mask is sampled about every 0.2 s for its whole life and must stay inside the machine's mask (D-052).
# Prints "SMOKE: PASS (...)" or "SMOKE: FAIL (...)"; exit 0 / 1. Log: Saved\Logs\Smoke_<Name>_<Culture>.log.
param(
	[ValidateSet("en", "zh-Hans")]
	[string]$Culture = "en",
	[string]$Cmds = "",
	[double]$Seconds = -1,
	[string]$Name = "arena_empty",
	[string]$Map = "/Game/DT/Maps/L_Arena_Test",
	[int]$LoadTimeout = 900,  # the first launch after render changes compiles shaders
	[switch]$Director,
	[switch]$Update,
	[switch]$Contrast,
	[switch]$DormantLine,
	[switch]$Burst
)

trap { Write-Host "SMOKE: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "Smoke"  # D-055 item 7: one heavy-job slot for this run, none for a step of a run that holds one

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$ShotDir = Join-Path $Root "Saved\Smoke"
New-Item -ItemType Directory -Force $LogDir, $ShotDir | Out-Null
$Tag = "${Name}_${Culture}"
$Log = Join-Path $LogDir "Smoke_$Tag.log"
$Shot = Join-Path $ShotDir "$Tag.png"
$Golden = Join-Path $Root "Docs\Validation\goldens\$Tag.png"

function Get-Constant([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*([-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return [double]$Line.Matches[0].Groups[1].Value
}
if ($Seconds -lt 0) { $Seconds = Get-Constant "SMOKE.SECONDS" }

# --- T-PRE-07, the config half (04_Presentation R-PRE-07): each setting 04_Presentation s4 lists as set in Config/DefaultEngine.ini holds the value it names.
# The table is the source; its words read as config values. The arena post-process volume half and the burst frame are not built (Validation/S1_gate_main.md).
$Words = @{ "lumen" = @("1"); "tsr" = @("4"); "true" = @("true", "1"); "on" = @("true", "1"); "false" = @("false", "0"); "off" = @("false", "0") }
$Ini = @{}
foreach ($L in Get-Content (Join-Path $Root "Config\DefaultEngine.ini")) { if ($L -match '^\s*([A-Za-z][\w.]*)\s*=\s*(.*?)\s*$') { $Ini[$Matches[1]] = $Matches[2] } }
$Section = @((Get-Content (Join-Path $Root "Docs\04_Presentation.md") -Raw -Encoding UTF8) -split "(?m)^## " | Where-Object { $_ -match "^4\. Render settings" })[0]
$Settings = @()
$Pre07 = @()
foreach ($Row in @("$Section" -split "`n" | Where-Object { $_ -match '^\|\s*`r\.' })) {
	$Cells = @($Row.Trim().Trim("|").Split("|") | ForEach-Object { $_.Trim() })
	$Want = $Words[$Cells[1].ToLowerInvariant()]
	foreach ($M in [regex]::Matches($Cells[0], '`([^`]+)`')) {
		$Key = $M.Groups[1].Value
		$Settings += $Key
		if (-not $Want) { $Pre07 += "$Key ('$($Cells[1])' has no config reading)" }
		elseif (-not $Ini.ContainsKey($Key)) { $Pre07 += "$Key missing" }
		elseif ($Want -notcontains $Ini[$Key].ToLowerInvariant()) { $Pre07 += "$Key=$($Ini[$Key]), 04_Presentation s4 says $($Cells[1])" }
	}
}
if ($Settings.Count -eq 0) { $Pre07 += "no setting read from 04_Presentation s4" }
if ($Pre07.Count -gt 0) { $Pre07 | ForEach-Object { "  ! T-PRE-07 $_" }; Write-Host ("SMOKE: FAIL (T-PRE-07: {0} of {1} render setting(s) wrong in Config/DefaultEngine.ini)" -f $Pre07.Count, $Settings.Count); exit 1 }
Write-Host ("smoke: T-PRE-07 config half, {0} render settings as 04_Presentation s4 says" -f $Settings.Count)

# --- T-PRE-07, the volume half (-Burst): the arena's post-process volume, read by the editor in a commandlet that saves nothing -------------
$Render = ""
if ($Burst) {
	$CmdExe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
	$RenderLog = Join-Path $LogDir "Smoke_render_settings.log"
	if (Test-Path $RenderLog) { Remove-Item $RenderLog -Force }
	$RenderProc = Start-DTHidden $CmdExe @("`"$Project`"", "-run=pythonscript", "-script=`"$(Join-Path $Root "Tools\render_settings_check.py")`"", "-unattended", "-nopause",
		"-nullrhi", "-nosplash", "-LiveCoding=false", "-ABSLOG=`"$RenderLog`"")
	if (-not $RenderProc.WaitForExit($LoadTimeout * 1000)) { Stop-DTHidden $RenderProc }
	$RenderLines = @(if (Test-Path $RenderLog) { Get-Content $RenderLog -Encoding UTF8 } else { @() })
	$RenderLines | Where-Object { $_ -match "render_settings: " } | ForEach-Object { "    " + ($_ -replace "^.*render_settings: ", "render_settings: ") }
	$Render = (($RenderLines | Where-Object { $_ -match "RENDER: (PASS|FAIL)" } | Select-Object -Last 1) -replace "^.*RENDER:", "RENDER:")
	if ($Render -notmatch "^RENDER: PASS") {
		Write-Host ("SMOKE: FAIL (T-PRE-07 volume: {0}; log {1})" -f $(if ($Render) { $Render } else { "no RENDER line" }), $RenderLog)
		exit 1
	}
}

# --- text: generated string table + compiled .locres must be current (R-TEC-08) --------------------------------------
$LocOut = & python -I (Join-Path $Root "Tools\loc_build.py") --build 2>&1
if ($LASTEXITCODE -ne 0) { $LocOut | ForEach-Object { "    $_" }; Write-Host "SMOKE: FAIL (loc_build)"; exit 1 }
$Po = Join-Path $Root "Content\Localization\Game\zh-Hans\Game.po"
$Locres = Join-Path $Root "Content\Localization\Game\zh-Hans\Game.locres"
if (-not (Test-Path $Locres) -or (Get-Item $Locres).LastWriteTime -lt (Get-Item $Po).LastWriteTime) {
	Write-Host "smoke: compiling .locres (Tools\Localize.bat)"
	$LocOut = & cmd /c "`"$Root\Tools\Localize.bat`" 2>&1"
	if ($LASTEXITCODE -ne 0) { $LocOut | Select-Object -Last 10 | ForEach-Object { "    $_" }; Write-Host "SMOKE: FAIL (Localize.bat)"; exit 1 }
}

# --- run ----------------------------------------------------------------------------------------------------------
if (Test-Path $Log) { Remove-Item $Log -Force }
if (Test-Path $Shot) { Remove-Item $Shot -Force }
$ArgList = @("`"$Project`"", $Map, "-game", "-windowed", "-RenderOffScreen", "-ResX=1280", "-ResY=720", "-LiveCoding=false", "-nosplash",
	"-unattended", "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", ("-DTShotAfter={0}" -f $Seconds))
if (-not $Director) { $ArgList += "-DTNoDirector" }
if ($Cmds) { $ArgList += "-ExecCmds=`"$Cmds`"" }
$Proc = Start-DTHidden $Exe $ArgList
Write-Host ("smoke: {0} culture={1} cmds='{2}' shot after {3} s (PID {4})" -f $Map, $Culture, $Cmds, $Seconds, $Proc.Id)

function Read-Log { if (Test-Path $Log) { Get-Content $Log -Raw -Encoding UTF8 -ErrorAction SilentlyContinue } else { "" } }

# D-052: the game's process mask is sampled about every 0.2 s for its whole life; one sample outside the machine's mask fails the run
# (Tools/HiddenLaunch.ps1 holds the launch in a job whose affinity limit is that mask).
$MaskBits = if ($DTMask -and $DTMask -ne "none") { [Convert]::ToUInt64($DTMask, 16) } else { [UInt64]0 }
$Samples = 0
$Outside = @()
function Wait-Sampled([double]$Seconds) {
	$Until = (Get-Date).AddSeconds($Seconds)
	do {
		if ($script:MaskBits -ne 0 -and -not $Proc.HasExited) {
			try {
				$Got = [UInt64][Int64]$Proc.ProcessorAffinity
				$script:Samples++
				if (($Got -band (-bnot $script:MaskBits)) -ne 0) { $script:Outside += ("{0:X}" -f $Got) }
			}
			catch { }  # the process ended between the check and the read
		}
		Start-Sleep -Milliseconds 200
	} while ((Get-Date) -lt $Until -and -not $Proc.HasExited)
}

$Killed = $false
try {
	$Deadline = (Get-Date).AddSeconds($LoadTimeout)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline -and (Read-Log) -notmatch "LogDT: Run start") { Wait-Sampled 1 }
	# After the run starts the game shoots and quits on its own; allow the shot delay plus a margin for slow frames.
	$Deadline = (Get-Date).AddSeconds($Seconds + 120)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Wait-Sampled 1 }
}
finally {
	if (-not $Proc.HasExited) {
		Stop-DTHidden $Proc
		$Killed = $true
	}
}

# --- verdict ------------------------------------------------------------------------------------------------------
$Text = Read-Log
$Lines = $Text -split "`r?`n"
$Started = $Text -match "LogDT: Run start"
$Crash = @($Lines | Where-Object { $_ -match "Fatal error|=== Critical error:? ===|Unhandled Exception|appError called" })
$Ensure = @($Lines | Where-Object { $_ -match "Ensure condition failed|LogOutputDevice: Error: Ensure" })
$DTErr = @($Lines | Where-Object { $_ -match "LogDT: (Error|Fatal)" })
$OtherErr = @($Lines | Where-Object { $_ -match "Error:" -and $_ -notmatch "LogDT:" })

$Lines | Where-Object { $_ -match "LogLoad: LoadMap|LogDT:" } | Select-Object -First 20 | ForEach-Object { "    $_" }
if ($OtherErr.Count) { Write-Host "    ($($OtherErr.Count) engine Error line(s), not judged; first:)"; $OtherErr | Select-Object -First 5 | ForEach-Object { "      $_" } }
$Crash + $Ensure + $DTErr | Select-Object -First 15 | ForEach-Object { "  ! $_" }

# Localisation round trip (12_Production §2 S0 exit): the HUD logs Test.LocRoundTrip once as drawn; it must equal the
# source CSV's column for this culture. Pixel goldens cannot tell two cultures apart, the log line can.
$Expected = (Import-Csv -Path (Join-Path $Root "Content\DT\Text\ST_UI.csv") -Encoding UTF8 | Where-Object { $_.Key -eq "Test.LocRoundTrip" }).$Culture
$Drawn = ($Lines | Select-String -Pattern "LogDT: LocRoundTrip culture=(\S+) text=(.*)$" | Select-Object -First 1)
$LocOk = $false
$Loc = "loc: no LocRoundTrip line"
if ($Drawn) {
	$DrawnCulture = $Drawn.Matches[0].Groups[1].Value
	$DrawnText = $Drawn.Matches[0].Groups[2].Value.Trim()
	$LocOk = ($DrawnCulture -eq $Culture) -and ($DrawnText -ceq $Expected)
	$Loc = if ($LocOk) { "loc ok ($Culture)" } else { "loc MISMATCH (culture '$DrawnCulture', drew '$DrawnText', expected '$Expected')" }
}

# Key labels (combat_core.md R-CMB-13): the HUD logs player 0's Dash and Intercession lines once as drawn; each must be its
# ST_UI label for this culture, then a key in brackets. No input reaches a smoke run, so the keys are the keyboard's.
$UiRows = Import-Csv -Path (Join-Path $Root "Content\DT\Text\ST_UI.csv") -Encoding UTF8
function Get-UiText([string]$Key) { return ($UiRows | Where-Object { $_.Key -eq $Key }).$Culture }
$KeyLine = ($Lines | Select-String -Pattern "LogDT: HudKeys culture=(\S+) dash=(.*) \| intercession=(.*)$" | Select-Object -First 1)
$KeysOk = $false
$Keys = "keys: no HudKeys line"
if ($KeyLine) {
	$DrawnDash = $KeyLine.Matches[0].Groups[2].Value.Trim()
	$DrawnMeter = $KeyLine.Matches[0].Groups[3].Value.Trim()
	$Bracket = "  \[\S[^\]]*\]$"
	$DashOk = $DrawnDash -cmatch ("^" + [regex]::Escape((Get-UiText "Hud.Dash")) + $Bracket)
	$MeterOk = $DrawnMeter -cmatch ("^(" + [regex]::Escape((Get-UiText "Hud.Intercession")) + "|" + [regex]::Escape((Get-UiText "Hud.IntercessionReady")) + ")" + $Bracket)
	$KeysOk = ($KeyLine.Matches[0].Groups[1].Value -eq $Culture) -and $DashOk -and $MeterOk
	$Keys = if ($KeysOk) { "keys ok ($DrawnDash | $DrawnMeter)" } else { "keys MISMATCH (drew '$DrawnDash' and '$DrawnMeter')" }
}

# A judge's verdict counts once two runs on the same frame print the same lines and exit code. This host's CPU has been seen
# to fault inside the checker under load (a Python TypeError or access violation in pure code, and now and then a reading off
# by a few pixels with exit 0): a verdict one run gives alone is not trusted. Nothing about the frame or the rule changes.
function Invoke-Judge([string[]]$JudgeArgs, [int]$MaxRuns = 6) {
	$Seen = @{}
	$Last = $null
	for ($Run = 1; $Run -le $MaxRuns; $Run++) {
		$Lines = @(& python -I @JudgeArgs 2>&1 | ForEach-Object { "$_" })
		$Rc = $LASTEXITCODE
		$Key = "$Rc`n" + ($Lines -join "`n")
		$Last = @{ Out = $Lines; Rc = $Rc; Runs = $Run }
		if ($Seen.ContainsKey($Key)) { return $Last }
		$Seen[$Key] = $true
	}
	Write-Host "    ! judge gave no two equal verdicts in $MaxRuns runs; the last is reported, as a failure"
	$Last.Rc = 1
	return $Last
}

$Image = "no screenshot"
$ImageOk = $false
if (Test-Path $Shot) {
	if ($Contrast) {
		$Judged = Invoke-Judge @((Join-Path $Root "Tools\contrast_check.py"), $Shot, "--hero", (Join-Path $ShotDir "arena_empty_$Culture.png"),
			"--known", (Join-Path $Root "Docs\Validation\contrast_debts.txt"))
		$ImageOk = ($Judged.Rc -eq 0)
		$Judged.Out | ForEach-Object { "    $_" }
		$Image = (($Judged.Out | Select-String "^CONTRAST:" | Select-Object -Last 1).Line)
	}
	elseif ($Burst) {
		$Judged = Invoke-Judge @((Join-Path $Root "Tools\image_check.py"), $Shot, "--exposure", (Join-Path $ShotDir "arena_empty_$Culture.png"))
		$ImageOk = ($Judged.Rc -eq 0)
		$Image = (($Judged.Out | Select-String "^IMAGE:" | Select-Object -Last 1).Line) -replace "^IMAGE:\s*", "image "
		if ($Render) { $Image = "$Render, $Image" }
	}
	elseif ($DormantLine) {
		$Judged = Invoke-Judge @((Join-Path $Root "Tools\contrast_check.py"), $Shot, "--dormant-line")
		$ImageOk = ($Judged.Rc -eq 0)
		$Judged.Out | ForEach-Object { "    $_" }
		$Image = (($Judged.Out | Select-String "^CONTRAST:" | Select-Object -Last 1).Line)
	}
	else {
		$CheckArgs = @((Join-Path $Root "Tools\image_check.py"), $Shot, "--golden", $Golden)
		if ($Update) { $CheckArgs += "--update" }
		$Judged = Invoke-Judge $CheckArgs
		$ImageOk = ($Judged.Rc -eq 0)
		$Image = (($Judged.Out | Select-String "^IMAGE:" | Select-Object -Last 1).Line) -replace "^IMAGE:\s*", "image "
	}
	if ($Judged.Runs -gt 2) { Write-Host "    judge: $($Judged.Runs) runs for two equal verdicts" }
}
Write-Host "    log: $Log"
Write-Host "    shot: $Shot"

$Affinity = if ($MaskBits -eq 0) { "mask none" } else { "mask $DTMask, {0} sample(s), {1} outside" -f $Samples, $Outside.Count }
if ($Outside.Count) { $Outside | Sort-Object -Unique | ForEach-Object { "  ! the game's process mask read $_, outside $DTMask" } }
$MaskOk = ($MaskBits -eq 0) -or ($Samples -gt 0 -and $Outside.Count -eq 0)
$Summary = "{0}, run start={1}, crash={2}, ensure={3}, LogDT errors={4}, killed={5}, {6}, {7}, {8}, {9}" -f $Tag, $Started, $Crash.Count, $Ensure.Count, $DTErr.Count, $Killed, $Loc, $Keys, $Image, $Affinity
if ($Started -and $Crash.Count -eq 0 -and $Ensure.Count -eq 0 -and $DTErr.Count -eq 0 -and $LocOk -and $KeysOk -and $ImageOk -and $MaskOk) {
	Write-Host "SMOKE: PASS ($Summary)"
	exit 0
}
Write-Host "SMOKE: FAIL ($Summary)"
exit 1
