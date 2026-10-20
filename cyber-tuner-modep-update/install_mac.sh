#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Cyber Audio
# SPDX-License-Identifier: MIT
# macOS Installer for Cyber Strobe Tuner in MOD Desktop

set -e

echo "================================================================"
echo "  CYBER STROBE & PEAK TUNER -- macOS INSTALLER"
echo "================================================================"

TARGET_DIR=""
CANDIDATES=(
    "/Applications/MOD Desktop.app/Contents/Resources"
    "/Applications/MOD Desktop.app/Contents/MacOS"
    "$HOME/Applications/MOD Desktop.app/Contents/Resources"
    "$HOME/Library/Application Support/MOD Desktop"
    "/usr/local/share/mod"
)

for dir in "${CANDIDATES[@]}"; do
    if [ -d "$dir/html" ] && [ -d "$dir/mod" ]; then
        TARGET_DIR="$dir"
        break
    fi
done

if [ -z "$TARGET_DIR" ]; then
    FOUND_PATH=$(find /Applications "$HOME/Applications" "$HOME/Library/Application Support" -maxdepth 5 -type d -name "html" 2>/dev/null | grep -i "MOD" | head -n 1 || true)
    if [ -n "$FOUND_PATH" ]; then
        TARGET_DIR=$(dirname "$FOUND_PATH")
    fi
fi

if [ -z "$TARGET_DIR" ]; then
    echo "ERROR: Could not automatically locate MOD Desktop.app on macOS."
    echo "Please ensure MOD Desktop is installed in /Applications or provide path as argument."
    exit 1
fi

echo "Target directory detected: $TARGET_DIR"

# Kill running MOD Desktop
killall "MOD Desktop" 2>/dev/null || killall "mod-desktop" 2>/dev/null || true

# Run Python installer
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$SCRIPT_DIR/install_cyber_tuner.py"

echo "================================================================"
echo "  macOS INSTALLATION COMPLETE! 100% SUCCESS"
echo "================================================================"
