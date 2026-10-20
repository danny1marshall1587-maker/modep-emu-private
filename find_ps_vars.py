import urllib.request
import re

html = urllib.request.urlopen('http://127.0.0.1:8080/').read().decode('utf-8')
# Find all script src in html
scripts = re.findall(r'<script[^>]+src=["\']([^"\']+)["\']', html)
print("Scripts loaded in 8080:")
for s in scripts:
    print(" ", s)
    if not s.startswith('http'):
        url = 'http://127.0.0.1:8080' + (s if s.startswith('/') else '/' + s)
    else:
        url = s
    try:
        content = urllib.request.urlopen(url, timeout=3).read().decode('utf-8', errors='ignore')
        for m in re.finditer(r'(PATCHSTORAGE\w*\s*=[^;\n]+;)', content):
            print("   ->", m.group(0))
    except Exception as e:
        print(f"   -> Error reading {url}: {e}")
