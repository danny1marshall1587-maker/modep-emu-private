// dsp-modals.js - Authentic Level-2 DSP Modal Router (ReEQ, Denoiser, AFD, De-Esser, Delay, Reverb, Room Cal)
(function(window) {
    'use strict';
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
            overlay.classList.add("open");
            if (type === 'reeq' || type === 'reaeq') {
                modal.classList.add("reeq-modal", "reaeq-modal", "fullscreen");
                if (typeof window.setCurrentReEqTarget === 'function') window.setCurrentReEqTarget(target);
                const state = getChannelReEq(target);
                const tArg = typeof target === 'string' ? `'${target}'` : target;
                const fToSl = window.freqToSlider || (f => Math.round(((Math.log(Math.max(20, Math.min(20000, f || 1000))) - Math.log(20)) / (Math.log(20000) - Math.log(20))) * 1000));
                const rtaMode = (window.rtaViewMode && window.rtaViewMode !== 'line') ? window.rtaViewMode : 'peaks';
                title.innerHTML = `🎛️ ReEQ + Tukan Green Dynamic EQ S2 (REAPER JSFX) &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div class="reeq-toolbar">
                        <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
                            <button class="btn-top" style="padding:4px 10px; font-size:0.75rem; background:#374151; font-weight:700;" onclick="closeDspModal()">← BACK TO RACK</button>
                            <span style="color:#818cf8; font-weight:bold;">ReEQ + Tukan DynEQ S2</span>
                            <button class="btn-top ${state.adaptiveRoomDynEq ? 'active' : ''}" id="reeq-tukan-adaptive-btn" style="padding:3px 10px; font-size:0.7rem; background:${state.adaptiveRoomDynEq ? '#059669' : '#1e293b'}; border-color:${state.adaptiveRoomDynEq ? '#10b981' : '#334155'}; font-weight:800;" onclick="toggleAdaptiveRoomDynEq(${tArg})" title="Tukan Studios Green Dynamic EQ S2: keeps room curve flat across quiet & loud volumes">
                                ${state.adaptiveRoomDynEq ? '🌊 TUKAN DYN-EQ S2: AUTO ROOM FLAT (ON)' : '🌊 TUKAN DYN-EQ S2: OFF'}
                            </button>
                            <button class="btn-top ${window.eqMidFocusScale !== false ? 'active' : ''}" data-eq-scale-btn="1" style="padding:3px 10px; font-size:0.7rem; background:${window.eqMidFocusScale !== false ? '#059669' : '#1e293b'}; border-color:${window.eqMidFocusScale !== false ? '#10b981' : '#334155'}; font-weight:800;" onclick="window.toggleEqMidFocusScale()" title="Expand 200Hz–2.5kHz midrange across 60% of horizontal width">
                                ${window.eqMidFocusScale !== false ? '🎯 MID-FOCUS SCALE (200Hz–2.5k)' : '📏 STD LOG SCALE'}
                            </button>
                            <div style="display:flex; align-items:center; gap:4px; background:#090d14; padding:2px 4px; border-radius:6px; border:1px solid #334155;">
                                <span style="font-size:0.65rem; color:#94a3b8; font-weight:700; margin-right:2px;">RTA VIEW:</span>
                                <button class="btn-top ${rtaMode === 'peaks' ? 'active' : ''}" data-rta-mode="peaks" style="padding:2px 8px; font-size:0.68rem; min-height:26px; background:${rtaMode === 'peaks' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'peaks' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('peaks')">📐 PEAKS</button>
                                <button class="btn-top ${rtaMode === 'bars' ? 'active' : ''}" data-rta-mode="bars" style="padding:2px 8px; font-size:0.68rem; min-height:26px; background:${rtaMode === 'bars' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'bars' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('bars')">📊 BARS</button>
                                <button class="btn-top ${rtaMode === 'spectral' ? 'active' : ''}" data-rta-mode="spectral" style="padding:2px 8px; font-size:0.68rem; min-height:26px; background:${rtaMode === 'spectral' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'spectral' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('spectral')">🔬 FINE SPECTRAL</button>
                                <button class="btn-top ${rtaMode === 'off' ? 'active' : ''}" data-rta-mode="off" style="padding:2px 7px; font-size:0.68rem; min-height:26px; background:${rtaMode === 'off' ? '#0284c7' : '#1e293b'}; border-color:${rtaMode === 'off' ? '#38bdf8' : '#334155'};" onclick="window.setRtaViewMode('off')">OFF</button>
                            </div>
                        </div>
                        <div style="display:flex; gap:8px; align-items:center;">
                            <label style="color:#8b949e; font-size:0.7rem;">dB Scale:</label>
                            <select onchange="setReEqRange(this.value)" style="background:#161a22; color:#fff; border:1px solid #30363d; border-radius:4px; font-size:0.7rem; padding:2px 6px;">
                                <option value="12" ${state.dbRange === 12 ? 'selected' : ''}>&plusmn;12 dB</option>
                                <option value="18" ${state.dbRange === 18 ? 'selected' : ''}>&plusmn;18 dB</option>
                                <option value="24" ${state.dbRange === 24 ? 'selected' : ''}>&plusmn;24 dB</option>
                                <option value="30" ${state.dbRange === 30 ? 'selected' : ''}>&plusmn;30 dB</option>
                            </select>
                            <button class="btn-top" id="reeq-toggle-fs-btn" style="padding:3px 8px; font-size:0.7rem; background:#4338ca; border-color:#6366f1;" onclick="toggleReaEQFullscreen()">⛶ WINDOWED</button>
                        </div>
                    </div>
                    <div class="reeq-canvas-container">
                        <canvas id="reeq-canvas"></canvas>
                    </div>
                    <div class="reeq-node-strip">
                        ${state.bands.map((b, idx) => {
                            const isCut = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8);
                            const hasSlope = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6);
                            return `
                            <div class="reeq-node-card ${state.selectedBand === idx ? 'selected' : ''} ${!b.enabled ? 'bypassed' : ''}" id="reeq-card-${idx}" onclick="selectReEqBand(${idx})" ondblclick="resetReEqBand(${idx})">
                                <div class="reeq-node-header" style="color:${REEQ_COLORS[idx % REEQ_COLORS.length]};">
                                    <span>Band ${idx + 1}</span>
                                    <div style="display:flex; gap:4px; align-items:center;" onclick="event.stopPropagation();">
                                        <button id="reeq-dyn-btn-${idx}" style="font-size:0.55rem; padding:1px 5px; border-radius:3px; border:1px solid ${b.dynEnabled ? '#10b981' : '#334155'}; background:${b.dynEnabled ? '#059669' : '#1e293b'}; color:#fff; font-weight:800; cursor:pointer;" onclick="toggleReEqBandDyn(${idx})" title="Enable Tukan Dynamic EQ SVF Bandpass Sidechain on this band">${b.dynEnabled ? 'DYN ON' : 'DYN'}</button>
                                        <input type="checkbox" ${b.enabled ? 'checked' : ''} onclick="toggleReEqBand(${idx})">
                                    </div>
                                </div>
                                <div class="reeq-node-controls" style="display:flex; flex-direction:column; gap:2px;" onclick="event.stopPropagation();">
                                    <div style="display:flex; gap:3px;">
                                        <select style="flex:1;" onchange="changeReEqType(${idx}, this.value)">
                                            ${FILTER_TYPES.map((t, tIdx) => `<option value="${tIdx}" ${b.type === tIdx ? 'selected' : ''}>${t}</option>`).join('')}
                                        </select>
                                        <select id="reeq-slope-${idx}" style="width:54px; display:${hasSlope ? 'block' : 'none'}; background:#0f172a; color:#38bdf8; border:1px solid #334155; border-radius:3px; font-size:0.62rem;" onchange="changeReEqSlope(${idx}, this.value)">
                                            <option value="1" ${(b.slope || 1) === 1 ? 'selected' : ''}>12dB</option>
                                            <option value="2" ${(b.slope || 1) === 2 ? 'selected' : ''}>24dB</option>
                                            <option value="3" ${(b.slope || 1) === 3 ? 'selected' : ''}>36dB</option>
                                            <option value="4" ${(b.slope || 1) === 4 ? 'selected' : ''}>48dB</option>
                                        </select>
                                    </div>
                                    <div style="display:flex; justify-content:space-between; font-size:0.63rem; color:#cbd5e1;">
                                        <span>Freq</span><span id="reeq-freq-${idx}" style="font-weight:700; color:#fff;">${b.freq >= 1000 ? (b.freq / 1000).toFixed(2) + ' kHz' : b.freq + ' Hz'}</span>
                                    </div>
                                    <input type="range" id="reeq-freq-slider-${idx}" min="0" max="1000" step="1" value="${fToSl(b.freq)}" style="width:100%; height:13px; accent-color:${REEQ_COLORS[idx % REEQ_COLORS.length]};" oninput="changeReEqFreqSlider(${idx}, this.value)">
                                    <div style="display:flex; justify-content:space-between; font-size:0.63rem; color:#cbd5e1;">
                                        <span>Gain</span><span id="reeq-gain-${idx}" style="font-weight:700; color:#fff;">${isCut ? '—' : ((b.gain >= 0 ? '+' : '') + b.gain.toFixed(1) + ' dB')}</span>
                                    </div>
                                    <input type="range" id="reeq-gain-slider-${idx}" min="-24" max="24" step="0.5" value="${b.gain}" ${isCut ? 'disabled' : ''} style="width:100%; height:13px; accent-color:#38bdf8;" oninput="changeReEqGain(${idx}, this.value)" ondblclick="changeReEqGain(${idx}, 0)">
                                    <div style="display:flex; justify-content:space-between; font-size:0.63rem; color:#cbd5e1;">
                                        <span>Width</span><span id="reeq-q-${idx}" style="font-weight:700; color:#818cf8;">Q ${b.q.toFixed(2)}</span>
                                    </div>
                                    <input type="range" id="reeq-q-slider-${idx}" min="0.1" max="18.0" step="0.05" value="${b.q}" style="width:100%; height:13px; accent-color:#818cf8;" oninput="changeReEqQ(${idx}, this.value)" ondblclick="changeReEqQ(${idx}, 0.71)">
                                    <div id="reeq-dyn-box-${idx}" style="display:${b.dynEnabled ? 'flex' : 'none'}; flex-direction:column; gap:1px; background:#091512; border:1px solid #059669; border-radius:4px; padding:3px;">
                                        <div style="display:flex; justify-content:space-between; font-size:0.58rem; color:#6ee7b7;"><span>Thresh</span><span id="reeq-dyn-th-val-${idx}">${b.dynThresh ?? -20}dB</span></div>
                                        <input type="range" min="-48" max="0" step="1" value="${b.dynThresh ?? -20}" style="width:100%; height:10px; accent-color:#10b981;" oninput="changeReEqDynThresh(${idx}, this.value)">
                                        <div style="display:flex; justify-content:space-between; font-size:0.58rem; color:#6ee7b7;"><span>Dyn Range</span><span id="reeq-dyn-rg-val-${idx}">${((b.dynRange ?? -6) >= 0 ? '+' : '') + (b.dynRange ?? -6)}dB</span></div>
                                        <input type="range" min="-18" max="12" step="0.5" value="${b.dynRange ?? -6}" style="width:100%; height:10px; accent-color:#10b981;" oninput="changeReEqDynRange(${idx}, this.value)">
                                    </div>
                                </div>
                            </div>`;
                        }).join('')}
                    </div>
                `;
                const canvas = document.getElementById("reeq-canvas");
                if (canvas) resizeReaEQCanvas(canvas, canvas.parentElement);
                else renderReEqCanvas();
                initReEqInteractions();
                requestAnimationFrame(() => {
                    const cvs = document.getElementById("reeq-canvas");
                    if (cvs) resizeReaEQCanvas(cvs, cvs.parentElement);
                });
            } else if (type === 'denoiser') {
                modal.classList.add("fullscreen");
                const chIdx = typeof target === 'number' ? target : (window.dspRackCurrentChannel || 0);
                if (typeof window.setCurrentReEqTarget === 'function') window.setCurrentReEqTarget(chIdx);
                const dSt = window.getChannelRackState ? window.getChannelRackState(chIdx).denoiser : {
                    enabled: true, threshold: -45, reduction: -18, sensitivity: 0.0, reductionPct: 85.0, lowCut: true, listen: false,
                    bands: [-60, -60, -60, -60, -60, -60, -60, -60, -60, -60]
                };
                if (!dSt.bands) dSt.bands = [-60, -60, -60, -60, -60, -60, -60, -60, -60, -60];
                const bandLabels = ['60Hz', '150Hz', '400Hz', '800Hz', '1.5kHz', '3kHz', '5kHz', '8kHz', '12kHz', '16kHz'];
                title.innerHTML = `🛡️ CYBER-DENOISER PRO (10-Band Subtractive Phase-Cancellation Engine) &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div style="display:flex; flex-direction:column; gap:10px; height:100%; min-height:0; background:#0d0a1a; padding:10px 14px; border-radius:10px; border:1px solid #4c1d95;">
                        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px;">
                            <div style="display:flex; align-items:center; gap:10px;">
                                <button class="btn-top" style="padding:4px 10px; font-size:0.75rem; background:#374151; font-weight:700;" onclick="closeDspModal()">← BACK TO RACK</button>
                                <span style="font-weight:900; font-size:0.95rem; letter-spacing:2px; color:#fff; text-shadow:0 0 12px #a855f7;">CYBER-DENOISER <span style="font-size:0.65rem; background:#a855f7; color:#000; padding:2px 6px; border-radius:4px; font-weight:900;">PRO 10-BAND</span></span>
                                <span style="font-size:0.72rem; color:#c084fc;">Adaptive Tail-Stabilized Anti-Chirp Release &bull; 18kHz Ultrasonic Cut</span>
                            </div>
                            <div style="display:flex; align-items:center; gap:8px;">
                                <span class="gate-led ${dSt.enabled ? 'open' : 'closed'}" id="modal-gate-led-${chIdx}"></span>
                                <button class="btn-learn-noise" id="modal-btn-learn-noise-${chIdx}" style="min-height:36px; padding:0 14px; font-size:0.76rem; background:linear-gradient(135deg, #a855f7, #ec4899); color:#fff; border:none; font-weight:800;" onclick="triggerDenoiseLearn(${chIdx})">⏺ LEARN NOISE FLOOR</button>
                                <button class="tap-mode-btn ${dSt.lowCut ? 'active' : ''}" id="cyber-denoise-lowcut-${chIdx}" style="min-height:36px; padding:0 12px; font-size:0.74rem;" onclick="var d=window.getChannelRackState(${chIdx}).denoiser; d.lowCut=!d.lowCut; dispatchDspParam(${chIdx},'denoiseLowCut',d.lowCut?1:0); this.classList.toggle('active',d.lowCut);">LOW CUT (40Hz)</button>
                                <button class="tap-mode-btn ${dSt.listen ? 'active' : ''}" id="cyber-denoise-listen-${chIdx}" style="min-height:36px; padding:0 12px; font-size:0.74rem; border-color:#ec4899;" onclick="var d=window.getChannelRackState(${chIdx}).denoiser; d.listen=!d.listen; dispatchDspParam(${chIdx},'denoiseListen',d.listen?1:0); this.classList.toggle('active',d.listen);">🎧 LISTEN NOISE (DELTA)</button>
                                <button class="tap-mode-btn ${dSt.enabled ? 'active' : ''}" id="modal-denoise-en-${chIdx}" style="min-height:36px; padding:0 14px; font-weight:800;" onclick="var d=window.getChannelRackState(${chIdx}).denoiser; d.enabled=!d.enabled; dispatchDspParam(${chIdx},'denoiseBypass',!d.enabled); this.classList.toggle('active',d.enabled); this.innerText=d.enabled?'ACTIVE':'BYPASS';">${dSt.enabled ? 'ACTIVE' : 'BYPASS'}</button>
                            </div>
                        </div>

                        <!-- Interactive 10-Band Liquid Silk & Forensic Spectral Thresholds Canvas -->
                        <div style="position:relative; flex:1 1 220px; min-height:200px; background:#05030a; border-radius:10px; border:1px solid #3b1c71; overflow:hidden; box-shadow:inset 0 0 30px #000;">
                            <canvas id="cyber-denoiser-canvas" data-ch="${chIdx}" style="width:100%; height:100%; display:block; touch-action:none; cursor:ns-resize;"></canvas>
                            <div style="position:absolute; top:8px; left:12px; pointer-events:none; display:flex; gap:14px; font-size:0.68rem; font-family:monospace; color:#cbd5e1;">
                                <span><b style="color:#00f2ff;">■</b> 10-BAND LIVE SPECTRUM</span>
                                <span><b style="color:#a855f7;">●</b> DRAG NODES TO SCULPT 10-BAND THRESHOLD CURVE (-100..0 dB)</span>
                            </div>
                        </div>

                        <!-- 10-Band Forensic Readout & Fine Trim Strip -->
                        <div style="display:grid; grid-template-columns:repeat(10, 1fr); gap:5px;">
                            ${bandLabels.map((lbl, idx) => `
                                <div style="background:rgba(26, 20, 48, 0.85); border:1px solid rgba(168, 85, 247, 0.3); border-radius:6px; padding:5px 4px; display:flex; flex-direction:column; align-items:center; gap:2px;">
                                    <span style="font-size:0.64rem; font-weight:800; color:#c084fc;">${lbl}</span>
                                    <input type="range" id="cyber-denoise-bslider-${chIdx}-${idx}" min="-100" max="0" step="1" value="${dSt.bands[idx] ?? -60}" style="width:100%; height:12px; accent-color:#a855f7;" oninput="window.setCyberDenoiserBand(${chIdx}, ${idx}, parseFloat(this.value))">
                                    <span id="cyber-denoise-bval-${chIdx}-${idx}" style="font-size:0.66rem; font-family:monospace; color:#00f2ff; font-weight:700;">${Math.round(dSt.bands[idx] ?? -60)}dB</span>
                                </div>
                            `).join('')}
                        </div>

                        <!-- Master Sensitivity & Reduction Depth Controls -->
                        <div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">
                            <div class="knob-card" style="background:rgba(26, 20, 48, 0.75); border-color:rgba(168, 85, 247, 0.35); padding:8px 14px;">
                                <div style="display:flex; justify-content:space-between; width:100%; align-items:center;">
                                    <span class="knob-label" style="color:#e9d5ff;">MASTER SENSITIVITY (THRESHOLD OFFSET)</span>
                                    <span class="knob-val" id="modal-denoise-sens-val-${chIdx}" style="color:#a855f7; font-size:0.85rem;">${(dSt.sensitivity >= 0 ? '+' : '') + parseFloat(dSt.sensitivity || 0).toFixed(1)} dB</span>
                                </div>
                                <input type="range" class="knob-slider" id="modal-denoise-sens-${chIdx}" min="-20" max="20" step="0.5" value="${dSt.sensitivity || 0}" style="accent-color:#a855f7;" oninput="var v=parseFloat(this.value); var d=window.getChannelRackState(${chIdx}).denoiser; d.sensitivity=v; d.threshold=Math.round(-45 + v/0.55); dispatchDspParam(${chIdx},'denoiseSensitivity',v); document.getElementById('modal-denoise-sens-val-${chIdx}').innerText=(v>=0?'+':'')+v.toFixed(1)+' dB'; if(window.drawCyberDenoiserCanvas)window.drawCyberDenoiserCanvas(${chIdx});">
                            </div>
                            <div class="knob-card" style="background:rgba(26, 20, 48, 0.75); border-color:rgba(0, 242, 255, 0.35); padding:8px 14px;">
                                <div style="display:flex; justify-content:space-between; width:100%; align-items:center;">
                                    <span class="knob-label" style="color:#cffafe;">REDUCTION DEPTH (PHASE SUBTRACTION)</span>
                                    <span class="knob-val" id="modal-denoise-reduc-val-${chIdx}" style="color:#00f2ff; font-size:0.85rem;">${Math.round(dSt.reductionPct ?? 85)}%</span>
                                </div>
                                <input type="range" class="knob-slider" id="modal-denoise-reduc-${chIdx}" min="0" max="100" step="1" value="${dSt.reductionPct ?? 85}" style="accent-color:#00f2ff;" oninput="var v=parseFloat(this.value); var d=window.getChannelRackState(${chIdx}).denoiser; d.reductionPct=v; d.reduction=-Math.round((v/100)*40); dispatchDspParam(${chIdx},'denoiseReduction',v); document.getElementById('modal-denoise-reduc-val-${chIdx}').innerText=Math.round(v)+'%'; var rv=document.getElementById('denoise-reduc-val-${chIdx}'); if(rv)rv.innerText=Math.round(v)+'% ('+d.reduction+'dB)';">
                            </div>
                        </div>
                    </div>
                `;
                if (typeof window.initCyberDenoiserCanvas === 'function') window.initCyberDenoiserCanvas(chIdx);
            } else if (type === 'preamp1176' || type === 'dual' || type === 'fet1176' || type === 'fetish' || type === 'la2a' || type === 'lala' || type === 'limiter' || type === 'comp') {
                window.renderUnifiedCompressorModal(type, target, displayTargetName, modal, title, body);
            } else if (type === 'summing') {
                window.renderAoModalContent(type, target, targetName, displayTargetName, modal, title, body);
            } else if (type === 'scenes') {
                if (typeof window.renderSceneManagerModal === 'function') window.renderSceneManagerModal(title, body);
            } else if (type === 'feedback') {
                const afdCh = getAfdChannel(target);
                const tArg = typeof target === 'string' ? `'${target}'` : target;
                title.innerHTML = `🎯 Hybrid -∞ dB Surgical AFD PRO &amp; Moving-Bass Tracker &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div class="afd-container">
                        <div class="afd-controls-row">
                            <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
                                <button class="afd-btn-action" onclick="toggleAfdAutoHunting(this)">AUTO HUNTING: ACTIVE</button>
                                <button class="btn-top ${afdState.infNullMode ? 'active' : ''}" style="background:#4c1d95; border-color:#8b5cf6; color:#f3e8ff; font-size:0.72rem;" onclick="toggleAfdInfNullMode(this)" title="64-bit Single-Filter -Infinity dB Spectral Null — eliminates multi-bus X32 EQ stacking">
                                    ${afdState.infNullMode ? '♾️ -∞ dB SINGLE-FILTER NULL: ON' : '♾️ -∞ dB NULL: OFF (-24dB)'}
                                </button>
                                <span style="font-size:0.7rem; color:#10b981; font-weight:700;">${afdCh.role === 'live_channel' ? '🎸 CH LIVE TRACKER + MOVING-BASS (60-280Hz AUTO-Q)' : '🔒 BUS SOUNDCHECK RING-OUT LOCK'}</span>
                            </div>
                            <div style="display:flex; gap:8px;">
                                <button class="afd-btn-danger" onclick="panicClearAfdFilters(${tArg})">PANIC: RESET NOTCHES</button>
                            </div>
                        </div>
                        <div class="afd-spectrum-container">
                            <canvas id="afd-spectrum-canvas"></canvas>
                            <div class="afd-hud">
                                <div class="afd-hud-item">
                                    <span>PRECISION:</span>
                                    <span class="afd-hud-val" style="color:#22c55e;">64-BIT DOUBLE SVF</span>
                                </div>
                                <div class="afd-hud-item">
                                    <span>NOTCHES:</span>
                                    <span class="afd-hud-val" id="afd-hud-count" style="color:#ef4444;">0 / 12 NOTCHES (-∞ dB NULL)</span>
                                </div>
                                <div class="afd-hud-item">
                                    <span>BASS TRACKER:</span>
                                    <span class="afd-hud-val" style="color:#38bdf8;">60-280Hz AUTO-Q</span>
                                </div>
                            </div>
                        </div>
                        <div style="display:flex; justify-content:space-between; align-items:center; margin-top:2px;">
                            <span style="font-size:0.72rem; font-weight:bold; color:#cbd5e1; text-transform:uppercase; letter-spacing:0.5px;">12 Surgical 64-Bit Notch Bank (6 Soundcheck Fixed + 6 Live Moving-Bass / Floating)</span>
                            <span style="font-size:0.68rem; color:#8b949e;">Single-filter -∞ dB null replaces multi-bus EQ stacking</span>
                        </div>
                        <div class="afd-filter-grid" id="afd-filter-grid"></div>
                    </div>
                `;
                updateAfdFilterCards(target);
                if (window.afdAnimId) cancelAnimationFrame(window.afdAnimId);
                renderAfdSpectrum(target);
            } else if (type === 'deesser') {
                modal.classList.add("fullscreen", "fx-fullscreen");
                const chIdx = typeof target === 'number' ? target : (window.dspRackCurrentChannel || 0);
                window.currentDeesserModalChannel = chIdx;
                if (typeof window.setCurrentReEqTarget === 'function') window.setCurrentReEqTarget(chIdx);
                const ds = getChannelRackState(chIdx).deesser;
                title.innerHTML = `🎙️ LOADES PRO Dual-Band Zero-Phase Dynamic De-Esser &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div style="display:flex; flex-direction:column; gap:10px; height:100%; min-height:0; background:#070c14; padding:10px 14px; border-radius:10px; border:1px solid #0284c7;">
                        <!-- Top Toolbar: Back, Active/Bypass, Solo Sibilance Audition Modes -->
                        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px;">
                            <div style="display:flex; align-items:center; gap:10px;">
                                <button class="btn-top" style="padding:4px 12px; font-size:0.75rem; background:#374151; font-weight:800;" onclick="dispatchDspParam(${chIdx},'loadesListen',0); getChannelRackState(${chIdx}).deesser.listenMode=0; closeDspModal();">← BACK TO RACK</button>
                                <span style="font-weight:900; font-size:0.95rem; letter-spacing:1.5px; color:#e0f2fe;">LOADES PRO <span style="font-size:0.65rem; background:#0284c7; color:#fff; padding:2px 6px; border-radius:4px; font-weight:900;">ZERO-PHASE 2-BAND SVF</span></span>
                                <button class="tap-mode-btn ${ds.enabled !== false ? 'active' : ''}" id="modal-deesser-en-${chIdx}" style="min-height:34px; padding:0 14px; font-weight:900;" onclick="var d=getChannelRackState(${chIdx}).deesser; d.enabled=(d.enabled===false); dispatchDspParam(${chIdx},'deesserEnabled',d.enabled?1:0); this.classList.toggle('active',d.enabled); this.innerText=d.enabled?'⚡ DE-ESSER ACTIVE':'🔇 BYPASSED';">${ds.enabled !== false ? '⚡ DE-ESSER ACTIVE' : '🔇 BYPASSED'}</button>
                            </div>
                            <div style="display:flex; align-items:center; gap:6px; flex-wrap:wrap;">
                                <span style="font-size:0.68rem; font-weight:800; color:#94a3b8;">🎧 SOLO SIBILANCE (DELTA AUDITION):</span>
                                <button class="btn-top ds-listen-btn ${(ds.listenMode||0)===0?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="window.setDeesserListenMode(${chIdx}, 0)">MIX (NORMAL)</button>
                                <button class="btn-top ds-listen-btn ${(ds.listenMode||0)===1?'active':''}" style="padding:4px 10px; font-size:0.7rem; border-color:#38bdf8;" onclick="window.setDeesserListenMode(${chIdx}, 1)">🎧 SOLO B1+B2</button>
                                <button class="btn-top ds-listen-btn ${(ds.listenMode||0)===2?'active':''}" style="padding:4px 10px; font-size:0.7rem; border-color:#38bdf8;" onclick="window.setDeesserListenMode(${chIdx}, 2)">🎧 SOLO B1 (ESS)</button>
                                <button class="btn-top ds-listen-btn ${(ds.listenMode||0)===3?'active':''}" style="padding:4px 10px; font-size:0.7rem; border-color:#f472b6;" onclick="window.setDeesserListenMode(${chIdx}, 3)">🎧 SOLO B2 (AIR)</button>
                            </div>
                        </div>

                        <!-- Interactive High-Frequency RTA + Draggable Band 1 & Band 2 Sibilance Node Canvas -->
                        <div style="position:relative; flex:1 1 200px; min-height:175px; background:#04080e; border-radius:10px; border:1px solid #1e3a5f; overflow:hidden; box-shadow:inset 0 0 28px #000;">
                            <canvas id="deesser-rta-canvas" data-ch="${chIdx}" style="width:100%; height:100%; display:block; touch-action:none; cursor:move;"></canvas>
                            <div style="position:absolute; top:8px; left:12px; pointer-events:none; display:flex; gap:16px; font-size:0.68rem; font-family:monospace; color:#cbd5e1;">
                                <span><b style="color:#38bdf8;">● BAND 1 (SIBILANCE)</b> DRAG FREQ / THRESHOLD</span>
                                <span><b style="color:#f472b6;">● BAND 2 (AIR HARSHNESS)</b> DRAG FREQ / THRESHOLD</span>
                                <span><b style="color:#fbbf24;">▼</b> LIVE 60FPS DYNAMIC ATTENUATION NOTCH</span>
                            </div>
                        </div>

                        <!-- Twin 60FPS Gain Reduction Meters Bar -->
                        <div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">
                            <div style="background:#091320; border:1px solid #0284c7; border-radius:8px; padding:6px 12px; display:flex; align-items:center; gap:10px;">
                                <span style="font-size:0.72rem; font-weight:900; color:#38bdf8; min-width:120px;">BAND 1 GR (SIBILANCE)</span>
                                <div style="flex:1; height:12px; background:#04080e; border-radius:6px; overflow:hidden; border:1px solid #1e293b;">
                                    <div id="modal-ds-gr1-bar" style="width:0%; height:100%; background:linear-gradient(90deg, #10b981, #38bdf8, #f59e0b); transition:none;"></div>
                                </div>
                                <span id="modal-ds-gr1-val" style="font-family:monospace; font-size:0.78rem; font-weight:900; color:#38bdf8; min-width:56px; text-align:right;">0.0 dB</span>
                            </div>
                            <div style="background:#160b1c; border:1px solid #db2777; border-radius:8px; padding:6px 12px; display:flex; align-items:center; gap:10px;">
                                <span style="font-size:0.72rem; font-weight:900; color:#f472b6; min-width:120px;">BAND 2 GR (AIR HARSH)</span>
                                <div style="flex:1; height:12px; background:#04080e; border-radius:6px; overflow:hidden; border:1px solid #1e293b;">
                                    <div id="modal-ds-gr2-bar" style="width:0%; height:100%; background:linear-gradient(90deg, #10b981, #f472b6, #ef4444); transition:none;"></div>
                                </div>
                                <span id="modal-ds-gr2-val" style="font-family:monospace; font-size:0.78rem; font-weight:900; color:#f472b6; min-width:56px; text-align:right;">0.0 dB</span>
                            </div>
                        </div>

                        <!-- Band 1 & Band 2 Control Cards -->
                        <div style="display:grid; grid-template-columns:1fr 1fr; gap:12px;">
                            <!-- BAND 1 CARD -->
                            <div style="background:#091320; border:1px solid #0284c7; border-radius:8px; padding:8px 12px; display:flex; flex-direction:column; gap:6px;">
                                <div style="display:flex; justify-content:space-between; align-items:center;">
                                    <span style="font-size:0.78rem; font-weight:900; color:#38bdf8;">🎯 BAND 1 — VOCAL SIBILANCE (2.0 kHz – 8.5 kHz)</span>
                                    <span id="ds-b1-freq-lbl" style="font-family:monospace; font-size:0.82rem; font-weight:900; color:#fff;">${(ds.freq1/1000).toFixed(2)} kHz</span>
                                </div>
                                <div style="display:grid; grid-template-columns:1fr 1fr; gap:8px;">
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Center Freq</span><span id="ds-b1-freq-v">${ds.freq1} Hz</span></div>
                                        <input type="range" id="ds-b1-freq-sl" min="2000" max="8500" step="50" value="${ds.freq1}" style="width:100%; accent-color:#38bdf8;" oninput="window.updateDeesserParam(${chIdx}, 1, 'freq', this.value)">
                                    </div>
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Threshold</span><span id="ds-b1-thresh-v">${ds.thresh1} dB</span></div>
                                        <input type="range" id="ds-b1-thresh-sl" min="-48" max="0" step="0.5" value="${ds.thresh1}" style="width:100%; accent-color:#38bdf8;" oninput="window.updateDeesserParam(${chIdx}, 1, 'thresh', this.value)">
                                    </div>
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Max Reduction</span><span id="ds-b1-max-v">-${ds.max1} dB</span></div>
                                        <input type="range" id="ds-b1-max-sl" min="0" max="24" step="0.5" value="${ds.max1}" style="width:100%; accent-color:#38bdf8;" oninput="window.updateDeesserParam(${chIdx}, 1, 'max', this.value)">
                                    </div>
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Bandwidth (Q)</span><span id="ds-b1-q-v">Q ${Number(ds.q1).toFixed(1)}</span></div>
                                        <input type="range" id="ds-b1-q-sl" min="0.7" max="8.0" step="0.1" value="${ds.q1}" style="width:100%; accent-color:#38bdf8;" oninput="window.updateDeesserParam(${chIdx}, 1, 'q', this.value)">
                                    </div>
                                </div>
                            </div>

                            <!-- BAND 2 CARD -->
                            <div style="background:#160b1c; border:1px solid #db2777; border-radius:8px; padding:8px 12px; display:flex; flex-direction:column; gap:6px;">
                                <div style="display:flex; justify-content:space-between; align-items:center;">
                                    <div style="display:flex; align-items:center; gap:8px;">
                                        <span style="font-size:0.78rem; font-weight:900; color:#f472b6;">✨ BAND 2 — HIGH / AIR HARSHNESS (4.5 kHz – 16 kHz)</span>
                                        <button class="tap-mode-btn ${ds.band2Enabled !== false ? 'active' : ''}" style="min-height:22px; padding:0 8px; font-size:0.62rem;" onclick="var d=getChannelRackState(${chIdx}).deesser; d.band2Enabled=(d.band2Enabled===false); dispatchDspParam(${chIdx},'loadesBand2En',d.band2Enabled?1:0); this.classList.toggle('active',d.band2Enabled); this.innerText=d.band2Enabled?'B2 ON':'B2 OFF'; if(window.drawDeesserModalCanvas)window.drawDeesserModalCanvas();">${ds.band2Enabled !== false ? 'B2 ON' : 'B2 OFF'}</button>
                                    </div>
                                    <span id="ds-b2-freq-lbl" style="font-family:monospace; font-size:0.82rem; font-weight:900; color:#fff;">${(ds.freq2/1000).toFixed(2)} kHz</span>
                                </div>
                                <div style="display:grid; grid-template-columns:1fr 1fr; gap:8px;">
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Center Freq</span><span id="ds-b2-freq-v">${ds.freq2} Hz</span></div>
                                        <input type="range" id="ds-b2-freq-sl" min="4500" max="16000" step="100" value="${ds.freq2}" style="width:100%; accent-color:#f472b6;" oninput="window.updateDeesserParam(${chIdx}, 2, 'freq', this.value)">
                                    </div>
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Threshold</span><span id="ds-b2-thresh-v">${ds.thresh2} dB</span></div>
                                        <input type="range" id="ds-b2-thresh-sl" min="-48" max="0" step="0.5" value="${ds.thresh2}" style="width:100%; accent-color:#f472b6;" oninput="window.updateDeesserParam(${chIdx}, 2, 'thresh', this.value)">
                                    </div>
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Max Reduction</span><span id="ds-b2-max-v">-${ds.max2} dB</span></div>
                                        <input type="range" id="ds-b2-max-sl" min="0" max="24" step="0.5" value="${ds.max2}" style="width:100%; accent-color:#f472b6;" oninput="window.updateDeesserParam(${chIdx}, 2, 'max', this.value)">
                                    </div>
                                    <div>
                                        <div style="display:flex; justify-content:space-between; font-size:0.65rem; color:#94a3b8;"><span>Release Speed</span><span id="ds-rel-v">${ds.release} ms</span></div>
                                        <input type="range" id="ds-rel-sl" min="10" max="250" step="5" value="${ds.release}" style="width:100%; accent-color:#fbbf24;" oninput="window.updateDeesserParam(${chIdx}, 0, 'release', this.value)">
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                `;
                if (typeof window.initDeesserModalCanvas === 'function') {
                    window.initDeesserModalCanvas(chIdx);
                }
            } else if (type === 'delay') {
                modal.classList.add("fullscreen", "fx-fullscreen");
                if (target === 'send-3' && window.delayDspState.presetIndex !== 1) window.cycleTalDelayPreset(1 - window.delayDspState.presetIndex);
                else if (target === 'send-4' && window.delayDspState.presetIndex !== 5) window.cycleTalDelayPreset(5 - window.delayDspState.presetIndex);
                title.innerHTML = `⏱️ TAL-DELAY-PRO Vintage Tape &amp; Digital Echo &bull; ${displayTargetName}`;
                body.innerHTML = renderDelayControls();
                initTalDelayRotaries();
                requestAnimationFrame(() => initTalDelayRotaries());
            } else if (type === 'reverb') {
                modal.classList.add("fullscreen", "fx-fullscreen");
                if (target === 'send-1') window.reverbDspState.engine = 'tal';
                else if (target === 'send-2') window.reverbDspState.engine = 'atlantis';
                title.innerHTML = `✨ Studio Reverb Suite &bull; ${displayTargetName}`;
                body.innerHTML = `
                    <div style="display:flex; gap:8px; margin-bottom:6px; flex-shrink:0;">
                        <button class="btn-top ${reverbDspState.engine === 'tal' ? 'active' : ''}" style="flex:1; min-height:36px; font-size:0.78rem;" onclick="switchReverbEngine('tal')">🏛️ TAL-Reverb-4 Vintage Plate &amp; 80s Hall</button>
                        <button class="btn-top ${reverbDspState.engine === 'atlantis' ? 'active' : ''}" style="flex:1; min-height:36px; font-size:0.78rem;" onclick="switchReverbEngine('atlantis')">🌌 Atlantis JSFX Acoustic Impulse Reverb</button>
                    </div>
                    <div id="reverb-engine-content" style="flex:1 1 0; min-height:0; display:flex; flex-direction:column;">
                        ${renderReverbControls()}
                    </div>
                `;
                if (reverbDspState.engine === 'tal') initTalRotaries();
                else initIrCanvas();
                requestAnimationFrame(() => {
                    if (reverbDspState.engine === 'tal') initTalRotaries();
                    else initIrCanvas();
                });
            } else if (type === 'room_cal' || type === 'roomcal') {
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
            } else if (type === 'tape') {
                if (typeof window.renderTapeMachineModal === 'function') {
                    window.renderTapeMachineModal(title, body, modal);
                }
            } else if (type === 'audio_hw') {
                const hw = window.audioHardwareState || { id: 'EVO4', name: 'Audient EVO 4', ins: 4, outs: 4, sr: 48000, buf: 128, cue: 1, patch: [1, 2, 0, 0, 0, 0, 0, 0], cards: [] };
                const cardsList = (Array.isArray(hw.cards) && hw.cards.length > 0)
                    ? hw.cards
                    : [{ id: hw.id || 'EVO4', name: hw.name || 'Audient EVO 4' }, { id: 'UMC1820', name: 'Behringer UMC1820 (18x20)' }, { id: 'dummy', name: 'Virtual Dummy Matrix (18x20)' }];
                title.innerHTML = `🔊 Universal Audio Interface &amp; Hardware I/O Matrix Setup`;
                body.innerHTML = `
                    <div style="display:flex; flex-direction:column; gap:12px; color:#e2e8f0;">
                        <div style="display:flex; justify-content:space-between; align-items:center; background:#0f172a; border:1px solid #334155; border-radius:8px; padding:10px 14px; flex-wrap:wrap; gap:8px;">
                            <div>
                                <div style="font-size:0.72rem; color:#94a3b8; font-weight:800;">ACTIVE ALSA / JACK HARDWARE ENGINE</div>
                                <div style="font-size:1.05rem; font-weight:900; color:#38bdf8;">${hw.name || hw.id} &bull; ${hw.ins} Inputs &times; ${hw.outs} Outputs @ ${hw.sr} Hz (${hw.buf} fpp)</div>
                            </div>
                            <span style="background:#065f46; border:1px solid #10b981; color:#a7f3d0; font-size:0.72rem; font-weight:900; padding:4px 10px; border-radius:5px;">32-BIT FLOAT DSP MATRIX ACTIVE</span>
                        </div>

                        <div class="dsp-control-grid" style="grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap:10px;">
                            <div class="knob-card" style="align-items:flex-start; text-align:left;">
                                <span class="knob-label">USB / ALSA Audio Interface</span>
                                <select id="hw-modal-card-select" class="soft-patch-select" style="width:100%; min-height:38px; font-size:0.8rem; margin-top:6px;">
                                    <option value="auto">🔄 Auto-Detect Connected USB Interface</option>
                                    ${cardsList.map(c => `<option value="${c.id}" ${c.id === hw.id ? 'selected' : ''}>🔌 ${c.name} (${c.id})</option>`).join('')}
                                    <option value="dummy" ${hw.id === 'dummy' ? 'selected' : ''}>🧪 Virtual Stand-Alone Matrix (18×20)</option>
                                </select>
                                <span style="font-size:0.65rem; color:#94a3b8; margin-top:4px;">Supports Audient EVO 4/8/16, Behringer UMC1820, Focusrite Scarlett, MOTU, RME, Pisound &amp; Class-Compliant USB 2.0/3.0 interfaces.</span>
                            </div>

                            <div class="knob-card" style="align-items:flex-start; text-align:left;">
                                <span class="knob-label">Sample Rate &amp; Latency Buffer</span>
                                <div style="display:flex; gap:6px; width:100%; margin-top:6px;">
                                    <select id="hw-modal-rate-select" class="soft-patch-select" style="flex:1; min-height:38px; font-size:0.78rem;">
                                        <option value="44100" ${Number(hw.sr) === 44100 ? 'selected' : ''}>44.1 kHz</option>
                                        <option value="48000" ${Number(hw.sr) === 48000 ? 'selected' : ''}>48.0 kHz (Pro)</option>
                                        <option value="96000" ${Number(hw.sr) === 96000 ? 'selected' : ''}>96.0 kHz</option>
                                    </select>
                                    <select id="hw-modal-buf-select" class="soft-patch-select" style="flex:1; min-height:38px; font-size:0.78rem;">
                                        <option value="64" ${Number(hw.buf) === 64 ? 'selected' : ''}>64 fpp (1.3ms)</option>
                                        <option value="128" ${Number(hw.buf) === 128 ? 'selected' : ''}>128 fpp (2.6ms)</option>
                                        <option value="256" ${Number(hw.buf) === 256 ? 'selected' : ''}>256 fpp (5.3ms)</option>
                                        <option value="512" ${Number(hw.buf) === 512 ? 'selected' : ''}>512 fpp (Safe)</option>
                                    </select>
                                </div>
                                <span style="font-size:0.65rem; color:#94a3b8; margin-top:4px;">Native 32-bit ALSA mode auto-adapts to 2-ch, 4-ch (EVO 4), or 18-ch (UMC1820) interfaces.</span>
                            </div>

                            <div class="knob-card" style="align-items:flex-start; text-align:left;">
                                <span class="knob-label">2-Out Interface Monitor Cue Routing</span>
                                <label style="display:flex; align-items:center; gap:8px; margin-top:8px; cursor:pointer; font-size:0.78rem; font-weight:700; color:#f8fafc;">
                                    <input type="checkbox" id="hw-modal-cue-toggle" ${hw.cue ? 'checked' : ''} style="width:20px; height:20px; accent-color:#10b981;">
                                    <span>Route Monitor SOLO/CUE to Main Out 1/2</span>
                                </label>
                                <span style="font-size:0.65rem; color:#94a3b8; margin-top:4px;">Allows auditioning Stage Monitors 1–6 on Headphone/Line Out 1/2 when using compact interfaces like the Audient EVO 4.</span>
                            </div>
                        </div>

                        <div style="background:#0f172a; border:1px solid #334155; border-radius:8px; padding:10px 12px;">
                            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
                                <span style="font-size:0.75rem; font-weight:900; color:#fbbf24;">QUICK 8-CHANNEL INPUT SOFT-PATCH MATRIX</span>
                                <div style="display:flex; gap:6px;">
                                    <button class="btn-top" style="padding:3px 8px; font-size:0.68rem; background:#0284c7;" onclick="for(var c=0;c<8;c++)handleSoftPatchChange(c, c<2?(c+1):0); openDspModal('audio_hw','master');">⚡ EVO 4 PRESET (CH 1-2 HW + CH 3-8 TAPE)</button>
                                    <button class="btn-top" style="padding:3px 8px; font-size:0.68rem; background:#374151;" onclick="for(var c=0;c<8;c++)handleSoftPatchChange(c, (c%Math.max(1,window.audioHardwareState.ins))+1); openDspModal('audio_hw','master');">🔢 1:1 sequential</button>
                                </div>
                            </div>
                            <div style="display:grid; grid-template-columns:repeat(8, 1fr); gap:6px;">
                                ${[0,1,2,3,4,5,6,7].map(ch => {
                                    const cur = (Array.isArray(hw.patch) && hw.patch[ch] !== undefined) ? Number(hw.patch[ch]) : (ch < 2 ? ch + 1 : 0);
                                    const maxIns = Math.max(2, Number(hw.ins) || 4);
                                    let opts = `<option value="0" ${cur === 0 ? 'selected' : ''}>VIRT/TAPE</option>`;
                                    for (let p = 1; p <= maxIns; p++) {
                                        opts += `<option value="${p}" ${cur === p ? 'selected' : ''}>HW IN ${p}</option>`;
                                    }
                                    return `
                                        <div style="background:#161b22; border:1px solid #30363d; border-radius:5px; padding:5px; text-align:center;">
                                            <div style="font-size:0.65rem; font-weight:800; color:#38bdf8; margin-bottom:4px;">CH ${ch + 1}</div>
                                            <select class="soft-patch-select" style="width:100%; font-size:0.65rem;" onchange="handleSoftPatchChange(${ch}, parseInt(this.value,10))">
                                                ${opts}
                                            </select>
                                        </div>`;
                                }).join('')}
                            </div>
                        </div>

                        <div style="display:flex; justify-content:space-between; align-items:center;">
                            <div id="hw-modal-status-msg" style="font-size:0.75rem; color:#94a3b8;">Changes take effect immediately across all connected tablets &amp; Web Portal clients.</div>
                            <button class="btn-top active" style="background:#059669; border-color:#10b981; padding:8px 20px; font-size:0.82rem; font-weight:900;" onclick="applyAudioInterfaceConfig()">✓ APPLY &amp; SYNC HARDWARE ENGINE</button>
                        </div>
                    </div>
                `;
            }
            overlay.classList.add("open");
        }
        function setCyberDenoiserBand(ch, bandIdx, valDb, fromRemote) {
            const c = Number(ch) || 0;
            const b = Math.max(0, Math.min(9, Number(bandIdx) || 0));
            const v = Math.max(-100, Math.min(0, Number(valDb) || -60));
            if (typeof window.getChannelRackState === 'function') {
                const dSt = window.getChannelRackState(c).denoiser;
                if (!dSt.bands) dSt.bands = [-60, -60, -60, -60, -60, -60, -60, -60, -60, -60];
                dSt.bands[b] = v;
            }
            const sl = document.getElementById(`cyber-denoise-bslider-${c}-${b}`);
            if (sl && parseFloat(sl.value) !== v) sl.value = v;
            const vl = document.getElementById(`cyber-denoise-bval-${c}-${b}`);
            if (vl) vl.innerText = Math.round(v) + 'dB';
            if (!fromRemote && typeof window.dispatchDspParam === 'function') {
                window.dispatchDspParam(c, 'denoiseBand_' + b, v);
            }
            drawCyberDenoiserCanvas(c);
        }

        function drawCyberDenoiserCanvas(ch) {
            const canvas = document.getElementById('cyber-denoiser-canvas');
            if (!canvas || canvas.offsetParent === null) return;
            const dpr = window.devicePixelRatio || 1;
            const cw = canvas.clientWidth || 720;
            const chPx = canvas.clientHeight || 220;
            if (canvas.width !== Math.round(cw * dpr) || canvas.height !== Math.round(chPx * dpr)) {
                canvas.width = Math.round(cw * dpr);
                canvas.height = Math.round(chPx * dpr);
            }
            const ctx = canvas.getContext('2d');
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
            const w = cw, h = chPx;
            const padX = 36, padTop = 28, padBot = 26;
            const plotW = Math.max(100, w - padX * 2);
            const plotH = Math.max(60, h - padTop - padBot);

            ctx.fillStyle = '#05030a';
            ctx.fillRect(0, 0, w, h);

            const cIdx = (ch !== undefined) ? Number(ch) : Number(canvas.dataset.ch || 0);
            const dSt = (typeof window.getChannelRackState === 'function') ? window.getChannelRackState(cIdx).denoiser : {
                enabled: true, sensitivity: 0, reductionPct: 85, listen: false, lowCut: true,
                bands: [-60, -60, -60, -60, -60, -60, -60, -60, -60, -60]
            };
            const bands = dSt.bands || [-60, -60, -60, -60, -60, -60, -60, -60, -60, -60];
            const sens = Number(dSt.sensitivity || 0);
            const live = Array.isArray(window.liveDenoiseBands) ? window.liveDenoiseBands : [0,0,0,0,0,0,0,0,0,0];
            const bandLabels = ['60Hz', '150Hz', '400Hz', '800Hz', '1.5k', '3k', '5k', '8k', '12k', '16k'];

            // 1. Subtle Cyber Grid (-90dB to 0dB)
            ctx.strokeStyle = 'rgba(168, 85, 247, 0.14)';
            ctx.lineWidth = 1;
            ctx.font = '9px monospace';
            ctx.fillStyle = 'rgba(192, 132, 252, 0.55)';
            ctx.textAlign = 'right';
            [-80, -60, -40, -20, 0].forEach(db => {
                const y = padTop + (Math.abs(db) / 100.0) * plotH;
                ctx.beginPath();
                ctx.moveTo(padX, y);
                ctx.lineTo(w - padX, y);
                ctx.stroke();
                ctx.fillText(db + 'dB', padX - 5, y + 3);
            });

            // 2. Purple Liquid Silk Background Wave (Authentic to Cyber-Denoiser PRO modgui/script.js)
            const phase = (performance.now() * 0.003);
            let totalEnergy = 0;
            for (let i = 0; i < 10; i++) totalEnergy += (Number(live[i]) || 0);
            const avgE = totalEnergy / 10.0;
            ctx.save();
            ctx.beginPath();
            ctx.strokeStyle = dSt.listen ? 'rgba(236, 72, 153, 0.38)' : 'rgba(168, 85, 247, 0.28)';
            ctx.lineWidth = 2.5;
            for (let x = padX; x <= w - padX; x += 6) {
                const normX = (x - padX) / plotW;
                const y = (padTop + plotH * 0.55)
                        + Math.sin(normX * 6.0 + phase) * (12.0 + avgE * 55.0)
                        + Math.cos(normX * 13.0 - phase * 1.4) * (6.0 + avgE * 28.0);
                if (x === padX) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
            ctx.restore();

            // 3. 10-Band Forensic Spectral Bars (Cyan -> Purple -> Magenta)
            const stepX = plotW / 9.0;
            const barW = Math.min(42, stepX * 0.56);
            for (let i = 0; i < 10; i++) {
                const cx = padX + i * stepX;
                const normL = Math.max(0, Math.min(1, Number(live[i]) || 0));
                const bh = normL * plotH;
                const by = padTop + plotH - bh;

                // Vertical guide line
                ctx.strokeStyle = 'rgba(168, 85, 247, 0.12)';
                ctx.beginPath();
                ctx.moveTo(cx, padTop);
                ctx.lineTo(cx, padTop + plotH);
                ctx.stroke();

                if (bh > 1) {
                    const grad = ctx.createLinearGradient(0, padTop + plotH, 0, padTop);
                    grad.addColorStop(0, 'rgba(0, 242, 255, 0.25)');
                    grad.addColorStop(0.55, 'rgba(168, 85, 247, 0.65)');
                    grad.addColorStop(1, 'rgba(236, 72, 153, 0.92)');
                    ctx.fillStyle = grad;
                    ctx.fillRect(cx - barW * 0.5, by, barW, bh);

                    ctx.fillStyle = '#00f2ff';
                    ctx.fillRect(cx - barW * 0.5, by, barW, 2);
                }

                ctx.fillStyle = '#cbd5e1';
                ctx.textAlign = 'center';
                ctx.font = 'bold 9.5px monospace';
                ctx.fillText(bandLabels[i], cx, h - 8);
            }

            // 4. Smooth 10-Band Subtractive Threshold Curve & Nodes
            const pts = [];
            for (let i = 0; i < 10; i++) {
                const cx = padX + i * stepX;
                const effDb = Math.max(-100, Math.min(0, (Number(bands[i]) ?? -60) + sens));
                const cy = padTop + (Math.abs(effDb) / 100.0) * plotH;
                pts.push({ x: cx, y: cy, rawDb: Number(bands[i]) ?? -60, effDb });
            }

            // Fill under threshold curve
            ctx.save();
            ctx.beginPath();
            ctx.moveTo(pts[0].x, pts[0].y);
            for (let i = 0; i < pts.length - 1; i++) {
                const xc = (pts[i].x + pts[i + 1].x) * 0.5;
                const yc = (pts[i].y + pts[i + 1].y) * 0.5;
                ctx.quadraticCurveTo(pts[i].x, pts[i].y, xc, yc);
            }
            ctx.lineTo(pts[9].x, pts[9].y);
            ctx.lineTo(pts[9].x, padTop + plotH);
            ctx.lineTo(pts[0].x, padTop + plotH);
            ctx.closePath();
            const curveFill = ctx.createLinearGradient(0, padTop, 0, padTop + plotH);
            curveFill.addColorStop(0, 'rgba(168, 85, 247, 0.24)');
            curveFill.addColorStop(1, 'rgba(168, 85, 247, 0.03)');
            ctx.fillStyle = curveFill;
            ctx.fill();

            // Stroke threshold curve
            ctx.beginPath();
            ctx.moveTo(pts[0].x, pts[0].y);
            for (let i = 0; i < pts.length - 1; i++) {
                const xc = (pts[i].x + pts[i + 1].x) * 0.5;
                const yc = (pts[i].y + pts[i + 1].y) * 0.5;
                ctx.quadraticCurveTo(pts[i].x, pts[i].y, xc, yc);
            }
            ctx.lineTo(pts[9].x, pts[9].y);
            ctx.strokeStyle = dSt.enabled ? '#a855f7' : '#64748b';
            ctx.lineWidth = 2.6;
            ctx.shadowBlur = 12;
            ctx.shadowColor = '#a855f7';
            ctx.stroke();
            ctx.restore();

            // Draw interactive 10 nodes
            for (let i = 0; i < pts.length; i++) {
                const p = pts[i];
                ctx.beginPath();
                ctx.arc(p.x, p.y, 6.5, 0, Math.PI * 2);
                ctx.fillStyle = '#0d0a1a';
                ctx.fill();
                ctx.lineWidth = 2.2;
                ctx.strokeStyle = '#00f2ff';
                ctx.stroke();

                ctx.fillStyle = '#e9d5ff';
                ctx.font = '8.5px monospace';
                ctx.textAlign = 'center';
                ctx.fillText(Math.round(p.effDb) + 'dB', p.x, Math.max(12, p.y - 10));
            }
        }

        function initCyberDenoiserCanvas(ch) {
            const canvas = document.getElementById('cyber-denoiser-canvas');
            if (!canvas) return;
            canvas.dataset.ch = ch;
            let dragging = false;

            function pointerToBand(e) {
                const rect = canvas.getBoundingClientRect();
                const clientX = (e.touches && e.touches.length > 0) ? e.touches[0].clientX : e.clientX;
                const clientY = (e.touches && e.touches.length > 0) ? e.touches[0].clientY : e.clientY;
                const x = clientX - rect.left;
                const y = clientY - rect.top;
                const padX = 36, padTop = 28, padBot = 26;
                const plotW = Math.max(100, rect.width - padX * 2);
                const plotH = Math.max(60, rect.height - padTop - padBot);
                const bandIdx = Math.max(0, Math.min(9, Math.round(((x - padX) / plotW) * 9)));
                const normY = Math.max(0, Math.min(1, (y - padTop) / plotH));
                const dbVal = Math.round(-normY * 100);
                setCyberDenoiserBand(ch, bandIdx, dbVal, false);
            }

            canvas.onpointerdown = (e) => {
                dragging = true;
                try { canvas.setPointerCapture(e.pointerId); } catch (_) {}
                pointerToBand(e);
            };
            canvas.onpointermove = (e) => {
                if (!dragging) return;
                pointerToBand(e);
            };
            canvas.onpointerup = canvas.onpointercancel = () => {
                dragging = false;
            };

            if (window.cyberDenoiserAnimId) cancelAnimationFrame(window.cyberDenoiserAnimId);
            function animLoop() {
                const cvs = document.getElementById('cyber-denoiser-canvas');
                if (!cvs || cvs.offsetParent === null) {
                    window.cyberDenoiserAnimId = null;
                    return;
                }
                drawCyberDenoiserCanvas(ch);
                window.cyberDenoiserAnimId = requestAnimationFrame(animLoop);
            }
            animLoop();
        }

        function updateDeesserParam(ch, band, key, rawVal) {
            const val = parseFloat(rawVal);
            const state = window.getChannelDspState(ch);
            if (!state.loades) return;
            const ld = state.loades;
            if (band === 1) {
                if (key === 'freq') {
                    ld.freq1 = Math.max(2000, Math.min(8500, val));
                    const lbl = document.getElementById('ds-modal-f1-lbl');
                    if (lbl) lbl.textContent = (ld.freq1 / 1000).toFixed(2) + ' kHz';
                    const sld = document.getElementById('ds-modal-f1-slider');
                    if (sld && Math.abs(parseFloat(sld.value) - ld.freq1) > 1) sld.value = ld.freq1;
                    window.sendDspParam('loades', 'loadesFreq1', ld.freq1, ch);
                } else if (key === 'thresh') {
                    ld.thresh1 = Math.max(-48, Math.min(0, val));
                    const lbl = document.getElementById('ds-modal-t1-lbl');
                    if (lbl) lbl.textContent = ld.thresh1.toFixed(1) + ' dB';
                    const sld = document.getElementById('ds-modal-t1-slider');
                    if (sld && Math.abs(parseFloat(sld.value) - ld.thresh1) > 0.1) sld.value = ld.thresh1;
                    window.sendDspParam('loades', 'loadesThresh1', ld.thresh1, ch);
                } else if (key === 'maxGr') {
                    ld.maxGr1 = Math.max(0, Math.min(24, val));
                    const lbl = document.getElementById('ds-modal-g1-lbl');
                    if (lbl) lbl.textContent = ld.maxGr1.toFixed(1) + ' dB';
                    window.sendDspParam('loades', 'loadesMaxGr1', ld.maxGr1, ch);
                } else if (key === 'q') {
                    ld.q1 = Math.max(0.7, Math.min(5.0, val));
                    const lbl = document.getElementById('ds-modal-q1-lbl');
                    if (lbl) lbl.textContent = ld.q1.toFixed(2);
                    window.sendDspParam('loades', 'loadesQ1', ld.q1, ch);
                }
            } else if (band === 2) {
                if (key === 'freq') {
                    ld.freq2 = Math.max(4500, Math.min(16000, val));
                    const lbl = document.getElementById('ds-modal-f2-lbl');
                    if (lbl) lbl.textContent = (ld.freq2 / 1000).toFixed(2) + ' kHz';
                    const sld = document.getElementById('ds-modal-f2-slider');
                    if (sld && Math.abs(parseFloat(sld.value) - ld.freq2) > 1) sld.value = ld.freq2;
                    window.sendDspParam('loades', 'loadesFreq2', ld.freq2, ch);
                } else if (key === 'thresh') {
                    ld.thresh2 = Math.max(-48, Math.min(0, val));
                    const lbl = document.getElementById('ds-modal-t2-lbl');
                    if (lbl) lbl.textContent = ld.thresh2.toFixed(1) + ' dB';
                    const sld = document.getElementById('ds-modal-t2-slider');
                    if (sld && Math.abs(parseFloat(sld.value) - ld.thresh2) > 0.1) sld.value = ld.thresh2;
                    window.sendDspParam('loades', 'loadesThresh2', ld.thresh2, ch);
                } else if (key === 'maxGr') {
                    ld.maxGr2 = Math.max(0, Math.min(24, val));
                    const lbl = document.getElementById('ds-modal-g2-lbl');
                    if (lbl) lbl.textContent = ld.maxGr2.toFixed(1) + ' dB';
                    window.sendDspParam('loades', 'loadesMaxGr2', ld.maxGr2, ch);
                } else if (key === 'q') {
                    ld.q2 = Math.max(0.7, Math.min(5.0, val));
                    const lbl = document.getElementById('ds-modal-q2-lbl');
                    if (lbl) lbl.textContent = ld.q2.toFixed(2);
                    window.sendDspParam('loades', 'loadesQ2', ld.q2, ch);
                }
            }
        }

        function setDeesserListenMode(ch, mode) {
            const state = window.getChannelDspState(ch);
            if (!state.loades) return;
            state.loades.listen = mode;
            window.sendDspParam('loades', 'loadesListen', mode, ch);
            for (let m = 0; m <= 3; m++) {
                const btn = document.getElementById(`ds-listen-btn-${m}`);
                if (!btn) continue;
                const active = m === mode;
                btn.style.background = active ? (m === 0 ? '#10b981' : '#f43f5e') : '#1e293b';
                btn.style.color = active ? '#ffffff' : '#94a3b8';
            }
        }

        function updateDeesserLiveMeters() {
            const curCh = window.dspRackCurrentChannel || 0;
            const gr1Arr = window.liveDsGr1 || [];
            const gr2Arr = window.liveDsGr2 || [];
            const gr1 = gr1Arr[curCh] || 0;
            const gr2 = gr2Arr[curCh] || 0;

            // Update inline DSP Rack twin GR bars
            const rackBar1 = document.getElementById(`rack-ds-gr1-${curCh}`);
            if (rackBar1) rackBar1.style.width = Math.min(100, (gr1 / 18.0) * 100).toFixed(1) + '%';
            const rackLbl1 = document.getElementById(`rack-ds-gr1-val-${curCh}`);
            if (rackLbl1) rackLbl1.textContent = '-' + gr1.toFixed(1) + ' dB';

            const rackBar2 = document.getElementById(`rack-ds-gr2-${curCh}`);
            if (rackBar2) rackBar2.style.width = Math.min(100, (gr2 / 18.0) * 100).toFixed(1) + '%';
            const rackLbl2 = document.getElementById(`rack-ds-gr2-val-${curCh}`);
            if (rackLbl2) rackLbl2.textContent = '-' + gr2.toFixed(1) + ' dB';

            // Update Fullscreen De-Esser Modal GR bars if open
            const modalBar1 = document.getElementById('modal-ds-gr1-bar');
            if (modalBar1) modalBar1.style.width = Math.min(100, (gr1 / 18.0) * 100).toFixed(1) + '%';
            const modalLbl1 = document.getElementById('modal-ds-gr1-lbl');
            if (modalLbl1) modalLbl1.textContent = '-' + gr1.toFixed(1) + ' dB';

            const modalBar2 = document.getElementById('modal-ds-gr2-bar');
            if (modalBar2) modalBar2.style.width = Math.min(100, (gr2 / 18.0) * 100).toFixed(1) + '%';
            const modalLbl2 = document.getElementById('modal-ds-gr2-lbl');
            if (modalLbl2) modalLbl2.textContent = '-' + gr2.toFixed(1) + ' dB';
        }

        function drawDeesserModalCanvas(ch) {
            const canvas = document.getElementById('deesser-rta-canvas');
            if (!canvas) return;
            const dpr = window.devicePixelRatio || 1;
            const w = canvas.clientWidth || 760;
            const h = canvas.clientHeight || 220;
            if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
                canvas.width = Math.round(w * dpr);
                canvas.height = Math.round(h * dpr);
            }
            const ctx = canvas.getContext('2d');
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

            const ld = window.getChannelDspState(ch).loades;
            const gr1 = (window.liveDsGr1 && window.liveDsGr1[ch]) || 0;
            const gr2 = (window.liveDsGr2 && window.liveDsGr2[ch]) || 0;

            ctx.fillStyle = '#070b14';
            ctx.fillRect(0, 0, w, h);

            const minF = 1000, maxF = 20000;
            const logSpan = Math.log(maxF / minF);
            const fToX = (f) => ((Math.log(Math.max(minF, Math.min(maxF, f)) / minF)) / logSpan) * w;
            const dbToY = (db) => {
                // Map +6dB..-48dB across canvas height
                const norm = (6.0 - Math.max(-48, Math.min(6, db))) / 54.0;
                return 14 + norm * (h - 34);
            };

            // Grid lines
            ctx.strokeStyle = 'rgba(148,163,184,0.12)';
            ctx.lineWidth = 1;
            [1000, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 12000, 16000].forEach(f => {
                const x = fToX(f);
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, h - 18);
                ctx.stroke();
                ctx.fillStyle = '#64748b';
                ctx.font = 'bold 10px monospace';
                ctx.textAlign = 'center';
                ctx.fillText((f / 1000) + 'k', x, h - 5);
            });
            [0, -12, -24, -36].forEach(db => {
                const y = dbToY(db);
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                ctx.strokeStyle = db === 0 ? 'rgba(56,189,248,0.35)' : 'rgba(148,163,184,0.1)';
                ctx.stroke();
                ctx.fillStyle = '#475569';
                ctx.textAlign = 'right';
                ctx.fillText(db + ' dB', w - 6, y - 3);
            });

            // High-Frequency RTA Spectrum (Bins covering 1kHz..20kHz)
            const rta = window.liveRtaData;
            if (rta && rta.length >= 32) {
                ctx.beginPath();
                ctx.moveTo(0, h - 18);
                for (let b = 0; b < rta.length; b++) {
                    const bf = 20 * Math.pow(1000, b / (rta.length - 1));
                    if (bf < 900) continue;
                    const x = fToX(bf);
                    const y = dbToY(rta[b] + 18); // Visual alignment with threshold scale
                    ctx.lineTo(x, y);
                }
                ctx.lineTo(w, h - 18);
                ctx.closePath();
                const grad = ctx.createLinearGradient(0, 14, 0, h - 18);
                grad.addColorStop(0, 'rgba(244,63,94,0.38)');
                grad.addColorStop(0.5, 'rgba(139,92,246,0.22)');
                grad.addColorStop(1, 'rgba(15,23,42,0.05)');
                ctx.fillStyle = grad;
                ctx.fill();
            }

            // Dynamic Suppression Notches Curve (Zero dB baseline with live Band 1 & Band 2 dips)
            const yZero = dbToY(0);
            ctx.beginPath();
            for (let x = 0; x <= w; x += 3) {
                const f = minF * Math.exp((x / w) * logSpan);
                let dipDb = 0;
                if (ld.band1On !== false) {
                    const oct1 = Math.log2(f / (ld.freq1 || 4800));
                    const bw1 = 0.7 / (ld.q1 || 1.8);
                    dipDb -= gr1 * Math.exp(-(oct1 * oct1) / (2 * bw1 * bw1));
                }
                if (ld.band2On !== false) {
                    const oct2 = Math.log2(f / (ld.freq2 || 9500));
                    const bw2 = 0.7 / (ld.q2 || 1.6);
                    dipDb -= gr2 * Math.exp(-(oct2 * oct2) / (2 * bw2 * bw2));
                }
                const y = dbToY(dipDb);
                if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.strokeStyle = '#38bdf8';
            ctx.lineWidth = 2.5;
            ctx.stroke();

            // Band 1 Node (Draggable Frequency + Threshold)
            const x1 = fToX(ld.freq1 || 4800);
            const y1 = dbToY(ld.thresh1 !== undefined ? ld.thresh1 : -22);
            ctx.strokeStyle = 'rgba(244,63,94,0.45)';
            ctx.setLineDash([4, 4]);
            ctx.beginPath(); ctx.moveTo(x1, 14); ctx.lineTo(x1, h - 18); ctx.stroke();
            ctx.setLineDash([]);
            ctx.beginPath();
            ctx.arc(x1, y1, 13, 0, Math.PI * 2);
            ctx.fillStyle = ld.band1On !== false ? '#f43f5e' : '#475569';
            ctx.fill();
            ctx.lineWidth = 2.5;
            ctx.strokeStyle = '#ffffff';
            ctx.stroke();
            ctx.fillStyle = '#ffffff';
            ctx.font = 'bold 10px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('B1', x1, y1 + 3.5);

            // Band 2 Node (Draggable Frequency + Threshold)
            const x2 = fToX(ld.freq2 || 9500);
            const y2 = dbToY(ld.thresh2 !== undefined ? ld.thresh2 : -24);
            ctx.strokeStyle = 'rgba(192,132,252,0.45)';
            ctx.setLineDash([4, 4]);
            ctx.beginPath(); ctx.moveTo(x2, 14); ctx.lineTo(x2, h - 18); ctx.stroke();
            ctx.setLineDash([]);
            ctx.beginPath();
            ctx.arc(x2, y2, 13, 0, Math.PI * 2);
            ctx.fillStyle = ld.band2On !== false ? '#a855f7' : '#475569';
            ctx.fill();
            ctx.lineWidth = 2.5;
            ctx.strokeStyle = '#ffffff';
            ctx.stroke();
            ctx.fillStyle = '#ffffff';
            ctx.font = 'bold 10px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('B2', x2, y2 + 3.5);
        }

        function initDeesserModalCanvas(ch) {
            const canvas = document.getElementById('deesser-rta-canvas');
            if (!canvas) return;
            let activeBand = 0;
            const minF = 1000, maxF = 20000;
            const logSpan = Math.log(maxF / minF);

            function pointerUpdate(e) {
                const rect = canvas.getBoundingClientRect();
                const x = Math.max(0, Math.min(rect.width, e.clientX - rect.left));
                const y = Math.max(14, Math.min(rect.height - 20, e.clientY - rect.top));
                const freq = Math.round(minF * Math.exp((x / rect.width) * logSpan));
                const db = +(6.0 - ((y - 14) / (rect.height - 34)) * 54.0).toFixed(1);
                if (activeBand === 1) {
                    updateDeesserParam(ch, 1, 'freq', freq);
                    updateDeesserParam(ch, 1, 'thresh', Math.min(0, Math.max(-48, db)));
                } else if (activeBand === 2) {
                    updateDeesserParam(ch, 2, 'freq', freq);
                    updateDeesserParam(ch, 2, 'thresh', Math.min(0, Math.max(-48, db)));
                }
            }

            canvas.onpointerdown = (e) => {
                const rect = canvas.getBoundingClientRect();
                const x = e.clientX - rect.left;
                const ld = window.getChannelDspState(ch).loades;
                const x1 = ((Math.log((ld.freq1 || 4800) / minF)) / logSpan) * rect.width;
                const x2 = ((Math.log((ld.freq2 || 9500) / minF)) / logSpan) * rect.width;
                activeBand = Math.abs(x - x1) <= Math.abs(x - x2) ? 1 : 2;
                try { canvas.setPointerCapture(e.pointerId); } catch (_) {}
                pointerUpdate(e);
            };
            canvas.onpointermove = (e) => {
                if (!activeBand) return;
                pointerUpdate(e);
            };
            canvas.onpointerup = canvas.onpointercancel = () => {
                activeBand = 0;
            };

            if (window.deesserAnimId) cancelAnimationFrame(window.deesserAnimId);
            function animLoop() {
                const cvs = document.getElementById('deesser-rta-canvas');
                if (!cvs || cvs.offsetParent === null) {
                    window.deesserAnimId = null;
                    return;
                }
                drawDeesserModalCanvas(ch);
                window.deesserAnimId = requestAnimationFrame(animLoop);
            }
            animLoop();
        }

        // LEVEL 2 CLOSE: Returns ONLY to Level 1 DSP Rack (never Level 0 mixer)
        // Triggered by: "← BACK TO RACK", "Close", "X", or backdrop overlay click
        function closeDspModal() {
            if (window.afdAnimId) {
                cancelAnimationFrame(window.afdAnimId);
                window.afdAnimId = null;
            }
            if (window.cyberDenoiserAnimId) {
                cancelAnimationFrame(window.cyberDenoiserAnimId);
                window.cyberDenoiserAnimId = null;
            }
            if (window.deesserAnimId) {
                cancelAnimationFrame(window.deesserAnimId);
                window.deesserAnimId = null;
                // Safety: automatically exit Solo Sibilance listen mode when closing modal
                const curCh = window.dspRackCurrentChannel || 0;
                const st = window.getChannelDspState && window.getChannelDspState(curCh);
                if (st && st.loades && st.loades.listen) {
                    st.loades.listen = 0;
                    window.sendDspParam('loades', 'loadesListen', 0, curCh);
                }
            }
            if (window.aoVuMeterAnimId) {
                cancelAnimationFrame(window.aoVuMeterAnimId);
                window.aoVuMeterAnimId = null;
            }
            const body = document.getElementById("modal-body");
            if (body) delete body.dataset.modalType;
            // Close ONLY Level 2 overlay — do NOT close Level 1
            const overlay = document.getElementById("dsp-modal-overlay");
            if (overlay) overlay.classList.remove("open");
            const auxModal = document.getElementById("auxModal");
            if (auxModal) auxModal.style.display = "none";
            const modal = document.getElementById("modal-container");
            if (modal) modal.classList.remove("fullscreen", "tape-fullscreen", "reaeq-modal", "reeq-modal");
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
    window.updateDeesserParam = updateDeesserParam;
    window.setDeesserListenMode = setDeesserListenMode;
    window.updateDeesserLiveMeters = updateDeesserLiveMeters;
    window.drawDeesserModalCanvas = drawDeesserModalCanvas;
    window.initDeesserModalCanvas = initDeesserModalCanvas;
    window.setCyberDenoiserBand = setCyberDenoiserBand;
    window.drawCyberDenoiserCanvas = drawCyberDenoiserCanvas;
    window.initCyberDenoiserCanvas = initCyberDenoiserCanvas;
    window.openDspModal = openDspModal;
    window.closeDspModal = closeDspModal;
    window.closeModalOnOverlay = closeModalOnOverlay;
})(window);

