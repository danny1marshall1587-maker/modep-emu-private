#!/usr/bin/env bash
# ==============================================================================
# MODEP Pro Live Mixer - Multi-Instance MODEP & JACK Insert Isolation Setup
# Configures 4 isolated MODEP instances (mod-host + mod-ui) and establishes
# virtual insert loops for channels 1-8 while fully isolating audio from
# physical hardware capture.
# ==============================================================================

set -euo pipefail

echo "=================================================================="
echo ">>> Setting up 4 Isolated MODEP Instances & Virtual JACK Routing"
echo "=================================================================="

# 1. Patch mod/host.py to support MOD_HOST_PORT and MOD_HOST_JACK_NAME
echo ">>> [1/5] Ensuring python mod package supports environment socket/client overrides..."
PYTHON_HOST_FILE="/usr/lib/python3/dist-packages/mod/host.py"

if [ -f "$PYTHON_HOST_FILE" ]; then
    if ! grep -q "MOD_HOST_PORT" "$PYTHON_HOST_FILE"; then
        echo ">>> Patching $PYTHON_HOST_FILE for MOD_HOST_PORT..."
        sudo cp "$PYTHON_HOST_FILE" "${PYTHON_HOST_FILE}.orig"
        sudo sed -i 's/self\.addr = ("localhost", 5555)/self.addr = ("localhost", int(os.environ.get("MOD_HOST_PORT", 5555)))/g' "$PYTHON_HOST_FILE"
    else
        echo ">>> MOD_HOST_PORT override already present in $PYTHON_HOST_FILE"
    fi
fi

# 2. Create isolated storage directories for each instance
echo ">>> [2/5] Creating isolated storage directories in /var/modep/instance_{1,2,3,4}..."
INSTANCES=(1 2 3 4)

for i in "${INSTANCES[@]}"; do
    TARGET_DIR="/var/modep/instance_${i}"
    echo "    -> Configuring $TARGET_DIR..."
    sudo mkdir -p "$TARGET_DIR"/{pedalboards,lv2-presets,user-files,pedalboard-tmp-data,.cache}
    
    # Copy template configs if not present
    if [ ! -f "$TARGET_DIR/prefs.json" ] && [ -f /var/modep/prefs.json ]; then
        sudo cp /var/modep/prefs.json "$TARGET_DIR/prefs.json"
    fi
    if [ ! -f "$TARGET_DIR/favorites.json" ]; then
        echo "[]" | sudo tee "$TARGET_DIR/favorites.json" > /dev/null
    fi

    # Set modep ownership
    sudo chown -R modep:modep "$TARGET_DIR"
    sudo chmod -R 775 "$TARGET_DIR"
done

# 3. Create systemd services for mod-host instances (5555, 5556, 5557, 5558)
echo ">>> [3/5] Configuring systemd services for mod-host and mod-ui..."

# Stop legacy default services to prevent port collisions
sudo systemctl stop modep-mod-ui modep-mod-host 2>/dev/null || true
sudo systemctl disable modep-mod-ui modep-mod-host 2>/dev/null || true

HOST_PORTS=(5555 5556 5557 5558)
FEEDBACK_PORTS=(5565 5566 5567 5568)
WEB_PORTS=(8881 8882 8883 8884)

for idx in 0 1 2 3; do
    INST=$((idx + 1))
    H_PORT=${HOST_PORTS[$idx]}
    F_PORT=${FEEDBACK_PORTS[$idx]}
    W_PORT=${WEB_PORTS[$idx]}
    DATA_PATH="/var/modep/instance_${INST}"

    # Generate mod-host service
    HOST_SERVICE_FILE="/etc/systemd/system/modep-mod-host-${INST}.service"
    cat <<EOF | sudo tee "$HOST_SERVICE_FILE" > /dev/null
[Unit]
Description=MOD-Host Isolated Instance ${INST} (Port ${H_PORT})
After=jack.service
BindsTo=jack.service

[Service]
LimitRTPRIO=95
LimitMEMLOCK=infinity
User=modep
Group=modep
Type=forking
Environment=JACK_PROMISCUOUS_SERVER=jack
Environment=JACK_CLIENT_NAME=mod-host-${INST}
Environment=LV2_PATH=/var/modep/lv2:${DATA_PATH}/lv2-presets
Environment=MOD_USER_FILES_DIR=${DATA_PATH}/user-files
Environment=MOD_PRESETS_DIR=${DATA_PATH}/lv2-presets
Environment=MOD_DATA_DIR=${DATA_PATH}
Environment=LD_PRELOAD=/usr/local/lib/libjack_property_fix.so
ExecStart=/usr/bin/mod-host -p ${H_PORT} -f ${F_PORT}
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

    # Generate mod-ui service
    UI_SERVICE_FILE="/etc/systemd/system/modep-mod-ui-${INST}.service"
    cat <<EOF | sudo tee "$UI_SERVICE_FILE" > /dev/null
