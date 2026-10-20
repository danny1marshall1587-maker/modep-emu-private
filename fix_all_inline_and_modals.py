import re

# 1. FIX ROOM-SUITE.JS:
#    Fix `window.openDspModal = openDspModal;` where `openDspModal` wasn't a local identifier
room_file = "/home/patch/modep-live-console-release/web/js/room-suite.js"
with open(room_file, "r", encoding="utf-8") as f:
    room = f.read()

# Make sure openDspModal is declared both locally and on window inside room-suite.js
room = room.replace(
    "window.openDspModal = function(type, target) {",
    "function openDspModal(type, target) {\n            window.openDspModal = openDspModal;"
)

# Clean the end of room-suite.js so it never references an undefined variable or overwrites window.openDspModal
room = re.sub(
    r'window\.openDspModal = openDspModal;\s*window\.closeDspModal = closeDspModal;\s*window\.closeModalOnOverlay = closeModalOnOverlay;\s*\}\)\(window\);[\s\S]*$',
    '''    window.openDspModal = openDspModal;
    window.closeDspModal = closeDspModal;
    window.closeModalOnOverlay = closeModalOnOverlay;
})(window);
''',
    room
)

# Replace any `oninput="dState.` or `onclick="dState.` in room-suite.js with `getChannelRackState(${chIdx}).deesser.`
room = re.sub(
    r'oninput="dState\.',
    'oninput="window.getChannelRackState(${chIdx}).deesser.',
    room
)
room = re.sub(
    r'onclick="dState\.mode=dState\.mode===\'band\'\?\'wide\':\'band\';\s*dispatchDspParam\(\$\{chIdx\},\'loadesMode\',dState\.mode\);\s*this\.innerText=dState\.mode\.toUpperCase\(\);\s*this\.classList\.toggle\(\'active\',dState\.mode===\'wide\'\);"',
    'onclick="const ds=window.getChannelRackState(${chIdx}).deesser; ds.mode=ds.mode===\'band\'?\'wide\':\'band\'; dispatchDspParam(${chIdx},\'loadesMode\',ds.mode); this.innerText=ds.mode.toUpperCase(); this.classList.toggle(\'active\',ds.mode===\'wide\');"',
    room
)

with open(room_file, "w", encoding="utf-8") as f:
    f.write(room)
print("Fixed room-suite.js: openDspModal and deesser inline handlers.")

# 2. FIX DSP-RACK.JS:
#    Replace EVERY inline `oninput="state.` and `onclick="state.` with `window.getChannelRackState(${ch}).`
#    Because inline HTML attributes execute in global window scope where `state` is NOT the local `const state`!
dsp_file = "/home/patch/modep-live-console-release/web/js/dsp-rack.js"
with open(dsp_file, "r", encoding="utf-8") as f:
    dsp = f.read()

# Replace all inline handler occurrences of `state.` inside oninput="..." and onclick="..."
# Specifically:
# state.gain -> window.getChannelRackState(${ch}).gain
# state.denoiser -> window.getChannelRackState(${ch}).denoiser
# state.compParams -> window.getChannelRackState(${ch}).compParams
# state.deesser -> window.getChannelRackState(${ch}).deesser

def replace_inline_state(match):
    attr_content = match.group(0)
    # Replace state.xxx with window.getChannelRackState(${ch}).xxx inside the handler string
    attr_content = re.sub(r'\bstate\.(gain|denoiser|compParams|deesser|phaseInvert|compModel|directTap)\b', r'window.getChannelRackState(${ch}).\1', attr_content)
    return attr_content

dsp = re.sub(r'on(?:input|click|change)="[^"]*"', replace_inline_state, dsp)

# Also wire handleRackGainChange and toggleRackRegain to the real channel gain functions!
real_rack_gain_handlers = """
        window.toggleRackRegain = function() {
            const ch = dspRackCurrentChannel || 0;
            if (typeof window.toggleRegain === 'function') {
                window.toggleRegain(ch);
            }
            const regainActive = (window.channelRegainActive && window.channelRegainActive[ch]) ?? true;
            const regainBtn = document.getElementById('dsp-rack-regain-btn');
            if (regainBtn) {
                regainBtn.classList.toggle('active', regainActive);
                regainBtn.innerText = regainActive ? 'REGAIN ON' : 'REGAIN OFF';
            }
        };

        window.handleRackGainChange = function(a, b) {
            const ch = (b !== undefined) ? a : (dspRackCurrentChannel || 0);
            const val = parseFloat((b !== undefined) ? b : a);
            const st = getChannelRackState(ch);
            st.gain = val;
            if (typeof window.handleInputGainChange === 'function') {
                window.handleInputGainChange(ch, val);
            }
            const gainVal = document.getElementById('dsp-rack-gain-val');
            if (gainVal) {
                gainVal.innerText = (val >= 0 ? '+' : '') + val.toFixed(1) + ' dB';
            }
        };
"""

if "window.handleRackGainChange = function(a, b)" in dsp:
    # Remove the console.log stub at top and put real implementation before closing IIFE
    dsp = re.sub(r'window\.toggleRackRegain = window\.toggleRackRegain \|\| function\(ch\) \{[\s\S]*?\};\s*window\.handleRackGainChange = function\(a, b\) \{[\s\S]*?\};', '// Rack gain handlers bound inside IIFE', dsp)

dsp = dsp.replace(
    "window.showDspRackPage = showDspRackPage;",
    real_rack_gain_handlers + "\n    window.showDspRackPage = showDspRackPage;\n    window.triggerDenoiseLearn = triggerDenoiseLearn;"
)

with open(dsp_file, "w", encoding="utf-8") as f:
    f.write(dsp)
print("Fixed dsp-rack.js: replaced all inline state.* references with window.getChannelRackState(${ch}).*")
