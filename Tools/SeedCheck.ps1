# Seed check (gate step `seed`, 07_Technical R-TEC-16, T-TEC-16; package half T-PKG-04): every human launch really gets a fresh seed, off-screen.
#   & Tools\SeedCheck.ps1 [-Package <folder of Play_*.bat>] [-LoadTimeout 900]
# Each launcher is read by running it: cmd expands a copy whose `start` line echoes instead, so the check sees what the launcher would start, not a
# fragment of its text. The game then starts with exactly those arguments, -fullscreen swapped for an off-screen window and no -unattended, which is
# what makes it a human launch. The seed read back is run_start.seed in the run file that launch wrote, and the file name must agree.
# Every launch runs on a hidden Win32 desktop of its own: a launch with no -unattended is one the engine may prompt on, and nothing it opens can reach the
# desktop anyone is looking at. Windows seen there are written to the log, by owner, class and title.
# Without -Package the game is the editor build (UnrealEditor -game), as Tools\Play.bat starts it; Play_*.bat name a packaged Dusktracer.exe, so their
# arguments run on the editor build here and on the real Shipping build with -Package (Tools\PadCheck.ps1). The check's run files are deleted.
# Prints "SEEDCHECK: PASS|FAIL (...)"; exit 0 / 1. Log: Saved\Logs\SeedCheck.log.
param(
	[string]$Package = "",
	[int]$LoadTimeout = 900
)

trap { Write-Host "SEEDCHECK: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Editor = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = "`"$(Join-Path $Root "Dusktracer.uproject")`""
$LogDir = Join-Path $Root "Saved\Logs"
$WorkDir = Join-Path $Root "Saved\SeedCheck"
New-Item -ItemType Directory -Force $LogDir, $WorkDir | Out-Null
$Log = Join-Path $LogDir "SeedCheck.log"
Set-Content -Path $Log -Value "" -Encoding ascii

if ($Package) { $Package = (Resolve-Path $Package).Path }
$Playtests = if ($Package) { Join-Path $env:LOCALAPPDATA "Dusktracer\Saved\Playtests" } else { Join-Path $Root "Saved\Playtests" }
$GameExe = if ($Package) { Join-Path $Package "Dusktracer.exe" } else { $Editor }
$OffScreen = @("-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720")
# Never -nullrhi here: UnrealEditor -game with it still creates a real game window (LogEngine: GameWindow did not exist). -RenderOffScreen creates none.
$Tool = $OffScreen + @("-unattended")
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
	if ($Arguments -notcontains "-fullscreen") { Note-Fail "$Name does not start full screen"; $Ok = $false }
	$Named = @($Arguments | Where-Object { $_ -match "(?i)DTSeed|%" })
	if ($Named.Count) { Note-Fail "$Name names a seed or left a variable: $($Named -join ' ')"; $Ok = $false }
	if ($Package -and $Exe -ne $GameExe) { Note-Fail "$Name starts $Exe, not $GameExe"; $Ok = $false }
	if (-not $Ok) { return $null }
	# Everything but -fullscreen is the launcher's own word. Play_*.bat name no project or map because the packaged exe needs none.
	$Kept = @($Arguments | Where-Object { $_ -ne "-fullscreen" })
	if (-not $Package -and ($Kept -notcontains "-game")) { $Kept = @($Project, "-game") + $Kept }
	return , $Kept
}

