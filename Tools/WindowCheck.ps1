# Window check (gate step `windows`, 07_Technical R-TEC-17, T-TEC-17): a bot run, a smoke run and a test run, each through its real tool, show nothing on this desktop.
#   & Tools\WindowCheck.ps1 [-Timeout 300]
# No game or editor outlives them either: nobody could see a stray on the hidden desktop. A shared daemon (zenserver, UnrealTraceServer) may, and is logged.
# Tools/WindowWatch.ps1 hooks EVENT_OBJECT_SHOW for the whole step, so a flash is caught; its limits are in that file. A launch that failed would show nothing and
# prove nothing, so each must also read PASS. Two controls run with it, so the step cannot pass blind:
#   probe    a process started past the helper, on this desktop, shows a window: one pixel, alpha 0, off every screen, no focus, no taskbar button, 0.2 s. No one
#            can see it. The hook must catch it and name it in the tree, or the step fails.
#   bypass   a real game launch past the helper (a raw CreateProcess of UnrealEditor.exe -game -windowed, which shows UnrealWindow) must make this same watch read FAIL.
#            It would open a window on the owner's desktop if run there, so the script runs on a hidden desktop as its own stand-in and refuses -Bypass on the Default
#            desktop. The probe runs there too, so a stand-in that cannot see at all reads as a broken control. UnrealEditor.exe is a GUI binary on purpose: a console
#            binary given a new console is handed to the default terminal, which opens on the real desktop whatever desktop the launch was on (Docs/Validation/S1_no_windows.md).
#            It has a -userdir of its own, Saved\WindowCheck\bypass, emptied each step: it runs beside the bot run on one seed, and telemetry names only reach the second, so in
#            one Saved\Playtests the two could append to one file and fail the step. Its 320x180 window would also be saved to this checkout's GameUserSettings.ini.
# Prints "WINDOWS: PASS|FAIL (...)"; exit 0 / 1. Log: Saved\Logs\WindowCheck.log; bypass log: Saved\WindowCheck\bypass.txt.
param(
	[int]$Timeout = 300,
	[switch]$Bypass,
	[switch]$ProbeWindow
)

