@echo off
setlocal
set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
call "%~dp0send_udp_command.bat" "%WII_IP%" SETTINGS_RELOAD
