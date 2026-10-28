# Include gate (TDD s17 2026-10-19): compiles .cpp files one at a time with UBT -SingleFile, so a missing #include fails
# here instead of on main when the unity buckets regroup. -DisableUnity does not catch these (the 2026-10-19 Boss tests
# compiled there and failed alone). Compile only, no link: safe while a game holds the DLL.
#   powershell -ExecutionPolicy Bypass -File Tools\BuildNonUnity.ps1 [-All] [-Base main] [-Files a.cpp,b.cpp]
# Default: the .cpp files changed against -Base (committed, staged, unstaged and untracked), plus the same-named .cpp of
# every changed .h. -All: every Source/**/*.cpp (~5-10 s each). Exit 0 = NON-UNITY: PASS, 1 = a file failed or warned.
param(
	[switch]$All,
	[string]$Base = "main",
	[string[]]$Files = @()
)

$ErrorActionPreference = "Stop"
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "E:\UE5\UE_5.8" }
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Project = Join-Path $Root "RogueHero.uproject"
$BuildBat = Join-Path $UERoot "Engine\Build\BatchFiles\Build.bat"

function Get-ChangedSources {
	Push-Location $Root
	try {
		$Names = @(git diff --name-only "$Base...HEAD") + @(git diff --name-only HEAD) + @(git ls-files --others --exclude-standard)
	}
	finally { Pop-Location }
	$Out = @()
	foreach ($N in ($Names | Where-Object { $_ -match '^Source/.*\.(cpp|h)$' } | Sort-Object -Unique)) {
		$Cpp = if ($N -like '*.h') { $N -replace '\.h$', '.cpp' } else { $N }
		$Full = Join-Path $Root $Cpp
		if (Test-Path $Full) { $Out += (Resolve-Path $Full).Path }
	}
	return @($Out | Sort-Object -Unique)
}

if ($Files.Count -gt 0) {
	# Via -File (and the .bat) a comma list arrives as one string: split it here.
	$Targets = @($Files | ForEach-Object { $_ -split '[,;]' } | Where-Object { $_.Trim() } | ForEach-Object { (Resolve-Path $_.Trim()).Path })
}
elseif ($All) {
	$Targets = @(Get-ChildItem (Join-Path $Root "Source") -Recurse -Filter *.cpp | ForEach-Object { $_.FullName } | Sort-Object)
}
else {
	$Targets = Get-ChangedSources
}

if ($Targets.Count -eq 0) {
	Write-Host "NON-UNITY: PASS (no .cpp changed against $Base; use -All for every file)"
	exit 0
}

Write-Host "---- Non-unity include gate: $($Targets.Count) file(s), one UBT -SingleFile call each ----"
$Failed = @()
$i = 0
foreach ($F in $Targets) {
	$i++
	$Rel = $F.Substring($Root.Length + 1)
	$Log = & cmd /c "`"$BuildBat`" RogueHeroEditor Win64 Development -Project=`"$Project`" -WaitMutex -NoHotReload -NoUBTMakefiles -SingleFile=`"$F`" 2>&1"
	$Rc = $LASTEXITCODE
	$Bad = @($Log | Where-Object { $_ -match '(error C\d+|fatal error|warning C\d+|error:)' })
	$Compiled = @($Log | Where-Object { $_ -match 'Compile \[' }).Count
	if ($Rc -ne 0 -or $Bad.Count -gt 0 -or $Compiled -eq 0) {
		$Why = if ($Compiled -eq 0 -and $Rc -eq 0 -and $Bad.Count -eq 0) { "nothing compiled" } else { "exit $Rc" }
		Write-Host ("[{0}/{1}] FAIL {2} ({3})" -f $i, $Targets.Count, $Rel, $Why)
		$Bad | Select-Object -First 8 | ForEach-Object { Write-Host "    $_" }
		$Failed += $Rel
	}
	else {
		Write-Host ("[{0}/{1}] ok   {2}" -f $i, $Targets.Count, $Rel)
	}
}

if ($Failed.Count -eq 0) {
	Write-Host "NON-UNITY: PASS ($($Targets.Count) file(s) compile alone)"
	exit 0
}
Write-Host "NON-UNITY: FAIL ($($Failed.Count) of $($Targets.Count)): add the missing #include to each file listed"
$Failed | ForEach-Object { Write-Host "  $_" }
exit 1
