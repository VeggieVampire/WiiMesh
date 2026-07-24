@echo off
setlocal
py "%~dp0listen_udp_logs.py"
if errorlevel 1 python "%~dp0listen_udp_logs.py"
