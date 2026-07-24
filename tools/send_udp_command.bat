@echo off
setlocal
set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.13"
set "CMD=%~2"
if "%CMD%"=="" set "CMD=PING"
py "%~dp0send_udp_command.py" "%WII_IP%" "%CMD%"
if errorlevel 1 python "%~dp0send_udp_command.py" "%WII_IP%" "%CMD%"