trap { Write-Host "WINDOWS: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ProbeTitle = "DTWindowProbe"

if ($ProbeWindow) {
	Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class DTProbeWindow {
	[DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr CreateWindowExW(uint exStyle, string type, string title, uint style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr instance, IntPtr param);
	[DllImport("user32.dll")] static extern bool SetLayeredWindowAttributes(IntPtr hWnd, uint key, byte alpha, uint flags);
	[DllImport("user32.dll")] static extern bool ShowWindow(IntPtr hWnd, int show);
	[DllImport("user32.dll")] static extern bool DestroyWindow(IntPtr hWnd);
	const uint Layered = 0x80000, Transparent = 0x20, ToolWindow = 0x80, NoActivate = 0x8000000, Popup = 0x80000000, AlphaKey = 2;
	public static bool Flash(string title) {
		IntPtr hWnd = CreateWindowExW(Layered | Transparent | ToolWindow | NoActivate, "STATIC", title, Popup, -20000, -20000, 1, 1, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
		if (hWnd == IntPtr.Zero) { return false; }
		SetLayeredWindowAttributes(hWnd, 0, 0, AlphaKey);
		ShowWindow(hWnd, 4); // SW_SHOWNOACTIVATE
		Thread.Sleep(200);
		DestroyWindow(hWnd);
		return true;
	}
}
"@
	exit $(if ([DTProbeWindow]::Flash($ProbeTitle)) { 0 } else { 1 })
}

$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
. (Join-Path $PSScriptRoot "WindowWatch.ps1")
$Editor = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$WorkDir = Join-Path $Root "Saved\WindowCheck"
$BypassReport = Join-Path $WorkDir "bypass.txt"
$BypassUser = Join-Path $WorkDir "bypass"
$Powershell = Join-Path $PSHOME "powershell.exe"
$Map = "/Game/DT/Maps/L_Arena_Test"
New-Item -ItemType Directory -Force $LogDir, $WorkDir | Out-Null

# --- the stand-in: this script again, on a hidden desktop, with one launch past the helper -----------------------------
if ($Bypass) {
	$Desktop = [DTWindowWatch]::DesktopName()
	if ($Desktop -ieq "Default") { Write-Host "WINDOWS: FAIL (-Bypass refused: this is the Default desktop, the window would reach it)"; exit 1 }
	$Log = Join-Path $BypassUser "bypass.log"
	New-Item -ItemType Directory -Force $BypassUser | Out-Null
	Start-DTWindowWatch
	& $Powershell -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -ProbeWindow
	# The one launch in Tools that skips the helper, on purpose. CreateProcess with no desktop named inherits this script's, the stand-in.
	$Info = New-Object Diagnostics.ProcessStartInfo $Editor, (@("`"$Project`"", $Map, "-game", "-windowed", "-ResX=320", "-ResY=180", "-nosplash", "-unattended", "-nopause", "-nosound", "-LiveCoding=false", "-userdir=`"$BypassUser`"",
		"-DTSeed=9107", "-DTBotFast=600", "-DTBotQuit", "-DTBot=None", "-DTBotMaxSeconds=20", "-DTBotOut=`"$(Join-Path $BypassUser 'bypass.csv')`"", "-ABSLOG=`"$Log`"") -join " ")
	$Info.UseShellExecute = $false
	$Proc = [Diagnostics.Process]::Start($Info)
	if (-not $Proc.WaitForExit($Timeout * 1000)) { & taskkill /PID $Proc.Id /T /F | Out-Null }
	$Seen = Stop-DTWindowWatch
	$Game = @($Seen.Windows | Where-Object { $_ -notlike "*'$ProbeTitle'" })
	$Lines = @("desktop $Desktop") + @($Seen.Windows)
	$Lines += if ($Game.Count) { "WINDOWS: FAIL ($($Game.Count) shown by the launch past the helper)" } else { "WINDOWS: PASS (nothing shown)" }
	[IO.File]::WriteAllLines($BypassReport, [string[]]$Lines)
	exit $(if ($Game.Count) { 1 } else { 0 })
}

# --- the step --------------------------------------------------------------------------------------------------------------
$Log = Join-Path $LogDir "WindowCheck.log"
$Watch = [Diagnostics.Stopwatch]::StartNew()
$script:Failures = @()
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; $script:Failures += $Text }
function Get-Verdict($Out, [string]$Word) { return (($Out | Select-String "^${Word}:" | Select-Object -Last 1).Line) }

$BotDir = Join-Path $WorkDir "bot"
foreach ($Old in $BotDir, $BypassReport, $BypassUser) { if (Test-Path $Old) { Remove-Item $Old -Recurse -Force } }

Start-DTWindowWatch
$Control = Start-DTHidden $Powershell @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`"", "-Bypass", "-Timeout", "$Timeout")

& $Powershell -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -ProbeWindow
if ($LASTEXITCODE -ne 0) { Note-Fail "the probe window could not be created" }

$Out = & $Powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "BotRuns.ps1") -Arms None -Seeds 1 -FirstSeed 9107 -MaxSeconds 20 -Timeout $Timeout -Out $BotDir 2>&1
$Bot = Get-Verdict $Out "BOTRUNS"
if ($LASTEXITCODE -ne 0) { Note-Fail "bot run: $Bot" }

$Out = & $Powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "Smoke.ps1") 2>&1
$Smoke = Get-Verdict $Out "SMOKE"
if ($LASTEXITCODE -ne 0) { Note-Fail "smoke run: $Smoke" }

$Out = & $Powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "RunTests.ps1") -Filter Dusktracer.Tec.RunSeed -Log (Join-Path $LogDir "WindowCheck_tests.log") -Timeout $Timeout 2>&1
$Tests = Get-Verdict $Out "TESTS"
if ($LASTEXITCODE -ne 0) { Note-Fail "test run: $Tests" }

if (-not $Control.WaitForExit($Timeout * 1000)) { Note-Fail "the bypass control did not end"; Stop-DTHidden $Control }
$Seen = Stop-DTWindowWatch
Close-DTHidden

$Probe = @($Seen.Windows | Where-Object { $_ -like "*'$ProbeTitle'" })
$Shown = @($Seen.Windows | Where-Object { $_ -notlike "*'$ProbeTitle'" })
if ($Probe.Count -eq 0) { Note-Fail "the hook missed the probe window, so a pass here would prove nothing" }
foreach ($Window in $Shown) { Note-Fail "shown on this desktop: $Window" }
foreach ($Stray in @($Seen.Left | Where-Object { $_ -match "^(UnrealEditor|Dusktracer)" })) { Note-Fail "still running after the step: $Stray" }

$BypassLines = if (Test-Path $BypassReport) { @(Get-Content $BypassReport -Encoding UTF8) } else { @() }
$BypassVerdict = ($BypassLines | Where-Object { $_ -like "WINDOWS:*" } | Select-Object -Last 1)
$BypassProbe = @($BypassLines | Where-Object { $_ -like "*'$ProbeTitle'" }).Count -gt 0
$BypassCaught = ($Control.ExitCode -eq 1) -and ($BypassVerdict -like "WINDOWS: FAIL*") -and @($BypassLines | Where-Object { $_ -like "UnrealEditor*" }).Count -gt 0
if (-not $BypassProbe) { Note-Fail "the stand-in desktop missed its probe, so the bypass control read nothing" }
if (-not $BypassCaught) { Note-Fail "the bypass control was not caught (exit $($Control.ExitCode)): $BypassVerdict" }

$Report = @("tree: $($Seen.Tree) processes; lost events: $($Seen.Lost); left running: $($Seen.Left -join ', ')", "bot: $Bot", "smoke: $Smoke", "tests: $Tests", "probe: $($Probe -join ' | ')", "bypass: $BypassVerdict")
$Report += @($BypassLines | ForEach-Object { "  $_" })
$Report += "windows of other processes on this desktop, not judged:"
$Report += @($Seen.Other | ForEach-Object { "  $_" })
[IO.File]::WriteAllLines($Log, [string[]]$Report)

$Seconds = [int]$Watch.Elapsed.TotalSeconds
if ($script:Failures.Count) { Write-Host "WINDOWS: FAIL ($($script:Failures.Count) failure(s), first: $($script:Failures[0]); $Seconds s)"; exit 1 }
Write-Host "WINDOWS: PASS (0 shown by bot, smoke and test runs across $($Seen.Tree) processes; probe caught; bypass caught; $($Seen.Lost) lost; $Seconds s)"
exit 0
