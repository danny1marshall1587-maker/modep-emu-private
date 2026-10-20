# Fix the syntax typos in dsp-rack.js and room-suite.js directly

dsp_file = "/home/patch/modep-live-console-release/web/js/dsp-rack.js"
with open(dsp_file, "r", encoding="utf-8") as f:
    dsp = f.read()

# Replace the mangled dispatchDspParam
bad_dispatch = """        function dispatchDspParam(ch, param, value) {
            const ws = window.meterWs;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'dsp_param', channel: ch, param: param, value: value }));
            }
        }));
            }
        }"""

clean_dispatch = """        function dispatchDspParam(ch, param, value) {
            const ws = window.meterWs;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'dsp_param', channel: ch, param: param, value: value }));
            }
        }"""

if bad_dispatch in dsp:
    dsp = dsp.replace(bad_dispatch, clean_dispatch)
else:
    # generic regex fix
    import re
    dsp = re.sub(
        r'function dispatchDspParam\([^\)]*\)\s*\{[\s\S]*?\}\s*\}\)\;\s*\}\s*\}',
        clean_dispatch.strip(),
        dsp
    )

with open(dsp_file, "w", encoding="utf-8") as f:
    f.write(dsp)
print("Fixed dsp-rack.js")

# Fix room-suite.js orphan console.log line
room_file = "/home/patch/modep-live-console-release/web/js/room-suite.js"
with open(room_file, "r", encoding="utf-8") as f:
    room = f.read()

bad_orphan = """// stub removed: setChannelCompModel
  console.log("[Channel] Comp model changed:", ch, model);
};"""

if bad_orphan in room:
    room = room.replace(bad_orphan, "// stub removed: setChannelCompModel")

with open(room_file, "w", encoding="utf-8") as f:
    f.write(room)
print("Fixed room-suite.js")
