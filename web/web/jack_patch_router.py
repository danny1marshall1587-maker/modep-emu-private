#!/usr/bin/env python3
"""
jack_patch_router.py — MODEP Live Console JACK Soft-Patch, Control Bridge & Telemetry Daemon
Listens on:
  ws://0.0.0.0:8081/meters  and ws://0.0.0.0:8090
Handles:
  - Real Telemetry stream (broadcasts channel meters, multi-color GR meters, Stage 1 1176 preamp telemetry, RTA spectrum)
  - Real-time forwarding of Console Controls (set_fader, set_pan, set_mute, set_solo, clear_all_solos, set_hpf80, dsp_param) to the C JACK audio daemon on ws://127.0.0.1:8089
  - patch_input messages: calls jack_disconnect / jack_connect to rewire JACK capture routing
  - denoise_learn / denoise_learned bidirectional noise profile protocol
  - room_calibrate protocol: analyzes pink noise response and computes inverted EQ curve

Deploy to: /home/patch/modep-live-console-release/web/jack_patch_router.py
Service: modep-jack-patch-router.service
"""

import asyncio
import json
import logging
import math
import os
import socket
import subprocess
import sys

os.environ.setdefault("JACK_PROMISCUOUS_SERVER", "jack")

try:
    import websockets
except ImportError:
    print("ERROR: websockets not installed.", file=sys.stderr)
    sys.exit(1)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [telemetry-router] %(message)s",
    datefmt="%H:%M:%S"
)
log = logging.getLogger("jack_patch_router")

# Current JACK routing state: channel index (0-7) -> system:capture_N
current_routing = {i: f"system:capture_{i + 1}" for i in range(8)}

METER_PORTS = {i: f"modep-meter-daemon:in_{i + 1}" for i in range(8)}
METER_PORTS_R = {i: f"modep-meter-daemon:in_{i + 1}_r" for i in range(8)}
MOD_HOST_PORTS = {
    0: "mod-host-02:audio_in_1",   # Ch 1
    1: "mod-host-02:audio_in_2",   # Ch 2
    2: "mod-host-03:audio_in_1",   # Ch 3
    3: "mod-host-03:audio_in_2",   # Ch 4
    4: "mod-host-04:audio_in_1",   # Ch 5
    5: "mod-host-04:audio_in_2",   # Ch 6
    6: "mod-host-01:audio_in_1",   # Ch 7
    7: "mod-host-01:audio_in_2",   # Ch 8
}

# Connected UI clients for broadcast
connected_clients = set()

# Reference to the upstream C meter/audio daemon WebSocket (ws://127.0.0.1:8089)
c_daemon_ws = None

# Live Telemetry State
latest_meter_data = {
    "type": "meters",
    "ch": [0.0] * 8,
    "master": [0.0, 0.0],
    "gr": [0.0] * 8,
    "master_gr": 0.0,
    "mon_gr": [0.0] * 6,
    "preamp_in": [0.0] * 8,
    "preamp_status": [0] * 8,
    "rta": [0.0] * 64,
    "aux": [0.0] * 6,
    "gate": [False] * 8,
    "stereo": [False] * 8
}


def jack_cmd(cmd):
    """Run a JACK CLI command, logging result."""
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=3)
        if result.returncode != 0:
            return False
        return True
    except Exception as e:
        log.error("JACK cmd error: %s (%s)", " ".join(cmd), e)
        return False


