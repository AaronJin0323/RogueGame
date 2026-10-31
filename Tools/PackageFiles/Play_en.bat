@echo off
rem One run, English, full screen. Alt+F4 quits; start this again for the next run (02_Systems/package.md R-PKG-04).
cd /d "%~dp0"
start "" "%~dp0Dusktracer.exe" -fullscreen -culture=en -DTSeed=%RANDOM%%RANDOM%
