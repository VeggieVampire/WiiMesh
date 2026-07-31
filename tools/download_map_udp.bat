@echo off
setlocal

set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"

where py >nul 2>nul
if errorlevel 1 (
  echo Python launcher py was not found.
  exit /b 1
)

py "%~dp0download_map_udp.py" "%WII_IP%"
exit /b %ERRORLEVEL%
