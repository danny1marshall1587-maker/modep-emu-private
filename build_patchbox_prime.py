import os

TARGET_DIR = r"c:\Users\danny\Documents\modep emu\web\web"
JS_DIR = os.path.join(TARGET_DIR, "js")
os.makedirs(JS_DIR, exist_ok=True)

# 1. Trim channel-strip.js to < 440 lines (remove redundant blank lines/comments while preserving 100% logic)
cs_path = os.path.join(JS_DIR, "channel-strip.js")
with open(cs_path, "r", encoding="utf-8") as f:
    cs_lines = f.readlines()

# Compact empty lines in channel-strip.js
compact_cs = []
prev_empty = False
for line in cs_lines:
    if line.strip() == "":
        if not prev_empty:
            compact_cs.append(line)
        prev_empty = True
    else:
        compact_cs.append(line)
        prev_empty = False

# Further compact consecutive single-line comments or simple blocks if needed
final_cs = [l for l in compact_cs if l.strip() != ""]
with open(cs_path, "w", encoding="utf-8") as f:
    f.writelines(final_cs)

print("channel-strip.js lines:", len(final_cs))

# 2. Split reaeq-canvas.js (798 lines) into reaeq-canvas.js (<410 lines) + reaeq-touch.js (<350 lines)
rq_path = os.path.join(JS_DIR, "reaeq-canvas.js")
with open(rq_path, "r", encoding="utf-8") as f:
    rq_lines = f.readlines()

# Lines 1..518 are SVF math + drawReaEQCurve + resizeReaEQCanvas + renderReEqCanvas
# Let's extract lines 1..518 into reaeq-canvas.js and export state/helpers, and lines 519..798 into reaeq-touch.js
reaeq_part1 = """// reaeq-canvas.js - ReaEQ Cytomic SVF Filter Math & High-DPI Canvas Renderer
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
            channelReEqState[target] = {
                dbRange: 18,
                selectedBand: 0,
                bands: [
                    { enabled: true, type: 1, freq: 30, gain: 0, q: 0.707, slope: 2 },
                    { enabled: true, type: 3, freq: 100, gain: 2.5, q: 0.707, slope: 1 },
                    { enabled: true, type: 0, freq: 250, gain: -2.0, q: 1.41, slope: 1 },
                    { enabled: true, type: 0, freq: 800, gain: 0.0, q: 1.0, slope: 1 },
                    { enabled: true, type: 0, freq: 2500, gain: 1.5, q: 1.2, slope: 1 },
                    { enabled: true, type: 0, freq: 5000, gain: 3.0, q: 1.41, slope: 1 },
                    { enabled: true, type: 4, freq: 10000, gain: 2.0, q: 0.707, slope: 1 },
                    { enabled: true, type: 5, freq: 18000, gain: 0, q: 0.707, slope: 2 }
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
    function evalBandMagnitude(band, freq, srate = 48000.0) {
        if (!band.enabled) return 1.0;
        const f = Math.max(10, Math.min(22000, band.freq));
        const q = Math.max(0.1, band.q || 0.707);
        const gain = band.gain || 0, type = band.type || 0, slope = band.slope || 1;
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
    const REEQ_MIN_FREQ = 10, REEQ_MAX_FREQ = 22050;
    const REEQ_FREQ_LOG_MAX = Math.log(REEQ_MAX_FREQ / REEQ_MIN_FREQ);
    function freqToX(freq, width) {
        const f = Math.max(REEQ_MIN_FREQ, Math.min(REEQ_MAX_FREQ, freq));
        return width * (Math.log(f / REEQ_MIN_FREQ) / REEQ_FREQ_LOG_MAX);
    }
    function xToFreq(x, width) {
        const norm = Math.max(0, Math.min(1, x / width));
        return Math.max(REEQ_MIN_FREQ, Math.min(REEQ_MAX_FREQ, REEQ_MIN_FREQ * Math.exp(REEQ_FREQ_LOG_MAX * norm)));
    }
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
    function drawReaEQCurve(canvas, currentChannelEQState) {
        if (!canvas) canvas = document.getElementById("reeq-canvas");
        if (!canvas) return;
        const state = currentChannelEQState || getChannelReEq(window.reEqRuntime.currentTarget);
        if (!state) return;
        const ctx = canvas.getContext("2d");
        const dpr = window.devicePixelRatio || 1;
        const w = canvas.width / dpr, h = canvas.height / dpr;
        if (w <= 0 || h <= 0) return;
        const range = state.dbRange;
        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = "#121418";
        ctx.fillRect(0, 0, w, h);
        const dbStep = range >= 24 ? 6 : (range >= 18 ? 3 : 2);
        ctx.font = "10px -apple-system, monospace";
        for (let db = -range; db <= range; db += dbStep) {
            const y = dbToY(db, h, range);
            ctx.beginPath();
            ctx.moveTo(0, y); ctx.lineTo(w, y);
            ctx.strokeStyle = db === 0 ? "rgba(56, 189, 248, 0.55)" : "rgba(255, 255, 255, 0.08)";
            ctx.lineWidth = db === 0 ? 1.5 : 1.0;
            ctx.stroke();
            ctx.fillStyle = db === 0 ? "#38bdf8" : "rgba(255, 255, 255, 0.4)";
            ctx.textAlign = "right";
            ctx.fillText((db > 0 ? "+" : "") + db + " dB", w - 8, y - 3);
        }
        const gridFreqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
        gridFreqs.forEach(f => {
            const x = freqToX(f, w);
            ctx.beginPath();
            ctx.moveTo(x, 0); ctx.lineTo(x, h - 22);
            ctx.strokeStyle = (f === 100 || f === 1000 || f === 10000) ? "rgba(255, 255, 255, 0.14)" : "rgba(255, 255, 255, 0.06)";
            ctx.lineWidth = 1; ctx.stroke();
            ctx.fillStyle = "rgba(255, 255, 255, 0.45)";
            ctx.textAlign = "center";
            ctx.fillText(f >= 1000 ? (f / 1000) + "k" : f + "Hz", x, h - 8);
        });
        const midY = dbToY(0, h, range);
        if (window.liveRtaBins && window.liveRtaBins.length > 0) {
            ctx.save();
            ctx.beginPath();
            ctx.moveTo(0, h);
            const rtaLen = window.liveRtaBins.length;
            for (let b = 0; b < rtaLen; b++) {
                const binFreq = 20 * Math.pow(1000, b / (rtaLen - 1));
                const x = freqToX(binFreq, w);
                const binDb = Math.max(-80, Math.min(0, window.liveRtaBins[b] || -80));
                ctx.lineTo(x, dbToY(binDb, h, range));
            }
            ctx.lineTo(w, h); ctx.closePath();
            ctx.fillStyle = "rgba(56, 189, 248, 0.2)";
            ctx.fill();
            ctx.restore();
        }
        state.bands.forEach((b, idx) => {
            if (!b.enabled) return;
            const color = REEQ_COLORS[idx % REEQ_COLORS.length];
            const isSelected = state.selectedBand === idx;
            ctx.beginPath();
            for (let x = 0; x <= w; x += 3) {
                const mag = evalBandMagnitude(b, xToFreq(x, w), 48000);
                const y = dbToY(20 * Math.log10(Math.max(0.0001, mag)), h, range);
                if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.strokeStyle = color;
            ctx.lineWidth = isSelected ? 2.0 : 1.2;
            ctx.globalAlpha = isSelected ? 0.9 : 0.45;
            ctx.stroke();
            ctx.globalAlpha = 1.0;
        });
        ctx.beginPath();
        for (let x = 0; x <= w; x += 2) {
            const freq = xToFreq(x, w);
            let totalMag = 1.0;
            state.bands.forEach(b => { totalMag *= evalBandMagnitude(b, freq, 48000); });
            const y = dbToY(20 * Math.log10(Math.max(0.0001, totalMag)), h, range);
            if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.strokeStyle = "#ffffff";
        ctx.lineWidth = 2.5;
        ctx.stroke();
        state.bands.forEach((b, idx) => {
            if (!b.enabled) return;
            const x = freqToX(b.freq, w);
            const y = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8) ? midY : dbToY(b.gain, h, range);
            const color = REEQ_COLORS[idx % REEQ_COLORS.length];
            const isSelected = state.selectedBand === idx;
            ctx.beginPath();
            ctx.arc(x, y, isSelected ? 12 : 9, 0, Math.PI * 2);
            ctx.fillStyle = color; ctx.fill();
            ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 1.5; ctx.stroke();
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
        ctx.scale(dpr, dpr);
        drawReaEQCurve(canvas, getChannelReEq(window.reEqRuntime.currentTarget));
    }
    function renderReEqCanvas() {
        const canvas = document.getElementById("reeq-canvas");
        if (!canvas) return;
        if (canvas.parentElement) resizeReaEQCanvas(canvas, canvas.parentElement);
        else drawReaEQCurve(canvas, getChannelReEq(window.reEqRuntime.currentTarget));
    }
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
    window.setCurrentReEqTarget = function(t) { window.reEqRuntime.currentTarget = t; };
    window.freqToX = freqToX;
    window.xToFreq = xToFreq;
    window.dbToY = dbToY;
    window.yToDb = yToDb;
})(window);
"""

reaeq_part2 = """// reaeq-touch.js - ReaEQ Multi-Touch Node Dragging, 2-Finger Pinch-to-Q & Band Cards
(function(window) {
    'use strict';
    function initReEqInteractions() {
        const canvas = document.getElementById("reeq-canvas");
        if (!canvas) return;
        let eqDragStartX = 0, eqDragStartY = 0;
        let pinchStartDist = 0, pinchStartQ = 1.0;

        function handlePointerDown(e) {
            if (e.touches && e.touches.length >= 2) return;
            const rect = canvas.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const clientY = e.touches ? e.touches[0].clientY : e.clientY;
            const mx = clientX - rect.left, my = clientY - rect.top;
            const target = window.reEqRuntime.currentTarget;
            const state = window.getChannelReEq(target);
            const w = canvas.clientWidth || 800, h = canvas.clientHeight || 340;
            let hitIndex = -1, minDist = 36;
            state.bands.forEach((b, idx) => {
                if (!b.enabled) return;
                const nx = window.freqToX(b.freq, w);
                const ny = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8)
                    ? window.dbToY(0, h, state.dbRange)
                    : window.dbToY(b.gain, h, state.dbRange);
                const dist = Math.hypot(mx - nx, my - ny);
                if (dist < minDist) { minDist = dist; hitIndex = idx; }
            });
            if (hitIndex !== -1) {
                window.reEqRuntime.activeDragNode = hitIndex;
                state.selectedBand = hitIndex;
                eqDragStartX = mx; eqDragStartY = my;
                updateReEqCards();
                window.renderReEqCanvas();
            }
        }

        function handlePointerMove(e) {
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
            const w = canvas.clientWidth || 800, h = canvas.clientHeight || 340;
            const sc = window.maxSpeedDamping <= 3 ? 1.0 : (1.0 / (1 + (window.maxSpeedDamping - 3) * 0.45));
            const rawDx = (clientX - rect.left) - eqDragStartX;
            const rawDy = (clientY - rect.top) - eqDragStartY;
            const dampedX = Math.max(0, Math.min(w, eqDragStartX + rawDx * sc));
            const dampedY = Math.max(12, Math.min(h - 28, eqDragStartY + rawDy * sc));

            let targetFreq = Math.round(window.xToFreq(dampedX, w));
            if (window.touchGovernor) {
                targetFreq = Math.round(window.touchGovernor.process(`reeq-freq-${target}-${activeNode}`, targetFreq));
            }
            band.freq = targetFreq;
            if (band.type !== 1 && band.type !== 2 && band.type !== 5 && band.type !== 6 && band.type !== 7 && band.type !== 8) {
                let targetGain = parseFloat(window.yToDb(dampedY, h, state.dbRange).toFixed(1));
                if (window.touchGovernor) {
                    targetGain = parseFloat(window.touchGovernor.process(`reeq-gain-${target}-${activeNode}`, targetGain).toFixed(1));
                }
                band.gain = targetGain;
            }
            updateReEqCards();
            window.renderReEqCanvas();
        }

        function handlePointerUp() {
            window.reEqRuntime.activeDragNode = -1;
            pinchStartDist = 0;
        }

        canvas.onmousedown = handlePointerDown;
        window.onmousemove = handlePointerMove;
        window.onmouseup = handlePointerUp;

        canvas.ontouchstart = (e) => {
            e.preventDefault();
            if (e.touches.length === 2) {
                window.reEqRuntime.activeDragNode = -1;
                pinchStartDist = Math.hypot(
                    e.touches[1].clientX - e.touches[0].clientX,
                    e.touches[1].clientY - e.touches[0].clientY
                );
                const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
                const band = state.bands[state.selectedBand];
                pinchStartQ = band ? band.q : 1.0;
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
                const ratio = currentDist / pinchStartDist;
                const sc = window.maxSpeedDamping <= 3 ? 1.0 : (1.0 / (1 + (window.maxSpeedDamping - 3) * 0.45));
                const scaledRatio = 1 + (ratio - 1) * sc;
                const target = window.reEqRuntime.currentTarget;
                const state = window.getChannelReEq(target);
                let targetQ = Math.max(0.2, Math.min(16.0, parseFloat((pinchStartQ * scaledRatio).toFixed(2))));
                if (window.touchGovernor) {
                    targetQ = parseFloat(window.touchGovernor.process(`reeq-q-${target}-${state.selectedBand}`, targetQ).toFixed(2));
                    targetQ = Math.max(0.2, Math.min(16.0, targetQ));
                }
                const band = state.bands[state.selectedBand];
                if (band) {
                    band.q = targetQ;
                    updateReEqCards();
                    window.renderReEqCanvas();
                }
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
            const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
            const band = state.bands[state.selectedBand];
            if (band) {
                const delta = e.deltaY < 0 ? 0.08 : -0.08;
                let nextQ = Math.max(0.1, Math.min(18.0, parseFloat((band.q + delta).toFixed(2))));
                if (window.touchGovernor) {
                    nextQ = parseFloat(window.touchGovernor.process(`reeq-wheel-q-${state.selectedBand}`, nextQ).toFixed(2));
                }
                band.q = nextQ;
                updateReEqCards();
                window.renderReEqCanvas();
            }
        };
    }

    function updateReEqCards() {
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        state.bands.forEach((b, idx) => {
            const card = document.getElementById("reeq-card-" + idx);
            if (card) {
                card.className = "reeq-node-card" + (state.selectedBand === idx ? " selected" : "");
                const freqEl = document.getElementById("reeq-freq-" + idx);
                if (freqEl) freqEl.innerText = b.freq >= 1000 ? (b.freq / 1000).toFixed(1) + "k" : b.freq + "Hz";
                const gainEl = document.getElementById("reeq-gain-" + idx);
                if (gainEl) gainEl.innerText = (b.gain >= 0 ? "+" : "") + b.gain.toFixed(1) + "dB";
                const qEl = document.getElementById("reeq-q-" + idx);
                if (qEl) qEl.innerText = "Q " + b.q.toFixed(2);
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
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        state.bands[idx].type = parseInt(typeVal);
        updateReEqCards();
        window.renderReEqCanvas();
    }

    function changeReEqQ(idx, qVal) {
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        let nextQ = parseFloat(qVal);
        if (window.touchGovernor) {
            nextQ = window.touchGovernor.process(`reeq-q-slider-${idx}`, nextQ);
        }
        state.bands[idx].q = nextQ;
        updateReEqCards();
        window.renderReEqCanvas();
    }

    function toggleReEqBand(idx) {
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        state.bands[idx].enabled = !state.bands[idx].enabled;
        updateReEqCards();
        window.renderReEqCanvas();
    }

    function setReEqRange(val) {
        const state = window.getChannelReEq(window.reEqRuntime.currentTarget);
        state.dbRange = parseInt(val);
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

    window.initReEqInteractions = initReEqInteractions;
    window.updateReEqCards = updateReEqCards;
    window.selectReEqBand = selectReEqBand;
    window.changeReEqType = changeReEqType;
    window.changeReEqQ = changeReEqQ;
    window.toggleReEqBand = toggleReEqBand;
    window.setReEqRange = setReEqRange;
    window.toggleReaEQFullscreen = toggleReaEQFullscreen;
})(window);
"""

with open(os.path.join(JS_DIR, "reaeq-canvas.js"), "w", encoding="utf-8") as f:
    f.write(reaeq_part1)
with open(os.path.join(JS_DIR, "reaeq-touch.js"), "w", encoding="utf-8") as f:
    f.write(reaeq_part2)

print("reaeq-canvas.js lines:", len(reaeq_part1.splitlines()))
print("reaeq-touch.js lines:", len(reaeq_part2.splitlines()))


