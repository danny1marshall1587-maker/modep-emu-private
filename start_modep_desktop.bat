@echo off
title MODEP Desktop - Native Low Latency Modular Pedalboard
cd /d "%~dp0"

echo ================================================================
echo       Starting MODEP Desktop (Native Low-Latency Audio)
echo ================================================================
echo.
echo  Host Audio:
echo   - Native Windows ASIO / WASAPI (Audient EVO 4, Focusrite, etc.)
echo   - Ultra-low latency DSP (Direct hardware access, NO QEMU)
echo.
echo  Web Interfaces:
echo   - MODEP Web UI:       http://127.0.0.1:18181
echo   - Patchstorage Cloud: http://127.0.0.1:18189 (Run open_patchstorage.bat)
echo.
echo  User Data Locations:
echo   - Pedalboards:        %USERPROFILE%\Documents\MOD Desktop\pedalboards
echo   - User Files (IR/NAM):%USERPROFILE%\Documents\MOD Desktop\user-files
echo   - Custom LV2 Plugins: %USERPROFILE%\Documents\MOD Desktop\lv2
echo ================================================================
echo.

echo 1. Initializing MODEP directories, cloud proxy & compiling plugins...
python "%~dp0setup_modep_desktop.py"
python "%~dp0build_windows_plugins.py"
python "%~dp0install_modep_patchstorage_to_desktop.py"
python "%~dp0enable_cloud_and_patchstorage_proxy.py"
python "%~dp0fix_cloudplugin_renderer.py"
python "%~dp0fix_patchstorage_and_cloud.py"
python "%~dp0ensure_p_button_visible.py"
python "%~dp0sync_exact_blokas_modep.py"
python "%~dp0fix_patchstorage_engine.py"
python "%~dp0fix_index_tornado.py"
python "%~dp0install_community_plugins.py"
python "%~dp0install_nam.py"
python "%~dp0download_nam_models.py"
python "%~dp0install_extracted_plugins_and_storage.py"
python "%~dp0install_all_windows_community_lv2.py"
python "%~dp0flatten_lv2.py"
python "%~dp0fix_windows_lv2_binaries.py"
python "%~dp0swap_all_linux_modguis.py"
python "%~dp0fix_files_list.py"

echo 2. Launching MODEP Desktop Engine (with Patchstorage Store)...
set "MOD_HTML_DIR=%USERPROFILE%\Documents\MOD Desktop\html\"
set "MOD_DATA_DIR=%USERPROFILE%\Documents\MOD Desktop"
set "MOD_USER_PLUGINS_DIR=%USERPROFILE%\Documents\MOD Desktop\lv2"
set "MOD_USER_PEDALBOARDS_DIR=%USERPROFILE%\Documents\MOD Desktop\pedalboards"
set "MOD_USER_FILES_DIR=%USERPROFILE%\Documents\MOD Desktop\user-files"
set "MOD_PEDALBOARD_TMP_DIR=%TEMP%\modep-pedalboard-tmp-data"

if exist "%~dp0modep_app\mod-desktop.exe" (
    start "" /d "%~dp0modep_app" "%~dp0modep_app\mod-desktop.exe"
) else if exist "C:\Program Files\MOD Desktop\mod-desktop.exe" (
    start "" /d "C:\Program Files\MOD Desktop" "C:\Program Files\MOD Desktop\mod-desktop.exe"
) else (
    echo [!] Error: MOD Desktop executable not found.
    pause
    exit /b 1
)

echo.
echo 3. Opening MODEP Web GUI in your default browser...
timeout /t 2 /nobreak >nul
start http://127.0.0.1:18181

echo.
echo ================================================================
echo   MODEP Desktop is now active in your system tray!
echo   - Select your audio interface (e.g. Audient EVO 4) and press Start.
echo   - To download cloud patches, run: open_patchstorage.bat
echo ================================================================
echo.
pause
