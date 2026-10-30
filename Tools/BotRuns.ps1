# Bot v1 run harness (S1 tool; Docs/Validation/S1_bot_baseline.md): seeded headless runs per arm at accelerated time.
#   & Tools\BotRuns.ps1 [-Arms None,Steered:Storm:Tempest,...] [-Seeds 10] [-FirstSeed 1] [-Skill 1] [-MaxSeconds 1100]
#                       [-Steps 600] [-Timeout 900] [-Check T-DIR-01[,T-OFF-15]] [-Out <dir>]
# Arms: None | Random | Steered:<Path>:<Archetype>[:<SecondPath>]. Each run launches the game headless
# (UnrealEditor-Cmd -game -nullrhi -unattended) with -DTSeed=<seed> -DTBot=... -DTBotFast=<Steps> -DTBotQuit, so the bot
# drives player 0, answers every hand and quits when the run is over (death, boss victory or -MaxSeconds of sim time).
# A run still alive after -Timeout wall seconds is killed by its own PID only.
# Per run, Saved/BotRuns/<stamp>/<arm>_s<seed>/ gets the bot record (bot.csv + bot.csv.txt), the run's telemetry pair
# copied from Saved/Playtests and the log. Then Tools/bot_summary.py writes summary.csv + summary.md and runs -Check.
# -Check T-DIR-01 alone defaults to the no-pick arm with C.DIR.BOT_RUNS seeds.
# Prints "BOTRUNS: PASS|FAIL (...)"; exit 0 / 1 (a failed check or a run with no record fails).
param(
	[string[]]$Arms = @(),
	[int]$Seeds = 0,
	[int]$FirstSeed = 1,
	[double]$Skill = 1.0,
	[int]$MaxSeconds = 1100,
	[int]$Steps = 600,
	[int]$Timeout = 900,
	[string]$Check = "",
	[string]$Out = "",
	[string]$Map = "/Game/DT/Maps/L_Arena_Test"
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
Write-Host ("botruns: arms {0}, seeds {1}..{2}, skill {3}, {4} steps/frame -> {5}" -f ($Arms -join ' '), $FirstSeed, ($FirstSeed + $Seeds - 1), $Skill, $Steps, $Out)

$Missing = 0
foreach ($Arm in $Arms) {
	$Parts = $Arm -split ':'
	$Policy = $Parts[0]
	$Label = ($Parts -join '-')
	foreach ($Seed in $FirstSeed..($FirstSeed + $Seeds - 1)) {
		$RunDir = Join-Path $Out ("{0}_s{1}" -f $Label, $Seed)
		New-Item -ItemType Directory -Force $RunDir | Out-Null
		$Log = Join-Path $RunDir "run.log"
		$Record = Join-Path $RunDir "bot.csv"
		$ArgList = @("`"$Project`"", $Map, "-game", "-nullrhi", "-unattended", "-nopause", "-nosound", "-LiveCoding=false",
			"-DTSeed=$Seed", "-DTBot=$Policy", "-DTBotFast=$Steps", "-DTBotQuit", "-DTBotMaxSeconds=$MaxSeconds",
			("-DTBotSkill={0}" -f $Skill.ToString([Globalization.CultureInfo]::InvariantCulture)), "-DTBotOut=`"$Record`"", "-ABSLOG=`"$Log`"")
		if ($Parts.Count -gt 1) { $ArgList += "-DTBotPath=$($Parts[1])" }
		if ($Parts.Count -gt 2) { $ArgList += "-DTBotArchetype=$($Parts[2])" }
		if ($Parts.Count -gt 3) { $ArgList += "-DTBotSecond=$($Parts[3])" }
		$Started = Get-Date
		$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden
		if (-not $Proc.WaitForExit($Timeout * 1000)) {
			& taskkill /PID $Proc.Id /T /F | Out-Null
			Write-Host ("  {0} seed {1}: killed after {2} s (PID {3})" -f $Label, $Seed, $Timeout, $Proc.Id)
		}
		# The telemetry pair named in the log ("Telemetry: session seed=<n> players=1 -> <path>.csv").
		$Session = Select-String -Path $Log -Pattern "Telemetry: session seed=\S+ players=\d+ -> (.+)\.csv" -ErrorAction SilentlyContinue | Select-Object -Last 1
		if ($Session) {
			$Base = $Session.Matches[0].Groups[1].Value
			foreach ($Ext in @(".csv", ".jsonl")) { if (Test-Path "$Base$Ext") { Copy-Item "$Base$Ext" $RunDir } }
		}
		$Kv = @{}
		if (Test-Path "$Record.txt") { Get-Content "$Record.txt" | ForEach-Object { $k, $v = $_ -split '=', 2; $Kv[$k] = $v } }
		else { $Missing++ }
		Write-Host ("  {0} seed {1}: {2} at {3} s, level@630 {4}, wall {5:N0} s" -f $Label, $Seed, $Kv["outcome"], $Kv["end_s"], $Kv["level_630"], ((Get-Date) - $Started).TotalSeconds)
	}
}

$SumArgs = @((Join-Path $Root "Tools\bot_summary.py"), $Out)
if ($Check) { $SumArgs += @("--check", $Check) }
$Summary = & python -I @SumArgs 2>&1
$Rc = $LASTEXITCODE
$Summary | ForEach-Object { "  $_" }
$Verdicts = @($Summary | Select-String "^T-[A-Z]+-\d+:" | ForEach-Object { $_.Line })
if ($Rc -ne 0 -or $Missing -gt 0) {
	Write-Host ("BOTRUNS: FAIL ({0}{1}; {2})" -f ($Verdicts -join '; '), $(if ($Missing) { "; $Missing run(s) without a bot record" } else { "" }), $Out)
	exit 1
}
Write-Host ("BOTRUNS: PASS ({0}; {1})" -f $(if ($Verdicts) { $Verdicts -join '; ' } else { "no check requested" }), $Out)
exit 0
