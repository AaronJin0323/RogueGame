# Does the packaged game end after a crash? (Docs/Validation/S1_crash_exit.md). Crashes it -Runs times on the hidden desktop and counts launches that hang.
#   & Tools\CrashExitCheck.ps1 [-Config Development|Shipping] [-Runs 10] [-TimeoutS 60] [-Package <root>] [-Crash <launch args>] [-Extra <launch args>]
#                              [-NoUnattended] [-WithSound]
# -Package is a package root as Tools\Package.ps1 writes it (default: this worktree's Saved\Package). The session A build in the main checkout's
# Saved\Package(s) is refused: it is never launched by a tool.
# -Crash is what makes the build crash (default -ExecCmds="debug crash", which a Shipping build has no console for); -Extra is added to every launch.
# Each launch: a fresh -userdir under Saved\CrashExit\<stamp>\<n>, off-screen as Tools\PadCheck.ps1 launches the package, -unattended -nosound
# -culture=en -DTSeed=770099, and its log there (-ABSLOG; a Shipping build writes none). -NoUnattended and -WithSound drop one of the two, one at a time,
# for the variations of the note; a launch without -unattended may show a dialog on the hidden desktop, and the hung line names it.
# The script waits up to -TimeoutS for the process to end. A launch still running then is recorded (threads and their states as Windows reports them,
# the dump's size, visible windows, the log's last line) and ended by the script itself, its own tree by PID (Stop-DTHidden).
# The lines come from Tools\crash_exit.py, one per launch, then: CRASHEXIT: PASS (<n> of <n> ended, slowest <s> s; <config>) | FAIL (<k> of <n> hung; <config>).
# Exit 0 on PASS, 1 on FAIL or a script error.
param(
	[ValidateSet("Development", "Shipping")]
	[string]$Config = "Development",
	[int]$Runs = 10,
	[int]$TimeoutS = 60,
	[string]$Package = "",
	[string]$Crash = '-ExecCmds="debug crash"',
	[string]$Extra = "",
	[switch]$NoUnattended,
	[switch]$WithSound
)

trap { Write-Host "CRASHEXIT: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "CrashExitCheck"  # R-TEC-18: one heavy-job slot for the whole series of launches

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")

if (-not $Package) { $Package = Join-Path $Root "Saved\Package" }
$Package = (Resolve-Path $Package).Path
$Common = (& git -C $Root rev-parse --path-format=absolute --git-common-dir 2>$null)
if ($Common) {
	$MainSaved = Join-Path (Split-Path $Common -Parent) "Saved"
	foreach ($Held in "Package", "Packages") {
		$Path = (Join-Path $MainSaved $Held).TrimEnd("\") + "\"
		if (($Package.TrimEnd("\") + "\").StartsWith($Path, [StringComparison]::OrdinalIgnoreCase)) {
			Write-Host "CRASHEXIT: FAIL (refused: $Package is the session build of the main checkout)"; exit 1
		}
	}
}
$Exe = if ($Config -eq "Shipping") { Join-Path $Package "Shipping\Windows\Dusktracer\Binaries\Win64\Dusktracer-Win64-Shipping.exe" } `
	else { Join-Path $Package "Development\Windows\Dusktracer\Binaries\Win64\Dusktracer.exe" }
if (-not (Test-Path $Exe)) { Write-Host "CRASHEXIT: FAIL (no $Exe; Tools\Package.ps1 first)"; exit 1 }

$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$Work = Join-Path $Root "Saved\CrashExit\$Stamp"
New-Item -ItemType Directory -Force $Work | Out-Null
$Base = @("-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-culture=en", "-DTSeed=770099")
if (-not $NoUnattended) { $Base += "-unattended" }
if (-not $WithSound) { $Base += "-nosound" }
$Label = $Config + $(if ($NoUnattended) { ", no -unattended" } else { "" }) + $(if ($WithSound) { ", with sound" } else { "" }) + $(if ($Extra) { ", $Extra" } else { "" })
Write-Host ("crashexit: {0} x{1}, timeout {2} s, {3}; crash {4}; work {5}" -f $Config, $Runs, $TimeoutS, $Exe, $Crash, $Work)

function Get-DumpBytes([string]$UserDir) {
	$Dumps = @(Get-ChildItem (Join-Path $UserDir "Saved\Crashes") -Recurse -Filter "UEMinidump.dmp" -ErrorAction SilentlyContinue)
	if (-not $Dumps.Count) { return $null }
	return [long](($Dumps | Measure-Object Length -Sum).Sum)
}

for ($N = 1; $N -le $Runs; $N++) {
	$Dir = Join-Path $Work $N
	$UserDir = Join-Path $Dir "user"
	$Log = Join-Path $Dir "game.log"
	New-Item -ItemType Directory -Force $UserDir | Out-Null
	$ArgList = $Base + @("-userdir=`"$UserDir`"", "-ABSLOG=`"$Log`"", $Crash)
	if ($Extra) { $ArgList += $Extra }
	$Start = Get-Date
	$Proc = Start-DTHidden $Exe $ArgList
	$Record = [ordered]@{ n = $N; pid = $Proc.Id }
	try {
		$Deadline = $Start.AddSeconds($TimeoutS)
		while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Milliseconds 250 }
		$Record.seconds = [math]::Round(((Get-Date) - $Start).TotalSeconds, 1)
		if ($Proc.HasExited) {
			$Record.ended = $true
			$Record.exit_code = $Proc.ExitCode
		}
		else {
			$Record.ended = $false
			$Proc.Refresh()
			$Threads = @($Proc.Threads)
			$Record.threads = $Threads.Count
			try {
				$Record.suspended = @($Threads | Where-Object { $_.ThreadState -eq "Wait" -and "$($_.WaitReason)" -eq "Suspended" }).Count
				$Record.running = @($Threads | Where-Object { -not ($_.ThreadState -eq "Wait" -and "$($_.WaitReason)" -eq "Suspended") } |
					ForEach-Object { "{0} {1}/{2}" -f $_.Id, $_.ThreadState, $(if ($_.ThreadState -eq "Wait") { $_.WaitReason } else { "-" }) })
			}
			catch { $Record.suspended = $null; $Record.running = @() }
			$Record.windows = @(Get-DTHiddenWindows | Where-Object { $_ -match "^$($Proc.Id) " })
		}
	}
	finally {
		Stop-DTHidden $Proc
	}
	$Record.dump_bytes = Get-DumpBytes $UserDir
	$Record.log = Test-Path $Log
	if ($Record.log) {
		$Text = @(Get-Content $Log -Encoding UTF8 -ErrorAction SilentlyContinue)
		$Record.shutdown = [bool]($Text | Select-String -SimpleMatch "StaticShutdownAfterError" -Quiet)
		$Record.last_line = (@($Text | Where-Object { $_.Trim() }) | Select-Object -Last 1)
	}
	$RecordFile = Join-Path $Dir "record.json"
	[IO.File]::WriteAllText($RecordFile, ($Record | ConvertTo-Json -Depth 4), (New-Object System.Text.UTF8Encoding($false)))
	& python -I (Join-Path $PSScriptRoot "crash_exit.py") line $RecordFile
}

& python -I (Join-Path $PSScriptRoot "crash_exit.py") result $Work $Label
exit $(if ($LASTEXITCODE -eq 0) { 0 } else { 1 })
