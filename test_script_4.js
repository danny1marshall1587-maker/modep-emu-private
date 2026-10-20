
            const sendNames = ["Aux 1: TAL-Reverb-4 (Stereo)", "Aux 2: Stereo IR Convolver", "Aux 3: Ping-Pong Delay", "Aux 4: Tape / Slapback Delay"];
            for (let s = 1; s <= 4; s++) {
                const isDelay = (s === 3 || s === 4);
                const isReverb = (s === 1 || s === 2);
                document.write(`
                <div class="channel-strip aux-send send-strip" id="send-strip-${s}" style="display:none;">
                    <div class="ch-header">
                        <div class="ch-title-badge" id="send-badge-${s}" onclick="openColorPalette('send-${s}', event)" style="background:#8b5cf6;" title="Tap to change color tag">SEND ${s}</div>
                        <div class="ch-sub-name" id="send-name-${s}" onclick="openRenameModal('send-${s}', 'SEND ${s}')" title="Tap or hold to rename">${sendNames[s-1]}</div>
                    </div>
                    <!-- MODEP Board on the Send Channel as an Insert -->
                    <div class="btn-super-insert" onclick="openSuperInsert('send-${s}')" style="color:#c084fc; border-color:#9333ea;" title="Open Send ${s} Insert MODEP Board">
                        <span>🎛️ MODEP SEND ${s} INSERT</span>
                    </div>
                    <!-- Permanent Reverb/Delay DSP processor on Send -->
                    <div class="dsp-badges">
                        <div class="dsp-pill active" onclick="openDspModal(${isDelay ? "'delay'" : "'reverb'"}, 'send-${s}')" style="grid-column: span 2; background:${isDelay ? '#064e3b' : '#312e81'}; border-color:${isDelay ? '#10b981' : '#818cf8'}; color:#fff;">
                            ${s === 1 ? '🏛️ TAL-REVERB-4 (STEREO)' : (s === 2 ? '🌌 IR CONVOLVER (STEREO)' : (s === 3 ? '⏱️ PING-PONG DELAY' : '📼 TAPE SLAPBACK DELAY'))}
                        </div>
                        <div class="dsp-pill active reeq-badge" onclick="openDspModal('reeq', 'send-${s}')">ReEQ SEND</div>
                        <div class="dsp-pill active" onclick="openDspModal('comp', 'send-${s}')">DUCK COMP</div>
                    </div>
                    <div class="fader-section">
                        <div class="meter-container">
                            <div class="meter-bar" id="meter-send-${s}"></div>
                        </div>
                        <div class="fader-track" id="fader-track-send-${s}" onpointerdown="startFaderDrag('send-${s}', event)">
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
                            <div class="fader-cap" id="fader-cap-send-${s}" style="bottom: 75%;"></div>
                            <div class="fader-db-display" id="fader-val-send-${s}">0.0 dB</div>
                        </div>
                    </div>
                    <div class="btn-group">
                        <button class="btn-strip btn-solo" id="solo-send-${s}" onclick="this.classList.toggle('active')">SOLO</button>
                        <button class="btn-strip btn-mute" id="mute-send-${s}" onclick="this.classList.toggle('active')">MUTE</button>
                    </div>
                </div>
                `);
            }
        