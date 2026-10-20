import urllib.request
import zipfile
import os

url = "https://github.com/mikeoliphant/neural-amp-modeler-lv2/releases/download/v0.2.3/neural_amp_modeler_lv2_win_x64.zip"
target_zip = r"c:\Users\danny\Documents\modep emu\tmp_nam_fresh.zip"

print("Downloading fresh official NAM release...")
req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
with urllib.request.urlopen(req, timeout=15) as r, open(target_zip, 'wb') as f:
    f.write(r.read())
print(f"Downloaded {os.path.getsize(target_zip)} bytes.")

with zipfile.ZipFile(target_zip, 'r') as zf:
    print("Files in official NAM release:")
    for info in zf.infolist():
        print(f"  {info.filename} ({info.file_size} bytes)")
