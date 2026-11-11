# Window watch: every window a script's process tree shows on the desktop this script runs on (gate step `windows`, T-PKG-07).
#   . (Join-Path $PSScriptRoot "WindowWatch.ps1")
#   Start-DTWindowWatch                    from here this script and all it starts are the tree
#   $Seen = Stop-DTWindowWatch             .Windows ("<image> pid <pid> class <class> '<title>'"), .Tree (processes), .Other (windows of anything else), .Left (tree still alive)
# SetWinEventHook EVENT_OBJECT_SHOW, out of context, so a flash too short for a sampled loop still queues an event; the hook sees one desktop, this script's.
# The tree is a job holding this script: every descendant joins it at birth, and a job message names it even after it has exited. Children the hidden launch
# helper puts in its own job nest inside this one, so a daemon that leaves that job stays in this one.
# Limits. It sees a window that shows through ShowWindow, SetWindowPos or WS_VISIBLE at creation; not one given WS_VISIBLE later by SetWindowLong, which fires
# no event. It reads one desktop, so a window the OS shows for a process outside the tree (WerFault, a UAC prompt) is counted in .Other and not judged:
# .Other keeps image and class only, never a title, since those windows are the owner's. A window gone before the event is read still counts when its
# thread can be named; if that thread has gone too, or is closed to this process, the event counts in .Lost, not in .Windows. csrss.exe's are: it flashes one
# window as each child PowerShell starts, so a step that starts four reads 4 lost (Docs/Validation/S1_no_windows.md).

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

