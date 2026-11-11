# Telemetry hand-over (02_Systems/package.md R-PKG-06). Makes one zip and sends nothing anywhere: a person carries it.
#   ExportTelemetry.ps1 [-OutDir <folder>] [-IncludeLogs] [-UserDir <folder>]
# Reads the run files of a Shipping package (%LOCALAPPDATA%\Dusktracer\Saved) and of a Development build (Saved beside this script).
# -UserDir reads the Saved folder of a game started with that -userdir instead, and nothing else: Tools\PadCheck.ps1 exports its own runs that way.
# Prints "EXPORT: <zip> (<n> runs)" or "EXPORT: FAIL (...)"; exit 0 / 1.
param(
	[string]$OutDir = [Environment]::GetFolderPath("Desktop"),
	[switch]$IncludeLogs,
	[string]$UserDir = ""
)

$ErrorActionPreference = "Stop"
trap { Write-Host "EXPORT: FAIL ($_)"; exit 1 }

$From = if ($UserDir) { @(Join-Path $UserDir "Saved") } else { @((Join-Path $env:LOCALAPPDATA "Dusktracer\Saved"), (Join-Path $PSScriptRoot "Dusktracer\Saved")) }
$Saved = @($From | Where-Object { Test-Path $_ })
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
		"every hit the hero took (what dealt it, where both stood, whether it had dropped in or was charging), health restored,",
		"how many enemies dropped in, hero positions on the arena (one every few seconds and at each hit taken), cards offered and picked,",
		"Keeper and weapon choices, whether a test bot ran the run at fast speed, and five clock readings: the start time (UTC inside",
		"each file and local in its name, so the two show the time zone), the seconds each hand of cards took, the seconds the run was",
		"paused, the run's length on the clock, and the steps this PC could not keep up with.",
		"Every run in this PC's run folder is included, earlier ones too.",
		"No name, account, machine name, path or hardware.",
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
