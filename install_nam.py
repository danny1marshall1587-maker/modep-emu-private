import os
import sys
import urllib.request
import zipfile
import shutil

USER_LV2_DIR = os.path.expanduser(r'~\Documents\MOD Desktop\lv2')
USER_FILES_DIR = os.path.expanduser(r'~\Documents\MOD Desktop\user-files')
USER_MODELS_DIR = os.path.join(USER_FILES_DIR, 'NAM-Models')
USER_IR_DIR = os.path.join(USER_FILES_DIR, 'Speaker-Cabinets-IRs')
TEMP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'temp_plugins')

os.makedirs(USER_LV2_DIR, exist_ok=True)
os.makedirs(USER_MODELS_DIR, exist_ok=True)
os.makedirs(USER_IR_DIR, exist_ok=True)
os.makedirs(TEMP_DIR, exist_ok=True)

nam_url = "https://github.com/mikeoliphant/neural-amp-modeler-lv2/releases/download/v0.2.3/neural_amp_modeler_lv2_win_x64.zip"
zip_path = os.path.join(TEMP_DIR, "nam_win64.zip")
extract_path = os.path.join(TEMP_DIR, "nam_extracted")

print(f"[*] Downloading latest Neural Amp Modeler (NAM v0.2.3) from {nam_url}...")
try:
    req = urllib.request.Request(nam_url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=30) as resp, open(zip_path, 'wb') as out_f:
        shutil.copyfileobj(resp, out_f)
    print(f" [+] Downloaded NAM ({os.path.getsize(zip_path)} bytes)")

    if os.path.exists(extract_path):
        shutil.rmtree(extract_path)
    with zipfile.ZipFile(zip_path, 'r') as zf:
        zf.extractall(extract_path)

    # Locate .lv2
    for root, dirs, files in os.walk(extract_path):
        for d in dirs:
            if d.endswith('.lv2'):
                src = os.path.join(root, d)
                dst = os.path.join(USER_LV2_DIR, d)
                if os.path.exists(dst):
                    shutil.rmtree(dst)
                shutil.copytree(src, dst)
                print(f" [+] Successfully installed {d} to {USER_LV2_DIR}")
except Exception as e:
    print(" [!] Error installing NAM:", e)
