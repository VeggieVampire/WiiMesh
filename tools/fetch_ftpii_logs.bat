@echo off
setlocal

set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.13"
set "WII_DEVICE=%~2"
if "%WII_DEVICE%"=="" set "WII_DEVICE=sd"
set "FTP_URL=ftp://%WII_IP%/"
set "APP_SLUG=wii-mesh"
set "OUT_DIR=%~dp0"
set "FETCH_LOG=%OUT_DIR%fetch_ftpii_logs.log"

> "%FETCH_LOG%" echo WiiMesh FTP fetch log
>> "%FETCH_LOG%" echo Started: %DATE% %TIME%
>> "%FETCH_LOG%" echo Wii IP: %WII_IP%
>> "%FETCH_LOG%" echo Wii device: %WII_DEVICE%
>> "%FETCH_LOG%" echo Target folder: /%WII_DEVICE%/apps/%APP_SLUG%/
>> "%FETCH_LOG%" echo.

where curl >nul 2>nul
if errorlevel 1 (
  echo curl was not found. Windows 10/11 normally includes it.
  exit /b 1
)

echo Fetching WiiMesh logs from %WII_IP%/%WII_DEVICE%/apps/%APP_SLUG%...
curl --fail --list-only "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/" -o "%OUT_DIR%remote_listing.txt" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 (
  echo Could not fetch remote listing.
  >> "%FETCH_LOG%" echo No remote listing fetched.
) else (
  echo Saved "%OUT_DIR%remote_listing.txt"
)

curl --fail --list-only "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/" -o "%OUT_DIR%theme_listing.txt" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 (
  >> "%FETCH_LOG%" echo No theme folder listing fetched.
  if exist "%OUT_DIR%theme_listing.txt" del "%OUT_DIR%theme_listing.txt" >nul 2>nul
) else (
  echo Saved "%OUT_DIR%theme_listing.txt"
)

curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug.log" -o "%OUT_DIR%debug.log" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/DEBUG.LOG" -o "%OUT_DIR%debug.log" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 (
  echo Could not fetch debug.log. Make sure FTPii is running and WiiMesh has launched at least once.
  >> "%FETCH_LOG%" echo ERROR: Could not fetch debug.log.
  exit /b 1
)

echo Saved "%OUT_DIR%debug.log"

curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/messages.dat" -o "%OUT_DIR%messages.dat" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/MESSAGES.DAT" -o "%OUT_DIR%messages.dat" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 (
  >> "%FETCH_LOG%" echo No messages.dat fetched.
  if exist "%OUT_DIR%messages.dat" del "%OUT_DIR%messages.dat" >nul 2>nul
) else (
  echo Saved "%OUT_DIR%messages.dat"
)

curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/GUI.config" -o "%OUT_DIR%GUI.config" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/gui.config" -o "%OUT_DIR%GUI.config" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 (
  >> "%FETCH_LOG%" echo No GUI.config fetched.
  if exist "%OUT_DIR%GUI.config" del "%OUT_DIR%GUI.config" >nul 2>nul
) else (
  echo Saved "%OUT_DIR%GUI.config"
)

curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/MeshLayout.config" -o "%OUT_DIR%MeshLayout.remote.config" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/MESHLAYOUT.CONFIG" -o "%OUT_DIR%MeshLayout.remote.config" >> "%FETCH_LOG%" 2>&1
if errorlevel 1 (
  >> "%FETCH_LOG%" echo No theme/MeshLayout.config fetched.
  if exist "%OUT_DIR%MeshLayout.remote.config" del "%OUT_DIR%MeshLayout.remote.config" >nul 2>nul
) else (
  echo Saved "%OUT_DIR%MeshLayout.remote.config"
)

echo Fetch log: "%FETCH_LOG%"
exit /b 0
