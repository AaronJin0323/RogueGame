# Seed check (gate step `seed`, 07_Technical R-TEC-16, T-TEC-16; package half T-PKG-04): every human launch really gets a fresh seed, off-screen.
#   & Tools\SeedCheck.ps1 [-Package <folder of Play_*.bat>] [-LoadTimeout 900]
# Each launcher is read by running it: cmd expands a copy whose `start` line echoes instead, so the check sees what the launcher would start, not a
# fragment of its text. The game then starts with exactly those arguments, an off-screen window added and no -unattended, which is
# what makes it a human launch. The seed read back is run_start.seed in the run file that launch wrote, and the file name must agree. run_start.driver
# is read from the same line (telemetry.md R-TEL-13, T-TEL-13): person on a launcher's arguments, tool on a named seed or -unattended.
# Every launch runs on the hidden desktop of Tools/HiddenLaunch.ps1: a launch with no -unattended is one the engine may prompt on, and nothing it opens can
# reach the desktop anyone is looking at. Windows seen there are written to the log, by owner, class and title.
# Every launch also has a -userdir of its own, under Saved\SeedCheck: the only run file in there is that launch's, so the check never has to guess which file
# is its own, and it neither reads nor deletes anything in %LOCALAPPDATA%\Dusktracer or Saved\Playtests, where a person's runs live. The last check's folders,
# with each launch's log, stay until the next one.
# Without -Package the game is the editor build (UnrealEditor -game), as Tools\Play.bat starts it; Play_*.bat name a packaged Dusktracer.exe, so their
# arguments run on the editor build here (Tools\PadCheck.ps1 passes -Package).
# A person's launch opens the hub and a run starts only at Begin (vigil.md R-VIG-01, R-VIG-07), so a launch that must reach a run passes -DTHubCheck=Begin, the
# keys a person presses. Shipping has no such flag and no keys, so it never gets past the hub with no hands: -Package reads the launchers, an explicit seed and
# the rejections on Shipping, and starts the launchers' arguments on the package's Development build, where the fresh seed can be read (D-028).
# Prints "SEEDCHECK: PASS|FAIL (...)"; exit 0 / 1. Log: Saved\Logs\SeedCheck.log.
param(
	[string]$Package = "",
	[int]$LoadTimeout = 900
)

trap { Write-Host "SEEDCHECK: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Editor = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = "`"$(Join-Path $Root "Dusktracer.uproject")`""
$LogDir = Join-Path $Root "Saved\Logs"
$WorkDir = Join-Path $Root "Saved\SeedCheck"
New-Item -ItemType Directory -Force $LogDir, $WorkDir | Out-Null
$Log = Join-Path $LogDir "SeedCheck.log"
Set-Content -Path $Log -Value "" -Encoding ascii

if ($Package) { $Package = (Resolve-Path $Package).Path }
$UserDirs = Join-Path $WorkDir $(if ($Package) { "package" } else { "editor" })
if (Test-Path $UserDirs) { Remove-Item $UserDirs -Recurse -Force }
$GameExe = if ($Package) { Join-Path $Package "Dusktracer.exe" } else { $Editor }
$OffScreen = @("-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720")
# With no -unattended the editor starts a Live Coding session, whose mutex is named from the editor's path: every worktree shares it, and a build in any of
# them fails while it is held (Tools\Play.bat passes the same flag). Shipping has no Live Coding.
if (-not $Package) { $OffScreen += "-LiveCoding=false" }
# -RenderOffScreen runs the null application, which creates no native window.
$Tool = $OffScreen + @("-unattended")
# Source/Dusktracer/Hub/DTHubCheck.cpp BuildBegin: real key events through the hub. Not a tool flag, so the launch stays a person's.
$Begin = @("-DTHubCheck=Begin")
# FDTRunSeed::FreshMin, FreshMax, RejectedExitCode (Source/Dusktracer/Core/DTRunSeed.h).
$FreshMin = 1
$FreshMax = [int]::MaxValue - 1
$RejectedExit = 2

$script:Failures = @()
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; Add-Content -Path $Log -Value "  ! $Text"; $script:Failures += $Text }
function Note([string]$Text) { Write-Host "    $Text"; Add-Content -Path $Log -Value "    $Text" }

