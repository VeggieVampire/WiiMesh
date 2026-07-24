@echo off
setlocal

set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.13"
set "WII_DEVICE=%~2"
if "%WII_DEVICE%"=="" set "WII_DEVICE=sd"
set "FTP_URL=ftp://%WII_IP%/"
set "APP_SLUG=wii-mesh"
set "OUT_DIR=%~dp0"

where curl >nul 2>nul
if errorlevel 1 (
  echo curl was not found. Windows 10/11 normally includes it.
  exit /b 1
)

echo Fetching WiiMesh logs from %WII_IP%/%WII_DEVICE%/apps/%APP_SLUG%...
curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug.log" -o "%OUT_DIR%debug.log"
if errorlevel 1 curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/DEBUG.LOG" -o "%OUT_DIR%debug.log"
if errorlevel 1 (
  echo Could not fetch debug.log. Make sure FTPii is running and WiiMesh has launched at least once.
  exit /b 1
)

echo Saved "%OUT_DIR%debug.log"
exit /b 0
