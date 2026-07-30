@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=%CD%\MeshLayout.config"

py "%~dp0mesh_layout_editor.py" "%CONFIG%"
exit /b %errorlevel%
