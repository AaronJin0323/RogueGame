# Run-loop proof on real launches (gate step `restart`, T-SET-28, settings.md R-SET-23, R-SET-24, R-SET-27), headless, no window.
#   & Tools\RestartCheck.ps1 [-Seed 9107] [-OtherSeed 2203] [-Seconds 90] [-RestartStep 1800]
# Launch A: a fresh bot run on -Seed. Launch B: a bot run on -OtherSeed that restarts through the menu's path, a real map
# reload, onto -Seed at -RestartStep. The two records must match by input_hash, steps and CSV, and the old run's file must
# read `restart`. Launch C: -DTInputCheck sends real key events through the player controller: Space held through Resume
# dashes nothing, a fresh press dashes, a held Accept holds the reload, and the keys work after it.
# Prints "RESTART: PASS (...)" or "RESTART: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\RestartCheck_<a|b|c>.log.
param(
	[int]$Seed = 9107,
	[int]$OtherSeed = 2203,
	[int]$Seconds = 90,
	[int]$RestartStep = 1800,
	[int]$Timeout = 600
)

trap { Write-Host "RESTART: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\RestartCheck"
$Playtests = Join-Path $Root "Saved\Playtests"
New-Item -ItemType Directory -Force $LogDir, $OutDir | Out-Null
foreach ($Old in @($Seed, $OtherSeed)) { Get-ChildItem $Playtests -Filter "${Old}_*" -ErrorAction SilentlyContinue | Remove-Item -Force }

function Invoke-Run([string]$Tag, [string[]]$RunArgs) {
	$Log = Join-Path $LogDir "RestartCheck_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-nullrhi", "-nosplash", "-nopause", "-unattended", "-LiveCoding=false", "-ABSLOG=`"$Log`"") + $RunArgs
	$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		Write-Host "RESTART: FAIL (launch $Tag timed out)"
		exit 1
	}
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

function Get-Csv([int]$RunSeed) {
	$File = Get-ChildItem $Playtests -Filter "${RunSeed}_*.csv" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
	if (-not $File) { return "" }
	return [IO.File]::ReadAllText($File.FullName)
}

$Bot = @("-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=0.6", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotFast=60", "-DTBotQuit", "-DTBotMaxSeconds=$Seconds")
$OutA = Join-Path $OutDir "fresh.csv"
$OutB = Join-Path $OutDir "restarted.csv"
foreach ($Out in @($OutA, $OutB)) { foreach ($File in @($Out, "$Out.txt")) { if (Test-Path $File) { Remove-Item $File -Force } } }

$null = Invoke-Run "a" ($Bot + @("-DTSeed=$Seed", "-DTBotOut=`"$OutA`""))
$CsvFresh = Get-Csv $Seed
Get-ChildItem $Playtests -Filter "${Seed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force
$null = Invoke-Run "b" ($Bot + @("-DTSeed=$OtherSeed", "-DTRestartAtStep=$RestartStep", "-DTRestartSeed=$Seed", "-DTBotOut=`"$OutB`""))
$CsvRestarted = Get-Csv $Seed
$LogC = Invoke-Run "c" @("-DTSeed=$Seed", "-DTNoDirector", "-DTInputCheck")

$Fresh = Read-Record $OutA
$Restarted = Read-Record $OutB
$OldRun = Get-ChildItem $Playtests -Filter "${OtherSeed}_*.jsonl" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
$OldEnd = $false
if ($OldRun) { $OldEnd = [IO.File]::ReadAllText($OldRun.FullName).Contains('"kind":"run_end","owner":-1,"reason":"restart"') }
$HashOk = $Fresh["input_hash"] -and ($Fresh["input_hash"] -eq $Restarted["input_hash"])
$StepsOk = $Fresh["steps"] -and ($Fresh["steps"] -eq $Restarted["steps"])
$CsvOk = ($CsvFresh.Length -gt 0) -and ($CsvFresh -ceq $CsvRestarted)
$Input = Select-String -Path $LogC -Pattern "INPUTCHECK" | ForEach-Object { $_.Line -replace "^.*LogDT: ", "" }
$Input | ForEach-Object { "    $_" }
$InputOk = [bool]($Input | Where-Object { $_ -match "^INPUTCHECK: PASS" })

$Summary = "seed {0}, input_hash {1} / {2}, steps {3} / {4}, csv same={5}, old run reads restart={6}, input check={7}" -f $Seed, $Fresh["input_hash"], $Restarted["input_hash"], $Fresh["steps"], $Restarted["steps"], $CsvOk, $OldEnd, $InputOk
if ($HashOk -and $StepsOk -and $CsvOk -and $OldEnd -and $InputOk) {
	Write-Host "RESTART: PASS ($Summary)"
	exit 0
}
Write-Host "RESTART: FAIL ($Summary)"
exit 1
