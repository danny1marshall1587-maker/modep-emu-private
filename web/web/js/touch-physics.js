// touch-physics.js - Fader dragging, speed damping governor, and zero-inertia physics
(function(window) {
    'use strict';

    // Realistic Audio Taper Fader to dB Conversion (0% = -inf, 75% = 0.0dB Unity gain, 100% = +10.0dB)
    window.toDbStr = function(pos) {
        if (pos <= 0.0) return "-inf dB";
        let db;
        if (pos >= 75.0) {
            db = ((pos - 75.0) / 25.0) * 10.0;
        } else if (pos >= 50.0) {
            db = -10.0 + ((pos - 50.0) / 25.0) * 10.0;
        } else if (pos >= 25.0) {
            db = -30.0 + ((pos - 25.0) / 25.0) * 20.0;
        } else {
            db = -60.0 + ((pos - 0.5) / 24.5) * 30.0;
            if (db < -60.0) return "-inf dB";
        }
        return (db >= 0 ? "+" : "") + db.toFixed(1) + " dB";
    };

    window.updateFaderSpeed = function(val) {
        window.maxSpeedDamping = parseInt(val);
        window.PatchboxState.maxSpeedDamping = window.maxSpeedDamping;
        const label = document.getElementById("speed-val");
        if (label && window.dampingLabels) {
            label.innerText = window.dampingLabels[window.maxSpeedDamping] || "SMOOTH";
        }
    };

    // Active animated faders map: targetKey -> { current: float, target: float }
    window.activeGlidingFaders = {};
    window.faderPhysicsAnimId = null;

    window.applyFaderVisual = function(target, percent) {
        percent = Math.max(0, Math.min(100, percent));
        const cap = document.getElementById(target === 'master' ? "fader-cap-master" : "fader-cap-" + target);
        const valDisplay = document.getElementById(target === 'master' ? "master-db-display" : "fader-val-" + target);
        if (cap) {
            // Precise slot throw: 14px top offset, 38px bottom offset in .fader-track
            cap.style.bottom = `calc(38px + (100% - 52px) * ${percent / 100})`;
        }
        if (valDisplay) {
            valDisplay.innerText = window.toDbStr(percent);
        }
        // Real-time synchronization to focused channel strip on DSP rack page
        if (typeof target === 'number' && target === window.dspRackCurrentChannel) {
            const focusedCap = document.getElementById('fader-cap-focused-rack');
            const focusedVal = document.getElementById('fader-val-focused-rack');
            if (focusedCap) focusedCap.style.bottom = `calc(38px + (100% - 52px) * ${percent / 100})`;
            if (focusedVal) focusedVal.innerText = window.toDbStr(percent);
        }
    };

    window.stepFaderPhysics = function() {
        const coeff = window.dampingGlideCoeffs[window.maxSpeedDamping] ?? 0.2;
        let hasActiveMotion = false;

        for (const [targetKey, state] of Object.entries(window.activeGlidingFaders)) {
            const diff = state.target - state.current;
            if (Math.abs(diff) > 0.08) {
                hasActiveMotion = true;
                if (coeff >= 1.0) {
                    state.current = state.target;
                } else {
                    state.current += diff * coeff;
                }
            } else {
                state.current = state.target;
            }

            const resolvedTarget = !isNaN(Number(targetKey)) ? Number(targetKey) : targetKey;

            if (window.currentSofMode && typeof resolvedTarget === 'number') {
                window.faderValues.sof[window.currentSofMode][resolvedTarget] = state.current;
            } else {
                window.faderValues.main[resolvedTarget] = state.current;
            }

            window.applyFaderVisual(resolvedTarget, state.current);
            if (typeof window.sendFaderToBackend === 'function') {
                window.sendFaderToBackend(resolvedTarget, state.current);
            }

            if (Math.abs(state.target - state.current) <= 0.08 && (!window.activeFader || window.activeFader.target !== resolvedTarget)) {
                delete window.activeGlidingFaders[targetKey];
            }
        }

        if (hasActiveMotion || window.activeFader) {
            window.faderPhysicsAnimId = requestAnimationFrame(window.stepFaderPhysics);
        } else {
            window.faderPhysicsAnimId = null;
        }
    };

    window.setFaderPosition = function(target, percent, isSceneRecall = false, skipBackend = false) {
        percent = Math.max(0, Math.min(100, percent));
        const targetKey = String(target);

        const currentVal = (window.currentSofMode && typeof target === 'number') 
            ? (window.faderValues.sof[window.currentSofMode][target] ?? 75)
            : (window.faderValues.main[target] ?? 75);

        if (isSceneRecall || window.maxSpeedDamping <= 1) {
            if (window.currentSofMode && typeof target === 'number') {
                window.faderValues.sof[window.currentSofMode][target] = percent;
            } else {
                window.faderValues.main[target] = percent;
            }
            window.applyFaderVisual(target, percent);
            if (!skipBackend && !window._applyingRemoteSync && typeof window.sendFaderToBackend === 'function') {
                window.sendFaderToBackend(target, percent);
            }
            if (window.activeGlidingFaders[targetKey]) {
                delete window.activeGlidingFaders[targetKey];
            }
            return;
        }

        if (!window.activeGlidingFaders[targetKey]) {
            window.activeGlidingFaders[targetKey] = {
                current: currentVal,
                target: percent
            };
        } else {
            window.activeGlidingFaders[targetKey].target = percent;
        }

        if (!window.faderPhysicsAnimId) {
            window.faderPhysicsAnimId = requestAnimationFrame(window.stepFaderPhysics);
        }
    };

    // Normalized volume controller
    window.setChannelVolume = function(target, normVal) {
        let pct = Math.max(0, Math.min(100, normVal * 100));
        if (window.touchGovernor) {
            pct = window.touchGovernor.process(`fader-${target}`, pct);
        }
        window.setFaderPosition(target, pct, false);
    };

    // Delegated MULTI-TOUCH pointer tracking for all fader caps/tracks across the entire mixer
    // Supports moving up to 8 faders simultaneously on Lenovo Tab M11 + 2-finger fine-tune + double-tap 0.0dB Unity
    window.activeFader = null;
    window.activeFadersMap = new Map();
    window.activePointersSet = new Set();
    window.activeTouchCount = 0;
    const lastFaderTapMap = {};

    document.addEventListener('pointerdown', (e) => {
        if (e.pointerId !== undefined) {
            window.activePointersSet.add(e.pointerId);
            window.activeTouchCount = window.activePointersSet.size;
        }

        // Close Portrait LV2 Drawer if tapping outside
        if (!e.target.closest('.linear-slot-container') && !e.target.closest('.btn-portrait-lv2-drawer')) {
            document.querySelectorAll('.linear-slot-container.drawer-open').forEach(el => el.classList.remove('drawer-open'));
        }

        const track = e.target.closest('.fader-track');
        if (!track) return;
        const cap = track.querySelector('.fader-cap');
        if (!cap) return;

        e.preventDefault();
        const targetAttr = cap.dataset.target || cap.dataset.channel;
        const resolvedTarget = (!isNaN(Number(targetAttr)) && targetAttr !== '') ? Number(targetAttr) : targetAttr;
        const targetKey = String(resolvedTarget);

        // Double-tap on fader cap/track -> snap to 0.0 dB Unity (75%)
        const now = Date.now();
        if (lastFaderTapMap[targetKey] && (now - lastFaderTapMap[targetKey] < 270)) {
            lastFaderTapMap[targetKey] = 0;
            window.setFaderPosition(resolvedTarget, 75.0, false);
            return;
        }
        lastFaderTapMap[targetKey] = now;

        const rect = track.getBoundingClientRect();
        const slotBottom = rect.bottom - 38;
        const slotTop = rect.top + 14;
        const travel = Math.max(24, slotBottom - slotTop);
        const capRect = cap.getBoundingClientRect();
        const capCenterY = capRect.top + capRect.height / 2;
        const hitCapDirectly = (e.target === cap || cap.contains(e.target));
        const grabOffsetY = hitCapDirectly ? (e.clientY - capCenterY) : 0;

        const startPct = (window.currentSofMode && typeof resolvedTarget === 'number')
            ? (window.faderValues.sof[window.currentSofMode][resolvedTarget] ?? 75)
            : (window.faderValues.main[resolvedTarget] ?? 75);

        cap.classList.add('dragging');
        const entry = {
            cap,
            track,
            target: resolvedTarget,
            slotBottom,
            travel,
            grabOffsetY,
            lastY: e.clientY,
            curNorm: startPct / 100.0,
            pointerId: e.pointerId ?? 1
        };

        window.activeFadersMap.set(entry.pointerId, entry);
        window.activeFader = entry;

        if (!hitCapDirectly) {
            const normVal = Math.max(0, Math.min(1, (slotBottom - e.clientY) / travel));
            entry.curNorm = normVal;
            window.setChannelVolume(resolvedTarget, normVal);
        }

        if (e.pointerId !== undefined && typeof cap.setPointerCapture === 'function') {
            try { cap.setPointerCapture(e.pointerId); } catch (_) {}
        }
    });

    window.addEventListener('pointermove', (e) => {
        const pid = e.pointerId ?? 1;
        const entry = window.activeFadersMap.get(pid) || (window.activeFader && window.activeFader.pointerId === pid ? window.activeFader : null);
        if (!entry) return;
        const { target, track, grabOffsetY } = entry;
        const rect = track.getBoundingClientRect();
        const slotBottom = rect.bottom - 38;
        const slotTop = rect.top + 14;
        const travel = Math.max(24, slotBottom - slotTop);

        // If 2+ fingers are touching AND only 1 fader is being dragged, engage 0.25x Fine-Tune Precision Mode!
        if (window.activeTouchCount >= 2 && window.activeFadersMap.size === 1) {
            const dy = entry.lastY - e.clientY;
            entry.lastY = e.clientY;
            entry.curNorm = Math.max(0, Math.min(1, entry.curNorm + (dy / travel) * 0.25));
            window.setChannelVolume(target, entry.curNorm);
            return;
        }

        entry.lastY = e.clientY;
        const effectivePointerY = e.clientY - (grabOffsetY || 0);
        const normVal = Math.max(0, Math.min(1, (slotBottom - effectivePointerY) / travel));
        entry.curNorm = normVal;
        window.setChannelVolume(target, normVal);
    });

    const handleFaderRelease = (e) => {
        if (e && e.pointerId !== undefined) {
            window.activePointersSet.delete(e.pointerId);
            window.activeTouchCount = window.activePointersSet.size;
        } else {
            window.activePointersSet.clear();
            window.activeTouchCount = 0;
        }

        const pid = (e && e.pointerId !== undefined) ? e.pointerId : null;
        if (pid !== null && window.activeFadersMap.has(pid)) {
            const entry = window.activeFadersMap.get(pid);
            if (entry && entry.cap) {
                entry.cap.classList.remove('dragging');
                if (typeof entry.cap.releasePointerCapture === 'function') {
                    try { entry.cap.releasePointerCapture(pid); } catch (_) {}
                }
            }
            window.activeFadersMap.delete(pid);
        } else if (pid === null) {
            window.activeFadersMap.forEach(entry => {
                if (entry.cap) entry.cap.classList.remove('dragging');
            });
            window.activeFadersMap.clear();
        }

        window.activeFader = window.activeFadersMap.size > 0
            ? Array.from(window.activeFadersMap.values())[window.activeFadersMap.size - 1]
            : null;

        if (window.activeFadersMap.size === 0) {
            if (window.faderPhysicsAnimId) {
                cancelAnimationFrame(window.faderPhysicsAnimId);
                window.faderPhysicsAnimId = null;
            }
            Object.keys(window.activeGlidingFaders).forEach(key => {
                window.activeGlidingFaders[key].target = window.activeGlidingFaders[key].current;
            });
        }
    };

    window.addEventListener('pointerup', handleFaderRelease);
    window.addEventListener('pointercancel', handleFaderRelease);
    window.addEventListener('mouseup', handleFaderRelease);
    window.addEventListener('touchend', handleFaderRelease);
    window.addEventListener('touchcancel', handleFaderRelease);

    // Portrait Compact LV2 4-Slot Drawer Toggle & Auto-Injection for Lenovo Tab M11
    window.togglePortraitLv2Drawer = function(chIdx, evt) {
        if (evt) evt.stopPropagation();
        const container = document.getElementById(`dsp-rack-container-${chIdx}`);
        if (!container) return;
        const wasOpen = container.classList.contains('drawer-open');
        document.querySelectorAll('.linear-slot-container.drawer-open').forEach(el => el.classList.remove('drawer-open'));
        if (!wasOpen) container.classList.add('drawer-open');
    };

    window.initPortraitLv2Drawers = function() {
        for (let i = 0; i < 8; i++) {
            const container = document.getElementById(`dsp-rack-container-${i}`);
            if (container && !document.getElementById(`btn-portrait-lv2-${i}`)) {
                const row = document.createElement('div');
                row.className = 'portrait-lv2-bar';
                row.innerHTML = `
                    <button type="button" class="btn-portrait-lv2-drawer" id="btn-portrait-lv2-${i}" onclick="togglePortraitLv2Drawer(${i}, event)" title="Open 4 LV2 Insert Slots Popover">🔌 4 SLOTS ▾</button>
                    <button type="button" class="btn-portrait-core-dsp" onclick="showDspRackPage(${i})" title="Open Channel ${i + 1} Core Desk DSP">🎛️ DSP</button>
                `;
                container.parentNode.insertBefore(row, container);
            }
        }
    };
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', window.initPortraitLv2Drawers);
    } else {
        window.initPortraitLv2Drawers();
    }
    document.addEventListener('pointerdown', (e) => {
        if (!e.target.closest('.linear-slot-container') && !e.target.closest('.btn-portrait-lv2-drawer')) {
            document.querySelectorAll('.linear-slot-container.drawer-open').forEach(el => el.classList.remove('drawer-open'));
        }
    });

    // Universal Omni-Axis (up+right = increase, down+left = decrease), 2-Finger Fine-Tune & Double-Tap Reset for sliders
    window.applySlowModeToAllSliders = function() {
        window.initPortraitLv2Drawers();
        const sliders = document.querySelectorAll(
            'input[type="range"], .ao-param-slider, .ao-slider, .ao-fader, .ao-drive-slider, .track-fader, .deess-slider, .knob-slider'
        );
        sliders.forEach(slider => {
            if (slider.dataset.slowModeBound) return;
            slider.dataset.slowModeBound = 'true';

            const min = parseFloat(slider.min);
            const max = parseFloat(slider.max);
            const step = parseFloat(slider.step) || 1;
            const defVal = slider.dataset.default !== undefined ? parseFloat(slider.dataset.default) : parseFloat(slider.getAttribute('value') || 0);
            const isKnobStyle = slider.classList.contains('knob-slider');
            let lastTap = 0;

            slider.addEventListener('pointerdown', (e) => {
                const now = performance.now();
                if (now - lastTap < 300 && !isNaN(defVal)) {
                    lastTap = 0;
                    slider.value = defVal;
                    slider.dispatchEvent(new Event('input', { bubbles: true }));
                    return;
                }
                lastTap = now;
                if (!isKnobStyle && window.maxSpeedDamping <= 1 && window.activeTouchCount < 2) return;
                e.preventDefault();
                const startX = e.clientX;
                const startY = e.clientY;
                const startVal = parseFloat(slider.value);
                const range = max - min;
                const damp = Math.max(1, window.maxSpeedDamping || 1);
                const baseCoeff = isKnobStyle ? 0.0055 : (0.015 / (1 + (damp - 1) * 0.45));

                const onMove = (ev) => {
                    if (ev.pointerId !== undefined && ev.pointerId !== e.pointerId) return;
                    const dy = startY - ev.clientY;
                    const dx = ev.clientX - startX;
                    const fine = (window.activeTouchCount >= 2) ? 0.25 : 1.0;
                    const delta = (dy + dx) * baseCoeff * fine;
                    let newVal = Math.round((startVal + delta * range) / step) * step;
                    newVal = Math.max(min, Math.min(max, newVal));
                    slider.value = newVal;
                    slider.dispatchEvent(new Event('input', { bubbles: true }));
                };
                const onUp = () => {
                    window.removeEventListener('pointermove', onMove);
                    window.removeEventListener('pointerup', onUp);
                    window.removeEventListener('pointercancel', onUp);
                };
                window.addEventListener('pointermove', onMove);
                window.addEventListener('pointerup', onUp);
                window.addEventListener('pointercancel', onUp);
            });
        });
    };

})(window);
