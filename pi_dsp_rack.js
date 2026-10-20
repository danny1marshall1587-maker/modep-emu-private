// --- COMPLETE CONSOLE GLUE & MISSING MATH HELPERS ---
// 1. ReEQ Canvas Inverse Math & Resize
window.xToFreq = window.xToFreq || function(x, width) {
  width = width || 280;
  var minF = Math.log10(20);
  var maxF = Math.log10(20000);
  var fraction = Math.max(0, Math.min(1, (x || 0) / width));
  return Math.round(Math.pow(10, minF + fraction * (maxF - minF)));
};

window.resizeReaEQCanvas = window.resizeReaEQCanvas || function() {
  var c = document.getElementById("reeq-canvas") || document.querySelector(".reeq-canvas");
  if (c && c.parentElement) {
    c.width = c.parentElement.clientWidth || 500;
    c.height = c.parentElement.clientHeight || 200;
  }
};

// 2. Button Handlers & Gain Alignment
// Rack gain handlers bound inside IIFE

// 3. De-Esser (LoaDeS) & Dynamic States
window.loadesState = window.loadesState || {
  threshold: -18.0,
  frequency: 6000.0,
  bandwidth: 2.0,
  reduction: 0.0,
  active: true
};
window.deessState = window.loadesState;

// 4. Default Channel States (Mode & DSP properties)
window.state = window.state || {};
window.channelState = window.channelState || {};
for (var i = 0; i <= 16; i++) {
  if (!window.state[i]) window.state[i] = { mode: 'rack', dspMode: 'rack', active: true };
  if (!window.channelState[i]) window.channelState[i] = { mode: 'rack', dspMode: 'rack', active: true };
}

// --- GLOBAL MIXER STATE & BUTTON HANDLERS ---
window.state = window.state || {};
window.FILTER_TYPES = ['bell', 'lowshelf', 'highshelf', 'lowpass', 'highpass', 'bandpass', 'notch'];

window.dbToY = window.dbToY || function(db, height) {
  height = height || 120;
  var clamped = Math.max(-24, Math.min(24, db || 0));
  return (height / 2) - (clamped / 24) * (height / 2);
};

window.fetish76State = window.fetish76State || {
  input: 0.0, output: 0.0, attack: 3.0, release: 3.0, ratio: 4, allButtons: false, bypass: false
};

// Dummy stub removed - using real getChannelRackState

// stub cleaned

// setChannelCompModel stub removed

// stub cleaned

// stub cleaned

// --- GLOBAL DSP & EQ HELPERS ---
window.freqToX = window.freqToX || function(freq, width) {
  width = width || 280;
  var minF = Math.log10(20);
  var maxF = Math.log10(20000);
  var f = Math.max(20, Math.min(20000, freq || 1000));
  return ((Math.log10(f) - minF) / (maxF - minF)) * width;
};

// stub cleaned

// stub cleaned

// --- REEQ BAND & STATE SUPPORT ---
window.REEQ_COLORS = window.REEQ_COLORS || [
  '#ef4444', '#f97316', '#f59e0b', '#10b981', '#06b6d4', '#3b82f6', '#8b5cf6', '#ec4899'
];
var REEQ_COLORS = window.REEQ_COLORS;

window.getChannelReEq = window.getChannelReEq || function(ch) {
  return {
    selectedBand: 0,
    bands: Array.from({ length: 8 }, function(_, idx) {
      return {
        enabled: true,
        freq: [32, 64, 125, 250, 1000, 2500, 8000, 16000][idx] || 1000,
        gain: 0.0,
        q: 1.0,
        type: idx === 0 ? 'hp' : (idx === 7 ? 'lp' : 'bell')
      };
    })
  };
};
var getChannelReEq = window.getChannelReEq;

// stub cleaned
// stub cleaned
// stub cleaned

