# Telemetry hand-over (02_Systems/package.md R-PKG-06). Makes one zip and sends nothing anywhere: a person carries it.
#   ExportTelemetry.ps1 [-OutDir <folder>] [-IncludeLogs]
# Reads the run files of a Shipping package (%LOCALAPPDATA%\Dusktracer\Saved) and of a Development build (Saved beside this script).
# Prints "EXPORT: <zip> (<n> runs)" or "EXPORT: FAIL (...)"; exit 0 / 1.
param(
	[string]$OutDir = [Environment]::GetFolderPath("Desktop"),
	[switch]$IncludeLogs
)

$ErrorActionPreference = "Stop"
trap { Write-Host "EXPORT: FAIL ($_)"; exit 1 }

$Saved = @((Join-Path $env:LOCALAPPDATA "Dusktracer\Saved"), (Join-Path $PSScriptRoot "Dusktracer\Saved")) | Where-Object { Test-Path $_ }
$Runs = @(foreach ($Dir in $Saved) {
		$Playtests = Join-Path $Dir "Playtests"
		if (Test-Path $Playtests) { Get-ChildItem $Playtests -File | Where-Object { $_.Extension -in ".csv", ".jsonl" } }
	})
if ($Runs.Count -eq 0) { Write-Host "EXPORT: FAIL (no run files; looked in $($Saved -join ', '))"; exit 1 }

$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$Work = Join-Path ([IO.Path]::GetTempPath()) "DTExport_$Stamp"
$Bundle = Join-Path $Work "Dusktracer_Telemetry_$Stamp"
New-Item -ItemType Directory -Force (Join-Path $Bundle "Playtests") | Out-Null
try {
	$Runs | Copy-Item -Destination (Join-Path $Bundle "Playtests") -Force
	$Extra = @()
	if ($IncludeLogs) {
		Write-Host "export: logs and crash files can name the Windows user and this machine; read them before handing them over"
		foreach ($Dir in $Saved) {
			foreach ($Sub in "Logs", "Crashes") {
				$From = Join-Path $Dir $Sub
				if (Test-Path $From) {
					Copy-Item $From (Join-Path $Bundle $Sub) -Recurse -Force
					$Extra += Get-ChildItem $From -Recurse -File
				}
			}
		}
	}

	$Lines = @("Dusktracer run data. Gameplay numbers only: seed, build hash, culture, last input device, combat numbers per interval,",
		"cards offered and picked, Keeper and weapon choices, and the local start time. No name, account, machine name, path or hardware.",
		"Exported $Stamp. Files:", "")
	$Lines += $Runs | Sort-Object Name | ForEach-Object { "{0}  {1} bytes" -f $_.Name, $_.Length }
	if ($Extra.Count) { $Lines += @("", "Added with -IncludeLogs (can name the Windows user and the machine):"); $Lines += $Extra | ForEach-Object { $_.Name } }
	[IO.File]::WriteAllLines((Join-Path $Bundle "MANIFEST.txt"), [string[]]$Lines, (New-Object System.Text.UTF8Encoding($false)))

	New-Item -ItemType Directory -Force $OutDir | Out-Null
	$Zip = Join-Path $OutDir "Dusktracer_Telemetry_$Stamp.zip"
	Compress-Archive -Path $Bundle -DestinationPath $Zip -Force
}
finally {
	Remove-Item $Work -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host ("EXPORT: {0} ({1} runs)" -f $Zip, [math]::Floor(@($Runs | Where-Object { $_.Extension -eq ".jsonl" }).Count))
exit 0
