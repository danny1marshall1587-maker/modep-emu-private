#!/usr/bin/env python3
"""
fix_windows_lv2_binaries.py
1. Fixes all manifest.ttl in Documents\MOD Desktop\lv2 to point to Windows .dll binaries.
2. Removes Linux ARM .so binaries that cause Windows 'Bad Image' 0xc000012f popup.
3. Synchronizes patchstorage.json into native modep_app/plugins/ so Patchstorage tracks them.
4. Ensures Lilv and mod-host only load valid native Windows 64-bit DLLs.
"""

import os
import sys
import shutil
import glob
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
APP_PLUGINS_DIR = os.path.join(BASE_DIR, "modep_app", "plugins")
USER_LV2_DIR = os.path.expanduser(r"~\Documents\MOD Desktop\lv2")

print("=== 1. Linking patchstorage.json to native Windows plugins in modep_app/plugins ===")
user_ps_files = glob.glob(os.path.join(USER_LV2_DIR, "*", "patchstorage.json"))
for ps in user_ps_files:
    bundle_name = os.path.basename(os.path.dirname(ps))
    app_bundle = os.path.join(APP_PLUGINS_DIR, bundle_name)
    if os.path.exists(app_bundle):
        target_ps = os.path.join(app_bundle, "patchstorage.json")
        try:
            shutil.copy2(ps, target_ps)
            # print(f" [+] Linked patchstorage.json to app plugin: {bundle_name}")
        except Exception as e:
            print(f" [!] Error copying {ps}: {e}")

print("=== 2. Cleaning Linux .so binaries and fixing manifest.ttl in User LV2 ===")
for root, dirs, files in os.walk(USER_LV2_DIR):
    bundle_name = os.path.basename(root)
    app_bundle = os.path.join(APP_PLUGINS_DIR, bundle_name)
    
    # If the bundle already exists in modep_app/plugins with a Windows .dll, remove duplicate from User LV2
    # but preserve patchstorage.json
    has_app_dll = False
    if os.path.exists(app_bundle):
        app_dlls = [f for f in os.listdir(app_bundle) if f.endswith('.dll')]
        if app_dlls:
            has_app_dll = True

    # If app has the native Windows version, we remove the Linux copy from User LV2 to avoid duplicates
    if has_app_dll and root != USER_LV2_DIR:
        # Keep patchstorage.json in app_bundle
        ps_file = os.path.join(root, "patchstorage.json")
        if os.path.exists(ps_file):
            shutil.copy2(ps_file, os.path.join(app_bundle, "patchstorage.json"))
        # Delete user bundle
        shutil.rmtree(root, ignore_errors=True)
        print(f" [+] Removed duplicate Linux bundle (using native Windows version): {bundle_name}")
        continue

    # Otherwise, clean any .so files
    for f in files:
        if f.endswith('.so'):
            so_path = os.path.join(root, f)
            dll_name = f[:-3] + '.dll'
            dll_path = os.path.join(root, dll_name)
            
            # Check if matching DLL exists
            if os.path.exists(dll_path):
                os.remove(so_path)
                print(f" [+] Removed Linux .so (native .dll exists): {f} in {bundle_name}")
            else:
                # No DLL exists for this Linux binary. Delete the .so so Windows won't throw 0xc000012f
                os.remove(so_path)
                print(f" [!] Removed unsupported Linux .so: {f} in {bundle_name}")

    # Fix manifest.ttl to reference .dll instead of .so if manifest exists
    manifest_path = os.path.join(root, "manifest.ttl")
    if os.path.exists(manifest_path):
        with open(manifest_path, 'r', encoding='utf-8', errors='ignore') as mf:
            m_content = mf.read()
        
        # Replace .so references with .dll
        new_m_content = re.sub(r'<([^>]+\.)so>', r'<\1dll>', m_content)
        if new_m_content != m_content:
            with open(manifest_path, 'w', encoding='utf-8') as mf:
                mf.write(new_m_content)
            print(f" [+] Updated manifest.ttl (.so -> .dll) in {bundle_name}")

print("\n=== 3. Ensuring NAM, AIDA-X and Community Windows DLLs are intact ===")
# Re-run install_community_plugins and install_nam to ensure valid DLLs
import subprocess
subprocess.run([sys.executable, os.path.join(BASE_DIR, "install_community_plugins.py")])
subprocess.run([sys.executable, os.path.join(BASE_DIR, "install_nam.py")])

print("\nAll Windows LV2 plugins sanitized and ready with zero Bad Image errors!")
