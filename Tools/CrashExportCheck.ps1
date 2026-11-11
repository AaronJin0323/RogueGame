# The export's redaction on a real crash (package.md R-PKG-11, T-PKG-15; review #35, #46). Needs Tools\Package.ps1 first. Opens no window.
#   & Tools\CrashExportCheck.ps1 [-Timeout 180] [-Export <path to an Export_Logs.ps1>]
# The Development package's game is crashed on purpose by the engine's own `debug crash` (a console command the Shipping build does not
# have; nothing was added to the game for this), under -unattended, so no dialog is asked for, and under a -userdir laid out like an
# account: Saved\CrashExport\account\Dusktracer. The engine writes the crash folder under that -userdir's Saved\Crashes. The check then
# reads the ids the real CrashContext.runtime-xml carries (MachineId, LoginId, EpicAccountId, DeviceId), runs Export_Logs.ps1 with
# %LOCALAPPDATA% pointed at that account for the export alone, and looks for each id's value in every file of the zip, as UTF-8 and UTF-16.
# Refuses to crash anything when the package carries a crash reporter (it would send the report) or when it cannot see where the crash
# folder will land. Nothing here reads, writes or lists %LOCALAPPDATA%\Dusktracer: the crash folder is found under the -userdir, or the check fails.
# -Export runs another copy of the export script on the same crash, for a fail-before reading of an older rule.
# Prints "CRASHEXPORT: PASS (...)" or "CRASHEXPORT: FAIL (...)"; exit 0 / 1, 2 when there is no package, it is not this tree's (its stamp: dirty, partial, stale) or the crash was not staged.
param(
	[int]$Timeout = 180,
	[string]$Export = ""
)

trap { Write-Host "CRASHEXPORT: FAIL (script error: $_)"; exit 1 }

. (Join-Path $PSScriptRoot "Machine.ps1"); $DTMask = Set-DTAffinity  # D-052: first, so every child inherits the processor mask
$DTHeavy = Enter-DTHeavy -What "CrashExportCheck"  # D-055 item 7: one heavy-job slot for this run, none for a step of a run that holds one

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$DevDir = Join-Path $Root "Saved\Package\Development\Windows"
$Stub = Join-Path $DevDir "Dusktracer.exe"
$WorkDir = Join-Path $Root "Saved\CrashExport"
$Account = Join-Path $WorkDir "account"
$UserDir = Join-Path $Account "Dusktracer"
$Crashes = Join-Path $UserDir "Saved\Crashes"
if (-not $Export) { $Export = Join-Path $PSScriptRoot "PackageFiles\Export_Logs.ps1" }
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
if (-not (Test-Path $Stub)) { Write-Host "CRASHEXPORT: NOT RUN (no $Stub; Tools\Package.ps1)"; exit 2 }
# A crash reporter in the package would be started by the crash and, unattended, would send the report. This package ships none.
$Reporter = @(Get-ChildItem $DevDir -Recurse -File -Filter "CrashReportClient*.exe" -ErrorAction SilentlyContinue)
if ($Reporter.Count) { Write-Host "CRASHEXPORT: NOT RUN (the package carries $($Reporter[0].Name): a staged crash would send a report)"; exit 2 }
# Which package (R-PKG-08), as Tools\ExportLogsCheck.ps1 asks: an old package's crash would pass or fail for a tree that is not this one.
# The export script is read from this tree, not from the package, so Tools/PackageFiles is not part of what makes the package stale here.
$StampFile = Join-Path $Root "Saved\Package\package_stamp.json"
if (-not (Test-Path $StampFile)) { Write-Host "CRASHEXPORT: NOT RUN (no package stamp; Tools\Package.ps1)"; exit 2 }
$Stamp = [IO.File]::ReadAllText($StampFile) | ConvertFrom-Json
if ($Stamp.dirty) { Write-Host "CRASHEXPORT: NOT RUN (package $($Stamp.commit) came from a dirty tree)"; exit 2 }
if (@($Stamp.configs) -notcontains "Development" -or $Stamp.skipcook) { Write-Host "CRASHEXPORT: NOT RUN (package $($Stamp.commit) came from a partial run: $(@($Stamp.configs) -join '+'), skipcook=$([bool]$Stamp.skipcook))"; exit 2 }
$Changed = @(& git -C $Root diff --name-only "$($Stamp.commit)" HEAD -- Source Content Config Dusktracer.uproject)
if ($LASTEXITCODE -ne 0) { Write-Host "CRASHEXPORT: NOT RUN (package commit $($Stamp.commit) is not in this history)"; exit 2 }
if ($Changed.Count -gt 0) { Write-Host "CRASHEXPORT: NOT RUN (package $($Stamp.commit) is stale, $($Changed.Count) file(s) changed since)"; exit 2 }

$script:Failures = @()
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; $script:Failures += $Text }