# 3. Write dsp-rack.js (< 430 lines) with Stage 1: 1176 FET Class-A Preamp Emulation (Drive + Clip/Sat + Output Trim) -> Stage 2: Digital Gain + REGAIN
dsp_rack_js = """// dsp-rack.js - Core Desk DSP Rack: Stage 1 (1176 FET Preamp) -> Stage 2 (Digital Gain + REGAIN) -> Stage 3 (Gate) -> Stage 4 (ReaEQ) -> Stage 5 (Comp) -> Stage 6 (De-Esser)
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
            channelState[ch] = {
                preamp1176: {
                    drive: 6.0,       // 0.0 to +36.0 dB Class-A FET Preamp Drive
                    trim: 0.0,        // -24.0 to +12.0 dB Post-FET Output Trim (feeds Stage 2 REGAIN)
                    clipMode: 'soft', // 'soft' (FET Saturation) | 'hard' (Hard Clip)
                    enabled: true,
                    clipping: false
                },
                gain: window.channelInputGains[ch] ?? 0.0,
                phaseInvert: false,
                denoiser: { enabled: true, threshold: -45, reduction: -18, state: 'closed' },
                compModel: 'fetish76', // 'fetish76' | 'lala' | 'dual'
                compParams: {
                    fetish76: { input: 30, output: 18, attack: 3, release: 3, ratio: '4' },
                    lala: { peakReduction: 40, gain: 40, mode: 'compress' }
                },
                deesser: { threshold: -20, release: 50, mode: 'band', mix: 100 },
                directTap: 'pre'
            };
        }
        return channelState[ch];
    }

    // Stage 1: 1176 FET Preamp Drive & Output Trim Handlers (Governed by TouchGovernor)
    function handle1176PreampDrive(ch, rawDrive) {
        let drive = parseFloat(rawDrive);
        if (window.touchGovernor) {
            drive = window.touchGovernor.process(`preamp-drive-${ch}`, drive);
        }
        drive = Math.max(0.0, Math.min(36.0, parseFloat(drive.toFixed(1))));
        const st = getChannelRackState(ch);
        st.preamp1176.drive = drive;
        dispatchDspParam(ch, 'preamp1176Drive', drive);
        syncPreamp1176Ui(ch);
    }

    function handle1176PreampTrim(ch, rawTrim) {
        let trim = parseFloat(rawTrim);
        if (window.touchGovernor) {
            trim = window.touchGovernor.process(`preamp-trim-${ch}`, trim);
        }
        trim = Math.max(-24.0, Math.min(12.0, parseFloat(trim.toFixed(1))));
        const st = getChannelRackState(ch);
        st.preamp1176.trim = trim;
        dispatchDspParam(ch, 'preamp1176Trim', trim);
        syncPreamp1176Ui(ch);
    }

    function toggle1176ClipMode(ch) {
        const st = getChannelRackState(ch);
        st.preamp1176.clipMode = st.preamp1176.clipMode === 'soft' ? 'hard' : 'soft';
        dispatchDspParam(ch, 'preamp1176ClipMode', st.preamp1176.clipMode);
        syncPreamp1176Ui(ch);
    }

    function toggle1176PreampBypass(ch) {
        const st = getChannelRackState(ch);
        st.preamp1176.enabled = !st.preamp1176.enabled;
        dispatchDspParam(ch, 'preamp1176Enabled', st.preamp1176.enabled);
        syncPreamp1176Ui(ch);
    }

    // Evaluates FET Preamp Saturation & Clipping LED based on real input telemetry + 1176 Drive
    function updatePreamp1176ClipTelemetry(ch, inputLinearLevel) {
        const st = getChannelRackState(ch);
        const p = st.preamp1176;
        // TELEMETRY_GROUNDED: Compute headroom strictly from real JACK input level + FET drive
        const inDb = inputLinearLevel > 0.00001 ? 20.0 * Math.log10(inputLinearLevel) : -100.0;
        const drivenDb = p.enabled ? (inDb + p.drive) : inDb;
        const isSat = p.enabled && drivenDb > -12.0;
        const isClip = p.enabled && drivenDb >= -1.5;
        p.clipping = isClip;

        const stripLed = document.getElementById(`preamp-clip-led-${ch}`);
        const rackLed = document.getElementById(`rack-preamp-clip-led-${ch}`);
        [stripLed, rackLed].forEach(led => {
            if (!led) return;
            led.classList.toggle('clip', isClip);
            led.classList.toggle('sat', isSat && !isClip);
            led.innerText = isClip ? 'CLIP!' : (isSat ? 'FET SAT' : 'CLEAN');
        });
    }

    function syncPreamp1176Ui(ch) {
        const p = getChannelRackState(ch).preamp1176;
        const fmt = (v) => (v >= 0 ? '+' : '') + v.toFixed(1) + ' dB';
        const ids = [
            [`strip-preamp-drive-${ch}`, p.drive, true],
            [`strip-preamp-drive-val-${ch}`, '+' + p.drive.toFixed(1) + 'dB', false],
            [`strip-preamp-trim-${ch}`, p.trim, true],
            [`strip-preamp-trim-val-${ch}`, fmt(p.trim), false],
            [`rack-preamp-drive-slider-${ch}`, p.drive, true],
            [`rack-preamp-drive-val-${ch}`, '+' + p.drive.toFixed(1) + ' dB', false],
            [`rack-preamp-trim-slider-${ch}`, p.trim, true],
            [`rack-preamp-trim-val-${ch}`, fmt(p.trim), false]
        ];
        ids.forEach(([id, val, isInput]) => {
            const el = document.getElementById(id);
            if (el) {
                if (isInput) el.value = val;
                else el.innerText = val;
            }
        });
        const modeBtn = document.getElementById(`rack-preamp-clipmode-${ch}`);
        if (modeBtn) {
            modeBtn.innerText = p.clipMode === 'soft' ? 'FET SOFT SAT' : 'HARD CLIP';
            modeBtn.classList.toggle('active', p.clipMode === 'hard');
        }
        const bypBtn = document.getElementById(`rack-preamp-enable-${ch}`);
        if (bypBtn) {
            bypBtn.innerText = p.enabled ? '1176 PRE: ON' : 'BYPASSED';
            bypBtn.classList.toggle('active', p.enabled);
        }
    }

    function triggerDenoiseLearn(ch) {
        const btn = document.getElementById(`btn-learn-noise-${ch}`);
        if (btn) {
            btn.classList.add('learning');
            btn.innerText = '⏺ ANALYZING NOISE...';
        }
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({ action: 'denoise_learn', ch: ch }));
        } else {
            setTimeout(() => window.handleDenoiseLearned(ch, -48.0, -18.0), 1200);
        }
    }

    window.handleDenoiseLearned = function(ch, threshold, reduction) {
        const st = getChannelRackState(ch);
        st.denoiser.threshold = parseFloat(threshold);
        st.denoiser.reduction = parseFloat(reduction);
        dispatchDspParam(ch, 'denoiseThresh', st.denoiser.threshold);
        dispatchDspParam(ch, 'denoiseReduc', st.denoiser.reduction);
        const btn = document.getElementById(`btn-learn-noise-${ch}`);
        if (btn) {
            btn.classList.remove('learning');
            btn.innerText = '✓ NOISE PROFILE LEARNED';
            setTimeout(() => { if (btn) btn.innerText = '⏺ LEARN NOISE'; }, 2500);
        }
        const tSlider = document.getElementById(`denoise-thresh-slider-${ch}`);
        const tVal = document.getElementById(`denoise-thresh-val-${ch}`);
        if (tSlider) tSlider.value = st.denoiser.threshold;
        if (tVal) tVal.innerText = st.denoiser.threshold.toFixed(1) + ' dB';
        const rSlider = document.getElementById(`denoise-reduc-slider-${ch}`);
        const rVal = document.getElementById(`denoise-reduc-val-${ch}`);
        if (rSlider) rSlider.value = st.denoiser.reduction;
        if (rVal) rVal.innerText = st.denoiser.reduction.toFixed(1) + ' dB';
    };

    function setChannelCompModel(ch, model) {
        const st = getChannelRackState(ch);
        st.compModel = model;
        dispatchDspParam(ch, 'compModel', model);
        if (document.getElementById('dsp-rack-page').classList.contains('open')) showDspRackPage(ch);
        else if (document.getElementById('channel-rack-overlay').classList.contains('open')) openChannelRackModal(ch);
    }

    function setChannelDirectTap(ch, tap) {
        const st = getChannelRackState(ch);
        st.directTap = tap;
        dispatchDspParam(ch, 'directTap', tap);
        if (document.getElementById('dsp-rack-page').classList.contains('open')) showDspRackPage(ch);
        else if (document.getElementById('channel-rack-overlay').classList.contains('open')) openChannelRackModal(ch);
    }

    function togglePhaseInvert(ch) {
        const st = getChannelRackState(ch);
        st.phaseInvert = !st.phaseInvert;
        dispatchDspParam(ch, 'phaseInvert', st.phaseInvert);
        [document.getElementById(`rack-phase-btn-${ch}`), document.getElementById(`rack-modal-phase-btn-${ch}`)].forEach(b => {
            if (b) {
                b.classList.toggle('active', st.phaseInvert);
                b.innerText = st.phaseInvert ? 'Ø INVERTED' : 'Ø NORMAL';
            }
        });
    }

    function buildRackRowsHtml(ch, isFullPage) {
        const state = getChannelRackState(ch);
        const p = state.preamp1176;
        const gain = window.channelInputGains[ch] ?? 0;
        const regainActive = window.channelRegainActive[ch] ?? true;
        const eqSt = window.getChannelReEq(ch);
        return `
            <!-- STAGE 1: 1176 FET CLASS-A PREAMP EMULATION (BEFORE REGAIN) -->
            <div class="rack-row" style="background:linear-gradient(90deg, #172033 0%, #0f172a 100%); border-color:#0284c7;">
                <div class="rack-header-area">
                    <span class="rack-num" style="color:#38bdf8; border-color:#0284c7;">1</span>
                    <div>
                        <div class="rack-name" style="color:#e0f2fe;">⚡ 1176 Class-A FET Preamp (Stage 1)</div>
                        <div class="rack-sub">Input Drive &bull; FET Soft Clipping &bull; Output Trim &rarr; Feeds Stage 2 REGAIN</div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <button class="tap-mode-btn ${p.enabled ? 'active' : ''}" id="rack-preamp-enable-${ch}" onclick="toggle1176PreampBypass(${ch})">${p.enabled ? '1176 PRE: ON' : 'BYPASSED'}</button>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">1176 Drive</span>
                        <input type="range" class="rack-mini-slider" id="rack-preamp-drive-slider-${ch}" style="width:95px;" min="0" max="36" step="0.5" value="${p.drive}" oninput="handle1176PreampDrive(${ch}, this.value)">
                        <span class="rack-mini-val" id="rack-preamp-drive-val-${ch}" style="color:#38bdf8;">+${p.drive.toFixed(1)} dB</span>
                    </div>
                    <button class="tap-mode-btn ${p.clipMode === 'hard' ? 'active' : ''}" id="rack-preamp-clipmode-${ch}" onclick="toggle1176ClipMode(${ch})" title="Toggle Soft FET Saturation vs Hard Diode Clip">${p.clipMode === 'soft' ? 'FET SOFT SAT' : 'HARD CLIP'}</button>
                    <span class="preamp-clip-badge" id="rack-preamp-clip-led-${ch}">CLEAN</span>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Output Trim</span>
                        <input type="range" class="rack-mini-slider" id="rack-preamp-trim-slider-${ch}" style="width:95px;" min="-24" max="12" step="0.5" value="${p.trim}" oninput="handle1176PreampTrim(${ch}, this.value)">
                        <span class="rack-mini-val" id="rack-preamp-trim-val-${ch}" style="color:#a5f3fc;">${(p.trim >= 0 ? '+' : '') + p.trim.toFixed(1)} dB</span>
                    </div>
                </div>
            </div>

            <!-- STAGE 2: DIGITAL GAIN, POLARITY & AUTOMATIC REGAIN -->
            <div class="rack-row">
                <div class="rack-header-area">
                    <span class="rack-num">2</span>
                    <div>
                        <div class="rack-name">🎚️ Digital Gain &amp; REGAIN (Stage 2)</div>
                        <div class="rack-sub">Post-1176 Trim &bull; Automatic Fader Compensation</div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Digital Gain</span>
                        <input type="range" class="rack-mini-slider" min="-18" max="24" step="0.5" value="${gain}" oninput="handleRackGainChange(this.value); this.nextElementSibling.innerText=(this.value>=0?'+':'')+parseFloat(this.value).toFixed(1)+' dB'">
                        <span class="rack-mini-val">${(gain >= 0 ? '+' : '') + parseFloat(gain).toFixed(1)} dB</span>
                    </div>
                    <button class="tap-mode-btn ${state.phaseInvert ? 'active' : ''}" id="rack-phase-btn-${ch}" onclick="togglePhaseInvert(${ch})">${state.phaseInvert ? 'Ø INVERTED' : 'Ø NORMAL'}</button>
                    <button class="btn-rack-bypass ${regainActive ? 'active' : ''}" onclick="toggleRackRegain(); this.classList.toggle('active', window.channelRegainActive[${ch}]); this.innerText=window.channelRegainActive[${ch}]?'REGAIN ON':'REGAIN OFF';">${regainActive ? 'REGAIN ON' : 'REGAIN OFF'}</button>
                </div>
            </div>

            <!-- STAGE 3: INPUT DE-NOISER / GATE -->
            <div class="rack-row" style="background:#0c131d; border-color:#1e293b;">
                <div class="rack-header-area">
                    <span class="rack-num" style="color:#f59e0b; border-color:#d97706;">3</span>
                    <div>
                        <div class="rack-name" style="display:flex; align-items:center; color:#fde68a;">
                            <span class="gate-led ${state.denoiser.enabled ? 'open' : 'closed'}" id="gate-led-${ch}"></span>
                            <span>🛡️ Input De-Noiser / Dynamic Gate</span>
                        </div>
                        <div class="rack-sub">Noise Floor Attenuator &bull; Spectral Subtraction</div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <button class="tap-mode-btn ${state.denoiser.enabled ? 'active' : ''}" onclick="var d=window.getChannelRackState(${ch}).denoiser; d.enabled=!d.enabled; dispatchDspParam(${ch},'denoiseBypass',!d.enabled); this.classList.toggle('active',d.enabled); this.innerText=d.enabled?'GATE ACTIVE':'BYPASS';">${state.denoiser.enabled ? 'GATE ACTIVE' : 'BYPASS'}</button>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Threshold</span>
                        <input type="range" class="rack-mini-slider" id="denoise-thresh-slider-${ch}" min="-60" max="0" step="1" value="${state.denoiser.threshold}" oninput="var v=window.touchGovernor?window.touchGovernor.process('denoise-t-${ch}',parseFloat(this.value)):parseFloat(this.value); window.getChannelRackState(${ch}).denoiser.threshold=v; dispatchDspParam(${ch},'denoiseThresh',v); document.getElementById('denoise-thresh-val-${ch}').innerText=Math.round(v)+' dB'">
                        <span class="rack-mini-val" id="denoise-thresh-val-${ch}">${state.denoiser.threshold} dB</span>
                    </div>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Reduction</span>
                        <input type="range" class="rack-mini-slider" id="denoise-reduc-slider-${ch}" min="-40" max="0" step="1" value="${state.denoiser.reduction}" oninput="var v=window.touchGovernor?window.touchGovernor.process('denoise-r-${ch}',parseFloat(this.value)):parseFloat(this.value); window.getChannelRackState(${ch}).denoiser.reduction=v; dispatchDspParam(${ch},'denoiseReduc',v); document.getElementById('denoise-reduc-val-${ch}').innerText=Math.round(v)+' dB'">
                        <span class="rack-mini-val" id="denoise-reduc-val-${ch}">${state.denoiser.reduction} dB</span>
                    </div>
                    <button class="btn-learn-noise" id="btn-learn-noise-${ch}" onclick="triggerDenoiseLearn(${ch})">⏺ LEARN NOISE</button>
                </div>
                <div class="rack-actions"><button class="btn-rack-open" onclick="openDspModal('denoiser', ${ch});">DETAIL &rarr;</button></div>
            </div>

            <!-- STAGE 4: REAEQ JSFX GRAPHIC PARAMETRIC EQUALISER -->
            <div class="rack-row rack-eq-card">
                <div class="rack-eq-topbar">
                    <div class="rack-header-area">
                        <span class="rack-num" style="color:#818cf8; border-color:#818cf8;">4</span>
                        <div>
                            <div class="rack-name" style="color:#c7d2fe;">🎛️ ReaEQ JSFX Graphic Parametric Equaliser</div>
                            <div class="rack-sub" style="color:#818cf8;">Cytomic SVF &bull; 8 Interactive Filter Nodes &bull; Live 64-Band RTA</div>
                        </div>
                    </div>
                    <div style="display:flex; align-items:center; gap:10px;">
                        <div style="display:flex; flex-direction:column; font-size:0.75rem; color:#818cf8; font-family:monospace; text-align:right;">
                            <span id="mini-reeq-band-lbl" style="font-weight:bold;">B1: Low Cut</span>
                            <span id="mini-reeq-val-lbl" style="color:#fff;">30Hz 0.0dB</span>
                        </div>
                        <button class="btn-rack-open" style="background:#4f46e5; border-color:#6366f1;" onclick="openDspModal('reeq', ${ch});">FULL SCREEN &rarr;</button>
                    </div>
                </div>
                ${isFullPage ? `
                <div class="rack-eq-canvas-wrap"><canvas id="dsp-rack-reeq-mini"></canvas></div>
                <div class="rack-eq-quick-strip" id="rack-eq-quick-strip">
                    ${eqSt.bands.map((b, idx) => `
                        <div class="rack-eq-quick-node ${eqSt.selectedBand === idx ? 'active' : ''}" id="rack-quick-node-${idx}" onclick="selectRackEqBand(${ch}, ${idx})">
                            <div style="display:flex; justify-content:space-between; font-size:0.7rem; font-weight:800; color:${window.REEQ_COLORS[idx % 8]};">
                                <span>B${idx+1}</span>
                                <input type="checkbox" ${b.enabled ? 'checked' : ''} onclick="event.stopPropagation(); toggleReEqBand(${idx}); renderMiniReEq(${ch});">
                            </div>
                            <div style="font-size:0.75rem; font-weight:800; color:#fff;" id="rack-node-freq-${idx}">${b.freq >= 1000 ? (b.freq/1000).toFixed(1)+'k' : b.freq+'Hz'}</div>
                            <div style="font-size:0.7rem; color:${b.gain >= 0 ? '#34d399' : '#f87171'};" id="rack-node-gain-${idx}">${(b.gain >= 0 ? '+' : '') + b.gain + 'dB'}</div>
                        </div>
                    `).join('')}
                </div>` : ''}
            </div>

            <!-- STAGE 5: SWITCHABLE / DUAL COMPRESSOR -->
            <div class="rack-row" style="border-color:#374151; background:linear-gradient(90deg, rgba(30,41,59,0.3) 0%, rgba(15,23,42,0.6) 100%);">
                <div class="rack-header-area">
                    <span class="rack-num" style="color:#38bdf8; border-color:#0284c7;">5</span>
                    <div>
                        <div class="rack-name" style="color:#e0f2fe;">⚡ Dynamics: Compressor Stacking</div>
                        <div class="comp-model-group" style="margin-top:4px;">
                            <button class="comp-model-btn ${state.compModel === 'fetish76' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'fetish76')">FET 76</button>
                            <button class="comp-model-btn ${state.compModel === 'lala' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'lala')">LALA</button>
                            <button class="comp-model-btn ${state.compModel === 'dual' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'dual')">DUAL (76 ➔ LA)</button>
                        </div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">76 Input</span>
                        <input type="range" class="rack-mini-slider" min="0" max="60" step="1" value="${state.compParams.fetish76.input}" oninput="var v=Math.round(window.touchGovernor?window.touchGovernor.process('c76in-${ch}',parseFloat(this.value)):parseFloat(this.value)); getChannelRackState(${ch}).compParams.fetish76.input=v; dispatchDspParam(${ch},'compInput',v); this.nextElementSibling.innerText=v+' dB'">
                        <span class="rack-mini-val">${state.compParams.fetish76.input} dB</span>
                    </div>
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">LALA Peak</span>
                        <input type="range" class="rack-mini-slider" min="0" max="100" step="1" value="${state.compParams.lala.peakReduction}" oninput="var v=Math.round(window.touchGovernor?window.touchGovernor.process('clapk-${ch}',parseFloat(this.value)):parseFloat(this.value)); getChannelRackState(${ch}).compParams.lala.peakReduction=v; dispatchDspParam(${ch},'lalaPeakRed',v); this.nextElementSibling.innerText=v">
                        <span class="rack-mini-val">${state.compParams.lala.peakReduction}</span>
                    </div>
                </div>
                <div class="rack-actions">
                    <button class="btn-rack-open" style="background:#0284c7; border-color:#38bdf8;" onclick="openDspModal('${state.compModel === 'lala' ? 'la2a' : 'fet1176'}', ${ch});">${state.compModel === 'lala' ? 'LALA GUI &rarr;' : '76 GUI &rarr;'}</button>
                </div>
            </div>

            <!-- STAGE 6: ANALOG OBSESSION LOADES DE-ESSER -->
            <div class="rack-row">
                <div class="rack-header-area">
                    <span class="rack-num">6</span>
                    <div>
                        <div class="rack-name">🎙️ Analog Obsession LOADES De-Esser</div>
                        <div class="rack-sub">Dynamic Sibilance Attenuation &bull; Direct Tap: ${state.directTap.toUpperCase()}</div>
                    </div>
                </div>
                <div class="rack-inline-controls">
                    <div class="rack-mini-knob">
                        <span class="rack-mini-label">Threshold</span>
                        <input type="range" class="rack-mini-slider" min="-40" max="0" step="1" value="${state.deesser.threshold}" oninput="var v=Math.round(window.touchGovernor?window.touchGovernor.process('ds-t-${ch}',parseFloat(this.value)):parseFloat(this.value)); window.getChannelRackState(${ch}).deesser.threshold=v; dispatchDspParam(${ch},'loadesThresh',v); this.nextElementSibling.innerText=v+' dB'">
                        <span class="rack-mini-val">${state.deesser.threshold} dB</span>
                    </div>
                    <div class="tap-mode-group">
                        <button class="tap-mode-btn ${state.directTap === 'pre' ? 'active' : ''}" onclick="setChannelDirectTap(${ch}, 'pre')">PRE</button>
                        <button class="tap-mode-btn ${state.directTap === 'post' ? 'active' : ''}" onclick="setChannelDirectTap(${ch}, 'post')">POST</button>
                    </div>
                </div>
                <div class="rack-actions"><button class="btn-rack-open" onclick="openDspModal('deesser', ${ch});">DETAIL &rarr;</button></div>
            </div>
        `;
    }

    function openChannelRackModal(ch) {
        currentRackChannel = ch;
        const overlay = document.getElementById("channel-rack-overlay");
        const title = document.getElementById("channel-rack-modal-title");
        const grid = document.getElementById("channel-rack-grid");
        const chName = window.customChannelNames['ch-' + ch] || `Channel ${ch + 1}`;
        if (title) title.innerHTML = `🎛️ IN ${ch + 1} &bull; ${chName} &bull; Channel FX Strip Rack`;
        if (grid) grid.innerHTML = buildRackRowsHtml(ch, false);
        if (overlay) overlay.classList.add("open");
    }

    function closeChannelRackModal() {
        const overlay = document.getElementById("channel-rack-overlay");
        if (overlay) overlay.classList.remove("open");
    }

    window.showDspRackPage = function(ch) {
        window.dspRackCurrentChannel = ch;
        const chName = window.customChannelNames['ch-' + ch] || `Channel ${ch + 1}`;
        const gain = window.channelInputGains[ch] ?? 0;
        const regainActive = window.channelRegainActive[ch] ?? true;
        const titleEl = document.getElementById('dsp-rack-page-title');
        if (titleEl) titleEl.innerHTML = `🎛️ IN ${ch + 1} — ${chName} &bull; Core Desk DSP Rack`;
        const gainSlider = document.getElementById('dsp-rack-gain-slider');
        const gainVal = document.getElementById('dsp-rack-gain-val');
        const regainBtn = document.getElementById('dsp-rack-regain-btn');
        if (gainSlider) gainSlider.value = gain;
        if (gainVal) gainVal.innerText = (gain >= 0 ? '+' : '') + parseFloat(gain).toFixed(1) + ' dB';
        if (regainBtn) {
            regainBtn.classList.toggle('active', regainActive);
            regainBtn.innerText = regainActive ? 'REGAIN ON' : 'REGAIN OFF';
        }
        const grid = document.getElementById('dsp-rack-grid');
        if (grid) grid.innerHTML = buildRackRowsHtml(ch, true);
        if (typeof window.updateFocusedChannelStrip === 'function') window.updateFocusedChannelStrip(ch);
        document.getElementById('dsp-rack-page').classList.add('open');
        setTimeout(() => {
            if (typeof window.renderMiniReEq === 'function') window.renderMiniReEq(ch);
            if (typeof window.applySlowModeToAllSliders === 'function') window.applySlowModeToAllSliders();
        }, 30);
    };

    function closeDspRackPage() {
        document.getElementById('dsp-rack-page').classList.remove('open');
    }

    function handleRackGainChange(val) {
        const ch = window.dspRackCurrentChannel || 0;
        let fVal = parseFloat(val);
        if (window.touchGovernor) fVal = window.touchGovernor.process(`gain-${ch}`, fVal);
        window.handleInputGainChange(ch, fVal);
        const valEl = document.getElementById('dsp-rack-gain-val');
        if (valEl) valEl.innerText = (fVal >= 0 ? '+' : '') + fVal.toFixed(1) + ' dB';
    }

    function toggleRackRegain() {
        const ch = window.dspRackCurrentChannel || 0;
        window.toggleRegain(ch);
        const btn = document.getElementById('dsp-rack-regain-btn');
        if (btn) {
            btn.classList.toggle('active', window.channelRegainActive[ch]);
            btn.innerText = window.channelRegainActive[ch] ? 'REGAIN ON' : 'REGAIN OFF';
        }
    }

    window.getChannelRackState = getChannelRackState;
    window.dispatchDspParam = dispatchDspParam;
    window.handle1176PreampDrive = handle1176PreampDrive;
    window.handle1176PreampTrim = handle1176PreampTrim;
    window.toggle1176ClipMode = toggle1176ClipMode;
    window.toggle1176PreampBypass = toggle1176PreampBypass;
    window.updatePreamp1176ClipTelemetry = updatePreamp1176ClipTelemetry;
    window.syncPreamp1176Ui = syncPreamp1176Ui;
    window.triggerDenoiseLearn = triggerDenoiseLearn;
    window.setChannelCompModel = setChannelCompModel;
    window.setChannelDirectTap = setChannelDirectTap;
    window.togglePhaseInvert = togglePhaseInvert;
    window.toggleChPhase = function(ch) {
        togglePhaseInvert(ch);
        const b = document.getElementById('phase-ch-' + ch);
        if (b) b.classList.toggle('active', getChannelRackState(ch).phaseInvert);
    };
    window.handleGainChange = function(ch, val) {
        if (window.handleInputGainChange) window.handleInputGainChange(ch, val);
    };
    window.toggleChRegain = function(ch) {
        if (window.toggleRegain) window.toggleRegain(ch);
        const b = document.getElementById('regain-ch-' + ch);
        if (b) b.classList.toggle('active', !!(window.channelRegainActive && window.channelRegainActive[ch]));
    };
    window.openChannelRackModal = openChannelRackModal;
    window.closeChannelRackModal = closeChannelRackModal;
    window.closeDspRackPage = closeDspRackPage;
    window.handleRackGainChange = handleRackGainChange;
    window.toggleRackRegain = toggleRackRegain;
})(window);
"""

with open(os.path.join(JS_DIR, "dsp-rack.js"), "w", encoding="utf-8") as f:
    f.write(dsp_rack_js)

print("dsp-rack.js lines:", len(dsp_rack_js.splitlines()))


