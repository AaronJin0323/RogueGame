# Impact art acceptance (02_Systems/impact_art.md, S4 wave 1 package E): the four staged frames at both densities, one line each.
#   & Tools\ImpactArt.ps1 [-NoMeshCheck] [-Scenes impact_hit,impact_kill,impact_dense,impact_hero] [-LoadTimeout 900]
# 1. meshes   Tools\blender\Mesh.ps1 -Check on Tools\blender\debris.py: the committed SourceArt shards are what the generator builds
#             (Blender in the background). -NoMeshCheck skips it.
# 2. frames   each scene of Tools\Scenes (its commands, seconds, director and seed, as Tools\Smoke.ps1 reads them) shot by the game at each of
#             C.EAR.SILHOUETTE_PX_PER_M: today's framing (C.ARENA.CAMERA_FRAME_RADIUS_UU) at 36 px/m is 1920x1080, at 27 px/m 1440x810. Each scene
#             runs DT.Impact.Stats hold, so its log names what the held frame holds; a frame whose line shows no effect of the kind its scene
#             is for fails: impact_hit six sparks or more and no flash in PerKill, impact_kill flashes and debris in PerKill with the rate never over C.PRE.DENSITY_LOD_KILLS_PER_S, impact_dense flashes in Crowd,
#             impact_hero a hero ring. impact_hero is shot again -LateSteps sim steps later (impact_hero_late), and each hero frame beside a twin
#             with effects off: Tools\impact_ring.py counts the pixels in Pal.Enemy.Muzzle's hue round the hero that the twin lacks, and the hurt-step
#             frame fails under -RingFloor36 at 36 px/m and -RingFloor27 at 27 px/m (the late frame's count is reported).
# The game is launched as Tools\EnemyArt.ps1 launches it (UnrealEditor -game -RenderOffScreen, on the hidden desktop of Tools\HiddenLaunch.ps1,
# -DTShot, -ForceRes). Shots: Saved\ImpactArt\<scene>_<px per m>.png; logs Saved\Logs\ImpactArt_<scene>_<px per m>.log.
# dt.Impact.Draw follows the environment variable DT_IMPACT_DRAW, which the game reads at start: set it to 0 to shoot the same frames
# without the effects (then every frame fails its kind check, by design).
# Not a gate step: Blender is not started by the gate. Prints "IMPACT ART ACCEPTANCE: PASS|FAIL (...)"; exit 0 / 1. A frame that holds an
# effect is not a person seeing it read.
param(
	[switch]$NoMeshCheck,
	[string[]]$Scenes = @("impact_hit", "impact_kill", "impact_dense", "impact_hero"),
	[int]$LoadTimeout = 900,
	[int]$LateSteps = 6,
	[int]$RingFloor36 = 250,
	[int]$RingFloor27 = 140
)

