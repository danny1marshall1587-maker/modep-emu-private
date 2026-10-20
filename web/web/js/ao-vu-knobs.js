// ao-vu-knobs.js - Authentic Analog Obsession Bakelite Knobs, VU Meters & Hardware State
(function(window) {
    'use strict';
        const lalaState = {
            gain: 38,
            peakReduction: 45,
            hpf: 60,
            mf: 1200,
            mg: 0,
            hfFlat: 80,
            mix: 100,
            ext: false,
            limitMode: false,
            power: true
        };
        const fetish76State = {
            input: 30,
            output: 24,
            attack: 3,
            release: 5,
            ratio: '4',
            allButtons: false,
            power: true
        };
        const busterLimiterState = {
            threshold: -1.0,
            ceiling: -0.2,
            release: 25,
            attack: 10,
            drive: 0.0,
            crush: false,
            power: true
        };
        const analogSummingState = {
            console: 'neve',
            drive: 2.5,
            warmth: 3.0,
            crosstalk: -60,
            ironSat: true,
            enabled: true
        };
        let aoVuMeterAnimId = null;
        // Render Bakelite Rotary Knobs with realistic fluting, metal pointers and dial ticks
        function drawAoKnob(canvasId, normVal, style = 'lala', isLarge = false) {
            const cvs = document.getElementById(canvasId);
            if (!cvs) return;
            const ctx = cvs.getContext('2d');
            const w = cvs.width;
            const h = cvs.height;
            const cx = w / 2;
            const cy = h / 2;
            const radius = Math.min(cx, cy) - (isLarge ? 7 : 5);
            ctx.clearRect(0, 0, w, h);
            // Graduation tick arc (-135 deg to +135 deg)
            const startAngle = 0.75 * Math.PI;
            const endAngle = 2.25 * Math.PI;
            const currentAngle = startAngle + normVal * (endAngle - startAngle);
            const numTicks = isLarge ? 21 : 11;
            for (let i = 0; i < numTicks; i++) {
                const angle = startAngle + (i / (numTicks - 1)) * (endAngle - startAngle);
                const isMajor = (i % 2 === 0);
                const rOuter = radius + (isLarge ? 6 : 4);
                const rInner = radius + (isMajor ? (isLarge ? 1 : 1) : (isLarge ? 3 : 2));
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(angle) * rInner, cy + Math.sin(angle) * rInner);
                ctx.lineTo(cx + Math.cos(angle) * rOuter, cy + Math.sin(angle) * rOuter);
                ctx.strokeStyle = style === 'lala' ? '#44403c' : '#64748b';
                ctx.lineWidth = isMajor ? 1.5 : 1;
                ctx.stroke();
            }
            // Outer drop shadow
            ctx.beginPath();
            ctx.arc(cx, cy + 2, radius, 0, 2 * Math.PI);
            ctx.fillStyle = 'rgba(0,0,0,0.5)';
            ctx.fill();
            // Outer fluted skirt
            ctx.beginPath();
            ctx.arc(cx, cy, radius, 0, 2 * Math.PI);
            const skirtGrad = ctx.createRadialGradient(cx - 3, cy - 3, 2, cx, cy, radius);
            if (style === 'lala') {
                skirtGrad.addColorStop(0, '#292524');
                skirtGrad.addColorStop(0.85, '#1c1917');
                skirtGrad.addColorStop(1, '#0c0a09');
            } else if (style === 'fetish') {
                skirtGrad.addColorStop(0, '#38bdf8');
                skirtGrad.addColorStop(0.3, '#0f172a');
                skirtGrad.addColorStop(1, '#020617');
            } else {
                skirtGrad.addColorStop(0, '#475569');
                skirtGrad.addColorStop(0.85, '#1e293b');
                skirtGrad.addColorStop(1, '#0f172a');
            }
            ctx.fillStyle = skirtGrad;
            ctx.fill();
            ctx.strokeStyle = style === 'lala' ? '#57534e' : '#334155';
            ctx.lineWidth = 1.5;
            ctx.stroke();
            // Fluted scalloped ridges around rim
            const numFlutes = isLarge ? 24 : 16;
            ctx.strokeStyle = 'rgba(255,255,255,0.08)';
            ctx.lineWidth = 1;
            for (let f = 0; f < numFlutes; f++) {
                const fAngle = (f / numFlutes) * 2 * Math.PI;
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(fAngle) * (radius - 3), cy + Math.sin(fAngle) * (radius - 3));
                ctx.lineTo(cx + Math.cos(fAngle) * radius, cy + Math.sin(fAngle) * radius);
                ctx.stroke();
            }
            // Inner metal cap / bakelite top disc
            const innerRadius = radius * (isLarge ? 0.68 : 0.62);
            ctx.beginPath();
            ctx.arc(cx, cy, innerRadius, 0, 2 * Math.PI);
            const innerGrad = ctx.createRadialGradient(cx - 2, cy - 2, 1, cx, cy, innerRadius);
            if (style === 'lala') {
                innerGrad.addColorStop(0, '#e7e5e4');
                innerGrad.addColorStop(0.7, '#a8a29e');
                innerGrad.addColorStop(1, '#44403c');
            } else if (style === 'fetish') {
                innerGrad.addColorStop(0, '#0284c7');
                innerGrad.addColorStop(0.5, '#0369a1');
                innerGrad.addColorStop(1, '#082f49');
            } else {
                innerGrad.addColorStop(0, '#cbd5e1');
                innerGrad.addColorStop(0.7, '#64748b');
                innerGrad.addColorStop(1, '#1e293b');
            }
            ctx.fillStyle = innerGrad;
            ctx.fill();
            ctx.strokeStyle = 'rgba(0,0,0,0.6)';
            ctx.lineWidth = 1;
            ctx.stroke();
            // Indicator pointer line / dot
            const pLength = radius - (isLarge ? 3 : 2);
            const px = cx + Math.cos(currentAngle) * pLength;
            const py = cy + Math.sin(currentAngle) * pLength;
            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(px, py);
            ctx.strokeStyle = (style === 'lala') ? '#ffffff' : (style === 'fetish' ? '#38bdf8' : '#f8fafc');
            ctx.lineWidth = isLarge ? 3.0 : 2.0;
            ctx.lineCap = 'round';
            ctx.stroke();
            // Center rivet
            ctx.beginPath();
            ctx.arc(cx, cy, isLarge ? 3.5 : 2.5, 0, 2 * Math.PI);
            ctx.fillStyle = '#1c1917';
            ctx.fill();
        }
        const _vuBgCache = {};
        let _aoVuSmoothDb = 0.0;
        // Draw illuminated analog VU meter with cached static dial + 60FPS swinging needle
        function renderAoVuMeter(canvasId, grDb = 0, style = 'lala') {
            const cvs = document.getElementById(canvasId);
            if (!cvs) return;
            const ctx = cvs.getContext('2d');
            const w = cvs.width;
            const h = cvs.height;
            const pivotX = w / 2;
            const pivotY = h * 1.35;
            const arcRadius = h * 1.05;
            const minAngle = -Math.PI * 0.30;
            const maxAngle = Math.PI * 0.30;
            function dbToAngle(db) {
                const clamped = Math.max(-20, Math.min(3, db));
                const norm = (clamped + 20) / 23;
                return minAngle + norm * (maxAngle - minAngle) - Math.PI / 2;
            }
            const cacheKey = `${w}x${h}_${style}`;
            if (!_vuBgCache[cacheKey]) {
                const off = document.createElement('canvas');
                off.width = w;
                off.height = h;
                const bctx = off.getContext('2d');
                const bgGrad = bctx.createRadialGradient(w / 2, h * 0.95, 10, w / 2, h * 0.95, w * 0.7);
                if (style === 'lala') {
                    bgGrad.addColorStop(0, '#fef08a');
                    bgGrad.addColorStop(0.4, '#fed7aa');
                    bgGrad.addColorStop(0.85, '#f59e0b');
                    bgGrad.addColorStop(1, '#b45309');
                } else {
                    bgGrad.addColorStop(0, '#bfdbfe');
                    bgGrad.addColorStop(0.4, '#93c5fd');
                    bgGrad.addColorStop(0.85, '#3b82f6');
                    bgGrad.addColorStop(1, '#1d4ed8');
                }
                bctx.fillStyle = bgGrad;
                bctx.fillRect(0, 0, w, h);
                const marks = [
                    { db: -20, label: '20' }, { db: -10, label: '10' }, { db: -7, label: '7' },
                    { db: -5, label: '5' }, { db: -3, label: '3' }, { db: -2, label: '2' },
                    { db: -1, label: '1' }, { db: 0, label: '0' }, { db: 1, label: '+1' },
                    { db: 2, label: '+2' }, { db: 3, label: '+3' }
                ];
                bctx.beginPath();
                bctx.arc(pivotX, pivotY, arcRadius, minAngle - Math.PI / 2, maxAngle - Math.PI / 2);
                bctx.strokeStyle = '#292524';
                bctx.lineWidth = 1.5;
                bctx.stroke();
                bctx.beginPath();
                const zeroAngle = dbToAngle(0);
                bctx.arc(pivotX, pivotY, arcRadius, zeroAngle, maxAngle - Math.PI / 2);
                bctx.strokeStyle = '#dc2626';
                bctx.lineWidth = 3.5;
                bctx.stroke();
                bctx.font = 'bold 9px sans-serif';
                bctx.textAlign = 'center';
                marks.forEach(m => {
                    const angle = dbToAngle(m.db);
                    const r1 = arcRadius - 2;
                    const r2 = arcRadius + 6;
                    const tx = pivotX + Math.cos(angle) * (arcRadius + 14);
                    const ty = pivotY + Math.sin(angle) * (arcRadius + 14);
                    bctx.beginPath();
                    bctx.moveTo(pivotX + Math.cos(angle) * r1, pivotY + Math.sin(angle) * r1);
                    bctx.lineTo(pivotX + Math.cos(angle) * r2, pivotY + Math.sin(angle) * r2);
                    bctx.strokeStyle = m.db > 0 ? '#dc2626' : '#1c1917';
                    bctx.lineWidth = 1.2;
                    bctx.stroke();
                    bctx.fillStyle = m.db > 0 ? '#dc2626' : '#1c1917';
                    bctx.fillText(m.label, tx, ty);
                });
                bctx.font = '900 11px sans-serif';
                bctx.fillStyle = style === 'lala' ? '#451a03' : '#1e3a8a';
                bctx.fillText('VU', pivotX, h * 0.48);
                bctx.font = 'bold 7px sans-serif';
                bctx.fillText('GAIN REDUCTION (dB)', pivotX, h * 0.58);
                _vuBgCache[cacheKey] = off;
            }
            ctx.drawImage(_vuBgCache[cacheKey], 0, 0);
            // Compute current needle angle (grDb is negative or 0)
            const needleDb = Math.max(-20, Math.min(3, grDb));
            const needleAngle = dbToAngle(needleDb);
            // Needle shadow
            ctx.beginPath();
            ctx.moveTo(pivotX + 2, pivotY + 2);
            ctx.lineTo(pivotX + Math.cos(needleAngle) * (arcRadius + 8) + 2, pivotY + Math.sin(needleAngle) * (arcRadius + 8) + 2);
            ctx.strokeStyle = 'rgba(0,0,0,0.35)';
            ctx.lineWidth = 2.0;
            ctx.stroke();
            // Needle
            ctx.beginPath();
            ctx.moveTo(pivotX, pivotY);
            ctx.lineTo(pivotX + Math.cos(needleAngle) * (arcRadius + 8), pivotY + Math.sin(needleAngle) * (arcRadius + 8));
            ctx.strokeStyle = '#1c1917';
            ctx.lineWidth = 1.5;
            ctx.stroke();
            // Needle Pivot Cap
            ctx.beginPath();
            ctx.arc(pivotX, pivotY, 18, 0, 2 * Math.PI);
            ctx.fillStyle = '#292524';
            ctx.fill();
            ctx.strokeStyle = '#1c1917';
            ctx.lineWidth = 2;
            ctx.stroke();
            // Glass reflection highlight
            ctx.beginPath();
            ctx.ellipse(w * 0.35, 18, w * 0.35, 12, -0.15, 0, 2 * Math.PI);
            ctx.fillStyle = 'rgba(255,255,255,0.22)';
            ctx.fill();
        }
        // 60FPS Analog VU Needle Loop driven by live WebSocket GR telemetry
        function startAoVuLoop(meterId, style = 'lala', isLimiter = false, customTargetGetter = null) {
            if (aoVuMeterAnimId) {
                cancelAnimationFrame(aoVuMeterAnimId);
                aoVuMeterAnimId = null;
            }
            _aoVuSmoothDb = 0.0;
            function stepVu60Fps() {
                const cvs = document.getElementById(meterId);
                if (!cvs) {
                    aoVuMeterAnimId = null;
                    return;
                }
                let rawGrPos = 0.0;
                if (typeof customTargetGetter === 'function') {
                    rawGrPos = Number(customTargetGetter()) || 0.0;
                } else if (typeof window.getTargetLiveGr === 'function') {
                    rawGrPos = Number(window.getTargetLiveGr(window.currentCompModalTarget)) || 0.0;
                }
                const targetDb = -Math.max(0, Math.min(20, Math.abs(rawGrPos)));
                if (targetDb < _aoVuSmoothDb) {
                    _aoVuSmoothDb = _aoVuSmoothDb * 0.42 + targetDb * 0.58; // Fast attack deflection
                } else {
                    _aoVuSmoothDb = _aoVuSmoothDb * 0.78 + targetDb * 0.22; // Snappy 60FPS release
                }
                renderAoVuMeter(meterId, _aoVuSmoothDb, style);
                if (!customTargetGetter && typeof window.updateDigitalGrBar === 'function') {
                    window.updateDigitalGrBar('comp-digital-gr-bar', Math.abs(_aoVuSmoothDb));
                }
                aoVuMeterAnimId = requestAnimationFrame(stepVu60Fps);
            }
            aoVuMeterAnimId = requestAnimationFrame(stepVu60Fps);
        }
        function bindAoRotary(canvasId, readoutId, min, max, unit, getter, setter, style = 'lala', isLarge = false) {
            const cvs = document.getElementById(canvasId);
            if (!cvs || cvs.dataset.bound) return;
            cvs.dataset.bound = 'true';
            cvs.style.touchAction = 'none';
            const defaultRawVal = getter();
            const initNorm = Math.max(0, Math.min(1, (defaultRawVal - min) / (max - min)));
            drawAoKnob(canvasId, initNorm, style, isLarge);
            let startX = 0, startY = 0, startVal = initNorm, lastTapMs = 0;

            cvs._refreshAoKnob = () => {
                const cur = Number(getter()) || 0;
                const n = Math.max(0, Math.min(1, (cur - min) / (max - min)));
                drawAoKnob(canvasId, n, style, isLarge);
                const readout = document.getElementById(readoutId);
                if (readout) {
                    readout.innerText = (unit === ' Hz' && cur >= 1000)
                        ? (cur / 1000).toFixed(1) + ' kHz'
                        : (Number.isInteger(min) && Number.isInteger(max) ? Math.round(cur) : cur.toFixed(1)) + unit;
                }
            };

            const applyNorm = (newNorm) => {
                newNorm = Math.max(0, Math.min(1, newNorm));
                let actualVal = min + newNorm * (max - min);
                setter(actualVal);
                cvs._refreshAoKnob();
            };

            cvs.addEventListener('pointerdown', (e) => {
                e.preventDefault();
                const now = Date.now();
                if (now - lastTapMs < 280) {
                    applyNorm((defaultRawVal - min) / (max - min));
                    lastTapMs = 0;
                    return;
                }
                lastTapMs = now;
                startX = e.clientX;
                startY = e.clientY;
                startVal = (getter() - min) / (max - min);
                try { cvs.setPointerCapture(e.pointerId); } catch (_) {}
                const onMove = (ev) => {
                    if (ev.pointerId !== undefined && ev.pointerId !== e.pointerId) return;
                    const dy = startY - ev.clientY;
                    const dx = ev.clientX - startX;
                    const delta = dy + dx; // Omni-axis: Up/Right/Diagonal-Up-Right increases, Down/Left/Diagonal-Down-Left decreases
                    const curDamp = window.maxSpeedDamping || 1;
                    const fineScale = (window.activeTouchCount && window.activeTouchCount >= 2) ? 0.25 : 1.0;
                    const speedCoeff = (curDamp <= 3 ? 0.0055 : (0.0055 / (1 + (curDamp - 3) * 0.45))) * fineScale;
                    applyNorm(startVal + delta * speedCoeff);
                };
                const onUp = (ev) => {
                    if (ev && ev.pointerId !== undefined && ev.pointerId !== e.pointerId) return;
                    window.removeEventListener('pointermove', onMove);
                    window.removeEventListener('pointerup', onUp);
                    window.removeEventListener('pointercancel', onUp);
                };
                window.addEventListener('pointermove', onMove);
                window.addEventListener('pointerup', onUp);
                window.addEventListener('pointercancel', onUp);
            });
        }
        // Reusable 20-Segment Digital Gain Reduction (GR) LED Bar Meter
        function renderDigitalGrBarHtml(barId, label = 'DIGITAL GAIN REDUCTION (0 to -24 dB)') {
            let segs = '';
            for (let s = 1; s <= 20; s++) {
                const dbThresh = s * 1.2; // 1.2 dB per LED up to 24 dB
                const c = dbThresh <= 6.0 ? '#10b981' : (dbThresh <= 12.0 ? '#f59e0b' : '#ef4444');
                segs += `<div id="${barId}-seg-${s}" data-thresh="${dbThresh.toFixed(1)}" data-color="${c}" style="flex:1; height:14px; border-radius:2px; background:#1e293b; border:1px solid #0f172a; transition:background 0.04s;"></div>`;
            }
            return `
                <div style="width:100%; background:#090d14; border:1px solid #334155; border-radius:6px; padding:6px 10px; box-sizing:border-box;">
                    <div style="display:flex; justify-content:space-between; align-items:center; font-size:0.65rem; font-weight:800; color:#94a3b8; margin-bottom:4px; font-family:monospace;">
                        <span>${label}</span>
                        <span id="${barId}-readout" style="color:#10b981; font-size:0.75rem;">0.0 dB GR</span>
                    </div>
                    <div style="display:flex; gap:2px; align-items:center;">${segs}</div>
                    <div style="display:flex; justify-content:space-between; font-size:0.58rem; color:#64748b; font-family:monospace; margin-top:2px;">
                        <span>0dB</span><span>-3</span><span>-6</span><span>-9</span><span>-12</span><span>-18</span><span>-24dB</span>
                    </div>
                </div>
            `;
        }
        function updateDigitalGrBar(barId, grDbPositive) {
            const gr = Math.max(0, Math.min(24, Math.abs(grDbPositive || 0)));
            const ro = document.getElementById(`${barId}-readout`);
            if (ro) {
                ro.innerText = gr > 0.05 ? `-${gr.toFixed(1)} dB GR` : '0.0 dB GR';
                ro.style.color = gr > 12 ? '#ef4444' : (gr > 6 ? '#f59e0b' : '#10b981');
            }
            for (let s = 1; s <= 20; s++) {
                const seg = document.getElementById(`${barId}-seg-${s}`);
                if (!seg) continue;
                const thresh = parseFloat(seg.dataset.thresh || (s * 1.2));
                if (gr >= thresh - 0.5) {
                    seg.style.background = seg.dataset.color;
                    seg.style.boxShadow = `0 0 6px ${seg.dataset.color}`;
                } else {
                    seg.style.background = '#1e293b';
                    seg.style.boxShadow = 'none';
                }
            }
        }
        // Analog Summing & Console Transformer Switchers
        function setSummingConsole(consoleType) {
            analogSummingState.console = consoleType;
            if (window.dispatchDspParam) window.dispatchDspParam(0, 'summingConsole', consoleType);
            updateSummingHeaderBtn();
        }
        function updateSummingHeaderBtn() {
            const btn = document.getElementById('btn-analog-summing');
            if (!btn) return;
            const names = { neve: 'NEVE 8048', api: 'API 2520', ssl: 'SSL 4000E', tube: 'TUBE DESK' };
            const cName = names[analogSummingState.console] || 'CONSOLE';
            if (analogSummingState.enabled) {
                btn.classList.add('active');
                btn.style.background = '#78350f';
                btn.style.borderColor = '#d97706';
                btn.style.color = '#fef3c7';
                btn.innerHTML = `🎛️ SUMMING: ${cName}`;
            } else {
                btn.classList.remove('active');
                btn.style.background = '#1f242d';
                btn.style.borderColor = '#374151';
                btn.style.color = '#94a3b8';
                btn.innerHTML = `🎛️ SUMMING: OFF`;
            }
        }
    window.fetish76State = fetish76State;
    window.lalaState = lalaState;
    window.busterLimiterState = busterLimiterState;
    window.analogSummingState = analogSummingState;
    window.drawAoKnob = drawAoKnob;
    window.renderAoVuMeter = renderAoVuMeter;
    window.startAoVuLoop = startAoVuLoop;
    window.bindAoRotary = bindAoRotary;
    window.renderDigitalGrBarHtml = renderDigitalGrBarHtml;
    window.updateDigitalGrBar = updateDigitalGrBar;
    window.setSummingConsole = setSummingConsole;
    window.updateSummingHeaderBtn = updateSummingHeaderBtn;
})(window);
