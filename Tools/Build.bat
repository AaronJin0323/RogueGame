@echo off
setlocal
rem Builds Dusktracer. Usage: Tools\Build.bat [Target] [Config]
rem Extra UBT arguments go in the DT_UBT_ARGS environment variable (batch arguments split on '=', so -SingleFile=... breaks there).
rem   default DusktracerEditor Development; Shipping gate: Tools\Build.bat Dusktracer Shipping
rem Override UE_ROOT to use another engine install.
if not defined UE_ROOT set "UE_ROOT=E:\UE5\UE_5.8"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=DusktracerEditor"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project="%~dp0..\Dusktracer.uproject" -WaitMutex -NoHotReload %DT_UBT_ARGS%
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" echo Build FAILED (exit %RC%)
exit /b %RC%
