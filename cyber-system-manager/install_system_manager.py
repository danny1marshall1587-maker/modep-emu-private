#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Cyber Audio
# Automated Installer for Cyber System & Hardware Settings Manager

import os
import sys
import shutil
import subprocess

def find_target_dir():
    candidates = [
        '/usr/share/mod',
        '/usr/local/share/mod',
        '/var/modep',
        r'C:\Program Files\MOD Desktop'
    ]
    for c in candidates:
        if os.path.exists(c) and os.path.exists(os.path.join(c, 'html')) and os.path.exists(os.path.join(c, 'mod')):
            return c
    return None

def main():
    print("================================================================")
    print("   CYBER HARDWARE & SYSTEM MANAGER INSTALLER")
    print("================================================================")

    target_dir = find_target_dir()
    if not target_dir:
        print("ERROR: Could not locate MODEP root directory.")
        sys.exit(1)

    print(f"Target directory: {target_dir}")
    src_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src')

    # 1. Copy Frontend Assets (system.css, system.js)
    print("1. Installing system.css and system.js...")
    shutil.copy2(os.path.join(src_root, 'html', 'css', 'system.css'), os.path.join(target_dir, 'html', 'css', 'system.css'))
    shutil.copy2(os.path.join(src_root, 'html', 'js', 'system.js'), os.path.join(target_dir, 'html', 'js', 'system.js'))

    # 2. Copy Backend system_api.py
    print("2. Installing mod/system_api.py...")
    shutil.copy2(os.path.join(src_root, 'mod', 'system_api.py'), os.path.join(target_dir, 'mod', 'system_api.py'))

    # 3. Patch html/index.html
    print("3. Patching html/index.html...")
    index_path = os.path.join(target_dir, 'html', 'index.html')
    with open(index_path, 'r', encoding='utf-8') as f:
        html = f.read()

    # Add CSS & JS links
    if 'system.css' not in html:
        html = html.replace('</head>', '<link rel="stylesheet" href="css/system.css">\n</head>')
    if 'system.js' not in html:
        html = html.replace('</body>', '<script src="js/system.js"></script>\n</body>')

    # Add Hardware/System button into top bar
    btn_html = '<button class="js-sys-mgr" id="mod-sys-mgr-btn" style="background:#0f2027; border:1px solid #00b4d8; color:#90e0ef; border-radius:4px; padding:4px 10px; margin-right:8px; cursor:pointer; font-weight:bold;" onclick="window.CyberSystemManager && window.CyberSystemManager.open();">&#9881; HARDWARE & WI-FI</button>'
    if 'id="mod-sys-mgr-btn"' not in html:
        if '<div class="actions">' in html:
            html = html.replace('<div class="actions">', '<div class="actions">\n                    ' + btn_html)
        elif '<header id="pedalboard-actions">' in html:
            html = html.replace('<header id="pedalboard-actions">', '<header id="pedalboard-actions">\n        ' + btn_html)

    with open(index_path, 'w', encoding='utf-8') as f:
        f.write(html)

    # 4. Patch mod/webserver.py with REST endpoints
    print("4. Patching mod/webserver.py...")
    webserver_path = os.path.join(target_dir, 'mod', 'webserver.py')
    with open(webserver_path, 'r', encoding='utf-8') as f:
        ws_content = f.read()

    api_routes = """
        # ── CYBER SYSTEM & HARDWARE MANAGER REST ROUTES ───────────────────
        (r"/api/system/network", CyberNetworkStatusHandler),
        (r"/api/system/network/scan", CyberWifiScanHandler),
        (r"/api/system/network/connect", CyberWifiConnectHandler),
        (r"/api/system/network/hotspot", CyberWifiHotspotHandler),
        (r"/api/system/audio", CyberAudioConfigHandler),
        (r"/api/system/stats", CyberSystemStatsHandler),
        (r"/api/system/power", CyberSystemPowerHandler),
"""
    handlers_code = """
# ── CYBER SYSTEM MANAGER HANDLERS ─────────────────────────────────────────
from mod.system_api import CyberSystemManager
import json

class CyberNetworkStatusHandler(tornado.web.RequestHandler):
    def get(self):
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps(CyberSystemManager.get_network_status()))

class CyberWifiScanHandler(tornado.web.RequestHandler):
    def get(self):
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps(CyberSystemManager.scan_wifi()))

class CyberWifiConnectHandler(tornado.web.RequestHandler):
    def post(self):
        ssid = self.get_argument("ssid", "")
        password = self.get_argument("password", "")
        ok, msg = CyberSystemManager.connect_wifi(ssid, password)
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps({"ok": ok, "message": msg}))

class CyberWifiHotspotHandler(tornado.web.RequestHandler):
    def post(self):
        ok, msg = CyberSystemManager.start_hotspot()
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps({"ok": ok, "message": msg}))

class CyberAudioConfigHandler(tornado.web.RequestHandler):
    def get(self):
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps(CyberSystemManager.get_audio_info()))

    def post(self):
        card = self.get_argument("card", "hw:0")
        buffer_size = self.get_argument("buffer", "128")
        rate = self.get_argument("rate", "48000")
        ok, msg = CyberSystemManager.set_audio_config(card, buffer_size, rate)
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps({"ok": ok, "message": msg}))

class CyberSystemStatsHandler(tornado.web.RequestHandler):
    def get(self):
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps(CyberSystemManager.get_system_stats()))

class CyberSystemPowerHandler(tornado.web.RequestHandler):
    def post(self):
        action = self.get_argument("action", "reboot")
        if action == "reboot":
            CyberSystemManager.reboot()
        else:
            CyberSystemManager.poweroff()
        self.write(json.dumps({"ok": True}))
"""

    if 'CyberNetworkStatusHandler' not in ws_content:
        # Add handlers before make_app
        ws_content = ws_content.replace('def make_app(', handlers_code + '\ndef make_app(')
        # Add routes inside Application initialization
        ws_content = ws_content.replace('(r"/", RootHandler),', '(r"/", RootHandler),\n' + api_routes)

        with open(webserver_path, 'w', encoding='utf-8') as f:
            f.write(ws_content)

    print("\nSUCCESS: Cyber System & Hardware Manager installed successfully!")
    # Restart mod-ui service
    subprocess.run(['systemctl', 'restart', 'modep-mod-ui'], stderr=subprocess.DEVNULL)

if __name__ == '__main__':
    main()
