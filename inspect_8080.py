import urllib.request
import json

urls = [
    'http://127.0.0.1:8080/js/patchstorage.js',
    'http://127.0.0.1:8080/patchstorage/patches',
    'http://127.0.0.1:8080/effect/list',
    'http://127.0.0.1:8080/include/patchstorage_plugin.html',
    'http://127.0.0.1:8080/include/patchstorage_plugin_info.html',
    'http://127.0.0.1:8080/css/main.css'
]

for url in urls:
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = resp.read()
            print(f"{url} -> Status: {resp.getcode()}, Length: {len(data)} bytes")
    except Exception as e:
        print(f"{url} -> Error: {e}")
