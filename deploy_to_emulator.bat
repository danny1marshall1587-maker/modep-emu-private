@echo off
title Deploy Plugins to MODEP Emulator
cd /d "%~dp0"

echo ================================================================
echo       Deploying Plugins to Emulated Patchbox OS / MODEP
echo ================================================================
echo.
echo  Target: localhost:2222 (User: patch, Pass: blokaslabs)
echo.

echo 1. Creating directories in Patchbox OS...
ssh -o StrictHostKeyChecking=no -p 2222 patch@localhost "mkdir -p ~/.lv2 /tmp/modep_plugins"

echo.
echo 2. Uploading LV2 plugin bundles...
scp -o StrictHostKeyChecking=no -P 2222 -r "%~dp0dist\*" patch@localhost:/tmp/modep_plugins/

echo.
echo 3. Installing bundles and restarting MODEP...
ssh -o StrictHostKeyChecking=no -p 2222 patch@localhost "sudo mkdir -p /var/modep/lv2 && sudo cp -rf /tmp/modep_plugins/* /var/modep/lv2/ && sudo cp -rf /tmp/modep_plugins/* ~/.lv2/ && sudo chown -R modep:modep /var/modep/lv2/*.lv2 2>/dev/null; sudo systemctl restart modep-mod-host modep-mod-ui 2>/dev/null; echo 'Installation complete!'"

echo.
echo ================================================================
echo   Plugins Deployed! Opening MODEP Web Interface in Browser...
echo ================================================================
start http://localhost:8080

pause
