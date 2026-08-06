@echo off
setlocal
set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
set "CMDLINE=%*"
if not "%~1"=="" (
  call set "CMD=%%CMDLINE:*%~1=%%"
)
if not defined CMD set "CMD=PING"
for /f "tokens=* delims= " %%A in ("%CMD%") do set "CMD=%%A"
if "%CMD%"=="" set "CMD=PING"
py "%~dp0send_udp_command.py" "%WII_IP%" "%CMD%"
if errorlevel 1 python "%~dp0send_udp_command.py" "%WII_IP%" "%CMD%"
