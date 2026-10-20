#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Cyber Audio
# SPDX-License-Identifier: MIT
# Uninstaller & Restore Tool for Cyber TONE3000

import os
import sys
import shutil
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
    print("  CYBER TONE3000 -- UNINSTALLER & RESTORE TOOL")
    print("================================================================")

    target_dir = find_target_dir()
    if not target_dir:
        print("ERROR: Target directory not found.")
        sys.exit(1)

    # Find most recent backup
    user_docs = os.path.expanduser('~')
    backup_base = os.path.join(user_docs, 'Documents', 'MOD Desktop', 'backups') if os.name == 'nt' else '/var/backups'

    if not os.path.exists(backup_base):
        print("ERROR: No backups directory found.")
        sys.exit(1)

    backups = [d for d in os.listdir(backup_base) if d.startswith('backup_cyber_tone3000_')]
    if not backups:
        print("ERROR: No Cyber TONE3000 backups found.")
        sys.exit(1)

    backups.sort()
    latest_backup = os.path.join(backup_base, backups[-1])
    print(f"Restoring from: {latest_backup}")

    for root, _, files in os.walk(latest_backup):
        for f in files:
            src = os.path.join(root, f)
            rel = os.path.relpath(src, latest_backup)
            dst = os.path.join(target_dir, rel)
            shutil.copy2(src, dst)
            print(f"  Restored: {rel}")

    # Remove installed assets
    for p in [
        os.path.join(target_dir, 'html', 'css', 'tone3000.css'),
        os.path.join(target_dir, 'html', 'js', 'tone3000.js'),
        os.path.join(target_dir, 'html', 'img', 'icons', '25', 'tone3000.png')
    ]:
        if os.path.exists(p):
            os.remove(p)
            print(f"  Removed asset: {os.path.basename(p)}")

    print("\n================================================================")
    print("  UNINSTALLATION & RESTORE COMPLETED SUCCESSFULLY!")
    print("================================================================")

if __name__ == '__main__':
    main()
