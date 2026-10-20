// reaeq-canvas.js - ReaEQ Cytomic SVF Filter Math & High-DPI Canvas Renderer
(function(window) {
    'use strict';
    const REEQ_COLORS = [
        "rgb(231, 33, 75)", "rgb(241, 111, 50)", "rgb(253, 199, 50)", "rgb(126, 188, 111)",
        "rgb(20, 146, 104)", "rgb(19, 124, 168)", "rgb(48, 57, 142)", "rgb(135, 45, 138)"
    ];
    const FILTER_TYPES = [
        "Peak", "Low Cut", "Low Cut (Butterworth)", "Low Shelf", "High Shelf",
        "High Cut", "High Cut (Butterworth)", "Notch", "Band Pass", "Tilt Shelf"
    ];
    const channelReEqState = {};
    function getChannelReEq(target) {
        if (!channelReEqState[target]) {
            const isMaster = (target === 'master');
            channelReEqState[target] = {
                dbRange: 18,
                selectedBand: 0,
                adaptiveRoomDynEq: isMaster,
                bands: [
                    { enabled: true, type: 1, freq: 30, gain: 0, q: 0.707, slope: 2, dynEnabled: false, dynThresh: -20, dynRange: -6.0, dynRatio: 3.0, dynGainDb: 0.0 },
                    { enabled: true, type: 3, freq: 100, gain: 2.5, q: 0.707, slope: 1, dynEnabled: isMaster, dynThresh: -20, dynRange: -5.0, dynRatio: 3.0, dynGainDb: 0.0 },
                    { enabled: true, type: 0, freq: 250, gain: -2.0, q: 1.41, slope: 1, dynEnabled: isMaster, dynThresh: -20, dynRange: -6.0, dynRatio: 3.5, dynGainDb: 0.0 },
                    { enabled: true, type: 0, freq: 800, gain: 0.0, q: 1.0, slope: 1, dynEnabled: isMaster, dynThresh: -18, dynRange: -5.0, dynRatio: 3.0, dynGainDb: 0.0 },
                    { enabled: true, type: 0, freq: 2500, gain: 1.5, q: 1.2, slope: 1, dynEnabled: isMaster, dynThresh: -18, dynRange: -6.0, dynRatio: 3.5, dynGainDb: 0.0 },
                    { enabled: true, type: 0, freq: 5000, gain: 3.0, q: 1.41, slope: 1, dynEnabled: isMaster, dynThresh: -18, dynRange: -6.0, dynRatio: 3.5, dynGainDb: 0.0 },
                    { enabled: true, type: 4, freq: 10000, gain: 2.0, q: 0.707, slope: 1, dynEnabled: isMaster, dynThresh: -20, dynRange: -4.5, dynRatio: 2.5, dynGainDb: 0.0 },
                    { enabled: true, type: 5, freq: 18000, gain: 0, q: 0.707, slope: 2, dynEnabled: false, dynThresh: -20, dynRange: -6.0, dynRatio: 3.0, dynGainDb: 0.0 }
                ]
            };
        }
        return channelReEqState[target];
    }
    window.reEqRuntime = {
        currentTarget: 0,
        activeDragNode: -1,
        tooltipActive: false,
        tooltipBand: -1,
        tooltipTimeout: null
    };
    function svf_mag_reeq(freq, g, k, m0, m1, m2, srate = 48000.0) {
        const zi = -2.0 * Math.PI * freq / srate;
        const zr_c = Math.cos(zi), zi_c = Math.sin(zi);
        const gsq = g * g, gm1 = g * m1, gk = g * k, twogsq = gsq * 2.0;
        const zsq_r = zr_c * zr_c - zi_c * zi_c, zsq_i = 2.0 * zr_c * zi_c;
        const twoz_r = zr_c * 2.0, twoz_i = zi_c * 2.0;
        const nr = gsq * m2 * (zsq_r + twoz_r + 1.0) - gm1 * (zsq_r - 1.0);
        const ni = gsq * m2 * (zsq_i + twoz_i) - gm1 * zsq_i;
        const dr = gsq + gk + zsq_r * (gsq - gk + 1.0) + zr_c * (twogsq - 2.0) + 1.0;
        const di = zsq_i * (gsq - gk + 1.0) + zi_c * (twogsq - 2.0);
        const norm = dr * dr + di * di;
        const ddr = (nr * dr + ni * di) / norm, ddi = (ni * dr - nr * di) / norm;
        const x = m0 + ddr, y = ddi;
        const s = Math.max(Math.abs(x), Math.abs(y));
        return s === 0 ? 0.0 : s * Math.sqrt((x / s) ** 2 + (y / s) ** 2);
    }
    function evalBandMagnitude(band, freq, srate = 48000.0, useDyn = false) {
        if (!band.enabled) return 1.0;
        const f = Math.max(10, Math.min(22000, band.freq));
        const q = Math.max(0.1, band.q || 0.707);
        const dynDelta = (useDyn && (band.dynEnabled || Math.abs(band.dynGainDb || 0) > 0.02)) ? (band.dynGainDb || 0) : 0;
        const gain = (band.gain || 0) + dynDelta, type = band.type || 0, slope = band.slope || 1;
        const A = Math.pow(10, gain / 40.0), g = Math.tan(Math.PI * f / srate);
        if (type === 0) {
            const k = 1.0 / (q * A);
            return svf_mag_reeq(freq, g, k, 1.0, k * (A * A - 1.0), 0.0, srate);
        } else if (type === 1) {
            const k = 1.0 / q;
            return Math.pow(svf_mag_reeq(freq, g, k, 1.0, -k, -1.0, srate), Math.max(1, slope));
        } else if (type === 2) {
            let total = 1.0, order = Math.max(1, slope * 2);
            for (let i = 0; i < slope; i++) {
                const k_i = 2.0 * Math.cos((Math.PI / (2 * order)) * (2 * i + 1));
                total *= svf_mag_reeq(freq, g, k_i, 1.0, -k_i, -1.0, srate);
            }
            return total;
        } else if (type === 3) {
            const g_ls = g / Math.sqrt(A), k_ls = 1.0 / q;
            return svf_mag_reeq(freq, g_ls, k_ls, 1.0, k_ls * (A - 1.0), (A * A - 1.0), srate);
        } else if (type === 4) {
            const g_hs = g * Math.sqrt(A), k_hs = 1.0 / q;
            return svf_mag_reeq(freq, g_hs, k_hs, A * A, k_hs * (1.0 - A) * A, (1.0 - A * A), srate);
        } else if (type === 5) {
            const k = 1.0 / q;
            return Math.pow(svf_mag_reeq(freq, g, k, 0.0, 0.0, 1.0, srate), Math.max(1, slope));
        } else if (type === 6) {
            let total = 1.0, order = Math.max(1, slope * 2);
            for (let i = 0; i < slope; i++) {
                const k_i = 2.0 * Math.cos((Math.PI / (2 * order)) * (2 * i + 1));
                total *= svf_mag_reeq(freq, g, k_i, 0.0, 0.0, 1.0, srate);
            }
            return total;
        } else if (type === 7) {
            const k = 1.0 / q;
            return svf_mag_reeq(freq, g, k, 1.0, -k, 0.0, srate);
        } else if (type === 8) {
            const k = 1.0 / q;
            return svf_mag_reeq(freq, g, k, 0.0, 1.0 / q, 0.0, srate);
        } else if (type === 9) {
            const g_hs = g * Math.sqrt(A), k_hs = 1.0 / q;
            const mag_hs = svf_mag_reeq(freq, g_hs, k_hs, A * A, k_hs * (1.0 - A) * A, (1.0 - A * A), srate);
            const A_ls = 1.0 / A, g_ls = g / Math.sqrt(A_ls);
            const mag_ls = svf_mag_reeq(freq, g_ls, k_hs, 1.0, k_hs * (A_ls - 1.0), (A_ls * A_ls - 1.0), srate);
            return mag_hs * mag_ls;
        }
        return 1.0;
    }
    const REEQ_MIN_FREQ = 20, REEQ_MAX_FREQ = 20000;
    const REEQ_FREQ_LOG_MAX = Math.log(REEQ_MAX_FREQ / REEQ_MIN_FREQ);
    if (window.eqMidFocusScale === undefined) window.eqMidFocusScale = true;

    // Piecewise Studio Mid-Focus Scale:
    // Segment A (Sub/Bass): 20 Hz .. 150 Hz -> 0.00 .. 0.14 (100 Hz sits at ~11.2% from left edge)
    // Segment B (Core Midrange): 150 Hz .. 3000 Hz -> 0.14 .. 0.74 (60% of horizontal width dedicated to 150 Hz - 3 kHz!)
    // Segment C (Highs/Air): 3000 Hz .. 20000 Hz -> 0.74 .. 1.00
    const MID_F1 = 150, MID_F2 = 3000;
    const MID_X1 = 0.14, MID_X2 = 0.74;
    const LOG_A = Math.log(MID_F1 / REEQ_MIN_FREQ);
    const LOG_B = Math.log(MID_F2 / MID_F1);
    const LOG_C = Math.log(REEQ_MAX_FREQ / MID_F2);

    function freqToX(freq, width) {
        const f = Math.max(REEQ_MIN_FREQ, Math.min(REEQ_MAX_FREQ, freq));
        if (!window.eqMidFocusScale) {
            return width * (Math.log(f / REEQ_MIN_FREQ) / REEQ_FREQ_LOG_MAX);
        }
        let norm;
        if (f <= MID_F1) {
            norm = MID_X1 * (Math.log(f / REEQ_MIN_FREQ) / LOG_A);
        } else if (f <= MID_F2) {
            norm = MID_X1 + (MID_X2 - MID_X1) * (Math.log(f / MID_F1) / LOG_B);
        } else {
            norm = MID_X2 + (1.0 - MID_X2) * (Math.log(f / MID_F2) / LOG_C);
        }
        return width * Math.max(0, Math.min(1, norm));
    }

    function xToFreq(x, width) {
        const norm = Math.max(0, Math.min(1, x / Math.max(1, width)));
        if (!window.eqMidFocusScale) {
            return Math.max(REEQ_MIN_FREQ, Math.min(REEQ_MAX_FREQ, REEQ_MIN_FREQ * Math.exp(REEQ_FREQ_LOG_MAX * norm)));
        }
        let f;
        if (norm <= MID_X1) {
            f = REEQ_MIN_FREQ * Math.exp(LOG_A * (norm / MID_X1));
        } else if (norm <= MID_X2) {
            f = MID_F1 * Math.exp(LOG_B * ((norm - MID_X1) / (MID_X2 - MID_X1)));
        } else {
            f = MID_F2 * Math.exp(LOG_C * ((norm - MID_X2) / (1.0 - MID_X2)));
        }
        return Math.max(REEQ_MIN_FREQ, Math.min(REEQ_MAX_FREQ, f));
    }

    window.toggleEqMidFocusScale = function() {
        window.eqMidFocusScale = !window.eqMidFocusScale;
        document.querySelectorAll('[data-eq-scale-btn]').forEach(btn => {
            const on = !!window.eqMidFocusScale;
            btn.textContent = on ? '🎯 MID-FOCUS SCALE (200–2.5k)' : '📏 STANDARD LOG SCALE';
            btn.style.background = on ? 'rgba(16,185,129,0.18)' : '#1e293b';
            btn.style.color = on ? '#34d399' : '#94a3b8';
            btn.style.borderColor = on ? '#10b981' : '#334155';
        });
        const fullCvs = document.getElementById('reeq-canvas');
        if (fullCvs) {
            fullCvs._eqCacheSig = '';
            drawReaEQCurve(fullCvs);
        }
        const miniCvs = document.getElementById('dsp-rack-reeq-mini');
        if (miniCvs) {
            miniCvs._eqCacheSig = '';
            if (typeof window.renderMiniReEq === 'function') {
                window.renderMiniReEq(window.dspRackCurrentChannel || 0);
            }
        }
        if (typeof window.updateQuickNodeCardUI === 'function' && window.reEqRuntime) {
            const st = getChannelReEq(window.reEqRuntime.currentTarget);
            if (st) window.updateQuickNodeCardUI(st.selectedBand || 0);
        }
    };
    function dbToY(db, height, range) {
        const topMargin = 12, bottomMargin = 28;
        const m = 1.0 - (((db / range) / 2) + 0.5);
        return topMargin + (m * (height - topMargin - bottomMargin));
    }
    function yToDb(y, height, range) {
        const topMargin = 12, bottomMargin = 28;
        const m = (y - topMargin) / (height - topMargin - bottomMargin);
        return Math.max(-range, Math.min(range, (1.0 - (m + 0.5)) * 2 * range));
    }
    // Live RTA View Mode ('peaks' | 'bars' | 'spectral' | 'off') & 60FPS Ballistics State
    window.rtaViewMode = (!window.rtaViewMode || window.rtaViewMode === 'line') ? 'peaks' : window.rtaViewMode;
    const rtaSmoothBins = new Float32Array(64).fill(-80);
    const rtaPeakHoldBins = new Float32Array(64).fill(-80);
    const rtaPeakHoldTimers = new Uint16Array(64).fill(0);

    function rtaDbToY(binDb, height, topMargin, bottomMargin) {
        const minDb = -78.0, maxDb = 0.0;
        const clamped = Math.max(minDb, Math.min(maxDb, binDb));
        const norm = Math.pow((clamped - minDb) / (maxDb - minDb), 0.85);
        const usableH = Math.max(10, height - topMargin - bottomMargin);
        return (height - bottomMargin) - norm * usableH;
    }

    function updateRtaBallistics() {
        const raw = window.liveRtaBins;
        if (!raw || !raw.length) return false;
        const n = Math.min(64, raw.length);
        for (let i = 0; i < n; i++) {
            const target = Math.max(-80, Math.min(0, raw[i] ?? -80));
            if (target > rtaSmoothBins[i]) {
                rtaSmoothBins[i] = rtaSmoothBins[i] * 0.15 + target * 0.85;
            } else {
                rtaSmoothBins[i] = rtaSmoothBins[i] * 0.72 + target * 0.28;
            }
            if (rtaSmoothBins[i] >= rtaPeakHoldBins[i]) {
                rtaPeakHoldBins[i] = rtaSmoothBins[i];
                rtaPeakHoldTimers[i] = 22; // ~0.36s hold at 60fps
            } else if (rtaPeakHoldTimers[i] > 0) {
                rtaPeakHoldTimers[i]--;
            } else {
                rtaPeakHoldBins[i] = Math.max(-80, rtaPeakHoldBins[i] - 0.75);
            }
        }
        return true;
    }

    function drawRtaSpectrum(ctx, w, h, topMargin = 12, bottomMargin = 28, isMini = false) {
        let mode = window.rtaViewMode || 'peaks';
        if (mode === 'line') mode = 'peaks';
        if (mode === 'off') return;
        if (!updateRtaBallistics()) return;

        const baseY = h - bottomMargin;
        const rtaLen = 64;
        const xs = new Float32Array(rtaLen);
        const ys = new Float32Array(rtaLen);
        const pys = new Float32Array(rtaLen);
        for (let b = 0; b < rtaLen; b++) {
            const binFreq = 20 * Math.pow(1000, b / (rtaLen - 1));
            xs[b] = freqToX(binFreq, w);
            ys[b] = rtaDbToY(rtaSmoothBins[b], h, topMargin, bottomMargin);
            pys[b] = rtaDbToY(rtaPeakHoldBins[b], h, topMargin, bottomMargin);
        }

        ctx.save();
        // Clip to plot area so RTA never bleeds into bottom frequency labels
        ctx.beginPath();
        ctx.rect(0, topMargin, w, Math.max(1, baseY - topMargin));
        ctx.clip();

        if (mode === 'bars') {
            const barGrad = ctx.createLinearGradient(0, topMargin, 0, baseY);
            barGrad.addColorStop(0.0, "rgba(244, 63, 94, 0.85)");
            barGrad.addColorStop(0.22, "rgba(245, 158, 11, 0.80)");
            barGrad.addColorStop(0.55, "rgba(56, 189, 248, 0.72)");
            barGrad.addColorStop(1.0, "rgba(14, 165, 233, 0.22)");
            ctx.fillStyle = barGrad;

            for (let b = 0; b < rtaLen; b++) {
                const xLeft = b === 0 ? xs[0] : (xs[b - 1] + xs[b]) * 0.5;
                const xRight = b === rtaLen - 1 ? xs[rtaLen - 1] : (xs[b] + xs[b + 1]) * 0.5;
                const bw = Math.max(1.5, (xRight - xLeft) - (isMini ? 1.0 : 2.0));
                const bx = xLeft + ((xRight - xLeft) - bw) * 0.5;
                const by = ys[b];
                const bh = Math.max(0, baseY - by);
                if (bh > 0.5) {
                    ctx.fillRect(bx, by, bw, bh);
                }
                // Floating Peak-Hold Cap
                if (rtaPeakHoldBins[b] > -75.5) {
                    ctx.fillStyle = "rgba(253, 224, 71, 0.9)";
                    ctx.fillRect(bx, Math.max(topMargin, pys[b] - 1.5), bw, isMini ? 1.5 : 2.0);
                    ctx.fillStyle = barGrad;
                }
            }
        } else if (mode === 'spectral') {
            // Fine Accurate Spectral Mode: 256-point Catmull-Rom interpolation + Spectral Comb + Peak Callouts
            if (!isMini) {
                ctx.font = "9px monospace";
                ctx.textAlign = "left";
                [-12, -24, -36, -48, -60].forEach(dbRef => {
                    const ry = rtaDbToY(dbRef, h, topMargin, bottomMargin);
                    ctx.fillStyle = "rgba(168, 85, 247, 0.45)";
                    ctx.fillText(dbRef + "dBFS", 6, ry - 2);
                });
            }

            const fineSteps = isMini ? 128 : 256;
            const fxs = new Float32Array(fineSteps + 1);
            const fys = new Float32Array(fineSteps + 1);
            const fpys = new Float32Array(fineSteps + 1);

            for (let s = 0; s <= fineSteps; s++) {
                const pos = (s / fineSteps) * (rtaLen - 1);
                const idx = Math.floor(pos);
                const frac = pos - idx;
                const i0 = Math.max(0, idx - 1);
                const i1 = idx;
                const i2 = Math.min(rtaLen - 1, idx + 1);
                const i3 = Math.min(rtaLen - 1, idx + 2);

                // Catmull-Rom cubic interpolation in dB domain for high-precision spectral contour
                const t = frac, t2 = t * t, t3 = t2 * t;
                const v0 = rtaSmoothBins[i0], v1 = rtaSmoothBins[i1], v2 = rtaSmoothBins[i2], v3 = rtaSmoothBins[i3];
                const interpDb = 0.5 * ((2 * v1) + (-v0 + v2) * t + (2 * v0 - 5 * v1 + 4 * v2 - v3) * t2 + (-v0 + 3 * v1 - 3 * v2 + v3) * t3);
                const pkDb = rtaPeakHoldBins[i1] * (1 - frac) + rtaPeakHoldBins[i2] * frac;

                const binFreq = 20 * Math.pow(1000, pos / (rtaLen - 1));
                fxs[s] = freqToX(binFreq, w);
                fys[s] = rtaDbToY(interpDb, h, topMargin, bottomMargin);
                fpys[s] = rtaDbToY(pkDb, h, topMargin, bottomMargin);
            }

            const specGrad = ctx.createLinearGradient(0, topMargin, 0, baseY);
            specGrad.addColorStop(0.0, "rgba(244, 63, 94, 0.62)");
            specGrad.addColorStop(0.25, "rgba(168, 85, 247, 0.48)");
            specGrad.addColorStop(0.60, "rgba(6, 182, 212, 0.34)");
            specGrad.addColorStop(1.0, "rgba(15, 23, 42, 0.08)");

            ctx.beginPath();
            ctx.moveTo(fxs[0], baseY);
            for (let s = 0; s <= fineSteps; s++) ctx.lineTo(fxs[s], fys[s]);
            ctx.lineTo(fxs[fineSteps], baseY);
            ctx.closePath();
            ctx.fillStyle = specGrad;
            ctx.fill();

            // Fine vertical spectral comb lines
            ctx.beginPath();
            const combStep = isMini ? 2 : 2;
            for (let s = 0; s <= fineSteps; s += combStep) {
                if (baseY - fys[s] > 1.5) {
                    ctx.moveTo(fxs[s], baseY);
                    ctx.lineTo(fxs[s], fys[s]);
                }
            }
            ctx.strokeStyle = "rgba(34, 211, 238, 0.22)";
            ctx.lineWidth = 1.0;
            ctx.stroke();

            // Peak-Hold Envelope Trace
            ctx.beginPath();
            for (let s = 0; s <= fineSteps; s++) {
                if (s === 0) ctx.moveTo(fxs[s], fpys[s]);
                else ctx.lineTo(fxs[s], fpys[s]);
            }
            ctx.strokeStyle = "rgba(250, 204, 21, 0.65)";
            ctx.lineWidth = 1.1;
            ctx.stroke();

            // Main Fine Spectral Contour Trace
            ctx.beginPath();
            for (let s = 0; s <= fineSteps; s++) {
                if (s === 0) ctx.moveTo(fxs[s], fys[s]);
                else ctx.lineTo(fxs[s], fys[s]);
            }
            ctx.strokeStyle = "#22d3ee";
            ctx.lineWidth = isMini ? 1.4 : 1.9;
            ctx.stroke();

            // Live Forensic Peak Callout Badges on Full-Screen EQ
            if (!isMini) {
                const peaks = [];
                for (let b = 2; b < rtaLen - 2; b++) {
                    const v = rtaSmoothBins[b];
                    if (v > -64.0 && v >= rtaSmoothBins[b - 1] && v >= rtaSmoothBins[b + 1] && v >= rtaSmoothBins[b - 2] && v >= rtaSmoothBins[b + 2]) {
                        peaks.push({ b, db: v, x: xs[b], y: ys[b], freq: 20 * Math.pow(1000, b / (rtaLen - 1)) });
                    }
                }
                peaks.sort((a, b) => b.db - a.db);
                const topPeaks = peaks.slice(0, 3);
                ctx.font = "bold 9px monospace";
                ctx.textAlign = "center";
                topPeaks.forEach(pk => {
                    const fStr = pk.freq >= 1000 ? (pk.freq / 1000).toFixed(2) + "kHz" : Math.round(pk.freq) + "Hz";
                    const lbl = `${fStr} (${pk.db.toFixed(0)}dB)`;
                    const by = Math.max(topMargin + 12, pk.y - 8);
                    ctx.fillStyle = "rgba(15, 23, 42, 0.82)";
                    ctx.fillRect(pk.x - 36, by - 9, 72, 13);
                    ctx.strokeStyle = "rgba(34, 211, 238, 0.7)";
                    ctx.lineWidth = 1;
                    ctx.strokeRect(pk.x - 36, by - 9, 72, 13);
                    ctx.fillStyle = "#67e8f9";
                    ctx.fillText(lbl, pk.x, by);
                });
            }
        } else {
            // Default: 'peaks' mode (Sharp Triangular Peaks per RTA Bin + Floating Triangle Peak-Hold Caps)
            const peakGrad = ctx.createLinearGradient(0, topMargin, 0, baseY);
            peakGrad.addColorStop(0.0, "rgba(244, 63, 94, 0.82)");
            peakGrad.addColorStop(0.28, "rgba(250, 204, 21, 0.72)");
            peakGrad.addColorStop(0.62, "rgba(56, 189, 248, 0.60)");
            peakGrad.addColorStop(1.0, "rgba(14, 165, 233, 0.10)");

            ctx.beginPath();
            for (let b = 0; b < rtaLen; b++) {
                const bh = baseY - ys[b];
                if (bh <= 0.6) continue;
                const xLeft = b === 0 ? Math.max(0, xs[0] - 4) : (xs[b - 1] + xs[b]) * 0.5;
                const xRight = b === rtaLen - 1 ? Math.min(w, xs[rtaLen - 1] + 4) : (xs[b] + xs[b + 1]) * 0.5;
                ctx.moveTo(xLeft, baseY);
                ctx.lineTo(xs[b], ys[b]);
                ctx.lineTo(xRight, baseY);
                ctx.closePath();
            }
            ctx.fillStyle = peakGrad;
            ctx.fill();

            // Crisp Triangular Spire Outlines
            ctx.beginPath();
            for (let b = 0; b < rtaLen; b++) {
                const bh = baseY - ys[b];
                if (bh <= 0.6) continue;
                const xLeft = b === 0 ? Math.max(0, xs[0] - 4) : (xs[b - 1] + xs[b]) * 0.5;
                const xRight = b === rtaLen - 1 ? Math.min(w, xs[rtaLen - 1] + 4) : (xs[b] + xs[b + 1]) * 0.5;
                ctx.moveTo(xLeft, baseY);
                ctx.lineTo(xs[b], ys[b]);
                ctx.lineTo(xRight, baseY);
            }
            ctx.strokeStyle = "rgba(56, 189, 248, 0.92)";
            ctx.lineWidth = isMini ? 1.0 : 1.35;
            ctx.stroke();

            // Sharp Floating Triangle Peak-Hold Caps (▲)
            ctx.beginPath();
            const capHalfW = isMini ? 2.2 : 3.2;
            const capH = isMini ? 2.8 : 4.0;
            for (let b = 0; b < rtaLen; b++) {
                if (rtaPeakHoldBins[b] <= -75.5) continue;
                const py = Math.max(topMargin + capH, pys[b]);
                ctx.moveTo(xs[b], py - capH);
                ctx.lineTo(xs[b] - capHalfW, py);
                ctx.lineTo(xs[b] + capHalfW, py);
                ctx.closePath();
            }
            ctx.fillStyle = "rgba(253, 224, 71, 0.95)";
            ctx.fill();
        }
        ctx.restore();
    }

    window.setRtaViewMode = function(mode, fromRemote) {
        if (mode === 'line') mode = 'peaks';
        if (!['peaks', 'bars', 'spectral', 'off'].includes(mode)) mode = 'peaks';
        window.rtaViewMode = mode;
        document.querySelectorAll('[data-rta-mode]').forEach(btn => {
            const active = btn.getAttribute('data-rta-mode') === mode;
            btn.classList.toggle('active', active);
            btn.style.background = active ? '#0284c7' : '#1e293b';
            btn.style.color = active ? '#ffffff' : '#94a3b8';
            btn.style.borderColor = active ? '#38bdf8' : '#334155';
        });
        if (!fromRemote && window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            try {
                window.meterWs.send(JSON.stringify({ type: 'dsp_param', param: 'rtaViewMode', value: mode, target: window.reEqRuntime.currentTarget }));
            } catch (e) {}
        }
        if (typeof window.drawMiniEqPreview === 'function') window.drawMiniEqPreview();
        const canvas = document.getElementById("reeq-canvas");
        if (canvas) drawReaEQCurve(canvas);
    };

    function drawReaEQCurve(canvas, currentChannelEQState) {
        if (!canvas) canvas = document.getElementById("reeq-canvas");
        if (!canvas) return;
        const state = currentChannelEQState || getChannelReEq(window.reEqRuntime.currentTarget);
        if (!state) return;
        const dpr = window.devicePixelRatio || 1;
        const cw = canvas.clientWidth || (canvas.parentElement ? canvas.parentElement.clientWidth : 0);
        const ch = canvas.clientHeight || (canvas.parentElement ? canvas.parentElement.clientHeight : 0);
        if (cw > 0 && ch > 0) {
            const targetW = Math.floor(cw * dpr);
            const targetH = Math.floor(ch * dpr);
            if (canvas.width !== targetW || canvas.height !== targetH) {
                canvas.width = targetW;
                canvas.height = targetH;
            }
        }
        const ctx = canvas.getContext("2d");
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        const w = canvas.width / dpr, h = canvas.height / dpr;
        if (w <= 0 || h <= 0) return;
        const range = state.dbRange;
        const midY = dbToY(0, h, range);

        // Cache static grid and static EQ band curves so 60FPS RTA never recalculates 6,000+ SVF points when bands are untouched
        let eqSig = `${window.reEqRuntime.currentTarget}_${canvas.width}_${canvas.height}_${range}_${state.selectedBand}_${window.eqMidFocusScale ? 1 : 0}`;
        for (let i = 0; i < state.bands.length; i++) {
            const b = state.bands[i];
            eqSig += `|${b.enabled ? 1 : 0},${b.type},${b.freq.toFixed(1)},${b.gain.toFixed(2)},${b.q.toFixed(3)},${b.slope || 1}`;
        }

        if (canvas._eqCacheSig !== eqSig || !canvas._gridLayer || !canvas._curveLayer) {
            canvas._eqCacheSig = eqSig;
            if (!canvas._gridLayer) canvas._gridLayer = document.createElement('canvas');
            if (!canvas._curveLayer) canvas._curveLayer = document.createElement('canvas');
            canvas._gridLayer.width = canvas.width;
            canvas._gridLayer.height = canvas.height;
            canvas._curveLayer.width = canvas.width;
            canvas._curveLayer.height = canvas.height;

            const gctx = canvas._gridLayer.getContext("2d");
            gctx.setTransform(dpr, 0, 0, dpr, 0, 0);
            gctx.fillStyle = "#121418";
            gctx.fillRect(0, 0, w, h);
            if (window.eqMidFocusScale) {
                const x150 = freqToX(150, w);
                const x3k = freqToX(3000, w);
                gctx.fillStyle = "rgba(16, 185, 129, 0.035)";
                gctx.fillRect(x150, 0, x3k - x150, h - 22);
            }
            const dbStep = range >= 24 ? 6 : (range >= 18 ? 3 : 2);
            gctx.font = "10px -apple-system, monospace";
            for (let db = -range; db <= range; db += dbStep) {
                const y = dbToY(db, h, range);
                gctx.beginPath();
                gctx.moveTo(0, y); gctx.lineTo(w, y);
                gctx.strokeStyle = db === 0 ? "rgba(56, 189, 248, 0.55)" : "rgba(255, 255, 255, 0.08)";
                gctx.lineWidth = db === 0 ? 1.5 : 1.0;
                gctx.stroke();
                gctx.fillStyle = db === 0 ? "#38bdf8" : "rgba(255, 255, 255, 0.4)";
                gctx.textAlign = "right";
                gctx.fillText((db > 0 ? "+" : "") + db + " dB", w - 8, y - 3);
            }
            const gridFreqs = window.eqMidFocusScale
                ? [20, 50, 100, 200, 350, 500, 800, 1200, 2000, 3000, 6000, 12000, 20000]
                : [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
            gridFreqs.forEach(f => {
                const x = freqToX(f, w);
                gctx.beginPath();
                gctx.moveTo(x, 0); gctx.lineTo(x, h - 22);
                gctx.strokeStyle = (f === 150 || f === 200 || f === 1000 || f === 2000 || f === 3000) ? "rgba(52, 211, 153, 0.16)" : "rgba(255, 255, 255, 0.06)";
                gctx.lineWidth = 1; gctx.stroke();
                gctx.fillStyle = (f >= 200 && f <= 3000 && window.eqMidFocusScale) ? "rgba(110, 231, 183, 0.78)" : "rgba(255, 255, 255, 0.45)";
                gctx.textAlign = "center";
                gctx.fillText(f >= 1000 ? (f % 1000 === 0 ? (f / 1000) + "k" : (f / 1000).toFixed(1) + "k") : f + "Hz", x, h - 8);
            });

            const cctx = canvas._curveLayer.getContext("2d");
            cctx.setTransform(dpr, 0, 0, dpr, 0, 0);
            cctx.clearRect(0, 0, w, h);
            state.bands.forEach((b, idx) => {
                if (!b.enabled) return;
                const color = REEQ_COLORS[idx % REEQ_COLORS.length];
                const isSelected = state.selectedBand === idx;
                cctx.beginPath();
                for (let x = 0; x <= w; x += 3) {
                    const mag = evalBandMagnitude(b, xToFreq(x, w), 48000);
                    const y = dbToY(20 * Math.log10(Math.max(0.0001, mag)), h, range);
                    if (x === 0) cctx.moveTo(x, y); else cctx.lineTo(x, y);
                }
                cctx.strokeStyle = color;
                cctx.lineWidth = isSelected ? 2.0 : 1.2;
                cctx.globalAlpha = isSelected ? 0.9 : 0.45;
                cctx.stroke();
                cctx.globalAlpha = 1.0;
            });
            cctx.beginPath();
            for (let x = 0; x <= w; x += 2) {
                const freq = xToFreq(x, w);
                let totalMag = 1.0;
                state.bands.forEach(b => { totalMag *= evalBandMagnitude(b, freq, 48000, false); });
                const y = dbToY(20 * Math.log10(Math.max(0.0001, totalMag)), h, range);
                if (x === 0) cctx.moveTo(x, y); else cctx.lineTo(x, y);
            }
            cctx.strokeStyle = "#ffffff";
            cctx.lineWidth = 2.5;
            cctx.stroke();
        }

        ctx.save();
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.drawImage(canvas._gridLayer, 0, 0);
        ctx.restore();

        drawRtaSpectrum(ctx, w, h, 12, 28, false);

        ctx.save();
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.drawImage(canvas._curveLayer, 0, 0);
        ctx.restore();

        // Tukan Studios Green Dynamic EQ S2: Live Modulated Envelope Curve & Node Activity
        const anyDyn = state.adaptiveRoomDynEq || state.bands.some(b => b.enabled && (b.dynEnabled || Math.abs(b.dynGainDb || 0) > 0.03));
        if (anyDyn) {
            ctx.save();
            ctx.beginPath();
            for (let x = 0; x <= w; x += 3) {
                const freq = xToFreq(x, w);
                let dynMag = 1.0;
                state.bands.forEach(b => { dynMag *= evalBandMagnitude(b, freq, 48000, true); });
                const y = dbToY(20 * Math.log10(Math.max(0.0001, dynMag)), h, range);
                if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.strokeStyle = "#10b981";
            ctx.lineWidth = 2.2;
            ctx.setLineDash([5, 3]);
            ctx.stroke();
            ctx.restore();
        }

        state.bands.forEach((b, idx) => {
            if (!b.enabled) return;
            const x = freqToX(b.freq, w);
            const isCut = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8);
            const y = isCut ? midY : dbToY(b.gain, h, range);
            const color = REEQ_COLORS[idx % REEQ_COLORS.length];
            const isSelected = state.selectedBand === idx;
            if (!isCut && (b.dynEnabled || Math.abs(b.dynGainDb || 0) > 0.05)) {
                const dynY = dbToY((b.gain || 0) + (b.dynGainDb || 0), h, range);
                ctx.beginPath();
                ctx.moveTo(x, y); ctx.lineTo(x, dynY);
                ctx.strokeStyle = "#10b981"; ctx.lineWidth = 2.5; ctx.stroke();
                ctx.beginPath();
                ctx.arc(x, dynY, 5, 0, Math.PI * 2);
                ctx.fillStyle = "#10b981"; ctx.fill();
            }
            ctx.beginPath();
            ctx.arc(x, y, isSelected ? 12 : 9, 0, Math.PI * 2);
            ctx.fillStyle = color; ctx.fill();
            ctx.strokeStyle = b.dynEnabled ? "#10b981" : "#ffffff";
            ctx.lineWidth = b.dynEnabled ? 2.2 : 1.5; ctx.stroke();
            ctx.fillStyle = "#ffffff";
            ctx.font = `bold ${isSelected ? '11px' : '9px'} sans-serif`;
            ctx.textAlign = "center"; ctx.textBaseline = "middle";
            ctx.fillText(idx + 1, x, y);
        });
    }
    function resizeReaEQCanvas(canvas, container) {
        if (!canvas) canvas = document.getElementById("reeq-canvas");
        if (!container) container = canvas ? canvas.parentElement : null;
        if (!canvas || !container) return;
        const dpr = window.devicePixelRatio || 1;
        const rect = container.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) return;
        canvas.width = Math.floor(rect.width * dpr);
        canvas.height = Math.floor(rect.height * dpr);
        const ctx = canvas.getContext('2d');
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        drawReaEQCurve(canvas, getChannelReEq(window.reEqRuntime.currentTarget));
    }
    function renderReEqCanvas() {
        const canvas = document.getElementById("reeq-canvas");
        if (!canvas) return;
        if (canvas.parentElement) resizeReaEQCanvas(canvas, canvas.parentElement);
        else drawReaEQCurve(canvas, getChannelReEq(window.reEqRuntime.currentTarget));
    }
    function dispatchReEqBand(target, bandIdx) {
        const st = getChannelReEq(target);
        if (!st || !st.bands || !st.bands[bandIdx]) return;
        const b = st.bands[bandIdx];
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            try {
                window.meterWs.send(JSON.stringify({
                    action: "set_eq_band",
                    target: target,
                    band: bandIdx,
                    enabled: b.enabled ? 1 : 0,
                    type: b.type || 0,
                    freq: b.freq || 1000,
                    gain: b.gain || 0,
                    q: b.q || 0.707,
                    slope: b.slope || 1,
                    dynEnabled: b.dynEnabled ? 1 : 0,
                    dynThresh: b.dynThresh ?? -20,
                    dynRange: b.dynRange ?? -6.0,
                    dynRatio: b.dynRatio ?? 3.0
                }));
            } catch (e) {}
        }
    }
    window.toggleAdaptiveRoomDynEq = function(target) {
        const t = target !== undefined ? target : window.reEqRuntime.currentTarget;
        const st = getChannelReEq(t);
        st.adaptiveRoomDynEq = !st.adaptiveRoomDynEq;
        if (st.adaptiveRoomDynEq) {
            for (let i = 1; i <= 6; i++) {
                st.bands[i].dynEnabled = true;
                dispatchReEqBand(t, i);
            }
        }
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            try {
                window.meterWs.send(JSON.stringify({
                    action: "set_adaptive_room_dyneq",
                    target: t,
                    enabled: st.adaptiveRoomDynEq ? 1 : 0
                }));
            } catch (e) {}
        }
        const btn = document.getElementById("reeq-tukan-adaptive-btn");
        if (btn) {
            btn.classList.toggle("active", st.adaptiveRoomDynEq);
            btn.style.background = st.adaptiveRoomDynEq ? "#059669" : "#1e293b";
            btn.style.borderColor = st.adaptiveRoomDynEq ? "#10b981" : "#334155";
            btn.innerHTML = st.adaptiveRoomDynEq ? "🌊 TUKAN DYN-EQ S2: AUTO ROOM FLAT (ON)" : "🌊 TUKAN DYN-EQ S2: OFF";
        }
        if (typeof window.updateReEqCards === 'function') window.updateReEqCards();
        renderReEqCanvas();
    };
    window.REEQ_MIN_FREQ = REEQ_MIN_FREQ;
    window.REEQ_MAX_FREQ = REEQ_MAX_FREQ;
    window.REEQ_COLORS = REEQ_COLORS;
    window.FILTER_TYPES = FILTER_TYPES;
    window.evalBandMagnitude = evalBandMagnitude;
    window.getChannelReEq = getChannelReEq;
    window.drawReaEQCurve = drawReaEQCurve;
    window.resizeReaEQCanvas = resizeReaEQCanvas;
    window.renderReEqCanvas = renderReEqCanvas;
    window.renderReaEQGraph = renderReEqCanvas;
    window.dispatchReEqBand = dispatchReEqBand;
    window.setCurrentReEqTarget = function(t) {
        window.reEqRuntime.currentTarget = t;
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            try { window.meterWs.send(JSON.stringify({ action: "set_rta_target", target: t })); } catch (e) {}
        }
    };
    window.freqToX = freqToX;
    window.xToFreq = xToFreq;
    window.dbToY = dbToY;
    window.yToDb = yToDb;
    window.drawRtaSpectrum = drawRtaSpectrum;
})(window);
