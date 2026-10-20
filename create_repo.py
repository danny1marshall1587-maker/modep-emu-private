import urllib.request
import json
import base64

token = "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"
auth = base64.b64encode(f"danny1marshall1587-maker:{token}".encode("ascii")).decode("ascii")

url = "https://api.github.com/user/repos"
data = json.dumps({
    "name": "modep-pro-live-mixer-daw",
    "description": "MODEP Pro Live Digital Mixing Console & 20-Track DAW System with Cyber-Denoiser PRO, 1176 FET, 7-Band Parametric EQ, LA-2A, Zero-Latency Limiter, and openDAW WASM",
    "private": False,
    "has_issues": True,
    "has_projects": True
}).encode("utf-8")

req = urllib.request.Request(url, data=data, headers={
    "User-Agent": "MyMODEPApp",
    "Authorization": "Basic " + auth,
    "Accept": "application/vnd.github.v3+json",
    "Content-Type": "application/json"
})

try:
    with urllib.request.urlopen(req) as resp:
        res = json.loads(resp.read().decode())
        print("CREATED REPO:", res.get("html_url"))
except urllib.error.HTTPError as e:
    print("HTTPError:", e.code, e.read().decode())
