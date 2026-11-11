# Machine facts (D-052): the processor mask of this PC, applied by every Tools entry script as its first statement, so the compiler, the engine,
# the bots and Python all inherit it.
#   . (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity
#   Set-DTAffinity [-Path <mask file>]       sets this process's affinity to the mask; returns it as text ("FFFFFBFE"), or "none"
#   Enter-DTHeavy -What <name>              one heavy-job slot for the life of this process, waiting while none is free or the machine is held alone (below)
#   Get-DTCommonDir <root>                   the git common dir of a tree (shared by every worktree), or $null when the tree is not a git checkout
#   & Tools\Machine.ps1 -DTRunFile <file> [args]   runs a batch file (or any command) under the mask and inside a job that holds it: the form the .bat entry points use
# The mask file is <git common dir>\dt-locks\affinity.txt: one hexadecimal mask on one line. It is a fact about one machine and is not in the repository.
# No file, or a tree that is not a git checkout: nothing changes and the mask reads "none". A file that does not parse, or a mask that leaves no
# processor of this machine in: a terminating error, which the caller's trap turns into its FAIL line.
# $env:DT_AFFINITY_FILE, or -Path, names another mask file (Tools/tools_test.py uses it, so no test depends on this PC's file).
param(
	[string]$DTRunFile = "",
	[Parameter(ValueFromRemainingArguments = $true)]
	[string[]]$DTRunArgs = @()
)

function Get-DTCommonDir([string]$Root) {
	if (-not (Get-Command git -ErrorAction SilentlyContinue)) { return $null }
	$Dir = & git -C $Root rev-parse --path-format=absolute --git-common-dir 2>$null
	if ($LASTEXITCODE -ne 0 -or -not $Dir) { return $null }
	return ([string]$Dir).Trim() -replace '/', '\'
}

function Get-DTMaskFile {
	if ($env:DT_AFFINITY_FILE) { return $env:DT_AFFINITY_FILE }
	$Common = Get-DTCommonDir (Join-Path $PSScriptRoot "..")
	if (-not $Common) { return $null }
	return Join-Path $Common "dt-locks\affinity.txt"
}

function Set-DTAffinity([string]$Path = "") {
	if (-not $Path) { $Path = Get-DTMaskFile }
	if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return "none" }
	$Lines = @(Get-Content -LiteralPath $Path | Where-Object { $_.Trim() })
	$Text = if ($Lines.Count -eq 1) { $Lines[0].Trim() -replace '^0[xX]', '' } else { "" }
	if ($Text -notmatch '^[0-9A-Fa-f]{1,16}$') { throw "mask file $Path does not hold one hexadecimal mask on one line" }
	$Mask = [Convert]::ToUInt64($Text, 16)
	$Count = [Environment]::ProcessorCount
	$System = if ($Count -ge 64) { [UInt64]::MaxValue } else { ([UInt64]1 -shl $Count) - 1 }
	$Usable = $Mask -band $System
	if ($Usable -eq 0) { throw "mask $Text in $Path leaves no processor of this machine ($Count logical processors)" }
	[System.Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity = [IntPtr][Int64]$Usable
	return $Usable.ToString("X")
}

