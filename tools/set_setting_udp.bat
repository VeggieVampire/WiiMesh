@echo off
setlocal
set "WII_IP=%~1"
set "SETTING=%~2"
if "%SETTING%"=="" (
  echo "%WII_IP%" | find "=" >nul
  if not errorlevel 1 (
    set "SETTING=%WII_IP%"
    set "WII_IP=192.168.0.42"
  )
)
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
if "%SETTING%"=="" (
  echo Usage: %~nx0 [wii-ip] key=value
  echo Example: %~nx0 192.168.0.42 font.size=4
  echo Keys: font.style font.size screensaver.mode screensaver.speed debug.enabled pointer.enabled
  exit /b 1
)
call "%~dp0send_udp_command.bat" "%WII_IP%" SETTINGS_SET "%SETTING%"
