# Package check (gate step `pad`, 02_Systems/package.md R-PKG-07..09): what a machine can prove about the package, off-screen.
#   & Tools\PadCheck.ps1 [-Cultures en,zh-Hans] [-LoadTimeout 300]
# Shipping (T-PKG-05, cultures T-PKG-03): -RenderOffScreen launch, the bot plays past the first CSV interval and quits the game; run_start and run_end are right.
# Export (T-PKG-06): the zip holds that run and power_split.py reads it. Development (T-PKG-02): drawn text, log, screenshot against the arena
# golden, and a game that exits mid-run closes its file with run_end quit. T-PKG-07: no visible window of the game's process at any time.
# -RenderOffScreen creates no window, so a close message has nothing to reach: Alt+F4 itself stays a human check. The check's run files are deleted.
# A real pad needs a human: the step passes only when a run of this build has input_device=pad (R-PKG-09); a machine never fakes that (T-PKG-09). Launchers: T-PKG-04.
# Prints one verdict line, "PADCHECK: PASS|FAIL|NOT RUN (...)"; exit 0 pass, 1 fail, 2 not run. Log: Saved\Logs\PadCheck.log.
param(
	[ValidateSet("en", "zh-Hans")]
	[string[]]$Cultures = @("en", "zh-Hans"),
	[int]$LoadTimeout = 300
)

