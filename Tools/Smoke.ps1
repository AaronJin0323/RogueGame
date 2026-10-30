# Smoke test (gate step `smoke`, 11_Quality s1): a rendered run on the stress arena, judged from its log and a screenshot.
#   & Tools\Smoke.ps1 [-Culture en|zh-Hans] [-Cmds "DT.Spawn.Keep 400,DT.Bullets 6000"] [-Seconds <s>] [-Name arena_empty]
#                     [-Map /Game/DT/Maps/L_Arena_Test] [-Director] [-Update]
# Launches the game (UnrealEditor -game, 1280x720 window) in the given culture, waits for "Run start", runs -Cmds as
# -ExecCmds, takes a screenshot -Seconds after run start (default C.SMOKE.SECONDS; ADTGameMode -DTShot hook), and the
# game quits itself once the file is written; a game still alive after the timeout is killed by its own PID only.
# The run director is off (-DTNoDirector, director.md) so the goldens stay deterministic; -Director turns it on (pair it
# with a -Name that has no golden, e.g. -Director -Seconds 60 -Name arena_director: the image check then fails by design).
# Verdict: no crash, no ensure, no Error/Fatal line from LogDT, "Run start" seen, the drawn Test.LocRoundTrip equals
# ST_UI.csv for the culture, screenshot present, and
# Tools/image_check.py PASS against Docs/Validation/goldens/<Name>_<Culture>.png (-Update re-baselines the golden).
# Prints "SMOKE: PASS (...)" or "SMOKE: FAIL (...)"; exit 0 / 1. Log: Saved\Logs\Smoke_<Name>_<Culture>.log.
param(
	[ValidateSet("en", "zh-Hans")]
	[string]$Culture = "en",
	[string]$Cmds = "",
	[double]$Seconds = -1,
	[string]$Name = "arena_empty",
	[string]$Map = "/Game/DT/Maps/L_Arena_Test",
	[int]$LoadTimeout = 900,  # the first launch after render changes compiles shaders
	[switch]$Director,
	[switch]$Update
)

