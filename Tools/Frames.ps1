# Frames: a staged scene shot at a named size into a named folder, for people and reviewers to look at. No judge.
#   & Tools\Frames.ps1 -Scene <name> [-Culture en|zh-Hans] [-Width 1920] [-Height 1080] [-Seconds <s>] [-ExtraCmds "DT.A 1,DT.B 2"]
#                      [-OutDir Saved\Frames] [-Tag <suffix>]
# What it is for (Docs/Validation/S4_frames.md): Tools\Smoke.ps1 shoots only 1280x720, 24 px/m at today's frame. Art is judged at 36 px/m, which is
# 1920x1080 (the default here), and must still read at 27 px/m, which is 1440x810 (the two densities Tools\EnemyArt.ps1 shoots, C.EAR.SILHOUETTE_PX_PER_M).
# The scene: Tools\Scenes\<name>.json, read as Tools\Smoke.ps1 -Scene reads it ("cmds", "seconds", "director", "seed", "map", the "weather" pin; the
# block below mirrors Smoke.ps1's, which cannot be dot-sourced). "judge", "gate", "elements" and "cultures" are read by no one here. -ExtraCmds is
# appended to the scene's commands (comma separated, as -ExecCmds takes them); -Seconds replaces the scene's shot time.
# The launch is EnemyArt.ps1's and Smoke.ps1's: UnrealEditor -game -RenderOffScreen on the hidden desktop of Tools\HiddenLaunch.ps1 (Start-DTHidden),
# -ForceRes with -ResX/-ResY (without it the engine shrinks a frame larger than 1280x720 on this desktop), -DTShot/-DTShotAfter (the sim is held on
# that step and shot once the still frame has settled, so the frame rate moves nothing), the director off unless the scene turns it on, seed 0
# unless the scene names one. Right after the mask it takes one machine-wide heavy-job slot (Enter-DTHeavy, Tools\Machine.ps1), waiting while none is
# free or the machine is held alone, and keeps it until the script ends. It holds the GPU semaphore (Tools\GpuLock.ps1) for the launch and refuses to
# start while another process holds it.
# The game's process mask is sampled about every 0.2 s for its whole life and must stay inside the machine's mask (D-052; Wait-Sampled mirrors Smoke.ps1).
# A game still alive after the timeout is killed by its own PID only, and the run fails.
# Verdict, and nothing more: "Run start" seen; no crash, no ensure, no Error/Fatal line from LogDT (as Smoke.ps1 reads them); the PNG exists and is
# exactly -Width x -Height (a shrunk frame fails, as in Perf.ps1). No golden, no contrast judge, no text check: a pass says the frame was taken, not
# that it is right. Output: <OutDir>\<scene>_<culture>_<W>x<H>[_<tag>].png (-OutDir relative to the tree; default Saved\Frames).
# Log: Saved\Logs\Frames_<scene>_<culture>_<W>x<H>[_<tag>].log. Prints "FRAMES: PASS (<file>)" or "FRAMES: FAIL (<why>)"; exit 0 / 1.
param(
	[Parameter(Mandatory = $true)]
	[string]$Scene,
	[ValidateSet("en", "zh-Hans")]
	[string]$Culture = "en",
	[int]$Width = 1920,
	[int]$Height = 1080,
	[double]$Seconds = -1,
	[string]$ExtraCmds = "",
	[string]$OutDir = "Saved\Frames",
	[string]$Tag = "",
	[int]$LoadTimeout = 900  # the first launch after render changes compiles shaders
)

