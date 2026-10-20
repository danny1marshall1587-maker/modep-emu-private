import subprocess
import os

p7z = r'C:\Program Files\7-Zip\7z.exe'

# Target extraction directory
EXTRACT_DIR = r"c:\Users\danny\Documents\modep emu\extracted_modep"
os.makedirs(EXTRACT_DIR, exist_ok=True)

print("Extracting /var/modep and /usr/share/mod from 1.img...")
cmd = [
    p7z, 'x', '1.img',
    r'var\modep\*',
    r'usr\share\mod\*',
    r'usr\lib\lv2\*',
    r'usr\local\modep\*',
    r'home\patch\*',
    f"-o{EXTRACT_DIR}",
    '-y'
]
res = subprocess.run(cmd, capture_output=True, text=True)
print("Extract return code:", res.returncode)
print("Extract output summary:\n", res.stdout[-400:] if res.stdout else res.stderr)
