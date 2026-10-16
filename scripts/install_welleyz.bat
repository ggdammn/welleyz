@echo off
setlocal
set "SETUP=%~dp0..\local\distributions\release\welleyz-0.4.0-windows-x64\welleyz-0.4.0-setup.exe"
if not exist "%SETUP%" (
  echo Release package not found. Run scripts\release.bat first.
  pause
  exit /b 1
)
call "%SETUP%"
