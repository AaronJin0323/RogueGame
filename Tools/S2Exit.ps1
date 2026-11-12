# S2 exit battery (12_Production §2 S2 row, Docs/Validation/S2_exit.md): one headless bot battery across all six Keepers, then Tools/s2_exit.py.
#   & Tools\S2Exit.ps1 [-Out Saved\BotRuns\s2_exit] [-Seeds 15] [-Skills 1.0,0.6] [-Parallel 12] [-Stagger 0.4] [-Builds 5] [-Unlock S2] [-ReadOnly] [-NoBand] [-BandSeeds Weapon.X=30,...]
# Run by hand. The S2 row says nightly: nothing schedules this.
# Steps: the 18 showcase arms at each skill on bot v2 (<Out>\bots_<skill>), T-KEE-18 on the first skill's L16 builds up to seed -Builds
# (<Out>\ab; judged by loss with share printed beside it, and the no-second-Keeper control scenes, D-047), T-KEE-15 on every Keeper (<Out>\ik), then the reader. -ReadOnly only reads. -Unlock "" reads the starter pool.
# -Unlock S2, the default, gives the arm batteries the S2 arsenal, the S1 non-starters and Reaction.Duos (BotRuns.ps1 Expand-Unlock), and the probe
# scenes (-Jobs) S2Arsenal, the same without Reaction.Duos: a scene's method changes only with a number-log row (10_Tuning section 3). Any other
# -Unlock goes to every call as given.
# The Reforging band's step (D-082, after the showcase arms): for each of the ten weapons with an evolution, three carry batteries on
# $BandArm at the first skill (<Out>\band\carry, \hold, \decline: the evolution, the Path card, no pick, R-BOT-18 and R-BOT-20), then
# Tools/probe.py's reforge stage (<Out>\rf) over the level 16 snapshots: the evolutions and Path cards from the carry arm's folders, the
# Path cards alone from the showcase arms' folders. -NoBand leaves the step out, so the older exit is reproduced; -ReadOnly reads <Out>\rf too.
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
	[switch]$ReadOnly,
	# D-082: leaves the Reforging band's step out (the exit before it).
	[switch]$NoBand,
	# D-082's ruling of 2026-11-13: seeds per card of the band's play arms, "Weapon.X=30,Weapon.Y=23"; a card not named runs -Seeds
	# (Docs/Validation/S2_evolve_read_decline.md: how many seeds give ten runs reaching the boss on each arm).
	[string]$BandSeeds = ""
)

