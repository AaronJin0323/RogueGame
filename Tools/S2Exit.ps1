# S2 exit battery (12_Production §2 S2 row, Docs/Validation/S2_exit.md): one headless bot battery across all six Keepers, then Tools/s2_exit.py.
#   & Tools\S2Exit.ps1 [-Out Saved\BotRuns\s2_exit] [-Seeds 15] [-Skills 1.0,0.6] [-Parallel 12] [-Stagger 0.4] [-Builds 5] [-Unlock S2] [-ReadOnly]
# Run by hand. The S2 row says nightly: nothing schedules this.
# Steps: the 18 showcase arms at each skill on bot v2 (<Out>\bots_<skill>), T-KEE-18 on the first skill's L16 builds up to seed -Builds
# (<Out>\ab), T-KEE-15 on every Keeper (<Out>\ik), then the reader. -ReadOnly only reads. -Unlock "" reads the starter pool.
# Every launch goes through Tools/BotRuns.ps1: -game -nullrhi -nosplash, hidden, killed by PID.
# Prints the reader's EXIT lines and "S2EXIT: ..."; exit 0 when it ran, whatever the verdicts, 1 on a script error.
param(
	[string]$Out = "",
	[int]$Seeds = 15,
	[string]$Skills = "1.0,0.6",
	[int]$Parallel = 12,
	[double]$Stagger = 0.4,
	[int]$Builds = 5,
	[string]$Unlock = "S2",
	[switch]$ReadOnly
)

trap { Write-Host "S2EXIT: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $Out) { $Out = Join-Path $Root "Saved\BotRuns\s2_exit" }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$BotRuns = Join-Path $Root "Tools\BotRuns.ps1"
$Probe = Join-Path $Root "Tools\probe.py"
# The labels Tools/probe.py reads as its 18 arms (SHOWCASE, SHOWCASE_S2).
$Arms = "Steered:Storm:Tempest,Steered:Storm:ArcGrid,Steered:Storm:Downpour,Steered:Forge:Hammer,Steered:Forge:Furnace,Steered:Forge:Slam," +
	"Steered:Tide:Frost,Steered:Tide:Stillness,Steered:Tide:Whirlpool"
$SkillList = @($Skills -split ',' | Where-Object { $_ })
$UnlockArgs = if ($Unlock) { @("-Unlock", $Unlock) } else { @() }

# A child process: BotRuns ends with exit, and a run that FAILs its check still leaves readings.
function Invoke-BotRuns([string[]]$BotArgs) {
	& powershell -NoProfile -ExecutionPolicy Bypass -File $BotRuns @BotArgs @UnlockArgs -Parallel $Parallel -Stagger $Stagger | Select-String "^BOTRUNS:" | ForEach-Object { $_.Line }
}

if (-not $ReadOnly) {
	foreach ($Skill in $SkillList) {
		Invoke-BotRuns @("-Arms", $Arms, "-ArmSet", "S2Keepers", "-Seeds", "$Seeds", "-BotVersion", "2", "-Skill", $Skill, "-Out", (Join-Path $Out "bots_$Skill"))
	}
	$Ab = Join-Path $Out "ab"
	$Ik = Join-Path $Out "ik"
	& python -I $Probe ablation $Ab --builds (Join-Path $Out ("bots_" + $SkillList[0])) --stage all --max-build $Builds
	Invoke-BotRuns @("-Jobs", (Join-Path $Ab "jobs.csv"), "-Out", (Join-Path $Ab "runs"))
	& python -I $Probe intercession $Ik --seeds 2 --stage all
	Invoke-BotRuns @("-Jobs", (Join-Path $Ik "jobs.csv"), "-Out", (Join-Path $Ik "runs"))
}

& python -I (Join-Path $Root "Tools\s2_exit.py") $Out
exit $LASTEXITCODE
