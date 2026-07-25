@echo off
setlocal

set "ROOT=%~dp0.."
set "OUT_DIR=%ROOT%\"
set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.13"
set "WII_DEVICE=%~2"
if "%WII_DEVICE%"=="" set "WII_DEVICE=sd"
set "FTP_URL=ftp://%WII_IP%/"
set "APP_SLUG=wii-mesh"
set "LOG=%~dp0ftp.log"
set "STAMP=%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%"
set "STAMP=%STAMP: =0%"
set "PREFETCH_DIR=%~dp0logs_before_upload"
set "PREFETCH_PREFIX=%PREFETCH_DIR%\%STAMP%_%WII_IP%_%WII_DEVICE%_%APP_SLUG%"

> "%LOG%" echo WiiMesh FTP upload log
>> "%LOG%" echo Started: %DATE% %TIME%
>> "%LOG%" echo Root: %ROOT%
>> "%LOG%" echo Output dir: %OUT_DIR%
>> "%LOG%" echo Wii IP: %WII_IP%
>> "%LOG%" echo Wii device: %WII_DEVICE%
>> "%LOG%" echo Target folder: /%WII_DEVICE%/apps/%APP_SLUG%/
>> "%LOG%" echo.
echo FTP log: %LOG%
if not exist "%PREFETCH_DIR%" mkdir "%PREFETCH_DIR%" >nul 2>nul

where curl >nul 2>nul
if errorlevel 1 (
  echo curl was not found. Windows 10/11 normally includes it.
  >> "%LOG%" echo ERROR: curl was not found.
  exit /b 1
)

echo.
echo Waiting for FTPii at %WII_IP%...
for /L %%I in (1,1,20) do (
  >> "%LOG%" echo Readiness attempt %%I: curl --list-only "%FTP_URL%"
  curl --connect-timeout 2 --max-time 4 --silent --show-error --list-only "%FTP_URL%" >> "%LOG%" 2>&1
  if not errorlevel 1 goto ftp_ready
  echo   FTPii not ready yet. Attempt %%I of 20...
  >> "%LOG%" echo FTPii not ready on attempt %%I.
  "%SystemRoot%\System32\timeout.exe" /t 2 /nobreak >nul
)

echo.
echo FTPii did not answer at %WII_IP%.
echo If the Wii says "net_gethostip() failed", FTPii does not have network yet.
echo Reboot the Wii, make sure Wi-Fi is connected, start FTPii, wait until it shows an IP, then run this again.
>> "%LOG%" echo ERROR: FTPii did not answer.
exit /b 1

:ftp_ready
echo FTPii ready. Pulling existing WiiMesh logs before upload...
>> "%LOG%" echo FTPii ready.
>> "%LOG%" echo Prefetch directory: %PREFETCH_DIR%

curl --fail --list-only "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/" -o "%PREFETCH_PREFIX%_listing.txt" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing remote app folder listing found before upload.
  if exist "%PREFETCH_PREFIX%_listing.txt" del "%PREFETCH_PREFIX%_listing.txt" >nul 2>nul
) else (
  echo Saved previous remote listing: %PREFETCH_PREFIX%_listing.txt
)

curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug.log" -o "%PREFETCH_PREFIX%_debug.log" >> "%LOG%" 2>&1
if errorlevel 1 curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/DEBUG.LOG" -o "%PREFETCH_PREFIX%_debug.log" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing debug.log found before upload.
  if exist "%PREFETCH_PREFIX%_debug.log" del "%PREFETCH_PREFIX%_debug.log" >nul 2>nul
) else (
  echo Saved previous debug log: %PREFETCH_PREFIX%_debug.log
)

curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/messages.dat" -o "%PREFETCH_PREFIX%_messages.dat" >> "%LOG%" 2>&1
if errorlevel 1 curl --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/MESSAGES.DAT" -o "%PREFETCH_PREFIX%_messages.dat" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing messages.dat found before upload.
  if exist "%PREFETCH_PREFIX%_messages.dat" del "%PREFETCH_PREFIX%_messages.dat" >nul 2>nul
) else (
  echo Saved previous messages: %PREFETCH_PREFIX%_messages.dat
)

if exist "%ROOT%\Makefile" (
  echo Building WiiMesh...
  >> "%LOG%" echo Running make...
  pushd "%ROOT%"
  make >> "%LOG%" 2>&1
  if errorlevel 1 (
    popd
    echo Build failed. Nothing uploaded.
    >> "%LOG%" echo ERROR: Build failed.
    exit /b 1
  )
  popd
  >> "%LOG%" echo Build complete.

) else (
  >> "%LOG%" echo Source tree not found; uploading existing files.
)

if not exist "%OUT_DIR%boot.dol" (
  echo Missing "%OUT_DIR%boot.dol"
  >> "%LOG%" echo ERROR: Missing boot.dol.
  exit /b 1
)

if not exist "%OUT_DIR%meta.xml" (
  echo Missing "%OUT_DIR%meta.xml"
  >> "%LOG%" echo ERROR: Missing meta.xml.
  exit /b 1
)

if not exist "%OUT_DIR%debug-target.txt" (
  > "%OUT_DIR%debug-target.txt" echo 192.168.0.110
  >> "%LOG%" echo Created default debug-target.txt
)

for %%F in ("%OUT_DIR%boot.dol") do (
  >> "%LOG%" echo Local boot.dol size: %%~zF bytes
  echo Local boot.dol size: %%~zF bytes
)

echo Uploading WiiMesh to FTPii at %WII_IP%/%WII_DEVICE%/apps/%APP_SLUG%...

>> "%LOG%" echo Upload boot.dol to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/boot.dol"
curl --verbose --fail --ftp-create-dirs -T "%OUT_DIR%boot.dol" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/boot.dol" >> "%LOG%" 2>&1
if errorlevel 1 goto upload_failed

>> "%LOG%" echo Verify remote app folder listing:
curl --fail --list-only "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/" >> "%LOG%" 2>&1

>> "%LOG%" echo Upload meta.xml to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/meta.xml"
curl --verbose --fail --ftp-create-dirs -T "%OUT_DIR%meta.xml" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/meta.xml" >> "%LOG%" 2>&1
if errorlevel 1 goto upload_failed

if exist "%ROOT%\assets\icon.png" (
  >> "%LOG%" echo Upload icon.png to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/icon.png"
  curl --verbose --fail --ftp-create-dirs -T "%ROOT%\assets\icon.png" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/icon.png" >> "%LOG%" 2>&1
  if errorlevel 1 goto upload_failed
) else (
  >> "%LOG%" echo No icon.png found; skipping icon upload.
)

>> "%LOG%" echo Upload debug-target.txt to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug-target.txt"
curl --verbose --fail --ftp-create-dirs -T "%OUT_DIR%debug-target.txt" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug-target.txt" >> "%LOG%" 2>&1
if errorlevel 1 goto upload_failed

echo.
echo Done. Start WiiMesh from the Homebrew Channel.
>> "%LOG%" echo Upload complete.
exit /b 0

:upload_failed
echo.
echo Upload failed. Make sure FTPii is running and the device path exists.
echo Try: %~nx0 %WII_IP% sd
echo Or if you use USB: %~nx0 %WII_IP% usb
>> "%LOG%" echo ERROR: Upload failed.
exit /b 1
