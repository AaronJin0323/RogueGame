# Enemy art acceptance (02_Systems/enemy_art.md, S4 wave 1 package B): every machine check of the Cast-off hulls, one line each.
#   & Tools\EnemyArt.ps1 [-NoFrames] [-NoMeshCheck] [-Seconds 4]
# 1. geometry   Tools\enemy_art_check.py on the generators: silhouette overlap, floor hidden, optic, elite light, triangles, parts (T-EAR-03 to T-EAR-07).
# 2. meshes     Tools\blender\Mesh.ps1 -Check per role: the committed SourceArt file is what its generator builds (Blender in the background). -NoMeshCheck skips it.
# 3. frames     the line-up of DT.Enemies.LineUp, shot by the game at each of C.EAR.SILHOUETTE_PX_PER_M, judged by enemy_art_check.py --frame (T-EAR-07).
#               The frame keeps today's framing (C.ARENA.CAMERA_FRAME_RADIUS_UU) and changes its height in pixels, so 36 px/m is 1920x1080 and 27 px/m,
#               the density the frame of D-036 falls to if it reverts, is 1440x810. The hulls are staged with "noshadow": the mask is the hull's.
#               -NoFrames skips it.
# The game is launched as Tools\Smoke.ps1 launches it (UnrealEditor -game -RenderOffScreen, on the hidden desktop of Tools\HiddenLaunch.ps1, -DTShot): the
# editor binary keeps its files under this tree's Saved. -ForceRes makes -ResX and -ResY the frame's size: without it the engine shrinks a frame
# larger than 1280x720 on this desktop (1920x1080 came out 888x500), and the judge refuses a frame at the wrong density. Shots: Saved\EnemyArt\lineup_<px per m>.png; the judged masks: Saved\EnemyArt\masks_frames.png and masks_geometry.png.
# Not a gate step: Blender is not started by the gate. Run it in the package's own acceptance.
# Prints "ENEMY ART ACCEPTANCE: PASS|FAIL (...)"; exit 0 / 1. A silhouette reading is a proxy: it does not replace a person naming the role.
param(
	[switch]$NoFrames,
	[switch]$NoMeshCheck,
	[double]$Seconds = 4,
	[int]$LoadTimeout = 900
)

trap { Write-Host "ENEMY ART ACCEPTANCE: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$OutDir = Join-Path $Root "Saved\EnemyArt"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Failed = @()

function Get-ConstantText([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*(\[[^\]]*\]|[-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return $Line.Matches[0].Groups[1].Value
}

# --- 1. geometry ----------------------------------------------------------------------------------------------------------
$Out = & python -I (Join-Path $Root "Tools\enemy_art_check.py") --sheet (Join-Path $OutDir "masks_geometry.png") 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { $Failed += "geometry" }
$Out | ForEach-Object { "    $_" }
Write-Host ("geometry:   " + (($Out | Select-String "^ENEMY ART:" | Select-Object -Last 1).Line))

# --- 2. the committed meshes are what the generators build ----------------------------------------------------------------
if ($NoMeshCheck) { Write-Host "meshes:     NOT RUN (-NoMeshCheck)" }
else {
	$Sources = @(Get-ChildItem (Join-Path $Root "Tools\blender") -Filter "enemy_*.py" | Sort-Object Name)
	$Bad = @()
	foreach ($Source in $Sources) {
		$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\blender\Mesh.ps1") -Source $Source.FullName -Check 2>&1 | ForEach-Object { "$_" }
		if ($LASTEXITCODE -ne 0) { $Bad += $Source.Name; $Out | Select-Object -Last 6 | ForEach-Object { "    $_" } }
	}
	if ($Bad.Count) { $Failed += "meshes"; Write-Host "meshes:     FAIL ($($Bad -join ', '): SourceArt differs from what the generator builds)" }
	else { Write-Host "meshes:     PASS ($($Sources.Count) generators, each SourceArt file byte-equal to a fresh build)" }
}

# --- 3. frames ------------------------------------------------------------------------------------------------------------
if ($NoFrames) { Write-Host "frames:     NOT RUN (-NoFrames)" }
else {
	$Radius = [double](Get-ConstantText "ARENA.CAMERA_FRAME_RADIUS_UU")
	$Densities = @((Get-ConstantText "EAR.SILHOUETTE_PX_PER_M").Trim("[]") -split "," | ForEach-Object { [double]$_.Trim() })
	$Shots = @()
	foreach ($Density in $Densities) {
		$ResY = [int][math]::Round($Density * 2.0 * $Radius / 100.0)
		$ResX = [int][math]::Round($ResY * 16.0 / 9.0)
		$Shot = Join-Path $OutDir ("lineup_{0}.png" -f [int]$Density)
		$Log = Join-Path $Root ("Saved\Logs\EnemyArt_{0}.log" -f [int]$Density)
		foreach ($Old in $Shot, $Log) { if (Test-Path $Old) { Remove-Item $Old -Force } }
		$ArgList = @("`"$Project`"", "/Game/DT/Maps/L_Arena_Test", "-game", "-windowed", "-RenderOffScreen", "-ResX=$ResX", "-ResY=$ResY", "-ForceRes", "-LiveCoding=false", "-nosplash",
			"-unattended", "-culture=en", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", ("-DTShotAfter={0}" -f $Seconds), "-DTNoDirector",
			"-ExecCmds=`"DT.God 1,DT.Enemies.LineUp 1 noshadow`"")
		$Proc = Start-DTHidden $Exe $ArgList
		Write-Host ("frames: {0} px/m at {1}x{2} (PID {3})" -f $Density, $ResX, $ResY, $Proc.Id)
		try {
			$Deadline = (Get-Date).AddSeconds($LoadTimeout + $Seconds + 120)
			while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1 }
		}
		finally {
			if (-not $Proc.HasExited) { Stop-DTHidden $Proc; Write-Host "    ! killed at the timeout" }
		}
		if (Test-Path $Shot) { $Shots += $Shot } else { Write-Host "    ! no shot at $Density px/m; see $Log" }
	}
	if ($Shots.Count -ne $Densities.Count) { $Failed += "frames"; Write-Host "frames:     FAIL ($($Shots.Count) of $($Densities.Count) shots)" }
	else {
		$JudgeArgs = @((Join-Path $Root "Tools\enemy_art_check.py"), "--sheet", (Join-Path $OutDir "masks_frames.png"))
		foreach ($Shot in $Shots) { $JudgeArgs += @("--frame", $Shot) }
		$Out = & python -I @JudgeArgs 2>&1 | ForEach-Object { "$_" }
		if ($LASTEXITCODE -ne 0) { $Failed += "frames" }
		$Out | ForEach-Object { "    $_" }
		Write-Host ("frames:     " + (($Out | Select-String "^ENEMY ART:" | Select-Object -Last 1).Line))
	}
}

if ($Failed.Count) { Write-Host "ENEMY ART ACCEPTANCE: FAIL ($($Failed -join ', '))"; exit 1 }
Write-Host "ENEMY ART ACCEPTANCE: PASS (geometry$(if (-not $NoMeshCheck) { ', meshes' })$(if (-not $NoFrames) { ', frames' }); the silhouette reading is a proxy, not a person naming the role)"
exit 0
