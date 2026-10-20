import urllib.request
import json
import base64

url = "https://api.github.com/user/repos"
auth = base64.b64encode(b"danny1marshall1587-maker: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").decode("ascii")

req = urllib.request.Request(url, headers={
    "User-Agent": "MyMODEPApp",
    "Authorization": "Basic " + auth,
    "Accept": "application/vnd.github.v3+json"
})

try:
    with urllib.request.urlopen(req) as resp:
        data = json.loads(resp.read().decode())
        for repo in data:
            print(f"REPO: {repo.get('full_name')} (private: {repo.get('private')})")
except Exception as e:
    print("ERR:", e)
