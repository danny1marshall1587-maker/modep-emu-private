// dsp-rack.js - Core Desk Strip State, Regain Stage, Gate, ReaEQ, Comp & Loades Orchestrator
(function(window) {
    'use strict';

    let currentRackChannel = 0;
    const channelState = {};

    function dispatchDspParam(ch, param, value) {
        const ws = window.meterWs;
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'dsp_param', channel: ch, param: param, value: value }));
        }
    }

    function getChannelRackState(ch) {
        if (!channelState[ch]) {
            const compSt = window.getCompState ? window.getCompState(ch) : null;
            channelState[ch] = {
                preamp1176: compSt ? compSt.preamp1176 : { enabled: true, drive: 6.0, trim: 0.0, clipMode: 'soft', clipping: false },
                gain: (window.channelInputGains && window.channelInputGains[ch]) ?? 0.0,
                phaseInvert: false,
                dspChainOrder: ['preamp1176', 'input', 'denoiser', 'reeq', 'comp', 'deesser', 'modep'],
                denoiser: {
                    enabled: true,
                    threshold: -45,
                    reduction: -18,
                    sensitivity: 0.0,
                    reductionPct: 85.0,
                    lowCut: true,
                    listen: false,
                    bands: [-60, -60, -60, -60, -60, -60, -60, -60, -60, -60],
                    state: 'open'
                },
                compModel: compSt ? compSt.compModel : 'fetish76', // 'fetish76' | 'lala' | 'dual'
                compParams: {
                    fetish76: compSt ? compSt.fetish76 : { input: 30, output: 18, attack: 3, release: 3, ratio: '4' },
                    lala: compSt ? compSt.lala : { peakReduction: 40, gain: 40, mode: 'compress' }
                },
                deesser: {
                    enabled: true,
                    freq1: 4800,
                    q1: 2.8,
                    thresh1: -24,
                    max1: 12,
                    band2Enabled: true,
                    freq2: 9500,
                    q2: 2.2,
                    thresh2: -26,
                    max2: 10,
                    release: 45,
                    listenMode: 0 // 0=Off, 1=Solo B1+B2, 2=Solo B1, 3=Solo B2
                },
                directTap: 'pre'
            };
        }
        if (channelState[ch].deesser && channelState[ch].deesser.freq1 === undefined) {
            channelState[ch].deesser = {
                enabled: true,
                freq1: 4800,
                q1: 2.8,
                thresh1: channelState[ch].deesser.threshold ?? -24,
                max1: 12,
                band2Enabled: true,
                freq2: 9500,
                q2: 2.2,
                thresh2: -26,
                max2: 10,
                release: channelState[ch].deesser.release ?? 45,
                listenMode: 0
            };
        }
        if (!channelState[ch].denoiser.bands) {
            channelState[ch].denoiser.bands = [-60, -60, -60, -60, -60, -60, -60, -60, -60, -60];
            channelState[ch].denoiser.sensitivity = channelState[ch].denoiser.sensitivity ?? 0.0;
            channelState[ch].denoiser.reductionPct = channelState[ch].denoiser.reductionPct ?? 85.0;
            channelState[ch].denoiser.lowCut = channelState[ch].denoiser.lowCut ?? true;
            channelState[ch].denoiser.listen = channelState[ch].denoiser.listen ?? false;
        }
        return channelState[ch];
    }

    const _denoiseLearnTimers = {};
    function triggerDenoiseLearn(ch) {
        const c = (ch !== undefined && ch !== null) ? Number(ch) : (window.dspRackCurrentChannel || 0);
        ['btn-learn-noise-' + c, 'modal-btn-learn-noise-' + c].forEach(id => {
            const btn = document.getElementById(id);
            if (btn) {
                btn.classList.add('learning');
                btn.innerText = '⏺ ANALYZING NOISE...';
            }
        });
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({ action: 'denoise_learn', channel: c, target: c }));
        }
        if (_denoiseLearnTimers[c]) clearTimeout(_denoiseLearnTimers[c]);
        _denoiseLearnTimers[c] = setTimeout(() => {
            _denoiseLearnTimers[c] = null;
            const b1 = document.getElementById('btn-learn-noise-' + c);
            const b2 = document.getElementById('modal-btn-learn-noise-' + c);
            if ((b1 && b1.classList.contains('learning')) || (b2 && b2.classList.contains('learning'))) {
                const live = Array.isArray(window.liveDenoiseBands) ? window.liveDenoiseBands : null;
                const fallbackProfile = [-48, -50, -54, -56, -58, -58, -56, -54, -52, -50];
                const learned = fallbackProfile.map((defDb, idx) => {
                    if (live && live[idx] > 0.02) {
                        return Math.max(-85, Math.min(-38, Math.round((live[idx] * 90) - 90 + 1.5)));
                    }
                    return defDb;
                });
                const avg = learned.reduce((a, b) => a + b, 0) / 10.0;
                window.handleDenoiseLearned(c, avg, undefined, learned);
            }
        }, 1150);
    }

    window.handleDenoiseLearned = function(ch, threshold, reduction, thresholdsArray) {
        const c = (ch !== undefined && ch !== null && !isNaN(Number(ch))) ? Number(ch) : (window.currentModalChannel ?? window.dspRackCurrentChannel ?? 0);
        if (_denoiseLearnTimers[c]) {
            clearTimeout(_denoiseLearnTimers[c]);
            _denoiseLearnTimers[c] = null;
        }
        const st = getChannelRackState(c);
        if (threshold !== undefined && !isNaN(parseFloat(threshold))) {
            st.denoiser.threshold = parseFloat(threshold);
        }
        if (reduction !== undefined && !isNaN(parseFloat(reduction))) {
            st.denoiser.reduction = parseFloat(reduction);
        }
        if (Array.isArray(thresholdsArray) && thresholdsArray.length === 10) {
            st.denoiser.bands = thresholdsArray.map(v => parseFloat(v));
            st.denoiser.bands.forEach((bVal, idx) => {
                const sl = document.getElementById(`cyber-denoise-bslider-${c}-${idx}`);
                if (sl) sl.value = Math.round(bVal);
                const lbl = document.getElementById(`cyber-denoise-bval-${c}-${idx}`);
                if (lbl) lbl.innerText = `${Math.round(bVal)}dB`;
            });
            if (typeof window.drawCyberDenoiserCanvas === 'function') {
                window.drawCyberDenoiserCanvas(c);
            }
        }
        const rackBtn = document.getElementById('btn-learn-noise-' + c);
        if (rackBtn) {
            rackBtn.classList.remove('learning');
            rackBtn.innerText = '✓ FLOOR LEARNED';
            setTimeout(() => { if (rackBtn) rackBtn.innerText = '⏺ LEARN NOISE'; }, 1800);
        }
        const modalBtn = document.getElementById('modal-btn-learn-noise-' + c);
        if (modalBtn) {
            modalBtn.classList.remove('learning');
            modalBtn.innerText = '✓ 10-BAND FLOOR LEARNED';
            setTimeout(() => { if (modalBtn) modalBtn.innerText = '⏺ LEARN NOISE FLOOR'; }, 1800);
        }
        ['denoise-thresh-slider-' + c, 'modal-denoise-thresh-' + c].forEach(id => {
            const el = document.getElementById(id);
            if (el) el.value = st.denoiser.threshold;
        });
        ['denoise-thresh-val-' + c, 'modal-denoise-thresh-val-' + c].forEach(id => {
            const el = document.getElementById(id);
            if (el) el.innerText = st.denoiser.threshold.toFixed(1) + ' dB';
        });
        ['denoise-reduc-slider-' + c, 'modal-denoise-reduc-' + c].forEach(id => {
            const el = document.getElementById(id);
            if (el) el.value = st.denoiser.reduction;
        });
        ['denoise-reduc-val-' + c, 'modal-denoise-reduc-val-' + c].forEach(id => {
            const el = document.getElementById(id);
            if (el) el.innerText = st.denoiser.reduction.toFixed(1) + ' dB';
        });
    };

    function setChannelCompModel(ch, model) {
        const st = getChannelRackState(ch);
        st.compModel = model;
        if (window.getCompState) window.getCompState(ch).compModel = model;
        dispatchDspParam(ch, 'compModel', model);
        if (document.getElementById('dsp-rack-page')?.classList.contains('open')) {
            window.showDspRackPage(ch);
        } else if (document.getElementById('channel-rack-overlay')?.classList.contains('open')) {
            openChannelRackModal(ch);
        }
    }

    function setChannelDirectTap(ch, tap) {
        const st = getChannelRackState(ch);
        st.directTap = tap;
        dispatchDspParam(ch, 'directTap', tap);
        if (document.getElementById('dsp-rack-page')?.classList.contains('open')) {
            window.showDspRackPage(ch);
        } else if (document.getElementById('channel-rack-overlay')?.classList.contains('open')) {
            openChannelRackModal(ch);
        }
    }

    function togglePhaseInvert(ch) {
        const st = getChannelRackState(ch);
        st.phaseInvert = !st.phaseInvert;
        dispatchDspParam(ch, 'phaseInvert', st.phaseInvert);
        ['rack-phase-btn-', 'rack-modal-phase-btn-'].forEach(p => {
            const b = document.getElementById(p + ch);
            if (b) {
                b.classList.toggle('active', st.phaseInvert);
                b.innerText = st.phaseInvert ? 'Ø INVERTED' : 'Ø NORMAL';
            }
        });
    }

    function openChannelRackModal(ch) {
        currentRackChannel = ch;
        const state = getChannelRackState(ch);
        const overlay = document.getElementById("channel-rack-overlay");
        const title = document.getElementById("channel-rack-modal-title");
        const grid = document.getElementById("channel-rack-grid");
        const chName = (window.customChannelNames && window.customChannelNames['ch-' + ch]) || (window.channelNames && window.channelNames[ch]) || `Channel ${ch + 1}`;
        if (title) title.innerHTML = `🎛️ IN ${ch + 1} &bull; ${chName} &bull; Channel FX Strip Rack`;
        if (grid) {
            grid.innerHTML = window.renderPreampStageRow(ch, state, true) + buildCoreRackRowsHtml(ch, state, true);
        }
        if (overlay) overlay.classList.add("open");
        if (window.updatePreampClipIndicator) window.updatePreampClipIndicator(ch);
    }

    function closeChannelRackModal() {
        const overlay = document.getElementById("channel-rack-overlay");
        if (overlay) overlay.classList.remove("open");
    }

    function buildCoreRackRowsHtml(ch, state, isModal) {
        const gain = (window.channelInputGains && window.channelInputGains[ch]) ?? 0;
        const regainActive = (window.channelRegainActive && window.channelRegainActive[ch]) ?? true;
        const REEQ_COLORS = window.REEQ_COLORS || ['#ef4444', '#f97316', '#f59e0b', '#10b981', '#06b6d4', '#3b82f6', '#8b5cf6', '#ec4899'];
        const eqSt = window.getChannelReEq ? window.getChannelReEq(ch) : { selectedBand: 0, bands: [] };
        const stage5Html = window.renderRackCompressorRow ? window.renderRackCompressorRow(ch, state) : '';
        const rtaMode = (window.rtaViewMode && window.rtaViewMode !== 'line') ? window.rtaViewMode : 'peaks';
        const midFocus = (window.eqMidFocusScale !== false);

        const slotsArr = (window.channelLv2Slots && window.channelLv2Slots[ch]) ? window.channelLv2Slots[ch] : [null, null, null, null];
        const activeSlotsCount = slotsArr.filter(s => s && s.uri).length;
        const slotPillsHtml = [0, 1, 2, 3].map(sIdx => {
            const sl = slotsArr[sIdx];
            if (!sl || !sl.uri) {
                return `<button class="tap-mode-btn" style="min-height:36px; padding:0 8px; border-style:dashed; color:#94a3b8; white-space:nowrap;" onclick="window.openFullScreenSlotsPage(${ch}, ${sIdx})">S${sIdx+1}: +FX</button>`;
            }
            const col = sl.bypass ? '#64748b' : '#10b981';
            return `<button class="tap-mode-btn active" style="min-height:36px; padding:0 9px; background:${sl.bypass ? '#1e293b' : '#064e3b'}; border-color:${col}; color:#ecfdf5; font-weight:900; white-space:nowrap;" onclick="window.openSlotModgui(${ch}, ${sIdx})">S${sIdx+1}: ${sl.name.substring(0, 10)}</button>`;
        }).join('');

        return `
            <!-- STAGE 2: Digital Pre-Gain, Automatic REGAIN & Phase Invert -->
            <div class="rack-row" data-rack-group="input_fx">
                <div class="rack-header-area">
                    <span class="rack-num">2</span>
                    <div>
                        <div class="rack-name">🎚️ Digital Pre-Gain &amp; Automatic REGAIN</div>
                        <div class="rack-sub">Post-1176 Trim &bull; Constant-Level Fader Compensation &bull; Polarity</div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Pre-Gain</span><input type="range" class="rack-mini-slider" id="rack-row-gain-slider-${ch}" style="width:95px;" min="-18" max="24" step="0.5" value="${gain}" oninput="handleRackGainChange(this.value)">
                        <span class="rack-mini-val" id="rack-row-gain-val-${ch}">${gain >= 0 ? '+' : ''}${parseFloat(gain).toFixed(1)} dB</span>
                    </div>
                    <button class="tap-mode-btn ${state.phaseInvert ? 'active' : ''}" id="${isModal ? 'rack-modal-phase-btn-' : 'rack-phase-btn-'}${ch}" style="min-height:38px; font-weight:900;" onclick="togglePhaseInvert(${ch})">
                        ${state.phaseInvert ? 'Ø INVERTED' : 'Ø NORMAL'}
                    </button>
                    <button class="btn-top ${regainActive ? 'active' : ''}" id="rack-row-regain-btn-${ch}" onclick="toggleRackRegain()">
                        ${regainActive ? 'REGAIN ON' : 'REGAIN OFF'}
                    </button>
                </div>
                <div class="rack-actions">
                    <button class="btn-rack-open" style="background:#0f172a; border-color:#38bdf8; color:#38bdf8;" onclick="openChannelSetupModal(${ch});">🎛️ IN / PAN / HPF &rarr;</button>
                </div>
            </div>
            <!-- STAGE 3: 4x Channel LV2/MOD-GUI Effect Insert Slots (Precedes Denoiser so noisy hardware emulations get cleaned!) -->
            <div class="rack-row" data-rack-group="input_fx" style="background:linear-gradient(90deg, #061814 0%, #09221c 100%); border-color:#10b981;">
                <div class="rack-header-area" style="min-width:0; flex:1 1 auto;">
                    <span class="rack-num" style="color:#34d399; border-color:#10b981;">3</span>
                    <div style="min-width:0;">
                        <div class="rack-name" style="color:#ecfdf5;">🎸 4-Slot FX Inserts (${activeSlotsCount}/4 Active)</div>
                        <div class="rack-sub" style="color:#6ee7b7;">Pre-Denoiser Loop &bull; Cyber Amps, NAM, IR &amp; Pedals</div>
                    </div>
                </div>
                <div class="rack-inline-controls" style="display:flex; gap:5px; flex-wrap:nowrap; flex-shrink:0;">
                    ${slotPillsHtml}
                </div>
                <div class="rack-actions" style="display:flex; gap:6px; flex-shrink:0;">
                    <button class="btn-rack-open" style="background:#059669; border-color:#34d399; color:#ffffff; min-height:40px; padding:6px 12px; font-weight:900;" onclick="window.openFullScreenSlotsPage(${ch});">🎛️ 4 SLOTS &rarr;</button>
                    <button class="btn-rack-open" style="background:#0f172a; border-color:#38bdf8; color:#38bdf8; min-height:40px; padding:6px 10px;" onclick="openModepCanvasModal(${ch});">CANVAS</button>
                </div>
            </div>
            <!-- STAGE 4: Cyber-Denoiser PRO (10-Band Post-Insert Subtractive Phase-Cancellation Engine) -->
            <div class="rack-row" data-rack-group="input_fx" style="background:linear-gradient(90deg, #0d0a1a 0%, #150d2a 100%); border-color:#4c1d95;">
                <div class="rack-header-area">
                    <span class="rack-num" style="color:#d946ef; border-color:#a855f7;">4</span>
                    <div>
                        <div class="rack-name" style="display:flex; align-items:center; color:#f5d0fe;">
                            <span class="gate-led ${state.denoiser.enabled ? 'open' : 'closed'}" id="gate-led-${ch}"></span>
                            <span>🛡️ Cyber-Denoiser PRO (Post-FX 10-Band Spectral)</span>
                        </div>
                        <div class="rack-sub" style="color:#c084fc;">Post-Slot Amp/Pedal Hiss &amp; Floor Cleaner &bull; 10-Band Phase-Cancellation &bull; 40Hz Cut</div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <button class="tap-mode-btn ${state.denoiser.enabled ? 'active' : ''}" id="rack-denoise-en-${ch}" style="min-height:36px; padding:0 12px;" onclick="var d=window.getChannelRackState(${ch}).denoiser; d.enabled=!d.enabled; dispatchDspParam(${ch},'denoiseBypass',!d.enabled); this.classList.toggle('active',d.enabled); this.innerText=d.enabled?'ACTIVE':'BYPASS';">
                        ${state.denoiser.enabled ? 'ACTIVE' : 'BYPASS'}
                    </button>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Sensitivity</span><input type="range" class="rack-mini-slider" id="denoise-thresh-slider-${ch}" style="width:90px; accent-color:#a855f7;" min="-60" max="0" step="1" value="${state.denoiser.threshold}" oninput="var v=parseFloat(this.value); var d=window.getChannelRackState(${ch}).denoiser; d.threshold=v; d.sensitivity=Math.max(-20,Math.min(20,(v+45)*0.55)); dispatchDspParam(${ch},'denoiseThresh',v); document.getElementById('denoise-thresh-val-${ch}').innerText=v+' dB'">
                        <span class="rack-mini-val" id="denoise-thresh-val-${ch}">${state.denoiser.threshold} dB</span>
                    </div>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Reduction</span><input type="range" class="rack-mini-slider" id="denoise-reduc-slider-${ch}" style="width:90px; accent-color:#00f2ff;" min="-40" max="0" step="1" value="${state.denoiser.reduction}" oninput="var v=parseFloat(this.value); var d=window.getChannelRackState(${ch}).denoiser; d.reduction=v; d.reductionPct=Math.round((-v/40)*100); dispatchDspParam(${ch},'denoiseReduc',v); document.getElementById('denoise-reduc-val-${ch}').innerText=d.reductionPct+'% ('+v+'dB)'">
                        <span class="rack-mini-val" id="denoise-reduc-val-${ch}">${Math.round(state.denoiser.reductionPct ?? 85)}% (${state.denoiser.reduction}dB)</span>
                    </div>
                    <button class="btn-learn-noise" id="btn-learn-noise-${ch}" onclick="triggerDenoiseLearn(${ch})">⏺ LEARN NOISE</button>
                </div>
                <div class="rack-actions">
                    <button class="btn-rack-open" style="background:#581c87; border-color:#a855f7; color:#f3e8ff;" onclick="openDspModal('denoiser', ${ch});">10-BAND PRO &rarr;</button>
                </div>
            </div>
            <!-- STAGE 5: ReaEQ JSFX Graphic Parametric Equaliser -->
            <div class="rack-row rack-eq-card" data-rack-group="eq">
                <div class="rack-eq-topbar">
                    <div class="rack-header-area">
                        <span class="rack-num" style="color:#818cf8; border-color:#818cf8;">5</span>
                        <div>
                            <div class="rack-name" style="color:#c7d2fe; font-size:0.95rem;">🎛️ ReaEQ JSFX Graphic Parametric Equaliser</div>
                            <div class="rack-sub" style="color:#818cf8;">Cytomic SVF &bull; Mid-Focus Studio Scale (200Hz–2.5kHz Expanded) &bull; 60FPS RTA</div>
                        </div>
                    </div>
                    <div style="display:flex; align-items:center; gap:6px; flex-wrap:wrap;">
                        <button class="btn-top ${midFocus ? 'active' : ''}" data-eq-scale-btn="1" style="padding:4px 10px; font-size:0.72rem; min-height:34px; background:${midFocus ? '#059669' : '#1e293b'}; border-color:${midFocus ? '#10b981' : '#334155'}; font-weight:800;" onclick="window.toggleEqMidFocusScale()" title="Expand 200Hz–2.5kHz vocal/instrument midrange across 60% of horizontal width">
                            ${midFocus ? '🎯 MID-FOCUS SCALE' : '📏 STD LOG SCALE'}
                        </button>
                        <div style="display:flex; gap:3px; background:#090d14; padding:2px; border-radius:5px; border:1px solid #1e293b;">
                            <button class="btn-top ${rtaMode === 'peaks' ? 'active' : ''}" data-rta-mode="peaks" style="padding:4px 9px; font-size:0.7rem; min-height:32px; background:${rtaMode === 'peaks' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'peaks' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('peaks')">📐 PEAKS</button>
                            <button class="btn-top ${rtaMode === 'bars' ? 'active' : ''}" data-rta-mode="bars" style="padding:4px 9px; font-size:0.7rem; min-height:32px; background:${rtaMode === 'bars' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'bars' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('bars')">📊 BARS</button>
                            <button class="btn-top ${rtaMode === 'spectral' ? 'active' : ''}" data-rta-mode="spectral" style="padding:4px 9px; font-size:0.7rem; min-height:32px; background:${rtaMode === 'spectral' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'spectral' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('spectral')">🔬 SPECTRAL</button>
                            <button class="btn-top ${rtaMode === 'off' ? 'active' : ''}" data-rta-mode="off" style="padding:4px 8px; font-size:0.7rem; min-height:32px; background:${rtaMode === 'off' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'off' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('off')">OFF</button>
                        </div>
                        <div style="display:flex; flex-direction:column; gap:2px; font-size:0.75rem; color:#818cf8; font-family:monospace; min-width:115px; text-align:right;">
                            <span id="mini-reeq-band-lbl" style="font-weight:bold;">B1: Low Cut</span>
                            <span id="mini-reeq-val-lbl" style="color:#fff; font-size:0.85rem;">30Hz 0.0dB</span>
                        </div>
                        <div class="rack-actions" style="display:flex; gap:6px;">
                            <button class="btn-rack-open" style="background:#7f1d1d; border-color:#ef4444; color:#fecaca; min-height:40px; padding:6px 12px; font-size:0.76rem;" onclick="openDspModal('feedback', ${ch});" title="Open Channel Hybrid -∞ dB Surgical AFD & Moving-Bass Tracker">🎯 AFD PRO (-&infin; dB)</button>
                            <button class="btn-rack-open" style="background:#4f46e5; border-color:#6366f1; min-height:40px; padding:6px 14px; font-size:0.8rem;" onclick="openDspModal('reeq', ${ch});">FULL SCREEN &rarr;</button>
                        </div>
                    </div>
                </div>
                ${isModal ? '' : `<div class="rack-eq-canvas-wrap"><canvas id="dsp-rack-reeq-mini"></canvas></div>`}
                <div class="rack-eq-quick-strip" id="rack-eq-quick-strip">
                    ${eqSt.bands.map((b, idx) => `
                        <div class="rack-eq-quick-node ${eqSt.selectedBand === idx ? 'active' : ''}" id="rack-quick-node-${idx}" onclick="selectRackEqBand(${ch}, ${idx})">
                            <div style="display:flex; justify-content:space-between; align-items:center; font-size:0.72rem; font-weight:800; color:${REEQ_COLORS[idx % REEQ_COLORS.length]};">
                                <span>B${idx+1}</span><input type="checkbox" ${b.enabled ? 'checked' : ''} onclick="event.stopPropagation(); window.setCurrentReEqTarget(${ch}); toggleReEqBand(${idx}); renderMiniReEq(${ch});" style="width:16px; height:16px; cursor:pointer;">
                            </div>
                            <div style="font-size:0.78rem; font-weight:800; color:#fff;" id="rack-node-freq-${idx}">${b.freq >= 1000 ? (b.freq/1000).toFixed(1)+'k' : b.freq+'Hz'}</div>
                            <div style="font-size:0.74rem; color:${b.gain >= 0 ? '#34d399' : '#f87171'};" id="rack-node-gain-${idx}">${(b.gain >= 0 ? '+' : '') + b.gain + 'dB'}</div>
                        </div>
                    `).join('')}
                </div>
            </div>
            ${stage5Html}
            <!-- STAGE 7: Analog Obsession LOADES Dual-Band Zero-Phase De-Esser -->
            <div class="rack-row" data-rack-group="dynamics" style="background:linear-gradient(90deg, #0b1926 0%, #0f172a 100%); border-color:#0284c7;">
                <div class="rack-header-area">
                    <span class="rack-num" style="color:#38bdf8; border-color:#0284c7;">7</span>
                    <div>
                        <div class="rack-name" style="color:#e0f2fe; display:flex; align-items:center; gap:8px;">
                            <span>🎙️ LOADES PRO Dual-Band Zero-Phase De-Esser</span>
                        </div>
                        <div class="rack-sub" style="color:#7dd3fc;">Band 1 Sibilance (${(state.deesser.freq1/1000).toFixed(1)}kHz) + Band 2 Air Harshness (${(state.deesser.freq2/1000).toFixed(1)}kHz) &bull; 60FPS GR</div>
                    </div>
                </div>
                <div class="rack-inline-controls" style="display:flex; align-items:center; gap:8px; flex-wrap:wrap;">
                    <button class="tap-mode-btn ${state.deesser.enabled !== false ? 'active' : ''}" id="rack-deesser-en-${ch}" style="min-height:36px; padding:0 12px; font-weight:900;" onclick="var ds=window.getChannelRackState(${ch}).deesser; ds.enabled=(ds.enabled===false); dispatchDspParam(${ch},'deesserEnabled',ds.enabled?1:0); this.classList.toggle('active',ds.enabled); this.innerText=ds.enabled?'ACTIVE':'BYPASS';">
                        ${state.deesser.enabled !== false ? 'ACTIVE' : 'BYPASS'}
                    </button>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label" style="color:#38bdf8;">B1 Sibilance</span>
                        <input type="range" class="rack-mini-slider" style="width:76px; accent-color:#38bdf8;" min="2000" max="8500" step="100" value="${state.deesser.freq1}" oninput="var ds=window.getChannelRackState(${ch}).deesser; ds.freq1=parseInt(this.value); dispatchDspParam(${ch},'loadesFreq1',ds.freq1); this.nextElementSibling.innerText=(ds.freq1/1000).toFixed(1)+'kHz'">
                        <span class="rack-mini-val">${(state.deesser.freq1/1000).toFixed(1)}kHz</span>
                    </div>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label" style="color:#38bdf8;">B1 Thresh</span>
                        <input type="range" class="rack-mini-slider" style="width:72px; accent-color:#38bdf8;" min="-48" max="0" step="1" value="${state.deesser.thresh1}" oninput="var ds=window.getChannelRackState(${ch}).deesser; ds.thresh1=parseInt(this.value); dispatchDspParam(${ch},'loadesThresh1',ds.thresh1); this.nextElementSibling.innerText=ds.thresh1+'dB'">
                        <span class="rack-mini-val">${state.deesser.thresh1}dB</span>
                    </div>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label" style="color:#f472b6;">B2 Air Harsh</span>
                        <input type="range" class="rack-mini-slider" style="width:76px; accent-color:#f472b6;" min="4500" max="16000" step="100" value="${state.deesser.freq2}" oninput="var ds=window.getChannelRackState(${ch}).deesser; ds.freq2=parseInt(this.value); dispatchDspParam(${ch},'loadesFreq2',ds.freq2); this.nextElementSibling.innerText=(ds.freq2/1000).toFixed(1)+'kHz'">
                        <span class="rack-mini-val">${(state.deesser.freq2/1000).toFixed(1)}kHz</span>
                    </div>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label" style="color:#f472b6;">B2 Thresh</span>
                        <input type="range" class="rack-mini-slider" style="width:72px; accent-color:#f472b6;" min="-48" max="0" step="1" value="${state.deesser.thresh2}" oninput="var ds=window.getChannelRackState(${ch}).deesser; ds.thresh2=parseInt(this.value); dispatchDspParam(${ch},'loadesThresh2',ds.thresh2); this.nextElementSibling.innerText=ds.thresh2+'dB'">
                        <span class="rack-mini-val">${state.deesser.thresh2}dB</span>
                    </div>
                    <!-- Live Twin 60FPS GR Mini Meters -->
                    <div style="display:flex; flex-direction:column; gap:3px; min-width:86px; background:#070b12; padding:3px 6px; border-radius:5px; border:1px solid #1e293b;">
                        <div style="display:flex; align-items:center; gap:4px; font-size:0.55rem; font-family:monospace; color:#38bdf8;">
                            <span>B1</span>
                            <div style="flex:1; height:5px; background:#1e293b; border-radius:3px; overflow:hidden;"><div id="rack-ds-gr1-${ch}" style="width:0%; height:100%; background:#38bdf8;"></div></div>
                        </div>
                        <div style="display:flex; align-items:center; gap:4px; font-size:0.55rem; font-family:monospace; color:#f472b6;">
                            <span>B2</span>
                            <div style="flex:1; height:5px; background:#1e293b; border-radius:3px; overflow:hidden;"><div id="rack-ds-gr2-${ch}" style="width:0%; height:100%; background:#f472b6;"></div></div>
                        </div>
                    </div>
                </div>
                <div class="rack-actions">
                    <button class="btn-rack-open" style="background:#0284c7; border-color:#38bdf8; color:#fff; font-weight:900;" onclick="openDspModal('deesser', ${ch});">2-BAND GUI &rarr;</button>
                </div>
            </div>
        `;
    }

    window.showDspRackPage = function(ch) {
        window.dspRackCurrentChannel = ch;
        if (typeof window.setCurrentReEqTarget === 'function') window.setCurrentReEqTarget(ch);
        const state = getChannelRackState(ch);
        const chName = (window.customChannelNames && window.customChannelNames['ch-' + ch]) || (window.channelNames && window.channelNames[ch]) || `Channel ${ch + 1}`;
        const gain = (window.channelInputGains && window.channelInputGains[ch]) ?? 0;
        const regainActive = (window.channelRegainActive && window.channelRegainActive[ch]) ?? true;
        const titleEl = document.getElementById('dsp-rack-page-title');
        if (titleEl) titleEl.innerHTML = `🎛️ IN ${ch + 1} — ${chName}`;
        document.querySelectorAll('#dsp-rack-ch-switcher .btn-rack-ch-pill').forEach((btn, idx) => {
            btn.classList.toggle('active', idx === Number(ch));
        });
        const gainSlider = document.getElementById('dsp-rack-gain-slider');
        const gainVal = document.getElementById('dsp-rack-gain-val');
        const gainLabel = document.getElementById('dsp-rack-gain-label');
        const regainBtn = document.getElementById('dsp-rack-regain-btn');
        if (gainSlider) gainSlider.value = gain;
        if (gainVal) gainVal.innerText = (gain >= 0 ? '+' : '') + parseFloat(gain).toFixed(1) + ' dB';
        if (gainLabel) gainLabel.innerText = `STAGE 2: DIGITAL GAIN — IN ${ch + 1} (POST-1176 PREAMP)`;
        if (regainBtn) {
            regainBtn.classList.toggle('active', regainActive);
            regainBtn.innerText = regainActive ? 'REGAIN ON' : 'REGAIN OFF';
        }
        const rackPage = document.getElementById('dsp-rack-page');
        if (rackPage) rackPage.classList.add('open');
        const grid = document.getElementById('dsp-rack-grid');
        if (grid) {
            grid.innerHTML = window.renderPreampStageRow(ch, state, false) + buildCoreRackRowsHtml(ch, state, false);
            grid.setAttribute('data-rack-tab', window.currentDspRackSubTab || 'input_fx');
        }
        if (typeof window.updateFocusedChannelStrip === 'function') window.updateFocusedChannelStrip(ch);
        if (typeof window.syncChannelSetupBadge === 'function') window.syncChannelSetupBadge(ch);
        if (window.updatePreampClipIndicator) window.updatePreampClipIndicator(ch);
        if (typeof window.renderMiniReEq === 'function') window.renderMiniReEq(ch);
        requestAnimationFrame(() => {
            if (typeof window.renderMiniReEq === 'function') window.renderMiniReEq(ch);
        });
    };

    window.getChannelRackState = getChannelRackState;
    window.openChannelRackModal = openChannelRackModal;
    window.closeChannelRackModal = closeChannelRackModal;
    window.dispatchDspParam = dispatchDspParam;
    window.triggerDenoiseLearn = triggerDenoiseLearn;
    window.setChannelCompModel = setChannelCompModel;
    window.togglePhaseInvert = togglePhaseInvert;
    window.setChannelDirectTap = setChannelDirectTap;
})(window);
