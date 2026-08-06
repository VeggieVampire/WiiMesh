@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%.."
set "OUT_DIR=%ROOT%"
if not exist "%ROOT%\Makefile" if exist "%SCRIPT_DIR%..\work\WiiMesh\Makefile" (
  set "ROOT=%SCRIPT_DIR%..\work\WiiMesh"
  set "OUT_DIR=%SCRIPT_DIR%"
)
set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
set "WII_DEVICE=%~2"
if "%WII_DEVICE%"=="" set "WII_DEVICE=sd"
set "UPLOAD_LAYOUT=0"
set "UPLOAD_THEME=0"
set "CLEAR_REMOTE_LOGS=1"
set "FTP_ACTIVE=0"
set "PREFETCH_LOGS=0"
set "VERIFY_REMOTE=0"
if /I "%~3"=="--upload-layout" set "UPLOAD_LAYOUT=1"
if /I "%~3"=="--upload-theme" set "UPLOAD_THEME=1"
if /I "%~3"=="--keep-logs" set "CLEAR_REMOTE_LOGS=0"
if /I "%~3"=="--active" set "FTP_ACTIVE=1"
if /I "%~3"=="--fetch-logs" set "PREFETCH_LOGS=1"
if /I "%~3"=="--verify" set "VERIFY_REMOTE=1"
if /I "%~4"=="--upload-layout" set "UPLOAD_LAYOUT=1"
if /I "%~4"=="--upload-theme" set "UPLOAD_THEME=1"
if /I "%~4"=="--keep-logs" set "CLEAR_REMOTE_LOGS=0"
if /I "%~4"=="--fetch-logs" set "PREFETCH_LOGS=1"
if /I "%~4"=="--verify" set "VERIFY_REMOTE=1"
if /I "%~5"=="--upload-layout" set "UPLOAD_LAYOUT=1"
if /I "%~5"=="--upload-theme" set "UPLOAD_THEME=1"
if /I "%~5"=="--keep-logs" set "CLEAR_REMOTE_LOGS=0"
if /I "%~5"=="--active" set "FTP_ACTIVE=1"
if /I "%~5"=="--fetch-logs" set "PREFETCH_LOGS=1"
if /I "%~5"=="--verify" set "VERIFY_REMOTE=1"
if /I "%~6"=="--upload-layout" set "UPLOAD_LAYOUT=1"
if /I "%~6"=="--upload-theme" set "UPLOAD_THEME=1"
if /I "%~6"=="--keep-logs" set "CLEAR_REMOTE_LOGS=0"
if /I "%~6"=="--active" set "FTP_ACTIVE=1"
if /I "%~6"=="--fetch-logs" set "PREFETCH_LOGS=1"
if /I "%~6"=="--verify" set "VERIFY_REMOTE=1"
if /I "%~4"=="--active" set "FTP_ACTIVE=1"
set "FTP_URL=ftp://%WII_IP%/"
set "APP_SLUG=wii-mesh"
set "LOG=%~dp0ftp.log"
set "STAMP=%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%"
set "STAMP=%STAMP: =0%"
set "PREFETCH_DIR=%~dp0logs_before_upload"
set "PREFETCH_PREFIX=%PREFETCH_DIR%\%STAMP%_%WII_IP%_%WII_DEVICE%_%APP_SLUG%"
set "CURL_BASE=--disable-epsv --connect-timeout 5 --speed-time 15 --speed-limit 1"
if "%FTP_ACTIVE%"=="1" set "CURL_BASE=--ftp-port - --connect-timeout 5 --speed-time 15 --speed-limit 1"
set "CURL_LIST=%CURL_BASE% --max-time 20"
set "CURL_GET=%CURL_BASE% --max-time 35"
set "CURL_UPLOAD=%CURL_BASE% --max-time 120 --retry 1 --retry-delay 1 --retry-all-errors"