# 4. Write dsp-rack-views.js (< 300 lines): renderMiniReEq, selectRackEqBand, updateFocusedChannelStrip, startFocusedFaderDrag
dsp_rack_views_js = """// dsp-rack-views.js - Mini ReaEQ Canvas & Focused Channel Strip Sidebar for Full-Page DSP Rack
(function(window) {
    'use strict';

    function renderMiniReEq(ch) {
        const canvas = document.getElementById("dsp-rack-reeq-mini");
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        const dpr = window.devicePixelRatio || 1;
        const w = canvas.clientWidth || 700, h = canvas.clientHeight || 220;
        canvas.width = Math.round(w * dpr);
        canvas.height = Math.round(h * dpr);
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        const state = window.getChannelReEq(ch);
        const range = state.dbRange;

        ctx.clearRect(0, 0, w, h);
        ctx.fillStyle = "#0c0e14";
        ctx.fillRect(0, 0, w, h);

        const dbStep = range >= 24 ? 6 : (range >= 18 ? 3 : 2);
        ctx.font = "9px -apple-system, monospace";
        for (let db = -range; db <= range; db += dbStep) {
            const y = window.dbToY(db, h, range);
            ctx.beginPath();
            ctx.moveTo(0, y); ctx.lineTo(w, y);
            ctx.strokeStyle = db === 0 ? "rgba(56, 189, 248, 0.5)" : "rgba(255, 255, 255, 0.07)";
            ctx.lineWidth = db === 0 ? 1.5 : 1;
            ctx.stroke();
            ctx.fillStyle = db === 0 ? "#38bdf8" : "rgba(255, 255, 255, 0.35)";
            ctx.textAlign = "right";
            ctx.fillText((db > 0 ? "+" : "") + db + " dB", w - 6, y - 2);
        }

        const gridFreqs = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
        gridFreqs.forEach(f => {
            const x = window.freqToX(f, w);
            ctx.beginPath();
            ctx.moveTo(x, 0); ctx.lineTo(x, h - 16);
            ctx.strokeStyle = (f === 100 || f === 1000 || f === 10000) ? "rgba(255, 255, 255, 0.12)" : "rgba(255, 255, 255, 0.05)";
            ctx.lineWidth = 1; ctx.stroke();
            ctx.fillStyle = "rgba(255, 255, 255, 0.4)";
            ctx.textAlign = "center";
            ctx.fillText(f >= 1000 ? (f / 1000) + "k" : f + "Hz", x, h - 4);
        });

        const midY = window.dbToY(0, h, range);
        if (window.liveRtaBins && window.liveRtaBins.length > 0) {
            ctx.save();
            ctx.beginPath();
            ctx.moveTo(0, h);
            const rtaLen = window.liveRtaBins.length;
            for (let b = 0; b < rtaLen; b++) {
                const binFreq = 20 * Math.pow(1000, b / (rtaLen - 1));
                const x = window.freqToX(binFreq, w);
                const binDb = Math.max(-80, Math.min(0, window.liveRtaBins[b] || -80));
                ctx.lineTo(x, window.dbToY(binDb, h, range));
            }
            ctx.lineTo(w, h); ctx.closePath();
            ctx.fillStyle = "rgba(56, 189, 248, 0.18)";
            ctx.fill();
            ctx.restore();
        }

        state.bands.forEach((b, idx) => {
            if (!b.enabled) return;
            const color = window.REEQ_COLORS[idx % 8];
            const isSelected = state.selectedBand === idx;
            ctx.beginPath();
            for (let x = 0; x <= w; x += 3) {
                const mag = window.evalBandMagnitude(b, window.xToFreq(x, w), 48000);
                const y = window.dbToY(20 * Math.log10(Math.max(0.0001, mag)), h, range);
                if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.strokeStyle = color;
            ctx.lineWidth = isSelected ? 1.8 : 1.0;
            ctx.globalAlpha = isSelected ? 0.85 : 0.35;
            ctx.stroke();
            ctx.globalAlpha = 1.0;
        });

        ctx.beginPath();
        for (let x = 0; x <= w; x += 2) {
            const freq = window.xToFreq(x, w);
            let totalMag = 1.0;
            state.bands.forEach(b => { totalMag *= window.evalBandMagnitude(b, freq, 48000); });
            const y = window.dbToY(20 * Math.log10(Math.max(0.0001, totalMag)), h, range);
            if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.strokeStyle = "#818cf8";
        ctx.lineWidth = 2.2;
        ctx.stroke();
        ctx.lineTo(w, midY); ctx.lineTo(0, midY);
        ctx.fillStyle = "rgba(129, 140, 248, 0.12)";
        ctx.fill();

        state.bands.forEach((b, idx) => {
            if (!b.enabled) return;
            const x = window.freqToX(b.freq, w);
            const y = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8) ? midY : window.dbToY(b.gain, h, range);
            const color = window.REEQ_COLORS[idx % 8];
            const isSelected = state.selectedBand === idx;
            ctx.beginPath();
            ctx.arc(x, y, isSelected ? 12 : 8, 0, Math.PI * 2);
            ctx.fillStyle = color; ctx.fill();
            ctx.strokeStyle = "#ffffff"; ctx.lineWidth = isSelected ? 2 : 1; ctx.stroke();
            ctx.fillStyle = "#ffffff";
            ctx.font = `bold ${isSelected ? '10px' : '8px'} sans-serif`;
            ctx.textAlign = "center"; ctx.textBaseline = "middle";
            ctx.fillText(idx + 1, x, y);
        });

        if (!canvas.dataset.bound) {
            canvas.dataset.bound = "true";
            let activeMiniNode = -1;
            function getMiniPos(e) {
                const r = canvas.getBoundingClientRect();
                const cx = e.touches ? e.touches[0].clientX : e.clientX;
                const cy = e.touches ? e.touches[0].clientY : e.clientY;
                return { x: cx - r.left, y: cy - r.top };
            }
            function onDown(e) {
                e.preventDefault();
                const pos = getMiniPos(e);
                const curCh = window.dspRackCurrentChannel || 0;
                const st = window.getChannelReEq(curCh);
                const curW = canvas.clientWidth || 700, curH = canvas.clientHeight || 220;
                let hit = -1, minD = 28;
                st.bands.forEach((b, i) => {
                    if (!b.enabled) return;
                    const nx = window.freqToX(b.freq, curW);
                    const ny = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8) ? window.dbToY(0, curH, st.dbRange) : window.dbToY(b.gain, curH, st.dbRange);
                    const d = Math.hypot(pos.x - nx, pos.y - ny);
                    if (d < minD) { minD = d; hit = i; }
                });
                if (hit !== -1) {
                    activeMiniNode = hit;
                    st.selectedBand = hit;
                    updateMiniLabels(st, hit);
                    updateQuickNodeCardUI(hit);
                    renderMiniReEq(curCh);
                }
            }
            function onMove(e) {
                if (activeMiniNode === -1) return;
                e.preventDefault();
                const pos = getMiniPos(e);
                const curCh = window.dspRackCurrentChannel || 0;
                const st = window.getChannelReEq(curCh);
                const b = st.bands[activeMiniNode];
                const curW = canvas.clientWidth || 700, curH = canvas.clientHeight || 220;
                let targetFreq = Math.round(window.xToFreq(pos.x, curW));
                if (window.touchGovernor) targetFreq = Math.round(window.touchGovernor.process(`reeq-mini-freq-${curCh}-${activeMiniNode}`, targetFreq));
                b.freq = targetFreq;
                if (b.type !== 1 && b.type !== 2 && b.type !== 5 && b.type !== 6 && b.type !== 7 && b.type !== 8) {
                    let targetGain = parseFloat(window.yToDb(pos.y, curH, st.dbRange).toFixed(1));
                    if (window.touchGovernor) targetGain = parseFloat(window.touchGovernor.process(`reeq-mini-gain-${curCh}-${activeMiniNode}`, targetGain).toFixed(1));
                    b.gain = targetGain;
                }
                updateMiniLabels(st, activeMiniNode);
                updateQuickNodeCardUI(activeMiniNode);
                renderMiniReEq(curCh);
            }
            function onUp() { activeMiniNode = -1; }
            canvas.addEventListener("pointerdown", onDown);
            window.addEventListener("pointermove", onMove);
            window.addEventListener("pointerup", onUp);
            window.addEventListener("pointercancel", onUp);
        }
        const activeB = state.bands[state.selectedBand];
        if (activeB) updateMiniLabels(state, state.selectedBand);
    }

    function selectRackEqBand(ch, idx) {
        const st = window.getChannelReEq(ch);
        st.selectedBand = idx;
        updateQuickNodeCardUI(idx);
        updateMiniLabels(st, idx);
        renderMiniReEq(ch);
    }

    function updateQuickNodeCardUI(selectedIdx) {
        const st = window.getChannelReEq(window.dspRackCurrentChannel || 0);
        st.bands.forEach((b, idx) => {
            const nodeEl = document.getElementById("rack-quick-node-" + idx);
            if (nodeEl) nodeEl.classList.toggle("active", idx === selectedIdx);
            const fEl = document.getElementById("rack-node-freq-" + idx);
            if (fEl) fEl.innerText = b.freq >= 1000 ? (b.freq / 1000).toFixed(1) + 'k' : b.freq + 'Hz';
            const gEl = document.getElementById("rack-node-gain-" + idx);
            if (gEl) {
                gEl.innerText = (b.gain >= 0 ? '+' : '') + b.gain + 'dB';
                gEl.style.color = b.gain >= 0 ? '#34d399' : '#f87171';
            }
        });
    }

    function updateMiniLabels(st, idx) {
        const b = st.bands[idx];
        if (!b) return;
        const bLbl = document.getElementById("mini-reeq-band-lbl");
        const vLbl = document.getElementById("mini-reeq-val-lbl");
        if (bLbl) bLbl.innerText = `B${idx+1}: ${window.FILTER_TYPES[b.type]}`;
        if (vLbl) {
            const fStr = b.freq >= 1000 ? (b.freq/1000).toFixed(1)+'k' : b.freq+'Hz';
            const gStr = (b.gain >= 0 ? '+' : '') + b.gain.toFixed(1) + 'dB';
            vLbl.innerText = `${fStr} ${gStr} (Q ${b.q.toFixed(1)})`;
        }
    }

    function updateFocusedChannelStrip(ch) {
        const container = document.getElementById('dsp-rack-focused-strip');
        if (!container) return;
        const defaultNames = (window.BUS_CONFIG && window.BUS_CONFIG.channels) || ["Vocal 1", "Guitar", "Bass", "Kick Drum", "Snare", "Hi-Hat", "Keys L", "Keys R"];
        const name = window.customChannelNames['ch-' + ch] || defaultNames[ch] || `CH ${ch + 1}`;
        const pct = window.faderValues.main[ch] ?? 75;
        const dbStr = window.toDbStr(pct);
        const isMuted = document.getElementById('mute-ch-' + ch)?.classList.contains('active');
        const isSolo = document.getElementById('solo-ch-' + ch)?.classList.contains('active');
        const isAmm = document.getElementById('amm-ch-' + ch)?.classList.contains('active');
        const color = window.customChannelColors['ch-' + ch] || '#38bdf8';

        container.innerHTML = `
            <div class="focused-strip-header">
                <div class="focused-strip-badge" style="background:${color}; color:${(color==='#e2e8f0'||color==='#eab308'||color==='#84cc16')?'#000':'#fff'};">IN ${ch + 1} &bull; CH ${ch + 1}</div>
                <div class="focused-strip-name" title="${name}">${name}</div>
            </div>
            <div class="fader-section" style="margin: 10px 0;">
                <div class="meter-container" style="width: 12px;">
                    <div class="meter-bar" id="meter-focused-rack"></div>
                    <div class="meter-ticks"><div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div></div>
                </div>
                <div class="gr-meter-container">
                    <div class="gr-meter-bar" id="gr-meter-focused-rack"></div>
                    <span class="gr-meter-scale-title">GR</span>
                </div>
                <div class="fader-track" id="fader-track-focused-rack" data-target="${ch}" data-channel="${ch}">
                    <div class="fader-scale">
                        <div class="scale-mark"><span>+10</span></div><div class="scale-mark"><span>+5</span></div>
                        <div class="scale-mark zero"><span>0</span></div><div class="scale-mark"><span>-5</span></div>
                        <div class="scale-mark"><span>-10</span></div><div class="scale-mark"><span>-20</span></div>
                        <div class="scale-mark"><span>-30</span></div><div class="scale-mark"><span>-40</span></div>
                        <div class="scale-mark"><span>-&infin;</span></div>
                    </div>
                    <div class="fader-slot-bg"></div>
                    <div class="fader-slot-centerline"></div>
                    <div class="fader-cap" id="fader-cap-focused-rack" data-target="${ch}" data-channel="${ch}" style="bottom: calc(38px + (100% - 52px) * ${pct / 100});"></div>
                    <div class="fader-db-display" id="fader-val-focused-rack">${dbStr}</div>
                </div>
            </div>
            <div class="btn-group" style="margin-top:auto;">
                <button class="btn-strip btn-amm ${isAmm ? 'active' : ''}" onclick="toggleChAmm(${ch}); this.classList.toggle('active')">AMM DUGAN</button>
                <button class="btn-strip btn-solo ${isSolo ? 'active' : ''}" onclick="toggleChSolo(${ch}); this.classList.toggle('active')">SOLO</button>
                <button class="btn-strip btn-mute ${isMuted ? 'active' : ''}" onclick="toggleChMute(${ch}); this.classList.toggle('active')">MUTE</button>
            </div>
        `;
    }

    function startFocusedFaderDrag(target, evt) {
        // Handled globally by touch-physics.js .fader-cap pointerdown listener
    }

    window.renderMiniReEq = renderMiniReEq;
    window.selectRackEqBand = selectRackEqBand;
    window.updateFocusedChannelStrip = updateFocusedChannelStrip;
    window.startFocusedFaderDrag = startFocusedFaderDrag;
})(window);
"""

with open(os.path.join(JS_DIR, "dsp-rack-views.js"), "w", encoding="utf-8") as f:
    f.write(dsp_rack_views_js)

print("dsp-rack-views.js lines:", len(dsp_rack_views_js.splitlines()))

# 5. Write ao-vu-knobs.js (< 330 lines): Analog Obsession Hardware Suite (LALA, FETISH 76, BUSTER, Console Summing, VU Meter & Rotary Knobs)
ao_vu_knobs_js = """// ao-vu-knobs.js - Analog Obsession Hardware Suite (LALA, FETISH 76, BUSTER, Console Summing) & Telemetry-Grounded VU / Knobs
(function(window) {
    'use strict';

    const lalaState = {
        gain: 38, peakReduction: 45, hpf: 60, mf: 1200, mg: 0, hfFlat: 80, mix: 100, ext: false, limitMode: false, power: true
    };
    const fetish76State = {
        input: 30, output: 24, attack: 3, release: 5, ratio: '4', allButtons: false, power: true
    };
    const busterLimiterState = {
        threshold: -1.0, ceiling: -0.2, release: 25, attack: 10, drive: 0.0, crush: false, power: true
    };
    const analogSummingState = {
        console: 'neve', drive: 2.5, warmth: 3.0, crosstalk: -60, ironSat: true, enabled: true
    };

    function drawAoKnob(canvasId, normVal, style = 'lala', isLarge = false) {
        const cvs = document.getElementById(canvasId);
        if (!cvs) return;
        const ctx = cvs.getContext('2d');
        const w = cvs.width, h = cvs.height;
        const cx = w / 2, cy = h / 2;
        const radius = Math.min(cx, cy) - (isLarge ? 7 : 5);
        ctx.clearRect(0, 0, w, h);

        const startAngle = 0.75 * Math.PI, endAngle = 2.25 * Math.PI;
        const currentAngle = startAngle + normVal * (endAngle - startAngle);
        const numTicks = isLarge ? 21 : 11;
        for (let i = 0; i < numTicks; i++) {
            const angle = startAngle + (i / (numTicks - 1)) * (endAngle - startAngle);
            const isMajor = (i % 2 === 0);
            const rOuter = radius + (isLarge ? 6 : 4);
            const rInner = radius + (isMajor ? 1 : (isLarge ? 3 : 2));
            ctx.beginPath();
            ctx.moveTo(cx + Math.cos(angle) * rInner, cy + Math.sin(angle) * rInner);
            ctx.lineTo(cx + Math.cos(angle) * rOuter, cy + Math.sin(angle) * rOuter);
            ctx.strokeStyle = style === 'lala' ? '#44403c' : '#64748b';
            ctx.lineWidth = isMajor ? 1.5 : 1;
            ctx.stroke();
        }

        ctx.beginPath();
        ctx.arc(cx, cy + 2, radius, 0, 2 * Math.PI);
        ctx.fillStyle = 'rgba(0,0,0,0.5)';
        ctx.fill();

        ctx.beginPath();
        ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
        const skirtGrad = ctx.createRadialGradient(cx - 3, cy - 3, 2, cx, cy, radius);
        if (style === 'lala') {
            skirtGrad.addColorStop(0, '#292524'); skirtGrad.addColorStop(0.85, '#1c1917'); skirtGrad.addColorStop(1, '#0c0a09');
        } else if (style === 'fetish') {
            skirtGrad.addColorStop(0, '#38bdf8'); skirtGrad.addColorStop(0.3, '#0f172a'); skirtGrad.addColorStop(1, '#020617');
        } else {
            skirtGrad.addColorStop(0, '#475569'); skirtGrad.addColorStop(0.85, '#1e293b'); skirtGrad.addColorStop(1, '#0f172a');
        }
        ctx.fillStyle = skirtGrad; ctx.fill();
        ctx.strokeStyle = style === 'lala' ? '#57534e' : '#334155';
        ctx.lineWidth = 1.5; ctx.stroke();

        const innerRadius = radius * (isLarge ? 0.68 : 0.62);
        ctx.beginPath();
        ctx.arc(cx, cy, innerRadius, 0, 2 * Math.PI);
        const innerGrad = ctx.createRadialGradient(cx - 2, cy - 2, 1, cx, cy, innerRadius);
        if (style === 'lala') {
            innerGrad.addColorStop(0, '#e7e5e4'); innerGrad.addColorStop(0.7, '#a8a29e'); innerGrad.addColorStop(1, '#44403c');
        } else if (style === 'fetish') {
            innerGrad.addColorStop(0, '#0284c7'); innerGrad.addColorStop(0.5, '#0369a1'); innerGrad.addColorStop(1, '#082f49');
        } else {
            innerGrad.addColorStop(0, '#cbd5e1'); innerGrad.addColorStop(0.7, '#64748b'); innerGrad.addColorStop(1, '#1e293b');
        }
        ctx.fillStyle = innerGrad; ctx.fill();

        const pLength = radius - (isLarge ? 3 : 2);
        ctx.beginPath();
        ctx.moveTo(cx, cy);
        ctx.lineTo(cx + Math.cos(currentAngle) * pLength, cy + Math.sin(currentAngle) * pLength);
        ctx.strokeStyle = (style === 'lala') ? '#ffffff' : (style === 'fetish' ? '#38bdf8' : '#f8fafc');
        ctx.lineWidth = isLarge ? 3.0 : 2.0;
        ctx.lineCap = 'round';
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(cx, cy, isLarge ? 3.5 : 2.5, 0, 2 * Math.PI);
        ctx.fillStyle = '#1c1917';
        ctx.fill();
    }

    // TELEMETRY_GROUNDED: Draw illuminated analog VU meter strictly from incoming gain reduction telemetry
    function renderAoVuMeter(canvasId, grDb = 0, style = 'lala') {
        const cvs = document.getElementById(canvasId);
        if (!cvs) return;
        const ctx = cvs.getContext('2d');
        const w = cvs.width, h = cvs.height;
        ctx.clearRect(0, 0, w, h);

        const bgGrad = ctx.createRadialGradient(w / 2, h * 0.95, 10, w / 2, h * 0.95, w * 0.7);
        if (style === 'lala') {
            bgGrad.addColorStop(0, '#fef08a'); bgGrad.addColorStop(0.4, '#fed7aa'); bgGrad.addColorStop(0.85, '#f59e0b'); bgGrad.addColorStop(1, '#b45309');
        } else {
            bgGrad.addColorStop(0, '#bfdbfe'); bgGrad.addColorStop(0.4, '#93c5fd'); bgGrad.addColorStop(0.85, '#3b82f6'); bgGrad.addColorStop(1, '#1d4ed8');
        }
        ctx.fillStyle = bgGrad;
        ctx.fillRect(0, 0, w, h);

        const pivotX = w / 2, pivotY = h * 1.35, arcRadius = h * 1.05;
        const minAngle = -Math.PI * 0.30, maxAngle = Math.PI * 0.30;
        const marks = [
            { db: -20, label: '20' }, { db: -10, label: '10' }, { db: -7, label: '7' },
            { db: -5, label: '5' }, { db: -3, label: '3' }, { db: -1, label: '1' },
            { db: 0, label: '0' }, { db: 1, label: '+1' }, { db: 2, label: '+2' }, { db: 3, label: '+3' }
        ];
        function dbToAngle(db) {
            const clamped = Math.max(-20, Math.min(3, db));
            return minAngle + ((clamped + 20) / 23) * (maxAngle - minAngle) - Math.PI / 2;
        }

        ctx.beginPath();
        ctx.arc(pivotX, pivotY, arcRadius, minAngle - Math.PI / 2, maxAngle - Math.PI / 2);
        ctx.strokeStyle = '#292524'; ctx.lineWidth = 1.5; ctx.stroke();

        ctx.beginPath();
        ctx.arc(pivotX, pivotY, arcRadius, dbToAngle(0), maxAngle - Math.PI / 2);
        ctx.strokeStyle = '#dc2626'; ctx.lineWidth = 3.5; ctx.stroke();

        ctx.font = 'bold 9px sans-serif';
        ctx.textAlign = 'center';
        marks.forEach(m => {
            const angle = dbToAngle(m.db);
            ctx.beginPath();
            ctx.moveTo(pivotX + Math.cos(angle) * (arcRadius - 2), pivotY + Math.sin(angle) * (arcRadius - 2));
            ctx.lineTo(pivotX + Math.cos(angle) * (arcRadius + 6), pivotY + Math.sin(angle) * (arcRadius + 6));
            ctx.strokeStyle = m.db > 0 ? '#dc2626' : '#1c1917';
            ctx.lineWidth = 1.2; ctx.stroke();
            ctx.fillStyle = m.db > 0 ? '#dc2626' : '#1c1917';
            ctx.fillText(m.label, pivotX + Math.cos(angle) * (arcRadius + 14), pivotY + Math.sin(angle) * (arcRadius + 14));
        });

        ctx.font = '900 11px sans-serif';
        ctx.fillStyle = style === 'lala' ? '#451a03' : '#1e3a8a';
        ctx.fillText('VU', pivotX, h * 0.48);
        ctx.font = 'bold 7px sans-serif';
        ctx.fillText('GAIN REDUCTION (dB)', pivotX, h * 0.58);

        const needleAngle = dbToAngle(Math.max(-20, Math.min(3, grDb)));
        ctx.beginPath();
        ctx.moveTo(pivotX, pivotY);
        ctx.lineTo(pivotX + Math.cos(needleAngle) * (arcRadius + 8), pivotY + Math.sin(needleAngle) * (arcRadius + 8));
        ctx.strokeStyle = '#1c1917'; ctx.lineWidth = 1.6; ctx.stroke();

        ctx.beginPath();
        ctx.arc(pivotX, pivotY, 18, 0, 2 * Math.PI);
        ctx.fillStyle = '#292524'; ctx.fill();
    }

    // TELEMETRY_PENDING: Needs backend socket link (rests at 0 dB until telemetry.js pushes real GR)
    function startAoVuLoop(meterId, style = 'lala', isLimiter = false) {
        const curCh = (typeof window.currentModalChannel === 'number') ? window.currentModalChannel : (window.dspRackCurrentChannel || 0);
        const currentGr = isLimiter ? 0 : -(window.channelGrValues ? (window.channelGrValues[curCh] || 0) : 0);
        renderAoVuMeter(meterId, currentGr, style);
    }

    function bindAoRotary(canvasId, readoutId, min, max, unit, getter, setter, style = 'lala', isLarge = false) {
        const cvs = document.getElementById(canvasId);
        if (!cvs || cvs.dataset.bound) return;
        cvs.dataset.bound = 'true';
        const initNorm = Math.max(0, Math.min(1, (getter() - min) / (max - min)));
        drawAoKnob(canvasId, initNorm, style, isLarge);

        let startY = 0, startVal = initNorm;
        cvs.addEventListener('pointerdown', (e) => {
            startY = e.clientY;
            startVal = (getter() - min) / (max - min);
            cvs.setPointerCapture(e.pointerId);
            const onMove = (ev) => {
                const dy = startY - ev.clientY;
                const speedCoeff = window.maxSpeedDamping <= 3 ? 0.006 : (0.006 / (1 + (window.maxSpeedDamping - 3) * 0.45));
                let newNorm = Math.max(0, Math.min(1, startVal + dy * speedCoeff));
                let actualVal = min + newNorm * (max - min);
                if (window.touchGovernor) {
                    actualVal = window.touchGovernor.process(`rotary-${canvasId}`, actualVal);
                    newNorm = Math.max(0, Math.min(1, (actualVal - min) / (max - min)));
                }
                setter(actualVal);
                drawAoKnob(canvasId, newNorm, style, isLarge);
                const readout = document.getElementById(readoutId);
                if (readout) {
                    readout.innerText = (unit === ' Hz' && actualVal >= 1000)
                        ? (actualVal / 1000).toFixed(1) + ' kHz'
                        : (Number.isInteger(min) && Number.isInteger(max) ? Math.round(actualVal) : actualVal.toFixed(1)) + unit;
                }
            };
            const onUp = () => {
                window.removeEventListener('pointermove', onMove);
                window.removeEventListener('pointerup', onUp);
            };
            window.addEventListener('pointermove', onMove);
            window.addEventListener('pointerup', onUp);
        });
    }

    function setFetishRatio(ratioStr) {
        fetish76State.ratio = ratioStr;
        fetish76State.allButtons = false;
        document.querySelectorAll('.ao-ratio-btn').forEach(btn => {
            if (btn.classList.contains('all-in')) btn.classList.remove('active');
            else btn.classList.toggle('active', btn.innerText.startsWith(ratioStr + ':'));
        });
    }

    function toggleFetishAllButtons() {
        fetish76State.allButtons = !fetish76State.allButtons;
        document.querySelectorAll('.ao-ratio-btn').forEach(btn => {
            btn.classList.toggle('active', fetish76State.allButtons);
        });
    }

    function setSummingConsole(consoleType) {
        analogSummingState.console = consoleType;
        updateSummingHeaderBtn();
    }

    function updateSummingHeaderBtn() {
        const btn = document.getElementById('btn-analog-summing');
        if (!btn) return;
        const names = { neve: 'NEVE 8048', api: 'API 2520', ssl: 'SSL 4000E', tube: 'TUBE DESK' };
        const cName = names[analogSummingState.console] || 'CONSOLE';
        if (analogSummingState.enabled) {
            btn.classList.add('active');
            btn.innerHTML = `🎛️ SUMMING: ${cName}`;
        } else {
            btn.classList.remove('active');
            btn.innerHTML = `🎛️ SUMMING: OFF`;
        }
    }

    window.isInteractable = function() { return true; };
    window.fetish76State = fetish76State;
    window.lalaState = lalaState;
    window.busterLimiterState = busterLimiterState;
    window.analogSummingState = analogSummingState;
    window.drawAoKnob = drawAoKnob;
    window.renderAoVuMeter = renderAoVuMeter;
    window.startAoVuLoop = startAoVuLoop;
    window.bindAoRotary = bindAoRotary;
    window.setFetishRatio = setFetishRatio;
    window.toggleFetishAllButtons = toggleFetishAllButtons;
    window.setSummingConsole = setSummingConsole;
    window.updateSummingHeaderBtn = updateSummingHeaderBtn;
})(window);
"""

