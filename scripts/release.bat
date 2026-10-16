@echo off
setlocal
cd /d "%~dp0.."
python "%~dp0release.py"
if errorlevel 1 (
  echo.
  echo Release packaging failed.
  pause
  exit /b 1
)
pause
