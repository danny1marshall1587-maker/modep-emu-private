import urllib.request
import json

repos = [
    'DISTRHO/DPF-Plugins',
    'AidaDSP/AIDA-X',
    'michaelwillis/dragonfly-reverb',
    'ChowDSP/ChowCentaur',
    'sadko4u/lsp-plugins',
]

for repo in repos:
    try:
        url = f"https://api.github.com/repos/{repo}/releases/latest"
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            print(f"=== {repo} ({data.get('tag_name')}) ===")
            for asset in data.get('assets', []):
                name = asset.get('name', '')
                if any(x in name.lower() for x in ['win', 'lv2', 'zip', '64']):
                    print(f"   Asset: {name} -> {asset.get('browser_download_url')}")
    except Exception as e:
        print(f"=== {repo} === Error: {e}")