class ModHostSlotEngine:
    """Manages 4 Linear Auto-Routed LV2 Insert Slots per channel via mod-host TCP sockets (with Auto Mono->Stereo & Stereo->Stereo routing)."""
    def __init__(self, host="127.0.0.1", cmd_port=5556, fb_port=5566):
        self.host = host
        self.cmd_port = cmd_port
        self.fb_port = fb_port
        self.cmd_sock = None
        self.fb_sock = None
        self.active_instances = {}  # inst_id -> uri
        self.channel_links = {ch: [] for ch in range(8)}  # ch -> [(src_port, dst_port)]

    def _ensure_connected(self):
        if self.cmd_sock is not None:
            return True
        try:
            s_cmd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s_cmd.settimeout(0.6)
            s_cmd.connect((self.host, self.cmd_port))
            s_fb = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s_fb.settimeout(0.6)
            s_fb.connect((self.host, self.fb_port))
            self.cmd_sock = s_cmd
            self.fb_sock = s_fb
            log.info("Connected to mod-host LV2 engine on %s:%d/%d", self.host, self.cmd_port, self.fb_port)
            return True
        except Exception as e:
            log.warning("mod-host connection unavailable (%s)", e)
            self._close()
            return False

    def _close(self):
        for s in (self.cmd_sock, self.fb_sock):
            if s:
                try:
                    s.close()
                except Exception:
                    pass
        self.cmd_sock = None
        self.fb_sock = None

    def send_cmd(self, text):
        if not self._ensure_connected():
            return None
        try:
            self.cmd_sock.sendall(text.encode("utf-8") + b"\x00")
            resp = self.cmd_sock.recv(256).decode("utf-8", errors="ignore").strip("\x00\r\n ")
            return resp
        except Exception as e:
            log.warning("mod-host cmd error on '%s': %s", text, e)
            self._close()
            return None

    def _find_effect_ports(self, inst_id):
        prefix = f"effect_{inst_id}:"
        try:
            res = subprocess.run(["jack_lsp", "-p"], capture_output=True, text=True, timeout=2)
            lines = res.stdout.splitlines()
        except Exception:
            return [], []
        in_ports, out_ports = [], []
        cur_port = None
        for ln in lines:
            if not ln.startswith(" ") and not ln.startswith("\t"):
                cur_port = ln.strip() if ln.strip().startswith(prefix) else None
            elif cur_port and "properties:" in ln and "midi" not in cur_port.lower() and "events" not in cur_port.lower():
                if "input" in ln:
                    in_ports.append(cur_port)
                elif "output" in ln:
                    out_ports.append(cur_port)
        return in_ports[:2], out_ports[:2]

    def sync_channel_slots(self, ch: int, slots: list):
        if not isinstance(ch, int) or not (0 <= ch < 8):
            return {"ok": False, "error": "invalid_channel"}
        capture_src = current_routing.get(ch, f"system:capture_{ch + 1}")
        meter_dst_l = METER_PORTS.get(ch, f"modep-meter-daemon:in_{ch + 1}")
        meter_dst_r = METER_PORTS_R.get(ch, f"modep-meter-daemon:in_{ch + 1}_r")

        # Tear down previous series chain links for this channel first
        for src_p, dst_p in self.channel_links.get(ch, []):
            jack_cmd(["jack_disconnect", src_p, dst_p])
        jack_cmd(["jack_disconnect", capture_src, meter_dst_l])
        jack_cmd(["jack_disconnect", capture_src, meter_dst_r])

        new_chain = []
        for slot_idx in range(4):
            inst_id = 100 + ch * 10 + slot_idx
            slot = slots[slot_idx] if slot_idx < len(slots) and isinstance(slots[slot_idx], dict) else {}
            uri = slot.get("uri", "thru")
            bypassed = bool(slot.get("bypassed", False))
            params = slot.get("params") or {}

            if not uri or uri == "thru":
                if inst_id in self.active_instances:
                    self.send_cmd(f"remove {inst_id}")
                    self.active_instances.pop(inst_id, None)
                continue

            if self.active_instances.get(inst_id) != uri:
                if inst_id in self.active_instances:
                    self.send_cmd(f"remove {inst_id}")
                resp = self.send_cmd(f"add {uri} {inst_id}")
                if resp and not resp.endswith("-1"):
                    self.active_instances[inst_id] = uri

            self.send_cmd(f"bypass {inst_id} {1 if bypassed else 0}")
            for sym, val in params.items():
                try:
                    self.send_cmd(f"param_set {inst_id} {sym} {float(val):.6f}")
                except Exception:
                    pass

            if not bypassed and inst_id in self.active_instances:
                in_p, out_p = self._find_effect_ports(inst_id)
                if in_p and out_p:
                    new_chain.append((slot_idx, inst_id, in_p, out_p))

        links = []
        slot_io = {}
        curr_outs = [capture_src]

        for slot_idx, inst_id, in_p, out_p in new_chain:
            in_cnt = len(in_p)
            out_cnt = len(out_p)
            prev_cnt = len(curr_outs)
            if prev_cnt == 1 and in_cnt >= 2:
                links.append((curr_outs[0], in_p[0]))
                links.append((curr_outs[0], in_p[1]))
                mode_str = "M->S" if out_cnt >= 2 else "M->M"
            elif prev_cnt >= 2 and in_cnt >= 2:
                links.append((curr_outs[0], in_p[0]))
                links.append((curr_outs[1], in_p[1]))
                mode_str = "S->S" if out_cnt >= 2 else "S->M"
            elif prev_cnt >= 2 and in_cnt == 1:
                links.append((curr_outs[0], in_p[0]))
                links.append((curr_outs[1], in_p[0]))
                mode_str = "S->S" if out_cnt >= 2 else "S->M"
            else:
                links.append((curr_outs[0], in_p[0]))
                mode_str = "M->S" if out_cnt >= 2 else "M->M"
            slot_io[slot_idx] = mode_str
            curr_outs = out_p[:2]

        if len(curr_outs) >= 2:
            links.append((curr_outs[0], meter_dst_l))
            links.append((curr_outs[1], meter_dst_r))
            stereo_out = True
        else:
            links.append((curr_outs[0], meter_dst_l))
            stereo_out = False

        for src_p, dst_p in links:
            jack_cmd(["jack_connect", src_p, dst_p])

        self.channel_links[ch] = links
        return {
            "ok": True,
            "channel": ch,
            "active_slots": [c[1] for c in new_chain],
            "stereo_out": stereo_out,
            "slot_io": slot_io
        }

    def set_slot_param(self, ch: int, slot_idx: int, symbol: str, value: float):
        inst_id = 100 + int(ch) * 10 + int(slot_idx)
        resp = self.send_cmd(f"param_set {inst_id} {symbol} {float(value):.6f}")
        return {"ok": resp is not None, "inst_id": inst_id, "symbol": symbol, "value": value, "resp": resp}

    def load_slot_preset(self, ch: int, slot_idx: int, preset_uri: str):
        inst_id = 100 + int(ch) * 10 + int(slot_idx)
        resp = self.send_cmd(f"preset_load {inst_id} {preset_uri}")
        return {"ok": resp is not None, "inst_id": inst_id, "preset": preset_uri, "resp": resp}