[Unit]
Description=MOD-UI Isolated Instance ${INST} (Web Port ${W_PORT}, Host Port ${H_PORT})
After=modep-mod-host-${INST}.service
Requires=modep-mod-host-${INST}.service

[Service]
UMask=0002
LimitRTPRIO=95
LimitMEMLOCK=infinity
Environment=LV2_PATH=/var/modep/lv2:${DATA_PATH}/lv2-presets
Environment=LV2_PLUGIN_DIR=/var/modep/lv2
Environment=MOD_USER_PEDALBOARDS_DIR=${DATA_PATH}/pedalboards
Environment=MOD_DEV_ENVIRONMENT=0
Environment=MOD_DEVICE_WEBSERVER_PORT=${W_PORT}
Environment=MOD_HOST_PORT=${H_PORT}
Environment=MOD_FEEDBACK_PORT=${F_PORT}
Environment=MOD_LOG=1
Environment=MOD_APP=0
Environment=MOD_LIVE_ISO=0
Environment=MOD_SYSTEM_OUTPUT=1
Environment=MOD_DATA_DIR=${DATA_PATH}
Environment=MOD_PRESETS_DIR=${DATA_PATH}/lv2-presets
Environment=MOD_USER_FILES_DIR=${DATA_PATH}/user-files
Environment=JACK_PROMISCUOUS_SERVER=jack
Environment=MOD_HTML_DIR=/usr/share/mod/html
Environment=MOD_DEFAULT_PEDALBOARD=/usr/share/mod/default.pedalboard
WorkingDirectory=${DATA_PATH}
ExecStart=/usr/bin/authbind /usr/bin/mod-ui
User=modep
Group=modep
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

    # Allow non-root authbind on web port
    sudo touch "/etc/authbind/byport/${W_PORT}" 2>/dev/null || true
    sudo chmod 777 "/etc/authbind/byport/${W_PORT}" 2>/dev/null || true
done

sudo systemctl daemon-reload

for INST in "${INSTANCES[@]}"; do
    echo "    -> Enabling & starting modep-mod-host-${INST} and modep-mod-ui-${INST}..."
    sudo systemctl enable --now "modep-mod-host-${INST}.service"
    sudo systemctl enable --now "modep-mod-ui-${INST}.service"
done

# 4. Generate Jack Insert Routing Script
echo ">>> [4/5] Deploying JACK insert isolation routing script..."
ROUTER_SCRIPT="/usr/local/bin/modep-jack-insert-router.sh"

cat <<'EOF' | sudo tee "$ROUTER_SCRIPT" > /dev/null
#!/usr/bin/env bash
# ==============================================================================
# JACK Insert Isolation & Virtual Insert Router
# Disconnects all mod-host instances from hardware capture (system:capture_*).
# Wires virtual insert loops between mixer channel strips and mod-host instances.
# Audient EVO 4 2-channel fallback: Ch 1 & 2 receive system:capture_1/2.
# ==============================================================================

set -u

# Wait for JACK daemon to be accessible
while ! jack_lsp >/dev/null 2>&1; do
    sleep 0.5
done

echo ">>> Applying JACK insert isolation routing..."

# 1. Disconnect all mod-host instances from physical capture inputs
# MODEP MUST NEVER CONNECT DIRECTLY TO HARDWARE CAPTURE
for client in $(jack_lsp | grep -E '^mod-host'); do
    client_name=$(echo "$client" | cut -d: -f1)
    for cap in $(jack_lsp "system:capture_.*" 2>/dev/null); do
        jack_disconnect "$cap" "${client_name}:in_1" 2>/dev/null || true
        jack_disconnect "$cap" "${client_name}:in_2" 2>/dev/null || true
        jack_disconnect "$cap" "${client_name}:in" 2>/dev/null || true
    done
done

