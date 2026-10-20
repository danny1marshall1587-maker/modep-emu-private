#!/usr/bin/env python3
"""
clone_8080_to_modep_app.py
Clones the exact working web UI, scripts, styles, templates, and assets
from the running emulated MODEP server (localhost:8080) directly into modep_app/html/.
"""

import os
import sys
import urllib.request
import re

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODEP_APP_DIR = os.path.join(BASE_DIR, "modep_app")
HTML_DIR = os.path.join(MODEP_APP_DIR, "html")

def fetch_and_save(rel_path):
    if not rel_path or rel_path.startswith('data:') or rel_path.startswith('http'):
        return None
    url = f"http://127.0.0.1:8080/{rel_path}"
    local_path = os.path.join(HTML_DIR, rel_path.replace('/', os.sep))
    os.makedirs(os.path.dirname(local_path), exist_ok=True)
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = resp.read()
            with open(local_path, 'wb') as f:
                f.write(data)
            print(f" [+] Synced: {rel_path} ({len(data)} bytes)")
            return data
    except Exception as e:
        print(f" [!] Error fetching {rel_path}: {e}")
        return None

def clone():
    print("=== Syncing exact working UI from localhost:8080 ===")
    
    # 1. Fetch index.html
    html_data = fetch_and_save("index.html")
    if not html_data:
        url = "http://127.0.0.1:8080/"
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=5) as resp:
            html_data = resp.read()
            with open(os.path.join(HTML_DIR, "index.html"), "wb") as f:
                f.write(html_data)
            print(" [+] Synced: index.html from root")

    html_str = html_data.decode('utf-8', errors='ignore')

    # 2. Extract and sync all scripts, links, includes
    scripts = re.findall(r'<script[^>]+src=["\']([^"\'?]+)', html_str)
    styles = re.findall(r'<link[^>]+href=["\']([^"\'?]+)', html_str)
    
    for s in set(scripts):
        fetch_and_save(s)
            
    for st in set(styles):
        fetch_and_save(st)

    # 3. Specific includes
    includes = [
        'include/patchstorage_plugin.html',
        'include/patchstorage_plugin_info.html',
        'include/cloudplugin.html',
        'include/cloudplugin_info.html',
        'include/pedalboard.html',
        'include/pedalboard_info.html',
        'js/patchstorage.js',
        'js/cloudplugin.js',
        'js/templates.js',
        'css/main.css'
    ]
    for inc in includes:
        fetch_and_save(inc)

    print("\nExact MODEP UI synchronization complete!")

if __name__ == '__main__':
    clone()