const evalBandMagnitude = function(band, freq, srate) { return (window.evalBandMagnitude || function() { return 1.0; })(band, freq, srate); };
// dsp-rack.js - Channel DSP Rack, Compressors (FET 76 / LALA / Serial), De-Noiser, De-Esser, Analog Obsession Suite
(function(window) {
    'use strict';

        // ==========================================
        // 6b. Unified Channel DSP Rack State & Quick Modal
        // Exact Chain: Input Gain -> ReaEQ Parametric -> [Switchable Comp: FETISH 76 | LALA] -> LOADES De-Esser -> MODEP Insert Loop
        // ==========================================
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
                    gain: channelInputGains[ch] ?? 0.0,
                    phaseInvert: false,
                    dspChainOrder: ['input', 'denoiser', 'reeq', 'comp', 'deesser', 'modep'],
                    denoiser: { enabled: true, threshold: -45, reduction: -18, state: 'closed' },
                    compModel: 'fetish76', // 'fetish76' | 'lala' | 'dual'
                    compParams: {
                        fetish76: { input: 30, output: 18, attack: 3, release: 3, ratio: '4' },
                        lala: { peakReduction: 40, gain: 40, mode: 'compress' }
                    },
                    deesser: { threshold: -20, release: 50, mode: 'band', mix: 100 },
                    directTap: 'pre' // 'pre' (Clean Converter) | 'post' (Post-DSP Rig)
                };
            }
            return channelState[ch];
        }

        // Bidirectional De-Noiser Learn Protocol
        function triggerDenoiseLearn(ch) {
            const btn = document.getElementById(`btn-learn-noise-${ch}`);
            if (btn) {
                btn.classList.add('learning');
                btn.innerText = '⏺ ANALYZING NOISE...';
            }
            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                meterWs.send(JSON.stringify({ action: 'denoise_learn', ch: ch }));
            } else {
                // If offline, provide safe fallback values
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
            if (document.getElementById('dsp-rack-page').classList.contains('open')) {
                showDspRackPage(ch);
            } else if (document.getElementById('channel-rack-overlay').classList.contains('open')) {
                openChannelRackModal(ch);
            }
        }

        function setChannelDirectTap(ch, tap) {
            const st = getChannelRackState(ch);
            st.directTap = tap;
            dispatchDspParam(ch, 'directTap', tap);
            if (document.getElementById('dsp-rack-page').classList.contains('open')) {
                showDspRackPage(ch);
            } else if (document.getElementById('channel-rack-overlay').classList.contains('open')) {
                openChannelRackModal(ch);
            }
        }

        function togglePhaseInvert(ch) {
            const st = getChannelRackState(ch);
            st.phaseInvert = !st.phaseInvert;
            dispatchDspParam(ch, 'phaseInvert', st.phaseInvert);
            const btn1 = document.getElementById(`rack-phase-btn-${ch}`);
            const btn2 = document.getElementById(`rack-modal-phase-btn-${ch}`);
            [btn1, btn2].forEach(b => {
                if (b) {
                    b.classList.toggle('active', st.phaseInvert);
                    b.innerText = st.phaseInvert ? 'Ø INVERTED' : 'Ø NORMAL';
                }
            });
        }

        function openChannelRackModal(ch) {
            currentRackChannel = ch;
            const state = getChannelRackState(ch);
            const overlay = document.getElementById("channel-rack-overlay");
            const title = document.getElementById("channel-rack-modal-title");
            const grid = document.getElementById("channel-rack-grid");
            const chName = customChannelNames['ch-' + ch] || `Channel ${ch + 1}`;

            if (title) {
                title.innerHTML = `🎛️ IN ${ch + 1} &bull; ${chName} &bull; Channel FX Strip Rack`;
            }

            if (grid) {
                const comp = state.compModel === 'lala' ? state.compParams.lala : state.compParams.fetish76;
                grid.innerHTML = `
                    <!-- 1. Input Stage: Gain, Phase Invert, Regain -->
                    <div class="rack-row">
                        <div class="rack-header-area">
                            <span class="rack-num">1</span>
                            <div>
                                <div class="rack-name">🎚️ Input Stage &amp; Phase</div>
                                <div class="rack-sub">Pre-Gain &bull; Polarity Invert</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Gain</span>
                                <input type="range" class="rack-mini-slider" min="-18" max="24" step="0.5" value="${state.gain}" oninput="window.getChannelRackState(${ch}).gain=parseFloat(this.value); handleInputGainChange(${ch}, window.getChannelRackState(${ch}).gain); this.nextElementSibling.innerText=(this.value>=0?'+':'')+parseFloat(this.value).toFixed(1)+' dB'">
                                <span class="rack-mini-val">${state.gain >= 0 ? '+' : ''}${parseFloat(state.gain).toFixed(1)} dB</span>
                            </div>
                            <button class="tap-mode-btn ${state.phaseInvert ? 'active' : ''}" id="rack-modal-phase-btn-${ch}" onclick="togglePhaseInvert(${ch})">${state.phaseInvert ? 'Ø INVERTED' : 'Ø NORMAL'}</button>
                            <button class="btn-rack-bypass ${channelRegainActive[ch] ? 'active' : ''}" onclick="toggleRegain(${ch}); this.classList.toggle('active', channelRegainActive[ch]); this.innerText=channelRegainActive[ch]?'REGAIN ON':'REGAIN OFF';">${channelRegainActive[ch] ? 'REGAIN ON' : 'REGAIN OFF'}</button>
                        </div>
                    </div>

                    <!-- 2. Input Stage De-Noiser / Gate Module -->
                    <div class="rack-row" style="background:#0c131d; border-color:#1e293b;">
                        <div class="rack-header-area">
                            <span class="rack-num" style="color:#f59e0b; border-color:#d97706;">2</span>
                            <div>
                                <div class="rack-name" style="display:flex; align-items:center; color:#fde68a;">
                                    <span class="gate-led ${state.denoiser.enabled ? 'open' : 'closed'}" id="gate-led-modal-${ch}"></span>
                                    <span>🛡️ Input De-Noiser / Gate</span>
                                </div>
                                <div class="rack-sub">Noise Floor &bull; Learn Calibration</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <button class="tap-mode-btn ${state.denoiser.enabled ? 'active' : ''}" style="min-height:36px; padding:0 10px;" onclick="window.getChannelRackState(${ch}).denoiser.enabled=!window.getChannelRackState(${ch}).denoiser.enabled; dispatchDspParam(${ch},'denoiseBypass',!window.getChannelRackState(${ch}).denoiser.enabled); this.classList.toggle('active',window.getChannelRackState(${ch}).denoiser.enabled); this.innerText=window.getChannelRackState(${ch}).denoiser.enabled?'GATE ACTIVE':'BYPASS';">
                                ${state.denoiser.enabled ? 'GATE ACTIVE' : 'BYPASS'}
                            </button>
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Thresh</span>
                                <input type="range" class="rack-mini-slider" min="-60" max="0" step="1" value="${state.denoiser.threshold}" oninput="window.getChannelRackState(${ch}).denoiser.threshold=parseFloat(this.value); dispatchDspParam(${ch},'denoiseThresh',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                <span class="rack-mini-val">${state.denoiser.threshold} dB</span>
                            </div>
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Depth</span>
                                <input type="range" class="rack-mini-slider" min="-40" max="0" step="1" value="${state.denoiser.reduction}" oninput="window.getChannelRackState(${ch}).denoiser.reduction=parseFloat(this.value); dispatchDspParam(${ch},'denoiseReduc',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                <span class="rack-mini-val">${state.denoiser.reduction} dB</span>
                            </div>
                            <button class="btn-learn-noise" onclick="triggerDenoiseLearn(${ch})" title="Analyzes room tone">
                                ⏺ LEARN
                            </button>
                        </div>
                    </div>

                    <!-- 3. ReaEQ Graphic Parametric EQ -->
                    <div class="rack-row" style="border-color:#4338ca;">
                        <div class="rack-header-area">
                            <span class="rack-num" style="color:#818cf8; border-color:#818cf8;">3</span>
                            <div>
                                <div class="rack-name" style="color:#c7d2fe;">🎛️ ReaEQ JSFX Parametric EQ</div>
                                <div class="rack-sub">16-Band Cytomic SVF &bull; 64-Band RTA</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <span style="font-size:0.75rem; color:#818cf8; font-weight:bold;">Visual Interactive Curve Active &bull; 8 Filter Nodes</span>
                        </div>
                        <div class="rack-actions">
                            <button class="btn-rack-open" style="background:#4f46e5; border-color:#6366f1;" onclick="openDspModal('reeq', ${ch});">FULL SCREEN &rarr;</button>
                        </div>
                    </div>

                    <!-- 4. Switchable / Dual Compressor -->
                    <div class="rack-row" style="border-color:#374151; background:linear-gradient(90deg, rgba(30,41,59,0.3) 0%, rgba(15,23,42,0.6) 100%);">
                        <div class="rack-header-area">
                            <span class="rack-num" style="color:#38bdf8; border-color:#0284c7;">4</span>
                            <div>
                                <div class="rack-name" style="color:#e0f2fe;">⚡ Switchable Compressor</div>
                                <div class="comp-model-group" style="margin-top:4px;">
                                    <button class="comp-model-btn ${state.compModel === 'fetish76' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'fetish76')">FET 76</button>
                                    <button class="comp-model-btn ${state.compModel === 'lala' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'lala')">LALA</button>
                                    <button class="comp-model-btn ${state.compModel === 'dual' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'dual')">DUAL (76 ➔ LA)</button>
                                </div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            ${state.compModel === 'dual' ? `
                                <span style="font-size:0.75rem; color:#38bdf8; font-weight:800;">SERIAL: FETISH 76 (Peak Catcher) ➔ LALA (Opto Leveler)</span>
                            ` : (state.compModel === 'fetish76' ? `
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Input</span>
                                    <input type="range" class="rack-mini-slider" min="0" max="60" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.input:30}" oninput="getChannelRackState(${ch}).compParams.fetish76.input=parseInt(this.value); dispatchDspParam(${ch},'compInput',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                    <span class="rack-mini-val">${state.compParams.fetish76.input} dB</span>
                                </div>
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Output</span>
                                    <input type="range" class="rack-mini-slider" min="0" max="48" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.output:18}" oninput="getChannelRackState(${ch}).compParams.fetish76.output=parseInt(this.value); dispatchDspParam(${ch},'compOutput',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                    <span class="rack-mini-val">${state.compParams.fetish76.output} dB</span>
                                </div>
                            ` : `
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Peak Red.</span>
                                    <input type="range" class="rack-mini-slider" min="0" max="100" step="1" value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.peakReduction:40}" oninput="getChannelRackState(${ch}).compParams.lala.peakReduction=parseInt(this.value); dispatchDspParam(${ch},'lalaPeakRed',this.value); this.nextElementSibling.innerText=this.value">
                                    <span class="rack-mini-val">${state.compParams.lala.peakReduction}</span>
                                </div>
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Gain</span>
                                    <input type="range" class="rack-mini-slider" min="0" max="100" step="1" value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.gain:40}" oninput="getChannelRackState(${ch}).compParams.lala.gain=parseInt(this.value); dispatchDspParam(${ch},'lalaGain',this.value); this.nextElementSibling.innerText=this.value">
                                    <span class="rack-mini-val">${state.compParams.lala.gain}</span>
                                </div>
                            `)}
                        </div>
                        <div class="rack-actions">
                            <button class="btn-rack-open" style="background:#0284c7; border-color:#38bdf8;" onclick="openDspModal('${state.compModel === 'lala' ? 'la2a' : 'fet1176'}', ${ch});">${state.compModel === 'lala' ? 'LALA GUI &rarr;' : '76 GUI &rarr;'}</button>
                        </div>
                    </div>

                    <!-- 5. Analog Obsession LOADES De-Esser -->
                    <div class="rack-row">
                        <div class="rack-header-area">
                            <span class="rack-num">5</span>
                            <div>
                                <div class="rack-name">🎙️ LOADES De-Esser</div>
                                <div class="rack-sub">Dynamic Sibilance Control</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Threshold</span>
                                <input type="range" class="rack-mini-slider" min="-40" max="0" step="1" value="${state.deesser.threshold}" oninput="window.getChannelRackState(${ch}).deesser.threshold=parseInt(this.value); dispatchDspParam(${ch},'loadesThresh',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                <span class="rack-mini-val">${state.deesser.threshold} dB</span>
                            </div>
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Release</span>
                                <input type="range" class="rack-mini-slider" min="10" max="200" step="5" value="${state.deesser.release}" oninput="window.getChannelRackState(${ch}).deesser.release=parseInt(this.value); dispatchDspParam(${ch},'loadesRelease',this.value); this.nextElementSibling.innerText=this.value+' ms'">
                                <span class="rack-mini-val">${state.deesser.release} ms</span>
                            </div>
                        </div>
                        <div class="rack-actions">
                            <button class="btn-rack-open" onclick="openDspModal('deesser', ${ch});">DETAIL &rarr;</button>
                        </div>
                    </div>

                    <!-- 6. MODEP Insert Loop -->
                    <div class="rack-row" style="background:#090d14; border-color:#1e293b;">
                        <div class="rack-header-area">
                            <span class="rack-num" style="color:#10b981; border-color:#059669;">6</span>
                            <div>
                                <div class="rack-name" style="color:#ecfdf5;">🎸 MODEP Insert Loop</div>
                                <div class="rack-sub">Pedalboard Hardware Insert &bull; Direct Tap</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <button class="btn-super-insert modep-insert-btn" style="margin-bottom:0; min-height:36px; padding:0 14px;" onclick="openModepInsert(${ch + 1});">
                                <span>🎸 OPEN MODEP INSERT ${Math.floor(ch / 2) + 1}</span>
                            </button>
                        </div>
                    </div>
`;
            }
            overlay.classList.add("open");
        }

        function closeChannelRackModal() {
            const overlay = document.getElementById("channel-rack-overlay");
            if (overlay) overlay.classList.remove("open");
        }

        // ==========================================
        // 6c. DSP RACK FULL-PAGE VIEW
        // Full screen page replacing the mixer canvas, with mini mixer sidebar
        // Exact Chain: Input Gain -> [Switchable Comp: FETISH 76 | LALA] -> ReaEQ -> De-Esser (LOADES) -> Direct Tap
        // ==========================================
        let dspRackCurrentChannel = 0;

        window.showDspRackPage = function(ch) {
            dspRackCurrentChannel = ch;
            const state = getChannelRackState(ch);
            const chName = customChannelNames['ch-' + ch] || `Channel ${ch + 1}`;
            const gain = channelInputGains[ch] ?? 0;
            const regainActive = channelRegainActive[ch] ?? true;

            const titleEl = document.getElementById('dsp-rack-page-title');
            if (titleEl) titleEl.innerHTML = `🎛️ IN ${ch + 1} — ${chName} &bull; DSP Rack`;

            const gainSlider = document.getElementById('dsp-rack-gain-slider');
            const gainVal = document.getElementById('dsp-rack-gain-val');
            const gainLabel = document.getElementById('dsp-rack-gain-label');
            const regainBtn = document.getElementById('dsp-rack-regain-btn');
            if (gainSlider) gainSlider.value = gain;
            if (gainVal) gainVal.innerText = (gain >= 0 ? '+' : '') + parseFloat(gain).toFixed(1) + ' dB';
            if (gainLabel) gainLabel.innerText = `INPUT GAIN — IN ${ch + 1} (UMC1820)`;
            if (regainBtn) {
                regainBtn.classList.toggle('active', regainActive);
                regainBtn.innerText = regainActive ? 'REGAIN ON' : 'REGAIN OFF';
            }

            const grid = document.getElementById('dsp-rack-grid');
            if (grid) {
                grid.innerHTML = `
                    <!-- 1. Input Stage: Gain, Phase Invert, Regain -->
                    <div class="rack-row">
                        <div class="rack-header-area">
                            <span class="rack-num">1</span>
                            <div>
                                <div class="rack-name">🎚️ Input Stage &amp; Phase</div>
                                <div class="rack-sub">Polarity &bull; Dynamic REGAIN</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <button class="tap-mode-btn ${state.phaseInvert ? 'active' : ''}" id="rack-phase-btn-${ch}" style="min-height:42px; min-width:130px; font-weight:900;" onclick="togglePhaseInvert(${ch})">
                                ${state.phaseInvert ? 'Ø INVERTED' : 'Ø NORMAL'}
                            </button>
                            <span style="font-size:0.75rem; color:#94a3b8; font-family:monospace; margin-left:12px;">Gain: ${(gain >= 0 ? '+' : '') + parseFloat(gain).toFixed(1)} dB &bull; Regain: ${regainActive ? 'ACTIVE' : 'OFF'}</span>
                        </div>
                        <div class="rack-actions">
                            <span style="font-size:0.72rem; color:#38bdf8; font-weight:bold;">STAGE 1 / 6</span>
                        </div>
                    </div>

                    <!-- 2. Input Stage De-Noiser / Gate Module -->
                    <div class="rack-row" style="background:#0c131d; border-color:#1e293b;">
                        <div class="rack-header-area">
                            <span class="rack-num" style="color:#f59e0b; border-color:#d97706;">2</span>
                            <div>
                                <div class="rack-name" style="display:flex; align-items:center; color:#fde68a;">
                                    <span class="gate-led ${state.denoiser.enabled ? 'open' : 'closed'}" id="gate-led-${ch}" title="Gate Status: Green=Open, Amber=Closed"></span>
                                    <span>🛡️ Input De-Noiser / Dynamic Gate</span>
                                </div>
                                <div class="rack-sub">Noise Floor Attenuator &bull; Spectral Subtraction</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <button class="tap-mode-btn ${state.denoiser.enabled ? 'active' : ''}" style="min-height:36px; padding:0 12px;" onclick="window.getChannelRackState(${ch}).denoiser.enabled=!window.getChannelRackState(${ch}).denoiser.enabled; dispatchDspParam(${ch},'denoiseBypass',!window.getChannelRackState(${ch}).denoiser.enabled); this.classList.toggle('active',window.getChannelRackState(${ch}).denoiser.enabled); this.innerText=window.getChannelRackState(${ch}).denoiser.enabled?'GATE ACTIVE':'BYPASS';">
                                ${state.denoiser.enabled ? 'GATE ACTIVE' : 'BYPASS'}
                            </button>
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Threshold</span>
                                <input type="range" class="rack-mini-slider" id="denoise-thresh-slider-${ch}" style="width:100px;" min="-60" max="0" step="1" value="${state.denoiser.threshold}" oninput="window.getChannelRackState(${ch}).denoiser.threshold=parseFloat(this.value); dispatchDspParam(${ch},'denoiseThresh',this.value); document.getElementById('denoise-thresh-val-${ch}').innerText=this.value+' dB'">
                                <span class="rack-mini-val" id="denoise-thresh-val-${ch}">${state.denoiser.threshold} dB</span>
                            </div>
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Reduction</span>
                                <input type="range" class="rack-mini-slider" id="denoise-reduc-slider-${ch}" style="width:100px;" min="-40" max="0" step="1" value="${state.denoiser.reduction}" oninput="window.getChannelRackState(${ch}).denoiser.reduction=parseFloat(this.value); dispatchDspParam(${ch},'denoiseReduc',this.value); document.getElementById('denoise-reduc-val-${ch}').innerText=this.value+' dB'">
                                <span class="rack-mini-val" id="denoise-reduc-val-${ch}">${state.denoiser.reduction} dB</span>
                            </div>
                            <button class="btn-learn-noise" id="btn-learn-noise-${ch}" onclick="triggerDenoiseLearn(${ch})" title="Analyzes room tone and locks noise gate threshold">
                                ⏺ LEARN NOISE
                            </button>
                        </div>
                        <div class="rack-actions">
                            <button class="btn-rack-open" onclick="openDspModal('denoiser', ${ch});">DETAIL &rarr;</button>
                        </div>
                    </div>

                    <!-- 3. ReaEQ JSFX Cytomic SVF Equaliser -->
                    <div class="rack-row rack-eq-card">
                        <div class="rack-eq-topbar">
                            <div class="rack-header-area">
                                <span class="rack-num" style="color:#818cf8; border-color:#818cf8;">3</span>
                                <div>
                                    <div class="rack-name" style="color:#c7d2fe; font-size:0.95rem;">🎛️ ReaEQ JSFX Graphic Parametric Equaliser</div>
                                    <div class="rack-sub" style="color:#818cf8;">Cytomic SVF &bull; 8 Interactive Filter Nodes &bull; Live 64-Band RTA Underlay</div>
                                </div>
                            </div>
                            <div style="display:flex; align-items:center; gap:10px;">
                                <div style="display:flex; flex-direction:column; gap:2px; font-size:0.75rem; color:#818cf8; font-family:monospace; min-width:130px; text-align:right;">
                                    <span id="mini-reeq-band-lbl" style="font-weight:bold;">B1: Low Cut</span>
                                    <span id="mini-reeq-val-lbl" style="color:#fff; font-size:0.85rem;">30Hz 0.0dB</span>
                                </div>
                                <div class="rack-actions">
                                    <button class="btn-rack-open" style="background:#4f46e5; border-color:#6366f1; min-height:44px; padding:6px 14px; font-size:0.8rem;" onclick="openDspModal('reeq', ${ch});">FULL SCREEN &rarr;</button>
                                </div>
                            </div>
                        </div>

                        <!-- Large Interactive Frequency Spectrum Canvas with RTA Underlay -->
                        <div class="rack-eq-canvas-wrap">
                            <canvas id="dsp-rack-reeq-mini"></canvas>
                        </div>

                        <!-- Quick Node Touch Strip for 8 Bands -->
                        <div class="rack-eq-quick-strip" id="rack-eq-quick-strip">
                            ${(() => {
                                const eqSt = getChannelReEq(ch);
                                return eqSt.bands.map((b, idx) => `
                                    <div class="rack-eq-quick-node ${eqSt.selectedBand === idx ? 'active' : ''}" id="rack-quick-node-${idx}" onclick="selectRackEqBand(${ch}, ${idx})">
                                        <div style="display:flex; justify-content:space-between; align-items:center; font-size:0.7rem; font-weight:800; color:${REEQ_COLORS[idx % REEQ_COLORS.length]};">
                                             <span>B${idx+1}</span>
                                            <input type="checkbox" ${b.enabled ? 'checked' : ''} onclick="event.stopPropagation(); toggleReEqBand(${idx}); renderMiniReEq(${ch});" style="width:14px; height:14px; cursor:pointer;">
                                        </div>
                                        <div style="font-size:0.75rem; font-weight:800; color:#fff;" id="rack-node-freq-${idx}">${b.freq >= 1000 ? (b.freq/1000).toFixed(1)+'k' : b.freq+'Hz'}</div>
                                        <div style="font-size:0.7rem; color:${b.gain >= 0 ? '#34d399' : '#f87171'};" id="rack-node-gain-${idx}">${(b.gain >= 0 ? '+' : '') + b.gain + 'dB'}</div>
                                    </div>
                                `).join('');
                            })()}
                        </div>
                    </div>

                    <!-- 4. Switchable / Dual Compressor: FETISH 76 | LALA | DUAL STACK -->
                    <div class="rack-row" style="border-color:#374151; background:linear-gradient(90deg, rgba(30,41,59,0.3) 0%, rgba(15,23,42,0.6) 100%);">
                        <div class="rack-header-area">
                            <span class="rack-num" style="color:#38bdf8; border-color:#0284c7;">4</span>
                            <div>
                                <div class="rack-name" style="color:#e0f2fe;">⚡ Dynamics: Compressor Stacking</div>
                                <div class="comp-model-group" style="margin-top:4px;">
                                    <button class="comp-model-btn ${state.compModel === 'fetish76' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'fetish76')">FET 76</button>
                                    <button class="comp-model-btn ${state.compModel === 'lala' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'lala')">LALA</button>
                                    <button class="comp-model-btn ${state.compModel === 'dual' ? 'active' : ''}" onclick="setChannelCompModel(${ch}, 'dual')">DUAL (76 ➔ LA)</button>
                                </div>
                            </div>
                        </div>
                        <div class="rack-inline-controls" style="flex:1;">
                            ${state.compModel === 'dual' ? `
                                <div class="dual-comp-container">
                                    <!-- Stage A: FETISH 76 (Peak Catcher) -->
                                    <div class="dual-comp-row">
                                        <span class="dual-comp-badge fet">1. FET 76 (PEAK)</span>
                                        <div class="rack-mini-knob">
                                            <span class="rack-mini-label">In</span>
                                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="60" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.input:30}" oninput="getChannelRackState(${ch}).compParams.fetish76.input=parseInt(this.value); dispatchDspParam(${ch},'compInput',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                            <span class="rack-mini-val">${state.compParams.fetish76.input} dB</span>
                                        </div>
                                        <div class="rack-mini-knob">
                                            <span class="rack-mini-label">Out</span>
                                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="48" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.output:18}" oninput="getChannelRackState(${ch}).compParams.fetish76.output=parseInt(this.value); dispatchDspParam(${ch},'compOutput',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                            <span class="rack-mini-val">${state.compParams.fetish76.output} dB</span>
                                        </div>
                                        <span style="font-size:0.68rem; color:#38bdf8; font-family:monospace;">GR: <b id="dual-fet-gr-${ch}">0.0</b> dB</span>
                                    </div>
                                    <!-- Stage B: LALA (Optical Leveler) -->
                                    <div class="dual-comp-row">
                                        <span class="dual-comp-badge la">2. LALA (OPTO)</span>
                                        <div class="rack-mini-knob">
                                            <span class="rack-mini-label">Peak Red</span>
                                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="100" step="1" value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.peakReduction:40}" oninput="getChannelRackState(${ch}).compParams.lala.peakReduction=parseInt(this.value); dispatchDspParam(${ch},'lalaPeakRed',this.value); this.nextElementSibling.innerText=this.value">
                                            <span class="rack-mini-val">${state.compParams.lala.peakReduction}</span>
                                        </div>
                                        <div class="rack-mini-knob">
                                            <span class="rack-mini-label">Gain</span>
                                            <input type="range" class="rack-mini-slider" style="width:75px;" min="0" max="100" step="1" value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.gain:40}" oninput="getChannelRackState(${ch}).compParams.lala.gain=parseInt(this.value); dispatchDspParam(${ch},'lalaGain',this.value); this.nextElementSibling.innerText=this.value">
                                            <span class="rack-mini-val">${state.compParams.lala.gain}</span>
                                        </div>
                                        <span style="font-size:0.68rem; color:#f59e0b; font-family:monospace;">GR: <b id="dual-lala-gr-${ch}">0.0</b> dB</span>
                                    </div>
                                </div>
                            ` : (state.compModel === 'fetish76' ? `
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Input</span>
                                    <input type="range" class="rack-mini-slider" style="width:90px;" min="0" max="60" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.input:30}" oninput="getChannelRackState(${ch}).compParams.fetish76.input=parseInt(this.value); dispatchDspParam(${ch},'compInput',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                    <span class="rack-mini-val">${state.compParams.fetish76.input} dB</span>
                                </div>
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Output</span>
                                    <input type="range" class="rack-mini-slider" style="width:90px;" min="0" max="48" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.output:18}" oninput="getChannelRackState(${ch}).compParams.fetish76.output=parseInt(this.value); dispatchDspParam(${ch},'compOutput',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                    <span class="rack-mini-val">${state.compParams.fetish76.output} dB</span>
                                </div>
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Attack</span>
                                    <input type="range" class="rack-mini-slider" style="width:75px;" min="1" max="7" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.attack:3}" oninput="getChannelRackState(${ch}).compParams.fetish76.attack=parseInt(this.value); dispatchDspParam(${ch},'compAttack',this.value); this.nextElementSibling.innerText=this.value">
                                    <span class="rack-mini-val">${state.compParams.fetish76.attack}</span>
                                </div>
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Release</span>
                                    <input type="range" class="rack-mini-slider" style="width:75px;" min="1" max="7" step="1" value="${(state.compParams&&state.compParams.fetish76)?state.compParams.fetish76.release:3}" oninput="getChannelRackState(${ch}).compParams.fetish76.release=parseInt(this.value); dispatchDspParam(${ch},'compRelease',this.value); this.nextElementSibling.innerText=this.value">
                                    <span class="rack-mini-val">${state.compParams.fetish76.release}</span>
                                </div>
                            ` : `
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Peak Red.</span>
                                    <input type="range" class="rack-mini-slider" style="width:110px;" min="0" max="100" step="1" value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.peakReduction:40}" oninput="getChannelRackState(${ch}).compParams.lala.peakReduction=parseInt(this.value); dispatchDspParam(${ch},'lalaPeakRed',this.value); this.nextElementSibling.innerText=this.value">
                                    <span class="rack-mini-val">${state.compParams.lala.peakReduction}</span>
                                </div>
                                <div class="rack-mini-knob">
                                    <span class="rack-mini-label">Gain</span>
                                    <input type="range" class="rack-mini-slider" style="width:110px;" min="0" max="100" step="1" value="${(state.compParams&&state.compParams.lala)?state.compParams.lala.gain:40}" oninput="getChannelRackState(${ch}).compParams.lala.gain=parseInt(this.value); dispatchDspParam(${ch},'lalaGain',this.value); this.nextElementSibling.innerText=this.value">
                                    <span class="rack-mini-val">${state.compParams.lala.gain}</span>
                                </div>
                                <button class="tap-mode-btn ${state.compParams.lala.mode === 'limit' ? 'active' : ''}" style="margin-left:8px;" onclick="window.getChannelRackState(${ch}).compParams.lala.mode=window.getChannelRackState(${ch}).compParams.lala.mode==='limit'?'compress':'limit'; dispatchDspParam(${ch},'lalaMode',window.getChannelRackState(${ch}).compParams.lala.mode); this.innerText=window.getChannelRackState(${ch}).compParams.lala.mode.toUpperCase(); this.classList.toggle('active', window.getChannelRackState(${ch}).compParams.lala.mode==='limit');">${state.compParams.lala.mode.toUpperCase()}</button>
                            `)}
                        </div>
                        <div class="rack-actions">
                            <button class="btn-rack-open" style="background:#0284c7; border-color:#38bdf8;" onclick="openDspModal('${state.compModel === 'lala' ? 'la2a' : 'fet1176'}', ${ch});">${state.compModel === 'lala' ? 'LALA GUI &rarr;' : (state.compModel === 'dual' ? 'DUAL GUI &rarr;' : 'AO 76 GUI &rarr;')}</button>
                        </div>
                    </div>

                    <!-- 5. Analog Obsession LOADES De-Esser -->
                    <div class="rack-row">
                        <div class="rack-header-area">
                            <span class="rack-num">5</span>
                            <div>
                                <div class="rack-name">🎙️ Analog Obsession LOADES De-Esser</div>
                                <div class="rack-sub">Dynamic Sibilance Attenuation &bull; Split-Band</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Threshold</span>
                                <input type="range" class="rack-mini-slider" style="width:110px;" min="-40" max="0" step="1" value="${state.deesser.threshold}" oninput="window.getChannelRackState(${ch}).deesser.threshold=parseInt(this.value); dispatchDspParam(${ch},'loadesThresh',this.value); this.nextElementSibling.innerText=this.value+' dB'">
                                <span class="rack-mini-val">${state.deesser.threshold} dB</span>
                            </div>
                            <div class="rack-mini-knob">
                                <span class="rack-mini-label">Release</span>
                                <input type="range" class="rack-mini-slider" style="width:110px;" min="10" max="200" step="5" value="${state.deesser.release}" oninput="window.getChannelRackState(${ch}).deesser.release=parseInt(this.value); dispatchDspParam(${ch},'loadesRelease',this.value); this.nextElementSibling.innerText=this.value+' ms'">
                                <span class="rack-mini-val">${state.deesser.release} ms</span>
                            </div>
                            <button class="tap-mode-btn ${state.deesser.mode === 'wide' ? 'active' : ''}" style="margin-left:8px;" onclick="window.getChannelRackState(${ch}).deesser.mode=window.getChannelRackState(${ch}).deesser.mode==='band'?'wide':'band'; dispatchDspParam(${ch},'loadesMode',window.getChannelRackState(${ch}).deesser.mode); this.innerText=window.getChannelRackState(${ch}).deesser.mode.toUpperCase(); this.classList.toggle('active', window.getChannelRackState(${ch}).deesser.mode==='wide');">
                                ${state.deesser.mode.toUpperCase()}
                            </button>
                        </div>
                        <div class="rack-actions">
                            <button class="btn-rack-open" onclick="openDspModal('deesser', ${ch});">DETAIL &rarr;</button>
                        </div>
                    </div>

                    <!-- 6. MODEP Insert Loop -->
                    <div class="rack-row" style="background:#090d14; border-color:#1e293b;">
                        <div class="rack-header-area">
                            <span class="rack-num" style="color:#10b981; border-color:#059669;">6</span>
                            <div>
                                <div class="rack-name" style="color:#ecfdf5;">🎸 MODEP Insert Loop</div>
                                <div class="rack-sub">Pedalboard Hardware Insert &bull; Direct Tap</div>
                            </div>
                        </div>
                        <div class="rack-inline-controls">
                            <button class="btn-super-insert modep-insert-btn" style="margin-bottom:0; min-height:36px; padding:0 14px;" onclick="openModepInsert(${ch + 1});">
                                <span>🎸 OPEN MODEP INSERT ${Math.floor(ch / 2) + 1}</span>
                            </button>
                            <div class="tap-mode-group" style="margin-left:12px;">
                                <button class="tap-mode-btn ${state.directTap === 'pre' ? 'active' : ''}" onclick="setChannelDirectTap(${ch}, 'pre')">PRE</button>
                                <button class="tap-mode-btn ${state.directTap === 'post' ? 'active' : ''}" onclick="setChannelDirectTap(${ch}, 'post')">POST</button>
                            </div>
                        </div>
                        <div class="rack-actions">
                            <span style="font-size:0.8rem; color:#10b981; font-weight:800; font-family:monospace;">${state.directTap === 'pre' ? 'TAP: PRE-DSP' : 'TAP: POST-DSP'}</span>
                        </div>
                    </div>
`;
            }

            updateFocusedChannelStrip(ch);
            document.getElementById('dsp-rack-page').classList.add('open');
            setTimeout(() => {
                renderMiniReEq(ch);
                applySlowModeToAllSliders(); // Bind damped drag to all rack sliders
            }, 30);
        }

        function renderMiniReEq(ch) {
            const canvas = document.getElementById("dsp-rack-reeq-mini");
            if (!canvas) return;
            const ctx = canvas.getContext("2d");
            const dpr = window.devicePixelRatio || 1;
            const w = canvas.clientWidth || 700;
            const h = canvas.clientHeight || 220;
            canvas.width = Math.round(w * dpr);
            canvas.height = Math.round(h * dpr);
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
            const state = getChannelReEq(ch);
            const range = state.dbRange;

            ctx.clearRect(0, 0, w, h);
            ctx.fillStyle = "#0c0e14";
            ctx.fillRect(0, 0, w, h);

            // Horizontal dB grid lines
            const dbStep = range >= 24 ? 6 : (range >= 18 ? 3 : 2);
            ctx.font = "9px -apple-system, monospace";
            for (let db = -range; db <= range; db += dbStep) {
                const y = dbToY(db, h, range);
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(w, y);
                if (db === 0) {
                    ctx.strokeStyle = "rgba(56, 189, 248, 0.5)";
                    ctx.lineWidth = 1.5;
                } else {
                    ctx.strokeStyle = "rgba(255, 255, 255, 0.07)";
                    ctx.lineWidth = 1;
                }
                ctx.stroke();

                ctx.fillStyle = db === 0 ? "#38bdf8" : "rgba(255, 255, 255, 0.35)";
                ctx.textAlign = "right";
                ctx.fillText((db > 0 ? "+" : "") + db + " dB", w - 6, y - 2);
            }

            // Minor grid lines
            const minorGridFreqs = [30, 40, 60, 70, 80, 90, 600, 700, 800, 900, 4000, 6000, 7000, 8000];
            ctx.beginPath();
            minorGridFreqs.forEach(f => {
                const x = freqToX(f, w);
                ctx.moveTo(x, 0);
                ctx.lineTo(x, h - 16);
            });
            ctx.strokeStyle = "rgba(255, 255, 255, 0.07)";
            ctx.lineWidth = 1;
            ctx.stroke();

            // Frequency lines (20, 50, 100, 200, 300, 400, 500, 1k, 2k, 3k, 5k, 10k, 20k)
            const gridFreqs = [20, 50, 100, 200, 300, 400, 500, 1000, 2000, 3000, 5000, 10000, 20000];
            gridFreqs.forEach(f => {
                const x = freqToX(f, w);
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, h - 16);
                ctx.strokeStyle = (f === 100 || f === 1000 || f === 10000) ? "rgba(255, 255, 255, 0.12)" : "rgba(255, 255, 255, 0.05)";
                ctx.lineWidth = 1;
                ctx.stroke();

                ctx.fillStyle = "rgba(255, 255, 255, 0.4)";
                ctx.textAlign = "center";
                const label = f >= 1000 ? (f / 1000) + "k" : f + "Hz";
                ctx.fillText(label, x, h - 4);
            });

            const midY = dbToY(0, h, range);

            // Live 64-Band RTA Underlay in Mini Rack EQ
            if (window.liveRtaBins && window.liveRtaBins.length > 0) {
                ctx.save();
                ctx.beginPath();
                ctx.moveTo(0, h);
                const rtaLen = window.liveRtaBins.length;
                for (let b = 0; b < rtaLen; b++) {
                    const binFreq = 20 * Math.pow(1000, b / (rtaLen - 1));
                    const x = freqToX(binFreq, w);
                    const binDb = Math.max(-80, Math.min(0, window.liveRtaBins[b] || -80));
                    const y = dbToY(binDb, h, range);
                    ctx.lineTo(x, y);
                }
                ctx.lineTo(w, h);
                ctx.closePath();
                ctx.fillStyle = "rgba(56, 189, 248, 0.18)";
                ctx.fill();
                ctx.restore();
            }

            // Draw individual band curves
            state.bands.forEach((b, idx) => {
                if (!b.enabled) return;
                const color = REEQ_COLORS[idx % REEQ_COLORS.length];
                const isSelected = state.selectedBand === idx;

                ctx.beginPath();
                for (let x = 0; x <= w; x += 3) {
                    const freq = xToFreq(x, w);
                    const mag = evalBandMagnitude(b, freq, 48000);
                    const db = 20 * Math.log10(Math.max(0.0001, mag));
                    const y = dbToY(db, h, range);
                    if (x === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.strokeStyle = color;
                ctx.lineWidth = isSelected ? 1.8 : 1.0;
                ctx.globalAlpha = isSelected ? 0.85 : 0.35;
                ctx.stroke();
                ctx.globalAlpha = 1.0;
            });

            // Master Composite Curve
            ctx.beginPath();
            for (let x = 0; x <= w; x += 2) {
                const freq = xToFreq(x, w);
                let totalMag = 1.0;
                state.bands.forEach(b => {
                    totalMag *= evalBandMagnitude(b, freq, 48000);
                });
                const totalDb = 20 * Math.log10(Math.max(0.0001, totalMag));
                const y = dbToY(totalDb, h, range);
                if (x === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.strokeStyle = "#818cf8";
            ctx.lineWidth = 2.2;
            ctx.stroke();

            // Fill under master curve
            ctx.lineTo(w, midY);
            ctx.lineTo(0, midY);
            ctx.fillStyle = "rgba(129, 140, 248, 0.12)";
            ctx.fill();

            // Band Node Points with Q wing lines for selected band
            state.bands.forEach((b, idx) => {
                if (!b.enabled) return;
                const x = freqToX(b.freq, w);
                const y = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8) 
                    ? dbToY(0, h, range) 
                    : dbToY(b.gain, h, range);
                const color = REEQ_COLORS[idx % REEQ_COLORS.length];
                const isSelected = state.selectedBand === idx;

                // Q Wing indicators if band is selected
                if (isSelected && b.type !== 1 && b.type !== 2) {
                    const bwOct = 2 * Math.asinh(1 / (2 * Math.max(0.1, b.q))) / Math.LN2;
                    const leftF = Math.max(REEQ_MIN_FREQ, b.freq * Math.pow(2, -bwOct / 2));
                    const rightF = Math.min(REEQ_MAX_FREQ, b.freq * Math.pow(2, bwOct / 2));
                    const lx = freqToX(leftF, w);
                    const rx = freqToX(rightF, w);

                    ctx.beginPath();
                    ctx.moveTo(lx, y);
                    ctx.lineTo(rx, y);
                    ctx.strokeStyle = color;
                    ctx.lineWidth = 1.5;
                    ctx.stroke();

                    [lx, rx].forEach(px => {
                        ctx.beginPath();
                        ctx.arc(px, y, 3.5, 0, Math.PI * 2);
                        ctx.fillStyle = "#ffffff";
                        ctx.fill();
                    });
                }

                // Node Body
                ctx.beginPath();
                ctx.arc(x, y, isSelected ? 12 : 8, 0, Math.PI * 2);
                ctx.fillStyle = color;
                ctx.fill();
                ctx.strokeStyle = "#ffffff";
                ctx.lineWidth = isSelected ? 2 : 1;
                ctx.stroke();

                ctx.fillStyle = "#ffffff";
                ctx.font = `bold ${isSelected ? '10px' : '8px'} sans-serif`;
                ctx.textAlign = "center";
                ctx.textBaseline = "middle";
                ctx.fillText(idx + 1, x, y);
            });

            // Interactive Mini Touch / Click Drag Handling
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
                    const st = getChannelReEq(dspRackCurrentChannel);
                    const curW = canvas.clientWidth || 700;
                    const curH = canvas.clientHeight || 220;
                    let hit = -1, minD = 28;
                    st.bands.forEach((b, i) => {
                        if (!b.enabled) return;
                        const nx = freqToX(b.freq, curW);
                        const ny = (b.type === 1 || b.type === 2 || b.type === 5 || b.type === 6 || b.type === 7 || b.type === 8)
                            ? dbToY(0, curH, st.dbRange)
                            : dbToY(b.gain, curH, st.dbRange);
                        const d = Math.hypot(pos.x - nx, pos.y - ny);
                        if (d < minD) { minD = d; hit = i; }
                    });
                    if (hit !== -1) {
                        activeMiniNode = hit;
                        st.selectedBand = hit;
                        updateMiniLabels(st, hit);
                        updateQuickNodeCardUI(hit);
                        renderMiniReEq(dspRackCurrentChannel);
                    }
                }

                function onMove(e) {
                    if (activeMiniNode === -1) return;
                    e.preventDefault();
                    const pos = getMiniPos(e);
                    const st = getChannelReEq(dspRackCurrentChannel);
                    const b = st.bands[activeMiniNode];
                    const curW = canvas.clientWidth || 700;
                    const curH = canvas.clientHeight || 220;
                    let targetFreq = Math.round(xToFreq(pos.x, curW));
                    if (window.touchGovernor) {
                        targetFreq = Math.round(window.touchGovernor.process(`reeq-mini-freq-${dspRackCurrentChannel}-${activeMiniNode}`, targetFreq));
                    }
                    b.freq = targetFreq;
                    if (b.type !== 1 && b.type !== 2 && b.type !== 5 && b.type !== 6 && b.type !== 7 && b.type !== 8) {
                        let targetGain = parseFloat(yToDb(pos.y, curH, st.dbRange).toFixed(1));
                        if (window.touchGovernor) {
                            targetGain = parseFloat(window.touchGovernor.process(`reeq-mini-gain-${dspRackCurrentChannel}-${activeMiniNode}`, targetGain).toFixed(1));
                        }
                        b.gain = targetGain;
                    }
                    updateMiniLabels(st, activeMiniNode);
                    updateQuickNodeCardUI(activeMiniNode);
                    renderMiniReEq(dspRackCurrentChannel);
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
            const st = getChannelReEq(ch);
            st.selectedBand = idx;
            updateQuickNodeCardUI(idx);
            updateMiniLabels(st, idx);
            renderMiniReEq(ch);
        }

        function updateQuickNodeCardUI(selectedIdx) {
            const st = getChannelReEq(dspRackCurrentChannel);
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
            if (bLbl) bLbl.innerText = `B${idx+1}: ${FILTER_TYPES[b.type]}`;
            if (vLbl) {
                const fStr = b.freq >= 1000 ? (b.freq/1000).toFixed(1)+'k' : b.freq+'Hz';
                const gStr = (b.gain >= 0 ? '+' : '') + b.gain.toFixed(1) + 'dB';
                vLbl.innerText = `${fStr} ${gStr} (Q ${b.q.toFixed(1)})`;
            }
        }

        function closeDspRackPage() {
            document.getElementById('dsp-rack-page').classList.remove('open');
        }

        function handleRackGainChange(val) {
            const ch = dspRackCurrentChannel;
            let fVal = parseFloat(val);
            if (window.touchGovernor) {
                fVal = window.touchGovernor.process(`gain-${ch}`, fVal);
            }
            handleInputGainChange(ch, fVal);
            const valEl = document.getElementById('dsp-rack-gain-val');
            if (valEl) valEl.innerText = (fVal >= 0 ? '+' : '') + fVal.toFixed(1) + ' dB';
        }

        function toggleRackRegain() {
            const ch = dspRackCurrentChannel;
            toggleRegain(ch);
            const btn = document.getElementById('dsp-rack-regain-btn');
            if (btn) {
                btn.classList.toggle('active', channelRegainActive[ch]);
                btn.innerText = channelRegainActive[ch] ? 'REGAIN ON' : 'REGAIN OFF';
            }
        }

        // Render the single focused channel strip on the left of the DSP Rack page
        function updateFocusedChannelStrip(ch) {
            const container = document.getElementById('dsp-rack-focused-strip');
            if (!container) return;

            const name = customChannelNames['ch-' + ch] || channelNames[ch];
            const pct = faderValues.main[ch] ?? 75;
            const dbStr = toDbStr(pct);
            const isMuted = document.getElementById('mute-ch-' + ch)?.classList.contains('active');
            const isSolo = document.getElementById('solo-ch-' + ch)?.classList.contains('active');
            const isAmm = document.getElementById('amm-ch-' + ch)?.classList.contains('active');
            const color = customChannelColors['ch-' + ch] || '#38bdf8';

            container.innerHTML = `
                <div class="focused-strip-header">
                    <div class="focused-strip-badge" style="background:${color}; color:${(color==='#e2e8f0'||color==='#eab308'||color==='#84cc16')?'#000':'#fff'};">IN ${ch + 1} &bull; CH ${ch + 1}</div>
                    <div class="focused-strip-name" title="${name}">${name}</div>
                </div>

                <div class="fader-section" style="margin: 10px 0;">
                    <div class="meter-container" style="width: 12px;" title="Channel Peak Level (-18dB Yellow, -6dB Orange, 0dB Red Clipping)">
                        <div class="meter-bar" id="meter-focused-rack"></div>
                        <div class="meter-ticks">
                            <div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div>
                        </div>
                    </div>
                    <!-- Focused Strip Gain Reduction Meter -->
                    <div class="gr-meter-container" title="Gain Reduction: 0-6dB Green, 6-12dB Orange, >12dB Red">
                        <div class="gr-meter-bar" id="gr-meter-focused-rack"></div>
                        <span class="gr-meter-scale-title">GR</span>
                    </div>
                    <div class="fader-track" id="fader-track-focused-rack" data-target="${ch}" data-channel="${ch}">
                        <div class="fader-scale">
                            <div class="scale-mark"><span>+10</span></div>
                            <div class="scale-mark"><span>+5</span></div>
                            <div class="scale-mark zero"><span>0</span></div>
                            <div class="scale-mark"><span>-5</span></div>
                            <div class="scale-mark"><span>-10</span></div>
                            <div class="scale-mark"><span>-20</span></div>
                            <div class="scale-mark"><span>-30</span></div>
                            <div class="scale-mark"><span>-40</span></div>
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

        // Focused fader drag - operates with identical precision & touch damping as console faders
        let activeFocusedF = null;
        function startFocusedFaderDrag(target, evt) {
            const cap = document.getElementById('fader-cap-focused-rack');
            const track = document.getElementById('fader-track-focused-rack');
            if (!cap || !track) return;
            const onCap = evt.target === cap || cap.contains(evt.target);
            if (!onCap) return; // Strict knob-only drag

            evt.preventDefault();
            const capRect = cap.getBoundingClientRect();
            const capCenterY = capRect.top + capRect.height / 2;
            const grabOffsetY = evt.clientY - capCenterY;

            cap.classList.add('dragging');
            activeFocusedF = { target, track, cap, grabOffsetY };
            if (evt.pointerId !== undefined) track.setPointerCapture(evt.pointerId);
        }
        window.addEventListener('pointermove', evt => {
            if (!activeFocusedF) return;
            const rect = activeFocusedF.track.getBoundingClientRect();
            const slotBottom = rect.bottom - 38;
            const slotTop = rect.top + 14;
            const travel = slotBottom - slotTop;
            if (travel <= 0) return;
            const effectivePointerY = evt.clientY - (activeFocusedF.grabOffsetY || 0);
            let pct = Math.max(0, Math.min(100, ((slotBottom - effectivePointerY) / travel) * 100));
            if (window.touchGovernor) {
                pct = window.touchGovernor.process(`fader-${activeFocusedF.target}`, pct);
            }
            setFaderPosition(activeFocusedF.target, pct, false);
            // Sync focused fader visual directly
            const currentPct = faderValues.main[activeFocusedF.target] ?? pct;
            if (activeFocusedF.cap) activeFocusedF.cap.style.bottom = `calc(38px + (100% - 52px) * ${currentPct / 100})`;
            const dbEl = document.getElementById('fader-val-focused-rack');
            if (dbEl) dbEl.innerText = toDbStr(currentPct);
        });
        window.addEventListener('pointerup', () => {
            if (activeFocusedF) {
                activeFocusedF.cap.classList.remove('dragging');
                activeFocusedF = null;
            }
        });
        window.addEventListener('pointercancel', () => {
            if (activeFocusedF) {
                activeFocusedF.cap.classList.remove('dragging');
                activeFocusedF = null;
            }
        });


        // ==========================================
        // 7. Pitch-Stable Delay & Studio Reverb Engines
        // ==========================================
        const delayDspState = {
            presetIndex: 0,
            presets: ["Tape Echo 80s", "Stereo Ping-Pong", "Quarter Clean", "Dotted Eighth Space", "Ambient Cloud", "Slapback 50s"],
            timeMs: 500,
            subdivision: "1/4",
            syncToTempo: true,
            feedback: 42,
            diffusion: 35,
            ducking: 0,
            lowCut: 120,
            highCut: 4800,
            modRate: 18,
            modDepth: 25,
            tune: 0,
            stereo: 100,
            dry: 100,
            wet: 25,
            pingPong: true
        };

        function updateDelayTimeMs(val) {
            delayDspState.timeMs = parseInt(val);
            const valEl = document.getElementById("delay-time-val");
            if (valEl) valEl.innerText = delayDspState.timeMs + " ms";
            // Disable tap sync if user manually tweaks slider
            if (delayDspState.syncToTempo) {
                delayDspState.syncToTempo = false;
                const btn = document.getElementById("delay-sync-btn");
                if (btn) {
                    btn.classList.remove("active");
                    btn.innerText = "TAP SYNC: OFF";
                }
            }
        }

        function updateDelaySubdivision(val) {
            delayDspState.subdivision = val;
            if (delayDspState.syncToTempo) {
                updateDelayTimeFromBpm();
            }
        }

        function toggleDelaySync() {
            delayDspState.syncToTempo = !delayDspState.syncToTempo;
            const btn = document.getElementById("delay-sync-btn");
            if (btn) {
                btn.classList.toggle("active", delayDspState.syncToTempo);
                btn.innerText = "TAP SYNC: " + (delayDspState.syncToTempo ? "ON" : "OFF");
            }
            if (delayDspState.syncToTempo) {
                updateDelayTimeFromBpm();
            }
        }

        function updateDelayTimeFromBpm() {
            if (!globalBpm || globalBpm <= 0) return;
            const quarterMs = 60000 / globalBpm;
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
            if (syncBpm) syncBpm.innerText = globalBpm.toFixed(1);
            const talTimeCap = document.getElementById("tal-cap-time");
            const talTimeVal = document.getElementById("tal-val-time");
            if (talTimeCap) talTimeCap.style.bottom = Math.min(100, Math.max(0, (delayDspState.timeMs / 1500) * 100)) + "%";
            if (talTimeVal) talTimeVal.innerText = delayDspState.timeMs + " ms";
        }

        // TAL-Delay preset switching
        function cycleTalDelayPreset(dir) {
            const d = delayDspState;
            d.presetIndex = (d.presetIndex + dir + d.presets.length) % d.presets.length;
            const disp = document.getElementById("tal-delay-preset-display");
            if (disp) disp.innerText = d.presets[d.presetIndex];
            if (d.presetIndex === 0) { // Tape Echo 80s
                d.timeMs = 380; d.subdivision = "1/8d"; d.feedback = 48; d.diffusion = 40; d.drive = 45; d.flutter = 30; d.lowCut = 120; d.highCut = 4800; d.modRate = 22; d.modDepth = 35;
            } else if (d.presetIndex === 1) { // Stereo Ping-Pong
                d.timeMs = 500; d.subdivision = "1/4"; d.feedback = 55; d.diffusion = 20; d.pingPong = true; d.stereo = 100; d.spread = 75; d.lowCut = 90; d.highCut = 7000;
            } else if (d.presetIndex === 2) { // Quarter Clean
                d.timeMs = 500; d.subdivision = "1/4"; d.feedback = 35; d.diffusion = 10; d.drive = 0; d.flutter = 5; d.lowCut = 80; d.highCut = 14000; d.modDepth = 10;
            } else if (d.presetIndex === 3) { // Dotted Eighth Space
                d.timeMs = 375; d.subdivision = "1/8d"; d.feedback = 60; d.diffusion = 55; d.drive = 20; d.flutter = 15; d.lowCut = 150; d.highCut = 6000; d.modRate = 35; d.modDepth = 40;
            } else if (d.presetIndex === 4) { // Ambient Cloud
                d.timeMs = 850; d.subdivision = "1/2"; d.feedback = 78; d.diffusion = 85; d.drive = 30; d.flutter = 40; d.lowCut = 200; d.highCut = 3500; d.modRate = 15; d.modDepth = 60;
            } else if (d.presetIndex === 5) { // Slapback 50s
                d.timeMs = 120; d.subdivision = "1/16"; d.feedback = 12; d.diffusion = 5; d.drive = 60; d.flutter = 50; d.lowCut = 250; d.highCut = 5000; d.pingPong = false;
            }
            updateTalDelayFadersVisual();
            initTalDelayRotaries();
        }

        function toggleTalDelaySidechain(btn) {
            delayDspState.sidechain = !delayDspState.sidechain;
            if (btn) btn.classList.toggle("active", delayDspState.sidechain);
        }

        function renderDelayControls() {
            const d = delayDspState;
            const preset = d.presets[d.presetIndex];
            const timePct = Math.min(100, Math.max(0, (d.timeMs / 1500) * 100));
            const subMap = { "1/4": 80, "1/8d": 60, "1/8": 40, "1/8t": 20, "1/16": 0 };
            const subPct = subMap[d.subdivision] ?? 80;

            return `
            <div class="tal-plugin-frame tal-theme-delay">
                <div class="tal-title-banner">TAL-DELAY-PRO</div>
                <div class="tal-header-bar">
                    <div class="tal-preset-box">
                        <button class="tal-preset-arrow" onclick="cycleTalDelayPreset(-1)">&ltrif;</button>
                        <span class="tal-preset-name" id="tal-delay-preset-display">${preset}</span>
                        <button class="tal-preset-arrow" onclick="cycleTalDelayPreset(1)">&rtrif;</button>
                    </div>
                    <button class="tal-icon-btn" title="Open Preset">📄</button>
                    <button class="tal-icon-btn" title="Save Preset">💾</button>
                    <button class="tal-icon-btn" title="Undo">↩</button>
                    <button class="tal-icon-btn" title="Redo">↪</button>
                    <button class="btn-top ${d.syncToTempo ? 'active' : ''}" id="delay-tal-sync-btn" style="padding:3px 10px; font-size:0.7rem; font-weight:800; border-radius:3px; background:${d.syncToTempo ? '#10b981' : '#1f242d'}; color:#fff;" onclick="toggleDelaySync()">TAP SYNC: ${d.syncToTempo ? 'ON (' + globalBpm.toFixed(0) + ' BPM)' : 'OFF'}</button>
                    <div style="flex:1;"></div>
                    <div style="width:120px; height:12px; background:#08100b; border:1px solid #143021; border-radius:2px;"></div>
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

        // ============================================================
        // AUTHENTIC REVERB ENGINE STATE & GUI CONTROLS
        // Exact 1:1 reproduction of TAL-Reverb-4 & Convolver IR
        // ============================================================
        const reverbDspState = {
            engine: "tal", // 'tal' | 'atlantis'
            tal: {
                presetIndex: 0,
                presets: ["Dark Big", "Vintage Plate 80s", "Large Hall", "Drum Room", "Warm Ambience", "Vocal Space", "Shimmer Cathedral"],
                // Top MAIN Faders (0..100)
                size: 96.15,
                damp: 0.0,
                delay: 0.0, // seconds
                diffuse: 100.0,
                stereo: 100.0,
                // Ducking & Sidechain
                ducking: 0.0,
                sidechain: false,
                // Top MIX Faders
                dry: 100.0,
                wet: 17.53,
                // Bottom Sections
                modRate: 16.58,
                modAmount: 49.49,
                tune: 0.0,
                tuneAmount: 0.0,
                eqLowCut: 31,     // Hz
                eqHighCut: 4709,  // Hz
                eqPeakFreq: 0,    // Hz
                eqPeakGain: 0.0,  // dB
                lofiBits: 24,     // Bit
                lofiSR: 48000     // Hz
            },
            atlantis: {
                irModel: "cathedral",
                preDelay: 25,
                damping: 45,
                roomSize: 85,
                preEqLow: 120,
                postEqHigh: 5500,
                dry: 0,   // dB
                wet: -3,  // dB
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
        }

        function renderReverbControls() {
            if (reverbDspState.engine === 'tal') {
                const s = reverbDspState.tal;
                const preset = s.presets[s.presetIndex];
                return `
                <div class="tal-plugin-frame">
                    <div class="tal-title-banner">TAL-REVERB-4</div>
                    <!-- Header with preset browser, disk, undo/redo -->
                    <div class="tal-header-bar">
                        <div class="tal-preset-box">
                            <button class="tal-preset-arrow" onclick="cycleTalPreset(-1)">&ltrif;</button>
                            <span class="tal-preset-name" id="tal-preset-display">${preset}</span>
                            <button class="tal-preset-arrow" onclick="cycleTalPreset(1)">&rtrif;</button>
                        </div>
                        <button class="tal-icon-btn" title="Open Preset">📄</button>
                        <button class="tal-icon-btn" title="Save Preset">💾</button>
                        <button class="tal-icon-btn" title="Undo">↩</button>
                        <button class="tal-icon-btn" title="Redo">↪</button>
                        <div style="flex:1;"></div>
                        <div style="width:140px; height:12px; background:#0d0e10; border:1px solid #282c32; border-radius:2px;"></div>
                    </div>

                    <!-- Top Section: MAIN faders + DUCKING + MIX faders -->
                    <div class="tal-top-panel">
                        <!-- MAIN Group -->
                        <div class="tal-section-box" style="flex: 2.2;">
                            <div class="tal-section-header tal-sh-main">MAIN</div>
                            <div class="tal-fader-row">
                                <!-- SIZE -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-size">${s.size.toFixed(2)} %</span>
                                    <div class="tal-vtrack" id="tal-track-size" onpointerdown="startTalFaderDrag('size', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div>
                                        </div>
                                        <div class="tal-fader-handle" id="tal-cap-size" style="bottom: ${s.size}%;"></div>
                                    </div>
                                    <span class="tal-fader-label">SIZE</span>
                                </div>
                                <!-- DAMP -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-damp">${s.damp.toFixed(2)} %</span>
                                    <div class="tal-vtrack" id="tal-track-damp" onpointerdown="startTalFaderDrag('damp', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div>
                                        </div>
                                        <div class="tal-fader-handle" id="tal-cap-damp" style="bottom: ${s.damp}%;"></div>
                                    </div>
                                    <span class="tal-fader-label">DAMP</span>
                                </div>
                                <!-- DELAY -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-delay">${s.delay.toFixed(3)} s</span>
                                    <div class="tal-vtrack" id="tal-track-delay" onpointerdown="startTalFaderDrag('delay', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div>
                                        </div>
                                        <div class="tal-fader-handle" id="tal-cap-delay" style="bottom: ${s.delay * 100}%;"></div>
                                    </div>
                                    <span class="tal-fader-label">DELAY</span>
                                </div>
                                <!-- DIFFUSE -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-diffuse">${s.diffuse.toFixed(2)} %</span>
                                    <div class="tal-vtrack" id="tal-track-diffuse" onpointerdown="startTalFaderDrag('diffuse', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div>
                                        </div>
                                        <div class="tal-fader-handle" id="tal-cap-diffuse" style="bottom: ${s.diffuse}%;"></div>
                                    </div>
                                    <span class="tal-fader-label">DIFFUSE</span>
                                </div>
                                <!-- STEREO -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-stereo">${s.stereo.toFixed(2)} %</span>
                                    <div class="tal-vtrack" id="tal-track-stereo" onpointerdown="startTalFaderDrag('stereo', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div>
                                        </div>
                                        <div class="tal-fader-handle" id="tal-cap-stereo" style="bottom: ${s.stereo}%;"></div>
                                    </div>
                                    <span class="tal-fader-label">STEREO</span>
                                </div>
                            </div>
                        </div>

                        <!-- DUCKING / S-CHAIN Mid Column -->
                        <div class="tal-mid-col">
                            <button class="tal-schain-btn ${s.sidechain ? 'active' : ''}" onclick="toggleTalSidechain(this)">S-CHAIN</button>
                            <div style="width:100%; height:18px; background:#111215; border-radius:3px; border:1px solid #23272e;"></div>
                            <div class="tal-rotary-box">
                                <span class="tal-rotary-val" id="tal-val-ducking">${s.ducking.toFixed(2)} %</span>
                                <canvas class="tal-knob-canvas" id="tal-knob-ducking" width="44" height="44"></canvas>
                                <span class="tal-rotary-label">DUCKING</span>
                            </div>
                        </div>

                        <!-- MIX Group -->
                        <div class="tal-section-box" style="flex: 1;">
                            <div class="tal-section-header tal-sh-mix">MIX</div>
                            <div class="tal-fader-row">
                                <!-- DRY -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-dry">${s.dry.toFixed(2)} %</span>
                                    <div class="tal-vtrack" id="tal-track-dry" onpointerdown="startTalFaderDrag('dry', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div>
                                        </div>
                                        <div class="tal-fader-handle" id="tal-cap-dry" style="bottom: ${s.dry}%;"></div>
                                    </div>
                                    <span class="tal-fader-label">DRY</span>
                                </div>
                                <!-- WET -->
                                <div class="tal-fader-col">
                                    <span class="tal-fader-val" id="tal-val-wet">${s.wet.toFixed(2)} %</span>
                                    <div class="tal-vtrack" id="tal-track-wet" onpointerdown="startTalFaderDrag('wet', event)">
                                        <div class="tal-vtrack-groove"></div>
                                        <div class="tal-vtrack-ticks">
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div><div class="tal-tick"></div><div class="tal-tick"></div>
                                            <div class="tal-tick major"></div>
                                        </div>
                                        <div class="tal-fader-handle" id="tal-cap-wet" style="bottom: ${s.wet}%;"></div>
                                    </div>
                                    <span class="tal-fader-label">WET</span>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Bottom Section: MOD | TUNE | EQ | LO-FI -->
                    <div class="tal-bottom-panel">
                        <!-- MOD -->
                        <div class="tal-btm-sub">
                            <div class="tal-section-header tal-sh-mod">MOD</div>
                            <div class="tal-btm-knobs">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-modRate">${s.modRate.toFixed(2)} %</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-modRate" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">RATE</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-modAmount">${s.modAmount.toFixed(2)} %</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-modAmount" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">AMOUNT</span>
                                </div>
                            </div>
                        </div>

                        <!-- TUNE -->
                        <div class="tal-btm-sub">
                            <div class="tal-section-header tal-sh-tune">TUNE</div>
                            <div class="tal-btm-knobs">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-tune">${s.tune.toFixed(2)}</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-tune" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">TUNE</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-tuneAmount">${s.tuneAmount.toFixed(2)} %</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-tuneAmount" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">AMOUNT</span>
                                </div>
                            </div>
                        </div>

                        <!-- EQ -->
                        <div class="tal-btm-sub" style="flex:1.5;">
                            <div class="tal-section-header tal-sh-eq">EQ</div>
                            <div class="tal-btm-knobs dual-col">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqLowCut">${s.eqLowCut} Hz</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-eqLowCut" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">LOW CUT</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqPeakFreq">${s.eqPeakFreq} Hz</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-eqPeakFreq" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">PEAK FREQ</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqHighCut">${s.eqHighCut} Hz</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-eqHighCut" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">HIGH CUT</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-eqPeakGain">${s.eqPeakGain.toFixed(2)} dB</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-eqPeakGain" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">PEAK GAIN</span>
                                </div>
                            </div>
                        </div>

                        <!-- LO-FI -->
                        <div class="tal-btm-sub">
                            <div class="tal-section-header tal-sh-lofi">LO-FI</div>
                            <div class="tal-btm-knobs">
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-lofiBits">${s.lofiBits} Bit</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-lofiBits" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">BITS</span>
                                </div>
                                <div class="tal-rotary-box">
                                    <span class="tal-rotary-val" id="tal-val-lofiSR">${s.lofiSR} Hz</span>
                                    <canvas class="tal-knob-canvas" id="tal-knob-lofiSR" width="44" height="44"></canvas>
                                    <span class="tal-rotary-label">SR</span>
                                </div>
                            </div>
                        </div>
                    </div>

                    <!-- Footer Bar -->
                    <div class="tal-footer-bar">
                        <div class="tal-footer-title">TAL REVERB 4</div>
                        <div>TAL Software GmbH 4.0.2</div>
                    </div>
                </div>
                `;
            } else {
                // Exact Convolver IR Reverb Plugin GUI
                const a = reverbDspState.atlantis;
                return `
                <div class="ir-plugin-frame">
                    <div class="ir-header-bar">
                        <div style="display:flex; align-items:center; gap:8px;">
                            <span style="font-size:1.1rem; font-weight:900; color:#818cf8; letter-spacing:1px;">CONVOLVER IR</span>
                            <span style="font-size:0.7rem; background:#1e1b4b; color:#c7d2fe; padding:2px 6px; border-radius:3px; font-weight:700;">ZERO LATENCY</span>
                        </div>
                        <div style="display:flex; gap:8px; align-items:center;">
                            <label style="font-size:0.75rem; color:#94a3b8;">ACOUSTIC SPACE:</label>
                            <select onchange="reverbDspState.atlantis.irModel=this.value; drawIrWaveform();" style="background:#0b0d11; color:#f1f5f9; border:1px solid #374151; border-radius:4px; padding:4px 8px; font-size:0.75rem; font-weight:700; max-width:280px;">
<optgroup label="Factory Acoustic Presets">
                                    <option value="cathedral" ${a.irModel === 'cathedral' ? 'selected' : ''}>Gothic Stone Cathedral (4.8s)</option>
                                    <option value="concert_hall" ${a.irModel === 'concert_hall' ? 'selected' : ''}>Symphony Concert Hall (2.4s)</option>
                                    <option value="emt140" ${a.irModel === 'emt140' ? 'selected' : ''}>EMT-140 Classic Plate (3.1s)</option>
                                    <option value="drum_room" ${a.irModel === 'drum_room' ? 'selected' : ''}>Live Oak Drum Room (0.8s)</option>
                                    <option value="ambient_space" ${a.irModel === 'ambient_space' ? 'selected' : ''}>Cosmic Void Ambience (7.5s)</option>
                                </optgroup>
                                <optgroup label="Bricasti M7 Halls">
                                    <option value="1 Halls 01 Large Hall" ${a.irModel === '1 Halls 01 Large Hall' ? 'selected' : ''}>Large Hall</option>
                                    <option value="1 Halls 02 Medium Hall" ${a.irModel === '1 Halls 02 Medium Hall' ? 'selected' : ''}>Medium Hall</option>
                                    <option value="1 Halls 03 Small Hall" ${a.irModel === '1 Halls 03 Small Hall' ? 'selected' : ''}>Small Hall</option>
                                    <option value="1 Halls 04 Large & Near" ${a.irModel === '1 Halls 04 Large & Near' ? 'selected' : ''}>Large & Near</option>
                                    <option value="1 Halls 05 Medium & Near" ${a.irModel === '1 Halls 05 Medium & Near' ? 'selected' : ''}>Medium & Near</option>
                                    <option value="1 Halls 06 Small & Near" ${a.irModel === '1 Halls 06 Small & Near' ? 'selected' : ''}>Small & Near</option>
                                    <option value="1 Halls 07 Large & Dark" ${a.irModel === '1 Halls 07 Large & Dark' ? 'selected' : ''}>Large & Dark</option>
                                    <option value="1 Halls 08 Large & Deep" ${a.irModel === '1 Halls 08 Large & Deep' ? 'selected' : ''}>Large & Deep</option>
                                    <option value="1 Halls 09 Medium & Deep" ${a.irModel === '1 Halls 09 Medium & Deep' ? 'selected' : ''}>Medium & Deep</option>
                                    <option value="1 Halls 10 Concert Hall" ${a.irModel === '1 Halls 10 Concert Hall' ? 'selected' : ''}>Concert Hall</option>
                                    <option value="1 Halls 11 Gold Hall" ${a.irModel === '1 Halls 11 Gold Hall' ? 'selected' : ''}>Gold Hall</option>
                                    <option value="1 Halls 12 Sandors Hall" ${a.irModel === '1 Halls 12 Sandors Hall' ? 'selected' : ''}>Sandors Hall</option>
                                    <option value="1 Halls 13 Dense Hall" ${a.irModel === '1 Halls 13 Dense Hall' ? 'selected' : ''}>Dense Hall</option>
                                    <option value="1 Halls 14 Clear Hall" ${a.irModel === '1 Halls 14 Clear Hall' ? 'selected' : ''}>Clear Hall</option>
                                    <option value="1 Halls 15 Brass Hall" ${a.irModel === '1 Halls 15 Brass Hall' ? 'selected' : ''}>Brass Hall</option>
                                    <option value="1 Halls 16 Amsterdam Hall" ${a.irModel === '1 Halls 16 Amsterdam Hall' ? 'selected' : ''}>Amsterdam Hall</option>
                                    <option value="1 Halls 17 Berliner Hall" ${a.irModel === '1 Halls 17 Berliner Hall' ? 'selected' : ''}>Berliner Hall</option>
                                    <option value="1 Halls 18 Boston Hall A" ${a.irModel === '1 Halls 18 Boston Hall A' ? 'selected' : ''}>Boston Hall A</option>
                                    <option value="1 Halls 19 Boston Hall B" ${a.irModel === '1 Halls 19 Boston Hall B' ? 'selected' : ''}>Boston Hall B</option>
                                    <option value="1 Halls 20 Chicago Hall" ${a.irModel === '1 Halls 20 Chicago Hall' ? 'selected' : ''}>Chicago Hall</option>
                                    <option value="1 Halls 21 Vienna Hall" ${a.irModel === '1 Halls 21 Vienna Hall' ? 'selected' : ''}>Vienna Hall</option>
                                    <option value="1 Halls 22 Worcester Hall" ${a.irModel === '1 Halls 22 Worcester Hall' ? 'selected' : ''}>Worcester Hall</option>
                                    <option value="1 Halls 23 The ArchDuke" ${a.irModel === '1 Halls 23 The ArchDuke' ? 'selected' : ''}>The ArchDuke</option>
                                    <option value="1 Halls 24 Troy Music Hall" ${a.irModel === '1 Halls 24 Troy Music Hall' ? 'selected' : ''}>Troy Music Hall</option>
                                    <option value="1 Halls 25 Saint Sylvain" ${a.irModel === '1 Halls 25 Saint Sylvain' ? 'selected' : ''}>Saint Sylvain</option>
                                    <option value="1 Halls 26 Mechanics Hall" ${a.irModel === '1 Halls 26 Mechanics Hall' ? 'selected' : ''}>Mechanics Hall</option>
                                    <option value="1 Halls 27 Saint Gerold" ${a.irModel === '1 Halls 27 Saint Gerold' ? 'selected' : ''}>Saint Gerold</option>
                                </optgroup>
                                <optgroup label="Bricasti M7 Plates">
                                    <option value="2 Plates 01 Bright Plate" ${a.irModel === '2 Plates 01 Bright Plate' ? 'selected' : ''}>Bright Plate</option>
                                    <option value="2 Plates 02 Dark Plate" ${a.irModel === '2 Plates 02 Dark Plate' ? 'selected' : ''}>Dark Plate</option>
                                    <option value="2 Plates 03 London Plate" ${a.irModel === '2 Plates 03 London Plate' ? 'selected' : ''}>London Plate</option>
                                    <option value="2 Plates 04 Snare Plate A" ${a.irModel === '2 Plates 04 Snare Plate A' ? 'selected' : ''}>Snare Plate A</option>
                                    <option value="2 Plates 05 Snare Plate B" ${a.irModel === '2 Plates 05 Snare Plate B' ? 'selected' : ''}>Snare Plate B</option>
                                    <option value="2 Plates 06 Vocal Plate" ${a.irModel === '2 Plates 06 Vocal Plate' ? 'selected' : ''}>Vocal Plate</option>
                                    <option value="2 Plates 07 Old Plate" ${a.irModel === '2 Plates 07 Old Plate' ? 'selected' : ''}>Old Plate</option>
                                    <option value="2 Plates 08 Rich Plate" ${a.irModel === '2 Plates 08 Rich Plate' ? 'selected' : ''}>Rich Plate</option>
                                    <option value="2 Plates 09 Gold Plate" ${a.irModel === '2 Plates 09 Gold Plate' ? 'selected' : ''}>Gold Plate</option>
                                    <option value="2 Plates 10 Dense Plate" ${a.irModel === '2 Plates 10 Dense Plate' ? 'selected' : ''}>Dense Plate</option>
                                    <option value="2 Plates 11 Silver Plate" ${a.irModel === '2 Plates 11 Silver Plate' ? 'selected' : ''}>Silver Plate</option>
                                    <option value="2 Plates 12 Percussion Plate" ${a.irModel === '2 Plates 12 Percussion Plate' ? 'selected' : ''}>Percussion Plate</option>
                                    <option value="2 Plates 13 Echo Plate" ${a.irModel === '2 Plates 13 Echo Plate' ? 'selected' : ''}>Echo Plate</option>
                                    <option value="2 Plates 14 CD Plate A" ${a.irModel === '2 Plates 14 CD Plate A' ? 'selected' : ''}>CD Plate A</option>
                                    <option value="2 Plates 15 CD Plate B" ${a.irModel === '2 Plates 15 CD Plate B' ? 'selected' : ''}>CD Plate B</option>
                                    <option value="2 Plates 16 Large Plate" ${a.irModel === '2 Plates 16 Large Plate' ? 'selected' : ''}>Large Plate</option>
                                    <option value="2 Plates 17 Small Plate" ${a.irModel === '2 Plates 17 Small Plate' ? 'selected' : ''}>Small Plate</option>
                                    <option value="2 Plates 18 Fat Plate" ${a.irModel === '2 Plates 18 Fat Plate' ? 'selected' : ''}>Fat Plate</option>
                                    <option value="2 Plates 19 Crystal Plate" ${a.irModel === '2 Plates 19 Crystal Plate' ? 'selected' : ''}>Crystal Plate</option>
                                    <option value="2 Plates 20 Sun Plate A" ${a.irModel === '2 Plates 20 Sun Plate A' ? 'selected' : ''}>Sun Plate A</option>
                                    <option value="2 Plates 21 Sun Plate B" ${a.irModel === '2 Plates 21 Sun Plate B' ? 'selected' : ''}>Sun Plate B</option>
                                    <option value="2 Plates 22 Sun Plate C" ${a.irModel === '2 Plates 22 Sun Plate C' ? 'selected' : ''}>Sun Plate C</option>
                                    <option value="2 Plates 23 Vocal Plate B" ${a.irModel === '2 Plates 23 Vocal Plate B' ? 'selected' : ''}>Vocal Plate B</option>
                                </optgroup>
                                <optgroup label="Bricasti M7 Rooms">
                                    <option value="3 Rooms 01 Studio A" ${a.irModel === '3 Rooms 01 Studio A' ? 'selected' : ''}>Studio A</option>
                                    <option value="3 Rooms 02 Studio B Close" ${a.irModel === '3 Rooms 02 Studio B Close' ? 'selected' : ''}>Studio B Close</option>
                                    <option value="3 Rooms 03 Studio B Far" ${a.irModel === '3 Rooms 03 Studio B Far' ? 'selected' : ''}>Studio B Far</option>
                                    <option value="3 Rooms 04 Studio C" ${a.irModel === '3 Rooms 04 Studio C' ? 'selected' : ''}>Studio C</option>
                                    <option value="3 Rooms 05 Studio D" ${a.irModel === '3 Rooms 05 Studio D' ? 'selected' : ''}>Studio D</option>
                                    <option value="3 Rooms 06 Studio E" ${a.irModel === '3 Rooms 06 Studio E' ? 'selected' : ''}>Studio E</option>
                                    <option value="3 Rooms 07 Deep Stone" ${a.irModel === '3 Rooms 07 Deep Stone' ? 'selected' : ''}>Deep Stone</option>
                                    <option value="3 Rooms 08 Music Room" ${a.irModel === '3 Rooms 08 Music Room' ? 'selected' : ''}>Music Room</option>
                                    <option value="3 Rooms 09 Heavy Room" ${a.irModel === '3 Rooms 09 Heavy Room' ? 'selected' : ''}>Heavy Room</option>
                                    <option value="3 Rooms 10 Large Wooden Room" ${a.irModel === '3 Rooms 10 Large Wooden Room' ? 'selected' : ''}>Large Wooden Room</option>
                                    <option value="3 Rooms 11 Small Wooden Room" ${a.irModel === '3 Rooms 11 Small Wooden Room' ? 'selected' : ''}>Small Wooden Room</option>
                                    <option value="3 Rooms 12 Large Tiled Room" ${a.irModel === '3 Rooms 12 Large Tiled Room' ? 'selected' : ''}>Large Tiled Room</option>
                                    <option value="3 Rooms 13 Medium Tiled Room" ${a.irModel === '3 Rooms 13 Medium Tiled Room' ? 'selected' : ''}>Medium Tiled Room</option>
                                    <option value="3 Rooms 14 Small Tiled Room" ${a.irModel === '3 Rooms 14 Small Tiled Room' ? 'selected' : ''}>Small Tiled Room</option>
                                    <option value="3 Rooms 15 Drum & Chamber" ${a.irModel === '3 Rooms 15 Drum & Chamber' ? 'selected' : ''}>Drum & Chamber</option>
                                    <option value="3 Rooms 16 Djangos Room" ${a.irModel === '3 Rooms 16 Djangos Room' ? 'selected' : ''}>Djangos Room</option>
                                    <option value="3 Rooms 17 Small Vox Room" ${a.irModel === '3 Rooms 17 Small Vox Room' ? 'selected' : ''}>Small Vox Room</option>
                                    <option value="3 Rooms 18 Glass Room" ${a.irModel === '3 Rooms 18 Glass Room' ? 'selected' : ''}>Glass Room</option>
                                    <option value="3 Rooms 19 Percussion Room" ${a.irModel === '3 Rooms 19 Percussion Room' ? 'selected' : ''}>Percussion Room</option>
                                    <option value="3 Rooms 20 Marble Foyer" ${a.irModel === '3 Rooms 20 Marble Foyer' ? 'selected' : ''}>Marble Foyer</option>
                                    <option value="3 Rooms 21 Large & Room" ${a.irModel === '3 Rooms 21 Large & Room' ? 'selected' : ''}>Large & Room</option>
                                    <option value="3 Rooms 22 Small & Room" ${a.irModel === '3 Rooms 22 Small & Room' ? 'selected' : ''}>Small & Room</option>
                                    <option value="3 Rooms 23 Large Red Room" ${a.irModel === '3 Rooms 23 Large Red Room' ? 'selected' : ''}>Large Red Room</option>
                                    <option value="3 Rooms 24 Red Room" ${a.irModel === '3 Rooms 24 Red Room' ? 'selected' : ''}>Red Room</option>
                                    <option value="3 Rooms 25 Blue Room" ${a.irModel === '3 Rooms 25 Blue Room' ? 'selected' : ''}>Blue Room</option>
                                    <option value="3 Rooms 26 Large Room" ${a.irModel === '3 Rooms 26 Large Room' ? 'selected' : ''}>Large Room</option>
                                    <option value="3 Rooms 27 Small Room" ${a.irModel === '3 Rooms 27 Small Room' ? 'selected' : ''}>Small Room</option>
                                    <option value="3 Rooms 28 Front Room" ${a.irModel === '3 Rooms 28 Front Room' ? 'selected' : ''}>Front Room</option>
                                    <option value="3 Rooms 29 Center Room" ${a.irModel === '3 Rooms 29 Center Room' ? 'selected' : ''}>Center Room</option>
                                    <option value="3 Rooms 30 Back Room" ${a.irModel === '3 Rooms 30 Back Room' ? 'selected' : ''}>Back Room</option>
                                    <option value="3 Rooms 31 Studio K" ${a.irModel === '3 Rooms 31 Studio K' ? 'selected' : ''}>Studio K</option>
                                    <option value="3 Rooms 32 Waits Room" ${a.irModel === '3 Rooms 32 Waits Room' ? 'selected' : ''}>Waits Room</option>
                                    <option value="3 Rooms 33 Corn Room" ${a.irModel === '3 Rooms 33 Corn Room' ? 'selected' : ''}>Corn Room</option>
                                    <option value="3 Rooms 34 Oakland Room" ${a.irModel === '3 Rooms 34 Oakland Room' ? 'selected' : ''}>Oakland Room</option>
                                    <option value="3 Rooms 35 SF Perf Room" ${a.irModel === '3 Rooms 35 SF Perf Room' ? 'selected' : ''}>SF Perf Room</option>
                                </optgroup>
                                <optgroup label="Bricasti M7 Chambers">
                                    <option value="4 Chambers 01 Large Chamber" ${a.irModel === '4 Chambers 01 Large Chamber' ? 'selected' : ''}>Large Chamber</option>
                                    <option value="4 Chambers 02 Medium Chamber" ${a.irModel === '4 Chambers 02 Medium Chamber' ? 'selected' : ''}>Medium Chamber</option>
                                    <option value="4 Chambers 03 Small Chamber" ${a.irModel === '4 Chambers 03 Small Chamber' ? 'selected' : ''}>Small Chamber</option>
                                    <option value="4 Chambers 04 Large & Dark" ${a.irModel === '4 Chambers 04 Large & Dark' ? 'selected' : ''}>Large & Dark</option>
                                    <option value="4 Chambers 05 Small & Dark" ${a.irModel === '4 Chambers 05 Small & Dark' ? 'selected' : ''}>Small & Dark</option>
                                    <option value="4 Chambers 06 Large & Bright" ${a.irModel === '4 Chambers 06 Large & Bright' ? 'selected' : ''}>Large & Bright</option>
                                    <option value="4 Chambers 07 Small & Bright" ${a.irModel === '4 Chambers 07 Small & Bright' ? 'selected' : ''}>Small & Bright</option>
                                    <option value="4 Chambers 08 Kick Chamber" ${a.irModel === '4 Chambers 08 Kick Chamber' ? 'selected' : ''}>Kick Chamber</option>
                                    <option value="4 Chambers 09 Snare Chamber" ${a.irModel === '4 Chambers 09 Snare Chamber' ? 'selected' : ''}>Snare Chamber</option>
                                    <option value="4 Chambers 10 Vocal Chamber" ${a.irModel === '4 Chambers 10 Vocal Chamber' ? 'selected' : ''}>Vocal Chamber</option>
                                    <option value="4 Chambers 11 A&M Chamber" ${a.irModel === '4 Chambers 11 A&M Chamber' ? 'selected' : ''}>A&M Chamber</option>
                                    <option value="4 Chambers 12 CD Chamber" ${a.irModel === '4 Chambers 12 CD Chamber' ? 'selected' : ''}>CD Chamber</option>
                                    <option value="4 Chambers 13 Old Chamber" ${a.irModel === '4 Chambers 13 Old Chamber' ? 'selected' : ''}>Old Chamber</option>
                                    <option value="4 Chambers 14 Deep Chamber" ${a.irModel === '4 Chambers 14 Deep Chamber' ? 'selected' : ''}>Deep Chamber</option>
                                    <option value="4 Chambers 15 Amb Chamber A" ${a.irModel === '4 Chambers 15 Amb Chamber A' ? 'selected' : ''}>Amb Chamber A</option>
                                    <option value="4 Chambers 16 Amb Chamber B" ${a.irModel === '4 Chambers 16 Amb Chamber B' ? 'selected' : ''}>Amb Chamber B</option>
                                    <option value="4 Chambers 17 Sunset Chamber" ${a.irModel === '4 Chambers 17 Sunset Chamber' ? 'selected' : ''}>Sunset Chamber</option>
                                </optgroup>
                                <optgroup label="Bricasti M7 Ambiences">
                                    <option value="5 Ambiences 01 Large Ambience" ${a.irModel === '5 Ambiences 01 Large Ambience' ? 'selected' : ''}>Large Ambience</option>
                                    <option value="5 Ambiences 02 Medium Ambience" ${a.irModel === '5 Ambiences 02 Medium Ambience' ? 'selected' : ''}>Medium Ambience</option>
                                    <option value="5 Ambiences 03 Small Ambience" ${a.irModel === '5 Ambiences 03 Small Ambience' ? 'selected' : ''}>Small Ambience</option>
                                    <option value="5 Ambiences 04 Large & Dark" ${a.irModel === '5 Ambiences 04 Large & Dark' ? 'selected' : ''}>Large & Dark</option>
                                    <option value="5 Ambiences 05 Medium & Dark" ${a.irModel === '5 Ambiences 05 Medium & Dark' ? 'selected' : ''}>Medium & Dark</option>
                                    <option value="5 Ambiences 06 Small & Dark" ${a.irModel === '5 Ambiences 06 Small & Dark' ? 'selected' : ''}>Small & Dark</option>
                                    <option value="5 Ambiences 07 Large & Bright" ${a.irModel === '5 Ambiences 07 Large & Bright' ? 'selected' : ''}>Large & Bright</option>
                                    <option value="5 Ambiences 08 Medium & Bright" ${a.irModel === '5 Ambiences 08 Medium & Bright' ? 'selected' : ''}>Medium & Bright</option>
                                    <option value="5 Ambiences 09 Small & Bright" ${a.irModel === '5 Ambiences 09 Small & Bright' ? 'selected' : ''}>Small & Bright</option>
                                    <option value="5 Ambiences 10 Deep Ambience" ${a.irModel === '5 Ambiences 10 Deep Ambience' ? 'selected' : ''}>Deep Ambience</option>
                                    <option value="5 Ambiences 11 Long Ambience" ${a.irModel === '5 Ambiences 11 Long Ambience' ? 'selected' : ''}>Long Ambience</option>
                                    <option value="5 Ambiences 12 Clear Ambience" ${a.irModel === '5 Ambiences 12 Clear Ambience' ? 'selected' : ''}>Clear Ambience</option>
                                    <option value="5 Ambiences 13 Heavy Ambience" ${a.irModel === '5 Ambiences 13 Heavy Ambience' ? 'selected' : ''}>Heavy Ambience</option>
                                    <option value="5 Ambiences 14 Bass XXL" ${a.irModel === '5 Ambiences 14 Bass XXL' ? 'selected' : ''}>Bass XXL</option>
                                    <option value="5 Ambiences 15 Percussion Air" ${a.irModel === '5 Ambiences 15 Percussion Air' ? 'selected' : ''}>Percussion Air</option>
                                </optgroup>
                                <optgroup label="Bricasti M7 Spaces">
                                    <option value="6 Spaces 01 North Church" ${a.irModel === '6 Spaces 01 North Church' ? 'selected' : ''}>North Church</option>
                                    <option value="6 Spaces 02 East Church" ${a.irModel === '6 Spaces 02 East Church' ? 'selected' : ''}>East Church</option>
                                    <option value="6 Spaces 03 South Church" ${a.irModel === '6 Spaces 03 South Church' ? 'selected' : ''}>South Church</option>
                                    <option value="6 Spaces 04 West Church" ${a.irModel === '6 Spaces 04 West Church' ? 'selected' : ''}>West Church</option>
                                    <option value="6 Spaces 05 Cinema Room" ${a.irModel === '6 Spaces 05 Cinema Room' ? 'selected' : ''}>Cinema Room</option>
                                    <option value="6 Spaces 06 Scoring Stage" ${a.irModel === '6 Spaces 06 Scoring Stage' ? 'selected' : ''}>Scoring Stage</option>
                                    <option value="6 Spaces 07 Bath House" ${a.irModel === '6 Spaces 07 Bath House' ? 'selected' : ''}>Bath House</option>
                                    <option value="6 Spaces 08 Car Park" ${a.irModel === '6 Spaces 08 Car Park' ? 'selected' : ''}>Car Park</option>
                                    <option value="6 Spaces 09 Arena" ${a.irModel === '6 Spaces 09 Arena' ? 'selected' : ''}>Arena</option>
                                    <option value="6 Spaces 10 Redwood Valley" ${a.irModel === '6 Spaces 10 Redwood Valley' ? 'selected' : ''}>Redwood Valley</option>
                                    <option value="6 Spaces 11 Tanglewood" ${a.irModel === '6 Spaces 11 Tanglewood' ? 'selected' : ''}>Tanglewood</option>
                                    <option value="6 Spaces 12 Academy Yard" ${a.irModel === '6 Spaces 12 Academy Yard' ? 'selected' : ''}>Academy Yard</option>
                                    <option value="6 Spaces 13 Hillside" ${a.irModel === '6 Spaces 13 Hillside' ? 'selected' : ''}>Hillside</option>
                                    <option value="6 Spaces 14 Cavern" ${a.irModel === '6 Spaces 14 Cavern' ? 'selected' : ''}>Cavern</option>
                                    <option value="6 Spaces 15 Stone Quarry" ${a.irModel === '6 Spaces 15 Stone Quarry' ? 'selected' : ''}>Stone Quarry</option>
                                    <option value="6 Spaces 16 Europa" ${a.irModel === '6 Spaces 16 Europa' ? 'selected' : ''}>Europa</option>
                                    <option value="6 Spaces 17 Gated Space" ${a.irModel === '6 Spaces 17 Gated Space' ? 'selected' : ''}>Gated Space</option>
                                </optgroup>
                            </select>
                        </div>
                    </div>

                    <!-- Real-time Impulse Response Acoustic Waveform & Envelope Display -->
                    <div class="ir-display-area">
                        <canvas class="ir-waveform-canvas" id="ir-waveform-canvas"></canvas>
                        <div class="ir-hud-row">
                            <span>PARTITIONED CONVOLUTION: GARDNER 3-STAGE FFT</span>
                            <span>SAMPLE RATE: 48 kHz / 24-bit TRUE STEREO</span>
                            <span id="ir-status-msg" style="color:#22c55e;">IR ACTIVE &bull; 0 LATENCY</span>
                        </div>
                    </div>

                    <!-- Main Convolver Controls -->
                    <div class="ir-controls-grid">
                        <div class="ir-card">
                            <span class="ir-card-label">PRE-DELAY</span>
                            <input type="range" class="knob-slider" min="0" max="500" step="1" value="${a.preDelay}" oninput="reverbDspState.atlantis.preDelay=parseInt(this.value); this.nextElementSibling.innerText=this.value+' ms'; drawIrWaveform();">
                            <span class="ir-card-val">${a.preDelay} ms</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">ROOM SIZE</span>
                            <input type="range" class="knob-slider" min="10" max="150" step="1" value="${a.roomSize}" oninput="reverbDspState.atlantis.roomSize=parseInt(this.value); this.nextElementSibling.innerText=this.value+' %'; drawIrWaveform();">
                            <span class="ir-card-val">${a.roomSize} %</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">DAMPING</span>
                            <input type="range" class="knob-slider" min="0" max="100" step="1" value="${a.damping}" oninput="reverbDspState.atlantis.damping=parseInt(this.value); this.nextElementSibling.innerText=this.value+' %'; drawIrWaveform();">
                            <span class="ir-card-val">${a.damping} %</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">WET LEVEL</span>
                            <input type="range" class="knob-slider" min="-40" max="0" step="0.5" value="${a.wet}" oninput="reverbDspState.atlantis.wet=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB'">
                            <span class="ir-card-val">${a.wet} dB</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">PRE-EQ LOW CUT</span>
                            <input type="range" class="knob-slider" min="20" max="500" step="5" value="${a.preEqLow}" oninput="reverbDspState.atlantis.preEqLow=parseInt(this.value); this.nextElementSibling.innerText=this.value+' Hz'">
                            <span class="ir-card-val">${a.preEqLow} Hz</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">POST-EQ HIGH CUT</span>
                            <input type="range" class="knob-slider" min="1000" max="18000" step="100" value="${a.postEqHigh}" oninput="reverbDspState.atlantis.postEqHigh=parseInt(this.value); this.nextElementSibling.innerText=(this.value/1000).toFixed(1)+' kHz'">
                            <span class="ir-card-val">${(a.postEqHigh/1000).toFixed(1)} kHz</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">NORMALIZE (NORM)</span>
                            <button class="btn-top ${a.norm ? 'active' : ''}" style="width:100%; margin-top:4px;" onclick="reverbDspState.atlantis.norm=!reverbDspState.atlantis.norm; this.classList.toggle('active'); this.innerText=reverbDspState.atlantis.norm?'NORM ON':'NORM OFF'; drawIrWaveform();">${a.norm ? 'NORM ON' : 'NORM OFF'}</button>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">REVERSE IR (REV)</span>
                            <button class="btn-top ${a.rev ? 'active' : ''}" style="width:100%; margin-top:4px;" onclick="reverbDspState.atlantis.rev=!reverbDspState.atlantis.rev; this.classList.toggle('active'); this.innerText=reverbDspState.atlantis.rev?'REV ON':'REV OFF'; drawIrWaveform();">${a.rev ? 'REV ON' : 'REV OFF'}</button>
                        </div>
                    </div>
                </div>
                `;
            }
        }

        // TAL-Reverb-4 Preset switching
        function cycleTalPreset(dir) {
            const s = reverbDspState.tal;
            s.presetIndex = (s.presetIndex + dir + s.presets.length) % s.presets.length;
            const disp = document.getElementById("tal-preset-display");
            if (disp) disp.innerText = s.presets[s.presetIndex];
            // Apply preset characteristic values
            if (s.presetIndex === 0) { // Dark Big
                s.size = 96.15; s.damp = 0.0; s.diffuse = 100.0; s.stereo = 100.0; s.eqLowCut = 31; s.eqHighCut = 4709;
            } else if (s.presetIndex === 1) { // Vintage Plate 80s
                s.size = 65.0; s.damp = 30.0; s.diffuse = 85.0; s.stereo = 90.0; s.eqLowCut = 120; s.eqHighCut = 6500;
            } else if (s.presetIndex === 2) { // Large Hall
                s.size = 85.0; s.damp = 15.0; s.diffuse = 95.0; s.stereo = 100.0; s.eqLowCut = 60; s.eqHighCut = 8000;
            }
            updateTalFadersVisual();
        }

        function toggleTalSidechain(btn) {
            reverbDspState.tal.sidechain = !reverbDspState.tal.sidechain;
            if (btn) btn.classList.toggle("active", reverbDspState.tal.sidechain);
        }

        // TAL Vertical Fader Drag Implementation (Drag-only, zero jump)
        let activeTalFader = null;
        function startTalFaderDrag(param, evt) {
            const cap = document.getElementById("tal-cap-" + param);
            const track = document.getElementById("tal-track-" + param);
            if (!cap || !track) return;
            const onCap = evt.target === cap || cap.contains(evt.target);
            if (!onCap) {
                const r = cap.getBoundingClientRect();
                const dist = Math.sqrt(Math.pow(evt.clientX - (r.left + r.width/2), 2) + Math.pow(evt.clientY - (r.top + r.height/2), 2));
                if (dist > 18) return;
            }
            evt.preventDefault();
            activeTalFader = { param, track, cap };
            if (evt.pointerId !== undefined) track.setPointerCapture(evt.pointerId);
        }

        window.addEventListener("pointermove", (evt) => {
            if (!activeTalFader) return;
            const r = activeTalFader.track.getBoundingClientRect();
            const topOffset = 8;
            const bottomOffset = 8;
            const travel = r.height - topOffset - bottomOffset;
            if (travel <= 0) return;
            const pointerY = evt.clientY;
            let pct = Math.max(0, Math.min(100, ((r.bottom - bottomOffset - pointerY) / travel) * 100));
            const param = activeTalFader.param;
            if (window.touchGovernor) {
                pct = window.touchGovernor.process(`tal-fader-${param}`, pct);
            }
            if (param === 'delay') {
                reverbDspState.tal.delay = pct / 100;
                document.getElementById("tal-val-delay").innerText = (pct / 100).toFixed(3) + " s";
            } else if (param === 'time') {
                const ms = Math.round(10 + (pct / 100) * 1490);
                delayDspState.timeMs = ms;
                if (delayDspState.syncToTempo) {
                    delayDspState.syncToTempo = false;
                    const btn = document.getElementById("delay-tal-sync-btn");
                    if (btn) { btn.classList.remove("active"); btn.innerText = "TAP SYNC: OFF"; }
                }
                const vEl = document.getElementById("tal-val-time");
                if (vEl) vEl.innerText = ms + " ms";
            } else if (param === 'subdivision') {
                const subSteps = ["1/16", "1/8t", "1/8", "1/8d", "1/4"];
                const idx = Math.min(4, Math.max(0, Math.floor((pct / 100) * 5)));
                delayDspState.subdivision = subSteps[idx];
                if (delayDspState.syncToTempo) updateDelayTimeFromBpm();
                const vEl = document.getElementById("tal-val-subdivision");
                if (vEl) vEl.innerText = delayDspState.subdivision;
            } else if (param === 'pingPong') {
                delayDspState.pingPong = pct >= 50;
                const vEl = document.getElementById("tal-val-pingPong");
                if (vEl) vEl.innerText = delayDspState.pingPong ? "PING-PONG" : "STEREO";
            } else if (param === 'delayDry') {
                delayDspState.dry = pct;
                const vEl = document.getElementById("tal-val-delayDry");
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
            } else if (param === 'delayWet') {
                delayDspState.wet = pct;
                const vEl = document.getElementById("tal-val-delayWet");
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
            } else if (param in delayDspState) {
                delayDspState[param] = pct;
                const vEl = document.getElementById("tal-val-" + param);
                if (vEl) vEl.innerText = pct.toFixed(1) + " %";
            } else {
                reverbDspState.tal[param] = pct;
                const vEl = document.getElementById("tal-val-" + param);
                if (vEl) vEl.innerText = pct.toFixed(2) + " %";
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
            const w = cvs.width, h = cvs.height;
            ctx.clearRect(0, 0, w, h);

            const cx = w / 2, cy = h / 2, radius = 16;
            const startAngle = 0.75 * Math.PI;
            const endAngle = 2.25 * Math.PI;
            const currentAngle = startAngle + normVal * (endAngle - startAngle);

            // Track background arc
            ctx.beginPath();
            ctx.arc(cx, cy, radius, startAngle, endAngle);
            ctx.strokeStyle = "#23272e";
            ctx.lineWidth = 3;
            ctx.stroke();

            // Active indicator arc
            ctx.beginPath();
            ctx.arc(cx, cy, radius, startAngle, currentAngle);
            ctx.strokeStyle = color;
            ctx.lineWidth = 3;
            ctx.stroke();

            // Inner knob body
            ctx.beginPath();
            ctx.arc(cx, cy, radius - 4, 0, 2 * Math.PI);
            const grad = ctx.createRadialGradient(cx - 3, cy - 3, 2, cx, cy, radius);
            grad.addColorStop(0, "#4a505a");
            grad.addColorStop(1, "#17181c");
            ctx.fillStyle = grad;
            ctx.fill();
            ctx.strokeStyle = "#000";
            ctx.lineWidth = 1;
            ctx.stroke();

            // Pointer line
            const px = cx + Math.cos(currentAngle) * (radius - 5);
            const py = cy + Math.sin(currentAngle) * (radius - 5);
            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(px, py);
            ctx.strokeStyle = "#fff";
            ctx.lineWidth = 2;
            ctx.stroke();
        }

        function initTalRotaries() {
            Object.keys(talKnobDefs).forEach(k => {
                const def = talKnobDefs[k];
                const rawVal = reverbDspState.tal[k] ?? def.min;
                const norm = Math.max(0, Math.min(1, (rawVal - def.min) / (def.max - def.min)));
                drawTalKnob("tal-knob-" + k, norm, def.color);

                const cvs = document.getElementById("tal-knob-" + k);
                if (cvs && !cvs.dataset.bound) {
                    cvs.dataset.bound = "true";
                    let startY = 0;
                    let startVal = norm;
                    cvs.addEventListener("pointerdown", (e) => {
                        startY = e.clientY;
                        startVal = (reverbDspState.tal[k] - def.min) / (def.max - def.min);
                        cvs.setPointerCapture(e.pointerId);
                        const onMove = (ev) => {
                            const dy = startY - ev.clientY;
                            const speedCoeff = maxSpeedDamping <= 3 ? 0.008 : (0.008 / (1 + (maxSpeedDamping - 3) * 0.45));
                            let newNorm = Math.max(0, Math.min(1, startVal + dy * speedCoeff));
                            let actualVal = def.min + newNorm * (def.max - def.min);
                            if (window.touchGovernor) {
                                actualVal = window.touchGovernor.process(`tal-${k}`, actualVal);
                                newNorm = Math.max(0, Math.min(1, (actualVal - def.min) / (def.max - def.min)));
                            }
                            reverbDspState.tal[k] = (def.unit.includes("Bit") || def.unit.includes("Hz")) ? Math.round(actualVal) : parseFloat(actualVal.toFixed(2));
                            drawTalKnob("tal-knob-" + k, newNorm, def.color);
                            const valEl = document.getElementById("tal-val-" + k);
                            if (valEl) valEl.innerText = reverbDspState.tal[k] + def.unit;
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
                const rawVal = delayDspState[def.key] ?? (k === 'drive' ? 30 : (k === 'flutter' ? 20 : (k === 'spread' ? 50 : def.min)));
                const norm = Math.max(0, Math.min(1, (rawVal - def.min) / (def.max - def.min)));
                drawTalKnob("tal-delay-knob-" + k, norm, def.color);

                const cvs = document.getElementById("tal-delay-knob-" + k);
                if (cvs && !cvs.dataset.bound) {
                    cvs.dataset.bound = "true";
                    let startY = 0;
                    let startVal = norm;
                    cvs.addEventListener("pointerdown", (e) => {
                        startY = e.clientY;
                        const currentVal = delayDspState[def.key] ?? (k === 'drive' ? 30 : (k === 'flutter' ? 20 : (k === 'spread' ? 50 : def.min)));
                        startVal = (currentVal - def.min) / (def.max - def.min);
                        cvs.setPointerCapture(e.pointerId);
                        const onMove = (ev) => {
                            const dy = startY - ev.clientY;
                            const speedCoeff = maxSpeedDamping <= 3 ? 0.008 : (0.008 / (1 + (maxSpeedDamping - 3) * 0.45));
                            let newNorm = Math.max(0, Math.min(1, startVal + dy * speedCoeff));
                            let actualVal = def.min + newNorm * (def.max - def.min);
                            if (window.touchGovernor) {
                                actualVal = window.touchGovernor.process(`tal-delay-${k}`, actualVal);
                                newNorm = Math.max(0, Math.min(1, (actualVal - def.min) / (def.max - def.min)));
                            }
                            delayDspState[def.key] = (def.unit.includes("Hz")) ? Math.round(actualVal) : parseFloat(actualVal.toFixed(1));
                            drawTalKnob("tal-delay-knob-" + k, newNorm, def.color);
                            const valEl = document.getElementById("tal-delay-val-" + k);
                            if (valEl) {
                                if (k === 'highCut') {
                                    valEl.innerText = (delayDspState[def.key] / 1000).toFixed(1) + " kHz";
                                } else {
                                    valEl.innerText = delayDspState[def.key] + def.unit;
                                }
                            }
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

        // Convolver Real-time Waveform Canvas
        function initIrCanvas() {
            setTimeout(drawIrWaveform, 50);
        }

        function drawIrWaveform() {
            const canvas = document.getElementById("ir-waveform-canvas");
            if (!canvas) return;
            const ctx = canvas.getContext("2d");
            const w = canvas.width = canvas.parentElement.clientWidth - 20;
            const h = canvas.height = 120;
            const a = reverbDspState.atlantis;

            ctx.clearRect(0, 0, w, h);

            // Time grid lines
            ctx.strokeStyle = "#161b24";
            ctx.lineWidth = 1;
            for (let x = 0; x < w; x += 40) {
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, h);
                ctx.stroke();
            }

            // Zero line
            ctx.strokeStyle = "#252e3d";
            ctx.beginPath();
            ctx.moveTo(0, h / 2);
            ctx.lineTo(w, h / 2);
            ctx.stroke();

            // Pre-delay blank region
            const preDelayPx = Math.min(w * 0.4, (a.preDelay / 500) * (w * 0.4));
            if (preDelayPx > 2) {
                ctx.fillStyle = "rgba(239, 68, 68, 0.15)";
                ctx.fillRect(0, 0, preDelayPx, h);
                ctx.strokeStyle = "#ef4444";
                ctx.beginPath();
                ctx.moveTo(preDelayPx, 0);
                ctx.lineTo(preDelayPx, h);
                ctx.stroke();
                ctx.fillStyle = "#ef4444";
                ctx.font = "9px monospace";
                ctx.fillText(`PRE-DELAY: ${a.preDelay}ms`, 4, 14);
            }

            // Synthesize and render realistic IR acoustic decay envelope tailored to preset acoustics
            const startX = preDelayPx;
            const irLength = w - startX;
            let baseDecay = 0.007;
            const model = (a.irModel || "").toLowerCase();
            if (model.includes("large hall") || model.includes("cathedral") || model.includes("ambient") || model.includes("deep space") || model.includes("bloom")) {
                baseDecay = 0.0035; // long spacious acoustic bloom
            } else if (model.includes("plate") || model.includes("emt140") || model.includes("gold plate")) {
                baseDecay = 0.006; // bright dense shimmering decay
            } else if (model.includes("room") || model.includes("studio") || model.includes("wood room") || model.includes("drum")) {
                baseDecay = 0.014; // fast tight early reflections
            } else if (model.includes("chamber")) {
                baseDecay = 0.008; // rich diffuse medium decay
            }
            const decayPower = (baseDecay + (a.damping / 100) * 0.018) * (100 / Math.max(20, a.roomSize));

            ctx.beginPath();
            ctx.moveTo(startX, h / 2);

            for (let i = 0; i < irLength; i++) {
                const t = a.rev ? (irLength - i) : i;
                const env = Math.exp(-t * decayPower);
                const noise = (Math.sin(i * 0.8) + Math.cos(i * 2.3) + Math.sin(i * 7.1)) / 3;
                const amp = env * noise * (h * 0.42);
                ctx.lineTo(startX + i, (h / 2) + amp);
            }

            ctx.strokeStyle = a.rev ? "#f59e0b" : "#818cf8";
            ctx.lineWidth = 1.5;
            ctx.stroke();

            // Fill gradient under envelope
            const grad = ctx.createLinearGradient(0, 0, 0, h);
            grad.addColorStop(0, a.rev ? "rgba(245, 158, 11, 0.35)" : "rgba(129, 140, 248, 0.35)");
            grad.addColorStop(1, "rgba(0, 0, 0, 0)");
            ctx.fillStyle = grad;
            ctx.lineTo(w, h / 2);
            ctx.lineTo(startX, h / 2);
            ctx.fill();
        }


        // ============================================================
        // 9. ANALOG OBSESSION VINTAGE HARDWARE SUITE
        // LALA Leveling Amp, FETISH 76, BUSTER Bus Limiter & Summing
        // ============================================================
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

        // Draw illuminated analog VU meter with swinging needle
        function renderAoVuMeter(canvasId, grDb = 0, style = 'lala') {
            const cvs = document.getElementById(canvasId);
            if (!cvs) return;
            const ctx = cvs.getContext('2d');
            const w = cvs.width;
            const h = cvs.height;

            ctx.clearRect(0, 0, w, h);

            // Warm incandescent backlight gradient
            const bgGrad = ctx.createRadialGradient(w / 2, h * 0.95, 10, w / 2, h * 0.95, w * 0.7);
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
            ctx.fillStyle = bgGrad;
            ctx.fillRect(0, 0, w, h);

            // Arched scale arc
            const pivotX = w / 2;
            const pivotY = h * 1.35;
            const arcRadius = h * 1.05;

            // Needle angle range: -40 deg to +40 deg
            const minAngle = -Math.PI * 0.30;
            const maxAngle = Math.PI * 0.30;

            // VU Markings: -20 to +3 dB
            const marks = [
                { db: -20, label: '20' },
                { db: -10, label: '10' },
                { db: -7,  label: '7' },
                { db: -5,  label: '5' },
                { db: -3,  label: '3' },
                { db: -2,  label: '2' },
                { db: -1,  label: '1' },
                { db: 0,   label: '0' },
                { db: 1,   label: '+1' },
                { db: 2,   label: '+2' },
                { db: 3,   label: '+3' }
            ];

            // Helper to map dB (-20 .. +3) to angle
            function dbToAngle(db) {
                const clamped = Math.max(-20, Math.min(3, db));
                // Non-linear VU curve
                let norm = (clamped + 20) / 23;
                return minAngle + norm * (maxAngle - minAngle) - Math.PI / 2;
            }

            // Draw Scale Arc
            ctx.beginPath();
            ctx.arc(pivotX, pivotY, arcRadius, minAngle - Math.PI / 2, maxAngle - Math.PI / 2);
            ctx.strokeStyle = '#292524';
            ctx.lineWidth = 1.5;
            ctx.stroke();

            // Red overload band on right (> 0 dB)
            ctx.beginPath();
            const zeroAngle = dbToAngle(0);
            ctx.arc(pivotX, pivotY, arcRadius, zeroAngle, maxAngle - Math.PI / 2);
            ctx.strokeStyle = '#dc2626';
            ctx.lineWidth = 3.5;
            ctx.stroke();

            // Draw graduation ticks and numbers
            ctx.font = 'bold 9px sans-serif';
            ctx.textAlign = 'center';
            marks.forEach(m => {
                const angle = dbToAngle(m.db);
                const r1 = arcRadius - 2;
                const r2 = arcRadius + 6;
                const tx = pivotX + Math.cos(angle) * (arcRadius + 14);
                const ty = pivotY + Math.sin(angle) * (arcRadius + 14);

                ctx.beginPath();
                ctx.moveTo(pivotX + Math.cos(angle) * r1, pivotY + Math.sin(angle) * r1);
                ctx.lineTo(pivotX + Math.cos(angle) * r2, pivotY + Math.sin(angle) * r2);
                ctx.strokeStyle = m.db > 0 ? '#dc2626' : '#1c1917';
                ctx.lineWidth = 1.2;
                ctx.stroke();

                ctx.fillStyle = m.db > 0 ? '#dc2626' : '#1c1917';
                ctx.fillText(m.label, tx, ty);
            });

            // Center VU label
            ctx.font = '900 11px sans-serif';
            ctx.fillStyle = style === 'lala' ? '#451a03' : '#1e3a8a';
            ctx.fillText('VU', pivotX, h * 0.48);
            ctx.font = 'bold 7px sans-serif';
            ctx.fillText('GAIN REDUCTION (dB)', pivotX, h * 0.58);

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

        // Analog VU Needle Rendering (driven exclusively by real WebSocket telemetry)
        function startAoVuLoop(meterId, style = 'lala', isLimiter = false) {
            if (aoVuMeterAnimId) {
                cancelAnimationFrame(aoVuMeterAnimId);
                aoVuMeterAnimId = null;
            }
            renderAoVuMeter(meterId, 0, style); // Statically at 0 until live telemetry arrives
        }

        // ============================================================

                function bindAoRotary(canvasId, readoutId, min, max, unit, getter, setter, style = 'lala', isLarge = false) {
            const cvs = document.getElementById(canvasId);
            if (!cvs || cvs.dataset.bound) return;
            cvs.dataset.bound = 'true';

            const rawVal = getter();
            const initNorm = Math.max(0, Math.min(1, (rawVal - min) / (max - min)));
            drawAoKnob(canvasId, initNorm, style, isLarge);

            let startY = 0;
            let startVal = initNorm;

            cvs.addEventListener('pointerdown', (e) => {
                startY = e.clientY;
                startVal = (getter() - min) / (max - min);
                cvs.setPointerCapture(e.pointerId);

                const onMove = (ev) => {
                    const dy = startY - ev.clientY;
                    // Apply touch slowing damping
                    const speedCoeff = maxSpeedDamping <= 3 ? 0.006 : (0.006 / (1 + (maxSpeedDamping - 3) * 0.45));
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

        // FETISH 76 Ratio Switchers
        function setFetishRatio(ratioStr) {
            fetish76State.ratio = ratioStr;
            fetish76State.allButtons = false;
            document.querySelectorAll('.ao-ratio-btn').forEach(btn => {
                if (btn.classList.contains('all-in')) {
                    btn.classList.remove('active');
                } else {
                    btn.classList.toggle('active', btn.innerText.startsWith(ratioStr + ':'));
                }
            });
        }

        function toggleFetishAllButtons() {
            fetish76State.allButtons = !fetish76State.allButtons;
            document.querySelectorAll('.ao-ratio-btn').forEach(btn => {
                if (btn.classList.contains('all-in')) {
                    btn.classList.toggle('active', fetish76State.allButtons);
                } else {
                    btn.classList.toggle('active', fetish76State.allButtons);
                }
            });
        }

        // Analog Summing & Console Transformer Switchers
        function setSummingConsole(consoleType) {
            analogSummingState.console = consoleType;
            updateSummingHeaderBtn();
        }

        function updateSummingHeaderBtn() {
            const btn = document.getElementById('btn-analog-summing');
            if (!btn) return;
            const names = {
                neve: 'NEVE 8048',
                api: 'API 2520',
                ssl: 'SSL 4000E',
                tube: 'TUBE DESK'
            };
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

        // Window Resize Listener for Responsive Canvas Bitmap Rendering


        // =========================================================================
        // DUAL-MODE CHANNEL DSP ENGINE (LINEAR RACK VS. MODULAR CANVAS)
        // =========================================================================

        // Plugin Catalog for Linear 6-Slot Inserts
        window.AVAILABLE_PLUGINS = [
    {
        "id": "thru",
        "name": "EMPTY (THRU)",
        "cat": "Passthrough",
        "icon": "➡️",
        "path": ""
    },
    {
        "id": "COUNTRY.pedalboard",
        "name": "Country",
        "cat": "Amp / Guitar Rig",
        "icon": "🎸",
        "path": "/var/modep/pedalboards/COUNTRY.pedalboard"
    },
    {
        "id": "Guitar_main.pedalboard",
        "name": "Guitar Main",
        "cat": "Amp / Guitar Rig",
        "icon": "🎸",
        "path": "/var/modep/pedalboards/Guitar_main.pedalboard"
    },
    {
        "id": "acoustic.pedalboard",
        "name": "Acoustic",
        "cat": "Acoustic / Piezo",
        "icon": "🪕",
        "path": "/var/modep/pedalboards/acoustic.pedalboard"
    },
    {
        "id": "acoustic_2_.pedalboard",
        "name": "Acoustic 2",
        "cat": "Acoustic / Piezo",
        "icon": "🪕",
        "path": "/var/modep/pedalboards/acoustic_2_.pedalboard"
    },
    {
        "id": "acustic_nam.pedalboard",
        "name": "Acustic Nam",
        "cat": "Amp / Guitar Rig",
        "icon": "🎸",
        "path": "/var/modep/pedalboards/acustic_nam.pedalboard"
    },
    {
        "id": "dan_2_.pedalboard",
        "name": "Dan 2",
        "cat": "General",
        "icon": "🎛️",
        "path": "/var/modep/pedalboards/dan_2_.pedalboard"
    },
    {
        "id": "dan_3.pedalboard",
        "name": "Dan 3",
        "cat": "General",
        "icon": "🎛️",
        "path": "/var/modep/pedalboards/dan_3.pedalboard"
    },
    {
        "id": "default.pedalboard",
        "name": "Default",
        "cat": "General",
        "icon": "🎛️",
        "path": "/var/modep/pedalboards/default.pedalboard"
    },
    {
        "id": "guitar_1.pedalboard",
        "name": "Guitar 1",
        "cat": "Amp / Guitar Rig",
        "icon": "🎸",
        "path": "/var/modep/pedalboards/guitar_1.pedalboard"
    },
    {
        "id": "parlor_setup.pedalboard",
        "name": "Parlor Setup",
        "cat": "Acoustic / Piezo",
        "icon": "🪕",
        "path": "/var/modep/pedalboards/parlor_setup.pedalboard"
    }
];

        let activePickerChannel = 0;
        let activePickerSlot = 0;

        // Initialize or retrieve Dual-Mode State from state / localStorage
        window.channelDualDspState = {};

        function loadDualDspState() {
            try {
                const saved = localStorage.getItem("modep_channel_dual_dsp");
                if (saved) {
                    window.channelDualDspState = JSON.parse(saved);
                }
            } catch(e) {}

            for (let ch = 0; ch < 8; ch++) {
                if (!window.channelDualDspState[ch]) {
                    window.channelDualDspState[ch] = {
                        mode: (ch === 6 || ch === 7) ? 'canvas' : 'rack', // Ch 7/8 default to Canvas
                        rackPort: (ch === 6 || ch === 7) ? 8881 : ((ch === 0 || ch === 1) ? 8881 : ((ch === 2 || ch === 3) ? 8883 : 8884)),
                        canvasBypass: false,
                                                slots: [
                            { name: "EMPTY (THRU)", bypassed: true },
                            { name: "EMPTY (THRU)", bypassed: true },
                            { name: "EMPTY (THRU)", bypassed: true },
                            { name: "EMPTY (THRU)", bypassed: true }
                        ]
                    };
                }
            }
        }

        function saveDualDspState() {
            try {
                localStorage.setItem("modep_channel_dual_dsp", JSON.stringify(window.channelDualDspState));
            } catch(e) {}
        }

        window.setChannelDspMode = function(ch, mode) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            window.channelDualDspState[ch].mode = mode;
            saveDualDspState();

            // Toggle active buttons on mode bar
            const btnRack = document.getElementById("btn-mode-rack-" + ch);
            const btnCanvas = document.getElementById("btn-mode-canvas-" + ch);
            if (btnRack) btnRack.classList.toggle("active", mode === "rack");
            if (btnCanvas) btnCanvas.classList.toggle("active", mode === "canvas");

            // Toggle containers
            const rackCont = document.getElementById("dsp-rack-container-" + ch);
            const canvasCont = document.getElementById("dsp-canvas-container-" + ch);
            if (rackCont) rackCont.style.display = (mode === "rack") ? "block" : "none";
            if (canvasCont) canvasCont.style.display = (mode === "canvas") ? "block" : "none";

            // Notify backend router via WebSocket if connected
            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                window.meterWs.send(JSON.stringify({
                    action: "set_dsp_mode",
                    channel: ch,
                    mode: mode,
                    rack_port: window.channelDualDspState[ch].rackPort
                }));
            }
        };

        window.setChannelCanvasRack = function(ch, port) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            window.channelDualDspState[ch].rackPort = parseInt(port);
            saveDualDspState();
            console.log(`[Dual-Mode DSP] Channel ${ch + 1} assigned to MODEP Rack on port :${port}`);
        };

        window.toggleCanvasHardBypass = function(ch) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            const curr = !window.channelDualDspState[ch].canvasBypass;
            window.channelDualDspState[ch].canvasBypass = curr;
            saveDualDspState();

            const btn = document.getElementById("btn-canvas-bypass-" + ch);
            if (btn) {
                btn.classList.toggle("bypassed", curr);
                btn.innerText = curr ? "CANVAS BYPASS: ACTIVE" : "CANVAS BYPASS: OFF";
            }

            if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
                window.meterWs.send(JSON.stringify({
                    action: "set_canvas_bypass",
                    channel: ch,
                    bypass: curr
                }));
            }
        };

        window.toggleSlotBypass = function(ch, slotIdx) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            const slot = window.channelDualDspState[ch].slots[slotIdx];
            slot.bypassed = !slot.bypassed;
            saveDualDspState();

            const slotEl = document.getElementById(`slot-${ch}-${slotIdx}`);
            const bypBtn = document.getElementById(`slot-byp-${ch}-${slotIdx}`);
            if (slotEl) slotEl.classList.toggle("bypassed", slot.bypassed);
            if (bypBtn) {
                bypBtn.classList.toggle("active", !slot.bypassed);
                bypBtn.innerText = slot.bypassed ? "BYP" : "ACT";
            }
        };

        window.openPluginPickerModal = function(ch, slotIdx) {
            activePickerChannel = ch;
            activePickerSlot = slotIdx;

            const overlay = document.getElementById("plugin-picker-overlay");
            const title = document.getElementById("plugin-picker-title");
            const list = document.getElementById("plugin-picker-list");

            if (title) {
                title.innerText = `CH ${ch + 1} &bull; Assign Slot ${slotIdx + 1}`;
            }

            if (list) {
                list.innerHTML = window.AVAILABLE_PLUGINS.map(p => `
                    <div class="plugin-picker-item" onclick="assignSlotPlugin('${p.name}')">
                        <span class="item-title">${p.icon} ${p.name}</span>
                        <span class="item-cat">${p.cat}</span>
                    </div>
                `).join('');
            }

            if (overlay) overlay.style.display = "flex";
        };

        window.closePluginPickerModal = function() {
            const overlay = document.getElementById("plugin-picker-overlay");
            if (overlay) overlay.style.display = "none";
        };

        window.assignSlotPlugin = function(pluginName) {
            const ch = activePickerChannel;
            const slotIdx = activePickerSlot;
            if (!window.channelDualDspState[ch]) loadDualDspState();
            window.channelDualDspState[ch].slots[slotIdx].name = pluginName;
            window.channelDualDspState[ch].slots[slotIdx].bypassed = false;
            saveDualDspState();

            const nameEl = document.getElementById(`slot-name-${ch}-${slotIdx}`);
            if (nameEl) nameEl.innerText = pluginName;

            const slotEl = document.getElementById(`slot-${ch}-${slotIdx}`);
            const bypBtn = document.getElementById(`slot-byp-${ch}-${slotIdx}`);
            if (slotEl) slotEl.classList.remove("bypassed");
            if (bypBtn) {
                bypBtn.classList.add("active");
                bypBtn.innerText = "ACT";
            }

            window.closePluginPickerModal();
        };

        window.openModepCanvasModal = function(ch) {
            if (!window.channelDualDspState[ch]) loadDualDspState();
            const port = window.channelDualDspState[ch].rackPort || 8881;
            const host = window.location.hostname || "192.168.1.148";
            const url = `http://${host}:${port}/`;

            const overlay = document.getElementById("canvas-modal-overlay");
            const title = document.getElementById("canvas-modal-title");
            const iframe = document.getElementById("canvas-modal-iframe");
            const newTabLink = document.getElementById("canvas-modal-newtab");

            if (title) title.innerText = `🎛️ Channel ${ch + 1} &bull; MODEP Canvas (Port :${port})`;
            if (newTabLink) newTabLink.href = url;
            if (iframe) iframe.src = url;
            if (overlay) overlay.style.display = "flex";
        };

        window.closeCanvasModal = function() {
            const overlay = document.getElementById("canvas-modal-overlay");
            const iframe = document.getElementById("canvas-modal-iframe");
            if (iframe) iframe.src = "about:blank";
            if (overlay) overlay.style.display = "none";
        };

        // Initialize state on DOM load
        window.initDualDspModes = function() {
            loadDualDspState();
            for (let ch = 0; ch < 8; ch++) {
                const state = window.channelDualDspState[ch];
                window.setChannelDspMode(ch, state.mode);

                // Update slot texts and bypass
                if (state.slots) {
                    state.slots.forEach((s, idx) => {
                        const nameEl = document.getElementById(`slot-name-${ch}-${idx}`);
                        if (nameEl) nameEl.innerText = s.name;
                        const slotEl = document.getElementById(`slot-${ch}-${idx}`);
                        const bypBtn = document.getElementById(`slot-byp-${ch}-${idx}`);
                        if (slotEl) slotEl.classList.toggle("bypassed", s.bypassed);
                        if (bypBtn) {
                            bypBtn.classList.toggle("active", !s.bypassed);
                            bypBtn.innerText = s.bypassed ? "BYP" : "ACT";
                        }
                    });
                }

                // Update canvas rack select
                const sel = document.getElementById(`canvas-rack-select-${ch}`);
                if (sel) sel.value = String(state.rackPort);

                // Update canvas bypass button
                const byp = document.getElementById(`btn-canvas-bypass-${ch}`);
                if (byp) {
                    byp.classList.toggle("bypassed", state.canvasBypass);
                    byp.innerText = state.canvasBypass ? "CANVAS BYPASS: ACTIVE" : "CANVAS BYPASS: OFF";
                }
            }
        };

    
    
    // Complete Global Window Exports for DSP Rack, Delay, Reverb, Compressors & Summing
    window.getChannelRackState = getChannelRackState;
    window.openChannelRackModal = openChannelRackModal;
    window.closeChannelRackModal = closeChannelRackModal;
    window.dispatchDspParam = dispatchDspParam;
    window.triggerDenoiseLearn = triggerDenoiseLearn;
    window.setChannelCompModel = setChannelCompModel;
    window.togglePhaseInvert = togglePhaseInvert;
    window.setChannelDirectTap = setChannelDirectTap;
    window.selectRackEqBand = selectRackEqBand;
    window.renderMiniReEq = renderMiniReEq;
    window.updateFocusedChannelStrip = updateFocusedChannelStrip;
    window.startFocusedFaderDrag = startFocusedFaderDrag;

    // Delay & Reverb Suite Exports
    window.delayDspState = delayDspState;
    window.reverbDspState = reverbDspState;
    window.renderDelayControls = renderDelayControls;
    window.updateDelayTimeMs = updateDelayTimeMs;
    window.updateDelaySubdivision = updateDelaySubdivision;
    window.toggleDelaySync = toggleDelaySync;
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

    // Analog Obsession Compressors, Limiter & Summing Exports
    window.fetish76State = fetish76State;
    window.lalaState = lalaState;
    window.busterLimiterState = busterLimiterState;
    window.analogSummingState = analogSummingState;
    window.renderAoVuMeter = renderAoVuMeter;
    window.startAoVuLoop = startAoVuLoop;
    window.drawAoKnob = drawAoKnob;
    window.bindAoRotary = bindAoRotary;
    window.setFetishRatio = setFetishRatio;
    window.toggleFetishAllButtons = toggleFetishAllButtons;
    window.setSummingConsole = setSummingConsole;
    window.updateSummingHeaderBtn = updateSummingHeaderBtn;

    window.showDspRackPage = showDspRackPage;
    window.handleRackGainChange = handleRackGainChange;
    window.toggleRackRegain = toggleRackRegain;
    window.closeDspRackPage = closeDspRackPage;
})(window);

