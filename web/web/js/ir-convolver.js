// ir-convolver.js - Authentic Bricasti M7 True-Stereo Partitioned IR Convolver Engine
(function(window) {
    'use strict';
    window.renderIrConvolverControls = function() {
        const reverbDspState = window.reverbDspState;
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
                            <select onchange="reverbDspState.atlantis.irModel=this.value; drawIrWaveform(); if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();" style="background:#0b0d11; color:#f1f5f9; border:1px solid #374151; border-radius:4px; padding:4px 8px; font-size:0.75rem; font-weight:700; max-width:280px;">
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
                            <input type="range" class="knob-slider" min="0" max="500" step="1" value="${a.preDelay}" oninput="reverbDspState.atlantis.preDelay=parseInt(this.value); this.nextElementSibling.innerText=this.value+' ms'; drawIrWaveform(); if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">
                            <span class="ir-card-val">${a.preDelay} ms</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">ROOM SIZE</span>
                            <input type="range" class="knob-slider" min="10" max="150" step="1" value="${a.roomSize}" oninput="reverbDspState.atlantis.roomSize=parseInt(this.value); this.nextElementSibling.innerText=this.value+' %'; drawIrWaveform(); if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">
                            <span class="ir-card-val">${a.roomSize} %</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">DAMPING</span>
                            <input type="range" class="knob-slider" min="0" max="100" step="1" value="${a.damping}" oninput="reverbDspState.atlantis.damping=parseInt(this.value); this.nextElementSibling.innerText=this.value+' %'; drawIrWaveform(); if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">
                            <span class="ir-card-val">${a.damping} %</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">WET LEVEL</span>
                            <input type="range" class="knob-slider" min="-40" max="0" step="0.5" value="${a.wet}" oninput="reverbDspState.atlantis.wet=parseFloat(this.value); this.nextElementSibling.innerText=this.value+' dB'; if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">
                            <span class="ir-card-val">${a.wet} dB</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">PRE-EQ LOW CUT</span>
                            <input type="range" class="knob-slider" min="20" max="500" step="5" value="${a.preEqLow}" oninput="reverbDspState.atlantis.preEqLow=parseInt(this.value); this.nextElementSibling.innerText=this.value+' Hz'; if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">
                            <span class="ir-card-val">${a.preEqLow} Hz</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">POST-EQ HIGH CUT</span>
                            <input type="range" class="knob-slider" min="1000" max="18000" step="100" value="${a.postEqHigh}" oninput="reverbDspState.atlantis.postEqHigh=parseInt(this.value); this.nextElementSibling.innerText=(this.value/1000).toFixed(1)+' kHz'; if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">
                            <span class="ir-card-val">${(a.postEqHigh/1000).toFixed(1)} kHz</span>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">NORMALIZE (NORM)</span>
                            <button class="btn-top ${a.norm ? 'active' : ''}" style="width:100%; margin-top:4px;" onclick="reverbDspState.atlantis.norm=!reverbDspState.atlantis.norm; this.classList.toggle('active'); this.innerText=reverbDspState.atlantis.norm?'NORM ON':'NORM OFF'; drawIrWaveform(); if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">${a.norm ? 'NORM ON' : 'NORM OFF'}</button>
                        </div>
                        <div class="ir-card">
                            <span class="ir-card-label">REVERSE IR (REV)</span>
                            <button class="btn-top ${a.rev ? 'active' : ''}" style="width:100%; margin-top:4px;" onclick="reverbDspState.atlantis.rev=!reverbDspState.atlantis.rev; this.classList.toggle('active'); this.innerText=reverbDspState.atlantis.rev?'REV ON':'REV OFF'; drawIrWaveform(); if(window.syncAuxReverbDsp)window.syncAuxReverbDsp();">${a.rev ? 'REV ON' : 'REV OFF'}</button>
                        </div>
                    </div>
                </div>
                `;
    };
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
            const a = window.reverbDspState.atlantis;
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
    window.initIrCanvas = initIrCanvas;
    window.drawIrWaveform = drawIrWaveform;
})(window);
