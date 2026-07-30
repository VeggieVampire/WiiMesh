@echo off
setlocal

if "%~1"=="" (
  echo Usage: %~nx0 image.png [output.rgb565]
  echo Example: %~nx0 zelda-theme.png background.rgb565
  exit /b 1
)

set "OUT=%~2"
if "%OUT%"=="" set "OUT=background.rgb565"

py "%~dp0make_theme_background.py" "%~1" "%OUT%"
exit /b %errorlevel%
