#!/usr/bin/env bash
set -euo pipefail

echo "=== Installing REAPER ARM64 (Linux aarch64) ==="

REAPER_VER="780"
REAPER_URL="https://dlcf.reaper.fm/7.x/reaper${REAPER_VER}_linux_aarch64.tar.xz"
TMP_DIR="/tmp/reaper_install"

rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

echo "Downloading REAPER from ${REAPER_URL}..."
curl -L -o "$TMP_DIR/reaper.tar.xz" "$REAPER_URL"

echo "Extracting REAPER..."
tar -xf "$TMP_DIR/reaper.tar.xz" -C "$TMP_DIR"

EXTRACTED_DIR=$(find "$TMP_DIR" -maxdepth 1 -type d -name "reaper_linux_aarch64" | head -n 1)
if [ -z "$EXTRACTED_DIR" ]; then
    EXTRACTED_DIR=$(find "$TMP_DIR" -mindepth 1 -maxdepth 1 -type d | head -n 1)
fi

echo "Installing to /opt/REAPER..."
sudo rm -rf /opt/REAPER
sudo cp -r "$EXTRACTED_DIR/REAPER" /opt/REAPER

echo "Creating symlink at /usr/local/bin/reaper..."
sudo ln -sf /opt/REAPER/reaper /usr/local/bin/reaper

# Clean up
rm -rf "$TMP_DIR"

echo "REAPER Installation Complete!"
/usr/local/bin/reaper -h | head -n 1
