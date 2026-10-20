import re

# 1. FIX INDEX.HTML / MIXER_INDEX.HTML
# - Remove the conflicting inline dummy script at the head
# - Replace the 6 fixed slots in each channel with 4 empty, assignable MODEP effect slot buttons
#   that automatically pass audio through and only bring in the assigned insert effect.

channel_slot_template = """                    <!-- MODE SELECTOR BAR -->
                    <div class="dsp-mode-bar" id="dsp-mode-bar-{ch}">
                      <button class="btn-dsp-mode active" id="btn-mode-rack-{ch}" onclick="setChannelDspMode({ch}, 'rack')">RACK SLOTS</button>
                      <button class="btn-dsp-mode" id="btn-mode-canvas-{ch}" onclick="setChannelDspMode({ch}, 'canvas')">CANVAS</button>
                    </div>

                    <!-- CONTAINER A: 4 MODEP INSERT SLOTS (Linear Through, Non-Cutting) -->
                    <div class="dsp-container linear-slot-container" id="dsp-rack-container-{ch}">
                      <div class="insert-slots-grid">
                        <div class="insert-slot empty" id="slot-{ch}-0" onclick="openPluginPickerModal({ch}, 0)">
                          <span class="slot-num">1</span>
                          <span class="slot-name" id="slot-name-{ch}-0">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-0" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 0)">BYP</button>
                        </div>
                        <div class="insert-slot empty" id="slot-{ch}-1" onclick="openPluginPickerModal({ch}, 1)">
                          <span class="slot-num">2</span>
                          <span class="slot-name" id="slot-name-{ch}-1">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-1" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 1)">BYP</button>
                        </div>
                        <div class="insert-slot empty" id="slot-{ch}-2" onclick="openPluginPickerModal({ch}, 2)">
                          <span class="slot-num">3</span>
                          <span class="slot-name" id="slot-name-{ch}-2">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-2" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 2)">BYP</button>
                        </div>
                        <div class="insert-slot empty" id="slot-{ch}-3" onclick="openPluginPickerModal({ch}, 3)">
                          <span class="slot-num">4</span>
                          <span class="slot-name" id="slot-name-{ch}-3">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-3" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 3)">BYP</button>
                        </div>
                      </div>
                      <div class="btn-fx-quick-open" onclick="showDspRackPage({ch})" title="Open Detailed Channel Core Desk Strip (Gate, ReaEQ, Comp, De-Esser)">
                        <div class="fx-quick-title">
                          <span>🎛️ CORE DESK DSP</span>
                        </div>
                      </div>
                    </div>"""

def patch_html_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        html = f.read()

    # Remove conflicting head dummy scripts that clobbered real functions
    html = re.sub(r'<script id="global-dsp-handlers">.*?</script>', '', html, flags=re.DOTALL)

    # Replace each channel's DSP section (0 to 7)
    for ch in range(8):
        pattern = rf'<!-- MODE SELECTOR BAR -->\s*<div class="dsp-mode-bar" id="dsp-mode-bar-{ch}">.*?<!-- CONTAINER B: MODULAR CANVAS ROUTER'
        replacement = channel_slot_template.format(ch=ch) + "\n\n                    <!-- CONTAINER B: MODULAR CANVAS ROUTER"
        html = re.sub(pattern, replacement, html, flags=re.DOTALL)

    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(html)
    print(f"Patched {file_path}")

patch_html_file('/home/patch/modep-live-console-release/web/mixer_index.html')
patch_html_file('/home/patch/modep-live-console-release/web/index.html')

# 2. UPDATE DSP-RACK.JS:
# - Clean conflicting stub definitions from header
# - Make sure sliders reference window.getChannelRackState(ch) safely so 'fetish76' is never undefined
# - Update channelDualDspState to default to 4 empty passthrough slots
# - Export setChannelCompModel, renderAoVuMeter, bindAoRotary, drawAoKnob globally

dsp_path = '/home/patch/modep-live-console-release/web/js/dsp-rack.js'
with open(dsp_path, 'r', encoding='utf-8') as f:
    dsp = f.read()

# Replace header stubs that clobber real models
dsp = dsp.replace(
    "window.setChannelCompModel = window.setChannelCompModel || function(ch, model) {\n  console.log(\"[Channel] Comp model changed:\", ch, model);\n};",
    "// setChannelCompModel stub removed"
)

# Fix slider string templates in showDspRackPage so they never evaluate undefined
old_slider_input = 'value="${state.compParams.fetish76.input}" oninput="state.compParams.fetish76.input=parseInt(this.value);'
new_slider_input = 'value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.input:30}" oninput="getChannelRackState(' + '${ch}' + ').compParams.fetish76.input=parseInt(this.value);'
dsp = dsp.replace(old_slider_input, new_slider_input)

old_slider_output = 'value="${state.compParams.fetish76.output}" oninput="state.compParams.fetish76.output=parseInt(this.value);'
new_slider_output = 'value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.output:18}" oninput="getChannelRackState(' + '${ch}' + ').compParams.fetish76.output=parseInt(this.value);'
dsp = dsp.replace(old_slider_output, new_slider_output)

old_slider_attack = 'value="${state.compParams.fetish76.attack}" oninput="state.compParams.fetish76.attack=parseInt(this.value);'
new_slider_attack = 'value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.attack:3}" oninput="getChannelRackState(' + '${ch}' + ').compParams.fetish76.attack=parseInt(this.value);'
dsp = dsp.replace(old_slider_attack, new_slider_attack)

