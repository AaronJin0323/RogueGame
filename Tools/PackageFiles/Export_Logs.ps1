# Log hand-over (02_Systems/package.md R-PKG-06, R-PKG-11). Makes one zip and sends nothing anywhere: a person carries it. Starts no game.
#   Export_Logs.ps1 [-OutDir <folder>]
# Reads a Shipping package's %LOCALAPPDATA%\Dusktracer\Saved and a Development build's Saved beside this script: Playtests (*.csv, *.jsonl),
# byte for byte; Logs (*.log) and the text files of Crashes, with the Windows user and machine names replaced. A crash dump is binary, so it
# cannot be redacted and stays behind. Shipping writes no log (R-PKG-02), so its export holds run files alone and the manifest says so.
# Prints "EXPORT: <zip> (<n> runs, <n> logs, <n> names redacted)" or "EXPORT: FAIL (...)"; exit 0 / 1.
# This file stays ASCII: Windows PowerShell 5.1 reads a script without a BOM in the system code page. The Chinese line is escaped for that reason.
param(
	[string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
trap { Write-Host "EXPORT: FAIL ($_)"; exit 1 }
try { [Console]::OutputEncoding = [Text.Encoding]::UTF8 } catch { }

$Utf8 = New-Object System.Text.UTF8Encoding($false)
$TextCrash = @(".log", ".txt", ".xml", ".json", ".runtime-xml", ".ini")

function Read-Shared([string]$Path) {
	# The game may still hold the file open for writing.
	$Stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]"ReadWrite, Delete")
	try {
		$Copy = New-Object IO.MemoryStream
		$Stream.CopyTo($Copy)
		return , $Copy.ToArray()
	}
	finally { $Stream.Dispose() }
}

function Get-NameRule([string]$Name) {
	# A short Latin name gets word edges, or "Al" would eat "Global". Chinese has no word edges, and anything longer goes wherever it appears.
	$Escaped = [regex]::Escape($Name)
	if ($Name.Length -lt 4 -and $Name -cmatch '^[\x00-\x7F]+$') { $Escaped = "(?<![A-Za-z0-9_])$Escaped(?![A-Za-z0-9_])" }
	return New-Object regex $Escaped, "IgnoreCase"
}

$Names = @($env:USERNAME; if ($env:USERPROFILE) { Split-Path $env:USERPROFILE -Leaf }) | Where-Object { $_ } | Sort-Object -Unique
$NameRules = @($Names | ForEach-Object { Get-NameRule $_ })
$MachineRule = if ($env:COMPUTERNAME) { Get-NameRule $env:COMPUTERNAME } else { $null }
# Any profile path, under either slash, doubled in JSON or in 8.3 form, and whatever the log's encoding did to the name.
$PathRule = New-Object regex '([\\/]Users[\\/]+)[^\\/:*?"<>|\r\n]+', "IgnoreCase"
# The engine's stable per-machine and account ids; a session or crash id is fine, it names one launch. A log writes key=value, a crash
# context writes <Key>value</Key> and JSON writes "Key":"value", so the rule takes all three.
$IdRule = New-Object regex '\b((?:MachineId|DeviceId|LoginId|EpicAccountId)(?:"?\s*[=:]\s*"?|>))[0-9a-z|_-]{8,}', "IgnoreCase"
$script:Redacted = 0

function Protect-Text([string]$Text) {
	$Text = $PathRule.Replace($Text, { param($M) $script:Redacted++; $M.Groups[1].Value + "<user>" })
	$Text = $IdRule.Replace($Text, { param($M) $script:Redacted++; $M.Groups[1].Value + "<id>" })
	foreach ($Rule in $NameRules) { $Text = $Rule.Replace($Text, { param($M) $script:Redacted++; "<user>" }) }
	if ($MachineRule) { $Text = $MachineRule.Replace($Text, { param($M) $script:Redacted++; "<machine>" }) }
	return $Text
}

