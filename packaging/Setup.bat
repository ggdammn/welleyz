@echo off
setlocal
cd /d "%~dp0"
start "" /wait "%~dp0welleyz-0.4.0-setup.exe"
if errorlevel 1 (
  echo.
  echo Installation failed. See the error above.
  pause
  exit /b 1
)
