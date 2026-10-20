#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Cyber Audio
# SPDX-License-Identifier: MIT
# Universal Linux & MODEP / Patchbox OS Installer for Cyber Strobe Tuner

set -e

echo "================================================================"
echo "  CYBER STROBE & PEAK TUNER -- LINUX & MODEP INSTALLER"
echo "================================================================"

# Check root privileges for system-wide install
if [ "$EUID" -ne 0 ] && [ ! -d "$HOME/.local/share/mod" ]; then
    echo "Notice: Running as non-root. If targeting system MODEP, run with sudo: sudo ./install_linux.sh"
fi

# Detect Target Directory
TARGET_DIR=""
CANDIDATES=(
    "/usr/share/mod"
    "/usr/local/share/mod"
    "/var/modep"
    "/var/modep/mod-ui"
    "$HOME/.local/share/mod"
    "/opt/mod-desktop"
)

for dir in "${CANDIDATES[@]}"; do
    if [ -d "$dir/html" ] && [ -d "$dir/mod" ]; then
        TARGET_DIR="$dir"
        break
    fi
done

if [ -z "$TARGET_DIR" ]; then
    # Try searching for mod-ui
    FOUND_PATH=$(find /usr /var /opt "$HOME" -maxdepth 4 -type d -name "html" 2>/dev/null | grep -E "mod(-ui|ep)?" | head -n 1 || true)
    if [ -n "$FOUND_PATH" ]; then
        TARGET_DIR=$(dirname "$FOUND_PATH")
    fi
fi

if [ -z "$TARGET_DIR" ]; then
    echo "ERROR: Could not automatically locate MOD-UI / MODEP directory."
    echo "Please specify the MOD installation path: e.g. sudo ./install_linux.sh /var/modep"
    exit 1
fi

echo "Target directory detected: $TARGET_DIR"

# Stop mod-ui service if running
echo "Stopping mod-ui service..."
systemctl stop modep-mod-ui 2>/dev/null || systemctl stop mod-ui 2>/dev/null || true

# Run Python installer
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$SCRIPT_DIR/install_cyber_tuner.py"

# Restart mod-ui service
echo "Restarting mod-ui service..."
systemctl start modep-mod-ui 2>/dev/null || systemctl start mod-ui 2>/dev/null || true

echo "================================================================"
echo "  LINUX / MODEP INSTALLATION COMPLETE! 100% SUCCESS"
echo "================================================================"
