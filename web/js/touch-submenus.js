// touch-submenus.js - Tablet Touchscreen Sub-Menu Drawers, Collapsible SOF Bar, Channel Setup Sub-Menu Modal & DSP Rack Sub-Tabs
(function(window) {
    'use strict';

    window.activeTouchSubmenu = null;
    window.activeChannelSetupCh = 0;
    window.currentDspRackSubTab = 'input_fx';

    // 1. Top Header Touch Sub-Menu Drawers (Master Suite & System/Scenes)
    window.toggleTouchSubmenu = function(menuKey) {
        const drawer = document.getElementById('submenu-' + menuKey);
        const backdrop = document.getElementById('touch-submenu-backdrop');
        if (!drawer) return;
        const isAlreadyOpen = drawer.classList.contains('open');
        window.closeAllTouchSubmenus();
        if (!isAlreadyOpen) {
            drawer.classList.add('open');
            if (backdrop) backdrop.classList.add('open');
            const trig = document.getElementById('btn-submenu-' + menuKey);
            if (trig) trig.classList.add('submenu-open');
            window.activeTouchSubmenu = menuKey;
        }
    };

    window.closeAllTouchSubmenus = function() {
        document.querySelectorAll('.touch-submenu-drawer.open').forEach(el => el.classList.remove('open'));
        document.querySelectorAll('.btn-submenu-trigger.submenu-open').forEach(el => el.classList.remove('submenu-open'));
        const backdrop = document.getElementById('touch-submenu-backdrop');
        if (backdrop) backdrop.classList.remove('open');
        window.activeTouchSubmenu = null;
    };

    // 2. Collapsible Sends-On-Fader (SOF) Touch Drawer
    window.toggleSofDrawer = function(forceState) {
        const drawer = document.getElementById('nav-row-sof-drawer');
        const btn = document.getElementById('btn-sof-drawer-toggle');
        if (!drawer) return;
        const nextOpen = (typeof forceState === 'boolean') ? forceState : !drawer.classList.contains('open');
        drawer.classList.toggle('open', nextOpen);
        if (btn) {
            const hasActiveSof = !!window.currentSofMode;
            btn.classList.toggle('active', nextOpen || hasActiveSof);
            btn.innerHTML = hasActiveSof
                ? `🎚️ SOF: ${String(window.currentSofMode).toUpperCase()} ▴`
                : (nextOpen ? '🎚️ SENDS ON FADER ▴' : '🎚️ SENDS ON FADER ▾');
        }
    };

    // Wrap toggleSofMode & exitSofMode so SOF drawer syncs automatically
    const _origToggleSof = window.toggleSofMode;
    if (typeof _origToggleSof === 'function') {
        window.toggleSofMode = function(sendKey, sendTitle) {
            _origToggleSof(sendKey, sendTitle);
            window.toggleSofDrawer(!!window.currentSofMode);
        };
    }
    const _origExitSof = window.exitSofMode;
    if (typeof _origExitSof === 'function') {
        window.exitSofMode = function() {
            _origExitSof();
            window.toggleSofDrawer(false);
        };
    }

    // 3. Channel Strip Setup Badge & Touch Sub-Menu Modal (Input, Pan, 80Hz HPF, Phase, Stereo Link)
    window.syncChannelSetupBadge = function(ch) {
        const c = Number(ch);
        if (isNaN(c) || c < 0 || c > 7) return;
        const hwPatch = (window.audioHardwareState && Array.isArray(window.audioHardwareState.patch))
            ? window.audioHardwareState.patch[c]
            : (c + 1);
        const inLbl = (hwPatch === 0) ? 'VIRT' : `IN ${hwPatch}`;
        const panVal = (window.channelPans && window.channelPans[c]) ?? 0;
        const panLbl = (typeof window.formatPanLabel === 'function') ? window.formatPanLabel(panVal) : 'C';
        const hpfOn = !!(window.channelHpf80 && window.channelHpf80[c]);
        const pairNum = Math.floor(c / 2);
        const linked = !!(window.stereoLinkState && window.stereoLinkState[pairNum]);

        let parts = [`🎛️ ${inLbl}`, `PAN ${panLbl}`];
        if (hpfOn) parts.push('Ø80');
        if (linked) parts.push('🔗');
        const html = parts.join(' &bull; ');

        ['btn-ch-setup-' + c, 'btn-ch-setup-focused-' + c].forEach(id => {
            const btn = document.getElementById(id);
            if (btn) {
                btn.innerHTML = html;
                btn.classList.toggle('has-active-mod', hpfOn || linked || panVal !== 0);
            }
        });
    };

    window.syncAllChannelSetupBadges = function() {
        for (let i = 0; i < 8; i++) window.syncChannelSetupBadge(i);
    };

    window.openChannelSetupModal = function(ch) {
        const c = (ch !== undefined && ch !== null) ? Number(ch) : 0;
        window.activeChannelSetupCh = c;
        const overlay = document.getElementById('ch-setup-modal-overlay');
        if (!overlay) return;
        window.renderChannelSetupModalContent(c);
        overlay.classList.add('open');
    };

    window.closeChannelSetupModal = function() {
        const overlay = document.getElementById('ch-setup-modal-overlay');
        if (overlay) overlay.classList.remove('open');
    };

    window.renderChannelSetupModalContent = function(ch) {
        const c = Number(ch);
        const titleEl = document.getElementById('ch-setup-modal-title');
        const bodyEl = document.getElementById('ch-setup-modal-body');
        if (!bodyEl) return;

        const chName = (window.customChannelNames && window.customChannelNames['ch-' + c])
            || (document.getElementById('ch-name-' + c)?.innerText)
            || `Channel ${c + 1}`;
        const color = (window.customChannelColors && window.customChannelColors['ch-' + c]) || '#38bdf8';
        if (titleEl) {
            titleEl.innerHTML = `<span style="display:inline-block;width:12px;height:12px;border-radius:50%;background:${color};margin-right:8px;"></span>🎛️ CH ${c + 1} (${chName}) — Input, Pan, HPF &amp; Link`;
        }

        const panVal = Math.round((window.channelPans && window.channelPans[c]) ?? 0);
        const panLbl = (typeof window.formatPanLabel === 'function') ? window.formatPanLabel(panVal) : 'C';
        const hpfOn = !!(window.channelHpf80 && window.channelHpf80[c]);
        const pairNum = Math.floor(c / 2);
        const chL = pairNum * 2 + 1;
        const chR = pairNum * 2 + 2;
        const linked = !!(window.stereoLinkState && window.stereoLinkState[pairNum]);
        const ammOn = !!(document.getElementById('amm-ch-' + c)?.classList.contains('active'));
        const rackSt = (typeof window.getChannelRackState === 'function') ? window.getChannelRackState(c) : { phaseInvert: false };
        const phaseOn = !!rackSt.phaseInvert;
        const gainDb = parseFloat((window.channelInputGains && window.channelInputGains[c]) ?? 0.0);
        const regainOn = (window.channelRegainActive && window.channelRegainActive[c]) ?? true;

        const hw = window.audioHardwareState || { ins: 8, patch: [1, 2, 3, 4, 5, 6, 7, 8] };
        const maxIns = Math.max(4, Math.min(18, Number(hw.ins) || 8));
        const curHw = (Array.isArray(hw.patch) && hw.patch[c] !== undefined)
            ? Number(hw.patch[c])
            : Number(document.getElementById('ch-input-source-' + c)?.value ?? (c + 1));

        let chTabsHtml = '';
        for (let i = 0; i < 8; i++) {
            const nm = (window.customChannelNames && window.customChannelNames['ch-' + i])
                || (document.getElementById('ch-name-' + i)?.innerText)
                || `CH ${i + 1}`;
            chTabsHtml += `<button type="button" class="ch-setup-tab-btn ${i === c ? 'active' : ''}" onclick="openChannelSetupModal(${i})">CH ${i + 1}<small>${nm.substring(0, 8)}</small></button>`;
        }

        let hwBtnsHtml = `<button type="button" class="ch-setup-hw-btn ${curHw === 0 ? 'active' : ''}" onclick="handleSoftPatchChange(${c}, 0); renderChannelSetupModalContent(${c});">📼 TAPE / VIRT</button>`;
        for (let p = 1; p <= maxIns; p++) {
            hwBtnsHtml += `<button type="button" class="ch-setup-hw-btn ${curHw === p ? 'active' : ''}" onclick="handleSoftPatchChange(${c}, ${p}); renderChannelSetupModalContent(${c});">🔌 HW IN ${p}</button>`;
        }

        bodyEl.innerHTML = `
            <div class="ch-setup-tabs-row">${chTabsHtml}</div>

            <div class="ch-setup-grid-2col">
                <!-- Left Card: Stereo Panning (L/C/R) -->
                <div class="ch-setup-card">
                    <div class="ch-setup-card-hdr">
                        <span>↔️ STEREO PANNING (L / C / R)</span>
                        <span class="ch-setup-readout" id="modal-ch-pan-val">${panLbl}</span>
                    </div>
                    <input type="range" class="ch-setup-big-slider" id="modal-ch-pan-slider" min="-100" max="100" step="1" value="${panVal}"
                        oninput="setChannelPan(${c}, this.value); document.getElementById('modal-ch-pan-val').innerText = formatPanLabel(this.value);">
                    <div class="ch-setup-preset-row">
                        <button type="button" class="ch-setup-preset-btn ${panVal <= -98 ? 'active' : ''}" onclick="setChannelPan(${c}, -100, true); renderChannelSetupModalContent(${c});">HARD L</button>
                        <button type="button" class="ch-setup-preset-btn ${panVal === -50 ? 'active' : ''}" onclick="setChannelPan(${c}, -50, true); renderChannelSetupModalContent(${c});">L 50</button>
                        <button type="button" class="ch-setup-preset-btn ${panVal === 0 ? 'active' : ''}" onclick="resetChannelPan(${c}); renderChannelSetupModalContent(${c});">CENTER (C)</button>
                        <button type="button" class="ch-setup-preset-btn ${panVal === 50 ? 'active' : ''}" onclick="setChannelPan(${c}, 50, true); renderChannelSetupModalContent(${c});">R 50</button>
                        <button type="button" class="ch-setup-preset-btn ${panVal >= 98 ? 'active' : ''}" onclick="setChannelPan(${c}, 100, true); renderChannelSetupModalContent(${c});">HARD R</button>
                    </div>
                </div>

                <!-- Right Card: Digital Pre-Gain & Auto REGAIN -->
                <div class="ch-setup-card">
                    <div class="ch-setup-card-hdr">
                        <span>🎚️ DIGITAL PRE-GAIN &amp; REGAIN</span>
                        <span class="ch-setup-readout" id="modal-ch-gain-val">${(gainDb >= 0 ? '+' : '') + gainDb.toFixed(1)} dB</span>
                    </div>
                    <input type="range" class="ch-setup-big-slider" id="modal-ch-gain-slider" min="-18" max="24" step="0.5" value="${gainDb}"
                        oninput="handleInputGainChange(${c}, this.value); document.getElementById('modal-ch-gain-val').innerText = (parseFloat(this.value)>=0?'+':'') + parseFloat(this.value).toFixed(1) + ' dB';">
                    <div class="ch-setup-preset-row">
                        <button type="button" class="ch-setup-preset-btn" onclick="handleInputGainChange(${c}, Math.max(-18, (${gainDb} - 3))); renderChannelSetupModalContent(${c});">-3 dB</button>
                        <button type="button" class="ch-setup-preset-btn ${Math.abs(gainDb) < 0.1 ? 'active' : ''}" onclick="handleInputGainChange(${c}, 0); renderChannelSetupModalContent(${c});">0.0 dB (UNITY)</button>
                        <button type="button" class="ch-setup-preset-btn" onclick="handleInputGainChange(${c}, Math.min(24, (${gainDb} + 3))); renderChannelSetupModalContent(${c});">+3 dB</button>
                        <button type="button" class="ch-setup-preset-btn ${regainOn ? 'active-emerald' : ''}" onclick="toggleRegain(${c}); renderChannelSetupModalContent(${c});">${regainOn ? '✓ REGAIN: ON' : 'REGAIN: OFF'}</button>
                    </div>
                </div>
            </div>

            <!-- 4 Large Touch Toggle Switches -->
            <div class="ch-setup-toggle-grid">
                <button type="button" class="ch-setup-toggle-card ${hpfOn ? 'active-amber' : ''}" onclick="toggleChannelHpf80(${c}); renderChannelSetupModalContent(${c});">
                    <span class="toggle-card-title">Ø 80Hz LOW-CUT (HPF)</span>
                    <span class="toggle-card-state">${hpfOn ? '🟢 ENGAGED (80Hz)' : '⚪ BYPASSED'}</span>
                </button>
                <button type="button" class="ch-setup-toggle-card ${phaseOn ? 'active-sky' : ''}" onclick="togglePhaseInvert(${c}); renderChannelSetupModalContent(${c});">
                    <span class="toggle-card-title">Ø POLARITY (180° PHASE)</span>
                    <span class="toggle-card-state">${phaseOn ? '🔄 INVERTED (180°)' : '⚪ NORMAL (0°)'}</span>
                </button>
                <button type="button" class="ch-setup-toggle-card ${linked ? 'active-sky' : ''}" onclick="toggleStereoLink(${pairNum}); renderChannelSetupModalContent(${c});">
                    <span class="toggle-card-title">🔗 STEREO LINK (CH ${chL}+${chR})</span>
                    <span class="toggle-card-state">${linked ? `🔗 LINKED L/R PAIR` : '⚪ MONO UNLINKED'}</span>
                </button>
                <button type="button" class="ch-setup-toggle-card ${ammOn ? 'active-emerald' : ''}" onclick="toggleChAmm(${c}); renderChannelSetupModalContent(${c});">
                    <span class="toggle-card-title">🎙️ DUGAN AUTO-MIX (AMM)</span>
                    <span class="toggle-card-state">${ammOn ? '🟢 AMM ACTIVE' : '⚪ AMM OFF'}</span>
                </button>
            </div>

            <!-- Hardware Input Soft-Patch Selector + Quick Actions -->
            <div class="ch-setup-card" style="padding:10px 12px;">
                <div class="ch-setup-card-hdr" style="margin-bottom:6px;">
                    <span>🔌 HARDWARE INPUT ROUTING (SOFT-PATCH)</span>
                    <div style="display:flex; gap:8px;">
                        <button type="button" class="ch-setup-action-btn" onclick="closeChannelSetupModal(); openRenameModal('ch-${c}', 'CH ${c + 1}');">🏷️ RENAME CH ${c + 1}</button>
                        <button type="button" class="ch-setup-action-btn primary" onclick="closeChannelSetupModal(); showDspRackPage(${c});">⚡ OPEN CH ${c + 1} DSP RACK &rarr;</button>
                    </div>
                </div>
                <div class="ch-setup-hw-grid">${hwBtnsHtml}</div>
            </div>
        `;
    };

    // Hook into Pan, HPF, SoftPatch & StereoLink so badges always stay live
    const _origSetPan = window.setChannelPan;
    if (typeof _origSetPan === 'function') {
        window.setChannelPan = function(target, rawVal, skipGov) {
            _origSetPan(target, rawVal, skipGov);
            if (typeof target === 'number') window.syncChannelSetupBadge(target);
        };
    }
    const _origToggleHpf = window.toggleChannelHpf80;
    if (typeof _origToggleHpf === 'function') {
        window.toggleChannelHpf80 = function(ch) {
            _origToggleHpf(ch);
            window.syncChannelSetupBadge(ch);
        };
    }
    const _origSoftPatch = window.handleSoftPatchChange;
    if (typeof _origSoftPatch === 'function') {
        window.handleSoftPatchChange = function(ch, hwNum) {
            _origSoftPatch(ch, hwNum);
            window.syncChannelSetupBadge(ch);
        };
    }
    const _origRefreshDrop = window.refreshSoftPatchDropdowns;
    if (typeof _origRefreshDrop === 'function') {
        window.refreshSoftPatchDropdowns = function() {
            _origRefreshDrop();
            window.syncAllChannelSetupBadges();
        };
    }
    const _origStereoLink = window.toggleStereoLink;
    if (typeof _origStereoLink === 'function') {
        window.toggleStereoLink = function(pairNum) {
            _origStereoLink(pairNum);
            window.syncChannelSetupBadge(pairNum * 2);
            window.syncChannelSetupBadge(pairNum * 2 + 1);
        };
    }

    // 4. Channel DSP Rack Sub-Tabs (1. Preamp/Slots/Denoise | 2. 8-Band EQ & AFD | 3. Comp & De-Esser | 4. All 7 Stages)
    window.setDspRackSubTab = function(tabKey) {
        const tab = tabKey || 'input_fx';
        window.currentDspRackSubTab = tab;
        const grid = document.getElementById('dsp-rack-grid');
        if (grid) grid.setAttribute('data-rack-tab', tab);
        document.querySelectorAll('.btn-rack-subtab').forEach(btn => {
            btn.classList.toggle('active', btn.id === ('rack-tab-btn-' + tab));
        });
        const ch = window.dspRackCurrentChannel ?? 0;
        if ((tab === 'eq' || tab === 'all') && typeof window.renderMiniReEq === 'function') {
            window.renderMiniReEq(ch);
            requestAnimationFrame(() => window.renderMiniReEq(ch));
        }
    };

    window.addEventListener('DOMContentLoaded', () => {
        window.syncAllChannelSetupBadges();
    });
})(window);
