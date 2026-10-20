@echo off
NET SESSION >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
    python "%~dp0install_cyber_tuner.py"
) ELSE (
    echo Elevating Administrator privileges to install into MOD Desktop...
    powershell -Command "Start-Process python -ArgumentList '\"%~dp0install_cyber_tuner.py\"' -Verb RunAs -Wait"
)
pause