> "%LOG%" echo WiiMesh FTP upload log
>> "%LOG%" echo Started: %DATE% %TIME%
>> "%LOG%" echo Root: %ROOT%
>> "%LOG%" echo Output dir: %OUT_DIR%
>> "%LOG%" echo Wii IP: %WII_IP%
>> "%LOG%" echo Wii device: %WII_DEVICE%
>> "%LOG%" echo Target folder: /%WII_DEVICE%/apps/%APP_SLUG%/
>> "%LOG%" echo Preserve GUI.config: yes
>> "%LOG%" echo Preserve Settings.config: yes
>> "%LOG%" echo Preserve USB.config: yes
>> "%LOG%" echo Clear remote logs after prefetch: %CLEAR_REMOTE_LOGS%
>> "%LOG%" echo Fetch logs/configs before upload: %PREFETCH_LOGS%
>> "%LOG%" echo Verify remote boot.dol by full download: %VERIFY_REMOTE%
>> "%LOG%" echo Upload MeshLayout.config: %UPLOAD_LAYOUT%
>> "%LOG%" echo Upload theme background: %UPLOAD_THEME%
>> "%LOG%" echo Active FTP mode: %FTP_ACTIVE%
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
  curl --disable-epsv --connect-timeout 2 --max-time 4 --silent --show-error --list-only "%FTP_URL%" >> "%LOG%" 2>&1
  if not errorlevel 1 goto ftp_ready
  echo   FTPii not ready yet. Attempt %%I of 20...
  >> "%LOG%" echo FTPii not ready on attempt %%I.
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Sleep -Seconds 2" >nul 2>nul
)

echo.
echo FTPii did not answer at %WII_IP%.
echo If the Wii says "net_gethostip() failed", FTPii does not have network yet.
echo Reboot the Wii, make sure Wi-Fi is connected, start FTPii, wait until it shows an IP, then run this again.
>> "%LOG%" echo ERROR: FTPii did not answer.
exit /b 1

:ftp_ready
echo FTPii ready.
>> "%LOG%" echo FTPii ready.
>> "%LOG%" echo Prefetch directory: %PREFETCH_DIR%

if not "%PREFETCH_LOGS%"=="1" goto skip_prefetch

echo Pulling existing WiiMesh logs before upload...
curl %CURL_LIST% --fail --list-only "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/" -o "%PREFETCH_PREFIX%_listing.txt" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing remote app folder listing found before upload.
  if exist "%PREFETCH_PREFIX%_listing.txt" del "%PREFETCH_PREFIX%_listing.txt" >nul 2>nul
) else (
  echo Saved previous remote listing: %PREFETCH_PREFIX%_listing.txt
)

curl %CURL_LIST% --fail --list-only "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/" -o "%PREFETCH_PREFIX%_theme_listing.txt" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing remote theme folder listing found before upload.
  if exist "%PREFETCH_PREFIX%_theme_listing.txt" del "%PREFETCH_PREFIX%_theme_listing.txt" >nul 2>nul
) else (
  echo Saved previous theme listing: %PREFETCH_PREFIX%_theme_listing.txt
)

curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug.log" -o "%PREFETCH_PREFIX%_debug.log" >> "%LOG%" 2>&1
if errorlevel 1 curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/DEBUG.LOG" -o "%PREFETCH_PREFIX%_debug.log" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing debug.log found before upload.
  if exist "%PREFETCH_PREFIX%_debug.log" del "%PREFETCH_PREFIX%_debug.log" >nul 2>nul
) else (
  echo Saved previous debug log: %PREFETCH_PREFIX%_debug.log
)

if "%CLEAR_REMOTE_LOGS%"=="1" (
  type nul > "%PREFETCH_DIR%\empty-debug.log"
  >> "%LOG%" echo Truncate remote debug.log after prefetch.
  curl %CURL_UPLOAD% --fail --ftp-create-dirs -T "%PREFETCH_DIR%\empty-debug.log" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug.log" >> "%LOG%" 2>&1
  if errorlevel 1 (
    >> "%LOG%" echo WARNING: Could not truncate remote debug.log.
  ) else (
    echo Cleared remote debug.log after saving a copy.
  )
) else (
  >> "%LOG%" echo Remote debug.log preserved by --keep-logs.
)

curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/messages.dat" -o "%PREFETCH_PREFIX%_messages.dat" >> "%LOG%" 2>&1
if errorlevel 1 curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/MESSAGES.DAT" -o "%PREFETCH_PREFIX%_messages.dat" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing messages.dat found before upload.
  if exist "%PREFETCH_PREFIX%_messages.dat" del "%PREFETCH_PREFIX%_messages.dat" >nul 2>nul
) else (
  echo Saved previous messages: %PREFETCH_PREFIX%_messages.dat
)

curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/GUI.config" -o "%PREFETCH_PREFIX%_GUI.config" >> "%LOG%" 2>&1
if errorlevel 1 curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/gui.config" -o "%PREFETCH_PREFIX%_GUI.config" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing GUI.config found before upload.
  if exist "%PREFETCH_PREFIX%_GUI.config" del "%PREFETCH_PREFIX%_GUI.config" >nul 2>nul
) else (
  echo Saved previous GUI config: %PREFETCH_PREFIX%_GUI.config
)
>> "%LOG%" echo Remote GUI.config is preserved. This script never uploads GUI.config.

curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/Settings.config" -o "%PREFETCH_PREFIX%_Settings.config" >> "%LOG%" 2>&1
if errorlevel 1 curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/settings.config" -o "%PREFETCH_PREFIX%_Settings.config" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing Settings.config found before upload.
  if exist "%PREFETCH_PREFIX%_Settings.config" del "%PREFETCH_PREFIX%_Settings.config" >nul 2>nul
) else (
  echo Saved previous Settings config: %PREFETCH_PREFIX%_Settings.config
)
>> "%LOG%" echo Remote Settings.config is preserved. This script never uploads Settings.config.

curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/USB.config" -o "%PREFETCH_PREFIX%_USB.config" >> "%LOG%" 2>&1
if errorlevel 1 curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/usb.config" -o "%PREFETCH_PREFIX%_USB.config" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing USB.config found before upload.
  if exist "%PREFETCH_PREFIX%_USB.config" del "%PREFETCH_PREFIX%_USB.config" >nul 2>nul
) else (
  echo Saved previous USB config: %PREFETCH_PREFIX%_USB.config
)
>> "%LOG%" echo Remote USB.config is preserved. This script never uploads USB.config.

curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/MeshLayout.config" -o "%PREFETCH_PREFIX%_MeshLayout.config" >> "%LOG%" 2>&1
if errorlevel 1 curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/MESHLAYOUT.CONFIG" -o "%PREFETCH_PREFIX%_MeshLayout.config" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo No existing theme/MeshLayout.config found before upload.
  if exist "%PREFETCH_PREFIX%_MeshLayout.config" del "%PREFETCH_PREFIX%_MeshLayout.config" >nul 2>nul
) else (
  echo Saved previous MeshLayout config: %PREFETCH_PREFIX%_MeshLayout.config
)
>> "%LOG%" echo Remote MeshLayout.config is preserved unless --upload-layout is passed.

goto after_prefetch

:skip_prefetch
echo Fast deploy: skipping remote log/config downloads. Pass --fetch-logs to pull them.
>> "%LOG%" echo Fast deploy: skipped remote log/config downloads.
if "%CLEAR_REMOTE_LOGS%"=="1" (
  type nul > "%PREFETCH_DIR%\empty-debug.log"
  >> "%LOG%" echo Fast deploy truncates remote debug.log without downloading it first.
  curl %CURL_UPLOAD% --fail --ftp-create-dirs -T "%PREFETCH_DIR%\empty-debug.log" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug.log" >> "%LOG%" 2>&1
  if errorlevel 1 (
    >> "%LOG%" echo WARNING: Could not truncate remote debug.log.
  ) else (
    echo Cleared remote debug.log.
  )
)
>> "%LOG%" echo Remote GUI.config, Settings.config, USB.config, and MeshLayout.config are preserved.

