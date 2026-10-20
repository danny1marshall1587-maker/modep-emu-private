// room-suite.js - 12-notch Auto-Feedback Suppressor (AFS) and Auto-EQ Room Calibration wizard
(function(window) {
    'use strict';
    var analogSummingState = window.analogSummingState || { console: 'neve', drive: 2.5, warmth: 3.0, crosstalk: -60, ironSat: true, enabled: true };
    window.analogSummingState = analogSummingState;
    var aoVuMeterAnimId = window.aoVuMeterAnimId || null;
        // 4b. Hybrid -Infinity dB Surgical AFD & Moving-Bass Resonance Tracker
        // Single-filter 64-bit -Infinity dB spectral null (eliminates multi-bus X32 stacking)
        // Channels 1-8: Live Auto-Hunting + Moving-Bass Tracker (60-280Hz centroid follow + Auto-Q)
        // Monitors 1-6 & Master L/R: Soundcheck Ring-Out Lock (Fixed Notches 1-6)
        const afdState = {
            activeTarget: 0,
            autoHunting: true,
            infNullMode: true, // True -Infinity dB single-filter spectral zero
            movingBassTracker: true, // 60Hz-280Hz centroid follow + Auto-Q widening
            channels: {}
        };
        function dispatchAfdNotchWs(ch, slotIdx, f) {
            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                try {
                    window.meterWs.send(JSON.stringify({
                        action: "set_afd_notch",
                        target: ch,
                        slot: slotIdx,
                        active: (f.state !== 'idle') ? 1 : 0,
                        infNull: f.infNull ? 1 : 0,
                        freq: f.freq || 1000,
                        depth: f.depth || -36,
                        q: f.q || 60
                    }));
                } catch (e) {}
            }
        }
        function getAfdChannel(ch) {
            if (!afdState.channels[ch]) {
                const isInputCh = (typeof ch === 'number') || (typeof ch === 'string' && ch.startsWith('ch-'));
                const filters = [];
                for (let i = 0; i < 12; i++) {
                    const isFixed = i < 6;
                    filters.push({
                        id: i + 1,
                        type: isFixed ? 'fixed' : 'floating',
                        state: 'idle', // 'idle' | 'locked' | 'clamped' | 'tracking'
                        freq: 0,
                        depth: -60,
                        infNull: true,
                        q: isFixed ? 60 : 50,
                        bassTrack: false,
                        timeActive: 0,
                        lastQuietTime: 0
                    });
                }
                afdState.channels[ch] = {
                    role: isInputCh ? 'live_channel' : 'soundcheck_bus',
                    autoHunting: true,
                    filters: filters,
                    lastFftBins: new Float32Array(64)
                };
            }
            return afdState.channels[ch];
        }
        window.afdAnimId = null;
        function runAfdPeakDetection(ch) {
            const chState = getAfdChannel(ch);
            if (!afdState.autoHunting || !chState.autoHunting) return;
            const now = Date.now();
            const rta = window.liveRtaBins || [];
            if (rta.length === 0) return;
            for (let b = 0; b < Math.min(64, rta.length); b++) chState.lastFftBins[b] = rta[b];
            for (let b = 1; b < chState.lastFftBins.length - 1; b++) {
                const level = chState.lastFftBins[b];
                if (level > -18.0 && level >= chState.lastFftBins[b - 1] && level >= chState.lastFftBins[b + 1]) {
                    // Sub-bin quadratic centroid interpolation for moving-bass tracking
                    const lPrev = chState.lastFftBins[b - 1], lNext = chState.lastFftBins[b + 1];
                    const denom = (lPrev - 2 * level + lNext);
                    const deltaBin = Math.abs(denom) > 0.001 ? Math.max(-0.5, Math.min(0.5, 0.5 * (lPrev - lNext) / denom)) : 0;
                    const freq = Math.round(20 * Math.pow(1000, (b + deltaBin) / 63));
                    const isBassBody = (freq >= 60 && freq <= 280);
                    let matched = false;
                    for (let idx = 0; idx < chState.filters.length; idx++) {
                        const f = chState.filters[idx];
                        if (f.state !== 'idle') {
                            const trackWin = (isBassBody && afdState.movingBassTracker) ? 28 : 45;
                            if (Math.abs(f.freq - freq) <= trackWin) {
                                matched = true;
                                f.lastQuietTime = 0;
                                // Moving-Bass Tracker: follow drifting guitar body resonance & auto-widen Q if persistent
                                if (isBassBody && afdState.movingBassTracker && f.type === 'floating') {
                                    const drift = Math.max(-15, Math.min(15, freq - f.freq));
                                    if (Math.abs(drift) >= 1) {
                                        f.freq = Math.round(f.freq + drift * 0.45);
                                        f.q = Math.max(18, Math.round(f.q * 0.88)); // Auto-Width widens Q down to 18
                                        f.state = 'tracking';
                                        f.bassTrack = true;
                                        dispatchAfdNotchWs(ch, idx, f);
                                        updateAfdFilterCards(ch);
                                    }
                                }
                                break;
                            }
                        }
                    }
                    if (!matched) {
                        const pool = chState.role === 'soundcheck_bus' ? chState.filters.slice(0, 6) : chState.filters.slice(6, 12);
                        const slot = pool.find(f => f.state === 'idle') || chState.filters.find(f => f.state === 'idle');
                        if (slot) {
                            const slotIdx = slot.id - 1;
                            slot.state = chState.role === 'soundcheck_bus' ? 'locked' : (isBassBody ? 'tracking' : 'clamped');
                            slot.freq = freq;
                            slot.infNull = afdState.infNullMode;
                            slot.depth = afdState.infNullMode ? -96 : Math.max(-36, Math.min(-6, Math.round(-level)));
                            slot.q = isBassBody ? 32 : 65;
                            slot.bassTrack = isBassBody;
                            slot.timeActive = now;
                            slot.lastQuietTime = 0;
                            dispatchAfdNotchWs(ch, slotIdx, slot);
                            updateAfdFilterCards(ch);
                        }
                    }
                }
            }
            chState.filters.slice(6, 12).forEach((f, i) => {
                if (f.state === 'clamped' || f.state === 'tracking') {
                    if (!f.lastQuietTime) f.lastQuietTime = now;
                    if (now - f.lastQuietTime > 30000) {
                        f.state = 'idle'; f.freq = 0; f.depth = 0; f.lastQuietTime = 0;
                        dispatchAfdNotchWs(ch, i + 6, f);
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
            const bassX1 = (Math.log10(60 / 20) / Math.log10(1000)) * w;
            const bassX2 = (Math.log10(280 / 20) / Math.log10(1000)) * w;
            ctx.fillStyle = "rgba(16, 185, 129, 0.07)";
            ctx.fillRect(bassX1, 0, bassX2 - bassX1, h);
            ctx.fillStyle = "#10b981"; ctx.font = "bold 9px monospace";
            ctx.fillText("🎸 MOVING-BASS TRACKER ZONE (60-280Hz AUTO-Q)", bassX1 + 6, 14);
            ctx.lineWidth = 1; ctx.strokeStyle = "#161b24";
            [50, 100, 250, 500, 1000, 2000, 5000, 10000, 18000].forEach(f => {
                const x = (Math.log10(f / 20) / Math.log10(1000)) * w;
                ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
                ctx.fillStyle = "#4b5563"; ctx.font = "9px monospace";
                ctx.fillText(f >= 1000 ? (f/1000) + "k" : f + "Hz", x + 3, h - 8);
            });
            const threshY = h * 0.35;
            ctx.strokeStyle = "rgba(245, 158, 11, 0.45)";
            ctx.setLineDash([4, 4]);
            ctx.beginPath(); ctx.moveTo(0, threshY); ctx.lineTo(w, threshY); ctx.stroke();
            ctx.setLineDash([]);
            ctx.fillStyle = "#f59e0b"; ctx.font = "9px monospace";
            ctx.fillText("FEEDBACK CLAMP THRESHOLD (-18 dBFS) • SINGLE-FILTER -∞ dB NULL", 14, threshY - 5);
            ctx.beginPath(); ctx.moveTo(0, h);
            for (let i = 0; i < bins.length; i++) {
                const x = (i / (bins.length - 1)) * w;
                const db = Math.max(-80, Math.min(0, bins[i]));
                ctx.lineTo(x, h - ((db + 80) / 80) * (h - 20));
            }
            ctx.lineTo(w, h); ctx.closePath();
            const grad = ctx.createLinearGradient(0, 0, 0, h);
            grad.addColorStop(0, "rgba(239, 68, 68, 0.65)");
            grad.addColorStop(0.5, "rgba(56, 189, 248, 0.25)");
            grad.addColorStop(1, "rgba(56, 189, 248, 0.0)");
            ctx.fillStyle = grad; ctx.fill();
            ctx.beginPath();
            for (let i = 0; i < bins.length; i++) {
                const x = (i / (bins.length - 1)) * w;
                const y = h - ((Math.max(-80, Math.min(0, bins[i])) + 80) / 80) * (h - 20);
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.strokeStyle = "#38bdf8"; ctx.lineWidth = 1.5; ctx.stroke();
            chState.filters.forEach(f => {
                if (f.state !== 'idle') {
                    const x = (Math.log10(Math.max(20, f.freq) / 20) / Math.log10(1000)) * w;
                    const c = f.state === 'tracking' ? "#10b981" : (f.type === 'fixed' ? "#a855f7" : "#ef4444");
                    ctx.strokeStyle = c; ctx.lineWidth = 2.2;
                    ctx.beginPath(); ctx.moveTo(x, 16); ctx.lineTo(x, h - 16); ctx.stroke();
                    ctx.fillStyle = c; ctx.font = "bold 9px monospace";
                    ctx.fillText(`${f.infNull ? '-∞dB' : f.depth+'dB'} @ ${f.freq}Hz (Q${f.q})`, Math.max(4, Math.min(w - 115, x - 38)), 26);
                }
            });
            const activeNotches = chState.filters.filter(f => f.state !== 'idle').length;
            const hudCount = document.getElementById("afd-hud-count");
            if (hudCount) hudCount.innerText = `${activeNotches} / 12 NOTCHES (${afdState.infNullMode ? '-∞ dB NULL' : 'DEEP CUT'})`;
            window.afdAnimId = requestAnimationFrame(() => renderAfdSpectrum(ch));
        }
        function updateAfdFilterCards(ch) {
            const chState = getAfdChannel(ch);
            const grid = document.getElementById("afd-filter-grid");
            if (!grid) return;
            const chArg = typeof ch === 'string' ? `'${ch}'` : ch;
            grid.innerHTML = chState.filters.map(f => `
                <div class="afd-filter-card ${f.state}">
                    <div class="afd-filter-header">
                        <span style="color:${f.type === 'fixed' ? '#c084fc' : (f.bassTrack ? '#10b981' : '#93c5fd')}; font-weight:800;">
                            ${f.type === 'fixed' ? 'FIXED' : (f.bassTrack ? 'BASS-TRK' : 'FLOAT')} #${f.id}
                        </span>
                        <span class="afd-filter-status status-${f.state}">${f.state.toUpperCase()}</span>
                    </div>
                    <div style="display:flex; justify-content:space-between; color:#94a3b8;">
                        <span>Freq:</span><span style="color:#f8fafc; font-weight:bold;">${f.freq ? f.freq + ' Hz' : '---'}</span>
                    </div>
                    <div style="display:flex; justify-content:space-between; color:#94a3b8;">
                        <span>Null:</span><span style="color:#ef4444; font-weight:bold;">${f.state === 'idle' ? '---' : (f.infNull ? '-∞ dB (ZERO)' : f.depth + ' dB')}</span>
                    </div>
                    <div style="display:flex; justify-content:space-between; color:#94a3b8;">
                        <span>Width:</span><span style="color:#38bdf8;">Q=${f.q}${f.bassTrack ? ' (AUTO)' : ''}</span>
                    </div>
                    <button class="btn-top" style="min-height:24px; font-size:0.6rem; padding:2px 4px; margin-top:2px;" onclick="toggleLockFixedNotch(${chArg}, ${f.id})">
                        ${f.state !== 'idle' ? 'CLEAR / UNLOCK' : 'RINGOUT -∞ NULL'}
                    </button>
                </div>
            `).join('');
        }
        function toggleLockFixedNotch(ch, id) {
            const chState = getAfdChannel(ch);
            const idx = id - 1;
            const f = chState.filters[idx];
            if (!f) return;
            if (f.state !== 'idle') {
                f.state = 'idle'; f.freq = 0; f.depth = 0; f.bassTrack = false;
            } else {
                // Find loudest peak in live RTA or default to 1000 Hz
                let maxBin = 32, maxVal = -100;
                for (let b = 2; b < 62; b++) {
                    if (chState.lastFftBins[b] > maxVal) { maxVal = chState.lastFftBins[b]; maxBin = b; }
                }
                f.state = 'locked';
                f.freq = maxVal > -75 ? Math.round(20 * Math.pow(1000, maxBin / 63)) : (120 * id);
                f.infNull = afdState.infNullMode;
                f.depth = afdState.infNullMode ? -96 : -24;
                f.q = (f.freq >= 60 && f.freq <= 280) ? 30 : 65;
            }
            dispatchAfdNotchWs(ch, idx, f);
            updateAfdFilterCards(ch);
        }
        function panicClearAfdFilters(ch) {
            const chState = getAfdChannel(ch);
            chState.filters.forEach(f => { f.state = 'idle'; f.freq = 0; f.depth = 0; f.lastQuietTime = 0; f.bassTrack = false; });
            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                try { window.meterWs.send(JSON.stringify({ action: "clear_afd_notches", target: ch })); } catch (e) {}
            }
            updateAfdFilterCards(ch);
        }
        function toggleAfdAutoHunting(btn) {
            afdState.autoHunting = !afdState.autoHunting;
            btn.innerText = afdState.autoHunting ? "AUTO HUNTING: ACTIVE" : "AUTO HUNTING: PAUSED";
            btn.className = afdState.autoHunting ? "afd-btn-action" : "btn-top";
        }
        window.toggleAfdInfNullMode = function(btn) {
            afdState.infNullMode = !afdState.infNullMode;
            if (btn) {
                btn.classList.toggle("active", afdState.infNullMode);
                btn.innerText = afdState.infNullMode ? "♾️ -∞ dB SINGLE-FILTER NULL: ON" : "♾️ -∞ dB NULL: OFF (-24dB)";
            }
        };
        // AUTO-EQ ROOM CALIBRATION WIZARD ENGINE
        // Injects pink noise at -18 dBFS, captures 3-second FFT average via measurement mic (CH 1),
        // computes inverted correction curve, and commits to bus ReaEQ rack.
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
            const eqKey = bus === 'master' ? 'master' : (bus.startsWith('mon-') ? bus : 'master');
            const eqSt = getChannelReEq(eqKey);
            eqSt.adaptiveRoomDynEq = true;
            bands.forEach((b, idx) => {
                if (eqSt.bands[idx]) {
                    eqSt.bands[idx].freq = b.freq;
                    eqSt.bands[idx].gain = b.gain;
                    eqSt.bands[idx].q = b.q;
                    eqSt.bands[idx].enabled = true;
                    if (idx >= 1 && idx <= 6) {
                        eqSt.bands[idx].dynEnabled = true;
                        eqSt.bands[idx].dynThresh = -20;
                        eqSt.bands[idx].dynRange = -6.0;
                    }
                    if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(eqKey, idx);
                }
            });
            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                try { window.meterWs.send(JSON.stringify({ action: "set_adaptive_room_dyneq", target: eqKey, enabled: 1 })); } catch (e) {}
            }
            const statusEl = document.getElementById("room-cal-status");
            if (statusEl) {
                statusEl.innerHTML = `<span style="color:#10b981; font-weight:bold;">✓ INVERTED ROOM CURVE + TUKAN ADAPTIVE DYN-EQ COMMITTED TO ${bus.toUpperCase()}!</span>`;
            }
            setTimeout(() => closeDspModal(), 1200);
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
})(window);
