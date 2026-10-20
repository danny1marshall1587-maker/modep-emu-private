
            const monNames = ["Stage Wedge L", "Stage Wedge R", "In-Ear Mon 1 (IEM)", "In-Ear Mon 2 (IEM)", "Drum Fill Sub", "Side Fill PA"];
            for (let m = 1; m <= 6; m++) {
                document.write(`
                <div class="channel-strip monitor-out monitor-strip" id="monitor-strip-${m}" style="display:none;">
                    <div class="ch-header">
                        <div class="ch-title-badge" id="mon-badge-${m}" onclick="openColorPalette('mon-${m}', event)" style="background:#06b6d4;" title="Tap to change color tag">MON ${m}</div>
                        <div class="ch-sub-name" id="mon-name-${m}" onclick="openRenameModal('mon-${m}', 'MONITOR ${m}')" title="Tap or hold to rename">${monNames[m-1]}</div>
                    </div>
                    <div class="btn-super-insert" onclick="openSuperInsert('mon-${m}')" style="color:#22d3ee; border-color:#0891b2;">
                        <span>🎛️ MON ${m} DSP</span>
                    </div>
                    <div class="dsp-badges">
                        <div class="dsp-pill active reeq-badge" onclick="openDspModal('reeq', 'mon-${m}')" style="grid-column: span 2;">ReEQ ROOM EQ</div>
                        <div class="dsp-pill active" onclick="openDspModal('feedback', 'mon-${m}')">FB-KILL PRO</div>
                        <div class="dsp-pill active" onclick="openDspModal('limiter', 'mon-${m}')" title="Safety Limiter: Hard Ceiling -1.0 dBFS">🛡️ LIMITER (-1dBFS)</div>
                    </div>
                    <div class="fader-section">
                        <div class="meter-container">
                            <div class="meter-bar" id="meter-mon-${m}"></div>
                        </div>
                        <div class="fader-track" id="fader-track-mon-${m}" onpointerdown="startFaderDrag('mon-${m}', event)">
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
                            <div class="fader-cap" id="fader-cap-mon-${m}" style="bottom: 75%;"></div>
                            <div class="fader-db-display" id="fader-val-mon-${m}">0.0 dB</div>
                        </div>
                    </div>
                    <div class="btn-group">
                        <button class="btn-strip" style="background:#0891b2" onclick="toggleSofMode('mon${m}', 'MONITOR ${m} (${monNames[m-1]})')">SENDS ON FADER</button>
                        <button class="btn-strip btn-mute" id="mute-mon-${m}" onclick="this.classList.toggle('active')">MUTE MON</button>
                    </div>
                </div>
                `);
            }
        