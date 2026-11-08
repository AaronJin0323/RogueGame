# Log hand-over proved on a real package (package.md R-PKG-11, T-PKG-11). Needs Tools\Package.ps1 first. Opens no window.
#   & Tools\ExportLogsCheck.ps1 [-Seed 770088] [-BotSeconds 15] [-Timeout 300] [-Keep]
# A Shipping bot run from the package, then a Development bot run from a copy under %TEMP% (inside the user profile, as a tester's Desktop or
# Downloads is, in a folder with a space), so the real log carries the real user name. The copy's Export_Logs.bat runs through cmd exactly as
# a double-click does, with no game process appearing. The zip is unpacked and read: both runs equal their source files byte for byte,
# the log came along and lost every name, power_split.py and pressure.py read the exported runs.
# Every game launch runs on the hidden desktop of Tools/HiddenLaunch.ps1 with a -userdir (R-TEC-17): Shipping's under Saved\ExportCheck\account, laid out like an
# account so the export finds it with %LOCALAPPDATA% pointed there, and the copy's at its own Dusktracer folder, where it writes anyway. Nothing here writes to or
# deletes from %LOCALAPPDATA%\Dusktracer, where a person's runs live. The export's cmd.exe is no engine binary and starts no game (tools_test.py LAUNCH_EXCEPTIONS).
# Prints "EXPORTCHECK: PASS (...)" or "EXPORTCHECK: FAIL (...)"; exit 0 / 1, 2 when there is no package. Work files: Saved\ExportCheck (the zip is kept).
param(
	[int]$Seed = 770088,
	[int]$BotSeconds = 15,
	[int]$Timeout = 300,
	[switch]$Keep
)

trap { Write-Host "EXPORTCHECK: FAIL (script error: $_)"; exit 1 }

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$PackageRoot = Join-Path $Root "Saved\Package"
$ShipDir = Join-Path $PackageRoot "Shipping\Windows"
$DevDir = Join-Path $PackageRoot "Development\Windows"
$ShipExe = Join-Path $ShipDir "Dusktracer\Binaries\Win64\Dusktracer-Win64-Shipping.exe"
$WorkDir = Join-Path $Root "Saved\ExportCheck"
$Account = Join-Path $WorkDir "account"
$ShipUser = Join-Path $Account "Dusktracer"
$ShipRuns = Join-Path $ShipUser "Saved\Playtests"
$Copy = Join-Path $env:TEMP "DT Export Check $PID"
$DevExe = Join-Path $Copy "Dusktracer\Binaries\Win64\Dusktracer.exe"
$DevUser = Join-Path $Copy "Dusktracer"
$DevSaved = Join-Path $DevUser "Saved"
. (Join-Path $PSScriptRoot "HiddenLaunch.ps1")
$Map = "/Game/DT/Maps/L_Arena_Test"
foreach ($File in $ShipExe, (Join-Path $DevDir "Export_Logs.bat")) {
	if (-not (Test-Path $File)) { Write-Host "EXPORTCHECK: NOT RUN (no $File; Tools\Package.ps1)"; exit 2 }
}

$script:Failures = @()
function Note-Fail([string]$Text) { Write-Host "  ! $Text"; $script:Failures += $Text }

