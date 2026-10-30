# Merge gate (11_Quality §1): runs every gate step on the current tree, one line per step.
#   & Tools\Gate.ps1 [-AllFiles] [-Base origin/main] [-Skip build,shipping,...]     (from PowerShell)
# Ends with "GATE: PASS (<n> not run)" or "GATE: FAIL at <step>". A step that cannot run yet prints NOT RUN and is
# never counted as passed. Exit 0 = pass, 1 = fail. Logs: Saved\Logs\Gate_<step>.log.
# Steps: secrets, docs, constants, catalog, loc, build (0 warnings), nonunity (T-TEC-09), shipping (T-TEC-12), tests, then the
# steps that do not exist yet: smoke, pad, scope; mac is deferred (D-007).
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

# --- C++ ---------------------------------------------------------------------------------------------------------
if (-not (Skipped "build")) {
	$R = Ubt "build" "DusktracerEditor" "Development"
	Report "build" ("rc {0}, {1} warning(s)" -f $R.Rc, $R.Warn)
	if ($R.Rc -ne 0 -or $R.Warn -gt 0) { Show $R.Out "error|warning"; Fail "build" }
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
		-unattended -nopause -nullrhi -LiveCoding=false "-ABSLOG=$Log" | Out-Null
	$All = @(Select-String -Path $Log -Pattern "Test Completed\. Result=\{")
	$Failed = @($All | Where-Object { $_.Line -notmatch "Result=\{Success\}" })
	$Crash = @(Select-String -Path $Log -Pattern "Fatal error|Unhandled Exception|=== Critical error ===")
	Report "tests" ("{0} run, {1} failed, floor {2}, {3} crash marker(s)" -f $All.Count, $Failed.Count, $Floor, $Crash.Count)
	if ($All.Count -eq 0 -or $All.Count -lt $Floor -or $Failed.Count -gt 0 -or $Crash.Count -gt 0) {
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
NotRun "pad" "no package yet"
NotRun "scope" "no brief in flight"
NotRun "mac" "deferred, D-007"

Write-Host "GATE: PASS ($Head; $NotRun not run)"
exit 0
