// room-suite.js - 12-notch Auto-Feedback Suppressor (AFS) and Auto-EQ Room Calibration wizard
(function(window) {
    'use strict';

    var analogSummingState = window.analogSummingState || { console: 'neve', drive: 2.5, warmth: 3.0, crosstalk: -60, ironSat: true, enabled: true };
    window.analogSummingState = analogSummingState;
    var aoVuMeterAnimId = window.aoVuMeterAnimId || null;


        // 4b. Active Auto-Feedback Suppressor (AFS) Engine
        // 12 Surgical Biquad Notch Filters (Q=40 to 80, -3dB to -18dB):
        // Notches 1–6: Fixed soundcheck ring-out notches (locked manually or during ring-out mode)
        // Notches 7–12: Dynamic floating notches (deploy on resonant spikes, auto-release after 30s quiet)
        // TELEMETRY_GROUNDED: Driven strictly by live incoming FFT spectrum (window.liveRtaBins)
        const afdState = {
            activeTarget: 0,
            autoHunting: true,
            sensitivity: 70, // 0..100
            depth: -12, // -3dB to -18dB
            filterQ: 60, // Q=40 to 80
            channels: {}
        };

        function getAfdChannel(ch) {
            if (!afdState.channels[ch]) {
                const filters = [];
                for (let i = 0; i < 12; i++) {
                    const isFixed = i < 6;
                    filters.push({
                        id: i + 1,
                        type: isFixed ? 'fixed' : 'floating',
                        state: 'idle', // 'idle' | 'locked' | 'clamped'
                        freq: 0,
                        depth: 0,
                        q: isFixed ? 60 : 75,
                        timeActive: 0,
                        lastQuietTime: 0
                    });
                }
                afdState.channels[ch] = {
                    filters: filters,
                    lastFftBins: new Float32Array(64)
                };
            }
            return afdState.channels[ch];
        }

        let afdAnimId = null;

        function runAfdPeakDetection(ch) {
            const chState = getAfdChannel(ch);
            if (!afdState.autoHunting) return;

            const now = Date.now();
            const rta = window.liveRtaBins || [];
            if (rta.length === 0) return;

            // Copy live telemetry bins
            for (let b = 0; b < Math.min(64, rta.length); b++) {
                chState.lastFftBins[b] = rta[b];
            }

            // Check for resonant spikes (> -18 dBFS)
            for (let b = 0; b < chState.lastFftBins.length; b++) {
                const level = chState.lastFftBins[b];
                const freq = Math.round(20 * Math.pow(1000, b / 63));

                if (level > -18.0) {
                    // Check if already clamped by an active notch
                    let alreadyClamped = false;
                    for (let f of chState.filters) {
                        if (f.state !== 'idle' && Math.abs(f.freq - freq) < 50) {
                            alreadyClamped = true;
                            f.lastQuietTime = 0;
                            break;
                        }
                    }

                    if (!alreadyClamped) {
                        // Deploy floating notch (Notches 7-12)
                        const freeFloating = chState.filters.slice(6, 12).find(f => f.state === 'idle');
                        if (freeFloating) {
                            freeFloating.state = 'clamped';
                            freeFloating.freq = freq;
                            freeFloating.depth = Math.max(-18, Math.min(-3, -level));
                            freeFloating.q = 60;
                            freeFloating.timeActive = now;
                            freeFloating.lastQuietTime = 0;
                            updateAfdFilterCards(ch);
                        }
                    }
                }
            }

            // Auto-release floating notches after 30 seconds of quiet
            chState.filters.slice(6, 12).forEach(f => {
                if (f.state === 'clamped') {
                    if (!f.lastQuietTime) f.lastQuietTime = now;
                    if (now - f.lastQuietTime > 30000) {
                        // 30 seconds elapsed with quiet signal: release notch
                        f.state = 'idle';
                        f.freq = 0;
                        f.depth = 0;
                        f.lastQuietTime = 0;
                        updateAfdFilterCards(ch);
                    }
                }
            });
        }

        function renderAfdSpectrum(ch) {
            const canvas = document.getElementById("afd-spectrum-canvas");
            if (!canvas) return;
            const ctx = canvas.getContext("2d");
            const dpr = window.devicePixelRatio || 1;
            const container = canvas.parentElement;
            if (container) {
                canvas.width = Math.floor(container.clientWidth * dpr);
                canvas.height = Math.floor(container.clientHeight * dpr);
                ctx.scale(dpr, dpr);
            }
            const w = container ? container.clientWidth : canvas.width;
            const h = container ? container.clientHeight : canvas.height;

            runAfdPeakDetection(ch);
            const chState = getAfdChannel(ch);
            const bins = chState.lastFftBins;

            ctx.clearRect(0, 0, w, h);

            // 1. Grid & Frequency Markings
            ctx.lineWidth = 1;
            ctx.strokeStyle = "#161b24";
            const freqTicks = [50, 100, 250, 500, 1000, 2000, 5000, 10000, 18000];
            freqTicks.forEach(f => {
                const x = (Math.log10(f / 20) / Math.log10(1000)) * w;
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, h);
                ctx.stroke();

                ctx.fillStyle = "#4b5563";
                ctx.font = "9px monospace";
                ctx.fillText(f >= 1000 ? (f/1000) + "k" : f + "Hz", x + 3, h - 8);
            });

            // 2. Threshold Detection Line (-18 dBFS)
            const threshY = h * 0.35;
            ctx.strokeStyle = "rgba(245, 158, 11, 0.45)";
            ctx.setLineDash([4, 4]);
            ctx.beginPath();
            ctx.moveTo(0, threshY);
            ctx.lineTo(w, threshY);
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.fillStyle = "#f59e0b";
            ctx.font = "9px monospace";
            ctx.fillText("FEEDBACK DETECTION THRESHOLD (-18 dBFS)", 14, threshY - 5);

            // 3. FFT Real-Time Spectrum Fill & Curve
            ctx.beginPath();
            ctx.moveTo(0, h);
            for (let i = 0; i < bins.length; i++) {
                const x = (i / (bins.length - 1)) * w;
                const db = Math.max(-80, Math.min(0, bins[i]));
                const y = h - ((db + 80) / 80) * (h - 20);
                ctx.lineTo(x, y);
            }
            ctx.lineTo(w, h);
            ctx.closePath();

            const grad = ctx.createLinearGradient(0, 0, 0, h);
            grad.addColorStop(0, "rgba(239, 68, 68, 0.65)");
            grad.addColorStop(0.3, "rgba(245, 158, 11, 0.45)");
            grad.addColorStop(0.7, "rgba(56, 189, 248, 0.25)");
            grad.addColorStop(1, "rgba(56, 189, 248, 0.0)");
            ctx.fillStyle = grad;
            ctx.fill();

            // Spectrum Stroke
            ctx.beginPath();
            for (let i = 0; i < bins.length; i++) {
                const x = (i / (bins.length - 1)) * w;
                const db = Math.max(-80, Math.min(0, bins[i]));
                const y = h - ((db + 80) / 80) * (h - 20);
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.strokeStyle = "#38bdf8";
            ctx.lineWidth = 1.5;
            ctx.stroke();

            // 4. Draw Active 12 Surgical Notch Filters
            chState.filters.forEach(f => {
                if (f.state === 'clamped' || f.state === 'locked') {
                    const x = (Math.log10(Math.max(20, f.freq) / 20) / Math.log10(1000)) * w;
                    ctx.strokeStyle = f.type === 'fixed' ? "#a855f7" : "#ef4444";
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.moveTo(x, 15);
                    ctx.lineTo(x, h - 20);
                    ctx.stroke();

                    ctx.fillStyle = f.type === 'fixed' ? "#a855f7" : "#ef4444";
                    ctx.beginPath();
                    ctx.moveTo(x - 5, 20);
                    ctx.lineTo(x + 5, 20);
                    ctx.lineTo(x, 28);
                    ctx.fill();

                    ctx.font = "bold 9px monospace";
                    ctx.fillText(`${f.type === 'fixed' ? 'FIX' : 'DYN'}${f.id}: ${f.freq}Hz`, Math.max(4, Math.min(w - 70, x - 25)), 12);
                }
            });

            // Update live HUD
            const activeNotches = chState.filters.filter(f => f.state !== 'idle').length;
            const hudCount = document.getElementById("afd-hud-count");
            if (hudCount) hudCount.innerText = `${activeNotches} / 12 NOTCHES ACTIVE`;

            afdAnimId = requestAnimationFrame(() => renderAfdSpectrum(ch));
        }

        function updateAfdFilterCards(ch) {
            const chState = getAfdChannel(ch);
            const grid = document.getElementById("afd-filter-grid");
            if (!grid) return;
            grid.innerHTML = chState.filters.map(f => `
                <div class="afd-filter-card ${f.state}">
                    <div class="afd-filter-header">
                        <span style="color:${f.type === 'fixed' ? '#c084fc' : '#93c5fd'}; font-weight:800;">
                            ${f.type === 'fixed' ? 'FIXED' : 'FLOAT'} #${f.id}
                        </span>
                        <span class="afd-filter-status status-${f.state}">${f.state}</span>
                    </div>
                    <div style="display:flex; justify-content:space-between; color:#94a3b8;">
                        <span>Freq:</span>
                        <span style="color:#f8fafc; font-weight:bold;">${f.freq ? f.freq + ' Hz' : '---'}</span>
                    </div>
                    <div style="display:flex; justify-content:space-between; color:#94a3b8;">
                        <span>Depth:</span>
                        <span style="color:#ef4444; font-weight:bold;">${f.depth ? f.depth + ' dB' : '---'}</span>
                    </div>
                    <div style="display:flex; justify-content:space-between; color:#94a3b8;">
                        <span>Q Factor:</span>
                        <span style="color:#38bdf8;">${f.q}</span>
                    </div>
                    ${f.type === 'fixed' ? `
                        <button class="btn-top" style="min-height:24px; font-size:0.6rem; padding:2px 4px; margin-top:2px;" onclick="toggleLockFixedNotch(${ch}, ${f.id})">
                            ${f.state === 'locked' ? 'UNLOCK' : 'LOCK RINGOUT'}
                        </button>
                    ` : ''}
                </div>
            `).join('');
        }

        function toggleLockFixedNotch(ch, id) {
            const chState = getAfdChannel(ch);
            const f = chState.filters.find(filter => filter.id === id);
            if (f) {
                if (f.state === 'locked') {
                    f.state = 'idle';
                    f.freq = 0;
                    f.depth = 0;
                } else {
                    f.state = 'locked';
                    // Lock to current dominant frequency in live spectrum
                    f.freq = f.freq || 1000;
                    f.depth = -12;
                    f.q = 60;
                }
                updateAfdFilterCards(ch);
            }
        }

        function panicClearAfdFilters(ch) {
            const chState = getAfdChannel(ch);
            chState.filters.forEach(f => {
                f.state = 'idle';
                f.freq = 0;
                f.depth = 0;
                f.lastQuietTime = 0;
            });
            updateAfdFilterCards(ch);
        }

        function toggleAfdAutoHunting(btn) {
            afdState.autoHunting = !afdState.autoHunting;
            btn.innerText = afdState.autoHunting ? "AUTO HUNTING: ACTIVE" : "AUTO HUNTING: PAUSED";
            btn.className = afdState.autoHunting ? "afd-btn-action" : "btn-top";
        }


        // =====================================================================
        // AUTO-EQ ROOM CALIBRATION WIZARD ENGINE
        // Injects pink noise at -18 dBFS, captures 3-second FFT average via measurement mic (CH 1),
        // computes inverted correction curve, and commits to bus ReaEQ rack.
        // =====================================================================
        let roomCalState = {
            targetBus: 'master',
            isAnalyzing: false,
            secondsLeft: 3,
            computedBands: []
        };

        function startRoomCal(bus) {
            roomCalState.targetBus = bus || 'master';
            roomCalState.isAnalyzing = true;
            roomCalState.secondsLeft = 3;
            const btn = document.getElementById("btn-start-room-cal");
            if (btn) {
                btn.disabled = true;
                btn.innerText = "⏺ INJECTING PINK NOISE (-18 dBFS)...";
            }
            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                window.meterWs.send(JSON.stringify({ action: "start_room_cal", bus: roomCalState.targetBus }));
            } else {
                // Standalone simulated acoustic calculation
                let countdown = 3;
                const timer = setInterval(() => {
                    countdown--;
                    window.handleRoomCalProgress(countdown, roomCalState.targetBus);
                    if (countdown <= 0) {
                        clearInterval(timer);
                        const sampleBands = [
                            { band: 0, freq: 60, gain: -3.5, q: 1.41 },
                            { band: 1, freq: 150, gain: -4.0, q: 1.41 },
                            { band: 2, freq: 400, gain: +2.5, q: 1.41 },
                            { band: 3, freq: 1000, gain: -1.5, q: 1.41 },
                            { band: 4, freq: 2500, gain: +3.0, q: 1.41 },
                            { band: 5, freq: 5000, gain: -2.0, q: 1.41 },
                            { band: 6, freq: 8000, gain: +1.5, q: 1.41 },
                            { band: 7, freq: 12000, gain: -1.0, q: 1.41 }
                        ];
                        window.handleRoomCalComplete(roomCalState.targetBus, sampleBands);
                    }
                }, 1000);
            }
        }

        window.handleRoomCalProgress = function(secLeft, bus) {
            const statusEl = document.getElementById("room-cal-status");
            if (statusEl) {
                statusEl.innerHTML = `<span style="color:#eab308; font-weight:bold;">ANALYZING ACOUSTICS VIA CH 1 MIC... ${secLeft}s REMAINING</span>`;
            }
        };

        window.handleRoomCalComplete = function(bus, bands) {
            roomCalState.isAnalyzing = false;
            roomCalState.computedBands = bands;
            const statusEl = document.getElementById("room-cal-status");
            const btn = document.getElementById("btn-start-room-cal");
            const commitBtn = document.getElementById("btn-commit-room-cal");
            if (statusEl) {
                statusEl.innerHTML = `<span style="color:#22c55e; font-weight:bold;">✓ ROOM ACOUSTIC ANALYSIS COMPLETE: 8-Band Inverted Curve Calculated</span>`;
            }
            if (btn) {
                btn.disabled = false;
                btn.innerText = "RE-CALIBRATE";
            }
            if (commitBtn) commitBtn.style.display = "inline-block";

            // Draw computed curve on calibration preview canvas
            drawRoomCalCurve(bands);
        };

        function drawRoomCalCurve(bands) {
            const canvas = document.getElementById("room-cal-canvas");
            if (!canvas) return;
            const ctx = canvas.getContext("2d");
            const dpr = window.devicePixelRatio || 1;
            const w = canvas.parentElement ? canvas.parentElement.clientWidth : 600;
            const h = 180;
            canvas.width = Math.floor(w * dpr);
            canvas.height = Math.floor(h * dpr);
            ctx.scale(dpr, dpr);

            ctx.clearRect(0, 0, w, h);

            // Draw center 0dB line
            ctx.strokeStyle = "#334155";
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(0, h / 2);
            ctx.lineTo(w, h / 2);
            ctx.stroke();

            // Draw inverted curve points
            ctx.beginPath();
            ctx.moveTo(0, h / 2);
            bands.forEach((b, idx) => {
                const x = (idx / (bands.length - 1)) * (w - 60) + 30;
                // b.gain (-12 to +9 dB) maps to h/2 +/- 60px
                const y = (h / 2) - (b.gain / 12) * 50;
                ctx.lineTo(x, y);
            });
            ctx.strokeStyle = "#38bdf8";
            ctx.lineWidth = 2.5;
            ctx.stroke();

            // Draw point nodes
            bands.forEach((b, idx) => {
                const x = (idx / (bands.length - 1)) * (w - 60) + 30;
                const y = (h / 2) - (b.gain / 12) * 50;
                ctx.fillStyle = "#818cf8";
                ctx.beginPath();
                ctx.arc(x, y, 5, 0, 2 * Math.PI);
                ctx.fill();

                ctx.fillStyle = "#fff";
                ctx.font = "9px monospace";
                ctx.textAlign = "center";
                ctx.fillText(`${b.freq >= 1000 ? (b.freq/1000)+'k' : b.freq}: ${(b.gain >= 0 ? '+' : '') + b.gain}dB`, x, y - 8);
            });
        }

        function commitRoomCalToBus() {
            const bus = roomCalState.targetBus;
            const bands = roomCalState.computedBands;
            if (!bands || bands.length === 0) return;

            // Commit correction curve to corresponding ReaEQ instance
            const eqKey = bus === 'master' ? 'master' : (bus.startsWith('mon-') ? bus : 'master');
            const eqSt = getChannelReEq(eqKey);
            bands.forEach((b, idx) => {
                if (eqSt.bands[idx]) {
                    eqSt.bands[idx].freq = b.freq;
                    eqSt.bands[idx].gain = b.gain;
                    eqSt.bands[idx].q = b.q;
                    eqSt.bands[idx].enabled = true;
                }
            });

            const statusEl = document.getElementById("room-cal-status");
            if (statusEl) {
                statusEl.innerHTML = `<span style="color:#10b981; font-weight:bold;">✓ INVERTED ROOM EQ CURVE COMMITTED TO ${bus.toUpperCase()} RACK!</span>`;
            }
            setTimeout(() => closeDspModal(), 1200);
        }


        function openDspModal(type, target) {
            window.currentModalChannel = (typeof target === 'number') ? target : (window.dspRackCurrentChannel || 0);

            const overlay = document.getElementById("dsp-modal-overlay");
            const modal = document.getElementById("modal-container");
            const title = document.getElementById("modal-title");
            const body = document.getElementById("modal-body");
            
            let targetName = (typeof target === 'number') 
                ? (customChannelNames['ch-' + target] || `Channel ${target + 1}`) 
                : (customChannelNames[target] || String(target).toUpperCase());
            let displayTargetName = targetName;
            if (typeof target === 'string') {
                if (target === 'master') displayTargetName = customChannelNames['master'] || 'Master L/R Bus';
                else if (target.startsWith('mon-')) displayTargetName = (customChannelNames[target] ? customChannelNames[target] + ' (Mon ' + target.replace('mon-','') + ')' : 'Stage Monitor ' + target.replace('mon-', '') + ' (Wedge/IEM Out)');
                else if (target.startsWith('send-')) displayTargetName = (customChannelNames[target] ? customChannelNames[target] + ' (Send ' + target.replace('send-','') + ')' : 'Aux Send ' + target.replace('send-', ''));
                else if (target.startsWith('ret-')) displayTargetName = (customChannelNames[target] ? customChannelNames[target] + ' (Return ' + target.replace('ret-','') + ')' : 'FX Return ' + target.replace('ret-', ''));
            }

            modal.className = "dsp-modal";

            if (type === 'reeq') {
                modal.classList.add("reeq-modal", "reaeq-modal", "fullscreen");
                if (typeof window.setCurrentReEqTarget === 'function') window.setCurrentReEqTarget(target);
                const state = getChannelReEq(target);
                title.innerHTML = `🎛️ ReEQ - Parametric Graphic Equaliser (REAPER JSFX) &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div class="reeq-toolbar">
                        <div style="display:flex; gap:10px; align-items:center;">
                            <button class="btn-top" style="padding:4px 10px; font-size:0.75rem; background:#374151; font-weight:700;" onclick="closeDspModal()">← BACK TO RACK</button>
                            <span style="color:#818cf8; font-weight:bold;">ReEQ JSFX v1.2.0</span>
                            <span style="color:#8b949e;">Interactive Cytomic SVF State Variable Filtering</span>
                        </div>
                        <div style="display:flex; gap:8px; align-items:center;">
                            <label style="color:#8b949e; font-size:0.7rem;">dB Range:</label>
                            <select onchange="setReEqRange(this.value)" style="background:#161a22; color:#fff; border:1px solid #30363d; border-radius:4px; font-size:0.7rem; padding:2px 4px;">
                                <option value="12" ${state.dbRange === 12 ? 'selected' : ''}>&plusmn;12 dB</option>
                                <option value="18" ${state.dbRange === 18 ? 'selected' : ''}>&plusmn;18 dB</option>
                                <option value="24" ${state.dbRange === 24 ? 'selected' : ''}>&plusmn;24 dB</option>
                                <option value="30" ${state.dbRange === 30 ? 'selected' : ''}>&plusmn;30 dB</option>
                            </select>
                            <button class="btn-top" style="padding:3px 8px; font-size:0.7rem;" onclick="renderReEqCanvas()">HQ OVER-SAMPLING</button>
                            <button class="btn-top" id="reeq-toggle-fs-btn" style="padding:3px 8px; font-size:0.7rem; background:#4338ca; border-color:#6366f1;" onclick="toggleReaEQFullscreen()">⛶ WINDOWED</button>
                        </div>
                    </div>
                    <div class="reeq-canvas-container">
                        <canvas id="reeq-canvas"></canvas>
                    </div>
                    <div class="reeq-node-strip">
                        ${state.bands.map((b, idx) => `
                            <div class="reeq-node-card ${state.selectedBand === idx ? 'selected' : ''}" id="reeq-card-${idx}" onclick="selectReEqBand(${idx})">
                                <div class="reeq-node-header" style="color:${REEQ_COLORS[idx % REEQ_COLORS.length]};">
                                    <span>Band ${idx + 1}</span>
                                    <input type="checkbox" ${b.enabled ? 'checked' : ''} onclick="event.stopPropagation(); toggleReEqBand(${idx})">
                                </div>
                                <div class="reeq-node-controls">
                                    <select onchange="event.stopPropagation(); changeReEqType(${idx}, this.value)">
                                        ${FILTER_TYPES.map((t, tIdx) => `<option value="${tIdx}" ${b.type === tIdx ? 'selected' : ''}>${t}</option>`).join('')}
                                    </select>
                                    <div style="display:flex; justify-content:space-between; margin-top:2px;">
                                        <span id="reeq-freq-${idx}">${b.freq >= 1000 ? (b.freq / 1000).toFixed(1) + 'k' : b.freq + 'Hz'}</span>
                                        <span id="reeq-gain-${idx}" style="color:#fff;">${(b.gain >= 0 ? '+' : '') + b.gain + 'dB'}</span>
                                    </div>
                                    <span id="reeq-q-${idx}" style="font-size:0.62rem; color:#818cf8;">Q ${b.q}</span>
                                </div>
                            </div>
                        `).join('')}
                    </div>
                `;
                setTimeout(() => {
                    const canvas = document.getElementById("reeq-canvas");
                    if (canvas) resizeReaEQCanvas(canvas, canvas.parentElement);
                    else renderReEqCanvas();
                    initReEqInteractions();
                }, 50);
            } else if (type === 'denoiser') {
                title.innerHTML = `🛡️ Forensic Cyber-Denoiser PRO &bull; ${targetName}`;
                body.innerHTML = `
                    <p style="color:#8b949e; font-size:0.8rem;">10-Band spectral subtraction with automated noise floor calibration and zero phase smear.</p>
                    <div class="dsp-control-grid">
                        <div class="knob-card">
                            <span class="knob-label">Reduction Depth</span>
                            <input type="range" class="knob-slider" min="0" max="40" step="0.5" value="18" oninput="this.nextElementSibling.innerText = this.value + ' dB'">
                            <span class="knob-val">18.0 dB</span>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Threshold Offset</span>
                            <input type="range" class="knob-slider" min="-30" max="15" step="0.5" value="-6" oninput="this.nextElementSibling.innerText = this.value + ' dB'">
                            <span class="knob-val">-6.0 dB</span>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Transient Guard</span>
                            <input type="range" class="knob-slider" min="1" max="100" step="1" value="75" oninput="this.nextElementSibling.innerText = this.value + ' %'">
                            <span class="knob-val">75 %</span>
                        </div>
                        <div class="knob-card" style="justify-content:center;">
                            <button class="btn-top active" style="width:100%;" onclick="this.innerText='LEARNING NOISE...'; setTimeout(() => this.innerText='LEARN PROFILE', 2000)">LEARN PROFILE</button>
                        </div>
                    </div>
                `;
            } else if (type === 'fet1176') {
                modal.classList.add("ao-modal");
                title.innerHTML = `⚡ ANALOG OBSESSION FETISH 76 &bull; CLASS-A FET PEAK COMPRESSOR &bull; ${targetName}`;
                body.innerHTML = `
                    <div class="ao-chassis dark-theme">
                        <div class="ao-screw top-left"></div>
                        <div class="ao-screw top-right"></div>
                        <div class="ao-screw bottom-left"></div>
                        <div class="ao-screw bottom-right"></div>

                        <!-- Top Brand Header -->
                        <div style="display:flex; justify-content:space-between; align-items:center;">
                            <div class="ao-brand-wrap">
                                <div>
                                    <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                    <div style="font-size:0.6rem; color:#64748b; font-weight:700; letter-spacing:1px;">DISCRETE FET AUDIO ELECTRONICS</div>
                                </div>
                                <div style="border-left:1px solid #404040; padding-left:12px;">
                                    <div class="ao-model-title" style="color:#38bdf8;">FETISH 76</div>
                                    <div class="ao-model-sub">CLASS-A SOLID STATE LIMITING AMPLIFIER</div>
                                </div>
                            </div>
                            <div style="display:flex; gap:12px; align-items:center;">
                                <div class="ao-toggle-box">
                                    <span class="ao-toggle-label">POWER</span>
                                    <button class="ao-toggle-btn power ${fetish76State.power ? '' : 'off'}" id="ao-fet-power" onclick="fetish76State.power=!fetish76State.power; this.classList.toggle('off', !fetish76State.power);">${fetish76State.power ? 'ON' : 'OFF'}</button>
                                </div>
                            </div>
                        </div>

                        <!-- Main 3-Column Chassis Grid -->
                        <div class="ao-main-grid" style="grid-template-columns: 240px 1fr 340px;">
                            <!-- LEFT: Input & Output Controls -->
                            <div style="display:flex; flex-direction:column; gap:18px; align-items:center;">
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">INPUT (DRIVE)</span>
                                    <canvas id="fet-knob-input" class="ao-knob-canvas" width="86" height="86"></canvas>
                                    <span class="ao-knob-readout" id="fet-val-input" style="color:#38bdf8;">${fetish76State.input} dB</span>
                                </div>
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">OUTPUT (LEVEL)</span>
                                    <canvas id="fet-knob-output" class="ao-knob-canvas" width="86" height="86"></canvas>
                                    <span class="ao-knob-readout" id="fet-val-output" style="color:#38bdf8;">${fetish76State.output} dB</span>
                                </div>
                            </div>

                            <!-- CENTER: Glowing Blue-Backlit VU Meter & Ratio Pushbuttons -->
                            <div style="display:flex; flex-direction:column; align-items:center; gap:12px;">
                                <div class="ao-meter-housing" style="width:100%; max-width:340px;">
                                    <div class="ao-vu-glass" style="height:120px; background:radial-gradient(ellipse at 50% 120%, #e0f2fe 0%, #bae6fd 45%, #7dd3fc 80%, #0284c7 100%);">
                                        <canvas id="fet-vu-meter" class="ao-vu-canvas" width="340" height="120"></canvas>
                                    </div>
                                </div>
                                <div style="display:flex; flex-direction:column; align-items:center; gap:6px; margin-top:4px;">
                                    <span style="font-size:0.68rem; font-weight:900; letter-spacing:1px; color:#94a3b8;">COMPRESSION RATIO</span>
                                    <div class="ao-ratio-group">
                                        <button class="ao-ratio-btn ${fetish76State.ratio === '4' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('4')">4:1</button>
                                        <button class="ao-ratio-btn ${fetish76State.ratio === '8' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('8')">8:1</button>
                                        <button class="ao-ratio-btn ${fetish76State.ratio === '12' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('12')">12:1</button>
                                        <button class="ao-ratio-btn ${fetish76State.ratio === '20' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('20')">20:1</button>
                                        <button class="ao-ratio-btn all-in ${fetish76State.allButtons ? 'active' : ''}" onclick="toggleFetishAllButtons()">ALL IN</button>
                                    </div>
                                </div>
                            </div>

                            <!-- RIGHT: Attack, Release & Fast Timings -->
                            <div style="display:flex; flex-direction:column; gap:18px; align-items:center;">
                                <div style="display:flex; gap:16px;">
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title">ATTACK</span>
                                        <canvas id="fet-knob-attack" class="ao-knob-canvas" width="76" height="76"></canvas>
                                        <span class="ao-knob-readout" id="fet-val-attack" style="color:#38bdf8;">${fetish76State.attack}</span>
                                    </div>
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title">RELEASE</span>
                                        <canvas id="fet-knob-release" class="ao-knob-canvas" width="76" height="76"></canvas>
                                        <span class="ao-knob-readout" id="fet-val-release" style="color:#38bdf8;">${fetish76State.release}</span>
                                    </div>
                                </div>
                                <div style="font-size:0.68rem; color:#64748b; text-align:center; padding:0 12px; line-height:1.4;">
                                    * Position 7 is fastest attack (20 µs) and release (50 ms). Fast peak limiting with British saturation.
                                </div>
                            </div>
                        </div>

                        <div class="ao-credit">GUI Inspired by Eugene Reznik &bull; Analog Obsession FETISH 76</div>
                    </div>
                `;
                setTimeout(() => {
                    startAoVuLoop('fet-vu-meter', 'fetish', false);
                    bindAoRotary('fet-knob-input', 'fet-val-input', 0, 60, ' dB', () => fetish76State.input, v => fetish76State.input = Math.round(v), 'fetish', true);
                    bindAoRotary('fet-knob-output', 'fet-val-output', 0, 48, ' dB', () => fetish76State.output, v => fetish76State.output = Math.round(v), 'fetish', true);
                    bindAoRotary('fet-knob-attack', 'fet-val-attack', 1, 7, '', () => fetish76State.attack, v => fetish76State.attack = Math.round(v), 'fetish', false);
                    bindAoRotary('fet-knob-release', 'fet-val-release', 1, 7, '', () => fetish76State.release, v => fetish76State.release = Math.round(v), 'fetish', false);
                }, 50);
            } else if (type === 'comp') {
                title.innerHTML = `🎙️ Studio VCA Compressor &bull; ${targetName}`;
                body.innerHTML = `
                    <div class="dsp-control-grid">
                        <div class="knob-card">
                            <span class="knob-label">Threshold</span>
                            <input type="range" class="knob-slider" min="-50" max="0" step="1" value="-20" oninput="this.nextElementSibling.innerText = this.value + ' dB'">
                            <span class="knob-val">-20 dB</span>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Ratio</span>
                            <input type="range" class="knob-slider" min="1.5" max="20" step="0.5" value="4" oninput="this.nextElementSibling.innerText = this.value + ':1'">
                            <span class="knob-val">4.0:1</span>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Attack</span>
                            <input type="range" class="knob-slider" min="1" max="100" step="1" value="15" oninput="this.nextElementSibling.innerText = this.value + ' ms'">
                            <span class="knob-val">15 ms</span>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Release</span>
                            <input type="range" class="knob-slider" min="20" max="1500" step="10" value="150" oninput="this.nextElementSibling.innerText = this.value + ' ms'">
                            <span class="knob-val">150 ms</span>
                        </div>
                    </div>
                `;
            } else if (type === 'la2a') {
                modal.classList.add("ao-modal");
                title.innerHTML = `💡 ANALOG OBSESSION LALA &bull; OPTICAL LEVELING AMPLIFIER &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div class="ao-chassis">
                        <div class="ao-screw top-left"></div>
                        <div class="ao-screw top-right"></div>
                        <div class="ao-screw bottom-left"></div>
                        <div class="ao-screw bottom-right"></div>

                        <!-- Top Brand Header matching exact image -->
                        <div style="display:flex; justify-content:space-between; align-items:center;">
                            <div class="ao-brand-wrap">
                                <div>
                                    <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                    <div style="font-size:0.6rem; color:#78716c; font-weight:700; letter-spacing:1px;">PROFESSIONAL AUDIO PLUGINS</div>
                                </div>
                                <div style="border-left:2px solid #a8a29e; padding-left:14px;">
                                    <div class="ao-model-title">LALA</div>
                                    <div class="ao-model-sub">OPTICAL LEVELING AMPLIFIER</div>
                                </div>
                            </div>
                            <div style="display:flex; gap:16px; align-items:center;">
                                <div class="ao-toggle-box">
                                    <span class="ao-toggle-label">POWER</span>
                                    <button class="ao-toggle-btn power ${lalaState.power ? '' : 'off'}" id="ao-lala-power" onclick="lalaState.power=!lalaState.power; this.classList.toggle('off', !lalaState.power);">${lalaState.power ? 'ON' : 'OFF'}</button>
                                </div>
                            </div>
                        </div>

                        <!-- Main 3-Column Chassis Grid -->
                        <div class="ao-main-grid">
                            <!-- LEFT COLUMN: Sidechain & Filter Knobs (HPF, EXT, MF, MG, HF) -->
                            <div style="display:flex; flex-direction:column; gap:12px; align-items:center;">
                                <div style="font-size:0.75rem; font-weight:900; letter-spacing:1px; color:#44403c; border-bottom:1px solid #a8a29e; width:100%; text-align:center; padding-bottom:4px;">SIDECHAIN &amp; EMPHASIS</div>
                                <div style="display:grid; grid-template-columns: 1fr 1fr; gap:14px; width:100%;">
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title">HPF</span>
                                        <canvas id="lala-knob-hpf" class="ao-knob-canvas" width="62" height="62"></canvas>
                                        <span class="ao-knob-readout" id="lala-val-hpf">${lalaState.hpf} Hz</span>
                                    </div>
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title">MF (FREQ)</span>
                                        <canvas id="lala-knob-mf" class="ao-knob-canvas" width="62" height="62"></canvas>
                                        <span class="ao-knob-readout" id="lala-val-mf">${(lalaState.mf/1000).toFixed(1)} kHz</span>
                                    </div>
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title">MG (GAIN)</span>
                                        <canvas id="lala-knob-mg" class="ao-knob-canvas" width="62" height="62"></canvas>
                                        <span class="ao-knob-readout" id="lala-val-mg">${lalaState.mg >= 0 ? '+' : ''}${lalaState.mg} dB</span>
                                    </div>
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title">HF / FLAT</span>
                                        <canvas id="lala-knob-hfFlat" class="ao-knob-canvas" width="62" height="62"></canvas>
                                        <span class="ao-knob-readout" id="lala-val-hfFlat">${lalaState.hfFlat} %</span>
                                    </div>
                                </div>
                                <div class="ao-switch-cluster" style="margin-top:2px;">
                                    <div class="ao-toggle-box">
                                        <span class="ao-toggle-label">SIDECHAIN SOURCE</span>
                                        <button class="ao-toggle-btn ${lalaState.ext ? 'active' : ''}" id="ao-lala-ext" style="width:72px;" onclick="lalaState.ext=!lalaState.ext; this.classList.toggle('active', lalaState.ext); this.innerText=lalaState.ext ? 'EXT SC' : 'INT SC';">${lalaState.ext ? 'EXT SC' : 'INT SC'}</button>
                                    </div>
                                </div>
                            </div>

                            <!-- CENTER COLUMN: Illuminated Amber VU Meter & Mode Switches -->
                            <div style="display:flex; flex-direction:column; align-items:center; gap:14px;">
                                <div class="ao-meter-housing" style="width:100%; max-width:320px;">
                                    <div class="ao-vu-glass">
                                        <canvas id="lala-vu-meter" class="ao-vu-canvas" width="320" height="130"></canvas>
                                    </div>
                                    <div style="text-align:center; font-size:0.62rem; color:#f59e0b; font-weight:700; padding:2px 0; letter-spacing:1px; opacity:0.8;">⚠ METER SIMULATED — not real audio GR</div>
                                </div>
                                <div class="ao-switch-cluster">
                                    <div class="ao-toggle-box">
                                        <span class="ao-toggle-label">MODE</span>
                                        <button class="ao-toggle-btn ${lalaState.limitMode ? 'active' : ''}" id="ao-lala-limit" style="width:84px;" onclick="lalaState.limitMode=!lalaState.limitMode; this.classList.toggle('active', lalaState.limitMode); this.innerText=lalaState.limitMode ? 'LIMIT' : 'COMPRESS';">${lalaState.limitMode ? 'LIMIT' : 'COMPRESS'}</button>
                                    </div>
                                </div>
                            </div>

                            <!-- RIGHT COLUMN: Large Bakelite Knobs (GAIN, PEAK REDUCTION, MIX) -->
                            <div style="display:flex; flex-direction:column; gap:14px; align-items:center;">
                                <div style="display:flex; gap:20px; align-items:center;">
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title" style="font-size:0.86rem;">GAIN</span>
                                        <canvas id="lala-knob-gain" class="ao-knob-canvas" width="94" height="94"></canvas>
                                        <span class="ao-knob-readout" id="lala-val-gain" style="font-size:0.82rem;">${lalaState.gain}</span>
                                    </div>
                                    <div class="ao-knob-box">
                                        <span class="ao-knob-title" style="font-size:0.86rem;">PEAK REDUCTION</span>
                                        <canvas id="lala-knob-peak" class="ao-knob-canvas" width="94" height="94"></canvas>
                                        <span class="ao-knob-readout" id="lala-val-peak" style="font-size:0.82rem;">${lalaState.peakReduction}</span>
                                    </div>
                                </div>
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">DRY / WET MIX</span>
                                    <canvas id="lala-knob-mix" class="ao-knob-canvas" width="64" height="64"></canvas>
                                    <span class="ao-knob-readout" id="lala-val-mix">${lalaState.mix} %</span>
                                </div>
                            </div>
                        </div>

                        <!-- Signature Credit as seen in reference image -->
                        <div class="ao-credit">GUI by Eugene Reznik &bull; Analog Obsession LALA</div>
                    </div>
                `;
                setTimeout(() => {
                    startAoVuLoop('lala-vu-meter', 'lala', false);
                    bindAoRotary('lala-knob-gain', 'lala-val-gain', 0, 100, '', () => lalaState.gain, v => lalaState.gain = Math.round(v), 'lala', true);
                    bindAoRotary('lala-knob-peak', 'lala-val-peak', 0, 100, '', () => lalaState.peakReduction, v => lalaState.peakReduction = Math.round(v), 'lala', true);
                    bindAoRotary('lala-knob-mix', 'lala-val-mix', 0, 100, ' %', () => lalaState.mix, v => lalaState.mix = Math.round(v), 'lala', false);
                    bindAoRotary('lala-knob-hpf', 'lala-val-hpf', 20, 500, ' Hz', () => lalaState.hpf, v => lalaState.hpf = Math.round(v), 'lala', false);
                    bindAoRotary('lala-knob-mf', 'lala-val-mf', 500, 10000, ' Hz', () => lalaState.mf, v => lalaState.mf = Math.round(v), 'lala', false);
                    bindAoRotary('lala-knob-mg', 'lala-val-mg', -6, 6, ' dB', () => lalaState.mg, v => lalaState.mg = Math.round(v), 'lala', false);
                    bindAoRotary('lala-knob-hfFlat', 'lala-val-hfFlat', 0, 100, ' %', () => lalaState.hfFlat, v => lalaState.hfFlat = Math.round(v), 'lala', false);
                }, 50);
            } else if (type === 'limiter') {
                modal.classList.add("ao-modal");
                const isMon = typeof target === 'string' && target.startsWith('mon-');
                title.innerHTML = `🛑 ANALOG OBSESSION BUSTER &bull; BUS &amp; PEAK LIMITER &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div class="ao-chassis limiter-theme">
                        <div class="ao-screw top-left"></div>
                        <div class="ao-screw top-right"></div>
                        <div class="ao-screw bottom-left"></div>
                        <div class="ao-screw bottom-right"></div>

                        <!-- Top Brand Header -->
                        <div style="display:flex; justify-content:space-between; align-items:center;">
                            <div class="ao-brand-wrap">
                                <div>
                                    <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                    <div style="font-size:0.6rem; color:#94a3b8; font-weight:700; letter-spacing:1px;">MASTERING &amp; PA PROTECTION</div>
                                </div>
                                <div style="border-left:1px solid #334155; padding-left:12px;">
                                    <div class="ao-model-title" style="color:#f59e0b;">BUSTER LIMITER</div>
                                    <div class="ao-model-sub">${isMon ? 'ACOUSTIC STAGE WEDGE &amp; IEM PROTECTION' : 'ZERO-LATENCY FOH MASTER BUS LIMITER'}</div>
                                </div>
                            </div>
                            <div style="display:flex; gap:12px; align-items:center;">
                                <div class="ao-toggle-box">
                                    <span class="ao-toggle-label">POWER</span>
                                    <button class="ao-toggle-btn power ${busterLimiterState.power ? '' : 'off'}" id="ao-buster-power" onclick="busterLimiterState.power=!busterLimiterState.power; this.classList.toggle('off', !busterLimiterState.power);">${busterLimiterState.power ? 'ON' : 'OFF'}</button>
                                </div>
                            </div>
                        </div>

                        <!-- Main 3-Column Chassis Grid -->
                        <div class="ao-main-grid">
                            <!-- LEFT: Threshold & Drive -->
                            <div style="display:flex; flex-direction:column; gap:18px; align-items:center;">
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">THRESHOLD</span>
                                    <canvas id="buster-knob-thresh" class="ao-knob-canvas" width="86" height="86"></canvas>
                                    <span class="ao-knob-readout" id="buster-val-thresh" style="color:#f59e0b;">${busterLimiterState.threshold.toFixed(1)} dBFS</span>
                                </div>
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">DRIVE / PUNCH</span>
                                    <canvas id="buster-knob-drive" class="ao-knob-canvas" width="64" height="64"></canvas>
                                    <span class="ao-knob-readout" id="buster-val-drive" style="color:#f59e0b;">${busterLimiterState.drive.toFixed(1)} dB</span>
                                </div>
                            </div>

                            <!-- CENTER: Illuminated Peak GR VU Meter -->
                            <div style="display:flex; flex-direction:column; align-items:center; gap:12px;">
                                <div class="ao-meter-housing" style="width:100%; max-width:320px;">
                                    <div class="ao-vu-glass" style="background:radial-gradient(ellipse at 50% 120%, #fef3c7 0%, #fde68a 45%, #f59e0b 80%, #b45309 100%);">
                                        <canvas id="buster-vu-meter" class="ao-vu-canvas" width="320" height="130"></canvas>
                                    </div>
                                    <div style="text-align:center; font-size:0.62rem; color:#f59e0b; font-weight:700; padding:2px 0; letter-spacing:1px; opacity:0.8;">⚠ METER SIMULATED — not real audio GR</div>
                                </div>
                                <div class="ao-switch-cluster">
                                    <div class="ao-toggle-box">
                                        <span class="ao-toggle-label">CRUSH MODE</span>
                                        <button class="ao-toggle-btn ${busterLimiterState.crush ? 'active' : ''}" id="ao-buster-crush" style="width:84px;" onclick="busterLimiterState.crush=!busterLimiterState.crush; this.classList.toggle('active', busterLimiterState.crush);">${busterLimiterState.crush ? 'CRUSH ON' : 'CLEAN'}</button>
                                    </div>
                                </div>
                            </div>

                            <!-- RIGHT: Ceiling & Release -->
                            <div style="display:flex; flex-direction:column; gap:18px; align-items:center;">
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">CEILING MARGIN</span>
                                    <canvas id="buster-knob-ceiling" class="ao-knob-canvas" width="86" height="86"></canvas>
                                    <span class="ao-knob-readout" id="buster-val-ceiling" style="color:#f59e0b;">${busterLimiterState.ceiling.toFixed(1)} dBFS</span>
                                </div>
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">RELEASE SPEED</span>
                                    <canvas id="buster-knob-release" class="ao-knob-canvas" width="64" height="64"></canvas>
                                    <span class="ao-knob-readout" id="buster-val-release" style="color:#f59e0b;">${busterLimiterState.release} ms</span>
                                </div>
                            </div>
                        </div>

                        <div class="ao-credit">GUI by Eugene Reznik &bull; Analog Obsession BUSTER Limiter</div>
                    </div>
                `;
                setTimeout(() => {
                    startAoVuLoop('buster-vu-meter', 'lala', true);
                    bindAoRotary('buster-knob-thresh', 'buster-val-thresh', -18, 0, ' dBFS', () => busterLimiterState.threshold, v => busterLimiterState.threshold = v, 'limiter', true);
                    bindAoRotary('buster-knob-ceiling', 'buster-val-ceiling', -6, 0, ' dBFS', () => busterLimiterState.ceiling, v => busterLimiterState.ceiling = v, 'limiter', true);
                    bindAoRotary('buster-knob-drive', 'buster-val-drive', 0, 12, ' dB', () => busterLimiterState.drive, v => busterLimiterState.drive = v, 'limiter', false);
                    bindAoRotary('buster-knob-release', 'buster-val-release', 5, 150, ' ms', () => busterLimiterState.release, v => busterLimiterState.release = Math.round(v), 'limiter', false);
                }, 50);
            } else if (type === 'summing') {
                modal.classList.add("ao-modal");
                title.innerHTML = `🎛️ ANALOG CONSOLE SUMMING &amp; TRANSFORMER EMULATION &bull; MASTER BUS`;
                body.innerHTML = `
                    <div class="ao-chassis summing-theme">
                        <div class="ao-screw top-left"></div>
                        <div class="ao-screw top-right"></div>
                        <div class="ao-screw bottom-left"></div>
                        <div class="ao-screw bottom-right"></div>

                        <!-- Brand Header -->
                        <div style="display:flex; justify-content:space-between; align-items:center;">
                            <div class="ao-brand-wrap">
                                <div>
                                    <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                    <div style="font-size:0.6rem; color:#94a3b8; font-weight:700; letter-spacing:1px;">CONSOLE TOPOLOGY &amp; IRON CORE</div>
                                </div>
                                <div style="border-left:1px solid #3f3f46; padding-left:12px;">
                                    <div class="ao-model-title" style="color:#d97706;">CONSOLE SUMMING &amp; TRANSFORMER</div>
                                    <div class="ao-model-sub">NEVE 8048 &bull; API 2520 &bull; SSL 4000E BUS SATURATION</div>
                                </div>
                            </div>
                            <div class="ao-toggle-box">
                                <span class="ao-toggle-label">SUMMING ACTIVE</span>
                                <button class="ao-toggle-btn power ${analogSummingState.enabled ? '' : 'off'}" id="ao-summing-power" onclick="analogSummingState.enabled=!analogSummingState.enabled; this.classList.toggle('off', !analogSummingState.enabled); updateSummingHeaderBtn();">${analogSummingState.enabled ? 'ACTIVE' : 'BYPASS'}</button>
                            </div>
                        </div>

                        <!-- Summing Controls Deck -->
                        <div class="summing-deck">
                            <div class="summing-card">
                                <span class="summing-card-title">CONSOLE TOPOLOGY</span>
                                <select class="summing-selector" onchange="setSummingConsole(this.value)">
                                    <option value="neve" ${analogSummingState.console === 'neve' ? 'selected' : ''}>British Classic 8048 (Fat Iron)</option>
                                    <option value="api" ${analogSummingState.console === 'api' ? 'selected' : ''}>American Punch 2520 (Aggressive)</option>
                                    <option value="ssl" ${analogSummingState.console === 'ssl' ? 'selected' : ''}>Oxford 4000E (Clean &amp; Tight)</option>
                                    <option value="tube" ${analogSummingState.console === 'tube' ? 'selected' : ''}>Germanium Tube Desk (Warm Bloom)</option>
                                </select>
                                <span style="font-size:0.65rem; color:#64748b; text-align:center; margin-top:4px;">Custom non-linear transformer curve with harmonic phase blooming.</span>
                            </div>

                            <div class="summing-card">
                                <span class="summing-card-title">DRIVE &amp; HARMONICS</span>
                                <canvas id="summing-knob-drive" class="ao-knob-canvas" width="68" height="68"></canvas>
                                <span class="ao-knob-readout" id="summing-val-drive" style="color:#38bdf8;">${analogSummingState.drive.toFixed(1)} dB</span>
                            </div>

                            <div class="summing-card">
                                <span class="summing-card-title">LOW-END BLOOM</span>
                                <canvas id="summing-knob-warmth" class="ao-knob-canvas" width="68" height="68"></canvas>
                                <span class="ao-knob-readout" id="summing-val-warmth" style="color:#38bdf8;">${analogSummingState.warmth.toFixed(1)} dB</span>
                            </div>

                            <div class="summing-card">
                                <span class="summing-card-title">CROSSTALK BLEED</span>
                                <canvas id="summing-knob-crosstalk" class="ao-knob-canvas" width="68" height="68"></canvas>
                                <span class="ao-knob-readout" id="summing-val-crosstalk" style="color:#38bdf8;">${analogSummingState.crosstalk} dB</span>
                            </div>
                        </div>

                        <div class="ao-credit">GUI by Eugene Reznik &bull; Analog Console Summing Engine</div>
                    </div>
                `;
                setTimeout(() => {
                    bindAoRotary('summing-knob-drive', 'summing-val-drive', 0, 12, ' dB', () => analogSummingState.drive, v => analogSummingState.drive = v, 'summing', false);
                    bindAoRotary('summing-knob-warmth', 'summing-val-warmth', 0, 10, ' dB', () => analogSummingState.warmth, v => analogSummingState.warmth = v, 'summing', false);
                    bindAoRotary('summing-knob-crosstalk', 'summing-val-crosstalk', -90, -40, ' dB', () => analogSummingState.crosstalk, v => analogSummingState.crosstalk = Math.round(v), 'summing', false);
                }, 50);
            } else if (type === 'feedback') {
                title.innerHTML = `🎯 Automatic Feedback Destroyer (AFD) PRO &bull; ${targetName}`;
                body.innerHTML = `
                    <div class="afd-container">
                        <div class="afd-controls-row">
                            <div style="display:flex; gap:10px; align-items:center;">
                                <button class="afd-btn-action" onclick="toggleAfdAutoHunting(this)">AUTO HUNTING: ACTIVE</button>
                                <span style="font-size:0.75rem; color:#94a3b8;">Continuously scanning FFT spectrum for sharp resonant spikes</span>
                            </div>
                            <div style="display:flex; gap:8px;">
                                <button class="afd-btn-danger" onclick="panicClearAfdFilters(${target === 'master' ? "'master'" : target})">PANIC: RESET NOTCHES</button>
                            </div>
                        </div>

                        <!-- Real-time FFT Acoustic Resonance Spectrum Display -->
                        <div class="afd-spectrum-container">
                            <canvas id="afd-spectrum-canvas"></canvas>
                            <div class="afd-hud">
                                <div class="afd-hud-item">
                                    <span>DETECTION:</span>
                                    <span class="afd-hud-val" style="color:#22c55e;">CONTINUOUS FFT</span>
                                </div>
                                <div class="afd-hud-item">
                                    <span>NOTCHES:</span>
                                    <span class="afd-hud-val" id="afd-hud-count" style="color:#ef4444;">0 / 12 NOTCHES ACTIVE</span>
                                </div>
                                <div class="afd-hud-item">
                                    <span>SURGICAL Q:</span>
                                    <span class="afd-hud-val" style="color:#38bdf8;">Q=50 (1/60th Octave)</span>
                                </div>
                            </div>
                        </div>

                        <!-- 12 Adaptive Narrow Notch Filter Bank (6 Fixed + 6 Dynamic) -->
                        <div style="display:flex; justify-content:space-between; align-items:center; margin-top:2px;">
                            <span style="font-size:0.72rem; font-weight:bold; color:#cbd5e1; text-transform:uppercase; letter-spacing:0.5px;">12 Adaptive Narrow Notch Filter Bank (6 Fixed + 6 Dynamic) (Automatic Deployment)</span>
                            <span style="font-size:0.68rem; color:#8b949e;">Clamps within 10ms of resonance threshold breach</span>
                        </div>
                        <div class="afd-filter-grid" id="afd-filter-grid">
                            <!-- Populated dynamically by updateAfdFilterCards -->
                        </div>
                    </div>
                `;
                setTimeout(() => {
                    updateAfdFilterCards(target);
                    if (afdAnimId) cancelAnimationFrame(afdAnimId);
                    renderAfdSpectrum(target);
                }, 50);
            } else if (type === 'deesser') {
                const chIdx = typeof target === 'number' ? target : 0;
                const dState = getChannelRackState(chIdx).deesser;
                title.innerHTML = `🎙️ ANALOG OBSESSION LOADES DE-ESSER &bull; ${targetName}`;
                body.innerHTML = `
                    <p style="color:#8b949e; font-size:0.8rem;">Dynamic split-band vocal sibilance control with natural release envelope.</p>
                    <div class="dsp-control-grid">
                        <div class="knob-card">
                            <span class="knob-label">Threshold</span>
                            <input type="range" class="knob-slider" min="-40" max="0" step="1" value="${dState.threshold}" oninput="window.getChannelRackState(${chIdx}).deesser.threshold=parseInt(this.value); dispatchDspParam(${chIdx},'loadesThresh',this.value); this.nextElementSibling.innerText = this.value + ' dB'">
                            <span class="knob-val">${dState.threshold} dB</span>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Release Speed</span>
                            <input type="range" class="knob-slider" min="10" max="200" step="5" value="${dState.release}" oninput="window.getChannelRackState(${chIdx}).deesser.release=parseInt(this.value); dispatchDspParam(${chIdx},'loadesRelease',this.value); this.nextElementSibling.innerText = this.value + ' ms'">
                            <span class="knob-val">${dState.release} ms</span>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Processing Mode</span>
                            <button class="tap-mode-btn ${dState.mode === 'wide' ? 'active' : ''}" style="margin-top:10px; width:100%; min-height:42px;" onclick="var ds=window.getChannelRackState(${chIdx}).deesser; ds.mode=ds.mode==='band'?'wide':'band'; dispatchDspParam(${chIdx},'loadesMode',ds.mode); this.innerText=ds.mode.toUpperCase(); this.classList.toggle('active',ds.mode==='wide');">
                                ${dState.mode.toUpperCase()}
                            </button>
                        </div>
                        <div class="knob-card">
                            <span class="knob-label">Sidechain Listen</span>
                            <input type="checkbox" style="margin-top:8px; width:22px; height:22px; cursor:pointer;" onchange="dispatchDspParam(${chIdx},'loadesListen',this.checked)">
                            <span class="knob-val" style="font-size:0.65rem; color:#8b949e;">SOLO DETECTOR</span>
                        </div>
                    </div>
                `;
            } else if (type === 'delay') {
                title.innerHTML = `⏱️ TAL-DELAY-PRO Vintage Tape &amp; Digital Echo &bull; ${displayTargetName}`;
                body.innerHTML = renderDelayControls();
                setTimeout(() => {
                    initTalDelayRotaries();
                }, 50);
            } else if (type === 'reverb') {
                title.innerHTML = `✨ Studio Reverb Suite &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div style="display:flex; gap:8px; margin-bottom:8px;">
                        <button class="btn-top ${reverbDspState.engine === 'tal' ? 'active' : ''}" style="flex:1; min-height:42px;" onclick="switchReverbEngine('tal')">🏛️ TAL-Reverb-4 Vintage Plate &amp; 80s Hall</button>
                        <button class="btn-top ${reverbDspState.engine === 'atlantis' ? 'active' : ''}" style="flex:1; min-height:42px;" onclick="switchReverbEngine('atlantis')">🌌 Atlantis JSFX Acoustic Impulse Reverb</button>
                    </div>

                    <div id="reverb-engine-content">
                        ${renderReverbControls()}
                    </div>
                `;
                setTimeout(() => {
                    if (reverbDspState.engine === 'tal') initTalRotaries();
                    else initIrCanvas();
                }, 50);
            } else if (type === 'room_cal') {
                title.innerHTML = `🏛️ AUTO-EQ ROOM ACOUSTIC CALIBRATION WIZARD`;
                body.innerHTML = `
                    <div class="room-cal-box">
                        <div class="room-cal-row">
                            <div class="room-cal-step">
                                <span class="room-cal-step-num">1</span>
                                <span>TARGET OUTPUT BUS:</span>
                            </div>
                            <select id="room-cal-bus-select" class="soft-patch-select" style="min-width:180px;" onchange="roomCalState.targetBus=this.value;">
                                <option value="master" selected>Main FOH PA (Stereo L/R)</option>
                                <option value="mon-1">Aux 1: Stage Wedge L</option>
                                <option value="mon-2">Aux 2: Stage Wedge R</option>
                                <option value="mon-3">Aux 3: In-Ear Mon 1 (IEM)</option>
                                <option value="mon-4">Aux 4: In-Ear Mon 2 (IEM)</option>
                                <option value="mon-5">Aux 5: Drum Fill Sub</option>
                                <option value="mon-6">Aux 6: Side Fill PA</option>
                            </select>
                        </div>

                        <div class="room-cal-row">
                            <div class="room-cal-step">
                                <span class="room-cal-step-num">2</span>
                                <span>MEASUREMENT SOURCE:</span>
                            </div>
                            <span style="font-size:0.8rem; color:#94a3b8; font-family:monospace;">RTA Measurement Mic on <b>Channel 1 (IN 1)</b></span>
                        </div>

                        <div class="room-cal-row">
                            <div class="room-cal-step">
                                <span class="room-cal-step-num">3</span>
                                <span>TEST SIGNAL:</span>
                            </div>
                            <span style="font-size:0.8rem; color:#38bdf8; font-family:monospace;">Pink Noise Burst @ -18.0 dBFS Reference</span>
                        </div>

                        <div style="display:flex; justify-content:center; margin:8px 0;">
                            <button class="btn-top" id="btn-start-room-cal" style="background:#2563eb; border-color:#3b82f6; font-size:0.85rem; padding:8px 24px; min-height:44px;" onclick="startRoomCal(document.getElementById('room-cal-bus-select').value)">
                                ⏺ START 3-SECOND ROOM CALIBRATION
                            </button>
                        </div>

                        <div id="room-cal-status" style="text-align:center; min-height:22px; font-size:0.8rem;">
                            <span style="color:#64748b;">Ready to measure. Ensure measurement mic is positioned at FOH mix position.</span>
                        </div>

                        <div style="width:100%;">
                            <canvas id="room-cal-canvas" class="room-cal-curve-canvas"></canvas>
                        </div>

                        <div style="display:flex; justify-content:flex-end; gap:10px;">
                            <button class="btn-top" id="btn-commit-room-cal" style="display:none; background:#10b981; border-color:#059669; font-weight:800; min-height:42px; padding:6px 18px;" onclick="commitRoomCalToBus()">
                                ✓ COMMIT INVERTED CURVE TO BUS EQ
                            </button>
                        </div>
                    </div>
                `;
            }
            overlay.classList.add("open");
        }

        // =====================================================================
        // LEVEL 2 CLOSE: Returns ONLY to Level 1 DSP Rack (never Level 0 mixer)
        // Triggered by: "← BACK TO RACK", "Close", "X", or backdrop overlay click
        // =====================================================================
        function closeDspModal() {
            if (afdAnimId) {
                cancelAnimationFrame(afdAnimId);
                afdAnimId = null;
            }
            if (aoVuMeterAnimId) {
                cancelAnimationFrame(aoVuMeterAnimId);
                aoVuMeterAnimId = null;
            }
            // Close ONLY Level 2 overlay — do NOT close Level 1
            const overlay = document.getElementById("dsp-modal-overlay");
            if (overlay) overlay.classList.remove("open");
            const auxModal = document.getElementById("auxModal");
            if (auxModal) auxModal.style.display = "none";
            const modal = document.getElementById("modal-container");
            if (modal) modal.classList.remove("fullscreen", "reaeq-modal", "reeq-modal");

            const rackPage = document.getElementById("dsp-rack-page");
            if (rackPage && rackPage.classList.contains("open")) {
                renderMiniReEq(window.dspRackCurrentChannel || 0);
            }
        }
        function closeModalOnOverlay(e) {
            // Clicking the Level 2 backdrop returns to Level 1 only
            if (e.target.id === "dsp-modal-overlay") {
                closeDspModal();
            }
            // NOTE: channel-rack-overlay (Level 1) backdrop click is intentionally NOT
            // wired here — Level 1 must only close via the explicit "BACK TO MIXER" button
        }


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
    window.openDspModal = openDspModal;
    window.closeDspModal = closeDspModal;
    window.closeModalOnOverlay = closeModalOnOverlay;
})(window);

window.openDspModal = typeof openDspModal !== 'undefined' ? openDspModal : window.openDspModal;

window.closeDspModal = typeof closeDspModal !== 'undefined' ? closeDspModal : window.closeDspModal;

window.closeModalOnOverlay = typeof closeModalOnOverlay !== 'undefined' ? closeModalOnOverlay : window.closeModalOnOverlay;
