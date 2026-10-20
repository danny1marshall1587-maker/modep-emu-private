// config.js - Global configuration and static bus constants
(function(window) {
    'use strict';

    window.MODEP_PORTS = {
        main_rig: 8881, // Ch 7/8
        insert_2: 8881, // Ch 1/2
        insert_3: 8883, // Ch 3/4
        insert_4: 8884  // Ch 5/6
    };

    window.getModepPortForChannel = function(ch) {
        if (ch === 7 || ch === 8) return window.MODEP_PORTS.main_rig;
        if (ch === 1 || ch === 2) return window.MODEP_PORTS.insert_2;
        if (ch === 3 || ch === 4) return window.MODEP_PORTS.insert_3;
        return window.MODEP_PORTS.insert_4;
    };

    window.openModepInsert = function(ch) {
        const chZero = Math.max(0, Math.min(7, (parseInt(ch, 10) || 1) - 1));
        if (typeof window.openModepCanvasModal === 'function') {
            window.openModepCanvasModal(chZero);
            return;
        }
        const port = window.getModepPortForChannel(ch);
        const host = window.location.hostname || '192.168.1.148';
        window.open(`http://${host}:${port}`, `_modep_${port}`);
    };

    window.openSuperInsert = function(target) {
        if (typeof target === 'string') {
            // Monitor strips: "🎛️ MON 1..6 DSP" opens Monitor Room EQ instead of guitar pedalboard
            if (target.startsWith('mon-') && typeof window.openDspModal === 'function') {
                window.openDspModal('reeq', target);
                return;
            }
            if (target.startsWith('ch-')) {
                const chZero = parseInt(target.replace('ch-', ''), 10) || 0;
                if (typeof window.openModepCanvasModal === 'function') {
                    window.openModepCanvasModal(chZero);
                    return;
                }
            }
        }
        if (typeof window.openModepCanvasModal === 'function') {
            window.openModepCanvasModal(0);
            return;
        }
        const host = window.location.hostname || '192.168.1.148';
        window.open(`http://${host}:8881`, '_modep_8881');
    };

    window.openModepExternal = function(ch = 7) {
        window.openModepInsert(ch);
    };

    window.openRootPedalboard = function() {
        if (typeof window.openModepCanvasModal === 'function') {
            window.openModepCanvasModal(0);
            return;
        }
        const host = window.location.hostname || '192.168.1.148';
        window.open(`http://${host}:8881/`, '_blank');
    };

    window.CONSOLE_COLORS = [
        { name: "Cyan", hex: "#38bdf8" },
        { name: "Emerald", hex: "#10b981" },
        { name: "Amber", hex: "#f59e0b" },
        { name: "Purple", hex: "#a855f7" },
        { name: "Rose", hex: "#f43f5e" },
        { name: "Orange", hex: "#f97316" },
        { name: "Blue", hex: "#3b82f6" },
        { name: "Lime", hex: "#84cc16" },
        { name: "Pink", hex: "#ec4899" },
        { name: "Teal", hex: "#14b8a6" },
        { name: "Gold", hex: "#eab308" },
        { name: "White", hex: "#e2e8f0" }
    ];

    window.BUS_CONFIG = {
        channels: ["Vocal 1", "Guitar", "Bass", "Kick Drum", "Snare", "Hi-Hat / O/H", "Keys L", "Keys R"],
        sends: ["Aux 1: TAL-Reverb-4 (Stereo)", "Aux 2: Stereo IR Convolver", "Aux 3: Ping-Pong Delay", "Aux 4: Tape / Slapback Delay"],
        returns: ["Reverb Return 1", "Reverb Return 2", "Delay Return 3", "Delay Return 4"],
        monitors: ["Stage Wedge L", "Stage Wedge R", "In-Ear Mon 1 (IEM)", "In-Ear Mon 2 (IEM)", "Drum Fill Sub", "Side Fill PA"]
    };
    window.channelNames = window.BUS_CONFIG.channels;

    window.dampingLabels = {
        1: "RAW (INSTANT)", 2: "LIGHT", 3: "FAST", 4: "NATURAL", 5: "SMOOTH",
        6: "CONTROLLED", 7: "HEAVY-GLIDE", 8: "SLOW-CONSOLE", 9: "SUPER-SLOW", 10: "ULTRA-SLOW GLIDE"
    };

    window.dampingGlideCoeffs = {
        1: 1.0, 2: 0.50, 3: 0.30, 4: 0.18, 5: 0.10,
        6: 0.055, 7: 0.028, 8: 0.014, 9: 0.007, 10: 0.0035
    };
})(window);
