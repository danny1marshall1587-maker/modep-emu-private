import re

def fix_all():
    # 1. Update reaeq-canvas.js
    reaeq_path = '/home/patch/modep-live-console-release/web/js/reaeq-canvas.js'
    with open(reaeq_path, 'r', encoding='utf-8') as f:
        reaeq_code = f.read()

    exports_reaeq = """
    // Export ReaEQ functions to window for cross-module access
    window.evalBandMagnitude = evalBandMagnitude;
    window.getChannelReEq = getChannelReEq;
    window.drawReaEQCurve = drawReaEQCurve;
    window.resizeReaEQCanvas = resizeReaEQCanvas;
    window.renderReEqCanvas = renderReEqCanvas;
    window.initReEqInteractions = initReEqInteractions;
    window.updateReEqCards = updateReEqCards;
    window.selectReEqBand = selectReEqBand;
    window.changeReEqType = changeReEqType;
    window.changeReEqQ = changeReEqQ;
    window.toggleReEqBand = toggleReEqBand;
    window.setReEqRange = setReEqRange;
    window.toggleReaEQFullscreen = toggleReaEQFullscreen;
    window.freqToX = freqToX;
    window.xToFreq = xToFreq;
    window.dbToY = dbToY;
    window.yToDb = yToDb;
    window.REEQ_COLORS = REEQ_COLORS;
    window.FILTER_TYPES = FILTER_TYPES;
})(window);"""

    if 'window.evalBandMagnitude = evalBandMagnitude;' not in reaeq_code:
        reaeq_code = re.sub(r'\}\)\(window\);\s*$', exports_reaeq + '\n', reaeq_code)
        with open(reaeq_path, 'w', encoding='utf-8') as f:
            f.write(reaeq_code)
        print("Exported ReaEQ symbols to window in reaeq-canvas.js")
    else:
        print("ReaEQ exports already present")

    # 2. Update dsp-rack.js
    dsp_path = '/home/patch/modep-live-console-release/web/js/dsp-rack.js'
    with open(dsp_path, 'r', encoding='utf-8') as f:
        dsp_code = f.read()

    dummy_stub = "window.getChannelRackState = window.getChannelRackState || function(ch) {\n  return { denoise: true, eq: true, comp: true, deess: true, tap: 'post-fader' };\n};"
    if dummy_stub in dsp_code:
        dsp_code = dsp_code.replace(dummy_stub, "// Dummy stub removed - using real getChannelRackState")
        print("Removed dummy getChannelRackState stub from dsp-rack.js")

    if 'const evalBandMagnitude = ' not in dsp_code:
        dsp_code = dsp_code.replace(
            "// dsp-rack.js - Channel DSP Rack",
            "const evalBandMagnitude = function(band, freq, srate) { return (window.evalBandMagnitude || function() { return 1.0; })(band, freq, srate); };\n// dsp-rack.js - Channel DSP Rack"
        )
        print("Added evalBandMagnitude fallback delegator in dsp-rack.js")

    dsp_exports = """
    window.getChannelRackState = getChannelRackState;
    window.openChannelRackModal = openChannelRackModal;
    window.dispatchDspParam = dispatchDspParam;
    window.reverbDspState = reverbDspState;
    window.renderDelayControls = renderDelayControls;
    window.renderReverbControls = renderReverbControls;
    window.switchReverbEngine = switchReverbEngine;
    window.initTalRotaries = initTalRotaries;
    window.busterLimiterState = busterLimiterState;
    window.renderMiniReEq = renderMiniReEq;
    window.showDspRackPage = showDspRackPage;
    window.closeDspRackPage = closeDspRackPage;
})(window);"""

    dsp_code = re.sub(r'window\.showDspRackPage = showDspRackPage;\s*window\.closeDspRackPage = closeDspRackPage;\s*\}\)\(window\);', dsp_exports, dsp_code)

    with open(dsp_path, 'w', encoding='utf-8') as f:
        f.write(dsp_code)
    print("Updated dsp-rack.js exports and states")

    # 3. Clean dummy getChannelRackState from room-suite.js
    room_path = '/home/patch/modep-live-console-release/web/js/room-suite.js'
    with open(room_path, 'r', encoding='utf-8') as f:
        room_code = f.read()

    if dummy_stub in room_code:
        room_code = room_code.replace(dummy_stub, "// Dummy stub removed - using window.getChannelRackState")
        with open(room_path, 'w', encoding='utf-8') as f:
            f.write(room_code)
        print("Removed dummy getChannelRackState stub from room-suite.js")

if __name__ == '__main__':
    fix_all()
