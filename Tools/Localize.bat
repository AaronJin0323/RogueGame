@echo off
setlocal
rem Compiles the game text for every culture (R-TEC-08):
rem   1. python Tools\loc_build.py --build   CSV truth -> Content/DT/Text/Generated/ST_*.csv + Game.po
rem   2. GatherText commandlet               gather (Config/Localization/Game_Gather.ini), import the .po
rem                                          (Game_Import.ini), compile .locres (Game_Compile.ini)
rem Usage: Tools\Localize.bat        Log: Saved\Logs\Localize.log
rem Outputs are build products (git-ignored); never commit or merge them by hand.
if not defined UE_ROOT set "UE_ROOT=E:\UE5\UE_5.8"
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
if not exist "%ROOT%\Saved\Logs" mkdir "%ROOT%\Saved\Logs"
set "LOG=%ROOT%\Saved\Logs\Localize.log"

python -I "%ROOT%\Tools\loc_build.py" --build
if errorlevel 1 (
	echo LOCALIZE: FAIL ^(loc_build^)
	exit /b 1
)

set "CFG=%ROOT%\Config\Localization\Game_Gather.ini;%ROOT%\Config\Localization\Game_Import.ini;%ROOT%\Config\Localization\Game_Compile.ini"
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%ROOT%\Dusktracer.uproject" -run=GatherText -config="%CFG%" -unattended -nopause -LiveCoding=false -ABSLOG="%LOG%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
	findstr /C:"Error:" "%LOG%"
	echo LOCALIZE: FAIL ^(GatherText rc %RC%; see %LOG%^)
	exit /b 1
)
if not exist "%ROOT%\Content\Localization\Game\zh-Hans\Game.locres" (
	echo LOCALIZE: FAIL ^(no zh-Hans Game.locres; see %LOG%^)
	exit /b 1
)
echo LOCALIZE: PASS ^(Content\Localization\Game\^<culture^>\Game.locres^)
exit /b 0
