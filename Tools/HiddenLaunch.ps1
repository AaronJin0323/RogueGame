# Hidden launch, the no-window rule (AGENTS.md): every start of UnrealEditor.exe, UnrealEditor-Cmd.exe or Dusktracer.exe by a tool goes through here.
#   . (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
#   $Proc = Start-DTHidden $Exe $ArgList   a [Diagnostics.Process], as Start-Process -PassThru gives, with its exit code held from the start
#   Stop-DTHidden $Proc                    kills its tree by PID and waits; nothing if it has ended
#   Get-DTHiddenWindows                    what is visible on the hidden desktop now: "<pid> <class> '<title>'"
#   Hold-DTProcess                         this script and what it starts from now on, in a job that carries the mask's affinity limit
#   Close-DTHidden                         ends whatever is left and closes the desktop. A script may leave this to its own exit: the OS does the same
# $ArgList is joined with spaces, as Start-Process does, so a path with a space carries its own quotes. The editor needs the project path first.
# Why a desktop: no launch flag is the rule. A launch that renders shows UnrealWindow, a flag can be forgotten, and -WindowStyle Hidden does not stop Unreal's own
# ShowWindow. A window on a desktop nobody switches to cannot reach the desktop anyone is looking at, whatever the engine does. A child inherits the desktop,
# so workers and a packaged stub's game land there too.
# Why CREATE_NO_WINDOW as well: a console binary given a new console is handed to the default terminal, and Windows Terminal opens on the real desktop whatever
# desktop the launch was on (Docs/Validation/S1_no_windows.md). The flag gives it a console with no window; a GUI binary ignores it.
# A desktop lives until its last user has gone: the handle here closes with the script, and a shared daemon a launch started (zenserver, UnrealTraceServer)
# keeps the empty desktop alive until it ends, no longer.
# One desktop per script process, opened at the first launch, with two jobs that kill what they hold if the script dies, because nobody can see a stray on this desktop.
# The editor's job holds the launched process only. Its children leave silently: a daemon other worktrees are using must outlive this script.
# The packaged game's job (Dusktracer*.exe) holds the whole tree: the stub's real game is its child, and a package ships no shared daemon.
# tools_test.py fails on a Start-Process or a direct call of an engine binary anywhere else in Tools.

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

