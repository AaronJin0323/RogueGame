# The meta save on real launches (gate step `meta`, meta.md T-MET-16, T-MET-17), headless, no window.
#   & Tools\MetaCheck.ps1 [-Seed 9108] [-Seconds 20]
# A: a tool launch with no save: the default save must not change. B: same seed with an empty isolated save: same input_hash,
# steps and CSV, and the save appears. C: a second launch reads it and adds to it. D: the file corrupted: it goes aside byte
# for byte and a fresh one appears. E: a saved win reaches a real fight's start.
# Prints "META: PASS (...)" or "META: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\MetaCheck_<tag>.log.
param(
	[int]$Seed = 9108,
	[int]$Seconds = 20,
	[int]$Timeout = 300
)

trap { Write-Host "META: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\MetaCheck"
$Playtests = Join-Path $Root "Saved\Playtests"
$Default = Join-Path $Root "Saved\SaveGames\DTMeta.sav"
$Save = Join-Path $OutDir "DTMeta.sav"
New-Item -ItemType Directory -Force $LogDir | Out-Null
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force $OutDir | Out-Null

function Invoke-Run([string]$Tag, [string[]]$RunArgs) {
	$Log = Join-Path $LogDir "MetaCheck_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$Record = Join-Path $OutDir "$Tag.csv"
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-nullrhi", "-nosplash", "-nopause", "-unattended", "-LiveCoding=false",
		"-ABSLOG=`"$Log`"", "-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=0.6", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotFast=60", "-DTBotQuit",
		"-DTBotMaxSeconds=$Seconds", "-DTSeed=$Seed", "-DTBotOut=`"$Record`"") + $RunArgs
	$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		Write-Host "META: FAIL (launch $Tag timed out)"
		exit 1
	}
	return $Log
}

function Read-Record([string]$Tag) {
	$Record = @{}
	$Path = Join-Path $OutDir "$Tag.csv.txt"
	if (Test-Path $Path) {
		foreach ($Line in [IO.File]::ReadAllLines($Path)) {
			$Pair = $Line -split "=", 2
			if ($Pair.Count -eq 2) { $Record[$Pair[0]] = $Pair[1] }
		}
	}
	return $Record
}

function Take-Csv() {
	$File = Get-ChildItem $Playtests -Filter "${Seed}_*.csv" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
	$Text = if ($File) { [IO.File]::ReadAllText($File.FullName) } else { "" }
	Get-ChildItem $Playtests -Filter "${Seed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force
	return $Text
}

function Get-Stamp([string]$Path) {
	if (Test-Path $Path) { return (Get-FileHash $Path -Algorithm SHA256).Hash + (Get-Item $Path).LastWriteTimeUtc.Ticks }
	return "absent"
}

function Find-Log([string]$Log, [string]$Pattern) { return [bool](Select-String -Path $Log -Pattern $Pattern -Quiet) }

Get-ChildItem $Playtests -Filter "${Seed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force

# A: the tool run.
$Before = Get-Stamp $Default
$LogA = Invoke-Run "a" @()
$CsvA = Take-Csv
$ToolOk = ((Get-Stamp $Default) -eq $Before) -and (Find-Log $LogA "Meta: opened \(0\) no save") -and -not (Find-Log $LogA "Meta: banked")

# B: an empty isolated save plays the same.
$LogB = Invoke-Run "b" @("-DTMetaFile=`"$Save`"")
$CsvB = Take-Csv
$A = Read-Record "a"
$B = Read-Record "b"
$SameOk = $A["input_hash"] -and ($A["input_hash"] -eq $B["input_hash"]) -and $A["steps"] -and ($A["steps"] -eq $B["steps"]) -and ($CsvA.Length -gt 0) -and ($CsvA -ceq $CsvB)
$WroteOk = (Test-Path $Save) -and (Find-Log $LogB "Meta: opened \(1\)") -and (Find-Log $LogB "Meta: banked \d+, balance \d+, runs 1, wins 0") -and ((Get-Stamp $Default) -eq $Before)

# C: the next launch reads it and adds.
$LogC = Invoke-Run "c" @("-DTMetaFile=`"$Save`"")
$null = Take-Csv
$ReadOk = (Find-Log $LogC "Meta: opened \(2\)") -and (Find-Log $LogC "Meta: banked \d+, balance \d+, runs 2, wins 0")

# D: a corrupted file goes aside whole; a fresh one replaces it.
$Bytes = [IO.File]::ReadAllBytes($Save)
$Bytes[$Bytes.Length - 1] = $Bytes[$Bytes.Length - 1] -bxor 1
[IO.File]::WriteAllBytes($Save, $Bytes)
$LogD = Invoke-Run "d" @("-DTMetaFile=`"$Save`"")
$null = Take-Csv
$Aside = "$Save.bad.1"
$KeptOk = (Test-Path $Aside) -and ((Get-FileHash $Aside).Hash -eq (Get-FileHash -InputStream ([IO.MemoryStream]::new($Bytes))).Hash)
$FreshOk = $KeptOk -and (Test-Path $Save) -and (Find-Log $LogD "Meta: opened \(3\)") -and (Find-Log $LogD "runs 1, wins 0")

# E: a saved win is read at a real fight's start (R-MET-17). The first launch sets it, the second starts the fight.
$LogE1 = Invoke-Run "e1" @("-DTMetaFile=`"$Save`"", "-ExecCmds=`"DT.Meta.Wins 1`"")
$null = Take-Csv
$LogE2 = Invoke-Run "e2" @("-DTMetaFile=`"$Save`"", "-DTNoDirector", "-ExecCmds=`"DT.Boss`"")
$null = Take-Csv
$WinsOk = (Find-Log $LogE2 "Meta: opened \(2\).*wins 1") -and (Find-Log $LogE2 "Boss: start form=\S+ wins=1 ")

$Summary = "input_hash {0} / {1}, steps {2} / {3}, csv same={4}, tool run clean={5}, wrote={6}, reread={7}, kept aside={8}, fresh={9}, win reaches fight={10}" -f $A["input_hash"], $B["input_hash"], $A["steps"], $B["steps"], $SameOk, $ToolOk, $WroteOk, $ReadOk, $KeptOk, $FreshOk, $WinsOk
if ($ToolOk -and $SameOk -and $WroteOk -and $ReadOk -and $KeptOk -and $FreshOk -and $WinsOk) {
	Write-Host "META: PASS ($Summary)"
	exit 0
}
Write-Host "META: FAIL ($Summary)"
exit 1