if (-not ("DTWindowWatch" -as [type])) {
	Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
public static class DTWindowWatch {
	delegate void WinEventProc(IntPtr hook, uint kind, IntPtr hWnd, int obj, int child, uint thread, uint time);
	[StructLayout(LayoutKind.Sequential)] struct PortLink { public IntPtr key, port; }
	[StructLayout(LayoutKind.Sequential)] struct Msg { public IntPtr hWnd; public uint message; public IntPtr wParam, lParam; public uint time; public int x, y; }
	[DllImport("user32.dll")] static extern IntPtr SetWinEventHook(uint min, uint max, IntPtr module, WinEventProc proc, uint pid, uint thread, uint flags);
	[DllImport("user32.dll")] static extern bool UnhookWinEvent(IntPtr hook);
	[DllImport("user32.dll")] static extern int GetMessageW(out Msg msg, IntPtr hWnd, uint min, uint max);
	[DllImport("user32.dll")] static extern bool PostThreadMessageW(uint thread, uint message, IntPtr wParam, IntPtr lParam);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
	[DllImport("user32.dll")] static extern bool IsWindow(IntPtr hWnd);
	[DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr hWnd, uint flags);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hWnd, StringBuilder text, int max);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr hWnd, StringBuilder text, int max);
	[DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool GetUserObjectInformationW(IntPtr handle, int index, StringBuilder info, int length, out int needed);
	[DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
	[DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
	[DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr CreateJobObjectW(IntPtr security, IntPtr name);
	[DllImport("kernel32.dll", SetLastError = true)] static extern bool SetInformationJobObject(IntPtr job, int kind, ref PortLink link, int size);
	[DllImport("kernel32.dll", SetLastError = true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
	[DllImport("kernel32.dll")] static extern bool IsProcessInJob(IntPtr process, IntPtr job, out bool inside);
	[DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr CreateIoCompletionPort(IntPtr file, IntPtr port, UIntPtr key, uint threads);
	[DllImport("kernel32.dll")] static extern bool GetQueuedCompletionStatus(IntPtr port, out uint message, out UIntPtr key, out IntPtr detail, uint ms);
	[DllImport("kernel32.dll")] static extern bool PostQueuedCompletionStatus(IntPtr port, uint message, UIntPtr key, IntPtr detail);
	[DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
	[DllImport("kernel32.dll")] static extern IntPtr OpenThread(uint access, bool inherit, uint tid);
	[DllImport("kernel32.dll")] static extern uint GetProcessIdOfThread(IntPtr thread);
	[DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern bool QueryFullProcessImageNameW(IntPtr process, uint flags, StringBuilder path, ref int size);
	[DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process, out uint code);
	[DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
	const uint StillActive = 259;
	const uint EventObjectShow = 0x8002;
	const uint WmQuit = 0x12;
	const uint RootAncestor = 2;
	const int NameInformation = 2;
	const int PortInformation = 7;
	const uint JobNewProcess = 6;
	const uint ProcessQueryLimited = 0x1000;
	const uint ThreadQueryLimited = 0x0800;
	const uint Finished = 0xDEAD;
	class Shown { public uint Pid; public bool InJob; public string Image, Type, Title; }
	static readonly WinEventProc Callback = OnShow; // held here so the collector never frees what the hook calls
	static readonly List<Shown> Events = new List<Shown>();
	static readonly HashSet<uint> Tree = new HashSet<uint>();
	static IntPtr Job;
	static IntPtr Port;
	static Thread Pump;
	static Thread Births;
	static uint PumpId;
	static int Lost;
	public static string[] Windows = new string[0];
	public static string[] Other = new string[0];
	public static string[] Left = new string[0];
	public static int TreeCount;
	public static int LostCount;

	public static string DesktopName() {
		StringBuilder name = new StringBuilder(256);
		int needed;
		GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), NameInformation, name, name.Capacity * 2, out needed);
		return name.ToString();
	}
	static string ImageOf(IntPtr process) {
		StringBuilder path = new StringBuilder(1024);
		int size = path.Capacity;
		if (!QueryFullProcessImageNameW(process, 0, path, ref size)) { return "?"; }
		string text = path.ToString();
		return text.Substring(text.LastIndexOf('\\') + 1);
	}
	static void OnShow(IntPtr hook, uint kind, IntPtr hWnd, int obj, int child, uint thread, uint time) {
		if (obj != 0 || child != 0 || hWnd == IntPtr.Zero) { return; }
		bool alive = IsWindow(hWnd);
		if (alive && GetAncestor(hWnd, RootAncestor) != hWnd) { return; } // a control: its top level window has its own event
		uint pid = 0;
		if (alive) { GetWindowThreadProcessId(hWnd, out pid); }
		if (pid == 0) {
			IntPtr ownerThread = OpenThread(ThreadQueryLimited, false, thread);
			if (ownerThread != IntPtr.Zero) { pid = GetProcessIdOfThread(ownerThread); CloseHandle(ownerThread); }
		}
		if (pid == 0) { Interlocked.Increment(ref Lost); return; }
		Shown shown = new Shown();
		shown.Pid = pid;
		shown.Image = "?";
		IntPtr process = OpenProcess(ProcessQueryLimited, false, pid);
		if (process != IntPtr.Zero) {
			IsProcessInJob(process, Job, out shown.InJob);
			shown.Image = ImageOf(process);
			CloseHandle(process);
		}
		StringBuilder title = new StringBuilder(256);
		StringBuilder type = new StringBuilder(256);
		GetWindowTextW(hWnd, title, 256);
		GetClassNameW(hWnd, type, 256);
		shown.Type = type.ToString();
		shown.Title = title.ToString();
		lock (Events) { Events.Add(shown); }
	}
	static void RunPump(object ready) {
		PumpId = GetCurrentThreadId();
		IntPtr hook = SetWinEventHook(EventObjectShow, EventObjectShow, IntPtr.Zero, Callback, 0, 0, 0);
		((ManualResetEvent)ready).Set();
		Msg msg;
		while (hook != IntPtr.Zero && GetMessageW(out msg, IntPtr.Zero, 0, 0) > 0) { }
		if (hook != IntPtr.Zero) { UnhookWinEvent(hook); }
	}
	static void RunBirths() {
		while (true) {
			uint message;
			UIntPtr key;
			IntPtr detail;
			if (!GetQueuedCompletionStatus(Port, out message, out key, out detail, 0xFFFFFFFF) || message == Finished) { return; }
			if (message == JobNewProcess) { lock (Tree) { Tree.Add((uint)detail.ToInt64()); } }
		}
	}
	public static void Start() {
		Events.Clear();
		Tree.Clear();
		Lost = 0;
		if (Job == IntPtr.Zero) {
			Job = CreateJobObjectW(IntPtr.Zero, IntPtr.Zero);
			Port = CreateIoCompletionPort(new IntPtr(-1), IntPtr.Zero, UIntPtr.Zero, 1);
			if (Job == IntPtr.Zero || Port == IntPtr.Zero) { throw new Win32Exception(); }
			PortLink link = new PortLink();
			link.key = Job;
			link.port = Port;
			if (!SetInformationJobObject(Job, PortInformation, ref link, Marshal.SizeOf(typeof(PortLink)))) { throw new Win32Exception(); }
			if (!AssignProcessToJobObject(Job, GetCurrentProcess())) { throw new Win32Exception(); }
		}
		Births = new Thread(RunBirths);
		Births.IsBackground = true;
		Births.Start();
		ManualResetEvent ready = new ManualResetEvent(false);
		Pump = new Thread(RunPump);
		Pump.IsBackground = true;
		Pump.Start(ready);
		ready.WaitOne();
	}
	public static void Stop() {
		Thread.Sleep(500); // what was queued when the last child ended
		PostThreadMessageW(PumpId, WmQuit, IntPtr.Zero, IntPtr.Zero);
		Pump.Join(5000);
		PostQueuedCompletionStatus(Port, Finished, UIntPtr.Zero, IntPtr.Zero);
		Births.Join(5000);
		List<string> windows = new List<string>();
		Dictionary<string, int> other = new Dictionary<string, int>();
		lock (Events) { lock (Tree) {
			foreach (Shown s in Events) {
				if (s.InJob || Tree.Contains(s.Pid)) { windows.Add(s.Image + " pid " + s.Pid + " class " + s.Type + " '" + s.Title + "'"); }
				else {
					string key = s.Image + " class " + s.Type;
					int count;
					other.TryGetValue(key, out count);
					other[key] = count + 1;
				}
			}
			TreeCount = Tree.Count;
		} }
		List<string> rows = new List<string>();
		foreach (KeyValuePair<string, int> row in other) { rows.Add(row.Value + " x " + row.Key); }
		List<string> left = new List<string>();
		uint self = (uint)System.Diagnostics.Process.GetCurrentProcess().Id;
		foreach (uint pid in Tree) {
			IntPtr process = OpenProcess(ProcessQueryLimited, false, pid);
			if (process == IntPtr.Zero) { continue; }
			bool inside;
			uint exit;
			if (pid != self && IsProcessInJob(process, Job, out inside) && inside && GetExitCodeProcess(process, out exit) && exit == StillActive) { left.Add(ImageOf(process) + " pid " + pid); }
			CloseHandle(process);
		}
		Windows = windows.ToArray();
		Other = rows.ToArray();
		Left = left.ToArray();
		LostCount = Lost;
	}
}
"@
}

# The watch puts this process in a job (the tree). Launch jobs that Tools/HiddenLaunch.ps1 made earlier in the same process, for an earlier script run
# in the same shell, lie outside that job, and Windows refuses to put a new launch into them: "Access is denied" at the next launch
# (Validation/S2_package.md). So the helper's jobs and desktop are closed first, and its next launch makes new ones inside the tree. Nothing of an
# earlier script is still running at this point; anything that were would be ended by the close, as at that script's own exit.
function Start-DTWindowWatch {
	if ("DTHiddenDesktop" -as [type]) { [DTHiddenDesktop]::Close() }
	[DTWindowWatch]::Start()
}

function Stop-DTWindowWatch {
	[DTWindowWatch]::Stop()
	return [pscustomobject]@{ Windows = @([DTWindowWatch]::Windows); Other = @([DTWindowWatch]::Other); Left = @([DTWindowWatch]::Left); Tree = [DTWindowWatch]::TreeCount; Lost = [DTWindowWatch]::LostCount }
}
