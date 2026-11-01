@echo off
rem The game, English, full screen. It opens in Vigil and every run starts and ends there (02_Systems/package.md R-PKG-04).
cd /d "%~dp0"
start "" "%~dp0Dusktracer.exe" -fullscreen -culture=en