old_slider_release = 'value="${state.compParams.fetish76.release}" oninput="state.compParams.fetish76.release=parseInt(this.value);'
new_slider_release = 'value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.release:3}" oninput="getChannelRackState(' + '${ch}' + ').compParams.fetish76.release=parseInt(this.value);'
dsp = dsp.replace(old_slider_release, new_slider_release)

# Fix LALA sliders as well
old_lala_peak = 'value="${state.compParams.lala.peakReduction}" oninput="state.compParams.lala.peakReduction=parseInt(this.value);'
new_lala_peak = 'value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.peakReduction:40}" oninput="getChannelRackState(' + '${ch}' + ').compParams.lala.peakReduction=parseInt(this.value);'
dsp = dsp.replace(old_lala_peak, new_lala_peak)

old_lala_gain = 'value="${state.compParams.lala.gain}" oninput="state.compParams.lala.gain=parseInt(this.value);'
new_lala_gain = 'value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.gain:40}" oninput="getChannelRackState(' + '${ch}' + ').compParams.lala.gain=parseInt(this.value);'
dsp = dsp.replace(old_lala_gain, new_lala_gain)

# Set 4 default empty passthrough slots in loadDualDspState
four_slots_def = """                        slots: [
                            { name: "EMPTY (THRU)", bypassed: true },
                            { name: "EMPTY (THRU)", bypassed: true },
                            { name: "EMPTY (THRU)", bypassed: true },
                            { name: "EMPTY (THRU)", bypassed: true }
                        ]"""
dsp = re.sub(r'slots:\s*\[[\s\S]*?\{ name: "MASTER BUS THRU"[\s\S]*?\]', four_slots_def, dsp)

# Ensure global exports at end of dsp-rack.js
full_exports = """
    window.getChannelRackState = getChannelRackState;
    window.openChannelRackModal = openChannelRackModal;
    window.dispatchDspParam = dispatchDspParam;
    window.setChannelCompModel = setChannelCompModel;
    window.togglePhaseInvert = togglePhaseInvert;
    window.setChannelDirectTap = setChannelDirectTap;
    window.reverbDspState = reverbDspState;
    window.renderDelayControls = renderDelayControls;
    window.renderReverbControls = renderReverbControls;
    window.switchReverbEngine = switchReverbEngine;
    window.initTalRotaries = initTalRotaries;
    window.busterLimiterState = busterLimiterState;
    window.renderMiniReEq = renderMiniReEq;
    window.renderAoVuMeter = renderAoVuMeter;
    window.startAoVuLoop = startAoVuLoop;
    window.drawAoKnob = drawAoKnob;
    window.bindAoRotary = bindAoRotary;
    window.setFetishRatio = setFetishRatio;
    window.showDspRackPage = showDspRackPage;
    window.closeDspRackPage = closeDspRackPage;
})(window);"""

dsp = re.sub(r'window\.showDspRackPage = showDspRackPage;[\s\S]*?\}\)\(window\);', full_exports, dsp)

with open(dsp_path, 'w', encoding='utf-8') as f:
    f.write(dsp)
print("Updated dsp-rack.js")

# 3. UPDATE TELEMETRY.JS:
# - Connect the FET 76 VU meter (fet-vu-meter) and LALA VU meter (lala-vu-meter) to live channel GR telemetry
#   so the meters draw immediately upon opening the compressor modals!
telem_path = '/home/patch/modep-live-console-release/web/js/telemetry.js'
with open(telem_path, 'r', encoding='utf-8') as f:
    telem = f.read()

meter_hook = """
                        // 3. Compressor Modal Live VU Needle Updates (FET 76, LALA, Buster)
                        if (typeof window.renderAoVuMeter === 'function') {
                            const curCh = (typeof window.currentModalChannel === 'number') ? window.currentModalChannel : (window.dspRackCurrentChannel || 0);
                            const chGr = -(window.channelGrValues[curCh] || 0.0);
                            const fetEl = document.getElementById('fet-vu-meter');
                            if (fetEl && fetEl.offsetParent !== null) {
                                window.renderAoVuMeter('fet-vu-meter', chGr, 'fetish');
                            }
                            const lalaEl = document.getElementById('lala-vu-meter');
                            if (lalaEl && lalaEl.offsetParent !== null) {
                                window.renderAoVuMeter('lala-vu-meter', chGr, 'lala');
                            }
                        }
"""
if "window.renderAoVuMeter('fet-vu-meter'" not in telem:
    telem = telem.replace("// 3. Master Limiter Buster VU needle", meter_hook + "\n                        // 3. Master Limiter Buster VU needle")
    with open(telem_path, 'w', encoding='utf-8') as f:
        f.write(telem)
    print("Updated telemetry.js with live compressor meters")

# 4. UPDATE ROOM-SUITE.JS:
# - Set window.currentModalChannel when opening compressor modals
# - Remove broken stub overrides
room_path = '/home/patch/modep-live-console-release/web/js/room-suite.js'
with open(room_path, 'r', encoding='utf-8') as f:
    room = f.read()

# Remove stub overrides
room = room.replace("window.setChannelCompModel = window.setChannelCompModel || function(ch, model) {", "// stub removed: setChannelCompModel")
room = room.replace("window.startAoVuLoop = window.startAoVuLoop || function() {", "// stub removed: startAoVuLoop")

# Track modal channel in openDspModal
room = room.replace(
    "window.openDspModal = function(type, target) {",
    "window.openDspModal = function(type, target) {\n            window.currentModalChannel = (typeof target === 'number') ? target : (window.dspRackCurrentChannel || 0);"
)

with open(room_path, 'w', encoding='utf-8') as f:
    f.write(room)
print("Updated room-suite.js")