$Staged = Test-Path $Crashes
if (-not $Export.StartsWith($PSScriptRoot, [StringComparison]::OrdinalIgnoreCase) -and $Staged) {
	Write-Host "crashexport: reading the crash already staged under $Crashes"
}
else {
	if (Test-Path $WorkDir) { Remove-Item $WorkDir -Recurse -Force }
	New-Item -ItemType Directory -Force $UserDir | Out-Null
	$Log = Join-Path $WorkDir "crash_launch.log"
	$ArgList = @("/Game/DT/Maps/L_Arena_Test", "-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-unattended", "-nopause", "-nosound", "-userdir=`"$UserDir`"", "-culture=en",
		"-ABSLOG=`"$Log`"", "-DTSeed=770099", "-ExecCmds=`"debug crash`"")
	$Proc = Start-DTHidden $Stub $ArgList
	if (-not $Proc.WaitForExit($Timeout * 1000)) { Stop-DTHidden $Proc; Write-Host "CRASHEXPORT: FAIL (the crash launch did not end in $Timeout s)"; exit 1 }
	Close-DTHidden
	Write-Host "crashexport: the game ended with exit code $($Proc.ExitCode)"
}

$Contexts = @(Get-ChildItem $Crashes -Recurse -File -Filter "CrashContext.runtime-xml" -ErrorAction SilentlyContinue)
if ($Contexts.Count -eq 0) { Write-Host "CRASHEXPORT: FAIL (no CrashContext.runtime-xml under ${Crashes}: the launch did not crash, or the crash went elsewhere)"; exit 1 }
$Raw = [IO.File]::ReadAllText($Contexts[0].FullName)
if ($Raw -notmatch "<CrashType>Crash</CrashType>|<IsAssert>|<ErrorMessage>") { Note-Fail "the context does not read as a crash" }
# The ids as the engine wrote them. Their values stay in this process: no line of this check prints one.
$Ids = @{}
foreach ($Key in @("MachineId", "LoginId", "EpicAccountId", "DeviceId")) {
	if ($Raw -match "<$Key>([^<]+)</$Key>") { $Ids[$Key] = $Matches[1] }
}
foreach ($Key in @("MachineId", "LoginId")) { if (-not $Ids.ContainsKey($Key)) { Note-Fail "the real context holds no $Key value: this proves less" } }
$CrashFiles = @(Get-ChildItem $Crashes -Recurse -File)
Write-Host ("    crash folder: {0} file(s) [{1}]; ids with a value: {2}" -f $CrashFiles.Count, (($CrashFiles | ForEach-Object { $_.Name } | Sort-Object -Unique) -join " "), (($Ids.Keys | Sort-Object) -join " "))

$Tag = if ($Export.StartsWith($PSScriptRoot, [StringComparison]::OrdinalIgnoreCase)) { "out" } else { "out_other" }
$OutDir = Join-Path $WorkDir $Tag
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
$OwnLocal = $env:LOCALAPPDATA
$env:LOCALAPPDATA = $Account
try { $Said = & powershell -NoProfile -ExecutionPolicy Bypass -File $Export -OutDir $OutDir 2>&1 }
finally { $env:LOCALAPPDATA = $OwnLocal }
$Verdict = ($Said | Where-Object { "$_" -like "EXPORT:*" } | Select-Object -Last 1)
Write-Host "    $Verdict"
$Zip = Get-ChildItem $OutDir -Filter "Dusktracer_Logs_*.zip" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $Zip) { Write-Host "CRASHEXPORT: FAIL (no zip: $($Said -join ' '))"; exit 1 }
$Open = Join-Path $OutDir "open"
Expand-Archive -LiteralPath $Zip.FullName -DestinationPath $Open
$All = @(Get-ChildItem $Open -Recurse -File)
$Carried = @($All | Where-Object { $_.Name -eq "CrashContext.runtime-xml" })
if ($Carried.Count -eq 0) { Note-Fail "the zip does not carry the crash context" }
else {
	$Text = [IO.File]::ReadAllText($Carried[0].FullName)
	foreach ($Key in $Ids.Keys) { if ($Text -notmatch "<$Key><id></$Key>") { Note-Fail "the exported context does not read <$Key><id></$Key>" } }
}
if (@($All | Where-Object { $_.Extension -in ".dmp", ".mdmp" }).Count) { Note-Fail "the zip carries a dump" }
$Names = @($env:USERNAME; (Split-Path $env:USERPROFILE -Leaf); $env:COMPUTERNAME) | Where-Object { $_ } | Sort-Object -Unique
$Leaks = 0
foreach ($File in $All) {
	$Bytes = [IO.File]::ReadAllBytes($File.FullName)
	$Text = [Text.Encoding]::UTF8.GetString($Bytes) + [Text.Encoding]::Unicode.GetString($Bytes)
	foreach ($Key in $Ids.Keys) {
		if ($Text.IndexOf($Ids[$Key], [StringComparison]::OrdinalIgnoreCase) -ge 0) { $Leaks++; Note-Fail "$($File.Name) still holds the $Key value" }
	}
	foreach ($Name in $Names) { if ($Text.IndexOf($Name, [StringComparison]::OrdinalIgnoreCase) -ge 0) { $Leaks++; Note-Fail "$($File.Name) still holds a user or machine name" } }
}

$Summary = "a real crash, {0} file(s) in its folder, {1} id value(s) read from it [{2}], {3} file(s) in the zip, {4} leak(s)" -f $CrashFiles.Count, $Ids.Count, (($Ids.Keys | Sort-Object) -join " "), $All.Count, $Leaks
if ($script:Failures.Count) { Write-Host "CRASHEXPORT: FAIL ($Summary; first: $($script:Failures[0]))"; exit 1 }
Write-Host "CRASHEXPORT: PASS ($Summary)"
exit 0
