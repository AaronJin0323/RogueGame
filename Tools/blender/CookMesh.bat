@echo off
setlocal
rem Cooks the named packages alone for Windows into a scratch folder, as a commandlet: no window (R-TEC-17). The last step of the mesh
rem pipeline's proof (Tools\blender\Mesh.ps1 -Cook): an imported mesh that cannot be cooked would only fail at packaging.
rem Usage: Tools\blender\CookMesh.bat <package>[+<package>...] <output folder>      Log: Saved\Logs\CookMesh.log
rem Override UE_ROOT to use another engine install.
if not defined UE_ROOT set "UE_ROOT=E:\UE5\UE_5.8"
set "ROOT=%~dp0..\.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
if "%~2"=="" (
	echo COOK MESH: FAIL ^(usage: CookMesh.bat packages output-folder^)
	exit /b 1
)
if not exist "%ROOT%\Saved\Logs" mkdir "%ROOT%\Saved\Logs"
set "LOG=%ROOT%\Saved\Logs\CookMesh.log"
if exist "%LOG%" del "%LOG%"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%ROOT%\Dusktracer.uproject" -run=cook -targetplatform=Windows -map=%~1 -cooksinglepackagenorefs -OutputDir="%~f2" -unattended -nopause -nullrhi -nosplash -LiveCoding=false -ABSLOG="%LOG%" >nul 2>&1
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
	findstr /C:"Error:" "%LOG%"
	echo COOK MESH: FAIL ^(rc %RC%; see %LOG%^)
	exit /b 1
)
echo COOK MESH: PASS
exit /b 0
