# A win on a real gated save, on one launch (gate step `win`, meta.md T-MET-29), off-screen, no window.
#   & Tools\WinCheck.ps1 [-Cultures en,zh-Hans] [-Seeds 9100,9101,9102] [-Timeout 600]
# One launch per culture on an empty isolated save: a bot wins a run on the first-run pool (-DTPoolGate), then Debug/DTDebugWinCheck.cpp reads
# the toast the run-over screen draws, the keys in the file on disk, goes back, and reads the Shrine, the bestiary and the Recollections in the hub.
# A bot can lose a seed, which proves nothing: a seed that dies goes to the next one, and none of them winning is a FAIL.
# -DTHub is what lets this -unattended launch see the hub, as in Tools/VigilCheck.ps1.
# Shots: Saved\WinCheck\<culture>\runover.png and codex.png. Prints "WIN: PASS (...)" or "WIN: FAIL (...)"; exit 0 / 1.
# Logs: Saved\Logs\WinCheck_<culture>.log.
param(
	[string[]]$Cultures = @("en", "zh-Hans"),
	[int[]]$Seeds = @(9100, 9101, 9102),
	[int]$Timeout = 600
)

trap { Write-Host "WIN: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\WinCheck"
$Cultures = @($Cultures | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
New-Item -ItemType Directory -Force $LogDir, $OutDir | Out-Null

function Invoke-Launch([string]$Culture, [int]$Seed, [string]$Save, [string]$Shots) {
	$Log = Join-Path $LogDir "WinCheck_$Culture.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test?DTSeed=${Seed}?DTPatron=Storm", "-game", "-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-LiveCoding=false",
		"-unattended", "-nopause", "-nosound", "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTHub", "-DTPoolGate", "-DTMetaFile=`"$Save`"", "-DTWinCheck=`"$Shots`"",
		"-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=1.0", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotFast=120")
	$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -WindowStyle Hidden
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		Write-Host "WIN: FAIL ($Culture seed $Seed timed out)"
		exit 1
	}
	return $Log
}

$Passed = 0
$Won = @()
foreach ($Culture in $Cultures) {
	$Shots = Join-Path $OutDir $Culture
	$Verdict = $null
	foreach ($Seed in $Seeds) {
		if (Test-Path $Shots) { Remove-Item $Shots -Recurse -Force }
		New-Item -ItemType Directory -Force $Shots | Out-Null
		$Log = Invoke-Launch $Culture $Seed (Join-Path $Shots "DTMeta.sav") $Shots
		$Lines = @(Select-String -Path $Log -Pattern "WINCHECK" | ForEach-Object { $_.Line -replace "^.*LogDT: ", "" })
		if (@($Lines | Where-Object { $_ -match "^WINCHECK the run is a win: WRONG" }).Count -gt 0) {
			Write-Host "    $Culture seed $Seed lost; next seed"
			continue
		}
		$Lines | Where-Object { $_ -match "WRONG|^WINCHECK(:| toast| unlocked)" } | ForEach-Object { "    $Culture seed $Seed $_" }
		$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed")
		$Verdict = [bool]($Lines | Where-Object { $_ -match "^WINCHECK: PASS" }) -and $Crash.Count -eq 0
		$Missing = @(@("runover", "codex") | Where-Object { -not (Test-Path (Join-Path $Shots "$_.png")) })
		if (-not $Verdict -or $Missing.Count -gt 0) {
			Write-Host "WIN: FAIL ($Culture seed $Seed, no shot of: $($Missing -join ' '))"
			exit 1
		}
		$Won += "$Culture seed $Seed"
		break
	}
	if (-not $Verdict) {
		Write-Host "WIN: FAIL ($Culture, no win on seeds $($Seeds -join ' '))"
		exit 1
	}
	$Passed++
}
Write-Host "WIN: PASS ($($Won -join ', '): toast, file, Shrine, bestiary, Recollections)"
exit 0
