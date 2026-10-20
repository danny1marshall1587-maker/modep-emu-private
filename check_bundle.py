import re

with open('/usr/share/mod/html/daw/main.ded92aa6-292f-4fad-b881-9393309466a9.js', 'r', encoding='utf-8') as f:
    text = f.read()

matches = set(re.findall(r'["\'`](/[a-zA-Z0-9_\-\.]+\.(?:js|wasm|worker|svg|png|json))["\'`]', text))
print('Potential absolute paths in bundle:')
for m in sorted(matches):
    print("  ", m)
