@echo off
setlocal
set "BAUD=%~1"
if "%BAUD%"=="" set "BAUD=115200"

for /f "usebackq tokens=*" %%P in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0find_rak4631_com.ps1"`) do set "COMPORT=%%P"
if "%COMPORT%"=="" (
  echo Could not find RAK4631 serial COM port.
  exit /b 1
)

where putty >nul 2>nul
if errorlevel 1 (
  echo PuTTY was not found on PATH.
  echo RAK4631 is on %COMPORT%. Open PuTTY manually with Serial, %COMPORT%, speed %BAUD%.
  exit /b 1
)

echo Opening PuTTY on %COMPORT% at %BAUD%...
start "" putty -serial %COMPORT% -sercfg %BAUD%,8,n,1,N
