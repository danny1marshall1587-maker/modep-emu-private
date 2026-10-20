@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Elevating Administrator privileges...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
echo Current directory: %cd%
echo Running installer...
if exist "C:\Program Files\Python314\python.exe" (
    "C:\Program Files\Python314\python.exe" "%~dp0install_cyber_tone3000.py"
) else (
    python "%~dp0install_cyber_tone3000.py"
)
pause
