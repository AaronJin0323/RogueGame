# The meta save on real launches (gate step `meta`, meta.md T-MET-16, T-MET-17), headless, no window.
#   & Tools\MetaCheck.ps1 [-Seed 9108] [-Seconds 20] [-PoolSeconds 600] [-PoolHands 12] [-KeyHands 8]
# A: a tool launch with no save: the default save must not change. B: same seed with an empty isolated save: same input_hash,
# steps and CSV, and the save appears. C: a second launch reads it and adds to it. D: the file corrupted: it goes aside byte
# for byte and a fresh one appears. E: a saved win reaches a real fight's start. F: a planted counter earns a Discovery into a file and none with no file.
# G: the Tree and the first-run locks bought back up to a tool run's pool, gate on, play exactly as P, the tool run of the pool window. H: -DTPoolGate with no save offers a person's first pool.
# I: a Discovery writes a Keeper and the duos, a gated launch that chooses that Keeper meets it, and a Patron chosen at the hub, or last played, leads the hand with Storm beside it and the last seat drawn; over the pool window that Keeper's Keys are taken and its Amps stay shut.
# Which pool a launch was dealt from (G, H, I) is read from the cards its hands offered, over -PoolSeconds of play by the skill 1.0 bot,
# against a tool run of the same seed and length as the control (P): Tools/pool_read.py, D-037. 20 s reach two hands and no Amp, so a
# pool step that short could not tell the pools apart; the save steps (A to F) need no more than that and stay at -Seconds.
# Prints "META: PASS (...)" or "META: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\MetaCheck_<tag>.log.
param(
	[int]$Seed = 9108,
	[int]$Seconds = 20,
	# D-037: the pool steps' window, its least count of hands, and the least count dealt after a met Keeper's first Key.
	[int]$PoolSeconds = 600,
	[int]$PoolHands = 12,
	[int]$KeyHands = 8,
	[int]$Timeout = 300
)

