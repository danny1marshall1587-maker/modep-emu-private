@echo off
title MODEP Desktop - Cloud & Patchstorage Community Manager
cd /d "%~dp0"

echo ================================================================
echo      MODEP Desktop Cloud & Patchstorage Community Manager
echo ================================================================
echo.
echo  - Browse & Search Community Patches & Plugins
echo  - 1-Click Install to MODEP Desktop
echo  - Auto-sorts Pedalboards, LV2s, NAM Models & Cabinet IRs
echo.
echo  Opening Dashboard at http://127.0.0.1:18189 ...
echo ================================================================
echo.

python "%~dp0patchstorage_manager.py"

pause
