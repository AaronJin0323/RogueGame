# Smoke test: launches the game on a map, runs console commands, waits, then closes the
# game window itself and prints a verdict from the log. Exit code 0 = pass, 1 = fail.
#   powershell -ExecutionPolicy Bypass -File Tools\Smoke.ps1 [-Seconds 45] [-Map /Game/RH/Maps/L_Arena_Test] [-Cmds "RH.Spawn 300,RH.Bullets 3000"] [-Visible]
# Renders off-screen (no window) by default; -Visible opens a normal window.
param(
	[int]$Seconds = 45,
	[string]$Map = "/Game/RH/Maps/L_Arena_Test",
	[string]$Cmds = "RH.Spawn 300,RH.Bullets 3000",
	[int]$LoadTimeout = 600,  # first launch after render changes compiles shaders
	[switch]$Visible
)

$ErrorActionPreference = "Stop"
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = (Resolve-Path (Join-Path $PSScriptRoot "..\RogueHero.uproject")).Path
$LogDir = Join-Path $PSScriptRoot "..\Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Log = Join-Path (Resolve-Path $LogDir).Path ("Smoke_{0:yyyyMMdd_HHmmss}.log" -f (Get-Date))

$ArgList = @("`"$Project`"", $Map, "-game", "-windowed", "-ResX=1280", "-ResY=720", "-LiveCoding=false", "-culture=en",
	"-ABSLOG=`"$Log`"", "-ExecCmds=`"$Cmds`"")
if (-not $Visible) { $ArgList += "-RenderOffscreen" }
$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru

function Read-Log { if (Test-Path $Log) { Get-Content $Log -Raw -ErrorAction SilentlyContinue } else { "" } }

try {
	# Wait for the map to load (shader compiles can make this slow), then run for $Seconds.
	$Deadline = (Get-Date).AddSeconds($LoadTimeout)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline -and (Read-Log) -notmatch "LogRH: Run start") {
		Start-Sleep -Seconds 2
	}
	if (-not $Proc.HasExited -and (Read-Log) -match "LogRH: Run start") {
		$End = (Get-Date).AddSeconds($Seconds)
		while (-not $Proc.HasExited -and (Get-Date) -lt $End) { Start-Sleep -Seconds 1 }
	}
}
finally {
	# Always close the game window (only this process tree, never other editor instances).
	if (-not $Proc.HasExited) { & taskkill /PID $Proc.Id /T /F | Out-Null }
	Start-Sleep -Seconds 1
}

$Text = Read-Log
$Lines = $Text -split "`r?`n"
$Loaded   = $Text -match "LogRH: Run start"
$Crashed  = $Text -match "Fatal error|=== Critical error ===|Unhandled Exception"
$Errors   = $Lines | Where-Object { $_ -match "(Error:|Ensure condition failed)" -and $_ -notmatch "UnifiedErrorTest|LogTemp: (Error|FError)" }

Write-Host "---- Smoke test: $Map ($Seconds s) ----"
$Lines | Where-Object { $_ -match "LogLoad: LoadMap|LogRH" } | ForEach-Object { Write-Host $_ }
if ($Errors) { Write-Host "---- Errors ----"; $Errors | Select-Object -First 30 | ForEach-Object { Write-Host $_ } }
Write-Host "Log: $Log"

if ($Loaded -and -not $Crashed -and -not $Errors) { Write-Host "SMOKE: PASS"; exit 0 }
Write-Host ("SMOKE: FAIL (loaded={0}, crashed={1}, errors={2})" -f $Loaded, $Crashed, @($Errors).Count)
exit 1