if (-not ("DTHiddenDesktop" -as [type])) {
	Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
public static class DTHiddenDesktop {
	delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
	[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
	struct StartupInfo { public int cb; public string reserved; public string desktop; public string title; public int x, y, w, h, cx, cy, fill, flags; public short show, reserved2; public IntPtr reserved3, stdIn, stdOut, stdErr; }
	[StructLayout(LayoutKind.Sequential)]
	struct ProcessInformation { public IntPtr process, thread; public int pid, tid; }
	[StructLayout(LayoutKind.Sequential)]
	struct JobLimits { public long perProcess, perJob; public uint flags; public UIntPtr minSet, maxSet; public uint active; public UIntPtr affinity; public uint priority, scheduling; public ulong io1, io2, io3, io4, io5, io6; public UIntPtr processMemory, jobMemory, peakProcess, peakJob; }
	[DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr CreateDesktopW(string name, IntPtr device, IntPtr mode, uint flags, uint access, IntPtr security);
	[DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
	[DllImport("user32.dll")] static extern bool EnumDesktopWindows(IntPtr desktop, EnumProc proc, IntPtr lParam);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hWnd, StringBuilder text, int max);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr hWnd, StringBuilder text, int max);
	[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CreateProcessW(string app, StringBuilder command, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string cwd, ref StartupInfo si, out ProcessInformation pi);
	[DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr CreateJobObjectW(IntPtr security, IntPtr name);
	[DllImport("kernel32.dll", SetLastError = true)] static extern bool SetInformationJobObject(IntPtr job, int kind, ref JobLimits limits, int size);
	[DllImport("kernel32.dll", SetLastError = true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
	[DllImport("kernel32.dll", SetLastError = true)] static extern uint ResumeThread(IntPtr thread);
	[DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process, uint code);
	[DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
	const uint GenericAll = 0x10000000;
	const uint CreateSuspended = 0x4;
	const uint CreateNoWindow = 0x08000000;
	const uint KillOnJobClose = 0x2000;
	const uint SilentBreakawayOk = 0x1000;
	const uint LimitAffinity = 0x10;
	const int ExtendedLimitInformation = 9;
	static IntPtr Desktop;
	static IntPtr EditorJob;
	static IntPtr GameJob;
	static string Name;
	// D-052: the machine's processor mask as the jobs' affinity limit (0: no limit). A process mask alone does not hold the engine: its threads ask for
	// every processor (FRunnableThreadWin::SetThreadAffinity) and Windows 11 widens the process to match. A job's limit refuses that request.
	public static ulong Affinity;
	public static uint JobFlags(uint flags, ulong affinity) { return affinity != 0 ? flags | LimitAffinity : flags; }
	static IntPtr NewJob(uint flags) {
		IntPtr job = CreateJobObjectW(IntPtr.Zero, IntPtr.Zero);
		if (job == IntPtr.Zero) { throw new Win32Exception(); }
		JobLimits limits = new JobLimits();
		limits.flags = JobFlags(flags, Affinity);
		limits.affinity = new UIntPtr(Affinity);
		if (!SetInformationJobObject(job, ExtendedLimitInformation, ref limits, Marshal.SizeOf(typeof(JobLimits)))) { CloseHandle(job); throw new Win32Exception(); }
		return job;
	}
	static void Open() {
		if (Desktop != IntPtr.Zero) { return; }
		string name = "DTHidden" + Process.GetCurrentProcess().Id;
		IntPtr desktop = CreateDesktopW(name, IntPtr.Zero, IntPtr.Zero, 0, GenericAll, IntPtr.Zero);
		if (desktop == IntPtr.Zero) { throw new Win32Exception(); }
		EditorJob = NewJob(KillOnJobClose | SilentBreakawayOk);
		GameJob = NewJob(KillOnJobClose);
		Desktop = desktop;
		Name = name;
	}
	public static Process Start(string exe, string arguments) {
		Open();
		bool packaged = System.IO.Path.GetFileName(exe).StartsWith("Dusktracer", StringComparison.OrdinalIgnoreCase);
		StartupInfo si = new StartupInfo();
		si.cb = Marshal.SizeOf(typeof(StartupInfo));
		si.desktop = "WinSta0\\" + Name;
		ProcessInformation pi;
		StringBuilder line = new StringBuilder("\"" + exe + "\" " + arguments);
		if (!CreateProcessW(exe, line, IntPtr.Zero, IntPtr.Zero, false, CreateSuspended | CreateNoWindow, IntPtr.Zero, null, ref si, out pi)) { throw new Win32Exception(); }
		// Suspended until the handle is held, so even a launch that dies at once keeps its exit code.
		try {
			if (!AssignProcessToJobObject(packaged ? GameJob : EditorJob, pi.process)) { throw new Win32Exception(); }
			Process proc = Process.GetProcessById(pi.pid);
			IntPtr held = proc.Handle;
			if (held == IntPtr.Zero || ResumeThread(pi.thread) == 0xFFFFFFFF) { throw new Win32Exception(); }
			return proc;
		}
		catch {
			TerminateProcess(pi.process, 1);
			throw;
		}
		finally {
			CloseHandle(pi.thread);
			CloseHandle(pi.process);
		}
	}
	// The calling process and every process it starts from now on, in a job of their own that carries the affinity limit: the form for work that is
	// not one launch (a batch file's commandlet, the cook RunUAT starts). No kill on close and no breakaway: a daemon the work starts stays held and lives on.
	static IntPtr HeldJob;
	public static bool HoldSelf() {
		if (Affinity == 0) { return false; }
		if (HeldJob != IntPtr.Zero) { return true; }
		IntPtr job = NewJob(0);
		if (!AssignProcessToJobObject(job, Process.GetCurrentProcess().Handle)) { CloseHandle(job); throw new Win32Exception(); }
		HeldJob = job;
		return true;
	}
	public static string[] Visible() {
		List<string> found = new List<string>();
		if (Desktop == IntPtr.Zero) { return found.ToArray(); }
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
	public static void Close() {
		if (Desktop == IntPtr.Zero) { return; }
		CloseHandle(EditorJob); // kills what is still in the job
		CloseHandle(GameJob);
		CloseDesktop(Desktop);
		EditorJob = IntPtr.Zero;
		GameJob = IntPtr.Zero;
		Desktop = IntPtr.Zero;
	}
}
"@
}

# The jobs take the helper's mask as their affinity limit, so the engine and everything in its job stay inside it; none when the helper returns none.
[DTHiddenDesktop]::Affinity = if ($DTMask -and $DTMask -ne "none") { [Convert]::ToUInt64($DTMask, 16) } else { [UInt64]0 }

function Start-DTHidden([string]$Exe, [string[]]$ArgList) { return [DTHiddenDesktop]::Start($Exe, ($ArgList -join " ")) }

function Stop-DTHidden($Proc) {
	if ($Proc -and -not $Proc.HasExited) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		$Proc.WaitForExit(30000) | Out-Null
	}
}

# Holds this script and all it starts from now on inside the machine's mask, by the same job limit as a launch (no limit without a mask).
function Hold-DTProcess { return [DTHiddenDesktop]::HoldSelf() }

# The one log line the launch job's affinity limit makes the engine write (D-052): a worker thread asks for a processor the limit leaves out
# and Windows refuses it with ERROR_GEN_FAILURE (0x1f). Shown with and without the limit on one package (Validation/S2_package.md). A check that fails on
# any log warning sets aside exactly these lines, and only for a launch the limit held; it counts them and says so. Every other warning still counts.
$DTLimitWarning = '^(\[[^\]]*\]\[[^\]]*\])?LogThreadingWindows: Warning: Runnable thread .+ call to SetThreadAffinity failed with: 0x1f$'
function Split-DTLimitWarnings([string[]]$Lines) {
	$Held = [DTHiddenDesktop]::Affinity -ne 0
	$Kept = @($Lines | Where-Object { -not ($Held -and $_ -cmatch $DTLimitWarning) })
	return @{ Kept = $Kept; SetAside = @($Lines).Count - $Kept.Count; Held = $Held }
}

function Get-DTHiddenWindows { return [DTHiddenDesktop]::Visible() }

function Close-DTHidden { [DTHiddenDesktop]::Close() }
