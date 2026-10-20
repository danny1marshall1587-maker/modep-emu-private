
            const channelNames = ["Vocal 1", "Guitar", "Bass", "Kick Drum", "Snare", "Hi-Hat / O/H", "Keys L", "Keys R"];
            for (let i = 0; i < 8; i++) {
                const pairIndex = Math.floor(i / 2) + 1;
                document.write(`
                <div class="channel-strip input-strip" id="ch-${i}">
                    <div class="ch-header">
                        <div class="ch-title-badge" id="ch-badge-${i}" onclick="openColorPalette('ch-${i}', event)" title="Tap to change channel color tag">IN ${i+1} &bull; CH ${i+1}</div>
                        <div class="ch-sub-name" id="ch-name-${i}" onclick="openRenameModal('ch-${i}', 'CH ${i+1}')" title="Tap or hold to rename channel">${channelNames[i]}</div>
                    </div>

                    <!-- Per-Channel Hardware Input Soft-Patch -->
                    <div class="soft-patch-row">
                        <span>IN:</span>
                        <select class="soft-patch-select" id="ch-input-source-${i}"
                            onchange="handleSoftPatchChange(${i}, parseInt(this.value))"
                            title="Route hardware capture port to this channel">
                            ${Array.from({length:18}, (_,n) => `<option value="${n+1}" ${n===i?'selected':''}>${n<9?'HW '+(n+1):'HW '+(n+1)}</option>`).join('')}
                        </select>
                    </div>

                    <!-- Clean Single FX Rack Button — opens full-page DSP rack -->
                    <div class="btn-fx-quick-open" onclick="showDspRackPage(${i})" title="Open Channel ${i+1} DSP Rack — Full Page (Input Gain → ReaEQ → Comp [FET/LALA] → LOADES De-Esser → MODEP Insert Loop)">
                        <div class="fx-quick-title">
                            <span>🎛️ CHANNEL DSP RACK</span>
                        </div>
                        <div class="fx-quick-flow">
                            <span class="fx-flow-dot"></span><span>GAIN</span>
                            <span>&bull;</span><span>DENOISE</span>
                            <span>&bull;</span><span>ReaEQ</span>
                            <span>&bull;</span><span>COMP</span>
                            <span>&bull;</span><span>DE-ESS</span>
                            <span>&bull;</span><span>MODEP</span>
                        </div>
                    </div>

                    <!-- Dedicated MODEP Board Insert per Stereo Pair with Isolated Stereo Canvas -->
                    <div class="btn-super-insert modep-insert-btn" onclick="openModepInsert(${i+1})" title="Isolated Stereo MODEP Pedalboard for Channel ${i+1} (Port ${getModepPortForChannel(i+1)})">
                        <span>🎸 MODEP INSERT ${pairIndex}</span>
                    </div>

                    <div class="fader-section">
                        <div class="meter-container" title="Channel Peak Level (-18dB Yellow, -6dB Orange, 0dB Red Clipping)">
                            <div class="meter-bar" id="meter-${i}"></div>
                            <div class="meter-ticks">
                                <div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div><div class="meter-tick"></div>
                            </div>
                        </div>
                        <!-- Multi-Color Channel Gain Reduction (GR) Meter -->
                        <div class="gr-meter-container" title="Gain Reduction: 0-6dB Green (Acoustic), 6-12dB Orange (Vocal Leveler), >12dB Red (Heavy)">
                            <div class="gr-meter-bar" id="gr-meter-${i}"></div>
                            <span class="gr-meter-scale-title">GR</span>
                        </div>
                        <div class="fader-track" id="fader-track-${i}" onpointerdown="startFaderDrag(${i}, event)">
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
                            <div class="fader-cap" id="fader-cap-${i}" style="bottom: 75%;"></div>
                            <div class="fader-db-display" id="fader-val-${i}">0.0 dB</div>
                        </div>
                    </div>
                    <div class="btn-group">
                        <button class="btn-strip btn-amm active" id="amm-ch-${i}" onclick="toggleChAmm(${i})">AMM DUGAN</button>
                        <button class="btn-strip btn-solo" id="solo-ch-${i}" onclick="toggleChSolo(${i})">SOLO</button>
                        <button class="btn-strip btn-mute" id="mute-ch-${i}" onclick="toggleChMute(${i})">MUTE</button>
                    </div>
                </div>
                `);
                // Insert LINK button between each stereo pair (after ch indices 1,3,5,7)
                if (i % 2 === 1) {
                    const pairNum = Math.floor(i / 2);
                    document.write(`
                    <div class="stereo-link-col" id="stereo-link-col-${pairNum}">
                        <button class="btn-stereo-link" id="btn-stereo-link-${pairNum}"
                            onclick="toggleStereoLink(${pairNum})"
                            title="Link CH ${i+1}+CH ${i+2} as stereo pair — hard pan L/R, mirror fader/mute/solo/DSP">
                            [ LINK ]
                        </button>
                    </div>
                    `);
                }
            }
        