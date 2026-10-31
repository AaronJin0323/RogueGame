# T-TEC-10 performance measurement (07_Technical R-TEC-10, 11_Quality s5) on the stress arena.
#   & Tools\Perf.ps1 [-Cmds "DT.Spawn.Keep 400,DT.Bullets 6000"] [-ResX 1920 -ResY 1080] [-Warmup <s>] [-Frames <n>]
#                    [-Map /Game/DT/Maps/L_Arena_Test] [-Culture en] [-Scene Caps|S2] [-Package]
# -Package measures the Development package of Tools\Package.ps1 instead of the editor (R-TEC-15: the frame cost a player's float mode pays;
# Shipping has no CSV profiler, so Development stands in). The same scene, budgets and proof; perf.csv marks the row "[package]".
# -Scene S2 is the S2 proof scene (DT.PerfScene S2, Docs/Validation/S2_perf.md): it adds to the same budgets and load floors a proof
# from the capture window itself (the CSV's DT/S2Sum_* last minus first, DT/S2Live_* median, and the DT/ phase timers as a profile),
# and fails when the window did not hold the load. -Cmds picks a variant of the scene and keeps the proof. Off, the run is as before.
# Holds the GPU semaphore Saved\gpu.lock (an exclusively opened file) for the whole run and refuses to start while
# another process holds it: readings taken under contention are discarded, so none are taken.
# Launches the game like Tools\Smoke.ps1 with the stress commands (default: C.ENEMIES.CAP enemies, C.BULLETS.CAP
# bullets), lets ADTGameMode record the CSV event DTPerfStart after -Warmup seconds (default C.PERF.WARMUP_S), and the
# UE CSV profiler captures -Frames frames from that event (default C.PERF.CAPTURE_FRAMES) and quits the game.
# Prints p50 / p95 of FrameTime, GameThreadTime and GPU time (when the CSV has it) against C.BUDGET.GAME_THREAD_P50_MS,
# C.BUDGET.GAME_THREAD_P95_MS and C.BUDGET.GPU_MS_4070 (judged on the GPU p95), then "PERF: PASS|FAIL (...)";
# appends one line to Saved\Perf\perf.csv. Exit 0 = pass, 1 = fail or no measurement.
param(
	[string]$Cmds = "",
	[int]$ResX = 1920,
	[int]$ResY = 1080,
	[double]$Warmup = -1,
	[int]$Frames = -1,
	[string]$Map = "/Game/DT/Maps/L_Arena_Test",
	[string]$Culture = "en",
	[int]$LoadTimeout = 900,
	[ValidateSet("Caps", "S2")]
	[string]$Scene = "Caps",
	[switch]$Package
)

