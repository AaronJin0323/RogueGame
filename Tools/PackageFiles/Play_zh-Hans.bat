@echo off
rem The game, Simplified Chinese. It opens in Vigil and every run starts and ends there (02_Systems/package.md R-PKG-04).
rem No window mode is named here: Settings' Window row decides it, and it is full screen until a person changes it (settings.md R-SET-08).
cd /d "%~dp0"
start "" "%~dp0Dusktracer.exe" -culture=zh-Hans
