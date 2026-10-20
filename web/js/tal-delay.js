// tal-delay.js - Authentic TAL-Delay-Pro Tape & Stereo Delay Engine
(function(window) {
    'use strict';
        const delayDspState = {
            presetIndex: 0,
            presets: ["Dotted 8th Ping-Pong", "Slapback Tape", "Warm Dub Echo", "Quarter Clean", "Ambient Cloud", "Tape Echo 80s"],
            timeMs: 375,
            subdivision: "1/8d",
            syncToTempo: true,
            feedback: 42,
            diffusion: 35,
            ducking: 18,
            sidechain: false,
            lowCut: 140,
            highCut: 5400,
            modRate: 22,
            modDepth: 28,
            drive: 25,
            flutter: 18,
            tune: 0,
            stereo: 100,
            spread: 65,
            dry: 0,
            wet: 85,
            pingPong: true
        };
        function syncAuxDelayDsp() {
            if (!window.meterWs || window.meterWs.readyState !== WebSocket.OPEN) return;
            const d = delayDspState;
            [["delayTimeMs", d.timeMs], ["delayFeedback", d.feedback], ["delayDiffuse", d.diffusion], ["delayPingPong", d.pingPong ? 1 : 0], ["delayDry", d.dry], ["delayWet", d.wet], ["delayLowCut", d.lowCut], ["delayHighCut", d.highCut], ["delayDrive", d.drive ?? 30], ["delayFlutter", d.flutter ?? 20], ["delayModRate", d.modRate], ["delayModDepth", d.modDepth], ["delayDucking", d.ducking], ["delayStereo", d.stereo], ["delaySpread", d.spread ?? 50]].forEach(([p, v]) => {
                window.meterWs.send(JSON.stringify({ type: "dsp_param", channel: "master", param: p, value: v }));
            });
        }
        window.syncAuxDelayDsp = syncAuxDelayDsp;
        function updateDelayTimeMs(val) {
            delayDspState.timeMs = parseInt(val);
            const valEl = document.getElementById("delay-time-val");
            if (valEl) valEl.innerText = delayDspState.timeMs + " ms";
            if (delayDspState.syncToTempo) {
                delayDspState.syncToTempo = false;
                const btn = document.getElementById("delay-sync-btn") || document.getElementById("delay-tal-sync-btn");
                if (btn) { btn.classList.remove("active"); btn.innerText = "TAP SYNC: OFF"; }
            }
            syncAuxDelayDsp();
        }
        function updateDelaySubdivision(val) {
            delayDspState.subdivision = val;
            if (delayDspState.syncToTempo) updateDelayTimeFromBpm();
        }
        function toggleDelaySync() {
            delayDspState.syncToTempo = !delayDspState.syncToTempo;
            const bpmVal = (window.globalBpm && window.globalBpm > 0) ? window.globalBpm : 120;
            const btn = document.getElementById("delay-tal-sync-btn") || document.getElementById("delay-sync-btn");
            if (btn) {
                btn.classList.toggle("active", delayDspState.syncToTempo);
                btn.style.background = delayDspState.syncToTempo ? "#10b981" : "#1f242d";
                btn.innerText = "TAP SYNC: " + (delayDspState.syncToTempo ? "ON (" + bpmVal.toFixed(0) + " BPM)" : "OFF");
            }
            if (delayDspState.syncToTempo) updateDelayTimeFromBpm();
        }
        function updateDelayTimeFromBpm() {
            const bpmVal = (window.globalBpm && window.globalBpm > 0) ? window.globalBpm : 120;
            const quarterMs = 60000 / bpmVal;
            let targetMs = quarterMs;
            if (delayDspState.subdivision === '1/4') targetMs = quarterMs;
            else if (delayDspState.subdivision === '1/8d') targetMs = quarterMs * 0.75;
            else if (delayDspState.subdivision === '1/8') targetMs = quarterMs * 0.5;
            else if (delayDspState.subdivision === '1/8t') targetMs = quarterMs * (1/3);
            else if (delayDspState.subdivision === '1/16') targetMs = quarterMs * 0.25;
            delayDspState.timeMs = Math.round(targetMs);
            const slider = document.getElementById("delay-time-slider");
            const valEl = document.getElementById("delay-time-val");
            const syncBpm = document.getElementById("delay-sync-bpm");
            if (slider) slider.value = delayDspState.timeMs;
            if (valEl) valEl.innerText = delayDspState.timeMs + " ms";
            if (syncBpm) syncBpm.innerText = bpmVal.toFixed(1);
            const talTimeCap = document.getElementById("tal-cap-time");
            const talTimeVal = document.getElementById("tal-val-time");
            if (talTimeCap) talTimeCap.style.bottom = Math.min(100, Math.max(0, (delayDspState.timeMs / 1500) * 100)) + "%";
            if (talTimeVal) talTimeVal.innerText = delayDspState.timeMs + " ms";
            syncAuxDelayDsp();
        }
        function applyQuickDelayPreset(idx) {
            const d = delayDspState;
            d.presetIndex = idx;
            if (idx === 0) { // Dotted 8th Ping-Pong
                d.timeMs = 375; d.subdivision = "1/8d"; d.feedback = 44; d.diffusion = 35; d.pingPong = true;
                d.drive = 22; d.flutter = 16; d.lowCut = 140; d.highCut = 5600; d.modRate = 22; d.modDepth = 28; d.ducking = 18; d.dry = 0; d.wet = 85;
            } else if (idx === 1) { // Slapback Tape
                d.timeMs = 115; d.subdivision = "1/16"; d.feedback = 14; d.diffusion = 12; d.pingPong = false;
                d.drive = 48; d.flutter = 35; d.lowCut = 180; d.highCut = 4200; d.modRate = 28; d.modDepth = 32; d.ducking = 0; d.dry = 0; d.wet = 85;
            } else if (idx === 2) { // Warm Dub Echo
                d.timeMs = 460; d.subdivision = "1/4"; d.feedback = 64; d.diffusion = 48; d.pingPong = true;
                d.drive = 55; d.flutter = 42; d.lowCut = 220; d.highCut = 3100; d.modRate = 18; d.modDepth = 45; d.ducking = 24; d.dry = 0; d.wet = 88;
            } else if (idx === 3) { // Quarter Clean
                d.timeMs = 500; d.subdivision = "1/4"; d.feedback = 36; d.diffusion = 15; d.pingPong = false;
                d.drive = 5; d.flutter = 6; d.lowCut = 100; d.highCut = 11000; d.modRate = 14; d.modDepth = 12; d.ducking = 15; d.dry = 0; d.wet = 82;
            } else if (idx === 4) { // Ambient Cloud
                d.timeMs = 750; d.subdivision = "1/4"; d.feedback = 74; d.diffusion = 82; d.pingPong = true;
                d.drive = 28; d.flutter = 36; d.lowCut = 180; d.highCut = 3800; d.modRate = 16; d.modDepth = 55; d.ducking = 28;
            } else if (idx === 5) { // Tape Echo 80s
                d.timeMs = 380; d.subdivision = "1/8d"; d.feedback = 48; d.diffusion = 40; d.pingPong = true;
                d.drive = 40; d.flutter = 28; d.lowCut = 130; d.highCut = 4800; d.modRate = 24; d.modDepth = 34; d.ducking = 16;
            }
            const disp = document.getElementById("tal-delay-preset-display");
            if (disp) disp.innerText = d.presets[d.presetIndex];
            document.querySelectorAll(".tal-delay-quick-btn").forEach((b, i) => b.classList.toggle("active", i === idx));
            updateTalDelayFadersVisual();
            initTalDelayRotaries();
            syncAuxDelayDsp();
        }
        window.applyQuickDelayPreset = applyQuickDelayPreset;
        function cycleTalDelayPreset(dir) {
            const d = delayDspState;
            const nextIdx = (d.presetIndex + dir + d.presets.length) % d.presets.length;
            applyQuickDelayPreset(nextIdx);
        }
        function toggleTalDelaySidechain(btn) {
            delayDspState.sidechain = !delayDspState.sidechain;
            if (btn) btn.classList.toggle("active", delayDspState.sidechain);
            syncAuxDelayDsp();
        }
        function renderDelayControls() {
            const d = delayDspState;
            const preset = d.presets[d.presetIndex];
            const bpmVal = (window.globalBpm && window.globalBpm > 0) ? window.globalBpm : 120;
            const timePct = Math.min(100, Math.max(0, (d.timeMs / 1500) * 100));
            const subMap = { "1/4": 80, "1/8d": 60, "1/8": 40, "1/8t": 20, "1/16": 0 };
            const subPct = subMap[d.subdivision] ?? 80;
            return `
            <div class="tal-plugin-frame tal-theme-delay">
                <div class="tal-title-banner" style="display:flex; justify-content:space-between; align-items:center; padding:6px 16px;">
                    <span style="font-size:0.78rem; font-family:'JetBrains Mono',monospace; letter-spacing:1px; color:#1f2937; font-weight:900;">HERMITE CUBIC FRACTIONAL TAP + AUTO-DUCKING</span>
                    <span>TAL-DELAY-PRO</span>
                </div>
                <div class="tal-header-bar" style="flex-wrap:wrap; gap:8px;">
                    <div class="tal-preset-box">
                        <button class="tal-preset-arrow" onclick="cycleTalDelayPreset(-1)">&ltrif;</button>
                        <span class="tal-preset-name" id="tal-delay-preset-display">${preset}</span>
                        <button class="tal-preset-arrow" onclick="cycleTalDelayPreset(1)">&rtrif;</button>
                    </div>
                    <div style="display:flex; gap:6px; flex-wrap:wrap;">
                        <button class="btn-top tal-delay-quick-btn ${d.presetIndex===0?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickDelayPreset(0)">🏓 DOTTED 8TH PING-PONG</button>
                        <button class="btn-top tal-delay-quick-btn ${d.presetIndex===1?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickDelayPreset(1)">📼 SLAPBACK TAPE</button>
                        <button class="btn-top tal-delay-quick-btn ${d.presetIndex===2?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickDelayPreset(2)">🌊 WARM DUB ECHO</button>
                        <button class="btn-top tal-delay-quick-btn ${d.presetIndex===3?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickDelayPreset(3)">💎 1/4 CLEAN</button>
                    </div>
                    <button class="btn-top ${d.syncToTempo ? 'active' : ''}" id="delay-tal-sync-btn" style="padding:4px 10px; font-size:0.7rem; font-weight:800; border-radius:4px; background:${d.syncToTempo ? '#10b981' : '#1f242d'}; color:#fff;" onclick="toggleDelaySync()">TAP SYNC: ${d.syncToTempo ? 'ON (' + bpmVal.toFixed(0) + ' BPM)' : 'OFF'}</button>
                    <div style="flex:1;"></div>
                </div>
                <!-- Top Section: 5 Delay Faders + Ducking + Dry/Wet Faders -->
                <div class="tal-top-panel">
                    <div class="tal-section-box" style="flex: 2.2;">
                        <div class="tal-section-header tal-sh-main">TAPE &amp; STEREO DELAY</div>
                        <div class="tal-fader-row">
                            <!-- TIME -->
                            <div class="tal-fader-col">
                                <span class="tal-fader-val" id="tal-val-time">${d.timeMs} ms</span>
                                <div class="tal-vtrack" id="tal-track-time" onpointerdown="startTalFaderDrag('time', event)">
                                    <div class="tal-vtrack-groove"></div>
                                    <div class="tal-vtrack-ticks">
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div>
                                    </div>
                                    <div class="tal-fader-handle" id="tal-cap-time" style="bottom: ${timePct}%;"></div>
                                </div>
                                <span class="tal-fader-label">TIME</span>
                            </div>
                            <!-- SUBDIV -->
                            <div class="tal-fader-col">
                                <span class="tal-fader-val" id="tal-val-subdivision">${d.subdivision}</span>
                                <div class="tal-vtrack" id="tal-track-subdivision" onpointerdown="startTalFaderDrag('subdivision', event)">
                                    <div class="tal-vtrack-groove"></div>
                                    <div class="tal-vtrack-ticks">
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div><div class="tal-tick"></div>
                                    </div>
                                    <div class="tal-fader-handle" id="tal-cap-subdivision" style="bottom: ${subPct}%;"></div>
                                </div>
                                <span class="tal-fader-label">BEAT</span>
                            </div>
                            <!-- FEEDBACK -->
                            <div class="tal-fader-col">
                                <span class="tal-fader-val" id="tal-val-feedback">${d.feedback.toFixed(1)} %</span>
                                <div class="tal-vtrack" id="tal-track-feedback" onpointerdown="startTalFaderDrag('feedback', event)">
                                    <div class="tal-vtrack-groove"></div>
                                    <div class="tal-vtrack-ticks">
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div>
                                    </div>
                                    <div class="tal-fader-handle" id="tal-cap-feedback" style="bottom: ${d.feedback}%;"></div>
                                </div>
                                <span class="tal-fader-label">FEEDBACK</span>
                            </div>
                            <!-- DIFFUSE -->
                            <div class="tal-fader-col">
                                <span class="tal-fader-val" id="tal-val-diffusion">${d.diffusion.toFixed(1)} %</span>
                                <div class="tal-vtrack" id="tal-track-diffusion" onpointerdown="startTalFaderDrag('diffusion', event)">
                                    <div class="tal-vtrack-groove"></div>
                                    <div class="tal-vtrack-ticks">
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div>
                                    </div>
                                    <div class="tal-fader-handle" id="tal-cap-diffusion" style="bottom: ${d.diffusion}%;"></div>
                                </div>
                                <span class="tal-fader-label">DIFFUSE</span>
                            </div>
                            <!-- PING-PONG / STEREO -->
                            <div class="tal-fader-col">
                                <span class="tal-fader-val" id="tal-val-pingPong">${d.pingPong ? 'PING-PONG' : 'STEREO'}</span>
                                <div class="tal-vtrack" id="tal-track-pingPong" onpointerdown="startTalFaderDrag('pingPong', event)">
                                    <div class="tal-vtrack-groove"></div>
                                    <div class="tal-vtrack-ticks">
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div>
                                    </div>
                                    <div class="tal-fader-handle" id="tal-cap-pingPong" style="bottom: ${d.pingPong ? '100' : '0'}%;"></div>
                                </div>
                                <span class="tal-fader-label">MODE</span>
                            </div>
                        </div>
                    </div>
                    <!-- DUCKING / S-CHAIN Mid Column -->
                    <div class="tal-mid-col">
                        <button class="tal-schain-btn ${d.sidechain ? 'active' : ''}" onclick="toggleTalDelaySidechain(this)">S-CHAIN</button>
                        <div style="width:100%; height:18px; background:#0d1410; border-radius:3px; border:1px solid #1a2f23;"></div>
                        <div class="tal-rotary-box">
                            <span class="tal-rotary-val" id="tal-delay-val-ducking">${d.ducking.toFixed(1)} %</span>
                            <canvas class="tal-knob-canvas" id="tal-delay-knob-ducking" width="44" height="44"></canvas>
                            <span class="tal-rotary-label">DUCKING</span>
                        </div>
                    </div>
                    <!-- MIX Group -->
                    <div class="tal-section-box" style="flex: 1;">
                        <div class="tal-section-header tal-sh-mix">MIX</div>
                        <div class="tal-fader-row">
                            <div class="tal-fader-col">
                                <span class="tal-fader-val" id="tal-val-delayDry">${d.dry.toFixed(1)} %</span>
                                <div class="tal-vtrack" id="tal-track-delayDry" onpointerdown="startTalFaderDrag('delayDry', event)">
                                    <div class="tal-vtrack-groove"></div>
                                    <div class="tal-vtrack-ticks">
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div>
                                    </div>
                                    <div class="tal-fader-handle" id="tal-cap-delayDry" style="bottom: ${d.dry}%;"></div>
                                </div>
                                <span class="tal-fader-label">DRY</span>
                            </div>
                            <div class="tal-fader-col">
                                <span class="tal-fader-val" id="tal-val-delayWet">${d.wet.toFixed(1)} %</span>
                                <div class="tal-vtrack" id="tal-track-delayWet" onpointerdown="startTalFaderDrag('delayWet', event)">
                                    <div class="tal-vtrack-groove"></div>
                                    <div class="tal-vtrack-ticks">
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                        <div class="tal-tick major"></div>
                                    </div>
                                    <div class="tal-fader-handle" id="tal-cap-delayWet" style="bottom: ${d.wet}%;"></div>
                                </div>
                                <span class="tal-fader-label">WET</span>
                            </div>
                        </div>
                    </div>
                </div>
                <!-- Bottom Section: MOD | TAPE | FILTERS | WIDTH -->
                <div class="tal-bottom-panel">
                    <!-- MOD -->
                    <div class="tal-btm-sub">
                        <div class="tal-section-header tal-sh-mod">MOD</div>
                        <div class="tal-btm-knobs">
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-modRate">${d.modRate.toFixed(1)} %</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-modRate" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">RATE</span>
                            </div>
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-modDepth">${d.modDepth.toFixed(1)} %</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-modDepth" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">DEPTH</span>
                            </div>
                        </div>
                    </div>
                    <!-- TAPE -->
                    <div class="tal-btm-sub">
                        <div class="tal-section-header tal-sh-tune">TAPE</div>
                        <div class="tal-btm-knobs">
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-drive">${(d.drive ?? 30).toFixed(1)} %</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-drive" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">DRIVE</span>
                            </div>
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-flutter">${(d.flutter ?? 20).toFixed(1)} %</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-flutter" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">FLUTTER</span>
                            </div>
                        </div>
                    </div>
                    <!-- FILTERS -->
                    <div class="tal-btm-sub" style="flex:1.5;">
                        <div class="tal-section-header tal-sh-eq">FILTERS</div>
                        <div class="tal-btm-knobs dual-col">
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-lowCut">${d.lowCut} Hz</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-lowCut" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">LOW CUT</span>
                            </div>
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-highCut">${(d.highCut/1000).toFixed(1)} kHz</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-highCut" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">HIGH CUT</span>
                            </div>
                        </div>
                    </div>
                    <!-- WIDTH -->
                    <div class="tal-btm-sub">
                        <div class="tal-section-header tal-sh-lofi">WIDTH</div>
                        <div class="tal-btm-knobs">
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-stereo">${d.stereo.toFixed(1)} %</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-stereo" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">STEREO</span>
                            </div>
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-delay-val-spread">${(d.spread ?? 50).toFixed(1)} %</span>
                                <canvas class="tal-knob-canvas" id="tal-delay-knob-spread" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">SPREAD</span>
                            </div>
                        </div>
                    </div>
                </div>
                <!-- Footer Bar -->
                <div class="tal-footer-bar">
                    <div class="tal-footer-title">TAL DELAY PRO</div>
                    <div>VINTAGE TAPE &amp; DIGITAL DUAL-ENGINE STEREO DELAY</div>
                </div>
            </div>
            `;
        }
        function updateTalDelayFadersVisual() {
            const d = delayDspState;
            const timePct = Math.min(100, Math.max(0, (d.timeMs / 1500) * 100));
            const subMap = { "1/4": 80, "1/8d": 60, "1/8": 40, "1/8t": 20, "1/16": 0 };
            const subPct = subMap[d.subdivision] ?? 80;
            const timeCap = document.getElementById("tal-cap-time");
            const timeVal = document.getElementById("tal-val-time");
            if (timeCap) timeCap.style.bottom = timePct + "%";
            if (timeVal) timeVal.innerText = d.timeMs + " ms";
            const subCap = document.getElementById("tal-cap-subdivision");
            const subVal = document.getElementById("tal-val-subdivision");
            if (subCap) subCap.style.bottom = subPct + "%";
            if (subVal) subVal.innerText = d.subdivision;
            const fbCap = document.getElementById("tal-cap-feedback");
            const fbVal = document.getElementById("tal-val-feedback");
            if (fbCap) fbCap.style.bottom = d.feedback + "%";
            if (fbVal) fbVal.innerText = d.feedback.toFixed(1) + " %";
            const diffCap = document.getElementById("tal-cap-diffusion");
            const diffVal = document.getElementById("tal-val-diffusion");
            if (diffCap) diffCap.style.bottom = d.diffusion + "%";
            if (diffVal) diffVal.innerText = d.diffusion.toFixed(1) + " %";
            const ppCap = document.getElementById("tal-cap-pingPong");
            const ppVal = document.getElementById("tal-val-pingPong");
            if (ppCap) ppCap.style.bottom = (d.pingPong ? 100 : 0) + "%";
            if (ppVal) ppVal.innerText = d.pingPong ? "PING-PONG" : "STEREO";
            const dryCap = document.getElementById("tal-cap-delayDry");
            const dryVal = document.getElementById("tal-val-delayDry");
            if (dryCap) dryCap.style.bottom = d.dry + "%";
            if (dryVal) dryVal.innerText = d.dry.toFixed(1) + " %";
            const wetCap = document.getElementById("tal-cap-delayWet");
            const wetVal = document.getElementById("tal-val-delayWet");
            if (wetCap) wetCap.style.bottom = d.wet + "%";
            if (wetVal) wetVal.innerText = d.wet.toFixed(1) + " %";
        }
        // Rotary Canvas Knobs for TAL Delay Pro
        const talDelayKnobDefs = {
            ducking: { min: 0, max: 100, unit: " %", color: "#34d399", key: "ducking" },
            modRate: { min: 0, max: 100, unit: " %", color: "#6ee7b7", key: "modRate" },
            modDepth: { min: 0, max: 100, unit: " %", color: "#6ee7b7", key: "modDepth" },
            drive: { min: 0, max: 100, unit: " %", color: "#67e8f9", key: "drive" },
            flutter: { min: 0, max: 100, unit: " %", color: "#67e8f9", key: "flutter" },
            lowCut: { min: 20, max: 1000, unit: " Hz", color: "#a5f3fc", key: "lowCut" },
            highCut: { min: 1000, max: 18000, unit: " Hz", color: "#a5f3fc", key: "highCut" },
            stereo: { min: 0, max: 100, unit: " %", color: "#fde047", key: "stereo" },
            spread: { min: 0, max: 100, unit: " %", color: "#fde047", key: "spread" }
        };
        function initTalDelayRotaries() {
            Object.keys(talDelayKnobDefs).forEach(k => {
                const def = talDelayKnobDefs[k];
                const defaultVal = delayDspState[def.key] ?? (k === 'drive' ? 30 : (k === 'flutter' ? 20 : (k === 'spread' ? 50 : def.min)));
                const norm = Math.max(0, Math.min(1, (defaultVal - def.min) / (def.max - def.min)));
                if (window.drawTalKnob) window.drawTalKnob("tal-delay-knob-" + k, norm, def.color);
                const cvs = document.getElementById("tal-delay-knob-" + k);
                if (cvs && !cvs.dataset.bound) {
                    cvs.dataset.bound = "true";
                    cvs.style.touchAction = "none";
                    let startX = 0, startY = 0, startVal = norm, lastTap = 0;
                    const applyNorm = (newNorm) => {
                        newNorm = Math.max(0, Math.min(1, newNorm));
                        const actualVal = def.min + newNorm * (def.max - def.min);
                        delayDspState[def.key] = (def.unit.includes("Hz")) ? Math.round(actualVal) : parseFloat(actualVal.toFixed(1));
                        if (window.drawTalKnob) window.drawTalKnob("tal-delay-knob-" + k, newNorm, def.color);
                        const valEl = document.getElementById("tal-delay-val-" + k);
                        if (valEl) valEl.innerText = (k === 'highCut') ? ((delayDspState[def.key] / 1000).toFixed(1) + " kHz") : (delayDspState[def.key] + def.unit);
                        syncAuxDelayDsp();
                    };
                    cvs.addEventListener("pointerdown", (e) => {
                        e.preventDefault();
                        const now = Date.now();
                        if (now - lastTap < 280) { applyNorm((defaultVal - def.min) / (def.max - def.min)); lastTap = 0; return; }
                        lastTap = now;
                        startX = e.clientX; startY = e.clientY;
                        const currentVal = delayDspState[def.key] ?? defaultVal;
                        startVal = (currentVal - def.min) / (def.max - def.min);
                        try { cvs.setPointerCapture(e.pointerId); } catch (_) {}
                        const onMove = (ev) => {
                            if (ev.pointerId !== undefined && ev.pointerId !== e.pointerId) return;
                            const delta = (startY - ev.clientY) + (ev.clientX - startX);
                            const fine = (window.activeTouchCount >= 2) ? 0.25 : 1.0;
                            applyNorm(startVal + delta * 0.006 * fine);
                        };
                        const onUp = () => { window.removeEventListener("pointermove", onMove); window.removeEventListener("pointerup", onUp); };
                        window.addEventListener("pointermove", onMove);
                        window.addEventListener("pointerup", onUp);
                    });
                }
            });
        }
    window.delayDspState = delayDspState;
    window.updateDelayTimeMs = updateDelayTimeMs;
    window.updateDelaySubdivision = updateDelaySubdivision;
    window.toggleDelaySync = toggleDelaySync;
    window.updateDelayTimeFromBpm = updateDelayTimeFromBpm;
    window.cycleTalDelayPreset = cycleTalDelayPreset;
    window.toggleTalDelaySidechain = toggleTalDelaySidechain;
    window.renderDelayControls = renderDelayControls;
    window.updateTalDelayFadersVisual = updateTalDelayFadersVisual;
    window.initTalDelayRotaries = initTalDelayRotaries;
})(window);
