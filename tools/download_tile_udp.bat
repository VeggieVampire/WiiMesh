@echo off
setlocal

set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
set "Z=%~2"
set "X=%~3"
set "Y=%~4"
set "STYLE=%~5"
if "%STYLE%"=="" set "STYLE=default"

if "%Z%"=="" (
  echo Usage: %~nx0 [wii_ip] z x y [style]
  echo Example: %~nx0 192.168.0.42 6 15 25 default
  exit /b 1
)

py "%~dp0send_udp_command.py" "%WII_IP%" TILE_GET %Z% %X% %Y% %STYLE%
exit /b %ERRORLEVEL%
