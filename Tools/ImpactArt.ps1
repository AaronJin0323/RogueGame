# Impact art acceptance (02_Systems/impact_art.md, S4 wave 1 package E): the four staged frames at both densities, one line each.
#   & Tools\ImpactArt.ps1 [-NoMeshCheck] [-Scenes impact_hit,impact_kill,impact_dense,impact_hero] [-LoadTimeout 900]
# 1. meshes   Tools\blender\Mesh.ps1 -Check on Tools\blender\debris.py: the committed SourceArt shards are what the generator builds
#             (Blender in the background). -NoMeshCheck skips it.
# 2. frames   each scene of Tools\Scenes (its commands, seconds, director and seed, as Tools\Smoke.ps1 reads them) shot by the game at each of
#             C.EAR.SILHOUETTE_PX_PER_M: today's framing (C.ARENA.CAMERA_FRAME_RADIUS_UU) at 36 px/m is 1920x1080, at 27 px/m 1440x810. Each scene
#             runs DT.Impact.Stats hold, so its log names what the held frame holds; a frame whose line shows no effect of the kind its scene
#             is for fails: impact_hit sparks and no flash in PerKill, impact_kill flashes and debris in PerKill with the rate never over C.PRE.DENSITY_LOD_KILLS_PER_S, impact_dense flashes in Crowd,
#             impact_hero a hero ring.
# The game is launched as Tools\EnemyArt.ps1 launches it (UnrealEditor -game -RenderOffScreen, on the hidden desktop of Tools\HiddenLaunch.ps1,
# -DTShot, -ForceRes). Shots: Saved\ImpactArt\<scene>_<px per m>.png; logs Saved\Logs\ImpactArt_<scene>_<px per m>.log.
# dt.Impact.Draw follows the environment variable DT_IMPACT_DRAW, which the game reads at start: set it to 0 to shoot the same frames
# without the effects (then every frame fails its kind check, by design).
# Not a gate step: Blender is not started by the gate. Prints "IMPACT ART ACCEPTANCE: PASS|FAIL (...)"; exit 0 / 1. A frame that holds an
# effect is not a person seeing it read.
param(
	[switch]$NoMeshCheck,
	[string[]]$Scenes = @("impact_hit", "impact_kill", "impact_dense", "impact_hero"),
	[int]$LoadTimeout = 900
)

trap { Write-Host "IMPACT ART ACCEPTANCE: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

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
		"impact_hit" { return ($S.tier -eq "PerKill" -and $S.sparks -gt 0 -and $S.flashes -eq 0) }
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
$Lines = @()
foreach ($Scene in $Scenes) {
	$File = Join-Path $Root "Tools\Scenes\$Scene.json"
	if (-not (Test-Path $File)) { $Failed += $Scene; Write-Host "frames: $Scene has no Tools\Scenes\$Scene.json"; continue }
	$Data = Get-Content $File -Raw -Encoding UTF8 | ConvertFrom-Json
	$Keys = @($Data.PSObject.Properties.Name)
	$Seconds = if ($Keys -contains "seconds") { [double]$Data.seconds } else { [double](Get-ConstantText "SMOKE.SECONDS") }
	$Cmds = (@($Data.cmds) -join ",")
	foreach ($Density in $Densities) {
		$ResY = [int][math]::Round($Density * 2.0 * $Radius / 100.0)
		$ResX = [int][math]::Round($ResY * 16.0 / 9.0)
		$Tag = "{0}_{1}" -f $Scene, [int]$Density
		$Shot = Join-Path $OutDir "$Tag.png"
		$Log = Join-Path $Root "Saved\Logs\ImpactArt_$Tag.log"
		foreach ($Old in $Shot, $Log) { if (Test-Path $Old) { Remove-Item $Old -Force } }
		$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-windowed", "-RenderOffScreen", "-ResX=$ResX", "-ResY=$ResY", "-ForceRes", "-LiveCoding=false", "-nosplash",
			"-unattended", "-culture=en", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", ("-DTShotAfter={0}" -f $Seconds))
		if (-not [bool]$Data.director) { $ArgList += "-DTNoDirector" }
		if ($Keys -contains "seed") { $ArgList += ("-DTSeed={0}" -f [int]$Data.seed) }
		if ($Cmds) { $ArgList += "-ExecCmds=`"$Cmds`"" }
		$Proc = Start-DTHidden $Exe $ArgList
		Write-Host ("frames: {0} at {1} px/m, {2}x{3} (PID {4})" -f $Scene, $Density, $ResX, $ResY, $Proc.Id)
		try {
			$Deadline = (Get-Date).AddSeconds($LoadTimeout + $Seconds + 120)
			while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1 }
		}
		finally {
			if (-not $Proc.HasExited) { Stop-DTHidden $Proc; Write-Host "    ! killed at the timeout" }
		}
		$Held = if (Test-Path $Log) { Get-Content $Log -Encoding UTF8 | Where-Object { $_ -match "DT\.Impact\.Stats held: " } | Select-Object -Last 1 } else { $null }
		if (-not (Test-Path $Shot)) { $Failed += $Tag; Write-Host "    ! no shot; see $Log"; continue }
		if (-not $Held) { $Failed += $Tag; Write-Host "    ! no DT.Impact.Stats held line; see $Log"; continue }
		$Line = $Held -replace "^.*DT\.Impact\.Stats held: ", ""
		$S = @{}
		if ($Line -match "tier (\w+) rate ([0-9.]+) kills/s \(peak ([0-9.]+)\)") { $S.tier = $Matches[1]; $S.rate = [double]$Matches[2]; $S.peak = [double]$Matches[3] }
		if ($Line -match "live sparks (\d+) flashes (\d+) pieces (\d+) rings (\d+)") { $S.sparks = [int]$Matches[1]; $S.flashes = [int]$Matches[2]; $S.pieces = [int]$Matches[3]; $S.rings = [int]$Matches[4] }
		$Ok = $S.ContainsKey("tier") -and $S.ContainsKey("sparks") -and (Test-Held $Scene $S)
		if (-not $Ok) { $Failed += $Tag }
		$Lines += ("    {0} {1}: {2}" -f $Tag, $(if ($Ok) { "ok" } else { "FAIL" }), $Line)
	}
}
$Lines | ForEach-Object { $_ }

if ($Failed.Count) { Write-Host "IMPACT ART ACCEPTANCE: FAIL ($($Failed -join ', '))"; exit 1 }
Write-Host ("IMPACT ART ACCEPTANCE: PASS ({0}{1} frames, each holding its effect; a frame that holds an effect is not a person seeing it read)" -f $(if (-not $NoMeshCheck) { "meshes, " } else { "" }), $Lines.Count)
exit 0