:after_prefetch

if exist "%ROOT%\Makefile" (
  set "EDIT_ICON_DIR="
  if exist "%SCRIPT_DIR%icons" set "EDIT_ICON_DIR=%SCRIPT_DIR%icons"
  if "%EDIT_ICON_DIR%"=="" if exist "%ROOT%\..\..\outputs\icons" set "EDIT_ICON_DIR=%ROOT%\..\..\outputs\icons"
  if "%EDIT_ICON_DIR%"=="" if exist "%SCRIPT_DIR%ui_icons" set "EDIT_ICON_DIR=%SCRIPT_DIR%ui_icons"
  if "%EDIT_ICON_DIR%"=="" if exist "%ROOT%\..\..\outputs\ui_icons" set "EDIT_ICON_DIR=%ROOT%\..\..\outputs\ui_icons"
  if not "%EDIT_ICON_DIR%"=="" (
    echo Syncing edited UI icons from "%EDIT_ICON_DIR%"...
    >> "%LOG%" echo Sync UI icons from "%EDIT_ICON_DIR%" to "%ROOT%\assets\ui_icons"
    if not exist "%ROOT%\assets\ui_icons" mkdir "%ROOT%\assets\ui_icons" >nul 2>nul
    copy /Y "%EDIT_ICON_DIR%\home.png" "%ROOT%\assets\ui_icons\" >> "%LOG%" 2>&1
    copy /Y "%EDIT_ICON_DIR%\node.png" "%ROOT%\assets\ui_icons\" >> "%LOG%" 2>&1
    copy /Y "%EDIT_ICON_DIR%\channels.png" "%ROOT%\assets\ui_icons\" >> "%LOG%" 2>&1
    copy /Y "%EDIT_ICON_DIR%\chat.png" "%ROOT%\assets\ui_icons\" >> "%LOG%" 2>&1
    copy /Y "%EDIT_ICON_DIR%\map.png" "%ROOT%\assets\ui_icons\" >> "%LOG%" 2>&1
    copy /Y "%EDIT_ICON_DIR%\settings.png" "%ROOT%\assets\ui_icons\" >> "%LOG%" 2>&1
    copy /Y "%EDIT_ICON_DIR%\bell.png" "%ROOT%\assets\ui_icons\" >> "%LOG%" 2>&1
    if errorlevel 1 (
      echo UI icon sync failed. Nothing uploaded.
      >> "%LOG%" echo ERROR: UI icon sync failed.
      exit /b 1
    )
  ) else (
    >> "%LOG%" echo No edited icons folder at outputs/icons or outputs/ui_icons; using project assets.
  )
  echo Building WiiMesh...
  >> "%LOG%" echo Running make...
  pushd "%ROOT%"
  make clean >> "%LOG%" 2>&1
  make >> "%LOG%" 2>&1
  if errorlevel 1 (
    popd
    echo Build failed. Nothing uploaded.
    >> "%LOG%" echo ERROR: Build failed.
    exit /b 1
  )
  popd
  copy /Y "%ROOT%\boot.dol" "%OUT_DIR%\boot.dol" >> "%LOG%" 2>&1
  copy /Y "%ROOT%\meta.xml" "%OUT_DIR%\meta.xml" >> "%LOG%" 2>&1
  >> "%LOG%" echo Build complete.

) else (
  >> "%LOG%" echo Source tree not found; uploading existing files.
)

for /f "tokens=3 delims=<>" %%V in ('findstr /C:"<version>" "%OUT_DIR%\meta.xml"') do set "EXPECTED_VERSION=%%V"
if not defined EXPECTED_VERSION (
  echo Could not read AppVersion from source.
  >> "%LOG%" echo ERROR: Could not read AppVersion from source.
  exit /b 1
)

