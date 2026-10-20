import urllib.request
import json

url = "http://127.0.0.1:18181/patchstorage/patches?per_page=10&platforms=8046"
try:
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=10) as resp:
        print("Status code:", resp.getcode())
        print("X-WP-TotalPages header:", resp.headers.get('X-WP-TotalPages'))
        data = json.loads(resp.read().decode('utf-8'))
        print("Total items parsed:", len(data))
        if data:
            print("First item title:", data[0].get('title'))
except Exception as e:
    print("Test Error:", e)
