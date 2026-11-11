# Merge gate (11_Quality §1): runs every gate step on the current tree, one line per step.
#   & Tools\Gate.ps1 [-AllFiles] [-Base origin/main] [-Skip build,shipping,...]     (from PowerShell)
# Ends with "GATE: PASS (<commit>; <n> not run; mask <mask>)" or "GATE: FAIL at <step> (<commit>; mask <mask>)": the processor mask of D-052 (Tools\Machine.ps1) it ran under. A step that cannot run yet prints NOT RUN and is
# never counted as passed; a Python step whose script is missing fails. Exit 0 = pass, 1 = fail. Logs: Saved\Logs\Gate_<step>.log.
# Steps: cpu (Tools/cpu_canary.py: each processor the mask leaves in runs the fault loop of Validation/S4_py_fault.md), secrets, docs, constants, catalog, matrix, loc, tools (Tools/tools_test.py), build (0 warnings), locbuild (regenerates git-ignored text), nonunity (T-TEC-09),
# shipping (T-TEC-12), devgame (the Development game target, 0 warnings), tests (Tools/RunTests.ps1, count = C.GATE.TEST_FLOOR), smoke, restart (T-SET-28), seed (Tools/SeedCheck.ps1, T-TEC-16), contrast (T-PRE-05, debts in Docs/Validation/contrast_debts.txt; then the HUD's dormant line on its own frame), windows (Tools/WindowCheck.ps1),
# pad (Tools/PadCheck.ps1, T-PKG-08: NOT RUN without a current package or a real pad), then scope, which does not exist yet; mac is deferred (D-007).
param(
	[switch]$AllFiles,
	[string]$Base = "origin/main",
	[string[]]$Skip = @()
)

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

