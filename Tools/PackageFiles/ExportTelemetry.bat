@echo off
rem Zips the run files to the desktop. Sends nothing anywhere (02_Systems/package.md R-PKG-06).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ExportTelemetry.ps1" %*
pause
