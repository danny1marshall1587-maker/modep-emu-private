import urllib.request
import json

url = "https://api.github.com/repos/mikeoliphant/neural-amp-modeler-lv2/releases/latest"
try:
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=10) as resp:
        data = json.loads(resp.read().decode('utf-8'))
        print(f"Latest NAM release: {data.get('tag_name')}")
        for asset in data.get('assets', []):
            print(f"  Asset: {asset.get('name')} -> {asset.get('browser_download_url')}")
except Exception as e:
    print("Error:", e)