# 2. Virtual Insert Wiring Table:
# - Instance 1 (Port 8881) -> Ch 7/8 (Main Rig)
#   mixer:ch7_insert_send -> mod-host-1:in_1
#   mixer:ch8_insert_send -> mod-host-1:in_2
#   mod-host-1:out_1      -> mixer:ch7_insert_return
#   mod-host-1:out_2      -> mixer:ch8_insert_return
#
# - Instance 2 (Port 8882) -> Ch 1/2 (Insert 2)
#   mixer:ch1_insert_send -> mod-host-2:in_1
#   mixer:ch2_insert_send -> mod-host-2:in_2
#   mod-host-2:out_1      -> mixer:ch1_insert_return
#   mod-host-2:out_2      -> mixer:ch2_insert_return
#
# - Instance 3 (Port 8883) -> Ch 3/4 (Insert 3)
#   mixer:ch3_insert_send -> mod-host-3:in_1
#   mixer:ch4_insert_send -> mod-host-3:in_2
#   mod-host-3:out_1      -> mixer:ch3_insert_return
#   mod-host-3:out_2      -> mixer:ch4_insert_return
#
# - Instance 4 (Port 8884) -> Ch 5/6 (Insert 4)
#   mixer:ch5_insert_send -> mod-host-4:in_1
#   mixer:ch6_insert_send -> mod-host-4:in_2
#   mod-host-4:out_1      -> mixer:ch5_insert_return
#   mod-host-4:out_2      -> mixer:ch6_insert_return

route_insert() {
    local host_client="$1"
    local chA="$2"
    local chB="$3"

    # Only wire if endpoints exist in JACK graph
    jack_connect "mixer:ch${chA}_insert_send" "${host_client}:in_1" 2>/dev/null || true
    jack_connect "mixer:ch${chB}_insert_send" "${host_client}:in_2" 2>/dev/null || true
    jack_connect "${host_client}:out_1" "mixer:ch${chA}_insert_return" 2>/dev/null || true
    jack_connect "${host_client}:out_2" "mixer:ch${chB}_insert_return" 2>/dev/null || true
}

# Resolve mod-host JACK client names (supports mod-host-1..4 or mod-host / mod-host-01)
for i in 1 2 3 4; do
    CLIENT="mod-host-${i}"
    if ! jack_lsp | grep -q "^${CLIENT}:"; then
        # Fallback if mod-host registered with default name
        if [ "$i" -eq 1 ] && jack_lsp | grep -q "^mod-host:"; then
            CLIENT="mod-host"
        fi
    fi

    case "$i" in
        1) route_insert "$CLIENT" 7 8 ;;
        2) route_insert "$CLIENT" 1 2 ;;
        3) route_insert "$CLIENT" 3 4 ;;
        4) route_insert "$CLIENT" 5 6 ;;
    esac
done

# 3. Hardware capture assignments:
# On 2-channel Audient EVO 4, connect system:capture_1/2 to mixer:ch1/2.
# Channels 3-8 remain silent until UMC1820 is connected.
if jack_lsp | grep -q "^mixer:ch1"; then
    jack_connect "system:capture_1" "mixer:ch1" 2>/dev/null || true
    jack_connect "system:capture_2" "mixer:ch2" 2>/dev/null || true
    
    # If 8-channel interface is connected, route the remaining hardware inputs
    for ch in 3 4 5 6 7 8; do
        if jack_lsp | grep -q "^system:capture_${ch}"; then
            jack_connect "system:capture_${ch}" "mixer:ch${ch}" 2>/dev/null || true
        fi
    done
fi

echo ">>> JACK insert isolation routing complete."
EOF

sudo chmod +x "$ROUTER_SCRIPT"

# Deploy systemd service to run routing on startup and after jack restarts
ROUTER_SERVICE="/etc/systemd/system/modep-jack-router.service"
cat <<EOF | sudo tee "$ROUTER_SERVICE" > /dev/null
[Unit]
Description=MODEP JACK Insert Router & Hardware Isolation
After=jack.service modep-mod-host-1.service modep-mod-host-2.service modep-mod-host-3.service modep-mod-host-4.service
Wants=jack.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/local/bin/modep-jack-insert-router.sh
User=patch
Environment=JACK_PROMISCUOUS_SERVER=jack

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now modep-jack-router.service

# 5. Verify frontend web deployment synchronization
echo ">>> [5/5] Synchronizing frontend HTML to active document roots..."
if [ -f "/tmp/mixer_pro_index.html" ]; then
    sudo cp "/tmp/mixer_pro_index.html" /usr/share/mod/html/mixer/index.html
    sudo cp "/tmp/mixer_pro_index.html" /usr/share/mod/html/mixer.html
    sudo cp "/tmp/mixer_pro_index.html" /home/patch/modep-live-console-release/web/mixer_index.html
fi

echo "=================================================================="
echo ">>> Setup finished successfully!"
echo ">>> Instance 1 (Ch 7/8 Main Rig): http://<pi-ip>:8881"
echo ">>> Instance 2 (Ch 1/2 Insert 2): http://<pi-ip>:8882"
echo ">>> Instance 3 (Ch 3/4 Insert 3): http://<pi-ip>:8883"
echo ">>> Instance 4 (Ch 5/6 Insert 4): http://<pi-ip>:8884"
echo "=================================================================="
