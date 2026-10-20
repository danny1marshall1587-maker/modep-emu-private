#!/usr/bin/env python3
"""
Patchbox Prime / MODEP Live Console — Multi-Threaded HTTP Portal Server (Port 8080)
Serves /var/modep/web with ThreadingHTTPServer, no-cache + CORS headers,
automatic /mixer/* and /web/* path aliasing, direct disk serving for /usr/share/mod/html assets,
live /effect/files user asset catalog, and reverse-proxying of MOD-UI /effect/* and /resources/*.
"""
import os
import sys
import json
import mimetypes
import urllib.request
import urllib.error
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

WEB_ROOT = "/var/modep/web"
MOD_HTML_ROOT = "/usr/share/mod/html"
USER_FILES_ROOT = "/var/modep/user-files"
MOD_UI_BASE = "http://127.0.0.1:8881"

PROXY_PREFIXES = (
    "/effect/",
    "/resources/",
    "/js/lib/",
    "/js/common.js",
    "/js/modgui.js",
    "/css/pedals.css",
    "/css/font",
    "/img/",
    "/fonts/",
)

# In-memory cache for /effect/get and /effect/list to survive transient MOD-UI restarts
METADATA_CACHE = {}


def scan_user_files():
    nam_files = []
    ir_files = []
    search_roots = [USER_FILES_ROOT, "/var/modep/instance_1/user-files", "/home/patch/modep-user-files"]

    for base in search_roots:
        if not os.path.exists(base):
            continue
        for root, dirs, files in os.walk(base):
            if ".git" in root or "node_modules" in root:
                continue
            rel_folder = os.path.relpath(root, base).replace("\\", "/")
            if rel_folder == ".":
                rel_folder = "Root"
            clean_cat = rel_folder
            for pfx in ("NAM Models/", "Reverb IRs/", "Speaker Cabinets IRs/", "Aida DSP Models/"):
                if clean_cat.startswith(pfx):
                    clean_cat = clean_cat[len(pfx):]
            if clean_cat == "NAM Models" or clean_cat == "Reverb IRs" or clean_cat == "Speaker Cabinets IRs":
                clean_cat = "General"

            for fn in sorted(files):
                low = fn.lower()
                full_path = os.path.join(root, fn)
                if low.endswith(".nam") or low.endswith(".aidax") or (low.endswith(".json") and "model" in low):
                    nam_files.append({
                        "name": fn,
                        "label": fn.replace(".nam", "").replace(".aidax", "").replace(".json", ""),
                        "category": clean_cat,
                        "path": full_path,
                        "type": "nam"
                    })
                elif low.endswith(".wav") and any(k in root.lower() or k in low for k in ["ir", "cab", "reverb", "plate", "hall", "convo", "impulse"]):
                    ir_files.append({
                        "name": fn,
                        "label": fn.replace(".wav", "").replace("_", " "),
                        "category": clean_cat,
                        "path": full_path,
                        "type": "ir"
                    })

    # Also search /var/modep/lv2 for bundled models/IRs
    lv2_base = "/var/modep/lv2"
    if os.path.exists(lv2_base):
        for root, dirs, files in os.walk(lv2_base):
            for fn in files:
                low = fn.lower()
                full_path = os.path.join(root, fn)
                if (low.endswith(".nam") or low.endswith(".aidax") or low.endswith(".json")) and "model" in root.lower():
                    nam_files.append({
                        "name": fn,
                        "label": fn,
                        "category": os.path.basename(root),
                        "path": full_path,
                        "type": "nam"
                    })
                elif low.endswith(".wav") and any(k in root.lower() for k in ["ir", "cab", "convo"]):
                    ir_files.append({
                        "name": fn,
                        "label": fn.replace(".wav", ""),
                        "category": os.path.basename(root),
                        "path": full_path,
                        "type": "ir"
                    })

    # De-duplicate by path
    dedup_nam = {f["path"]: f for f in nam_files}
    dedup_ir = {f["path"]: f for f in ir_files}

    # Curated fallbacks if user-files is empty
    curated_nam = [
        {"name": "Fender_65_Twin_Clean.nam", "label": "Fender '65 Twin Reverb Clean", "category": "Fender", "path": "/var/modep/user-files/NAM Models/Fender_65_Twin_Clean.nam", "type": "nam"},
        {"name": "Marshall_JCM800_2203.nam", "label": "Marshall JCM800 2203 Crunch", "category": "Marshall", "path": "/var/modep/user-files/NAM Models/Marshall_JCM800_2203.nam", "type": "nam"},
        {"name": "Vox_AC30_TopBoost.nam", "label": "Vox AC30 Top Boost Chime", "category": "Vox", "path": "/var/modep/user-files/NAM Models/Vox_AC30_TopBoost.nam", "type": "nam"},
        {"name": "Dumble_ODS_Lead.nam", "label": "Dumble ODS Overdrive Special", "category": "Dumble", "path": "/var/modep/user-files/NAM Models/Dumble_ODS_Lead.nam", "type": "nam"},
        {"name": "Mesa_Dual_Rectifier.nam", "label": "Mesa Dual Rectifier High-Gain", "category": "Mesa", "path": "/var/modep/user-files/NAM Models/Mesa_Dual_Rectifier.nam", "type": "nam"},
        {"name": "Ampeg_SVT_VR_8x10.nam", "label": "Ampeg SVT-VR 8x10 Bass Rig", "category": "Bass", "path": "/var/modep/user-files/NAM Models/Ampeg_SVT_VR_8x10.nam", "type": "nam"}
    ]
    curated_ir = [
        {"name": "Abbey_Plate_140.wav", "label": "Abbey Road Plate 140", "category": "Studio Plate", "path": "/var/modep/user-files/Reverb IRs/Abbey_Plate_140.wav", "type": "ir"},
        {"name": "Lexicon_480L_Vocal_Hall.wav", "label": "Lexicon 480L Large Vocal Hall", "category": "Concert Hall", "path": "/var/modep/user-files/Reverb IRs/Lexicon_480L_Vocal_Hall.wav", "type": "ir"},
        {"name": "Celestion_4x12_GB.wav", "label": "Celestion 4x12 Greenback Cab", "category": "Guitar Cab", "path": "/var/modep/user-files/Speaker Cabinets IRs/Celestion_4x12_GB.wav", "type": "ir"},
        {"name": "Ampeg_8x10_Cab.wav", "label": "Ampeg 8x10 Fridge Bass Cab", "category": "Bass Cab", "path": "/var/modep/user-files/Speaker Cabinets IRs/Ampeg_8x10_Cab.wav", "type": "ir"},
        {"name": "Vintage_Spring_63.wav", "label": "Vintage '63 Outboard Spring Tank", "category": "Spring Reverb", "path": "/var/modep/user-files/Reverb IRs/Vintage_Spring_63.wav", "type": "ir"}
    ]

    final_nam = list(dedup_nam.values()) if dedup_nam else curated_nam
    final_ir = list(dedup_ir.values()) if dedup_ir else curated_ir

    return {
        "ok": True,
        "total": len(final_nam) + len(final_ir),
        "files": {
            "nam": sorted(final_nam, key=lambda x: (x["category"], x["label"])),
            "ir": sorted(final_ir, key=lambda x: (x["category"], x["label"]))
        }
    }


