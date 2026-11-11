# The alone hold (D-050 item 4, D-055 item 7, 07_Technical R-TEC-18): the machine for one reading, with no other heavy job on it.
#   & Tools\Alone.ps1 -Take "<what>"     waits until every heavy slot is free, writes <git common dir>\dt-locks\alone.txt and prints its token
#   & Tools\Alone.ps1 -Release           removes alone.txt
#   & Tools\Alone.ps1 -Status            prints alone.txt (or that the machine is not held) and the slots in use
# While alone.txt exists, Enter-DTHeavy (Tools\Machine.ps1) lets through only a caller that carries the token: set $env:DT_ALONE_TOKEN to it in the shell
# that runs the reading. A file without a token line, as written by hand, blocks everyone until it is deleted.
# -Take holds every slot at once for the moment it writes the file, so no heavy job can start in between; it waits, one line a minute naming the holders,
# and fails after the same limit as Enter-DTHeavy. Prints "ALONE: TAKEN <token> (...)", "ALONE: RELEASED", "ALONE: HELD (...)" or "ALONE: FREE"; exit 0 / 1.
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
	if ($Existing) { Write-Host "ALONE: FAIL (already held: $($Existing.Text))"; exit 1 }
	$MaxS = if ($env:DT_HEAVY_WAIT_MAX_S) { [double]$env:DT_HEAVY_WAIT_MAX_S } else { $DTHeavyWaitMaxMinutes * 60 }
	$Start = Get-Date
	$Said = $null
	while ($true) {
		$All = @(Open-DTSlots $LockDir $Count -All)
		if ($All.Count -eq $Count) { break }
		$Waited = ((Get-Date) - $Start).TotalSeconds
		$Who = (Get-DTSlotHolders $LockDir $Count) -join "; "
		if ($Waited -ge $MaxS) { Write-Host ("ALONE: FAIL (waited {0:0} s for the heavy slots, the limit; {1})" -f $Waited, $Who); exit 1 }
		if (-not $Said -or ((Get-Date) - $Said).TotalSeconds -ge 60) { Write-Host ("alone: waits for the heavy slots ({0:0} s so far): {1}" -f $Waited, $Who); $Said = Get-Date }
		Start-Sleep -Seconds 2
	}
	try {
		$Token = [guid]::NewGuid().ToString("N").Substring(0, 12)
		$Since = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
		$Worktree = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
		@("ALONE since $Since, taken by Tools\Alone.ps1 (PID $PID, $env:USERNAME, worktree $Worktree).", "What: $Take",
			"Rule (D-050 item 4, D-055 item 7): while this file exists, only a caller with the token below starts a heavy job (Tools\Machine.ps1 Enter-DTHeavy).",
			"token: $Token") | Set-Content -LiteralPath $File -Encoding ascii
	}
	finally { $All | ForEach-Object { $_.Stream.Dispose() } }
	Write-Host "ALONE: TAKEN $Token ($Take; set `$env:DT_ALONE_TOKEN = '$Token' where the reading runs)"
	exit 0
}

$Held = Read-DTAlone $LockDir
$Slots = @(Get-DTSlotHolders $LockDir $Count)
if ($Held) {
	Get-Content -LiteralPath $File | ForEach-Object { "  $_" }
	Write-Host ("ALONE: HELD ({0}; {1})" -f $Held.Text, $(if ($Held.Token) { "token $($Held.Token)" } else { "no token: blocks everyone until deleted" }))
}
else { Write-Host "ALONE: FREE" }
Write-Host ("slots: {0} of {1} in use" -f $Slots.Count, $Count)
$Slots | ForEach-Object { "  $_" }
exit 0