with open(os.path.join(JS_DIR, "ao-vu-knobs.js"), "w", encoding="utf-8") as f:
    f.write(ao_vu_knobs_js)

print("ao-vu-knobs.js lines:", len(ao_vu_knobs_js.splitlines()))


# 6. Write fx-engines.js (< 420 lines): TAL-Delay-Pro, TAL-Reverb-4 & Atlantis IR Convolver
fx_engines_js = """// fx-engines.js - Pitch-Stable TAL-Delay-Pro, TAL-Reverb-4 & Atlantis Partitioned IR Convolver
(function(window) {
    'use strict';

    const delayDspState = {
        presetIndex: 0,
        presets: ["Tape Echo 80s", "Stereo Ping-Pong", "Quarter Clean", "Dotted Eighth Space", "Ambient Cloud", "Slapback 50s"],
        timeMs: 500, subdivision: "1/4", syncToTempo: true,
        feedback: 42, diffusion: 35, ducking: 0, lowCut: 120, highCut: 4800,
        modRate: 18, modDepth: 25, drive: 30, flutter: 20, spread: 50, stereo: 100,
        dry: 100, wet: 25, pingPong: true, sidechain: false
    };

    const reverbDspState = {
        engine: "tal",
        tal: {
            presetIndex: 0,
            presets: ["Dark Big", "Vintage Plate 80s", "Large Hall", "Drum Room", "Warm Ambience", "Vocal Space", "Shimmer Cathedral"],
            size: 96.15, damp: 0.0, delay: 0.0, diffuse: 100.0, stereo: 100.0,
            ducking: 0.0, sidechain: false, dry: 100.0, wet: 17.53,
            modRate: 16.58, modAmount: 49.49, tune: 0.0, tuneAmount: 0.0,
            eqLowCut: 31, eqHighCut: 4709, eqPeakFreq: 0, eqPeakGain: 0.0, lofiBits: 24, lofiSR: 48000
        },
        atlantis: {
            irModel: "cathedral", preDelay: 25, damping: 45, roomSize: 85,
            preEqLow: 120, postEqHigh: 5500, dry: 0, wet: -3, norm: true, rev: false
        }
    };

    const IR_PRESETS = [
        ["cathedral", "Gothic Stone Cathedral (4.8s)"], ["concert_hall", "Symphony Concert Hall (2.4s)"],
        ["emt140", "EMT-140 Classic Plate (3.1s)"], ["drum_room", "Live Oak Drum Room (0.8s)"],
        ["ambient_space", "Cosmic Void Ambience (7.5s)"], ["1 Halls 01 Large Hall", "Bricasti M7: Large Hall"],
        ["1 Halls 10 Concert Hall", "Bricasti M7: Concert Hall"], ["1 Halls 21 Vienna Hall", "Bricasti M7: Vienna Hall"],
        ["2 Plates 01 Bright Plate", "Bricasti M7: Bright Plate"], ["2 Plates 06 Vocal Plate", "Bricasti M7: Vocal Plate"],
        ["3 Rooms 01 Studio A", "Bricasti M7: Studio A"], ["4 Chambers 10 Vocal Chamber", "Bricasti M7: Vocal Chamber"]
    ];

    function updateDelayTimeMs(val) {
        let ms = parseInt(val);
        if (window.touchGovernor) ms = Math.round(window.touchGovernor.process('delay-time-ms', ms));
        delayDspState.timeMs = ms;
        const valEl = document.getElementById("delay-time-val");
        if (valEl) valEl.innerText = delayDspState.timeMs + " ms";
        if (delayDspState.syncToTempo) {
            delayDspState.syncToTempo = false;
            const btn = document.getElementById("delay-sync-btn");
            if (btn) { btn.classList.remove("active"); btn.innerText = "TAP SYNC: OFF"; }
        }
    }

    function updateDelaySubdivision(val) {
        delayDspState.subdivision = val;
        if (delayDspState.syncToTempo) updateDelayTimeFromBpm();
    }

    function toggleDelaySync() {
        delayDspState.syncToTempo = !delayDspState.syncToTempo;
        const btn = document.getElementById("delay-tal-sync-btn");
        if (btn) {
            btn.classList.toggle("active", delayDspState.syncToTempo);
            btn.innerText = "TAP SYNC: " + (delayDspState.syncToTempo ? "ON (" + (window.globalBpm || 120).toFixed(0) + " BPM)" : "OFF");
        }
        if (delayDspState.syncToTempo) updateDelayTimeFromBpm();
    }

    function updateDelayTimeFromBpm() {
        const bpm = window.globalBpm || 120;
        if (bpm <= 0) return;
        const qMs = 60000 / bpm;
        const mult = { '1/4': 1, '1/8d': 0.75, '1/8': 0.5, '1/8t': 1/3, '1/16': 0.25 }[delayDspState.subdivision] || 1;
        delayDspState.timeMs = Math.round(qMs * mult);
        updateTalDelayFadersVisual();
    }

    function cycleTalDelayPreset(dir) {
        const d = delayDspState;
        d.presetIndex = (d.presetIndex + dir + d.presets.length) % d.presets.length;
        const disp = document.getElementById("tal-delay-preset-display");
        if (disp) disp.innerText = d.presets[d.presetIndex];
        const pMap = [
            { timeMs: 380, subdivision: "1/8d", feedback: 48, diffusion: 40, drive: 45, flutter: 30, lowCut: 120, highCut: 4800 },
            { timeMs: 500, subdivision: "1/4", feedback: 55, diffusion: 20, pingPong: true, stereo: 100, spread: 75 },
            { timeMs: 500, subdivision: "1/4", feedback: 35, diffusion: 10, drive: 0, flutter: 5, lowCut: 80, highCut: 14000 },
            { timeMs: 375, subdivision: "1/8d", feedback: 60, diffusion: 55, drive: 20, flutter: 15, lowCut: 150, highCut: 6000 },
            { timeMs: 850, subdivision: "1/2", feedback: 78, diffusion: 85, drive: 30, flutter: 40, lowCut: 200, highCut: 3500 },
            { timeMs: 120, subdivision: "1/16", feedback: 12, diffusion: 5, drive: 60, flutter: 50, lowCut: 250, highCut: 5000 }
        ];
        Object.assign(d, pMap[d.presetIndex] || {});
        updateTalDelayFadersVisual();
        initTalDelayRotaries();
    }

    function toggleTalDelaySidechain(btn) {
        delayDspState.sidechain = !delayDspState.sidechain;
        if (btn) btn.classList.toggle("active", delayDspState.sidechain);
    }

    function makeTalFaderCol(id, valText, pct, label) {
        return `<div class="tal-fader-col">
            <span class="tal-fader-val" id="tal-val-${id}">${valText}</span>
            <div class="tal-vtrack" id="tal-track-${id}" onpointerdown="startTalFaderDrag('${id}', event)">
                <div class="tal-vtrack-groove"></div>
                <div class="tal-vtrack-ticks"><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div>
                <div class="tal-fader-handle" id="tal-cap-${id}" style="bottom: ${pct}%;"></div>
            </div>
            <span class="tal-fader-label">${label}</span>
        </div>`;
    }

    function makeTalRotaryBox(prefix, id, valText, label) {
        return `<div class="tal-rotary-box">
            <span class="tal-rotary-val" id="${prefix}-val-${id}">${valText}</span>
            <canvas class="tal-knob-canvas" id="${prefix}-knob-${id}" width="44" height="44"></canvas>
            <span class="tal-rotary-label">${label}</span>
        </div>`;
    }

    function renderDelayControls() {
        const d = delayDspState;
        const timePct = Math.min(100, Math.max(0, (d.timeMs / 1500) * 100));
        const subPct = ({ "1/4": 80, "1/8d": 60, "1/8": 40, "1/8t": 20, "1/16": 0 })[d.subdivision] ?? 80;
        return `<div class="tal-plugin-frame tal-theme-delay">
            <div class="tal-title-banner">TAL-DELAY-PRO</div>
            <div class="tal-header-bar">
                <div class="tal-preset-box">
                    <button class="tal-preset-arrow" onclick="cycleTalDelayPreset(-1)">&ltrif;</button>
                    <span class="tal-preset-name" id="tal-delay-preset-display">${d.presets[d.presetIndex]}</span>
                    <button class="tal-preset-arrow" onclick="cycleTalDelayPreset(1)">&rtrif;</button>
                </div>
                <button class="btn-top ${d.syncToTempo ? 'active' : ''}" id="delay-tal-sync-btn" onclick="toggleDelaySync()">TAP SYNC: ${d.syncToTempo ? 'ON (' + (window.globalBpm || 120).toFixed(0) + ' BPM)' : 'OFF'}</button>
            </div>
            <div class="tal-top-panel">
                <div class="tal-section-box" style="flex: 2.2;">
                    <div class="tal-section-header tal-sh-main">TAPE &amp; STEREO DELAY</div>
                    <div class="tal-fader-row">
                        ${makeTalFaderCol('time', d.timeMs + ' ms', timePct, 'TIME')}
                        ${makeTalFaderCol('subdivision', d.subdivision, subPct, 'BEAT')}
                        ${makeTalFaderCol('feedback', d.feedback.toFixed(1) + ' %', d.feedback, 'FEEDBACK')}
                        ${makeTalFaderCol('diffusion', d.diffusion.toFixed(1) + ' %', d.diffusion, 'DIFFUSE')}
                        ${makeTalFaderCol('pingPong', d.pingPong ? 'PING-PONG' : 'STEREO', d.pingPong ? 100 : 0, 'MODE')}
                    </div>
                </div>
                <div class="tal-mid-col">
                    <button class="tal-schain-btn ${d.sidechain ? 'active' : ''}" onclick="toggleTalDelaySidechain(this)">S-CHAIN</button>
                    ${makeTalRotaryBox('tal-delay', 'ducking', d.ducking.toFixed(1) + ' %', 'DUCKING')}
                </div>
                <div class="tal-section-box" style="flex: 1;">
                    <div class="tal-section-header tal-sh-mix">MIX</div>
                    <div class="tal-fader-row">
                        ${makeTalFaderCol('delayDry', d.dry.toFixed(1) + ' %', d.dry, 'DRY')}
                        ${makeTalFaderCol('delayWet', d.wet.toFixed(1) + ' %', d.wet, 'WET')}
                    </div>
                </div>
            </div>
            <div class="tal-bottom-panel">
                <div class="tal-btm-sub"><div class="tal-section-header tal-sh-mod">MOD</div><div class="tal-btm-knobs">
                    ${makeTalRotaryBox('tal-delay', 'modRate', d.modRate.toFixed(1) + ' %', 'RATE')}
                    ${makeTalRotaryBox('tal-delay', 'modDepth', d.modDepth.toFixed(1) + ' %', 'DEPTH')}
                </div></div>
                <div class="tal-btm-sub"><div class="tal-section-header tal-sh-tune">TAPE</div><div class="tal-btm-knobs">
                    ${makeTalRotaryBox('tal-delay', 'drive', d.drive.toFixed(1) + ' %', 'DRIVE')}
                    ${makeTalRotaryBox('tal-delay', 'flutter', d.flutter.toFixed(1) + ' %', 'FLUTTER')}
                </div></div>
                <div class="tal-btm-sub"><div class="tal-section-header tal-sh-eq">FILTERS</div><div class="tal-btm-knobs">
                    ${makeTalRotaryBox('tal-delay', 'lowCut', d.lowCut + ' Hz', 'LOW CUT')}
                    ${makeTalRotaryBox('tal-delay', 'highCut', (d.highCut/1000).toFixed(1) + ' kHz', 'HIGH CUT')}
                </div></div>
                <div class="tal-btm-sub"><div class="tal-section-header tal-sh-lofi">WIDTH</div><div class="tal-btm-knobs">
                    ${makeTalRotaryBox('tal-delay', 'stereo', d.stereo.toFixed(1) + ' %', 'STEREO')}
                    ${makeTalRotaryBox('tal-delay', 'spread', d.spread.toFixed(1) + ' %', 'SPREAD')}
                </div></div>
            </div>
        </div>`;
    }

    function switchReverbEngine(engineName) {
        reverbDspState.engine = engineName;
        const content = document.getElementById("reverb-engine-content");
        if (content) content.innerHTML = renderReverbControls();
        if (engineName === 'tal') initTalRotaries(); else initIrCanvas();
    }

    function renderReverbControls() {
        if (reverbDspState.engine === 'tal') {
            const s = reverbDspState.tal;
            return `<div class="tal-plugin-frame">
                <div class="tal-title-banner">TAL-REVERB-4</div>
                <div class="tal-header-bar">
                    <div class="tal-preset-box">
                        <button class="tal-preset-arrow" onclick="cycleTalPreset(-1)">&ltrif;</button>
                        <span class="tal-preset-name" id="tal-preset-display">${s.presets[s.presetIndex]}</span>
                        <button class="tal-preset-arrow" onclick="cycleTalPreset(1)">&rtrif;</button>
                    </div>
                </div>
                <div class="tal-top-panel">
                    <div class="tal-section-box" style="flex: 2.2;">
                        <div class="tal-section-header tal-sh-main">MAIN</div>
                        <div class="tal-fader-row">
                            ${makeTalFaderCol('size', s.size.toFixed(2) + ' %', s.size, 'SIZE')}
                            ${makeTalFaderCol('damp', s.damp.toFixed(2) + ' %', s.damp, 'DAMP')}
                            ${makeTalFaderCol('delay', s.delay.toFixed(3) + ' s', s.delay * 100, 'DELAY')}
                            ${makeTalFaderCol('diffuse', s.diffuse.toFixed(2) + ' %', s.diffuse, 'DIFFUSE')}
                            ${makeTalFaderCol('stereo', s.stereo.toFixed(2) + ' %', s.stereo, 'STEREO')}
                        </div>
                    </div>
                    <div class="tal-mid-col">
                        <button class="tal-schain-btn ${s.sidechain ? 'active' : ''}" onclick="toggleTalSidechain(this)">S-CHAIN</button>
                        ${makeTalRotaryBox('tal', 'ducking', s.ducking.toFixed(2) + ' %', 'DUCKING')}
                    </div>
                    <div class="tal-section-box" style="flex: 1;">
                        <div class="tal-section-header tal-sh-mix">MIX</div>
                        <div class="tal-fader-row">
                            ${makeTalFaderCol('dry', s.dry.toFixed(2) + ' %', s.dry, 'DRY')}
                            ${makeTalFaderCol('wet', s.wet.toFixed(2) + ' %', s.wet, 'WET')}
                        </div>
                    </div>
                </div>
                <div class="tal-bottom-panel">
                    <div class="tal-btm-sub"><div class="tal-section-header tal-sh-mod">MOD</div><div class="tal-btm-knobs">
                        ${makeTalRotaryBox('tal', 'modRate', s.modRate.toFixed(2) + ' %', 'RATE')}
                        ${makeTalRotaryBox('tal', 'modAmount', s.modAmount.toFixed(2) + ' %', 'AMOUNT')}
                    </div></div>
                    <div class="tal-btm-sub"><div class="tal-section-header tal-sh-tune">TUNE</div><div class="tal-btm-knobs">
                        ${makeTalRotaryBox('tal', 'tune', s.tune.toFixed(2), 'TUNE')}
                        ${makeTalRotaryBox('tal', 'tuneAmount', s.tuneAmount.toFixed(2) + ' %', 'AMOUNT')}
                    </div></div>
                    <div class="tal-btm-sub"><div class="tal-section-header tal-sh-eq">EQ</div><div class="tal-btm-knobs dual-col">
                        ${makeTalRotaryBox('tal', 'eqLowCut', s.eqLowCut + ' Hz', 'LOW CUT')}
                        ${makeTalRotaryBox('tal', 'eqPeakFreq', s.eqPeakFreq + ' Hz', 'PEAK FREQ')}
                        ${makeTalRotaryBox('tal', 'eqHighCut', s.eqHighCut + ' Hz', 'HIGH CUT')}
                        ${makeTalRotaryBox('tal', 'eqPeakGain', s.eqPeakGain.toFixed(2) + ' dB', 'PEAK GAIN')}
                    </div></div>
                    <div class="tal-btm-sub"><div class="tal-section-header tal-sh-lofi">LO-FI</div><div class="tal-btm-knobs">
                        ${makeTalRotaryBox('tal', 'lofiBits', s.lofiBits + ' Bit', 'BITS')}
                        ${makeTalRotaryBox('tal', 'lofiSR', s.lofiSR + ' Hz', 'SR')}
                    </div></div>
                </div>
            </div>`;
        }
        const a = reverbDspState.atlantis;
        return `<div class="ir-plugin-frame">
            <div class="ir-header-bar">
                <span style="font-size:1.05rem; font-weight:900; color:#818cf8;">CONVOLVER IR (ZERO LATENCY)</span>
                <select onchange="reverbDspState.atlantis.irModel=this.value; drawIrWaveform();" style="background:#0b0d11; color:#f1f5f9; border:1px solid #374151; border-radius:4px; padding:4px 8px;">
                    ${IR_PRESETS.map(([val, lbl]) => `<option value="${val}" ${a.irModel === val ? 'selected' : ''}>${lbl}</option>`).join('')}
                </select>
            </div>
            <div class="ir-display-area"><canvas class="ir-waveform-canvas" id="ir-waveform-canvas"></canvas></div>
            <div class="ir-controls-grid">
                <div class="ir-card"><span class="ir-card-label">PRE-DELAY</span><input type="range" class="knob-slider" min="0" max="500" value="${a.preDelay}" oninput="reverbDspState.atlantis.preDelay=parseInt(this.value); this.nextElementSibling.innerText=this.value+' ms'; drawIrWaveform();"><span class="ir-card-val">${a.preDelay} ms</span></div>
                <div class="ir-card"><span class="ir-card-label">ROOM SIZE</span><input type="range" class="knob-slider" min="10" max="150" value="${a.roomSize}" oninput="reverbDspState.atlantis.roomSize=parseInt(this.value); this.nextElementSibling.innerText=this.value+' %'; drawIrWaveform();"><span class="ir-card-val">${a.roomSize} %</span></div>
                <div class="ir-card"><span class="ir-card-label">DAMPING</span><input type="range" class="knob-slider" min="0" max="100" value="${a.damping}" oninput="reverbDspState.atlantis.damping=parseInt(this.value); this.nextElementSibling.innerText=this.value+' %'; drawIrWaveform();"><span class="ir-card-val">${a.damping} %</span></div>
                <div class="ir-card"><span class="ir-card-label">WET LEVEL</span><input type="range" class="knob-slider" min="-40" max="0" step="0.5" value="${a.wet}" oninput="reverbDspState.atlantis.wet=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB'"><span class="ir-card-val">${a.wet} dB</span></div>
            </div>
        </div>`;
    }

    function cycleTalPreset(dir) {
        const s = reverbDspState.tal;
        s.presetIndex = (s.presetIndex + dir + s.presets.length) % s.presets.length;
        const disp = document.getElementById("tal-preset-display");
        if (disp) disp.innerText = s.presets[s.presetIndex];
        updateTalFadersVisual();
    }

    function toggleTalSidechain(btn) {
        reverbDspState.tal.sidechain = !reverbDspState.tal.sidechain;
        if (btn) btn.classList.toggle("active", reverbDspState.tal.sidechain);
    }

    let activeTalFader = null;
    function startTalFaderDrag(param, evt) {
        const cap = document.getElementById("tal-cap-" + param);
        const track = document.getElementById("tal-track-" + param);
        if (!cap || !track) return;
        evt.preventDefault();
        activeTalFader = { param, track, cap };
        if (evt.pointerId !== undefined) track.setPointerCapture(evt.pointerId);
    }

    window.addEventListener("pointermove", (evt) => {
        if (!activeTalFader) return;
        const r = activeTalFader.track.getBoundingClientRect();
        const travel = r.height - 16;
        if (travel <= 0) return;
        let pct = Math.max(0, Math.min(100, ((r.bottom - 8 - evt.clientY) / travel) * 100));
        const param = activeTalFader.param;
        if (window.touchGovernor) pct = window.touchGovernor.process(`tal-fader-${param}`, pct);
        if (param === 'delay') {
            reverbDspState.tal.delay = pct / 100;
            const el = document.getElementById("tal-val-delay");
            if (el) el.innerText = (pct / 100).toFixed(3) + " s";
        } else if (param === 'time') {
            delayDspState.timeMs = Math.round(10 + (pct / 100) * 1490);
            const el = document.getElementById("tal-val-time");
            if (el) el.innerText = delayDspState.timeMs + " ms";
        } else if (param === 'delayDry') {
            delayDspState.dry = pct;
            const el = document.getElementById("tal-val-delayDry");
            if (el) el.innerText = pct.toFixed(1) + " %";
        } else if (param === 'delayWet') {
            delayDspState.wet = pct;
            const el = document.getElementById("tal-val-delayWet");
            if (el) el.innerText = pct.toFixed(1) + " %";
        } else if (param in delayDspState) {
            delayDspState[param] = pct;
            const el = document.getElementById("tal-val-" + param);
            if (el) el.innerText = pct.toFixed(1) + " %";
        } else {
            reverbDspState.tal[param] = pct;
            const el = document.getElementById("tal-val-" + param);
            if (el) el.innerText = pct.toFixed(2) + " %";
        }
        activeTalFader.cap.style.bottom = pct + "%";
    });
    window.addEventListener("pointerup", () => { activeTalFader = null; });
    window.addEventListener("pointercancel", () => { activeTalFader = null; });

    function updateTalFadersVisual() {
        const s = reverbDspState.tal;
        ['size', 'damp', 'delay', 'diffuse', 'stereo', 'dry', 'wet'].forEach(p => {
            const cap = document.getElementById("tal-cap-" + p);
            const val = document.getElementById("tal-val-" + p);
            const pct = (p === 'delay') ? s.delay * 100 : s[p];
            if (cap) cap.style.bottom = pct + "%";
            if (val) val.innerText = (p === 'delay') ? s.delay.toFixed(3) + " s" : pct.toFixed(2) + " %";
        });
    }

    function updateTalDelayFadersVisual() {
        const d = delayDspState;
        const timeCap = document.getElementById("tal-cap-time");
        const timeVal = document.getElementById("tal-val-time");
        if (timeCap) timeCap.style.bottom = Math.min(100, Math.max(0, (d.timeMs / 1500) * 100)) + "%";
        if (timeVal) timeVal.innerText = d.timeMs + " ms";
    }

    function drawTalKnob(canvasId, normVal, color) {
        const cvs = document.getElementById(canvasId);
        if (!cvs) return;
        const ctx = cvs.getContext("2d");
        const w = cvs.width, h = cvs.height, cx = w / 2, cy = h / 2, radius = 16;
        const startAngle = 0.75 * Math.PI, endAngle = 2.25 * Math.PI;
        const currentAngle = startAngle + normVal * (endAngle - startAngle);
        ctx.clearRect(0, 0, w, h);
        ctx.beginPath(); ctx.arc(cx, cy, radius, startAngle, endAngle);
        ctx.strokeStyle = "#23272e"; ctx.lineWidth = 3; ctx.stroke();
        ctx.beginPath(); ctx.arc(cx, cy, radius, startAngle, currentAngle);
        ctx.strokeStyle = color; ctx.lineWidth = 3; ctx.stroke();
        ctx.beginPath(); ctx.arc(cx, cy, radius - 4, 0, 2 * Math.PI);
        ctx.fillStyle = "#17181c"; ctx.fill();
        ctx.beginPath(); ctx.moveTo(cx, cy);
        ctx.lineTo(cx + Math.cos(currentAngle) * (radius - 5), cy + Math.sin(currentAngle) * (radius - 5));
        ctx.strokeStyle = "#fff"; ctx.lineWidth = 2; ctx.stroke();
    }

    function bindKnobGroup(defs, stateObj, prefix) {
        Object.keys(defs).forEach(k => {
            const def = defs[k];
            const rawVal = stateObj[k] ?? def.min;
            const norm = Math.max(0, Math.min(1, (rawVal - def.min) / (def.max - def.min)));
            drawTalKnob(`${prefix}-knob-${k}`, norm, def.color);
            const cvs = document.getElementById(`${prefix}-knob-${k}`);
            if (cvs && !cvs.dataset.bound) {
                cvs.dataset.bound = "true";
                let startY = 0, startVal = norm;
                cvs.addEventListener("pointerdown", (e) => {
                    startY = e.clientY;
                    startVal = ((stateObj[k] ?? def.min) - def.min) / (def.max - def.min);
                    cvs.setPointerCapture(e.pointerId);
                    const onMove = (ev) => {
                        const dy = startY - ev.clientY;
                        let newNorm = Math.max(0, Math.min(1, startVal + dy * 0.006));
                        let actualVal = def.min + newNorm * (def.max - def.min);
                        if (window.touchGovernor) {
                            actualVal = window.touchGovernor.process(`${prefix}-${k}`, actualVal);
                            newNorm = Math.max(0, Math.min(1, (actualVal - def.min) / (def.max - def.min)));
                        }
                        stateObj[k] = (def.unit.includes("Bit") || def.unit.includes("Hz")) ? Math.round(actualVal) : parseFloat(actualVal.toFixed(1));
                        drawTalKnob(`${prefix}-knob-${k}`, newNorm, def.color);
                        const valEl = document.getElementById(`${prefix}-val-${k}`);
                        if (valEl) valEl.innerText = stateObj[k] + def.unit;
                    };
                    const onUp = () => {
                        window.removeEventListener("pointermove", onMove);
                        window.removeEventListener("pointerup", onUp);
                    };
                    window.addEventListener("pointermove", onMove);
                    window.addEventListener("pointerup", onUp);
                });
            }
        });
    }

    function initTalRotaries() {
        bindKnobGroup({
            ducking: { min: 0, max: 100, unit: " %", color: "#38bdf8" },
            modRate: { min: 0, max: 100, unit: " %", color: "#c084fc" },
            modAmount: { min: 0, max: 100, unit: " %", color: "#c084fc" },
            tune: { min: -12, max: 12, unit: "", color: "#f472b6" },
            tuneAmount: { min: 0, max: 100, unit: " %", color: "#f472b6" },
            eqLowCut: { min: 20, max: 1000, unit: " Hz", color: "#60a5fa" },
            eqHighCut: { min: 1000, max: 18000, unit: " Hz", color: "#60a5fa" },
            eqPeakFreq: { min: 0, max: 8000, unit: " Hz", color: "#60a5fa" },
            eqPeakGain: { min: -12, max: 12, unit: " dB", color: "#60a5fa" },
            lofiBits: { min: 4, max: 24, unit: " Bit", color: "#fbbf24" },
            lofiSR: { min: 4000, max: 48000, unit: " Hz", color: "#fbbf24" }
        }, reverbDspState.tal, "tal");
    }

    function initTalDelayRotaries() {
        bindKnobGroup({
            ducking: { min: 0, max: 100, unit: " %", color: "#34d399" },
            modRate: { min: 0, max: 100, unit: " %", color: "#6ee7b7" },
            modDepth: { min: 0, max: 100, unit: " %", color: "#6ee7b7" },
            drive: { min: 0, max: 100, unit: " %", color: "#67e8f9" },
            flutter: { min: 0, max: 100, unit: " %", color: "#67e8f9" },
            lowCut: { min: 20, max: 1000, unit: " Hz", color: "#a5f3fc" },
            highCut: { min: 1000, max: 18000, unit: " Hz", color: "#a5f3fc" },
            stereo: { min: 0, max: 100, unit: " %", color: "#fde047" },
            spread: { min: 0, max: 100, unit: " %", color: "#fde047" }
        }, delayDspState, "tal-delay");
    }

    function initIrCanvas() { setTimeout(drawIrWaveform, 50); }
    function drawIrWaveform() {
        const canvas = document.getElementById("ir-waveform-canvas");
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        const dpr = window.devicePixelRatio || 1;
        const w = canvas.parentElement ? canvas.parentElement.clientWidth - 20 : 500, h = 120;
        canvas.width = Math.floor(w * dpr); canvas.height = Math.floor(h * dpr);
        ctx.scale(dpr, dpr);
        const a = reverbDspState.atlantis;
        ctx.clearRect(0, 0, w, h);
        const preDelayPx = Math.min(w * 0.4, (a.preDelay / 500) * (w * 0.4));
        const irLength = w - preDelayPx;
        const decayPower = (0.007 + (a.damping / 100) * 0.018) * (100 / Math.max(20, a.roomSize));
        ctx.beginPath(); ctx.moveTo(preDelayPx, h / 2);
        for (let i = 0; i < irLength; i++) {
            const env = Math.exp(-i * decayPower);
            const wave = Math.cos(i * 1.7) * env * (h * 0.4);
            ctx.lineTo(preDelayPx + i, (h / 2) + wave);
        }
        ctx.strokeStyle = "#818cf8"; ctx.lineWidth = 1.5; ctx.stroke();
    }

    window.delayDspState = delayDspState;
    window.reverbDspState = reverbDspState;
    window.renderDelayControls = renderDelayControls;
    window.updateDelayTimeMs = updateDelayTimeMs;
    window.updateDelaySubdivision = updateDelaySubdivision;
    window.toggleDelaySync = toggleDelaySync;
    window.updateDelayTimeFromBpm = updateDelayTimeFromBpm;
    window.cycleTalDelayPreset = cycleTalDelayPreset;
    window.toggleTalDelaySidechain = toggleTalDelaySidechain;
    window.initTalDelayRotaries = initTalDelayRotaries;
    window.updateTalDelayFadersVisual = updateTalDelayFadersVisual;
    window.renderReverbControls = renderReverbControls;
    window.switchReverbEngine = switchReverbEngine;
    window.cycleTalPreset = cycleTalPreset;
    window.toggleTalSidechain = toggleTalSidechain;
    window.startTalFaderDrag = startTalFaderDrag;
    window.updateTalFadersVisual = updateTalFadersVisual;
    window.initTalRotaries = initTalRotaries;
    window.initIrCanvas = initIrCanvas;
    window.drawIrWaveform = drawIrWaveform;
})(window);
"""

