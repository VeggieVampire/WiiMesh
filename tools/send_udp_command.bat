@echo off
setlocal
set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
set "CMD="
shift /1
:collect_args
if "%~1"=="" goto args_done
if defined CMD (
  set "CMD=%CMD% %~1"
) else (
  set "CMD=%~1"
)
shift /1
goto collect_args
:args_done
if "%CMD%"=="" set "CMD=PING"
py "%~dp0send_udp_command.py" "%WII_IP%" "%CMD%"
if errorlevel 1 python "%~dp0send_udp_command.py" "%WII_IP%" "%CMD%"
