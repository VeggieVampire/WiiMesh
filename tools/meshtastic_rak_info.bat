@echo off
setlocal
set "LOG=%~dp0..\..\..\outputs\rak4631-pc-serial.log"

for /f "usebackq tokens=*" %%P in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0find_rak4631_com.ps1"`) do set "COMPORT=%%P"
if "%COMPORT%"=="" (
  echo Could not find RAK4631 serial COM port.
  exit /b 1
)

> "%LOG%" echo RAK4631 PC serial capture
>> "%LOG%" echo Started: %DATE% %TIME%
>> "%LOG%" echo Device: 239a:8029
>> "%LOG%" echo Port: %COMPORT%
>> "%LOG%" echo.

echo RAK4631 is on %COMPORT%.
echo Writing log to %LOG%

py -m meshtastic --port %COMPORT% --info >> "%LOG%" 2>&1
py -m meshtastic --port %COMPORT% --nodes >> "%LOG%" 2>&1

echo.
echo Done. Log:
echo %LOG%
