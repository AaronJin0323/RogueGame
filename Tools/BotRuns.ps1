# Bot run harness (Docs/02_Systems/bot.md; Docs/Validation/S1_bot_baseline.md): seeded headless runs per arm at accelerated time.
#   & Tools\BotRuns.ps1 [-Arms None,Steered:Storm:Tempest,...] [-Seeds 10] [-FirstSeed 1] [-BotVersion 1] [-Skill 1] [-MaxSeconds 1100]
#                       [-Steps 600] [-Timeout 900] [-Check T-DIR-01[,T-OFF-15]] [-Out <dir>] [-Parallel 1]
#                       [-Jobs <jobs.csv>] [-Stagger 1.5]
# -BotVersion 1 is the S1 bot and stays the default, so every S1 command reads as it did (R-BOT-01); 2 is bot v2.
# Arms: None | Random | Steered:<Path>:<Archetype>[:<SecondPath>]. Each run launches the game headless
# (UnrealEditor-Cmd -game -nullrhi -unattended) with -DTSeed=<seed> -DTBot=... -DTBotFast=<Steps> -DTBotQuit, so the bot
# drives player 0, answers every hand and quits when the run is over (death, boss victory or -MaxSeconds of sim time).
# A run still alive after -Timeout wall seconds is killed by its own PID only. -Parallel runs that many processes at once
# (the sim is deterministic per seed, so parallel runs equal serial ones); starts are spaced so no two telemetry names collide.
# Per run, Saved/BotRuns/<stamp>/<arm>_s<seed>/ gets the bot record (bot.csv + bot.csv.txt), the run's telemetry pair
# copied from Saved/Playtests and the log. Then Tools/bot_summary.py writes summary.csv + summary.md and runs -Check.
# -Check T-DIR-01 alone defaults to the no-pick arm with C.DIR.BOT_RUNS seeds.
# Prints "BOTRUNS: PASS|FAIL (...)"; exit 0 / 1 (a failed check or a run with no record fails).
# -Jobs runs the probe scenes of Tools/probe.py instead of arms (label,seed,script per row): the director stays off and each
# run folder gets the scene as scene.txt; -Stagger is the wall time between starts, which a job list's distinct seeds let shrink.
param(
	[string[]]$Arms = @(),
	[int]$Seeds = 0,
	[int]$FirstSeed = 1,
	[int]$BotVersion = 1,
	[double]$Skill = 1.0,
	[int]$MaxSeconds = 1100,
	[int]$Steps = 600,
	[int]$Timeout = 900,
	[string]$Check = "",
	[string]$Out = "",
	[int]$Parallel = 1,
	[string]$Map = "/Game/DT/Maps/L_Arena_Test",
	[string]$Jobs = "",
	[double]$Stagger = 1.5
)

