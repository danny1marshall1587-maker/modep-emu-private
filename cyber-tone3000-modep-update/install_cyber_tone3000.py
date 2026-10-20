#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Cyber Audio
# SPDX-License-Identifier: MIT
# Automated Installer & Backup Tool for Cyber TONE3000 System Addon

import os
import sys
import shutil
import time
import datetime
import subprocess

def find_target_dir():
    candidates = [
        r'C:\Program Files\MOD Desktop',
        '/usr/share/mod',
        '/usr/local/share/mod',
        '/var/modep'
    ]
    for c in candidates:
        if os.path.exists(c) and os.path.exists(os.path.join(c, 'html')) and os.path.exists(os.path.join(c, 'mod')):
            return c
    return None

def main():
    print("================================================================")
    print("  CYBER TONE3000 -- SYSTEM ADDON INSTALLER & BACKUP TOOL")
    print("================================================================")

    target_dir = find_target_dir()
    if not target_dir:
        print("ERROR: Could not locate MOD Desktop / MODEP root directory.")
        print("Expected C:\\Program Files\\MOD Desktop on Windows or /usr/share/mod on Linux.")
        sys.exit(1)

    print(f"Target directory found: {target_dir}")

    # Stop running processes
    print("\nStopping running MOD instances...")
    if os.name == 'nt':
        subprocess.run(['powershell', '-Command', 'Stop-Process -Name mod-desktop, mod-ui, jackd -Force -ErrorAction SilentlyContinue'], stderr=subprocess.DEVNULL)
    else:
        subprocess.run(['systemctl', 'stop', 'modep-mod-ui'], stderr=subprocess.DEVNULL)
    time.sleep(1)

    # Determine Backup location
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    if os.name == 'nt':
        user_docs = os.path.expanduser('~')
        backup_base = os.path.join(user_docs, 'Documents', 'MOD Desktop', 'backups')
        backup_dir = os.path.join(backup_base, f"backup_cyber_tone3000_{timestamp}")
    else:
        backup_base = '/var/backups'
        backup_dir = os.path.join(backup_base, f"backup_cyber_tone3000_{timestamp}")

    os.makedirs(backup_dir, exist_ok=True)
    print(f"Creating timestamped backup at: {backup_dir}")

    files_to_backup = [
        os.path.join(target_dir, 'html', 'index.html'),
        os.path.join(target_dir, 'html', 'js', 'desktop.js'),
    ]

    for f in files_to_backup:
        if os.path.exists(f):
            rel = os.path.relpath(f, target_dir)
            dst = os.path.join(backup_dir, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(f, dst)
            print(f"  Backed up: {rel}")

    src_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src')
    if not os.path.exists(src_root):
        src_root = os.path.join(r'C:\Users\danny\Documents\modep emu\cyber-tone3000-modep-update\src')

    # 1. Copy Frontend Assets (tone3000.css, tone3000.js, images & SVGs)
    print("\n1. Installing frontend assets...")
    try:
        shutil.copy2(os.path.join(src_root, 'html', 'css', 'tone3000.css'), os.path.join(target_dir, 'html', 'css', 'tone3000.css'))
        print("  Installed html/css/tone3000.css")
        shutil.copy2(os.path.join(src_root, 'html', 'js', 'tone3000.js'), os.path.join(target_dir, 'html', 'js', 'tone3000.js'))
        print("  Installed html/js/tone3000.js")

        img_src = os.path.join(src_root, 'html', 'img')
        img_dst = os.path.join(target_dir, 'html', 'img')
        if os.path.exists(img_src):
            shutil.copytree(img_src, img_dst, dirs_exist_ok=True)
            print("  Installed html/img assets (t3k.svg, t3k-mark.svg, knobs, icons)")
    except PermissionError:
        print("\nERROR: Permission denied writing to Program Files!")
        print("Please right-click install_cyber_tone3000.bat and select 'Run as Administrator'.")
        sys.exit(1)

    # 2. Patch html/index.html
    print("\n2. Patching html/index.html...")
    index_path = os.path.join(target_dir, 'html', 'index.html')
    with open(index_path, 'r', encoding='utf-8') as f:
        html_content = f.read()

    # Add CSS link
    if 'tone3000.css' not in html_content:
        if 'css/tuner.css?v={{version}}' in html_content:
            html_content = html_content.replace(
                '<link rel="stylesheet" type="text/css" href="css/tuner.css?v={{version}}"/>',
                '<link rel="stylesheet" type="text/css" href="css/tuner.css?v={{version}}"/>\n<link rel="stylesheet" type="text/css" href="css/tone3000.css?v={{version}}"/>'
            )
        elif 'css/main.css?v={{version}}' in html_content:
            html_content = html_content.replace(
                '<link rel="stylesheet" type="text/css" href="css/main.css?v={{version}}"/>',
                '<link rel="stylesheet" type="text/css" href="css/main.css?v={{version}}"/>\n<link rel="stylesheet" type="text/css" href="css/tone3000.css?v={{version}}"/>'
            )

    # Add JS script
    if 'tone3000.js' not in html_content:
        if 'js/tuner.js?v={{version}}' in html_content:
            html_content = html_content.replace(
                '<script type="text/javascript" src="js/tuner.js?v={{version}}"></script>',
                '<script type="text/javascript" src="js/tuner.js?v={{version}}"></script>\n<script type="text/javascript" src="js/tone3000.js?v={{version}}"></script>'
            )
        elif 'js/desktop.js?v={{version}}' in html_content:
            html_content = html_content.replace(
                '<script type="text/javascript" src="js/desktop.js?v={{version}}"></script>',
                '<script type="text/javascript" src="js/tone3000.js?v={{version}}"></script>\n<script type="text/javascript" src="js/desktop.js?v={{version}}"></script>'
            )

    # Add Top Header Button
    top_btn_html = '<button class="js-tone3000" id="mod-tone3000-top-btn" title="TONE3000 Cloud & NAM Player">TONE3000</button>'
    if 'id="mod-tone3000-top-btn"' not in html_content:
        if 'id="mod-tuner-top-btn"' in html_content:
            html_content = html_content.replace(
                '<button class="js-tuner" id="mod-tuner-top-btn"',
                top_btn_html + '\n                    <button class="js-tuner" id="mod-tuner-top-btn"'
            )
        elif '<div id="mod-transport-icon"' in html_content:
            html_content = html_content.replace(
                '<div id="mod-transport-icon"',
                top_btn_html + '\n            <div id="mod-transport-icon"'
            )

    # Add Modal Window container
    modal_html = '    <div id="mod-tone3000-window" class="mod-hidden"></div>'
    if 'id="mod-tone3000-window"' not in html_content:
        if 'id="mod-tuner-window"' in html_content:
            html_content = html_content.replace(
                '<div id="mod-tuner-window"',
                modal_html + '\n    <div id="mod-tuner-window"'
            )
        else:
            html_content = html_content.replace('</body>', modal_html + '\n</body>')

    with open(index_path, 'w', encoding='utf-8') as f:
        f.write(html_content)
    print("  Patched html/index.html successfully")

    # 3. Patch html/js/desktop.js
    print("\n3. Patching html/js/desktop.js...")
    desktop_path = os.path.join(target_dir, 'html', 'js', 'desktop.js')
    with open(desktop_path, 'r', encoding='utf-8') as f:
        desktop_content = f.read()

    # Instantiate CyberTone3000
    init_code = """
        if (window.CyberTone3000 && $("#mod-tone3000-window").length) {
            self.cyberTone3000 = new CyberTone3000({
                topButton: $("#mod-tone3000-top-btn"),
                windowModal: $("#mod-tone3000-window")
            });
            window.CyberTone3000Instance = self.cyberTone3000;
        }
"""
    if 'self.cyberTone3000 = new CyberTone3000' not in desktop_content:
        if 'if (window.CyberTuner && $("#mod-tuner-window").length) {' in desktop_content:
            desktop_content = desktop_content.replace(
                'if (window.CyberTuner && $("#mod-tuner-window").length) {',
                init_code + '\n        if (window.CyberTuner && $("#mod-tuner-window").length) {'
            )
        elif 'this.transportControls = new TransportControls({' in desktop_content:
            desktop_content = desktop_content.replace(
                'this.transportControls = new TransportControls({',
                init_code + '\n        this.transportControls = new TransportControls({'
            )

    with open(desktop_path, 'w', encoding='utf-8') as f:
        f.write(desktop_content)
    print("  Patched html/js/desktop.js successfully")

    print("\n================================================================")
    print("  INSTALLATION COMPLETED SUCCESSFULLY!")
    print(f"  Backup saved to: {backup_dir}")
    print("  You can now start MOD Desktop / MODEP.")
    print("================================================================")

if __name__ == '__main__':
    main()
