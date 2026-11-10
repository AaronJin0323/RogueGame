# Two game processes on one save (meta.md T-MET-48, R-MET-11, review #0 of Docs/Validation/S3_review_fixes.md), headless, no window.
#   & Tools\TwoWindowsCheck.ps1 [-Seed 9131] [-HoldSeconds 60] [-Timeout 300]
# Four scenes, each two real game processes sharing one -userdir under Saved\TwoWindows, so one DTMeta.sav at its default path there.
# The holder opens the save first and changes it last; the other process changes the file in between and exits:
#   bank over purchase      the holder plays a run while the other buys a node; the holder's bank must keep the purchase and its price
#   bank over bank          the holder plays a run while the other plays and banks one; both runs and both awards must stand
#   purchase over purchase  on a balance that pays for one node, the other buys one, then the holder asks for another: refused, the first stands
#   purchase over bank      the other banks a run, then the holder buys: the run and its award must stand under the purchase
# The order is forced, not timed: the other process starts only after the holder's log shows the save opened, and the holder's change
# comes after the other has exited (a run still going, or DT.Tree.BuyWhen waiting on a file this script writes). A scene whose order did
# not hold fails as such. The save is read afterwards by a third process (DT.Meta.Dump).
# Not covered: two writes inside each other's read-to-write window (a few milliseconds); see Docs/Validation/S3_verify.md.
# Prints "TWOWINDOWS: PASS (...)" or "TWOWINDOWS: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\TwoWindowsCheck_<scene>_<role>.log.
param(
	[int]$Seed = 9131,
	[int]$HoldSeconds = 60,
	[int]$Timeout = 300
)

trap { Write-Host "TWOWINDOWS: FAIL (script error: $_)"; exit 1 }

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
