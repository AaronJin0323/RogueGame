# Merge gate (11_Quality §1): runs every gate step on the current tree, one line per step.
#   & Tools\Gate.ps1 [-AllFiles] [-Base origin/main] [-Skip build,shipping,...]     (from PowerShell)
# Ends with "GATE: PASS (<n> not run)" or "GATE: FAIL at <step>". A step that cannot run yet prints NOT RUN and is
# never counted as passed. Exit 0 = pass, 1 = fail. Logs: Saved\Logs\Gate_<step>.log.
# Steps: secrets, docs, constants, catalog, loc, tools (Tools/tools_test.py), build (0 warnings), locbuild (regenerates git-ignored text), nonunity (T-TEC-09),
# shipping (T-TEC-12), tests, smoke, contrast (T-PRE-05, debts in Docs/Validation/contrast_debts.txt), pad (Tools/PadCheck.ps1, T-PKG-08: NOT RUN without a current package or a real pad), then scope, which does not exist yet; mac is deferred (D-007).
param(
	[switch]$AllFiles,
	[string]$Base = "origin/main",
	[string[]]$Skip = @()
)

# Any script error is a gate failure, never a silent exit 0.
trap { Write-Host "GATE: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Head = (& git -C $Root rev-parse --short HEAD) 2>$null
$Skip = @($Skip | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$NotRun = 0

function Report([string]$Step, [string]$Text) { Write-Host ("{0,-11} {1}" -f "${Step}:", $Text) }
function Fail([string]$Step) { Write-Host "GATE: FAIL at $Step ($Head)"; exit 1 }
function NotRun([string]$Step, [string]$Why) { Report $Step "NOT RUN ($Why)"; $script:NotRun++ }
function Show($Lines, [string]$Pattern) { $Lines | Select-String -Pattern $Pattern | Select-Object -First 12 | ForEach-Object { "    " + $_.Line } }
function Skipped([string]$Step) { if ($Skip -contains $Step) { NotRun $Step "skipped by -Skip"; return $true }; return $false }

function Invoke-PyStep([string]$Step, [string]$Script, [string]$Verdict, [string[]]$ScriptArgs = @()) {
	if (Skipped $Step) { return }
	$Path = Join-Path $Root $Script
	if (-not (Test-Path $Path)) { NotRun $Step "$Script missing"; return }
	$Out = & python -I $Path @ScriptArgs 2>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_$Step.log") -Encoding utf8
	Report $Step (($Out | Select-String "^${Verdict}:" | Select-Object -Last 1).Line)
	if ($Rc -ne 0) { Show $Out "^(error|secret|LOC|CONSTANTS|CATALOG)"; Fail $Step }
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

# --- text and data -----------------------------------------------------------------------------------------------
Invoke-PyStep "secrets" "Tools\secret_scan.py" "SECRETS"
Invoke-PyStep "docs" "Tools\docs_lint.py" "DOCS"
Invoke-PyStep "constants" "Tools\gen_constants.py" "CONSTANTS" @("--check")
Invoke-PyStep "catalog" "Tools\gen_catalog.py" "CATALOG" @("--check")
Invoke-PyStep "loc" "Tools\loc_build.py" "LOC" @("--check")
Invoke-PyStep "tools" "Tools\tools_test.py" "TOOLS"

# --- C++ ---------------------------------------------------------------------------------------------------------
if (-not (Skipped "build")) {
	$R = Ubt "build" "DusktracerEditor" "Development"
	Report "build" ("rc {0}, {1} warning(s)" -f $R.Rc, $R.Warn)
	if ($R.Rc -ne 0 -or $R.Warn -gt 0) { Show $R.Out "error|warning"; Fail "build" }
}

if (-not (Skipped "locbuild")) {
	# Generated text (Content/DT/Text/Generated, .locres) is git-ignored: rebuild it here so tests and smoke never read stale
	# or missing tables in a fresh checkout or after a merge (R-TEC-08).
	$Out = & cmd /c "`"$Root\Tools\Localize.bat`" 2>&1"
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_locbuild.log") -Encoding utf8
	Report "locbuild" (($Out | Select-String "^LOCALIZE:" | Select-Object -Last 1).Line)
	if ($Rc -ne 0) { Show $Out "^(LOCALIZE|Error)"; Fail "locbuild" }
}

if (-not (Skipped "nonunity")) {
	# Every .cpp must compile alone (R-TEC-09): UBT -SingleFile per changed file (plus the .cpp of each changed .h).
	if ($AllFiles) {
		$Files = @(Get-ChildItem (Join-Path $Root "Source") -Recurse -Filter *.cpp | ForEach-Object { $_.FullName })
	}
	else {
		Push-Location $Root
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

if (-not (Skipped "shipping")) {
	$R = Ubt "shipping" "Dusktracer" "Shipping"
	Report "shipping" ("rc {0}, {1} warning(s)" -f $R.Rc, $R.Warn)
	if ($R.Rc -ne 0 -or $R.Warn -gt 0) { Show $R.Out "error|warning"; Fail "shipping" }
}

if (-not (Skipped "tests")) {
	$Floor = [int]((Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern "GATE\.TEST_FLOOR:\s*\{\s*value:\s*(\d+)").Matches[0].Groups[1].Value)
	$Log = Join-Path $LogDir "Gate_tests.log"
	if (Test-Path $Log) { Remove-Item $Log }
	& (Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe") $Project -ExecCmds="Automation RunTests Dusktracer;Quit" `
		-unattended -nopause -nullrhi -nosplash -LiveCoding=false "-ABSLOG=$Log" | Out-Null
	$All = @(Select-String -Path $Log -Pattern "Test Completed\. Result=\{")
	$Failed = @($All | Where-Object { $_.Line -notmatch "Result=\{Success\}" })
	$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error:? ===")
	# A crash can stop the run early, so every test found must have completed.
	$FoundLine = Select-String -Path $Log -Pattern "Found (\d+) automation tests" | Select-Object -First 1
	$Found = if ($FoundLine) { [int]$FoundLine.Matches[0].Groups[1].Value } else { 0 }
	Report "tests" ("{0} run of {1} found, {2} failed, floor {3}, {4} crash marker(s)" -f $All.Count, $Found, $Failed.Count, $Floor, $Crash.Count)
	if ($All.Count -eq 0 -or $All.Count -ne $Found -or $All.Count -lt $Floor -or $Failed.Count -gt 0 -or $Crash.Count -gt 0) {
		$Failed + $Crash | Select-Object -First 12 | ForEach-Object { "    " + $_.Line }; Fail "tests"
	}
}

# --- not built yet -------------------------------------------------------------------------------------------------
if (-not (Skipped "smoke")) {
	foreach ($Culture in @("en", "zh-Hans")) {
		$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "Tools\Smoke.ps1") -Culture $Culture 2>&1
		$Rc = $LASTEXITCODE
		$Out | Set-Content (Join-Path $LogDir "Gate_smoke_$Culture.log") -Encoding utf8
		Report "smoke" (($Out | Select-String "^SMOKE:" | Select-Object -Last 1).Line)
		if ($Rc -ne 0) { Show $Out "^  ! |FAIL"; Fail "smoke" }
	}
}
if (-not (Skipped "contrast")) {
	# T-PRE-05: a staged late frame (one weather exists). Needs the arena_empty shot the smoke step left, for the hero.
	$Scene = "DT.God 1,DT.Dir.Time 700,DT.Gems.Drop 30 4 700,DT.Spawn.Role Swarm 6 elite,DT.Spawn.Role Turret 18,DT.Spawn.Role Rusher 8,DT.Bullets 800"
	$Out = & (Join-Path $Root "Tools\Smoke.ps1") -Name contrast -Director -Contrast -Seconds 12 -Cmds $Scene *>&1
	$Rc = $LASTEXITCODE
	$Out | Set-Content (Join-Path $LogDir "Gate_contrast.log") -Encoding utf8
	Report "contrast" (($Out | Select-String "CONTRAST:" | Select-Object -Last 1).Line.Trim() -replace "^.*CONTRAST:", "CONTRAST:" -replace "\)\)$", ")")
	if ($Rc -ne 0) { Show $Out "^  ! |FAIL|not found"; Fail "contrast" }
}
if (-not (Skipped "pad")) {
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

Write-Host "GATE: PASS ($Head; $NotRun not run)"
exit 0
