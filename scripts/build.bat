@echo off
chcp 65001 >nul
cd /d "%~dp0.."
echo Building welleyz with CMake...
cmake -S . -B build
if errorlevel 1 exit /b 1
cmake --build build --config Release --target Welleyz_VST3 Welleyz_Standalone
if errorlevel 1 exit /b 1
pause
