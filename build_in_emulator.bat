@echo off
title Build and Install Plugins Inside Patchbox OS Emulator
cd /d "%~dp0"

echo ================================================================
echo     Building & Installing Plugins Inside Patchbox OS (ARM)
echo ================================================================
echo.
echo  This script uploads the source code to the emulated Raspberry Pi,
echo  compiles the native ARM shared libraries (.so), and installs them.
echo.

echo 1. Creating workspace directory in Patchbox OS...
ssh -o StrictHostKeyChecking=no -p 2222 patch@localhost "mkdir -p ~/modep_build"

echo.
echo 2. Uploading plugin source code and installation script...
scp -o StrictHostKeyChecking=no -P 2222 -r "%~dp0plugins" "%~dp0install_to_modep.sh" patch@localhost:~/modep_build/

echo.
echo 3. Running native build and install script on ARM Linux...
ssh -o StrictHostKeyChecking=no -p 2222 patch@localhost "cd ~/modep_build && chmod +x install_to_modep.sh && ./install_to_modep.sh"

echo.
echo ================================================================
echo   Build & Install Complete! Opening MODEP Web UI...
echo ================================================================
start http://localhost:8080

pause