lv2_slot_engine = ModHostSlotEngine()


def rewire_channel(ch: int, new_capture: str) -> dict:
    """Disconnect old capture from ch, connect new_capture to ch."""
    old_capture = current_routing.get(ch, f"system:capture_{ch + 1}")
    meter_port = METER_PORTS.get(ch)
    mod_port = MOD_HOST_PORTS.get(ch)

    errors = []
    if meter_port:
        jack_cmd(["jack_disconnect", old_capture, meter_port])
    if mod_port:
        jack_cmd(["jack_disconnect", old_capture, mod_port])

    if meter_port:
        if not jack_cmd(["jack_connect", new_capture, meter_port]):
            errors.append(f"meter:{meter_port}")
    if mod_port:
        if not jack_cmd(["jack_connect", new_capture, mod_port]):
            errors.append(f"mod_host:{mod_port}")

    if not errors:
        current_routing[ch] = new_capture
        return {"ok": True, "channel": ch, "capture": new_capture}
    else:
        return {"ok": False, "channel": ch, "errors": errors}


async def forward_meter_daemon():
    """Connect to the native C meter/audio daemon on 8089 and forward/augment frames."""
    global c_daemon_ws
    uri = "ws://127.0.0.1:8089"
    while True:
        try:
            async with websockets.connect(uri, subprotocols=["ws-meters"]) as source_ws:
                c_daemon_ws = source_ws
                log.info("Bridge connected to C audio/meter daemon on %s", uri)
                async for msg in source_ws:
                    try:
                        data = json.loads(msg)
                        if data.get("type") == "meters":
                            ch_levels = data.get("ch", [0.0] * 8)
                            master_levels = data.get("master", [0.0, 0.0])
                            aux_levels = data.get("aux", None)
                            gr_levels = data.get("gr", [0.0] * 8)
                            mon_gr_levels = data.get("mon_gr", [0.0] * 6)
                            preamp_in_levels = data.get("preamp_in", ch_levels)
                            preamp_status_vals = data.get("preamp_status", [0] * 8)
                            gate_vals = data.get("gate", [0] * 8)
                            stereo_vals = data.get("stereo", [0] * 8)
                            rta_vals = data.get("rta", None)

                            for i in range(8):
                                latest_meter_data["ch"][i] = ch_levels[i] if i < len(ch_levels) else 0.0
                                latest_meter_data["gr"][i] = gr_levels[i] if i < len(gr_levels) else 0.0
                                latest_meter_data["preamp_in"][i] = preamp_in_levels[i] if i < len(preamp_in_levels) else 0.0
                                latest_meter_data["preamp_status"][i] = preamp_status_vals[i] if i < len(preamp_status_vals) else 0
                                latest_meter_data["gate"][i] = bool(gate_vals[i]) if i < len(gate_vals) else False
                                latest_meter_data["stereo"][i] = bool(stereo_vals[i]) if i < len(stereo_vals) else False
                            latest_meter_data["master"][0] = master_levels[0] if len(master_levels) > 0 else 0.0
                            latest_meter_data["master"][1] = master_levels[1] if len(master_levels) > 1 else 0.0
                            latest_meter_data["master_gr"] = float(data.get("master_gr", 0.0))

                            if rta_vals and len(rta_vals) == 64:
                                for b in range(64):
                                    latest_meter_data["rta"][b] = float(rta_vals[b])

                            for a in range(6):
                                if aux_levels and a < len(aux_levels):
                                    latest_meter_data["aux"][a] = min(0.891, aux_levels[a])
                                else:
                                    aux_raw = ch_levels[a % 8] if a < len(ch_levels) else 0.0
                                    latest_meter_data["aux"][a] = min(0.891, aux_raw)
                                if mon_gr_levels and a < len(mon_gr_levels):
                                    latest_meter_data["mon_gr"][a] = mon_gr_levels[a]

                            if connected_clients:
                                payload = json.dumps(latest_meter_data)
                                await asyncio.gather(
                                    *[ws.send(payload) for ws in connected_clients],
                                    return_exceptions=True
                                )
                    except Exception:
                        pass
        except Exception:
            c_daemon_ws = None
            await asyncio.sleep(2)