trap { Write-Host "BOTRUNS: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$Playtests = Join-Path $Root "Saved\Playtests"

function Get-Constant([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*([-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return [double]$Line.Matches[0].Groups[1].Value
}

$Arms = @($Arms | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if ($Arms.Count -eq 0) { $Arms = @("None") }
if ($Seeds -le 0) { $Seeds = if ($Check -match "T-DIR-01") { [int](Get-Constant "DIR.BOT_RUNS") } else { 1 } }
if (-not $Out) { $Out = Join-Path $Root ("Saved\BotRuns\" + (Get-Date -Format "yyyyMMdd_HHmmss")) }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path # the game resolves -ABSLOG / -DTBotOut against its own working directory, not this shell's
Write-Host ("botruns: arms {0}, seeds {1}..{2}, version {3}, skill {4}, {5} steps/frame -> {6}" -f ($Arms -join ' '), $FirstSeed, ($FirstSeed + $Seeds - 1), $BotVersion, $Skill, $Steps, $Out)

$Missing = 0
function Finish-Run($Run) {
	# The telemetry pair named in the log ("Telemetry: session seed=<n> players=1 -> <path>.csv").
	$Session = Select-String -Path $Run.Log -Pattern "Telemetry: session seed=\S+ players=\d+ -> (.+)\.csv" -ErrorAction SilentlyContinue | Select-Object -Last 1
	if ($Session) {
		$Base = $Session.Matches[0].Groups[1].Value
		foreach ($Ext in @(".csv", ".jsonl")) { if (Test-Path "$Base$Ext") { Copy-Item "$Base$Ext" $Run.Dir } }
	}
	$Kv = @{}
	if (Test-Path "$($Run.Record).txt") { Get-Content "$($Run.Record).txt" | ForEach-Object { $k, $v = $_ -split '=', 2; $Kv[$k] = $v } }
	else { $script:Missing++ }
	Write-Host ("  {0} seed {1}: {2} at {3} s, level@630 {4}, wall {5:N0} s" -f $Run.Label, $Run.Seed, $Kv["outcome"], $Kv["end_s"], $Kv["level_630"], ((Get-Date) - $Run.Started).TotalSeconds)
}

function Reap-Runs([int]$Wanted) {
	# Waits until fewer than $Wanted runs are active.
	while ($true) {
		$Still = @()
		foreach ($Run in $script:Active) {
			if ($Run.Proc.HasExited) { Finish-Run $Run }
			elseif (((Get-Date) - $Run.Started).TotalSeconds -gt $Timeout) {
				& taskkill /PID $Run.Proc.Id /T /F | Out-Null
				Write-Host ("  {0} seed {1}: killed after {2} s (PID {3})" -f $Run.Label, $Run.Seed, $Timeout, $Run.Proc.Id)
				Finish-Run $Run
			}
			else { $Still += $Run }
		}
		$script:Active = $Still
		if ($Still.Count -lt $Wanted) { return }
		Start-Sleep -Milliseconds 200
	}
}

$script:Active = @()
function Start-Run($Label, $Seed, $Extra) {
	$RunDir = Join-Path $Out ("{0}_s{1}" -f $Label, $Seed)
	New-Item -ItemType Directory -Force $RunDir | Out-Null
	$Log = Join-Path $RunDir "run.log"
	$Record = Join-Path $RunDir "bot.csv"
	$ArgList = @("`"$Project`"", $Map, "-game", "-nullrhi", "-unattended", "-nopause", "-nosound", "-LiveCoding=false",
		"-DTSeed=$Seed", "-DTBotFast=$Steps", "-DTBotQuit", "-DTBotVersion=$BotVersion", ("-DTBotSkill={0}" -f $Skill.ToString([Globalization.CultureInfo]::InvariantCulture)),
		"-DTBotOut=`"$Record`"", "-ABSLOG=`"$Log`"") + $Extra
	Reap-Runs ([Math]::Max(1, $Parallel))
	$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden
	$script:Active += [pscustomobject]@{ Proc = $Proc; Label = $Label; Seed = $Seed; Dir = $RunDir; Log = $Log; Record = $Record; Started = (Get-Date) }
	if ($Parallel -gt 1) { Start-Sleep -Milliseconds ([int]($Stagger * 1000)) }
	return $RunDir
}

if ($Jobs) {
	foreach ($Job in (Import-Csv $Jobs)) {
		$RunDir = Start-Run $Job.label $Job.seed @("-DTBot=Steered", "-DTNoDirector", "-DTProbe=`"$($Job.script)`"")
		Copy-Item $Job.script (Join-Path $RunDir "scene.txt")
	}
	Reap-Runs 1
	$Summary = & python -I (Join-Path $Root "Tools\probe.py") read $Out 2>&1
	$Summary | Select-String "^T-KEE" | ForEach-Object { "  " + $_.Line }
	Write-Host ("BOTRUNS: {0} ({1} run(s) without a bot record; {2})" -f $(if ($Missing) { "FAIL" } else { "PASS" }), $Missing, $Out)
	exit $(if ($Missing) { 1 } else { 0 })
}

foreach ($Arm in $Arms) {
	$Parts = $Arm -split ':'
	$Policy = $Parts[0]
	$Label = ($Parts -join '-')
	foreach ($Seed in $FirstSeed..($FirstSeed + $Seeds - 1)) {
		$Extra = @("-DTBot=$Policy", "-DTBotMaxSeconds=$MaxSeconds")
		if ($Parts.Count -gt 1) { $Extra += "-DTBotPath=$($Parts[1])" }
		if ($Parts.Count -gt 2) { $Extra += "-DTBotArchetype=$($Parts[2])" }
		if ($Parts.Count -gt 3) { $Extra += "-DTBotSecond=$($Parts[3])" }
		$null = Start-Run $Label $Seed $Extra
	}
}
Reap-Runs 1

$SumArgs = @((Join-Path $Root "Tools\bot_summary.py"), $Out)
# PowerShell joins an unquoted comma list into one space-separated string.
if ($Check) { $SumArgs += @("--check", ($Check -split '[,\s]+' -join ',')) }
$Summary = & python -I @SumArgs 2>&1
$Rc = $LASTEXITCODE
$Summary | ForEach-Object { "  $_" }
$Verdicts = @($Summary | Select-String "^T-[A-Z]+-\d+:" | ForEach-Object { $_.Line })
if ($Check -and $Verdicts.Count -eq 0) {
	Write-Host "BOTRUNS: FAIL (-Check $Check produced no verdict; $Out)"
	exit 1
}
if ($Rc -ne 0 -or $Missing -gt 0) {
	Write-Host ("BOTRUNS: FAIL ({0}{1}; {2})" -f ($Verdicts -join '; '), $(if ($Missing) { "; $Missing run(s) without a bot record" } else { "" }), $Out)
	exit 1
}
Write-Host ("BOTRUNS: PASS ({0}; {1})" -f $(if ($Verdicts) { $Verdicts -join '; ' } else { "no check requested" }), $Out)
exit 0