# Any script error is a gate failure, never a silent exit 0.
trap { Write-Host "GATE: FAIL (script error: $_; mask $DTMask)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Head = (& git -C $Root rev-parse --short HEAD) 2>$null
$Skip = @($Skip | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$NotRun = 0

function Report([string]$Step, [string]$Text) { Write-Host ("{0,-11} {1}" -f "${Step}:", $Text) }
function Fail([string]$Step) { Write-Host "GATE: FAIL at $Step ($Head; mask $DTMask)"; exit 1 }
function NotRun([string]$Step, [string]$Why) { Report $Step "NOT RUN ($Why)"; $script:NotRun++ }
function Show($Lines, [string]$Pattern) { $Lines | Select-String -Pattern $Pattern | Select-Object -First 12 | ForEach-Object { "    " + $_.Line } }
function Skipped([string]$Step) { if ($Skip -contains $Step) { NotRun $Step "skipped by -Skip"; return $true }; return $false }
# D-055 item 7, R-TEC-18: every step below that is guarded by Start-HeavyStep starts a compiler, the engine, a commandlet or a package; the text steps
# (Invoke-PyStep) start none. The run takes its one heavy-job slot right before the first heavy step that is not skipped, so a text-only run never
# waits for a slot or for the alone hold; the steps' own scripts inherit the slot and take no second one.
function Start-HeavyStep([string]$Step) {
	if (Skipped $Step) { return $false }
	if (-not $script:DTHeavy) { $script:DTHeavy = Enter-DTHeavy -What "Gate" }
	return $true
}

function Invoke-PyStep([string]$Step, [string]$Script, [string]$Verdict, [string[]]$ScriptArgs = @()) {
	if (Skipped $Step) { return }
	$Path = Join-Path $Root $Script
	if (-not (Test-Path $Path)) { Report $Step "FAIL ($Script missing: every Python step is live)"; Fail $Step }
	$Out = & python -I $Path @ScriptArgs 2>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_$Step.log") -Encoding utf8
	Report $Step (($Out | Select-String "^${Verdict}:" | Select-Object -Last 1).Line)
	if ($Rc -ne 0) { Show $Out "^(error|secret|LOC|CONSTANTS|CATALOG|MATRIX|  ST_|  Source/|  ! )"; Fail $Step }
}

function Ubt([string]$Step, [string]$Target, [string]$Config, [string]$Extra = "") {
	$Log = Join-Path $LogDir "Gate_$Step.log"
	$env:DT_UBT_ARGS = $Extra
	$Out = & cmd /c "`"$Root\Tools\Build.bat`" $Target $Config 2>&1"
	$Rc = $LASTEXITCODE
	$env:DT_UBT_ARGS = $null
	$Out | Set-Content $Log -Encoding utf8
	return @{ Rc = $Rc; Out = $Out; Warn = @($Out | Select-String -Pattern "warning C\d+|: warning ").Count }
}

# --- the machine (D-052): a processor that faults in plain code would make every later step a guess -----------------------------
Invoke-PyStep "cpu" "Tools\cpu_canary.py" "CPU" @("--mask", $DTMask)

# --- text and data -----------------------------------------------------------------------------------------------
Invoke-PyStep "secrets" "Tools\secret_scan.py" "SECRETS"
Invoke-PyStep "docs" "Tools\docs_lint.py" "DOCS"
Invoke-PyStep "constants" "Tools\gen_constants.py" "CONSTANTS" @("--check")
Invoke-PyStep "catalog" "Tools\gen_catalog.py" "CATALOG" @("--check")
Invoke-PyStep "matrix" "Tools\gen_matrix.py" "MATRIX" @("--check")
Invoke-PyStep "loc" "Tools\loc_build.py" "LOC" @("--check")
Invoke-PyStep "tools" "Tools\tools_test.py" "TOOLS"

# --- C++ ---------------------------------------------------------------------------------------------------------
if (Start-HeavyStep "build") {
	$R = Ubt "build" "DusktracerEditor" "Development"
	Report "build" ("rc {0}, {1} warning(s)" -f $R.Rc, $R.Warn)
	if ($R.Rc -ne 0 -or $R.Warn -gt 0) { Show $R.Out "error|warning"; Fail "build" }
}

if (Start-HeavyStep "locbuild") {
	# Generated text (Content/DT/Text/Generated, .locres) is git-ignored: rebuild it here so tests and smoke never read stale
	# or missing tables in a fresh checkout or after a merge (R-TEC-08).
	$Out = & cmd /c "`"$Root\Tools\Localize.bat`" 2>&1"
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_locbuild.log") -Encoding utf8
	Report "locbuild" (($Out | Select-String "^LOCALIZE:" | Select-Object -Last 1).Line)
	if ($Rc -ne 0) { Show $Out "^(LOCALIZE|Error)"; Fail "locbuild" }
}

if (Start-HeavyStep "nonunity") {
	# Every .cpp must compile alone (R-TEC-09): UBT -SingleFile per changed file (plus the .cpp of each changed .h).
	if ($AllFiles) {
		$Files = @(Get-ChildItem (Join-Path $Root "Source") -Recurse -Filter *.cpp | ForEach-Object { $_.FullName })
	}
	else {
		Push-Location $Root
		# A base that does not resolve would read as no changed file and pass with nothing compiled.
		$null = & git rev-parse --verify --quiet "$Base^{commit}" 2>$null
		if ($LASTEXITCODE -ne 0) { Pop-Location; Report "nonunity" "FAIL (base '$Base' does not resolve)"; Fail "nonunity" }
		$Names = @(git diff --name-only "$Base...HEAD" 2>$null) + @(git diff --name-only HEAD 2>$null) + @(git ls-files --others --exclude-standard 2>$null)
		Pop-Location
		$Files = @($Names | Where-Object { $_ -match '^Source/.*\.(cpp|h)$' } | ForEach-Object { $_ -replace '\.h$', '.cpp' } |
			Sort-Object -Unique | ForEach-Object { Join-Path $Root $_ } | Where-Object { Test-Path $_ })
	}
	$Bad = @()
	foreach ($F in $Files) {
		$R = Ubt "nonunity" "DusktracerEditor" "Development" "-NoUBTMakefiles -SingleFile=`"$F`""
		# Zero "Compile [" lines means UBT never compiled the file (bad arguments, wrong target): that is a failure too.
		$Compiled = @($R.Out | Select-String "Compile \[").Count
		if ($R.Rc -ne 0 -or $R.Warn -gt 0 -or $Compiled -eq 0 -or @($R.Out | Select-String "error C\d+|fatal error").Count -gt 0) { $Bad += $F }
	}
	Report "nonunity" ("{0} file(s), {1} failed" -f $Files.Count, $Bad.Count)
	if ($Bad.Count -gt 0) { $Bad | ForEach-Object { "    " + $_.Substring($Root.Length + 1) }; Fail "nonunity" }
}

if (Start-HeavyStep "shipping") {
	$R = Ubt "shipping" "Dusktracer" "Shipping"
	Report "shipping" ("rc {0}, {1} warning(s)" -f $R.Rc, $R.Warn)
	if ($R.Rc -ne 0 -or $R.Warn -gt 0) { Show $R.Out "error|warning"; Fail "shipping" }
}

if (Start-HeavyStep "devgame") {
	# The Development game target: an editor-only API inside WITH_DEV_AUTOMATION_TESTS compiles in the editor target and is compiled out of Shipping,
	# so only this target sees it (the owner's scheduler brief of 2026-11-11).
	$R = Ubt "devgame" "Dusktracer" "Development"
	Report "devgame" ("rc {0}, {1} warning(s)" -f $R.Rc, $R.Warn)
	if ($R.Rc -ne 0 -or $R.Warn -gt 0) { Show $R.Out "error|warning"; Fail "devgame" }
}
if (Start-HeavyStep "tests") {
	$Floor = [int]((Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern "GATE\.TEST_FLOOR:\s*\{\s*value:\s*(\d+)").Matches[0].Groups[1].Value)
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\RunTests.ps1") -Log (Join-Path $LogDir "Gate_tests.log") -Floor $Floor 2>&1
	$Rc = $LASTEXITCODE
	Report "tests" ((($Out | Select-String "^TESTS:" | Select-Object -Last 1).Line) -replace "^TESTS:\s*\w+\s*\((.*)\)$", '$1')
	if ($Rc -ne 0) { Show $Out "^  ! "; Fail "tests" }
}

# --- not built yet -------------------------------------------------------------------------------------------------
if (Start-HeavyStep "smoke") {
	foreach ($Culture in @("en", "zh-Hans")) {
		$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\Smoke.ps1") -Culture $Culture 2>&1
		$Rc = $LASTEXITCODE
		$Out | Set-Content (Join-Path $LogDir "Gate_smoke_$Culture.log") -Encoding utf8
		Report "smoke" (($Out | Select-String "^SMOKE:" | Select-Object -Last 1).Line)
		if ($Rc -ne 0) { Show $Out "^  ! |FAIL"; Fail "smoke" }
	}
	# T-PRE-07: the arena's post-process volume, then a burst of fire against the calm arena_empty shot above: no exposure change.
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\Smoke.ps1") -Name burst -Burst -Cmds "DT.Bullets 6000" 2>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_smoke_burst.log") -Encoding utf8
	Report "smoke" (($Out | Select-String "^SMOKE:" | Select-Object -Last 1).Line)
	if ($Rc -ne 0) { Show $Out "^  ! |FAIL"; Fail "smoke" }
}
if (Start-HeavyStep "restart") {
	# T-SET-28: a real map reload against a fresh launch by input hash, and real keys through a pause and a reload; headless.
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\RestartCheck.ps1") 2>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_restart.log") -Encoding utf8
	Report "restart" (($Out | Select-String "^RESTART:" | Select-Object -Last 1).Line)
	if ($Rc -ne 0) { Show $Out "INPUTCHECK|RESTART: FAIL"; Fail "restart" }
}
if (Start-HeavyStep "seed") {
	# 07_Technical R-TEC-16: every launcher started as written, off-screen, reads back a fresh seed; tools and explicit seeds stay exact.
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\SeedCheck.ps1") 2>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_seed.log") -Encoding utf8
	Report "seed" ((($Out | Select-String "^SEEDCHECK:" | Select-Object -Last 1).Line) -replace "^SEEDCHECK:\s*", "")
	if ($Rc -ne 0) { Show $Out "^  ! "; Fail "seed" }
}
if (Start-HeavyStep "contrast") {
	# T-PRE-05: a staged late frame (one weather exists). Needs the arena_empty shot the smoke step left, for the hero.
	$Scene = "DT.God 1,DT.Dir.Time 700,DT.Gems.Drop 30 4 700,DT.Spawn.Role Swarm 6 elite,DT.Spawn.Role Turret 18,DT.Spawn.Role Rusher 8,DT.Bullets 800"
	$Out = & (Join-Path $Root "Tools\Smoke.ps1") -Name contrast -Director -Contrast -Seconds 12 -Cmds $Scene *>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_contrast.log") -Encoding utf8
	Report "contrast" (($Out | Select-String "CONTRAST:" | Select-Object -Last 1).Line.Trim() -replace "^.*CONTRAST:", "CONTRAST:" -replace "\)\)$", ")")
	if ($Rc -ne 0) { Show $Out "^  ! |FAIL|not found"; Fail "contrast" }
	# R-MOD-08: the HUD's dormant line on its own frame. Staged in the scene above it would take the Rifle's rounds out of that frame and read as a tracer.
	$Scene = "DT.Mod Weapon.PulseRifle Mod.ScatterShell,DT.Bless Blessing.Forge.Hammer.Key"
	$Out = & (Join-Path $Root "Tools\Smoke.ps1") -Name dormant_line -DormantLine -Seconds 4 -Cmds $Scene *>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_contrast_line.log") -Encoding utf8
	Report "contrast" (($Out | Select-String "CONTRAST:" | Select-Object -Last 1).Line.Trim() -replace "^.*CONTRAST:", "CONTRAST:" -replace "\)\)$", ")")
	if ($Rc -ne 0) { Show $Out "^  ! |FAIL|not found"; Fail "contrast" }
}
if (Start-HeavyStep "windows") {
	# The no-window rule (AGENTS.md): a bot, a smoke and a test run show nothing on this desktop; Tools\WindowCheck.ps1 holds its controls and its detection limit.
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\WindowCheck.ps1") 2>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_windows.log") -Encoding utf8
	Report "windows" ((($Out | Select-String "^WINDOWS:" | Select-Object -Last 1).Line) -replace "^WINDOWS:\s*", "")
	if ($Rc -ne 0) { Show $Out "^  ! "; Fail "windows" }
}
if (Start-HeavyStep "pad") {
	# package.md R-PKG-08/09: judges a clean, current package off-screen; a real pad needs a human, so this reads NOT RUN until a run shows one.
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\PadCheck.ps1") 2>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_pad.log") -Encoding utf8
	$Verdict = (($Out | Select-String "^PADCHECK:" | Select-Object -Last 1).Line) -replace "^PADCHECK:\s*", ""
	if ($Rc -eq 0) { Report "pad" $Verdict }
	elseif ($Rc -eq 2) { NotRun "pad" ($Verdict -replace "^NOT RUN \((.*)\)$", '$1') }
	else { Report "pad" $Verdict; Show $Out "^  ! "; Fail "pad" }
}
NotRun "scope" "no brief in flight"
NotRun "mac" "deferred, D-007"

Write-Host "GATE: PASS ($Head; $NotRun not run; mask $DTMask)"
exit 0
