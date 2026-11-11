# GPU semaphore (07_Technical R-TEC-10, 11_Quality s5): one lock file for the whole machine, so a measurement in one worktree sees one in any other.
#   . (Join-Path $PSScriptRoot "GpuLock.ps1")
#   $Path = Get-DTGpuLockPath <root>    <git common dir>\dt-locks\gpu.lock, which every worktree of the repository shares;
#                                       <root>\Saved\gpu.lock only when the tree is not a git checkout
#   $Lock = Open-DTGpuLock <root>       the lock as an [IO.FileStream] opened with FileShare.None, held until Dispose; $null when another process holds it
# Not %TEMP% (a session may redirect it) and not %LOCALAPPDATA%\Dusktracer (the owner's files).

. (Join-Path $PSScriptRoot "Machine.ps1")

function Get-DTGpuLockPath([string]$Root) {
	$Common = Get-DTCommonDir $Root
	$Dir = if ($Common) { Join-Path $Common "dt-locks" } else { Join-Path $Root "Saved" }
	New-Item -ItemType Directory -Force $Dir | Out-Null
	return Join-Path $Dir "gpu.lock"
}

function Open-DTGpuLock([string]$Root) {
	$Path = Get-DTGpuLockPath $Root
	try {
		return [System.IO.File]::Open($Path, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
	}
	catch [System.IO.IOException] {
		return $null
	}
}
