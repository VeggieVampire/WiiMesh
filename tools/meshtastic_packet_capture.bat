@echo off
setlocal
set "SECONDS=%~1"
if "%SECONDS%"=="" set "SECONDS=120"
set "LOG=%~dp0..\..\..\outputs\meshtastic-packets.jsonl"

for /f "usebackq tokens=*" %%P in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0find_rak4631_com.ps1"`) do set "COMPORT=%%P"
if "%COMPORT%"=="" (
  echo Could not find RAK4631 serial COM port.
  exit /b 1
)

echo Capturing Meshtastic Python packets from %COMPORT% for %SECONDS% seconds...
echo Output: %LOG%
py "%~dp0meshtastic_packet_capture.py" "%COMPORT%" "%SECONDS%" "%LOG%"
