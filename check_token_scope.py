import urllib.request
import base64

url = "https://api.github.com/user"
auth = base64.b64encode(b"danny1marshall1587-maker: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").decode("ascii")

req = urllib.request.Request(url, headers={
    "User-Agent": "MyMODEPApp",
    "Authorization": "Basic " + auth,
    "Accept": "application/vnd.github.v3+json"
})

try:
    with urllib.request.urlopen(req) as resp:
        print("X-OAuth-Scopes:", resp.headers.get("X-OAuth-Scopes"))
        print("X-Accepted-OAuth-Scopes:", resp.headers.get("X-Accepted-OAuth-Scopes"))
        print("Status:", resp.status)
except urllib.error.HTTPError as e:
    print("HTTPError:", e.code, e.headers)
