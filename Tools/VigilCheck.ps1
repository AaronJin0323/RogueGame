# The hub proved on real launches (gate step `vigil`, vigil.md R-VIG-17, T-VIG-17), off-screen, no window.
#   & Tools\VigilCheck.ps1 [-Cultures en,zh-Hans] [-Seed 9107] [-HandSeed 9110] [-Timeout 300]
# For each culture, four launches on one isolated save: `First` goes by real key events from boot through every page to the
# Shrine, a run and back, then buys two Contracts nodes on a planted balance; `Reboot` opens a new process on that save and proves
# the purchases, LastPatron and that Restart in a run still reloads into a run; `Gated` begins a run from the hub under -DTPoolGate
# and reads what was bought in its pool; `Large` reads every page at the largest text, every row on the real font.
# -DTHub is what lets these -unattended launches see the hub; nothing else passes it.
# Then two bot runs on the seed -HandSeed, gate on: one on a copy of the save, where the bought cards must reach a hand, and one with
# no save, where they must not. Shots of every page: Saved\VigilCheck\<culture>\<page>.png.
# Prints "VIGIL: PASS (...)" or "VIGIL: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\VigilCheck_<culture>_<script>.log.
param(
	[string[]]$Cultures = @("en", "zh-Hans"),
	[int]$Seed = 9107,
	[int]$HandSeed = 9110,
	[int]$BotSeconds = 300,
	[int]$Timeout = 300
)

trap { Write-Host "VIGIL: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "VigilCheck"  # D-055 item 7: one heavy-job slot for this run, none for a step of a run that holds one

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$CmdExe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$OutDir = Join-Path $Root "Saved\VigilCheck"
# Every launch has a -userdir under Saved\VigilCheck\userdir: its run files land in that folder's Saved\Playtests, never in Saved\Playtests, where a person's Play.bat runs
# live, so this check deletes by seed only what it wrote itself (review #58, #104).
$UserDir = Join-Path $OutDir "userdir"
$Playtests = Join-Path $UserDir "Saved\Playtests"
$Pages = @("root", "shrine", "contracts", "contracts_bought", "contracts_sealed", "contracts_locked", "contracts_after", "codex", "ledger", "settings", "root_chosen", "runover", "ledger_after",
	"root_large", "shrine_large", "contracts_large", "codex_large", "ledger_large")
$Cultures = @($Cultures | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
New-Item -ItemType Directory -Force $LogDir, $OutDir | Out-Null
if (Test-Path $UserDir) { Remove-Item $UserDir -Recurse -Force }

function Invoke-Launch([string]$Culture, [string]$Script, [string]$Save, [string]$Shots, [string[]]$Extra = @()) {
	$Log = Join-Path $LogDir "VigilCheck_${Culture}_$Script.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-LiveCoding=false",
		"-unattended", "-nopause", "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTHub", "-DTHubCheck=$Script", "-DTHubShots=`"$Shots`"", "-DTMetaFile=`"$Save`"", "-DTSeed=$Seed", "-userdir=`"$UserDir`"") + $Extra
	$Proc = Start-DTHidden $Exe $ArgList
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		Stop-DTHidden $Proc
		Write-Host "VIGIL: FAIL ($Culture $Script timed out)"
		exit 1
	}
	$Lines = @(Select-String -Path $Log -Pattern "VIGILCHECK" | ForEach-Object { $_.Line -replace "^.*LogDT: ", "" })
	$Lines | Where-Object { $_ -match "WRONG|past their room|VIGILCHECK:" } | ForEach-Object { "    $Culture $Script $_" } | Write-Host
	$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed")
	return [bool]($Lines | Where-Object { $_ -match "^VIGILCHECK: PASS" }) -and $Crash.Count -eq 0
}

