import urllib.request
import json

url = "https://patchstorage.com/api/beta/patches?per_page=100&platforms=8046"
req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
try:
    with urllib.request.urlopen(req, timeout=10) as resp:
        patches = json.loads(resp.read().decode('utf-8'))
        print(f"Checking {len(patches)} patches...")
        null_artwork = [p for p in patches if not p.get('artwork')]
        null_state = [p for p in patches if not p.get('state')]
        null_author = [p for p in patches if not p.get('author')]
        null_categories = [p for p in patches if not p.get('categories')]
        null_tags = [p for p in patches if not p.get('tags')]
        null_uids = [p for p in patches if not p.get('uids')]
        
        print("null artwork:", len(null_artwork))
        print("null state:", len(null_state))
        print("null author:", len(null_author))
        print("null categories:", len(null_categories))
        print("null tags:", len(null_tags))
        print("null uids:", len(null_uids))
except Exception as e:
    print("Error:", e)