# --- heavy jobs (D-055 item 7, 07_Technical R-TEC-18) ----------------------------------------------------------------------------------------------
# A heavy job is a compiler run, an engine or game launch, a commandlet, a cook, a package or a Blender render. Every Tools entry script that starts one
# calls Enter-DTHeavy right after Set-DTAffinity:
#   $DTHeavy = Enter-DTHeavy -What <name> [-Alone <token>]
# It takes one of N slot files <git common dir>\dt-locks\heavy\slot<k>.lock, opened exclusively and held for the life of the process (a dead process frees
# its slot by itself), and writes who holds it beside it (slot<k>.txt: PID, worktree, script, what, since). N is the one line of dt-locks\slots.txt,
# DTHeavySlotsDefault when the file is missing. While no slot is free it waits, one line a minute naming the holders.
# The alone hold (Tools\Alone.ps1): while dt-locks\alone.txt exists, Enter-DTHeavy waits, naming its holder, unless the caller carries the file's token
# (-Alone, or $env:DT_ALONE_TOKEN). A file without a token line (written by hand) blocks everyone until it is deleted.
# A Tools script started by one that already holds a slot takes no second one and never waits ($env:DT_HEAVY_HELD is inherited), so a whole gate is one
# heavy job and a step never waits on its own gate. A wait longer than DTHeavyWaitMaxMinutes fails with a HEAVY line instead of hanging.
# A tree that is not a git checkout has no lock directory: nothing is taken and the result reads "none". $env:DT_LOCK_DIR names another lock directory (tests).
$DTHeavySlotsDefault = 4
$DTHeavyWaitMaxMinutes = 120

function Get-DTLockDir {
	if ($env:DT_LOCK_DIR) { return $env:DT_LOCK_DIR }
	$Common = Get-DTCommonDir (Join-Path $PSScriptRoot "..")
	if (-not $Common) { return $null }
	return Join-Path $Common "dt-locks"
}

function Get-DTSlotCount([string]$LockDir) {
	$Path = Join-Path $LockDir "slots.txt"
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $DTHeavySlotsDefault }
	$Lines = @(Get-Content -LiteralPath $Path | Where-Object { $_.Trim() })
	if ($Lines.Count -ne 1 -or $Lines[0].Trim() -notmatch '^[1-9][0-9]*$') { throw "slot file $Path does not hold one positive whole number on one line" }
	return [int]$Lines[0].Trim()
}

function Read-DTAlone([string]$LockDir) {
	# $null when the machine is not held; else the text and its token ("" for a file written by hand without one).
	$Path = Join-Path $LockDir "alone.txt"
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
	$Text = @(Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue)
	$Token = ""
	foreach ($Line in $Text) { if ($Line -match '^\s*token:\s*(\S+)\s*$') { $Token = $Matches[1] } }
	$First = @($Text | Where-Object { $_.Trim() } | Select-Object -First 2) -join " / "
	return @{ Token = $Token; Text = $First }
}

function Get-DTSlotHolders([string]$LockDir, [int]$Count) {
	$Held = @()
	for ($K = 1; $K -le $Count; $K++) {
		$Lock = Join-Path $LockDir "heavy\slot$K.lock"
		if (-not (Test-Path -LiteralPath $Lock)) { continue }
		try { $S = [System.IO.File]::Open($Lock, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None); $S.Dispose(); continue }
		catch [System.IO.IOException] { }
		$Who = Join-Path $LockDir "heavy\slot$K.txt"
		$Held += "slot $K " + $(if (Test-Path -LiteralPath $Who) { (Get-Content -LiteralPath $Who -Raw -ErrorAction SilentlyContinue).Trim() } else { "(holder unknown)" })
	}
	return $Held
}

