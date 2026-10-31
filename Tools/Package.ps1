# Package (02_Systems/package.md, T-PKG-01): headless Win64 BuildCookRun to the git-ignored Saved\Package\<Config>\Windows.
#   & Tools\Package.ps1 [-Configs Shipping,Development] [-SkipCook]
# Shipping is the validator build; Development, staged from the same cook, is the proof build for Tools\PadCheck.ps1 (R-PKG-02).
# Nothing here uploads or publishes. Opens no window: UAT runs the cook through UnrealEditor-Cmd.
# Prints "PACKAGE: PASS (...)" or "PACKAGE: FAIL (...)"; exit 0 / 1. Logs: Saved\Logs\Package_<Config>.log.
param(
	[ValidateSet("Shipping", "Development")]
	[string[]]$Configs = @("Shipping", "Development"),
	[switch]$SkipCook
)

trap { Write-Host "PACKAGE: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Uat = Join-Path $UERoot "Engine\Build\BatchFiles\RunUAT.bat"
$Project = Join-Path $Root "Dusktracer.uproject"
$Map = "/Game/DT/Maps/L_Arena_Test"
$Cultures = @("en", "zh-Hans")
$OutRoot = Join-Path $Root "Saved\Package"
$LogDir = Join-Path $Root "Saved\Logs"
New-Item -ItemType Directory -Force $OutRoot, $LogDir | Out-Null
$Utf8 = New-Object System.Text.UTF8Encoding($false)

function Stop-Package([string]$Why) { Write-Host "PACKAGE: FAIL ($Why)"; exit 1 }

# A package from a dirty tree can still be played, but the gate never judges it (R-PKG-08).
Push-Location $Root
$Commit = (& git rev-parse --short HEAD).Trim()
$Dirty = @(& git status --porcelain).Count -gt 0
Pop-Location
if ($Dirty) { Write-Host "package: tree is dirty, the gate will not judge this package" }

# Text first: the generated tables and .locres are git-ignored build products (R-TEC-08). The editor target has to exist before that.
Write-Host "package: editor target"
$Out = & cmd /c "`"$Root\Tools\Build.bat`" DusktracerEditor Development 2>&1"
if ($LASTEXITCODE -ne 0) { $Out | Select-Object -Last 10 | ForEach-Object { "    $_" }; Stop-Package "editor build" }
Write-Host "package: text"
$Out = & cmd /c "`"$Root\Tools\Localize.bat`" 2>&1"
if ($LASTEXITCODE -ne 0) { $Out | Select-Object -Last 10 | ForEach-Object { "    $_" }; Stop-Package "Localize.bat" }

$First = $true
foreach ($Config in $Configs) {
	$Dest = Join-Path $OutRoot $Config
	if (Test-Path $Dest) { Remove-Item $Dest -Recurse -Force }
	New-Item -ItemType Directory -Force $Dest | Out-Null
	$Log = Join-Path $LogDir "Package_$Config.log"
	$Args = @("BuildCookRun", "-project=`"$Project`"", "-noP4", "-platform=Win64", "-clientconfig=$Config", "-map=$Map",
		"-skipbuild", "-nocompileeditor", "-stage", "-pak", "-iostore", "-compressed", "-prereqs", "-nodebuginfo",
		"-archive", "-archivedirectory=`"$Dest`"", "-unattended", "-utf8output", "-nosplash",
		("-CookCultures=" + ($Cultures -join "+")))
	# The cook does not depend on the config: the second config stages the first one's cooked output.
	if ($First -and -not $SkipCook) { $Args += "-cook" } else { $Args += "-skipcook" }
	# Tools\Build.bat waits for the UBT mutex; BuildCookRun -build fails at once when another worktree is compiling.
	Write-Host "package: game target $Config"
	$Out = & cmd /c "`"$Root\Tools\Build.bat`" Dusktracer $Config 2>&1"
	if ($LASTEXITCODE -ne 0) { $Out | Select-Object -Last 10 | ForEach-Object { "    $_" }; Stop-Package "$Config build" }
	Write-Host "package: $Config (log $Log)"
	$Watch = [Diagnostics.Stopwatch]::StartNew()
	& cmd /c "`"$Uat`" $($Args -join ' ') > `"$Log`" 2>&1"
	$Rc = $LASTEXITCODE
	if ($Rc -ne 0) {
		Get-Content $Log -Encoding UTF8 | Select-String "Error:|ERROR:|BUILD FAILED" | Select-Object -First 12 | ForEach-Object { "    " + $_.Line }
		Stop-Package "$Config BuildCookRun rc $Rc"
	}
	$Package = Join-Path $Dest "Windows"
	if (-not (Test-Path (Join-Path $Package "Dusktracer.exe"))) { Stop-Package "$Config has no Dusktracer.exe under $Package" }
	Copy-Item (Join-Path $Root "Tools\PackageFiles\*") $Package -Force
	Write-Host ("package: {0} done in {1:n0} s" -f $Config, $Watch.Elapsed.TotalSeconds)
	$First = $false
}

$Size = 0L
foreach ($Config in $Configs) {
	$Size += (Get-ChildItem (Join-Path $OutRoot $Config) -Recurse -File | Measure-Object Length -Sum).Sum
}
$Stamp = @{ commit = $Commit; dirty = $Dirty; configs = $Configs; cultures = $Cultures; size_bytes = $Size; wall = (Get-Date).ToString("s") } | ConvertTo-Json
[IO.File]::WriteAllText((Join-Path $OutRoot "package_stamp.json"), $Stamp, $Utf8)

Write-Host ("PACKAGE: PASS ({0}{1}, {2}, {3:n0} MB under {4})" -f $Commit, $(if ($Dirty) { "+dirty" } else { "" }), ($Configs -join "+"), ($Size / 1MB), $OutRoot)
exit 0