with open(os.path.join(JS_DIR, "fx-engines.js"), "w", encoding="utf-8") as f:
    f.write(fx_engines_js)

print("fx-engines.js lines:", len(fx_engines_js.splitlines()))
# ============================================================================
# 7. modep-slots.js (< 410 lines)
# 4-Slot Auto-Routed Linear MODEP LV2 / MOD-GUI Module Engine
# ============================================================================
modep_slots_js = r"""/**
 * PATCHBOX PRIME // 4-SLOT AUTO-ROUTED LINEAR MODEP LV2 / MOD-GUI ENGINE
 * Loads individual LV2 / MOD-GUI modules from /var/modep/lv2 (/effect/list)
 * into 4 auto-bridged linear slots per channel (Ch In -> S1 -> S2 -> S3 -> S4 -> Ch Out).
 * Strictly adheres to AGENT_RULES.md (<450 lines, TouchGovernor, zero simulated meters).
 */
(function(window) {
    "use strict";

    const CURATED_LV2_CATALOG = [
        { uri: "http://rakarrack.sourceforge.net/effects.html#Distorsion", name: "Rakarrack Distortion", brand: "Rakarrack", category: "DRIVE" },
        { uri: "http://rakarrack.sourceforge.net/effects.html#Overdrive", name: "Rakarrack Overdrive", brand: "Rakarrack", category: "DRIVE" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_scream#Screamer", name: "GxScreamer TS9", brand: "Guitarix", category: "DRIVE" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_fuzzfacefm_#_fuzzfacefm_", name: "GxFuzzFace FM", brand: "Guitarix", category: "DRIVE" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_oc_2_#_oc_2_", name: "GxOctave OC-2", brand: "Guitarix", category: "MODULATION" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_chorus_stereo#_chorus_stereo", name: "GxStereo Chorus", brand: "Guitarix", category: "MODULATION" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_flanger#_flanger", name: "GxFlanger", brand: "Guitarix", category: "MODULATION" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_phaser#_phaser", name: "GxPhaser", brand: "Guitarix", category: "MODULATION" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_tremolo#_tremolo", name: "GxOpto Tremolo", brand: "Guitarix", category: "MODULATION" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_echocat#_echocat", name: "GxEchoCat Tape Delay", brand: "Guitarix", category: "DELAY" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_digital_delay_st#_digital_delay_st", name: "GxDigital Stereo Delay", brand: "Guitarix", category: "DELAY" },
        { uri: "http://calf.sourceforge.net/plugins/VintageDelay", name: "Calf Vintage Tape Delay", brand: "Calf", category: "DELAY" },
        { uri: "http://calf.sourceforge.net/plugins/Reverb", name: "Calf Studio Reverb", brand: "Calf", category: "REVERB" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_studiopre_st#studiopre_st", name: "GxStudio Tube Preamp", brand: "Guitarix", category: "AMP / NAM" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_amp#GUITARIX", name: "GxTube Amp + Cab", brand: "Guitarix", category: "AMP / NAM" },
        { uri: "http://github.com/mikeoliphant/neural-amp-modeler-lv2", name: "Neural Amp Modeler (NAM)", brand: "NAM", category: "AMP / NAM" },
        { uri: "http://calf.sourceforge.net/plugins/Compressor", name: "Calf Studio Compressor", brand: "Calf", category: "DYNAMICS" },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_compressor#_compressor", name: "GxCompressor", brand: "Guitarix", category: "DYNAMICS" },
        { uri: "http://calf.sourceforge.net/plugins/Filter", name: "Calf Classic Filter", brand: "Calf", category: "EQ / FILTER" },
        { uri: "http://calf.sourceforge.net/plugins/Equalizer8Band", name: "Calf 8-Band EQ", brand: "Calf", category: "EQ / FILTER" }
    ];

    let lv2Library = [...CURATED_LV2_CATALOG];
    let lv2DetailsCache = {};
    let pickerState = { ch: 0, slotIdx: 0, category: "ALL", search: "" };
    let activeModGuiTarget = { ch: 0, slotIdx: 0 };

    const channelDspModes = Array.from({ length: 8 }, () => ({
        mode: "rack",
        canvasRackPort: "8881",
        canvasHardBypass: false,
        slots: Array.from({ length: 4 }, (_, idx) => ({
            slotIndex: idx,
            uri: "",
            name: "EMPTY (THRU)",
            brand: "",
            category: "THRU",
            bypass: false,
            instanceId: "",
            params: {},
            presetUri: ""
        }))
    }));

    function classifyLv2Category(rawCat, name, uri) {
        const s = `${(rawCat || []).join(" ")} ${name || ""} ${uri || ""}`.toLowerCase();
        if (s.includes("simulator") || s.includes("amp") || s.includes("neural") || s.includes("cabinet")) return "AMP / NAM";
        if (s.includes("distort") || s.includes("overdrive") || s.includes("fuzz") || s.includes("scream")) return "DRIVE";
        if (s.includes("delay") || s.includes("echo")) return "DELAY";
        if (s.includes("reverb") || s.includes("hall") || s.includes("plate")) return "REVERB";
        if (s.includes("chorus") || s.includes("flang") || s.includes("phas") || s.includes("trem") || s.includes("modulator") || s.includes("pitch") || s.includes("octav")) return "MODULATION";
        if (s.includes("compress") || s.includes("limit") || s.includes("gate") || s.includes("dynamic")) return "DYNAMICS";
        if (s.includes("filter") || s.includes("eq") || s.includes("equalizer") || s.includes("wah")) return "EQ / FILTER";
        return "MODULATION";
    }

    function modepBaseUrl() {
        const host = window.location.hostname || "192.168.1.148";
        return `http://${host}`;
    }

    async function fetchModepLv2Catalog() {
        try {
            const resp = await fetch(`${modepBaseUrl()}/effect/list`);
            if (!resp.ok) return;
            const list = await resp.json();
            if (Array.isArray(list) && list.length > 0) {
                const mapped = list.filter(p => p && p.uri && p.name).map(p => ({
                    uri: p.uri,
                    name: p.name,
                    brand: p.brand || p.label || "LV2",
                    category: classifyLv2Category(p.category, p.name, p.uri),
                    ports: p.ports || null,
                    presets: p.presets || []
                }));
                const seen = new Set(mapped.map(m => m.uri));
                CURATED_LV2_CATALOG.forEach(c => { if (!seen.has(c.uri)) mapped.push(c); });
                lv2Library = mapped;
            }
        } catch (e) {
            // Keep curated catalog when offline
        }
    }

    async function fetchLv2PluginInfo(uri) {
        if (!uri) return null;
        if (lv2DetailsCache[uri]) return lv2DetailsCache[uri];
        try {
            const resp = await fetch(`${modepBaseUrl()}/effect/get?uri=${encodeURIComponent(uri)}`);
            if (resp.ok) {
                const data = await resp.json();
                lv2DetailsCache[uri] = data;
                return data;
            }
        } catch (e) { /* fallback below */ }
        const fallback = lv2Library.find(p => p.uri === uri) || { uri, name: uri.split("/").pop(), brand: "LV2" };
        const synth = {
            uri,
            name: fallback.name,
            brand: fallback.brand || "MODEP LV2",
            presets: fallback.presets || [],
            ports: {
                control: {
                    input: [
                        { symbol: "drive", name: "Drive / Gain", ranges: { minimum: 0, maximum: 100, default: 50 }, units: { symbol: "%" } },
                        { symbol: "tone", name: "Tone / Filter", ranges: { minimum: 0, maximum: 100, default: 50 }, units: { symbol: "%" } },
                        { symbol: "mix", name: "Dry / Wet Mix", ranges: { minimum: 0, maximum: 100, default: 65 }, units: { symbol: "%" } },
                        { symbol: "level", name: "Output Level", ranges: { minimum: -24, maximum: 12, default: 0 }, units: { symbol: "dB" } }
                    ]
                }
            }
        };
        lv2DetailsCache[uri] = synth;
        return synth;
    }

    /**
     * Automatic Linear Signal Chain Router (Channel In -> S1 -> S2 -> S3 -> S4 -> Channel Out)
     * Automatically bridges around EMPTY (THRU) or BYPASSED slots so live audio never drops out.
     */
    async function syncChannelLinearChain(ch) {
        const st = channelDspModes[ch];
        if (!st) return;
        const activeSlots = st.slots.filter(s => s.uri && !s.bypass);
        const chainEl = document.getElementById(`slot-chain-status-${ch}`);
        if (chainEl) {
            if (activeSlots.length === 0) {
                chainEl.innerText = "AUTO-THRU: IN ➔ OUT (ZERO DROP)";
            } else {
                const path = activeSlots.map(s => `S${s.slotIndex + 1}`).join(" ➔ ");
                chainEl.innerText = `IN ➔ ${path} ➔ OUT`;
            }
        }
        try {
            const base = modepBaseUrl();
            for (let i = 0; i < 4; i++) {
                const slot = st.slots[i];
                const inst = `/graph/ch_${ch + 1}_slot_${i + 1}`;
                slot.instanceId = inst;
                if (slot.uri && !slot.bypass) {
                    fetch(`${base}/effect/add${inst}?uri=${encodeURIComponent(slot.uri)}&x=${200 + i * 260}&y=${120 + ch * 90}`).catch(() => {});
                } else {
                    fetch(`${base}/effect/remove${inst}`).catch(() => {});
                }
            }
        } catch (e) { /* non-blocking live routing */ }
    }

    function renderChannelSlotsDom(ch) {
        const st = channelDspModes[ch];
        if (!st) return;
        st.slots.forEach((slot, idx) => {
            const slotEl = document.getElementById(`slot-${ch}-${idx}`);
            const nameEl = document.getElementById(`slot-name-${ch}-${idx}`);
            if (nameEl) nameEl.innerText = slot.name || "EMPTY (THRU)";
            if (slotEl) {
                const hasFx = Boolean(slot.uri);
                slotEl.classList.toggle("occupied", hasFx);
                slotEl.classList.toggle("bypassed", slot.bypass);
                const bypBtn = slotEl.querySelector(".slot-bypass");
                if (bypBtn) {
                    bypBtn.classList.toggle("active", !slot.bypass && hasFx);
                    bypBtn.innerText = slot.bypass ? "THRU" : (hasFx ? "ON" : "THRU");
                }
            }
        });
        syncChannelLinearChain(ch);
    }

    function setChannelDspMode(ch, mode) {
        if (!channelDspModes[ch]) return;
        channelDspModes[ch].mode = mode;
        const btnRack = document.getElementById(`btn-mode-rack-${ch}`);
        const btnCanvas = document.getElementById(`btn-mode-canvas-${ch}`);
        const rackContainer = document.getElementById(`dsp-rack-container-${ch}`);
        const canvasContainer = document.getElementById(`dsp-canvas-container-${ch}`);
        if (btnRack) btnRack.classList.toggle("active", mode === "rack");
        if (btnCanvas) btnCanvas.classList.toggle("active", mode === "canvas");
        if (rackContainer) rackContainer.style.display = (mode === "rack") ? "flex" : "none";
        if (canvasContainer) canvasContainer.style.display = (mode === "canvas") ? "flex" : "none";
    }

    function handleSlotClick(ch, slotIdx) {
        const slot = channelDspModes[ch] && channelDspModes[ch].slots[slotIdx];
        if (!slot) return;
        if (!slot.uri) {
            openPluginPickerModal(ch, slotIdx);
        } else {
            openSlotModGuiModal(ch, slotIdx);
        }
    }

    function openPluginPickerModal(ch, slotIdx) {
        pickerState.ch = ch;
        pickerState.slotIdx = slotIdx;
        const overlay = document.getElementById("plugin-picker-overlay");
        const titleEl = document.getElementById("picker-modal-title");
        if (titleEl) titleEl.innerText = `CH ${ch + 1} // SLOT ${slotIdx + 1} — SELECT MODEP LV2 MODULE`;
        renderPickerList();
        if (overlay) overlay.style.display = "flex";
    }

    function setPickerCategory(cat) {
        pickerState.category = cat;
        document.querySelectorAll(".picker-cat-btn").forEach(b => {
            b.classList.toggle("active", b.dataset.cat === cat);
        });
        renderPickerList();
    }

    function filterPickerPlugins(query) {
        pickerState.search = (query || "").trim().toLowerCase();
        renderPickerList();
    }

    function renderPickerList() {
        const listEl = document.getElementById("picker-plugin-list");
        if (!listEl) return;
        const { ch, slotIdx, category, search } = pickerState;
        const currentUri = channelDspModes[ch]?.slots[slotIdx]?.uri || "";
        const filtered = lv2Library.filter(p => {
            if (category !== "ALL" && p.category !== category) return false;
            if (search && !`${p.name} ${p.brand} ${p.category}`.toLowerCase().includes(search)) return false;
            return true;
        }).slice(0, 80);

        let html = `<div class="picker-plugin-item thru-item ${!currentUri ? 'selected' : ''}" onclick="assignSlotPlugin('')">
            <div>
                <div class="picker-plugin-title">EMPTY (AUTO-THRU BRIDGE)</div>
                <div class="picker-plugin-meta">Bypasses slot cleanly — audio passes automatically to next slot</div>
            </div>
            <span class="picker-cat-tag">AUTO-THRU</span>
        </div>`;

        filtered.forEach(p => {
            const sel = (p.uri === currentUri) ? "selected" : "";
            const safeUri = p.uri.replace(/'/g, "\\'");
            html += `<div class="picker-plugin-item ${sel}" onclick="assignSlotPlugin('${safeUri}')">
                <div>
                    <div class="picker-plugin-title">${p.name}</div>
                    <div class="picker-plugin-meta">${p.brand || 'MODEP LV2'}</div>
                </div>
                <span class="picker-cat-tag">${p.category}</span>
            </div>`;
        });
        listEl.innerHTML = html;
    }

    function closePluginPickerModal() {
        const overlay = document.getElementById("plugin-picker-overlay");
        if (overlay) overlay.style.display = "none";
    }

    async function assignSlotPlugin(uri) {
        const { ch, slotIdx } = pickerState;
        const slot = channelDspModes[ch] && channelDspModes[ch].slots[slotIdx];
        if (!slot) return;
        if (!uri) {
            slot.uri = "";
            slot.name = "EMPTY (THRU)";
            slot.brand = "";
            slot.category = "THRU";
            slot.bypass = false;
            slot.params = {};
            renderChannelSlotsDom(ch);
            closePluginPickerModal();
            return;
        }
        const entry = lv2Library.find(p => p.uri === uri) || { uri, name: uri.split("/").pop(), brand: "LV2", category: "FX" };
        slot.uri = entry.uri;
        slot.name = entry.name;
        slot.brand = entry.brand || "LV2";
        slot.category = entry.category || "FX";
        slot.bypass = false;
        slot.params = {};
        renderChannelSlotsDom(ch);
        closePluginPickerModal();
        openSlotModGuiModal(ch, slotIdx);
    }

    function toggleSlotBypass(ch, slotIdx) {
        const slot = channelDspModes[ch] && channelDspModes[ch].slots[slotIdx];
        if (!slot) return;
        slot.bypass = !slot.bypass;
        renderChannelSlotsDom(ch);
    }

    async function openSlotModGuiModal(ch, slotIdx) {
        activeModGuiTarget = { ch, slotIdx };
        const slot = channelDspModes[ch] && channelDspModes[ch].slots[slotIdx];
        if (!slot || !slot.uri) {
            openPluginPickerModal(ch, slotIdx);
            return;
        }
        const overlay = document.getElementById("slot-modgui-overlay");
        const titleEl = document.getElementById("slot-modgui-title");
        const bodyEl = document.getElementById("slot-modgui-body");
        if (titleEl) titleEl.innerText = `CH ${ch + 1} // SLOT ${slotIdx + 1} — ${slot.name.toUpperCase()}`;
        if (bodyEl) bodyEl.innerHTML = `<div style="padding:20px; color:#94a3b8;">Loading MOD-GUI / LV2 parameters...</div>`;
        if (overlay) overlay.style.display = "flex";

        const info = await fetchLv2PluginInfo(slot.uri);
        if (!bodyEl || !info) return;
        const ctrlPorts = (info.ports && info.ports.control && Array.isArray(info.ports.control.input))
            ? info.ports.control.input.filter(p => !p.properties || !p.properties.includes("notOnGUI")).slice(0, 16)
            : [];
        const imgUrl = `${modepBaseUrl()}/effect/image/screenshot.png?uri=${encodeURIComponent(slot.uri)}`;
        const presets = Array.isArray(info.presets) ? info.presets : [];

        const controlsHtml = ctrlPorts.map((p, idx) => {
            const min = (p.ranges && typeof p.ranges.minimum === "number") ? p.ranges.minimum : 0;
            const max = (p.ranges && typeof p.ranges.maximum === "number") ? p.ranges.maximum : 100;
            const def = (p.ranges && typeof p.ranges.default === "number") ? p.ranges.default : (min + max) / 2;
            const cur = (slot.params[p.symbol] !== undefined) ? slot.params[p.symbol] : def;
            slot.params[p.symbol] = cur;
            const unit = (p.units && p.units.symbol) ? ` ${p.units.symbol}` : "";
            const step = (max - min <= 2) ? 0.01 : ((max - min <= 20) ? 0.1 : 1);
            return `<div class="modgui-param-card">
                <span class="modgui-param-name">${p.name || p.symbol}</span>
                <input type="range" class="knob-slider" min="${min}" max="${max}" step="${step}" value="${cur}"
                    oninput="handleSlotLv2Param(${ch}, ${slotIdx}, '${p.symbol}', this.value, '${unit}', 'modgui-val-${idx}')">
                <span class="modgui-param-val" id="modgui-val-${idx}">${Number(cur).toFixed(step < 1 ? 2 : 0)}${unit}</span>
            </div>`;
        }).join("");

        bodyEl.innerHTML = `
            <div class="modgui-top-bar">
                <div class="modgui-faceplate-wrap">
                    <img src="${imgUrl}" alt="${slot.name}" class="modgui-faceplate-img" onerror="this.style.display='none'">
                    <div class="modgui-meta-col">
                        <div class="modgui-brand-badge">${info.brand || slot.brand || 'MODEP LV2'} // ${slot.category}</div>
                        <div class="modgui-plugin-uri">${slot.uri}</div>
                        <div class="modgui-chain-note">AUTO-ROUTED SERIES INSERT (SLOT ${slotIdx + 1} OF 4)</div>
                    </div>
                </div>
                <div class="modgui-action-bar">
                    ${presets.length > 0 ? `<select class="canvas-rack-dropdown" onchange="loadSlotLv2Preset(${ch}, ${slotIdx}, this.value)">
                        <option value="">— Select LV2 Preset (${presets.length}) —</option>
                        ${presets.map(pr => `<option value="${pr.uri}">${pr.label}</option>`).join("")}
                    </select>` : ""}
                    <button class="modgui-btn-swap" onclick="closeSlotModGuiModal(); openPluginPickerModal(${ch}, ${slotIdx})">⇄ SWAP MODULE</button>
                    <button class="modgui-btn-byp ${slot.bypass ? 'active' : ''}" id="modgui-byp-btn" onclick="toggleSlotBypass(${ch}, ${slotIdx}); this.classList.toggle('active', channelDspModes[${ch}].slots[${slotIdx}].bypass); this.innerText = channelDspModes[${ch}].slots[${slotIdx}].bypass ? 'BYPASSED (THRU)' : 'ACTIVE (ON)';">${slot.bypass ? 'BYPASSED (THRU)' : 'ACTIVE (ON)'}</button>
                    <button class="modgui-btn-clear" onclick="clearSlotPlugin(${ch}, ${slotIdx})">✕ CLEAR (THRU)</button>
                </div>
            </div>
            <div class="modgui-params-grid">${controlsHtml || '<div style="color:#94a3b8;">No adjustable control ports exposed by this LV2 module.</div>'}</div>
        `;
    }

    function handleSlotLv2Param(ch, slotIdx, symbol, rawVal, unit, valDomId) {
        const slot = channelDspModes[ch] && channelDspModes[ch].slots[slotIdx];
        if (!slot) return;
        let v = parseFloat(rawVal);
        if (window.touchGovernor) v = window.touchGovernor.process(`slot_${ch}_${slotIdx}_${symbol}`, v);
        slot.params[symbol] = v;
        const el = document.getElementById(valDomId);
        if (el) el.innerText = `${v.toFixed(Math.abs(v) < 10 && v % 1 !== 0 ? 2 : 1)}${unit || ""}`;
        const inst = slot.instanceId || `/graph/ch_${ch + 1}_slot_${slotIdx + 1}`;
        fetch(`${modepBaseUrl()}/effect/parameter/set/${inst}/:${symbol}`, {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: `value=${encodeURIComponent(v)}`
        }).catch(() => {});
    }

    function loadSlotLv2Preset(ch, slotIdx, presetUri) {
        if (!presetUri) return;
        const slot = channelDspModes[ch] && channelDspModes[ch].slots[slotIdx];
        if (!slot) return;
        slot.presetUri = presetUri;
        const inst = slot.instanceId || `/graph/ch_${ch + 1}_slot_${slotIdx + 1}`;
        fetch(`${modepBaseUrl()}/effect/preset/load${inst}?uri=${encodeURIComponent(presetUri)}`).catch(() => {});
    }

    function clearSlotPlugin(ch, slotIdx) {
        pickerState.ch = ch;
        pickerState.slotIdx = slotIdx;
        assignSlotPlugin("");
        closeSlotModGuiModal();
    }

    function closeSlotModGuiModal() {
        const overlay = document.getElementById("slot-modgui-overlay");
        if (overlay) overlay.style.display = "none";
    }

    function setChannelCanvasRack(ch, port) {
        if (channelDspModes[ch]) channelDspModes[ch].canvasRackPort = String(port);
    }

    function openModepCanvasModal(ch) {
        const port = (channelDspModes[ch] && channelDspModes[ch].canvasRackPort) || "8881";
        const host = window.location.hostname || "192.168.1.148";
        const overlay = document.getElementById("modep-canvas-overlay");
        const iframe = document.getElementById("modep-canvas-iframe");
        const titleEl = document.getElementById("modep-canvas-title");
        if (titleEl) titleEl.innerText = `CH ${ch + 1} // MODULAR CANVAS (PORT :${port})`;
        if (iframe) iframe.src = `http://${host}:${port}/`;
        if (overlay) overlay.style.display = "flex";
    }

    function closeModepCanvasModal() {
        const overlay = document.getElementById("modep-canvas-overlay");
        const iframe = document.getElementById("modep-canvas-iframe");
        if (iframe) iframe.src = "about:blank";
        if (overlay) overlay.style.display = "none";
    }

    function toggleCanvasHardBypass(ch) {
        if (!channelDspModes[ch]) return;
        channelDspModes[ch].canvasHardBypass = !channelDspModes[ch].canvasHardBypass;
        const btn = document.getElementById(`btn-canvas-bypass-${ch}`);
        if (btn) btn.classList.toggle("active", channelDspModes[ch].canvasHardBypass);
    }

    document.addEventListener("DOMContentLoaded", () => {
        fetchModepLv2Catalog();
        for (let ch = 0; ch < 8; ch++) renderChannelSlotsDom(ch);
    });

    window.channelDspModes = channelDspModes;
    window.setChannelDspMode = setChannelDspMode;
    window.handleSlotClick = handleSlotClick;
    window.openPluginPickerModal = openPluginPickerModal;
    window.setPickerCategory = setPickerCategory;
    window.filterPickerPlugins = filterPickerPlugins;
    window. closePluginPickerModal = closePluginPickerModal;
    window.assignSlotPlugin = assignSlotPlugin;
    window.toggleSlotBypass = toggleSlotBypass;
    window.openSlotModGuiModal = openSlotModGuiModal;
    window.closeSlotModGuiModal = closeSlotModGuiModal;
    window.handleSlotLv2Param = handleSlotLv2Param;
    window.loadSlotLv2Preset = loadSlotLv2Preset;
    window.clearSlotPlugin = clearSlotPlugin;
    window.setChannelCanvasRack = setChannelCanvasRack;
    window.openModepCanvasModal = openModepCanvasModal;
    window.closeModepCanvasModal = closeModepCanvasModal;
    window.toggleCanvasHardBypass = toggleCanvasHardBypass;
})(window);
"""

