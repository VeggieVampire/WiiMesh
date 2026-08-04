@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%.."
if exist "%SCRIPT_DIR%..\work\WiiMesh\Makefile" set "ROOT=%SCRIPT_DIR%..\work\WiiMesh"
set "OUT_ICONS=%SCRIPT_DIR%ui_icons"
if not exist "%OUT_ICONS%" set "OUT_ICONS=%ROOT%\..\..\outputs\ui_icons"
if not exist "%OUT_ICONS%" set "OUT_ICONS=%SCRIPT_DIR%..\..\outputs\ui_icons"
set "OUT_DIR=%ROOT%\..\..\outputs"

if exist "%OUT_ICONS%" (
  echo Syncing editable icons from "%OUT_ICONS%"...
  copy /Y "%OUT_ICONS%\*.png" "%ROOT%\assets\ui_icons\" >nul
) else (
  echo No outputs\ui_icons folder found. Building with project icons.
)

pushd "%ROOT%"
make clean
if errorlevel 1 (
  popd
  exit /b 1
)
make
if errorlevel 1 (
  popd
  exit /b 1
)
popd

if exist "%OUT_DIR%" (
  copy /Y "%ROOT%\boot.dol" "%OUT_DIR%\boot.dol" >nul
  copy /Y "%ROOT%\meta.xml" "%OUT_DIR%\meta.xml" >nul
  copy /Y "%ROOT%\CHANGELOG.md" "%OUT_DIR%\CHANGELOG.md" >nul
  copy /Y "%ROOT%\tools\deploy_ftpii.bat" "%OUT_DIR%\deploy_ftpii.bat" >nul
  copy /Y "%ROOT%\tools\fetch_ftpii_logs.bat" "%OUT_DIR%\fetch_ftpii_logs.bat" >nul
  copy /Y "%ROOT%\tools\send_udp_command.bat" "%OUT_DIR%\send_udp_command.bat" >nul
  copy /Y "%ROOT%\tools\send_udp_command.py" "%OUT_DIR%\send_udp_command.py" >nul
)

echo Built boot.dol with compiled UI icons.
exit /b 0
