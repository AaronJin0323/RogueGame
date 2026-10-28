@echo off
setlocal
rem Builds RogueHeroEditor Win64 Development. Override UE_ROOT to use another engine install.
if not defined UE_ROOT set "UE_ROOT=E:\UE5\UE_5.8"

call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" RogueHeroEditor Win64 Development -Project="%~dp0..\RogueHero.uproject" -WaitMutex -NoHotReload
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" echo Build FAILED (exit %RC%)
exit /b %RC%