class MixerPortalHandler(SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_ROOT, **kwargs)

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_POST(self):
        clean = self.path.split("?", 1)[0].split("#", 1)[0]
        content_length = int(self.headers.get("Content-Length", 0))
        post_body = self.rfile.read(content_length) if content_length > 0 else b""

        # Proxy any POST under /effect/, /files/, /pedalboard/ to MOD-UI (Port 8881)
        target_url = MOD_UI_BASE + self.path
        try:
            req = urllib.request.Request(
                target_url,
                data=post_body,
                headers={
                    "Content-Type": self.headers.get("Content-Type", "application/json"),
                    "User-Agent": "MODEP-Portal-Proxy/1.0"
                },
                method="POST"
            )
            with urllib.request.urlopen(req, timeout=4.0) as resp:
                resp_body = resp.read()
                ctype = resp.headers.get("Content-Type", "application/json")
                self.send_response(resp.status)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(resp_body)))
                self.end_headers()
                self.wfile.write(resp_body)
        except urllib.error.HTTPError as he:
            err_body = he.read() if he.fp else b""
            self.send_response(he.code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(err_body)))
            self.end_headers()
            if err_body:
                self.wfile.write(err_body)
        except Exception as exc:
            msg = json.dumps({"ok": False, "error": str(exc)}).encode("utf-8")
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(msg)))
            self.end_headers()
            self.wfile.write(msg)

    def do_GET(self):
        clean = self.path.split("?", 1)[0].split("#", 1)[0]

        # 1. LIVE USER FILES ENDPOINT (/effect/files or /files/list)
        if clean in ("/effect/files", "/files/list", "/effect/file/list"):
            self.send_json_response(scan_user_files())
            return

        # 2. DIRECT DISK SERVING FOR /usr/share/mod/html STATIC ASSETS (0ms, ZERO 502 DEPENDENCY)
        if clean.startswith(("/js/lib/", "/js/common.js", "/js/modgui.js", "/css/pedals.css", "/css/font", "/fonts/", "/img/", "/resources/pedals/", "/resources/templates/")):
            rel_sub = clean.lstrip("/")
            disk_path = os.path.join(MOD_HTML_ROOT, rel_sub)
            if os.path.isfile(disk_path):
                self.serve_disk_file(disk_path)
                return

        # 3. PROXY TO MOD-UI (PORT 8881) WITH IN-MEMORY CACHE & ZERO-HANG FALLBACK
        if any(clean.startswith(pfx) for pfx in PROXY_PREFIXES):
            self.proxy_to_mod_ui()
            return

        super().do_GET()

    def do_HEAD(self):
        clean = self.path.split("?", 1)[0].split("#", 1)[0]
        if clean.startswith(("/js/lib/", "/js/common.js", "/js/modgui.js", "/css/pedals.css", "/css/font", "/fonts/", "/img/", "/resources/pedals/", "/resources/templates/")):
            rel_sub = clean.lstrip("/")
            disk_path = os.path.join(MOD_HTML_ROOT, rel_sub)
            if os.path.isfile(disk_path):
                ctype, _ = mimetypes.guess_type(disk_path)
                self.send_response(200)
                if ctype:
                    self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(os.path.getsize(disk_path)))
                self.end_headers()
                return
        super().do_HEAD()

    def serve_disk_file(self, disk_path):
        try:
            with open(disk_path, "rb") as f:
                body = f.read()
            ctype, _ = mimetypes.guess_type(disk_path)
            if not ctype:
                if disk_path.endswith(".css"):
                    ctype = "text/css"
                elif disk_path.endswith(".js"):
                    ctype = "application/javascript"
                elif disk_path.endswith(".html") or disk_path.endswith(".htm"):
                    ctype = "text/html"
                elif disk_path.endswith(".svg"):
                    ctype = "image/svg+xml"
                elif disk_path.endswith(".woff2"):
                    ctype = "font/woff2"
                elif disk_path.endswith(".woff"):
                    ctype = "font/woff"
                elif disk_path.endswith(".ttf"):
                    ctype = "font/ttf"
                else:
                    ctype = "application/octet-stream"

            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except Exception as e:
            self.send_error(500, f"Disk file error: {str(e)}")

    def send_json_response(self, obj, status=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def proxy_to_mod_ui(self):
        target_url = MOD_UI_BASE + self.path
        cache_key = self.path

        try:
            req = urllib.request.Request(target_url, headers={"User-Agent": "MODEP-Portal-Proxy/1.0"})
            with urllib.request.urlopen(req, timeout=4.0) as resp:
                body = resp.read()
                ctype = resp.headers.get("Content-Type", "application/octet-stream")
                # Cache successful /effect/get and /effect/list responses
                if resp.status == 200 and ("/effect/get" in self.path or "/effect/list" in self.path):
                    METADATA_CACHE[cache_key] = (200, ctype, body)
                self.send_response(resp.status)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
        except urllib.error.HTTPError as he:
            body = he.read() if he.fp else b""
            # Check cache if upstream returned error
            if cache_key in METADATA_CACHE:
                c_status, c_type, c_body = METADATA_CACHE[cache_key]
                self.send_response(c_status)
                self.send_header("Content-Type", c_type)
                self.send_header("Content-Length", str(len(c_body)))
                self.end_headers()
                self.wfile.write(c_body)
                return
            self.send_response(he.code)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if body:
                self.wfile.write(body)
        except Exception as exc:
            # Check cache if upstream timed out or failed to connect
            if cache_key in METADATA_CACHE:
                c_status, c_type, c_body = METADATA_CACHE[cache_key]
                self.send_response(c_status)
                self.send_header("Content-Type", c_type)
                self.send_header("Content-Length", str(len(c_body)))
                self.end_headers()
                self.wfile.write(c_body)
                return

            # Graceful JSON fallback for /effect/get so modgui-host never hangs
            if "/effect/get" in self.path:
                uri_param = ""
                if "uri=" in self.path:
                    try:
                        uri_param = urllib.parse.unquote(self.path.split("uri=")[1].split("&")[0])
                    except Exception:
                        pass
                fallback_info = {
                    "uri": uri_param,
                    "name": uri_param.split("/")[-1].replace("-", " ").title() if uri_param else "Effect",
                    "category": ["Utility"],
                    "ports": {"control": {"input": []}},
                    "gui": {}
                }
                self.send_json_response(fallback_info, 200)
                return

            msg = f'{{"error": "mod-ui proxy error: {str(exc)}"}}'.encode("utf-8")
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(msg)))
            self.end_headers()
            self.wfile.write(msg)

    def translate_path(self, path):
        clean = path.split("?", 1)[0].split("#", 1)[0]
        if clean == "/mixer" or clean == "/mixer/":
            path = "/index.html"
        elif clean.startswith("/mixer/"):
            path = clean[len("/mixer"):]
        elif clean == "/web" or clean == "/web/":
            path = "/index.html"
        elif clean.startswith("/web/"):
            path = clean[len("/web"):]
        return super().translate_path(path)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()

    def log_message(self, format, *args):
        pass


class ThreadedHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    os.chdir(WEB_ROOT)
    server = ThreadedHTTPServer(("0.0.0.0", port), MixerPortalHandler)
    print(f"[modep-web-portal] Serving {WEB_ROOT} + MOD-UI Direct & Proxy on 0.0.0.0:{port}", flush=True)
    server.serve_forever()
