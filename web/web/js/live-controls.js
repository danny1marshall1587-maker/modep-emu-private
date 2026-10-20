// live-controls.js - Step 1 Live Console Controls: L/C/R Pan, 80Hz HPF, Sticky Clip Hold, Clear Solo, Stage Mic Mute & WS Audio Sync
(function(window) {
    'use strict';

    // 1. WebSocket Dispatch Helper
    window.sendMixerWs = function(payload) {
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            try {
                window.meterWs.send(JSON.stringify(payload));
            } catch (e) {}
        }
    };

    // 2. Per-Channel & FX Return L/C/R Pan Control (-100 = L100, 0 = C, +100 = R100)
    window.formatPanLabel = function(panVal) {
        const p = Math.round(Number(panVal) || 0);
        if (Math.abs(p) <= 2) return 'C';
        return p < 0 ? ('L' + Math.abs(p)) : ('R' + p);
    };

    window.setChannelPan = function(target, rawVal, skipGov) {
        let p = Math.max(-100, Math.min(100, parseFloat(rawVal) || 0));
        if (!skipGov && window.touchGovernor) {
            p = window.touchGovernor.process('pan-' + target, p);
        }
        p = Math.round(p);
        if (Math.abs(p) <= 2 && !skipGov) p = 0; // Center detent

        window.channelPans[target] = p;
        const lblText = window.formatPanLabel(p);

        const slider = document.getElementById('pan-slider-' + target);
        if (slider && parseFloat(slider.value) !== p) slider.value = p;
        const lbl = document.getElementById('pan-val-' + target);
        if (lbl) {
            lbl.innerText = lblText;
            lbl.classList.toggle('panned-lr', p !== 0);
        }

        // Sync Focused Rack Strip if open for this channel
        if (typeof target === 'number' && target === window.dspRackCurrentChannel) {
            const fSlider = document.getElementById('pan-slider-focused-rack');
            if (fSlider && parseFloat(fSlider.value) !== p) fSlider.value = p;
            const fLbl = document.getElementById('pan-val-focused-rack');
            if (fLbl) {
                fLbl.innerText = lblText;
                fLbl.classList.toggle('panned-lr', p !== 0);
            }
        }

        window.sendMixerWs({
            action: 'set_pan',
            target: target,
            pan: +(p / 100.0).toFixed(3)
        });
    };

    window.resetChannelPan = function(target) {
        window.setChannelPan(target, 0, true);
    };

    // 3. One-Tap 80Hz High-Pass Filter (Low-Cut) on Input Channels 1-8
    window.toggleChannelHpf80 = function(ch) {
        const active = !window.channelHpf80[ch];
        window.channelHpf80[ch] = active;

        const btn = document.getElementById('btn-hpf-' + ch);
        if (btn) btn.classList.toggle('active', active);

        if (ch === window.dspRackCurrentChannel) {
            const fBtn = document.getElementById('btn-hpf-focused-rack');
            if (fBtn) fBtn.classList.toggle('active', active);
        }

        // Sync directly with Band 1 (index 0, Low Cut) of Channel ReEQ
        if (typeof window.getChannelReEq === 'function') {
            const eqSt = window.getChannelReEq(ch);
            if (eqSt && eqSt.bands && eqSt.bands[0]) {
                eqSt.bands[0].type = 1; // Low Cut
                eqSt.bands[0].freq = active ? 80 : 30;
                eqSt.bands[0].q = 0.71;
                eqSt.bands[0].enabled = active;
            }
            if (typeof window.dispatchReEqBand === 'function') {
                window.dispatchReEqBand(ch, 0);
            }
            if (ch === window.dspRackCurrentChannel && typeof window.renderMiniReEq === 'function') {
                window.renderMiniReEq(ch);
                if (typeof window.updateQuickNodeCardUI === 'function') {
                    window.updateQuickNodeCardUI(eqSt.selectedBand || 0);
                }
            }
        }

        window.sendMixerWs({
            action: 'set_hpf80',
            channel: ch,
            enabled: active,
            freq: active ? 80 : 30
        });
    };

    // 4. Sticky Peak / Clip Hold LEDs (Latches at >= -0.5 dBFS / level >= 0.944)
    window.latchClipHold = function(target) {
        if (window.clipHoldState[target]) return;
        window.clipHoldState[target] = true;
        const led = document.getElementById('clip-led-' + target);
        if (led) led.classList.add('latched');
        if (typeof target === 'number' && target === window.dspRackCurrentChannel) {
            const fLed = document.getElementById('clip-led-focused-rack');
            if (fLed) fLed.classList.add('latched');
        }
        window.updateClearClipsButton();
    };

    window.clearClipHold = function(target, evt) {
        if (evt && evt.stopPropagation) evt.stopPropagation();
        delete window.clipHoldState[target];
        const led = document.getElementById('clip-led-' + target);
        if (led) led.classList.remove('latched');
        if (typeof target === 'number' && target === window.dspRackCurrentChannel) {
            const fLed = document.getElementById('clip-led-focused-rack');
            if (fLed) fLed.classList.remove('latched');
        }
        window.updateClearClipsButton();
    };

    window.clearAllClips = function() {
        window.clipHoldState = {};
        window.PatchboxState.clipHoldState = window.clipHoldState;
        document.querySelectorAll('.clip-hold-led.latched').forEach(el => el.classList.remove('latched'));
        window.updateClearClipsButton();
    };

    window.updateClearClipsButton = function() {
        const anyClip = Object.keys(window.clipHoldState).some(k => window.clipHoldState[k]);
        const btn = document.getElementById('btn-clear-clips');
        if (btn) btn.classList.toggle('active-alert', anyClip);
    };

    // 5. Bus Mute & Solo (Sends 1-4, Returns 1-4, Monitors 1-6) + Flashing CLEAR SOLO Warning
    window.toggleBusMute = function(target) {
        const btn = document.getElementById('mute-' + target);
        const active = btn ? btn.classList.toggle('active') : !window.muteState[target];
        window.muteState[target] = active;
        window.sendMixerWs({ action: 'set_mute', target: target, muted: active });
    };

    window.toggleBusSolo = function(target) {
        const btn = document.getElementById('solo-' + target);
        const active = btn ? btn.classList.toggle('active') : !window.soloState[target];
        window.soloState[target] = active;
        window.updateSoloWarningBanner();
        window.sendMixerWs({ action: 'set_solo', target: target, solo: active });
    };

    window.updateSoloWarningBanner = function() {
        const anySolo = document.querySelectorAll('.btn-solo.active').length > 0;
        const btn = document.getElementById('btn-clear-solo');
        if (btn) {
            btn.classList.toggle('active-flash', anySolo);
            btn.innerText = anySolo ? '⚠️ CLEAR SOLO' : 'SOLO CLEAR';
        }
    };

    window.clearAllSolos = function() {
        for (let i = 0; i < 8; i++) {
            window.soloState[i] = false;
            const b = document.getElementById('solo-ch-' + i);
            if (b) b.classList.remove('active');
        }
        ['send-1', 'send-2', 'send-3', 'send-4', 'ret-1', 'ret-2', 'ret-3', 'ret-4'].forEach(t => {
            window.soloState[t] = false;
            const b = document.getElementById('solo-' + t);
            if (b) b.classList.remove('active');
        });
        const fSolo = document.querySelector('#dsp-rack-focused-strip .btn-solo');
        if (fSolo) fSolo.classList.remove('active');
        window.updateSoloWarningBanner();
        window.sendMixerWs({ action: 'clear_all_solos' });
    };

    // 6. Mute Stage Mics (CH 1-6) Group Button (Leaves CH 7-8 open for intermission playback)
    window.toggleMuteStageMics = function() {
        const st = window.PatchboxState;
        st.stageMicsMuted = !st.stageMicsMuted;
        const btn = document.getElementById('btn-mute-stage-mics');
        if (btn) {
            btn.classList.toggle('engaged', st.stageMicsMuted);
            btn.innerHTML = st.stageMicsMuted
                ? '<span>🔇 MICS 1-6 MUTED</span>'
                : '<span>🎙️ MUTE MICS (1-6)</span>';
        }

        if (st.stageMicsMuted) {
            st.savedStageMicMuteState = {};
            for (let ch = 0; ch < 6; ch++) {
                const mBtn = document.getElementById('mute-ch-' + ch);
                const wasMuted = mBtn ? mBtn.classList.contains('active') : !!window.muteState[ch];
                st.savedStageMicMuteState[ch] = wasMuted;
                if (!wasMuted) window.toggleChMute(ch);
            }
        } else {
            for (let ch = 0; ch < 6; ch++) {
                const shouldBeMuted = !!st.savedStageMicMuteState[ch];
                const mBtn = document.getElementById('mute-ch-' + ch);
                const isNowMuted = mBtn ? mBtn.classList.contains('active') : !!window.muteState[ch];
                if (isNowMuted !== shouldBeMuted) window.toggleChMute(ch);
            }
            st.savedStageMicMuteState = {};
        }
    };

    // 7. Momentary FX Mute Panic (Sends 1-4 & Returns 1-4)
    window.startMomentaryFxMute = function(e) {
        if (window.PatchboxState.isMomentaryFxMuted) return;
        window.PatchboxState.isMomentaryFxMuted = true;
        const btn = document.getElementById("btn-fx-mute-momentary");
        if (btn) btn.classList.add("engaged");
        for (let r = 1; r <= 4; r++) {
            const muteBtn = document.getElementById(`mute-ret-${r}`);
            if (muteBtn) {
                window.PatchboxState.previousFxMuteState[`ret-${r}`] = muteBtn.classList.contains("active");
                muteBtn.classList.add("active");
                window.sendMixerWs({ action: 'set_mute', target: `ret-${r}`, muted: true });
            }
        }
        for (let s = 1; s <= 4; s++) {
            const muteBtn = document.getElementById(`mute-send-${s}`);
            if (muteBtn) {
                window.PatchboxState.previousFxMuteState[`send-${s}`] = muteBtn.classList.contains("active");
                muteBtn.classList.add("active");
                window.sendMixerWs({ action: 'set_mute', target: `send-${s}`, muted: true });
            }
        }
    };
    window.stopMomentaryFxMute = function(e) {
        if (!window.PatchboxState.isMomentaryFxMuted) return;
        window.PatchboxState.isMomentaryFxMuted = false;
        const btn = document.getElementById("btn-fx-mute-momentary");
        if (btn) btn.classList.remove("engaged");
        for (let r = 1; r <= 4; r++) {
            const muteBtn = document.getElementById(`mute-ret-${r}`);
            if (muteBtn && window.PatchboxState.previousFxMuteState[`ret-${r}`] !== undefined) {
                const prev = !!window.PatchboxState.previousFxMuteState[`ret-${r}`];
                muteBtn.classList.toggle("active", prev);
                window.sendMixerWs({ action: 'set_mute', target: `ret-${r}`, muted: prev });
            }
        }
        for (let s = 1; s <= 4; s++) {
            const muteBtn = document.getElementById(`mute-send-${s}`);
            if (muteBtn && window.PatchboxState.previousFxMuteState[`send-${s}`] !== undefined) {
                const prev = !!window.PatchboxState.previousFxMuteState[`send-${s}`];
                muteBtn.classList.toggle("active", prev);
                window.sendMixerWs({ action: 'set_mute', target: `send-${s}`, muted: prev });
            }
        }
        window.PatchboxState.previousFxMuteState = {};
    };

    // 8. Fader Position to Real Backend Audio Matrix Dispatch
    window.sendFaderToBackend = function(target, pct) {
        const db = (typeof window.faderPercentToDb === 'function')
            ? window.faderPercentToDb(pct)
            : (pct <= 0 ? -100.0 : ((pct - 75.0) / 25.0) * 10.0);
        const gain = (db <= -60.0 || pct <= 0.2) ? 0.0 : Math.pow(10.0, db / 20.0);
        window.sendMixerWs({
            action: 'set_fader',
            target: target,
            sof: (typeof target === 'number' && window.currentSofMode) ? window.currentSofMode : null,
            pct: +pct.toFixed(2),
            db: +db.toFixed(2),
            gain: +gain.toFixed(5)
        });
    };

    // 9. Stage 6: 1-Tap Scene Snapshot Save & Recall Manager (8 Show Slots)
    const SCENE_STORAGE_KEY = 'modep_live_console_scenes_v1';
    const DEFAULT_SCENE_NAMES = [
        '1. Main Band Acoustic Set', '2. Headliner Full Mix', '3. Vocal & Guitar Duo', '4. Intermission / Playback',
        '5. Soundcheck Baseline', '6. Monitor Wedge Safe', '7. Late Night Dynamic Flat', '8. Custom User Show'
    ];
    function loadSavedScenes() {
        try {
            return JSON.parse(localStorage.getItem(SCENE_STORAGE_KEY) || '{}');
        } catch (e) { return {}; }
    }
    window.saveMixerScene = function(slotIdx) {
        const scenes = loadSavedScenes();
        const nameInput = document.getElementById('scene-name-input-' + slotIdx);
        const label = (nameInput && nameInput.value.trim()) ? nameInput.value.trim() : DEFAULT_SCENE_NAMES[slotIdx];
        const eqSnap = {};
        const targets = [0,1,2,3,4,5,6,7,'master','mon-1','mon-2','mon-3','mon-4','mon-5','mon-6'];
        if (typeof window.getChannelReEq === 'function') {
            targets.forEach(t => { eqSnap[t] = JSON.parse(JSON.stringify(window.getChannelReEq(t))); });
        }
        scenes[slotIdx] = {
            name: label,
            savedAt: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }),
            faderPositions: JSON.parse(JSON.stringify(window.faderPositions || {})),
            sofMatrices: JSON.parse(JSON.stringify(window.sofMatrices || {})),
            channelPans: JSON.parse(JSON.stringify(window.channelPans || {})),
            channelHpf80: JSON.parse(JSON.stringify(window.channelHpf80 || {})),
            muteState: JSON.parse(JSON.stringify(window.muteState || {})),
            customChannelNames: JSON.parse(JSON.stringify(window.customChannelNames || {})),
            analogSummingState: JSON.parse(JSON.stringify(window.analogSummingState || {})),
            eqSnap: eqSnap
        };
        try { localStorage.setItem(SCENE_STORAGE_KEY, JSON.stringify(scenes)); } catch (e) {}
        const title = document.getElementById('modal-title'), body = document.getElementById('modal-body');
        if (title && body) window.renderSceneManagerModal(title, body, `✓ SAVED "${label.toUpperCase()}" TO SLOT ${slotIdx + 1}`);
    };
    window.recallMixerScene = function(slotIdx) {
        const scenes = loadSavedScenes();
        const sc = scenes[slotIdx];
        if (!sc) return;
        if (sc.sofMatrices) Object.assign(window.sofMatrices, sc.sofMatrices);
        if (sc.faderPositions && typeof window.setFaderPosition === 'function') {
            Object.keys(sc.faderPositions).forEach(t => {
                const key = isNaN(t) ? t : parseInt(t, 10);
                window.setFaderPosition(key, sc.faderPositions[t], true);
            });
        }
        if (sc.channelPans && typeof window.setChannelPan === 'function') {
            Object.keys(sc.channelPans).forEach(t => {
                const key = isNaN(t) ? t : parseInt(t, 10);
                window.setChannelPan(key, sc.channelPans[t], true);
            });
        }
        if (sc.eqSnap && typeof window.getChannelReEq === 'function') {
            Object.keys(sc.eqSnap).forEach(t => {
                const key = isNaN(t) ? t : parseInt(t, 10);
                const dst = window.getChannelReEq(key);
                Object.assign(dst, sc.eqSnap[t]);
                if (typeof window.dispatchReEqBand === 'function') {
                    for (let b = 0; b < 8; b++) window.dispatchReEqBand(key, b);
                }
            });
        }
        if (sc.analogSummingState && window.analogSummingState) {
            Object.assign(window.analogSummingState, sc.analogSummingState);
            if (typeof window.updateSummingHeaderBtn === 'function') window.updateSummingHeaderBtn();
        }
        const title = document.getElementById('modal-title'), body = document.getElementById('modal-body');
        if (title && body) window.renderSceneManagerModal(title, body, `⚡ RECALLED "${sc.name.toUpperCase()}" TO LIVE JACK DSP`);
    };
    window.renderSceneManagerModal = function(title, body, statusMsg) {
        const scenes = loadSavedScenes();
        title.innerHTML = `💾 STAGE 6: 1-TAP SHOW SCENE SNAPSHOT MANAGER (8 FULL-DESK SLOTS)`;
        body.innerHTML = `
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;">
                <span style="color:#94a3b8; font-size:0.78rem;">Captures &amp; restores all Faders, SOF Monitor Mixes, Pan, 80Hz HPF, ReEQ + Tukan Dynamic EQ curves, and Analog Summing.</span>
                <span style="color:#10b981; font-weight:800; font-size:0.78rem;">${statusMsg || 'READY'}</span>
            </div>
            <div style="display:grid; grid-template-columns:repeat(2, 1fr); gap:10px;">
                ${DEFAULT_SCENE_NAMES.map((defName, idx) => {
                    const sc = scenes[idx];
                    return `
                    <div style="background:#0f172a; border:1px solid ${sc ? '#10b981' : '#334155'}; border-radius:8px; padding:10px; display:flex; flex-direction:column; gap:8px;">
                        <div style="display:flex; justify-content:space-between; align-items:center; gap:8px;">
                            <input id="scene-name-input-${idx}" type="text" value="${sc ? sc.name : defName}" style="flex:1; background:#1e293b; color:#f8fafc; border:1px solid #475569; border-radius:4px; padding:4px 8px; font-weight:800; font-size:0.78rem;">
                            <span style="font-size:0.65rem; font-family:monospace; color:${sc ? '#34d399' : '#64748b'};">${sc ? 'SAVED ' + sc.savedAt : 'EMPTY'}</span>
                        </div>
                        <div style="display:flex; gap:8px;">
                            <button class="btn-top" style="flex:1; min-height:36px; background:#2563eb; border-color:#3b82f6; font-weight:800;" onclick="saveMixerScene(${idx})">💾 SAVE SCENE ${idx + 1}</button>
                            <button class="btn-top" style="flex:1; min-height:36px; background:${sc ? '#059669' : '#1e293b'}; border-color:${sc ? '#10b981' : '#334155'}; font-weight:800;" ${sc ? '' : 'disabled'} onclick="recallMixerScene(${idx})">⚡ RECALL SCENE ${idx + 1}</button>
                        </div>
                    </div>`;
                }).join('')}
            </div>
        `;
    };

})(window);
