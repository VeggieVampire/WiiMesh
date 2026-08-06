@echo off
setlocal
set "WII_IP=%~1"
if "%WII_IP%"=="" set "WII_IP=192.168.0.42"
set "CMDLINE=%*"
if not "%~1"=="" (
  call set "ARGS=%%CMDLINE:*%~1=%%"
)
if not defined ARGS set "ARGS=status"
for /f "tokens=* delims= " %%A in ("%ARGS%") do set "ARGS=%%A"
py "%~dp0wiimesh_admin_udp.py" "%WII_IP%" %ARGS%
if errorlevel 1 python "%~dp0wiimesh_admin_udp.py" "%WII_IP%" %ARGS%