if not exist "%OUT_DIR%\boot.dol" (
  echo Missing "%OUT_DIR%\boot.dol"
  >> "%LOG%" echo ERROR: Missing boot.dol.
  exit /b 1
)

if not exist "%OUT_DIR%\meta.xml" (
  echo Missing "%OUT_DIR%\meta.xml"
  >> "%LOG%" echo ERROR: Missing meta.xml.
  exit /b 1
)

if not exist "%OUT_DIR%\debug-target.txt" (
  > "%OUT_DIR%\debug-target.txt" echo 192.168.0.233
  >> "%LOG%" echo Created default debug-target.txt
)

for %%F in ("%OUT_DIR%\boot.dol") do (
  >> "%LOG%" echo Local boot.dol size: %%~zF bytes
  echo Local boot.dol size: %%~zF bytes
  set "LOCAL_BOOT_SIZE=%%~zF"
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes('%OUT_DIR%\boot.dol')); exit -not $text.Contains('%EXPECTED_VERSION%')" >> "%LOG%" 2>&1
if errorlevel 1 (
  echo Built boot.dol does not contain expected version %EXPECTED_VERSION%.
  >> "%LOG%" echo ERROR: boot.dol version check failed for %EXPECTED_VERSION%.
  exit /b 1
)
echo Built boot.dol contains WiiMesh v%EXPECTED_VERSION%.
>> "%LOG%" echo Built boot.dol contains WiiMesh v%EXPECTED_VERSION%.

echo Uploading WiiMesh to FTPii at %WII_IP%/%WII_DEVICE%/apps/%APP_SLUG%...

>> "%LOG%" echo Delete old remote boot.dol before upload.
curl %CURL_BASE% --max-time 20 --quote "DELE /%WII_DEVICE%/apps/%APP_SLUG%/boot.dol" "%FTP_URL%" >> "%LOG%" 2>&1
>> "%LOG%" echo Delete command completed or was ignored.

>> "%LOG%" echo Upload boot.dol to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/boot.dol"
curl %CURL_UPLOAD% --verbose --fail --ftp-create-dirs -T "%OUT_DIR%\boot.dol" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/boot.dol" >> "%LOG%" 2>&1
if errorlevel 1 goto upload_failed

set "REMOTE_BOOT_CHECK=%PREFETCH_PREFIX%_uploaded_boot.dol"
if not "%VERIFY_REMOTE%"=="1" goto skip_remote_boot_verify
>> "%LOG%" echo Download uploaded boot.dol for size verification.
curl %CURL_GET% --fail "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/boot.dol" -o "%REMOTE_BOOT_CHECK%" >> "%LOG%" 2>&1
if errorlevel 1 (
  >> "%LOG%" echo ERROR: Could not download uploaded boot.dol for verification.
  goto upload_failed
)
for %%F in ("%REMOTE_BOOT_CHECK%") do (
  >> "%LOG%" echo Remote uploaded boot.dol size: %%~zF bytes
  echo Remote uploaded boot.dol size: %%~zF bytes
  if not "%%~zF"=="%LOCAL_BOOT_SIZE%" (
    echo Remote boot.dol size does not match local boot.dol.
    >> "%LOG%" echo ERROR: Remote boot.dol size mismatch. Local %LOCAL_BOOT_SIZE%, remote %%~zF.
    goto upload_failed
  )
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes('%REMOTE_BOOT_CHECK%')); exit -not $text.Contains('%EXPECTED_VERSION%')" >> "%LOG%" 2>&1
if errorlevel 1 (
  echo Remote boot.dol does not contain expected version %EXPECTED_VERSION%.
  >> "%LOG%" echo ERROR: Remote boot.dol version check failed for %EXPECTED_VERSION%.
  goto upload_failed
)
echo Remote boot.dol contains WiiMesh v%EXPECTED_VERSION%.
>> "%LOG%" echo Remote boot.dol contains WiiMesh v%EXPECTED_VERSION%.

