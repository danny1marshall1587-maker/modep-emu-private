import urllib.request
import re

def fetch(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=5) as resp:
        return resp.read().decode('utf-8', errors='ignore')

# 1. Fetch index.html
html = fetch('http://127.0.0.1:8080/')
print("=== Constants in 8080 index.html ===")
for match in re.finditer(r'(PATCHSTORAGE\w*\s*=\s*[^;\n]+;)', html):
    print(" ", match.group(0))

# 2. Fetch patchstorage.js from 8080
ps_js = fetch('http://127.0.0.1:8080/js/patchstorage.js')
with open(r'modep_app\html\js\patchstorage.js', 'w', encoding='utf-8') as f:
    f.write(ps_js)
print(" [+] Saved exact patchstorage.js from 8080 to modep_app")

# 3. Fetch templates from 8080
tpl_card = fetch('http://127.0.0.1:8080/include/patchstorage_plugin.html')
with open(r'modep_app\html\include\patchstorage_plugin.html', 'w', encoding='utf-8') as f:
    f.write(tpl_card)

tpl_info = fetch('http://127.0.0.1:8080/include/patchstorage_plugin_info.html')
with open(r'modep_app\html\include\patchstorage_plugin_info.html', 'w', encoding='utf-8') as f:
    f.write(tpl_info)
print(" [+] Saved exact patchstorage_plugin.html & patchstorage_plugin_info.html from 8080")

# Check if there are other patchstorage files or templates
print("\nSample of 8080 patchstorage_plugin.html:")
print(tpl_card[:300])