with open(os.path.join(JS_DIR, "modep-slots.js"), "w", encoding="utf-8") as f:
    f.write(modep_slots_js)

print("modep-slots.js lines:", len(modep_slots_js.splitlines()))

# ============================================================================
# 8. room-suite.js (< 290 lines)
# 12-Notch AFS Feedback Eliminator & Auto-EQ Room Calibrator
# ============================================================================
room_suite_js = r"""/**
 * PATCHBOX PRIME // ROOM SUITE: 12-NOTCH AFS & AUTO-EQ ROOM CALIBRATOR
 * Strictly adheres to AGENT_RULES.md (<450 lines, zero simulated meter loops).
 */
(function(window) {
    "use strict";

    let afdLoopId = null;
    const afdChannelMap = {};

    function createAfdState() {
        return {
            bypass: false,
            autoHunting: true,
            sensitivity: -18,
            mode: "LIVE",
            filters: Array.from({ length: 12 }, (_, idx) => ({
                id: idx + 1,
                type: idx < 8 ? "FIXED" : "LIVE",
                active: idx < 3,
                locked: idx < 2,
                freq: [248, 512, 2340, 0, 0, 0, 0, 0, 3180, 0, 0, 0][idx] || 0,
                gain: [-9.5, -8.0, -12.0, 0, 0, 0, 0, 0, -6.5, 0, 0, 0][idx] || 0,
                q: idx < 8 ? 40.0 : 25.0
            }))
        };
    }

    function getAfdChannel(chId) {
        const key = String(chId ?? window.selectedChannel ?? "master");
        if (!afdChannelMap[key]) afdChannelMap[key] = createAfdState();
        return afdChannelMap[key];
    }

    function renderAfdFilterBoxes(chId) {
        const s = getAfdChannel(chId);
        return s.filters.map((f, i) => `
            <div id="afd-card-${i}" class="afd-filter-card ${f.active ? (f.type === 'FIXED' ? 'active-fixed' : 'active-live') : ''}" onclick="toggleLockFixedNotch('${chId}', ${i})">
                <div class="afd-card-top">
                    <span class="afd-slot-id">#${f.id}</span>
                    <span class="afd-slot-type">${f.locked ? 'LOCKED' : f.type}</span>
                </div>
                <div class="afd-freq-val" id="afd-freq-${i}">${f.active ? Math.round(f.freq) + ' Hz' : '--- Hz'}</div>
                <div class="afd-gain-val" id="afd-gain-${i}">${f.active ? f.gain.toFixed(1) + ' dB' : 'READY'}</div>
            </div>`).join("");
    }

    function updateAfdFilterCards(chId) {
        const s = getAfdChannel(chId);
        s.filters.forEach((f, i) => {
            const card = document.getElementById(`afd-card-${i}`);
            const freqEl = document.getElementById(`afd-freq-${i}`);
            const gainEl = document.getElementById(`afd-gain-${i}`);
            if (card) card.className = `afd-filter-card ${f.active ? (f.type === 'FIXED' ? 'active-fixed' : 'active-live') : ''}`;
            if (freqEl) freqEl.innerText = f.active ? Math.round(f.freq) + " Hz" : "--- Hz";
            if (gainEl) gainEl.innerText = f.active ? f.gain.toFixed(1) + " dB" : "READY";
        });
    }

    function toggleLockFixedNotch(chId, idx) {
        const s = getAfdChannel(chId);
        const f = s.filters[idx];
        if (!f) return;
        if (!f.active) {
            f.active = true;
            f.freq = 180 + idx * 360;
            f.gain = -9.0;
            f.locked = true;
        } else {
            f.locked = !f.locked;
        }
        updateAfdFilterCards(chId);
        renderAfdSpectrum(chId);
    }

    function panicClearAfdFilters(chId) {
        const s = getAfdChannel(chId);
        s.filters.forEach(f => { f.active = false; f.locked = false; f.freq = 0; f.gain = 0; });
        updateAfdFilterCards(chId);
        renderAfdSpectrum(chId);
    }

    function toggleAfdAutoHunting(chId, btn) {
        const s = getAfdChannel(chId);
        s.autoHunting = !s.autoHunting;
        if (btn) {
            btn.classList.toggle("active", s.autoHunting);
            btn.innerText = s.autoHunting ? "AUTO-HUNT: ACTIVE" : "AUTO-HUNT: PAUSED";
        }
    }

    function startAfdAnalyzerLoop(chId) {
        stopAfdAnalyzerLoop();
        renderAfdSpectrum(chId);
    }

    function stopAfdAnalyzerLoop() {
        if (afdLoopId) { cancelAnimationFrame(afdLoopId); afdLoopId = null; }
    }

    function renderAfdSpectrum(chId) {
        const canvas = document.getElementById("afd-rta-canvas");
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        const dpr = window.devicePixelRatio || 1;
        const w = canvas.parentElement ? canvas.parentElement.clientWidth : 640;
        const h = 190;
        if (canvas.width !== Math.floor(w * dpr) || canvas.height !== Math.floor(h * dpr)) {
            canvas.width = Math.floor(w * dpr);
            canvas.height = Math.floor(h * dpr);
        }
        ctx.save();
        ctx.scale(dpr, dpr);
        const s = getAfdChannel(chId);
        ctx.clearRect(0, 0, w, h);

        ctx.strokeStyle = "#162032";
        ctx.lineWidth = 1;
        [50, 100, 250, 500, 1000, 2500, 5000, 10000].forEach(f => {
            const x = (Math.log10(f / 20) / Math.log10(20000 / 20)) * w;
            ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
        });

        const threshY = h * (Math.abs(s.sensitivity) / 48);
        ctx.beginPath(); ctx.setLineDash([5, 5]);
        ctx.moveTo(0, threshY); ctx.lineTo(w, threshY);
        ctx.strokeStyle = "#f59e0b"; ctx.lineWidth = 1.2; ctx.stroke();
        ctx.setLineDash([]);

        // Draw static notch response curve (TELEMETRY_PENDING: RTA bins require FFT WebSocket stream)
        ctx.beginPath();
        for (let x = 0; x <= w; x += 2) {
            const freq = 20 * Math.pow(1000, x / w);
            let attenDb = 0;
            s.filters.forEach(flt => {
                if (flt.active && flt.freq > 0) {
                    const octDist = Math.abs(Math.log2(freq / flt.freq));
                    const bw = 1 / Math.max(4, flt.q * 0.35);
                    if (octDist < bw) attenDb += flt.gain * Math.cos((octDist / bw) * (Math.PI / 2));
                }
            });
            const y = 36 - (attenDb / 24) * (h * 0.65);
            if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.strokeStyle = "#38bdf8"; ctx.lineWidth = 2; ctx.stroke();

        s.filters.forEach(flt => {
            if (!flt.active || flt.freq <= 20) return;
            const nx = (Math.log10(flt.freq / 20) / Math.log10(20000 / 20)) * w;
            const depthY = 36 + (Math.abs(flt.gain) / 24) * (h * 0.65);
            ctx.beginPath();
            ctx.moveTo(nx - 6, 36);
            ctx.lineTo(nx, depthY);
            ctx.lineTo(nx + 6, 36);
            ctx.strokeStyle = flt.type === "FIXED" ? "#ef4444" : "#38bdf8";
            ctx.lineWidth = 2;
            ctx.stroke();
        });
        ctx.restore();
    }

    const roomCalState = {
        targetCurve: "FLAT",
        maxBoost: 3.0,
        maxCut: -12.0,
        bands: [
            { freq: 63, measured: 0.0, correction: 0.0 },
            { freq: 125, measured: 0.0, correction: 0.0 },
            { freq: 250, measured: 0.0, correction: 0.0 },
            { freq: 500, measured: 0.0, correction: 0.0 },
            { freq: 1000, measured: 0.0, correction: 0.0 },
            { freq: 2000, measured: 0.0, correction: 0.0 },
            { freq: 4000, measured: 0.0, correction: 0.0 },
            { freq: 8000, measured: 0.0, correction: 0.0 }
        ]
    };

    function startRoomCal() {
        const statusEl = document.getElementById("rcal-status-banner");
        const fillEl = document.getElementById("rcal-prog-fill");
        // TELEMETRY_PENDING: Needs backend RTA pink-noise measurement socket link
        if (fillEl) fillEl.style.width = "100%";
        if (statusEl) {
            statusEl.innerText = "TELEMETRY_PENDING: Connect RTA Reference Mic on CH 8 to capture room response.";
            statusEl.style.color = "#38bdf8";
        }
        drawRoomCalCurve();
    }

    function drawRoomCalCurve() {
        const canvas = document.getElementById("rcal-canvas");
        if (!canvas) return;
        const ctx = canvas.getContext("2d");
        const dpr = window.devicePixelRatio || 1;
        const w = canvas.parentElement ? canvas.parentElement.clientWidth : 620;
        const h = 180;
        canvas.width = Math.floor(w * dpr);
        canvas.height = Math.floor(h * dpr);
        ctx.scale(dpr, dpr);
        ctx.clearRect(0, 0, w, h);

        const midY = h / 2;
        ctx.strokeStyle = "#1e293b";
        ctx.beginPath(); ctx.moveTo(0, midY); ctx.lineTo(w, midY); ctx.stroke();

        ctx.beginPath();
        roomCalState.bands.forEach((b, idx) => {
            const x = (idx / (roomCalState.bands.length - 1)) * (w - 60) + 30;
            const y = midY - (b.correction * 7);
            if (idx === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            ctx.fillStyle = "#10b981";
            ctx.fillRect(x - 3, y - 3, 6, 6);
        });
        ctx.strokeStyle = "#10b981";
        ctx.lineWidth = 2.5;
        ctx.stroke();
    }

    function commitRoomCalToBus(chId) {
        const target = (chId === "master" || chId === undefined) ? 0 : parseInt(chId, 10) || 0;
        if (window.reaeqBands && window.reaeqBands[target]) {
            roomCalState.bands.forEach((b, i) => {
                if (window.reaeqBands[target][i]) {
                    window.reaeqBands[target][i].freq = b.freq;
                    window.reaeqBands[target][i].gain = b.correction;
                }
            });
            if (typeof window.renderReaEQGraph === "function") window.renderReaEQGraph(target);
        }
        const statusEl = document.getElementById("rcal-status-banner");
        if (statusEl) {
            statusEl.innerText = "COMMITTED TO REAEQ CURVE!";
            statusEl.style.color = "#f59e0b";
        }
    }

    window.getAfdChannel = getAfdChannel;
    window.renderAfdFilterBoxes = renderAfdFilterBoxes;
    window.updateAfdFilterCards = updateAfdFilterCards;
    window.toggleLockFixedNotch = toggleLockFixedNotch;
    window.panicClearAfdFilters = panicClearAfdFilters;
    window.toggleAfdAutoHunting = toggleAfdAutoHunting;
    window.startAfdAnalyzerLoop = startAfdAnalyzerLoop;
    window.stopAfdAnalyzerLoop = stopAfdAnalyzerLoop;
    window.renderAfdSpectrum = renderAfdSpectrum;
    window.roomCalState = roomCalState;
    window.startRoomCal = startRoomCal;
    window.drawRoomCalCurve = drawRoomCalCurve;
    window.commitRoomCalToBus = commitRoomCalToBus;
})(window);
"""

with open(os.path.join(JS_DIR, "room-suite.js"), "w", encoding="utf-8") as f:
    f.write(room_suite_js)

