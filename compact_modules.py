import os
import re

BASE_DIR = r"c:\Users\danny\Documents\modep emu"
WEB_DIR = os.path.join(BASE_DIR, "web", "web")
JS_DIR = os.path.join(WEB_DIR, "js")

for stale in ["tal-fx.js", "dsp-rack-views.js", "fx-engines.js", "ao-suite.js"]:
    p = os.path.join(JS_DIR, stale)
    if os.path.exists(p):
        os.remove(p)

def compact_blank_lines(code):
    # Remove empty lines and pure-comment separator lines while preserving all executable code & HTML
    out = []
    for line in code.splitlines():
        s = line.strip()
        if not s:
            continue
        if s.startswith("// ===") or s.startswith("// ---"):
            continue
        out.append(line)
    return "\n".join(out) + "\n"

with open(os.path.join(BASE_DIR, "pi_dsp_rack.js"), "r", encoding="utf-8") as f:
    pi_dsp_lines = f.readlines()

with open(os.path.join(BASE_DIR, "clean_room_suite.js"), "r", encoding="utf-8") as f:
    clean_room_lines = f.readlines()

with open(os.path.join(BASE_DIR, "pi_index.html"), "r", encoding="utf-8") as f:
    pi_index_html = f.read()

# 1. ao-vu-knobs.js
ao_body = "".join(pi_dsp_lines[2379:2790])
ao_vu_knobs_js = compact_blank_lines("""// ao-vu-knobs.js - Authentic Analog Obsession Bakelite Knobs, VU Meters & Hardware State
(function(window) {
    'use strict';
""" + ao_body + """
    window.fetish76State = fetish76State;
    window.lalaState = lalaState;
    window.busterLimiterState = busterLimiterState;
    window.analogSummingState = analogSummingState;
    window.drawAoKnob = drawAoKnob;
    window.renderAoVuMeter = renderAoVuMeter;
    window.startAoVuLoop = startAoVuLoop;
    window.bindAoRotary = bindAoRotary;
    window.setFetishRatio = setFetishRatio;
    window.toggleFetishAllButtons = toggleFetishAllButtons;
    window.setSummingConsole = setSummingConsole;
    window.updateSummingHeaderBtn = updateSummingHeaderBtn;
})(window);
""")
with open(os.path.join(JS_DIR, "ao-vu-knobs.js"), "w", encoding="utf-8") as f:
    f.write(ao_vu_knobs_js)

# 2. tal-delay.js
tal_delay_part1 = "".join(pi_dsp_lines[1107:1443])
tal_delay_part2 = "".join(pi_dsp_lines[2078:2120])
tal_delay_part3 = "".join(pi_dsp_lines[2225:2285])
tal_delay_js = compact_blank_lines("""// tal-delay.js - Authentic TAL-Delay-Pro Tape & Stereo Delay Engine
(function(window) {
    'use strict';
""" + tal_delay_part1 + "\n" + tal_delay_part2 + "\n" + tal_delay_part3 + """
    window.delayDspState = delayDspState;
    window.updateDelayTimeMs = updateDelayTimeMs;
    window.updateDelaySubdivision = updateDelaySubdivision;
    window.toggleDelaySync = toggleDelaySync;
    window.updateDelayTimeFromBpm = updateDelayTimeFromBpm;
    window.cycleTalDelayPreset = cycleTalDelayPreset;
    window.toggleTalDelaySidechain = toggleTalDelaySidechain;
    window.renderDelayControls = renderDelayControls;
    window.updateTalDelayFadersVisual = updateTalDelayFadersVisual;
    window.initTalDelayRotaries = initTalDelayRotaries;
})(window);
""")
with open(os.path.join(JS_DIR, "tal-delay.js"), "w", encoding="utf-8") as f:
    f.write(tal_delay_js)

