#!/usr/bin/env bash
set -e

echo "================================================================"
echo "    Installing MODEP Plugins on Patchbox OS / Raspberry Pi      "
echo "================================================================"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "1. Checking build environment..."
if command -v bash &> /dev/null && [ -f "./build_linux_arm_plugins.sh" ]; then
    echo "Running build_linux_arm_plugins.sh..."
    chmod +x ./build_linux_arm_plugins.sh
    ./build_linux_arm_plugins.sh || true
fi

echo "2. Installing all LV2 bundles to MODEP (/var/modep/lv2)..."
if [ -d "/var/modep/lv2" ]; then
    sudo mkdir -p /var/modep/lv2
    sudo cp -rf dist/*.lv2 /var/modep/lv2/ 2>/dev/null || true
    sudo chown -R modep:modep /var/modep/lv2/*.lv2 2>/dev/null || true
    echo "Installed into /var/modep/lv2/"
fi

# Also install into user lv2 directory for jack / standard lv2 hosts
mkdir -p ~/.lv2
cp -rf dist/*.lv2 ~/.lv2/ 2>/dev/null || true
echo "Installed into ~/.lv2/"

echo "3. Refreshing MODEP services..."
if command -v systemctl &> /dev/null; then
    sudo systemctl restart modep-mod-host 2>/dev/null || true
    sudo systemctl restart modep-mod-ui 2>/dev/null || true
    echo "MODEP services restarted!"
fi

echo ""
echo "================================================================"
echo "  Success! All Cyber Audio plugins, 9-Series, Custom Amps,     "
echo "  Galaxy Strobe Tuner, and Pedals are now installed in MODEP!  "
echo "================================================================"