function Copy-Text([string]$From, [string]$To) {
	New-Item -ItemType Directory -Force (Split-Path $To) | Out-Null
	$Reader = New-Object IO.StreamReader (New-Object IO.MemoryStream (, (Read-Shared $From))), $Utf8, $true
	try { $Text = $Reader.ReadToEnd() } finally { $Reader.Dispose() }
	[IO.File]::WriteAllText($To, (Protect-Text $Text), $Utf8)
}

$Looked = @((Join-Path $env:LOCALAPPDATA "Dusktracer\Saved"), (Join-Path $PSScriptRoot "Dusktracer\Saved"))
$Saved = @($Looked | Where-Object { Test-Path -LiteralPath $_ })
$Runs = @(); $Logs = @(); $Crashes = @(); $Dumps = 0
foreach ($Dir in $Saved) {
	$Sub = Join-Path $Dir "Playtests"
	if (Test-Path -LiteralPath $Sub) { $Runs += @(Get-ChildItem -LiteralPath $Sub -File | Where-Object { $_.Extension -in ".csv", ".jsonl" }) }
	$Sub = Join-Path $Dir "Logs"
	if (Test-Path -LiteralPath $Sub) { $Logs += @(Get-ChildItem -LiteralPath $Sub -File | Where-Object { $_.Extension -eq ".log" }) }
	$Sub = Join-Path $Dir "Crashes"
	if (Test-Path -LiteralPath $Sub) {
		foreach ($File in Get-ChildItem -LiteralPath $Sub -File -Recurse) {
			if ($File.Extension -in $TextCrash) { $Crashes += @{ File = $File; Rel = $File.FullName.Substring($Sub.Length + 1) } } else { $Dumps++ }
		}
	}
}
if ($Runs.Count + $Logs.Count + $Crashes.Count -eq 0) {
	Write-Host "EXPORT: FAIL (no run files or logs; looked in $($Looked -join ', '))"
	Write-Host "There is nothing to export yet. Play one run, then export again."
	Write-Host ([regex]::Unescape("\u8fd8\u6ca1\u6709\u53ef\u5bfc\u51fa\u7684\u5185\u5bb9\u3002\u8bf7\u5148\u73a9\u4e00\u5c40\uff0c\u518d\u5bfc\u51fa\u4e00\u6b21\u3002"))
	exit 1
}

if (-not $OutDir) { $OutDir = [Environment]::GetFolderPath("Desktop") }
if (-not $OutDir) { $OutDir = Join-Path $env:USERPROFILE "Desktop" }