function Open-DTSlots([string]$LockDir, [int]$Count, [switch]$All) {
	# One free slot (or, with -All, every slot at once) as open FileStreams; @() when that cannot be had now.
	$Dir = Join-Path $LockDir "heavy"
	New-Item -ItemType Directory -Force $Dir | Out-Null
	$Got = @()
	for ($K = 1; $K -le $Count; $K++) {
		try {
			$Got += @{ K = $K; Stream = [System.IO.File]::Open((Join-Path $Dir "slot$K.lock"), [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None) }
			if (-not $All) { return $Got }
		}
		catch [System.IO.IOException] {
			if ($All) { $Got | ForEach-Object { $_.Stream.Dispose() }; return @() }
		}
	}
	if ($All) { return $Got }
	return @()
}

function Enter-DTHeavy([string]$What = "", [string]$Alone = "") {
	if ($env:DT_HEAVY_HELD) { return "inherited ($env:DT_HEAVY_HELD)" }
	$LockDir = Get-DTLockDir
	if (-not $LockDir) { return "none" }
	if (-not $Alone) { $Alone = $env:DT_ALONE_TOKEN }
	$Count = Get-DTSlotCount $LockDir
	$Script = if ($MyInvocation.PSCommandPath) { Split-Path -Leaf $MyInvocation.PSCommandPath } else { "(a command line)" }
	if (-not $What) { $What = $Script }
	$MaxS = if ($env:DT_HEAVY_WAIT_MAX_S) { [double]$env:DT_HEAVY_WAIT_MAX_S } else { $DTHeavyWaitMaxMinutes * 60 }
	$Start = Get-Date
	$Said = $null
	while ($true) {
		$Why = $null
		$Hold = Read-DTAlone $LockDir
		if ($Hold -and -not ($Hold.Token -and $Alone -eq $Hold.Token)) { $Why = "the machine is held alone: $($Hold.Text)" }
		else {
			$Got = @(Open-DTSlots $LockDir $Count)
			if ($Got.Count) {
				# The hold is read again with the slot in hand, so an alone hold taken in between is never missed.
				$Hold = Read-DTAlone $LockDir
				if ($Hold -and -not ($Hold.Token -and $Alone -eq $Hold.Token)) { $Got[0].Stream.Dispose(); $Why = "the machine is held alone: $($Hold.Text)" }
				else {
					$K = $Got[0].K
					$Info = "PID $PID, worktree $((Resolve-Path (Join-Path $PSScriptRoot '..')).Path), script $Script, what $What, since $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
					Set-Content -LiteralPath (Join-Path $LockDir "heavy\slot$K.txt") -Value $Info -Encoding ascii
					if (-not $global:DTHeavyStreams) { $global:DTHeavyStreams = @() }
					$global:DTHeavyStreams += $Got[0].Stream  # held for the life of this process
					$env:DT_HEAVY_HELD = "slot $K of $Count, PID $PID"
					return "slot $K of $Count"
				}
			}
			else { $Why = "all $Count heavy slots are taken: " + ((Get-DTSlotHolders $LockDir $Count) -join "; ") }
		}
		$Waited = ((Get-Date) - $Start).TotalSeconds
		if ($Waited -ge $MaxS) { throw ("HEAVY: FAIL ({0} waited {1:0} s, the limit; {2})" -f $What, $Waited, $Why) }
		if (-not $Said -or ((Get-Date) - $Said).TotalSeconds -ge 60) { Write-Host ("heavy: {0} waits ({1:0} s so far): {2}" -f $What, $Waited, $Why); $Said = Get-Date }
		Start-Sleep -Seconds 2
	}
}

if ($DTRunFile) {
	# A batch file cannot set its own affinity: it runs again as a child of this process, which holds the mask. DT_MASKED_BAT tells the batch file it is the
	# masked run, so it does not come back here.
	trap { Write-Host "MACHINE: FAIL ($_)"; exit 1 }
	# The batch file's commandlet starts engine threads that ask for every processor, which a process mask alone lets them have (S1_gate_main.md s5):
	# this process joins a job that carries the mask as its affinity limit (Tools/HiddenLaunch.ps1, Hold-DTProcess), and the batch file and all it
	# starts are born in it. Dot-sourcing the helper runs this file again with no arguments, so the run is kept first.
	$Run = @{ File = $DTRunFile; Args = @($DTRunArgs) }
	. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
	$null = Hold-DTProcess
	$null = Enter-DTHeavy -What (Split-Path -Leaf $Run.File)  # a batch entry (Build, Localize, CreateArena) is a heavy job
	$env:DT_MASKED_BAT = (Resolve-Path -LiteralPath $Run.File).Path
	$RunArgs = $Run.Args
	& $Run.File @RunArgs
	exit $LASTEXITCODE
}
