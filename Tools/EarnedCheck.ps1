# A person's loop on real launches (gate step `earned`, meta.md R-MET-30, T-MET-30), off-screen, no window.
#   & Tools\EarnedCheck.ps1 [-Seeds 9100,9101,9102] [-BranchSeed 9120] [-DuoSeeds 9130,9131,9132] [-DuoRunSeeds 9230,9231,9232] [-HandSeconds 300] [-Timeout 300]
# Part one, nothing planted, on one save from empty. `earn`: a bot plays a gated run and banks what it earned. `Earned`: a second process opens Contracts and a
# real key buys a node that run can afford. `EarnedRead`: a third reads the purchase. `next`: the same seed, gated: a hand now holds what the node opened, which `earn`'s hands could not.
# Part two, on a copy of the save `earn` left, with only a balance planted, so its three Keepers are doors a run opened. `Doors`: keys buy Reaction.Duos' two prerequisites and
# one node of each new branch, then choose Keepers 4 to 6 at the Shrine, begin a gated run on each, and take a real Restart. Then bots on that save: each new Keeper's run holds its
# bought card, the last with nobody named, as on a Restart; a run on Storm reads an L8 hand holding a met Keeper and earns TwoPaths6; the run after it holds a duo card, which that one could not.
# A bot stands in for the hands; the keys, the file and the pool are real. A seed that leaves a door shut, or a duo card out of its hands, goes to the next, as in Tools/WinCheck.ps1.
# -DTHub is what lets these -unattended launches see the hub, as in Tools/VigilCheck.ps1.
# Prints "EARNED: PASS (...)" or "EARNED: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\EarnedCheck_<tag>.log. Saves and shots: Saved\EarnedCheck.
param(
	[int[]]$Seeds = @(9100, 9101, 9102),
	[int]$BranchSeed = 9120,
	[int[]]$DuoSeeds = @(9130, 9131, 9132),
	[int[]]$DuoRunSeeds = @(9230, 9231, 9232),
	[int]$HandSeconds = 300,
	[int]$Timeout = 300
)

trap { Write-Host "EARNED: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\EarnedCheck"
$Playtests = Join-Path $Root "Saved\Playtests"
$Map = "/Game/DT/Maps/L_Arena_Test"
$RunSeconds = 1100
$Clock = [Diagnostics.Stopwatch]::StartNew()
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force $LogDir, $OutDir | Out-Null

function Stop-Check([string]$Why) {
	Write-Host "EARNED: FAIL ($Why)"
	exit 1
}

function Start-Launch([string]$Tag, [string]$MapUrl, [string[]]$Extra) {
	$Log = Join-Path $LogDir "EarnedCheck_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$ArgList = @("`"$Project`"", $MapUrl, "-game", "-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-LiveCoding=false",
		"-unattended", "-nopause", "-nosound", "-culture=en", "-ABSLOG=`"$Log`"") + $Extra
	$Proc = Start-DTHidden $Exe $ArgList
	return @{ Proc = $Proc; Log = $Log; Tag = $Tag }
}

function Wait-Launch($Launch) {
	if (-not $Launch.Proc.WaitForExit($Timeout * 1000)) {
		Stop-DTHidden $Launch.Proc
		Stop-Check "launch $($Launch.Tag) timed out"
	}
	if (Select-String -Path $Launch.Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed" -Quiet) {
		Stop-Check "launch $($Launch.Tag) crashed, see $($Launch.Log)"
	}
	return $Launch.Log
}

# Real key events in a hub, by a script of Hub/DTHubCheck.cpp, on the named save.
function Invoke-Hub([string]$Script, [string]$Save, [string[]]$Extra = @()) {
	$Log = Wait-Launch (Start-Launch $Script $Map (@("-DTHub", "-DTHubCheck=$Script", "-DTHubShots=`"$OutDir`"", "-DTMetaFile=`"$Save`"", "-DTSeed=$BranchSeed") + $Extra))
	$Lines = @(Select-String -Path $Log -Pattern "VIGILCHECK" | ForEach-Object { $_.Line -replace "^.*LogDT: ", "" })
	$Lines | Where-Object { $_ -match "WRONG|VIGILCHECK:" } | ForEach-Object { "    $Script $_" } | Write-Host
	if (-not ($Lines | Where-Object { $_ -match "^VIGILCHECK: PASS" })) { Stop-Check "$Script did not pass, see $Log" }
	return $Log
}

