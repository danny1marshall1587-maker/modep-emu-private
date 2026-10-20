// reaeq-touch.js - ReaEQ Direct 1:1 Multi-Touch Node Dragging, 2-Finger Pinch-to-Q & Full JSFX Band Controls
(function(window) {
    'use strict';
    const LOG_20 = Math.log(20);
    const LOG_20000 = Math.log(20000);
    const LOG_SPAN = LOG_20000 - LOG_20;

    function freqToSlider(freq) {
        const f = Math.max(20, Math.min(20000, freq || 1000));
        if (typeof window.freqToX === 'function') {
            return Math.round(window.freqToX(f, 1000));
        }
        return Math.round(((Math.log(f) - LOG_20) / LOG_SPAN) * 1000);
    }

    function sliderToFreq(pos) {
        const p = Math.max(0, Math.min(1000, parseFloat(pos) || 0));
        if (typeof window.xToFreq === 'function') {
            return Math.round(window.xToFreq(p, 1000));
        }
        const norm = p / 1000.0;
        return Math.round(Math.exp(LOG_20 + norm * LOG_SPAN));
    }

    function isCutOrNotchType(t) {
        return t === 1 || t === 2 || t === 5 || t === 6 || t === 7 || t === 8;
    }

    function isCutWithSlope(t) {
        return t === 1 || t === 2 || t === 5 || t === 6;
    }

    function syncMiniRackIfOpen(target, bandIdx) {
        if (target === window.dspRackCurrentChannel) {
            if (typeof window.updateQuickNodeCardUI === 'function') window.updateQuickNodeCardUI(bandIdx);
            if (typeof window.renderMiniReEq === 'function') window.renderMiniReEq(target);
        }
    }

    function initReEqInteractions() {
        const canvas = document.getElementById("reeq-canvas");
        if (!canvas) return;
        canvas.style.touchAction = "none";
        let pinchStartDist = 0, pinchStartQ = 1.0;
        const activePts = new Map();

        function selectNearestBandAtX(clientX) {
            const rect = canvas.getBoundingClientRect();
            const mx = clientX - rect.left;
            const target = window.reEqRuntime.currentTarget;
            const state = window.getChannelReEq(target);
            const w = canvas.clientWidth || 800;
            let bestIdx = state.selectedBand || 0, minDx = 9999;
            state.bands.forEach((b, idx) => {
                if (!b.enabled) return;
                const dx = Math.abs(mx - window.freqToX(b.freq, w));
                if (dx < minDx) { minDx = dx; bestIdx = idx; }
            });
            state.selectedBand = bestIdx;
            return state.bands[bestIdx];
        }

        function applyPinchRatio(ratio) {
            const target = window.reEqRuntime.currentTarget;
            const state = window.getChannelReEq(target);
            const band = state.bands[state.selectedBand];
            if (band) {
                // Pinching apart (ratio > 1) widens bandwidth (lowers Q); pinching together (ratio < 1) narrows notch (raises Q)
                // Or intuitive Q scaling: support smooth Q adjustment across 0.1..18.0
                band.q = Math.max(0.1, Math.min(18.0, parseFloat((pinchStartQ * ratio).toFixed(2))));
                if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, state.selectedBand);
                updateReEqCards();
                window.renderReEqCanvas();
                syncMiniRackIfOpen(target, state.selectedBand);
            }
        }

        function handlePointerDown(e) {
            if (e.pointerId !== undefined) activePts.set(e.pointerId, { x: e.clientX, y: e.clientY });
            if ((e.touches && e.touches.length >= 2) || activePts.size >= 2) {
                window.reEqRuntime.activeDragNode = -1;
                if (activePts.size >= 2) {
                    const pts = Array.from(activePts.values());
                    pinchStartDist = Math.hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
                    const b = selectNearestBandAtX((pts[0].x + pts[1].x) * 0.5);
                    pinchStartQ = b ? b.q : 1.0;
                    updateReEqCards();
                    window.renderReEqCanvas();
                }
                return;
            }
            const rect = canvas.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const clientY = e.touches ? e.touches[0].clientY : e.clientY;
            const mx = clientX - rect.left, my = clientY - rect.top;
            const target = window.reEqRuntime.currentTarget;
            const state = window.getChannelReEq(target);
            const w = canvas.clientWidth || 800, h = canvas.clientHeight || 340;
            let hitIndex = -1, minDist = 42;
            state.bands.forEach((b, idx) => {
                if (!b.enabled) return;
                const nx = window.freqToX(b.freq, w);
                const ny = isCutOrNotchType(b.type)
                    ? window.dbToY(0, h, state.dbRange)
                    : window.dbToY(b.gain, h, state.dbRange);
                const dist = Math.hypot(mx - nx, my - ny);
                if (dist < minDist) { minDist = dist; hitIndex = idx; }
            });
            if (hitIndex !== -1) {
                window.reEqRuntime.activeDragNode = hitIndex;
                state.selectedBand = hitIndex;
                updateReEqCards();
                window.renderReEqCanvas();
            }
        }

        function handlePointerMove(e) {
            if (e.pointerId !== undefined && activePts.has(e.pointerId)) {
                activePts.set(e.pointerId, { x: e.clientX, y: e.clientY });
            }
            if (activePts.size >= 2 && pinchStartDist > 8) {
                e.preventDefault();
                const pts = Array.from(activePts.values());
                const curDist = Math.hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
                applyPinchRatio(curDist / pinchStartDist);
                return;
            }
            const activeNode = window.reEqRuntime.activeDragNode;
            if (activeNode === -1) return;
            if (e.touches && e.touches.length >= 2) return;
            e.preventDefault();
            const rect = canvas.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const clientY = e.touches ? e.touches[0].clientY : e.clientY;
            const target = window.reEqRuntime.currentTarget;
            const state = window.getChannelReEq(target);
            const band = state.bands[activeNode];
            if (!band) return;
            const w = canvas.clientWidth || 800, h = canvas.clientHeight || 340;
            const curX = Math.max(0, Math.min(w, clientX - rect.left));
            const curY = Math.max(12, Math.min(h - 28, clientY - rect.top));

            /* Direct 1:1 unthrottled frequency and gain mapping (Rule 11: Zero slowing on EQ) */
            band.freq = Math.max(20, Math.min(20000, Math.round(window.xToFreq(curX, w))));
            if (!isCutOrNotchType(band.type)) {
                band.gain = parseFloat(window.yToDb(curY, h, state.dbRange).toFixed(1));
            }
            if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, activeNode);
            updateReEqCards();
            window.renderReEqCanvas();
            syncMiniRackIfOpen(target, activeNode);
        }

        function handlePointerUp(e) {
            if (e && e.pointerId !== undefined) activePts.delete(e.pointerId);
            else activePts.clear();
            if (activePts.size < 2) pinchStartDist = 0;
            if (activePts.size === 0) window.reEqRuntime.activeDragNode = -1;
        }

        canvas.onpointerdown = handlePointerDown;
        window.addEventListener('pointermove', handlePointerMove);
        window.addEventListener('pointerup', handlePointerUp);
        window.addEventListener('pointercancel', handlePointerUp);

        canvas.ontouchstart = (e) => {
            e.preventDefault();
            if (e.touches.length === 2) {
                window.reEqRuntime.activeDragNode = -1;
                pinchStartDist = Math.hypot(
                    e.touches[1].clientX - e.touches[0].clientX,
                    e.touches[1].clientY - e.touches[0].clientY
                );
                const b = selectNearestBandAtX((e.touches[0].clientX + e.touches[1].clientX) * 0.5);
                pinchStartQ = b ? b.q : 1.0;
                updateReEqCards();
                window.renderReEqCanvas();
            } else if (e.touches.length === 1) {
                handlePointerDown(e);
            }
        };

        window.ontouchmove = (e) => {
            if (e.touches.length === 2 && pinchStartDist > 0) {
                e.preventDefault();
                const currentDist = Math.hypot(
                    e.touches[1].clientX - e.touches[0].clientX,
                    e.touches[1].clientY - e.touches[0].clientY
                );
                applyPinchRatio(currentDist / pinchStartDist);
            } else if (e.touches.length === 1 && window.reEqRuntime.activeDragNode !== -1) {
                e.preventDefault();
                handlePointerMove(e);
            }
        };

        window.ontouchend = (e) => {
            if (e.touches.length < 2) pinchStartDist = 0;
            if (e.touches.length === 0) handlePointerUp();
        };

        canvas.onwheel = (e) => {
            e.preventDefault();
            const target = window.reEqRuntime.currentTarget;
            const state = window.getChannelReEq(target);
            const band = state.bands[state.selectedBand];
            if (band) {
                const delta = e.deltaY < 0 ? 0.12 : -0.12;
                band.q = Math.max(0.1, Math.min(18.0, parseFloat((band.q + delta).toFixed(2))));
                if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, state.selectedBand);
                updateReEqCards();
                window.renderReEqCanvas();
                syncMiniRackIfOpen(target, state.selectedBand);
            }
        };
    }

    function updateReEqCards() {
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        state.bands.forEach((b, idx) => {
            const card = document.getElementById("reeq-card-" + idx);
            if (card) {
                card.className = "reeq-node-card" + (state.selectedBand === idx ? " selected" : "") + (!b.enabled ? " bypassed" : "");
                const freqEl = document.getElementById("reeq-freq-" + idx);
                if (freqEl) freqEl.innerText = b.freq >= 1000 ? (b.freq / 1000).toFixed(2) + " kHz" : b.freq + " Hz";
                const freqSl = document.getElementById("reeq-freq-slider-" + idx);
                if (freqSl) freqSl.value = freqToSlider(b.freq);

                const gainEl = document.getElementById("reeq-gain-" + idx);
                if (gainEl) gainEl.innerText = isCutOrNotchType(b.type) ? "—" : ((b.gain >= 0 ? "+" : "") + b.gain.toFixed(1) + " dB");
                const gainSl = document.getElementById("reeq-gain-slider-" + idx);
                if (gainSl) {
                    gainSl.value = b.gain;
                    gainSl.disabled = isCutOrNotchType(b.type);
                }

                const qEl = document.getElementById("reeq-q-" + idx);
                if (qEl) qEl.innerText = "Q " + b.q.toFixed(2);
                const qSl = document.getElementById("reeq-q-slider-" + idx);
                if (qSl) qSl.value = b.q;

                const slopeSel = document.getElementById("reeq-slope-" + idx);
                if (slopeSel) {
                    slopeSel.value = b.slope || 1;
                    slopeSel.style.display = isCutWithSlope(b.type) ? "block" : "none";
                }
                const dynBtn = document.getElementById("reeq-dyn-btn-" + idx);
                if (dynBtn) {
                    dynBtn.classList.toggle("active", !!b.dynEnabled);
                    dynBtn.style.background = b.dynEnabled ? "#059669" : "#1e293b";
                    dynBtn.style.borderColor = b.dynEnabled ? "#10b981" : "#334155";
                    const dg = b.dynGainDb || 0;
                    dynBtn.innerText = b.dynEnabled ? (Math.abs(dg) > 0.05 ? `DYN ${(dg >= 0 ? '+' : '') + dg.toFixed(1)}dB` : "DYN ON") : "DYN";
                }
                const dynBox = document.getElementById("reeq-dyn-box-" + idx);
                if (dynBox) dynBox.style.display = b.dynEnabled ? "flex" : "none";
                const dynThVal = document.getElementById("reeq-dyn-th-val-" + idx);
                if (dynThVal) dynThVal.innerText = (b.dynThresh ?? -20) + "dB";
                const dynRgVal = document.getElementById("reeq-dyn-rg-val-" + idx);
                if (dynRgVal) dynRgVal.innerText = ((b.dynRange ?? -6) >= 0 ? "+" : "") + (b.dynRange ?? -6) + "dB";
            }
        });
    }

    function selectReEqBand(idx) {
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        state.selectedBand = idx;
        updateReEqCards();
        window.renderReEqCanvas();
    }

    function changeReEqType(idx, typeVal) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.selectedBand = idx;
        state.bands[idx].type = parseInt(typeVal, 10);
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
        syncMiniRackIfOpen(target, idx);
    }

    function changeReEqSlope(idx, slopeVal) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.selectedBand = idx;
        state.bands[idx].slope = Math.max(1, Math.min(4, parseInt(slopeVal, 10) || 1));
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
        syncMiniRackIfOpen(target, idx);
    }

    function changeReEqFreqSlider(idx, sliderPos) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.selectedBand = idx;
        state.bands[idx].freq = sliderToFreq(sliderPos);
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
        syncMiniRackIfOpen(target, idx);
    }

    function changeReEqGain(idx, gainVal) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.selectedBand = idx;
        state.bands[idx].gain = parseFloat(parseFloat(gainVal).toFixed(1));
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
        syncMiniRackIfOpen(target, idx);
    }

    function changeReEqQ(idx, qVal) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.selectedBand = idx;
        state.bands[idx].q = Math.max(0.1, Math.min(18.0, parseFloat(parseFloat(qVal).toFixed(2))));
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
        syncMiniRackIfOpen(target, idx);
    }

    function toggleReEqBand(idx) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.bands[idx].enabled = !state.bands[idx].enabled;
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
        syncMiniRackIfOpen(target, idx);
    }

    function toggleReEqBandDyn(idx) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.selectedBand = idx;
        state.bands[idx].dynEnabled = !state.bands[idx].dynEnabled;
        if (!state.bands[idx].dynEnabled) state.bands[idx].dynGainDb = 0.0;
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
    }

    function changeReEqDynThresh(idx, val) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.bands[idx].dynThresh = parseFloat(val);
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
    }

    function changeReEqDynRange(idx, val) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.bands[idx].dynRange = parseFloat(val);
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
    }

    function resetReEqBand(idx) {
        const target = window.reEqRuntime.currentTarget;
        const state = window.getChannelReEq(target);
        state.selectedBand = idx;
        state.bands[idx].gain = 0.0;
        state.bands[idx].q = 0.71;
        if (typeof window.dispatchReEqBand === 'function') window.dispatchReEqBand(target, idx);
        updateReEqCards();
        window.renderReEqCanvas();
        syncMiniRackIfOpen(target, idx);
    }

    function setReEqRange(val) {
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        state.dbRange = parseInt(val, 10);
        window.renderReEqCanvas();
    }

    function toggleReaEQFullscreen() {
        const modal = document.getElementById("modal-container");
        const btn = document.getElementById("reeq-toggle-fs-btn");
        if (!modal) return;
        const isFs = modal.classList.toggle("fullscreen");
        if (btn) btn.innerText = isFs ? "⛶ WINDOWED" : "⛶ FULLSCREEN";
        setTimeout(() => {
            const canvas = document.getElementById("reeq-canvas");
            if (canvas) window.resizeReaEQCanvas(canvas, canvas.parentElement);
        }, 30);
    }

    window.freqToSlider = freqToSlider;
    window.sliderToFreq = sliderToFreq;
    window.initReEqInteractions = initReEqInteractions;
    window.updateReEqCards = updateReEqCards;
    window.selectReEqBand = selectReEqBand;
    window.changeReEqType = changeReEqType;
    window.changeReEqSlope = changeReEqSlope;
    window.changeReEqFreqSlider = changeReEqFreqSlider;
    window.changeReEqGain = changeReEqGain;
    window.changeReEqQ = changeReEqQ;
    window.toggleReEqBand = toggleReEqBand;
    window.toggleReEqBandDyn = toggleReEqBandDyn;
    window.changeReEqDynThresh = changeReEqDynThresh;
    window.changeReEqDynRange = changeReEqDynRange;
    window.resetReEqBand = resetReEqBand;
    window.setReEqRange = setReEqRange;
    window.toggleReaEQFullscreen = toggleReaEQFullscreen;
})(window);
