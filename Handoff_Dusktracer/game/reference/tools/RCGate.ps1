# RC gate (TDD s17 2026-10-20): run on every main merge before reporting it. Stops at the first failing step.
#   powershell -ExecutionPolicy Bypass -File Tools\RCGate.ps1 [-AllFiles] [-NoTests] [-NoSmoke]
# Steps: Tools\Build.bat (editor, 0 warnings) -> Tools\BuildNonUnity.bat (files changed vs main, or -AllFiles) ->
# Tools\check_shipping_commands.py (RH.* commands compiled out) -> Tools\BuildShipping.bat (game target, Shipping) -> automation tests "RogueHero" (headless) -> Tools\Smoke.ps1.
# Prints one line per step and a final "RC GATE: PASS" / "RC GATE: FAIL at <step>". Exit 0 = pass.
param(
	[switch]$AllFiles,
	[switch]$NoTests,
	[switch]$NoSmoke
)

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Head = (& git -C $Root rev-parse --short HEAD) 2>$null

function Fail([string]$Step) { Write-Host "RC GATE: FAIL at $Step ($Head)"; exit 1 }
function Lines($Out, [string]$Pattern) { $Out | Select-String -Pattern $Pattern | Select-Object -First 10 | ForEach-Object { "    " + $_.Line } }

$Out = & cmd /c "`"$Root\Tools\Build.bat`" 2>&1"
$Warn = @($Out | Select-String -Pattern "warning C\d+").Count
Write-Host ("build (editor):     rc {0}, {1} warning(s)" -f $LASTEXITCODE, $Warn)
if ($LASTEXITCODE -ne 0 -or $Warn -gt 0) { Lines $Out "error C|error LNK|warning C"; Fail "build" }

$GateArgs = if ($AllFiles) { "-All" } else { "" }
$Out = & cmd /c "`"$Root\Tools\BuildNonUnity.bat`" $GateArgs 2>&1"
Write-Host ("include gate:       {0}" -f (($Out | Select-String "NON-UNITY:" | Select-Object -Last 1).Line))
if ($LASTEXITCODE -ne 0) { Lines $Out "FAIL|error C"; Fail "include gate" }

$Out = & python -I "$Root\Tools\check_shipping_commands.py" $Root 2>&1
Write-Host ("shipping commands:  {0}" -f (($Out | Select-String "SHIPPING-COMMANDS:" | Select-Object -Last 1).Line))
if ($LASTEXITCODE -ne 0) { Lines $Out "unguarded"; Fail "shipping commands" }

$Out = & cmd /c "`"$Root\Tools\BuildShipping.bat`" 2>&1"
Write-Host ("shipping compile:   {0}" -f (($Out | Select-String "SHIPPING:" | Select-Object -Last 1).Line))
if ($LASTEXITCODE -ne 0) { Lines $Out "error C|error LNK"; Fail "shipping compile" }

if (-not $NoTests) {
	$Log = Join-Path $Root "Saved\Logs\RCGate_Tests.log"
	if (Test-Path $Log) { Remove-Item $Log }
	& (Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe") "$Root\RogueHero.uproject" -ExecCmds="Automation RunTests RogueHero;Quit" `
		-unattended -nopause -nullrhi -LiveCoding=false "-ABSLOG=$Log" | Out-Null
	$All = @(Select-String -Path $Log -Pattern "Test Completed\. Result=\{")
	$Bad = @($All | Where-Object { $_.Line -notmatch "Result=\{Success\}" })
	Write-Host ("tests:              {0} run, {1} failed" -f $All.Count, $Bad.Count)
	if ($All.Count -eq 0 -or $Bad.Count -gt 0) { $Bad | Select-Object -First 10 | ForEach-Object { "    " + $_.Line }; Fail "tests" }
}

if (-not $NoSmoke) {
	$Out = & powershell -ExecutionPolicy Bypass -File "$Root\Tools\Smoke.ps1"
	Write-Host ("smoke:              {0}" -f (($Out | Select-String "SMOKE:" | Select-Object -Last 1).Line))
	if ($LASTEXITCODE -ne 0) { Fail "smoke" }
}

Write-Host "RC GATE: PASS ($Head)"
exit 0