trap { Write-Host "S2EXIT: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
if (-not $ReadOnly) { $DTHeavy = Enter-DTHeavy -What "S2Exit" }  # D-055 item 7: one slot for the whole battery (its -Parallel launches are this one job); -ReadOnly launches nothing

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
$JobUnlockArgs = if ($Unlock -eq "S2") { @("-Unlock", "S2Arsenal") } else { $UnlockArgs }
# D-082: the pair every carry battery of the band runs, and the ten weapons with an evolution (reforging.csv).
$BandArm = "Steered:Storm:Tempest:Tide"
$BandWeapons = @(Import-Csv (Join-Path $Root "Docs\09_Catalog\reforging.csv") | Where-Object { $_.kind -eq "Evolve" } | ForEach-Object { $_.source })
$Band = Join-Path $Out "band"
$Rf = Join-Path $Out "rf"

# A child process: BotRuns ends with exit, and a run that FAILs its check still leaves readings.
function Invoke-BotRuns([string[]]$BotArgs, [string[]]$PoolArgs) {
	& powershell -NoProfile -ExecutionPolicy Bypass -File $BotRuns @BotArgs @PoolArgs -Parallel $Parallel -Stagger $Stagger | Select-String "^BOTRUNS:" | ForEach-Object { $_.Line }
}

if (-not $ReadOnly) {
	foreach ($Skill in $SkillList) {
		Invoke-BotRuns @("-Arms", $Arms, "-ArmSet", "S2Keepers", "-Seeds", "$Seeds", "-BotVersion", "2", "-Skill", $Skill, "-Out", (Join-Path $Out "bots_$Skill")) $UnlockArgs
	}
	$Ab = Join-Path $Out "ab"
	$Ik = Join-Path $Out "ik"
	& python -I $Probe ablation $Ab --builds (Join-Path $Out ("bots_" + $SkillList[0])) --stage all --max-build $Builds
	Invoke-BotRuns @("-Jobs", (Join-Path $Ab "jobs.csv"), "-Out", (Join-Path $Ab "runs")) $JobUnlockArgs
	& python -I $Probe intercession $Ik --seeds 2 --stage all
	Invoke-BotRuns @("-Jobs", (Join-Path $Ik "jobs.csv"), "-Out", (Join-Path $Ik "runs")) $JobUnlockArgs
	if (-not $NoBand) {
		# One battery of each play arm per seed count: every card on -Seeds unless -BandSeeds names more for it.
		$PerCard = @{}
		foreach ($Pair in @($BandSeeds -split ',' | Where-Object { $_ })) { $Name, $Count = $Pair -split '='; $PerCard[$Name.Trim()] = [int]$Count }
		$ByCount = [ordered]@{}
		foreach ($Weapon in $BandWeapons) { $N = if ($PerCard.Contains($Weapon)) { $PerCard[$Weapon] } else { $Seeds }; if (-not $ByCount.Contains($N)) { $ByCount[$N] = @() }; $ByCount[$N] += $Weapon }
		foreach ($N in $ByCount.Keys) {
			$CarryArgs = @("-Arms", $BandArm, "-Carry", ($ByCount[$N] -join ','), "-Seeds", "$N", "-BotVersion", "2", "-Skill", $SkillList[0])
			Invoke-BotRuns ($CarryArgs + @("-Out", (Join-Path $Band "carry"))) $UnlockArgs
			Invoke-BotRuns ($CarryArgs + @("-CarryHold", "-Out", (Join-Path $Band "hold"))) $UnlockArgs
			Invoke-BotRuns ($CarryArgs + @("-DeclineReforge", "-Out", (Join-Path $Band "decline"))) $UnlockArgs
		}
		# D-085: the no-pick series first (<Out>\rf\series), then the fixed and far stages at the health it gives; none when it gives none.
		& python -I $Probe reforge-series $Rf --builds (Join-Path $Band "carry") --path-builds (Join-Path $Out ("bots_" + $SkillList[0]))
		Invoke-BotRuns @("-Jobs", (Join-Path $Rf "series\jobs.csv"), "-Out", (Join-Path $Rf "series\runs")) $JobUnlockArgs
		& python -I $Probe reforge $Rf --builds (Join-Path $Band "carry") --path-builds (Join-Path $Out ("bots_" + $SkillList[0])) --calibrated
		if ($LASTEXITCODE -eq 0) { Invoke-BotRuns @("-Jobs", (Join-Path $Rf "jobs.csv"), "-Out", (Join-Path $Rf "runs")) $JobUnlockArgs }
	}
}

& python -I (Join-Path $Root "Tools\s2_exit.py") $Out
$Rc = $LASTEXITCODE
if (-not $NoBand -and (Test-Path (Join-Path $Rf "runs"))) {
	# D-082: printed, judged nowhere; the reader's own sentence of item 7 comes with it.
	& python -I $Probe read $Rf | Select-String "^(PLAY|SERIES|HEALTH|FIELD|REFORGE|FAR):|^The field reads" | ForEach-Object { $_.Line }
}
if (-not $NoBand -and (Test-Path (Join-Path $Band "decline"))) {
	# D-082 items 4 to 6: the play arms' floors against the arm that declines the hand, with their intervals, and the boss's side of the
	# band; printed, the exit code is the reader's above.
	& python -I (Join-Path $Root "Tools\evolve_read.py") --carry (Join-Path $Band "carry") --hold (Join-Path $Band "hold") --control (Join-Path $Band "decline") --check-floors |
		Select-String "against the control|the band's boss side|^FLOORS:" | ForEach-Object { $_.Line }
}
exit $Rc
