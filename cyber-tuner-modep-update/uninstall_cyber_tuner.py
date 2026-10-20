#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Cyber Audio
# SPDX-License-Identifier: MIT
# 1-Click Uninstaller & Factory Restore Tool for Cyber Strobe Tuner

import os
import sys
import shutil
import subprocess
import time

def main():
    print("================================================================")
    print("  CYBER STROBE & PEAK TUNER -- FACTORY RESTORE TOOL")
    print("================================================================")

    target_dir = r'C:\Program Files\MOD Desktop' if os.name == 'nt' else '/usr/share/mod'
    if not os.path.exists(target_dir):
        print("ERROR: Target directory does not exist.")
        sys.exit(1)

    # Find backup directory in user docs or target dir
    user_docs = os.path.expanduser('~')
    backup_base = os.path.join(user_docs, 'Documents', 'MOD Desktop', 'backups') if os.name == 'nt' else '/var/backups'
    
    backups = []
    if os.path.exists(backup_base):
        backups = [os.path.join(backup_base, d) for d in os.listdir(backup_base) if d.startswith('backup_cyber_tuner_')]
    
    if not backups and os.path.exists(target_dir):
        backups = [os.path.join(target_dir, d) for d in os.listdir(target_dir) if d.startswith('.backup_cyber_tuner_')]

    if not backups:
        print("ERROR: No backup directories found.")
        sys.exit(1)

    backups.sort()
    latest_backup = backups[-1]
    print(f"Found latest backup: {latest_backup}")

    # Stop processes
    print("\nStopping running MOD instances...")
    if os.name == 'nt':
        subprocess.run(['powershell', '-Command', 'Stop-Process -Name mod-desktop, mod-ui, jackd -Force -ErrorAction SilentlyContinue'])
    else:
        subprocess.run(['systemctl', 'stop', 'modep-mod-ui'], stderr=subprocess.DEVNULL)
    time.sleep(1)

    # Restore backed up files
    for root, dirs, files in os.walk(latest_backup):
        for f in files:
            src_file = os.path.join(root, f)
            rel_path = os.path.relpath(src_file, latest_backup)
            dst_file = os.path.join(target_dir, rel_path)
            shutil.copy2(src_file, dst_file)
            print(f"  Restored: {rel_path}")

    # Remove added assets
    tuner_css = os.path.join(target_dir, 'html', 'css', 'tuner.css')
    tuner_js  = os.path.join(target_dir, 'html', 'js', 'tuner.js')
    if os.path.exists(tuner_css): os.remove(tuner_css)
    if os.path.exists(tuner_js):  os.remove(tuner_js)

    print("\n================================================================")
    print("  FACTORY RESTORE COMPLETED! System reverted to original state.")
    print("================================================================")

if __name__ == '__main__':
    main()
