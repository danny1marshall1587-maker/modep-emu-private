// channel-strip.js - View switching, mute/solo toggles, soft-patching, stereo linking, custom names/colors
(function(window) {
    'use strict';
    // 1. Channel Custom Names & Colors Persistence
    window.loadChannelCustomizations = function() {
        try {
            const savedNames = localStorage.getItem("modep_channel_names");
            if (savedNames) window.customChannelNames = JSON.parse(savedNames);
            const savedColors = localStorage.getItem("modep_channel_colors");
            if (savedColors) window.customChannelColors = JSON.parse(savedColors);
        } catch(e) {}
        // Apply loaded customizations to DOM
        for (const [chId, name] of Object.entries(window.customChannelNames)) {
            const el = document.getElementById(chId.startsWith('ch-') ? `ch-name-${chId.replace('ch-','')}` : `${chId}-name`);
            if (el) el.innerText = name;
        }
        for (const [chId, color] of Object.entries(window.customChannelColors)) {
            window.applyChannelColor(chId, color, false);
        }
    };
    window.applyChannelColor = function(chId, colorHex, save = true) {
        const strip = document.getElementById(chId.startsWith('ch-') ? chId : (chId.startsWith('send-') ? `send-strip-${chId.replace('send-','')}` : (chId.startsWith('ret-') ? `return-strip-${chId.replace('ret-','')}` : (chId.startsWith('mon-') ? `monitor-strip-${chId.replace('mon-','')}` : `${chId}-strip`))));
        if (strip) {
            strip.style.setProperty('--ch-accent', colorHex);
            strip.style.borderColor = colorHex;
            strip.style.borderTopColor = colorHex;
        }
        const badge = document.getElementById(chId.startsWith('ch-') ? `ch-badge-${chId.replace('ch-','')}` : `${chId}-badge`);
        if (badge) {
            badge.style.background = colorHex;
            badge.style.color = (colorHex === '#e2e8f0' || colorHex === '#eab308' || colorHex === '#84cc16') ? '#000' : '#fff';
        }
        if (save) {
            window.customChannelColors[chId] = colorHex;
            try { localStorage.setItem("modep_channel_colors", JSON.stringify(window.customChannelColors)); } catch(e) {}
        }
    };
    // Rename Channel Dialog Handlers
    window.activeRenameChannelId = null;
    window.openRenameModal = function(chId, defaultTitle) {
        window.activeRenameChannelId = chId;
        const input = document.getElementById("rename-modal-input");
        const titleEl = document.getElementById("rename-modal-title");
        const currentName = window.customChannelNames[chId] || (document.getElementById(chId.startsWith('ch-') ? `ch-name-${chId.replace('ch-','')}` : `${chId}-name`)?.innerText || defaultTitle);
        if (titleEl) titleEl.innerText = `🏷️ Rename ${defaultTitle}`;
        if (input) {
            input.value = currentName;
            document.getElementById("rename-modal-overlay").classList.add("open");
            setTimeout(() => input.focus(), 80);
        }
    };
    window.saveChannelRename = function() {
        if (!window.activeRenameChannelId) return;
        const input = document.getElementById("rename-modal-input");
        const newName = input ? input.value.trim() : "";
        if (newName) {
            window.customChannelNames[window.activeRenameChannelId] = newName;
            const el = document.getElementById(window.activeRenameChannelId.startsWith('ch-') ? `ch-name-${window.activeRenameChannelId.replace('ch-','')}` : `${window.activeRenameChannelId}-name`);
            if (el) el.innerText = newName;
            try { localStorage.setItem("modep_channel_names", JSON.stringify(window.customChannelNames)); } catch(e) {}
        }
        window.closeRenameModal();
    };
    window.closeRenameModal = function() {
        const overlay = document.getElementById("rename-modal-overlay");
        if (overlay) overlay.classList.remove("open");
        window.activeRenameChannelId = null;
    };
    // Color Palette Popover Handlers
    window.activeColorChannelId = null;
    window.openColorPalette = function(chId, evt) {
        evt.stopPropagation();
        window.activeColorChannelId = chId;
        const popover = document.getElementById("color-palette-popover");
        const grid = document.getElementById("color-swatch-grid");
        if (!popover || !grid) return;
        grid.innerHTML = (window.CONSOLE_COLORS || []).map(c => `
            <div class="color-swatch" style="background:${c.hex};" title="${c.name}" onclick="selectColor('${c.hex}')"></div>
        `).join('');
        const rect = evt.currentTarget.getBoundingClientRect();
        let top = Math.min(window.innerHeight - 240, Math.max(70, rect.bottom + 8));
        let left = Math.min(window.innerWidth - 300, Math.max(10, rect.left - 40));
        popover.style.top = top + 'px';
        popover.style.left = left + 'px';
        popover.classList.add("open");
    };
    window.selectColor = function(hex) {
        if (window.activeColorChannelId) {
            window.applyChannelColor(window.activeColorChannelId, hex, true);
        }
        window.closeColorPopover();
    };
    window.closeColorPopover = function() {
        const popover = document.getElementById("color-palette-popover");
        if (popover) popover.classList.remove("open");
        window.activeColorChannelId = null;
    };
    // Long Press Event Binder
    window.longPressTimer = null;
    window.bindLongPress = function(element, callback) {
        let startX = 0, startY = 0;
        let triggered = false;
        const start = (e) => {
            triggered = false;
            startX = e.clientX || (e.touches ? e.touches[0].clientX : 0);
            startY = e.clientY || (e.touches ? e.touches[0].clientY : 0);
            window.longPressTimer = setTimeout(() => {
                triggered = true;
                callback(e);
            }, 550);
        };
        const cancel = (e) => {
            if (e.type === 'pointermove' || e.type === 'touchmove') {
                const curX = e.clientX || (e.touches ? e.touches[0].clientX : 0);
                const curY = e.clientY || (e.touches ? e.touches[0].clientY : 0);
                const dist = Math.hypot(curX - startX, curY - startY);
                if (dist > 10) {
                    clearTimeout(window.longPressTimer);
                }
                return;
            }
            clearTimeout(window.longPressTimer);
        };
        element.addEventListener('pointerdown', start);
        element.addEventListener('pointermove', cancel);
        element.addEventListener('pointerup', cancel);
        element.addEventListener('pointercancel', cancel);
    };
    // 2. Tap Tempo Engine
    window.handleTapTempo = function() {
        const now = performance.now();
        const btn = document.getElementById("btn-tap-tempo");
        if (btn) {
            btn.classList.add("pulse");
            setTimeout(() => btn.classList.remove("pulse"), 120);
        }
        if (window.PatchboxState.tapTimeout) clearTimeout(window.PatchboxState.tapTimeout);
        window.PatchboxState.tapTimeout = setTimeout(() => {
            window.PatchboxState.tapTimes = [];
        }, 2200);
        window.PatchboxState.tapTimes.push(now);
        if (window.PatchboxState.tapTimes.length > 4) window.PatchboxState.tapTimes.shift();
        if (window.PatchboxState.tapTimes.length >= 2) {
            let intervals = [];
            for (let i = 1; i < window.PatchboxState.tapTimes.length; i++) {
                intervals.push(window.PatchboxState.tapTimes[i] - window.PatchboxState.tapTimes[i - 1]);
            }
            const avgInterval = intervals.reduce((a, b) => a + b, 0) / intervals.length;
            let calculatedBpm = 60000 / avgInterval;
            calculatedBpm = Math.max(30.0, Math.min(300.0, calculatedBpm));
            window.setGlobalBpm(calculatedBpm);
        }
    };
    window.setGlobalBpm = function(bpm) {
        window.globalBpm = Math.round(bpm * 10) / 10;
        window.PatchboxState.globalBpm = window.globalBpm;
        const display = document.getElementById("tempo-bpm-val");
        if (display) display.innerText = window.globalBpm.toFixed(1);
        if (typeof window.delayDspState !== 'undefined' && window.delayDspState && window.delayDspState.syncToTempo) {
            if (typeof window.updateDelayTimeFromBpm === 'function') {
                window.updateDelayTimeFromBpm();
            }
        }
        if (!window._applyingRemoteSync && window.sendMixerWs) {
            window.sendMixerWs({ action: 'set_bpm', bpm: window.globalBpm });
        }
    };
    // 3. View Switcher (Channels 1-8, FX Sends & Returns, Main PA & Monitors)
    window.switchMixerView = function(view) {
        window.currentMixerView = view;
        window.PatchboxState.currentMixerView = view;
        if (view !== "inputs" && window.currentSofMode) {
            window.exitSofMode();
        }
        document.querySelectorAll(".btn-tab").forEach(b => b.classList.remove("active"));
        const activeTabBtn = document.getElementById("tab-btn-" + view);
        if (activeTabBtn) activeTabBtn.classList.add("active");
        const inputStrips = document.querySelectorAll(".input-strip");
        const sendStrips = document.querySelectorAll(".send-strip");
        const returnStrips = document.querySelectorAll(".return-strip");
        const monitorStrips = document.querySelectorAll(".monitor-strip");
        const masterStrip = document.getElementById("master-strip");
        const linkCols = document.querySelectorAll(".stereo-link-col");
        if (view === "inputs") {
            inputStrips.forEach(s => s.style.display = "flex");
            linkCols.forEach(c => c.style.display = "flex");
            sendStrips.forEach(s => s.style.display = "none");
            returnStrips.forEach(s => s.style.display = "none");
            monitorStrips.forEach(s => s.style.display = "none");
            if (masterStrip) masterStrip.style.display = "flex";
        } else if (view === "sends") {
            inputStrips.forEach(s => s.style.display = "none");
            linkCols.forEach(c => c.style.display = "none");
            sendStrips.forEach(s => s.style.display = "flex");
            returnStrips.forEach(s => s.style.display = "flex");
            monitorStrips.forEach(s => s.style.display = "none");
            if (masterStrip) masterStrip.style.display = "flex";
        } else if (view === "monitors") {
            inputStrips.forEach(s => s.style.display = "none");
            linkCols.forEach(c => c.style.display = "none");
            sendStrips.forEach(s => s.style.display = "none");
            returnStrips.forEach(s => s.style.display = "none");
            monitorStrips.forEach(s => s.style.display = "flex");
            if (masterStrip) masterStrip.style.display = "none";
        }
    };
    // Sends on Fader (SOF) Mode
    window.toggleSofMode = function(sendKey, sendTitle) {
        if (window.currentSofMode === sendKey) {
            window.exitSofMode();
            return;
        }
        window.currentSofMode = sendKey;
        window.PatchboxState.currentSofMode = sendKey;
        window.switchMixerView("inputs");
        document.querySelectorAll(".btn-sof").forEach(b => b.classList.remove("active"));
        const activeBtn = document.getElementById("sof-btn-" + sendKey);
        if (activeBtn) activeBtn.classList.add("active");
        const exitBtn = document.getElementById("sof-exit-btn");
        if (exitBtn) exitBtn.classList.add("visible");
        const banner = document.getElementById("sof-active-banner");
        const bannerText = document.getElementById("sof-banner-text");
        if (banner) banner.classList.add("active");
        if (bannerText) {
            bannerText.innerHTML = `SENDS ON FADER: ${sendTitle.toUpperCase()} <span style="display:inline-flex;align-items:center;gap:6px;margin-left:14px;background:#090b10;padding:2px 8px;border-radius:4px;border:1px solid #fbbf24;"><span style="font-size:0.65rem;color:#fbbf24;">BUS METER</span><span style="width:90px;height:8px;background:#1e293b;border-radius:3px;overflow:hidden;display:inline-block;"><span id="sof-banner-meter-fill" style="display:block;height:100%;width:0%;background:#10b981;"></span></span><span id="sof-banner-meter-db" style="font-family:monospace;font-size:0.68rem;color:#f8fafc;min-width:44px;">-inf</span></span>`;
        }
        const sendLevels = window.faderValues.sof[sendKey];
        for (let i = 0; i < 8; i++) {
            const targetVal = sendLevels ? (sendLevels[i] ?? 0) : 0;
            window.setFaderPosition(i, targetVal, true, true);
        }
    };
    window.exitSofMode = function() {
        if (!window.currentSofMode) return;
        window.currentSofMode = null;
        window.PatchboxState.currentSofMode = null;
        document.querySelectorAll(".btn-sof").forEach(b => b.classList.remove("active"));
        const exitBtn = document.getElementById("sof-exit-btn");
        if (exitBtn) exitBtn.classList.remove("visible");
        const banner = document.getElementById("sof-active-banner");
        if (banner) banner.classList.remove("active");
        for (let i = 0; i < 8; i++) {
            window.setFaderPosition(i, window.faderValues.main[i] ?? 75, true, true);
        }
    };
    // Channel Buttons: Mute, Solo, AMM Dugan, Feedback Destroyer
    let _stereoBtnGuard = false;
    window.toggleChMute = function(ch) {
        const btn = document.getElementById("mute-ch-" + ch);
        const active = btn ? btn.classList.toggle("active") : !window.muteState[ch];
        if (window.muteState) window.muteState[ch] = active;
        if (typeof window.refreshLocalChannelMeterImmediate === 'function') {
            window.refreshLocalChannelMeterImmediate(ch);
        }
        if (window.dspRackCurrentChannel === ch) {
            const rBtn = document.getElementById("mute-focused-rack");
            if (rBtn) rBtn.classList.toggle("active", active);
        }
        if (window.sendMixerWs) window.sendMixerWs({ action: 'set_mute', target: ch, muted: active });
        if (!_stereoBtnGuard && window.stereoLinkState[Math.floor(ch / 2)]) {
            const partner = (ch % 2 === 0) ? ch + 1 : ch - 1;
            const pBtn = document.getElementById("mute-ch-" + partner);
            if (pBtn && pBtn.classList.contains("active") !== active) {
                _stereoBtnGuard = true;
                window.toggleChMute(partner);
                _stereoBtnGuard = false;
            }
        }
    };
    window.toggleChSolo = function(ch) {
        const btn = document.getElementById("solo-ch-" + ch);
        const active = btn ? btn.classList.toggle("active") : !window.soloState[ch];
        if (window.soloState) window.soloState[ch] = active;
        if (window.dspRackCurrentChannel === ch) {
            const rBtn = document.getElementById("solo-focused-rack");
            if (rBtn) rBtn.classList.toggle("active", active);
        }
        if (typeof window.updateSoloWarningBanner === 'function') window.updateSoloWarningBanner();
        if (window.sendMixerWs) window.sendMixerWs({ action: 'set_solo', target: ch, solo: active });
        if (!_stereoBtnGuard && window.stereoLinkState[Math.floor(ch / 2)]) {
            const partner = (ch % 2 === 0) ? ch + 1 : ch - 1;
            const pBtn = document.getElementById("solo-ch-" + partner);
            if (pBtn && pBtn.classList.contains("active") !== active) {
                _stereoBtnGuard = true;
                window.toggleChSolo(partner);
                _stereoBtnGuard = false;
            }
        }
    };
    window.toggleChAmm = function(ch) {
        const btn = document.getElementById("amm-ch-" + ch);
        const active = btn ? btn.classList.toggle("active") : false;
        if (window.dspRackCurrentChannel === ch) {
            const rBtn = document.getElementById("amm-focused-rack");
            if (rBtn) rBtn.classList.toggle("active", active);
        }
        if (window.sendMixerWs) window.sendMixerWs({ action: 'set_amm', channel: ch, enabled: active });
    };
    window.toggleMasterMute = function() {
        const btn = document.getElementById("master-mute-btn");
        const active = btn ? btn.classList.toggle("active") : !window.muteState.master;
        if (window.muteState) window.muteState.master = active;
        if (window.sendMixerWs) window.sendMixerWs({ action: 'set_mute', target: 'master', muted: active });
    };
    window.toggleGlobalAmm = function() {
        const btn = document.getElementById("btn-amm-master");
        if (btn) {
            const active = btn.classList.toggle("active");
            btn.innerText = active ? "AMM DUGAN: ON" : "AMM DUGAN: OFF";
            if (window.sendMixerWs) window.sendMixerWs({ action: 'set_global_amm', enabled: active });
        }
    };
    window.toggleGlobalFb = function() {
        const btn = document.getElementById("btn-fb-master");
        if (btn) {
            const active = btn.classList.toggle("active");
            if (window.afdState) {
                window.afdState.autoHunting = active;
            }
            btn.innerText = active ? "AUTO FEEDBACK DESTROYER: ACTIVE" : "AUTO FEEDBACK DESTROYER: BYPASSED";
            if (!window._applyingRemoteSync && window.sendMixerWs) {
                window.sendMixerWs({ action: 'set_global_fb', enabled: active });
            }
        }
    };
    // 4. Digital Gain & REGAIN Engine
    window.toggleRegain = function(ch) {
        window.channelRegainActive[ch] = !window.channelRegainActive[ch];
        const active = !!window.channelRegainActive[ch];
        const btn = document.getElementById(`btn-regain-${ch}`);
        if (btn) btn.classList.toggle("active", active);
        const rBtn = document.getElementById(`rack-row-regain-btn-${ch}`);
        if (rBtn) {
            rBtn.classList.toggle("active", active);
            rBtn.innerText = active ? "REGAIN ON" : "REGAIN OFF";
        }
        if (window.dspRackCurrentChannel === ch) {
            const topBtn = document.getElementById("dsp-rack-regain-btn");
            if (topBtn) {
                topBtn.classList.toggle("active", active);
                topBtn.innerText = active ? "REGAIN ON" : "REGAIN OFF";
            }
        }
        if (!window._applyingRemoteSync && window.sendMixerWs) {
            window.sendMixerWs({ type: 'dsp_param', channel: ch, param: 'regainActive', value: active });
        }
    };
    window.dbToFaderPercent = function(db) {
        if (db <= -60.0) return 0.0;
        if (db < -30.0) return 0.5 + ((db + 60.0) / 30.0) * 24.5;
        if (db < -10.0) return 25.0 + ((db + 30.0) / 20.0) * 25.0;
        if (db < 0.0)   return 50.0 + ((db + 10.0) / 10.0) * 25.0;
        return 75.0 + (db / 10.0) * 25.0;
    };
    window.faderPercentToDb = function(pos) {
        if (pos <= 0.0) return -100.0;
        if (pos >= 75.0) return ((pos - 75.0) / 25.0) * 10.0;
        if (pos >= 50.0) return -10.0 + ((pos - 50.0) / 25.0) * 10.0;
        if (pos >= 25.0) return -30.0 + ((pos - 25.0) / 25.0) * 20.0;
        return -60.0 + ((pos - 0.5) / 24.5) * 30.0;
    };
    window.handleInputGainChange = function(ch, newGainDb) {
        newGainDb = parseFloat(newGainDb);
        const oldGainDb = window.channelInputGains[ch] ?? 0.0;
        const deltaGain = newGainDb - oldGainDb;
        window.channelInputGains[ch] = newGainDb;
        if (typeof window.getChannelRackState === 'function') {
            window.getChannelRackState(ch).gain = newGainDb;
        }
        const txt = (newGainDb >= 0 ? "+" : "") + newGainDb.toFixed(1) + " dB";
        const valDisplay = document.getElementById(`gain-val-${ch}`);
        if (valDisplay) valDisplay.innerText = txt;
        const rowSlider = document.getElementById(`rack-row-gain-slider-${ch}`);
        if (rowSlider && parseFloat(rowSlider.value) !== newGainDb) rowSlider.value = newGainDb;
        const rowVal = document.getElementById(`rack-row-gain-val-${ch}`);
        if (rowVal) rowVal.innerText = txt;
        if (window.dspRackCurrentChannel === ch) {
            const topSlider = document.getElementById('dsp-rack-gain-slider');
            if (topSlider && parseFloat(topSlider.value) !== newGainDb) topSlider.value = newGainDb;
            const topVal = document.getElementById('dsp-rack-gain-val');
            if (topVal) topVal.innerText = txt;
        }
        if (!window._applyingRemoteSync && window.sendMixerWs) {
            window.sendMixerWs({ type: 'dsp_param', channel: ch, param: 'pregainDb', value: newGainDb });
        }
        if (window.channelRegainActive[ch]) {
            const currentFaderPercent = (window.currentSofMode ? window.faderValues.sof[window.currentSofMode][ch] : window.faderValues.main[ch]) ?? 75;
            const currentDb = window.faderPercentToDb(currentFaderPercent);
            const compensatedDb = Math.max(-60.0, Math.min(10.0, currentDb - deltaGain));
            let targetPercent = window.dbToFaderPercent(compensatedDb);
            if (window.touchGovernor) {
                targetPercent = window.touchGovernor.process(`gain-regain-${ch}`, targetPercent);
            }
            window.setFaderPosition(ch, targetPercent, false);
        }
    };
    // 5. Hardware Input Soft-Patch & Universal Audio Interface Auto-Detection
    window.audioHardwareState = {
        id: 'EVO4',
        name: 'Audient EVO 4',
        ins: 4,
        outs: 4,
        sr: 48000,
        buf: 128,
        cue: 1,
        patch: [1, 2, 0, 0, 0, 0, 0, 0],
        cards: []
    };

    window.refreshSoftPatchDropdowns = function() {
        const hw = window.audioHardwareState;
        const maxIns = Math.max(2, Math.min(32, Number(hw.ins) || 18));
        const isEvo4 = String(hw.id || '').toUpperCase().includes('EVO4');
        for (let ch = 0; ch < 8; ch++) {
            const sel = document.getElementById('ch-input-source-' + ch);
            if (!sel) continue;
            const currentVal = (Array.isArray(hw.patch) && hw.patch[ch] !== undefined) ? Number(hw.patch[ch]) : Number(sel.value);
            let optsHtml = `<option value="0" ${currentVal === 0 ? 'selected' : ''}>TAPE / VIRT</option>`;
            for (let p = 1; p <= maxIns; p++) {
                let lbl = `HW ${p}`;
                if (isEvo4 && p >= 3) lbl = `LOOP ${p - 2} (HW ${p})`;
                optsHtml += `<option value="${p}" ${currentVal === p ? 'selected' : ''}>${lbl}</option>`;
            }
            if (sel.dataset.renderedIns !== `${hw.id}_${maxIns}`) {
                sel.innerHTML = optsHtml;
                sel.dataset.renderedIns = `${hw.id}_${maxIns}`;
            }
            sel.value = String(currentVal);
            const badge = document.getElementById('ch-badge-' + ch);
            if (badge) {
                const inLabel = currentVal === 0 ? 'VIRT' : `IN ${currentVal}`;
                badge.innerHTML = `${inLabel} &bull; CH ${ch + 1}`;
            }
        }
    };

    window.updateAudioHardwareTelemetry = function(hwData) {
        if (!hwData || typeof hwData !== 'object') return;
        const st = window.audioHardwareState;
        const prevSig = `${st.id}_${st.ins}_${st.outs}_${st.sr}_${st.buf}`;
        if (hwData.id !== undefined) st.id = hwData.id;
        if (hwData.name !== undefined) st.name = hwData.name;
        if (hwData.ins !== undefined) st.ins = Number(hwData.ins) || 2;
        if (hwData.outs !== undefined) st.outs = Number(hwData.outs) || 2;
        if (hwData.sr !== undefined) st.sr = Number(hwData.sr) || 48000;
        if (hwData.buf !== undefined) st.buf = Number(hwData.buf) || 128;
        if (hwData.cue !== undefined) st.cue = Number(hwData.cue) ? 1 : 0;
        if (Array.isArray(hwData.patch)) st.patch = hwData.patch.slice(0, 8);
        if (Array.isArray(hwData.cards)) st.cards = hwData.cards;

        const nextSig = `${st.id}_${st.ins}_${st.outs}_${st.sr}_${st.buf}`;
        if (prevSig !== nextSig || !window._hwBadgeInitialized) {
            window._hwBadgeInitialized = true;
            const badgeEl = document.getElementById('hw-interface-badge');
            if (badgeEl) {
                const cleanName = (st.name || st.id || 'USB AUDIO').toUpperCase();
                badgeEl.textContent = `🔊 ${cleanName} (${st.ins}×${st.outs} @ ${Math.round(st.sr / 1000)}kHz)`;
            }
            const rackHwTitle = document.querySelector('.rack-gain-hw');
            if (rackHwTitle) {
                rackHwTitle.textContent = `${st.name || st.id} Digital Pre-Gain (${st.ins} In / ${st.outs} Out)`;
            }
        }
        if (prevSig !== nextSig || Array.isArray(hwData.patch)) {
            window.refreshSoftPatchDropdowns();
        }
    };

    window.applyAudioInterfaceConfig = function() {
        const cardEl = document.getElementById('hw-modal-card-select');
        const rateEl = document.getElementById('hw-modal-rate-select');
        const bufEl  = document.getElementById('hw-modal-buf-select');
        const cueEl  = document.getElementById('hw-modal-cue-toggle');
        const st = window.audioHardwareState;
        const card = cardEl ? cardEl.value : st.id;
        const rate = rateEl ? parseInt(rateEl.value, 10) : st.sr;
        const buf  = bufEl  ? parseInt(bufEl.value, 10)  : st.buf;
        const cue  = cueEl  ? (cueEl.checked ? 1 : 0)    : st.cue;
        st.solo_cue_to_main = cue;
        st.cue = cue;
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: 'set_audio_interface',
                card: card,
                rate: rate,
                buffer: buf,
                soloCue: cue
            }));
        }
        const statusEl = document.getElementById('hw-modal-status-msg');
        if (statusEl) {
            statusEl.innerHTML = `<span style="color:#34d399; font-weight:800;">✓ Restarting JACK Audio Engine on <b>${card}</b> (${rate} Hz / ${buf} fpp)...</span>`;
        }
    };

    window.handleSoftPatchChange = function(ch, hwNum) {
        const n = parseInt(hwNum, 10) || 0;
        if (window.audioHardwareState && Array.isArray(window.audioHardwareState.patch)) {
            window.audioHardwareState.patch[ch] = n;
        }
        const capturePort = n > 0 ? ('system:capture_' + n) : 'virtual';
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: 'patch_input',
                channel: ch,
                hw_in: n,
                capture_port: capturePort
            }));
        }
        const sel = document.getElementById('ch-input-source-' + ch);
        if (sel) {
            sel.value = String(n);
            sel.title = n > 0 ? ('Routed from ' + capturePort) : 'Virtual / 2" Tape Return Only (Noise-Free)';
        }
        const badge = document.getElementById('ch-badge-' + ch);
        if (badge) {
            badge.innerHTML = `${n === 0 ? 'VIRT' : ('IN ' + n)} &bull; CH ${ch + 1}`;
        }
    };
    // 6. Stereo Channel Link Engine (Auto Hard-Pan L100 / R100 when linked, Center when unlinked)
    window.toggleStereoLink = function(pairNum) {
        const chL = pairNum * 2;
        const chR = chL + 1;
        window.stereoLinkState[pairNum] = !window.stereoLinkState[pairNum];
        const linked = window.stereoLinkState[pairNum];
        const btn = document.getElementById('btn-stereo-link-' + pairNum);
        if (btn) {
            btn.classList.toggle('active', linked);
            btn.innerHTML = linked ? `[ LINKED <br>CH ${chL+1}+${chR+1} ]` : `[ LINK ]`;
        }
        const stripL = document.getElementById('ch-' + chL);
        const stripR = document.getElementById('ch-' + chR);
        if (stripL) stripL.classList.toggle('stereo-linked', linked);
        if (stripR) stripR.classList.toggle('stereo-linked', linked);
        if (linked) {
            if (typeof window.setChannelPan === 'function') {
                window.setChannelPan(chL, -100, true);
                window.setChannelPan(chR, 100, true);
            }
            const pctL = window.faderValues.main[chL] != null ? window.faderValues.main[chL] : 75;
            window.setFaderPosition(chR, pctL, false);
            const isMutedL = !!(document.getElementById('mute-ch-' + chL) &&
                               document.getElementById('mute-ch-' + chL).classList.contains('active'));
            const muteBtnR = document.getElementById('mute-ch-' + chR);
            if (muteBtnR && muteBtnR.classList.contains('active') !== isMutedL) {
                window.toggleChMute(chR);
            }
            const isSoloL = !!(document.getElementById('solo-ch-' + chL) &&
                               document.getElementById('solo-ch-' + chL).classList.contains('active'));
            const soloBtnR = document.getElementById('solo-ch-' + chR);
            if (soloBtnR && soloBtnR.classList.contains('active') !== isSoloL) {
                window.toggleChSolo(chR);
            }
            if (typeof window.getChannelRackState === 'function') {
                const stL = window.getChannelRackState(chL);
                const stR = window.getChannelRackState(chR);
                stR.compModel = stL.compModel;
                stR.compParams = JSON.parse(JSON.stringify(stL.compParams));
                stR.deesser = JSON.parse(JSON.stringify(stL.deesser));
                stR.phaseInvert = stL.phaseInvert;
            }
        } else {
            if (typeof window.setChannelPan === 'function') {
                window.setChannelPan(chL, 0, true);
                window.setChannelPan(chR, 0, true);
            }
        }
    };
    // Propagate fader moves to stereo partner when linked
    let _stereoFaderGuard = false;
    const _origSetFaderPos = window.setFaderPosition;
    window.setFaderPosition = function(target, pct, animate) {
        _origSetFaderPos(target, pct, animate);
        if (typeof target === 'number' && typeof window.refreshLocalChannelMeterImmediate === 'function') {
            window.refreshLocalChannelMeterImmediate(target);
        }
        if (_stereoFaderGuard || typeof target !== 'number') return;
        const pairNum = Math.floor(target / 2);
        if (!window.stereoLinkState[pairNum]) return;
        const partner = (target % 2 === 0) ? target + 1 : target - 1;
        _stereoFaderGuard = true;
        _origSetFaderPos(partner, pct, animate);
        if (typeof window.refreshLocalChannelMeterImmediate === 'function') {
            window.refreshLocalChannelMeterImmediate(partner);
        }
        _stereoFaderGuard = false;
    };
})(window);