trap { Write-Host "PERF: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$Saved = Join-Path $Root "Saved"
$LogDir = Join-Path $Saved "Logs"
$PerfDir = Join-Path $Saved "Perf"
$CsvDir = Join-Path $Saved "Profiling\CSV"
$Target = @("`"$Project`"", $Map, "-game")
if ($Package) {
	$Staged = Join-Path $Saved "Package\Development\Windows\Dusktracer"
	$Exe = Join-Path $Staged "Binaries\Win64\Dusktracer.exe"
	if (-not (Test-Path $Exe)) { Write-Host "PERF: FAIL (no $Exe; Tools\Package.ps1; not run)"; exit 1 }
	$CsvDir = Join-Path $Staged "Saved\Profiling\CSV"
	$Target = @($Map)
}
New-Item -ItemType Directory -Force $LogDir, $PerfDir | Out-Null
$Log = Join-Path $LogDir "Perf.log"

function Get-Constant([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*([-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return [double]$Line.Matches[0].Groups[1].Value
}
$GtP50Max = Get-Constant "BUDGET.GAME_THREAD_P50_MS"
$GtP95Max = Get-Constant "BUDGET.GAME_THREAD_P95_MS"
$GpuMax = Get-Constant "BUDGET.GPU_MS_4070"
# D-011: on an RTX 4080 the GPU reading is scaled up by a pessimistic proxy factor before it is judged.
$GpuProxy = Get-Constant "PERF.GPU_PROXY_4080_TO_4070"
$LoadMin = Get-Constant "PERF.LOAD_MIN_RATIO"
$BulletCap = Get-Constant "BULLETS.CAP"
$EnemyCap = Get-Constant "ENEMIES.CAP"
$IsS2 = $Scene -eq "S2"
if ($Warmup -lt 0) { $Warmup = Get-Constant $(if ($IsS2) { "PERF.S2_WARMUP_S" } else { "PERF.WARMUP_S" }) }
if ($Frames -lt 0) { $Frames = [int](Get-Constant "PERF.CAPTURE_FRAMES") }
if (-not $PSBoundParameters.ContainsKey("Cmds")) {
	$Cmds = if ($IsS2) { "DT.PerfScene S2" } else { "DT.Spawn.Keep {0},DT.Bullets {1}" -f (Get-Constant "ENEMIES.CAP"), (Get-Constant "BULLETS.CAP") }
}

# --- GPU semaphore --------------------------------------------------------------------------------------------------
$LockPath = Join-Path $Saved "gpu.lock"
try {
	$Lock = [System.IO.File]::Open($LockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
}
catch {
	Write-Host "PERF: FAIL (Saved\gpu.lock is held by another measurement; not run)"
	exit 1
}

$Proc = $null
$Killed = $false
$Csv = $null
try {
	$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
	$Bytes = [System.Text.Encoding]::ASCII.GetBytes("Perf.ps1 PID $PID $Stamp`r`n")
	$Lock.SetLength(0); $Lock.Write($Bytes, 0, $Bytes.Length); $Lock.Flush()

	$Gpu = ((Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name) -join " + ")
	$Head = (& git -C $Root rev-parse --short HEAD) 2>$null
	$Before = @(if (Test-Path $CsvDir) { Get-ChildItem $CsvDir -Filter *.csv | ForEach-Object { $_.FullName } })

	if (Test-Path $Log) { Remove-Item $Log -Force }
	$ArgList = $Target + @("-windowed", "-RenderOffScreen", "-ResX=$ResX", "-ResY=$ResY", "-LiveCoding=false", "-nosplash",
		"-unattended", "-culture=$Culture", "-ABSLOG=`"$Log`"", ("-DTCsvAfter={0}" -f $Warmup), "-csvStartOnEvent=DTPerfStart",
		"-csvCaptureOnEventFrameCount=$Frames", "-ExitAfterCsvProfiling", "-csvGpuStats")
	if ($Cmds) { $ArgList += "-ExecCmds=`"$Cmds`"" }
	if ($IsS2) { $ArgList += "-DTNoDirector" }  # no timeline in the scene: what fires is the build's, the crowd is the scene's
	$Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru
	Write-Host ("perf: {0} {1}x{2} cmds='{3}' warmup {4} s, {5} frames, GPU '{6}' (PID {7})" -f $Map, $ResX, $ResY, $Cmds, $Warmup, $Frames, $Gpu, $Proc.Id)

	function Read-Log { if (Test-Path $Log) { Get-Content $Log -Raw -ErrorAction SilentlyContinue } else { "" } }
	$Deadline = (Get-Date).AddSeconds($LoadTimeout)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline -and (Read-Log) -notmatch "LogDT: Run start") { Start-Sleep -Seconds 1 }
	# Capture ends after $Frames frames; allow down to 10 fps before giving up.
	$Deadline = (Get-Date).AddSeconds($Warmup + $Frames / 10 + 60)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Start-Sleep -Seconds 1 }
}
finally {
	if ($Proc -and -not $Proc.HasExited) {
		& taskkill /PID $Proc.Id /T /F | Out-Null
		$Killed = $true
		Start-Sleep -Seconds 1
	}
	$Lock.Dispose()
	Remove-Item $LockPath -Force -ErrorAction SilentlyContinue
}

# --- parse the CSV profiler output ------------------------------------------------------------------------------------
$New = @(if (Test-Path $CsvDir) { Get-ChildItem $CsvDir -Filter *.csv | Where-Object { $Before -notcontains $_.FullName } | Sort-Object LastWriteTime })
if ($New.Count -eq 0) {
	Write-Host "    log: $Log"
	Write-Host ("PERF: FAIL (no CSV capture written; killed={0})" -f $Killed)
	exit 1
}
$Csv = $New[-1].FullName
$Lines = Get-Content $Csv
$Header = $Lines[0].Split(",")
function Col([string[]]$Names) { foreach ($N in $Names) { $I = [array]::IndexOf($Header, $N); if ($I -ge 0) { return $I } }; return -1 }
$Cols = [ordered]@{
	"frame" = Col @("FrameTime")
	"game"  = Col @("GameThreadTime")
	"gpu"   = Col @("GPUTime", "GPU/Total", "RHI/GPUTime")
	"bul"   = Col @("DT/LiveBullets")
	"enm"   = Col @("DT/LiveEnemies")
}
$Values = @{}
foreach ($K in $Cols.Keys) { $Values[$K] = New-Object System.Collections.Generic.List[double] }
foreach ($L in $Lines[1..($Lines.Count - 1)]) {
	if ($L.StartsWith("[") -or $L -eq $Lines[0]) { break }  # metadata / repeated header at the end
	$Cells = $L.Split(",")
	foreach ($K in $Cols.Keys) {
		$I = $Cols[$K]
		$V = 0.0
		if ($I -ge 0 -and $I -lt $Cells.Count -and [double]::TryParse($Cells[$I], [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$V)) {
			$Values[$K].Add($V)
		}
	}
}
function Pct($List, [double]$P) {
	if ($List.Count -eq 0) { return [double]::NaN }
	$S = $List.ToArray(); [array]::Sort($S)
	return $S[[math]::Min($S.Count - 1, [math]::Max(0, [math]::Ceiling($P * $S.Count) - 1))]
}
$R = @{}
foreach ($K in $Cols.Keys) { $R["${K}50"] = Pct $Values[$K] 0.50; $R["${K}95"] = Pct $Values[$K] 0.95 }
$N = $Values["frame"].Count

function Fmt([double]$V) { if ([double]::IsNaN($V)) { "n/a" } else { $V.ToString("0.00", [Globalization.CultureInfo]::InvariantCulture) } }
Write-Host ("    frames      {0} (csv {1})" -f $N, $Csv)
Write-Host ("    FrameTime   p50 {0} ms  p95 {1} ms" -f (Fmt $R.frame50), (Fmt $R.frame95))
Write-Host ("    GameThread  p50 {0} ms  p95 {1} ms   budget p50 <= {2}, p95 <= {3}" -f (Fmt $R.game50), (Fmt $R.game95), $GtP50Max, $GtP95Max)
Write-Host ("    Load        bullets p50 {0} (cap {1})  enemies p50 {2} (cap {3})  need >= {4} of cap" -f (Fmt $R.bul50), $BulletCap, (Fmt $R.enm50), $EnemyCap, $LoadMin)
$Factor = if ($Gpu -match "4080") { $GpuProxy } else { 1.0 }
# --- the S2 scene's proof, from the capture window ------------------------------------------------------------------------
$Sums = @{}
$Lives = @{}
if ($IsS2) {
	$Rows = New-Object System.Collections.Generic.List[string]
	foreach ($L in $Lines[1..($Lines.Count - 1)]) {
		if ($L.StartsWith("[") -or $L -eq $Lines[0]) { break }
		$Rows.Add($L)
	}
	$Inv = [Globalization.CultureInfo]::InvariantCulture
	function Cell([string[]]$Cells, [int]$I) {  # a stat not yet written in a frame is an empty cell
		$V = 0.0
		if ($I -lt $Cells.Count) { [void][double]::TryParse($Cells[$I], [Globalization.NumberStyles]::Float, $Inv, [ref]$V) }
		return $V
	}
	$Seconds = 0.0
	$FrameCol = $Cols["frame"]
	foreach ($Row in $Rows) { $Seconds += (Cell $Row.Split(",") $FrameCol) / 1000.0 }
	for ($I = 0; $I -lt $Header.Count; $I++) {
		$Name = $Header[$I]
		if ($Name -like "DT/S2Sum_*") {
			$List = New-Object System.Collections.Generic.List[double]  # monotonic: the window is its span (the last frames hold no stats)
			foreach ($Row in $Rows) { $List.Add((Cell $Row.Split(",") $I)) }
			$Span = @($List.ToArray() | Where-Object { $_ -gt 0 } | Measure-Object -Minimum -Maximum)
			$Sums[$Name.Substring(9)] = if ($Span.Count -gt 0) { $Span[0].Maximum - $Span[0].Minimum } else { 0 }
		}
		elseif ($Name -like "DT/S2Live_*") {
			$List = New-Object System.Collections.Generic.List[double]
			foreach ($Row in $Rows) { $List.Add((Cell $Row.Split(",") $I)) }
			$Lives[$Name.Substring(10)] = Pct $List 0.50
		}
		elseif ($Name -like "DT/*" -and $Name -notlike "DT/Live*") {
			$List = New-Object System.Collections.Generic.List[double]
			foreach ($Row in $Rows) { $List.Add((Cell $Row.Split(",") $I)) }
			Write-Host ("    profile     {0,-32} mean {1} p50 {2} p95 {3} ms" -f $Name, (Fmt (($List | Measure-Object -Average).Average)), (Fmt (Pct $List 0.50)), (Fmt (Pct $List 0.95)))
		}
	}
	Write-Host ("    window      {0:0.0} s" -f $Seconds)
	$Fires = @($Sums.Keys | Where-Object { $_ -like "Fire_*" } | Sort-Object)
	foreach ($K in $Fires) { Write-Host ("    fired       {0,-26} {1,7}  {2,7:0.0}/s" -f $K.Substring(5), $Sums[$K], ($Sums[$K] / $Seconds)) }
	foreach ($K in ($Sums.Keys | Where-Object { $_ -notlike "Fire_*" } | Sort-Object)) { Write-Host ("    counted     {0,-26} {1,7}  {2,7:0.0}/s" -f $K, $Sums[$K], ($Sums[$K] / $Seconds)) }
	foreach ($K in ($Lives.Keys | Sort-Object)) { Write-Host ("    live p50    {0,-26} {1,7}" -f $K, $Lives[$K]) }
}
$Gpu95Judged = $R.gpu95 * $Factor
# A scene with Reaction.Duos proves its reactions from the run's telemetry (reactions.md).
$Session = Select-String -Path $Log -Pattern "Telemetry: session seed=\S+ players=\d+ -> (.+)\.csv" -ErrorAction SilentlyContinue | Select-Object -Last 1
if ($Session -and (Test-Path ($Session.Matches[0].Groups[1].Value + ".jsonl"))) {
	Select-String -Path ($Session.Matches[0].Groups[1].Value + ".jsonl") -Pattern '"kind":"reaction".*"fires":(\{[^}]*\})' | ForEach-Object { Write-Host ("    reaction  " + $_.Matches[0].Groups[1].Value) }
}
Write-Host ("    GPU         p50 {0} ms  p95 {1} ms   x{2} proxy = {3} ms, budget p95 <= {4} (C.BUDGET.GPU_MS_4070; GPU here: {5})" -f (Fmt $R.gpu50), (Fmt $R.gpu95), $Factor, (Fmt $Gpu95Judged), $GpuMax, $Gpu)

$Why = @()
if ($N -lt $Frames * 0.9) { $Why += "captured $N of $Frames frames" }
if ([double]::IsNaN($R.game50)) { $Why += "no GameThreadTime column" }
elseif ($R.game50 -gt $GtP50Max -or $R.game95 -gt $GtP95Max) { $Why += "game thread over budget" }
if (-not [double]::IsNaN($R.gpu95) -and $Gpu95Judged -gt $GpuMax) { $Why += "GPU over budget" }
# A pass at a lighter load than the caps proves nothing: the median live counts must reach C.PERF.LOAD_MIN_RATIO of them.
if ($IsS2 -or $PSBoundParameters.ContainsKey("Cmds") -eq $false) {
	if ([double]::IsNaN($R.bul50) -or $R.bul50 -lt $BulletCap * $LoadMin) { $Why += "bullet load below cap" }
	if ([double]::IsNaN($R.enm50) -or $R.enm50 -lt $EnemyCap * $LoadMin) { $Why += "enemy load below cap" }
}
# The S2 scene proves its load from what fired in the window, never from its settings.
if ($IsS2) {
	$Steam = if ($Sums.ContainsKey("Fire_ForgeTide")) { $Sums["Fire_ForgeTide"] / $Seconds } else { 0 }
	$Pairs = @($Sums.Keys | Where-Object { $_ -like "Fire_*" -and $Sums[$_] -gt 0 }).Count
	if ($Steam -lt (Get-Constant "PERF.S2_STEAM_MIN_PER_S")) { $Why += ("Steam {0:0.0}/s under its floor" -f $Steam) }
	if ($Pairs -lt (Get-Constant "PERF.S2_PAIRS_MIN")) { $Why += "$Pairs pairs fired" }
	if ($Lives["Elites"] -lt (Get-Constant "PERF.S2_ELITES_MIN")) { $Why += "too few elites" }
	foreach ($A in 0..5) { if (-not ($Lives["Affix$A"] -gt 0)) { $Why += "affix $A not alive" } }
	foreach ($Need in @("Lances", "Rings", "Shades", "Kills", "Dashes", "DuoFields0", "Discharges", "Husks", "Pillars")) {
		if (-not ($Sums[$Need] -gt 0)) { $Why += "no $Need in the window" }
	}
	if ($Lives["Shades"] -lt 1) { $Why += "no Shade alive" }
	if ($Sums["Decisions"] -ne 0) { $Why += "the build changed in the window" }
}
$Verdict = if ($Why.Count -eq 0) { "PASS" } else { "FAIL" }

$Result = Join-Path $PerfDir "perf.csv"
if (-not (Test-Path $Result)) { "time,commit,gpu,res,cmds,frames,frame_p50,frame_p95,game_p50,game_p95,gpu_p50,gpu_p95,verdict" | Set-Content $Result -Encoding ascii }
('{0},{1},"{2}",{3}x{4},"{5}",{6},{7},{8},{9},{10},{11},{12},{13}' -f $Stamp, $Head, $Gpu, $ResX, $ResY, ($(if ($Package) { "[package] " }) + $Cmds), $N,
	(Fmt $R.frame50), (Fmt $R.frame95), (Fmt $R.game50), (Fmt $R.game95), (Fmt $R.gpu50), (Fmt $R.gpu95), $Verdict) | Add-Content $Result -Encoding ascii

$Summary = "game p50 {0} / p95 {1} ms, GPU p95 {2} ms, load {3} bullets / {4} enemies, {5} frames" -f (Fmt $R.game50), (Fmt $R.game95), (Fmt $R.gpu95), (Fmt $R.bul50), (Fmt $R.enm50), $N
if ($Verdict -eq "PASS") { Write-Host "PERF: PASS ($Summary)"; exit 0 }
Write-Host ("PERF: FAIL ({0}; {1})" -f $Summary, ($Why -join "; "))
exit 1
