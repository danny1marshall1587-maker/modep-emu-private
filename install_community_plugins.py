import os
import sys
import urllib.request
import zipfile
import shutil

USER_LV2_DIR = os.path.expanduser(r'~\Documents\MOD Desktop\lv2')
TEMP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'temp_plugins')

os.makedirs(USER_LV2_DIR, exist_ok=True)
os.makedirs(TEMP_DIR, exist_ok=True)

plugin_urls = [
    ("DPF-Plugins", "https://github.com/DISTRHO/DPF-Plugins/releases/download/v1.7/DPF-Plugins-v1.7-win64.zip"),
    ("AIDA-X", "https://github.com/AidaDSP/AIDA-X/releases/download/1.1.0/AIDA-X-1.1.0-win64.zip"),
    ("Dragonfly-Reverb", "https://github.com/michaelwillis/dragonfly-reverb/releases/download/3.2.10/dragonfly-reverb-3.2.10-win64.zip"),
    ("Sfizz", "https://github.com/sfztools/sfizz/releases/download/1.2.3/sfizz-1.2.3-win64.zip")
]

def download_and_extract(name, url):
    zip_path = os.path.join(TEMP_DIR, f"{name}.zip")
    extract_path = os.path.join(TEMP_DIR, name)
    print(f"[*] Downloading {name} from {url}...")
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=30) as resp, open(zip_path, 'wb') as out_f:
            shutil.copyfileobj(resp, out_f)
        print(f" [+] Downloaded {name} ({os.path.getsize(zip_path)} bytes)")

        with zipfile.ZipFile(zip_path, 'r') as zf:
            zf.extractall(extract_path)

        # Locate .lv2 bundles in extract_path
        installed_count = 0
        for root, dirs, files in os.walk(extract_path):
            for d in dirs:
                if d.endswith('.lv2'):
                    src = os.path.join(root, d)
                    dst = os.path.join(USER_LV2_DIR, d)
                    if os.path.exists(dst):
                        shutil.rmtree(dst)
                    shutil.copytree(src, dst)
                    print(f"    -> Installed {d} to {USER_LV2_DIR}")
                    installed_count += 1
        print(f" [+] Finished {name}: {installed_count} LV2 bundles installed.\n")
    except Exception as e:
        print(f" [!] Error processing {name}: {e}\n")

for name, url in plugin_urls:
    download_and_extract(name, url)

print("\n=== Current LV2 bundles in user folder ===")
bundles = [b for b in os.listdir(USER_LV2_DIR) if os.path.isdir(os.path.join(USER_LV2_DIR, b)) and b.endswith('.lv2')]
print(f"Total: {len(bundles)} plugins installed:")
for b in sorted(bundles):
    print(f"  - {b}")
