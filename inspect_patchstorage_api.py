import urllib.request
import json

url = "https://patchstorage.com/api/beta/patches?per_page=5&platforms=8046"
req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
try:
    with urllib.request.urlopen(req, timeout=10) as resp:
        data = json.loads(resp.read().decode('utf-8'))
        print("Total items returned:", len(data))
        if data:
            print("First item keys:", list(data[0].keys()))
            print("\nSample item 0:")
            for k in ['id', 'title', 'slug', 'categories', 'tags', 'files', 'custom_fields', 'uids', 'state', 'author', 'artwork']:
                print(f"  {k}: {data[0].get(k)}")
except Exception as e:
    print("Error:", e)
