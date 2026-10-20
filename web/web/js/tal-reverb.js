// tal-reverb.js - Authentic TAL-Reverb-4 Plate & Hall Engine
(function(window) {
    'use strict';
        const reverbDspState = {
            engine: "tal", // 'tal' | 'atlantis'
            tal: {
                presetIndex: 0,
                presets: ["Lush Plate", "Abbey Vocal Hall", "Tight Studio Room", "Warm Chamber", "Vintage Plate 80s", "Shimmer Cathedral"],
                // Top MAIN Faders (0..100)
                size: 76.0,
                damp: 28.0,
                delay: 0.024, // 24ms studio pre-delay
                diffuse: 92.0,
                stereo: 100.0,
                irMix: 35.0,  // Early Reflections IR Hybrid Body (0..100%)
                // Ducking & Sidechain
                ducking: 12.0,
                sidechain: false,
                // Top MIX Faders (Aux Return defaults to 0% Dry / 85% Wet for pure parallel Aux bus)
                dry: 0.0,
                wet: 85.0,
                // Bottom Sections (Abbey Road High/Low Cut voicing)
                modRate: 24.0,
                modAmount: 38.0,
                tune: 0.0,
                tuneAmount: 0.0,
                eqLowCut: 160,    // Hz (Abbey Road mud cut)
                eqHighCut: 7200,  // Hz (Silk air roll-off)
                eqPeakFreq: 1800, // Hz
                eqPeakGain: 0.0,  // dB
                lofiBits: 24,     // Bit
                lofiSR: 48000     // Hz
            },
            atlantis: {
                irModel: "cathedral",
                preDelay: 25,
                damping: 35,
                roomSize: 82,
                irMix: 55,
                preEqLow: 160,
                postEqHigh: 7200,
                dry: -60, // dB (Aux return pure wet)
                wet: 0,   // dB
                norm: true,
                rev: false
            }
        };
        function switchReverbEngine(engineName) {
            reverbDspState.engine = engineName;
            const content = document.getElementById("reverb-engine-content");
            if (content) content.innerHTML = renderReverbControls();
            const btns = document.querySelectorAll("#reverb-engine-content")?.[0]?.parentElement?.querySelectorAll(".btn-top");
            if (btns && btns.length >= 2) {
                btns[0].classList.toggle("active", engineName === 'tal');
                btns[1].classList.toggle("active", engineName === 'atlantis');
            }
            if (engineName === 'tal') {
                initTalRotaries();
            } else {
                initIrCanvas();
            }
            syncAuxReverbDsp();
        }
        function applyQuickReverbPreset(idx) {
            const s = reverbDspState.tal;
            s.presetIndex = idx;
            if (idx === 0) { // Lush Plate (EMT 140 / Lexicon hybrid)
                s.size = 76.0; s.damp = 26.0; s.delay = 0.022; s.diffuse = 94.0; s.stereo = 100.0; s.irMix = 32.0;
                s.eqLowCut = 160; s.eqHighCut = 7800; s.modRate = 24.0; s.modAmount = 38.0; s.ducking = 12.0; s.dry = 0.0; s.wet = 85.0;
            } else if (idx === 1) { // Abbey Vocal Hall
                s.size = 86.0; s.damp = 22.0; s.delay = 0.038; s.diffuse = 96.0; s.stereo = 100.0; s.irMix = 45.0;
                s.eqLowCut = 180; s.eqHighCut = 6800; s.modRate = 18.0; s.modAmount = 42.0; s.ducking = 18.0; s.dry = 0.0; s.wet = 88.0;
            } else if (idx === 2) { // Tight Studio Room
                s.size = 44.0; s.damp = 48.0; s.delay = 0.012; s.diffuse = 84.0; s.stereo = 92.0; s.irMix = 62.0;
                s.eqLowCut = 140; s.eqHighCut = 8500; s.modRate = 14.0; s.modAmount = 20.0; s.ducking = 0.0; s.dry = 0.0; s.wet = 82.0;
            } else if (idx === 3) { // Warm Chamber
                s.size = 68.0; s.damp = 36.0; s.delay = 0.028; s.diffuse = 90.0; s.stereo = 98.0; s.irMix = 48.0;
                s.eqLowCut = 150; s.eqHighCut = 5800; s.modRate = 20.0; s.modAmount = 30.0; s.ducking = 10.0; s.dry = 0.0; s.wet = 85.0;
            } else if (idx === 4) { // Vintage Plate 80s
                s.size = 82.0; s.damp = 18.0; s.diffuse = 92.0; s.delay = 0.030; s.stereo = 100.0; s.irMix = 25.0;
                s.eqLowCut = 200; s.eqHighCut = 9200; s.modRate = 30.0; s.modAmount = 48.0; s.ducking = 15.0;
            } else if (idx === 5) { // Shimmer Cathedral
                s.size = 95.0; s.damp = 10.0; s.diffuse = 100.0; s.delay = 0.055; s.stereo = 100.0; s.irMix = 40.0;
                s.eqLowCut = 120; s.eqHighCut = 11500; s.modRate = 22.0; s.modAmount = 55.0; s.ducking = 20.0;
            }
            const disp = document.getElementById("tal-preset-display");
            if (disp) disp.innerText = s.presets[s.presetIndex];
            document.querySelectorAll(".tal-quick-preset-btn").forEach((b, i) => b.classList.toggle("active", i === idx));
            updateTalFadersVisual();
            initTalRotaries();
            syncAuxReverbDsp();
        }
        window.applyQuickReverbPreset = applyQuickReverbPreset;
        function renderReverbControls() {
            if (reverbDspState.engine === 'tal') {
                const s = reverbDspState.tal;
                const preset = s.presets[s.presetIndex];
                return `
                <div class="tal-plugin-frame">
                    <div class="tal-title-banner" style="display:flex; justify-content:space-between; align-items:center; padding:6px 16px;">
                        <span style="font-size:0.78rem; font-family:'JetBrains Mono',monospace; letter-spacing:1px; color:#1f2937; font-weight:900;">4×4 HADAMARD FDN + 8-TAP EARLY IR HYBRID</span>
                        <span>TAL-REVERB-4 PRO</span>
                    </div>
                    <!-- Header with preset browser + 1-Tap Studio Presets -->
                    <div class="tal-header-bar" style="flex-wrap:wrap; gap:8px;">
                        <div class="tal-preset-box">
                            <button class="tal-preset-arrow" onclick="cycleTalPreset(-1)">&ltrif;</button>
                            <span class="tal-preset-name" id="tal-preset-display">${preset}</span>
                            <button class="tal-preset-arrow" onclick="cycleTalPreset(1)">&rtrif;</button>
                        </div>
                        <div style="display:flex; gap:6px; flex-wrap:wrap;">
                            <button class="btn-top tal-quick-preset-btn ${s.presetIndex===0?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickReverbPreset(0)">✨ LUSH PLATE</button>
                            <button class="btn-top tal-quick-preset-btn ${s.presetIndex===1?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickReverbPreset(1)">🏛️ ABBEY VOCAL HALL</button>
                            <button class="btn-top tal-quick-preset-btn ${s.presetIndex===2?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickReverbPreset(2)">🥁 TIGHT STUDIO ROOM</button>
                            <button class="btn-top tal-quick-preset-btn ${s.presetIndex===3?'active':''}" style="padding:4px 10px; font-size:0.7rem;" onclick="applyQuickReverbPreset(3)">🎻 WARM CHAMBER</button>
                        </div>
                        <div style="flex:1;"></div>
                    </div>
                    <!-- Top Section: MAIN faders + DUCKING + MIX faders -->
                    <div class="tal-top-panel">
                        <!-- MAIN Group -->
                        <div class="tal-section-box" style="flex: 2.5;">
                            <div class="tal-section-header tal-sh-main">MAIN + EARLY IR HYBRID</div>
                            <div class="tal-fader-row">
                                <!-- SIZE -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-size">${s.size.toFixed(1)} %</span>
                                    <div class="tal-vtrack" id="tal-track-size" onpointerdown="startTalFaderDrag('size', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-size" style="bottom: ${s.size}%;"></div>
                                    </div><span class="tal-fader-label">DECAY SIZE</span>
                                </div>
                                <!-- DAMP -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-damp">${s.damp.toFixed(1)} %</span>
                                    <div class="tal-vtrack" id="tal-track-damp" onpointerdown="startTalFaderDrag('damp', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-damp" style="bottom: ${s.damp}%;"></div>
                                    </div><span class="tal-fader-label">HF DAMP</span>
                                </div>
                                <!-- DELAY -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-delay">${Math.round(s.delay * 500)} ms</span>
                                    <div class="tal-vtrack" id="tal-track-delay" onpointerdown="startTalFaderDrag('delay', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-delay" style="bottom: ${s.delay * 100}%;"></div>
                                    </div><span class="tal-fader-label">PRE-DLY</span>
                                </div>
                                <!-- IR EARLY BODY -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-irMix" style="color:#38bdf8;">${(s.irMix ?? 35).toFixed(1)} %</span>
                                    <div class="tal-vtrack" id="tal-track-irMix" onpointerdown="startTalFaderDrag('irMix', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-irMix" style="bottom: ${s.irMix ?? 35}%;"></div>
                                    </div><span class="tal-fader-label" style="color:#38bdf8;">IR BODY</span>
                                </div>
                                <!-- DIFFUSE -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-diffuse">${s.diffuse.toFixed(1)} %</span>
                                    <div class="tal-vtrack" id="tal-track-diffuse" onpointerdown="startTalFaderDrag('diffuse', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-diffuse" style="bottom: ${s.diffuse}%;"></div>
                                    </div><span class="tal-fader-label">DIFFUSE</span>
                                </div>
                                <!-- STEREO -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-stereo">${s.stereo.toFixed(1)} %</span>
                                    <div class="tal-vtrack" id="tal-track-stereo" onpointerdown="startTalFaderDrag('stereo', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-stereo" style="bottom: ${s.stereo}%;"></div>
                                    </div><span class="tal-fader-label">STEREO</span>
                                </div>
                            </div>
                        </div>
                        <!-- DUCKING / S-CHAIN Mid Column -->
                        <div class="tal-mid-col">
                            <button class="tal-schain-btn ${s.sidechain ? 'active' : ''}" onclick="toggleTalSidechain(this)">S-CHAIN</button>
                            <div style="width:100%; height:18px; background:#111215; border-radius:3px; border:1px solid #23272e;"></div>
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-val-ducking">${s.ducking.toFixed(1)} %</span><canvas class="tal-knob-canvas" id="tal-knob-ducking" width="44" height="44"></canvas><span class="tal-rotary-label">DUCKING</span>
                            </div>
                        </div>
                        <!-- MIX Group -->
                        <div class="tal-section-box" style="flex: 1;">
                            <div class="tal-section-header tal-sh-mix">AUX BUS MIX</div>
                            <div class="tal-fader-row">
                                <!-- DRY -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-dry">${s.dry.toFixed(1)} %</span>
                                    <div class="tal-vtrack" id="tal-track-dry" onpointerdown="startTalFaderDrag('dry', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-dry" style="bottom: ${s.dry}%;"></div>
                                    </div><span class="tal-fader-label">DRY</span>
                                </div>
                                <!-- WET -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-wet">${s.wet.toFixed(1)} %</span>
                                    <div class="tal-vtrack" id="tal-track-wet" onpointerdown="startTalFaderDrag('wet', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div><div class="tal-tick major"></div></div><div class="tal-fader-handle" id="tal-cap-wet" style="bottom: ${s.wet}%;"></div>
                                    </div><span class="tal-fader-label">WET</span>
                                </div>
                            </div>
                        </div>
                    </div>
                    <!-- Bottom Section: MOD | TUNE | EQ | LO-FI -->
                    <div class="tal-bottom-panel">
                        <!-- MOD -->
                        <div class="tal-btm-sub">
                            <div class="tal-section-header tal-sh-mod">CHORUS MOD</div>
                            <div class="tal-btm-knobs">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-modRate">${s.modRate.toFixed(1)} %</span><canvas class="tal-knob-canvas" id="tal-knob-modRate" width="44" height="44"></canvas><span class="tal-rotary-label">RATE</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-modAmount">${s.modAmount.toFixed(1)} %</span><canvas class="tal-knob-canvas" id="tal-knob-modAmount" width="44" height="44"></canvas><span class="tal-rotary-label">DEPTH</span>
                                </div>
                            </div>
                        </div>
                        <!-- TUNE -->
                        <div class="tal-btm-sub">
                            <div class="tal-section-header tal-sh-tune">TUNE</div>
                            <div class="tal-btm-knobs">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-tune">${s.tune.toFixed(1)}</span><canvas class="tal-knob-canvas" id="tal-knob-tune" width="44" height="44"></canvas><span class="tal-rotary-label">TUNE</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-tuneAmount">${s.tuneAmount.toFixed(1)} %</span><canvas class="tal-knob-canvas" id="tal-knob-tuneAmount" width="44" height="44"></canvas><span class="tal-rotary-label">AMOUNT</span>
                                </div>
                            </div>
                        </div>
                        <!-- EQ -->
                        <div class="tal-btm-sub" style="flex:1.5;">
                            <div class="tal-section-header tal-sh-eq">ABBEY ROAD EQ</div>
                            <div class="tal-btm-knobs dual-col">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqLowCut">${s.eqLowCut} Hz</span><canvas class="tal-knob-canvas" id="tal-knob-eqLowCut" width="44" height="44"></canvas><span class="tal-rotary-label">LOW CUT</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqPeakFreq">${s.eqPeakFreq} Hz</span><canvas class="tal-knob-canvas" id="tal-knob-eqPeakFreq" width="44" height="44"></canvas><span class="tal-rotary-label">PEAK FREQ</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqHighCut">${s.eqHighCut} Hz</span><canvas class="tal-knob-canvas" id="tal-knob-eqHighCut" width="44" height="44"></canvas><span class="tal-rotary-label">HIGH CUT</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqPeakGain">${s.eqPeakGain.toFixed(1)} dB</span><canvas class="tal-knob-canvas" id="tal-knob-eqPeakGain" width="44" height="44"></canvas><span class="tal-rotary-label">PEAK GAIN</span>
                                </div>
                            </div>
                        </div>
                        <!-- LO-FI -->
                        <div class="tal-btm-sub">
                            <div class="tal-section-header tal-sh-lofi">VINTAGE DAC</div>
                            <div class="tal-btm-knobs">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-lofiBits">${s.lofiBits} Bit</span><canvas class="tal-knob-canvas" id="tal-knob-lofiBits" width="44" height="44"></canvas><span class="tal-rotary-label">BITS</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-lofiSR">${s.lofiSR} Hz</span><canvas class="tal-knob-canvas" id="tal-knob-lofiSR" width="44" height="44"></canvas><span class="tal-rotary-label">SR</span>
                                </div>
                            </div>
                        </div>
                    </div>
                    <!-- Footer Bar -->
                    <div class="tal-footer-bar">
                        <div class="tal-footer-title">TAL REVERB 4 PRO • 4×4 HADAMARD FDN + EARLY IR</div>
                        <div>ZERO-METALLIC STUDIO REVERB ENGINE</div>
                    </div>
                </div>
                `;
            } else {
                return window.renderIrConvolverControls ? window.renderIrConvolverControls() : '';
            }
        }
        function syncAuxReverbDsp() {
            if (!window.meterWs || window.meterWs.readyState !== WebSocket.OPEN) return;
            if (reverbDspState.engine === 'atlantis') {
                const a = reverbDspState.atlantis;
                const wetPct = Math.max(0, Math.min(100, Math.pow(10, a.wet / 20) * 100));
                const szPct = Math.max(0, Math.min(100, (a.roomSize / 150) * 100));
                [["reverbSize", szPct], ["reverbDamp", a.damping], ["reverbPreDelay", a.preDelay], ["reverbIrMix", a.irMix ?? 55], ["reverbDiffuse", a.rev ? 100 : 92], ["reverbStereo", 100], ["reverbDry", 0], ["reverbWet", wetPct], ["reverbLowCut", a.preEqLow], ["reverbHighCut", a.postEqHigh], ["reverbModRate", 18], ["reverbModAmount", a.norm ? 35 : 18], ["reverbDucking", 0], ["reverbLofiBits", 24], ["reverbLofiSR", 48000]].forEach(([p, v]) => {
                    window.meterWs.send(JSON.stringify({ type: "dsp_param", channel: "master", param: p, value: v }));
                });
                return;
            }
            const s = reverbDspState.tal;
            [["reverbSize", s.size], ["reverbDamp", s.damp], ["reverbPreDelay", s.delay * 500], ["reverbIrMix", s.irMix ?? 35], ["reverbDiffuse", s.diffuse], ["reverbStereo", s.stereo], ["reverbDry", s.dry], ["reverbWet", s.wet], ["reverbLowCut", s.eqLowCut], ["reverbHighCut", s.eqHighCut], ["reverbModRate", s.modRate], ["reverbModAmount", s.modAmount], ["reverbDucking", s.ducking], ["reverbLofiBits", s.lofiBits], ["reverbLofiSR", s.lofiSR]].forEach(([p, v]) => {
                window.meterWs.send(JSON.stringify({ type: "dsp_param", channel: "master", param: p, value: v }));
            });
        }
        window.syncAuxReverbDsp = syncAuxReverbDsp;
        function cycleTalPreset(dir) {
            const s = reverbDspState.tal;
            const nextIdx = (s.presetIndex + dir + s.presets.length) % s.presets.length;
            applyQuickReverbPreset(nextIdx);
        }
        function toggleTalSidechain(btn) {
            reverbDspState.tal.sidechain = !reverbDspState.tal.sidechain;
            if (btn) btn.classList.toggle("active", reverbDspState.tal.sidechain);
            syncAuxReverbDsp();
        }
        let activeTalFader = null;
        function applyTalFaderAtClientY(clientY) {
            if (!activeTalFader) return;
            const r = activeTalFader.track.getBoundingClientRect();
            const travel = r.height - 16;
            if (travel <= 0) return;
            const pct = Math.max(0, Math.min(100, ((r.bottom - 8 - clientY) / travel) * 100));
            const param = activeTalFader.param;
            const dState = window.delayDspState;
            if (param === 'delay') {
                reverbDspState.tal.delay = pct / 100;
                const vEl = document.getElementById("tal-val-delay");
                if (vEl) vEl.innerText = Math.round((pct / 100) * 500) + " ms";
                syncAuxReverbDsp();
            } else if (param === 'irMix') {
                reverbDspState.tal.irMix = pct;
                const vEl = document.getElementById("tal-val-irMix");
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
                syncAuxReverbDsp();
            } else if (param === 'time' && dState) {
                const ms = Math.round(10 + (pct / 100) * 1490);
                dState.timeMs = ms;
                if (dState.syncToTempo) {
                    dState.syncToTempo = false;
                    const btn = document.getElementById("delay-tal-sync-btn");
                    if (btn) { btn.classList.remove("active"); btn.innerText = "TAP SYNC: OFF"; }
                }
                const vEl = document.getElementById("tal-val-time");
                if (vEl) vEl.innerText = ms + " ms";
                if (window.syncAuxDelayDsp) window.syncAuxDelayDsp();
            } else if (param === 'subdivision' && dState) {
                const subSteps = ["1/16", "1/8t", "1/8", "1/8d", "1/4"];
                const idx = Math.min(4, Math.max(0, Math.floor((pct / 100) * 5)));
                dState.subdivision = subSteps[idx];
                if (dState.syncToTempo && window.updateDelayTimeFromBpm) window.updateDelayTimeFromBpm();
                const vEl = document.getElementById("tal-val-subdivision");
                if (vEl) vEl.innerText = dState.subdivision;
            } else if (param === 'pingPong' && dState) {
                dState.pingPong = pct >= 50;
                const vEl = document.getElementById("tal-val-pingPong");
                if (vEl) vEl.innerText = dState.pingPong ? "PING-PONG" : "STEREO";
                if (window.syncAuxDelayDsp) window.syncAuxDelayDsp();
            } else if (param === 'delayDry' && dState) {
                dState.dry = pct;
                const vEl = document.getElementById("tal-val-delayDry");
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
                if (window.syncAuxDelayDsp) window.syncAuxDelayDsp();
            } else if (param === 'delayWet' && dState) {
                dState.wet = pct;
                const vEl = document.getElementById("tal-val-delayWet");
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
                if (window.syncAuxDelayDsp) window.syncAuxDelayDsp();
            } else if (dState && (param in dState)) {
                dState[param] = pct;
                const vEl = document.getElementById("tal-val-" + param);
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
                if (window.syncAuxDelayDsp) window.syncAuxDelayDsp();
            } else {
                reverbDspState.tal[param] = pct;
                const vEl = document.getElementById("tal-val-" + param);
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
                syncAuxReverbDsp();
            }
            activeTalFader.cap.style.bottom = pct + "%";
        }
        function startTalFaderDrag(param, evt) {
            const cap = document.getElementById("tal-cap-" + param);
            const track = document.getElementById("tal-track-" + param);
            if (!cap || !track) return;
            evt.preventDefault();
            activeTalFader = { param, track, cap };
            if (evt.pointerId !== undefined && track.setPointerCapture) {
                try { track.setPointerCapture(evt.pointerId); } catch (_) {}
            }
            applyTalFaderAtClientY(evt.clientY);
        }
        window.addEventListener("pointermove", (evt) => {
            if (!activeTalFader) return;
            applyTalFaderAtClientY(evt.clientY);
        });
        window.addEventListener("pointerup", () => { activeTalFader = null; });
        window.addEventListener("pointercancel", () => { activeTalFader = null; });
        function updateTalFadersVisual() {
            const s = reverbDspState.tal;
            ['size', 'damp', 'delay', 'irMix', 'diffuse', 'stereo', 'dry', 'wet'].forEach(p => {
                const cap = document.getElementById("tal-cap-" + p);
                const val = document.getElementById("tal-val-" + p);
                const pct = (p === 'delay') ? s.delay * 100 : (s[p] ?? 35);
                if (cap) cap.style.bottom = pct + "%";
                if (val) val.innerText = (p === 'delay') ? Math.round(s.delay * 500) + " ms" : pct.toFixed(1) + " %";
            });
        }
        // Rotary Canvas Knobs for TAL
        const talKnobDefs = {
            ducking: { min: 0, max: 100, unit: "%", color: "#38bdf8" },
            modRate: { min: 0, max: 100, unit: "%", color: "#c084fc" },
            modAmount: { min: 0, max: 100, unit: "%", color: "#c084fc" },
            tune: { min: -12, max: 12, unit: "", color: "#f472b6" },
            tuneAmount: { min: 0, max: 100, unit: "%", color: "#f472b6" },
            eqLowCut: { min: 20, max: 1000, unit: " Hz", color: "#60a5fa" },
            eqHighCut: { min: 1000, max: 18000, unit: " Hz", color: "#60a5fa" },
            eqPeakFreq: { min: 0, max: 8000, unit: " Hz", color: "#60a5fa" },
            eqPeakGain: { min: -12, max: 12, unit: " dB", color: "#60a5fa" },
            lofiBits: { min: 4, max: 24, unit: " Bit", color: "#fbbf24" },
            lofiSR: { min: 4000, max: 48000, unit: " Hz", color: "#fbbf24" }
        };
        function drawTalKnob(canvasId, normVal, color) {
            const cvs = document.getElementById(canvasId);
            if (!cvs) return;
            const ctx = cvs.getContext("2d");
            const w = cvs.width, h = cvs.height, cx = w / 2, cy = h / 2, radius = 16;
            ctx.clearRect(0, 0, w, h);
            const startAngle = 0.75 * Math.PI, endAngle = 2.25 * Math.PI;
            const currentAngle = startAngle + normVal * (endAngle - startAngle);
            ctx.beginPath(); ctx.arc(cx, cy, radius, startAngle, endAngle);
            ctx.strokeStyle = "#23272e"; ctx.lineWidth = 3; ctx.stroke();
            ctx.beginPath(); ctx.arc(cx, cy, radius, startAngle, currentAngle);
            ctx.strokeStyle = color; ctx.lineWidth = 3; ctx.stroke();
            ctx.beginPath(); ctx.arc(cx, cy, radius - 4, 0, 2 * Math.PI);
            const grad = ctx.createRadialGradient(cx - 3, cy - 3, 2, cx, cy, radius);
            grad.addColorStop(0, "#4a505a"); grad.addColorStop(1, "#17181c");
            ctx.fillStyle = grad; ctx.fill(); ctx.strokeStyle = "#000"; ctx.lineWidth = 1; ctx.stroke();
            ctx.beginPath(); ctx.moveTo(cx, cy);
            ctx.lineTo(cx + Math.cos(currentAngle) * (radius - 5), cy + Math.sin(currentAngle) * (radius - 5));
            ctx.strokeStyle = "#fff"; ctx.lineWidth = 2; ctx.stroke();
        }
        function initTalRotaries() {
            Object.keys(talKnobDefs).forEach(k => {
                const def = talKnobDefs[k];
                const defaultVal = reverbDspState.tal[k] ?? def.min;
                const norm = Math.max(0, Math.min(1, (defaultVal - def.min) / (def.max - def.min)));
                drawTalKnob("tal-knob-" + k, norm, def.color);
                const cvs = document.getElementById("tal-knob-" + k);
                if (cvs && !cvs.dataset.bound) {
                    cvs.dataset.bound = "true";
                    cvs.style.touchAction = "none";
                    let startX = 0, startY = 0, startVal = norm, lastTap = 0;
                    const applyNorm = (newNorm) => {
                        newNorm = Math.max(0, Math.min(1, newNorm));
                        const actualVal = def.min + newNorm * (def.max - def.min);
                        reverbDspState.tal[k] = (def.unit.includes("Bit") || def.unit.includes("Hz")) ? Math.round(actualVal) : parseFloat(actualVal.toFixed(2));
                        drawTalKnob("tal-knob-" + k, newNorm, def.color);
                        const valEl = document.getElementById("tal-val-" + k);
                        if (valEl) valEl.innerText = reverbDspState.tal[k] + def.unit;
                        syncAuxReverbDsp();
                    };
                    cvs.addEventListener("pointerdown", (e) => {
                        e.preventDefault();
                        const now = Date.now();
                        if (now - lastTap < 280) { applyNorm((defaultVal - def.min) / (def.max - def.min)); lastTap = 0; return; }
                        lastTap = now;
                        startX = e.clientX; startY = e.clientY;
                        startVal = (reverbDspState.tal[k] - def.min) / (def.max - def.min);
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
    window.reverbDspState = reverbDspState;
    window.switchReverbEngine = switchReverbEngine;
    window.renderReverbControls = renderReverbControls;
    window.cycleTalPreset = cycleTalPreset;
    window.toggleTalSidechain = toggleTalSidechain;
    window.startTalFaderDrag = startTalFaderDrag;
    window.updateTalFadersVisual = updateTalFadersVisual;
    window.drawTalKnob = drawTalKnob;
    window.initTalRotaries = initTalRotaries;
})(window);
