import subprocess
import re

p7z = r'C:\Program Files\7-Zip\7z.exe'
res = subprocess.run([p7z, 'l', '1.img'], capture_output=True, text=True)

lv2_paths = []
patchstorage_paths = []
mod_paths = []

for line in res.stdout.splitlines():
    parts = line.split()
    if len(parts) >= 6:
        path = parts[-1]
        if '.lv2' in path:
            lv2_paths.append(path)
        if 'patchstorage' in path.lower():
            patchstorage_paths.append(path)
        if 'mod-ui' in path or 'mod_app' in path or 'modep' in path:
            mod_paths.append(path)

print(f"=== Found {len(lv2_paths)} LV2 files/folders inside 1.img ===")
# Find unique .lv2 bundles
bundles = set()
for p in lv2_paths:
    m = re.search(r'([^/\\]+\.lv2)', p)
    if m:
        bundles.add(m.group(1))

print(f"Total Unique LV2 Bundles found in emulated MODEP: {len(bundles)}")
for b in sorted(bundles):
    print("  *", b)

print(f"\n=== Patchstorage / MOD paths found ({len(patchstorage_paths)}): ===")
for p in sorted(set(patchstorage_paths))[:30]:
    print("  -", p)