$Stamp = Get-Date -Format "yyyy-MM-dd_HHmmss"
$Title = "Dusktracer_Logs_$Stamp"
$Work = Join-Path ([IO.Path]::GetTempPath()) "DT${Title}_$PID"  # two exports in one second each get their own
$Bundle = Join-Path $Work $Title
New-Item -ItemType Directory -Force (Join-Path $Bundle "Playtests") | Out-Null
try {
	foreach ($File in $Runs) { [IO.File]::WriteAllBytes((Join-Path $Bundle "Playtests\$($File.Name)"), (Read-Shared $File.FullName)) }
	$Listed = @()  # what the zip holds, as the manifest names it
	$Index = 0
	foreach ($File in $Logs) {
		$Index++
		$Name = $File.Name
		if (Test-Path -LiteralPath (Join-Path $Bundle "Logs\$Name")) { $Name = "${Index}_$Name" }  # the same name in both Saved folders
		Copy-Text $File.FullName (Join-Path $Bundle "Logs\$Name")
		$Listed += "Logs/{0}  {1} bytes before redaction" -f $Name, $File.Length
	}
	$Index = 0
	foreach ($Crash in $Crashes) {
		$Index++
		Copy-Text $Crash.File.FullName (Join-Path $Bundle "Crashes\$Index\$($Crash.Rel)")
		$Listed += "Crashes/{0}/{1}" -f $Index, ((Protect-Text $Crash.Rel) -replace '\\', '/')
	}

	$Lines = @("Dusktracer run data, exported $Stamp. Nothing was sent anywhere by this tool.",
		"Playtests: gameplay numbers only: seed, build hash and build type, who drove the run (a person, a tool or a bot), culture, last input device, combat numbers per interval,",
		"where the hero was every few seconds and where each hit on the hero came from, cards offered and picked, Keeper and weapon choices, how long each card took to pick and their total per run,",
		"and the start time, UTC inside each file and local in its name, so the two show the time zone. No name, account, machine name, path or hardware.",
		([regex]::Unescape("\u5bf9\u5c40\u6587\u4ef6\u8fd8\u8bb0\u6709\u6bcf\u5f20\u724c\u7684\u9009\u62e9\u7528\u65f6\uff0c\u4ee5\u53ca\u6bcf\u5c40\u7684\u5408\u8ba1\u7528\u65f6\u3002")),
		"The save and the settings file stay on this PC.",
		"Logs and Crashes: the engine's own text, which names the graphics card, the CPU and the Windows version. The Windows user name and the",
		"machine name are replaced by <user> and <machine>, in every profile path and wherever else they appear, and the engine's machine ids by <id> ($($script:Redacted) replaced).", "")
	if ($Logs.Count -eq 0) { $Lines += @("No game log: a Shipping build writes none.", "") }
	if ($Dumps) { $Lines += @("Left behind: $Dumps crash file(s) that are not text and cannot be redacted.", "") }
	$Lines += "Files:"
	$Lines += $Runs | Sort-Object Name | ForEach-Object { "Playtests/{0}  {1} bytes" -f $_.Name, $_.Length }
	$Lines += $Listed
	[IO.File]::WriteAllLines((Join-Path $Bundle "MANIFEST.txt"), [string[]]$Lines, $Utf8)

	New-Item -ItemType Directory -Force $OutDir | Out-Null
	$Zip = Join-Path $OutDir "$Title.zip"
	Compress-Archive -LiteralPath $Bundle -DestinationPath $Zip -Force
}
finally {
	Remove-Item -LiteralPath $Work -Recurse -Force -ErrorAction SilentlyContinue
}

if (Get-Process -Name "Dusktracer*" -ErrorAction SilentlyContinue) {
	Write-Host "export: the game is still running, so its newest run is not closed; quit it and export again"
	Write-Host ([regex]::Unescape("\u6e38\u620f\u8fd8\u5728\u8fd0\u884c\uff0c\u6700\u65b0\u4e00\u5c40\u5c1a\u672a\u7ed3\u675f\uff1b\u8bf7\u5148\u9000\u51fa\u6e38\u620f\uff0c\u518d\u5bfc\u51fa\u4e00\u6b21\u3002"))
}
Write-Host ("EXPORT: {0} ({1} runs, {2} logs, {3} names redacted)" -f $Zip, @($Runs | Where-Object { $_.Extension -eq ".jsonl" }).Count, ($Logs.Count + $Crashes.Count), $script:Redacted)
Write-Host "Your logs are in the file above. Send that one file; the game sends nothing by itself. The Windows user and PC names are hidden in the logs."
Write-Host ([regex]::Unescape("\u65e5\u5fd7\u5df2\u6253\u5305\u5230\u4e0a\u9762\u8fd9\u4e2a\u6587\u4ef6\u3002\u628a\u8fd9\u4e00\u4e2a\u6587\u4ef6\u53d1\u7ed9\u6211\u4eec\u5373\u53ef\uff0c\u6e38\u620f\u4e0d\u4f1a\u81ea\u52a8\u53d1\u9001\u4efb\u4f55\u5185\u5bb9\u3002\u65e5\u5fd7\u91cc\u7684 Windows \u7528\u6237\u540d\u548c\u7535\u8111\u540d\u5df2\u9690\u53bb\u3002"))
exit 0
