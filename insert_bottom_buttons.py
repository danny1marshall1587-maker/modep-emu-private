with open("/usr/share/mod/html/index.html", "r") as f:
    html = f.read()

target = '<div id="mod-pedalboard" class="icon" data-message="Pedalboards Library"></div>'
buttons = """<div id="mod-pedalboard" class="icon" data-message="Pedalboards Library"></div>
        <div id="mod-mixer-btn" class="icon" data-message="8-Ch Live Mixer Console" onclick="window.open('/mixer', '_blank')" style="cursor:pointer; display:flex; align-items:center; justify-content:center; color:#3498db; font-size:10px; font-weight:bold; border:1px solid #2980b9; border-radius:4px; margin:0 3px; background:#1c2833; padding:0 6px;">🎛️ MIXER</div>
        <div id="mod-daw-btn" class="icon" data-message="20-Track Live DAW Recorder" onclick="window.open('/daw', '_blank')" style="cursor:pointer; display:flex; align-items:center; justify-content:center; color:#e74c3c; font-size:10px; font-weight:bold; border:1px solid #c0392b; border-radius:4px; margin:0 3px; background:#2c1b1b; padding:0 6px;">⏺️ DAW</div>"""

if target in html and "mod-mixer-btn" not in html:
    html = html.replace(target, buttons, 1)
    with open("/usr/share/mod/html/index.html", "w") as f:
        f.write(html)
    print("BUTTONS_INSERTED_SUCCESSFULLY")
else:
    print("BUTTONS_ALREADY_PRESENT_OR_TARGET_NOT_FOUND")
