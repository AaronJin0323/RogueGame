# The mesh pipeline (07_Technical R-TEC-06, 04_Presentation R-PRE-10; Docs/Validation/S4_p0.md): one command, no window, from a Python
# source file in this repo to a static mesh in the project.
#   & Tools\blender\Mesh.ps1 -Source Tools\blender\enemy_swarm.py [-Cook] [-Check]
#   & Tools\blender\Mesh.ps1 -SelfTest
# 1. Blender 5.2.2 (pinned: D-003, the path below or $env:DT_BLENDER) runs Tools\blender\build_mesh.py with --background, which calls each
#    function of the source's MESHES and exports SourceArt\<FAMILY>\<name>.fbx. The export is byte-reproducible.
# 2. The editor imports every exported file as /Game/DT/Meshes/<name> (Tools\blender\import_mesh.py through Tools\EditorPython.bat, a commandlet).
# 3. -Cook: the imported packages are cooked alone for Windows into Saved\MeshCook (Tools\blender\CookMesh.bat), and each must come out.
# -Check builds into a scratch folder instead and compares each file with the committed SourceArt one, byte for byte: a SourceArt file
# that its generator no longer makes fails. Nothing is imported and nothing in the repo is written.
# -SelfTest proves the whole path on Tools\blender\test_triangle.py with -Cook, twice through Blender to prove the export reproducible,
# then removes every file it made: the test mesh is never committed.
# A licence row comes before the binary (R-PRE-10): the script refuses a family with no SourceArt row in Docs\09_Catalog\licenses.csv.
# Prints "MESH: PASS (...)" or "MESH: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\Mesh_blender.log, EditorPython_import_mesh.log, CookMesh.log.
param(
	[string]$Source = "",
	[switch]$Cook,
	[switch]$Check,
	[switch]$SelfTest
)

