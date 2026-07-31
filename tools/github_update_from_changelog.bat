@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%.."
set "LOG=%SCRIPT_DIR%github_update.log"
set "MSG_FILE=%TEMP%\wiimesh_github_commit_msg.txt"

> "%LOG%" echo WiiMesh GitHub update log
>> "%LOG%" echo Started: %DATE% %TIME%
>> "%LOG%" echo Root: %ROOT%
>> "%LOG%" echo.

where git >nul 2>nul
if errorlevel 1 (
  echo git was not found.
  >> "%LOG%" echo ERROR: git was not found.
  exit /b 1
)

echo Building WiiMesh before GitHub update...
call "%SCRIPT_DIR%build_with_output_icons.bat" >> "%LOG%" 2>&1
if errorlevel 1 (
  echo Build failed. Nothing committed.
  echo See "%LOG%"
  >> "%LOG%" echo ERROR: build failed.
  exit /b 1
)

pushd "%ROOT%"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p = 'CHANGELOG.md';" ^
  "$lines = Get-Content $p;" ^
  "$start = ($lines | Select-String -Pattern '^## ' | Select-Object -First 1).LineNumber - 1;" ^
  "if ($start -lt 0) { throw 'No changelog section found' }" ^
  "$endMatch = $lines[($start + 1)..($lines.Count - 1)] | Select-String -Pattern '^## ' | Select-Object -First 1;" ^
  "$end = if ($endMatch) { $start + $endMatch.LineNumber } else { $lines.Count };" ^
  "$section = $lines[$start..($end - 1)];" ^
  "$title = ($section[0] -replace '^##\s*','').Trim();" ^
  "$subject = 'Update WiiMesh to ' + (($title -split '\s+')[0]);" ^
  "$body = @($subject, '', ($section -join [Environment]::NewLine));" ^
  "Set-Content -Path '%MSG_FILE%' -Value $body -Encoding UTF8" >> "%LOG%" 2>&1
if errorlevel 1 (
  popd
  echo Could not create commit message from CHANGELOG.md.
  echo See "%LOG%"
  exit /b 1
)

for /f "usebackq delims=" %%S in (`powershell -NoProfile -Command "(Get-Content '%MSG_FILE%' -First 1)"`) do set "SUBJECT=%%S"
if "%SUBJECT%"=="" set "SUBJECT=Update WiiMesh"

git status -sb >> "%LOG%" 2>&1
git diff --quiet
if not errorlevel 1 (
  git diff --cached --quiet
  if not errorlevel 1 (
    popd
    echo No Git changes to commit.
    >> "%LOG%" echo No Git changes to commit.
    exit /b 0
  )
)

echo Staging changes...
git add -A >> "%LOG%" 2>&1
if errorlevel 1 (
  popd
  echo Git add failed. See "%LOG%"
  exit /b 1
)

echo Committing: %SUBJECT%
git commit -F "%MSG_FILE%" >> "%LOG%" 2>&1
if errorlevel 1 (
  popd
  echo Git commit failed. See "%LOG%"
  exit /b 1
)

echo Pushing to GitHub...
git push >> "%LOG%" 2>&1
if errorlevel 1 (
  popd
  echo Git push failed. See "%LOG%"
  exit /b 1
)

popd
echo Done. Pushed GitHub update: %SUBJECT%
echo Log: "%LOG%"
exit /b 0