goto after_remote_boot_verify

:skip_remote_boot_verify
echo Fast deploy: skipped remote boot.dol download verification. Pass --verify to enable it.
>> "%LOG%" echo Fast deploy: skipped remote boot.dol download verification.

:after_remote_boot_verify

>> "%LOG%" echo Verify remote app folder listing:
curl %CURL_LIST% --fail --list-only "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/" >> "%LOG%" 2>&1

>> "%LOG%" echo Upload meta.xml to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/meta.xml"
curl %CURL_UPLOAD% --verbose --fail --ftp-create-dirs -T "%OUT_DIR%\meta.xml" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/meta.xml" >> "%LOG%" 2>&1
if errorlevel 1 goto upload_failed

if exist "%ROOT%\assets\icon.png" (
  >> "%LOG%" echo Upload icon.png to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/icon.png"
  curl %CURL_UPLOAD% --verbose --fail --ftp-create-dirs -T "%ROOT%\assets\icon.png" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/icon.png" >> "%LOG%" 2>&1
  if errorlevel 1 goto upload_failed
) else (
  >> "%LOG%" echo No icon.png found; skipping icon upload.
)

set "THEME_BG="
if exist "%ROOT%\theme\background.rgb565" set "THEME_BG=%ROOT%\theme\background.rgb565"
if "%THEME_BG%"=="" if exist "%ROOT%\background.rgb565" set "THEME_BG=%ROOT%\background.rgb565"
if "%UPLOAD_THEME%"=="1" (
  if not "%THEME_BG%"=="" (
    for %%F in ("%THEME_BG%") do (
      >> "%LOG%" echo Local theme background size: %%~zF bytes
      echo Local theme background size: %%~zF bytes
    )
    >> "%LOG%" echo Upload theme background to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/background.rgb565"
    curl %CURL_UPLOAD% --verbose --fail --ftp-create-dirs -T "%THEME_BG%" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/background.rgb565" >> "%LOG%" 2>&1
    if errorlevel 1 goto upload_failed
  ) else (
    >> "%LOG%" echo No theme background found; using built-in GUI skin.
  )
) else (
  >> "%LOG%" echo Theme background upload skipped by default. Pass --upload-theme to upload it.
)

set "LAYOUT_CONFIG="
if exist "%ROOT%\theme\MeshLayout.config" set "LAYOUT_CONFIG=%ROOT%\theme\MeshLayout.config"
if "%LAYOUT_CONFIG%"=="" if exist "%ROOT%\MeshLayout.config" set "LAYOUT_CONFIG=%ROOT%\MeshLayout.config"
if "%LAYOUT_CONFIG%"=="" if exist "%OUT_DIR%\MeshLayout.config" set "LAYOUT_CONFIG=%OUT_DIR%\MeshLayout.config"
if "%UPLOAD_LAYOUT%"=="1" (
  if not "%LAYOUT_CONFIG%"=="" (
    >> "%LOG%" echo Upload MeshLayout.config to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/MeshLayout.config"
    curl %CURL_UPLOAD% --verbose --fail --ftp-create-dirs -T "%LAYOUT_CONFIG%" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/theme/MeshLayout.config" >> "%LOG%" 2>&1
    if errorlevel 1 goto upload_failed
  ) else (
    >> "%LOG%" echo No MeshLayout.config found; using built-in layout.
  )
) else (
  >> "%LOG%" echo MeshLayout.config upload skipped by default. Pass --upload-layout to upload it.
)

>> "%LOG%" echo Upload debug-target.txt to "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug-target.txt"
curl %CURL_UPLOAD% --verbose --fail --ftp-create-dirs -T "%OUT_DIR%\debug-target.txt" "%FTP_URL%%WII_DEVICE%/apps/%APP_SLUG%/debug-target.txt" >> "%LOG%" 2>&1
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
