@echo off
setlocal
rem D-052: the machine's processor mask. This file runs again as a child of Tools\Machine.ps1, which holds the mask, so every child inherits it.
if /i "%DT_MASKED_BAT%"=="%~f0" goto :masked
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Machine.ps1" -DTRunFile "%~f0" %*
exit /b %ERRORLEVEL%
:masked
set "DT_MASKED_BAT="
rem Runs one editor Python script headless, as a commandlet: no window (R-TEC-17), no renderer. The way a generated asset is made
rem (07_Technical R-TEC-06): Tools\create_arena.py has its own launcher; mesh imports and the packages' material scripts use this one.
rem Usage: Tools\EditorPython.bat <script.py>        Log: Saved\Logs\EditorPython_<script>.log
rem The script's last log line must be "<script name without .py>: DONE"; anything else is a failure, whatever the exit code says.
rem Override UE_ROOT to use another engine install.
if not defined UE_ROOT set "UE_ROOT=E:\UE5\UE_5.8"
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
if "%~1"=="" (
	echo EDITOR PYTHON: FAIL ^(no script given^)
	exit /b 1
)
set "SCRIPT=%~f1"
set "NAME=%~n1"
if not exist "%SCRIPT%" (
	echo EDITOR PYTHON: FAIL ^(%SCRIPT% does not exist^)
	exit /b 1
)
if not exist "%ROOT%\Saved\Logs" mkdir "%ROOT%\Saved\Logs"
set "LOG=%ROOT%\Saved\Logs\EditorPython_%NAME%.log"
if exist "%LOG%" del "%LOG%"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%ROOT%\Dusktracer.uproject" -run=pythonscript -script="%SCRIPT%" -unattended -nopause -nullrhi -nosplash -LiveCoding=false -ABSLOG="%LOG%" >nul 2>&1
set "RC=%ERRORLEVEL%"
findstr /C:"%NAME%: DONE" "%LOG%" >nul
if errorlevel 1 (
	findstr /C:"%NAME%:" /C:"Error:" "%LOG%"
	echo EDITOR PYTHON: FAIL ^(%NAME%, rc %RC%; see %LOG%^)
	exit /b 1
)
findstr /C:"%NAME%:" "%LOG%"
echo EDITOR PYTHON: PASS ^(%NAME%^)
exit /b 0
