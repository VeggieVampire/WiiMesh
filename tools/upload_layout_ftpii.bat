@echo off
setlocal

set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.13"
set "WII_DEVICE=%~2"
if "%WII_DEVICE%"=="" set "WII_DEVICE=sd"
set "SCRIPT_DIR=%~dp0"
set "OUT_DIR=%SCRIPT_DIR%..\..\outputs"
if exist "%SCRIPT_DIR%MeshLayout.config" set "OUT_DIR=%SCRIPT_DIR%"
if not exist "%OUT_DIR%\MeshLayout.config" set "OUT_DIR=%CD%"
set "LAYOUT=%OUT_DIR%\MeshLayout.config"
set "FTP_URL=ftp://%WII_IP%/"
set "LOG=%SCRIPT_DIR%layout_ftp.log"

if not exist "%LAYOUT%" (
  echo Missing MeshLayout.config.
  echo Put it in the current folder or outputs folder.
  exit /b 1
)

where curl >nul 2>nul
if errorlevel 1 (
  echo curl was not found. Windows 10/11 normally includes it.
  exit /b 1
)

> "%LOG%" echo WiiMesh layout upload log
>> "%LOG%" echo Started: %DATE% %TIME%
>> "%LOG%" echo Layout: %LAYOUT%
>> "%LOG%" echo Wii: %WII_IP% %WII_DEVICE%

echo Uploading MeshLayout.config to %WII_IP%...
curl --verbose --fail --ftp-create-dirs -T "%LAYOUT%" "%FTP_URL%%WII_DEVICE%/apps/wii-mesh/theme/MeshLayout.config" >> "%LOG%" 2>&1
if errorlevel 1 (
  echo Upload failed. See %LOG%
  exit /b 1
)

echo Telling WiiMesh to reload layout over UDP...
py "%SCRIPT_DIR%send_udp_command.py" "%WII_IP%" LAYOUT
if errorlevel 1 python "%SCRIPT_DIR%send_udp_command.py" "%WII_IP%" LAYOUT
exit /b %errorlevel%
