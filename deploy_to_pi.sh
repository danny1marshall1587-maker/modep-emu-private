#!/bin/bash
set -e

echo "=== Building MODEP 8-Channel Live Mixer & 20-Channel DAW Plugins ==="

INSTALL_DIR="/var/modep/lv2"
mkdir -p "$INSTALL_DIR"

# 1. Compile 8-Channel Digital Mixer
echo "Compiling mod-digital-mixer-8..."
cd /home/patch/plugins/mod-digital-mixer-8
g++ -O3 -fPIC -shared -std=c++17 -march=armv8-a+crc+crypto \
    mod_digital_mixer_8.cpp -o mod_digital_mixer_8.so -lm

BUNDLE_MIXER="$INSTALL_DIR/mod-digital-mixer-8.lv2"
mkdir -p "$BUNDLE_MIXER"
cp mod_digital_mixer_8.so manifest.ttl mod_digital_mixer_8.ttl "$BUNDLE_MIXER/"
echo "Installed $BUNDLE_MIXER successfully."

# 2. Compile 20-Channel Multi-Track DAW
echo "Compiling mod-multitrack-daw-20..."
cd /home/patch/plugins/mod-multitrack-daw-20
g++ -O3 -fPIC -shared -std=c++17 -march=armv8-a+crc+crypto -pthread \
    mod_multitrack_daw_20.cpp -o mod_multitrack_daw_20.so -lm

BUNDLE_DAW="$INSTALL_DIR/mod-multitrack-daw-20.lv2"
mkdir -p "$BUNDLE_DAW"
cp mod_multitrack_daw_20.so manifest.ttl mod_multitrack_daw_20.ttl "$BUNDLE_DAW/" 2>/dev/null || true
echo "Installed $BUNDLE_DAW successfully."

# Refresh MODEP LV2 Cache
echo "Refreshing MODEP plugin cache..."
systemctl restart modep-mod-host || true
systemctl restart modep-mod-ui || true

echo "=== Build and Installation Complete! ==="