# A gated bot run on -HandSeed; returns the hands it was offered.
function Get-Hands([string]$Tag, [string[]]$Extra = @()) {
	$Log = Join-Path $LogDir "VigilCheck_hands_$Tag.log"
	if (Test-Path $Log) { Remove-Item $Log -Force }
	Get-ChildItem $Playtests -Filter "${HandSeed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-nullrhi", "-nosplash", "-nopause", "-unattended", "-LiveCoding=false", "-ABSLOG=`"$Log`"",
		"-DTBot=Steered", "-DTBotVersion=2", "-DTBotSkill=1.0", "-DTBotPath=Storm", "-DTBotArchetype=Tempest", "-DTBotFast=60", "-DTBotQuit", "-DTBotMaxSeconds=$BotSeconds", "-DTSeed=$HandSeed", "-DTPoolGate", "-userdir=`"$UserDir`"") + $Extra
	$Proc = Start-DTHidden $CmdExe $ArgList
	if (-not $Proc.WaitForExit($Timeout * 1000)) {
		Stop-DTHidden $Proc
		Write-Host "VIGIL: FAIL (hands $Tag timed out)"
		exit 1
	}
	$File = Get-ChildItem $Playtests -Filter "${HandSeed}_*.jsonl" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
	$Text = if ($File) { [IO.File]::ReadAllText($File.FullName) } else { "" }
	Get-ChildItem $Playtests -Filter "${HandSeed}_*" -ErrorAction SilentlyContinue | Remove-Item -Force
	# A run that crashed, or ended before a level hand, proves nothing about which cards reach a hand: both arms must reach their end with hands past the Patron's.
	$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===|Ensure condition failed" -ErrorAction SilentlyContinue)
	$Hands = ([regex]::Matches($Text, '"kind":\s*"offer",')).Count
	if ($Crash.Count -gt 0 -or $Text -notmatch '"kind":\s*"run_end"' -or $Hands -lt 2) {
		Write-Host "VIGIL: FAIL (hands $Tag ran short: crash lines $($Crash.Count), run_end $($Text -match '"kind":\s*"run_end"'), $Hands hand(s) offered)"
		exit 1
	}
	return $Text
}

$Passed = 0
$HandSave = $null
foreach ($Culture in $Cultures) {
	$Shots = Join-Path $OutDir $Culture
	if (Test-Path $Shots) { Remove-Item $Shots -Recurse -Force }
	New-Item -ItemType Directory -Force $Shots | Out-Null
	$Save = Join-Path $Shots "DTMeta.sav"
	$Large = Join-Path $Shots "Large.ini"
	[IO.File]::WriteAllText($Large, "Version=1`nText=150`n", (New-Object Text.UTF8Encoding($false)))
	$First = Invoke-Launch $Culture "First" $Save $Shots
	$Reboot = $First -and (Invoke-Launch $Culture "Reboot" $Save $Shots)
	$Gated = $Reboot -and (Invoke-Launch $Culture "Gated" $Save $Shots @("-DTPoolGate"))
	$Fits = $Gated -and (Invoke-Launch $Culture "Large" $Save $Shots @("-DTSettingsFile=`"$Large`""))
	$Missing = @($Pages | Where-Object { -not (Test-Path (Join-Path $Shots "$_.png")) })
	if (-not $Fits -or $Missing.Count -gt 0) {
		Write-Host "VIGIL: FAIL ($Culture first=$First reboot=$Reboot gated=$Gated large=$Fits, no shot of: $($Missing -join ' '))"
		exit 1
	}
	$HandSave = $Save
	$Passed++
}

$Bought = Join-Path $OutDir "Hands.sav"
Copy-Item $HandSave $Bought -Force
$WithSave = Get-Hands "bought" @("-DTMetaFile=`"$Bought`"")
$NoSave = Get-Hands "fresh"
$Cards = @('"id":"Weapon\.ScatterCannon"', '"id":"Blessing\.Storm\.Tempest\.Amp1"')
$Reached = @($Cards | Where-Object { $WithSave -match $_ })
$Leaked = @($Cards | Where-Object { $NoSave -match $_ })
if ($Reached.Count -ne $Cards.Count -or $Leaked.Count -gt 0 -or $NoSave.Length -eq 0) {
	Write-Host "VIGIL: FAIL (bought cards reached $($Reached.Count) of $($Cards.Count) on seed $HandSeed, $($Leaked.Count) leaked to a fresh pool)"
	exit 1
}
Write-Host "VIGIL: PASS ($Passed culture(s), boot to run and back, a purchase across processes into a gated run and a hand on seed $HandSeed, $($Pages.Count) shots each)"
exit 0
