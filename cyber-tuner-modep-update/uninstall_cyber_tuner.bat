@echo off
NET SESSION >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
    python "%~dp0uninstall_cyber_tuner.py"
) ELSE (
    echo Elevating Administrator privileges to restore MOD Desktop...
    powershell -Command "Start-Process python -ArgumentList '\"%~dp0uninstall_cyber_tuner.py\"' -Verb RunAs -Wait"
)
pause
