# S3 exit reader (12_Production section 2 S3 row, 02_Systems/package.md R-PKG-14, Docs/Validation/S3_kit.md): reads what validators sent back, then Tools/s3_exit.py.
#   & Tools\S3Exit.ps1 -Logs <folder> [-Build <hash>] [-PadLog <file>] [-Findings <file>]
# -Logs holds one folder per validator, V1, V2, V3, each with that validator's Dusktracer_Logs_*.zip. Run by hand after a session. Starts no game and writes nothing.
# -Build is the kit's build; without it, Saved\Package\package_stamp.json names it unless that package came from a dirty tree. With no build named, no run counts.
# -PadLog is a file holding Tools\PadCheck.ps1's last line: by default <Logs>\PadCheck.txt, which Tools\PadCheck.ps1 -Keep writes (S3_kit.md), then the gate's Saved\Logs\Gate_pad.log,
# which every later gate overwrites. -Findings defaults to Docs\Validation\findings_S3.md.
# A bot or tool run, a designer's or unstamped build and a file too old to say are refused by the fields of run_start (telemetry.md R-TEL-13).
# Prints the reader's EXIT lines and "S3EXIT: ..."; exit 0 when it ran, whatever the verdicts, 1 on a script error.
param(
	[string]$Logs = "",
	[string]$Build = "",
	[string]$PadLog = "",
	[string]$Findings = ""
)

trap { Write-Host "S3EXIT: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask

if (-not $Logs) { throw "-Logs names the folder of validators' exports" }
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Logs = (Resolve-Path -LiteralPath $Logs -ErrorAction Stop).Path
# Windows PowerShell 5.1 quotes a spaced argument without doubling a backslash before the closing quote, and python then reads the rest of the line as the path.
if ($Logs.Length -gt 3) { $Logs = $Logs.TrimEnd('\') }
if (-not $Build) {
	$StampFile = Join-Path $Root "Saved\Package\package_stamp.json"
	if (Test-Path $StampFile) {
		$Stamp = [IO.File]::ReadAllText($StampFile) | ConvertFrom-Json
		if (-not $Stamp.dirty) { $Build = $Stamp.commit }
	}
}
if (-not $PadLog) {
	$PadLog = Join-Path $Logs "PadCheck.txt"
	if (-not (Test-Path -LiteralPath $PadLog)) { $PadLog = Join-Path $Root "Saved\Logs\Gate_pad.log" }
}
if (-not $Findings) { $Findings = Join-Path $Root "Docs\Validation\findings_S3.md" }

& python -I (Join-Path $Root "Tools\s3_exit.py") $Logs "--build=$Build" "--pad-log=$PadLog" "--findings=$Findings"
exit $LASTEXITCODE
