# Plants isolated saves by the doors' own writer, for a battery that reads a person's pool after a run (meta.md R-MET-28, D-069), off-screen, no window.
#   & Tools\PlantSave.ps1 -Out <dir> [-Plant "Best.PathsAtTier2 2","Best.WeaponLevel 8"] [-Seconds 20] [-Patrons Storm,Forge] [-Seed 9100] [-Timeout 900]
# One gated bot run (Steered Storm Tempest, -DTPoolGate) on <Out>\base.sav plants each counter with DT.Meta.Plant and banks. The Discoveries the run meets write
# their targets as on a person's save (R-MET-42): nothing here writes Unlocked. A run that must last (Survive10 reads the clock) takes -Seconds; whatever else that
# run meets is banked too, and the line names every id the save then holds. Then, for each of -Patrons, a copy <Out>\<Patron>.sav takes that Keeper as its last
# Patron by DT.Meta.LastPatron, on a launch with no bot that plays no step: the run-start Patron hand stays open, so nothing is banked over it (R-MET-04), and
# DT.Meta.Dump logs what the save holds. A battery reads those saves with Tools/BotRuns.ps1 -SaveFrom <Out>: each run's first Patron seat is its arm's Keeper,
# the seat a Restart gives a save's last Patron (keepers.md R-KEE-23), and no map option is passed (vigil.md R-VIG-02).
# Prints "PLANT: PASS (...)" or "PLANT: FAIL (...)"; exit 0 / 1. Logs: <Out>\plant_<tag>.log.
param(
	[Parameter(Mandatory = $true)][string]$Out,
	[string[]]$Plant = @(),
	[int]$Seconds = 20,
	[string[]]$Patrons = @("Storm"),
	[int]$Seed = 9100,
	[int]$Timeout = 900
)

trap { Write-Host "PLANT: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "PlantSave"  # D-055 item 7: one heavy-job slot for this run, none for a step of a run that holds one

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Project = Join-Path $Root "Dusktracer.uproject"
$Map = "/Game/DT/Maps/L_Arena_Test"
$Plant = @($Plant | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Patrons = @($Patrons | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Out = [IO.Path]::GetFullPath($Out)
# Every launch has a -userdir under <Out>: its run files land there, never in Saved\Playtests, where a person's Play.bat runs live.
$UserDir = Join-Path $Out "userdir"
New-Item -ItemType Directory -Force $Out, $UserDir | Out-Null

function Stop-Plant([string]$Why) {
	Write-Host "PLANT: FAIL ($Why)"
	exit 1
}

function Invoke-Plant([string]$Tag, [string]$Save, [string[]]$Extra) {
	$Log = Join-Path $Out "plant_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$ArgList = @("`"$Project`"", $Map, "-game", "-nullrhi", "-nosplash", "-unattended", "-nopause", "-nosound", "-LiveCoding=false",
		"-DTSeed=$Seed", "-DTPoolGate", "-DTMetaFile=`"$Save`"", "-ABSLOG=`"$Log`"", "-userdir=`"$UserDir`"") + $Extra
	$Proc = Start-DTHidden $Exe $ArgList
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		Stop-DTHidden $Proc
		Stop-Plant "launch $Tag timed out"
	}
	if (Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed" -Quiet) { Stop-Plant "launch $Tag crashed, see $Log" }
	return $Log
}

# The run: plants, plays, banks once.
$Base = Join-Path $Out "base.sav"
if (Test-Path $Base) { Remove-Item $Base -Force }
$Cmds = @($Plant | ForEach-Object { "DT.Meta.Plant $_" })
$Extra = @("-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=1.0", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotFast=600", "-DTBotQuit", "-DTBotMaxSeconds=$Seconds")
if ($Cmds.Count) { $Extra += "-ExecCmds=`"$($Cmds -join ',')`"" }
$LogRun = Invoke-Plant "run" $Base $Extra
$Bank = Select-String -Path $LogRun -Pattern "Meta: banked \d+, balance \d+, runs 1," | Select-Object -Last 1
if (-not $Bank -or -not (Test-Path $Base)) { Stop-Plant "the planting run banked nothing, see $LogRun" }
$Met = (Select-String -Path $LogRun -Pattern "Meta: discoveries met \d+ of \d+ \[.*\], new \[(.*)\]" | Select-Object -Last 1).Matches[0].Groups[1].Value

# Each Patron's copy: the last Patron set, no step played, the profile read back.
$Read = @()
foreach ($Patron in $Patrons) {
	$Save = Join-Path $Out "$Patron.sav"
	Copy-Item $Base $Save -Force
	$Log = Invoke-Plant $Patron $Save @("-ExecCmds=`"DT.Meta.LastPatron $Patron,DT.Meta.Dump,quit`"")
	if (-not (Select-String -Path $Log -Pattern "DT.Meta.LastPatron ${Patron}: written" -Quiet)) { Stop-Plant "$Patron's save did not take its last Patron, see $Log" }
	if (Select-String -Path $Log -Pattern "Meta: banked" -Quiet) { Stop-Plant "$Patron's launch banked a run over its save, see $Log" }
	$Dump = Select-String -Path $Log -Pattern "Meta: profile balance -?\d+, runs 1, wins \d+, ranks \[[^\]]*\], unlocked \[([^\]]*)\], last patron '$Patron'" | Select-Object -Last 1
	if (-not $Dump) { Stop-Plant "$Patron's save does not read back one run and its last Patron, see $Log" }
	$Read += "${Patron}: [$($Dump.Matches[0].Groups[1].Value)]"
}
Write-Host "PLANT: PASS (run of $Seconds s on seed $Seed met new [$Met]; $($Read -join '; '); $Out)"
exit 0
