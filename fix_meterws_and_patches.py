import os
import json
import re

def main():
    pedalboard_dirs = [
        "/var/modep/pedalboards",
        "/var/modep/instance_1/pedalboards",
        "/var/modep/instance_2/pedalboards",
        "/var/modep/instance_3/pedalboards",
        "/var/modep/instance_4/pedalboards",
        "/home/patch/.pedalboards"
    ]

    patches = [
        {
            "id": "thru",
            "name": "EMPTY (THRU)",
            "cat": "Passthrough",
            "icon": "➡️",
            "path": ""
        }
    ]
    seen = {"empty (thru)"}

    for pdir in pedalboard_dirs:
        if os.path.exists(pdir):
            for entry in sorted(os.listdir(pdir)):
                if entry.endswith(".pedalboard"):
                    name = entry.replace(".pedalboard", "").replace("_", " ").title().strip()
                    bundle_path = os.path.join(pdir, entry)
                    if name.lower() not in seen:
                        seen.add(name.lower())
                        low = name.lower()
                        cat = "General"
                        icon = "🎛️"
                        if "guitar" in low or "nam" in low or "country" in low:
                            cat = "Amp / Guitar Rig"
                            icon = "🎸"
                        elif "acoustic" in low or "parlor" in low:
                            cat = "Acoustic / Piezo"
                            icon = "🪕"
                        elif "bass" in low:
                            cat = "Bass Rig"
                            icon = "🔊"
                        elif "vocal" in low:
                            cat = "Vocal Strip"
                            icon = "🎙️"
                        elif "verb" in low or "delay" in low or "echo" in low:
                            cat = "Time & Space"
                            icon = "🏛️"

                        patches.append({
                            "id": entry,
                            "name": name,
                            "cat": cat,
                            "icon": icon,
                            "path": bundle_path
                        })

    print(f"Loaded {len(patches)} MODEP patches.")

    # 1. Update dsp-rack.js
    dsp_file = "/home/patch/modep-live-console-release/web/js/dsp-rack.js"
    with open(dsp_file, "r", encoding="utf-8") as f:
        dsp = f.read()

    # Fix dispatchDspParam meterWs undefined error
    dsp = re.sub(
        r'function dispatchDspParam\([^\)]*\)\s*\{[\s\S]*?\}',
        '''function dispatchDspParam(ch, param, value) {
            const ws = window.meterWs;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'dsp_param', channel: ch, param: param, value: value }));
            }
        }''',
        dsp,
        count=1
    )

    dsp = dsp.replace(
        "if (meterWs && meterWs.readyState === WebSocket.OPEN)",
        "if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN)"
    )

    # Replace window.AVAILABLE_PLUGINS directly with string formatting (no regex backslash issues)
    start_tag = "window.AVAILABLE_PLUGINS = ["
    idx = dsp.find(start_tag)
    if idx != -1:
        end_idx = dsp.find("];", idx)
        if end_idx != -1:
            patches_str = "window.AVAILABLE_PLUGINS = " + json.dumps(patches, ensure_ascii=False, indent=4) + ";"
            dsp = dsp[:idx] + patches_str + dsp[end_idx+2:]
            print("Injected real MODEP patches into dsp-rack.js.")

    with open(dsp_file, "w", encoding="utf-8") as f:
        f.write(dsp)

    # 2. Fix reaper-transport.js stubs
    rt_file = "/home/patch/modep-live-console-release/web/js/reaper-transport.js"
    with open(rt_file, "r", encoding="utf-8") as f:
        rt = f.read()

    stub_header = "// --- GLOBAL DSP MODAL & RACK HANDLERS ---"
    if stub_header in rt:
        end_stub = "window.closeModalOnOverlay = function(event) {"
        s_idx = rt.find(stub_header)
        e_idx = rt.find("};", rt.find(end_stub))
        if s_idx != -1 and e_idx != -1:
            rt = "// DSP modal stubs removed - using real room-suite.js\n" + rt[e_idx+2:].lstrip()
            with open(rt_file, "w", encoding="utf-8") as f:
                f.write(rt)
            print("Removed dummy modal stubs from reaper-transport.js.")

    # 3. Clean HTML head
    for html_path in ["/home/patch/modep-live-console-release/web/index.html", "/home/patch/modep-live-console-release/web/mixer_index.html"]:
        with open(html_path, "r", encoding="utf-8") as f:
            h = f.read()
        h = re.sub(r'<script id="global-dsp-handlers">[\s\S]*?</script>', '', h)
        with open(html_path, "w", encoding="utf-8") as f:
            f.write(h)

if __name__ == "__main__":
    main()