trap { Write-Host "MESH: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "..\Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
# Blender and the editor commandlets this script starts are engines and tools whose threads would widen a process mask: this script and all it
# starts are held in a job with the mask as its affinity limit (Tools/HiddenLaunch.ps1, Hold-DTProcess), as Tools/Package.ps1 holds its cook.
. (Join-Path $PSScriptRoot "..\HiddenLaunch.ps1"); $null = Hold-DTProcess

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Blender = if ($env:DT_BLENDER) { $env:DT_BLENDER } else { "E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe" }
$BlenderVersion = "5.2.2"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
function Fail([string]$Why) { Write-Host "MESH: FAIL ($Why)"; exit 1 }

if ($SelfTest) { $Source = Join-Path $PSScriptRoot "test_triangle.py"; $Cook = $true }
if (-not $Source) { Fail "no -Source given" }
if (-not (Test-Path $Source)) { Fail "$Source does not exist" }
$Source = (Resolve-Path $Source).Path
if (-not (Test-Path $Blender)) { Fail "Blender $BlenderVersion is not at $Blender (set DT_BLENDER); not run" }

# --- the licence row comes first (R-PRE-10) -------------------------------------------------------------------------------
$Family = (Select-String -Path $Source -Pattern '^FAMILY\s*=\s*"(\w+)"' | Select-Object -First 1).Matches.Groups[1].Value
if (-not $Family) { Fail "$Source states no FAMILY" }
if (-not $SelfTest) {
	$Rows = Import-Csv -Path (Join-Path $Root "Docs\09_Catalog\licenses.csv") -Encoding UTF8
	if (-not ($Rows | Where-Object { "SourceArt/$Family/x.fbx" -like $_.asset_path })) { Fail "no licenses.csv row covers SourceArt/$Family/: the row comes before the binary (R-PRE-10)" }
}

# --- 1. Blender, in the background ----------------------------------------------------------------------------------------
function Invoke-Blender([string]$OutRoot) {
	# --background opens no window; --factory-startup ignores the user's preferences and add-ons, so the machine cannot change the mesh.
	$Out = & $Blender --background --factory-startup --python-exit-code 1 --python (Join-Path $PSScriptRoot "build_mesh.py") -- $Source $OutRoot $Root 2>&1 | ForEach-Object { "$_" }
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Mesh_blender.log") -Encoding utf8
	if ($Rc -ne 0 -or -not ($Out | Select-String "^BUILD: DONE")) { $Out | Select-Object -Last 12 | ForEach-Object { "    $_" }; Fail "Blender, rc $Rc" }
	if (-not ($Out | Select-String ("^Blender " + [regex]::Escape($BlenderVersion)))) { Fail "not Blender $BlenderVersion; the generators are pinned to it (D-003)" }
	return @($Out | Select-String "^MESH: (\S+) (.*)$" | ForEach-Object { [pscustomobject]@{ Path = $_.Matches[0].Groups[1].Value; Read = $_.Matches[0].Groups[2].Value } })
}
function Hash([string]$Path) { (Get-FileHash $Path -Algorithm SHA256).Hash }

if ($Check) {
	$Scratch = Join-Path $Root "Saved\MeshCheck"
	if (Test-Path $Scratch) { Remove-Item $Scratch -Recurse -Force }
	$Built = Invoke-Blender $Scratch
	$Bad = @()
	foreach ($M in $Built) {
		$Committed = Join-Path $Root $M.Path
		if (-not (Test-Path $Committed)) { $Bad += "$($M.Path) is not in the repo" }
		elseif ((Hash $Committed) -ne (Hash (Join-Path $Scratch $M.Path))) { $Bad += "$($M.Path) differs from what its generator builds" }
	}
	Remove-Item $Scratch -Recurse -Force
	if ($Bad.Count) { $Bad | ForEach-Object { "  ! $_" }; Fail "$($Bad.Count) of $($Built.Count) SourceArt file(s) do not match $([IO.Path]::GetFileName($Source))" }
	Write-Host "MESH: PASS (check: $($Built.Count) SourceArt file(s) are what $([IO.Path]::GetFileName($Source)) builds)"
	exit 0
}

$Built = Invoke-Blender $Root
if ($Built.Count -eq 0) { Fail "Blender exported nothing" }
$Built | ForEach-Object { "    $($_.Path) $($_.Read)" }
$Made = @($Built | ForEach-Object { Join-Path $Root $_.Path })
$Repro = ""
if ($SelfTest) {
	$First = @($Made | ForEach-Object { Hash $_ })
	$null = Invoke-Blender $Root
	$Second = @($Made | ForEach-Object { Hash $_ })
	if (($First -join ",") -ne ($Second -join ",")) { Fail "two Blender runs gave different files: the export is not reproducible" }
	$Repro = ", export reproducible"
}

# --- 2. the editor imports, as a commandlet -------------------------------------------------------------------------------
$Dest = "/Game/DT/Meshes"
$Assets = @($Built | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_.Path) })
$AssetFiles = @($Assets | ForEach-Object { Join-Path $Root "Content\DT\Meshes\$_.uasset" })
try {
	$env:DT_MESH_FILES = ($Built | ForEach-Object { $_.Path }) -join ";"
	$env:DT_MESH_DEST = $Dest
	$Out = & cmd /c "`"$Root\Tools\EditorPython.bat`" `"$PSScriptRoot\import_mesh.py`" 2>&1"
	$Rc = $LASTEXITCODE
	$env:DT_MESH_FILES = $null
	$env:DT_MESH_DEST = $null
	$Out | Select-String "import_mesh: /Game" | ForEach-Object { "    " + ($_.Line -replace "^.*import_mesh: ", "") }
	if ($Rc -ne 0) { $Out | Select-Object -Last 8 | ForEach-Object { "    $_" }; Fail "import, see Saved\Logs\EditorPython_import_mesh.log" }
	foreach ($File in $AssetFiles) { if (-not (Test-Path $File)) { Fail "the import left no $File" } }

	# --- 3. the cook, as a commandlet -------------------------------------------------------------------------------------
	$Cooked = ""
	if ($Cook) {
		$CookDir = Join-Path $Root "Saved\MeshCook"
		if (Test-Path $CookDir) { Remove-Item $CookDir -Recurse -Force }
		$Packages = ($Assets | ForEach-Object { "$Dest/$_" }) -join "+"
		$Out = & cmd /c "`"$PSScriptRoot\CookMesh.bat`" $Packages `"$CookDir`" 2>&1"
		if ($LASTEXITCODE -ne 0) { $Out | Select-Object -Last 8 | ForEach-Object { "    $_" }; Fail "cook, see Saved\Logs\CookMesh.log" }
		# The cook writes into the Zen store, not loose files, so its own records are read: the set of packages it cooked, its count, and no error.
		$CookLog = Get-Content (Join-Path $LogDir "CookMesh.log") -Encoding UTF8
		$Set = @(Get-ChildItem $CookDir -Recurse -Filter "ReferencedSet.txt" -ErrorAction SilentlyContinue | ForEach-Object { Get-Content $_.FullName })
		$Count = $CookLog | Select-String "LogCook: Display: Packages Cooked: (\d+)," | Select-Object -Last 1
		$Errors = @($CookLog | Select-String "LogCook.*: Error:|LogStaticMesh: Error:|LogInit: Display: Failure - ")
		foreach ($Asset in $Assets) {
			if ($Set -notcontains "$Dest/$Asset".ToLowerInvariant()) { Fail "the cook's ReferencedSet.txt does not name $Dest/$Asset" }
		}
		if (-not $Count -or [int]$Count.Matches[0].Groups[1].Value -ne $Assets.Count) { Fail "the cook did not report $($Assets.Count) package(s) cooked" }
		if ($Errors.Count) { $Errors | Select-Object -First 5 | ForEach-Object { "  ! $($_.Line)" }; Fail "the cook logged $($Errors.Count) error(s)" }
		"    cooked $($Assets.Count) package(s): $(($Assets | ForEach-Object { "$Dest/$_" }) -join ', ')"
		$Cooked = ", cooked for Windows"
	}
}
finally {
	if ($SelfTest) {
		# The test mesh leaves nothing behind: its export, its asset, and the folders it alone made.
		foreach ($File in $Made + $AssetFiles) { if (Test-Path $File) { Remove-Item $File -Force } }
		foreach ($Dir in @((Join-Path $Root "SourceArt\$Family"), (Join-Path $Root "SourceArt"), (Join-Path $Root "Content\DT\Meshes"))) {
			if ((Test-Path $Dir) -and -not (Get-ChildItem $Dir -Force)) { Remove-Item $Dir -Force }
		}
	}
}
$Tail = if ($SelfTest) { "; self-test: its files are removed" } else { "" }
Write-Host "MESH: PASS ($($Assets -join ', '): Blender $BlenderVersion to SourceArt\$Family to $Dest$Repro$Cooked$Tail)"
exit 0
