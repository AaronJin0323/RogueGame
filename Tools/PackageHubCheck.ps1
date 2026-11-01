# A person's whole path proved in the package, off-screen, no window (package.md R-PKG-13, T-PKG-13). Needs Tools\Package.ps1 first, from a clean tree with
# this HEAD's Source, Content and Config (R-PKG-08).
#   & Tools\PackageHubCheck.ps1 [-Seed 9107] [-Timeout 300]
# Every launch starts from the package's own Dusktracer.exe, as Play_*.bat does. A person's launch passes no tool flag (-unattended, -DTBot*, -DTHub,
# -DTNoSave, -DTMetaFile): Start-Game refuses one. Each profile is a folder of its own under Saved\PackageHub, named by -userdir: a temporary LOCALAPPDATA
# moves nothing, because the engine asks Windows for that folder and not the environment.
#   A  staged: every Content\DT\Text\ST_*.csv and both Game.locres are in each config's .pak; both configs hold the same cook.
#   B  Development, real keys, -DTPathCheck=Walk. en on -Seed, a launch that names one, picking Storm; zh-Hans with no seed, picking Forge. Fresh save, boot,
#      every hub page, the Shrine, a run played out to a real death, Vigil, the bank, a second run, Abandon, Vigil, Quit to desktop. Every table reads against
#      its CSV in that culture, the hands hold nothing a first run keeps shut, and a launch's second run takes a fresh seed. A hero who stands still can die
#      before a level, and that is the draw's: a walk with no hand starts over on a new draw, en on the next seed, and only $WalkTries walks with none fail.
#   C  Development, -DTPathCheck=Reboot: a new process on each save reads the Patron and the Ledger-stone, switches the language live and quits.
#   D  Shipping has no log, no key and ignores a map on its command line, so it never gets past the hub with no hands. A person's boot on a bad save at the
#      default path: the file goes aside byte for byte (only a person's launch reads it), no run file, no new save. Killed by PID.
#   E  Shipping, -unattended -DTPoolGate -DTMetaFile: the seed the en walk played, the first pool, no input. Its CSV must equal that run's, so
#      Shipping played what Development was seen to play, and its bank lands in the file a person's default path names.
#   F  Development reads E's save on a person's launch: Shipping's bank, by the balance its own run file gives.
#   G  Shipping banks on top of the zh-Hans walk's save, and Development reads the sum.
#   H  Seed -Seed by the bot, in both configs, each under -userdir: the input_hash. A tool run leaves a bad file at the default path alone.
# The owner's save and settings are compared before and after. Prints "PKGHUB: PASS|FAIL|NOT RUN (...)", naming the package; exit 0 / 1 / 2. NOT RUN: a
# missing, dirty or stale package, as for PadCheck; nothing is launched or cleared.
# Logs: Saved\Logs\PkgHub_<tag>.log. Shots: Saved\PackageHub\<culture>\shots\<page>.png. Run by hand after Tools\Package.ps1, not by the gate.
param(
	[int]$Seed = 9107,
	[int]$Timeout = 300
)

