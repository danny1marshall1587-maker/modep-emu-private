# SPDX-FileCopyrightText: 2026 Cyber Audio
# SPDX-License-Identifier: MIT
# Cyber Audio System & Hardware Controller Module for MODEP / Patchbox OS

import os
import re
import json
import logging
import subprocess

def run_cmd(cmd, timeout=10):
    try:
        res = subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True, text=True, timeout=timeout)
        return res.returncode == 0, res.stdout.strip(), res.stderr.strip()
    except Exception as e:
        return False, "", str(e)

class CyberSystemManager:
    """Manages Wi-Fi scanning/connecting, Audio interface/buffer sizing, and System health."""

    # ── NETWORK / WI-FI ──────────────────────────────────────────────────
    @staticmethod
    def get_network_status():
        """Returns active IP, connection type, SSID, and mode (hotspot vs wifi)."""
        status = {
            "mode": "unknown",
            "ssid": "",
            "ip": "127.0.0.1",
            "is_hotspot": False,
            "connected": False
        }

        # Check if hotspot service is active
        ok, out, _ = run_cmd("systemctl is-active wifi-hotspot")
        if ok and "active" in out:
            status["is_hotspot"] = True
            status["mode"] = "hotspot"
            status["ssid"] = "Patchbox"
            status["ip"] = "172.24.1.1"
            status["connected"] = True
            return status

        # Check active NetworkManager connections
        ok, out, _ = run_cmd("nmcli -t -f NAME,TYPE,DEVICE,STATE c show --active")
        if ok and out:
            for line in out.splitlines():
                parts = line.split(":")
                if len(parts) >= 4 and parts[1] == "802-11-wireless":
                    status["ssid"] = parts[0]
                    status["mode"] = "wifi"
                    status["connected"] = True
                    break

        # Get IP address
        ok, out, _ = run_cmd("hostname -I")
        if ok and out:
            ips = out.split()
            if ips:
                status["ip"] = ips[0]

        return status

    @staticmethod
    def scan_wifi():
        """Scans for nearby wireless networks and returns unique SSIDs with signal strength."""
        # Ensure wifi is enabled
        run_cmd("nmcli r wifi on")
        ok, out, _ = run_cmd("nmcli -t -f SSID,SIGNAL,SECURITY dev wifi list --rescan yes", timeout=15)
        if not ok or not out:
            ok, out, _ = run_cmd("nmcli -t -f SSID,SIGNAL,SECURITY dev wifi list", timeout=5)

        networks = []
        seen = set()
        if ok and out:
            for line in out.splitlines():
                parts = line.split(":")
                if len(parts) >= 2:
                    ssid = parts[0].strip()
                    if not ssid or ssid in seen:
                        continue
                    seen.add(ssid)
                    try:
                        signal = int(parts[1])
                    except:
                        signal = 50
                    sec = parts[2] if len(parts) > 2 else ""
                    networks.append({
                        "ssid": ssid,
                        "signal": signal,
                        "secure": bool(sec and sec != "--")
                    })

        # Sort by signal strength descending
        networks.sort(key=lambda x: x["signal"], reverse=True)
        return networks

    @staticmethod
    def connect_wifi(ssid, password=""):
        """Connects to a specific Wi-Fi network and stops hotspot mode."""
        # 1. Stop conflicting hotspot service
        run_cmd("systemctl stop wifi-hotspot")

        # 2. Connect via nmcli
        if password:
            cmd = f'nmcli dev wifi connect "{ssid}" password "{password}"'
        else:
            cmd = f'nmcli dev wifi connect "{ssid}"'

        ok, out, err = run_cmd(cmd, timeout=25)
        if not ok:
            # If connect failed, restart hotspot so user isn't permanently locked out!
            run_cmd("systemctl start wifi-hotspot")
            return False, err or out

        # Ensure connection persists and autoconnects
        run_cmd(f'nmcli c modify "{ssid}" connection.autoconnect yes connection.autoconnect-priority 100')
        return True, "Connected successfully"

    @staticmethod
    def start_hotspot():
        """Switches device back into standalone Access Point Hotspot mode."""
        run_cmd("systemctl start wifi-hotspot")
        return True, "Hotspot started on 172.24.1.1"

    # ── AUDIO HARDWARE & BUFFER ──────────────────────────────────────────
    @staticmethod
    def get_audio_info():
        """Reads current JACK audio card, buffer size, sample rate, and available ALSA cards."""
        info = {
            "current_card": "default",
            "buffer_size": 128,
            "sample_rate": 48000,
            "cards": []
        }

        # Inspect current JACK configuration
        jack_config = "/etc/jackdrc"
        if not os.path.exists(jack_config):
            jack_config = os.path.expanduser("~/.jackdrc")

        if os.path.exists(jack_config):
            try:
                with open(jack_config, "r") as f:
                    content = f.read()
                    # Look for -p (period/buffer size)
                    m_p = re.search(r'-p\s*(\d+)', content)
                    if m_p:
                        info["buffer_size"] = int(m_p.group(1))
                    # Look for -r (rate)
                    m_r = re.search(r'-r\s*(\d+)', content)
                    if m_r:
                        info["sample_rate"] = int(m_r.group(1))
                    # Look for -d (device hw:X)
                    m_d = re.search(r'-d\s*([^\s]+)', content)
                    if m_d:
                        info["current_card"] = m_d.group(1)
            except Exception as e:
                logging.error(f"Error reading jackdrc: {e}")

        # Scan ALSA audio cards via aplay -l
        ok, out, _ = run_cmd("aplay -l")
        if ok and out:
            card_blocks = re.findall(r'card\s+(\d+):\s+([^\[]+)\[([^\]]+)\],\s+device\s+(\d+):\s+([^\[]+)', out)
            for c_id, c_name, c_desc, d_id, d_name in card_blocks:
                info["cards"].append({
                    "id": f"hw:{c_id},{d_id}",
                    "card": int(c_id),
                    "device": int(d_id),
                    "name": c_desc.strip(),
                    "display": f"Card {c_id}: {c_desc.strip()} ({d_name.strip()})"
                })

        return info

    @staticmethod
    def set_audio_config(card_id, buffer_size, sample_rate=48000):
        """Updates JACK configuration file and restarts jack + modep services."""
        buffer_size = int(buffer_size)
        sample_rate = int(sample_rate)

        # 1. Update /etc/jackdrc and /var/modep/jackdrc
        jack_cmd = f"/usr/bin/jackd -t 2000 -d alsa -d {card_id} -r {sample_rate} -p {buffer_size} -n 2 -X seq\n"

        for path in ["/etc/jackdrc", os.path.expanduser("~/.jackdrc"), "/var/modep/jackdrc"]:
            try:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w") as f:
                    f.write(jack_cmd)
            except Exception as e:
                logging.error(f"Could not write {path}: {e}")

        # 2. Restart JACK service
        run_cmd("systemctl restart jack.service")
        run_cmd("systemctl restart modep-mod-host.service")
        return True, f"Audio set to {buffer_size} smp @ {sample_rate}Hz"

    # ── SYSTEM HEALTH & ACTIONS ──────────────────────────────────────────
    @staticmethod
    def get_system_stats():
        """Returns CPU temperature, RAM usage, CPU load, and disk space."""
        stats = {
            "cpu_temp": "N/A",
            "ram_used": "N/A",
            "disk_free": "N/A"
        }
        # Temperature
        if os.path.exists("/sys/class/thermal/thermal_zone0/temp"):
            try:
                with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
                    temp_raw = int(f.read().strip())
                    stats["cpu_temp"] = f"{round(temp_raw / 1000.0, 1)} °C"
            except:
                pass

        # Free disk
        ok, out, _ = run_cmd("df -h /")
        if ok and out:
            lines = out.splitlines()
            if len(lines) > 1:
                parts = lines[1].split()
                if len(parts) >= 4:
                    stats["disk_free"] = parts[3]

        return stats

    @staticmethod
    def reboot():
        run_cmd("systemctl reboot")
        return True, "Rebooting..."

    @staticmethod
    def poweroff():
        run_cmd("systemctl poweroff")
        return True, "Shutting down..."