function Get-Runs { if (Test-Path $Playtests) { @(Get-ChildItem $Playtests -Filter *.jsonl | ForEach-Object { $_.FullName }) } else { @() } }

Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
public static class DTHiddenDesktop {
	delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
	[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
	struct StartupInfo { public int cb; public string reserved; public string desktop; public string title; public int x, y, w, h, cx, cy, fill, flags; public short show, reserved2; public IntPtr reserved3, stdIn, stdOut, stdErr; }
	[StructLayout(LayoutKind.Sequential)]
	struct ProcessInformation { public IntPtr process, thread; public int pid, tid; }
	[DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr CreateDesktopW(string name, IntPtr device, IntPtr mode, uint flags, uint access, IntPtr security);
	[DllImport("user32.dll")] static extern bool EnumDesktopWindows(IntPtr desktop, EnumProc proc, IntPtr lParam);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hWnd, StringBuilder text, int max);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr hWnd, StringBuilder text, int max);
	[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CreateProcessW(string app, StringBuilder command, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string cwd, ref StartupInfo si, out ProcessInformation pi);
	[DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
	const uint GenericAll = 0x10000000;
	static IntPtr Desktop;
	static string Name;
	public static void Open(string name) {
		Name = name;
		Desktop = CreateDesktopW(name, IntPtr.Zero, IntPtr.Zero, 0, GenericAll, IntPtr.Zero);
		if (Desktop == IntPtr.Zero) { throw new Win32Exception(); }
	}
	public static int Start(string exe, string arguments) {
		StartupInfo si = new StartupInfo();
		si.cb = Marshal.SizeOf(typeof(StartupInfo));
		si.desktop = "WinSta0\\" + Name;
		ProcessInformation pi;
		StringBuilder line = new StringBuilder("\"" + exe + "\" " + arguments);
		if (!CreateProcessW(exe, line, IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, null, ref si, out pi)) { throw new Win32Exception(); }
		CloseHandle(pi.thread);
		CloseHandle(pi.process);
		return pi.pid;
	}
	public static string[] Visible() {
		List<string> found = new List<string>();
		EnumDesktopWindows(Desktop, delegate(IntPtr hWnd, IntPtr lParam) {
			if (IsWindowVisible(hWnd)) {
				uint pid;
				GetWindowThreadProcessId(hWnd, out pid);
				StringBuilder title = new StringBuilder(256);
				StringBuilder type = new StringBuilder(256);
				GetWindowTextW(hWnd, title, 256);
				GetClassNameW(hWnd, type, 256);
				found.Add(pid + " " + type + " '" + title + "'");
			}
			return true;
		}, IntPtr.Zero);
		return found.ToArray();
	}
}
"@
[DTHiddenDesktop]::Open("DTSeedCheck$PID")

function Invoke-Launch([string]$Label, [string[]]$ArgList) {
	# One launch. Result: Seed ($null when no run file), Exited (by itself, with no run file) and its ExitCode.
	# The editor takes the project only as its first argument and opens a dialog without it, so nothing starts in any other order.
	if (-not $Package -and $ArgList[0] -ne $Project) { throw "$Label would start without the project first" }
	$Before = Get-Runs
	$Started = Get-Date
	$Proc = [Diagnostics.Process]::GetProcessById([DTHiddenDesktop]::Start($GameExe, ($ArgList -join " ")))
	$null = $Proc.Handle # ExitCode needs the handle held from the start
	Add-Content -Path $Log -Value "launch ${Label} (PID $($Proc.Id)): $($ArgList -join ' ')"
	$Run = $null
	$Seed = $null
	$Exited = $false
	$Windows = @{}
	try {
		$Deadline = (Get-Date).AddSeconds($LoadTimeout)
		while ((Get-Date) -lt $Deadline) {
			foreach ($Window in [DTHiddenDesktop]::Visible()) { $Windows[$Window] = $true }
			$Run = Get-Runs | Where-Object { $Before -notcontains $_ } | Select-Object -First 1
			if ($Run) {
				$First = Get-Content $Run -TotalCount 1 -Encoding UTF8 -ErrorAction SilentlyContinue
				if ($First -match '"kind":"run_start".*"seed":(-?\d+)') { $Seed = [int64]$Matches[1]; break }
			}
			elseif ($Proc.HasExited) { $Exited = $true; break }
			Start-Sleep -Milliseconds 250
		}
	}
	finally {
		if (-not $Proc.HasExited) { & taskkill /PID $Proc.Id /T /F | Out-Null; $Proc.WaitForExit(30000) | Out-Null }
	}
	$Result = [pscustomobject]@{ Seed = $Seed; Exited = $Exited; ExitCode = $null; Seconds = [int]((Get-Date) - $Started).TotalSeconds }
	if ($Exited) { $Result.ExitCode = $Proc.ExitCode }
	if ($Run) {
		if ($null -ne $Seed -and (Split-Path $Run -Leaf) -notmatch "^${Seed}_") { Note-Fail "${Label}: run_start.seed $Seed is not the file name $(Split-Path $Run -Leaf)" }
		Remove-Item $Run, ([IO.Path]::ChangeExtension($Run, ".csv")) -Force -ErrorAction SilentlyContinue
	}
	foreach ($Window in $Windows.Keys) {
		$Owner = $Window -split " ", 2
		$Name = (Get-Process -Id ([int]$Owner[0]) -ErrorAction SilentlyContinue).ProcessName
		Add-Content -Path $Log -Value "  window on the hidden desktop during ${Label}: $Name $Window"
	}
	return $Result
}

function Get-NoRun($R) { if ($R.Exited) { "exited with code $($R.ExitCode), no run" } else { "no run in $LoadTimeout s" } }

function Expect-Fresh([string]$Label, [string[]]$ArgList) {
	$R = Invoke-Launch $Label ($ArgList + $OffScreen)
	if ($null -eq $R.Seed) { Note-Fail "${Label}: $(Get-NoRun $R)" }
	elseif ($R.Seed -lt $FreshMin -or $R.Seed -gt $FreshMax) { Note-Fail "${Label}: seed $($R.Seed) is not in $FreshMin..$FreshMax" }
	elseif ($script:Fresh -contains $R.Seed) { Note-Fail "${Label}: seed $($R.Seed) was drawn by an earlier launch" }
	else { $script:Fresh += $R.Seed; Note "$Label fresh seed $($R.Seed) ($($R.Seconds) s)" }
}

function Expect-Seed([string]$Label, [string[]]$ArgList, [int64]$Wanted) {
	$R = Invoke-Launch $Label $ArgList
	if ($null -eq $R.Seed) { Note-Fail "${Label}: $(Get-NoRun $R)" }
	elseif ($R.Seed -ne $Wanted) { Note-Fail "${Label}: seed $($R.Seed), expected $Wanted" }
	else { $script:Exact++; Note "$Label exact seed $Wanted ($($R.Seconds) s)" }
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
$Overflow = "-DTSeed=3276732767" # the old launcher's %RANDOM%%RANDOM% at its highest
$JustOver = "-DTSeed=2147483648"

# Order matters. Launchers are read first, which starts no game. Unattended launches go next: -unattended answers every engine prompt, so a stale build
# fails there. Only after those pass do the launches with no -unattended run, the ones that are human launches and could show a dialog.
$Human = [ordered]@{}
if ($Package) {
	foreach ($Culture in "en", "zh-Hans") {
		$Arguments = Read-Launcher (Join-Path $Package "Play_$Culture.bat") $Culture
		if ($null -ne $Arguments) { $Human["Play_$Culture.bat on Shipping"] = $Arguments }
	}
	$Human["bare Dusktracer.exe"] = @()
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
	foreach ($Label in $Human.Keys) { Expect-Fresh $Label $Human[$Label] }
	$Replay = if ($Package) { @("-DTSeed=9107") } else { @($Project, "-game", "-DTSeed=9107") }
	Expect-Seed "replay by hand, no -unattended" ($Replay + $OffScreen) 9107
}

$Line = "{0} human launches on fresh seeds, {1} exact, {2} rejected" -f $script:Fresh.Count, $script:Exact, $script:Rejected
if ($script:Failures.Count) { Write-Host "SEEDCHECK: FAIL ($($script:Failures.Count) failure(s), first: $($script:Failures[0]))"; exit 1 }
Write-Host "SEEDCHECK: PASS ($Line)"
exit 0