function Read-Launcher([string]$Bat, [string]$Culture) {
	# Returns the arguments the launcher would start the game with, or $null after noting why it cannot be judged.
	$Name = Split-Path $Bat -Leaf
	if (-not (Test-Path $Bat)) { Note-Fail "$Name is missing"; return $null }
	$Text = [IO.File]::ReadAllText($Bat)
	$Start = '(?m)^[ \t]*start[ \t]+""[ \t]+'
	if ([regex]::Matches($Text, $Start).Count -ne 1) { Note-Fail "$Name does not have one start line"; return $null }
	$Echoing = [regex]::Replace($Text, $Start, "echo DTLAUNCH ").Replace("%~dp0", (Split-Path $Bat -Parent) + "\")
	$Copy = Join-Path $WorkDir $Name
	[IO.File]::WriteAllText($Copy, $Echoing, [Text.Encoding]::ASCII)
	$Out = @(& cmd /c "`"$Copy`" $Culture" 2>&1)
	Remove-Item $Copy -Force
	$Line = ($Out | Where-Object { "$_" -match "^DTLAUNCH " } | Select-Object -First 1)
	if (-not $Line) { Note-Fail "$Name started nothing: $($Out -join ' | ')"; return $null }
	$Tokens = @([regex]::Matches("$Line", '"[^"]*"|\S+') | ForEach-Object { $_.Value } | Select-Object -Skip 1)
	Add-Content -Path $Log -Value "launcher ${Name}: $($Tokens -join ' ')"
	$Exe = $Tokens[0].Trim('"')
	$Arguments = @($Tokens | Select-Object -Skip 1)
	$Ok = $true
	if ($Arguments -notcontains "-culture=$Culture") { Note-Fail "$Name does not pass -culture=$Culture"; $Ok = $false }
	# No launcher names a window mode: a -fullscreen here would beat Settings' Window row on every launch (settings.md R-SET-08, review #43).
	$Mode = @($Arguments | Where-Object { $_ -match '^-(fullscreen|windowed)$' })
	if ($Mode.Count) { Note-Fail "$Name names a window mode ($($Mode -join ' ')): Settings' Window row decides it"; $Ok = $false }
	$Named = @($Arguments | Where-Object { $_ -match "(?i)DTSeed|%" })
	if ($Named.Count) { Note-Fail "$Name names a seed or left a variable: $($Named -join ' ')"; $Ok = $false }
	if ($Package -and $Exe -ne $GameExe) { Note-Fail "$Name starts $Exe, not $GameExe"; $Ok = $false }
	if (-not $Ok) { return $null }
	# Every argument is the launcher's own word. Play_*.bat name no project or map because the packaged exe needs none.
	$Kept = @($Arguments)
	if (-not $Package -and ($Kept -notcontains "-game")) { $Kept = @($Project, "-game") + $Kept }
	return , $Kept
}

function Invoke-Launch([string]$Label, [string[]]$ArgList) {
	# One launch. Result: Seed ($null when no run file), Exited (by itself, with no run file) and its ExitCode.
	# The editor takes the project only as its first argument and opens a dialog without it, so nothing starts in any other order.
	if (-not $Package -and $ArgList[0] -ne $Project) { throw "$Label would start without the project first" }
	$script:Launches++
	$UserDir = Join-Path $UserDirs $script:Launches
	$Playtests = Join-Path $UserDir "Saved\Playtests"
	New-Item -ItemType Directory -Force $UserDir | Out-Null
	$ArgList += "-userdir=`"$UserDir`""
	$Started = Get-Date
	$Proc = Start-DTHidden $GameExe $ArgList
	Add-Content -Path $Log -Value "launch ${Label} (PID $($Proc.Id)): $($ArgList -join ' ')"
	$Run = $null
	$Seed = $null
	$Driver = ""
	$Exited = $false
	$Windows = @{}
	try {
		$Deadline = (Get-Date).AddSeconds($LoadTimeout)
		while ((Get-Date) -lt $Deadline) {
			foreach ($Window in Get-DTHiddenWindows) { $Windows[$Window] = $true }
			$Run = Get-ChildItem $Playtests -Filter *.jsonl -ErrorAction SilentlyContinue | Select-Object -First 1
			if ($Run) {
				$First = Get-Content $Run.FullName -TotalCount 1 -Encoding UTF8 -ErrorAction SilentlyContinue
				if ($First -match '"kind":"run_start".*"seed":(-?\d+)') {
					$Seed = [int64]$Matches[1]
					if ($First -match '"driver":"(\w+)"') { $Driver = $Matches[1] }
					break
				}
			}
			elseif ($Proc.HasExited) { $Exited = $true; break }
			Start-Sleep -Milliseconds 250
		}
	}
	finally {
		Stop-DTHidden $Proc
	}
	$Result = [pscustomobject]@{ Seed = $Seed; Driver = $Driver; Exited = $Exited; ExitCode = $null; Seconds = [int]((Get-Date) - $Started).TotalSeconds }
	if ($Exited) { $Result.ExitCode = $Proc.ExitCode }
	if ($null -ne $Seed -and $Run.Name -notmatch "^${Seed}_") { Note-Fail "${Label}: run_start.seed $Seed is not the file name $($Run.Name)" }
	foreach ($Window in $Windows.Keys) {
		$Owner = $Window -split " ", 2
		$Name = (Get-Process -Id ([int]$Owner[0]) -ErrorAction SilentlyContinue).ProcessName
		Add-Content -Path $Log -Value "  window on the hidden desktop during ${Label}: $Name $Window"
	}
	return $Result
}

function Get-NoRun($R) { if ($R.Exited) { "exited with code $($R.ExitCode), no run" } else { "no run in $LoadTimeout s" } }

function Expect-Fresh([string]$Label, [string[]]$ArgList) {
	$R = Invoke-Launch $Label ($ArgList + $OffScreen + $Begin)
	if ($null -eq $R.Seed) { Note-Fail "${Label}: $(Get-NoRun $R)" }
	elseif ($R.Seed -lt $FreshMin -or $R.Seed -gt $FreshMax) { Note-Fail "${Label}: seed $($R.Seed) is not in $FreshMin..$FreshMax" }
	elseif ($script:Fresh -contains $R.Seed) { Note-Fail "${Label}: seed $($R.Seed) was drawn by an earlier launch" }
	elseif ($R.Driver -ne "person") { Note-Fail "${Label}: run_start reads driver '$($R.Driver)', a launcher's arguments are a person's (R-TEL-13)" }
	else { $script:Fresh += $R.Seed; Note "$Label fresh seed $($R.Seed), driver person ($($R.Seconds) s)" }
}

function Expect-Seed([string]$Label, [string[]]$ArgList, [int64]$Wanted) {
	$R = Invoke-Launch $Label $ArgList
	if ($null -eq $R.Seed) { Note-Fail "${Label}: $(Get-NoRun $R)" }
	elseif ($R.Seed -ne $Wanted) { Note-Fail "${Label}: seed $($R.Seed), expected $Wanted" }
	elseif ($R.Driver -ne "tool") { Note-Fail "${Label}: run_start reads driver '$($R.Driver)', a named seed or -unattended is a tool's (R-TEL-13)" }
	else { $script:Exact++; Note "$Label exact seed $Wanted, driver tool ($($R.Seconds) s)" }
}

function Expect-Rejected([string]$Label, [string[]]$ArgList) {
	$R = Invoke-Launch $Label $ArgList
	if ($null -ne $R.Seed) { Note-Fail "${Label}: ran on seed $($R.Seed)" }
	elseif (-not $R.Exited) { Note-Fail "${Label}: did not exit in $LoadTimeout s" }
	elseif ($R.ExitCode -ne $RejectedExit) { Note-Fail "${Label}: exit code $($R.ExitCode), expected $RejectedExit" }
	else { $script:Rejected++; Note "$Label rejected, exit code $($R.ExitCode), no run file ($($R.Seconds) s)" }
}

$script:Fresh = @()
$script:Exact = 0
$script:Rejected = 0
$script:Launches = 0
$Overflow = "-DTSeed=3276732767" # the old launcher's %RANDOM%%RANDOM% at its highest
$JustOver = "-DTSeed=2147483648"

# Order matters. Launchers are read first, which starts no game. Unattended launches go next: -unattended answers every engine prompt, so a stale build
# fails there. Only after those pass do the launches with no -unattended run, the ones that are human launches and could show a dialog.
$Human = [ordered]@{}
if ($Package) {
	foreach ($Culture in "en", "zh-Hans") {
		$Arguments = Read-Launcher (Join-Path $Package "Play_$Culture.bat") $Culture
		if ($null -ne $Arguments) { $Human["Play_$Culture.bat on Development"] = $Arguments }
	}
	$Human["bare Dusktracer.exe on Development"] = @()
	Expect-Seed "explicit seed on Shipping" ($Tool + @("-DTSeed=9107")) 9107
	Expect-Rejected "the old launcher's overflow on Shipping" ($Tool + @($Overflow))
}
else {
	$Launchers = [ordered]@{ "Tools\Play.bat" = "en"; "Tools\PackageFiles\Play_en.bat" = "en"; "Tools\PackageFiles\Play_zh-Hans.bat" = "zh-Hans" }
	foreach ($Bat in $Launchers.Keys) {
		$Arguments = Read-Launcher (Join-Path $Root $Bat) $Launchers[$Bat]
		if ($null -ne $Arguments) { $Human[$Bat] = $Arguments }
	}
	$Human["bare launch"] = @($Project, "-game")
	$Game = @($Project, "-game") + $Tool
	Expect-Seed "explicit 0" ($Game + @("-DTSeed=0")) 0
	Expect-Seed "explicit top of range" ($Game + @("-DTSeed=2147483647")) 2147483647
	Expect-Seed "tool, no seed" $Game 0
	Expect-Rejected "one past the range" ($Game + @($JustOver))
	Expect-Rejected "the old launcher's overflow" ($Game + @($Overflow))
}
if ($script:Failures.Count -eq 0) {
	# A person's launch reaches the hub, so this reads the rejection ahead of it: with it behind, the launch would sit there until the timeout.
	$Bad = @($Overflow) + $OffScreen
	if (-not $Package) { $Bad = @($Project, "-game") + $Bad }
	Expect-Rejected "the overflow on a person's launch" $Bad
	if ($Package) {
		$GameExe = Join-Path (Split-Path (Split-Path $Package -Parent) -Parent) "Development\Windows\Dusktracer.exe"
		if (-not (Test-Path $GameExe)) { Note-Fail "no Development build at $GameExe" }
	}
}
if ($script:Failures.Count -eq 0) {
	foreach ($Label in $Human.Keys) { Expect-Fresh $Label $Human[$Label] }
	$Replay = @("-DTSeed=9107") + $OffScreen + $Begin
	if (-not $Package) { $Replay = @($Project, "-game") + $Replay }
	Expect-Seed "replay by hand, no -unattended" $Replay 9107
}

$Line = "{0} human launches on fresh seeds, {1} exact, {2} rejected" -f $script:Fresh.Count, $script:Exact, $script:Rejected
if ($script:Failures.Count) { Write-Host "SEEDCHECK: FAIL ($($script:Failures.Count) failure(s), first: $($script:Failures[0]))"; exit 1 }
Write-Host "SEEDCHECK: PASS ($Line)"
exit 0