function Invoke-Game([string]$Exe, [string]$Tag, [string]$UserDir) {
	$ArgList = @($Map, "-windowed", "-RenderOffScreen", "-nosplash", "-ResX=1280", "-ResY=720", "-unattended", "-nopause", "-userdir=`"$UserDir`"", "-culture=en", "-DTSeed=$Seed",
		"-DTOfferAutoPick", "-DTBot=Steered", "-DTBotMaxSeconds=$BotSeconds", "-DTBotQuit")
	$Proc = Start-DTHidden $Exe $ArgList
	if (-not $Proc.WaitForExit($Timeout * 1000)) { Stop-DTHidden $Proc; Note-Fail "$Tag timed out" }
	Write-Host "exportcheck: $Tag run done"
}

function Get-Run([string]$Dir, [string]$Extension) { return Get-ChildItem $Dir -Filter "${Seed}_*.$Extension" -ErrorAction SilentlyContinue | Select-Object -First 1 }

# Every way this machine's names could still read, in any file: as UTF-8 and as UTF-16.
$Names = @($env:USERNAME; (Split-Path $env:USERPROFILE -Leaf); $env:COMPUTERNAME) | Where-Object { $_ } | Sort-Object -Unique
function Find-Names([string]$Path) {
	$Bytes = [IO.File]::ReadAllBytes($Path)
	$Text = [Text.Encoding]::UTF8.GetString($Bytes) + [Text.Encoding]::Unicode.GetString($Bytes)
	$Hits = @($Names | Where-Object { $Text.IndexOf($_, [StringComparison]::OrdinalIgnoreCase) -ge 0 })
	if ($Text -match '(?i)[\\/]Users[\\/]+(?!<user>)[^\\/\s"]') { $Hits += "a profile path" }
	return $Hits
}

New-Item -ItemType Directory -Force $WorkDir | Out-Null
Remove-Item (Join-Path $WorkDir "*") -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $ShipUser | Out-Null
$Summary = ""
try {
	# --- the two runs ----------------------------------------------------------------------------------------------------
	Invoke-Game $ShipExe "Shipping" $ShipUser
	$ShipCsv = Get-Run $ShipRuns "csv"
	$ShipJsonl = Get-Run $ShipRuns "jsonl"
	if (-not $ShipCsv -or -not $ShipJsonl) { Note-Fail "Shipping left no run files" }

	& robocopy $DevDir $Copy /E /NFL /NDL /NJH /NJS /NP | Out-Null
	if ($LASTEXITCODE -ge 8) { throw "robocopy rc $LASTEXITCODE" }
	Remove-Item (Join-Path $DevSaved "*") -Recurse -Force -ErrorAction SilentlyContinue
	Invoke-Game $DevExe "Development" $DevUser
	$DevRuns = Join-Path $DevSaved "Playtests"
	$DevCsv = Get-Run $DevRuns "csv"
	$DevJsonl = Get-Run $DevRuns "jsonl"
	if (-not $DevCsv -or -not $DevJsonl) { Note-Fail "Development left no run files" }
	$RawLog = Join-Path $DevSaved "Logs\Dusktracer.log"
	if (-not (Test-Path $RawLog)) { throw "no game log at $RawLog" }
	$Ids = @(Select-String -Path $RawLog -Pattern 'MachineId=([0-9a-fA-F]{8,})' | ForEach-Object { $_.Matches[0].Groups[1].Value })
	if ($Ids.Count -eq 0) { Note-Fail "the raw log holds no MachineId: this proves less" }
	$Names = @($Names + $Ids | Sort-Object -Unique)
	$RawHits = @(Find-Names $RawLog)
	$RawCount = ([regex]::Matches([IO.File]::ReadAllText($RawLog), [regex]::Escape($env:USERNAME), "IgnoreCase")).Count
	if ($RawCount -eq 0) { Note-Fail "the raw log never names the user: this proves nothing" }
	Write-Host ("    raw log {0:n0} bytes, user name {1} times, names found: {2}" -f (Get-Item $RawLog).Length, $RawCount, ($RawHits.Count))

	# --- the export, as a double-click runs it ---------------------------------------------------------------------------
	$OutDir = Join-Path $WorkDir "out"
	$Nul = Join-Path $WorkDir "stdin.txt"
	$Said = Join-Path $WorkDir "said.txt"
	[IO.File]::WriteAllText($Nul, "`r`n")
	$Line = "/d /c `"`"$(Join-Path $Copy 'Export_Logs.bat')`" -OutDir `"$OutDir`"`""
	# The export reads %LOCALAPPDATA%\Dusktracer\Saved: pointed at the check's account for this child alone, so it reads Shipping's run there and never a person's.
	$OwnLocal = $env:LOCALAPPDATA
	$env:LOCALAPPDATA = $Account
	try { $Proc = Start-Process -FilePath "cmd.exe" -ArgumentList $Line -PassThru -WindowStyle Hidden -RedirectStandardInput $Nul -RedirectStandardOutput $Said }
	finally { $env:LOCALAPPDATA = $OwnLocal }
	$Games = 0
	while (-not $Proc.HasExited) {
		$Games += @(Get-Process -Name "Dusktracer*" -ErrorAction SilentlyContinue | Where-Object { $_.Path -and ($_.Path.StartsWith($Copy) -or $_.Path.StartsWith($PackageRoot)) }).Count
		Start-Sleep -Milliseconds 100
	}
	if ($Games) { Note-Fail "the export started a game ($Games sample(s))" }
	$Report = [IO.File]::ReadAllText($Said, [Text.Encoding]::UTF8)
	$Verdict = ($Report -split "`r?`n" | Where-Object { $_ -like "EXPORT:*" } | Select-Object -Last 1)
	Write-Host "    $Verdict"
	$Zip = Get-ChildItem $OutDir -Filter "Dusktracer_Logs_*.zip" -ErrorAction SilentlyContinue | Select-Object -First 1
	if (-not $Zip) { throw "no zip: $Report" }
	if ($Verdict -notmatch [regex]::Escape($Zip.FullName)) { Note-Fail "the report does not say where the file went: $Verdict" }

	# --- what came out ---------------------------------------------------------------------------------------------------
	$Open = Join-Path $WorkDir "open"
	Expand-Archive -LiteralPath $Zip.FullName -DestinationPath $Open
	$All = @(Get-ChildItem $Open -Recurse -File)
	foreach ($Source in @($ShipCsv, $ShipJsonl, $DevCsv, $DevJsonl) | Where-Object { $_ }) {
		$Got = $All | Where-Object { $_.Name -eq $Source.Name } | Select-Object -First 1
		if (-not $Got) { Note-Fail "$($Source.Name) is not in the zip" }
		elseif ((Get-FileHash $Got.FullName).Hash -ne (Get-FileHash $Source.FullName).Hash) { Note-Fail "$($Source.Name) changed in the zip" }
	}
	if (@($All | Where-Object { $_.Name -eq "MANIFEST.txt" }).Count -ne 1) { Note-Fail "no MANIFEST.txt" }
	$Log = $All | Where-Object { $_.Name -eq "Dusktracer.log" } | Select-Object -First 1
	if (-not $Log) { Note-Fail "the game log is not in the zip" }
	else {
		$Text = [IO.File]::ReadAllText($Log.FullName)
		if ($Text -notmatch "LogDT: Run start") { Note-Fail "the exported log has no Run start" }
		if ($Text -notmatch "<user>") { Note-Fail "the exported log has no <user>" }
	}
	$Stray = @($All | Where-Object { $_.Extension -notin ".csv", ".jsonl", ".log", ".txt" })
	if ($Stray.Count) { Note-Fail "the zip carries $($Stray[0].Name)" }

	$Dirty = 0
	foreach ($File in $All) {
		$Hits = @(Find-Names $File.FullName)
		if ($Hits.Count) { $Dirty++; Note-Fail "$($File.Name) still holds: $($Hits -join ', ')" }
		if (@($Names | Where-Object { $File.FullName.Substring($Open.Length).IndexOf($_, [StringComparison]::OrdinalIgnoreCase) -ge 0 }).Count) { Note-Fail "a name in the path of $($File.Name)" }
	}
	foreach ($Run in @($All | Where-Object { $_.Name -like "${Seed}_*.jsonl" })) {
		$Split = & python -I (Join-Path $Root "Tools\power_split.py") $Run.FullName 2>&1
		$Press = & python -I (Join-Path $Root "Tools\pressure.py") $Run.FullName 2>&1
		$PressRc = $LASTEXITCODE
		$Power = ($Split | Select-String "POWER:" | Select-Object -Last 1).Line
		if (-not $Power) { Note-Fail "power_split.py read nothing from $($Run.Name)" }
		if ($PressRc -ne 0) { Note-Fail "pressure.py rc $PressRc on $($Run.Name)" }
		Write-Host "    $($Run.Name): $Power; pressure rc $PressRc"
	}
	$Summary = "zip {0:n0} KB, {1} files, {2} scanned clean, raw log named the user {3} times, 0 game processes" -f ($Zip.Length / 1KB), $All.Count, ($All.Count - $Dirty), $RawCount
}
finally {
	Close-DTHidden
	if (-not $Keep) {
		Remove-Item -LiteralPath $Copy -Recurse -Force -ErrorAction SilentlyContinue
		Remove-Item (Join-Path $WorkDir "open") -Recurse -Force -ErrorAction SilentlyContinue
	}
}

if ($script:Failures.Count) { Write-Host "EXPORTCHECK: FAIL ($($script:Failures.Count) failure(s), first: $($script:Failures[0]))"; exit 1 }
Write-Host "EXPORTCHECK: PASS ($Summary)"
exit 0
