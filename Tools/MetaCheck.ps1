# The meta save on real launches (gate step `meta`, meta.md T-MET-16, T-MET-17), headless, no window.
#   & Tools\MetaCheck.ps1 [-Seed 9108] [-Seconds 20]
# A: a tool launch with no save: the default save must not change. B: same seed with an empty isolated save: same input_hash,
# steps and CSV, and the save appears. C: a second launch reads it and adds to it. D: the file corrupted: it goes aside byte
# for byte and a fresh one appears. E: a saved win reaches a real fight's start. F: a planted counter earns a Discovery into a file and none with no file.
# G: the Tree and the first-run locks bought back up to a tool run's pool, gate on, play exactly as A. H: -DTPoolGate with no save offers a person's first pool.
# I: a Discovery writes a Keeper and the duos, the next gated launch meets that Keeper, and a Patron chosen at the hub, or last played, keeps its seat.
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

function Invoke-Run([string]$Tag, [string[]]$RunArgs, [string]$Patron = "Storm", [string]$Archetype = "Tempest", [string]$MapOptions = "") {
	$Log = Join-Path $LogDir "MetaCheck_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$Record = Join-Path $OutDir "$Tag.csv"
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test$MapOptions", "-game", "-nullrhi", "-nosplash", "-nopause", "-unattended", "-LiveCoding=false",
		"-ABSLOG=`"$Log`"", "-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=0.6", "-DTBotPath=$Patron", "-DTBotArchetype=$Archetype", "-DTBotFast=60", "-DTBotQuit",
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

function Read-Jsonl() {
	$File = Get-ChildItem $Playtests -Filter "${Seed}_*.jsonl" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
	return $(if ($File) { [IO.File]::ReadAllText($File.FullName) } else { "" })
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

# F (T-MET-45): a planted counter on a real launch earns a Discovery into the file, and the next launch holds it; the same plant with no file earns nothing and writes nothing (meta.md R-MET-40).
$Plant = Join-Path $OutDir "Plant.sav"
$BeforeF = Get-Stamp $Default
$LogF0 = Invoke-Run "f0" @("-ExecCmds=`"DT.Meta.Plant Dashes 100`"")
$null = Take-Csv
$NoFileOk = (Find-Log $LogF0 "Meta: discoveries met \d+ of 15 \[.*Dashes100.*\], new \[\]") -and ((Get-Stamp $Default) -eq $BeforeF)
$LogF1 = Invoke-Run "f1" @("-DTMetaFile=`"$Plant`"", "-ExecCmds=`"DT.Meta.Plant Dashes 100`"")
$null = Take-Csv
$LogF2 = Invoke-Run "f2" @("-DTMetaFile=`"$Plant`"")
$null = Take-Csv
$DiscoveryOk = (Find-Log $LogF1 "Meta: discoveries met \d+ of 15 \[.*Dashes100.*\], new \[Dashes100\]") -and (Find-Log $LogF2 "Meta: opened \(2\).*discoveries 1") -and (Find-Log $LogF2 "Meta: counters ")

# G: the Tree on real launches (T-MET-20, T-MET-22). A profile at the tool pool with the gate on plays the seed of A: the nodes raise no number.
$TreeSave = Join-Path $OutDir "DTTree.sav"
$LogG1 = Invoke-Run "g1" @("-DTMetaFile=`"$TreeSave`"", "-ExecCmds=`"DT.Meta.Currency 1000000,DT.Tree.Buy ToolPool`"")
$null = Take-Csv
$LogG2 = Invoke-Run "g2" @("-DTMetaFile=`"$TreeSave`"", "-DTPoolGate")
$CsvG = Take-Csv
$G = Read-Record "g2"
$TreeOk = (Find-Log $LogG1 "DT.Tree.Buy ToolPool: 12 bought") -and (Find-Log $LogG2 "Offers: pool gated, 34 unlocked, tools \+1 \+1 \+1") -and
	($G["input_hash"] -eq $A["input_hash"]) -and ($G["steps"] -eq $A["steps"]) -and ($CsvG -ceq $CsvA)

# H: a tool asks for a person's first pool: no file, no unlock, and no locked card in a hand.
$LogH = Invoke-Run "h" @("-DTPoolGate")
$Hands = Read-Jsonl
$null = Take-Csv
$FirstOk = (Find-Log $LogH "Offers: pool gated, 0 unlocked, tools \+0 \+0 \+0") -and (Find-Log $LogH "Meta: opened \(0\) no save") -and ($Hands -match '"id":"Blessing\.[^"]*\.Key"') -and -not ($Hands -match '"id":"(Blessing\.[^"]*\.(Amp1|Amp2|Trait)|Mod\.(ScatterShell|ChainTrigger|Conduit|LingeringField))"')

# I (T-MET-28): the doors on real launches. A planted row writes a Keeper and the duos into a file; the next gated launch meets that Keeper, and a Patron chosen at the hub keeps its seat beside it.
$DoorSave = Join-Path $OutDir "Door.sav"
$LogI1 = Invoke-Run "i1" @("-DTMetaFile=`"$DoorSave`"", "-ExecCmds=`"DT.Meta.Plant Best.WeaponLevel 8,DT.Meta.Plant Best.PathsAtTier2 2`"")
$null = Take-Csv
$LogI2 = Invoke-Run "i2" @("-DTMetaFile=`"$DoorSave`"", "-DTPoolGate") "Radiance" "Lance"
$Met = Read-Jsonl
$null = Take-Csv
$LogI3 = Invoke-Run "i3" @("-DTMetaFile=`"$DoorSave`"", "-DTPoolGate") "Tide" "Frost" "?DTSeed=${Seed}?DTPatron=Tide"
$Chosen = Read-Jsonl
$null = Take-Csv
$LogI4 = Invoke-Run "i4" @("-DTMetaFile=`"$DoorSave`"", "-DTPoolGate") "Tide" "Frost"
$Again = Read-Jsonl
$null = Take-Csv
$WroteDoorOk = (Find-Log $LogI1 "Meta: discoveries met \d+ of 15 \[.*TwoPaths6.*WeaponL8.*\]|Meta: discoveries met \d+ of 15 \[.*WeaponL8.*TwoPaths6.*\]") -and (Find-Log $LogI2 "Offers: pool gated, 5 unlocked")
$MetOk = ($Met -match '"kind":\s*"offer_pick"[^\n]*"id":\s*"Keeper\.Radiance"') -and ($Met -match '"id":\s*"Blessing\.Radiance\.[A-Za-z]+\.Key"') -and -not ($Met -match '"id":\s*"Blessing\.Radiance\.[A-Za-z]+\.(Amp1|Amp2|Trait)"')
$ChosenOk = ($Chosen -match '"kind":\s*"offer_pick"[^\n]*"id":\s*"Keeper\.Tide"') -and ($Chosen -match '"hand_kind":\s*"Patron"[^\n]*"id":\s*"Keeper\.Radiance"') -and ($Again -match '"kind":\s*"offer_pick"[^\n]*"id":\s*"Keeper\.Tide"')

$Summary = "input_hash {0} / {1}, steps {2} / {3}, csv same={4}, tool run clean={5}, wrote={6}, reread={7}, kept aside={8}, fresh={9}, win reaches fight={10}, discovery saved={11}, no file earns none={12}, tool pool plays the same={13}, first-run pool={14}, door written={15}, Keeper met={16}, chosen Patron seated={17}" -f $A["input_hash"], $B["input_hash"], $A["steps"], $B["steps"], $SameOk, $ToolOk, $WroteOk, $ReadOk, $KeptOk, $FreshOk, $WinsOk, $DiscoveryOk, $NoFileOk, $TreeOk, $FirstOk, $WroteDoorOk, $MetOk, $ChosenOk
if ($ToolOk -and $SameOk -and $WroteOk -and $ReadOk -and $KeptOk -and $FreshOk -and $WinsOk -and $DiscoveryOk -and $NoFileOk -and $TreeOk -and $FirstOk -and $WroteDoorOk -and $MetOk -and $ChosenOk) {
	Write-Host "META: PASS ($Summary)"
	exit 0
}
Write-Host "META: FAIL ($Summary)"
exit 1
