# Output paths (Docs/Validation/S4_outdir_root.md): a folder or file a caller names becomes an absolute path inside this tree, or is refused, before it
# reaches the engine. The engine resolves a relative path on its command line (-DTShot=, -ABSLOG=, ...) against its own binaries folder, not the tree.
#   . (Join-Path $PSScriptRoot "OutDir.ps1")
#   $OutDir = Resolve-DTOutDir $OutDir $Root [-Default "Saved\Frames"]
# Rules: empty takes -Default (the caller's own default); a relative path is joined to the root; every path is normalised ([IO.Path]::GetFullPath), and
# the result must lie inside the root, compared without regard to case and with a trailing separator (E:\Tree2\x is not inside E:\Tree). Refused, by a
# terminating error that names the path and the root: a path that climbs out (..), another drive, a UNC or device path (\\server\share, \\?\, \\.\),
# a drive-relative path (E:x) or one rooted at the current drive (\x), the root itself, and nothing at all with no default. The result has no trailing
# separator. It creates nothing and deletes nothing; it only answers. Junctions and links inside the tree are not followed.
# Free of anything of one module: the shared launchers may dot-source it as well.

function Resolve-DTOutDir([string]$Path, [string]$Root, [string]$Default = "") {
	if (-not $Root -or -not [IO.Path]::IsPathRooted($Root) -or $Root -match '^[\\/]{2}' -or $Root -match '^[A-Za-z]:(?![\\/])' -or $Root -match '^[\\/](?![\\/])') {
		throw "the tree root '$Root' is not an absolute local path"
	}
	$Base = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
	$Given = $Path
	if (-not $Path) { $Path = $Default }
	if (-not $Path) { throw "no path given and no default, for this worktree $Base" }
	if ($Path -match '^[\\/]{2}') { throw "$Given is a UNC or device path, not inside this worktree $Base" }
	if ($Path -match '^[A-Za-z]:(?![\\/])') { throw "$Path is drive-relative, not inside this worktree $Base" }
	if ($Path -match '^[\\/]') { throw "$Path is rooted at the current drive, not inside this worktree $Base" }
	$Joined = if ([IO.Path]::IsPathRooted($Path)) { $Path } else { [IO.Path]::Combine($Base, $Path) }
	try { $Full = [IO.Path]::GetFullPath($Joined).TrimEnd('\', '/') }
	catch { throw "$Path is not a valid path ($($_.Exception.Message)), for this worktree $Base" }
	if ($Full -ieq $Base) { throw "$Path is this worktree's root $Base itself, not a folder inside it" }
	if (-not $Full.StartsWith($Base + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "$Path is outside this worktree $Base" }
	return $Full
}