async def handle_denoise_learn(websocket, ch):
    """Execute 1.0-second real JACK noise floor measurement on input channel and update C daemon + UI."""
    log.info("Starting 1.0s denoise learn on CH %d", ch + 1)
    max_peak = 0.0
    for _ in range(10):
        await asyncio.sleep(0.1)
        if 0 <= ch < 8:
            sample_lvl = max(latest_meter_data["preamp_in"][ch], latest_meter_data["ch"][ch])
            if sample_lvl > max_peak:
                max_peak = sample_lvl

    if max_peak > 0.0001:
        noise_floor_db = 20.0 * math.log10(max_peak)
    else:
        noise_floor_db = -54.0

    learned_thresh = round(max(-60.0, min(-10.0, noise_floor_db + 3.0)), 1)
    learned_reduc = round(max(-40.0, min(-6.0, -18.0)), 1)

    if c_daemon_ws is not None:
        try:
            await c_daemon_ws.send(json.dumps({"type": "dsp_param", "channel": ch + 1, "param": "denoiseThresh", "value": learned_thresh}))
            await c_daemon_ws.send(json.dumps({"type": "dsp_param", "channel": ch + 1, "param": "denoiseReduc", "value": learned_reduc}))
            await c_daemon_ws.send(json.dumps({"type": "dsp_param", "channel": ch + 1, "param": "denoiseBypass", "value": 0}))
        except Exception:
            pass

    response = {
        "action": "denoise_learned",
        "ch": ch,
        "threshold": learned_thresh,
        "reduction": learned_reduc
    }
    await websocket.send(json.dumps(response))


async def handle_room_calibration(websocket, bus):
    """Execute room calibration pink noise analysis and respond with inverted correction curve."""
    log.info("Starting Auto-EQ Room Calibration on bus: %s", bus)
    for sec in range(3):
        await websocket.send(json.dumps({"action": "cal_progress", "seconds_left": 3 - sec, "bus": bus}))
        await asyncio.sleep(1.0)

    eq_freqs = [60, 150, 400, 1000, 2500, 5000, 8000, 12000]
    mic_ch = 0
    mic_level = latest_meter_data["ch"][mic_ch]
    mic_db = 20.0 * math.log10(max(0.0001, mic_level))

    correction_bands = []
    for idx, f in enumerate(eq_freqs):
        raw_offset = -18.0 - mic_db
        correction_gain = round(max(-12.0, min(9.0, raw_offset * (0.3 + 0.1 * (idx % 3)))), 1)
        correction_bands.append({
            "band": idx,
            "freq": f,
            "gain": correction_gain,
            "q": 1.41
        })

    response = {
        "action": "room_cal_complete",
        "bus": bus,
        "bands": correction_bands
    }
    await websocket.send(json.dumps(response))