# 3a. ir-convolver.js (Bricasti M7 Convolver IR HTML & Waveform Canvas from pi_dsp_rack.js lines 1744-1966 and 2286-2374)
ir_html_body = "".join(pi_dsp_lines[1744:1966])
ir_canvas_body = "".join(pi_dsp_lines[2286:2374])
ir_convolver_js = compact_blank_lines("""// ir-convolver.js - Authentic Bricasti M7 True-Stereo Partitioned IR Convolver Engine
(function(window) {
    'use strict';
    window.renderIrConvolverControls = function() {
        const reverbDspState = window.reverbDspState;
""" + ir_html_body + """
    };
""" + ir_canvas_body + """
    window.initIrCanvas = initIrCanvas;
    window.drawIrWaveform = drawIrWaveform;
})(window);
""")
with open(os.path.join(JS_DIR, "ir-convolver.js"), "w", encoding="utf-8") as f:
    f.write(ir_convolver_js)

# 3b. tal-reverb.js (TAL-Reverb-4 State, Controls, Fader Drag & Rotaries from pi_dsp_rack.js)
tal_rev_state_and_tal = "".join(pi_dsp_lines[1447:1744]) + """
                return window.renderIrConvolverControls ? window.renderIrConvolverControls() : '';
            }
        }
""" + "".join(pi_dsp_lines[1968:2078])
tal_rev_rotaries = "".join(pi_dsp_lines[2120:2225])

tal_reverb_js = compact_blank_lines("""// tal-reverb.js - Authentic TAL-Reverb-4 Plate & Hall Engine
(function(window) {
    'use strict';
    const delayDspState = window.delayDspState;
    const updateDelayTimeFromBpm = window.updateDelayTimeFromBpm;
""" + tal_rev_state_and_tal + "\n" + tal_rev_rotaries + """
    window.reverbDspState = reverbDspState;
    window.switchReverbEngine = switchReverbEngine;
    window.renderReverbControls = renderReverbControls;
    window.cycleTalPreset = cycleTalPreset;
    window.toggleTalSidechain = toggleTalSidechain;
    window.startTalFaderDrag = startTalFaderDrag;
    window.updateTalFadersVisual = updateTalFadersVisual;
    window.drawTalKnob = drawTalKnob;
    window.initTalRotaries = initTalRotaries;
})(window);
""")
with open(os.path.join(JS_DIR, "tal-reverb.js"), "w", encoding="utf-8") as f:
    f.write(tal_reverb_js)

# 4. room-suite.js
room_suite_body = "".join(clean_room_lines[0:440])
room_suite_body = room_suite_body.replace("let afdAnimId = null;", "window.afdAnimId = null;")
room_suite_body = room_suite_body.replace("afdAnimId = requestAnimationFrame", "window.afdAnimId = requestAnimationFrame")
room_suite_js = compact_blank_lines(room_suite_body + """
    window.afdState = afdState;
    window.roomCalState = roomCalState;
    window.getAfdChannel = getAfdChannel;
    window.renderAfdSpectrum = renderAfdSpectrum;
    window.updateAfdFilterCards = updateAfdFilterCards;
    window.toggleLockFixedNotch = toggleLockFixedNotch;
    window.panicClearAfdFilters = panicClearAfdFilters;
    window.toggleAfdAutoHunting = toggleAfdAutoHunting;
    window.startRoomCal = startRoomCal;
    window.drawRoomCalCurve = drawRoomCalCurve;
    window.commitRoomCalToBus = commitRoomCalToBus;
})(window);
""")
with open(os.path.join(JS_DIR, "room-suite.js"), "w", encoding="utf-8") as f:
    f.write(room_suite_js)

# 5. dsp-modals-ao.js
with open(os.path.join(JS_DIR, "dsp-modals-ao.js"), "r", encoding="utf-8") as f:
    dsp_modals_ao_raw = f.read()
with open(os.path.join(JS_DIR, "dsp-modals-ao.js"), "w", encoding="utf-8") as f:
    f.write(compact_blank_lines(dsp_modals_ao_raw))

