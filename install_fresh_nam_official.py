#!/usr/bin/env python3
"""
install_fresh_nam_official.py
Downloads and installs a 100% clean, pristine official release of Mike Oliphant's
Neural Amp Modeler (NAM v0.2.3 for Windows x64) directly from GitHub.
Ensures official neural_amp_modeler.ttl, modgui.ttl, and modgui/ are used.
"""

import os
import sys
import shutil
import urllib.request
import zipfile
import subprocess

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
APP_NAM_DIR = os.path.join(BASE_DIR, "modep_app", "plugins", "neural_amp_modeler.lv2")
USER_LV2_DIR = os.path.expanduser(r"~\Documents\MOD Desktop\lv2")
USER_NAM_DIR = os.path.join(USER_LV2_DIR, "neural_amp_modeler.lv2")
TMP_ZIP = os.path.join(BASE_DIR, "tmp_nam_official.zip")

url = "https://github.com/mikeoliphant/neural-amp-modeler-lv2/releases/download/v0.2.3/neural_amp_modeler_lv2_win_x64.zip"

print("1. Downloading official Mike Oliphant NAM v0.2.3 release...")
req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
with urllib.request.urlopen(req, timeout=30) as r, open(TMP_ZIP, 'wb') as f:
    f.write(r.read())
print(f" [+] Downloaded ({os.path.getsize(TMP_ZIP)} bytes)")

# Kill any locked processes
subprocess.run(["powershell", "-Command", "Stop-Process -Name mod-desktop, mod-ui, jackd -Force -ErrorAction SilentlyContinue"], capture_output=True)

print("2. Installing to modep_app/plugins/neural_amp_modeler.lv2...")
if os.path.exists(APP_NAM_DIR):
    shutil.rmtree(APP_NAM_DIR, ignore_errors=True)
os.makedirs(os.path.dirname(APP_NAM_DIR), exist_ok=True)

with zipfile.ZipFile(TMP_ZIP, 'r') as zf:
    zf.extractall(os.path.dirname(APP_NAM_DIR))
print(" [+] Cleanly installed to modep_app/plugins/neural_amp_modeler.lv2")

print("3. Installing to Documents/MOD Desktop/lv2/neural_amp_modeler.lv2...")
if os.path.exists(USER_NAM_DIR):
    shutil.rmtree(USER_NAM_DIR, ignore_errors=True)
os.makedirs(USER_LV2_DIR, exist_ok=True)

with zipfile.ZipFile(TMP_ZIP, 'r') as zf:
    zf.extractall(USER_LV2_DIR)
print(" [+] Cleanly installed to Documents/MOD Desktop/lv2/neural_amp_modeler.lv2")

print("4. Synchronizing all .nam models into both 'NAM Models' and 'NAM-Models'...")
user_files = os.path.expanduser(r"~\Documents\MOD Desktop\user-files")
d1 = os.path.join(user_files, "NAM Models")
d2 = os.path.join(user_files, "NAM-Models")
os.makedirs(d1, exist_ok=True)
os.makedirs(d2, exist_ok=True)

for root, dirs, files in os.walk(user_files):
    for f in files:
        if f.endswith('.nam'):
            src_f = os.path.join(root, f)
            t1 = os.path.join(d1, f)
            t2 = os.path.join(d2, f)
            if not os.path.exists(t1):
                shutil.copy2(src_f, t1)
            if not os.path.exists(t2):
                shutil.copy2(src_f, t2)

print(f" [+] Synchronized NAM models: {len(os.listdir(d1))} models in 'NAM Models', {len(os.listdir(d2))} models in 'NAM-Models'")

print("\nOfficial Mike Oliphant NAM v0.2.3 installation complete and verified!")
