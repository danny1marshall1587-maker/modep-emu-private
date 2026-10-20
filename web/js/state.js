// state.js - Centralized application state (PatchboxState)
(function(window) {
    'use strict';

    window.PatchboxState = {
        // Fader levels for Main LR and Sends On Fader (SOF)
        faderValues: {
            main: {
                0: 75, 1: 75, 2: 75, 3: 75, 4: 75, 5: 75, 6: 75, 7: 75,
                master: 75,
                'send-1': 75, 'send-2': 75, 'send-3': 75, 'send-4': 75,
                'ret-1': 75, 'ret-2': 75, 'ret-3': 75, 'ret-4': 75,
                'mon-1': 75, 'mon-2': 75, 'mon-3': 75, 'mon-4': 75, 'mon-5': 75, 'mon-6': 75
            },
            sof: {
                mon1: [0, 0, 0, 0, 0, 0, 0, 0],
                mon2: [0, 0, 0, 0, 0, 0, 0, 0],
                mon3: [0, 0, 0, 0, 0, 0, 0, 0],
                mon4: [0, 0, 0, 0, 0, 0, 0, 0],
                mon5: [0, 0, 0, 0, 0, 0, 0, 0],
                mon6: [0, 0, 0, 0, 0, 0, 0, 0],
                fx1:  [0, 0, 0, 0, 0, 0, 0, 0],
                fx2:  [0, 0, 0, 0, 0, 0, 0, 0],
                fx3:  [0, 0, 0, 0, 0, 0, 0, 0],
                fx4:  [0, 0, 0, 0, 0, 0, 0, 0]
            }
        },

        // Active views and modes
        currentMixerView: 'inputs', // 'inputs' | 'sends' | 'monitors'
        currentSofMode: null,      // null | 'mon1'..'mon6' | 'fx1'..'fx4'
        currentRackChannel: 0,
        dspRackCurrentChannel: 0,

        // Speed damping governor (1 = raw instant, 10 = ultra slow)
        maxSpeedDamping: 1,

        // Channels gain and REGAIN
        channelInputGains: { 0: 0, 1: 0, 2: 0, 3: 0, 4: 0, 5: 0, 6: 0, 7: 0 },
        channelRegainActive: { 0: true, 1: true, 2: true, 3: true, 4: true, 5: true, 6: true, 7: true },

        // Custom names and colors
        customChannelNames: {},
        customChannelColors: {},

        // Stereo linking (0: 0+1, 1: 2+3, 2: 4+5, 3: 6+7)
        stereoLinkState: { 0: false, 1: false, 2: false, 3: false },

        // Step 1: Per-Channel & FX Return L/C/R Pan (-100 = L100, 0 = C, +100 = R100)
        channelPans: {
            0: 0, 1: 0, 2: 0, 3: 0, 4: 0, 5: 0, 6: 0, 7: 0,
            'ret-1': 0, 'ret-2': 0, 'ret-3': 0, 'ret-4': 0
        },

        // Step 1: Per-Channel 80Hz High-Pass Filter Quick Toggle
        channelHpf80: { 0: false, 1: false, 2: false, 3: false, 4: false, 5: false, 6: false, 7: false },

        // Step 1: Sticky Peak / Clip Hold States & Mute/Solo Matrix
        clipHoldState: {},
        muteState: {
            0: false, 1: false, 2: false, 3: false, 4: false, 5: false, 6: false, 7: false,
            'send-1': false, 'send-2': false, 'send-3': false, 'send-4': false,
            'ret-1': false, 'ret-2': false, 'ret-3': false, 'ret-4': false,
            'mon-1': false, 'mon-2': false, 'mon-3': false, 'mon-4': false, 'mon-5': false, 'mon-6': false,
            master: false
        },
        soloState: {
            0: false, 1: false, 2: false, 3: false, 4: false, 5: false, 6: false, 7: false,
            'send-1': false, 'send-2': false, 'send-3': false, 'send-4': false,
            'ret-1': false, 'ret-2': false, 'ret-3': false, 'ret-4': false
        },
        stageMicsMuted: false,
        savedStageMicMuteState: {},

        // Mute / Solo states
        isMomentaryFxMuted: false,
        previousFxMuteState: {},

        // Tempo
        globalBpm: 120.0,
        tapTimes: [],
        tapTimeout: null,

        // Live telemetry arrays
        liveRtaBins: new Float32Array(64),
        channelGrValues: new Float32Array(8),
        busGrValues: {
            'send-1': 0, 'send-2': 0, 'send-3': 0, 'send-4': 0,
            'ret-1': 0, 'ret-2': 0, 'ret-3': 0, 'ret-4': 0,
            'mon-1': 0, 'mon-2': 0, 'mon-3': 0, 'mon-4': 0, 'mon-5': 0, 'mon-6': 0,
            master: 0
        }
    };

    // Aliases on window for backward-compatibility with existing scripts and handlers
    window.faderValues = window.PatchboxState.faderValues;
    window.currentMixerView = window.PatchboxState.currentMixerView;
    window.currentSofMode = window.PatchboxState.currentSofMode;
    window.currentRackChannel = window.PatchboxState.currentRackChannel;
    window.dspRackCurrentChannel = window.PatchboxState.dspRackCurrentChannel;
    window.maxSpeedDamping = window.PatchboxState.maxSpeedDamping;
    window.channelInputGains = window.PatchboxState.channelInputGains;
    window.channelRegainActive = window.PatchboxState.channelRegainActive;
    window.customChannelNames = window.PatchboxState.customChannelNames;
    window.customChannelColors = window.PatchboxState.customChannelColors;
    window.stereoLinkState = window.PatchboxState.stereoLinkState;
    window.channelPans = window.PatchboxState.channelPans;
    window.channelHpf80 = window.PatchboxState.channelHpf80;
    window.clipHoldState = window.PatchboxState.clipHoldState;
    window.muteState = window.PatchboxState.muteState;
    window.soloState = window.PatchboxState.soloState;
    window.globalBpm = window.PatchboxState.globalBpm;
    window.liveRtaBins = window.PatchboxState.liveRtaBins;
    window.channelGrValues = window.PatchboxState.channelGrValues;
    window.busGrValues = window.PatchboxState.busGrValues;

})(window);
