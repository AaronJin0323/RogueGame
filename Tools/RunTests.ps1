# Automation tests, headless and on the hidden desktop: the launch behind gate step `tests`, and the way to run a filter by hand.
#   & Tools\RunTests.ps1 [-Filter Dusktracer.Keepers] [-Log <path>] [-Floor 0] [-Timeout 3600] [-ReadLog <log>]
# -Floor (the gate passes C.GATE.TEST_FLOOR) holds the count both ways: fewer tests found fails, and so do more, with a line naming the number to set,
# because the floor rises in the commit that adds tests (11_Quality s1). -ReadLog judges a log already written and launches nothing (Tools/tools_test.py).
# Every test found must complete: a crash can stop the run early. A run still alive after -Timeout seconds is killed by its own PID and fails on that count.
# Prints "TESTS: PASS|FAIL (<n> run of <m> found, <f> failed, floor <n>, <c> crash marker(s)[, killed after <s> s])", with "  ! " lines for what failed; exit 0 / 1. Default log: Saved\Logs\RunTests.log.
param(
	[string]$Filter = "Dusktracer",
	[string]$Log = "",
	[int]$Floor = 0,
	[int]$Timeout = 3600,
	[string]$ReadLog = ""
)

trap { Write-Host "TESTS: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
if (-not $ReadLog) { $DTHeavy = Enter-DTHeavy -What "RunTests" }  # D-055 item 7: one heavy-job slot; -ReadLog launches nothing and takes none

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$Killed = $false
if ($ReadLog) { $Log = $ReadLog }
else {
	if (-not $Log) { $Log = Join-Path $Root "Saved\Logs\RunTests.log" }
	New-Item -ItemType Directory -Force (Split-Path $Log -Parent) | Out-Null
	if (Test-Path $Log) { Remove-Item $Log }

	$Proc = Start-DTHidden $Exe @("`"$Project`"", "-ExecCmds=`"Automation RunTests $Filter;Quit`"", "-unattended", "-nopause", "-nullrhi", "-nosplash", "-LiveCoding=false", "-ABSLOG=`"$Log`"")
	$Killed = -not $Proc.WaitForExit($Timeout * 1000)
	if ($Killed) {
		Write-Host "  ! killed after $Timeout s (PID $($Proc.Id))"
		Stop-DTHidden $Proc
	}
}

$All = @(Select-String -Path $Log -Pattern "Test Completed\. Result=\{")
$Failed = @($All | Where-Object { $_.Line -notmatch "Result=\{Success\}" })
$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===")
$FoundLine = Select-String -Path $Log -Pattern "Found (\d+) automation tests" | Select-Object -First 1
$Found = if ($FoundLine) { [int]$FoundLine.Matches[0].Groups[1].Value } else { 0 }
$Summary = "{0} run of {1} found, {2} failed, floor {3}, {4} crash marker(s)" -f $All.Count, $Found, $Failed.Count, $Floor, $Crash.Count
if ($Killed) { $Summary += ", killed after $Timeout s" }
$Above = $Floor -gt 0 -and $Found -gt $Floor
if ($Above) { Write-Host ("  ! {0} tests found, above the floor {1}: set C.GATE.TEST_FLOOR to {0} in Docs/constants.yaml (11_Quality s1)" -f $Found, $Floor) }
if ($Killed -or $All.Count -eq 0 -or $All.Count -ne $Found -or $All.Count -lt $Floor -or $Above -or $Failed.Count -gt 0 -or $Crash.Count -gt 0) {
	$Failed + $Crash | Select-Object -First 12 | ForEach-Object { "  ! " + $_.Line }
	Write-Host "TESTS: FAIL ($Summary)"
	exit 1
}
Write-Host "TESTS: PASS ($Summary)"
exit 0