trap { Write-Host "META: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "MetaCheck"  # D-055 item 7: one heavy-job slot for this run, none for a step of a run that holds one

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\MetaCheck"
# Every launch has a -userdir under Saved\MetaCheck\userdir: its run files land in that folder's Saved\Playtests, never in Saved\Playtests, where a person's Play.bat runs
# live, so this check deletes by seed only what it wrote itself (review #58, #104).
# The default save a tool run must not touch is that folder's too, and it starts absent.
$UserDir = Join-Path $OutDir "userdir"
$Playtests = Join-Path $UserDir "Saved\Playtests"
$Default = Join-Path $UserDir "Saved\SaveGames\DTMeta.sav"
$Save = Join-Path $OutDir "DTMeta.sav"
New-Item -ItemType Directory -Force $LogDir | Out-Null
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force $OutDir | Out-Null

function Invoke-Run([string]$Tag, [string[]]$RunArgs, [string]$Patron = "Storm", [string]$Archetype = "Tempest", [string]$MapOptions = "", [switch]$Pool) {
	# A pool launch plays to the time cut at skill 1.0, where the bot is alive, so its end is as exact as a 20 s launch's (a death's last step is not).
	$Skill = if ($Pool) { "1.0" } else { "0.6" }
	$Fast = if ($Pool) { 600 } else { 60 }
	$Until = if ($Pool) { $PoolSeconds } else { $Seconds }
	$Log = Join-Path $LogDir "MetaCheck_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$Record = Join-Path $OutDir "$Tag.csv"
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test$MapOptions", "-game", "-nullrhi", "-nosplash", "-nopause", "-unattended", "-LiveCoding=false",
		"-ABSLOG=`"$Log`"", "-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=$Skill", "-DTBotPath=$Patron", "-DTBotArchetype=$Archetype", "-DTBotFast=$Fast", "-DTBotQuit",
		"-DTBotMaxSeconds=$Until", "-DTSeed=$Seed", "-DTBotOut=`"$Record`"", "-userdir=`"$UserDir`"") + $RunArgs
	$Proc = Start-DTHidden $Exe $ArgList
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		Stop-DTHidden $Proc
		Write-Host "META: FAIL (launch $Tag timed out)"
		exit 1
	}
	# A launch that crashed or hit an ensure proves none of what its records say.
	$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed" -ErrorAction SilentlyContinue)
	if ($Crash.Count) {
		Write-Host "META: FAIL (launch $Tag crashed or hit an ensure: $($Crash[0].Line))"
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

# Keeps the launch's hands for Tools/pool_read.py; call it before Take-Csv, which clears the seed's telemetry.
function Keep-Jsonl([string]$Tag) {
	$File = Get-ChildItem $Playtests -Filter "${Seed}_*.jsonl" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
	$Kept = Join-Path $OutDir "$Tag.jsonl"
	if ($File) { Copy-Item $File.FullName $Kept }
	return $Kept
}

$PoolLines = @()
function Read-Pool([string[]]$ReadArgs) {
	$Line = (& python -I (Join-Path $PSScriptRoot "pool_read.py") @ReadArgs 2>&1 | Select-String "^POOL:" | Select-Object -Last 1).Line
	$Ok = ($LASTEXITCODE -eq 0) -and ($Line -match "^POOL: PASS")
	$script:PoolLines += $(if ($Ok) { "  $Line" } else { "  ! $(if ($Line) { $Line } else { "POOL: no verdict from pool_read.py $($ReadArgs[0])" })" })
	return $Ok
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
# P is the tool run of that length, the control of every pool step: the gated profile plays it hash for hash and is dealt its hands card for card.
$LogP = Invoke-Run "p" @() -Pool
$JsonP = Keep-Jsonl "p"
$CsvP = Take-Csv
$P = Read-Record "p"
$LogG2 = Invoke-Run "g2" @("-DTMetaFile=`"$TreeSave`"", "-DTPoolGate") -Pool
$JsonG = Keep-Jsonl "g2"
$CsvG = Take-Csv
$G = Read-Record "g2"
$SameHands = Read-Pool @("same", $JsonP, $JsonG, "--hands", $PoolHands)
$TreeOk = (Find-Log $LogG1 "DT.Tree.Buy ToolPool: 12 bought") -and (Find-Log $LogG2 "Offers: pool gated, 34 unlocked, tools \+1 \+1 \+1") -and (Find-Log $LogP "Meta: opened \(0\) no save") -and
	($P["outcome"] -eq "timeout") -and ($G["outcome"] -eq "timeout") -and $P["input_hash"] -and ($G["input_hash"] -eq $P["input_hash"]) -and $P["steps"] -and ($G["steps"] -eq $P["steps"]) -and
	($CsvP.Length -gt 0) -and ($CsvG -ceq $CsvP) -and $SameHands

# H: a tool asks for a person's first pool: no file, no unlock, and no locked card in a hand.
$LogH = Invoke-Run "h" @("-DTPoolGate") -Pool
$Hands = Read-Jsonl
$JsonH = Keep-Jsonl "h"
$null = Take-Csv
$FirstOk = (Read-Pool @("first", $JsonP, $JsonH, "--hands", $PoolHands)) -and (Find-Log $LogH "Offers: pool gated, 0 unlocked, tools \+0 \+0 \+0") -and (Find-Log $LogH "Meta: opened \(0\) no save") -and ($Hands -match '"id":"Blessing\.[^"]*\.Key"') -and -not ($Hands -match '"id":"(Blessing\.[^"]*\.(Amp1|Amp2|Trait)|Mod\.(ScatterShell|ChainTrigger|Conduit|LingeringField))"')

# I (T-MET-28): the doors on real launches. A planted row writes a Keeper and the duos into a file; a gated launch that chooses that Keeper meets it. With it last played a Restart, which names nobody, reads it first and then Path order, with the last seat drawn (keepers.md R-KEE-23). A Patron chosen at the hub leads the same way, and the met Keeper has no kept seat beside it.
$DoorSave = Join-Path $OutDir "Door.sav"
$LogI1 = Invoke-Run "i1" @("-DTMetaFile=`"$DoorSave`"", "-ExecCmds=`"DT.Meta.Plant Best.WeaponLevel 8,DT.Meta.Plant Best.PathsAtTier2 2`"")
$null = Take-Csv
$LogI2 = Invoke-Run "i2" @("-DTMetaFile=`"$DoorSave`"", "-DTPoolGate") "Radiance" "Lance" "?DTSeed=${Seed}?DTPatron=Radiance"
$Met = Read-Jsonl
$null = Take-Csv
$LogI3 = Invoke-Run "i3" @("-DTMetaFile=`"$DoorSave`"", "-DTPoolGate") "Radiance" "Lance"
$Again = Read-Jsonl
$null = Take-Csv
$LogI4 = Invoke-Run "i4" @("-DTMetaFile=`"$DoorSave`"", "-DTPoolGate") "Tide" "Frost" "?DTSeed=${Seed}?DTPatron=Tide"
$Chosen = Read-Jsonl
$null = Take-Csv
# The Amps of a met Keeper stay shut (R-MET-28): two hands deal no Amp on any pool, so this is read over the pool window, on a copy of the
# door's save so that run's bank stays out of I2 to I4, against a tool run that chose Radiance and was dealt them.
$DoorLong = Join-Path $OutDir "DoorLong.sav"
Copy-Item $DoorSave $DoorLong
$LogI5 = Invoke-Run "i5" @("-DTMetaFile=`"$DoorLong`"", "-DTPoolGate") "Radiance" "Lance" "?DTSeed=${Seed}?DTPatron=Radiance" -Pool
$JsonI5 = Keep-Jsonl "i5"
$null = Take-Csv
$LogI6 = Invoke-Run "i6" @("-DTUnlock=Keeper.Radiance") "Radiance" "Lance" -Pool
$JsonI6 = Keep-Jsonl "i6"
$null = Take-Csv
$AmpsShutOk = (Find-Log $LogI5 "Offers: pool gated, 5 unlocked") -and (Find-Log $LogI6 "Meta: opened \(0\) no save") -and (Read-Pool @("met", $JsonI6, $JsonI5, "Radiance", "--hands", $KeyHands))
$WroteDoorOk = (Find-Log $LogI1 "Meta: discoveries met \d+ of 15 \[.*TwoPaths6.*WeaponL8.*\]|Meta: discoveries met \d+ of 15 \[.*WeaponL8.*TwoPaths6.*\]") -and (Find-Log $LogI2 "Offers: pool gated, 5 unlocked")
$MetOk = ($Met -match '"kind":\s*"offer_pick"[^\n]*"id":\s*"Keeper\.Radiance"') -and ($Met -match '"id":\s*"Blessing\.Radiance\.[A-Za-z]+\.Key"') -and -not ($Met -match '"id":\s*"Blessing\.Radiance\.[A-Za-z]+\.(Amp1|Amp2|Trait)"')
$RestartOk = ($Again -match '"kind":\s*"offer_pick"[^\n]*"id":\s*"Keeper\.Radiance"') -and ($Again -match '"hand_kind":\s*"Patron"[^\n]*"slot":\s*0,"kind":\s*"Keeper","id":\s*"Keeper\.Radiance"[^\n]*"slot":\s*1,"kind":\s*"Keeper","id":\s*"Keeper\.Storm"')
$ChosenOk = $RestartOk -and ($Chosen -match '"kind":\s*"offer_pick"[^\n]*"id":\s*"Keeper\.Tide"') -and ($Chosen -match '"hand_kind":\s*"Patron"[^\n]*"slot":\s*0,"kind":\s*"Keeper","id":\s*"Keeper\.Tide"[^\n]*"slot":\s*1,"kind":\s*"Keeper","id":\s*"Keeper\.Storm"')

$Summary = "input_hash {0} / {1}, steps {2} / {3}, csv same={4}, tool run clean={5}, wrote={6}, reread={7}, kept aside={8}, fresh={9}, win reaches fight={10}, discovery saved={11}, no file earns none={12}, tool pool plays the same={13}, first-run pool={14}, door written={15}, Keeper met={16}, chosen Patron seated={17}, met Keeper's Amps shut={18}" -f $A["input_hash"], $B["input_hash"], $A["steps"], $B["steps"], $SameOk, $ToolOk, $WroteOk, $ReadOk, $KeptOk, $FreshOk, $WinsOk, $DiscoveryOk, $NoFileOk, $TreeOk, $FirstOk, $WroteDoorOk, $MetOk, $ChosenOk, $AmpsShutOk
$PoolLines
if ($ToolOk -and $SameOk -and $WroteOk -and $ReadOk -and $KeptOk -and $FreshOk -and $WinsOk -and $DiscoveryOk -and $NoFileOk -and $TreeOk -and $FirstOk -and $WroteDoorOk -and $MetOk -and $ChosenOk -and $AmpsShutOk) {
	Write-Host "META: PASS ($Summary)"
	exit 0
}
Write-Host "META: FAIL ($Summary)"
exit 1
