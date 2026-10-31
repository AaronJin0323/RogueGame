@echo off
rem Zips the run files and the game log to the desktop. Starts no game, sends nothing anywhere (02_Systems/package.md R-PKG-06).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Export_Logs.ps1" %*
pause
