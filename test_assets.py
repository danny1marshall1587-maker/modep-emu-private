import urllib.request

urls = [
    'http://127.0.0.1:18181/img/patchstorage-menu.png',
    'http://127.0.0.1:18181/css/main.css',
    'http://127.0.0.1:18181/js/patchstorage.js',
    'http://127.0.0.1:18181/patchstorage/patches'
]

for url in urls:
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=3) as resp:
            data = resp.read()
            print(f"{url} -> Status: {resp.getcode()}, Length: {len(data)} bytes")
            if 'css' in url:
                print("  contains '#main-menu #patchstorage':", "#main-menu #patchstorage" in data.decode('utf-8', errors='ignore'))
    except Exception as e:
        print(f"{url} -> ERROR: {e}")