# A gated bot run on the named save. With -Named the map names the Patron, as Begin does; without it nobody does, as on a Restart. Nothing else reaches the run: no -ExecCmds, no -DTUnlock.
function Start-Bot([string]$Tag, [int]$Seed, [string]$Save, [string]$Patron, [string]$Archetype, [int]$Seconds, [switch]$Named, [string]$Second = "") {
	Get-ChildItem $Playtests -Filter "${Seed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force
	$Url = if ($Named) { "${Map}?DTSeed=${Seed}?DTPatron=$Patron" } else { $Map }
	$Extra = @("-DTPoolGate", "-DTMetaFile=`"$Save`"", "-DTSeed=$Seed", "-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=1.0", "-DTBotPath=$Patron", "-DTBotArchetype=$Archetype",
		"-DTBotFast=120", "-DTBotQuit", "-DTBotMaxSeconds=$Seconds")
	if ($Second) { $Extra += "-DTBotSecond=$Second" }
	return Start-Launch $Tag $Url $Extra
}

# Every hand and pick of these runs' telemetry, one to a line; the lines stay in Saved\EarnedCheck\hands_<tag>.txt for a reader, the telemetry files go.
function Read-Hands([string]$Tag, [System.IO.FileInfo[]]$Files) {
	$Hands = @()
	foreach ($File in $Files) {
		$Hands += @([IO.File]::ReadAllLines($File.FullName) -match '"kind":"offer(_pick)?"')
		Get-ChildItem $File.DirectoryName -Filter "$($File.BaseName).*" | Remove-Item -Force
	}
	$Text = $Hands -join "`n"
	[IO.File]::WriteAllText((Join-Path $OutDir "hands_$Tag.txt"), $Text, (New-Object Text.UTF8Encoding($false)))
	return $Text
}

function Read-SeedHands([string]$Tag, [int]$Seed) {
	return Read-Hands $Tag @(Get-ChildItem $Playtests -Filter "${Seed}_*.jsonl" -ErrorAction SilentlyContinue)
}

# A launch that draws its own seeds: its files are the ones its log names, never what the clock finds. Saved\Playtests is where a person's runs live (Tools\SeedCheck.ps1), and Read-Hands deletes what it reads.
function Read-LogHands([string]$Tag, [string]$Log) {
	$Files = @(Select-String -Path $Log -Pattern "Telemetry: session seed=\d+ players=\d+ -> (.+)\.csv\s*$" | ForEach-Object { Get-Item -LiteralPath ($_.Matches[0].Groups[1].Value + ".jsonl") -ErrorAction Stop })
	return Read-Hands $Tag $Files
}

function Read-Bank([string]$Log, [int]$Runs) {
	$Line = Select-String -Path $Log -Pattern "Meta: banked (\d+), balance (\d+), runs $Runs, wins \d+" | Select-Object -Last 1
	if (-not $Line) { Stop-Check "no bank of run $Runs in $Log" }
	return @{ Award = [int64]$Line.Matches[0].Groups[1].Value; Balance = [int64]$Line.Matches[0].Groups[2].Value }
}

function Read-New([string]$Log) {
	$Line = Select-String -Path $Log -Pattern "Meta: discoveries met \d+ of \d+ \[.*\], new \[(.*)\]" | Select-Object -Last 1
	if (-not $Line) { return @() }
	return @($Line.Matches[0].Groups[1].Value -split " " | Where-Object { $_ })
}

function Read-Unlocked([string]$Log) {
	$Line = Select-String -Path $Log -Pattern "Offers: pool gated, (\d+) unlocked" | Select-Object -Last 1
	if (-not $Line) { Stop-Check "no gated pool in $Log" }
	return [int]$Line.Matches[0].Groups[1].Value
}

# The Keepers of a run's L8 hand, in seat order.
function Read-L8([string]$Hands) {
	$Hand = [regex]::Match($Hands, '"kind":"offer",[^
]*"hand_kind":"Keeper"[^
]*').Value
	return @([regex]::Matches($Hand, '"id":"Keeper\.([A-Za-z]+)"') | ForEach-Object { $_.Groups[1].Value }) -join " "
}

$Bound = '"id":"Blessing\.[A-Za-z]+\.[A-Za-z]+\.(Amp1|Amp2|Trait)"'
$Doors = @("ReachL10", "WeaponL8", "Survive10")

# --- part one: Ichor earned in a run, spent on the page, the pool changed on the next run ---------------------------------
$Earned = Join-Path $OutDir "Earned.sav"
$RunSeed = $null
foreach ($Seed in $Seeds) {
	if (Test-Path $Earned) { Remove-Item $Earned -Force }
	$LogEarn = Wait-Launch (Start-Bot "earn" $Seed $Earned "Storm" "Tempest" $RunSeconds -Named)
	$New = Read-New $LogEarn
	if (@($Doors | Where-Object { $New -notcontains $_ }).Count -eq 0) {
		$RunSeed = $Seed
		break
	}
	Write-Host "    seed $Seed earned [$($New -join ' ')], no three doors; next seed"
}
if ($null -eq $RunSeed) { Stop-Check "no seed of $($Seeds -join ' ') opened $($Doors -join ' ')" }
if (-not (Select-String -Path $LogEarn -Pattern "Meta: opened \(\d+\) .*, balance 0, wins 0, discoveries 0" -Quiet)) { Stop-Check "earn did not start from an empty save" }
$First = Read-Bank $LogEarn 1
$HandsBefore = Read-SeedHands "earn" $RunSeed
if ((Read-Unlocked $LogEarn) -ne 0) { Stop-Check "earn's pool was not a first run's" }
$Planted = Join-Path $OutDir "Planted.sav"
Copy-Item $Earned $Planted -Force

$LogBuy = Invoke-Hub "Earned" $Earned
$Spent = $First.Balance - (Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern "TREE\.COST_BLESSING4:\s*\{\s*value:\s*(\d+)").Matches[0].Groups[1].Value
$PageOk = (Select-String -Path $LogBuy -Pattern "Hub: open, balance $($First.Balance), runs 1," -Quiet) -and (Select-String -Path $LogBuy -Pattern "Tree: bought Branch\.Storm\.Blessing4, balance $Spent$" -Quiet)
$LogRead = Invoke-Hub "EarnedRead" $Earned
$ReadOk = Select-String -Path $LogRead -Pattern "Meta: opened \(2\) .*, balance $Spent, " -Quiet

$LogNext = Wait-Launch (Start-Bot "next" $RunSeed $Earned "Storm" "Tempest" $RunSeconds -Named)
$HandsAfter = Read-SeedHands "next" $RunSeed
$Second = Read-Bank $LogNext 2
$null = Read-Unlocked $LogNext # stops on a run that was not gated; the count itself proves nothing, since earn's bank alone raises it, and only a bought card in a hand needs the purchase
$NextOk = ($HandsBefore.Length -gt 0) -and ($HandsBefore -notmatch $Bound) -and ($HandsAfter -match '"id":"Blessing\.Storm\.[A-Za-z]+\.(Amp1|Amp2|Trait)"') -and -not ($HandsAfter -match '"id":"Blessing\.(Forge|Tide)\.[A-Za-z]+\.(Amp1|Amp2|Trait)"') -and
	($Second.Balance -eq ($Spent + $Second.Award))
if (-not ($PageOk -and $ReadOk -and $NextOk)) { Stop-Check "the loop on seed $RunSeed`: page=$PageOk read=$ReadOk next=$NextOk, banked $($First.Award)" }

# --- part two: the page's rows the doors added, from a planted balance ------------------------------------------------------
$LogDoors = Invoke-Hub "Doors" $Planted @("-DTPoolGate")
$ChainHands = Read-LogHands "doors" $LogDoors
$ChainOk = @("Shade", "Radiance", "Quicksilver" | Where-Object { $ChainHands -notmatch ('"hand_kind":"Patron"[^\n]*"slot":0,"kind":"Keeper","id":"Keeper\.' + $_ + '"[^\n]*"slot":1,"kind":"Keeper","id":"Keeper\.Storm"') }).Count -eq 0

# Each new branch's card in a hand. Quicksilver, the Patron the chain left, is named by nobody: that run's hand is a Restart's.
$Arms = @(
	@{ Tag = "shade"; Path = "Shade"; Archetype = "Hound"; Named = $true },
	@{ Tag = "radiance"; Path = "Radiance"; Archetype = "Lance"; Named = $true },
	@{ Tag = "quicksilver"; Path = "Quicksilver"; Archetype = "Vector"; Named = $false })
$Next = $BranchSeed
$Started = @()
foreach ($Arm in $Arms) {
	$Next++
	$Copy = Join-Path $OutDir "Branch_$($Arm.Tag).sav"
	Copy-Item $Planted $Copy -Force
	$Started += @{ Arm = $Arm; Seed = $Next; Launch = (Start-Bot $Arm.Tag $Next $Copy $Arm.Path $Arm.Archetype $HandSeconds -Named:$Arm.Named) }
}
$BranchBad = @()
foreach ($Run in $Started) {
	$null = Wait-Launch $Run.Launch
	$Hands = Read-SeedHands $Run.Arm.Tag $Run.Seed
	$Path = $Run.Arm.Path
	$Picked = $Hands -match ('"kind":"offer_pick"[^\n]*"id":"Keeper\.' + $Path + '"')
	$Led = $Hands -match ('"hand_kind":"Patron"[^\n]*"slot":0,"kind":"Keeper","id":"Keeper\.' + $Path + '"[^\n]*"slot":1,"kind":"Keeper","id":"Keeper\.Storm"')
	$Card = $Hands -match ('"id":"Blessing\.' + $Path + '\.[A-Za-z]+\.(Amp1|Amp2|Trait)"')
	if (-not ($Picked -and $Led -and $Card)) { $BranchBad += "$Path picked=$Picked led=$Led card=$Card" }
}

# TwoPaths6 on the two bought Blessing4s, then the duo card it opens. The L8 hand has three seats and five Keepers left, so this hand must hold a met one.
$L8Ok = $false
$DuosOpen = $false
$DuoIndex = 0
$TwoHands = ""
while ($DuoIndex -lt $DuoSeeds.Count -and -not $DuosOpen) {
	$Seed = $DuoSeeds[$DuoIndex]
	$DuoIndex++
	$LogTwo = Wait-Launch (Start-Bot "twopaths" $Seed $Planted "Storm" "Tempest" $RunSeconds -Named -Second "Forge")
	$TwoHands = Read-SeedHands "twopaths" $Seed
	$L8Ok = $L8Ok -or ($TwoHands -match '"hand_kind":"Keeper"[^\n]*"id":"Keeper\.(Shade|Radiance|Quicksilver)"')
	$DuosOpen = (Read-New $LogTwo) -contains "TwoPaths6"
}
if (-not $DuosOpen) { Stop-Check "no seed of $($DuoSeeds -join ' ') earned TwoPaths6 on the two bought nodes" }

# The run after it, which TwoPaths6's run could not be. The card goes with whichever pair the run commits, so a Forge that misses the L8 hand only changes the pair, but it is one card in a drawn hand: a seed that never deals it goes to the next, on a fresh copy of the same save.
$DuoSeed = $null
$DuoMissed = @()
foreach ($Seed in $DuoRunSeeds) {
	$Copy = Join-Path $OutDir "Duo_$Seed.sav"
	Copy-Item $Planted $Copy -Force
	$LogDuo = Wait-Launch (Start-Bot "duo" $Seed $Copy "Storm" "Tempest" $RunSeconds -Named -Second "Forge")
	$null = Read-Unlocked $LogDuo # stops on a run that was not gated
	$DuoHands = Read-SeedHands "duo" $Seed
	if ($DuoHands -match '"id":"Blessing\.Duo\.') {
		$DuoSeed = $Seed
		break
	}
	$DuoMissed += "seed $Seed L8 [$(Read-L8 $DuoHands)]"
	Write-Host "    seed $Seed held no duo card, L8 [$(Read-L8 $DuoHands)]; next seed"
}
$DuoOk = (-not ($TwoHands -match '"id":"Blessing\.Duo\.')) -and ($null -ne $DuoSeed)
$DuoRead = if ($DuoSeed) { "on seed $DuoSeed" } else { "on no seed of $($DuoRunSeeds -join ' ')" }
if ($DuoMissed.Count) { $DuoRead += ", missed $($DuoMissed -join ', ')" }

$Summary = "seed $RunSeed banked $($First.Award), [$($New -join ' ')], key spent it to $Spent, new process read it, next run's hands hold the bought cards; keys bought five nodes on a planted balance; Shrine chain=$ChainOk, branch cards=$($BranchBad.Count -eq 0), L8 holds a met Keeper=$L8Ok, TwoPaths6 on seed $($DuoSeeds[$DuoIndex - 1]), duo card=$DuoOk $DuoRead; $([int]$Clock.Elapsed.TotalSeconds) s"
if ($ChainOk -and $BranchBad.Count -eq 0 -and $L8Ok -and $DuoOk) {
	Write-Host "EARNED: PASS ($Summary)"
	exit 0
}
$BranchBad | ForEach-Object { "    $_" } | Write-Host
Write-Host "EARNED: FAIL ($Summary)"
exit 1
