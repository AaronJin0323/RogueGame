# The hub proved on real launches (gate step `vigil`, vigil.md R-VIG-17, T-VIG-17), off-screen, no window.
#   & Tools\VigilCheck.ps1 [-Cultures en,zh-Hans] [-Seed 9107] [-Timeout 300]
# For each culture, two launches on one isolated save: `First` goes by real key events from boot through every page to the
# Shrine, a run and back; `Reboot` opens a new process on that save and proves LastPatron, then that Restart in a run still
# reloads into a run. -DTHub is what lets these two -unattended launches see the hub; nothing else passes it.
# Shots of every page: Saved\VigilCheck\<culture>\<page>.png. Prints "VIGIL: PASS (...)" or "VIGIL: FAIL (...)"; exit 0 / 1.
# Logs: Saved\Logs\VigilCheck_<culture>_<script>.log.
param(
	[string[]]$Cultures = @("en", "zh-Hans"),
	[int]$Seed = 9107,
	[int]$Timeout = 300
)

trap { Write-Host "VIGIL: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\VigilCheck"
$Pages = @("root", "shrine", "contracts", "codex", "ledger", "settings", "root_chosen", "runover", "ledger_after")
$Cultures = @($Cultures | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
New-Item -ItemType Directory -Force $LogDir, $OutDir | Out-Null

function Invoke-Launch([string]$Culture, [string]$Script, [string]$Save, [string]$Shots) {
	$Log = Join-Path $LogDir "VigilCheck_${Culture}_$Script.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-LiveCoding=false",
		"-unattended", "-nopause", "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTHub", "-DTHubCheck=$Script", "-DTHubShots=`"$Shots`"", "-DTMetaFile=`"$Save`"", "-DTSeed=$Seed")
	$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		Write-Host "VIGIL: FAIL ($Culture $Script timed out)"
		exit 1
	}
	$Lines = @(Select-String -Path $Log -Pattern "VIGILCHECK" | ForEach-Object { $_.Line -replace "^.*LogDT: ", "" })
	$Lines | Where-Object { $_ -match "WRONG|VIGILCHECK:" } | ForEach-Object { "    $Culture $Script $_" }
	$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed")
	return [bool]($Lines | Where-Object { $_ -match "^VIGILCHECK: PASS" }) -and $Crash.Count -eq 0
}

$Passed = 0
foreach ($Culture in $Cultures) {
	$Shots = Join-Path $OutDir $Culture
	if (Test-Path $Shots) { Remove-Item $Shots -Recurse -Force }
	New-Item -ItemType Directory -Force $Shots | Out-Null
	$Save = Join-Path $Shots "DTMeta.sav"
	$First = Invoke-Launch $Culture "First" $Save $Shots
	$Reboot = $First -and (Invoke-Launch $Culture "Reboot" $Save $Shots)
	$Missing = @($Pages | Where-Object { -not (Test-Path (Join-Path $Shots "$_.png")) })
	if (-not $First -or -not $Reboot -or $Missing.Count -gt 0) {
		Write-Host "VIGIL: FAIL ($Culture first=$First reboot=$Reboot, no shot of: $($Missing -join ' '))"
		exit 1
	}
	$Passed++
}
Write-Host "VIGIL: PASS ($Passed culture(s), boot to run and back, $($Pages.Count) shots each)"
exit 0
