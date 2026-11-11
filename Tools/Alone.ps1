# The alone hold (D-050 item 4, D-055 item 7, 07_Technical R-TEC-18): the machine for one reading, with no other heavy job on it.
#   & Tools\Alone.ps1 -Take "<what>"     writes <git common dir>\dt-locks\alone.txt at once (state pending), waits until every heavy slot is free, then marks it taken and prints its token
#   & Tools\Alone.ps1 -Release           removes alone.txt
#   & Tools\Alone.ps1 -Status            prints alone.txt (or that the machine is not held) and the slots in use
# While alone.txt exists, Enter-DTHeavy (Tools\Machine.ps1) lets through only a caller that carries the token: set $env:DT_ALONE_TOKEN to it in the shell
# that runs the reading. A file without a token line, as written by hand, blocks everyone until it is deleted.
# -Take claims the machine from its first moment: while pending, no new heavy job starts but one with the token, and the jobs already running end by
# themselves. It waits, one line a minute naming the holders, and removes its own file when it fails or is stopped (a hard kill: the next reader
# finds its owner gone and removes it). A second -Take fails at once while one is pending or taken. -Status says PENDING or HELD and since when.
# It fails after the same limit as Enter-DTHeavy. Prints "ALONE: TAKEN <token> (...)", "ALONE: RELEASED", "ALONE: PENDING|HELD (...)" or "ALONE: FREE"; exit 0 / 1.
param(
	[string]$Take = "",
	[switch]$Release,
	[switch]$Status
)

trap { Write-Host "ALONE: FAIL ($_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1")

$LockDir = Get-DTLockDir
if (-not $LockDir) { Write-Host "ALONE: FAIL (this tree is not a git checkout: no lock directory)"; exit 1 }
New-Item -ItemType Directory -Force $LockDir | Out-Null
$File = Join-Path $LockDir "alone.txt"
$Count = Get-DTSlotCount $LockDir

if ($Release) {
	if (Test-Path -LiteralPath $File) { Remove-Item -LiteralPath $File -Force; Write-Host "ALONE: RELEASED ($File)" } else { Write-Host "ALONE: FREE (nothing to release)" }
	exit 0
}

if ($Take) {
	$Existing = Read-DTAlone $LockDir
	if ($Existing) { Write-Host "ALONE: FAIL (already held, $($Existing.State): $($Existing.Text))"; exit 1 }
	$MaxS = if ($env:DT_HEAVY_WAIT_MAX_S) { [double]$env:DT_HEAVY_WAIT_MAX_S } else { $DTHeavyWaitMaxMinutes * 60 }
	$Token = [guid]::NewGuid().ToString("N").Substring(0, 12)
	$Worktree = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
	$Me = [System.Diagnostics.Process]::GetCurrentProcess()
	function Write-Hold([string]$State) {
		@("ALONE since $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'), taken by Tools\Alone.ps1 (PID $PID, $env:USERNAME, worktree $Worktree).", "What: $Take",
			"Rule (D-050 item 4, D-055 item 7): while this file exists, only a caller with the token below starts a heavy job (Tools\Machine.ps1 Enter-DTHeavy).",
			"state: $State since $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')", "owner: PID $PID started $($Me.StartTime.ToUniversalTime().Ticks)",
			"token: $Token") | Set-Content -LiteralPath $File -Encoding ascii
	}
	# A take is a claim from its first moment: the hold is written now, pending, so no new heavy job starts but one with the token, and the jobs
	# already running end by themselves. Waiting for a moment when every slot happens to be free, holding nothing meanwhile, can starve.
	New-Item -ItemType Directory -Force $LockDir | Out-Null
	try { $Stream = [System.IO.File]::Open($File, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None); $Stream.Dispose() }
	catch [System.IO.IOException] { Write-Host "ALONE: FAIL (already held: $((Read-DTAlone $LockDir).Text))"; exit 1 }
	$Done = $false
	$Failed = $null
	try {
		Write-Hold "pending"
		Write-Host "alone: pending $Token ($Take): no new heavy job starts without the token; waiting for the slots in use to end"
		$Start = Get-Date
		$Said = (Get-Date)
		while ($true) {
			$All = @(Open-DTSlots $LockDir $Count -All)
			if ($All.Count -eq $Count) { $All | ForEach-Object { $_.Stream.Dispose() }; break }
			$Waited = ((Get-Date) - $Start).TotalSeconds
			$Who = (Get-DTSlotHolders $LockDir $Count) -join "; "
			if ($Waited -ge $MaxS) { $Failed = "ALONE: FAIL (waited {0:0} s for the heavy slots, the limit; the pending hold is removed; {1})" -f $Waited, $Who; break }
			if (((Get-Date) - $Said).TotalSeconds -ge 60) { Write-Host ("alone: pending, waits for the heavy slots ({0:0} s so far): {1}" -f $Waited, $Who); $Said = Get-Date }
			Start-Sleep -Seconds 2
		}
		if (-not $Failed) { Write-Hold "taken"; $Done = $true }
	}
	finally {
		# A take that fails or is stopped never leaves the machine held. Only this take's own file is removed.
		if (-not $Done) { $Now = Read-DTAlone $LockDir; if ($Now -and $Now.Token -eq $Token) { Remove-Item -LiteralPath $File -Force -ErrorAction SilentlyContinue } }
	}
	if ($Failed) { Write-Host $Failed; exit 1 }
	Write-Host "ALONE: TAKEN $Token ($Take; set `$env:DT_ALONE_TOKEN = '$Token' where the reading runs)"
	exit 0
}

$Held = Read-DTAlone $LockDir
$Slots = @(Get-DTSlotHolders $LockDir $Count)
if ($Held) {
	Get-Content -LiteralPath $File | ForEach-Object { "  $_" }
	Write-Host ("ALONE: {0} ({1}; {2})" -f $(if ($Held.State -eq "pending") { "PENDING" } else { "HELD" }), $Held.Text, $(if ($Held.Token) { "token $($Held.Token)" } else { "no token: blocks everyone until deleted" }))
}
else { Write-Host "ALONE: FREE" }
Write-Host ("slots: {0} of {1} in use" -f $Slots.Count, $Count)
$Slots | ForEach-Object { "  $_" }
exit 0