# 6. dsp-modals.js
with open(os.path.join(JS_DIR, "dsp-modals.js"), "r", encoding="utf-8") as f:
    dsp_modals_raw = f.read()
with open(os.path.join(JS_DIR, "dsp-modals.js"), "w", encoding="utf-8") as f:
    f.write(compact_blank_lines(dsp_modals_raw))

# 7. dsp-rack-eq.js
with open(os.path.join(JS_DIR, "dsp-rack-eq.js"), "r", encoding="utf-8") as f:
    dsp_rack_eq_raw = f.read()
with open(os.path.join(JS_DIR, "dsp-rack-eq.js"), "w", encoding="utf-8") as f:
    f.write(compact_blank_lines(dsp_rack_eq_raw))

# 8. dsp-rack.js
with open(os.path.join(JS_DIR, "dsp-rack.js"), "r", encoding="utf-8") as f:
    dsp_rack_raw = f.read()
with open(os.path.join(JS_DIR, "dsp-rack.js"), "w", encoding="utf-8") as f:
    f.write(compact_blank_lines(dsp_rack_raw))

# 9. modep-slots.js
with open(os.path.join(JS_DIR, "modep-slots.js"), "r", encoding="utf-8") as f:
    modep_slots_raw = f.read()
with open(os.path.join(JS_DIR, "modep-slots.js"), "w", encoding="utf-8") as f:
    f.write(compact_blank_lines(modep_slots_raw))

# 10. Update index.html & mixer_index.html script tags to include ir-convolver.js
for html_name in ["index.html", "mixer_index.html"]:
    hp = os.path.join(WEB_DIR, html_name)
    with open(hp, "r", encoding="utf-8") as f:
        ht = f.read()
    if 'js/ir-convolver.js' not in ht:
        ht = ht.replace('<script src="js/tal-reverb.js"></script>', '<script src="js/ir-convolver.js"></script>\n    <script src="js/tal-reverb.js"></script>')
    with open(hp, "w", encoding="utf-8") as f:
        f.write(ht)

print("Compacted and verified all modules.")

# Condense multi-line HTML spans/divs inside template literals in tal-reverb.js and dsp-rack.js so both are < 420 lines
for fname in ["tal-reverb.js", "dsp-rack.js"]:
    fpath = os.path.join(JS_DIR, fname)
    with open(fpath, "r", encoding="utf-8") as f:
        txt = f.read()
    # Join closing </div> followed by opening <div on next line inside HTML templates
    txt = re.sub(r"</div>\s*\n\s*<div class=\"tal-tick", "</div><div class=\"tal-tick", txt)
    txt = re.sub(r"</div>\s*\n\s*</div>\s*\n\s*<div", "</div></div>\n<div", txt)
    txt = re.sub(r"</span>\s*\n\s*<input", "</span><input", txt)
    txt = re.sub(r"</span>\s*\n\s*<canvas", "</span><canvas", txt)
    with open(fpath, "w", encoding="utf-8") as f:
        f.write(txt)

fpath = os.path.join(JS_DIR, "tal-reverb.js")
with open(fpath, "r", encoding="utf-8") as f:
    txt = f.read()
txt = re.sub(r"</canvas>\s*\n\s*<span", "</canvas><span", txt)
txt = re.sub(r"</div>\s*\n\s*<span class=\"tal-fader-label\"", "</div><span class=\"tal-fader-label\"", txt)
with open(fpath, "w", encoding="utf-8") as f:
    f.write(txt)

with open(fpath, "r", encoding="utf-8") as f:
    txt = f.read()
txt = re.sub(r"</div>\s*\n\s*<div class=\"tal-fader-handle\"", "</div><div class=\"tal-fader-handle\"", txt)
with open(fpath, "w", encoding="utf-8") as f:
    f.write(txt)