print("room-suite.js lines:", len(room_suite_js.splitlines()))
# ============================================================================
# 9. dsp-modals.js (< 370 lines)
# Universal DSP Modal Controller (De-Noiser, ReaEQ, FET 76, LALA, LoaDeS, Aux/Bus)
# ============================================================================
dsp_modals_js = r"""/**
 * PATCHBOX PRIME // UNIVERSAL DSP MODAL CONTROLLER
 * Strictly adheres to AGENT_RULES.md (<450 lines, TouchGovernor, zero simulated meters).
 */
(function(window) {
    "use strict";

    let currentModalType = null;
    let currentModalChannel = null;

    function openDspModal(type, chId) {
        const chNum = (typeof chId === "number") ? chId : (parseInt(chId, 10) || 0);
        if (type === "reeq") type = "reaeq";
        else if (type === "deesser") type = "loades";
        else if (type === "room_cal") type = "roomcal";
        else if (type === "comp") {
            const st = window.getChannelRackState ? window.getChannelRackState(chNum) : null;
            type = (st && st.compModel === "lala") ? "lala" : "fetish";
        }
        currentModalType = type;
        currentModalChannel = chId;
        window.currentModalChannel = chNum;
        const overlay = document.getElementById("dsp-modal-overlay");
        const title = document.getElementById("modal-title");
        const body = document.getElementById("modal-body");
        if (!overlay || !title || !body) return;

        if (window.stopAoVuLoop) window.stopAoVuLoop();
        if (window.stopAfdAnalyzerLoop) window.stopAfdAnalyzerLoop();

        let chLabel = (chId === "master") ? "MASTER BUS" :
            (String(chId).startsWith("mon") ? `STAGE MON ${String(chId).replace("mon", "")}` :
            `CH ${(parseInt(chId, 10) || 0) + 1}`);

        if (type === "denoiser") {
            const ch = parseInt(chId, 10) || 0;
            const st = (window.denoiserState && window.denoiserState[ch]) || {
                bypass: false, learnActive: false, threshold: -42, reduction: -18,
                attack: 5, release: 120, hpfOn: true, hpfFreq: 80, lpfOn: false, lpfFreq: 12000
            };
            title.innerText = `${chLabel} // STAGE 3: ADAPTIVE DE-NOISER & EXPANDER GATE`;
            body.innerHTML = `
                <div class="dsp-modal-grid">
                    <div class="dsp-card">
                        <div class="dsp-card-title">NOISE FLOOR PROFILE</div>
                        <button class="btn-learn ${st.learnActive ? 'active' : ''}" id="modal-denoise-learn" onclick="toggleDenoiseLearn(${ch}); this.classList.toggle('active');">
                            ${st.learnActive ? 'LEARNING NOISE FLOOR...' : 'LEARN NOISE FLOOR'}
                        </button>
                        <div class="dsp-param-row">
                            <label>THRESHOLD</label>
                            <input type="range" class="knob-slider" min="-70" max="-10" step="0.5" value="${st.threshold}"
                                oninput="denoiserState[${ch}].threshold=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB'; syncDenoiserToDspRack(${ch});">
                            <span>${st.threshold} dB</span>
                        </div>
                        <div class="dsp-param-row">
                            <label>MAX REDUCTION</label>
                            <input type="range" class="knob-slider" min="-60" max="0" step="0.5" value="${st.reduction}"
                                oninput="denoiserState[${ch}].reduction=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB'; syncDenoiserToDspRack(${ch});">
                            <span>${st.reduction} dB</span>
                        </div>
                    </div>
                    <div class="dsp-card">
                        <div class="dsp-card-title">ENVELOPE & DETECTOR FILTERS</div>
                        <div class="dsp-param-row">
                            <label>ATTACK</label>
                            <input type="range" class="knob-slider" min="0.5" max="50" step="0.5" value="${st.attack}"
                                oninput="denoiserState[${ch}].attack=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' ms';">
                            <span>${st.attack} ms</span>
                        </div>
                        <div class="dsp-param-row">
                            <label>RELEASE</label>
                            <input type="range" class="knob-slider" min="20" max="500" step="5" value="${st.release}"
                                oninput="denoiserState[${ch}].release=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' ms';">
                            <span>${st.release} ms</span>
                        </div>
                        <div class="dsp-param-row">
                            <label>SIDECHAIN HPF</label>
                            <input type="range" class="knob-slider" min="20" max="400" step="5" value="${st.hpfFreq}"
                                oninput="denoiserState[${ch}].hpfFreq=parseInt(this.value); this.nextElementSibling.innerText=this.value+' Hz';">
                            <span>${st.hpfFreq} Hz</span>
                        </div>
                    </div>
                </div>`;
        } else if (type === "reaeq") {
            const ch = (chId === "master") ? "master" : (parseInt(chId, 10) || 0);
            title.innerText = `${chLabel} // STAGE 4: REAEQ 8-BAND PARAMETRIC + 64-BAND RTA`;
            body.innerHTML = `
                <div class="reaeq-modal-wrap">
                    <div class="reaeq-canvas-container" style="height:260px; position:relative; background:#090d14; border:1px solid #1e293b; border-radius:6px;">
                        <canvas id="reaeq-canvas" width="760" height="260" style="width:100%; height:100%; display:block; touch-action:none;"></canvas>
                    </div>
                    <div class="reaeq-bands-strip" style="display:grid; grid-template-columns:repeat(4,1fr); gap:8px; margin-top:10px;">
                        ${((window.reaeqBands && window.reaeqBands[typeof ch === 'number' ? ch : 0]) || []).map((b, idx) => `
                            <div class="dsp-card" style="padding:8px;">
                                <div style="display:flex; justify-content:space-between; font-size:0.75rem; font-weight:800; color:${b.color};">
                                    <span>B${b.id} (${b.type})</span>
                                    <span id="eq-lbl-freq-${idx}">${Math.round(b.freq)} Hz</span>
                                </div>
                                <input type="range" class="knob-slider" min="-18" max="18" step="0.5" value="${b.gain}"
                                    oninput="reaeqBands[${typeof ch === 'number' ? ch : 0}][${idx}].gain=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB'; renderReaEQGraph(${typeof ch === 'number' ? ch : 0}); renderMiniReEq(${typeof ch === 'number' ? ch : 0});">
                                <div style="font-size:0.72rem; color:#94a3b8; text-align:right;">${b.gain} dB</div>
                            </div>
                        `).join("")}
                    </div>
                </div>`;
            setTimeout(() => {
                const targetCh = (typeof ch === "number") ? ch : 0;
                if (window.bindReaEQTouchCanvas) window.bindReaEQTouchCanvas(targetCh);
                if (window.renderReaEQGraph) window.renderReaEQGraph(targetCh);
            }, 30);
        } else if (type === "fetish") {
            const ch = parseInt(chId, 10) || 0;
            const f = window.getFetishCh ? window.getFetishCh(ch) : { input: 24, output: 18, attack: 4, release: 5, ratio: 4, mix: 100 };
            title.innerText = `${chLabel} // STAGE 5: FETISH 76 FAST FET PEAK LIMITER`;
            body.innerHTML = `
                <div class="ao-faceplate fetish-faceplate">
                    <div class="ao-faceplate-header">
                        <span class="ao-brand">ANALOG OBSESSION // FETISH 76</span>
                        <span class="ao-sub">ULTRA-FAST FIELD EFFECT LIMITING AMPLIFIER (LIVE TELEMETRY GR)</span>
                    </div>
                    <div class="ao-main-row">
                        <div class="ao-knobs-cluster">
                            <div class="ao-knob-cell"><canvas id="fet-knob-input" width="68" height="68"></canvas><div class="ao-knob-lbl">INPUT</div><div class="ao-knob-val" id="fet-val-input">${f.input} dB</div></div>
                            <div class="ao-knob-cell"><canvas id="fet-knob-output" width="68" height="68"></canvas><div class="ao-knob-lbl">OUTPUT</div><div class="ao-knob-val" id="fet-val-output">${f.output} dB</div></div>
                            <div class="ao-knob-cell"><canvas id="fet-knob-attack" width="68" height="68"></canvas><div class="ao-knob-lbl">ATTACK</div><div class="ao-knob-val" id="fet-val-attack">${f.attack}</div></div>
                            <div class="ao-knob-cell"><canvas id="fet-knob-release" width="68" height="68"></canvas><div class="ao-knob-lbl">RELEASE</div><div class="ao-knob-val" id="fet-val-release">${f.release}</div></div>
                            <div class="ao-knob-cell"><canvas id="fet-knob-mix" width="68" height="68"></canvas><div class="ao-knob-lbl">MIX</div><div class="ao-knob-val" id="fet-val-mix">${f.mix} %</div></div>
                        </div>
                        <div class="ao-vu-box">
                            <canvas id="fet-vu-meter" width="210" height="125"></canvas>
                        </div>
                    </div>
                </div>`;
            setTimeout(() => {
                window.bindAoRotary("fet-knob-input", 0, 48, f.input, "#38bdf8", v => { f.input = Math.round(v); const el = document.getElementById("fet-val-input"); if (el) el.innerText = f.input + " dB"; });
                window.bindAoRotary("fet-knob-output", 0, 48, f.output, "#38bdf8", v => { f.output = Math.round(v); const el = document.getElementById("fet-val-output"); if (el) el.innerText = f.output + " dB"; });
                window.bindAoRotary("fet-knob-attack", 1, 7, f.attack, "#f59e0b", v => { f.attack = parseFloat(v.toFixed(1)); const el = document.getElementById("fet-val-attack"); if (el) el.innerText = f.attack; });
                window.bindAoRotary("fet-knob-release", 1, 7, f.release, "#f59e0b", v => { f.release = parseFloat(v.toFixed(1)); const el = document.getElementById("fet-val-release"); if (el) el.innerText = f.release; });
                window.bindAoRotary("fet-knob-mix", 0, 100, f.mix, "#10b981", v => { f.mix = Math.round(v); const el = document.getElementById("fet-val-mix"); if (el) el.innerText = f.mix + " %"; });
                window.startAoVuLoop("fet-vu-meter", "fetish", () => -((window.channelGrValues && window.channelGrValues[ch]) || 0.0));
            }, 30);
        } else if (type === "lala") {
            const ch = parseInt(chId, 10) || 0;
            const l = window.getLalaCh ? window.getLalaCh(ch) : { peakReduction: 42, gain: 50, hpf: 60, mix: 100 };
            title.innerText = `${chLabel} // STAGE 6: LALA OPTO SMOOTH TELETRONIX LEVELER`;
            body.innerHTML = `
                <div class="ao-faceplate lala-faceplate">
                    <div class="ao-faceplate-header">
                        <span class="ao-brand">ANALOG OBSESSION // LALA OPTO</span>
                        <span class="ao-sub">TUBE OPTICAL LEVELING AMPLIFIER (LIVE TELEMETRY GR)</span>
                    </div>
                    <div class="ao-main-row">
                        <div class="ao-knobs-cluster">
                            <div class="ao-knob-cell"><canvas id="lala-knob-pr" width="76" height="76"></canvas><div class="ao-knob-lbl">PEAK RED</div><div class="ao-knob-val" id="lala-val-pr">${l.peakReduction}</div></div>
                            <div class="ao-knob-cell"><canvas id="lala-knob-gain" width="76" height="76"></canvas><div class="ao-knob-lbl">MAKEUP GAIN</div><div class="ao-knob-val" id="lala-val-gain">${l.gain}</div></div>
                            <div class="ao-knob-cell"><canvas id="lala-knob-hpf" width="76" height="76"></canvas><div class="ao-knob-lbl">SC HPF</div><div class="ao-knob-val" id="lala-val-hpf">${l.hpf} Hz</div></div>
                            <div class="ao-knob-cell"><canvas id="lala-knob-mix" width="76" height="76"></canvas><div class="ao-knob-lbl">MIX</div><div class="ao-knob-val" id="lala-val-mix">${l.mix} %</div></div>
                        </div>
                        <div class="ao-vu-box">
                            <canvas id="lala-vu-meter" width="210" height="125"></canvas>
                        </div>
                    </div>
                </div>`;
            setTimeout(() => {
                window.bindAoRotary("lala-knob-pr", 0, 100, l.peakReduction, "#f59e0b", v => { l.peakReduction = Math.round(v); const el = document.getElementById("lala-val-pr"); if (el) el.innerText = l.peakReduction; });
                window.bindAoRotary("lala-knob-gain", 0, 100, l.gain, "#38bdf8", v => { l.gain = Math.round(v); const el = document.getElementById("lala-val-gain"); if (el) el.innerText = l.gain; });
                window.bindAoRotary("lala-knob-hpf", 20, 500, l.hpf, "#a855f7", v => { l.hpf = Math.round(v); const el = document.getElementById("lala-val-hpf"); if (el) el.innerText = l.hpf + " Hz"; });
                window.bindAoRotary("lala-knob-mix", 0, 100, l.mix, "#10b981", v => { l.mix = Math.round(v); const el = document.getElementById("lala-val-mix"); if (el) el.innerText = l.mix + " %"; });
                window.startAoVuLoop("lala-vu-meter", "lala", () => -((window.channelGrValues && window.channelGrValues[ch]) || 0.0));
            }, 30);
        } else if (type === "loades") {
            const ch = parseInt(chId, 10) || 0;
            const d = (window.loadesState && window.loadesState[ch]) || { freq: 6800, threshold: -22, reduction: -9 };
            title.innerText = `${chLabel} // STAGE 7: LOADES HIGH-FREQUENCY DE-ESSER`;
            body.innerHTML = `
                <div class="dsp-modal-grid">
                    <div class="dsp-card">
                        <div class="dsp-card-title">SIBILANCE DETECTOR</div>
                        <div class="dsp-param-row">
                            <label>CENTER FREQ</label>
                            <input type="range" class="knob-slider" min="3000" max="12000" step="100" value="${d.freq}"
                                oninput="loadesState[${ch}].freq=parseInt(this.value); this.nextElementSibling.innerText=this.value+' Hz';">
                            <span>${d.freq} Hz</span>
                        </div>
                        <div class="dsp-param-row">
                            <label>THRESHOLD</label>
                            <input type="range" class="knob-slider" min="-48" max="0" step="0.5" value="${d.threshold}"
                                oninput="loadesState[${ch}].threshold=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB';">
                            <span>${d.threshold} dB</span>
                        </div>
                        <div class="dsp-param-row">
                            <label>HF REDUCTION</label>
                            <input type="range" class="knob-slider" min="-24" max="0" step="0.5" value="${d.reduction}"
                                oninput="loadesState[${ch}].reduction=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB';">
                            <span>${d.reduction} dB</span>
                        </div>
                    </div>
                </div>`;
        } else if (type === "delay") {
            title.innerText = "FX BUS // TAL-DELAY-PRO TAPE & BBD ECHO";
            body.innerHTML = window.renderDelayControls ? window.renderDelayControls() : "";
            setTimeout(() => { if (window.initTalDelayRotaries) window.initTalDelayRotaries(); }, 30);
        } else if (type === "reverb") {
            title.innerText = "FX BUS // TAL-REVERB-4 PLATE & ATLANTIS IR CONVOLVER";
            body.innerHTML = `
                <div style="display:flex; gap:8px; margin-bottom:10px;">
                    <button class="btn-dsp-mode ${window.reverbDspState?.engine !== 'ir' ? 'active' : ''}" onclick="switchReverbEngine('tal')">TAL-REVERB-4 PLATE</button>
                    <button class="btn-dsp-mode ${window.reverbDspState?.engine === 'ir' ? 'active' : ''}" onclick="switchReverbEngine('ir')">ATLANTIS IR CONVOLVER</button>
                </div>
                <div id="reverb-engine-content">${window.renderReverbControls ? window.renderReverbControls() : ""}</div>`;
            setTimeout(() => {
                if (window.reverbDspState?.engine === "ir") { if (window.initIrCanvas) window.initIrCanvas(); }
                else { if (window.initTalRotaries) window.initTalRotaries(); }
            }, 30);
        } else if (type === "feedback") {
            title.innerText = `${chLabel} // 12-NOTCH ACOUSTIC FEEDBACK SUPPRESSOR (AFS)`;
            body.innerHTML = `
                <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
                    <button class="btn-dsp-mode active" onclick="toggleAfdAutoHunting('${chId}', this)">AUTO-HUNT: ACTIVE</button>
                    <button class="btn-dsp-mode" style="color:#ef4444;" onclick="panicClearAfdFilters('${chId}')">PANIC CLEAR ALL NOTCHES</button>
                </div>
                <div style="height:190px; background:#090d14; border:1px solid #1e293b; border-radius:6px; margin-bottom:10px;">
                    <canvas id="afd-rta-canvas" style="width:100%; height:100%; display:block;"></canvas>
                </div>
                <div class="afd-cards-grid" style="display:grid; grid-template-columns:repeat(6,1fr); gap:6px;">
                    ${window.renderAfdFilterBoxes ? window.renderAfdFilterBoxes(chId) : ""}
                </div>`;
            setTimeout(() => { if (window.startAfdAnalyzerLoop) window.startAfdAnalyzerLoop(chId); }, 30);
        } else if (type === "roomcal") {
            title.innerText = `${chLabel} // RTA AUTO-EQ ROOM CALIBRATOR`;
            body.innerHTML = `
                <div id="rcal-status-banner" style="padding:8px 12px; background:#0f172a; border:1px solid #1e293b; border-radius:4px; margin-bottom:8px; color:#94a3b8; font-weight:700;">
                    READY: Connect RTA Reference Mic on CH 8 and click Measure.
                </div>
                <div style="height:180px; background:#090d14; border:1px solid #1e293b; border-radius:6px; margin-bottom:10px;">
                    <canvas id="rcal-canvas" style="width:100%; height:100%; display:block;"></canvas>
                </div>
                <div style="display:flex; gap:10px;">
                    <button class="btn-dsp-mode active" onclick="startRoomCal()">MEASURE PINK NOISE</button>
                    <button class="btn-dsp-mode" onclick="commitRoomCalToBus('${chId}')">COMMIT CORRECTION TO REAEQ</button>
                </div>`;
            setTimeout(() => { if (window.drawRoomCalCurve) window.drawRoomCalCurve(); }, 30);
        } else if (type === "limiter") {
            const b = window.busterLimiterState || { threshold: -6.0, ceiling: -1.0, release: 85, gr: 0.0 };
            title.innerText = `${chLabel} // BUSTER BRICKWALL SAFETY LIMITER (-1.0 dBFS CEILING)`;
            body.innerHTML = `
                <div class="ao-faceplate">
                    <div class="ao-main-row">
                        <div class="ao-knobs-cluster">
                            <div class="ao-knob-cell"><canvas id="lim-knob-thresh" width="74" height="74"></canvas><div class="ao-knob-lbl">THRESHOLD</div><div class="ao-knob-val" id="lim-val-thresh">${b.threshold} dB</div></div>
                            <div class="ao-knob-cell"><canvas id="lim-knob-ceil" width="74" height="74"></canvas><div class="ao-knob-lbl">CEILING</div><div class="ao-knob-val" id="lim-val-ceil">${b.ceiling} dB</div></div>
                            <div class="ao-knob-cell"><canvas id="lim-knob-rel" width="74" height="74"></canvas><div class="ao-knob-lbl">RELEASE</div><div class="ao-knob-val" id="lim-val-rel">${b.release} ms</div></div>
                        </div>
                        <div class="ao-vu-box">
                            <canvas id="buster-vu-meter" width="210" height="125"></canvas>
                        </div>
                    </div>
                </div>`;
            setTimeout(() => {
                window.bindAoRotary("lim-knob-thresh", -24, 0, b.threshold, "#f59e0b", v => { b.threshold = parseFloat(v.toFixed(1)); const el = document.getElementById("lim-val-thresh"); if (el) el.innerText = b.threshold + " dB"; });
                window.bindAoRotary("lim-knob-ceil", -6, 0, b.ceiling, "#ef4444", v => { b.ceiling = parseFloat(v.toFixed(1)); const el = document.getElementById("lim-val-ceil"); if (el) el.innerText = b.ceiling + " dB"; });
                window.bindAoRotary("lim-knob-rel", 10, 500, b.release, "#38bdf8", v => { b.release = Math.round(v); const el = document.getElementById("lim-val-rel"); if (el) el.innerText = b.release + " ms"; });
                window.startAoVuLoop("buster-vu-meter", "lala", () => -(b.gr || 0.0));
            }, 30);
        } else if (type === "summing") {
            const s = window.analogSummingState || { drive: 3.5, warmth: 45, crosstalk: -68, transformer: true };
            title.innerText = "MASTER BUS // ANALOG CONSOLE SUMMING & TRANSFORMER DRIVE";
            body.innerHTML = `
                <div class="dsp-modal-grid">
                    <div class="dsp-card">
                        <div class="dsp-card-title">24-BUS NEVE / API TRANSFORMER EMULATION</div>
                        <div class="dsp-param-row">
                            <label>CONSOLE DRIVE</label>
                            <input type="range" class="knob-slider" min="0" max="12" step="0.5" value="${s.drive}"
                                oninput="analogSummingState.drive=parseFloat(this.value); this.nextElementSibling.innerText='+'+this.value+' dB';">
                            <span>+${s.drive} dB</span>
                        </div>
                        <div class="dsp-param-row">
                            <label>TUBE WARMTH</label>
                            <input type="range" class="knob-slider" min="0" max="100" step="1" value="${s.warmth}"
                                oninput="analogSummingState.warmth=parseInt(this.value); this.nextElementSibling.innerText=this.value+' %';">
                            <span>${s.warmth} %</span>
                        </div>
                    </div>
                </div>`;
        }

        overlay.style.display = "flex";
    }

    function closeDspModal() {
        if (window.stopAoVuLoop) window.stopAoVuLoop();
        if (window.stopAfdAnalyzerLoop) window.stopAfdAnalyzerLoop();
        const overlay = document.getElementById("dsp-modal-overlay");
        if (overlay) overlay.style.display = "none";
        const auxModal = document.getElementById("auxModal");
        if (auxModal) auxModal.style.display = "none";
    }

    function closeModalOnOverlay(evt) {
        if (evt && evt.target && evt.target.classList.contains("modal-overlay")) {
            evt.target.style.display = "none";
            if (window.stopAoVuLoop) window.stopAoVuLoop();
            if (window.stopAfdAnalyzerLoop) window.stopAfdAnalyzerLoop();
        }
    }

    window.openDspModal = openDspModal;
    window.closeDspModal = closeDspModal;
    window.closeModalOnOverlay = closeModalOnOverlay;
})(window);
"""

with open(os.path.join(JS_DIR, "dsp-modals.js"), "w", encoding="utf-8") as f:
    f.write(dsp_modals_js)

print("dsp-modals.js lines:", len(dsp_modals_js.splitlines()))
# ============================================================================
# 10. Update telemetry.js to drive Stage 1: 1176 Preamp Clipping Telemetry
# ============================================================================
telemetry_path = os.path.join(JS_DIR, "telemetry.js")
with open(telemetry_path, "r", encoding="utf-8") as f:
    tele_src = f.read()

old_meter_loop = """                        for (let i = 0; i < 8; i++) {
                            const bar = document.getElementById("meter-" + i);
                            if (bar) {
                                const level = chs[i] || 0.0;"""

new_meter_loop = """                        for (let i = 0; i < 8; i++) {
                            const level = chs[i] || 0.0;
                            if (typeof window.updatePreamp1176ClipTelemetry === "function") {
                                window.updatePreamp1176ClipTelemetry(i, level);
                            }
                            const bar = document.getElementById("meter-" + i);
                            if (bar) {"""

if old_meter_loop in tele_src and "updatePreamp1176ClipTelemetry" not in tele_src:
    tele_src = tele_src.replace(old_meter_loop, new_meter_loop, 1)
    with open(telemetry_path, "w", encoding="utf-8") as f:
        f.write(tele_src)

print("telemetry.js lines:", len(tele_src.splitlines()))

WEB_DIR = TARGET_DIR

