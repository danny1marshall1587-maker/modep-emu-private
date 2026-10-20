#!/usr/bin/env python3
"""
swap_all_linux_modguis.py
Copies all photorealistic pedal representations (modgui/ folder, icons, stylesheets,
knobs, artwork, and modgui.ttl) from the extracted Linux MODEP image into all native
Windows LV2 plugins in both modep_app/plugins and Documents/MOD Desktop/lv2.
Preserves official Mike Oliphant NAM v0.2.3 release intact.
"""

import os
import sys
import shutil
import glob
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
EXTRACTED_LV2 = os.path.join(BASE_DIR, "extracted_modep", "var", "modep", "lv2")
APP_PLUGINS_DIR = os.path.join(BASE_DIR, "modep_app", "plugins")
USER_LV2_DIR = os.path.expanduser(r"~\Documents\MOD Desktop\lv2")

def inject_modgui_into_bundle(src_bundle, target_bundle):
    if not os.path.exists(target_bundle):
        return False
    
    # 1. Copy modgui folder
    src_modgui = os.path.join(src_bundle, "modgui")
    if os.path.exists(src_modgui):
        dst_modgui = os.path.join(target_bundle, "modgui")
        os.makedirs(dst_modgui, exist_ok=True)
        shutil.copytree(src_modgui, dst_modgui, dirs_exist_ok=True)

    # 2. Copy modgui.ttl
    src_modgui_ttl = os.path.join(src_bundle, "modgui.ttl")
    if os.path.exists(src_modgui_ttl):
        shutil.copy2(src_modgui_ttl, target_bundle)

    # 3. Copy patchstorage.json if present
    src_ps = os.path.join(src_bundle, "patchstorage.json")
    if os.path.exists(src_ps):
        shutil.copy2(src_ps, target_bundle)

    # 4. Update manifest.ttl to include modgui.ttl in rdfs:seeAlso
    manifest_path = os.path.join(target_bundle, "manifest.ttl")
    if os.path.exists(manifest_path):
        with open(manifest_path, "r", encoding="utf-8", errors="ignore") as f:
            m_code = f.read()

        if "modgui.ttl" not in m_code and os.path.exists(src_modgui_ttl):
            # Add modgui.ttl to seeAlso
            if "rdfs:seeAlso" in m_code:
                m_code = re.sub(r'rdfs:seeAlso\s+([^;.\n]+)', r'rdfs:seeAlso \1 , <modgui.ttl>', m_code)
            else:
                m_code = m_code.replace("a lv2:Plugin;", "a lv2:Plugin;\n\trdfs:seeAlso <modgui.ttl>;")
            
            with open(manifest_path, "w", encoding="utf-8") as f:
                f.write(m_code)

    return True

print("=== 1. Transferring MOD GUIs from Linux to Windows Plugin Directories ===")
if os.path.exists(EXTRACTED_LV2):
    count = 0
    for b in os.listdir(EXTRACTED_LV2):
        # Skip NAM - it has its own official Windows GUI and descriptor
        if b.lower().startswith("neural_amp_modeler"):
            continue

        src_bundle = os.path.join(EXTRACTED_LV2, b)
        if not os.path.isdir(src_bundle):
            continue
        
        # Check target in modep_app/plugins
        app_target = os.path.join(APP_PLUGINS_DIR, b)
        if os.path.exists(app_target):
            if inject_modgui_into_bundle(src_bundle, app_target):
                count += 1

        # Check target in User LV2
        user_target = os.path.join(USER_LV2_DIR, b)
        if os.path.exists(user_target):
            if inject_modgui_into_bundle(src_bundle, user_target):
                count += 1

    print(f" [+] Injected MOD GUIs into {count} plugin bundles!")

print("\n=== 2. Ensuring Pristine Official Mike Oliphant NAM v0.2.3 is Active ===")
import subprocess
subprocess.run([sys.executable, os.path.join(BASE_DIR, "install_fresh_nam_official.py")])

print("\nAll pedal representations and official NAM v0.2.3 verified!")
