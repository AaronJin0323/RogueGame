# Two game processes on one save (meta.md T-MET-48, R-MET-11, review #0 of Docs/Validation/S3_review_fixes.md), headless, no window.
#   & Tools\TwoWindowsCheck.ps1 [-Seed 9131] [-HoldSeconds 60] [-Timeout 300]
#   & Tools\TwoWindowsCheck.ps1 -Race 40 [-RaceProcs 4]     the timed race alone, T-MET-49; see below
# Four scenes, each two real game processes sharing one -userdir under Saved\TwoWindows, so one DTMeta.sav at its default path there.
# The holder opens the save first and changes it last; the other process changes the file in between and exits:
#   bank over purchase      the holder plays a run while the other buys a node; the holder's bank must keep the purchase and its price
#   bank over bank          the holder plays a run while the other plays and banks one; both runs and both awards must stand
#   purchase over purchase  on a balance that pays for one node, the other buys one, then the holder asks for another: refused, the first stands
#   purchase over bank      the other banks a run, then the holder buys: the run and its award must stand under the purchase
# The order is forced, not timed: the other process starts only after the holder's log shows the save opened, and the holder's change
# comes after the other has exited (a run still going, or DT.Tree.BuyWhen waiting on a file this script writes). A scene whose order did
# not hold fails as such. The save is read afterwards by a third process (DT.Meta.Dump).
# Prints "TWOWINDOWS: PASS (...)" or "TWOWINDOWS: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\TwoWindowsCheck_<scene>_<role>.log.
#
# -Race <trials>: two writes inside each other's read-to-write window, which no forced order reaches (R-MET-11, D-042). -RaceProcs game
# processes (2 to 4) stay open on one save. Each trial the script names one instant half a second ahead, every process holds for it and banks
# one run there (DT.Meta.BankWhen, the call a run's end makes), and the script reads the run count out of the file once each has logged its bank.
# A trial lost a change when the file's shortfall grew and no process said it kept its bank in memory. A last reading, by a process started
# after the others have gone (DT.Meta.Dump), must hold every run and every award.
# Then three scenes with the save's lock held by this script for longer than a launch waits (C.SAVE.LOCK_WAIT_MS), T-MET-50:
#   held bank      a bank is kept in memory and said; once the lock is free the launch's next bank writes both runs
#   held purchase  a purchase is refused as busy (result 6) and nothing is spent
#   held to exit   a bot run ends and quits with the lock never free: its log and its telemetry say the bank was kept, the log says it is lost,
#                  and it leaves DTMeta.sav.lost beside the save; the next launch reads the marker, its hub holds the line for a run not saved
#                  (Hub.Notice.RunLost) and the marker is gone; the launch after that says nothing (T-MET-51)
# Prints "TWOWINDOWS RACE: PASS (...)" or "TWOWINDOWS RACE: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\TwoWindowsCheck_race_<role>.log.
param(
	[int]$Seed = 9131,
	[int]$HoldSeconds = 60,
	[int]$Timeout = 300,
	[int]$Race = 0,
	[int]$RaceProcs = 4
)

trap { Write-Host "TWOWINDOWS: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\TwoWindows"
New-Item -ItemType Directory -Force $LogDir | Out-Null
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force $OutDir | Out-Null

$Scatter = "Trunk.ScatterCannon"
$Rail = "Trunk.RailLance"
$Crashed = "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed"
$script:Started = @()

function Fail([string]$Why) {
	foreach ($Proc in $script:Started) { Stop-DTHidden $Proc }
	Write-Host "TWOWINDOWS: FAIL ($Why)"
	exit 1
}

# One launch on the scene's folder. -unattended makes it a tool run, so the save is named: it is the default path of that -userdir.
function Start-Game([string]$Scene, [string]$Role, [string[]]$Extra) {
	$Dir = Join-Path $OutDir $Scene
	$Log = Join-Path $LogDir "TwoWindowsCheck_${Scene}_$Role.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$Save = Join-Path $Dir "Saved\SaveGames\DTMeta.sav"
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-nullrhi", "-nosplash", "-nopause", "-unattended", "-LiveCoding=false",
		"-ABSLOG=`"$Log`"", "-userdir=`"$Dir`"", "-DTMetaFile=`"$Save`"") + $Extra
	$Proc = Start-DTHidden $Exe $ArgList
	$script:Started += $Proc
	return @{ Proc = $Proc; Log = $Log; Tag = "$Scene $Role" }
}

