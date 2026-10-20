#!/usr/bin/env python3
"""
install_all_windows_community_lv2.py
Downloads and installs native Windows 64-bit LV2 binaries for:
1. Airwindows LV2 (300+ native Windows plugins)
2. Guitarix GxPlugins.lv2 (all Gx native Windows plugins)
3. Preserves all MOD GUIs, screenshots, and patchstorage.json metadata!
"""

import os
import sys
import urllib.request
import zipfile
import shutil
import subprocess
import glob

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
EXTRACTED_DIR = os.path.join(BASE_DIR, "extracted_modep")
USER_LV2_DIR = os.path.expanduser(r"~\Documents\MOD Desktop\lv2")
TMP_DIR = os.path.join(BASE_DIR, "tmp_community_downloads")

os.makedirs(USER_LV2_DIR, exist_ok=True)
os.makedirs(TMP_DIR, exist_ok=True)

p7z = r"C:\Program Files\7-Zip\7z.exe"

def download_and_extract(url, target_zip):
    print(f"Downloading: {url}")
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=30) as r, open(target_zip, 'wb') as f:
        f.write(r.read())
    print(f" [+] Downloaded ({os.path.getsize(target_zip)} bytes)")

# 1. Download Airwindows LV2 for Windows 64-bit
airwindows_url = "https://github.com/hannesbraun/airwindows-lv2/releases/download/v20.0/airwindows-lv2-amd64-windows-20.0.zip"
airwindows_zip = os.path.join(TMP_DIR, "airwindows.zip")
if not os.path.exists(airwindows_zip):
    try:
        download_and_extract(airwindows_url, airwindows_zip)
    except Exception as e:
        print(" [!] Error downloading Airwindows:", e)

if os.path.exists(airwindows_zip):
    print("Extracting Airwindows LV2 plugins to User LV2...")
    with zipfile.ZipFile(airwindows_zip, 'r') as zf:
        zf.extractall(USER_LV2_DIR)
    print(" [+] Extracted Airwindows LV2 suite!")

# 2. Download GxPlugins for Windows 64-bit
gx_url = "https://github.com/brummer10/GxPlugins.lv2/releases/download/master/gxplugins_master_win64.7z"
gx_7z = os.path.join(TMP_DIR, "gxplugins.7z")
if not os.path.exists(gx_7z):
    try:
        download_and_extract(gx_url, gx_7z)
    except Exception as e:
        print(" [!] Error downloading GxPlugins:", e)

if os.path.exists(gx_7z) and os.path.exists(p7z):
    print("Extracting GxPlugins LV2 suite to User LV2...")
    subprocess.run([p7z, 'x', gx_7z, f"-o{USER_LV2_DIR}", '-y'], capture_output=True)
    print(" [+] Extracted GxPlugins LV2 suite!")

# 3. Inject MOD GUIs and patchstorage.json from extracted_modep into the native Windows bundles
print("\n=== Injecting MOD GUIs and patchstorage.json into native Windows bundles ===")
extracted_lv2 = os.path.join(EXTRACTED_DIR, "var", "modep", "lv2")
if os.path.exists(extracted_lv2):
    for d in os.listdir(extracted_lv2):
        src_bundle = os.path.join(extracted_lv2, d)
        if not os.path.isdir(src_bundle):
            continue
        
        # Match against USER_LV2_DIR (case insensitive check)
        target_bundle = os.path.join(USER_LV2_DIR, d)
        if not os.path.exists(target_bundle):
            # Check if exists with different casing
            for existing in os.listdir(USER_LV2_DIR):
                if existing.lower() == d.lower():
                    target_bundle = os.path.join(USER_LV2_DIR, existing)
                    break
        
        if os.path.exists(target_bundle):
            # Copy modgui folder if present
            src_modgui = os.path.join(src_bundle, "modgui")
            if os.path.exists(src_modgui):
                dst_modgui = os.path.join(target_bundle, "modgui")
                if not os.path.exists(dst_modgui):
                    shutil.copytree(src_modgui, dst_modgui, dirs_exist_ok=True)
            
            # Copy modgui.ttl
            src_modgui_ttl = os.path.join(src_bundle, "modgui.ttl")
            if os.path.exists(src_modgui_ttl):
                shutil.copy2(src_modgui_ttl, target_bundle)

            # Copy patchstorage.json
            src_ps = os.path.join(src_bundle, "patchstorage.json")
            if os.path.exists(src_ps):
                shutil.copy2(src_ps, target_bundle)

print("\n=== Sanitizing all manifests and removing any leftover Linux .so files ===")
subprocess.run([sys.executable, os.path.join(BASE_DIR, "fix_windows_lv2_binaries.py")])

print("\nComplete native Windows 64-bit LV2 plugin ecosystem is installed and ready!")
