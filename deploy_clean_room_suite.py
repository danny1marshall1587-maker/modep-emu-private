with open(r'c:\Users\danny\Documents\modep emu\web\js\room-suite.js', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Keep from line 298 (0-indexed 297: // room-suite.js - 12-notch Auto-Feedback Suppressor...)
body = ''.join(lines[297:])

# Fix analogSummingState defaults so .drive.toFixed(1) and .warmth.toFixed(1) never fail
body = body.replace(
    "var analogSummingState = window.analogSummingState || { enabled: false, model: 'neve', drive: 0 };",
    "var analogSummingState = window.analogSummingState || { console: 'neve', drive: 2.5, warmth: 3.0, crosstalk: -60, ironSat: true, enabled: true };\n    window.analogSummingState = analogSummingState;"
)

# Fix bare meterWs in startRoomCal
body = body.replace(
    "if (meterWs && meterWs.readyState === WebSocket.OPEN) {\n                meterWs.send(",
    "if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {\n                window.meterWs.send("
)

# Fix openDspModal declaration and sync currentModalChannel + setCurrentReEqTarget
body = body.replace(
    "window.openDspModal = function(type, target) {\n    // Bound to global window",
    "function openDspModal(type, target) {\n            window.currentModalChannel = (typeof target === 'number') ? target : (window.dspRackCurrentChannel || 0);"
)

body = body.replace(
    "currentReEqTarget = target;",
    "if (typeof window.setCurrentReEqTarget === 'function') window.setCurrentReEqTarget(target);"
)

# Fix inline dState references in deesser modal
body = body.replace(
    "oninput=\"dState.threshold=parseInt(this.value);",
    "oninput=\"window.getChannelRackState(${chIdx}).deesser.threshold=parseInt(this.value);"
)
body = body.replace(
    "oninput=\"dState.release=parseInt(this.value);",
    "oninput=\"window.getChannelRackState(${chIdx}).deesser.release=parseInt(this.value);"
)
body = body.replace(
    "onclick=\"dState.mode=dState.mode==='band'?'wide':'band'; dispatchDspParam(${chIdx},'loadesMode',dState.mode); this.innerText=dState.mode.toUpperCase(); this.classList.toggle('active',dState.mode==='wide');\"",
    "onclick=\"var ds=window.getChannelRackState(${chIdx}).deesser; ds.mode=ds.mode==='band'?'wide':'band'; dispatchDspParam(${chIdx},'loadesMode',ds.mode); this.innerText=ds.mode.toUpperCase(); this.classList.toggle('active',ds.mode==='wide');\""
)

# Fix closeDspModal so closing a modal opened from the main mixer doesn't force-open dsp-rack-page
old_close_tail = """            // Always return to Level 1 DSP Rack page for the active channel
            const rackPage = document.getElementById("dsp-rack-page");
            if (rackPage && rackPage.classList.contains("open")) {
                // Level 1 is already open — just refresh the mini EQ canvas
                renderMiniReEq(dspRackCurrentChannel);
            } else {
                // Level 1 was not open (came from channel-rack-overlay) — reopen it
                showDspRackPage(dspRackCurrentChannel);
            }"""

new_close_tail = """            const rackPage = document.getElementById("dsp-rack-page");
            if (rackPage && rackPage.classList.contains("open")) {
                renderMiniReEq(window.dspRackCurrentChannel || 0);
            }"""

body = body.replace(old_close_tail, new_close_tail)

# Export all functions cleanly at bottom of IIFE
old_bottom = """    window.openDspModal = openDspModal;
    window.closeDspModal = closeDspModal;
    window.closeModalOnOverlay = closeModalOnOverlay;
})(window);"""

new_bottom = """    window.afdState = afdState;
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

body = body.replace(old_bottom, new_bottom)

with open(r'c:\Users\danny\Documents\modep emu\clean_room_suite.js', 'w', encoding='utf-8') as f:
    f.write(body)

print("Wrote clean_room_suite.js:", len(body), "bytes")