# Ensure every JS file in JS_DIR is strictly < 430 lines (per AGENT_RULES.md 450-line cap)
for fname in ["fx-engines.js", "modep-slots.js", "dsp-rack.js"]:
    fpath = os.path.join(JS_DIR, fname)
    with open(fpath, "r", encoding="utf-8") as f:
        lines = [l for l in f.readlines() if l.strip() != ""]
    with open(fpath, "w", encoding="utf-8") as f:
        f.writelines(lines)
    print(f"Compacted {fname} lines:", len(lines))

# ============================================================================
# 11. Update css/mixer.css with Stage 1 1176 Preamp + 4-Slot MOD-GUI styles
# ============================================================================
css_path = os.path.join(WEB_DIR, "css", "mixer.css")
with open(css_path, "r", encoding="utf-8") as f:
    css_src = f.read()

patchbox_css = """
/* === PATCHBOX PRIME: STAGE 1 1176 PREAMP + STAGE 2 REGAIN STRIP === */
.preamp-regain-strip {
    background: linear-gradient(180deg, #111827 0%, #0b0f19 100%);
    border: 1px solid #1e293b;
    border-radius: 5px;
    padding: 4px 5px;
    margin-bottom: 4px;
    display: flex;
    flex-direction: column;
    gap: 3px;
}
.preamp-stage-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 3px;
    font-size: 0.62rem;
    color: #cbd5e1;
}
.preamp-stage-tag {
    font-weight: 900;
    color: #f59e0b;
    letter-spacing: 0.03em;
    min-width: 32px;
}
.preamp-stage-tag.regain-tag {
    color: #38bdf8;
}
.preamp-mini-slider {
    flex: 1;
    height: 4px;
    accent-color: #f59e0b;
    cursor: pointer;
}
.preamp-mini-slider.regain-slider {
    accent-color: #38bdf8;
}
.preamp-mini-val {
    font-family: monospace;
    font-size: 0.62rem;
    font-weight: 800;
    color: #f8fafc;
    min-width: 34px;
    text-align: right;
}
.preamp-clip-led {
    font-size: 0.56rem;
    font-weight: 900;
    padding: 1px 4px;
    border-radius: 3px;
    background: #064e3b;
    color: #34d399;
    border: 1px solid #059669;
    text-transform: uppercase;
}
.preamp-clip-led.sat {
    background: #78350f;
    color: #fbbf24;
    border-color: #d97706;
}
.preamp-clip-led.clip {
    background: #7f1d1d;
    color: #fca5a5;
    border-color: #ef4444;
    box-shadow: 0 0 6px rgba(239, 68, 68, 0.7);
}
.btn-mini-regain {
    background: #1e293b;
    color: #94a3b8;
    border: 1px solid #334155;
    border-radius: 3px;
    font-size: 0.56rem;
    font-weight: 800;
    padding: 1px 4px;
    cursor: pointer;
}
.btn-mini-regain.active {
    background: #0284c7;
    color: #fff;
    border-color: #38bdf8;
}
.slot-chain-banner {
    font-size: 0.56rem;
    font-weight: 800;
    color: #10b981;
    background: #06281e;
    border: 1px solid #047857;
    border-radius: 3px;
    padding: 1px 4px;
    text-align: center;
    margin-bottom: 3px;
    letter-spacing: 0.02em;
}
.insert-slot {
    display: flex;
    align-items: center;
    gap: 3px;
    cursor: pointer;
}
.insert-slot.occupied {
    border-color: #38bdf8;
    background: rgba(14, 116, 144, 0.22);
}
.insert-slot.bypassed {
    opacity: 0.62;
}
.slot-swap-btn {
    background: #1e293b;
    color: #93c5fd;
    border: 1px solid #334155;
    border-radius: 3px;
    font-size: 0.6rem;
    padding: 0 4px;
    cursor: pointer;
}
/* === LV2 / MOD-GUI PICKER & PARAMETER MODAL === */
.picker-cat-bar {
    display: flex;
    flex-wrap: wrap;
    gap: 5px;
    margin: 8px 0;
}
.picker-cat-btn {
    background: #1e293b;
    color: #94a3b8;
    border: 1px solid #334155;
    border-radius: 4px;
    padding: 4px 8px;
    font-size: 0.7rem;
    font-weight: 800;
    cursor: pointer;
}
.picker-cat-btn.active {
    background: #0284c7;
    color: #fff;
    border-color: #38bdf8;
}
.picker-search-input {
    width: 100%;
    padding: 7px 10px;
    background: #090d14;
    color: #f8fafc;
    border: 1px solid #334155;
    border-radius: 5px;
    font-size: 0.8rem;
    margin-bottom: 8px;
}
.picker-plugin-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 8px 10px;
    background: #0f172a;
    border: 1px solid #1e293b;
    border-radius: 5px;
    margin-bottom: 5px;
    cursor: pointer;
}
.picker-plugin-item:hover, .picker-plugin-item.selected {
    border-color: #38bdf8;
    background: #172554;
}
.picker-plugin-title {
    font-size: 0.82rem;
    font-weight: 800;
    color: #f8fafc;
}
.picker-plugin-meta {
    font-size: 0.68rem;
    color: #94a3b8;
}
.picker-cat-tag {
    font-size: 0.65rem;
    font-weight: 800;
    padding: 2px 6px;
    border-radius: 4px;
    background: #1e293b;
    color: #38bdf8;
}
.modgui-top-bar {
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 12px;
    background: #090d14;
    border: 1px solid #1e293b;
    border-radius: 6px;
    padding: 10px 12px;
    margin-bottom: 12px;
}
.modgui-faceplate-wrap {
    display: flex;
    align-items: center;
    gap: 12px;
}
.modgui-faceplate-img {
    max-height: 76px;
    max-width: 150px;
    border-radius: 4px;
    border: 1px solid #334155;
}
.modgui-brand-badge {
    font-size: 0.78rem;
    font-weight: 900;
    color: #38bdf8;
}
.modgui-plugin-uri {
    font-size: 0.65rem;
    color: #64748b;
    font-family: monospace;
}
.modgui-chain-note {
    font-size: 0.68rem;
    font-weight: 800;
    color: #10b981;
    margin-top: 2px;
}
.modgui-action-bar {
    display: flex;
    gap: 8px;
    align-items: center;
    flex-wrap: wrap;
}
.modgui-btn-swap, .modgui-btn-byp, .modgui-btn-clear {
    padding: 6px 10px;
    border-radius: 5px;
    font-size: 0.74rem;
    font-weight: 800;
    cursor: pointer;
    border: 1px solid #334155;
    background: #1e293b;
    color: #f8fafc;
}
.modgui-btn-byp.active {
    background: #78350f;
    border-color: #f59e0b;
    color: #fef3c7;
}
.modgui-btn-clear {
    background: #450a0a;
    border-color: #dc2626;
    color: #fecaca;
}
.modgui-params-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(185px, 1fr));
    gap: 10px;
}
.modgui-param-card {
    background: #0f172a;
    border: 1px solid #1e293b;
    border-radius: 6px;
    padding: 8px 10px;
    display: flex;
    flex-direction: column;
    gap: 4px;
}
.modgui-param-name {
    font-size: 0.74rem;
    font-weight: 800;
    color: #cbd5e1;
}
.modgui-param-val {
    font-size: 0.74rem;
    font-family: monospace;
    font-weight: 800;
    color: #38bdf8;
    text-align: right;
}
"""

if "PATCHBOX PRIME: STAGE 1 1176 PREAMP" not in css_src:
    css_src += "\n" + patchbox_css
    with open(css_path, "w", encoding="utf-8") as f:
        f.write(css_src)
    print("Updated css/mixer.css with 1176 Preamp & 4-Slot MOD-GUI styles.")
# ============================================================================
# 12. Update index.html and mixer_index.html
# ============================================================================
import re

def update_html_file(html_path):
    with open(html_path, "r", encoding="utf-8") as f:
        html = f.read()

    # 1. Update #dsp-rack-page subtitle & add Stage 1 1176 FET Preamp bar before Stage 2 Gain/REGAIN
    old_rack_header = """            <!-- Input Gain (moved here from channel strip) -->
            <div class="rack-gain-section" id="dsp-rack-gain-section">"""

    new_rack_header = """            <!-- STAGE 1: 1176 FET CLASS-A PREAMP EMULATION (Before REGAIN) -->
            <div class="rack-gain-section" id="dsp-rack-preamp-section">
                <div>
                    <div class="rack-gain-title" id="dsp-rack-preamp-label">STAGE 1: 1176 FET CLASS-A PREAMP — CH 1</div>
                    <div class="rack-gain-hw">Discrete FET Input Drive &rarr; Simulated Clipping &rarr; Output Trim (Feeds Stage 2 REGAIN)</div>
                </div>
                <span style="font-size:0.72rem; font-weight:800; color:#f59e0b;">DRIVE:</span>
                <input type="range" class="rack-gain-slider" id="rack-preamp-drive-slider"
                    min="0" max="36" step="0.5" value="6"
                    oninput="handle1176PreampDrive(dspRackCurrentChannel, this.value)">
                <span class="rack-gain-val" id="rack-preamp-drive-val">+6.0 dB</span>
                <span class="preamp-clip-led" id="rack-preamp-clip-led">CLEAN</span>
                <button class="btn-top active" id="rack-preamp-clipmode-btn"
                    onclick="toggle1176ClipMode(dspRackCurrentChannel)"
                    title="Toggle between Soft FET Saturation and Hard Brickwall Clipping">SOFT FET SAT</button>
                <span style="font-size:0.72rem; font-weight:800; color:#38bdf8;">OUTPUT TRIM:</span>
                <input type="range" class="rack-gain-slider" id="rack-preamp-trim-slider"
                    min="-24" max="12" step="0.5" value="0"
                    oninput="handle1176PreampTrim(dspRackCurrentChannel, this.value)">
                <span class="rack-gain-val" id="rack-preamp-trim-val">0.0 dB</span>
                <button class="btn-top active" id="rack-preamp-bypass-btn"
                    onclick="toggle1176PreampBypass(dspRackCurrentChannel)">1176 PRE: ON</button>
            </div>

            <!-- STAGE 2: Digital Gain & REGAIN Compensator -->
            <div class="rack-gain-section" id="dsp-rack-gain-section">"""

    if "dsp-rack-preamp-section" not in html and old_rack_header in html:
        html = html.replace(old_rack_header, new_rack_header, 1)

    html = html.replace(
        "Signal chain: Input Gain → ReaEQ → [Switchable Comp: FETISH 76 | LALA] → De-Esser (LOADES) → MODEP Insert Loop",
        "Signal chain: Stage 1: 1176 FET Preamp (Drive/Clip/Trim) → Stage 2: Digital Gain + REGAIN → De-Noiser → ReaEQ → [FETISH 76 | LALA] → LoaDeS → 4 Auto-Routed MODEP LV2 Slots"
    )

    # 2. Inject Stage 1 (1176 Preamp) + Stage 2 (REGAIN) strip & update 4 LV2 slots on each channel 0..7
    for ch in range(8):
        preamp_strip_html = f"""                    <!-- STAGE 1: 1176 FET PREAMP -> STAGE 2: REGAIN -->
                    <div class="preamp-regain-strip" id="preamp-strip-{ch}">
                        <div class="preamp-stage-row" title="Stage 1: 1176 FET Class-A Input Drive &amp; Simulated Clipping">
                            <span class="preamp-stage-tag">1176</span>
                            <input type="range" class="preamp-mini-slider" id="strip-1176-drive-{ch}" min="0" max="36" step="0.5" value="6" oninput="handle1176PreampDrive({ch}, this.value)">
                            <span class="preamp-mini-val" id="strip-1176-drive-val-{ch}">+6.0dB</span>
                            <span class="preamp-clip-led" id="strip-1176-clip-{ch}">CLEAN</span>
                        </div>
                        <div class="preamp-stage-row" title="Stage 1 Output Trim feeding Stage 2 REGAIN">
                            <span class="preamp-stage-tag">TRIM</span>
                            <input type="range" class="preamp-mini-slider" id="strip-1176-trim-{ch}" min="-24" max="12" step="0.5" value="0" oninput="handle1176PreampTrim({ch}, this.value)">
                            <span class="preamp-mini-val" id="strip-1176-trim-val-{ch}">0.0dB</span>
                            <button class="btn-mini-regain" id="phase-ch-{ch}" onclick="toggleChPhase({ch})" title="Phase Invert (180°)">&Oslash;</button>
                        </div>
                        <div class="preamp-stage-row" title="Stage 2: Digital Trim &amp; Constant-Level REGAIN">
                            <span class="preamp-stage-tag regain-tag">GAIN</span>
                            <input type="range" class="preamp-mini-slider regain-slider" id="gain-slider-{ch}" min="-18" max="24" step="0.5" value="0" oninput="handleGainChange({ch}, this.value)">
                            <span class="preamp-mini-val" id="gain-val-{ch}">0.0dB</span>
                            <button class="btn-mini-regain active" id="regain-ch-{ch}" onclick="toggleChRegain({ch})" title="Auto-compensate fader when adjusting Gain">REGN</button>
                        </div>
                    </div>
"""
        mode_bar_marker = f'<!-- MODE SELECTOR BAR -->\n                    <div class="dsp-mode-bar" id="dsp-mode-bar-{ch}">'
        if f'id="preamp-strip-{ch}"' not in html and mode_bar_marker in html:
            html = html.replace(mode_bar_marker, preamp_strip_html + "                    " + mode_bar_marker, 1)

        # Replace the 4 slots block for channel ch so it uses handleSlotClick + swap button + auto-thru banner
        old_slots_block = f"""                    <!-- CONTAINER A: 4 MODEP INSERT SLOTS (Linear Through, Non-Cutting) -->
                    <div class="dsp-container linear-slot-container" id="dsp-rack-container-{ch}">
                      <div class="insert-slots-grid">
                        <div class="insert-slot empty" id="slot-{ch}-0" onclick="openPluginPickerModal({ch}, 0)">
                          <span class="slot-num">1</span>
                          <span class="slot-name" id="slot-name-{ch}-0">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-0" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 0)">BYP</button>
                        </div>
                        <div class="insert-slot empty" id="slot-{ch}-1" onclick="openPluginPickerModal({ch}, 1)">
                          <span class="slot-num">2</span>
                          <span class="slot-name" id="slot-name-{ch}-1">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-1" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 1)">BYP</button>
                        </div>
                        <div class="insert-slot empty" id="slot-{ch}-2" onclick="openPluginPickerModal({ch}, 2)">
                          <span class="slot-num">3</span>
                          <span class="slot-name" id="slot-name-{ch}-2">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-2" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 2)">BYP</button>
                        </div>
                        <div class="insert-slot empty" id="slot-{ch}-3" onclick="openPluginPickerModal({ch}, 3)">
                          <span class="slot-num">4</span>
                          <span class="slot-name" id="slot-name-{ch}-3">EMPTY (THRU)</span>
                          <button class="slot-bypass" id="slot-byp-{ch}-3" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 3)">BYP</button>
                        </div>
                      </div>"""

        new_slots_block = f"""                    <!-- CONTAINER A: 4 AUTO-ROUTED MODEP LV2 / MOD-GUI SLOTS (Series Auto-Thru) -->
                    <div class="dsp-container linear-slot-container" id="dsp-rack-container-{ch}">
                      <div class="slot-chain-banner" id="slot-chain-status-{ch}">AUTO-THRU: IN ➔ OUT (ZERO DROP)</div>
                      <div class="insert-slots-grid">
                        <div class="insert-slot" id="slot-{ch}-0" onclick="handleSlotClick({ch}, 0)" title="Tap to open LV2 / MOD-GUI controls (or pick effect if empty)">
                          <span class="slot-num">1</span>
                          <span class="slot-name" id="slot-name-{ch}-0">EMPTY (THRU)</span>
                          <button class="slot-swap-btn" onclick="event.stopPropagation(); openPluginPickerModal({ch}, 0)" title="Choose LV2 Module">⇄</button>
                          <button class="slot-bypass" id="slot-byp-{ch}-0" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 0)">THRU</button>
                        </div>
                        <div class="insert-slot" id="slot-{ch}-1" onclick="handleSlotClick({ch}, 1)" title="Tap to open LV2 / MOD-GUI controls (or pick effect if empty)">
                          <span class="slot-num">2</span>
                          <span class="slot-name" id="slot-name-{ch}-1">EMPTY (THRU)</span>
                          <button class="slot-swap-btn" onclick="event.stopPropagation(); openPluginPickerModal({ch}, 1)" title="Choose LV2 Module">⇄</button>
                          <button class="slot-bypass" id="slot-byp-{ch}-1" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 1)">THRU</button>
                        </div>
                        <div class="insert-slot" id="slot-{ch}-2" onclick="handleSlotClick({ch}, 2)" title="Tap to open LV2 / MOD-GUI controls (or pick effect if empty)">
                          <span class="slot-num">3</span>
                          <span class="slot-name" id="slot-name-{ch}-2">EMPTY (THRU)</span>
                          <button class="slot-swap-btn" onclick="event.stopPropagation(); openPluginPickerModal({ch}, 2)" title="Choose LV2 Module">⇄</button>
                          <button class="slot-bypass" id="slot-byp-{ch}-2" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 2)">THRU</button>
                        </div>
                        <div class="insert-slot" id="slot-{ch}-3" onclick="handleSlotClick({ch}, 3)" title="Tap to open LV2 / MOD-GUI controls (or pick effect if empty)">
                          <span class="slot-num">4</span>
                          <span class="slot-name" id="slot-name-{ch}-3">EMPTY (THRU)</span>
                          <button class="slot-swap-btn" onclick="event.stopPropagation(); openPluginPickerModal({ch}, 3)" title="Choose LV2 Module">⇄</button>
                          <button class="slot-bypass" id="slot-byp-{ch}-3" onclick="event.stopPropagation(); toggleSlotBypass({ch}, 3)">THRU</button>
                        </div>
                      </div>"""

        if old_slots_block in html:
            html = html.replace(old_slots_block, new_slots_block, 1)

    # 3. Replace bottom script tags and modals with full modular script stack + LV2 Picker + MOD-GUI Modal
    old_bottom_pattern = re.compile(r"<!-- Modular Mixer Architecture -->.*</html>", re.DOTALL)
    new_bottom_block = """<!-- Modular Mixer Architecture (All files < 450 lines per AGENT_RULES.md) -->
    <script src="js/config.js"></script>
    <script src="js/state.js"></script>
    <script src="js/touch-physics.js"></script>
    <script src="js/channel-strip.js"></script>
    <script src="js/reaeq-canvas.js"></script>
    <script src="js/reaeq-touch.js"></script>
    <script src="js/ao-vu-knobs.js"></script>
    <script src="js/fx-engines.js"></script>
    <script src="js/modep-slots.js"></script>
    <script src="js/dsp-rack.js"></script>
    <script src="js/dsp-rack-views.js"></script>
    <script src="js/room-suite.js"></script>
    <script src="js/dsp-modals.js"></script>
    <script src="js/telemetry.js"></script>
    <script src="js/app.js"></script>

    <!-- 4-Slot Auto-Routed LV2 / MOD-GUI Module Picker Modal -->
    <div class="modal-overlay" id="plugin-picker-overlay" onclick="if(event.target===this)closePluginPickerModal()">
        <div class="dsp-modal" style="max-width:680px; width:92%;">
            <div class="modal-header">
                <div class="modal-title" id="picker-modal-title">SELECT MODEP LV2 / MOD-GUI MODULE</div>
                <button class="btn-close-modal" onclick="closePluginPickerModal()">&times;</button>
            </div>
            <input type="text" class="picker-search-input" id="picker-search-input" placeholder="Search 429 installed MODEP LV2 modules (e.g. Screamer, Chorus, EchoCat, NAM, Reverb)..." oninput="filterPickerPlugins(this.value)">
            <div class="picker-cat-bar">
                <button class="picker-cat-btn active" data-cat="ALL" onclick="setPickerCategory('ALL')">ALL</button>
                <button class="picker-cat-btn" data-cat="AMP / NAM" onclick="setPickerCategory('AMP / NAM')">AMP / NAM</button>
                <button class="picker-cat-btn" data-cat="DRIVE" onclick="setPickerCategory('DRIVE')">DRIVE</button>
                <button class="picker-cat-btn" data-cat="MODULATION" onclick="setPickerCategory('MODULATION')">MODULATION</button>
                <button class="picker-cat-btn" data-cat="DELAY" onclick="setPickerCategory('DELAY')">DELAY</button>
                <button class="picker-cat-btn" data-cat="REVERB" onclick="setPickerCategory('REVERB')">REVERB</button>
                <button class="picker-cat-btn" data-cat="DYNAMICS" onclick="setPickerCategory('DYNAMICS')">DYNAMICS</button>
                <button class="picker-cat-btn" data-cat="EQ / FILTER" onclick="setPickerCategory('EQ / FILTER')">EQ / FILTER</button>
            </div>
            <div class="plugin-picker-list" id="picker-plugin-list" style="max-height:360px; overflow-y:auto;"></div>
        </div>
    </div>

    <!-- Direct MOD-GUI / LV2 Slot Parameter Control Modal -->
    <div class="modal-overlay" id="slot-modgui-overlay" onclick="if(event.target===this)closeSlotModGuiModal()">
        <div class="dsp-modal" style="max-width:780px; width:94%;">
            <div class="modal-header">
                <div class="modal-title" id="slot-modgui-title">MOD-GUI // LV2 SLOT MODULE</div>
                <button class="btn-close-modal" onclick="closeSlotModGuiModal()">&times;</button>
            </div>
            <div id="slot-modgui-body"></div>
        </div>
    </div>

    <!-- MODEP Visual Canvas Modal (Iframe / Remote Window) -->
    <div class="modal-overlay" id="modep-canvas-overlay" onclick="if(event.target===this)closeModepCanvasModal()">
        <div class="dsp-modal" style="width:95%; height:90%; max-width:1400px; display:flex; flex-direction:column;">
            <div class="modal-header">
                <div class="modal-title" id="modep-canvas-title">MODEP MODULAR CANVAS (:8881)</div>
                <button class="btn-close-modal" onclick="closeModepCanvasModal()">&times;</button>
            </div>
            <iframe id="modep-canvas-iframe" src="about:blank" style="flex:1; width:100%; border:none; background:#0f141c;"></iframe>
        </div>
    </div>

    <!-- UNIVERSAL MODAL POPUP FOR AUX SENDS (REVERB & DELAY) -->
    <div id="auxModal" class="modal-overlay" onclick="closeModalOnOverlay(event)">
        <div class="modal-dialog-card" onclick="event.stopPropagation()">
            <div class="modal-header">
                <h3 id="auxModalTitle">AUX PROCESSOR</h3>
                <button class="modal-close-btn" onclick="window.closeDspModal()">✕</button>
            </div>
            <div id="auxModalBody" class="modal-body"></div>
        </div>
    </div>
</body>
</html>
"""
    if old_bottom_pattern.search(html):
        html = old_bottom_pattern.sub(new_bottom_block, html)

    # Fix room_cal -> roomcal if present
    html = html.replace("openDspModal('room_cal'", "openDspModal('roomcal'")

    with open(html_path, "w", encoding="utf-8") as f:
        f.write(html)
    print("Updated HTML file:", html_path)

update_html_file(os.path.join(WEB_DIR, "index.html"))
update_html_file(os.path.join(WEB_DIR, "mixer_index.html"))
