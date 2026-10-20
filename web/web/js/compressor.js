// compressor.js - Unified Compressor, Limiter & Stage 1 1176 Preamp Engine for All Channels, Sends, Returns, Monitors & Master
(function(window) {
    'use strict';

    const compStates = {};
    window.currentCompModalTarget = 0;
    window.currentCompModalType = 'fet1176';

    function normKey(target) {
        if (typeof target === 'number') return String(target);
        if (typeof target === 'string' && target.startsWith('ch-')) return target.replace('ch-', '');
        return String(target ?? '0');
    }

    function getCompState(target) {
        const key = normKey(target);
        if (!compStates[key]) {
            const isMon = key.startsWith('mon-');
            const isSend = key.startsWith('send-');
            const isRet = key.startsWith('ret-');
            const isMaster = key === 'master';
            compStates[key] = {
                target: key,
                compBypass: false,
                compModel: isMon ? 'limiter' : (isRet || isMaster ? 'lala' : 'fetish76'),
                preamp1176: { enabled: true, drive: 6.0, trim: 0.0, clipMode: 'soft', clipping: false, livePeakDb: -60.0 },
                fetish76: {
                    power: true,
                    input: isSend ? 34 : 30,
                    output: 20,
                    attack: isSend ? 5 : 3,
                    release: isSend ? 4 : 4,
                    ratio: isSend ? '8' : '4',
                    allButtons: false
                },
                lala: {
                    power: true,
                    gain: 40,
                    peakReduction: isRet ? 48 : 42,
                    hpf: 60,
                    mf: 1200,
                    mg: 0,
                    hfFlat: 80,
                    mix: 100,
                    ext: isSend,
                    limitMode: false
                },
                limiter: {
                    power: true,
                    threshold: isMon ? -1.5 : -1.0,
                    ceiling: isMon ? -1.0 : -0.2,
                    drive: isMon ? 1.5 : 2.0,
                    release: 25,
                    crush: false
                }
            };
        }
        return compStates[key];
    }

    function sendCompParam(target, param, value) {
        const key = normKey(target);
        const chNum = /^[0-7]$/.test(key) ? parseInt(key, 10) : key;
        if (typeof window.dispatchDspParam === 'function') {
            window.dispatchDspParam(chNum, param, value);
        }
    }

    window.toggleCompBypass = function(target) {
        const key = normKey(target);
        const cs = getCompState(key);
        cs.compBypass = !cs.compBypass;
        sendCompParam(key, 'compBypass', cs.compBypass ? 1 : 0);
        ['rack-comp-byp-btn-', 'modal-comp-byp-btn-'].forEach(prefix => {
            const btn = document.getElementById(prefix + key);
            if (btn) {
                btn.classList.toggle('active', !cs.compBypass);
                btn.style.background = cs.compBypass ? '#7f1d1d' : '#059669';
                btn.style.borderColor = cs.compBypass ? '#ef4444' : '#10b981';
                btn.style.color = '#ffffff';
                btn.innerText = cs.compBypass ? '🔇 COMP BYPASSED' : '⚡ COMP ACTIVE';
            }
        });
    };

    function getTargetLiveGr(target) {
        const key = normKey(target);
        if (/^[0-7]$/.test(key)) {
            return (window.channelGrValues && window.channelGrValues[parseInt(key, 10)]) || 0.0;
        }
        return (window.busGrValues && window.busGrValues[key]) || 0.0;
    }

    // --- STAGE 1: 1176 CLASS-A FET PREAMP EMULATION (PRE-REGAIN) ---
    window.setPreampDrive = function(ch, val) {
        const st = getCompState(ch);
        let dVal = parseFloat(val);
        if (window.touchGovernor) dVal = window.touchGovernor.process(`preamp-drive-${ch}`, dVal);
        st.preamp1176.drive = parseFloat(dVal.toFixed(1));
        if (window.getChannelRackState) window.getChannelRackState(ch).preamp1176.drive = st.preamp1176.drive;
        sendCompParam(ch, 'preamp1176Drive', st.preamp1176.drive);
        ['rack-preamp-drive-val-', 'modal-preamp-drive-val-', 'full-preamp-drive-val-'].forEach(p => {
            const el = document.getElementById(p + ch);
            if (el) el.innerText = '+' + st.preamp1176.drive.toFixed(1) + ' dB';
        });
        window.updatePreampClipIndicator(ch, st.preamp1176.livePeakDb);
    };

    window.setPreampTrim = function(ch, val) {
        const st = getCompState(ch);
        let tVal = parseFloat(val);
        if (window.touchGovernor) tVal = window.touchGovernor.process(`preamp-trim-${ch}`, tVal);
        st.preamp1176.trim = parseFloat(tVal.toFixed(1));
        if (window.getChannelRackState) window.getChannelRackState(ch).preamp1176.trim = st.preamp1176.trim;
        sendCompParam(ch, 'preamp1176Trim', st.preamp1176.trim);
        const txt = (st.preamp1176.trim >= 0 ? '+' : '') + st.preamp1176.trim.toFixed(1) + ' dB';
        ['rack-preamp-trim-val-', 'modal-preamp-trim-val-', 'full-preamp-trim-val-'].forEach(p => {
            const el = document.getElementById(p + ch);
            if (el) el.innerText = txt;
        });
        window.updatePreampClipIndicator(ch, st.preamp1176.livePeakDb);
    };

    window.togglePreampEnabled = function(ch) {
        const st = getCompState(ch);
        st.preamp1176.enabled = !st.preamp1176.enabled;
        if (window.getChannelRackState) window.getChannelRackState(ch).preamp1176.enabled = st.preamp1176.enabled;
        sendCompParam(ch, 'preamp1176Enabled', st.preamp1176.enabled);
        ['rack-preamp-en-btn-', 'modal-preamp-en-btn-', 'full-preamp-en-btn-'].forEach(p => {
            const btn = document.getElementById(p + ch);
            if (btn) {
                btn.classList.toggle('active', st.preamp1176.enabled);
                btn.classList.toggle('off', !st.preamp1176.enabled);
                btn.innerText = st.preamp1176.enabled ? 'ON' : 'BYPASS';
            }
        });
        window.updatePreampClipIndicator(ch, st.preamp1176.livePeakDb);
    };

    window.togglePreampClipMode = function(ch) {
        const st = getCompState(ch);
        st.preamp1176.clipMode = (st.preamp1176.clipMode === 'soft') ? 'hard' : 'soft';
        if (window.getChannelRackState) window.getChannelRackState(ch).preamp1176.clipMode = st.preamp1176.clipMode;
        sendCompParam(ch, 'preamp1176ClipMode', st.preamp1176.clipMode);
        ['rack-preamp-mode-btn-', 'modal-preamp-mode-btn-', 'full-preamp-mode-btn-'].forEach(p => {
            const btn = document.getElementById(p + ch);
            if (btn) {
                btn.innerText = st.preamp1176.clipMode === 'soft' ? 'SOFT FET SAT' : 'HARD CLIP';
                btn.classList.toggle('active', st.preamp1176.clipMode === 'hard');
            }
        });
        window.updatePreampClipIndicator(ch, st.preamp1176.livePeakDb);
    };

    window.updatePreampClipIndicator = function(ch, livePeakDb, backendStatus) {
        const st = getCompState(ch);
        const p = st.preamp1176;
        if (typeof livePeakDb === 'number') p.livePeakDb = livePeakDb;
        const inputDb = (typeof p.livePeakDb === 'number') ? p.livePeakDb : -60.0;
        const internalStageDb = inputDb + p.drive;
        const ceilingDb = (p.clipMode === 'hard') ? -1.0 : -3.0;
        const isClipping = p.enabled && ((backendStatus === 2) || (internalStageDb >= ceilingDb));
        const isSaturating = p.enabled && !isClipping && ((backendStatus === 1) || (internalStageDb >= ceilingDb - 6.0));
        p.clipping = isClipping;
        ['rack-preamp-clip-led-', 'modal-preamp-clip-led-', 'strip-preamp-led-', 'full-preamp-clip-led-'].forEach(prefix => {
            const led = document.getElementById(prefix + ch);
            if (!led) return;
            if (!p.enabled) {
                led.style.background = '#334155'; led.style.color = '#94a3b8'; led.innerText = 'BYP';
            } else if (isClipping) {
                led.style.background = '#ef4444'; led.style.color = '#fff'; led.innerText = 'CLIP!';
            } else if (isSaturating) {
                led.style.background = '#f59e0b'; led.style.color = '#000'; led.innerText = 'FET SAT';
            } else {
                led.style.background = '#10b981'; led.style.color = '#052e16'; led.innerText = 'CLEAN';
            }
        });
        const satBarId = `preamp-sat-bar-${ch}`;
        if (document.getElementById(`${satBarId}-readout`) && typeof window.updateDigitalGrBar === 'function') {
            const satAmount = p.enabled ? Math.max(0, Math.min(24, (internalStageDb + 48.0) * 0.5)) : 0.0;
            window.updateDigitalGrBar(satBarId, satAmount);
        }
    };

    window.updatePreamp1176ClipTelemetry = function(ch, linearLevel, backendStatus) {
        const db = (linearLevel > 0.00001) ? (20.0 * Math.log10(linearLevel)) : -60.0;
        window.updatePreampClipIndicator(ch, db, backendStatus);
    };

    window.renderPreampStageRow = function(ch, state, isModal) {
        const p = getCompState(ch).preamp1176;
        const prefix = isModal ? 'modal' : 'rack';
        return `
            <!-- STAGE 1: 1176 FET Class-A Preamp Emulation (Feeds Stage 2 Regain) -->
            <div class="rack-row" data-rack-group="input_fx" style="background:linear-gradient(90deg, #1c1917 0%, #0f172a 100%); border-color:#d97706;">
                <div class="rack-header-area">
                    <span class="rack-num" style="color:#fbbf24; border-color:#d97706;">1</span>
                    <div>
                        <div class="rack-name" style="color:#fef3c7; display:flex; align-items:center; gap:8px;">
                            <span>🔥 1176 FET Class-A Preamp Emulation</span>
                            <span id="${prefix}-preamp-clip-led-${ch}" style="font-size:0.65rem; padding:1px 6px; border-radius:4px; background:#10b981; color:#052e16; font-weight:900;">CLEAN</span>
                        </div>
                        <div class="rack-sub" style="color:#fbbf24;">Stage 1 Saturation &amp; Output Trim &rarr; Feeds Stage 2 REGAIN</div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <button class="tap-mode-btn ${p.enabled ? 'active' : ''}" id="${prefix}-preamp-en-btn-${ch}" style="min-height:36px; padding:0 10px;" onclick="togglePreampEnabled(${ch})">${p.enabled ? 'ON' : 'BYPASS'}</button>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">1176 Drive</span><input type="range" class="rack-mini-slider" style="width:95px;" min="0" max="36" step="0.5" value="${p.drive}" oninput="setPreampDrive(${ch}, this.value)">
                        <span class="rack-mini-val" id="${prefix}-preamp-drive-val-${ch}">+${p.drive.toFixed(1)} dB</span>
                    </div>
                    <button class="tap-mode-btn ${p.clipMode === 'hard' ? 'active' : ''}" id="${prefix}-preamp-mode-btn-${ch}" style="min-height:36px; padding:0 10px;" onclick="togglePreampClipMode(${ch})">
                        ${p.clipMode === 'soft' ? 'SOFT FET SAT' : 'HARD CLIP'}
                    </button>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Output Trim</span><input type="range" class="rack-mini-slider" style="width:95px;" min="-24" max="12" step="0.5" value="${p.trim}" oninput="setPreampTrim(${ch}, this.value)">
                        <span class="rack-mini-val" id="${prefix}-preamp-trim-val-${ch}">${p.trim >= 0 ? '+' : ''}${p.trim.toFixed(1)} dB</span>
                    </div>
                </div>
                <div class="rack-actions" style="display:flex; align-items:center; gap:8px;">
                    <button class="btn-rack-open" style="background:#b45309; border-color:#f59e0b; color:#fef3c7;" onclick="openDspModal('preamp1176', ${ch});">1176 PRE GUI &rarr;</button>
                </div>
            </div>
        `;
    };

    // --- STAGE 6: INLINE RACK COMPRESSOR ROW (FET 76 | LALA | 2-ROW DUAL 76 -> LA) ---
    window.renderRackCompressorRow = function(ch, rackState) {
        const key = normKey(ch);
        const cs = getCompState(ch);
        const model = (rackState && rackState.compModel) ? rackState.compModel : cs.compModel;
        cs.compModel = model;
        const f = cs.fetish76;
        const l = cs.lala;
        const byp = !!cs.compBypass;

        let inlineHtml = '';
        let actionBtns = '';
        if (model === 'dual') {
            inlineHtml = `
                <div class="dual-comp-container" style="display:flex; flex-direction:column; gap:6px; width:100%;">
                    <div class="dual-comp-stage fet" style="display:flex; align-items:center; gap:10px; background:rgba(2,132,199,0.12); border:1px solid #0284c7; border-radius:6px; padding:4px 8px;">
                        <button class="tap-mode-btn ${f.power ? 'active' : ''}" style="min-height:26px; padding:0 8px; font-size:0.65rem;" onclick="var c=getCompState('${ch}').fetish76; c.power=!c.power; setCompParam('${ch}','fetish76','power',c.power?1:0); this.classList.toggle('active',c.power); this.innerText=c.power?'1. FET 76: ON':'1. FET 76: BYP';">${f.power ? '1. FET 76: ON' : '1. FET 76: BYP'}</button>
                        <div class="rack-mini-knob">
                            <span class="rack-mini-label">Input</span>
                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="60" step="1" value="${f.input}" oninput="setCompParam('${ch}','fetish76','input',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                            <span class="rack-mini-val">${f.input} dB</span>
                        </div>
                        <div class="rack-mini-knob">
                            <span class="rack-mini-label">Output</span>
                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="48" step="1" value="${f.output}" oninput="setCompParam('${ch}','fetish76','output',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                            <span class="rack-mini-val">${f.output} dB</span>
                        </div>
                        <div class="rack-mini-knob">
                            <span class="rack-mini-label">Ratio</span>
                            <button class="tap-mode-btn active" style="min-height:28px; padding:0 8px; font-size:0.7rem;" onclick="cycleFetRatioInline('${ch}', this)">${f.allButtons ? 'ALL' : f.ratio + ':1'}</button>
                        </div>
                    </div>
                    <div class="dual-comp-stage opto" style="display:flex; align-items:center; gap:10px; background:rgba(180,83,9,0.14); border:1px solid #d97706; border-radius:6px; padding:4px 8px;">
                        <button class="tap-mode-btn ${l.power ? 'active' : ''}" style="min-height:26px; padding:0 8px; font-size:0.65rem;" onclick="var c=getCompState('${ch}').lala; c.power=!c.power; setCompParam('${ch}','lala','power',c.power?1:0); this.classList.toggle('active',c.power); this.innerText=c.power?'2. LALA: ON':'2. LALA: BYP';">${l.power ? '2. LALA: ON' : '2. LALA: BYP'}</button>
                        <div class="rack-mini-knob">
                            <span class="rack-mini-label">Peak Red.</span>
                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="100" step="1" value="${l.peakReduction}" oninput="setCompParam('${ch}','lala','peakReduction',this.value); this.nextElementSibling.innerText=this.value">
                            <span class="rack-mini-val">${l.peakReduction}</span>
                        </div>
                        <div class="rack-mini-knob">
                            <span class="rack-mini-label">Gain</span>
                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="100" step="1" value="${l.gain}" oninput="setCompParam('${ch}','lala','gain',this.value); this.nextElementSibling.innerText=this.value">
                            <span class="rack-mini-val">${l.gain}</span>
                        </div>
                        <button class="tap-mode-btn ${l.limitMode ? 'active' : ''}" style="min-height:28px; padding:0 8px; font-size:0.7rem;" onclick="toggleLalaModeInline('${ch}', this)">${l.limitMode ? 'LIMIT' : 'COMPRESS'}</button>
                    </div>
                </div>
            `;
            actionBtns = `<button class="btn-rack-open" style="background:linear-gradient(135deg, #0284c7 0%, #b45309 100%); border-color:#38bdf8;" onclick="openDspModal('dual', ${ch});">DUAL GUI &rarr;</button>`;
        } else if (model === 'lala') {
            inlineHtml = `
                <div class="rack-mini-knob">
                    <span class="rack-mini-label">Peak Red.</span>
                    <input type="range" class="rack-mini-slider" style="width:90px;" min="0" max="100" step="1" value="${l.peakReduction}" oninput="setCompParam('${ch}','lala','peakReduction',this.value); this.nextElementSibling.innerText=this.value">
                    <span class="rack-mini-val">${l.peakReduction}</span>
                </div>
                <div class="rack-mini-knob">
                    <span class="rack-mini-label">Makeup Gain</span>
                    <input type="range" class="rack-mini-slider" style="width:90px;" min="0" max="100" step="1" value="${l.gain}" oninput="setCompParam('${ch}','lala','gain',this.value); this.nextElementSibling.innerText=this.value">
                    <span class="rack-mini-val">${l.gain}</span>
                </div>
                <button class="tap-mode-btn ${l.limitMode ? 'active' : ''}" style="min-height:34px; padding:0 10px;" onclick="toggleLalaModeInline('${ch}', this)">${l.limitMode ? 'LIMIT' : 'COMPRESS'}</button>
            `;
            actionBtns = `<button class="btn-rack-open" style="background:#b45309; border-color:#f59e0b;" onclick="openDspModal('la2a', ${ch});">LALA GUI &rarr;</button>`;
        } else {
            inlineHtml = `
                <div class="rack-mini-knob">
                    <span class="rack-mini-label">76 Input</span>
                    <input type="range" class="rack-mini-slider" style="width:80px;" min="0" max="60" step="1" value="${f.input}" oninput="setCompParam('${ch}','fetish76','input',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                    <span class="rack-mini-val">${f.input} dB</span>
                </div>
                <div class="rack-mini-knob">
                    <span class="rack-mini-label">76 Output</span>
                    <input type="range" class="rack-mini-slider" style="width:80px;" min="0" max="48" step="1" value="${f.output}" oninput="setCompParam('${ch}','fetish76','output',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                    <span class="rack-mini-val">${f.output} dB</span>
                </div>
                <div class="rack-mini-knob">
                    <span class="rack-mini-label">Attack</span>
                    <input type="range" class="rack-mini-slider" style="width:65px;" min="1" max="7" step="1" value="${f.attack}" oninput="setCompParam('${ch}','fetish76','attack',this.value); this.nextElementSibling.innerText=this.value">
                    <span class="rack-mini-val">${f.attack}</span>
                </div>
                <div class="rack-mini-knob">
                    <span class="rack-mini-label">Release</span>
                    <input type="range" class="rack-mini-slider" style="width:65px;" min="1" max="7" step="1" value="${f.release}" oninput="setCompParam('${ch}','fetish76','release',this.value); this.nextElementSibling.innerText=this.value">
                    <span class="rack-mini-val">${f.release}</span>
                </div>
            `;
            actionBtns = `<button class="btn-rack-open" style="background:#0284c7; border-color:#38bdf8;" onclick="openDspModal('fet1176', ${ch});">AO 76 GUI &rarr;</button>`;
        }

        return `
            <!-- STAGE 6: Switchable / Dual Compressor -->
            <div class="rack-row" data-rack-group="dynamics" style="border-color:#374151; background:linear-gradient(90deg, rgba(30,41,59,0.3) 0%, rgba(15,23,42,0.6) 100%);">
                <div class="rack-header-area">
                    <span class="rack-num" style="color:#38bdf8; border-color:#0284c7;">6</span>
                    <div>
                        <div class="rack-name" style="color:#e0f2fe; display:flex; align-items:center; gap:8px;">
                            <span>⚡ Dynamics: Compressor Stacking</span>
                        </div>
                        <div class="comp-model-group" style="margin-top:4px; display:flex; gap:4px; align-items:center;">
                            <button class="tap-mode-btn ${!byp ? 'active' : ''}" id="rack-comp-byp-btn-${key}" style="min-height:24px; padding:1px 8px; font-size:0.64rem; font-weight:900; background:${byp ? '#7f1d1d' : '#059669'}; border-color:${byp ? '#ef4444' : '#10b981'}; color:#fff;" onclick="toggleCompBypass('${ch}')">${byp ? '🔇 COMP BYPASSED' : '⚡ COMP ACTIVE'}</button>
                            <button class="comp-model-btn ${model === 'fetish76' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'fetish76')">FET 76</button>
                            <button class="comp-model-btn ${model === 'lala' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'lala')">LALA</button>
                            <button class="comp-model-btn ${model === 'dual' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'dual')">DUAL (76 ➔ LA)</button>
                        </div>
                    </div>
                </div>
                <div class="rack-inline-controls" style="flex:1;">${inlineHtml}</div>
                <div class="rack-actions" style="display:flex; gap:6px;">${actionBtns}</div>
            </div>
        `;
    };

    window.setCompParam = function(target, modelKey, param, val) {
        const cs = getCompState(target);
        const num = parseFloat(val);
        cs[modelKey][param] = Number.isInteger(num) ? parseInt(val, 10) : num;
        sendCompParam(target, `${modelKey}_${param}`, cs[modelKey][param]);
    };

    window.cycleFetRatioInline = function(target, btnEl) {
        const f = getCompState(target).fetish76;
        const order = ['4', '8', '12', '20', 'ALL'];
        const cur = f.allButtons ? 'ALL' : f.ratio;
        const next = order[(order.indexOf(cur) + 1) % order.length];
        if (next === 'ALL') { f.allButtons = true; } else { f.allButtons = false; f.ratio = next; }
        if (btnEl) btnEl.innerText = f.allButtons ? 'ALL' : f.ratio + ':1';
        sendCompParam(target, 'fetRatio', next);
    };

    window.toggleLalaModeInline = function(target, btnEl) {
        const l = getCompState(target).lala;
        l.limitMode = !l.limitMode;
        if (btnEl) {
            btnEl.classList.toggle('active', l.limitMode);
            btnEl.innerText = l.limitMode ? 'LIMIT' : 'COMPRESS';
        }
        sendCompParam(target, 'lalaMode', l.limitMode ? 'limit' : 'compress');
    };

    window.setFetishRatio = function(ratioStr, target) {
        const t = target !== undefined ? target : window.currentCompModalTarget;
        const f = getCompState(t).fetish76;
        f.ratio = ratioStr;
        f.allButtons = false;
        sendCompParam(t, 'fetRatio', ratioStr);
        document.querySelectorAll('.ao-ratio-btn').forEach(btn => {
            if (btn.classList.contains('all-in')) btn.classList.remove('active');
            else btn.classList.toggle('active', btn.innerText.startsWith(ratioStr + ':'));
        });
    };

    window.toggleFetishAllButtons = function(target) {
        const t = target !== undefined ? target : window.currentCompModalTarget;
        const f = getCompState(t).fetish76;
        f.allButtons = !f.allButtons;
        sendCompParam(t, 'fetAllButtons', f.allButtons);
        document.querySelectorAll('.ao-ratio-btn').forEach(btn => {
            btn.classList.toggle('active', f.allButtons || (!f.allButtons && btn.innerText.startsWith(f.ratio + ':')));
        });
    };

    // --- UNIFIED FULL-SCREEN MODAL RENDERER FOR ALL CHANNELS, SENDS, RETURNS, MONITORS & MASTER ---
    window.renderUnifiedCompressorModal = function(type, target, displayTargetName, modal, title, body) {
        const key = normKey(target);
        const cs = getCompState(key);
        window.currentCompModalTarget = key;

        // Map 'comp' (Sends/Returns) to the target's active compressor model
        let activeType = type;
        if (type === 'comp') {
            activeType = cs.compModel === 'lala' ? 'la2a' : (cs.compModel === 'dual' ? 'dual' : (cs.compModel === 'limiter' ? 'limiter' : 'fet1176'));
        }
        window.currentCompModalType = activeType;
        modal.classList.add('ao-modal');
        const byp = !!cs.compBypass;

        // Top mode selector bar so ANY Channel, Send, Return, Monitor, or Master can switch models or bypass live
        const modeBarHtml = activeType === 'preamp1176' ? '' : `
            <div style="display:flex; justify-content:space-between; align-items:center; background:#090d14; border:1px solid #1e293b; border-radius:8px; padding:6px 12px; margin-bottom:10px; flex-wrap:wrap; gap:8px;">
                <div style="display:flex; align-items:center; gap:10px;">
                    <span style="font-size:0.72rem; font-weight:800; color:#94a3b8;">UNIFIED DYNAMICS ENGINE (${displayTargetName}):</span>
                    <button class="tap-mode-btn ${!byp ? 'active' : ''}" id="modal-comp-byp-btn-${key}" style="min-height:32px; padding:2px 14px; font-size:0.75rem; font-weight:900; background:${byp ? '#7f1d1d' : '#059669'}; border-color:${byp ? '#ef4444' : '#10b981'}; color:#fff;" onclick="toggleCompBypass('${key}')">${byp ? '🔇 COMP BYPASSED (TAP TO ENGAGE)' : '⚡ COMP ACTIVE (TAP TO BYPASS)'}</button>
                </div>
                <div style="display:flex; gap:6px;">
                    <button class="comp-model-btn ${activeType === 'fet1176' || activeType === 'fetish' ? 'active' : ''}" onclick="switchUnifiedCompModalMode('fet1176', '${key}')">FETISH 76 (PEAK)</button>
                    <button class="comp-model-btn ${activeType === 'la2a' || activeType === 'lala' ? 'active' : ''}" onclick="switchUnifiedCompModalMode('la2a', '${key}')">LALA (OPTO)</button>
                    <button class="comp-model-btn ${activeType === 'dual' ? 'active' : ''}" onclick="switchUnifiedCompModalMode('dual', '${key}')">DUAL (76 ➔ LALA)</button>
                    <button class="comp-model-btn ${activeType === 'limiter' ? 'active' : ''}" onclick="switchUnifiedCompModalMode('limiter', '${key}')">BUSTER LIMITER</button>
                </div>
            </div>
        `;
        return window.renderAoModalContent(activeType, key, displayTargetName, displayTargetName, modal, title, body, cs, modeBarHtml);
    };

    window.switchUnifiedCompModalMode = function(newType, targetKey) {
        const cs = getCompState(targetKey);
        cs.compModel = newType === 'la2a' ? 'lala' : (newType === 'dual' ? 'dual' : (newType === 'limiter' ? 'limiter' : 'fetish76'));
        if (/^[0-7]$/.test(targetKey) && typeof window.setChannelCompModel === 'function' && cs.compModel !== 'limiter') {
            window.setChannelCompModel(parseInt(targetKey, 10), cs.compModel);
        }
        window.openDspModal(newType, /^[0-7]$/.test(targetKey) ? parseInt(targetKey, 10) : targetKey);
    };

    window.getCompState = getCompState;
    window.getTargetLiveGr = getTargetLiveGr;
})(window);