trap { Write-Host "PADCHECK: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$PackageRoot = Join-Path $Root "Saved\Package"
$StampFile = Join-Path $PackageRoot "package_stamp.json"
$LogDir = Join-Path $Root "Saved\Logs"
$WorkDir = Join-Path $Root "Saved\PadCheck"
$Playtests = Join-Path $env:LOCALAPPDATA "Dusktracer\Saved\Playtests"
$DevPlaytests = Join-Path $PackageRoot "Development\Windows\Dusktracer\Saved\Playtests"
$Map = "/Game/DT/Maps/L_Arena_Test"
$MarkerSeed = 770077
New-Item -ItemType Directory -Force $LogDir, $WorkDir | Out-Null

function Stop-Check([string]$Verdict, [int]$Code) { Write-Host "PADCHECK: $Verdict"; exit $Code }

function Get-Constant([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*([-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return [double]$Line.Matches[0].Groups[1].Value
}

# --- which package (R-PKG-08) -----------------------------------------------------------------------------------------
if (-not (Test-Path $StampFile)) { Stop-Check "NOT RUN (no package; Tools\Package.ps1)" 2 }
$Stamp = Get-Content $StampFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Stamp.dirty) { Stop-Check "NOT RUN (package $($Stamp.commit) came from a dirty tree)" 2 }
Push-Location $Root
$Changed = @(& git diff --name-only "$($Stamp.commit)" HEAD -- Source Content Config Dusktracer.uproject)
$DiffRc = $LASTEXITCODE
Pop-Location
if ($DiffRc -ne 0) { Stop-Check "NOT RUN (package commit $($Stamp.commit) is not in this history)" 2 }
if ($Changed.Count -gt 0) { Stop-Check "NOT RUN (package $($Stamp.commit) is stale, $($Changed.Count) file(s) changed since)" 2 }

$ShipExe = Join-Path $PackageRoot "Shipping\Windows\Dusktracer\Binaries\Win64\Dusktracer-Win64-Shipping.exe"
$DevExe = Join-Path $PackageRoot "Development\Windows\Dusktracer\Binaries\Win64\Dusktracer.exe"
$Export = Join-Path $PackageRoot "Shipping\Windows\Export_Logs.ps1"
foreach ($File in $ShipExe, $DevExe, $Export) {
	if (-not (Test-Path $File)) { Stop-Check "NOT RUN (package incomplete, no $File)" 2 }
}

foreach ($Culture in $Cultures) {
	$Bat = Join-Path $PackageRoot "Shipping\Windows\Play_$Culture.bat"
	if (-not (Test-Path $Bat) -or -not (Select-String -Path $Bat -SimpleMatch "-culture=$Culture -DTSeed=%RANDOM%" -Quiet)) { Stop-Check "FAIL (launcher Play_$Culture.bat missing or wrong)" 1 }
}
# --- window helper: the check must never show one ------------------------------------------------------------------
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class DTWindows {
	delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
	[DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
	static List<IntPtr> Of(uint target) {
		var found = new List<IntPtr>();
		EnumWindows((h, l) => { uint pid; GetWindowThreadProcessId(h, out pid); if (pid == target) found.Add(h); return true; }, IntPtr.Zero);
		return found;
	}
	public static int Visible(int pid) { int n = 0; foreach (var h in Of((uint)pid)) if (IsWindowVisible(h)) n++; return n; }
}
"@

$script:Failures = @()
$script:ShownWindows = 0
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; $script:Failures += $Text }
function Watch-Windows($Proc) { if (-not $Proc.HasExited) { $script:ShownWindows += [DTWindows]::Visible($Proc.Id) } }
function Stop-ByPid($Proc) { if (-not $Proc.HasExited) { & taskkill /PID $Proc.Id /T /F | Out-Null } }

function Remove-MarkerRuns {
	foreach ($Dir in $Playtests, $DevPlaytests) {
		if (Test-Path $Dir) { Get-ChildItem $Dir -Filter "${MarkerSeed}_*" -File | Remove-Item -Force }
	}
}

$OffScreen = @("-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-unattended")
$Interval = Get-Constant "TEL.INTERVAL_S"

try {
	Remove-MarkerRuns

	# --- Shipping: run start, telemetry, quit ---------------------------------------------------------------------------
	foreach ($Culture in $Cultures) {
		$Before = @(if (Test-Path $Playtests) { Get-ChildItem $Playtests -Filter "${MarkerSeed}_*.jsonl" | ForEach-Object { $_.Name } })
		$BotArgs = @("-DTBot=Steered", ("-DTBotMaxSeconds={0}" -f ($Interval + 5)), "-DTBotQuit")
		$Proc = Start-Process -FilePath $ShipExe -ArgumentList ($OffScreen + @("-culture=$Culture", "-DTSeed=$MarkerSeed", "-DTOfferAutoPick") + $BotArgs) -PassThru
		Write-Host "padcheck: Shipping $Culture (PID $($Proc.Id))"
		try {
			$Deadline = (Get-Date).AddSeconds($LoadTimeout + $Interval)
			while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1; Watch-Windows $Proc }
			if (-not $Proc.HasExited) { Note-Fail "$Culture Shipping: the game did not quit by itself" }
		}
		finally {
			Stop-ByPid $Proc
		}
		$Jsonl = if (Test-Path $Playtests) { Get-ChildItem $Playtests -Filter "${MarkerSeed}_*.jsonl" | Where-Object { $Before -notcontains $_.Name } | Select-Object -First 1 }
		if (-not $Jsonl) { Note-Fail "$Culture Shipping: no telemetry file in $Playtests"; continue }
		$Rows = @(Get-Content ([IO.Path]::ChangeExtension($Jsonl.FullName, ".csv")) -Encoding UTF8).Count
		if ($Rows -lt 3) { Note-Fail "$Culture Shipping: $Rows CSV line(s), expected a header, a closed interval and the last partial one" }
		$Lines = @(Get-Content $Jsonl.FullName -Encoding UTF8)
		$Start = $Lines | Select-Object -First 1
		$End = $Lines | Select-Object -Last 1
		$Build = if ($Start -match '"build":"([^"]*)"') { $Matches[1] } else { "" }
		if ($Start -notmatch '"kind":"run_start"') { Note-Fail "$Culture Shipping: first record is not run_start" }
		if ($Start -notmatch ('"culture":"' + [regex]::Escape($Culture) + '"')) { Note-Fail "$Culture Shipping: run_start culture: $Start" }
		if ($Build -ne $Stamp.commit) { Note-Fail "$Culture Shipping: build '$Build' is not the stamp '$($Stamp.commit)'" }
		if ($End -notmatch '"kind":"run_end"' -or $End -notmatch '"reason":"bot_timeout"') { Note-Fail "$Culture Shipping: last record is not run_end bot_timeout: $($End.Substring(0, [Math]::Min(160, $End.Length)))" }
		Write-Host ("    {0} records, build {1}, last: {2}" -f $Lines.Count, $Build, ($End -replace '^(.{0,90}).*$', '$1'))
	}

	# --- Export: the hand-over zip, read by the tool the scheduler uses ------------------------------------------------
	$ExportDir = Join-Path $WorkDir "export"
	if (Test-Path $ExportDir) { Remove-Item $ExportDir -Recurse -Force }
	$Out = & powershell -NoProfile -ExecutionPolicy Bypass -File $Export -OutDir $ExportDir 2>&1
	$Rc = $LASTEXITCODE
	$Zip = Get-ChildItem $ExportDir -Filter *.zip -ErrorAction SilentlyContinue | Select-Object -First 1
	if ($Rc -ne 0 -or -not $Zip) { $Out | ForEach-Object { "    $_" }; Note-Fail "export failed" }
	else {
		$Open = Join-Path $ExportDir "open"
		Expand-Archive $Zip.FullName $Open
		$Inside = @(Get-ChildItem $Open -Recurse -Filter "${MarkerSeed}_*.jsonl")
		$Manifest = @(Get-ChildItem $Open -Recurse -Filter MANIFEST.txt)
		if ($Inside.Count -ne $Cultures.Count) { Note-Fail "export holds $($Inside.Count) check run(s), expected $($Cultures.Count)" }
		if ($Manifest.Count -ne 1) { Note-Fail "export has no MANIFEST.txt" }
		$Stray = @(Get-ChildItem $Open -Recurse -File | Where-Object { $_.Extension -notin ".csv", ".jsonl", ".log", ".txt" })
		if ($Stray.Count) { Note-Fail "export carries other files: $($Stray[0].Name)" }
		if ($Inside.Count) {
			$Split = & python -I (Join-Path $Root "Tools\power_split.py") $Inside[0].FullName 2>&1
			$SplitRc = $LASTEXITCODE
			if (-not ($Split | Select-String "POWER:")) { $Split | ForEach-Object { "    $_" }; Note-Fail "power_split.py gave no verdict (rc $SplitRc)" }
			else { Write-Host ("    export ok, " + (($Split | Select-String "POWER:" | Select-Object -Last 1).Line)) }
		}
	}

	# --- Development: what Shipping cannot show (R-PKG-02); the same cook -------------------------------------------------
	$Seconds = Get-Constant "SMOKE.SECONDS"
	foreach ($Culture in $Cultures) {
		$Tag = "package_$Culture"
		$Log = Join-Path $LogDir "PadCheck_$Tag.log"
		$Shot = Join-Path $WorkDir "$Tag.png"
		$Remove = @($Log, $Shot) | Where-Object { Test-Path $_ }
		if ($Remove) { Remove-Item $Remove -Force }
		$ArgList = $OffScreen + @($Map, "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", "-DTShotAfter=$Seconds", "-DTNoDirector", "-DTSeed=$MarkerSeed")
		$Proc = Start-Process -FilePath $DevExe -ArgumentList $ArgList -PassThru
		Write-Host "padcheck: Development $Culture (PID $($Proc.Id))"
		$Deadline = (Get-Date).AddSeconds($LoadTimeout)
		try {
			while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1; Watch-Windows $Proc }
			if (-not $Proc.HasExited) { Note-Fail "$Culture Development: no self-quit in $LoadTimeout s" }
		}
		finally {
			Stop-ByPid $Proc
		}
		$Text = if (Test-Path $Log) { Get-Content $Log -Raw -Encoding UTF8 } else { "" }
		$Lines = $Text -split "`r?`n"
		if ($Text -notmatch "LogDT: Run start") { Note-Fail "$Culture Development: no Run start" }
		$Bad = @($Lines | Where-Object { $_ -match "Fatal error|=== Critical error:? ===|Unhandled Exception|Ensure condition failed|LogDT: (Error|Fatal)" })
		if ($Bad.Count) { Note-Fail "$Culture Development: $($Bad[0])" }
		$Expected = (Import-Csv -Path (Join-Path $Root "Content\DT\Text\ST_UI.csv") -Encoding UTF8 | Where-Object { $_.Key -eq "Test.LocRoundTrip" }).$Culture
		$Drawn = $Lines | Select-String -Pattern "LogDT: LocRoundTrip culture=(\S+) text=(.*)$" | Select-Object -First 1
		if (-not $Drawn) { Note-Fail "$Culture Development: no LocRoundTrip line" }
		elseif ($Drawn.Matches[0].Groups[1].Value -ne $Culture -or $Drawn.Matches[0].Groups[2].Value.Trim() -cne $Expected) {
			Note-Fail "$Culture Development: drew '$($Drawn.Matches[0].Groups[2].Value.Trim())' as '$($Drawn.Matches[0].Groups[1].Value)', expected '$Expected'"
		}
		# The screenshot hook exits mid-run through RequestExit, where a close message also ends up (R-PKG-05).
		$DevRun = if (Test-Path $DevPlaytests) { Get-ChildItem $DevPlaytests -Filter "${MarkerSeed}_*.jsonl" | Sort-Object LastWriteTime | Select-Object -Last 1 }
		$DevEnd = if ($DevRun) { Get-Content $DevRun.FullName -Encoding UTF8 | Select-Object -Last 1 } else { "" }
		if ($DevEnd -notmatch '"kind":"run_end"' -or $DevEnd -notmatch '"reason":"quit"') { Note-Fail "$Culture Development: exit mid-run left no run_end quit: $DevEnd" }
		if (-not (Test-Path $Shot)) { Note-Fail "$Culture Development: no screenshot" }
		else {
			$Golden = Join-Path $Root "Docs\Validation\goldens\arena_empty_$Culture.png"
			$Img = & python -I (Join-Path $Root "Tools\image_check.py") $Shot --golden $Golden 2>&1
			$ImgRc = $LASTEXITCODE
			$ImgLine = ($Img | Select-String "^IMAGE:" | Select-Object -Last 1).Line
			Write-Host "    $ImgLine"
			if ($ImgRc -ne 0) { Note-Fail "$Culture Development: $ImgLine" }
		}
	}
}
finally {
	Remove-MarkerRuns
}

if ($script:ShownWindows -gt 0) { Note-Fail "a game window was visible ($($script:ShownWindows) sample(s))" }
$Culturelist = $Cultures -join "+"
if ($script:Failures.Count) { Stop-Check "FAIL ($($script:Failures.Count) failure(s), first: $($script:Failures[0]))" 1 }

# --- the human half (R-PKG-09) ------------------------------------------------------------------------------------------
$PadRuns = @(if (Test-Path $Playtests) {
		Get-ChildItem $Playtests -Filter *.csv | Where-Object { Select-String -Path $_.FullName -Pattern (",$($Stamp.commit),[^,]*,pad\s*$") -Quiet }
	})
if ($PadRuns.Count) { Stop-Check "PASS (package $($Stamp.commit) ok, $Culturelist; real pad in $($PadRuns.Count) run(s), first $($PadRuns[0].Name))" 0 }
Stop-Check "NOT RUN (package $($Stamp.commit) ok, $Culturelist, 0 windows; real pad needs a human, no run of this build reads input_device=pad)" 2