trap { Write-Host "IMPACT ART ACCEPTANCE: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "ImpactArt"  # D-055 item 7, R-TEC-18: one machine slot for Blender and the game

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$OutDir = Join-Path $Root "Saved\ImpactArt"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Failed = @()

function Get-ConstantText([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*(\[[^\]]*\]|[-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return $Line.Matches[0].Groups[1].Value
}

# What each scene's held frame must hold, read from its DT.Impact.Stats line.
function Test-Held([string]$Scene, [hashtable]$S) {
	switch ($Scene) {
		"impact_hit" { return ($S.tier -eq "PerKill" -and $S.sparks -ge 6 -and $S.flashes -eq 0) }
		"impact_kill" { return ($S.tier -eq "PerKill" -and $S.peak -le [double](Get-ConstantText "PRE.DENSITY_LOD_KILLS_PER_S") -and $S.flashes -gt 0 -and $S.pieces -gt 0) }
		"impact_dense" { return ($S.tier -eq "Crowd" -and $S.flashes -gt 0) }
		"impact_hero" { return ($S.rings -gt 0) }
		default { return ($S.sparks + $S.flashes + $S.pieces + $S.rings -gt 0) }
	}
}

# --- 1. the committed shards are what the generator builds ----------------------------------------------------------------
if ($NoMeshCheck) { Write-Host "meshes:     NOT RUN (-NoMeshCheck)" }
else {
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\blender\Mesh.ps1") -Source (Join-Path $Root "Tools\blender\debris.py") -Check 2>&1 | ForEach-Object { "$_" }
	if ($LASTEXITCODE -ne 0) { $Failed += "meshes"; $Out | Select-Object -Last 6 | ForEach-Object { "    $_" } }
	Write-Host ("meshes:     " + (($Out | Select-String "^MESH:" | Select-Object -Last 1).Line))
}

# --- 2. frames ------------------------------------------------------------------------------------------------------------
$Radius = [double](Get-ConstantText "ARENA.CAMERA_FRAME_RADIUS_UU")
$Densities = @((Get-ConstantText "EAR.SILHOUETTE_PX_PER_M").Trim("[]") -split "," | ForEach-Object { [double]$_.Trim() })
$Hz = [double](Get-ConstantText "SIM.FIXED_HZ")
$HeroRadius = [double](Get-ConstantText "HERO.RADIUS_UU")
$RingFloor = @{ 36 = $RingFloor36; 27 = $RingFloor27 }
$Lines = @()

# One launch: returns the stats line of its held step, or $null (and records the failure).
function Invoke-Shot([string]$Tag, [object]$Data, [string[]]$Keys, [double]$Seconds, [string]$Cmds, [int]$ResX, [int]$ResY, [string]$Draw) {
	$Shot = Join-Path $OutDir "$Tag.png"
	$Log = Join-Path $Root "Saved\Logs\ImpactArt_$Tag.log"
	foreach ($Old in $Shot, $Log) { if (Test-Path $Old) { Remove-Item $Old -Force } }
	$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-windowed", "-RenderOffScreen", "-ResX=$ResX", "-ResY=$ResY", "-ForceRes", "-LiveCoding=false", "-nosplash",
		"-unattended", "-culture=en", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", ("-DTShotAfter={0}" -f $Seconds))
	if (-not [bool]$Data.director) { $ArgList += "-DTNoDirector" }
	if ($Keys -contains "seed") { $ArgList += ("-DTSeed={0}" -f [int]$Data.seed) }
	if ($Cmds) { $ArgList += "-ExecCmds=`"$Cmds`"" }
	$Before = $env:DT_IMPACT_DRAW
	if ($Draw) { $env:DT_IMPACT_DRAW = $Draw }
	try { $Proc = Start-DTHidden $Exe $ArgList }
	finally { if ($null -eq $Before) { Remove-Item Env:\DT_IMPACT_DRAW -ErrorAction SilentlyContinue } else { $env:DT_IMPACT_DRAW = $Before } }
	Write-Host ("frames: {0}, {1}x{2} (PID {3})" -f $Tag, $ResX, $ResY, $Proc.Id)
	try {
		$Deadline = (Get-Date).AddSeconds($LoadTimeout + $Seconds + 120)
		while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1 }
	}
	finally {
		if (-not $Proc.HasExited) { Stop-DTHidden $Proc; Write-Host "    ! killed at the timeout" }
	}
	$Held = if (Test-Path $Log) { Get-Content $Log -Encoding UTF8 | Where-Object { $_ -match "DT\.Impact\.Stats held: " } | Select-Object -Last 1 } else { $null }
	if (-not (Test-Path $Shot)) { $script:Failed += $Tag; Write-Host "    ! no shot; see $Log"; return $null }
	if (-not $Held) { $script:Failed += $Tag; Write-Host "    ! no DT.Impact.Stats held line; see $Log"; return $null }
	return ($Held -replace "^.*DT\.Impact\.Stats held: ", "")
}

function Read-Stats([string]$Line) {
	$S = @{}
	if ($Line -match "tier (\w+) rate ([0-9.]+) kills/s \(peak ([0-9.]+)\)") { $S.tier = $Matches[1]; $S.rate = [double]$Matches[2]; $S.peak = [double]$Matches[3] }
	if ($Line -match "live sparks (\d+) flashes (\d+) pieces (\d+) rings (\d+)") { $S.sparks = [int]$Matches[1]; $S.flashes = [int]$Matches[2]; $S.pieces = [int]$Matches[3]; $S.rings = [int]$Matches[4] }
	return $S
}

foreach ($Scene in $Scenes) {
	$File = Join-Path $Root "Tools\Scenes\$Scene.json"
	if (-not (Test-Path $File)) { $Failed += $Scene; Write-Host "frames: $Scene has no Tools\Scenes\$Scene.json"; continue }
	$Data = Get-Content $File -Raw -Encoding UTF8 | ConvertFrom-Json
	$Keys = @($Data.PSObject.Properties.Name)
	$Seconds = if ($Keys -contains "seconds") { [double]$Data.seconds } else { [double](Get-ConstantText "SMOKE.SECONDS") }
	$Cmds = (@($Data.cmds) -join ",")
	# The hero scene is shot again -LateSteps sim steps after its hurt step (impact_hero_late), each with a twin drawn without effects.
	$Holds = @(@{ Name = $Scene; Seconds = $Seconds })
	if ($Scene -eq "impact_hero") { $Holds += @{ Name = "impact_hero_late"; Seconds = ([math]::Round($Seconds * $Hz) + $LateSteps) / $Hz } }
	foreach ($Density in $Densities) {
		$ResY = [int][math]::Round($Density * 2.0 * $Radius / 100.0)
		$ResX = [int][math]::Round($ResY * 16.0 / 9.0)
		foreach ($Hold in $Holds) {
			$Tag = "{0}_{1}" -f $Hold.Name, [int]$Density
			$Line = Invoke-Shot $Tag $Data $Keys $Hold.Seconds $Cmds $ResX $ResY ""
			if (-not $Line) { continue }
			$S = Read-Stats $Line
			$Ok = $S.ContainsKey("tier") -and $S.ContainsKey("sparks") -and (Test-Held $Scene $S)
			$Ring = ""
			if ($Scene -eq "impact_hero") {
				# The ring's own pixels: Pal.Enemy.Muzzle's hue round the hero that the same step without effects does not hold (R-IMP-09).
				$Off = Invoke-Shot ("{0}_draw0" -f $Tag) $Data $Keys $Hold.Seconds $Cmds $ResX $ResY "0"
				$Box = [int][math]::Ceiling(3.0 * $HeroRadius * $Density / 100.0 * 1.5)
				$Need = if ($Hold.Name -eq "impact_hero") { $RingFloor[[int]$Density] } else { $null }
				$RingArgs = @((Join-Path $Root "Tools\impact_ring.py"), (Join-Path $OutDir "$Tag.png"), (Join-Path $OutDir ("{0}_draw0.png" -f $Tag)), "--radius-px", $Box)
				if ($Need) { $RingArgs += @("--need", $Need) }
				if ($Off) {
					$Ring = (& python -I @RingArgs 2>&1 | Select-Object -Last 1)
					if ($LASTEXITCODE -ne 0) { $Ok = $false }
				}
				else { $Ok = $false; $Ring = "RING: no shot without effects" }
				$Ring = " | $Ring"
			}
			if (-not $Ok) { $Failed += $Tag }
			$Lines += ("    {0} {1}: {2}{3}" -f $Tag, $(if ($Ok) { "ok" } else { "FAIL" }), $Line, $Ring)
		}
	}
}
$Lines | ForEach-Object { $_ }

if ($Failed.Count) { Write-Host "IMPACT ART ACCEPTANCE: FAIL ($($Failed -join ', '))"; exit 1 }
Write-Host ("IMPACT ART ACCEPTANCE: PASS ({0}{1} frames, each holding its effect; a frame that holds an effect is not a person seeing it read)" -f $(if (-not $NoMeshCheck) { "meshes, " } else { "" }), $Lines.Count)
exit 0