trap { Write-Host "FRAMES: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "Frames"  # D-055 item 7: one machine-wide heavy-job slot for this run, none for a run started by one that holds one
Write-Host "frames: heavy slot $DTHeavy"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
. (Join-Path $PSScriptRoot "GpuLock.ps1")
$Exe = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $Root "Dusktracer.uproject"
$LogDir = Join-Path $Root "Saved\Logs"

if ($Scene -notmatch '^[a-z][a-z0-9_]*$') { Write-Host "FRAMES: FAIL (scene name '$Scene': lower case, digits and underscores)"; exit 1 }
if ($Tag -and $Tag -notmatch '^[A-Za-z0-9_-]+$') { Write-Host "FRAMES: FAIL (tag '$Tag': letters, digits, _ and - only)"; exit 1 }
if ($Width -lt 16 -or $Height -lt 16) { Write-Host "FRAMES: FAIL (size ${Width}x${Height})"; exit 1 }
if (-not [IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $Root $OutDir }
New-Item -ItemType Directory -Force $LogDir, $OutDir | Out-Null

# --- the scene, as data (mirrors Tools\Smoke.ps1 -Scene) ------------------------------------------------------------------------
$Weathers = @("Dusk")  # the weathers that exist; Smoke.ps1 holds the list the gate checks
$SceneFile = Join-Path $PSScriptRoot "Scenes\$Scene.json"
if (-not (Test-Path $SceneFile)) { Write-Host "FRAMES: FAIL (no scene Tools\Scenes\$Scene.json)"; exit 1 }
$Data = Get-Content $SceneFile -Raw -Encoding UTF8 | ConvertFrom-Json
$Keys = @($Data.PSObject.Properties.Name)
$Unknown = @($Keys | Where-Object { @("note", "cmds", "seconds", "director", "map", "seed", "judge", "elements", "cultures", "gate", "weather") -notcontains $_ })
if ($Unknown.Count) { Write-Host "FRAMES: FAIL (scene ${Scene}: unknown key(s) $($Unknown -join ', '))"; exit 1 }
if ($Weathers -notcontains $Data.weather) { Write-Host "FRAMES: FAIL (scene ${Scene}: weather '$($Data.weather)' is not pinned to one of: $($Weathers -join ', '))"; exit 1 }
$Weather = $Data.weather
$Cmds = @(@($Data.cmds) + @(if ($ExtraCmds) { $ExtraCmds -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ } })) -join ","
if ($Cmds -match '"') { Write-Host "FRAMES: FAIL (a command holds a quote: it is joined into -ExecCmds)"; exit 1 }
$Map = if ($Keys -contains "map") { $Data.map } else { "/Game/DT/Maps/L_Arena_Test" }
$Seed = if ($Keys -contains "seed") { [int]$Data.seed } else { $null }
$Director = [bool]$Data.director

function Get-Constant([string]$Key) {
	$Line = Select-String -Path (Join-Path $Root "Docs\constants.yaml") -Pattern ("^\s+" + [regex]::Escape($Key) + ":\s*\{\s*value:\s*([-0-9.]+)")
	if (-not $Line) { throw "constant $Key missing from Docs/constants.yaml" }
	return [double]$Line.Matches[0].Groups[1].Value
}
if ($Seconds -lt 0) { $Seconds = if ($Keys -contains "seconds") { [double]$Data.seconds } else { Get-Constant "SMOKE.SECONDS" } }

$Name = "${Scene}_${Culture}_${Width}x${Height}" + $(if ($Tag) { "_$Tag" } else { "" })
$Shot = Join-Path $OutDir "$Name.png"
$Log = Join-Path $LogDir "Frames_$Name.log"
$Head = (& git -C $Root rev-parse --short HEAD) 2>$null
$Dirty = if (@(& git -C $Root status --porcelain --untracked-files=no 2>$null).Count) { "+dirty" } else { "" }

# --- text: generated string table + compiled .locres must be current (R-TEC-08; mirrors Tools\Smoke.ps1) --------------------------
$LocOut = & python -I (Join-Path $Root "Tools\loc_build.py") --build 2>&1
if ($LASTEXITCODE -ne 0) { $LocOut | ForEach-Object { "    $_" }; Write-Host "FRAMES: FAIL (loc_build)"; exit 1 }
$Po = Join-Path $Root "Content\Localization\Game\zh-Hans\Game.po"
$Locres = Join-Path $Root "Content\Localization\Game\zh-Hans\Game.locres"
if (-not (Test-Path $Locres) -or (Get-Item $Locres).LastWriteTime -lt (Get-Item $Po).LastWriteTime) {
	Write-Host "frames: compiling .locres (Tools\Localize.bat)"
	$LocOut = & cmd /c "`"$Root\Tools\Localize.bat`" 2>&1"
	if ($LASTEXITCODE -ne 0) { $LocOut | Select-Object -Last 10 | ForEach-Object { "    $_" }; Write-Host "FRAMES: FAIL (Localize.bat)"; exit 1 }
}

# --- GPU semaphore (as Tools\Perf.ps1 holds it) ---------------------------------------------------------------------------------
$LockPath = Get-DTGpuLockPath $Root
$Lock = Open-DTGpuLock $Root
if (-not $Lock) { Write-Host "FRAMES: FAIL ($LockPath is held by another process; not run)"; exit 1 }

# D-052: the game's process mask is sampled about every 0.2 s for its whole life (mirrors Tools\Smoke.ps1).
$MaskBits = if ($DTMask -and $DTMask -ne "none") { [Convert]::ToUInt64($DTMask, 16) } else { [UInt64]0 }
$Samples = 0
$Outside = @()
$Proc = $null
function Wait-Sampled([double]$Seconds) {
	$Until = (Get-Date).AddSeconds($Seconds)
	do {
		if ($script:MaskBits -ne 0 -and -not $Proc.HasExited) {
			try {
				$Got = [UInt64][Int64]$Proc.ProcessorAffinity
				$script:Samples++
				if (($Got -band (-bnot $script:MaskBits)) -ne 0) { $script:Outside += ("{0:X}" -f $Got) }
			}
			catch { }  # the process ended between the check and the read
		}
		Start-Sleep -Milliseconds 200
	} while ((Get-Date) -lt $Until -and -not $Proc.HasExited)
}
function Read-Log { if (Test-Path $Log) { Get-Content $Log -Raw -Encoding UTF8 -ErrorAction SilentlyContinue } else { "" } }

# --- run --------------------------------------------------------------------------------------------------------------------------
$Killed = $false
try {
	$Bytes = [System.Text.Encoding]::ASCII.GetBytes("Frames.ps1 PID $PID $(Get-Date -Format 'yyyyMMdd_HHmmss') $Name`r`n")
	$Lock.SetLength(0); $Lock.Write($Bytes, 0, $Bytes.Length); $Lock.Flush()
	foreach ($Old in $Shot, $Log) { if (Test-Path $Old) { Remove-Item $Old -Force } }
	$ArgList = @("`"$Project`"", $Map, "-game", "-windowed", "-RenderOffScreen", "-ResX=$Width", "-ResY=$Height", "-ForceRes", "-LiveCoding=false", "-nosplash",
		"-unattended", "-culture=$Culture", "-ABSLOG=`"$Log`"", "-DTShot=`"$Shot`"", ("-DTShotAfter={0}" -f $Seconds))
	if (-not $Director) { $ArgList += "-DTNoDirector" }
	if ($null -ne $Seed) { $ArgList += "-DTSeed=$Seed" }
	if ($Cmds) { $ArgList += "-ExecCmds=`"$Cmds`"" }
	$Proc = Start-DTHidden $Exe $ArgList
	Write-Host ("frames: {0} {1} {2}x{3} culture={4} weather={5} commit {6}{7} cmds='{8}' shot after {9} s (PID {10})" -f $Scene, $Map, $Width, $Height, $Culture, $Weather, $Head, $Dirty, $Cmds, $Seconds, $Proc.Id)
	$Deadline = (Get-Date).AddSeconds($LoadTimeout)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline -and (Read-Log) -notmatch "LogDT: Run start") { Wait-Sampled 1 }
	# After the run starts the game shoots and quits on its own; allow the shot delay plus a margin for slow frames.
	$Deadline = (Get-Date).AddSeconds($Seconds + 120)
	while (-not $Proc.HasExited -and (Get-Date) -lt $Deadline) { Wait-Sampled 1 }
}
finally {
	if ($Proc -and -not $Proc.HasExited) {
		Stop-DTHidden $Proc
		$Killed = $true
	}
	$Lock.Dispose()
	Remove-Item $LockPath -Force -ErrorAction SilentlyContinue
}

# --- verdict: the log (as Tools\Smoke.ps1 reads it) and the frame's size ------------------------------------------------------------
$Text = Read-Log
$Lines = $Text -split "`r?`n"
$Started = $Text -match "LogDT: Run start"
$Crash = @($Lines | Where-Object { $_ -match "Fatal error|=== Critical error:? ===|Unhandled Exception|appError called" })
$Ensure = @($Lines | Where-Object { $_ -match "Ensure condition failed|LogOutputDevice: Error: Ensure" })
$DTErr = @($Lines | Where-Object { $_ -match "LogDT: (Error|Fatal)" })
$Crash + $Ensure + $DTErr | Select-Object -First 15 | ForEach-Object { "  ! $_" }

# The size the frame really has, from the PNG's own IHDR (bytes 16-23, big-endian), never from the flags.
function Get-PngSize([string]$Path) {
	$B = New-Object byte[] 24
	$S = [IO.File]::OpenRead($Path)
	try { $N = $S.Read($B, 0, 24) } finally { $S.Dispose() }
	if ($N -lt 24 -or $B[1] -ne 0x50 -or $B[2] -ne 0x4E -or $B[3] -ne 0x47 -or [Text.Encoding]::ASCII.GetString($B, 12, 4) -ne "IHDR") { return "not a PNG" }
	$W = ([int]$B[16] -shl 24) -bor ([int]$B[17] -shl 16) -bor ([int]$B[18] -shl 8) -bor [int]$B[19]
	$H = ([int]$B[20] -shl 24) -bor ([int]$B[21] -shl 16) -bor ([int]$B[22] -shl 8) -bor [int]$B[23]
	return "${W}x${H}"
}
$Got = if (Test-Path $Shot) { Get-PngSize $Shot } else { "no screenshot" }
if ($Outside.Count) { $Outside | Sort-Object -Unique | ForEach-Object { "  ! the game's process mask read $_, outside $DTMask" } }
$MaskOk = ($MaskBits -eq 0) -or ($Samples -gt 0 -and $Outside.Count -eq 0)
Write-Host "    log: $Log"

$Why = @()
if (-not $Started) { $Why += "no Run start" }
if ($Killed) { $Why += "killed at the timeout" }
if ($Crash.Count) { $Why += "$($Crash.Count) crash line(s)" }
if ($Ensure.Count) { $Why += "$($Ensure.Count) ensure(s)" }
if ($DTErr.Count) { $Why += "$($DTErr.Count) LogDT error(s)" }
if ($Got -ne "${Width}x${Height}") { $Why += "frame $Got, not ${Width}x${Height}" }
if (-not $MaskOk) { $Why += "process mask outside $DTMask ($Samples sample(s), $($Outside.Count) outside)" }
if ($Why.Count) { Write-Host ("FRAMES: FAIL ({0}; {1})" -f ($Why -join "; "), $Name); exit 1 }
Write-Host "FRAMES: PASS ($Shot)"
exit 0
