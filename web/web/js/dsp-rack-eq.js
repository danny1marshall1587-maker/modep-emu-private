// dsp-rack-eq.js - Mini ReaEQ Spectrum Renderer & Focused Channel Strip for DSP Rack Page
(function(window) {
    'use strict';
    const REEQ_MIN_FREQ = 20;
    const REEQ_MAX_FREQ = 20000;
    const FILTER_TYPES = window.FILTER_TYPES || ['Bell', 'Low Cut', 'High Cut', 'Low Shelf', 'High Shelf', 'Notch', 'Band Pass', 'All Pass'];
    const REEQ_COLORS = window.REEQ_COLORS || ['#ef4444', '#f97316', '#f59e0b', '#10b981', '#06b6d4', '#3b82f6', '#8b5cf6', '#ec4899'];
    const freqToX = (f, w) => window.freqToX(f, w);
    const xToFreq = (x, w) => window.xToFreq(x, w);
    const dbToY = (db, h, r) => window.dbToY(db, h, r);
    const yToDb = (y, h, r) => window.yToDb ? window.yToDb(y, h, r) : (((h / 2) - y) / (h / 2)) * (r || 18);
    const evalBandMagnitude = (b, f, sr) => window.evalBandMagnitude ? window.evalBandMagnitude(b, f, sr) : 1.0;
    const getChannelReEq = (ch) => window.getChannelReEq(ch);
        function renderMiniReEq(ch) {
            const canvas = document.getElementById("dsp-rack-reeq-mini");
            if (!canvas) return;
            const ctx = canvas.getContext("2d");
            const dpr = window.devicePixelRatio || 1;
            const w = canvas.clientWidth || 700;
            const h = canvas.clientHeight || 220;
            const targetW = Math.round(w * dpr);
            const targetH = Math.round(h * dpr);
            if (canvas.width !== targetW || canvas.height !== targetH) {
                canvas.width = targetW;
                canvas.height = targetH;
            }
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
            const state = getChannelReEq(ch);
            const range = state.dbRange;
            const midY = dbToY(0, h, range);

            // Build state signature for static Grid + EQ Curves caches
            let eqSig = `${ch}_${targetW}_${targetH}_${range}_${state.selectedBand}_${window.eqMidFocusScale ? 1 : 0}`;
            for (let i = 0; i < state.bands.length; i++) {
                const b = state.bands[i];
                eqSig += `|${b.enabled ? 1 : 0},${b.type},${b.freq.toFixed(1)},${b.gain.toFixed(2)},${b.q.toFixed(3)},${b.slope || 1}`;
            }

            if (canvas._eqCacheSig !== eqSig || !canvas._gridLayer || !canvas._curveLayer) {
                canvas._eqCacheSig = eqSig;
                if (!canvas._gridLayer) canvas._gridLayer = document.createElement('canvas');
                if (!canvas._curveLayer) canvas._curveLayer = document.createElement('canvas');
                canvas._gridLayer.width = targetW;
                canvas._gridLayer.height = targetH;
                canvas._curveLayer.width = targetW;
                canvas._curveLayer.height = targetH;

                const gctx = canvas._gridLayer.getContext('2d');
                gctx.setTransform(dpr, 0, 0, dpr, 0, 0);
                gctx.fillStyle = "#0c0e14";
                gctx.fillRect(0, 0, w, h);
                if (window.eqMidFocusScale) {
                    const x150 = freqToX(150, w);
                    const x3k = freqToX(3000, w);
                    gctx.fillStyle = "rgba(16, 185, 129, 0.035)";
                    gctx.fillRect(x150, 0, x3k - x150, h - 16);
                }
                const dbStep = range >= 24 ? 6 : (range >= 18 ? 3 : 2);
                gctx.font = "9px -apple-system, monospace";
                for (let db = -range; db <= range; db += dbStep) {
                    const y = dbToY(db, h, range);
                    gctx.beginPath();
                    gctx.moveTo(0, y);
                    gctx.lineTo(w, y);
                    if (db === 0) {
                        gctx.strokeStyle = "rgba(56, 189, 248, 0.5)";
                        gctx.lineWidth = 1.5;
                    } else {
                        gctx.strokeStyle = "rgba(255, 255, 255, 0.07)";
                        gctx.lineWidth = 1;
                    }
                    gctx.stroke();
                    gctx.fillStyle = db === 0 ? "#38bdf8" : "rgba(255, 255, 255, 0.35)";
                    gctx.textAlign = "right";
                    gctx.fillText((db > 0 ? "+" : "") + db + " dB", w - 6, y - 2);
                }
                const minorGridFreqs = [30, 70, 250, 400, 600, 700, 900, 1500, 2500, 4000, 8000];
                gctx.beginPath();
                minorGridFreqs.forEach(f => {
                    const x = freqToX(f, w);
                    gctx.moveTo(x, 0);
                    gctx.lineTo(x, h - 16);
                });
                gctx.strokeStyle = "rgba(255, 255, 255, 0.06)";
                gctx.lineWidth = 1;
                gctx.stroke();
                const gridFreqs = window.eqMidFocusScale
                    ? [20, 50, 100, 200, 350, 500, 800, 1200, 2000, 3000, 6000, 12000, 20000]
                    : [20, 50, 100, 200, 300, 500, 1000, 2000, 5000, 10000, 20000];
                gridFreqs.forEach(f => {
                    const x = freqToX(f, w);
                    gctx.beginPath();
                    gctx.moveTo(x, 0);
                    gctx.lineTo(x, h - 16);
                    gctx.strokeStyle = (f === 200 || f === 1000 || f === 2000 || f === 3000) ? "rgba(52, 211, 153, 0.15)" : "rgba(255, 255, 255, 0.05)";
                    gctx.lineWidth = 1;
                    gctx.stroke();
                    gctx.fillStyle = (f >= 200 && f <= 3000 && window.eqMidFocusScale) ? "rgba(110, 231, 183, 0.78)" : "rgba(255, 255, 255, 0.4)";
                    gctx.textAlign = "center";
                    const label = f >= 1000 ? (f % 1000 === 0 ? (f / 1000) + "k" : (f / 1000).toFixed(1) + "k") : f + "Hz";
                    gctx.fillText(label, x, h - 4);
                });

                const cctx = canvas._curveLayer.getContext('2d');
                cctx.setTransform(dpr, 0, 0, dpr, 0, 0);
                cctx.clearRect(0, 0, w, h);
                state.bands.forEach((b, idx) => {
                    if (!b.enabled) return;
                    const color = REEQ_COLORS[idx % REEQ_COLORS.length];
                    const isSelected = state.selectedBand === idx;
                    cctx.beginPath();
                    for (let x = 0; x <= w; x += 3) {
                        const freq = xToFreq(x, w);
                        const mag = evalBandMagnitude(b, freq, 48000);
                        const db = 20 * Math.log10(Math.max(0.0001, mag));
                        const y = dbToY(db, h, range);
                        if (x === 0) cctx.moveTo(x, y);
                        else cctx.lineTo(x, y);
                    }
                    cctx.strokeStyle = color;
                    cctx.lineWidth = isSelected ? 1.8 : 1.0;
                    cctx.globalAlpha = isSelected ? 0.85 : 0.35;
                    cctx.stroke();
                    cctx.globalAlpha = 1.0;
                });
                cctx.beginPath();
                for (let x = 0; x <= w; x += 2) {
                    const freq = xToFreq(x, w);
                    let totalMag = 1.0;
                    state.bands.forEach(b => {
                        totalMag *= evalBandMagnitude(b, freq, 48000);
                    });
                    const totalDb = 20 * Math.log10(Math.max(0.0001, totalMag));
                    const y = dbToY(totalDb, h, range);
                    if (x === 0) cctx.moveTo(x, y);
                    else cctx.lineTo(x, y);
                }
                cctx.strokeStyle = "#818cf8";
                cctx.lineWidth = 2.2;
                cctx.stroke();
                cctx.lineTo(w, midY);
                cctx.lineTo(0, midY);
                cctx.fillStyle = "rgba(129, 140, 248, 0.12)";
                cctx.fill();
            }

            // 1. Blit cached background grid
            ctx.save();
            ctx.setTransform(1, 0, 0, 1, 0, 0);
            ctx.drawImage(canvas._gridLayer, 0, 0);
            ctx.restore();

            // 2. Live 64-Band RTA Underlay in Mini Rack EQ (60FPS)
            if (typeof window.drawRtaSpectrum === 'function') {
                window.drawRtaSpectrum(ctx, w, h, 8, 18, true);
            }

            // 3. Blit cached EQ Band + Composite Curves
            ctx.save();
            ctx.setTransform(1, 0, 0, 1, 0, 0);
            ctx.drawImage(canvas._curveLayer, 0, 0);
            ctx.restore();
            // Band Node Points with Q wing lines for selected band
            state.bands.forEach((b, idx) => {
                if (!b.enabled) return;
                const x = freqToX(b.freq, w);
                const y = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8) 
                    ? dbToY(0, h, range) 
                    : dbToY(b.gain, h, range);
                const color = REEQ_COLORS[idx % REEQ_COLORS.length];
                const isSelected = state.selectedBand === idx;
                // Q Wing indicators if band is selected
                if (isSelected && b.type !== 1 && b.type !== 2) {
                    const bwOct = 2 * Math.asinh(1 / (2 * Math.max(0.1, b.q))) / Math.LN2;
                    const leftF = Math.max(REEQ_MIN_FREQ, b.freq * Math.pow(2, -bwOct / 2));
                    const rightF = Math.min(REEQ_MAX_FREQ, b.freq * Math.pow(2, bwOct / 2));
                    const lx = freqToX(leftF, w);
                    const rx = freqToX(rightF, w);
                    ctx.beginPath();
                    ctx.moveTo(lx, y);
                    ctx.lineTo(rx, y);
                    ctx.strokeStyle = color;
                    ctx.lineWidth = 1.5;
                    ctx.stroke();
                    [lx, rx].forEach(px => {
                        ctx.beginPath();
                        ctx.arc(px, y, 3.5, 0, Math.PI * 2);
                        ctx.fillStyle = "#ffffff";
                        ctx.fill();
                    });
                }
                // Node Body
                ctx.beginPath();
                ctx.arc(x, y, isSelected ? 12 : 8, 0, Math.PI * 2);
                ctx.fillStyle = color;
                ctx.fill();
                ctx.strokeStyle = "#ffffff";
                ctx.lineWidth = isSelected ? 2 : 1;
                ctx.stroke();
                ctx.fillStyle = "#ffffff";
                ctx.font = `bold ${isSelected ? '10px' : '8px'} sans-serif`;
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";
                ctx.fillText(idx + 1, x, y);
            });
            // Interactive Mini Touch / Click Drag + 2-Finger Pinch-to-Q Handling
            if (!canvas.dataset.bound) {
                canvas.dataset.bound = "true";
                canvas.style.touchAction = "none";
                let activeMiniNode = -1, miniPinchDist = 0, miniPinchQ = 1.0;
                const miniPointers = new Map();
                function getMiniPos(e) {
                    const r = canvas.getBoundingClientRect();
                    const cx = e.touches ? e.touches[0].clientX : e.clientX;
                    const cy = e.touches ? e.touches[0].clientY : e.clientY;
                    return { x: cx - r.left, y: cy - r.top };
                }
                function applyMiniQ(ratio) {
                    const chIdx = window.dspRackCurrentChannel;
                    const st = getChannelReEq(chIdx);
                    const b = st.bands[st.selectedBand];
                    if (!b) return;
                    b.q = Math.max(0.1, Math.min(18.0, parseFloat((miniPinchQ * ratio).toFixed(2))));
                    if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(chIdx, st.selectedBand);
                    updateMiniLabels(st, st.selectedBand);
                    updateQuickNodeCardUI(st.selectedBand);
                    renderMiniReEq(chIdx);
                }
                function onDown(e) {
                    e.preventDefault();
                    if (e.pointerId !== undefined) miniPointers.set(e.pointerId, { x: e.clientX, y: e.clientY });
                    if (miniPointers.size === 2) {
                        activeMiniNode = -1;
                        const pts = Array.from(miniPointers.values());
                        miniPinchDist = Math.hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
                        const st = getChannelReEq(window.dspRackCurrentChannel);
                        miniPinchQ = st.bands[st.selectedBand]?.q || 1.0;
                        return;
                    }
                    const pos = getMiniPos(e);
                    const st = getChannelReEq(window.dspRackCurrentChannel);
                    const curW = canvas.clientWidth || 700, curH = canvas.clientHeight || 220;
                    let hit = -1, minD = 36;
                    st.bands.forEach((b, i) => {
                        if (!b.enabled) return;
                        const nx = freqToX(b.freq, curW);
                        const ny = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8) ? dbToY(0, curH, st.dbRange) : dbToY(b.gain, curH, st.dbRange);
                        const d = Math.hypot(pos.x - nx, pos.y - ny);
                        if (d < minD) { minD = d; hit = i; }
                    });
                    if (hit !== -1) {
                        activeMiniNode = hit; st.selectedBand = hit;
                        updateMiniLabels(st, hit); updateQuickNodeCardUI(hit); renderMiniReEq(window.dspRackCurrentChannel);
                    }
                }
                function onMove(e) {
                    if (e.pointerId !== undefined && miniPointers.has(e.pointerId)) {
                        miniPointers.set(e.pointerId, { x: e.clientX, y: e.clientY });
                    }
                    if (miniPointers.size === 2 && miniPinchDist > 8) {
                        e.preventDefault();
                        const pts = Array.from(miniPointers.values());
                        const dist = Math.hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
                        applyMiniQ(dist / miniPinchDist);
                        return;
                    }
                    if (activeMiniNode === -1) return;
                    e.preventDefault();
                    const pos = getMiniPos(e);
                    const chIdx = window.dspRackCurrentChannel;
                    const st = getChannelReEq(chIdx);
                    const b = st.bands[activeMiniNode];
                    if (!b) return;
                    const curW = canvas.clientWidth || 700, curH = canvas.clientHeight || 220;
                    b.freq = Math.max(20, Math.min(20000, Math.round(xToFreq(Math.max(0, Math.min(curW, pos.x)), curW))));
                    if (b.type !== 1 && b.type !== 2 && b.type !== 5 && b.type !== 6 && b.type !== 7 && b.type !== 8) {
                        b.gain = parseFloat(yToDb(Math.max(12, Math.min(curH - 22, pos.y)), curH, st.dbRange).toFixed(1));
                    }
                    if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(chIdx, activeMiniNode);
                    updateMiniLabels(st, activeMiniNode); updateQuickNodeCardUI(activeMiniNode); renderMiniReEq(chIdx);
                }
                function onUp(e) {
                    if (e && e.pointerId !== undefined) miniPointers.delete(e.pointerId);
                    else miniPointers.clear();
                    if (miniPointers.size < 2) miniPinchDist = 0;
                    if (miniPointers.size === 0) activeMiniNode = -1;
                }
                canvas.addEventListener("pointerdown", onDown);
                window.addEventListener("pointermove", onMove);
                window.addEventListener("pointerup", onUp);
                window.addEventListener("pointercancel", onUp);
            }
            const activeB = state.bands[state.selectedBand];
            if (activeB) updateMiniLabels(state, state.selectedBand);
        }
        function selectRackEqBand(ch, idx) {
            const st = getChannelReEq(ch);
            st.selectedBand = idx;
            updateQuickNodeCardUI(idx);
            updateMiniLabels(st, idx);
            renderMiniReEq(ch);
        }
        function updateQuickNodeCardUI(selectedIdx) {
            const st = getChannelReEq(window.dspRackCurrentChannel);
            st.bands.forEach((b, idx) => {
                const nodeEl = document.getElementById("rack-quick-node-" + idx);
                if (nodeEl) nodeEl.classList.toggle("active", idx === selectedIdx);
                const fEl = document.getElementById("rack-node-freq-" + idx);
                if (fEl) fEl.innerText = b.freq >= 1000 ? (b.freq / 1000).toFixed(1) + 'k' : b.freq + 'Hz';
                const gEl = document.getElementById("rack-node-gain-" + idx);
                if (gEl) {
                    gEl.innerText = (b.gain >= 0 ? '+' : '') + b.gain + 'dB';
                    gEl.style.color = b.gain >= 0 ? '#34d399' : '#f87171';
                }
            });
        }
        function updateMiniLabels(st, idx) {
            const b = st.bands[idx];
            if (!b) return;
            const bLbl = document.getElementById("mini-reeq-band-lbl");
            const vLbl = document.getElementById("mini-reeq-val-lbl");
            if (bLbl) bLbl.innerText = `B${idx+1}: ${FILTER_TYPES[b.type]}`;
            if (vLbl) {
                const fStr = b.freq >= 1000 ? (b.freq/1000).toFixed(1)+'k' : b.freq+'Hz';
                const gStr = (b.gain >= 0 ? '+' : '') + b.gain.toFixed(1) + 'dB';
                vLbl.innerText = `${fStr} ${gStr} (Q ${b.q.toFixed(1)})`;
            }
        }
        function closeDspRackPage() {
            document.getElementById('dsp-rack-page').classList.remove('open');
        }
        function handleRackGainChange(val) {
            const ch = window.dspRackCurrentChannel;
            let fVal = parseFloat(val);
            if (window.touchGovernor) {
                fVal = window.touchGovernor.process(`gain-${ch}`, fVal);
            }
            if (window.getChannelRackState) window.getChannelRackState(ch).gain = fVal;
            handleInputGainChange(ch, fVal);
            const txt = (fVal >= 0 ? '+' : '') + fVal.toFixed(1) + ' dB';
            const topSlider = document.getElementById('dsp-rack-gain-slider');
            if (topSlider && parseFloat(topSlider.value) !== fVal) topSlider.value = fVal;
            const valEl = document.getElementById('dsp-rack-gain-val');
            if (valEl) valEl.innerText = txt;
            const rowSlider = document.getElementById('rack-row-gain-slider-' + ch);
            if (rowSlider && parseFloat(rowSlider.value) !== fVal) rowSlider.value = fVal;
            const rowVal = document.getElementById('rack-row-gain-val-' + ch);
            if (rowVal) rowVal.innerText = txt;
            /* Sync focused channel fader cap & dB readout when REGAIN counter-trims the fader */
            const currentPct = window.faderValues && window.faderValues.main ? (window.faderValues.main[ch] ?? 75) : 75;
            const fCap = document.getElementById('fader-cap-focused-rack');
            if (fCap) fCap.style.bottom = `calc(38px + (100% - 52px) * ${currentPct / 100})`;
            const fDb = document.getElementById('fader-val-focused-rack');
            if (fDb && typeof window.toDbStr === 'function') fDb.innerText = window.toDbStr(currentPct);
        }
        function toggleRackRegain() {
            const ch = window.dspRackCurrentChannel;
            toggleRegain(ch);
            ['dsp-rack-regain-btn', 'rack-row-regain-btn-' + ch].forEach(id => {
                const btn = document.getElementById(id);
                if (btn) {
                    btn.classList.toggle('active', channelRegainActive[ch]);
                    btn.innerText = channelRegainActive[ch] ? 'REGAIN ON' : 'REGAIN OFF';
                }
            });
        }
        // Render the single focused channel strip on the left of the DSP Rack page
        function updateFocusedChannelStrip(ch) {
            const container = document.getElementById('dsp-rack-focused-strip');
            if (!container) return;
            const name = (window.customChannelNames && window.customChannelNames['ch-' + ch]) || (window.channelNames && window.channelNames[ch]) || (window.BUS_CONFIG && window.BUS_CONFIG.channels[ch]) || ('Channel ' + (ch + 1));
            const pct = faderValues.main[ch] ?? 75;
            const dbStr = toDbStr(pct);
            const isMuted = document.getElementById('mute-ch-' + ch)?.classList.contains('active');
            const isSolo = document.getElementById('solo-ch-' + ch)?.classList.contains('active');
            const isAmm = document.getElementById('amm-ch-' + ch)?.classList.contains('active');
            const color = customChannelColors['ch-' + ch] || '#38bdf8';
            const panVal = (window.channelPans && window.channelPans[ch]) ?? 0;
            const panLbl = typeof window.formatPanLabel === 'function' ? window.formatPanLabel(panVal) : 'C';
            const hpfActive = !!(window.channelHpf80 && window.channelHpf80[ch]);
            const clipLatched = !!(window.clipHoldState && window.clipHoldState[ch]);
            container.innerHTML = `
                <div class="focused-strip-header">
                    <div class="focused-strip-badge" style="background:${color}; color:${(color==='#e2e8f0'||color==='#eab308'||color==='#84cc16')?'#000':'#fff'};">IN ${ch + 1} &bull; CH ${ch + 1}</div>
                    <div class="focused-strip-name" title="${name}">${name}</div>
                </div>
                <button type="button" class="btn-ch-setup-open" id="btn-ch-setup-focused-${ch}" style="margin:2px 0 4px 0; min-height:30px;" onclick="openChannelSetupModal(${ch})" title="Open Input, Pan, 80Hz HPF &amp; Stereo Link Sub-Menu">🎛️ IN ${ch + 1} &bull; PAN ${panLbl}</button>
                <div class="ch-pan-hpf-row" style="display:none;">
                    <button class="btn-ch-hpf ${hpfActive ? 'active' : ''}" id="btn-hpf-focused-rack" onclick="toggleChannelHpf80(${ch})" title="One-Tap 80Hz High-Pass Filter">Ø80</button>
                    <input type="range" class="ch-pan-slider" id="pan-slider-focused-rack" min="-100" max="100" step="1" value="${panVal}" oninput="setChannelPan(${ch}, this.value)" ondblclick="resetChannelPan(${ch})" title="Channel Pan (Double-tap to Center)">
                    <span class="ch-pan-val ${panVal !== 0 ? 'panned-lr' : ''}" id="pan-val-focused-rack" onclick="resetChannelPan(${ch})" title="Tap to reset Pan to Center">${panLbl}</span>
                </div>
                <div class="fader-section" style="margin: 6px 0;">
                    <div class="meter-container" style="width: 12px;" title="Channel Peak Level (-18dB Yellow, -6dB Orange, 0dB Red Clipping)">
                        <div class="clip-hold-led ${clipLatched ? 'latched' : ''}" id="clip-led-focused-rack" onclick="clearClipHold(${ch}, event)" title="Sticky Peak Clip Hold (Tap to Clear)">CLIP</div>
                        <div class="meter-bar" id="meter-focused-rack"></div>
                        <div class="meter-ticks">
                            <div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div>
                        </div>
                    </div>
                    <!-- Focused Strip Gain Reduction Meter -->
                    <div class="gr-meter-container" title="Gain Reduction: 0-6dB Green, 6-12dB Orange, >12dB Red">
                        <div class="gr-meter-bar" id="gr-meter-focused-rack"></div>
                        <span class="gr-meter-scale-title">GR</span>
                    </div>
                    <div class="fader-track" id="fader-track-focused-rack" data-target="${ch}" data-channel="${ch}">
                        <div class="fader-scale">
                            <div class="scale-mark"><span>+10</span></div><div class="scale-mark"><span>+5</span></div>
                            <div class="scale-mark zero"><span>0</span></div><div class="scale-mark"><span>-5</span></div>
                            <div class="scale-mark"><span>-10</span></div><div class="scale-mark"><span>-20</span></div>
                            <div class="scale-mark"><span>-30</span></div><div class="scale-mark"><span>-40</span></div>
                            <div class="scale-mark"><span>-&infin;</span></div>
                        </div>
                        <div class="fader-slot-bg"></div>
                        <div class="fader-slot-centerline"></div>
                        <div class="fader-cap" id="fader-cap-focused-rack" data-target="${ch}" data-channel="${ch}" style="bottom: calc(38px + (100% - 52px) * ${pct / 100});"></div>
                        <div class="fader-db-display" id="fader-val-focused-rack">${dbStr}</div>
                    </div>
                </div>
                <div class="btn-group" style="margin-top:auto;">
                    <button class="btn-strip btn-amm ${isAmm ? 'active' : ''}" id="amm-focused-rack" onclick="toggleChAmm(${ch})">AMM DUGAN</button>
                    <button class="btn-strip btn-solo ${isSolo ? 'active' : ''}" id="solo-focused-rack" onclick="toggleChSolo(${ch})">SOLO</button>
                    <button class="btn-strip btn-mute ${isMuted ? 'active' : ''}" id="mute-focused-rack" onclick="toggleChMute(${ch})">MUTE</button>
                </div>`;
        }
        let activeFocusedF = null;
        function startFocusedFaderDrag(target, evt) {
            const cap = document.getElementById('fader-cap-focused-rack');
            const track = document.getElementById('fader-track-focused-rack');
            if (!cap || !track || !(evt.target === cap || cap.contains(evt.target))) return;
            evt.preventDefault();
            const capRect = cap.getBoundingClientRect();
            const grabOffsetY = evt.clientY - (capRect.top + capRect.height / 2);
            cap.classList.add('dragging');
            activeFocusedF = { target, track, cap, grabOffsetY };
            if (evt.pointerId !== undefined) track.setPointerCapture(evt.pointerId);
        }
        window.addEventListener('pointermove', evt => {
            if (!activeFocusedF) return;
            const rect = activeFocusedF.track.getBoundingClientRect();
            const slotBottom = rect.bottom - 38, travel = slotBottom - (rect.top + 14);
            if (travel <= 0) return;
            let pct = Math.max(0, Math.min(100, ((slotBottom - (evt.clientY - (activeFocusedF.grabOffsetY || 0))) / travel) * 100));
            if (window.touchGovernor) pct = window.touchGovernor.process(`fader-${activeFocusedF.target}`, pct);
            setFaderPosition(activeFocusedF.target, pct, false);
            const currentPct = faderValues.main[activeFocusedF.target] ?? pct;
            if (activeFocusedF.cap) activeFocusedF.cap.style.bottom = `calc(38px + (100% - 52px) * ${currentPct / 100})`;
            const dbEl = document.getElementById('fader-val-focused-rack');
            if (dbEl) dbEl.innerText = toDbStr(currentPct);
        });
        const endFocusedFader = () => { if (activeFocusedF) { activeFocusedF.cap.classList.remove('dragging'); activeFocusedF = null; } };
        window.addEventListener('pointerup', endFocusedFader);
        window.addEventListener('pointercancel', endFocusedFader);
    Object.assign(window, { renderMiniReEq, selectRackEqBand, updateQuickNodeCardUI, updateMiniLabels, closeDspRackPage, handleRackGainChange, toggleRackRegain, updateFocusedChannelStrip, startFocusedFaderDrag });
})(window);
