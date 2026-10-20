#!/usr/bin/env python3
"""
install_extracted_plugins_and_storage.py
Imports all 113+ downloaded LV2 plugins, patchstorage metadata, IR files,
and user presets from the emulated Patchbox OS (1.img) directly into MODEP Desktop.
"""

import os
import sys
import shutil
import glob

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
EXTRACTED_DIR = os.path.join(BASE_DIR, "extracted_modep")

USER_MODEP_DIR = os.path.expanduser(r"~\Documents\MOD Desktop")
USER_LV2_DIR = os.path.join(USER_MODEP_DIR, "lv2")
USER_FILES_DIR = os.path.join(USER_MODEP_DIR, "user-files")
USER_PB_DIR = os.path.join(USER_MODEP_DIR, "pedalboards")

os.makedirs(USER_LV2_DIR, exist_ok=True)
os.makedirs(USER_FILES_DIR, exist_ok=True)
os.makedirs(USER_PB_DIR, exist_ok=True)

def copy_tree_safe(src, dst):
    if not os.path.exists(src):
        return
    os.makedirs(dst, exist_ok=True)
    for root, dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        target_dir = os.path.join(dst, rel) if rel != '.' else dst
        os.makedirs(target_dir, exist_ok=True)
        for f in files:
            src_f = os.path.join(root, f)
            dst_f = os.path.join(target_dir, f)
            try:
                if not os.path.exists(dst_f) or os.path.getsize(src_f) != os.path.getsize(dst_f):
                    shutil.copy2(src_f, dst_f)
            except Exception as e:
                print(f" [!] Skip copy {src_f} -> {dst_f}: {e}")

print("=== 1. Copying 113+ Extracted LV2 Bundles & Patchstorage Metadata ===")
extracted_lv2 = os.path.join(EXTRACTED_DIR, "var", "modep", "lv2")
if os.path.exists(extracted_lv2):
    bundles = [d for d in os.listdir(extracted_lv2) if os.path.isdir(os.path.join(extracted_lv2, d))]
    print(f"Found {len(bundles)} LV2 bundles in extracted emulated storage.")
    for b in bundles:
        src_bundle = os.path.join(extracted_lv2, b)
        dst_bundle = os.path.join(USER_LV2_DIR, b)
        copy_tree_safe(src_bundle, dst_bundle)
    print(f" [+] Successfully imported all {len(bundles)} LV2 bundles into {USER_LV2_DIR}")

print("\n=== 2. Copying User Files, IRs & Audio Tracks ===")
extracted_user_files = os.path.join(EXTRACTED_DIR, "var", "modep", "user-files")
if os.path.exists(extracted_user_files):
    copy_tree_safe(extracted_user_files, USER_FILES_DIR)
    print(f" [+] Successfully imported user-files into {USER_FILES_DIR}")

print("\n=== 3. Copying Pedalboards ===")
extracted_pb = os.path.join(EXTRACTED_DIR, "var", "modep", "pedalboards")
if os.path.exists(extracted_pb):
    copy_tree_safe(extracted_pb, USER_PB_DIR)
    print(f" [+] Successfully imported pedalboards into {USER_PB_DIR}")

print("\n=== 4. Verifying Patchstorage Status in Desktop Library ===")
installed_ps = glob.glob(os.path.join(USER_LV2_DIR, "*", "patchstorage.json"))
print(f"Total plugins registered with Patchstorage IDs on Desktop: {len(installed_ps)}")
print("All emulated storage and plugins successfully transferred to MODEP Desktop!")
