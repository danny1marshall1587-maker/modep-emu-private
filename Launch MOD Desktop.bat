@echo off
title MOD Desktop Launcher
cd /d "C:\Program Files\MOD Desktop"
set "MOD_DEV_ENVIRONMENT=0"
set "MOD_DESKTOP=1"
set "MOD_DATA_DIR=%USERPROFILE%\Documents\MOD Desktop"
set "MOD_USER_FILES_DIR=%USERPROFILE%\Documents\MOD Desktop\user-files"
set "MOD_KEYS_PATH=%USERPROFILE%\Documents\MOD Desktop\keys\"
set "MOD_USER_PLUGINS_DIR=%USERPROFILE%\Documents\MOD Desktop\lv2"
set "MOD_USER_PEDALBOARDS_DIR=%USERPROFILE%\Documents\MOD Desktop\pedalboards"
start "" /D "C:\Program Files\MOD Desktop" "C:\Program Files\MOD Desktop\mod-desktop.exe"
