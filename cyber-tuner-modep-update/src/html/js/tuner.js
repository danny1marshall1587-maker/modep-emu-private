// SPDX-FileCopyrightText: 2026 Cyber Audio
// SPDX-License-Identifier: MIT
// Cyber Strobe & Peak Tuner System for MOD-UI & MODEP

(function (window, $) {
    'use strict';

    var PALETTES = {
        green:  { hex: '#00ff66', glow: 'rgba(0, 255, 102, 0.65)',  dim: 'rgba(0, 255, 102, 0.20)' },
        azure:  { hex: '#00b0ff', glow: 'rgba(0, 176, 255, 0.65)',  dim: 'rgba(0, 176, 255, 0.20)' },
        amber:  { hex: '#ffaa00', glow: 'rgba(255, 170, 0, 0.65)',  dim: 'rgba(255, 170, 0, 0.20)' },
        gold:   { hex: '#ffd700', glow: 'rgba(255, 215, 0, 0.65)',  dim: 'rgba(255, 215, 0, 0.20)' },
        violet: { hex: '#bb44ff', glow: 'rgba(187, 68, 255, 0.65)', dim: 'rgba(187, 68, 255, 0.20)' },
        white:  { hex: '#ffffff', glow: 'rgba(255, 255, 255, 0.65)', dim: 'rgba(255, 255, 255, 0.20)' },
        red:    { hex: '#ff3333', glow: 'rgba(255, 51, 51, 0.65)',  dim: 'rgba(255, 51, 51, 0.20)' }
    };

    var SWEETENED_TUNINGS = {
        standard: { name: 'Standard (12-TET)', offsets: {} },
        peterson_gtr: {
            name: 'Peterson GTR Sweetened',
            offsets: { 'E2': -2.3, 'A2': -2.1, 'D3': -2.1, 'G3': -1.7, 'B3': -1.0, 'E4': 0.0, 'E': -1.2, 'A': -2.1, 'D': -2.1, 'G': -1.7, 'B': -1.0 }
        },
        james_taylor: {
            name: 'James Taylor Acoustic',
            offsets: { 'E2': -12.0, 'A2': -10.0, 'D3': -8.0, 'G3': -4.0, 'B3': -3.0, 'E4': -1.0, 'E': -6.5, 'A': -10.0, 'D': -8.0, 'G': -4.0, 'B': -3.0 }
        },
        dadgad: {
            name: 'DADGAD Celtic Sweetened',
            offsets: { 'D2': -2.0, 'A2': -2.0, 'D3': -1.5, 'G3': -1.5, 'A3': -1.0, 'D4': 0.0, 'D': -1.2, 'A': -1.5, 'G': -1.5 }
        },
        drop_d: {
            name: 'Drop D Compensation',
            offsets: { 'D2': -2.5, 'A2': -2.0, 'D3': -2.0, 'G3': -1.5, 'B3': -1.0, 'E4': 0.0, 'D': -2.2, 'A': -2.0, 'G': -1.5, 'B': -1.0, 'E': 0.0 }
        },
        half_step: {
            name: 'Half Step Down (Eb)',
            offsets: { 'D#': -2.0, 'G#': -2.0, 'C#': -2.0, 'F#': -1.5, 'A#': -1.0, 'Eb': -2.0, 'Ab': -2.0, 'Db': -2.0, 'Gb': -1.5, 'Bb': -1.0 }
        },
        bass: {
            name: '4/5-String Bass Optimised',
            offsets: { 'B0': -1.0, 'E1': -1.0, 'A1': -0.5, 'D2': 0.0, 'G2': 0.0, 'C3': 0.0, 'E': -1.0, 'A': -0.5, 'D': 0.0, 'G': 0.0 }
        },
        bfts: {
            name: 'Buzz Feiten System (BFTS)',
            offsets: { 'E': -1.0, 'A': 0.0, 'D': 0.0, 'G': 1.0, 'B': -1.0 }
        }
    };

    var NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

    function freqToNoteCents(freq, refFreq) {
        if (!freq || freq < 15.0) return { note: '--', accidental: '', octave: '', cents: 0.0, in_tune: false };
        var midi = 69 + 12 * Math.log2(freq / refFreq);
        var roundedMidi = Math.round(midi);
        var cents = (midi - roundedMidi) * 100.0;
        var noteIdx = (roundedMidi % 12 + 12) % 12;
        var octave = Math.floor(roundedMidi / 12) - 1;
        var name = NOTE_NAMES[noteIdx];
        var base = name[0];
        var acc = name.length > 1 ? name[1] : '';
        return {
            note: base,
            accidental: acc,
            octave: octave.toString(),
            cents: cents,
            in_tune: Math.abs(cents) < 1.5
        };
    }

    function CyberTuner(options) {
        var self = this;

        this.options = $.extend({
            topButton: $('#mod-tuner-top-btn'),
            windowModal: $('#mod-tuner-window')
        }, options);

        this.isOpen = false;
        this.viewMode = 'strobe'; // 'strobe' or 'peak'
        this.currentTheme = 'green';
        this.refFreq = 440;
        this.sweeteningKey = 'standard';
        this.isMuted = true;
        this.inputGainDb = 18; // Default +18dB massive headroom on tap
        this.selectedChannel = 'channel_1'; // Default: Channel 1 (Input 1 / EVO 4 Left)
        this.selectedDeviceId = '';
        this.discoveredDevices = [];

        // MIDI Assignment & Trigger Mode
        this.midiMap = null; // e.g. { type: 'cc', number: 64, channel: 1, label: 'CC 64 (Ch 1)' }
        this.midiMode = 'momentary'; // 'momentary' (toggle on press), 'hold' (press to open, release to close), 'latching'
        this.isLearningMidi = false;

        // DSP / Pitch Tracking State
        this.detectedFreq = 0.0;
        this.detectedNote = '--';
        this.detectedAccidental = '';
        this.detectedOctave = '';
        this.rawCents = 0.0;
        this.smoothCents = 0.0;
        this.hasSignal = false;
        this.lastSignalTime = 0;
        this.currentRms = 0.0;

        // Strobe Animation Angles
        this.outerAngle = 0.0;
        this.innerAngle = 0.0;
        this.animId = null;

        // Web Audio graph nodes
        this.audioCtx = null;
        this.analyser = null;
        this.gainNode = null;
        this.lpFilter = null;
        this.hpFilter = null;
        this.audioStream = null;
        this.audioBuffer = null;

        this.init();
    }

    CyberTuner.prototype.init = function () {
        var self = this;
        this.loadPreferences();

        // Create Backdrop Overlay if missing
        if (!$('#mod-tuner-backdrop').length) {
            $('body').append('<div id="mod-tuner-backdrop"></div>');
        }
        this.backdrop = $('#mod-tuner-backdrop');

        // Build HTML into #mod-tuner-window immediately
        this.buildModalHTML();
        this.applyTheme(this.currentTheme);
        this.bindEvents();
        this.enumerateAudioDevices();
        this.initMidi();
        this.initWsPitchListener();

        // Listen for device change events (e.g. plugging in USB interface)
        if (navigator.mediaDevices && 'ondevicechange' in navigator.mediaDevices) {
            navigator.mediaDevices.ondevicechange = function () {
                self.enumerateAudioDevices();
            };
        }

        // Ensure window is completely hidden initially
        this.options.windowModal.hide().removeClass('mod-hidden');
        this.backdrop.hide();

        // Render first stationary frame so canvas is ready
        this.renderCanvas();
    };

    CyberTuner.prototype.loadPreferences = function () {
        try {
            var saved = localStorage.getItem('cyber_tuner_prefs');
            if (saved) {
                var p = JSON.parse(saved);
                if (p.viewMode) this.viewMode = p.viewMode;
                if (p.theme && PALETTES[p.theme]) this.currentTheme = p.theme;
                if (p.refFreq) this.refFreq = parseInt(p.refFreq, 10);
                if (p.sweeteningKey && SWEETENED_TUNINGS[p.sweeteningKey]) this.sweeteningKey = p.sweeteningKey;
                if (typeof p.isMuted !== 'undefined') this.isMuted = !!p.isMuted;
                if (typeof p.inputGainDb !== 'undefined') this.inputGainDb = parseInt(p.inputGainDb, 10);
                if (p.selectedChannel) {
                    this.selectedChannel = p.selectedChannel;
                } else {
                    this.selectedChannel = 'channel_1';
                }
                if (p.selectedDeviceId) this.selectedDeviceId = p.selectedDeviceId;
                if (p.midiMap) this.midiMap = p.midiMap;
                if (p.midiMode) this.midiMode = p.midiMode;
            }
        } catch (e) {
            console.error('Failed to load tuner prefs:', e);
        }
    };

    CyberTuner.prototype.savePreferences = function () {
        try {
            var data = {
                viewMode: this.viewMode,
                theme: this.currentTheme,
                refFreq: this.refFreq,
                sweeteningKey: this.sweeteningKey,
                isMuted: this.isMuted,
                inputGainDb: this.inputGainDb,
                selectedChannel: this.selectedChannel,
                selectedDeviceId: this.selectedDeviceId,
                midiMap: this.midiMap,
                midiMode: this.midiMode
            };
            localStorage.setItem('cyber_tuner_prefs', JSON.stringify(data));
        } catch (e) {
            console.error('Failed to save tuner prefs:', e);
        }
    };

    CyberTuner.prototype.buildDeviceSelectOptions = function () {
        var self = this;
        var html = [];

        var channels = [
            { value: 'channel_1', label: 'Input 1 (EVO 4 Ch 1 / Left)' },
            { value: 'channel_2', label: 'Input 2 (EVO 4 Ch 2 / Right)' },
            { value: 'stereo',    label: 'Input 1+2 (Stereo In)' }
        ];

        html.push('<optgroup label="Audio Channels">');
        channels.forEach(function (ch) {
            var isSel = (!self.selectedDeviceId && self.selectedChannel === ch.value) ||
                        (!self.selectedDeviceId && !self.selectedChannel && ch.value === 'channel_1');
            var sel = isSel ? ' selected' : '';
            html.push('    <option value="' + ch.value + '"' + sel + '>' + ch.label + '</option>');
        });
        html.push('</optgroup>');

        if (this.discoveredDevices && this.discoveredDevices.length > 0) {
            html.push('<optgroup label="Audio Input Devices">');
            this.discoveredDevices.forEach(function (d, i) {
                var label = d.label || ('Hardware In ' + (i + 1));
                var sel = (self.selectedDeviceId === d.deviceId) ? ' selected' : '';
                html.push('    <option value="dev:' + d.deviceId + '"' + sel + '>' + label + '</option>');
            });
            html.push('</optgroup>');
        }

        return html.join('\n');
    };

    CyberTuner.prototype.buildModalHTML = function () {
        var sweetOpts = '';
        for (var k in SWEETENED_TUNINGS) {
            var sel = (k === this.sweeteningKey) ? ' selected' : '';
            sweetOpts += '<option value="' + k + '"' + sel + '>' + SWEETENED_TUNINGS[k].name + '</option>';
        }

        var midiLabel = this.midiMap ? this.midiMap.label : 'MIDI ASSIGN';
        var midiClass = this.midiMap ? 'mapped' : '';
        var deviceOpts = this.buildDeviceSelectOptions();

        var html = [
            '<div class="cyber-tuner-header">',
            '    <div class="cyber-tuner-title">',
            '        <span class="cyber-tuner-brand">CYBER AUDIO</span>',
            '        <span class="cyber-tuner-name">Studio Strobe &amp; Peak Tuner</span>',
            '    </div>',
            '    <div class="cyber-header-controls">',
            '        <button class="cyber-midi-learn-btn ' + midiClass + '" id="cyber-tuner-midi-learn" title="Click to assign MIDI CC or footswitch (Right-click to clear)">' + midiLabel + '</button>',
            '        <button class="cyber-midi-mode-btn" id="cyber-tuner-midi-mode" title="Click to cycle MIDI Mode (MOMENTARY / HOLD / LATCHING)">MODE: <span class="cyber-midi-mode-text" id="cyber-midi-mode-val">' + this.midiMode.toUpperCase() + '</span></button>',
            '        <select class="cyber-device-select" id="cyber-device-select" title="Select Audio Input Channel / Device">' + deviceOpts + '</select>',
            '        <div class="cyber-tuner-close js-cyber-close" title="Close Tuner (ESC)">&#10005;</div>',
            '    </div>',
            '</div>',
            '<div class="cyber-tuner-body">',
            '    <div class="cyber-note-container">',
            '        <span class="cyber-note-name" id="cyber-tuner-note">--</span>',
            '        <span class="cyber-note-accidental" id="cyber-tuner-acc"></span>',
            '        <span class="cyber-note-octave" id="cyber-tuner-oct"></span>',
            '    </div>',
            '    <div class="cyber-stats-row">',
            '        <span class="cyber-cents-badge" id="cyber-tuner-cents">0.0 Cents</span>',
            '        <span class="cyber-cents-badge" id="cyber-tuner-freq">0.0 Hz</span>',
            '    </div>',
            '    <div class="cyber-signal-meter-container">',
            '        <span class="cyber-signal-label">SIG</span>',
            '        <div class="cyber-signal-bar-track">',
            '            <div class="cyber-signal-bar-fill" id="cyber-signal-fill"></div>',
            '        </div>',
            '        <span class="cyber-signal-db" id="cyber-signal-db">-inf dB</span>',
            '    </div>',
            '    <div class="cyber-canvas-wrapper">',
            '        <canvas id="cyber-tuner-canvas" width="480" height="230"></canvas>',
            '    </div>',
            '</div>',
            '<div class="cyber-tuner-footer">',
            '    <div class="cyber-control-group">',
            '        <span class="cyber-control-label">View:</span>',
            '        <div class="cyber-view-switcher">',
            '            <button class="cyber-view-btn ' + (this.viewMode === 'strobe' ? 'active' : '') + '" data-mode="strobe">STROBE</button>',
            '            <button class="cyber-view-btn ' + (this.viewMode === 'peak' ? 'active' : '') + '" data-mode="peak">PEAK</button>',
            '        </div>',
            '    </div>',
            '    <div class="cyber-control-group">',
            '        <span class="cyber-control-label">Gain:</span>',
            '        <div class="cyber-stepper">',
            '            <button class="cyber-step-btn" id="cyber-gain-down">-</button>',
            '            <span class="cyber-stepper-val" id="cyber-gain-val">+' + this.inputGainDb + ' dB</span>',
            '            <button class="cyber-step-btn" id="cyber-gain-up">+</button>',
            '        </div>',
            '    </div>',
            '    <div class="cyber-control-group">',
            '        <span class="cyber-control-label">Theme:</span>',
            '        <div class="cyber-color-palette">',
            '            <div class="cyber-color-dot ' + (this.currentTheme === 'green' ? 'active' : '') + '" data-theme="green" style="background:#00ff66;" title="Cyber Green"></div>',
            '            <div class="cyber-color-dot ' + (this.currentTheme === 'azure' ? 'active' : '') + '" data-theme="azure" style="background:#00b0ff;" title="Cobalt Azure"></div>',
            '            <div class="cyber-color-dot ' + (this.currentTheme === 'amber' ? 'active' : '') + '" data-theme="amber" style="background:#ffaa00;" title="Electric Amber"></div>',
            '            <div class="cyber-color-dot ' + (this.currentTheme === 'gold' ? 'active' : '') + '" data-theme="gold" style="background:#ffd700;" title="Centaur Gold"></div>',
            '            <div class="cyber-color-dot ' + (this.currentTheme === 'violet' ? 'active' : '') + '" data-theme="violet" style="background:#bb44ff;" title="Electric Violet"></div>',
            '            <div class="cyber-color-dot ' + (this.currentTheme === 'white' ? 'active' : '') + '" data-theme="white" style="background:#ffffff;" title="Studio Ice White"></div>',
            '            <div class="cyber-color-dot ' + (this.currentTheme === 'red' ? 'active' : '') + '" data-theme="red" style="background:#ff3333;" title="Crimson Stage"></div>',
            '        </div>',
            '    </div>',
            '    <div class="cyber-control-group">',
            '        <span class="cyber-control-label">Ref:</span>',
            '        <div class="cyber-stepper">',
            '            <button class="cyber-step-btn" id="cyber-pitch-down">-</button>',
            '            <span class="cyber-stepper-val" id="cyber-pitch-val">' + this.refFreq + ' Hz</span>',
            '            <button class="cyber-step-btn" id="cyber-pitch-up">+</button>',
            '        </div>',
            '    </div>',
            '    <div class="cyber-control-group">',
            '        <span class="cyber-control-label">Sweet:</span>',
            '        <select class="cyber-select" id="cyber-sweet-select">' + sweetOpts + '</select>',
            '    </div>',
            '    <div class="cyber-control-group">',
            '        <div class="cyber-mute-toggle ' + (this.isMuted ? 'active' : '') + '" id="cyber-mute-btn" title="Toggle Audio Mute during tuning">',
            '            <div class="cyber-mute-led"></div>',
            '            <span class="cyber-mute-text">MUTE</span>',
            '        </div>',
            '    </div>',
            '</div>'
        ].join('\n');
        this.options.windowModal.html(html);
        this.canvas = document.getElementById('cyber-tuner-canvas');
        if (this.canvas) {
            this.ctx = this.canvas.getContext('2d');
        }
    };


    // ========================================================
    // NATIVE MOD DESKTOP MIDI INTEGRATION
    // Uses the same hardware addressing system as all other controls (/pedalboard/:tuner)
    // ========================================================

    CyberTuner.prototype.initMidi = function () {
        window.CyberTunerInstance = this;
        var self = this;
        if (navigator.requestMIDIAccess) {
            navigator.requestMIDIAccess().then(function (midiAccess) {
                self.midiAccess = midiAccess;
                var inputs = midiAccess.inputs.values();
                for (var input = inputs.next(); input && !input.done; input = inputs.next()) {
                    input.value.onmidimessage = function (msg) {
                        self.handleMidiMessage(msg);
                    };
                }
                midiAccess.onstatechange = function (e) {
                    if (e.port.type === 'input' && e.port.state === 'connected') {
                        e.port.onmidimessage = function (msg) {
                            self.handleMidiMessage(msg);
                        };
                    }
                };
            }).catch(function () {});
        }
    };

    // Called by host.js when midi_map arrives for /pedalboard :tuner
    CyberTuner.prototype.handleNativeMidi = function (channel, control, value) {
        this.updateMidiMappingLabel(channel - 1, control);
    };

    // Called by host.js when param_set arrives for /pedalboard :tuner
    CyberTuner.prototype.handleNativeParamSet = function (val) {
        var isDown = (parseFloat(val) > 0.5);
        if (this.midiMode === 'momentary') {
            // Momentary Footswitch: Toggle on press (down), ignore release (up)
            if (isDown) {
                this.toggle();
            }
        } else if (this.midiMode === 'hold') {
            // Hold Mode: Open on press, close on release
            if (isDown) {
                if (!this.isOpen) this.open();
            } else {
                if (this.isOpen) this.close();
            }
        } else if (this.midiMode === 'latching') {
            // Latching Mode: Open when 1, Close when 0
            if (isDown) {
                if (!this.isOpen) this.open();
            } else {
                if (this.isOpen) this.close();
            }
        }
    };

    CyberTuner.prototype.updateMidiMappingLabel = function (channel, control) {
        this.midiMap = {
            type: 'cc',
            number: control,
            channel: channel + 1,
            label: 'MIDI: CC ' + control + ' (Ch ' + (channel + 1) + ')'
        };
        this.savePreferences();
        $('#cyber-tuner-midi-learn')
            .removeClass('learning')
            .addClass('mapped')
            .text(this.midiMap.label);
    };

    // Opens the standard MOD Desktop hardware addressing dialog for the tuner toggle
    // Identical to assigning any other pedalboard or plugin control
    CyberTuner.prototype.openMidiAssignDialog = function () {
        $('.mod-pedal-settings-address').remove();
        if (!window.desktop || !window.desktop.hardwareManager) {
            alert('MOD Desktop hardware manager not available.\nMake sure a pedalboard is loaded.');
            return;
        }
        var tunerPort = {
            name: 'Tuner',
            shortName: 'Tuner',
            symbol: ':tuner',
            ranges: { minimum: 0.0, maximum: 1.0, default: 0.0 },
            comment: "Toggle Cyber Tuner",
            designation: "",
            properties: ["toggled"],
            enabled: true,
            value: (this.isOpen ? 1.0 : 0.0),
            format: null,
            units: {},
            scalePoints: [],
            widget: this.options.topButton || $("#mod-tuner-top-btn")
        };
        window.desktop.hardwareManager.open(
            "/pedalboard",
            tunerPort,
            "Pedalboard"
        );
    };


    CyberTuner.prototype.handleMidiMessage = function (msg) {
        if (!msg || !msg.data || msg.data.length < 2) return;
        var status = msg.data[0];
        var data1 = msg.data[1];
        var data2 = msg.data.length > 2 ? msg.data[2] : 0;
        var channel = (status & 0x0f) + 1;
        var cmd = status & 0xf0;

        // 1. Learning Mode
        if (this.isLearningMidi) {
            if (cmd === 0xb0) { // Control Change (CC)
                this.midiMap = {
                    type: 'cc',
                    number: data1,
                    channel: channel,
                    label: 'CC ' + data1 + ' (Ch ' + channel + ')'
                };
            } else if (cmd === 0x90 && data2 > 0) { // Note On
                var noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
                var nName = noteNames[data1 % 12] + (Math.floor(data1 / 12) - 1);
                this.midiMap = {
                    type: 'note',
                    number: data1,
                    channel: channel,
                    label: 'Note ' + nName + ' (Ch ' + channel + ')'
                };
            } else if (cmd === 0xc0) { // Program Change (PC)
                this.midiMap = {
                    type: 'pc',
                    number: data1,
                    channel: channel,
                    label: 'PC ' + data1 + ' (Ch ' + channel + ')'
                };
            }

            if (this.midiMap) {
                this.isLearningMidi = false;
                this.savePreferences();
                $('#cyber-tuner-midi-learn')
                    .removeClass('learning')
                    .addClass('mapped')
                    .text(this.midiMap.label);
            }
            return;
        }

        // 2. Trigger Action when Mapped
        if (this.midiMap) {
            var isTarget = false;
            if (this.midiMap.type === 'cc' && cmd === 0xb0 && data1 === this.midiMap.number) {
                isTarget = true;
            } else if (this.midiMap.type === 'note' && cmd === 0x90 && data1 === this.midiMap.number) {
                isTarget = true;
            } else if (this.midiMap.type === 'pc' && cmd === 0xc0 && data1 === this.midiMap.number) {
                isTarget = true;
            }

            if (!isTarget) return;

            // Trigger based on Selected Mode
            if (this.midiMode === 'momentary') {
                // Toggle on Press (value > 0), ignore release (value == 0)
                if (data2 > 0 || cmd === 0xc0) {
                    this.toggle();
                }
            } else if (this.midiMode === 'hold') {
                // Open on Press (value > 0), Close on Release (value == 0)
                if (data2 > 0) {
                    if (!this.isOpen) this.open();
                } else {
                    if (this.isOpen) this.close();
                }
            } else if (this.midiMode === 'latching') {
                // Value >= 64 opens, Value < 64 closes
                if (data2 >= 64) {
                    if (!this.isOpen) this.open();
                } else {
                    if (this.isOpen) this.close();
                }
            }
        }
    };

    CyberTuner.prototype.enumerateAudioDevices = function () {
        var self = this;
        if (!navigator.mediaDevices || !navigator.mediaDevices.enumerateDevices) return;

        navigator.mediaDevices.enumerateDevices().then(function (devices) {
            var audioInputs = devices.filter(function (d) { return d.kind === 'audioinput'; });
            self.discoveredDevices = audioInputs;

            var select = $('#cyber-device-select');
            if (select.length) {
                var currentVal = select.val();
                select.html(self.buildDeviceSelectOptions());
                if (currentVal) {
                    select.val(currentVal);
                } else if (self.selectedDeviceId) {
                    select.val('dev:' + self.selectedDeviceId);
                } else {
                    select.val(self.selectedChannel || 'channel_1');
                }
            }
        }).catch(function (e) {
            console.warn('Tuner device enumeration note:', e);
        });
    };

    CyberTuner.prototype.initWsPitchListener = function () {
        var self = this;
        var hookWs = function () {
            if (typeof ws !== 'undefined' && ws && !ws._cyberTunerHooked) {
                ws._cyberTunerHooked = true;
                var origOnMessage = ws.onmessage;
                ws.onmessage = function (evt) {
                    if (evt && evt.data && typeof evt.data === 'string' && evt.data.indexOf('tuner-pitch ') === 0) {
                        try {
                            var data = JSON.parse(evt.data.substring(12));
                            self.handlePitchMessage(data);
                        } catch (e) {}
                        return;
                    }
                    if (origOnMessage) origOnMessage(evt);
                };
            }
        };

        hookWs();
        var attempts = 0;
        var timer = setInterval(function () {
            hookWs();
            if (attempts++ > 40 || (typeof ws !== 'undefined' && ws && ws._cyberTunerHooked)) {
                clearInterval(timer);
            }
        }, 500);
    };

    CyberTuner.prototype.applyTheme = function (themeKey) {
        if (!PALETTES[themeKey]) themeKey = 'green';
        this.currentTheme = themeKey;
        var p = PALETTES[themeKey];

        document.documentElement.style.setProperty('--tuner-theme-color', p.hex);
        document.documentElement.style.setProperty('--tuner-theme-glow', p.glow);
        document.documentElement.style.setProperty('--tuner-theme-dim', p.dim);

        this.options.windowModal.find('.cyber-color-dot').removeClass('active');
        this.options.windowModal.find('.cyber-color-dot[data-theme="' + themeKey + '"]').addClass('active');

        this.savePreferences();
    };

    CyberTuner.prototype.bindEvents = function () {
        var self = this;

        // Top button and main menu icon click -> toggle modal
        $(document).on('click', '#mod-tuner-top-btn, #mod-tuner-icon, .js-tuner', function (e) {
            e.preventDefault();
            e.stopPropagation();
            self.toggle();
        });

        // Top button right click -> open MIDI Hardware Addressing Dialog (same as all other controls)
        $(document).off('contextmenu', '#mod-tuner-top-btn, #mod-tuner-icon').on('contextmenu', '#mod-tuner-top-btn, #mod-tuner-icon', function (e) {
            e.preventDefault();
            e.stopPropagation();
            self.openMidiAssignDialog();
        });


        // Close button click
        this.options.windowModal.on('click', '.js-cyber-close', function (e) {
            e.preventDefault();
            e.stopPropagation();
            self.close();
        });

        // Click outside (on backdrop) -> close modal
        this.backdrop.on('click', function (e) {
            e.preventDefault();
            self.close();
        });

        // ESC key -> close modal
        $(document).on('keydown', function (e) {
            if (e.keyCode === 27 && self.isOpen) {
                self.close();
            }
        });

        // Top-bar MIDI Assign Button inside modal
        this.options.windowModal.off('click', '#cyber-tuner-midi-learn').on('click', '#cyber-tuner-midi-learn', function (e) {
            e.preventDefault();
            self.openMidiAssignDialog();
        });

        // MIDI Mode Button click (cycles: momentary -> hold -> latching)
        this.options.windowModal.on('click', '#cyber-tuner-midi-mode', function (e) {
            e.preventDefault();
            if (self.midiMode === 'momentary') {
                self.midiMode = 'hold';
            } else if (self.midiMode === 'hold') {
                self.midiMode = 'latching';
            } else {
                self.midiMode = 'momentary';
            }
            $('#cyber-midi-mode-val').text(self.midiMode.toUpperCase());
            self.savePreferences();
        });

        // MIDI Assign Button right-click -> Clear mapping
        this.options.windowModal.on('contextmenu', '#cyber-tuner-midi-learn', function (e) {
            e.preventDefault();
            self.midiMap = null;
            self.isLearningMidi = false;
            self.savePreferences();
            $(this).removeClass('learning mapped').text('MIDI ASSIGN');
        });


        // Audio Input channel / device select
        this.options.windowModal.on('change', '#cyber-device-select', function () {
            var val = $(this).val();
            if (!val) return;

            if (val.indexOf('dev:') === 0) {
                self.selectedDeviceId = val.substring(4);
            } else {
                self.selectedDeviceId = '';
                self.selectedChannel = val; // 'channel_1', 'channel_2', or 'stereo'
            }
            self.savePreferences();

            var jackPort = (self.selectedChannel === 'channel_2') ? '2' : '1';
            self.sendWsCommand('tuner-input ' + jackPort);

            if (self.isOpen) {
                self.stopAudioFallback();
                self.startAudioFallback();
            }
        });

        // Gain Stepper (- / +)
        this.options.windowModal.on('click', '#cyber-gain-down', function () {
            if (self.inputGainDb > 0) {
                self.inputGainDb -= 3;
                self.updateGain();
            }
        });
        this.options.windowModal.on('click', '#cyber-gain-up', function () {
            if (self.inputGainDb < 36) {
                self.inputGainDb += 3;
                self.updateGain();
            }
        });

        // View Mode buttons
        this.options.windowModal.on('click', '.cyber-view-btn', function () {
            var mode = $(this).attr('data-mode');
            self.viewMode = mode;
            self.options.windowModal.find('.cyber-view-btn').removeClass('active');
            $(this).addClass('active');
            self.savePreferences();
        });

        // Theme palette dots
        this.options.windowModal.on('click', '.cyber-color-dot', function () {
            var theme = $(this).attr('data-theme');
            self.applyTheme(theme);
        });

        // Reference Pitch Stepper
        this.options.windowModal.on('click', '#cyber-pitch-down', function () {
            if (self.refFreq > 415) {
                self.refFreq--;
                self.updateRefPitch();
            }
        });
        this.options.windowModal.on('click', '#cyber-pitch-up', function () {
            if (self.refFreq < 466) {
                self.refFreq++;
                self.updateRefPitch();
            }
        });

        // Sweetened Tuning dropdown
        this.options.windowModal.on('change', '#cyber-sweet-select', function () {
            self.sweeteningKey = $(this).val();
            self.savePreferences();
        });


        // --- AUTO RESTORE NATIVE MIDI ON LOAD & PEDALBOARD CHANGE ---
        var restoreMidi = function() {
            if (self.midiMap && self.midiMap.type === 'cc') {
                var restore_ch = self.midiMap.channel - 1;
                var restore_cc = self.midiMap.number;
                if (typeof ws !== 'undefined' && ws && ws.readyState === 1) {
                    ws.send('tuner_midi_map ' + restore_ch + ' ' + restore_cc);
                }
            }
        };

        // 1. Restore on first load (wait for GUI to finish initializing)
        var attempts = 0;
        var restoreInt = setInterval(function() {
            if ($("body").hasClass("initialized") && typeof ws !== 'undefined' && ws && ws.readyState === 1) {
                restoreMidi();
                clearInterval(restoreInt);
            }
            if (attempts++ > 100) clearInterval(restoreInt); // give up after 50s
        }, 500);

        // 2. Restore after ANY pedalboard load/switch
        $(document).ajaxComplete(function(event, xhr, settings) {
            if (settings.url && (
                settings.url.indexOf('/pedalboard/load_bundle') !== -1 ||
                settings.url.indexOf('/snapshot/load') !== -1 ||
                settings.url.indexOf('/banks/load') !== -1 ||
                settings.url.indexOf('/pedalboard/info') !== -1
            )) {
                // Wait 1.5 seconds for backend to settle after load, then inject Tuner
                setTimeout(restoreMidi, 1500);
            }
        });
        // ----------------------------------------

        // Mute toggle
        this.options.windowModal.on('click', '#cyber-mute-btn', function () {
            self.isMuted = !self.isMuted;
            $(this).toggleClass('active', self.isMuted);
            self.savePreferences();
            self.sendWsCommand('tuner-mute ' + (self.isMuted ? '1' : '0'));
        });
    };

    CyberTuner.prototype.updateGain = function () {
        $('#cyber-gain-val').text('+' + this.inputGainDb + ' dB');
        if (this.gainNode) {
            this.gainNode.gain.value = Math.pow(10, this.inputGainDb / 20.0);
        }
        this.savePreferences();
    };

    CyberTuner.prototype.updateRefPitch = function () {
        $('#cyber-pitch-val').text(this.refFreq + ' Hz');
        this.savePreferences();
        this.sendWsCommand('tuner-ref-freq ' + this.refFreq);
    };

    CyberTuner.prototype.sendWsCommand = function (cmd) {
        if (typeof ws !== 'undefined' && ws && ws.readyState === WebSocket.OPEN) {
            ws.send(cmd);
        }
    };

    CyberTuner.prototype.toggle = function () {
        if (this.isOpen) {
            this.close();
        } else {
            this.open();
        }
    };

    CyberTuner.prototype.open = function () {
        this.isOpen = true;
        this.backdrop.fadeIn(150);
        this.options.windowModal.css('display', 'flex').hide().fadeIn(150);
        $('#mod-tuner-top-btn, #mod-tuner-icon').addClass('active');

        this.sendWsCommand('tuner-enable ' + (this.isMuted ? '1' : '0'));
        this.sendWsCommand('tuner-ref-freq ' + this.refFreq);
        var jackPort = (this.selectedChannel === 'channel_2') ? '2' : '1';
        this.sendWsCommand('tuner-input ' + jackPort);

        this.enumerateAudioDevices();
        this.startAnimation();
        this.startAudioFallback();
    };

    CyberTuner.prototype.close = function () {
        this.isOpen = false;
        this.backdrop.fadeOut(150);
        this.options.windowModal.fadeOut(150);
        $('#mod-tuner-top-btn, #mod-tuner-icon').removeClass('active');

        this.sendWsCommand('tuner-disable');

        this.stopAnimation();
        this.stopAudioFallback();
    };

    CyberTuner.prototype.startAudioFallback = function () {
        var self = this;
        if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) return;

        try {
            var AudioContext = window.AudioContext || window.webkitAudioContext;
            if (!AudioContext) return;
            if (!this.audioCtx) this.audioCtx = new AudioContext();

            if (this.audioCtx.state === 'suspended') {
                this.audioCtx.resume();
            }

            var constraints = {
                audio: {
                    echoCancellation: false,
                    noiseSuppression: false,
                    autoGainControl: false,
                    channelCount: { ideal: 2 }
                },
                video: false
            };

            if (this.selectedDeviceId) {
                constraints.audio.deviceId = { exact: this.selectedDeviceId };
            }

            navigator.mediaDevices.getUserMedia(constraints).then(function (stream) {
                self.audioStream = stream;

                // Re-enumerate devices now that permission is granted to get real hardware labels
                if (navigator.mediaDevices.enumerateDevices) {
                    self.enumerateAudioDevices();
                }

                var source = self.audioCtx.createMediaStreamSource(stream);

                // High-headroom Preamp Gain
                self.gainNode = self.audioCtx.createGain();
                self.gainNode.gain.value = Math.pow(10, self.inputGainDb / 20.0);

                // Channel Selection & Isolation using ChannelSplitterNode
                // Channel 1 = Left / Instrument (Output 0)
                // Channel 2 = Right / Mic 2 (Output 1)
                // Stereo    = Mixed
                try {
                    var splitter = self.audioCtx.createChannelSplitter(2);
                    source.connect(splitter);

                    if (self.selectedChannel === 'channel_2') {
                        splitter.connect(self.gainNode, 1);
                    } else if (self.selectedChannel === 'stereo') {
                        source.connect(self.gainNode);
                    } else {
                        // DEFAULT: Channel 1 (Instrument / EVO 4 In 1 / Left)
                        splitter.connect(self.gainNode, 0);
                    }
                } catch (splitErr) {
                    source.connect(self.gainNode);
                }

                // DC sub-bass filter (25Hz highpass)
                self.hpFilter = self.audioCtx.createBiquadFilter();
                self.hpFilter.type = 'highpass';
                self.hpFilter.frequency.value = 25;

                // Fundamental isolate filter (1200Hz lowpass)
                self.lpFilter = self.audioCtx.createBiquadFilter();
                self.lpFilter.type = 'lowpass';
                self.lpFilter.frequency.value = 1200;

                self.analyser = self.audioCtx.createAnalyser();
                self.analyser.fftSize = 4096;
                self.analyser.smoothingTimeConstant = 0.0;

                self.gainNode.connect(self.hpFilter);
                self.hpFilter.connect(self.lpFilter);
                self.lpFilter.connect(self.analyser);

                self.audioBuffer = new Float32Array(self.analyser.fftSize);
            }).catch(function (err) {
                console.warn('Microphone/Audio capture notice:', err);
            });
        } catch (e) {
            console.error('AudioContext error:', e);
        }
    };

    CyberTuner.prototype.stopAudioFallback = function () {
        if (this.audioStream) {
            this.audioStream.getTracks().forEach(function (t) { t.stop(); });
            this.audioStream = null;
        }
    };

    // ========================================================
    // HIGH-ACCURACY YIN PITCH DETECTION ALGORITHM
    // ========================================================
    CyberTuner.prototype.detectPitchFromAudio = function () {
        if (!this.analyser || !this.audioBuffer || !this.audioCtx) return;
        this.analyser.getFloatTimeDomainData(this.audioBuffer);

        var buf = this.audioBuffer;
        var SIZE = buf.length;

        // 1. Calculate RMS Level
        var sumSquares = 0;
        for (var i = 0; i < SIZE; i++) {
            sumSquares += buf[i] * buf[i];
        }
        var rms = Math.sqrt(sumSquares / SIZE);
        this.currentRms = rms;

        // Update live VU meter
        var db = 20 * Math.log10(Math.max(rms, 0.00001));
        var meterPct = Math.max(0, Math.min(100, (db + 60) * (100 / 60)));
        $('#cyber-signal-fill').css('width', meterPct + '%');
        $('#cyber-signal-db').text(db > -59 ? db.toFixed(0) + ' dB' : '-inf dB');

        if (rms < 0.0003) return;

        // 2. YIN Difference Function
        var halfSize = Math.floor(SIZE / 2);
        var yinBuffer = new Float32Array(halfSize);
        yinBuffer[0] = 1.0;

        var runningSum = 0.0;
        for (var tau = 1; tau < halfSize; tau++) {
            var diff = 0.0;
            for (var j = 0; j < halfSize; j++) {
                var delta = buf[j] - buf[j + tau];
                diff += delta * delta;
            }
            runningSum += diff;
            yinBuffer[tau] = diff * tau / runningSum;
        }

        // 3. Absolute threshold minimum search
        var threshold = 0.15;
        var tauEstimate = -1;
        for (var tau = 2; tau < halfSize; tau++) {
            if (yinBuffer[tau] < threshold) {
                while (tau + 1 < halfSize && yinBuffer[tau + 1] < yinBuffer[tau]) {
                    tau++;
                }
                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate === -1) {
            var minVal = 999;
            for (var tau = 4; tau < halfSize; tau++) {
                if (yinBuffer[tau] < minVal) {
                    minVal = yinBuffer[tau];
                    tauEstimate = tau;
                }
            }
            if (minVal > 0.4) return;
        }

        // 4. Parabolic Interpolation
        var x0 = (tauEstimate > 0) ? tauEstimate - 1 : tauEstimate;
        var x2 = (tauEstimate + 1 < halfSize) ? tauEstimate + 1 : tauEstimate;
        var betterTau;
        if (x0 === tauEstimate) {
            betterTau = (yinBuffer[tauEstimate] <= yinBuffer[x2]) ? tauEstimate : x2;
        } else if (x2 === tauEstimate) {
            betterTau = (yinBuffer[tauEstimate] <= yinBuffer[x0]) ? tauEstimate : x0;
        } else {
            var s0 = yinBuffer[x0];
            var s1 = yinBuffer[tauEstimate];
            var s2 = yinBuffer[x2];
            betterTau = tauEstimate + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
        }

        var detectedFreq = this.audioCtx.sampleRate / betterTau;

        if (detectedFreq >= 25.0 && detectedFreq <= 2500.0) {
            var calc = freqToNoteCents(detectedFreq, this.refFreq);
            this.handlePitchMessage({
                freq: detectedFreq,
                note: calc.note + calc.accidental + calc.octave,
                cents: calc.cents
            });
        }
    };

    CyberTuner.prototype.handlePitchMessage = function (data) {
        if (!data) return;

        this.hasSignal = (data.freq && data.freq > 15.0);
        this.lastSignalTime = Date.now();

        if (this.hasSignal) {
            this.detectedFreq = data.freq;
            var rawNote = data.note || '--';
            if (rawNote.length >= 2 && (rawNote[1] === '#' || rawNote[1] === 'b')) {
                this.detectedNote = rawNote[0];
                this.detectedAccidental = rawNote[1];
                this.detectedOctave = rawNote.slice(2);
            } else {
                this.detectedNote = rawNote[0] || '--';
                this.detectedAccidental = '';
                this.detectedOctave = rawNote.slice(1);
            }

            var noteKey = (this.detectedNote + this.detectedAccidental + this.detectedOctave);
            var simpleKey = (this.detectedNote + this.detectedAccidental);
            var sweetOffsets = SWEETENED_TUNINGS[this.sweeteningKey].offsets || {};
            var offset = sweetOffsets[noteKey] !== undefined ? sweetOffsets[noteKey] : (sweetOffsets[simpleKey] || 0.0);

            this.rawCents = (data.cents || 0.0) - offset;
        } else {
            this.detectedNote = '--';
            this.detectedAccidental = '';
            this.detectedOctave = '';
            this.rawCents = 0.0;
        }

        this.updateStatsUI();
    };

    CyberTuner.prototype.updateStatsUI = function () {
        $('#cyber-tuner-note').text(this.detectedNote);
        $('#cyber-tuner-acc').text(this.detectedAccidental);
        $('#cyber-tuner-oct').text(this.detectedOctave);

        if (this.hasSignal) {
            var sign = this.rawCents > 0 ? '+' : '';
            var centsText = sign + this.rawCents.toFixed(1) + ' Cents';
            var inTune = Math.abs(this.rawCents) < 1.5;

            $('#cyber-tuner-cents').text(centsText).toggleClass('in-tune', inTune);
            $('#cyber-tuner-freq').text(this.detectedFreq.toFixed(1) + ' Hz');
        } else {
            $('#cyber-tuner-cents').text('0.0 Cents').removeClass('in-tune');
            $('#cyber-tuner-freq').text('0.0 Hz');
        }
    };

    CyberTuner.prototype.startAnimation = function () {
        var self = this;
        if (this.animId) cancelAnimationFrame(this.animId);

        function loop() {
            if (!self.isOpen) return;
            if (self.audioStream) {
                self.detectPitchFromAudio();
            }
            self.renderCanvas();
            self.animId = requestAnimationFrame(loop);
        }
        loop();
    };

    CyberTuner.prototype.stopAnimation = function () {
        if (this.animId) {
            cancelAnimationFrame(this.animId);
            this.animId = null;
        }
    };

    CyberTuner.prototype.renderCanvas = function () {
        if (!this.ctx || !this.canvas) return;

        var ctx = this.ctx;
        var w = this.canvas.width;
        var h = this.canvas.height;
        var cx = w / 2;
        var cy = h / 2;

        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, w, h);

        if (this.hasSignal && (Date.now() - this.lastSignalTime > 1200)) {
            this.hasSignal = false;
            this.updateStatsUI();
        }

        var targetCents = this.hasSignal ? this.rawCents : 0.0;
        this.smoothCents += (targetCents - this.smoothCents) * 0.25;

        var themeHex = PALETTES[this.currentTheme].hex;
        var inTune = this.hasSignal && (Math.abs(this.smoothCents) < 1.5);

        if (this.viewMode === 'strobe') {
            this.renderStrobeDisc(ctx, cx, cy, themeHex, inTune);
        } else {
            this.renderPeakMeter(ctx, cx, cy, themeHex, inTune);
        }
    };

    // ========================================================
    // MODE 1: CYBER CONCENTRIC DUAL-RING STROBE DISC
    // ========================================================
    CyberTuner.prototype.renderStrobeDisc = function (ctx, cx, cy, themeHex, inTune) {
        var cents = this.smoothCents;

        var coarseVelocity = (cents / 50.0) * 0.08;
        var fineVelocity   = (cents / 15.0) * 0.035;

        if (Math.abs(cents) < 0.3) {
            coarseVelocity = 0.0;
            fineVelocity   = 0.0;
        }

        if (this.hasSignal) {
            this.outerAngle += coarseVelocity;
            this.innerAngle += fineVelocity;
        }

        ctx.save();
        ctx.strokeStyle = inTune ? themeHex : 'rgba(40, 40, 40, 0.7)';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.arc(cx, cy, 108, 0, Math.PI * 2);
        ctx.arc(cx, cy, 48, 0, Math.PI * 2);
        ctx.stroke();

        // 1. Outer Ring (16 Segmented Blocks)
        var outerBlocks = 16;
        var outerRadius = 96;
        var outerBlockLen = 13;
        for (var i = 0; i < outerBlocks; i++) {
            var ang = this.outerAngle + (i * (Math.PI * 2 / outerBlocks));
            ctx.save();
            ctx.translate(cx, cy);
            ctx.rotate(ang);

            ctx.fillStyle = inTune ? themeHex : (i % 2 === 0 ? themeHex : 'rgba(25, 25, 25, 0.8)');
            if (inTune || (this.hasSignal && i % 2 === 0)) {
                ctx.shadowColor = themeHex;
                ctx.shadowBlur = inTune ? 12 : 6;
            } else {
                ctx.shadowBlur = 0;
            }
            ctx.beginPath();
            ctx.roundRect(-outerBlockLen / 2, -outerRadius - 5, outerBlockLen, 11, 3);
            ctx.fill();
            ctx.restore();
        }

        // 2. Inner Ring (24 Segmented Blocks)
        var innerBlocks = 24;
        var innerRadius = 66;
        var innerBlockLen = 7;
        for (var j = 0; j < innerBlocks; j++) {
            var iang = this.innerAngle + (j * (Math.PI * 2 / innerBlocks));
            ctx.save();
            ctx.translate(cx, cy);
            ctx.rotate(iang);

            ctx.fillStyle = inTune ? themeHex : (j % 2 === 0 ? themeHex : 'rgba(18, 18, 18, 0.8)');
            if (inTune || (this.hasSignal && j % 2 === 0)) {
                ctx.shadowColor = themeHex;
                ctx.shadowBlur = inTune ? 10 : 4;
            } else {
                ctx.shadowBlur = 0;
            }
            ctx.beginPath();
            ctx.roundRect(-innerBlockLen / 2, -innerRadius - 4, innerBlockLen, 9, 2);
            ctx.fill();
            ctx.restore();
        }

        // 3. Center In-Tune Lock Diamond
        ctx.save();
        ctx.translate(cx, cy);
        if (inTune) {
            ctx.shadowColor = themeHex;
            ctx.shadowBlur = 24;
            ctx.fillStyle = themeHex;
        } else {
            ctx.fillStyle = '#111111';
            ctx.strokeStyle = '#333333';
            ctx.lineWidth = 2;
        }
        ctx.beginPath();
        ctx.moveTo(0, -14);
        ctx.lineTo(14, 0);
        ctx.lineTo(0, 14);
        ctx.lineTo(-14, 0);
        ctx.closePath();
        ctx.fill();
        if (!inTune) ctx.stroke();
        ctx.restore();

        // 4. Center Crosshairs
        ctx.strokeStyle = inTune ? themeHex : 'rgba(80, 80, 80, 0.6)';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(cx, cy - 110);
        ctx.lineTo(cx, cy - 100);
        ctx.moveTo(cx, cy + 100);
        ctx.lineTo(cx, cy + 110);
        ctx.stroke();

        ctx.restore();
    };

    // ========================================================
    // MODE 2: PEAKED 21-SEGMENT LED BAR METER
    // ========================================================
    CyberTuner.prototype.renderPeakMeter = function (ctx, cx, cy, themeHex, inTune) {
        var numSegments = 21;
        var centerIdx = 10;
        var cents = this.smoothCents;

        var segWidth = 14;
        var segHeight = 46;
        var segGap = 6;
        var totalWidth = numSegments * segWidth + (numSegments - 1) * segGap;
        var startX = cx - totalWidth / 2;

        var activePos = centerIdx + (cents / 50.0) * 10.0;
        activePos = Math.max(0, Math.min(20, activePos));

        ctx.save();

        for (var i = 0; i < numSegments; i++) {
            var x = startX + i * (segWidth + segGap);
            var isCenter = (i === centerIdx);
            var distFromActive = Math.abs(i - activePos);
            var isLit = this.hasSignal && (distFromActive < 1.25);

            var hOffset = isCenter ? 20 : (10 - Math.abs(i - centerIdx)) * 1.4;
            var curHeight = segHeight + hOffset;
            var y = cy - curHeight / 2;

            if (isCenter) {
                if (inTune) {
                    ctx.fillStyle = themeHex;
                    ctx.shadowColor = themeHex;
                    ctx.shadowBlur = 22;
                } else if (isLit) {
                    ctx.fillStyle = themeHex;
                    ctx.shadowColor = themeHex;
                    ctx.shadowBlur = 14;
                } else {
                    ctx.fillStyle = '#141414';
                    ctx.shadowBlur = 0;
                }
            } else {
                if (isLit) {
                    var dist = Math.abs(i - centerIdx);
                    if (dist > 7) {
                        ctx.fillStyle = '#ef4444';
                        ctx.shadowColor = '#ef4444';
                    } else if (dist > 4) {
                        ctx.fillStyle = '#f59e0b';
                        ctx.shadowColor = '#f59e0b';
                    } else {
                        ctx.fillStyle = themeHex;
                        ctx.shadowColor = themeHex;
                    }
                    ctx.shadowBlur = 14;
                } else {
                    ctx.fillStyle = '#101010';
                    ctx.shadowBlur = 0;
                }
            }

            ctx.beginPath();
            ctx.roundRect(x, y, segWidth, curHeight, 3);
            ctx.fill();
        }

        ctx.strokeStyle = inTune ? themeHex : 'rgba(100, 100, 100, 0.7)';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(cx, cy - 44);
        ctx.lineTo(cx, cy - 34);
        ctx.moveTo(cx, cy + 34);
        ctx.lineTo(cx, cy + 44);
        ctx.stroke();

        ctx.restore();
    };

    if (CanvasRenderingContext2D && !CanvasRenderingContext2D.prototype.roundRect) {
        CanvasRenderingContext2D.prototype.roundRect = function (x, y, w, h, r) {
            if (w < 2 * r) r = w / 2;
            if (h < 2 * r) r = h / 2;
            this.beginPath();
            this.moveTo(x + r, y);
            this.arcTo(x + w, y, x + w, y + h, r);
            this.arcTo(x + w, y + h, x, y + h, r);
            this.arcTo(x, y + h, x, y, r);
            this.arcTo(x, y + x + w, y, r);
            this.closePath();
            return this;
        };
    }

    window.CyberTuner = CyberTuner;

    // Self-initialize on DOM ready
    $(document).ready(function () {
        if ($('#mod-tuner-window').length && !window.cyberTunerInstance) {
            window.cyberTunerInstance = new CyberTuner({
                topButton: $('#mod-tuner-top-btn'),
                windowModal: $('#mod-tuner-window')
            });
        }
    });

})(window, jQuery);
