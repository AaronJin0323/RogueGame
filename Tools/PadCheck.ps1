# Package check (gate step `pad`, 02_Systems/package.md R-PKG-07..09): what a machine can prove about the package, off-screen.
#   & Tools\PadCheck.ps1 [-Cultures en,zh-Hans] [-LoadTimeout 300]
# Shipping (T-PKG-02): -RenderOffScreen launch, a run starts, the sim reaches the first CSV interval, WM_CLOSE (what Alt+F4 sends) ends it,
# run_end reads quit. Export (T-PKG-04): the zip holds that run and power_split.py reads it. Development (T-PKG-03): drawn text, log, screenshot
# against the arena golden. T-PKG-05: no visible window of the game's process at any time. The check's own run files are deleted afterwards.
# A real pad needs a human: the step passes only when a run of this build has input_device=pad (R-PKG-09); a machine never fakes that.
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
$Export = Join-Path $PackageRoot "Shipping\Windows\ExportTelemetry.ps1"
foreach ($File in $ShipExe, $DevExe, $Export) {
	if (-not (Test-Path $File)) { Stop-Check "NOT RUN (package incomplete, no $File)" 2 }
}

# --- window helpers (the check must never show one; WM_CLOSE reaches the hidden game window) ------------------------
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class DTWindows {
	delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
	[DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hWnd);
	[DllImport("user32.dll")] static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr w, IntPtr l);
	static List<IntPtr> Of(uint target) {
		var found = new List<IntPtr>();
		EnumWindows((h, l) => { uint pid; GetWindowThreadProcessId(h, out pid); if (pid == target) found.Add(h); return true; }, IntPtr.Zero);
		return found;
	}
	public static int Visible(int pid) { int n = 0; foreach (var h in Of((uint)pid)) if (IsWindowVisible(h)) n++; return n; }
	public static int Close(int pid) { var all = Of((uint)pid); foreach (var h in all) PostMessage(h, 0x0010, IntPtr.Zero, IntPtr.Zero); return all.Count; }
}
"@

$script:Failures = @()
$script:ShownWindows = 0
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; $script:Failures += $Text }
function Watch-Windows($Proc) { if (-not $Proc.HasExited) { $script:ShownWindows += [DTWindows]::Visible($Proc.Id) } }
function Stop-ByPid($Proc) { if (-not $Proc.HasExited) { & taskkill /PID $Proc.Id /T /F | Out-Null } }

function Remove-MarkerRuns {
	if (Test-Path $Playtests) { Get-ChildItem $Playtests -Filter "${MarkerSeed}_*" -File | Remove-Item -Force }
}

$OffScreen = @("-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-unattended")
$Interval = Get-Constant "TEL.INTERVAL_S"

try {
	Remove-MarkerRuns

	# --- Shipping: run start, telemetry, quit ---------------------------------------------------------------------------
	foreach ($Culture in $Cultures) {
		$Before = @(if (Test-Path $Playtests) { Get-ChildItem $Playtests -Filter "${MarkerSeed}_*.jsonl" | ForEach-Object { $_.Name } })
		$Proc = Start-Process -FilePath $ShipExe -ArgumentList ($OffScreen + @("-culture=$Culture", "-DTSeed=$MarkerSeed", "-DTOfferAutoPick")) -PassThru
		Write-Host "padcheck: Shipping $Culture (PID $($Proc.Id))"
		$Jsonl = $null
		$Csv = $null
		try {
			$Deadline = (Get-Date).AddSeconds($LoadTimeout)
			while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline -and -not $Jsonl) {
				Start-Sleep -Seconds 1
				Watch-Windows $Proc
				if (Test-Path $Playtests) { $Jsonl = Get-ChildItem $Playtests -Filter "${MarkerSeed}_*.jsonl" | Where-Object { $Before -notcontains $_.Name } | Select-Object -First 1 }
			}
			if (-not $Jsonl) { Note-Fail "$Culture Shipping: no telemetry file in $LoadTimeout s (exited=$($Proc.HasExited))"; continue }
			$Csv = [IO.Path]::ChangeExtension($Jsonl.FullName, ".csv")
			$Deadline = (Get-Date).AddSeconds($Interval + 60)
			$Rows = 0
			while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline -and $Rows -lt 2) {
				Start-Sleep -Seconds 1
				Watch-Windows $Proc
				if (Test-Path $Csv) { $Rows = @(Get-Content $Csv -Encoding UTF8).Count }
			}
			if ($Rows -lt 2) { Note-Fail "$Culture Shipping: no closed CSV interval (exited=$($Proc.HasExited))" }
			$Exited = $Proc.HasExited
			$Posted = [DTWindows]::Close($Proc.Id)
			if (-not $Exited -and -not $Proc.WaitForExit(30000)) { Note-Fail "$Culture Shipping: WM_CLOSE to $Posted window(s) did not end the game" }
		}
		finally {
			Stop-ByPid $Proc
		}
		$Lines = @(Get-Content $Jsonl.FullName -Encoding UTF8)
		$Start = $Lines | Select-Object -First 1
		$End = $Lines | Select-Object -Last 1
		$Build = if ($Start -match '"build":"([^"]*)"') { $Matches[1] } else { "" }
		if ($Start -notmatch '"kind":"run_start"') { Note-Fail "$Culture Shipping: first record is not run_start" }
		if ($Start -notmatch ('"culture":"' + [regex]::Escape($Culture) + '"')) { Note-Fail "$Culture Shipping: run_start culture: $Start" }
		if ($Build -ne $Stamp.commit) { Note-Fail "$Culture Shipping: build '$Build' is not the stamp '$($Stamp.commit)'" }
		if ($End -notmatch '"kind":"run_end"' -or $End -notmatch '"reason":"quit"') { Note-Fail "$Culture Shipping: last record is not run_end quit: $($End.Substring(0, [Math]::Min(160, $End.Length)))" }
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
		$Stray = @(Get-ChildItem $Open -Recurse -File | Where-Object { $_.Extension -notin ".csv", ".jsonl", ".txt" })
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
		$ArgList = $OffScreen + @($Map, "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", "-DTShotAfter=$Seconds", "-DTNoDirector")
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
