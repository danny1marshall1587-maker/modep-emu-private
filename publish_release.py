import urllib.request
import json
import base64
import os

token = "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"
auth = base64.b64encode(f"danny1marshall1587-maker:{token}".encode("ascii")).decode("ascii")

# 1. Create Release
rel_url = "https://api.github.com/repos/danny1marshall1587-maker/modep-pro-live-mixer-daw/releases"
rel_data = json.dumps({
    "tag_name": "v2.0-live-mixer-daw",
    "name": "MODEP Pro Live Digital Mixing Console & 20-Track DAW v2.0",
    "body": "Production Release for Raspberry Pi 5 & Behringer UMC1820.\n\n- Real-time JACK WebSocket meter daemon (0 dBFS true signal tracking, zero fake animations)\n- Forensic Cyber-Denoiser PRO (10-Band spectral subtraction with noise floor learning)\n- 1176 FET Class-A Preamp emulation\n- 7-Band Parametric continuous EQ across all channels & Master\n- Teletronix LA-2A Optical Leveler\n- Zero-Latency Live Brickwall Limiter\n- Dugan AMM Automatic Microphone Mixer\n- Dynamic FX Ducking on Aux Sends\n- Complete openDAW Studio engine bundle with WASM audio unit & 25 stock devices\n- MODEP Super-Insert nested pedalboard canvas support",
    "draft": False,
    "prerelease": False
}).encode("utf-8")

req = urllib.request.Request(rel_url, data=rel_data, headers={
    "User-Agent": "MyMODEPApp",
    "Authorization": "Basic " + auth,
    "Accept": "application/vnd.github.v3+json",
    "Content-Type": "application/json"
})

try:
    with urllib.request.urlopen(req) as resp:
        rel = json.loads(resp.read().decode())
        upload_url = rel.get("upload_url").split("{")[0]
        print("RELEASE CREATED:", rel.get("html_url"))

        # 2. Upload Binary Tarball Asset
        tar_path = r"c:\Users\danny\Documents\modep emu\modep_pro_mixer_daw_suite_v2.0.tar.gz"
        if os.path.exists(tar_path):
            file_size = os.path.getsize(tar_path)
            with open(tar_path, "rb") as f:
                asset_data = f.read()

            upload_req_url = f"{upload_url}?name=modep_pro_mixer_daw_suite_v2.0.tar.gz"
            upload_req = urllib.request.Request(upload_req_url, data=asset_data, headers={
                "User-Agent": "MyMODEPApp",
                "Authorization": "Basic " + auth,
                "Content-Type": "application/gzip",
                "Content-Length": str(file_size)
            })

            with urllib.request.urlopen(upload_req) as up_resp:
                up_res = json.loads(up_resp.read().decode())
                print("ASSET UPLOADED:", up_res.get("browser_download_url"))
except urllib.error.HTTPError as e:
    print("HTTPError:", e.code, e.read().decode())
except Exception as e:
    print("Error:", e)