trap { Write-Host "PKGHUB: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$PackageRoot = Join-Path $Root "Saved\Package"
$WorkRoot = Join-Path $Root "Saved\PackageHub"
$LogDir = Join-Path $Root "Saved\Logs"
$TextDir = Join-Path $Root "Content\DT\Text"
$Pages = @{
	Walk = @("root_fresh", "contracts", "codex", "ledger_fresh", "settings", "shrine", "root_chosen", "run", "hand", "runover_death", "root_banked", "ledger_banked", "runover_abandoned", "root_end")
	Reboot = @("root_reboot", "ledger_reboot", "root_switched")
}
$Configs = @("Development", "Shipping")
$BootWait = 20
$WalkTries = 4
New-Item -ItemType Directory -Force $LogDir | Out-Null
foreach ($Config in $Configs) {
	$Stub = Join-Path $PackageRoot "$Config\Windows\Dusktracer.exe"
	if (-not (Test-Path $Stub)) { Write-Host "PKGHUB: NOT RUN (no $Stub; Tools\Package.ps1)"; exit 2 }
}
# Which package (R-PKG-08): old binaries with the same text would pass. Before anything is cleared, so a refusal keeps the last pass's logs and shots.
$StampFile = Join-Path $PackageRoot "package_stamp.json"
if (-not (Test-Path $StampFile)) { Write-Host "PKGHUB: NOT RUN (no package stamp; Tools\Package.ps1)"; exit 2 }
$Stamp = [IO.File]::ReadAllText($StampFile) | ConvertFrom-Json
if ($Stamp.dirty) { Write-Host "PKGHUB: NOT RUN (package $($Stamp.commit) came from a dirty tree)"; exit 2 }
$Changed = @(& git -C $Root diff --name-only "$($Stamp.commit)" HEAD -- Source Content Config Dusktracer.uproject)
if ($LASTEXITCODE -ne 0) { Write-Host "PKGHUB: NOT RUN (package commit $($Stamp.commit) is not in this history)"; exit 2 }
if ($Changed.Count -gt 0) { Write-Host "PKGHUB: NOT RUN (package $($Stamp.commit) is stale, $($Changed.Count) file(s) changed since)"; exit 2 }
if (Test-Path $WorkRoot) { Remove-Item $WorkRoot -Recurse -Force }
New-Item -ItemType Directory -Force $WorkRoot | Out-Null

Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class DTHubWindows {
	delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
	[DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
	public static int Visible(int target) {
		int shown = 0;
		EnumWindows((h, l) => { uint pid; GetWindowThreadProcessId(h, out pid); if (pid == (uint)target && IsWindowVisible(h)) shown++; return true; }, IntPtr.Zero);
		return shown;
	}
}
"@

$script:Failures = @()
$script:ShownWindows = 0
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; $script:Failures += $Text }

# --- the owner's folder, read only: Windows' answer, so a changed environment cannot aim this at the wrong place -----------------
$OwnerSaved = Join-Path ([Environment]::GetFolderPath("LocalApplicationData")) "Dusktracer\Saved"
function Get-OwnerState {
	$Files = @(Get-ChildItem (Join-Path $OwnerSaved "SaveGames") -File -ErrorAction SilentlyContinue)
	$Settings = Join-Path $OwnerSaved "Config\DTSettings.ini"
	if (Test-Path $Settings) { $Files += Get-Item $Settings }
	return (@($Files | Sort-Object FullName | ForEach-Object { "{0} {1} {2} {3}" -f $_.Name, $_.Length, $_.LastWriteTimeUtc.Ticks, (Get-FileHash $_.FullName -Algorithm SHA256).Hash }) -join "`n")
}
$OwnerBefore = Get-OwnerState

# --- launches --------------------------------------------------------------------------------------------------------------
function Start-Game([string]$Config, [string]$UserDir, [string[]]$RunArgs, [switch]$Tool) {
	$Stub = Join-Path $PackageRoot "$Config\Windows\Dusktracer.exe"
	$ArgList = @("-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-userdir=`"$UserDir`"") + $RunArgs
	if (-not $Tool) {
		$Flag = @($ArgList | Where-Object { $_ -match '^-(unattended|nullrhi|DTBot|DTNoSave|DTMetaFile|DTSettingsFile|DTPoolGate)' -or $_ -ceq "-DTHub" })
		if ($Flag.Count) { throw "a person's launch was handed a tool flag: $($Flag -join ' ')" }
	}
	$Proc = Start-Process -FilePath $Stub -ArgumentList $ArgList -PassThru -WindowStyle Hidden
	$null = $Proc.Handle # PowerShell 5.1 forgets the exit code without it
	return $Proc
}

function Get-Games { return @(Get-Process -Name "Dusktracer*" -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($PackageRoot, [StringComparison]::OrdinalIgnoreCase) }) }

# Waits for the process to end, or for $Done, or for $Seconds; counts any visible window of the game on the way. True when it stopped waiting for a reason.
function Watch-Game($Proc, [int]$Seconds, [scriptblock]$Done) {
	$Deadline = (Get-Date).AddSeconds($Seconds)
	while ((Get-Date) -lt $Deadline) {
		foreach ($Game in Get-Games) { $script:ShownWindows += [DTHubWindows]::Visible($Game.Id) }
		if ($Proc.HasExited) { return $true }
		if ($Done -and (& $Done)) { return $true }
		Start-Sleep -Milliseconds 250
	}
	return $false
}

function Stop-Game($Proc) {
	foreach ($Game in Get-Games) { Stop-Process -Id $Game.Id -Force -ErrorAction SilentlyContinue }
	if (-not $Proc.HasExited) { Stop-Process -Id $Proc.Id -Force -ErrorAction SilentlyContinue }
	$null = $Proc.WaitForExit(5000)
}

function Get-RunFiles([string]$UserDir, [string]$Extension) {
	return @(Get-ChildItem (Join-Path $UserDir "Saved\Playtests") -Filter "*.$Extension" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime, Name)
}

function Get-SavePath([string]$UserDir) { return Join-Path $UserDir "Saved\SaveGames\DTMeta.sav" }

function Get-Field([string]$Text, [string]$Pattern) { if ($Text -match $Pattern) { return $Matches[1] }; return "" }

# Hub, save and verdict lines of one Development log; crash, warning and error lines fail the launch.
function Read-Log([string]$Tag, [string]$Log) {
	$Lines = if (Test-Path $Log) { @([IO.File]::ReadAllLines($Log)) } else { @() }
	if (-not $Lines.Count) { Note-Fail "$Tag has no log" }
	$Result = @{ Lines = $Lines; Opens = @(); Begins = @(); Meta = ""; Gated = 0; Picked = 0; NoHand = $false }
	foreach ($Line in $Lines) {
		if ($Line -match "LogDT: Hub: open, balance (\d+), runs (\d+), last patron '([^']*)'") { $Result.Opens += ,@([int64]$Matches[1], [int]$Matches[2], $Matches[3]) }
		elseif ($Line -match "LogDT: Hub: begin (.*)$") { $Result.Begins += $Matches[1].Trim() }
		elseif ($Line -match "LogDT: Meta: opened \(\d+\) (.*), balance \d+") { $Result.Meta = $Matches[1] }
		elseif ($Line -match "LogDT: Offers: pool gated, 0 unlocked") { $Result.Gated++ }
		elseif ($Line -match "LogDT: PATHCHECK: \w+ \(\d+ wrong, (\d+) hand") { $Result.Picked = [int]$Matches[1] }
		elseif ($Line -match "LogDT: PATHCHECK no hand before the death") { $Result.NoHand = $true }
	}
	$Crash = @($Lines | Where-Object { $_ -match "Fatal error|=== Critical error:? ===|Unhandled Exception|Ensure condition failed|LogDT: (Error|Fatal)" })
	if ($Crash.Count) { Note-Fail "$Tag crash marker: $($Crash[0])" }
	$Noise = @($Lines | Where-Object { $_ -cmatch ": (Warning|Error): " })
	if ($Noise.Count) { Note-Fail "$Tag log warning or error ($($Noise.Count)): $($Noise[0])" }
	$Lines | Where-Object { $_ -match "PATHCHECK .*: WRONG" } | ForEach-Object { Note-Fail "$Tag $($_ -replace '^.*LogDT: ', '')" }
	$Verdict = @($Lines | Where-Object { $_ -match "PATHCHECK: " } | ForEach-Object { $_ -replace "^.*LogDT: ", "" })
	if (-not ($Verdict | Where-Object { $_ -match "^PATHCHECK: PASS" })) { Note-Fail "$Tag verdict: $($Verdict -join ' ')" }
	else { Write-Host "    $Tag $($Verdict[-1])" }
	return $Result
}

# The tables' truth: rows, and rows whose zh-Hans differs from their en, against what the running game showed.
function Read-TextTruth {
	$Truth = @{}
	foreach ($File in Get-ChildItem $TextDir -Filter "ST_*.csv") {
		$Rows = @([IO.File]::ReadAllText($File.FullName) | ConvertFrom-Csv)
		$Truth[$File.BaseName] = @{ Keys = $Rows.Count; Zh = @($Rows | Where-Object { $_.'zh-Hans' -cne $_.en }).Count }
	}
	return $Truth
}

function Test-Text([string]$Tag, [string]$Culture, $Parsed) {
	foreach ($Name in $Truth.Keys) {
		$Row = @($Parsed.Lines | ForEach-Object { if ($_ -match "PATHCHECK table $Name keys=(\d+) localized=(\d+) missing=(\d+)") { ,@([int]$Matches[1], [int]$Matches[2], [int]$Matches[3]) } })
		if (-not $Row.Count) { Note-Fail "$Tag table $Name is not registered"; continue }
		$Want = if ($Culture -eq "zh-Hans") { $Truth[$Name].Zh } else { 0 }
		if ($Row[0][0] -ne $Truth[$Name].Keys) { Note-Fail "$Tag $Name holds $($Row[0][0]) keys, its CSV $($Truth[$Name].Keys)" }
		if ($Row[0][1] -ne $Want) { Note-Fail "$Tag $Name shows $($Row[0][1]) localized row(s) in $Culture, its CSV $Want" }
		if ($Row[0][2] -ne 0) { Note-Fail "$Tag $Name shows nothing for $($Row[0][2]) key(s)" }
	}
}

function Test-Shots([string]$Tag, [string]$Dir, [string[]]$Names) {
	foreach ($Name in $Names) {
		$Shot = Join-Path $Dir "$Name.png"
		if (-not (Test-Path $Shot)) { Note-Fail "$Tag no shot of $Name"; continue }
		$Img = & python -I (Join-Path $Root "Tools\image_check.py") $Shot 2>&1
		if ($LASTEXITCODE -ne 0) { Note-Fail "$Tag $Name : $(($Img | Select-String '^IMAGE:' | Select-Object -Last 1).Line)" }
	}
}

# One run file as its records say.
function Read-Run($File) {
	$Lines = @([IO.File]::ReadAllLines($File.FullName))
	$Award = @($Lines | Where-Object { $_ -match '"kind":"run_award"' }) | Select-Object -Last 1
	$End = $Lines | Select-Object -Last 1
	$Pick = @($Lines | Where-Object { $_ -match '"kind":"offer_pick"' }) | Select-Object -First 1
	return @{ Lines = $Lines; Culture = Get-Field $Lines[0] '"culture":"([^"]*)"'; Seed = Get-Field $Lines[0] '"seed":(\d+)'; Retained = [int64](Get-Field $Award '"retained":(\d+)')
		AwardEnd = Get-Field $Award '"end":"([^"]*)"'; Reason = Get-Field $End '"reason":"([^"]*)"'; Steps = Get-Field $End '"steps":(\d+)'; Patron = Get-Field $Pick '"id":"Keeper\.([A-Za-z]+)"'
		Decisions = [int](Get-Field $End '"decisions":(\d+)') } # the picks of every hand but the Patron's, which a Shrine choice takes by itself (offers.md R-OFF-08)
}

function Read-Locked {
	$Ids = @([IO.File]::ReadAllText((Join-Path $Root "Docs\09_Catalog\first_run_locked.csv")) | ConvertFrom-Csv | ForEach-Object { $_.id })
	foreach ($Node in [IO.File]::ReadAllText((Join-Path $Root "Docs\09_Catalog\tree.csv")) | ConvertFrom-Csv) {
		if ($Node.unlocks) { $Ids += @($Node.unlocks -split ';' | Where-Object { $_ }) }
	}
	return $Ids
}

# A first run's hands: nothing the Tree or the first-run list holds shut (meta.md R-MET-22, R-MET-25). Returns the hands past the Patron's, which every run has.
function Test-Gated([string]$Tag, $Run) {
	$Offers = @($Run.Lines | Where-Object { $_ -match '"kind":"offer"' })
	$Seen = @($Offers | ForEach-Object { [regex]::Matches($_, '"id":"([^"]+)"') | ForEach-Object { $_.Groups[1].Value } } | Sort-Object -Unique)
	$Shut = @($Seen | Where-Object { $Locked -contains $_ })
	if ($Shut.Count) { Note-Fail "$Tag offered what a first run keeps shut: $($Shut -join ' ')" }
	return @($Offers | Where-Object { $_ -notmatch '"hand_kind":"Patron"' }).Count
}

$Truth = Read-TextTruth
$Locked = Read-Locked
Write-Host ("pkghub: {0} tables, {1} ids shut on a first run, package {2}" -f $Truth.Count, $Locked.Count, $Stamp.commit)

# --- A: staged ------------------------------------------------------------------------------------------------------------
$UnrealPak = Join-Path $UERoot "Engine\Binaries\Win64\UnrealPak.exe"
$Hash = @{}
foreach ($Config in $Configs) {
	$Paks = Join-Path $PackageRoot "$Config\Windows\Dusktracer\Content\Paks"
	$Listing = & $UnrealPak (Join-Path $Paks "Dusktracer-Windows.pak") -List 2>&1 | ForEach-Object { "$_" }
	foreach ($Name in $Truth.Keys) {
		if (-not ($Listing | Where-Object { $_ -match "DT/Text/Generated/$Name\.csv" })) { Note-Fail "$Config has no staged $Name.csv" }
	}
	foreach ($Culture in "en", "zh-Hans") {
		if (-not ($Listing | Where-Object { $_ -match "Localization/Game/$Culture/Game\.locres" })) { Note-Fail "$Config has no $Culture Game.locres" }
	}
	$Hash[$Config] = @(Get-ChildItem $Paks -File | Where-Object { $_.Extension -in ".utoc", ".ucas" } | Sort-Object Name | ForEach-Object { "{0} {1} {2}" -f $_.Name, $_.Length, (Get-FileHash $_.FullName -Algorithm SHA256).Hash })
}
$SameCook = ($Hash.Development.Count -gt 0) -and (($Hash.Development -join "`n") -ceq ($Hash.Shipping -join "`n"))
if (-not $SameCook) { Note-Fail "the configs hold different cooks" }
Write-Host "pkghub: A staged, same cook=$SameCook"

# --- B, C: Development, real keys ---------------------------------------------------------------------------------------------
function Invoke-Script([string]$Tag, [string]$Script, [string]$Culture, [string]$UserDir, [string]$Shots, [string[]]$Extra) {
	$Log = Join-Path $LogDir "PkgHub_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	New-Item -ItemType Directory -Force $Shots | Out-Null
	$Watch = [Diagnostics.Stopwatch]::StartNew()
	$Proc = Start-Game "Development" $UserDir (@("-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTPathCheck=$Script", "-DTPathShots=`"$Shots`"") + @($Extra | Where-Object { $_ }))
	if (-not (Watch-Game $Proc $Timeout $null)) { Note-Fail "$Tag did not end by itself" }
	elseif ($Proc.ExitCode -ne 0) { Note-Fail "$Tag exit code $($Proc.ExitCode)" }
	Stop-Game $Proc
	Write-Host ("pkghub: {0} done in {1:n0} s" -f $Tag, $Watch.Elapsed.TotalSeconds)
	$Parsed = Read-Log $Tag $Log
	if (-not ($Parsed.Lines | Where-Object { $_ -match "Engine exit requested \(reason: Win RequestExit\)" })) { Note-Fail "$Tag did not leave through Quit to desktop" }
	Test-Text $Tag $Culture $Parsed
	Test-Shots $Tag $Shots @($Pages[$Script] | Where-Object { $_ -ne "hand" -or -not $Parsed.NoHand })
	return $Parsed
}

function Expect-Open([string]$Tag, $Open, [int64]$Balance, [int]$Runs, [string]$Patron) {
	if (-not $Open) { Note-Fail "$Tag the hub did not open"; return }
	if ($Open[0] -ne $Balance -or $Open[1] -ne $Runs -or $Open[2] -ne $Patron) { Note-Fail "$Tag hub reads balance $($Open[0]), runs $($Open[1]), patron '$($Open[2])'; expected $Balance, $Runs, '$Patron'" }
}

function Expect-Language([string]$Tag, [string]$UserDir, [string]$Culture) {
	$Other = if ($Culture -eq "en") { "zh-Hans" } else { "en" }
	$File = Join-Path $UserDir "Saved\Config\DTSettings.ini"
	$Text = if (Test-Path $File) { [IO.File]::ReadAllText($File) } else { "" }
	if ($Text -notmatch "Language=$Other\b") { Note-Fail "$Tag the settings file does not hold $Other" }
}

$Walked = @{}
$Walks = @{}
foreach ($Culture in "en", "zh-Hans") {
	$Dir = Join-Path $WorkRoot $Culture
	$User = Join-Path $Dir "user"
	$Shots = Join-Path $Dir "shots"
	$Patron = if ($Culture -eq "en") { "Storm" } else { "Forge" }
	$Tag = "walk_$Culture"
	# The driver's word that no hand came says nothing of the build (S3_package.md) and proves no hand, so the walk starts over on a clean profile.
	# Only that: a walk that hung or went wrong has already failed. A hand that never comes in $WalkTries walks fails too.
	for ($Try = 1; $Try -le $WalkTries; $Try++) {
		if (Test-Path $Dir) { Remove-Item $Dir -Recurse -Force }
		New-Item -ItemType Directory -Force $User | Out-Null
		$WalkSeed = $Seed + $Try - 1
		$SeedArg = if ($Culture -eq "en") { @("-DTSeed=$WalkSeed") } else { @() }
		$Log = Invoke-Script $Tag "Walk" $Culture $User $Shots ($SeedArg + @("-DTPathPatron=$Patron"))
		$Walks[$Culture] = $Try
		if (-not $Log.NoHand) { break }
		Write-Host ("    {0} walk {1} of {2}: no hand before the death on {3}" -f $Tag, $Try, $WalkTries, (Get-RunFiles $User "jsonl" | Select-Object -First 1).BaseName)
	}
	if ($Log.NoHand) { Note-Fail "$Tag no hand came in $WalkTries walks" }
	$Runs = Get-RunFiles $User "jsonl"
	if ($Runs.Count -ne 2) { Note-Fail "$Tag wrote $($Runs.Count) run file(s), expected 2"; continue }
	$First = Read-Run $Runs[0]
	$Second = Read-Run $Runs[1]
	if ($First.Reason -ne "death" -or $First.AwardEnd -ne "death") { Note-Fail "$Tag first run ends '$($First.Reason)', award '$($First.AwardEnd)'" }
	if ($Second.Reason -ne "abandoned") { Note-Fail "$Tag second run ends '$($Second.Reason)'" }
	if ($Second.Retained -le 0) { Note-Fail "$Tag second run earned nothing, so the bank's sum proves nothing" }
	foreach ($Run in $First, $Second) {
		if ($Run.Culture -ne $Culture) { Note-Fail "$Tag run_start reads culture '$($Run.Culture)'" }
		if ($Run.Patron -ne $Patron) { Note-Fail "$Tag the hand took '$($Run.Patron)'" }
	}
	if ($First.Seed -eq $Second.Seed) { Note-Fail "$Tag both runs of one launch played seed $($First.Seed)" }
	if ($SeedArg.Count -and $First.Seed -ne "$WalkSeed") { Note-Fail "$Tag first run on seed $($First.Seed), not the launch's $WalkSeed" }
	if ($First.Decisions -gt $Log.Picked) { Note-Fail "$Tag $($First.Decisions) hand(s) closed in the first run, only $($Log.Picked) by a key" }
	$Hands = Test-Gated $Tag $First
	if ($Log.Gated -ne ($Log.Opens.Count + $Log.Begins.Count)) { Note-Fail "$Tag $($Log.Gated) world(s) gated of $($Log.Opens.Count + $Log.Begins.Count)" }
	$Expected = (Get-SavePath $User) -replace "\\", "/"
	if ($Log.Meta -ne $Expected) { Note-Fail "$Tag save at '$($Log.Meta)', expected '$Expected'" }
	if (-not (Test-Path (Get-SavePath $User))) { Note-Fail "$Tag no save at $(Get-SavePath $User)" }
	if ($Log.Opens.Count -ne 3) { Note-Fail "$Tag hub opened $($Log.Opens.Count) time(s), expected 3" }
	else {
		Expect-Open "$Tag fresh" $Log.Opens[0] 0 0 "None"
		Expect-Open "$Tag banked" $Log.Opens[1] $First.Retained 1 $Patron
		Expect-Open "$Tag end" $Log.Opens[2] ($First.Retained + $Second.Retained) 2 $Patron
	}
	Write-Host ("    {0} seeds {1} / {2}, steps {3} / {4}, retained {5} / {6}, {7} hand(s) offered, begins: {8}" -f $Tag, $First.Seed, $Second.Seed, $First.Steps, $Second.Steps, $First.Retained, $Second.Retained, $Hands, ($Log.Begins -join " | "))
	$Tag = "reboot_$Culture"
	$Again = Invoke-Script $Tag "Reboot" $Culture $User $Shots $SeedArg
	if ($Again.Opens.Count -ne 1) { Note-Fail "$Tag hub opened $($Again.Opens.Count) time(s)" }
	else { Expect-Open $Tag $Again.Opens[0] ($First.Retained + $Second.Retained) 2 $Patron }
	if ($Again.Begins.Count) { Note-Fail "$Tag began a run" }
	Expect-Language $Tag $User $Culture
	$Walked[$Culture] = @{ User = $User; Csv = (Get-RunFiles $User "csv")[0]; First = $First; Balance = $First.Retained + $Second.Retained }
}

# --- D: Shipping, a person's boot ----------------------------------------------------------------------------------------------
$ShipUser = Join-Path $WorkRoot "shipping\user"
$ShipSave = Get-SavePath $ShipUser
New-Item -ItemType Directory -Force (Split-Path $ShipSave) | Out-Null
$BadBytes = [Text.Encoding]::ASCII.GetBytes("not a save")
[IO.File]::WriteAllBytes($ShipSave, $BadBytes)
$Proc = Start-Game "Shipping" $ShipUser @("-culture=en", "-DTSeed=$Seed")
$Gone = Watch-Game $Proc $BootWait $null
$Alive = -not $Proc.HasExited
Stop-Game $Proc
if (-not $Alive -or $Gone) { Note-Fail "D Shipping boot ended by itself" }
$Aside = "$ShipSave.bad.1"
$BadKept = (Test-Path $Aside) -and (([Convert]::ToBase64String([IO.File]::ReadAllBytes($Aside))) -ceq ([Convert]::ToBase64String($BadBytes)))
if (-not $BadKept) { Note-Fail "D a person's boot did not set the bad file aside at the default path" }
$BootRuns = (Get-RunFiles $ShipUser "jsonl").Count
if ($BootRuns -ne 0) { Note-Fail "D Shipping boot wrote $BootRuns run file(s): a run started with no hub" }
if (Test-Path $ShipSave) { Note-Fail "D the hub wrote a save" }
Write-Host ("pkghub: D Shipping boot alive {0} s, bad file aside={1}, {2} run file(s), save={3}" -f $BootWait, $BadKept, $BootRuns, (Test-Path $ShipSave))

# --- E, F, G: Shipping's bank, read by Development ------------------------------------------------------------------------------
function Invoke-ShippingRun([string]$Tag, [string]$Culture, [string]$UserDir, [int]$RunSeed) {
	$Save = Get-SavePath $UserDir
	$Known = @(Get-RunFiles $UserDir "jsonl" | ForEach-Object { $_.Name })
	$Before = if (Test-Path $Save) { (Get-Item $Save).LastWriteTimeUtc.Ticks } else { 0 }
	$Proc = Start-Game "Shipping" $UserDir @("-unattended", "-DTPoolGate", "-DTMetaFile=`"$Save`"", "-culture=$Culture", "-DTSeed=$RunSeed") -Tool
	$Watch = [Diagnostics.Stopwatch]::StartNew()
	$script:RunFileAt = -1.0
	$Over = Watch-Game $Proc $Timeout {
		$File = Get-RunFiles $UserDir "jsonl" | Where-Object { $Known -notcontains $_.Name } | Select-Object -Last 1
		if ($File -and $script:RunFileAt -lt 0) { $script:RunFileAt = $Watch.Elapsed.TotalSeconds }
		$File -and ([IO.File]::ReadAllText($File.FullName) -match '"kind":"run_end"') -and (Test-Path $Save) -and ((Get-Item $Save).LastWriteTimeUtc.Ticks -ne $Before)
	}
	$Alive = -not $Proc.HasExited
	Stop-Game $Proc
	if (-not $Over) { Note-Fail "$Tag the run never ended" }
	if (-not $Alive) { Note-Fail "$Tag the process left by itself" }
	$File = Get-RunFiles $UserDir "jsonl" | Where-Object { $Known -notcontains $_.Name } | Select-Object -Last 1
	if (-not $File) { Note-Fail "$Tag no run file"; return $null }
	$Run = Read-Run $File
	if ($Run.Reason -ne "death") { Note-Fail "$Tag ends '$($Run.Reason)'" }
	if ($Run.Culture -ne $Culture) { Note-Fail "$Tag run_start reads culture '$($Run.Culture)'" }
	if ($Run.Seed -ne "$RunSeed") { Note-Fail "$Tag run_start reads seed '$($Run.Seed)'" }
	$Hands = Test-Gated $Tag $Run
	Write-Host ("pkghub: {0} Shipping done in {1:n0} s, run file at {2:n1} s, patron {3}, steps {4}, retained {5}, {6} hand(s) offered" -f $Tag, $Watch.Elapsed.TotalSeconds, $script:RunFileAt, $Run.Patron, $Run.Steps, $Run.Retained, $Hands)
	if ($script:RunFileAt -lt 0 -or $script:RunFileAt -ge $BootWait) { Note-Fail "$Tag a run's file took $($script:RunFileAt) s, so D's $BootWait s proves no hub" }
	return $Run
}

$CsvOk = $false
$EnSeed = if ($Walked["en"]) { [int]$Walked["en"].First.Seed } else { $Seed }
$Fresh = Invoke-ShippingRun "E" "en" $ShipUser $EnSeed
if ($Fresh) {
	if ($Walked["en"]) {
		$A = [IO.File]::ReadAllText($Walked["en"].Csv.FullName)
		$B = [IO.File]::ReadAllText((Get-RunFiles $ShipUser "csv" | Select-Object -Last 1).FullName)
		$CsvOk = ($A.Length -gt 0) -and ($A -ceq $B)
		if (-not $CsvOk) { Note-Fail "E Shipping's run is not the en walk's first run: steps $($Walked['en'].First.Steps) / $($Fresh.Steps), retained $($Walked['en'].First.Retained) / $($Fresh.Retained)" }
		Write-Host "    E csv same as the en walk's first run=$CsvOk"
	}
	$Read = Invoke-Script "read_shipping" "Reboot" "en" $ShipUser (Join-Path $WorkRoot "shipping\shots") @()
	if ($Read.Opens.Count -eq 1) { Expect-Open "F Development reading Shipping's save" $Read.Opens[0] $Fresh.Retained 1 $Fresh.Patron } else { Note-Fail "F hub opened $($Read.Opens.Count) time(s)" }
}
if ($Walked["zh-Hans"]) {
	$On = $Walked["zh-Hans"]
	$Top = Invoke-ShippingRun "G" "zh-Hans" $On.User ($Seed + 1)
	if ($Top) {
		$Read = Invoke-Script "read_top" "Reboot" "zh-Hans" $On.User (Join-Path $WorkRoot "zh-Hans\shots_top") @()
		if ($Read.Opens.Count -eq 1) { Expect-Open "G Development reading the sum" $Read.Opens[0] ($On.Balance + $Top.Retained) 3 $Top.Patron } else { Note-Fail "G hub opened $($Read.Opens.Count) time(s)" }
	}
}

# --- H: the seed's hash in both configs ----------------------------------------------------------------------------------------
$Bot = @("-unattended", "-culture=en", "-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=0.6", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotFast=60", "-DTBotQuit", "-DTBotMaxSeconds=90", "-DTSeed=$Seed")
$Hashes = @{}
foreach ($Config in $Configs) {
	$User = Join-Path $WorkRoot "hash_$Config"
	$Save = Get-SavePath $User
	New-Item -ItemType Directory -Force (Split-Path $Save) | Out-Null
	[IO.File]::WriteAllBytes($Save, $BadBytes)
	$Out = Join-Path $User "bot.csv"
	$Proc = Start-Game $Config $User ($Bot + @("-DTBotOut=`"$Out`"")) -Tool
	if (-not (Watch-Game $Proc $Timeout $null)) { Note-Fail "H $Config timed out" }
	Stop-Game $Proc
	$Record = if (Test-Path "$Out.txt") { [IO.File]::ReadAllText("$Out.txt") } else { "" }
	$Hashes[$Config] = Get-Field $Record 'input_hash=([0-9a-f]+)'
	if ((Test-Path "$Save.bad.1") -or ([IO.File]::ReadAllBytes($Save).Length -ne $BadBytes.Length)) { Note-Fail "H ${Config}: a tool run touched the file at the default path" }
}
if (-not $Hashes.Development -or $Hashes.Development -ne $Hashes.Shipping) { Note-Fail "H input_hash Development '$($Hashes.Development)', Shipping '$($Hashes.Shipping)'" }
Write-Host "pkghub: H seed $Seed input_hash Development $($Hashes.Development), Shipping $($Hashes.Shipping)"

# --- what the launches left --------------------------------------------------------------------------------------------------
$Crashes = @(Get-ChildItem $WorkRoot -Recurse -Directory -Filter "Crashes" | Where-Object { @(Get-ChildItem $_.FullName -Force).Count -gt 0 })
if ($Crashes.Count) { Note-Fail "a crash folder has files: $($Crashes[0].FullName)" }
$OwnerAfter = Get-OwnerState
if ($OwnerAfter -cne $OwnerBefore) { Note-Fail "the owner's save or settings changed" }
$Strays = @()
foreach ($Dir in Get-ChildItem $WorkRoot -Recurse -Directory -Filter "Playtests") {
	foreach ($File in Get-ChildItem $Dir.FullName -File) { $Strays += @(Get-ChildItem (Join-Path $OwnerSaved "Playtests") -Filter $File.Name -ErrorAction SilentlyContinue) }
}
if ($Strays.Count) { Note-Fail "a run file of this check is in the owner's folder: $($Strays[0].Name)" }
if ($script:ShownWindows -gt 0) { Note-Fail "a game window was visible ($($script:ShownWindows) sample(s))" }

$Summary = "package {0}, same cook={1}, {2} tables read in en and zh-Hans, walks {3} en on seed {4} + {5} zh-Hans, Shipping csv same as the walk={6}, seed {7} hash {8}, owner's save unchanged={9}, windows {10}" -f $Stamp.commit, $SameCook, $Truth.Count, $Walks["en"], $EnSeed, $Walks["zh-Hans"], $CsvOk, $Seed, $Hashes.Development, ($OwnerAfter -ceq $OwnerBefore), $script:ShownWindows
if ($script:Failures.Count) {
	Write-Host "PKGHUB: FAIL ($($script:Failures.Count) failure(s), first: $($script:Failures[0]); $Summary)"
	exit 1
}
Write-Host "PKGHUB: PASS ($Summary)"
exit 0
