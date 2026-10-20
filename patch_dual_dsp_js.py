import sys

js_path = "/home/patch/modep-live-console-release/web/js/dsp-rack.js"
with open(js_path, "r", encoding="utf-8") as f:
    js_content = f.read()

dual_dsp_logic = """
        // =========================================================================
        // DUAL-MODE CHANNEL DSP ENGINE (LINEAR RACK VS. MODULAR CANVAS)
        // =========================================================================

        // Plugin Catalog for Linear 6-Slot Inserts
        window.AVAILABLE_PLUGINS = [
            { id: "denoiser", name: "DE-NOISER (GATE)", cat: "Dynamics", icon: "🛡️" },
            { id: "reaeq", name: "ReaEQ (PARAMETRIC)", cat: "Equalizer", icon: "🎛️" },
            { id: "fetish76", name: "FETISH 76 (PEAK COMP)", cat: "Compressor", icon: "⚡" },
            { id: "lala", name: "LALA (OPTO LEVELER)", cat: "Compressor", icon: "📻" },
            { id: "loades", name: "LOADES (DE-ESSER)", cat: "Dynamics", icon: "🎙️" },
            { id: "saturation", name: "TUBE / TAPE SATURATION", cat: "Coloration", icon: "🔥" },
            { id: "delay", name: "TEMPO TAPE DELAY", cat: "Time-Based", icon: "⏱️" },
            { id: "reverb", name: "CONVOLUTION REVERB", cat: "Space", icon: "🏛️" },
            { id: "thru", name: "MASTER BUS THRU (EMPTY)", cat: "Utility", icon: "➡️" }
        ];

        let activePickerChannel = 0;
        let activePickerSlot = 0;

        // Initialize or retrieve Dual-Mode State from state / localStorage
        window.channelDualDspState = {};

        function loadDualDspState() {
            try {
                const saved = localStorage.getItem("modep_channel_dual_dsp");
                if (saved) {
                    window.channelDualDspState = JSON.parse(saved);
                }
            } catch(e) {}

            for (let ch = 0; ch < 8; ch++) {
                if (!window.channelDualDspState[ch]) {
                    window.channelDualDspState[ch] = {
                        mode: (ch === 6 || ch === 7) ? 'canvas' : 'rack', // Ch 7/8 default to Canvas
                        rackPort: (ch === 6 || ch === 7) ? 8881 : ((ch === 0 || ch === 1) ? 8882 : ((ch === 2 || ch === 3) ? 8883 : 8884)),
                        canvasBypass: false,
                        slots: [
                            { name: "DE-NOISER (GATE)", bypassed: false },
                            { name: "ReaEQ (PARAMETRIC)", bypassed: false },
                            { name: "FETISH 76 (PEAK COMP)", bypassed: false },
                            { name: "LALA (OPTO LEVELER)", bypassed: false },
                            { name: "LOADES (DE-ESSER)", bypassed: false },
                            { name: "MASTER BUS THRU", bypassed: false }
                        ]
                    };
                }
            }
        }

        function saveDualDspState() {
            try {
                localStorage.setItem("modep_channel_dual_dsp", JSON.stringify(window.channelDualDspState));
            } catch(e) {}
        }

        window.setChannelDspMode = function(ch, mode) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            window.channelDualDspState[ch].mode = mode;
            saveDualDspState();

            // Toggle active buttons on mode bar
            const btnRack = document.getElementById("btn-mode-rack-" + ch);
            const btnCanvas = document.getElementById("btn-mode-canvas-" + ch);
            if (btnRack) btnRack.classList.toggle("active", mode === "rack");
            if (btnCanvas) btnCanvas.classList.toggle("active", mode === "canvas");

            // Toggle containers
            const rackCont = document.getElementById("dsp-rack-container-" + ch);
            const canvasCont = document.getElementById("dsp-canvas-container-" + ch);
            if (rackCont) rackCont.style.display = (mode === "rack") ? "block" : "none";
            if (canvasCont) canvasCont.style.display = (mode === "canvas") ? "block" : "none";

            // Notify backend router via WebSocket if connected
            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                window.meterWs.send(JSON.stringify({
                    action: "set_dsp_mode",
                    channel: ch,
                    mode: mode,
                    rack_port: window.channelDualDspState[ch].rackPort
                }));
            }
        };

        window.setChannelCanvasRack = function(ch, port) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            window.channelDualDspState[ch].rackPort = parseInt(port);
            saveDualDspState();
            console.log(`[Dual-Mode DSP] Channel ${ch + 1} assigned to MODEP Rack on port :${port}`);
        };

        window.toggleCanvasHardBypass = function(ch) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            const curr = !window.channelDualDspState[ch].canvasBypass;
            window.channelDualDspState[ch].canvasBypass = curr;
            saveDualDspState();

            const btn = document.getElementById("btn-canvas-bypass-" + ch);
            if (btn) {
                btn.classList.toggle("bypassed", curr);
                btn.innerText = curr ? "CANVAS BYPASS: ACTIVE" : "CANVAS BYPASS: OFF";
            }

            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                window.meterWs.send(JSON.stringify({
                    action: "set_canvas_bypass",
                    channel: ch,
                    bypass: curr
                }));
            }
        };

        window.toggleSlotBypass = function(ch, slotIdx) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            const slot = window.channelDualDspState[ch].slots[slotIdx];
            slot.bypassed = !slot.bypassed;
            saveDualDspState();

            const slotEl = document.getElementById(`slot-${ch}-${slotIdx}`);
            const bypBtn = document.getElementById(`slot-byp-${ch}-${slotIdx}`);
            if (slotEl) slotEl.classList.toggle("bypassed", slot.bypassed);
            if (bypBtn) {
                bypBtn.classList.toggle("active", !slot.bypassed);
                bypBtn.innerText = slot.bypassed ? "BYP" : "ACT";
            }
        };

        window.openPluginPickerModal = function(ch, slotIdx) {
            activePickerChannel = ch;
            activePickerSlot = slotIdx;

            const overlay = document.getElementById("plugin-picker-overlay");
            const title = document.getElementById("plugin-picker-title");
            const list = document.getElementById("plugin-picker-list");

            if (title) {
                title.innerText = `CH ${ch + 1} &bull; Assign Slot ${slotIdx + 1}`;
            }

            if (list) {
                list.innerHTML = window.AVAILABLE_PLUGINS.map(p => `
                    <div class="plugin-picker-item" onclick="assignSlotPlugin('${p.name}')">
                        <span class="item-title">${p.icon} ${p.name}</span>
                        <span class="item-cat">${p.cat}</span>
                    </div>
                `).join('');
            }

            if (overlay) overlay.style.display = "flex";
        };

        window.closePluginPickerModal = function() {
            const overlay = document.getElementById("plugin-picker-overlay");
            if (overlay) overlay.style.display = "none";
        };

        window.assignSlotPlugin = function(pluginName) {
            const ch = activePickerChannel;
            const slotIdx = activePickerSlot;
            if (!window.channelDualDspState[ch]) loadDualDspState();
            window.channelDualDspState[ch].slots[slotIdx].name = pluginName;
            window.channelDualDspState[ch].slots[slotIdx].bypassed = false;
            saveDualDspState();

            const nameEl = document.getElementById(`slot-name-${ch}-${slotIdx}`);
            if (nameEl) nameEl.innerText = pluginName;

            const slotEl = document.getElementById(`slot-${ch}-${slotIdx}`);
            const bypBtn = document.getElementById(`slot-byp-${ch}-${slotIdx}`);
            if (slotEl) slotEl.classList.remove("bypassed");
            if (bypBtn) {
                bypBtn.classList.add("active");
                bypBtn.innerText = "ACT";
            }

            window.closePluginPickerModal();
        };

        window.openModepCanvasModal = function(ch) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            const port = window.channelDualDspState[ch].rackPort || 8881;
            const host = window.location.hostname || "192.168.1.148";
            const url = `http://${host}:${port}/`;

            const overlay = document.getElementById("canvas-modal-overlay");
            const title = document.getElementById("canvas-modal-title");
            const iframe = document.getElementById("canvas-modal-iframe");
            const newTabLink = document.getElementById("canvas-modal-newtab");

            if (title) title.innerText = `🎛️ Channel ${ch + 1} &bull; MODEP Canvas (Port :${port})`;
            if (newTabLink) newTabLink.href = url;
            if (iframe) iframe.src = url;
            if (overlay) overlay.style.display = "flex";
        };

        window.closeCanvasModal = function() {
            const overlay = document.getElementById("canvas-modal-overlay");
            const iframe = document.getElementById("canvas-modal-iframe");
            if (iframe) iframe.src = "about:blank";
            if (overlay) overlay.style.display = "none";
        };

        // Initialize state on DOM load
        window.initDualDspModes = function() {
            loadDualDspState();
            for (let ch = 0; ch < 8; ch++) {
                const state = window.channelDualDspState[ch];
                window.setChannelDspMode(ch, state.mode);

                // Update slot texts and bypass
                if (state.slots) {
                    state.slots.forEach((s, idx) => {
                        const nameEl = document.getElementById(`slot-name-${ch}-${idx}`);
                        if (nameEl) nameEl.innerText = s.name;
                        const slotEl = document.getElementById(`slot-${ch}-${idx}`);
                        const bypBtn = document.getElementById(`slot-byp-${ch}-${idx}`);
                        if (slotEl) slotEl.classList.toggle("bypassed", s.bypassed);
                        if (bypBtn) {
                            bypBtn.classList.toggle("active", !s.bypassed);
                            bypBtn.innerText = s.bypassed ? "BYP" : "ACT";
                        }
                    });
                }

                // Update canvas rack select
                const sel = document.getElementById(`canvas-rack-select-${ch}`);
                if (sel) sel.value = String(state.rackPort);

                // Update canvas bypass button
                const byp = document.getElementById(`btn-canvas-bypass-${ch}`);
                if (byp) {
                    byp.classList.toggle("bypassed", state.canvasBypass);
                    byp.innerText = state.canvasBypass ? "CANVAS BYPASS: ACTIVE" : "CANVAS BYPASS: OFF";
                }
            }
        };
"""

target_end = "    window.showDspRackPage = showDspRackPage;\n    window.closeDspRackPage = closeDspRackPage;\n})(window);"
replacement_end = dual_dsp_logic + "\n" + target_end

if "window.setChannelDspMode" not in js_content:
    if target_end in js_content:
        js_content = js_content.replace(target_end, replacement_end)
        with open(js_path, "w", encoding="utf-8") as f:
            f.write(js_content)
        print("Updated dsp-rack.js with Dual-Mode DSP logic.")
    else:
        print("Target end closure not found in dsp-rack.js.")
else:
    print("dsp-rack.js already contains Dual-Mode DSP logic.")