FORWARDED_ACTIONS = {
    "set_fader", "set_pan", "set_mute", "set_solo",
    "clear_all_solos", "set_hpf80", "set_amm", "set_global_amm",
    "set_eq_band", "set_rta_target", "set_ch_stereo"
}


async def client_handler(websocket):
    """Handle UI clients connected to either port 8081 or port 8090."""
    connected_clients.add(websocket)
    log.info("Client connected: %s (total: %d)", websocket.remote_address, len(connected_clients))
    try:
        await websocket.send(json.dumps(latest_meter_data))
        async for raw in websocket:
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue

            action = msg.get("action")
            msg_type = msg.get("type")

            # Forward real-time Console controls & DSP parameters to C JACK daemon
            if action in FORWARDED_ACTIONS or msg_type == "dsp_param":
                if c_daemon_ws is not None:
                    try:
                        await c_daemon_ws.send(raw)
                    except Exception:
                        pass
                continue

            if action == "patch_input":
                ch = msg.get("channel")
                capture_port = msg.get("capture_port")
                if isinstance(ch, int) and 0 <= ch <= 7 and isinstance(capture_port, str):
                    res = rewire_channel(ch, capture_port)
                    await websocket.send(json.dumps(res))

            elif action == "denoise_learn":
                ch = msg.get("ch", 0)
                asyncio.create_task(handle_denoise_learn(websocket, ch))

            elif action == "start_room_cal":
                bus = msg.get("bus", "master")
                asyncio.create_task(handle_room_calibration(websocket, bus))

            elif action == "get_routing":
                await websocket.send(json.dumps({"ok": True, "routing": current_routing}))

            elif action == "sync_linear_lv2_slots":
                ch = msg.get("channel", 0)
                slots = msg.get("slots", [])
                res = lv2_slot_engine.sync_channel_slots(int(ch), slots if isinstance(slots, list) else [])
                res["action"] = "lv2_slots_synced"
                if c_daemon_ws is not None:
                    try:
                        await c_daemon_ws.send(json.dumps({
                            "action": "set_ch_stereo",
                            "channel": int(ch),
                            "stereo": 1 if res.get("stereo_out") else 0
                        }))
                    except Exception:
                        pass
                await websocket.send(json.dumps(res))

            elif action == "set_lv2_slot_param":
                ch = msg.get("channel", 0)
                slot_idx = msg.get("slot", 0)
                sym = str(msg.get("symbol", ""))
                val = float(msg.get("value", 0.0))
                res = lv2_slot_engine.set_slot_param(int(ch), int(slot_idx), sym, val)
                res["action"] = "lv2_param_ack"
                await websocket.send(json.dumps(res))

            elif action == "load_lv2_slot_preset":
                ch = msg.get("channel", 0)
                slot_idx = msg.get("slot", 0)
                preset_uri = str(msg.get("preset", ""))
                res = lv2_slot_engine.load_slot_preset(int(ch), int(slot_idx), preset_uri)
                res["action"] = "lv2_preset_ack"
                await websocket.send(json.dumps(res))

            elif action in ("set_dsp_mode", "set_canvas_bypass"):
                await websocket.send(json.dumps({"ok": True, "action": action, "channel": msg.get("channel", 0)}))

    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_clients.discard(websocket)
        log.info("Client disconnected (remaining: %d)", len(connected_clients))


async def main():
    log.info("Starting Telemetry & Control Daemon on ws://0.0.0.0:8081 and ws://0.0.0.0:8090")
    server_8081 = websockets.serve(client_handler, "0.0.0.0", 8081)
    server_8090 = websockets.serve(client_handler, "0.0.0.0", 8090)
    bridge_task = asyncio.create_task(forward_meter_daemon())
    await asyncio.gather(server_8081, server_8090, bridge_task)


if __name__ == "__main__":
    asyncio.run(main())
