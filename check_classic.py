import urllib.request
import base64

url = "https://api.github.com/user"
token = "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"

req = urllib.request.Request(url, headers={
    "User-Agent": "MyMODEPApp",
    "Authorization": "token " + token,
    "Accept": "application/vnd.github.v3+json"
})

try:
    with urllib.request.urlopen(req) as resp:
        print("User:", resp.read().decode()[:120])
        print("X-OAuth-Scopes:", resp.headers.get("X-OAuth-Scopes"))
        print("Status:", resp.status)
except urllib.error.HTTPError as e:
    print("HTTPError:", e.code, e.headers)
