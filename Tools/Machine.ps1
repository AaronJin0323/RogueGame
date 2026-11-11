# Machine facts (D-052): the processor mask of this PC, applied by every Tools entry script as its first statement, so the compiler, the engine,
# the bots and Python all inherit it.
#   . (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity
#   Set-DTAffinity [-Path <mask file>]       sets this process's affinity to the mask; returns it as text ("FFFFFBFE"), or "none"
#   Get-DTCommonDir <root>                   the git common dir of a tree (shared by every worktree), or $null when the tree is not a git checkout
#   & Tools\Machine.ps1 -DTRunFile <file> [args]   runs a batch file (or any command) under the mask: the form the .bat entry points use
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

if ($DTRunFile) {
	# A batch file cannot set its own affinity: it runs again as a child of this process, which holds the mask. DT_MASKED_BAT tells the batch file it is the
	# masked run, so it does not come back here.
	trap { Write-Host "MACHINE: FAIL ($_)"; exit 1 }
	$null = Set-DTAffinity
	$env:DT_MASKED_BAT = (Resolve-Path -LiteralPath $DTRunFile).Path
	& $DTRunFile @DTRunArgs
	exit $LASTEXITCODE
}