trap { Write-Host "SMOKE: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"
$ShotDir = Join-Path $Root "Saved\Smoke"
New-Item -ItemType Directory -Force $LogDir, $ShotDir | Out-Null
$Tag = "${Name}_${Culture}"
$Log = Join-Path $LogDir "Smoke_$Tag.log"
$Shot = Join-Path $ShotDir "$Tag.png"
$Golden = Join-Path $Root "Docs\Validation\goldens\$Tag.png"

function Get-Constant([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*([-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return [double]$Line.Matches[0].Groups[1].Value
}
if ($Seconds -lt 0) { $Seconds = Get-Constant "SMOKE.SECONDS" }

# --- text: generated string table + compiled .locres must be current (R-TEC-08) --------------------------------------
$LocOut = & python -I (Join-Path $Root "Tools\loc_build.py") --build 2>&1
if ($LASTEXITCODE -ne 0) { $LocOut | ForEach-Object { "    $_" }; Write-Host "SMOKE: FAIL (loc_build)"; exit 1 }
$Po = Join-Path $Root "Content\Localization\Game\zh-Hans\Game.po"
$Locres = Join-Path $Root "Content\Localization\Game\zh-Hans\Game.locres"
if (-not (Test-Path $Locres) -or (Get-Item $Locres).LastWriteTime -lt (Get-Item $Po).LastWriteTime) {
	Write-Host "smoke: compiling .locres (Tools\Localize.bat)"
	$LocOut = & cmd /c "`"$Root\Tools\Localize.bat`" 2>&1"
	if ($LASTEXITCODE -ne 0) { $LocOut | Select-Object -Last 10 | ForEach-Object { "    $_" }; Write-Host "SMOKE: FAIL (Localize.bat)"; exit 1 }
}

# --- run ----------------------------------------------------------------------------------------------------------
if (Test-Path $Log) { Remove-Item $Log -Force }
if (Test-Path $Shot) { Remove-Item $Shot -Force }
$ArgList = @("`"$Project`"", $Map, "-game", "-windowed", "-ResX=1280", "-ResY=720", "-LiveCoding=false", "-nosplash",
	"-unattended", "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", ("-DTShotAfter={0}" -f $Seconds))
if (-not $Director) { $ArgList += "-DTNoDirector" }
if ($Cmds) { $ArgList += "-ExecCmds=`"$Cmds`"" }
$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru
Write-Host ("smoke: {0} culture={1} cmds='{2}' shot after {3} s (PID {4})" -f $Map, $Culture, $Cmds, $Seconds, $Proc.Id)

function Read-Log { if (Test-Path $Log) { Get-Content $Log -Raw -Encoding UTF8 -ErrorAction SilentlyContinue } else { "" } }

$Killed = $false
try {
	$Deadline = (Get-Date).AddSeconds($LoadTimeout)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline -and (Read-Log) -notmatch "LogDT: Run start") { Start-Sleep -Seconds 1 }
	# After the run starts the game shoots and quits on its own; allow the shot delay plus a margin for slow frames.
	$Deadline = (Get-Date).AddSeconds($Seconds + 120)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1 }
}
finally {
	if (-not $Proc.HasExited) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		$Killed = $true
		Start-Sleep -Seconds 1
	}
}

# --- verdict ------------------------------------------------------------------------------------------------------
$Text = Read-Log
$Lines = $Text -split "`r?`n"
$Started = $Text -match "LogDT: Run start"
$Crash = @($Lines | Where-Object { $_ -match "Fatal error|=== Critical error ===|Unhandled Exception|appError called" })
$Ensure = @($Lines | Where-Object { $_ -match "Ensure condition failed|LogOutputDevice: Error: Ensure" })
$DTErr = @($Lines | Where-Object { $_ -match "LogDT: (Error|Fatal)" })
$OtherErr = @($Lines | Where-Object { $_ -match "Error:" -and $_ -notmatch "LogDT:" })

$Lines | Where-Object { $_ -match "LogLoad: LoadMap|LogDT:" } | Select-Object -First 20 | ForEach-Object { "    $_" }
if ($OtherErr.Count) { Write-Host "    ($($OtherErr.Count) engine Error line(s), not judged; first:)"; $OtherErr | Select-Object -First 5 | ForEach-Object { "      $_" } }
$Crash + $Ensure + $DTErr | Select-Object -First 15 | ForEach-Object { "  ! $_" }

# Localisation round trip (12_Production §2 S0 exit): the HUD logs Test.LocRoundTrip once as drawn; it must equal the
# source CSV's column for this culture. Pixel goldens cannot tell two cultures apart, the log line can.
$Expected = (Import-Csv -Path (Join-Path $Root "Content\DT\Text\ST_UI.csv") -Encoding UTF8 | Where-Object { $_.Key -eq "Test.LocRoundTrip" }).$Culture
$Drawn = ($Lines | Select-String -Pattern "LogDT: LocRoundTrip culture=(\S+) text=(.*)$" | Select-Object -First 1)
$LocOk = $false
$Loc = "loc: no LocRoundTrip line"
if ($Drawn) {
	$DrawnCulture = $Drawn.Matches[0].Groups[1].Value
	$DrawnText = $Drawn.Matches[0].Groups[2].Value.Trim()
	$LocOk = ($DrawnCulture -eq $Culture) -and ($DrawnText -ceq $Expected)
	$Loc = if ($LocOk) { "loc ok ($Culture)" } else { "loc MISMATCH (culture '$DrawnCulture', drew '$DrawnText', expected '$Expected')" }
}

$Image = "no screenshot"
$ImageOk = $false
if (Test-Path $Shot) {
	$CheckArgs = @((Join-Path $Root "Tools\image_check.py"), $Shot, "--golden", $Golden)
	if ($Update) { $CheckArgs += "--update" }
	$ImgOut = & python -I @CheckArgs 2>&1
	$ImageOk = ($LASTEXITCODE -eq 0)
	$Image = (($ImgOut | Select-String "^IMAGE:" | Select-Object -Last 1).Line) -replace "^IMAGE:\s*", "image "
}
Write-Host "    log: $Log"
Write-Host "    shot: $Shot"

$Summary = "{0}, run start={1}, crash={2}, ensure={3}, LogDT errors={4}, killed={5}, {6}, {7}" -f $Tag, $Started, $Crash.Count, $Ensure.Count, $DTErr.Count, $Killed, $Loc, $Image
if ($Started -and $Crash.Count -eq 0 -and $Ensure.Count -eq 0 -and $DTErr.Count -eq 0 -and $LocOk -and $ImageOk) {
	Write-Host "SMOKE: PASS ($Summary)"
	exit 0
}
Write-Host "SMOKE: FAIL ($Summary)"
exit 1
