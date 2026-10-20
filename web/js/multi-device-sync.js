// multi-device-sync.js - Real-Time Multi-Device State & UI Synchronization Engine
// Synchronizes faders, SOF sends, knobs, sliders, EQ bands, 1176 preamps, compressors, FX, and tape controls across all connected tablets & browsers.
(function(window) {
    'use strict';

    function gainToPercent(g) {
        if (!g || g <= 0.0001) return 0;
        const db = 20.0 * Math.log10(g);
        if (db >= 0) return Math.min(100, 75.0 + (db / 10.0) * 25.0);
        if (db >= -20.0) return 40.0 + ((db + 20.0) / 20.0) * 35.0;
        if (db >= -60.0) return ((db + 60.0) / 40.0) * 40.0;
        return 0;
    }

    function withRemoteSyncLock(fn) {
        const prev = window._applyingRemoteSync;
        window._applyingRemoteSync = true;
        try { fn(); } catch (e) { console.warn('[MultiSync]', e); }
        finally { window._applyingRemoteSync = prev; }
    }

    function refreshAoKnobById(id, val, fmtFn) {
        const cvs = document.getElementById(id);
        if (cvs && typeof cvs._refreshAoKnob === 'function') cvs._refreshAoKnob(val);
        const valEl = document.getElementById(id + '-val');
        if (valEl && fmtFn) valEl.innerText = fmtFn(val);
    }

    function normalizeTargetKey(raw) {
        if (raw === undefined || raw === null) return 0;
        if (typeof raw === 'number') return raw;
        const s = String(raw);
        if (s.startsWith('ch-')) {
            const n = parseInt(s.slice(3), 10);
            if (!isNaN(n)) return n;
        }
        if (!isNaN(Number(s)) && s.indexOf('-') === -1 && s !== 'master') {
            return Number(s);
        }
        return s;
    }

    function applyDspParamSync(msg) {
        const param = msg.param;
        const val = msg.value;
        if (!param) return;
        const rawTarget = msg.channel !== undefined ? msg.channel : (msg.target !== undefined ? msg.target : msg.bus);
        const target = normalizeTargetKey(rawTarget);
        const ch = (typeof target === 'number') ? target : 0;

        // 1. Stage 1: 1176 Class-A Preamp Emulation
        if (param.startsWith('preamp1176')) {
            const cSt = (typeof window.getCompState === 'function') ? window.getCompState(ch) : null;
            const rSt = (typeof window.getChannelRackState === 'function') ? window.getChannelRackState(ch) : null;
            if (cSt && !cSt.preamp1176) cSt.preamp1176 = { enabled: true, drive: 6.0, trim: 0.0, clipMode: 'soft' };
            if (rSt && !rSt.preamp1176) rSt.preamp1176 = { enabled: true, drive: 6.0, trim: 0.0, clipMode: 'soft' };
            const pComp = cSt ? cSt.preamp1176 : {};
            const pRack = rSt ? rSt.preamp1176 : {};

            if (param === 'preamp1176Enabled') {
                const en = !!val;
                pComp.enabled = en;
                pRack.enabled = en;
                ['rack-preamp-en-btn-', 'modal-preamp-en-btn-', 'full-preamp-en-btn-'].forEach(p => {
                    const b = document.getElementById(p + ch);
                    if (b) {
                        b.classList.toggle('active', en);
                        b.innerText = en ? '1176 ON' : 'BYPASSED';
                    }
                });
                const pBtn = document.getElementById('pre1176-btn-pwr');
                if (pBtn && String(window.currentCompModalTarget) === String(ch)) {
                    pBtn.className = 'btn-strip ' + (en ? 'active' : '');
                    pBtn.style.background = en ? '#d97706' : '#1e293b';
                    pBtn.style.color = en ? '#fff' : '#94a3b8';
                    pBtn.innerText = en ? '🔥 ANALOG IN' : 'BYPASSED';
                }
            } else if (param === 'preamp1176Drive') {
                const drv = Number(val);
                pComp.drive = drv;
                pComp.inputDriveDb = drv;
                pRack.drive = drv;
                ['rack-preamp-drive-val-', 'modal-preamp-drive-val-', 'full-preamp-drive-val-'].forEach(p => {
                    const el = document.getElementById(p + ch);
                    if (el) el.innerText = '+' + drv.toFixed(1) + ' dB';
                });
                document.querySelectorAll(`input[oninput*="setPreampDrive(${ch}"]`).forEach(sl => { sl.value = drv; });
                if (String(window.currentCompModalTarget) === String(ch)) {
                    refreshAoKnobById('pre1176-knob-drive', drv);
                    const vEl = document.getElementById('pre1176-val-drive');
                    if (vEl) vEl.innerText = '+' + drv.toFixed(1) + ' dB';
                }
            } else if (param === 'preamp1176Trim') {
                const trm = Number(val);
                pComp.trim = trm;
                pComp.outputTrimDb = trm;
                pRack.trim = trm;
                const txt = (trm >= 0 ? '+' : '') + trm.toFixed(1) + ' dB';
                ['rack-preamp-trim-val-', 'modal-preamp-trim-val-', 'full-preamp-trim-val-'].forEach(p => {
                    const el = document.getElementById(p + ch);
                    if (el) el.innerText = txt;
                });
                document.querySelectorAll(`input[oninput*="setPreampTrim(${ch}"]`).forEach(sl => { sl.value = trm; });
                if (String(window.currentCompModalTarget) === String(ch)) {
                    refreshAoKnobById('pre1176-knob-trim', trm);
                    const vEl = document.getElementById('pre1176-val-trim');
                    if (vEl) vEl.innerText = txt;
                }
            } else if (param === 'preamp1176ClipMode') {
                const mode = (val === 'hard' || val === 1 || val === true) ? 'hard' : 'soft';
                pComp.clipMode = mode;
                pRack.clipMode = mode;
                ['rack-preamp-mode-btn-', 'modal-preamp-mode-btn-', 'full-preamp-mode-btn-'].forEach(p => {
                    const b = document.getElementById(p + ch);
                    if (b) {
                        b.innerText = mode === 'soft' ? 'CLASS-A FET (SOFT)' : '1176 ALL-BTN (HARD)';
                        b.style.color = mode === 'soft' ? '#f59e0b' : '#ef4444';
                        b.style.borderColor = mode === 'soft' ? '#f59e0b' : '#ef4444';
                    }
                });
                if (String(window.currentCompModalTarget) === String(ch)) {
                    const sBtn = document.getElementById('pre1176-clip-soft');
                    const hBtn = document.getElementById('pre1176-clip-hard');
                    if (sBtn) { sBtn.style.background = mode === 'soft' ? '#d97706' : '#1e293b'; sBtn.style.color = mode === 'soft' ? '#fff' : '#94a3b8'; }
                    if (hBtn) { hBtn.style.background = mode === 'hard' ? '#ef4444' : '#1e293b'; hBtn.style.color = mode === 'hard' ? '#fff' : '#94a3b8'; }
                }
            }
            return;
        }

        // 1b. RTA View Mode Selector (Line / Bars / Fine Spectral / Off)
        if (param === 'rtaViewMode') {
            if (typeof window.setRtaViewMode === 'function') {
                window.setRtaViewMode(String(val));
            }
            return;
        }

        // 2. Stage 2 & 3: Phase Invert, Input Pre-Gain, REGAIN, and Forensic 10-Band Cyber-Denoiser PRO
        if (['phaseInvert', 'pregainDb', 'regainActive', 'denoiseBypass', 'denoiseThresh', 'denoiseSensitivity', 'denoiseReduc', 'denoiseReduction', 'denoiseLowCut', 'denoiseListen'].includes(param) || param.startsWith('denoiseBand_')) {
            const rSt = (typeof window.getChannelRackState === 'function') ? window.getChannelRackState(ch) : null;
            if (param.startsWith('denoiseBand_')) {
                const bandIdx = parseInt(param.replace('denoiseBand_', ''), 10);
                if (!isNaN(bandIdx) && typeof window.setCyberDenoiserBand === 'function') {
                    window.setCyberDenoiserBand(ch, bandIdx, Number(val), true);
                }
                return;
            }
            if (param === 'phaseInvert') {
                const inv = !!val;
                if (window.phaseInvertState) window.phaseInvertState[ch] = inv;
                if (rSt) rSt.phaseInvert = inv;
                ['phase-btn-', 'rack-phase-btn-', 'rack-modal-phase-btn-'].forEach(p => {
                    const b = document.getElementById(p + ch);
                    if (b) {
                        b.classList.toggle('active', inv);
                        b.innerText = inv ? 'Ø INVERTED' : 'Ø NORMAL';
                    }
                });
                const mBtn = document.getElementById(`modal-phase-btn-${ch}`);
                if (mBtn) {
                    mBtn.classList.toggle('active', inv);
                    mBtn.innerText = `Ø PHASE (${inv ? '180° INV' : '0° NORM'})`;
                }
            } else if (param === 'pregainDb') {
                const db = Number(val);
                if (window.channelInputGains) window.channelInputGains[ch] = db;
                if (window.pregainState) window.pregainState[ch] = db;
                if (rSt) rSt.gain = db;
                const txt = (db >= 0 ? '+' : '') + db.toFixed(1) + ' dB';
                ['gain-val-', 'pregain-val-', 'rack-row-gain-val-'].forEach(p => {
                    const vl = document.getElementById(p + ch);
                    if (vl) vl.innerText = txt;
                });
                ['gain-slider-', 'pregain-slider-', 'rack-row-gain-slider-'].forEach(p => {
                    const sl = document.getElementById(p + ch);
                    if (sl && parseFloat(sl.value) !== db) sl.value = db;
                });
                if (window.dspRackCurrentChannel === ch) {
                    const topSl = document.getElementById('dsp-rack-gain-slider');
                    if (topSl && parseFloat(topSl.value) !== db) topSl.value = db;
                    const topVl = document.getElementById('dsp-rack-gain-val');
                    if (topVl) topVl.innerText = txt;
                }
            } else if (param === 'regainActive') {
                const active = !!val;
                if (window.channelRegainActive) window.channelRegainActive[ch] = active;
                const btn = document.getElementById(`btn-regain-${ch}`);
                if (btn) btn.classList.toggle('active', active);
                const rBtn = document.getElementById(`rack-row-regain-btn-${ch}`);
                if (rBtn) {
                    rBtn.classList.toggle('active', active);
                    rBtn.innerText = active ? 'REGAIN ON' : 'REGAIN OFF';
                }
                if (window.dspRackCurrentChannel === ch) {
                    const topBtn = document.getElementById('dsp-rack-regain-btn');
                    if (topBtn) {
                        topBtn.classList.toggle('active', active);
                        topBtn.innerText = active ? 'REGAIN ON' : 'REGAIN OFF';
                    }
                }
            } else if (param === 'denoiseBypass') {
                const byp = !!val;
                const en = !byp;
                if (window.denoiseState && window.denoiseState[ch]) window.denoiseState[ch].bypass = byp;
                if (rSt && rSt.denoiser) rSt.denoiser.enabled = en;
                document.querySelectorAll(`button[onclick*="denoiseBypass"][onclick*="channel: ${ch}"]`).forEach(b => {
                    b.classList.toggle('active', en);
                    b.innerText = en ? 'GATE ON' : 'BYPASSED';
                });
                const bBtn = document.getElementById(`denoise-byp-btn-${ch}`);
                if (bBtn) { bBtn.classList.toggle('active', en); bBtn.innerText = en ? 'ACTIVE' : 'BYPASS'; }
                const mBtn = document.getElementById(`modal-denoise-en-${ch}`) || document.getElementById(`modal-denoise-byp-${ch}`);
                if (mBtn) { mBtn.classList.toggle('active', en); mBtn.innerText = en ? 'ACTIVE' : 'BYPASS'; }
                if (typeof window.drawCyberDenoiserCanvas === 'function') window.drawCyberDenoiserCanvas(ch);
            } else if (param === 'denoiseThresh' || param === 'denoiseSensitivity') {
                const num = Number(val);
                const isSens = (param === 'denoiseSensitivity');
                const sens = isSens ? num : Math.max(-20, Math.min(20, (num + 45.0) * 0.55));
                const db = isSens ? Math.round(-45 + num / 0.55) : num;
                if (window.denoiseState && window.denoiseState[ch]) window.denoiseState[ch].thresh = db;
                if (rSt && rSt.denoiser) {
                    rSt.denoiser.threshold = db;
                    rSt.denoiser.sensitivity = sens;
                }
                ['denoise-thresh-slider-', 'modal-denoise-thresh-'].forEach(p => {
                    const sl = document.getElementById(p + ch);
                    if (sl) sl.value = db;
                });
                ['denoise-thresh-val-', 'modal-denoise-thresh-val-'].forEach(p => {
                    const vl = document.getElementById(p + ch);
                    if (vl) vl.innerText = db + 'dB (' + (sens >= 0 ? '+' : '') + sens.toFixed(1) + ')';
                });
                const sSl = document.getElementById(`modal-denoise-sens-${ch}`);
                if (sSl) sSl.value = sens;
                const sVl = document.getElementById(`modal-denoise-sens-val-${ch}`);
                if (sVl) sVl.innerText = (sens >= 0 ? '+' : '') + sens.toFixed(1) + ' dB';
                if (typeof window.drawCyberDenoiserCanvas === 'function') window.drawCyberDenoiserCanvas(ch);
            } else if (param === 'denoiseReduc' || param === 'denoiseReduction') {
                const num = Number(val);
                const isPct = (param === 'denoiseReduction' || num > 0);
                const pct = isPct ? Math.max(0, Math.min(100, num)) : Math.max(0, Math.min(100, Math.round((Math.abs(num) / 40.0) * 100)));
                const db = isPct ? -Math.round((pct / 100.0) * 40.0) : num;
                if (window.denoiseState && window.denoiseState[ch]) window.denoiseState[ch].reduc = db;
                if (rSt && rSt.denoiser) {
                    rSt.denoiser.reduction = db;
                    rSt.denoiser.reductionPct = pct;
                }
                const rSl = document.getElementById(`denoise-reduc-slider-${ch}`);
                if (rSl) rSl.value = db;
                const rVl = document.getElementById(`denoise-reduc-val-${ch}`);
                if (rVl) rVl.innerText = Math.round(pct) + '% (' + db + 'dB)';
                const mSl = document.getElementById(`modal-denoise-reduc-${ch}`);
                if (mSl) mSl.value = pct;
                const mVl = document.getElementById(`modal-denoise-reduc-val-${ch}`);
                if (mVl) mVl.innerText = Math.round(pct) + '%';
            } else if (param === 'denoiseLowCut') {
                const lc = !!val;
                if (rSt && rSt.denoiser) rSt.denoiser.lowCut = lc;
                const btn = document.getElementById(`cyber-denoise-lowcut-${ch}`);
                if (btn) btn.classList.toggle('active', lc);
            } else if (param === 'denoiseListen') {
                const ls = !!val;
                if (rSt && rSt.denoiser) rSt.denoiser.listen = ls;
                ['cyber-denoise-listen-', 'rack-denoise-listen-'].forEach(p => {
                    const btn = document.getElementById(p + ch);
                    if (btn) btn.classList.toggle('active', ls);
                });
                if (typeof window.drawCyberDenoiserCanvas === 'function') window.drawCyberDenoiserCanvas(ch);
            }
            return;
        }

        // 3. Stage 5B: TAL Reverb-4 Aux Send Engine
        if (param.startsWith('reverb') && window.reverbDspState && window.reverbDspState.tal) {
            const tal = window.reverbDspState.tal;
            if (param === 'reverbSize') tal.size = Number(val);
            else if (param === 'reverbDamp') tal.damp = Number(val);
            else if (param === 'reverbPreDelay') tal.delay = Number(val) / 500.0;
            else if (param === 'reverbDiffuse') tal.diffuse = Number(val);
            else if (param === 'reverbStereo') tal.stereo = Number(val);
            else if (param === 'reverbDry') tal.dry = Number(val);
            else if (param === 'reverbWet') tal.wet = Number(val);
            else if (param === 'reverbLowCut') tal.eqLowCut = Number(val);
            else if (param === 'reverbHighCut') tal.eqHighCut = Number(val);
            else if (param === 'reverbModRate') tal.modRate = Number(val);
            else if (param === 'reverbModAmount') tal.modAmount = Number(val);
            else if (param === 'reverbDucking') tal.ducking = Number(val);
            else if (param === 'reverbLofiBits') tal.lofiBits = Number(val);
            else if (param === 'reverbLofiSR') tal.lofiSR = Number(val);
            if (typeof window.updateTalFadersVisual === 'function') window.updateTalFadersVisual();
            if (document.getElementById('tal-knob-lowcut') && typeof window.initTalRotaries === 'function') {
                window.initTalRotaries();
            }
            return;
        }

        // 4. Stage 5B: TAL Dub-III Delay Aux Send Engine
        if (param.startsWith('delay') && window.delayDspState) {
            const dly = window.delayDspState;
            if (param === 'delayTimeMs') dly.timeMs = Number(val);
            else if (param === 'delayFeedback') dly.feedback = Number(val);
            else if (param === 'delayDiffuse') dly.diffusion = Number(val);
            else if (param === 'delayPingPong') dly.pingPong = !!val;
            else if (param === 'delayDry') dly.dry = Number(val);
            else if (param === 'delayWet') dly.wet = Number(val);
            else if (param === 'delayLowCut') dly.lowCut = Number(val);
            else if (param === 'delayHighCut') dly.highCut = Number(val);
            else if (param === 'delayDrive') dly.drive = Number(val);
            else if (param === 'delayFlutter') dly.flutter = Number(val);
            else if (param === 'delayModRate') dly.modRate = Number(val);
            else if (param === 'delayModDepth') dly.modDepth = Number(val);
            else if (param === 'delayDucking') dly.ducking = Number(val);
            else if (param === 'delayStereo') dly.stereo = Number(val);
            else if (param === 'delaySpread') dly.spread = Number(val);
            if (typeof window.updateTalDelayFadersVisual === 'function') window.updateTalDelayFadersVisual();
            if (document.getElementById('dly-knob-drive') && typeof window.initTalDelayRotaries === 'function') {
                window.initTalDelayRotaries();
            }
            return;
        }

        // 5. Stage 6: Master Analog Console Summing
        if (param.startsWith('summing') && window.analogSummingState) {
            const s = window.analogSummingState;
            if (param === 'summingEnabled') s.enabled = !!val;
            else if (param === 'summingConsole') s.console = String(val);
            else if (param === 'summingDrive') {
                s.drive = Number(val);
                refreshAoKnobById('summing-knob-drive', s.drive);
                const el = document.getElementById('summing-val-drive');
                if (el) el.innerText = '+' + s.drive.toFixed(1) + ' dB';
            } else if (param === 'summingWarmth') {
                s.warmth = Number(val);
                refreshAoKnobById('summing-knob-warmth', s.warmth);
                const el = document.getElementById('summing-val-warmth');
                if (el) el.innerText = '+' + s.warmth.toFixed(1) + ' dB';
            } else if (param === 'summingCrosstalk') {
                s.crosstalk = Number(val);
                refreshAoKnobById('summing-knob-crosstalk', s.crosstalk);
                const el = document.getElementById('summing-val-crosstalk');
                if (el) el.innerText = Math.round(s.crosstalk) + ' dB';
            }
            return;
        }

        // 6. Stage 5 & 6: Unified Analog Obsession Compressors, Brickwall Limiter & LOADES De-Esser
        if (typeof window.getCompState === 'function') {
            const st = window.getCompState(target);
            const rSt = (typeof target === 'number' && typeof window.getChannelRackState === 'function') ? window.getChannelRackState(ch) : null;
            if (!st) return;

            let needsRackRefresh = false;

            if (param === 'compModel') {
                st.compModel = String(val);
                st.model = String(val);
                if (rSt) rSt.compModel = String(val);
                needsRackRefresh = true;
            } else if (param.startsWith('fetish76_') && st.fetish76) {
                const fk = param.replace('fetish76_', '');
                st.fetish76[fk] = (fk === 'power') ? !!val : Number(val);
                const knobIds = { input: ['fet-knob-input', 'dual-fet-in'], output: ['fet-knob-output', 'dual-fet-out'], attack: ['fet-knob-attack'], release: ['fet-knob-release', 'dual-fet-rel'] };
                (knobIds[fk] || []).forEach(id => refreshAoKnobById(id, st.fetish76[fk]));
                document.querySelectorAll(`input[oninput*="setCompParam(${ch},'fetish76','${fk}'"]`).forEach(sl => {
                    sl.value = st.fetish76[fk];
                    if (sl.nextElementSibling) sl.nextElementSibling.innerText = st.fetish76[fk].toFixed(1);
                });
            } else if (param === 'fetRatio' && st.fetish76) {
                st.fetish76.ratio = Number(val);
                st.fetish76.allButtons = false;
                needsRackRefresh = true;
            } else if (param === 'fetAllButtons' && st.fetish76) {
                st.fetish76.allButtons = !!val;
                needsRackRefresh = true;
            } else if (param.startsWith('lala_') && st.lala) {
                const lk = param.replace('lala_', '');
                st.lala[lk] = (lk === 'power') ? !!val : Number(val);
                const knobIds = {
                    gain: ['lala-knob-gain', 'dual-la-gain'],
                    peakReduction: ['lala-knob-peak', 'dual-la-peak'],
                    mix: ['lala-knob-mix', 'dual-la-mix'],
                    hpf: ['lala-knob-hpf'], mf: ['lala-knob-mf'], mg: ['lala-knob-mg'], hfFlat: ['lala-knob-hfFlat']
                };
                (knobIds[lk] || []).forEach(id => refreshAoKnobById(id, st.lala[lk]));
                document.querySelectorAll(`input[oninput*="setCompParam(${ch},'lala','${lk}'"]`).forEach(sl => {
                    sl.value = st.lala[lk];
                    if (sl.nextElementSibling) sl.nextElementSibling.innerText = Math.round(st.lala[lk]);
                });
            } else if (param === 'lalaMode' && st.lala) {
                st.lala.limitMode = (val === 'limit' || val === 1 || val === true);
                st.lala.mode = st.lala.limitMode ? 'limit' : 'comp';
                needsRackRefresh = true;
            } else if (param.startsWith('limiter_') && st.limiter) {
                const limk = param.replace('limiter_', '');
                st.limiter[limk] = (limk === 'power' || limk === 'crush') ? !!val : Number(val);
                const knobIds = { threshold: 'buster-knob-thresh', ceiling: 'buster-knob-ceiling', drive: 'buster-knob-drive', release: 'buster-knob-release' };
                if (knobIds[limk]) refreshAoKnobById(knobIds[limk], st.limiter[limk]);
                document.querySelectorAll(`input[oninput*="setCompParam(${ch},'limiter','${limk}'"]`).forEach(sl => {
                    sl.value = st.limiter[limk];
                });
                if (limk === 'crush' || limk === 'power') needsRackRefresh = true;
            } else if (param.startsWith('loades')) {
                if (!st.loades) st.loades = { thresh: -24, release: 45, mode: 'split', listen: false };
                if (param === 'loadesThresh') {
                    const th = Number(val);
                    st.loades.thresh = th;
                    if (rSt && rSt.deesser) rSt.deesser.threshold = th;
                    document.querySelectorAll('input[oninput*="loadesThresh"]').forEach(sl => {
                        sl.value = th;
                        if (sl.nextElementSibling) sl.nextElementSibling.innerText = th + ' dB';
                    });
                    const vl = document.getElementById('loades-val-thresh');
                    if (vl) vl.innerText = th + ' dB';
                } else if (param === 'loadesRelease') {
                    const rel = Number(val);
                    st.loades.release = rel;
                    if (rSt && rSt.deesser) rSt.deesser.release = rel;
                    document.querySelectorAll('input[oninput*="loadesRelease"]').forEach(sl => {
                        sl.value = rel;
                        if (sl.nextElementSibling) sl.nextElementSibling.innerText = rel + ' ms';
                    });
                    const vl = document.getElementById('loades-val-rel');
                    if (vl) vl.innerText = rel + ' ms';
                } else if (param === 'loadesMode') {
                    st.loades.mode = String(val);
                    if (rSt && rSt.deesser) rSt.deesser.mode = String(val);
                    needsRackRefresh = true;
                } else if (param === 'loadesListen') {
                    st.loades.listen = !!val;
                    if (rSt && rSt.deesser) rSt.deesser.listen = !!val;
                    needsRackRefresh = true;
                }
            }

            if (needsRackRefresh) {
                const rackPage = document.getElementById('dsp-rack-page');
                if (rackPage && rackPage.classList.contains('open') && window.dspRackCurrentChannel === ch && typeof window.showDspRackPage === 'function') {
                    window.showDspRackPage(ch);
                }
                const rackModal = document.getElementById('channel-rack-overlay');
                if (rackModal && rackModal.classList.contains('open') && window.currentRackChannel === ch && typeof window.openChannelRack === 'function') {
                    window.openChannelRack(ch);
                }
                const dspModal = document.getElementById('dsp-modal-overlay');
                if (dspModal && dspModal.classList.contains('open') && String(window.currentCompModalTarget) === String(target) && typeof window.renderCompModal === 'function') {
                    window.renderCompModal(target);
                }
            }
        }
    }

    window.linkViewsEnabled = false;
    window.toggleLinkViews = function() {
        window.linkViewsEnabled = !window.linkViewsEnabled;
        const btn = document.getElementById('btn-link-views');
        if (btn) {
            btn.classList.toggle('active', window.linkViewsEnabled);
            btn.style.background = window.linkViewsEnabled ? '#2563eb' : '#1e293b';
            btn.style.borderColor = window.linkViewsEnabled ? '#60a5fa' : '#475569';
            btn.style.color = '#fff';
            btn.innerText = window.linkViewsEnabled ? '🔗 LINK VIEWS: ON' : '🔗 LINK VIEWS: OFF';
        }
        if (!window._applyingRemoteSync && window.meterWs?.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: 'sync_view_state',
                linkViews: window.linkViewsEnabled,
                mixerView: window.currentMixerView || 'inputs',
                sofMode: window.currentSofMode || null
            }));
        }
    };

    function applySyncPayload(msg) {
        if (!msg || typeof msg !== 'object') return;
        const act = msg.action || msg.type;
        if (!act) return;
        if (act === 'request_peer_snapshot') { window.broadcastPeerUiSnapshot(); return; }
        if (act === 'peer_ui_snapshot') { applyPeerUiSnapshot(msg); return; }

        if (act === 'set_fader') {
            const pct = msg.pct !== undefined ? Number(msg.pct) : (msg.percent !== undefined ? Number(msg.percent) : gainToPercent(Number(msg.gain)));
            const sofBus = msg.sof || msg.sof_bus || null;
            const t = normalizeTargetKey(msg.target);
            if (sofBus && typeof t === 'number') {
                if (window.faderValues && window.faderValues.sof) {
                    if (!window.faderValues.sof[sofBus]) window.faderValues.sof[sofBus] = [0,0,0,0,0,0,0,0];
                    window.faderValues.sof[sofBus][t] = pct;
                }
                if (window.currentSofMode === sofBus && typeof window.setFaderPosition === 'function') {
                    window.setFaderPosition(t, pct, true);
                }
            } else {
                if (window.faderValues && window.faderValues.main) {
                    window.faderValues.main[t] = pct;
                }
                if ((!window.currentSofMode || typeof t !== 'number') && typeof window.setFaderPosition === 'function') {
                    window.setFaderPosition(t, pct, true);
                }
                if (typeof t === 'number' && t === window.dspRackCurrentChannel && typeof window.setFaderPosition === 'function') {
                    window.setFaderPosition('focused-rack', pct, true);
                }
            }
        } else if (act === 'set_pan') {
            const rawT = msg.target !== undefined ? msg.target : (msg.channel !== undefined ? msg.channel : msg.bus);
            const t = normalizeTargetKey(rawT);
            const panVal = Number(msg.pan);
            const panPct = (Math.abs(panVal) <= 1.0 && String(msg.pan).indexOf('.') !== -1) ? Math.round(panVal * 100) : Math.round(panVal);
            if (typeof window.setChannelPan === 'function') {
                window.setChannelPan(t, panPct, true);
            }
            const sl = document.getElementById(`pan-slider-${t}`);
            if (sl) sl.value = panPct;
            if (t === window.dspRackCurrentChannel) {
                const rSl = document.getElementById('pan-slider-focused-rack');
                if (rSl) rSl.value = panPct;
                if (typeof window.setChannelPan === 'function') window.setChannelPan('focused-rack', panPct, true);
            }
        } else if (act === 'set_mute' || act === 'set_solo') {
            const isMute = (act === 'set_mute');
            const t = normalizeTargetKey(msg.target);
            const state = isMute ? !!msg.muted : !!msg.solo;
            const prefix = isMute ? 'mute-' : 'solo-';
            const btnId = (typeof t === 'number') ? `${prefix}ch-${t}` : (t === 'master' && isMute ? 'master-mute-btn' : `${prefix}${t}`);
            const btn = document.getElementById(btnId);
            if (btn) btn.classList.toggle('active', state);
            if (isMute && window.muteState) window.muteState[t] = state;
            if (!isMute && window.soloState) window.soloState[t] = state;
            if (typeof t === 'number' && t === window.dspRackCurrentChannel) {
                const rBtn = document.getElementById(isMute ? 'mute-focused-rack' : 'solo-focused-rack');
                if (rBtn) rBtn.classList.toggle('active', state);
            }
            if (!isMute && typeof window.updateSoloAlertBanner === 'function') window.updateSoloAlertBanner();
            if (!isMute && typeof window.updateSoloWarningBanner === 'function') window.updateSoloWarningBanner();
        } else if (act === 'set_amm') {
            const ch = Number(msg.channel);
            const en = !!msg.enabled;
            const btn = document.getElementById(`amm-ch-${ch}`);
            if (btn) btn.classList.toggle('active', en);
            if (ch === window.dspRackCurrentChannel) {
                const rBtn = document.getElementById('amm-focused-rack');
                if (rBtn) rBtn.classList.toggle('active', en);
            }
        } else if (act === 'set_global_amm') {
            const en = !!msg.enabled;
            const btn = document.getElementById('btn-amm-master');
            if (btn) {
                btn.classList.toggle('active', en);
                btn.innerText = en ? 'AMM DUGAN: ON' : 'AMM DUGAN: OFF';
            }
        } else if (act === 'set_global_fb') {
            const en = !!msg.enabled;
            if (window.afdState) window.afdState.autoHunting = en;
            const btn = document.getElementById('btn-fb-master');
            if (btn) {
                btn.classList.toggle('active', en);
                btn.innerText = en ? 'AUTO FEEDBACK DESTROYER: ACTIVE' : 'AUTO FEEDBACK DESTROYER: BYPASSED';
            }
        } else if (act === 'set_bpm') {
            if (msg.bpm !== undefined && typeof window.setGlobalBpm === 'function') {
                window.setGlobalBpm(Number(msg.bpm));
            }
        } else if (act === 'clear_all_solos') {
            document.querySelectorAll('.btn-solo.active').forEach(b => b.classList.remove('active'));
            if (window.soloState) Object.keys(window.soloState).forEach(k => { window.soloState[k] = false; });
            if (typeof window.updateSoloAlertBanner === 'function') window.updateSoloAlertBanner();
            if (typeof window.updateSoloWarningBanner === 'function') window.updateSoloWarningBanner();
        } else if (act === 'set_hpf80') {
            const ch = Number(msg.channel);
            const en = !!msg.enabled;
            if (window.channelHpf80) window.channelHpf80[ch] = en;
            document.getElementById(`btn-hpf-${ch}`)?.classList.toggle('active', en);
            if (ch === window.dspRackCurrentChannel) {
                document.getElementById('btn-hpf-focused-rack')?.classList.toggle('active', en);
            }
            if (typeof window.getChannelReEq === 'function') {
                const eq = window.getChannelReEq(ch);
                if (eq && eq.bands && eq.bands[0]) {
                    eq.bands[0].enabled = en;
                    if (en) { eq.bands[0].type = 1; eq.bands[0].freq = 80; eq.bands[0].q = 0.71; }
                }
                if (typeof window.renderMiniReEq === 'function' && window.dspRackCurrentChannel === ch) window.renderMiniReEq(ch);
                if (typeof window.drawAllStripMiniCurves === 'function') window.drawAllStripMiniCurves();
            }
        } else if (act === 'set_stereo_link') {
            const p = Number(msg.pair);
            if (window.stereoLinkState && Boolean(window.stereoLinkState[p]) !== Boolean(msg.linked) && typeof window.toggleStereoLink === 'function') {
                window.toggleStereoLink(p);
            }
        } else if (act === 'patch_input') {
            const ch = Number(msg.channel);
            let hwIn = msg.hw_in !== undefined ? Number(msg.hw_in) : 0;
            if (msg.capture_port && typeof msg.capture_port === 'string') {
                if (msg.capture_port === 'virtual' || msg.capture_port === 'none') hwIn = 0;
                else if (msg.capture_port.indexOf('_') !== -1) hwIn = parseInt(msg.capture_port.split('_').pop(), 10) || 0;
            }
            if (typeof window.handleSoftPatchChange === 'function' && ch >= 0 && ch < 8) {
                window.handleSoftPatchChange(ch, hwIn);
            }
        } else if (act === 'tape_transport' && window.tapeState) {
            const cmd = msg.command;
            if (cmd === 'rec') window.tapeState.transport = 2;
            else if (cmd === 'play') window.tapeState.transport = 1;
            else if (cmd === 'stop') window.tapeState.transport = 0;
            else if (cmd === 'rtz') window.tapeState.sec = 0.0;
            if (typeof window.refreshTapeModalView === 'function') window.refreshTapeModalView();
        } else if (act === 'tape_set_track' && window.tapeState && Array.isArray(window.tapeState.tracks)) {
            const trk = window.tapeState.tracks[Number(msg.track)];
            if (trk) {
                if (msg.vol !== undefined) {
                    trk.vol = Math.round(Number(msg.vol) * 100);
                    const lbl = document.getElementById(`tape-trk-vol-val-${trk.idx}`);
                    const cap = document.getElementById(`tape-red-cap-${trk.idx}`);
                    if (lbl) lbl.textContent = `${trk.vol}%`;
                    if (cap) cap.style.bottom = `${(trk.vol / 150.0) * 82 + 9}%`;
                }
                if (msg.pan !== undefined) {
                    trk.pan = Math.round(Number(msg.pan) * 100);
                    const lbl = document.getElementById(`tape-trk-pan-val-${trk.idx}`);
                    if (lbl) lbl.textContent = trk.pan === 0 ? 'C' : (trk.pan < 0 ? `L${Math.abs(trk.pan)}` : `R${trk.pan}`);
                }
                if (msg.drive !== undefined) {
                    trk.drive = Number(msg.drive);
                    const lbl = document.getElementById(`tape-trk-drv-val-${trk.idx}`);
                    if (lbl) lbl.textContent = `${trk.drive >= 0 ? '+' : ''}${trk.drive.toFixed(1)}dB`;
                }
                if (msg.mute !== undefined) { trk.mute = !!msg.mute; if (typeof window.refreshTapeModalView === 'function') window.refreshTapeModalView(); }
                if (msg.solo !== undefined) { trk.solo = !!msg.solo; if (typeof window.refreshTapeModalView === 'function') window.refreshTapeModalView(); }
                if (msg.rec !== undefined)  { trk.rec  = !!msg.rec;  if (typeof window.refreshTapeModalView === 'function') window.refreshTapeModalView(); }
            }
        } else if (act === 'tape_set_machine' && window.tapeState) {
            const ts = window.tapeState;
            if (msg.ips !== undefined) ts.ips = Number(msg.ips) === 30 ? 30 : 15;
            if (msg.emuEnabled !== undefined) ts.emuEnabled = !!msg.emuEnabled;
            if (msg.printWav !== undefined) ts.printWav = !!msg.printWav;
            if (msg.masterVol !== undefined) {
                ts.masterVol = Math.round(Number(msg.masterVol) * 100);
                const el = document.getElementById('tape-master-vol-val'), cap = document.getElementById('tape-out-cap-master');
                if (el) el.textContent = `${ts.masterVol}%`;
                if (cap) cap.style.bottom = `${(ts.masterVol / 150.0) * 82 + 9}%`;
            }
            if (msg.monIdx !== undefined && msg.monSend !== undefined) {
                const m = Number(msg.monIdx) || 0;
                ts.monSends[m] = Math.round(Number(msg.monSend) * 100);
                const el = document.getElementById(`tape-mon-send-val-${m}`), cap = document.getElementById(`tape-out-cap-mon-${m}`);
                if (el) el.textContent = `${ts.monSends[m]}%`;
                if (cap) cap.style.bottom = `${(ts.monSends[m] / 150.0) * 82 + 9}%`;
            }
            if (typeof window.refreshTapeModalView === 'function') window.refreshTapeModalView();
        } else if (act === 'sync_view_state') {
            if (msg.linkViews !== undefined && Boolean(window.linkViewsEnabled) !== Boolean(msg.linkViews)) {
                window.toggleLinkViews();
            }
            if (window.linkViewsEnabled) {
                if (msg.mixerView && typeof window.switchMixerView === 'function') window.switchMixerView(msg.mixerView);
                if (msg.sofMode !== undefined) {
                    if (msg.sofMode && typeof window.toggleSofMode === 'function' && window.currentSofMode !== msg.sofMode) {
                        window.toggleSofMode(msg.sofMode, msg.sofTitle || msg.sofMode.toUpperCase());
                    } else if (!msg.sofMode && window.currentSofMode && typeof window.exitSofMode === 'function') {
                        window.exitSofMode();
                    }
                }
                if (msg.modalType) {
                    if (msg.modalType === 'close' && typeof window.closeDspModal === 'function') window.closeDspModal();
                    else if (msg.modalType === 'rack_open' && typeof window.showDspRackPage === 'function') window.showDspRackPage(msg.modalTarget || 0);
                    else if (msg.modalType === 'rack_close' && typeof window.closeDspRackPage === 'function') window.closeDspRackPage();
                    else if (typeof window.openDspModal === 'function') window.openDspModal(msg.modalType, msg.modalTarget);
                }
            }
        } else if (act === 'set_ch_custom') {
            if (msg.id && msg.name && window.customChannelNames) {
                window.customChannelNames[msg.id] = msg.name;
                const el = document.getElementById(msg.id.startsWith('ch-') ? `ch-name-${msg.id.replace('ch-','')}` : `${msg.id}-name`);
                if (el) el.innerText = msg.name;
            }
            if (msg.id && msg.color && typeof window.applyChannelColor === 'function') {
                window.applyChannelColor(msg.id, msg.color, false);
            }
        } else if (act === 'set_eq_band' && typeof window.getChannelReEq === 'function') {
            const rawT = msg.target !== undefined ? msg.target : (msg.bus !== undefined ? msg.bus : msg.channel);
            const target = normalizeTargetKey(rawT);
            const eq = window.getChannelReEq(target);
            const b = Number(msg.band);
            if (eq && eq.bands && eq.bands[b]) {
                Object.assign(eq.bands[b], {
                    enabled: !!msg.enabled,
                    type: Number(msg.type),
                    freq: Number(msg.freq),
                    gain: Number(msg.gain),
                    q: Number(msg.q),
                    slope: Number(msg.slope || 12),
                    dynEnabled: !!msg.dynEnabled,
                    dynThresh: Number(msg.dynThresh ?? msg.dynThreshDb ?? -20),
                    dynRange: Number(msg.dynRange ?? msg.dynRangeDb ?? -6),
                    dynRatio: Number(msg.dynRatio ?? 4.0)
                });
                const activeEqTarget = (window.reEqRuntime && window.reEqRuntime.currentTarget !== undefined)
                    ? normalizeTargetKey(window.reEqRuntime.currentTarget)
                    : (window.reEqState ? window.reEqState.activeChannel : 0);
                if (activeEqTarget === target) {
                    if (typeof window.updateReEqCards === 'function') window.updateReEqCards();
                    if (typeof window.renderReEqCanvas === 'function') window.renderReEqCanvas();
                    const eqCvs = document.getElementById('reeq-canvas');
                    if (eqCvs && typeof window.drawReaEQCurve === 'function') window.drawReaEQCurve(eqCvs, eq);
                }
                if (typeof window.renderMiniReEq === 'function' && window.dspRackCurrentChannel === target) {
                    if (typeof window.updateQuickNodeCardUI === 'function') window.updateQuickNodeCardUI(eq.selectedBand || 0);
                    window.renderMiniReEq(target);
                }
                if (typeof window.drawAllStripMiniCurves === 'function') {
                    window.drawAllStripMiniCurves();
                }
            }
        } else if (act === 'dsp_param') {
            applyDspParamSync(msg);
        } else if (act === 'denoise_learned' && typeof window.handleDenoiseLearned === 'function') {
            const lch = (msg.channel !== undefined) ? msg.channel : msg.ch;
            window.handleDenoiseLearned(lch, msg.threshold ?? msg.thresh, msg.reduction ?? msg.reduc, msg.thresholds);
        } else if (act === 'set_afd_notch' && typeof window.updateAfdFilterCards === 'function') {
            const t = normalizeTargetKey(msg.target);
            const idx = Number(msg.notch !== undefined ? msg.notch : msg.slot);
            if (typeof window.getAfdChannel === 'function') {
                const chSt = window.getAfdChannel(t);
                if (chSt && chSt.filters && chSt.filters[idx]) {
                    const f = chSt.filters[idx];
                    f.state = msg.active ? 'locked' : 'idle';
                    f.infNull = !!msg.infNull;
                    f.freq = Number(msg.freq || 0);
                    f.depth = Number(msg.depth || -60);
                    f.q = Number(msg.q || 60);
                    window.updateAfdFilterCards(t);
                }
            }
        } else if (act === 'clear_afd_notches' && typeof window.updateAfdFilterCards === 'function') {
            const t = normalizeTargetKey(msg.target);
            if (typeof window.getAfdChannel === 'function') {
                const chSt = window.getAfdChannel(t);
                if (chSt && chSt.filters) {
                    chSt.filters.forEach(f => { f.state = 'idle'; f.freq = 0; f.depth = 0; });
                    window.updateAfdFilterCards(t);
                }
            }
        }
    }

    function applyFullStateSync(data) {
        if (data.hw && typeof window.updateAudioHardwareTelemetry === 'function') {
            window.updateAudioHardwareTelemetry(data.hw);
        }
        if (data.snapshot && typeof data.snapshot === 'object') applyPeerUiSnapshot(data.snapshot);
        if (Array.isArray(data.ch_fader)) for (let i = 0; i < 8; i++) applySyncPayload({ action: 'set_fader', target: i, gain: data.ch_fader[i] });
        if (Array.isArray(data.ch_pan)) for (let i = 0; i < 8; i++) applySyncPayload({ action: 'set_pan', target: i, pan: data.ch_pan[i] });
        if (Array.isArray(data.ch_mute)) for (let i = 0; i < 8; i++) applySyncPayload({ action: 'set_mute', target: i, muted: data.ch_mute[i] });
        if (Array.isArray(data.ch_solo)) for (let i = 0; i < 8; i++) applySyncPayload({ action: 'set_solo', target: i, solo: data.ch_solo[i] });
        if (Array.isArray(data.ch_hpf80)) for (let i = 0; i < 8; i++) applySyncPayload({ action: 'set_hpf80', channel: i, enabled: data.ch_hpf80[i] });
        if (Array.isArray(data.ch_pregain)) {
            for (let i = 0; i < 8; i++) {
                const lin = Number(data.ch_pregain[i]);
                const db = lin > 0.0001 ? Math.round(20.0 * Math.log10(Math.abs(lin)) * 10) / 10 : 0.0;
                applyDspParamSync({ channel: i, param: 'pregainDb', value: db });
            }
        }
        if (Array.isArray(data.ch_phase)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'phaseInvert', value: !!data.ch_phase[i] });
        if (Array.isArray(data.preamp_en)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'preamp1176Enabled', value: !!data.preamp_en[i] });
        if (Array.isArray(data.preamp_drive)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'preamp1176Drive', value: data.preamp_drive[i] });
        if (Array.isArray(data.preamp_trim)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'preamp1176Trim', value: data.preamp_trim[i] });
        if (Array.isArray(data.preamp_clip)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'preamp1176ClipMode', value: data.preamp_clip[i] ? 'hard' : 'soft' });
        if (Array.isArray(data.gate_en)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'denoiseBypass', value: !data.gate_en[i] });
        if (Array.isArray(data.gate_thresh)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'denoiseThresh', value: data.gate_thresh[i] });
        if (Array.isArray(data.gate_floor)) for (let i = 0; i < 8; i++) applyDspParamSync({ channel: i, param: 'denoiseReduc', value: data.gate_floor[i] });
        if (data.master_fader !== undefined) applySyncPayload({ action: 'set_fader', target: 'master', gain: data.master_fader });
        if (data.master_mute !== undefined) applySyncPayload({ action: 'set_mute', target: 'master', muted: data.master_mute });
        if (Array.isArray(data.mon_fader)) for (let m = 1; m <= 6; m++) applySyncPayload({ action: 'set_fader', target: 'mon-' + m, gain: data.mon_fader[m - 1] });
        if (Array.isArray(data.mon_mute)) for (let m = 1; m <= 6; m++) applySyncPayload({ action: 'set_mute', target: 'mon-' + m, muted: data.mon_mute[m - 1] });
        if (Array.isArray(data.mon_solo)) for (let m = 1; m <= 6; m++) applySyncPayload({ action: 'set_solo', target: 'mon-' + m, solo: data.mon_solo[m - 1] });
        if (Array.isArray(data.send_fader)) for (let s = 1; s <= 4; s++) applySyncPayload({ action: 'set_fader', target: 'send-' + s, gain: data.send_fader[s - 1] });
        if (Array.isArray(data.send_mute)) for (let s = 1; s <= 4; s++) applySyncPayload({ action: 'set_mute', target: 'send-' + s, muted: data.send_mute[s - 1] });
        if (Array.isArray(data.send_solo)) for (let s = 1; s <= 4; s++) applySyncPayload({ action: 'set_solo', target: 'send-' + s, solo: data.send_solo[s - 1] });
        if (Array.isArray(data.ret_fader)) for (let r = 1; r <= 4; r++) applySyncPayload({ action: 'set_fader', target: 'ret-' + r, gain: data.ret_fader[r - 1] });
        if (Array.isArray(data.ret_pan)) for (let r = 1; r <= 4; r++) applySyncPayload({ action: 'set_pan', target: 'ret-' + r, pan: data.ret_pan[r - 1] });
        if (Array.isArray(data.ret_mute)) for (let r = 1; r <= 4; r++) applySyncPayload({ action: 'set_mute', target: 'ret-' + r, muted: data.ret_mute[r - 1] });
        if (Array.isArray(data.ret_solo)) for (let r = 1; r <= 4; r++) applySyncPayload({ action: 'set_solo', target: 'ret-' + r, solo: data.ret_solo[r - 1] });
        if (Array.isArray(data.sof_mon)) {
            for (let m = 0; m < Math.min(6, data.sof_mon.length); m++) {
                const row = data.sof_mon[m];
                if (Array.isArray(row)) {
                    for (let c = 0; c < Math.min(8, row.length); c++) {
                        applySyncPayload({ action: 'set_fader', target: c, sof: 'mon' + (m + 1), gain: row[c] });
                    }
                }
            }
        }
        if (Array.isArray(data.sof_fx)) {
            for (let fx = 0; fx < Math.min(4, data.sof_fx.length); fx++) {
                const row = data.sof_fx[fx];
                if (Array.isArray(row)) {
                    for (let c = 0; c < Math.min(8, row.length); c++) {
                        applySyncPayload({ action: 'set_fader', target: c, sof: 'fx' + (fx + 1), gain: row[c] });
                    }
                }
            }
        }
        if (data.reverb && window.reverbDspState && window.reverbDspState.tal) {
            Object.assign(window.reverbDspState.tal, {
                size: data.reverb.size, damp: data.reverb.damp, delay: Number(data.reverb.predelay || 0) / 500.0, diffuse: data.reverb.diffuse,
                stereo: data.reverb.stereo, dry: data.reverb.dry, wet: data.reverb.wet, eqLowCut: data.reverb.lowcut,
                eqHighCut: data.reverb.highcut, modRate: data.reverb.mod_rate, modAmount: data.reverb.mod_amount,
                ducking: data.reverb.ducking, lofiBits: data.reverb.bits, lofiSR: data.reverb.sr
            });
            if (typeof window.updateTalFadersVisual === 'function') window.updateTalFadersVisual();
            if (document.getElementById('tal-knob-lowcut') && typeof window.initTalRotaries === 'function') window.initTalRotaries();
        }
        if (data.delay && window.delayDspState) {
            Object.assign(window.delayDspState, {
                timeMs: data.delay.time, feedback: data.delay.fb, diffusion: data.delay.diffuse, dry: data.delay.dry,
                wet: data.delay.wet, pingPong: !!data.delay.pp, lowCut: data.delay.lowcut, highCut: data.delay.highcut,
                drive: data.delay.drive, flutter: data.delay.flutter, modRate: data.delay.mod_rate, modDepth: data.delay.mod_depth,
                ducking: data.delay.ducking, stereo: data.delay.stereo, spread: data.delay.spread
            });
            if (typeof window.updateTalDelayFadersVisual === 'function') window.updateTalDelayFadersVisual();
            if (document.getElementById('dly-knob-drive') && typeof window.initTalDelayRotaries === 'function') window.initTalDelayRotaries();
        }
    }

    function applyPeerUiSnapshot(snap) {
        if (!snap) return;
        if (snap.sof && window.faderValues && window.faderValues.sof) {
            Object.assign(window.faderValues.sof, snap.sof);
            if (window.currentSofMode && window.faderValues.sof[window.currentSofMode]) {
                for (let i = 0; i < 8; i++) {
                    if (typeof window.setFaderPosition === 'function') {
                        window.setFaderPosition(i, window.faderValues.sof[window.currentSofMode][i] ?? 0, true);
                    }
                }
            }
        }
        if (snap.faders && window.faderValues && window.faderValues.main) {
            Object.keys(snap.faders).forEach(k => {
                const t = normalizeTargetKey(k);
                window.faderValues.main[t] = snap.faders[k];
                if ((!window.currentSofMode || typeof t !== 'number') && typeof window.setFaderPosition === 'function') {
                    window.setFaderPosition(t, snap.faders[k], true);
                }
            });
        }
        if (Array.isArray(snap.gains)) {
            for (let i = 0; i < 8; i++) {
                if (snap.gains[i] !== undefined) applyDspParamSync({ channel: i, param: 'pregainDb', value: snap.gains[i] });
            }
        }
        if (Array.isArray(snap.regain)) {
            for (let i = 0; i < 8; i++) {
                if (snap.regain[i] !== undefined) applyDspParamSync({ channel: i, param: 'regainActive', value: snap.regain[i] });
            }
        }
        if (snap.names && window.customChannelNames) {
            Object.assign(window.customChannelNames, snap.names);
            Object.keys(snap.names).forEach(id => applySyncPayload({ action: 'set_ch_custom', id: id, name: snap.names[id] }));
        }
        if (snap.colors && typeof window.applyChannelColor === 'function') {
            Object.keys(snap.colors).forEach(id => window.applyChannelColor(id, snap.colors[id], false));
        }
        if (snap.eq && window.reEqState && window.reEqState.channels) {
            Object.assign(window.reEqState.channels, snap.eq);
        }
        if (snap.comp && window.compState) {
            Object.assign(window.compState, snap.comp);
        }
        if (snap.rack && window.channelRackState) {
            Object.assign(window.channelRackState, snap.rack);
        }
        if (snap.bpm && typeof window.setGlobalBpm === 'function') {
            window.setGlobalBpm(Number(snap.bpm));
        }
        if (typeof window.drawAllStripMiniCurves === 'function') window.drawAllStripMiniCurves();
    }

    window.broadcastPeerUiSnapshot = function() {
        if (typeof window.sendRawWs !== 'function' || !window.meterWs || window.meterWs.readyState !== WebSocket.OPEN) return;
        try {
            window.sendRawWs(JSON.stringify({
                action: 'peer_ui_snapshot',
                sof: (window.faderValues && window.faderValues.sof) ? window.faderValues.sof : null,
                faders: (window.faderValues && window.faderValues.main) ? window.faderValues.main : null,
                gains: window.channelInputGains || null,
                regain: window.channelRegainActive || null,
                names: window.customChannelNames || null,
                colors: window.customChannelColors || null,
                eq: (window.reEqState && window.reEqState.channels) ? window.reEqState.channels : null,
                comp: window.compState || null,
                rack: window.channelRackState || null,
                bpm: window.globalBpm || 120
            }));
        } catch (e) {}
    };

    window.onMultiDeviceWsOpen = function() {
        if (window._multiSyncWrapped) return;
        window._multiSyncWrapped = true;
        const origLink = window.toggleStereoLink;
        if (typeof origLink === 'function') {
            window.toggleStereoLink = function(pairNum) {
                origLink(pairNum);
                if (!window._applyingRemoteSync && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'set_stereo_link', pair: pairNum, linked: !!(window.stereoLinkState && window.stereoLinkState[pairNum]) }));
                }
            };
        }
        const origRename = window.saveChannelRename;
        if (typeof origRename === 'function') {
            window.saveChannelRename = function() {
                const id = window.activeRenameChannelId;
                origRename();
                if (!window._applyingRemoteSync && id && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'set_ch_custom', id: id, name: window.customChannelNames[id] }));
                }
            };
        }
        const origColor = window.applyChannelColor;
        if (typeof origColor === 'function') {
            window.applyChannelColor = function(chId, hex, save) {
                origColor(chId, hex, save);
                if (!window._applyingRemoteSync && save && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'set_ch_custom', id: chId, color: hex }));
                }
            };
        }
        // Optional View Mirroring hooks when LINK VIEWS is enabled
        const origSwitchView = window.switchMixerView;
        if (typeof origSwitchView === 'function') {
            window.switchMixerView = function(view) {
                origSwitchView(view);
                if (window.linkViewsEnabled && !window._applyingRemoteSync && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'sync_view_state', mixerView: view }));
                }
            };
        }
        const origOpenModal = window.openDspModal;
        if (typeof origOpenModal === 'function') {
            window.openDspModal = function(type, target) {
                origOpenModal(type, target);
                if (window.linkViewsEnabled && !window._applyingRemoteSync && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'sync_view_state', modalType: type, modalTarget: target }));
                }
            };
        }
        const origCloseModal = window.closeDspModal;
        if (typeof origCloseModal === 'function') {
            window.closeDspModal = function() {
                origCloseModal();
                if (window.linkViewsEnabled && !window._applyingRemoteSync && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'sync_view_state', modalType: 'close' }));
                }
            };
        }
        const origShowRack = window.showDspRackPage;
        if (typeof origShowRack === 'function') {
            window.showDspRackPage = function(ch) {
                origShowRack(ch);
                if (window.linkViewsEnabled && !window._applyingRemoteSync && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'sync_view_state', modalType: 'rack_open', modalTarget: ch }));
                }
            };
        }
        const origCloseRack = window.closeDspRackPage;
        if (typeof origCloseRack === 'function') {
            window.closeDspRackPage = function() {
                origCloseRack();
                if (window.linkViewsEnabled && !window._applyingRemoteSync && window.meterWs?.readyState === WebSocket.OPEN) {
                    window.meterWs.send(JSON.stringify({ action: 'sync_view_state', modalType: 'rack_close' }));
                }
            };
        }
    };

    window.handleMultiDeviceWsMessage = function(data) {
        withRemoteSyncLock(() => {
            if (data.type === 'full_state_sync') applyFullStateSync(data);
            else if (data.type === 'sync_event' && data.payload) applySyncPayload(data.payload);
        });
    };
})(window);
