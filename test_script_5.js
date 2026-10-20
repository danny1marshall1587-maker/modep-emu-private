
            const retNames = ["Reverb Return 1", "Reverb Return 2", "Delay Return 3", "Delay Return 4"];
            for (let r = 1; r <= 4; r++) {
                document.write(`
                <div class="channel-strip fx-return return-strip" id="return-strip-${r}" style="display:none;">
                    <div class="ch-header">
                        <div class="ch-title-badge" id="ret-badge-${r}" onclick="openColorPalette('ret-${r}', event)" style="background:#ec4899;" title="Tap to change color tag">RET ${r}</div>
                        <div class="ch-sub-name" id="ret-name-${r}" onclick="openRenameModal('ret-${r}', 'RET ${r}')" title="Tap or hold to rename">${retNames[r-1]}</div>
                    </div>
                    <div class="dsp-badges" style="margin-top:4px;">
                        <div class="dsp-pill active reeq-badge" onclick="openDspModal('reeq', 'ret-${r}')" style="grid-column: span 2;">ReEQ RET EQ</div>
                        <div class="dsp-pill active" onclick="openDspModal('comp', 'ret-${r}')">GLUE COMP</div>
                        <div class="dsp-pill active" onclick="openDspModal('feedback', 'ret-${r}')">FB-KILL</div>
                    </div>
                    <div class="fader-section">
                        <div class="meter-container" style="margin-right:2px;">
                            <div class="meter-bar" id="meter-ret-${r}-l"></div>
                        </div>
                        <div class="meter-container">
                            <div class="meter-bar" id="meter-ret-${r}-r"></div>
                        </div>
                        <div class="fader-track" id="fader-track-ret-${r}" onpointerdown="startFaderDrag('ret-${r}', event)">
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
                            <div class="fader-cap" id="fader-cap-ret-${r}" style="bottom: 75%;"></div>
                            <div class="fader-db-display" id="fader-val-ret-${r}">0.0 dB</div>
                        </div>
                    </div>
                    <div class="btn-group">
                        <button class="btn-strip btn-solo" id="solo-ret-${r}" onclick="this.classList.toggle('active')">SOLO</button>
                        <button class="btn-strip btn-mute" id="mute-ret-${r}" onclick="this.classList.toggle('active')">MUTE</button>
                    </div>
                </div>
                `);
            }
        