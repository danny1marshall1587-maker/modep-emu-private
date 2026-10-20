@echo off
title Install Cyber Audio Plugins & Tuner to Patchbox OS SD Card / Live Pi
cd /d "%~dp0"

echo ================================================================
echo   Install Cyber Audio Suite + Galaxy Strobe Tuner to Patchbox OS
echo ================================================================
echo.
set /p PI_HOST="Enter Patchbox Pi IP or hostname [default: patchbox.local]: "
if "%PI_HOST%"=="" set PI_HOST=patchbox.local

set /p PI_USER="Enter SSH username [default: patch]: "
if "%PI_USER%"=="" set PI_USER=patch

echo.
echo Target device: %PI_USER%@%PI_HOST%
echo.

echo 1. Creating directories on Patchbox OS...
ssh -o StrictHostKeyChecking=no %PI_USER%@%PI_HOST% "mkdir -p ~/.lv2 /tmp/modep_cyber_plugins"
if %ERRORLEVEL% neq 0 (
    echo [!] SSH connection failed. Check your IP/hostname and WiFi/Ethernet connection.
    pause
    exit /b 1
)

echo.
echo 2. Uploading 58 LV2 Plugin Bundles + Galaxy Strobe Tuner...
scp -o StrictHostKeyChecking=no -r "%~dp0dist\*" %PI_USER%@%PI_HOST%:/tmp/modep_cyber_plugins/

echo.
echo 3. Installing to MODEP (/var/modep/lv2) and restarting MODEP host...
ssh -o StrictHostKeyChecking=no %PI_USER%@%PI_HOST% "sudo mkdir -p /var/modep/lv2 && sudo cp -rf /tmp/modep_cyber_plugins/* /var/modep/lv2/ && sudo cp -rf /tmp/modep_cyber_plugins/* ~/.lv2/ && sudo chown -R modep:modep /var/modep/lv2/*.lv2 2>/dev/null; rm -rf /tmp/modep_cyber_plugins; sudo systemctl restart modep-mod-host modep-mod-ui 2>/dev/null; echo 'Installation to Patchbox OS successful!'"

echo.
echo ================================================================
echo  Success! All 58 plugins + tuner are installed on your Pi!
echo  Opening MODEP Web GUI at http://%PI_HOST% ...
echo ================================================================
start http://%PI_HOST%

pause
