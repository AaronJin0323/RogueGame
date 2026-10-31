@echo off
setlocal
rem Starts a play session for a human (S1 validation, owner playtests): the arena in a full-screen game window,
rem director on, bot off, offers chosen by the player. Telemetry lands in Saved\Playtests (per run, per player).
rem Usage: Tools\Play.bat [en|zh-Hans]        default: the language saved in Settings (settings.md R-SET-06)
rem Build first with Tools\Build.bat; this never builds.
if not defined UE_ROOT set "UE_ROOT=E:\UE5\UE_5.8"
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "CULTURE=%~1"
set "CULTUREARG="
if not "%CULTURE%"=="" set "CULTUREARG=-culture=%CULTURE%"
start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%ROOT%\Dusktracer.uproject" /Game/DT/Maps/L_Arena_Test -game -fullscreen -LiveCoding=false %CULTUREARG%
echo PLAY: started. Logs: %ROOT%\Saved\Playtests
