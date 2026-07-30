@echo off
setlocal

set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.13"
set "LAYOUT=%~2"
if "%LAYOUT%"=="" set "LAYOUT=%CD%\MeshLayout.config"

py "%~dp0upload_layout_udp.py" "%WII_IP%" "%LAYOUT%"
if errorlevel 1 python "%~dp0upload_layout_udp.py" "%WII_IP%" "%LAYOUT%"
exit /b %errorlevel%