function Read-Log([string]$Log) {
	if (-not (Test-Path $Log)) { return "" }
	# The game still holds the file: read it shared.
	$Stream = New-Object IO.FileStream($Log, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
	try { return (New-Object IO.StreamReader($Stream)).ReadToEnd() } finally { $Stream.Dispose() }
}

function Wait-Line($Game, [string]$Pattern) {
	$Until = (Get-Date).AddSeconds($Timeout)
	while ((Get-Date) -lt $Until) {
		if ((Read-Log $Game.Log) -match $Pattern) { return }
		if ($Game.Proc.HasExited) { Fail "$($Game.Tag) ended before its log read '$Pattern'" }
		Start-Sleep -Milliseconds 200
	}
	Fail "$($Game.Tag) never logged '$Pattern'"
}

function Wait-Exit($Game) {
	if (-not $Game.Proc.WaitForExit($Timeout * 1000)) { Fail "$($Game.Tag) timed out" }
	$Text = Read-Log $Game.Log
	if ($Text -match $Crashed) { Fail "$($Game.Tag) crashed or hit an ensure" }
	return $Text
}

function Get-Hub([string]$Commands) { return @("-DTHub", "-ExecCmds=`"$Commands`"") }

function Get-Run([int]$RunSeed, [int]$Seconds, [switch]$Fast) {
	# The holder plays in real time at skill 1.0, where the bot is alive at the cut, so its bank comes -HoldSeconds after its start.
	$Bot = @("-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=1.0", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotQuit", "-DTBotMaxSeconds=$Seconds", "-DTSeed=$RunSeed")
	if ($Fast) { $Bot += "-DTBotFast=60" }
	return $Bot
}

function Invoke-Hub([string]$Scene, [string]$Role, [string]$Commands) { return Wait-Exit (Start-Game $Scene $Role (Get-Hub $Commands)) }

function Read-Save([string]$Scene) {
	$Text = Invoke-Hub $Scene "read" "DT.Meta.Dump,quit"
	if ($Text -notmatch "Meta: profile balance (-?\d+), runs (\d+), wins (\d+), ranks \[([^\]]*)\], unlocked \[([^\]]*)\]") { Fail "${Scene}: the save was not read" }
	return @{ Balance = [long]$Matches[1]; Runs = [int]$Matches[2]; Ranks = $Matches[4]; Unlocked = $Matches[5] }
}

function Read-Banked([string]$Text, [string]$Tag) {
	$Found = [regex]::Matches($Text, "Meta: banked (\d+), balance (\d+), runs (\d+)")
	if ($Found.Count -ne 1) { Fail "$Tag banked $($Found.Count) time(s), not once" }
	return [long]$Found[0].Groups[1].Value
}

$Lines = @()
$Passed = 0
function Add-Verdict([string]$Scene, [bool]$Ok, [string]$Read) {
	$script:Lines += "  $(if ($Ok) { ' ' } else { '!' }) ${Scene}: $Read"
	if ($Ok) { $script:Passed++ }
}

if ($Race -gt 0) {
	if ($RaceProcs -lt 2 -or $RaceProcs -gt 4) { Fail "-RaceProcs is 2 to 4" }
	$S = "race"
	$Prefix = Join-Path $OutDir "race.at"
	$File = Join-Path $OutDir "$S\Saved\SaveGames\DTMeta.sav"
	# Runs is the second field of the payload, after the 16-byte envelope header and the balance (meta.md Data, R-TEC-04).
	function Read-Runs {
		for ($Try = 0; $Try -lt 20; $Try++) {
			try {
				$Stream = New-Object IO.FileStream($File, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
				try {
					$Bytes = New-Object byte[] 28
					if ($Stream.Read($Bytes, 0, 28) -eq 28 -and [Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -eq "DTSV") { return [BitConverter]::ToInt32($Bytes, 24) }
				} finally { $Stream.Dispose() }
			} catch { }
			Start-Sleep -Milliseconds 50
		}
		Fail "race: the save was not read"
	}
	$Steps = 3600 # one minute of sim
	$Games = @(1..$RaceProcs | ForEach-Object { Start-Game $S "p$_" (Get-Hub "DT.Meta.BankWhen $Prefix $Race $Steps") })
	foreach ($Game in $Games) { Wait-Line $Game "DT\.Meta\.BankWhen: waiting" }
	$Short = 0
	$LostTrials = 0
	$Deferred = 0
	$Spreads = @()
	$RaceLines = @()
	for ($Trial = 1; $Trial -le $Race; $Trial++) {
		$At = [DateTime]::UtcNow.AddMilliseconds(500).Ticks
		[IO.File]::WriteAllText("$Prefix.tmp", "$At")
		[IO.File]::Move("$Prefix.tmp", "$Prefix.$Trial")
		$Times = @()
		$Kept = 0
		foreach ($Game in $Games) {
			Wait-Line $Game "DT\.Meta\.BankWhen round ${Trial}:"
			$Text = Read-Log $Game.Log
			if ($Text -match "\[\d{4}\.\d\d\.\d\d-(\d\d)\.(\d\d)\.(\d\d):(\d{3})\]\[\s*\d+\]LogDT: DT\.Meta\.BankWhen round ${Trial}:") {
				$Times += ((([int]$Matches[1] * 60 + [int]$Matches[2]) * 60 + [int]$Matches[3]) * 1000 + [int]$Matches[4])
			}
			$Kept += [regex]::Matches($Text, "Meta: [^\r\n]* is locked by another launch").Count
		}
		$Runs = Read-Runs
		$Now = $RaceProcs * $Trial - $Runs
		$NewKept = $Kept - $Deferred
		$Deferred = $Kept
		$Spread = if ($Times.Count -eq $RaceProcs) { ($Times | Measure-Object -Maximum).Maximum - ($Times | Measure-Object -Minimum).Minimum } else { -1 }
		$Spreads += $Spread
		if ($Now -gt $Short -and $NewKept -eq 0) {
			$LostTrials++
			$RaceLines += "  ! trial ${Trial}: the file holds $Runs run(s) of $($RaceProcs * $Trial), $($Now - $Short) lost here, spread $Spread ms"
		}
		elseif ($NewKept -gt 0) { $RaceLines += "    trial ${Trial}: $NewKept bank(s) kept in memory for the next save, the file holds $Runs run(s) of $($RaceProcs * $Trial)" }
		$Short = $Now
	}
	$Retained = 0
	foreach ($Game in $Games) {
		$Text = Wait-Exit $Game
		$Found = [regex]::Matches($Text, "DT\.Meta\.BankWhen round \d+: retained (\d+),")
		if ($Found.Count -ne $Race) { Fail "$($Game.Tag) banked $($Found.Count) time(s), not $Race" }
		foreach ($One in $Found) { $Retained += [long]$One.Groups[1].Value }
	}
	$Save = Read-Save $S
	$All = $RaceProcs * $Race

	# The lock held by somebody else. The script's handle shares nothing, as a launch's does; closing it leaves a file nobody holds.
	function Lock-Save([string]$Scene) {
		$Dir = Join-Path $OutDir "$Scene\Saved\SaveGames"
		New-Item -ItemType Directory -Force $Dir | Out-Null
		return [IO.File]::Open((Join-Path $Dir "DTMeta.sav.lock"), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
	}
	$KeptLine = "Meta: [^\r\n]* is locked by another launch; this change is kept in memory"
	$HeldPassed = 0
	$HeldLines = @()
	function Add-Held([string]$Scene, [bool]$Ok, [string]$Read) {
		$script:HeldLines += "  $(if ($Ok) { ' ' } else { '!' }) ${Scene}: $Read"
		if ($Ok) { $script:HeldPassed++ }
	}

	$H = "held_bank"
	$HPrefix = Join-Path $OutDir "$H.at"
	$HFile = Join-Path $OutDir "$H\Saved\SaveGames\DTMeta.sav"
	$Lock = Lock-Save $H
	$Game = Start-Game $H "holder" (Get-Hub "DT.Meta.BankWhen $HPrefix 2 $Steps")
	Wait-Line $Game "DT\.Meta\.BankWhen: waiting"
	[IO.File]::WriteAllText("$HPrefix.1", "$([DateTime]::UtcNow.AddMilliseconds(300).Ticks)")
	Wait-Line $Game "DT\.Meta\.BankWhen round 1:"
	$SaidKept = (Read-Log $Game.Log) -match $KeptLine
	$Unwritten = -not (Test-Path $HFile)
	$Lock.Dispose()
	[IO.File]::WriteAllText("$HPrefix.2", "$([DateTime]::UtcNow.AddMilliseconds(300).Ticks)")
	$Text = Wait-Exit $Game
	$HSave = Read-Save $H
	$HRetained = 0
	foreach ($One in [regex]::Matches($Text, "DT\.Meta\.BankWhen round \d+: retained (\d+),")) { $HRetained += [long]$One.Groups[1].Value }
	$NoMarker = -not (Test-Path "$HFile.lost")
	Add-Held $H ($SaidKept -and $Unwritten -and $HSave.Runs -eq 2 -and $HSave.Balance -eq $HRetained -and $HRetained -gt 0 -and $NoMarker) `
		"lock held: said kept=$SaidKept, file unwritten=$Unwritten; lock free, next bank -> runs $($HSave.Runs) (want 2), balance $($HSave.Balance) (want $HRetained), no marker left=$NoMarker"

	$H = "held_purchase"
	$Signal = Join-Path $OutDir "$H.go"
	$null = Invoke-Hub $H "plant" "DT.Meta.Currency 400,quit"
	$Lock = Lock-Save $H
	$Game = Start-Game $H "holder" (Get-Hub "DT.Tree.BuyWhen $Scatter $Signal")
	Wait-Line $Game "DT\.Tree\.BuyWhen $([regex]::Escape($Scatter)): waiting"
	[IO.File]::WriteAllText($Signal, "go")
	$Text = Wait-Exit $Game
	$Lock.Dispose()
	$Result = if ($Text -match "DT\.Tree\.BuyWhen $([regex]::Escape($Scatter)): result (-?\d+)") { [int]$Matches[1] } else { -2 }
	$HSave = Read-Save $H
	# 6 is EDTTreeBuy::Busy.
	Add-Held $H ($Result -eq 6 -and $HSave.Balance -eq 400 -and $HSave.Ranks -eq "") "planted 400, lock held, one node asked -> result $Result (want 6, busy), balance $($HSave.Balance) (want 400), ranks [$($HSave.Ranks)] (want none)"

	$H = "held_to_exit"
	$HFile = Join-Path $OutDir "$H\Saved\SaveGames\DTMeta.sav"
	$Lock = Lock-Save $H
	$Text = Wait-Exit (Start-Game $H "holder" (Get-Run $Seed 20 -Fast))
	$Lock.Dispose()
	$SaidKept = $Text -match $KeptLine
	$SaidLost = $Text -match "Meta: [^\r\n]* could not be written at exit; 1 change\(s\) of this launch are lost"
	$Record = $false
	if ($Text -match "Telemetry: session seed=\S+ players=\d+ -> ([^\r\n]+)\.csv") {
		$Jsonl = "$($Matches[1]).jsonl"
		$Record = (Test-Path $Jsonl) -and [bool](Select-String -Path $Jsonl -Pattern '"kind":"save_deferred","owner":-1,"change":"bank","waiting":1' -SimpleMatch -Quiet)
	}
	$Unwritten = -not (Test-Path $HFile)
	# The marker the exit left, and what the next two launches make of it. Each opens the hub on the same folder, as a person's next launch does.
	$Marker = "$HFile.lost"
	$Marked = (Test-Path $Marker) -and ([IO.File]::ReadAllText($Marker).Trim() -eq "1")
	$Next = Invoke-Hub $H "next" "DT.Meta.Dump,quit"
	$Told = ($Next -match "Meta: the last launch could not save 1 run\(s\)") -and ($Next -match "Hub: notice RunLost")
	$Removed = -not (Test-Path $Marker)
	$After = Invoke-Hub $H "after" "DT.Meta.Dump,quit"
	$Quiet = ($After -match "Hub: open") -and ($After -notmatch "Hub: notice") -and ($After -notmatch "could not save")
	Add-Held $H ($SaidKept -and $SaidLost -and $Record -and $Unwritten -and $Marked -and $Told -and $Removed -and $Quiet) `
		"lock never free: log says kept=$SaidKept, telemetry save_deferred=$Record, log says lost at exit=$SaidLost, file unwritten=$Unwritten, marker left=$Marked; next launch: hub line=$Told, marker removed=$Removed; the one after: says nothing=$Quiet"
	$Median = ($Spreads | Sort-Object)[[int][math]::Floor($Spreads.Count / 2)]
	$RaceLines
	$HeldLines
	$Summary = "$Race trial(s) of $RaceProcs launches banking at one instant, $LostTrials trial(s) lost a change, $($All - $Save.Runs) of $All bank(s) lost, balance $($Save.Balance) of $Retained, $Deferred kept in memory and written later, median spread $Median ms, $HeldPassed of 3 held-lock scenes"
	if ($LostTrials -eq 0 -and $Save.Runs -eq $All -and $Save.Balance -eq $Retained -and $HeldPassed -eq 3) {
		Write-Host "TWOWINDOWS RACE: PASS ($Summary)"
		exit 0
	}
	Write-Host "TWOWINDOWS RACE: FAIL ($Summary)"
	exit 1
}

# 1: a bank over a purchase.
$S = "bank_over_purchase"
$null = Invoke-Hub $S "plant" "DT.Meta.Currency 400,quit"
$Holder = Start-Game $S "holder" (Get-Run $Seed $HoldSeconds)
Wait-Line $Holder "Meta: opened \(2\).*balance 400,"
$Other = Invoke-Hub $S "other" "DT.Tree.Buy $Scatter,quit"
if ($Other -notmatch "Tree: bought $([regex]::Escape($Scatter)), balance 0") { Fail "${S}: the other process did not buy" }
if ((Read-Log $Holder.Log) -match "Meta: banked") { Fail "${S}: order not held, the holder banked before the other process had bought; raise -HoldSeconds" }
$Award = Read-Banked (Wait-Exit $Holder) "$S holder"
$Save = Read-Save $S
Add-Verdict $S ($Save.Balance -eq $Award -and $Save.Runs -eq 1 -and $Save.Ranks -eq "$Scatter=1" -and $Save.Unlocked -match "Weapon\.ScatterCannon") `
	"planted 400, bought 400, banked $Award -> balance $($Save.Balance) (want $Award), runs $($Save.Runs) (want 1), ranks [$($Save.Ranks)] (want [$Scatter=1])"

# 2: a bank over a bank.
$S = "bank_over_bank"
$Holder = Start-Game $S "holder" (Get-Run $Seed $HoldSeconds)
Wait-Line $Holder "Meta: opened \(1\)"
$OtherAward = Read-Banked (Wait-Exit (Start-Game $S "other" (Get-Run ($Seed + 1) 60 -Fast))) "$S other"
if ((Read-Log $Holder.Log) -match "Meta: banked") { Fail "${S}: order not held, the holder banked before the other process had; raise -HoldSeconds" }
$Award = Read-Banked (Wait-Exit $Holder) "$S holder"
$Save = Read-Save $S
Add-Verdict $S ($Save.Balance -eq ($Award + $OtherAward) -and $Save.Runs -eq 2) `
	"banked $OtherAward then $Award -> balance $($Save.Balance) (want $($Award + $OtherAward)), runs $($Save.Runs) (want 2)"

# 3: a purchase over a purchase, on a balance that pays for one.
$S = "purchase_over_purchase"
$Signal = Join-Path $OutDir "$S.go"
$null = Invoke-Hub $S "plant" "DT.Meta.Currency 400,quit"
$Holder = Start-Game $S "holder" (Get-Hub "DT.Tree.BuyWhen $Rail $Signal")
Wait-Line $Holder "Meta: opened \(2\).*balance 400,"
Wait-Line $Holder "DT\.Tree\.BuyWhen $([regex]::Escape($Rail)): waiting"
$Other = Invoke-Hub $S "other" "DT.Tree.Buy $Scatter,quit"
if ($Other -notmatch "Tree: bought $([regex]::Escape($Scatter)), balance 0") { Fail "${S}: the other process did not buy" }
[IO.File]::WriteAllText($Signal, "go")
$Text = Wait-Exit $Holder
$Result = if ($Text -match "DT\.Tree\.BuyWhen $([regex]::Escape($Rail)): result (-?\d+)") { [int]$Matches[1] } else { -2 }
$Save = Read-Save $S
# 5 is EDTTreeBuy::Short.
Add-Verdict $S ($Result -eq 5 -and $Save.Balance -eq 0 -and $Save.Ranks -eq "$Scatter=1") `
	"planted 400, two nodes of 400 asked -> second result $Result (want 5, short), balance $($Save.Balance) (want 0), ranks [$($Save.Ranks)] (want [$Scatter=1])"

# 4: a purchase over a bank.
$S = "purchase_over_bank"
$Signal = Join-Path $OutDir "$S.go"
$null = Invoke-Hub $S "plant" "DT.Meta.Currency 400,quit"
$Holder = Start-Game $S "holder" (Get-Hub "DT.Tree.BuyWhen $Scatter $Signal")
Wait-Line $Holder "Meta: opened \(2\).*balance 400,"
Wait-Line $Holder "DT\.Tree\.BuyWhen $([regex]::Escape($Scatter)): waiting"
$OtherAward = Read-Banked (Wait-Exit (Start-Game $S "other" (Get-Run ($Seed + 1) 60 -Fast))) "$S other"
[IO.File]::WriteAllText($Signal, "go")
$Text = Wait-Exit $Holder
$Result = if ($Text -match "DT\.Tree\.BuyWhen $([regex]::Escape($Scatter)): result (-?\d+)") { [int]$Matches[1] } else { -2 }
$Save = Read-Save $S
Add-Verdict $S ($Result -eq 0 -and $Save.Balance -eq $OtherAward -and $Save.Runs -eq 1 -and $Save.Ranks -eq "$Scatter=1") `
	"planted 400, banked $OtherAward, bought 400 -> result $Result (want 0), balance $($Save.Balance) (want $OtherAward), runs $($Save.Runs) (want 1), ranks [$($Save.Ranks)] (want [$Scatter=1])"

$Lines
$Summary = "$Passed of 4 scenes, two processes on one save each, seed $Seed"
if ($Passed -eq 4) {
	Write-Host "TWOWINDOWS: PASS ($Summary)"
	exit 0
}
Write-Host "TWOWINDOWS: FAIL ($Summary)"
exit 1
