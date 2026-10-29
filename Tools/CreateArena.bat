@echo off
setlocal
rem Builds the S0 stress arena and its materials headless (Tools\create_arena.py). Idempotent.
rem Usage: Tools\CreateArena.bat        Log: Saved\Logs\CreateArena.log
rem Override UE_ROOT to use another engine install.
if not defined UE_ROOT set "UE_ROOT=E:\UE5\UE_5.8"
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
if not exist "%ROOT%\Saved\Logs" mkdir "%ROOT%\Saved\Logs"
set "LOG=%ROOT%\Saved\Logs\CreateArena.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%ROOT%\Dusktracer.uproject" -run=pythonscript -script="%ROOT%\Tools\create_arena.py" -unattended -nopause -LiveCoding=false -ABSLOG="%LOG%"
set "RC=%ERRORLEVEL%"
findstr /C:"create_arena: DONE" "%LOG%" >nul
if errorlevel 1 (
	findstr /C:"create_arena" /C:"Error:" "%LOG%"
	echo CREATE ARENA: FAIL ^(rc %RC%; see %LOG%^)
	exit /b 1
)
findstr /C:"create_arena:" "%LOG%"
echo CREATE ARENA: PASS
exit /b 0
