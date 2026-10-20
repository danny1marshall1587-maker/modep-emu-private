import re

# 1. CLEAN DSP-RACK.JS:
#    - Remove remaining console.log stubs at top (lines 1-130) that shadow or conflict
#    - Export ALL modal and interactive functions to window so zero buttons fail
dsp_path = "/home/patch/modep-live-console-release/web/js/dsp-rack.js"
with open(dsp_path, "r", encoding="utf-8") as f:
    dsp = f.read()

# Remove dummy stubs in the top glue block
stubs_to_remove = [
    r'window\.togglePhaseInvert = window\.togglePhaseInvert \|\| function\(ch\) \{[\s\S]*?\};',
    r'window\.setChannelDirectTap = window\.setChannelDirectTap \|\| function\(ch, tap\) \{[\s\S]*?\};',
    r'window\.startAoVuLoop = window\.startAoVuLoop \|\| function\(\) \{[\s\S]*?\};',
    r'window\.handleRackGainChange = window\.handleRackGainChange \|\| function\(ch, val\) \{[\s\S]*?\};',
    r'window\.triggerDenoiseLearn = window\.triggerDenoiseLearn \|\| function\(ch\) \{[\s\S]*?\};',
    r'window\.selectRackEqBand = window\.selectRackEqBand \|\| function\(ch, idx\) \{[\s\S]*?\};',
    r'window\.toggleReEqBand = window\.toggleReEqBand \|\| function\(idx\) \{[\s\S]*?\};',
    r'window\.renderMiniReEq = window\.renderMiniReEq \|\| function\(ch\) \{[\s\S]*?\};'
]
for pat in stubs_to_remove:
    dsp = re.sub(pat, '// stub cleaned', dsp)

# Export every single function and state object needed by modals/buttons at the bottom of dsp-rack.js
dsp_exports = """
    // Complete Global Window Exports for DSP Rack, Delay, Reverb, Compressors & Summing
    window.getChannelRackState = getChannelRackState;
    window.openChannelRackModal = openChannelRackModal;
    window.closeChannelRackModal = closeChannelRackModal;
    window.dispatchDspParam = dispatchDspParam;
    window.triggerDenoiseLearn = triggerDenoiseLearn;
    window.setChannelCompModel = setChannelCompModel;
    window.togglePhaseInvert = togglePhaseInvert;
    window.setChannelDirectTap = setChannelDirectTap;
    window.selectRackEqBand = selectRackEqBand;
    window.renderMiniReEq = renderMiniReEq;
    window.updateFocusedChannelStrip = updateFocusedChannelStrip;
    window.startFocusedFaderDrag = startFocusedFaderDrag;

    // Delay & Reverb Suite Exports
    window.delayDspState = delayDspState;
    window.reverbDspState = reverbDspState;
    window.renderDelayControls = renderDelayControls;
    window.updateDelayTimeMs = updateDelayTimeMs;
    window.updateDelaySubdivision = updateDelaySubdivision;
    window.toggleDelaySync = toggleDelaySync;
    window.cycleTalDelayPreset = cycleTalDelayPreset;
    window.toggleTalDelaySidechain = toggleTalDelaySidechain;
    window.initTalDelayRotaries = initTalDelayRotaries;
    window.updateTalDelayFadersVisual = updateTalDelayFadersVisual;
    window.renderReverbControls = renderReverbControls;
    window.switchReverbEngine = switchReverbEngine;
    window.cycleTalPreset = cycleTalPreset;
    window.toggleTalSidechain = toggleTalSidechain;
    window.startTalFaderDrag = startTalFaderDrag;
    window.updateTalFadersVisual = updateTalFadersVisual;
    window.initTalRotaries = initTalRotaries;
    window.initIrCanvas = initIrCanvas;
    window.drawIrWaveform = drawIrWaveform;

    // Analog Obsession Compressors, Limiter & Summing Exports
    window.fetish76State = fetish76State;
    window.lalaState = lalaState;
    window.busterLimiterState = busterLimiterState;
    window.analogSummingState = analogSummingState;
    window.renderAoVuMeter = renderAoVuMeter;
    window.startAoVuLoop = startAoVuLoop;
    window.drawAoKnob = drawAoKnob;
    window.bindAoRotary = bindAoRotary;
    window.setFetishRatio = setFetishRatio;
    window.toggleFetishAllButtons = toggleFetishAllButtons;
    window.setSummingConsole = setSummingConsole;
    window.updateSummingHeaderBtn = updateSummingHeaderBtn;

    window.showDspRackPage = showDspRackPage;
    window.closeDspRackPage = closeDspRackPage;
})(window);"""

dsp = re.sub(r'window\.getChannelRackState = getChannelRackState;[\s\S]*?\}\)\(window\);', dsp_exports, dsp)

with open(dsp_path, "w", encoding="utf-8") as f:
    f.write(dsp)
print("Updated dsp-rack.js with all global exports.")

# 2. CLEAN ROOM-SUITE.JS:
#    - Remove the top fake bindAoRotary/resizeReaEQCanvas block (lines 1-257) that was shadowing the real Bakelite knobs and Cytomic ReaEQ!
#    - Export AFS Feedback Killer & Room Calibrator functions to window:
#      toggleLockFixedNotch, panicClearAfdFilters, toggleAfdAutoHunting, startRoomCal, commitRoomCalToBus, roomCalState
room_path = "/home/patch/modep-live-console-release/web/js/room-suite.js"
with open(room_path, "r", encoding="utf-8") as f:
    room = f.read()

# Strip the fake header before `// room-suite.js - 12-notch Auto-Feedback Suppressor`
marker = "// room-suite.js - 12-notch Auto-Feedback Suppressor"
m_idx = room.find(marker)
if m_idx != -1:
    room = room[m_idx:]
    print("Stripped fake knob/EQ override header from room-suite.js so real Bakelite knobs & Cytomic SVF ReaEQ run.")

room_exports = """
    // Export AFS Feedback Killer & Auto-EQ Room Calibrator functions to window
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

    window.openDspModal = openDspModal;
    window.closeDspModal = closeDspModal;
    window.closeModalOnOverlay = closeModalOnOverlay;
})(window);"""

room = re.sub(r'window\.openDspModal = openDspModal;[\s\S]*?\}\)\(window\);', room_exports, room)

with open(room_path, "w", encoding="utf-8") as f:
    f.write(room)
print("Updated room-suite.js with all AFS and Room Calibrator window exports.")

# 3. ENSURE initDualDspModes() RUNS ON STARTUP IN APP.JS
app_path = "/home/patch/modep-live-console-release/web/js/app.js"
with open(app_path, "r", encoding="utf-8") as f:
    app = f.read()

if "window.initDualDspModes" not in app:
    app = app.replace(
        'if (typeof window.switchMixerView === \'function\') {',
        'if (typeof window.initDualDspModes === \'function\') { window.initDualDspModes(); }\n            if (typeof window.switchMixerView === \'function\') {'
    )
    with open(app_path, "w", encoding="utf-8") as f:
        f.write(app)
    print("Added initDualDspModes() call to app.js startup.")
