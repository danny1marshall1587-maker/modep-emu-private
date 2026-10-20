// app.js - Application bootstrap and DOMContentLoaded orchestration
(function(window, document) {
    'use strict';

    window.addEventListener('DOMContentLoaded', () => {
        // 1. Initialize faders across all views to 75% (0.0 dB Unity gain)
        try {
            if (typeof window.setFaderPosition === 'function') {
                // Inputs
                for (let i = 0; i < 8; i++) {
                    window.setFaderPosition(i, 75, true);
                }
                // Master
                window.setFaderPosition('master', 75, true);

                // FX Sends 1-4
                for (let s = 1; s <= 4; s++) {
                    window.setFaderPosition('send-' + s, 75, true);
                }
                // FX Returns 1-4
                for (let r = 1; r <= 4; r++) {
                    window.setFaderPosition('ret-' + r, 75, true);
                }
                // Monitor Outputs 1-6
                for (let m = 1; m <= 6; m++) {
                    window.setFaderPosition('mon-' + m, 75, true);
                }
            }
        } catch (e) {
            console.error("Error initializing faders:", e);
        }

        // 2. Restore user-saved channel customizations (names & colors)
        try {
            if (typeof window.loadChannelCustomizations === 'function') {
                window.loadChannelCustomizations();
            }
        } catch (e) {
            console.error("Error loading channel customizations:", e);
        }

        // 3. Attach long-press bindings for rename and color tagging
        try {
            if (typeof window.bindLongPress === 'function') {
                // Channels 1-8
                for (let i = 0; i < 8; i++) {
                    const nameEl = document.getElementById(`ch-name-${i}`);
                    if (nameEl) window.bindLongPress(nameEl, () => window.openRenameModal(`ch-${i}`, `CH ${i+1}`));
                    const badgeEl = document.getElementById(`ch-badge-${i}`);
                    if (badgeEl) window.bindLongPress(badgeEl, (ev) => window.openColorPalette(`ch-${i}`, ev));
                }
                // Sends 1-4
                for (let s = 1; s <= 4; s++) {
                    const nameEl = document.getElementById(`send-name-${s}`);
                    if (nameEl) window.bindLongPress(nameEl, () => window.openRenameModal(`send-${s}`, `SEND ${s}`));
                    const badgeEl = document.getElementById(`send-badge-${s}`);
                    if (badgeEl) window.bindLongPress(badgeEl, (ev) => window.openColorPalette(`send-${s}`, ev));
                }
                // Returns 1-4
                for (let r = 1; r <= 4; r++) {
                    const nameEl = document.getElementById(`ret-name-${r}`);
                    if (nameEl) window.bindLongPress(nameEl, () => window.openRenameModal(`ret-${r}`, `RET ${r}`));
                    const badgeEl = document.getElementById(`ret-badge-${r}`);
                    if (badgeEl) window.bindLongPress(badgeEl, (ev) => window.openColorPalette(`ret-${r}`, ev));
                }
                // Monitors 1-6
                for (let m = 1; m <= 6; m++) {
                    const nameEl = document.getElementById(`mon-name-${m}`);
                    if (nameEl) window.bindLongPress(nameEl, () => window.openRenameModal(`mon-${m}`, `MONITOR ${m}`));
                    const badgeEl = document.getElementById(`mon-badge-${m}`);
                    if (badgeEl) window.bindLongPress(badgeEl, (ev) => window.openColorPalette(`mon-${m}`, ev));
                }
            }
        } catch (e) {
            console.error("Error binding long-press listeners:", e);
        }

        // 4. Connect to Real-Time JACK WebSocket Telemetry
        try {
            if (typeof window.connectMeterWs === 'function') {
                window.connectMeterWs();
            }
        } catch (e) {
            console.error("Error establishing telemetry WebSocket:", e);
        }

        // 5. Apply universal slow-mode touch governors to static sliders
        try {
            if (typeof window.applySlowModeToAllSliders === 'function') {
                window.applySlowModeToAllSliders();
            }
        } catch (e) {
            console.error("Error applying slow mode to sliders:", e);
        }

        // 6. Initialize default mix view
        try {
            if (typeof window.initDualDspModes === 'function') { window.initDualDspModes(); }
            if (typeof window.switchMixerView === 'function') {
                window.switchMixerView("inputs");
            }
        } catch (e) {
            console.error("Error switching to default mixer view:", e);
        }
    });

    // Window Resize Listener for Responsive Canvas Rendering
    window.addEventListener('resize', () => {
        try {
            const reaeqCanvas = document.getElementById("reeq-canvas");
            if (reaeqCanvas && reaeqCanvas.offsetParent !== null && typeof window.resizeReaEQCanvas === 'function') {
                window.resizeReaEQCanvas(reaeqCanvas, reaeqCanvas.parentElement);
            }
            const rackPage = document.getElementById("dsp-rack-page");
            if (rackPage && rackPage.classList.contains("open") && typeof window.renderMiniReEq === 'function') {
                window.renderMiniReEq(window.dspRackCurrentChannel);
            }
        } catch (e) {
            console.error("Error handling resize:", e);
        }
    });

})(window, document);
