# The hero's clear zone, acceptance (02_Systems/tracer_art.md R-TRC-01, T-TRC-01; D-068): tracer_crossing at both frames, zone on and off.
#   & Tools\HeroZone.ps1 [-Runs 1] [-LoadTimeout 900] [-OutDir Saved\HeroZone]
# Shoots Tools/Scenes/tracer_crossing.json (its commands, seconds, director and seed, as Tools\Smoke.ps1 reads them) at each of
# C.EAR.SILHOUETTE_PX_PER_M (36 px/m is 1920x1080, 27 px/m 1440x810), with the zone on (the build's C.TRC.CLEAR_ZONE_RATIO) and off
# (DT_TRACER_CLEAR_ZONE=0, read by the game at start), -Runs times each, and reads every frame with Tools\hero_zone.py: the zone's share of
# round-head tone, the round-head pixels over the figure's box, the next ring's share. Frames with the zone on must pass the script's limits
# for their size; frames with it off are printed as the "before".
# The game is launched as Tools\ImpactArt.ps1 launches it (UnrealEditor -game -RenderOffScreen, on the hidden desktop of Tools\HiddenLaunch.ps1,
# -DTShot, -ForceRes). Shots: <OutDir>\tracer_crossing_<px per m>_<on|off>_<run>.png; logs Saved\Logs\HeroZone_<same>.log.
# Not a gate step. Prints "HERO ZONE ACCEPTANCE: PASS|FAIL (...)"; exit 0 / 1. A frame that passes is not a person finding the hero.
param(
	[int]$Runs = 1,
	[int]$LoadTimeout = 900,
	[string]$OutDir = ""
)

trap { Write-Host "HERO ZONE ACCEPTANCE: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "HeroZone"  # D-055 item 7, R-TEC-18: one machine slot for the game launches

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
if (-not $OutDir) { $OutDir = Join-Path $Root "Saved\HeroZone" }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Failed = @()

function Get-ConstantText([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*(\[[^\]]*\]|[-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return $Line.Matches[0].Groups[1].Value
}

$Scene = "tracer_crossing"
$Data = Get-Content (Join-Path $Root "Tools\Scenes\$Scene.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$Keys = @($Data.PSObject.Properties.Name)
$Seconds = if ($Keys -contains "seconds") { [double]$Data.seconds } else { [double](Get-ConstantText "SMOKE.SECONDS") }
$Cmds = (@($Data.cmds) -join ",")
$Radius = [double](Get-ConstantText "ARENA.CAMERA_FRAME_RADIUS_UU")
$Densities = @((Get-ConstantText "EAR.SILHOUETTE_PX_PER_M").Trim("[]") -split "," | ForEach-Object { [double]$_.Trim() })
$Lines = @()

foreach ($Density in $Densities) {
	$ResY = [int][math]::Round($Density * 2.0 * $Radius / 100.0)
	$ResX = [int][math]::Round($ResY * 16.0 / 9.0)
	foreach ($Side in "off", "on") {
		for ($Run = 1; $Run -le $Runs; $Run++) {
			$Tag = "{0}_{1}_{2}_{3}" -f $Scene, [int]$Density, $Side, $Run
			$Shot = Join-Path $OutDir "$Tag.png"
			$Log = Join-Path $Root "Saved\Logs\HeroZone_$Tag.log"
			foreach ($Old in $Shot, $Log) { if (Test-Path $Old) { Remove-Item $Old -Force } }
			$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-windowed", "-RenderOffScreen", "-ResX=$ResX", "-ResY=$ResY", "-ForceRes", "-LiveCoding=false", "-nosplash",
				"-unattended", "-culture=en", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", ("-DTShotAfter={0}" -f $Seconds))
			if (-not [bool]$Data.director) { $ArgList += "-DTNoDirector" }
			if ($Keys -contains "seed") { $ArgList += ("-DTSeed={0}" -f [int]$Data.seed) }
			if ($Cmds) { $ArgList += "-ExecCmds=`"$Cmds`"" }
			$Before = $env:DT_TRACER_CLEAR_ZONE
			if ($Side -eq "off") { $env:DT_TRACER_CLEAR_ZONE = "0" } else { Remove-Item Env:\DT_TRACER_CLEAR_ZONE -ErrorAction SilentlyContinue }
			try { $Proc = Start-DTHidden $Exe $ArgList }
			finally { if ($null -eq $Before) { Remove-Item Env:\DT_TRACER_CLEAR_ZONE -ErrorAction SilentlyContinue } else { $env:DT_TRACER_CLEAR_ZONE = $Before } }
			Write-Host ("frames: {0}, {1}x{2} (PID {3})" -f $Tag, $ResX, $ResY, $Proc.Id)
			try {
				$Deadline = (Get-Date).AddSeconds($LoadTimeout + $Seconds + 120)
				while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1 }
			}
			finally {
				if (-not $Proc.HasExited) { Stop-DTHidden $Proc; Write-Host "    ! killed at the timeout" }
			}
			if (-not (Test-Path $Shot)) { $Failed += $Tag; Write-Host "    ! no shot; see $Log"; continue }
			$CheckArgs = @((Join-Path $Root "Tools\hero_zone.py"), $Shot)
			if ($Side -eq "on") { $CheckArgs += "--check" }
			$Out = & python -I @CheckArgs 2>&1 | ForEach-Object { "$_" }
			if ($Side -eq "on" -and $LASTEXITCODE -ne 0) { $Failed += $Tag }
			$Lines += ("    {0}: {1}" -f $Tag, (($Out | Select-String "^HERO ZONE:" | ForEach-Object { $_.Line -replace "^HERO ZONE: ", "" }) -join " | "))
		}
	}
}
$Lines | ForEach-Object { $_ }

if ($Failed.Count) { Write-Host "HERO ZONE ACCEPTANCE: FAIL ($($Failed -join ', '))"; exit 1 }
Write-Host ("HERO ZONE ACCEPTANCE: PASS ({0} frames with the zone on inside their limits; the frames with it off are the before)" -f ($Densities.Count * $Runs))
exit 0
