// dsp-modals-ao.js - Authentic Analog Obsession Hardware Chassis Modals (FETISH 76, LALA, BUSTER, SUMMING)
(function(window) {
    'use strict';

    window.renderAoModalContent = function(type, target, targetName, displayTargetName, modal, title, body, compStateOverride, modeBarHtml = '') {
        const cs = compStateOverride || (window.getCompState ? window.getCompState(target) : null);
        const fetish76State = cs ? cs.fetish76 : window.fetish76State;
        const lalaState = cs ? cs.lala : window.lalaState;
        const busterLimiterState = cs ? cs.limiter : window.busterLimiterState;
        const analogSummingState = window.analogSummingState;
        const startAoVuLoop = window.startAoVuLoop;
        const bindAoRotary = window.bindAoRotary;
        const grBarHtml = (typeof window.renderDigitalGrBarHtml === 'function')
            ? `<div style="width:100%; margin-top:4px;">${window.renderDigitalGrBarHtml('comp-digital-gr-bar', 'DIGITAL GAIN REDUCTION (0 to -24 dB)')}</div>`
            : '';

        if (type === 'preamp1176') {
            const p = cs.preamp1176;
            const chNum = parseInt(target, 10) || 0;
            title.innerHTML = `🔥 STAGE 1: 1176 CLASS-A FET PREAMP EMULATION &bull; ${displayTargetName}`;
            body.innerHTML = `
                <div class="ao-chassis dark-theme" style="border-color:#d97706;">
                    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;">
                        <div class="ao-brand-wrap">
                            <div><div class="ao-brand-logo" style="color:#fbbf24;">PATCHBOX PRIME</div><div style="font-size:0.6rem; color:#94a3b8; font-weight:700;">DISCRETE CLASS-A INPUT TRANSFORMER &amp; FET STAGE</div></div>
                            <div style="border-left:1px solid #404040; padding-left:12px;"><div class="ao-model-title" style="color:#fbbf24;">1176 INPUT PREAMP</div><div class="ao-model-sub">STAGE 1 HARMONIC SATURATION &rarr; FEEDS STAGE 2 REGAIN</div></div>
                        </div>
                        <div style="display:flex; gap:10px; align-items:center;">
                            <span id="full-preamp-clip-led-${chNum}" style="font-size:0.75rem; padding:4px 10px; border-radius:4px; background:#10b981; color:#052e16; font-weight:900;">CLEAN</span>
                            <button class="ao-toggle-btn power ${p.enabled ? '' : 'off'}" id="full-preamp-en-btn-${chNum}" onclick="togglePreampEnabled(${chNum})">${p.enabled ? 'ON' : 'BYPASS'}</button>
                        </div>
                    </div>
                    <div class="ao-main-grid" style="grid-template-columns: 240px 1fr 240px; align-items:center;">
                        <div class="ao-knob-box"><span class="ao-knob-title">1176 INPUT DRIVE</span><canvas id="pre1176-knob-drive" class="ao-knob-canvas" width="92" height="92"></canvas><span class="ao-knob-readout" id="full-preamp-drive-val-${chNum}" style="color:#fbbf24;">+${p.drive.toFixed(1)} dB</span></div>
                        <div style="display:flex; flex-direction:column; gap:10px; align-items:center; width:100%;">
                            <div class="ao-meter-housing" style="width:100%; max-width:320px;">
                                <div class="ao-vu-glass">
                                    <canvas id="pre1176-vu-meter" class="ao-vu-canvas" width="320" height="105"></canvas>
                                </div>
                            </div>
                            ${window.renderDigitalGrBarHtml(`preamp-sat-bar-${chNum}`, 'STAGE 1 FET DRIVE & HARMONIC SATURATION')}
                            <div style="display:flex; gap:10px; align-items:center; margin-top:4px;">
                                <span style="font-size:0.72rem; color:#94a3b8; font-weight:800;">CLIP TOPOLOGY:</span>
                                <button class="tap-mode-btn ${p.clipMode === 'hard' ? 'active' : ''}" id="full-preamp-mode-btn-${chNum}" style="min-height:38px; padding:0 16px;" onclick="togglePreampClipMode(${chNum})">${p.clipMode === 'soft' ? 'SOFT FET SATURATION' : 'HARD BRICKWALL CLIP'}</button>
                            </div>
                        </div>
                        <div class="ao-knob-box"><span class="ao-knob-title">OUTPUT TRIM (TO REGAIN)</span><canvas id="pre1176-knob-trim" class="ao-knob-canvas" width="92" height="92"></canvas><span class="ao-knob-readout" id="full-preamp-trim-val-${chNum}" style="color:#38bdf8;">${p.trim >= 0 ? '+' : ''}${p.trim.toFixed(1)} dB</span></div>
                    </div>
                </div>`;
            startAoVuLoop('pre1176-vu-meter', 'lala', false, () => {
                const stNow = window.getCompState ? window.getCompState(chNum).preamp1176 : p;
                if (!stNow || !stNow.enabled) return 0.0;
                const stageDb = (typeof stNow.livePeakDb === 'number' ? stNow.livePeakDb : -60.0) + (stNow.drive || 0);
                return Math.max(0, Math.min(20, (stageDb + 45.0) * 0.45));
            });
            bindAoRotary('pre1176-knob-drive', `full-preamp-drive-val-${chNum}`, 0, 36, ' dB', () => p.drive, v => window.setPreampDrive(chNum, v), 'lala', true);
            bindAoRotary('pre1176-knob-trim', `full-preamp-trim-val-${chNum}`, -24, 12, ' dB', () => p.trim, v => window.setPreampTrim(chNum, v), 'fetish', true);
            window.updatePreampClipIndicator(chNum, p.livePeakDb);
            return true;
        } else if (type === 'dual') {
            const f = fetish76State, l = lalaState;
            title.innerHTML = `⚡ DUAL SERIES COMPRESSOR (1. FETISH 76 ➔ 2. LALA OPTO) &bull; ${displayTargetName}`;
            body.innerHTML = modeBarHtml + `
                <div class="ao-chassis dark-theme" style="padding:14px;">
                    <div style="margin-bottom:10px;">${window.renderDigitalGrBarHtml('comp-digital-gr-bar', 'TOTAL SERIES GAIN REDUCTION (FET 76 + LALA OPTO)')}</div>
                    <div style="display:grid; grid-template-columns: 1fr 1fr; gap:14px;">
                        <div style="background:#0f172a; border:1px solid #0284c7; border-radius:8px; padding:12px; display:flex; flex-direction:column; align-items:center; gap:10px;">
                            <div style="font-size:0.8rem; font-weight:900; color:#38bdf8;">STAGE A: FETISH 76 FAST PEAK LIMITER</div>
                            <div style="display:flex; gap:18px;">
                                <div class="ao-knob-box"><span class="ao-knob-title">76 INPUT</span><canvas id="dual-fet-in" width="74" height="74"></canvas><span class="ao-knob-readout" id="dual-fet-in-val">${f.input} dB</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">76 OUTPUT</span><canvas id="dual-fet-out" width="74" height="74"></canvas><span class="ao-knob-readout" id="dual-fet-out-val">${f.output} dB</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">RELEASE</span><canvas id="dual-fet-rel" width="66" height="66"></canvas><span class="ao-knob-readout" id="dual-fet-rel-val">${f.release}</span></div>
                            </div>
                            <div class="ao-ratio-group">
                                ${['4','8','12','20'].map(r => `<button class="ao-ratio-btn ${f.ratio===r && !f.allButtons ? 'active':''}" onclick="setFetishRatio('${r}','${target}')">${r}:1</button>`).join('')}
                                <button class="ao-ratio-btn all-in ${f.allButtons ? 'active':''}" onclick="toggleFetishAllButtons('${target}')">ALL IN</button>
                            </div>
                        </div>
                        <div style="background:#1c1917; border:1px solid #d97706; border-radius:8px; padding:12px; display:flex; flex-direction:column; align-items:center; gap:10px;">
                            <div style="font-size:0.8rem; font-weight:900; color:#fbbf24;">STAGE B: LALA OPTICAL LEVELING AMPLIFIER</div>
                            <div style="display:flex; gap:18px;">
                                <div class="ao-knob-box"><span class="ao-knob-title">PEAK REDUCTION</span><canvas id="dual-la-peak" width="74" height="74"></canvas><span class="ao-knob-readout" id="dual-la-peak-val">${l.peakReduction}</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">MAKEUP GAIN</span><canvas id="dual-la-gain" width="74" height="74"></canvas><span class="ao-knob-readout" id="dual-la-gain-val">${l.gain}</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">DRY/WET MIX</span><canvas id="dual-la-mix" width="66" height="66"></canvas><span class="ao-knob-readout" id="dual-la-mix-val">${l.mix} %</span></div>
                            </div>
                            <button class="ao-toggle-btn ${l.limitMode ? 'active' : ''}" style="width:110px;" onclick="toggleLalaModeInline('${target}', this)">${l.limitMode ? 'LIMIT' : 'COMPRESS'}</button>
                        </div>
                    </div>
                </div>`;
            bindAoRotary('dual-fet-in', 'dual-fet-in-val', 0, 60, ' dB', () => f.input, v => window.setCompParam(target, 'fetish76', 'input', Math.round(v)), 'fetish', false);
            bindAoRotary('dual-fet-out', 'dual-fet-out-val', 0, 48, ' dB', () => f.output, v => window.setCompParam(target, 'fetish76', 'output', Math.round(v)), 'fetish', false);
            bindAoRotary('dual-fet-rel', 'dual-fet-rel-val', 1, 7, '', () => f.release, v => window.setCompParam(target, 'fetish76', 'release', Math.round(v)), 'fetish', false);
            bindAoRotary('dual-la-peak', 'dual-la-peak-val', 0, 100, '', () => l.peakReduction, v => window.setCompParam(target, 'lala', 'peakReduction', Math.round(v)), 'lala', false);
            bindAoRotary('dual-la-gain', 'dual-la-gain-val', 0, 100, '', () => l.gain, v => window.setCompParam(target, 'lala', 'gain', Math.round(v)), 'lala', false);
            bindAoRotary('dual-la-mix', 'dual-la-mix-val', 0, 100, ' %', () => l.mix, v => window.setCompParam(target, 'lala', 'mix', Math.round(v)), 'lala', false);
            if (window.updateDigitalGrBar && window.getTargetLiveGr) window.updateDigitalGrBar('comp-digital-gr-bar', window.getTargetLiveGr(target));
            return true;
        } else if (type === 'fet1176' || type === 'fetish') {
            modal.classList.add("ao-modal");
            title.innerHTML = `⚡ ANALOG OBSESSION FETISH 76 &bull; CLASS-A FET PEAK COMPRESSOR &bull; ${displayTargetName}`;
            body.innerHTML = modeBarHtml + `
                <div class="ao-chassis dark-theme">
                    <div class="ao-screw top-left"></div><div class="ao-screw top-right"></div>
                    <div class="ao-screw bottom-left"></div><div class="ao-screw bottom-right"></div>
                    <div style="display:flex; justify-content:space-between; align-items:center;">
                        <div class="ao-brand-wrap">
                            <div>
                                <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                <div style="font-size:0.6rem; color:#64748b; font-weight:700; letter-spacing:1px;">DISCRETE FET AUDIO ELECTRONICS</div>
                            </div>
                            <div style="border-left:1px solid #404040; padding-left:12px;">
                                <div class="ao-model-title" style="color:#38bdf8;">FETISH 76</div>
                                <div class="ao-model-sub">CLASS-A SOLID STATE LIMITING AMPLIFIER &bull; ${displayTargetName}</div>
                            </div>
                        </div>
                        <div class="ao-toggle-box">
                            <span class="ao-toggle-label">POWER</span>
                            <button class="ao-toggle-btn power ${fetish76State.power ? '' : 'off'}" id="ao-fet-power" onclick="var s=window.getCompState('${target}').fetish76; s.power=!s.power; this.classList.toggle('off', !s.power); this.innerText=s.power?'ON':'OFF'; window.setCompParam('${target}','fetish76','power',s.power);">${fetish76State.power ? 'ON' : 'OFF'}</button>
                        </div>
                    </div>
                    <div class="ao-main-grid" style="grid-template-columns: 220px 1fr 280px;">
                        <div style="display:flex; flex-direction:column; gap:16px; align-items:center;">
                            <div class="ao-knob-box">
                                <span class="ao-knob-title">INPUT (DRIVE)</span>
                                <canvas id="fet-knob-input" class="ao-knob-canvas" width="84" height="84"></canvas>
                                <span class="ao-knob-readout" id="fet-val-input" style="color:#38bdf8;">${fetish76State.input} dB</span>
                            </div>
                            <div class="ao-knob-box">
                                <span class="ao-knob-title">OUTPUT (LEVEL)</span>
                                <canvas id="fet-knob-output" class="ao-knob-canvas" width="84" height="84"></canvas>
                                <span class="ao-knob-readout" id="fet-val-output" style="color:#38bdf8;">${fetish76State.output} dB</span>
                            </div>
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:10px;">
                            <div class="ao-meter-housing" style="width:100%; max-width:340px;">
                                <div class="ao-vu-glass" style="height:115px; background:radial-gradient(ellipse at 50% 120%, #e0f2fe 0%, #bae6fd 45%, #7dd3fc 80%, #0284c7 100%);">
                                    <canvas id="fet-vu-meter" class="ao-vu-canvas" width="340" height="115"></canvas>
                                </div>
                            </div>
                            ${grBarHtml}
                            <div style="display:flex; flex-direction:column; align-items:center; gap:4px;">
                                <span style="font-size:0.68rem; font-weight:900; letter-spacing:1px; color:#94a3b8;">COMPRESSION RATIO</span>
                                <div class="ao-ratio-group">
                                    <button class="ao-ratio-btn ${fetish76State.ratio === '4' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('4', '${target}')">4:1</button>
                                    <button class="ao-ratio-btn ${fetish76State.ratio === '8' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('8', '${target}')">8:1</button>
                                    <button class="ao-ratio-btn ${fetish76State.ratio === '12' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('12', '${target}')">12:1</button>
                                    <button class="ao-ratio-btn ${fetish76State.ratio === '20' && !fetish76State.allButtons ? 'active' : ''}" onclick="setFetishRatio('20', '${target}')">20:1</button>
                                    <button class="ao-ratio-btn all-in ${fetish76State.allButtons ? 'active' : ''}" onclick="toggleFetishAllButtons('${target}')">ALL IN</button>
                                </div>
                            </div>
                        </div>
                        <div style="display:flex; flex-direction:column; gap:16px; align-items:center;">
                            <div style="display:flex; gap:14px;">
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">ATTACK</span>
                                    <canvas id="fet-knob-attack" class="ao-knob-canvas" width="76" height="76"></canvas>
                                    <span class="ao-knob-readout" id="fet-val-attack" style="color:#38bdf8;">${fetish76State.attack}</span>
                                </div>
                                <div class="ao-knob-box">
                                    <span class="ao-knob-title">RELEASE</span>
                                    <canvas id="fet-knob-release" class="ao-knob-canvas" width="76" height="76"></canvas>
                                    <span class="ao-knob-readout" id="fet-val-release" style="color:#38bdf8;">${fetish76State.release}</span>
                                </div>
                            </div>
                            <div style="font-size:0.66rem; color:#64748b; text-align:center; padding:0 10px; line-height:1.35;">
                                * Position 7 is fastest attack (20 µs) and release (50 ms). Fast peak limiting with British saturation.
                            </div>
                        </div>
                    </div>
                </div>
            `;
            startAoVuLoop('fet-vu-meter', 'fetish', false);
            bindAoRotary('fet-knob-input', 'fet-val-input', 0, 60, ' dB', () => fetish76State.input, v => window.setCompParam(target, 'fetish76', 'input', Math.round(v)), 'fetish', true);
            bindAoRotary('fet-knob-output', 'fet-val-output', 0, 48, ' dB', () => fetish76State.output, v => window.setCompParam(target, 'fetish76', 'output', Math.round(v)), 'fetish', true);
            bindAoRotary('fet-knob-attack', 'fet-val-attack', 1, 7, '', () => fetish76State.attack, v => window.setCompParam(target, 'fetish76', 'attack', Math.round(v)), 'fetish', false);
            bindAoRotary('fet-knob-release', 'fet-val-release', 1, 7, '', () => fetish76State.release, v => window.setCompParam(target, 'fetish76', 'release', Math.round(v)), 'fetish', false);
            if (window.updateDigitalGrBar && window.getTargetLiveGr) window.updateDigitalGrBar('comp-digital-gr-bar', window.getTargetLiveGr(target));
            return true;
        } else if (type === 'la2a' || type === 'lala') {
            modal.classList.add("ao-modal");
            title.innerHTML = `💡 ANALOG OBSESSION LALA &bull; OPTICAL LEVELING AMPLIFIER &bull; ${displayTargetName}`;
            body.innerHTML = modeBarHtml + `
                <div class="ao-chassis">
                    <div class="ao-screw top-left"></div><div class="ao-screw top-right"></div>
                    <div class="ao-screw bottom-left"></div><div class="ao-screw bottom-right"></div>
                    <div style="display:flex; justify-content:space-between; align-items:center;">
                        <div class="ao-brand-wrap">
                            <div>
                                <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                <div style="font-size:0.6rem; color:#78716c; font-weight:700; letter-spacing:1px;">PROFESSIONAL AUDIO PLUGINS</div>
                            </div>
                            <div style="border-left:2px solid #a8a29e; padding-left:14px;">
                                <div class="ao-model-title">LALA</div>
                                <div class="ao-model-sub">OPTICAL LEVELING AMPLIFIER &bull; ${displayTargetName}</div>
                            </div>
                        </div>
                        <div class="ao-toggle-box">
                            <span class="ao-toggle-label">POWER</span>
                            <button class="ao-toggle-btn power ${lalaState.power ? '' : 'off'}" id="ao-lala-power" onclick="var s=window.getCompState('${target}').lala; s.power=!s.power; this.classList.toggle('off', !s.power); this.innerText=s.power?'ON':'OFF'; window.setCompParam('${target}','lala','power',s.power);">${lalaState.power ? 'ON' : 'OFF'}</button>
                        </div>
                    </div>
                    <div class="ao-main-grid">
                        <div style="display:flex; flex-direction:column; gap:10px; align-items:center;">
                            <div style="font-size:0.72rem; font-weight:900; color:#44403c; border-bottom:1px solid #a8a29e; width:100%; text-align:center; padding-bottom:3px;">SIDECHAIN &amp; EMPHASIS</div>
                            <div style="display:grid; grid-template-columns: 1fr 1fr; gap:12px; width:100%;">
                                <div class="ao-knob-box"><span class="ao-knob-title">HPF</span><canvas id="lala-knob-hpf" class="ao-knob-canvas" width="60" height="60"></canvas><span class="ao-knob-readout" id="lala-val-hpf">${lalaState.hpf} Hz</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">MF (FREQ)</span><canvas id="lala-knob-mf" class="ao-knob-canvas" width="60" height="60"></canvas><span class="ao-knob-readout" id="lala-val-mf">${(lalaState.mf/1000).toFixed(1)} kHz</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">MG (GAIN)</span><canvas id="lala-knob-mg" class="ao-knob-canvas" width="60" height="60"></canvas><span class="ao-knob-readout" id="lala-val-mg">${lalaState.mg >= 0 ? '+' : ''}${lalaState.mg} dB</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">HF / FLAT</span><canvas id="lala-knob-hfFlat" class="ao-knob-canvas" width="60" height="60"></canvas><span class="ao-knob-readout" id="lala-val-hfFlat">${lalaState.hfFlat} %</span></div>
                            </div>
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:10px;">
                            <div class="ao-meter-housing" style="width:100%; max-width:320px;">
                                <div class="ao-vu-glass"><canvas id="lala-vu-meter" class="ao-vu-canvas" width="320" height="120"></canvas></div>
                            </div>
                            ${grBarHtml}
                            <div class="ao-switch-cluster">
                                <div class="ao-toggle-box">
                                    <span class="ao-toggle-label">MODE</span>
                                    <button class="ao-toggle-btn ${lalaState.limitMode ? 'active' : ''}" id="ao-lala-limit" style="width:88px;" onclick="window.toggleLalaModeInline('${target}', this)">${lalaState.limitMode ? 'LIMIT' : 'COMPRESS'}</button>
                                </div>
                            </div>
                        </div>
                        <div style="display:flex; flex-direction:column; gap:12px; align-items:center;">
                            <div style="display:flex; gap:18px; align-items:center;">
                                <div class="ao-knob-box"><span class="ao-knob-title">GAIN</span><canvas id="lala-knob-gain" class="ao-knob-canvas" width="90" height="90"></canvas><span class="ao-knob-readout" id="lala-val-gain">${lalaState.gain}</span></div>
                                <div class="ao-knob-box"><span class="ao-knob-title">PEAK REDUCTION</span><canvas id="lala-knob-peak" class="ao-knob-canvas" width="90" height="90"></canvas><span class="ao-knob-readout" id="lala-val-peak">${lalaState.peakReduction}</span></div>
                            </div>
                            <div class="ao-knob-box"><span class="ao-knob-title">DRY / WET MIX</span><canvas id="lala-knob-mix" class="ao-knob-canvas" width="62" height="62"></canvas><span class="ao-knob-readout" id="lala-val-mix">${lalaState.mix} %</span></div>
                        </div>
                    </div>
                </div>
            `;
            startAoVuLoop('lala-vu-meter', 'lala', false);
            bindAoRotary('lala-knob-gain', 'lala-val-gain', 0, 100, '', () => lalaState.gain, v => window.setCompParam(target, 'lala', 'gain', Math.round(v)), 'lala', true);
            bindAoRotary('lala-knob-peak', 'lala-val-peak', 0, 100, '', () => lalaState.peakReduction, v => window.setCompParam(target, 'lala', 'peakReduction', Math.round(v)), 'lala', true);
            bindAoRotary('lala-knob-mix', 'lala-val-mix', 0, 100, ' %', () => lalaState.mix, v => window.setCompParam(target, 'lala', 'mix', Math.round(v)), 'lala', false);
            bindAoRotary('lala-knob-hpf', 'lala-val-hpf', 20, 500, ' Hz', () => lalaState.hpf, v => window.setCompParam(target, 'lala', 'hpf', Math.round(v)), 'lala', false);
            bindAoRotary('lala-knob-mf', 'lala-val-mf', 500, 10000, ' Hz', () => lalaState.mf, v => window.setCompParam(target, 'lala', 'mf', Math.round(v)), 'lala', false);
            bindAoRotary('lala-knob-mg', 'lala-val-mg', -6, 6, ' dB', () => lalaState.mg, v => window.setCompParam(target, 'lala', 'mg', Math.round(v)), 'lala', false);
            bindAoRotary('lala-knob-hfFlat', 'lala-val-hfFlat', 0, 100, ' %', () => lalaState.hfFlat, v => window.setCompParam(target, 'lala', 'hfFlat', Math.round(v)), 'lala', false);
            if (window.updateDigitalGrBar && window.getTargetLiveGr) window.updateDigitalGrBar('comp-digital-gr-bar', window.getTargetLiveGr(target));
            return true;
        } else if (type === 'limiter') {
            modal.classList.add("ao-modal");
            const isMon = String(target).startsWith('mon-');
            title.innerHTML = `🛑 ANALOG OBSESSION BUSTER &bull; BUS &amp; PEAK LIMITER &bull; ${displayTargetName}`;
            body.innerHTML = modeBarHtml + `
                <div class="ao-chassis limiter-theme">
                    <div class="ao-screw top-left"></div><div class="ao-screw top-right"></div>
                    <div class="ao-screw bottom-left"></div><div class="ao-screw bottom-right"></div>
                    <div style="display:flex; justify-content:space-between; align-items:center;">
                        <div class="ao-brand-wrap">
                            <div>
                                <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                <div style="font-size:0.6rem; color:#94a3b8; font-weight:700; letter-spacing:1px;">MASTERING &amp; PA PROTECTION</div>
                            </div>
                            <div style="border-left:1px solid #334155; padding-left:12px;">
                                <div class="ao-model-title" style="color:#f59e0b;">BUSTER LIMITER</div>
                                <div class="ao-model-sub">${isMon ? 'ACOUSTIC STAGE WEDGE &amp; IEM PROTECTION' : 'ZERO-LATENCY FOH MASTER BUS LIMITER'}</div>
                            </div>
                        </div>
                        <div class="ao-toggle-box">
                            <span class="ao-toggle-label">POWER</span>
                            <button class="ao-toggle-btn power ${busterLimiterState.power ? '' : 'off'}" id="ao-buster-power" onclick="var s=window.getCompState('${target}').limiter; s.power=!s.power; this.classList.toggle('off', !s.power); this.innerText=s.power?'ON':'OFF'; window.setCompParam('${target}','limiter','power',s.power);">${busterLimiterState.power ? 'ON' : 'OFF'}</button>
                        </div>
                    </div>
                    <div class="ao-main-grid">
                        <div style="display:flex; flex-direction:column; gap:16px; align-items:center;">
                            <div class="ao-knob-box"><span class="ao-knob-title">THRESHOLD</span><canvas id="buster-knob-thresh" class="ao-knob-canvas" width="84" height="84"></canvas><span class="ao-knob-readout" id="buster-val-thresh" style="color:#f59e0b;">${busterLimiterState.threshold.toFixed(1)} dBFS</span></div>
                            <div class="ao-knob-box"><span class="ao-knob-title">DRIVE / PUNCH</span><canvas id="buster-knob-drive" class="ao-knob-canvas" width="64" height="64"></canvas><span class="ao-knob-readout" id="buster-val-drive" style="color:#f59e0b;">${busterLimiterState.drive.toFixed(1)} dB</span></div>
                        </div>
                        <div style="display:flex; flex-direction:column; align-items:center; gap:10px;">
                            <div class="ao-meter-housing" style="width:100%; max-width:320px;">
                                <div class="ao-vu-glass" style="background:radial-gradient(ellipse at 50% 120%, #fef3c7 0%, #fde68a 45%, #f59e0b 80%, #b45309 100%);">
                                    <canvas id="buster-vu-meter" class="ao-vu-canvas" width="320" height="120"></canvas>
                                </div>
                            </div>
                            ${grBarHtml}
                            <div class="ao-switch-cluster">
                                <div class="ao-toggle-box">
                                    <span class="ao-toggle-label">CRUSH MODE</span>
                                    <button class="ao-toggle-btn ${busterLimiterState.crush ? 'active' : ''}" id="ao-buster-crush" style="width:88px;" onclick="var s=window.getCompState('${target}').limiter; s.crush=!s.crush; this.classList.toggle('active', s.crush); this.innerText=s.crush?'CRUSH ON':'CLEAN'; window.setCompParam('${target}','limiter','crush',s.crush);">${busterLimiterState.crush ? 'CRUSH ON' : 'CLEAN'}</button>
                                </div>
                            </div>
                        </div>
                        <div style="display:flex; flex-direction:column; gap:16px; align-items:center;">
                            <div class="ao-knob-box"><span class="ao-knob-title">CEILING MARGIN</span><canvas id="buster-knob-ceiling" class="ao-knob-canvas" width="84" height="84"></canvas><span class="ao-knob-readout" id="buster-val-ceiling" style="color:#f59e0b;">${busterLimiterState.ceiling.toFixed(1)} dBFS</span></div>
                            <div class="ao-knob-box"><span class="ao-knob-title">RELEASE SPEED</span><canvas id="buster-knob-release" class="ao-knob-canvas" width="64" height="64"></canvas><span class="ao-knob-readout" id="buster-val-release" style="color:#f59e0b;">${busterLimiterState.release} ms</span></div>
                        </div>
                    </div>
                </div>
            `;
            startAoVuLoop('buster-vu-meter', 'lala', true);
            bindAoRotary('buster-knob-thresh', 'buster-val-thresh', -18, 0, ' dBFS', () => busterLimiterState.threshold, v => window.setCompParam(target, 'limiter', 'threshold', parseFloat(v.toFixed(1))), 'limiter', true);
            bindAoRotary('buster-knob-ceiling', 'buster-val-ceiling', -6, 0, ' dBFS', () => busterLimiterState.ceiling, v => window.setCompParam(target, 'limiter', 'ceiling', parseFloat(v.toFixed(1))), 'limiter', true);
            bindAoRotary('buster-knob-drive', 'buster-val-drive', 0, 12, ' dB', () => busterLimiterState.drive, v => window.setCompParam(target, 'limiter', 'drive', parseFloat(v.toFixed(1))), 'limiter', false);
            bindAoRotary('buster-knob-release', 'buster-val-release', 5, 150, ' ms', () => busterLimiterState.release, v => window.setCompParam(target, 'limiter', 'release', Math.round(v)), 'limiter', false);
            if (window.updateDigitalGrBar && window.getTargetLiveGr) window.updateDigitalGrBar('comp-digital-gr-bar', window.getTargetLiveGr(target));
            return true;
        } else if (type === 'summing') {
            modal.classList.add("ao-modal");
            title.innerHTML = `🎛️ ANALOG CONSOLE SUMMING &amp; TRANSFORMER EMULATION &bull; MASTER BUS`;
            body.innerHTML = `
                <div class="ao-chassis summing-theme">
                    <div style="display:flex; justify-content:space-between; align-items:center;">
                        <div class="ao-brand-wrap">
                            <div>
                                <div class="ao-brand-logo">ANALOG OBSESSION</div>
                                <div style="font-size:0.6rem; color:#94a3b8; font-weight:700; letter-spacing:1px;">CONSOLE TOPOLOGY &amp; IRON CORE</div>
                            </div>
                            <div style="border-left:1px solid #3f3f46; padding-left:12px;">
                                <div class="ao-model-title" style="color:#d97706;">CONSOLE SUMMING &amp; TRANSFORMER</div>
                                <div class="ao-model-sub">NEVE 8048 &bull; API 2520 &bull; SSL 4000E BUS SATURATION</div>
                            </div>
                        </div>
                        <div class="ao-toggle-box">
                            <span class="ao-toggle-label">SUMMING ACTIVE</span>
                            <button class="ao-toggle-btn power ${analogSummingState.enabled ? '' : 'off'}" id="ao-summing-power" onclick="analogSummingState.enabled=!analogSummingState.enabled; this.classList.toggle('off', !analogSummingState.enabled); this.innerText=analogSummingState.enabled?'ACTIVE':'BYPASS'; if(window.dispatchDspParam)window.dispatchDspParam(0,'summingEnabled',analogSummingState.enabled); updateSummingHeaderBtn();">${analogSummingState.enabled ? 'ACTIVE' : 'BYPASS'}</button>
                        </div>
                    </div>
                    <div class="summing-deck">
                        <div class="summing-card">
                            <span class="summing-card-title">CONSOLE TOPOLOGY</span>
                            <select class="summing-selector" onchange="setSummingConsole(this.value)">
                                <option value="neve" ${analogSummingState.console === 'neve' ? 'selected' : ''}>British Classic 8048 (Fat Iron)</option>
                                <option value="api" ${analogSummingState.console === 'api' ? 'selected' : ''}>American Punch 2520 (Aggressive)</option>
                                <option value="ssl" ${analogSummingState.console === 'ssl' ? 'selected' : ''}>Oxford 4000E (Clean &amp; Tight)</option>
                                <option value="tube" ${analogSummingState.console === 'tube' ? 'selected' : ''}>Germanium Tube Desk (Warm Bloom)</option>
                            </select>
                        </div>
                        <div class="summing-card"><span class="summing-card-title">DRIVE &amp; HARMONICS</span><canvas id="summing-knob-drive" class="ao-knob-canvas" width="68" height="68"></canvas><span class="ao-knob-readout" id="summing-val-drive" style="color:#38bdf8;">${analogSummingState.drive.toFixed(1)} dB</span></div>
                        <div class="summing-card"><span class="summing-card-title">LOW-END BLOOM</span><canvas id="summing-knob-warmth" class="ao-knob-canvas" width="68" height="68"></canvas><span class="ao-knob-readout" id="summing-val-warmth" style="color:#38bdf8;">${analogSummingState.warmth.toFixed(1)} dB</span></div>
                        <div class="summing-card"><span class="summing-card-title">CROSSTALK BLEED</span><canvas id="summing-knob-crosstalk" class="ao-knob-canvas" width="68" height="68"></canvas><span class="ao-knob-readout" id="summing-val-crosstalk" style="color:#38bdf8;">${analogSummingState.crosstalk} dB</span></div>
                    </div>
                </div>
            `;
            bindAoRotary('summing-knob-drive', 'summing-val-drive', 0, 12, ' dB', () => analogSummingState.drive, v => { analogSummingState.drive = parseFloat(v.toFixed(1)); if (window.dispatchDspParam) window.dispatchDspParam(0, 'summingDrive', analogSummingState.drive); }, 'summing', false);
            bindAoRotary('summing-knob-warmth', 'summing-val-warmth', 0, 10, ' dB', () => analogSummingState.warmth, v => { analogSummingState.warmth = parseFloat(v.toFixed(1)); if (window.dispatchDspParam) window.dispatchDspParam(0, 'summingWarmth', analogSummingState.warmth); }, 'summing', false);
            bindAoRotary('summing-knob-crosstalk', 'summing-val-crosstalk', -90, -40, ' dB', () => analogSummingState.crosstalk, v => { analogSummingState.crosstalk = Math.round(v); if (window.dispatchDspParam) window.dispatchDspParam(0, 'summingCrosstalk', analogSummingState.crosstalk); }, 'summing', false);
            return true;
        }
        return false;
    };
})(window);
