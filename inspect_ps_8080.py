with open(r'modep_app\html\js\patchstorage.js', 'r', encoding='utf-8') as f:
    js = f.read()

import re
urls = re.findall(r'https?://[^\s\'"]+', js)
print("URLs in 8080 patchstorage.js:")
for u in set(urls):
    print(" ", u)

print("\n--- getCloudPlugins in 8080 patchstorage.js ---")
start = js.find('getCloudPlugins:')
print(js[start:start+1200])
