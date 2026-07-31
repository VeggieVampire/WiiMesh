@echo off
setlocal

set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
set "TILE_URL=%~2"
if "%TILE_URL%"=="" (
  echo Usage: %~nx0 [wii_ip] http://server/path/{z}/{x}/{y}.png
  exit /b 1
)

py "%~dp0send_udp_command.py" "%WII_IP%" TILE_SET_URL "%TILE_URL%"
exit /b %ERRORLEVEL%
