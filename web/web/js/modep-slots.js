// modep-slots.js - 100% Full-Screen 4-Slot Channel Effect Rack, Category Picker & MOD-GUI / Touch Control Deck
(function(window) {
    'use strict';

    const AMP_PORTS = [
        { sym: "gain", name: "Preamp Gain", min: 0, max: 10, def: 5.5 },
        { sym: "bass", name: "Bass", min: 0, max: 10, def: 5.0 },
        { sym: "mid", name: "Middle", min: 0, max: 10, def: 5.5 },
        { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.0 },
        { sym: "presence", name: "Presence", min: 0, max: 10, def: 5.5 },
        { sym: "master", name: "Master Level", min: 0, max: 10, def: 7.0 },
        { sym: "bright", name: "Bright Switch", min: 0, max: 1, def: 0, toggled: true },
        { sym: "deep", name: "Deep / Fat Voicing", min: 0, max: 1, def: 0, toggled: true }
    ];

    const DRIVE_PORTS = [
        { sym: "drive", name: "Drive / Gain", min: 0, max: 10, def: 5.5 },
        { sym: "tone", name: "Tone / Treble", min: 0, max: 10, def: 5.8 },
        { sym: "level", name: "Output Level", min: 0, max: 10, def: 7.2 },
        { sym: "bright", name: "Fat / Bright Boost", min: 0, max: 1, def: 0, toggled: true },
        { sym: "voice", name: "Clipping Diode Mode", min: 0, max: 1, def: 0, toggled: true }
    ];

    const SYNTH9_PORTS = [
        { sym: "dry", name: "Dry Guitar Level", min: 0, max: 10, def: 5.0 },
        { sym: "effect", name: "Synth / Organ Level", min: 0, max: 10, def: 7.5 },
        { sym: "tone", name: "Filter / Cutoff", min: 0, max: 10, def: 6.0 },
        { sym: "depth", name: "Mod / Harmonics", min: 0, max: 10, def: 5.0 },
        { sym: "bright", name: "Octave Up Shimmer", min: 0, max: 1, def: 1, toggled: true },
        { sym: "mode", name: "Preset Voice Bank", min: 0, max: 1, def: 0, toggled: true }
    ];

    const TONE3000_MODELS = [
        { label: "Martin D-45 Studio Mic (Acoustic)", value: 0 },
        { label: "Taylor 814ce Grand Auditorium", value: 1 },
        { label: "Gibson J-45 Vintage Round Shoulder", value: 2 },
        { label: "Guild F-512 12-String Acoustic", value: 3 },
        { label: "Fender '65 Deluxe Reverb Blackface", value: 4 },
        { label: "Fender '59 Tweed Bassman 4x10", value: 5 },
        { label: "Dumble Overdrive Special (Clean)", value: 6 },
        { label: "Matchless DC-30 Chime Channel", value: 7 },
        { label: "Vox AC30 Top Boost 1964", value: 8 },
        { label: "Marshall Bluesbreaker 1962 (JTM45)", value: 9 },
        { label: "Orange Rockerverb 50 MkIII", value: 10 },
        { label: "Marshall JCM800 2203 Classic 80s", value: 11 },
        { label: "Soldano SLO-100 Super Lead Overdrive", value: 12 },
        { label: "Mesa/Boogie Dual Rectifier Multi-Watt", value: 13 },
        { label: "Ampeg SVT Classic 8x10 Tube Stack", value: 14 },
        { label: "Darkglass Microtubes B7K Ultra", value: 15 }
    ];

    const TONE3000_IRS = [
        { label: "Martin D-45 Studio Condenser (Neumann U87)", value: 0 },
        { label: "Taylor 814ce Ribbon Mic (Royer R-121)", value: 1 },
        { label: "Gibson J-45 Vintage Dynamic (Shure SM7B)", value: 2 },
        { label: "Guild 12-String Stereo Room Pair", value: 3 },
        { label: "Fender Deluxe 1x12 Jensen C12N", value: 4 },
        { label: "Fender Bassman 4x10 Jensen P10R", value: 5 },
        { label: "Dumble 2x12 Electro-Voice EVM12L", value: 6 },
        { label: "Vox AC30 2x12 Celestion Alnico Blue", value: 7 },
        { label: "Marshall 1960A 4x12 Celestion V30", value: 8 },
        { label: "Orange PPC412 4x12 Celestion V30", value: 9 },
        { label: "Soldano 4x12 Eminence Legend X12", value: 10 },
        { label: "Mesa Rectifier Oversized 4x12 V30", value: 11 },
        { label: "Ampeg SVT-810E 8x10 Sealed Bass Cab", value: 12 },
        { label: "Darkglass DG410N 4x10 Neodymium Bass", value: 13 }
    ];

    const NAM_LOADER_PROFILES = [
        { label: "Fender '65 Deluxe Reverb", value: 0 },
        { label: "Fender '59 Tweed Bassman", value: 1 },
        { label: "Dumble ODS Clean Channel", value: 2 },
        { label: "Matchless DC-30 Clean", value: 3 },
        { label: "Vox AC30 Top Boost 1964", value: 4 },
        { label: "Marshall Bluesbreaker 1962", value: 5 },
        { label: "Marshall JTM45 Plexi", value: 6 },
        { label: "Orange Rockerverb 50", value: 7 },
        { label: "Marshall JCM800 2203", value: 8 },
        { label: "Soldano SLO-100 Super Lead", value: 9 },
        { label: "Peavey 5150 Block Letter", value: 10 },
        { label: "Mesa Dual Rectifier Multi-Watt", value: 11 },
        { label: "Friedman BE-100 Brown Eye", value: 12 },
        { label: "Bogner Ecstasy 101B Red", value: 13 },
        { label: "ENGL Powerball II Lead", value: 14 },
        { label: "Diezel VH4 Channel 3", value: 15 },
        { label: "Ampeg SVT-CL Classic Stack", value: 16 },
        { label: "Darkglass Microtubes B7K", value: 17 },
        { label: "Acoustic Tube DI Preamp", value: 18 },
        { label: "Klon Centaur Gold Horsie", value: 19 },
        { label: "Ibanez TS808 Tube Screamer", value: 20 },
        { label: "ProCo Rat 2 Vintage LM308", value: 21 },
        { label: "Electro-Harmonix Big Muff Pi", value: 22 },
        { label: "King of Tone High Gain Side", value: 23 }
    ];

    const EVANS_BASS_PRESETS = [
        { label: "1: Evans Precision", value: 0 },
        { label: "2: Longhorn", value: 1 },
        { label: "3: Fretless", value: 2 },
        { label: "4: Synth (Taurus)", value: 3 },
        { label: "5: Virtual", value: 4 },
        { label: "6: Bowed", value: 5 },
        { label: "7: Split Bass", value: 6 },
        { label: "8: 3:03", value: 7 },
        { label: "9: Flip-Flop", value: 8 },
        { label: "10: Studio Low-Fat Bass (Wavetable)", value: 9 },
        { label: "11: Real Electric Studio C2", value: 10 },
        { label: "12: Real 5-String Sub B0", value: 11 },
        { label: "13: Real Bridge Fretless D", value: 12 },
        { label: "14: Real Acoustic Upright Pluck", value: 13 },
        { label: "15: Real Acoustic Cello Body", value: 14 },
        { label: "16: Real Accordion Bass Reed", value: 15 },
        { label: "17: Real Upright Piano A0 Sub", value: 16 },
        { label: "18: Real Fat Vintage P-Bass", value: 17 }
    ];

    window.MODEP_LV2_CATALOG = [
        { uri: "thru", name: "EMPTY (THRU)", category: "Pass-Through", catKey: "all", icon: "➡️", stereo: false, isCyber: false, ports: [], presets: [] },

        // === USER'S CUSTOM CYBER AMPS & NAM (Exact .ttl ControlPort Symbols & Defaults) ===
        { uri: "http://cyber-audio.co.uk/plugins/cyber-ultimate-hybrid", name: "Cyber Ultimate Hybrid ('65 BF + '59 JMP)", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🏆", stereo: false, ports: [
            { sym: "channel_mode", name: "Channel Topology", min: 0, max: 2, def: 2, integer: true, scalePoints: [{ label: "'65 Blackface Glassy Clean", value: 0 }, { label: "'59 Bassman / JMP Crunch", value: 1 }, { label: "Jumpered Parallel Blend", value: 2 }] },
            { sym: "clean_vol", name: "Blackface Clean Vol", min: 0, max: 10, def: 6.2 },
            { sym: "drive_gain", name: "Bassman / JMP Gain", min: 0, max: 10, def: 5.8 },
            { sym: "drive_vol", name: "Bassman / JMP Vol", min: 0, max: 10, def: 5.5 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 5.5 },
            { sym: "mid", name: "Middle", min: 0, max: 10, def: 6.0 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.5 },
            { sym: "bright_sw", name: "120pF Bright Switch", min: 0, max: 1, def: 1, toggled: true },
            { sym: "slope_mode", name: "Tone Stack Voicing", min: 0, max: 1, def: 0, integer: true, scalePoints: [{ label: "56k Fender Bassman", value: 0 }, { label: "33k Marshall JMP", value: 1 }] },
            { sym: "power_tubes", name: "Power Tubes", min: 0, max: 1, def: 0, integer: true, scalePoints: [{ label: "2x 6L6GC American", value: 0 }, { label: "2x EL34 British", value: 1 }] },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 5.5 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 5.8 },
            { sym: "sag_amount", name: "B+ Tube Sag", min: 0, max: 10, def: 6.0 },
            { sym: "rev_dwell", name: "Spring Dwell (12AT7)", min: 0, max: 10, def: 5.0 },
            { sym: "rev_mix", name: "Spring Reverb Mix", min: 0, max: 100, def: 22.0 },
            { sym: "rev_tone", name: "Spring Reverb Tone", min: 0, max: 10, def: 6.2 },
            { sym: "rev_drip", name: "Spring Drip Dispersion", min: 0, max: 10, def: 6.8 },
            { sym: "trem_speed", name: "Vibrato Speed", min: 0.5, max: 12, def: 3.8 },
            { sym: "trem_depth", name: "Vibrato Intensity", min: 0, max: 100, def: 28.0 },
            { sym: "trem_mode", name: "Harmonic Tube Vibrato", min: 0, max: 1, def: 1, toggled: true },
            { sym: "quality_mode", name: "Physics / CPU Scale", min: 0, max: 3, def: 1, integer: true, scalePoints: [{ label: "Eco Live (1x Fast)", value: 0 }, { label: "Stage (2x Oversample)", value: 1 }, { label: "Studio (4x High-Precision)", value: 2 }, { label: "Ultra Bounce (8x Pure Physics)", value: 3 }] },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 3, def: 0, integer: true, scalePoints: [{ label: "2x12 '65 Twin Jensen C12N", value: 0 }, { label: "4x10 '59 Bassman Jensen P10R", value: 1 }, { label: "4x12 '68 Marshall Greenback", value: 2 }, { label: "Direct Out / Cab Bypass", value: 3 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 5.5 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/euphoria-amp", name: "Euphoria Amp (Pure Fluid Dynamics '65 BF + '59 JMP)", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "✨", stereo: false, ports: [
            { sym: "channel_mode", name: "Channel Topology", min: 0, max: 2, def: 2, integer: true, scalePoints: [{ label: "'65 Blackface Fluid Clean", value: 0 }, { label: "'59 Bassman / JMP Fluid Crunch", value: 1 }, { label: "Jumpered Parallel Fluid Blend", value: 2 }] },
            { sym: "clean_vol", name: "Blackface Clean Vol", min: 0, max: 10, def: 6.2 },
            { sym: "drive_gain", name: "Bassman / JMP Gain", min: 0, max: 10, def: 5.8 },
            { sym: "drive_vol", name: "Bassman / JMP Vol", min: 0, max: 10, def: 5.5 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 5.5 },
            { sym: "mid", name: "Middle", min: 0, max: 10, def: 6.0 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.5 },
            { sym: "bright_sw", name: "120pF Bright Switch", min: 0, max: 1, def: 1, toggled: true },
            { sym: "slope_mode", name: "Tone Stack Voicing", min: 0, max: 1, def: 0, integer: true, scalePoints: [{ label: "56k Tweed Bassman", value: 0 }, { label: "33k Marshall JMP", value: 1 }] },
            { sym: "power_tubes", name: "Power Tubes", min: 0, max: 1, def: 0, integer: true, scalePoints: [{ label: "2x 6L6GC American", value: 0 }, { label: "2x EL34 British", value: 1 }] },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 5.5 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 5.8 },
            { sym: "sag_amount", name: "Fluid Chamber Sag", min: 0, max: 10, def: 6.0 },
            { sym: "rev_dwell", name: "Spring Dwell (12AT7)", min: 0, max: 10, def: 5.0 },
            { sym: "rev_mix", name: "Spring Reverb Mix", min: 0, max: 100, def: 22.0 },
            { sym: "rev_tone", name: "Spring Reverb Tone", min: 0, max: 10, def: 6.2 },
            { sym: "rev_drip", name: "Spring Drip Dispersion", min: 0, max: 10, def: 6.8 },
            { sym: "trem_speed", name: "Vibrato Speed", min: 0.5, max: 12, def: 3.8 },
            { sym: "trem_depth", name: "Vibrato Intensity", min: 0, max: 100, def: 28.0 },
            { sym: "trem_mode", name: "Harmonic Tube Vibrato", min: 0, max: 1, def: 1, toggled: true },
            { sym: "quality_mode", name: "Fluid Dynamics / CPU Scale", min: 0, max: 3, def: 1, integer: true, scalePoints: [{ label: "Eco Live (1x Fast)", value: 0 }, { label: "Stage (2x Oversample)", value: 1 }, { label: "Studio (4x High-Precision)", value: 2 }, { label: "Ultra Bounce (8x Pure Fluid Physics)", value: 3 }] },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 3, def: 0, integer: true, scalePoints: [{ label: "2x12 '65 Twin Jensen C12N", value: 0 }, { label: "4x10 '59 Bassman Jensen P10R", value: 1 }, { label: "4x12 '68 Marshall Greenback", value: 2 }, { label: "Direct Out / Cab Bypass", value: 3 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 5.5 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-fender-vibroverb63", name: "Cyber '63 Fender Vibroverb", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🎸", stereo: false, ports: [
            { sym: "vib_vol", name: "Vibrato Vol", min: 0, max: 10, def: 6.3 },
            { sym: "vib_treble", name: "Vibrato Treble", min: 0, max: 10, def: 6.7 },
            { sym: "vib_bass", name: "Vibrato Bass", min: 0, max: 10, def: 3.4 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 5.3 },
            { sym: "channel", name: "Channel (Normal / Vibrato)", min: 0, max: 1, def: 1, toggled: true },
            { sym: "norm_vol", name: "Normal Vol", min: 0, max: 10, def: 5.0 },
            { sym: "norm_treble", name: "Normal Treble", min: 0, max: 10, def: 5.5 },
            { sym: "norm_bass", name: "Normal Bass", min: 0, max: 10, def: 5.0 },
            { sym: "trem_speed", name: "Vibrato Speed", min: 1, max: 12, def: 4.5 },
            { sym: "trem_intensity", name: "Harmonic Intensity", min: 0, max: 100, def: 13.0 },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "1x15 JBL D130F", value: 0 }, { label: "2x10 Oxford", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 5.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-fender-bassman59", name: "Cyber '59 Fender Bassman Tweed", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🎸", stereo: false, ports: [
            { sym: "bright_vol", name: "Bright Vol", min: 0, max: 10, def: 1.6 },
            { sym: "normal_vol", name: "Normal Vol", min: 0, max: 10, def: 1.6 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 7.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 4.1 },
            { sym: "middle", name: "Middle", min: 0, max: 10, def: 7.0 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 5.5 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 7.4 },
            { sym: "channel_link", name: "Channel Link / Jumper", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "4x10 Jensen P10R", value: 0 }, { label: "2x12 Tweed Oxford", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 6.7 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-fender-deluxe6g3", name: "Cyber '62 Fender Brown Deluxe 6G3", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🎸", stereo: false, ports: [
            { sym: "bright_vol", name: "Bright Vol", min: 0, max: 10, def: 2.0 },
            { sym: "bright_tone", name: "Bright Tone", min: 0, max: 10, def: 4.8 },
            { sym: "norm_vol", name: "Normal Vol", min: 0, max: 10, def: 5.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 3.6 },
            { sym: "channel", name: "Channel (Normal / Bright)", min: 0, max: 1, def: 1, toggled: true },
            { sym: "norm_tone", name: "Normal Tone", min: 0, max: 10, def: 5.5 },
            { sym: "trem_speed", name: "Tremolo Speed", min: 1, max: 12, def: 6.4 },
            { sym: "trem_intensity", name: "Tremolo Intensity", min: 0, max: 100, def: 35.3 },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "1x12 Oxford 12K5", value: 0 }, { label: "1x12 Celestion Blue", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 10.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-fender-vibrolux6g11", name: "Cyber Fender Vibrolux 6G11", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🎸", stereo: false, ports: [
            { sym: "bright_vol", name: "Bright Vol", min: 0, max: 10, def: 5.1 },
            { sym: "bright_treble", name: "Bright Treble", min: 0, max: 10, def: 7.3 },
            { sym: "bright_bass", name: "Bright Bass", min: 0, max: 10, def: 6.5 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 4.5 },
            { sym: "channel", name: "Channel (Normal / Bright)", min: 0, max: 1, def: 1, toggled: true },
            { sym: "norm_vol", name: "Normal Vol", min: 0, max: 10, def: 5.0 },
            { sym: "norm_treble", name: "Normal Treble", min: 0, max: 10, def: 5.5 },
            { sym: "norm_bass", name: "Normal Bass", min: 0, max: 10, def: 5.0 },
            { sym: "trem_speed", name: "Tremolo Speed", min: 1, max: 12, def: 4.4 },
            { sym: "trem_intensity", name: "Tremolo Intensity", min: 0, max: 100, def: 30.0 },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "1x12 Jensen P12N", value: 0 }, { label: "2x10 Jensen C10Q", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 4.5 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-dumble-ods", name: "Cyber Dumble Overdrive Special", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "👑", stereo: false, ports: [
            { sym: "clean_vol", name: "Preamp Volume", min: 0, max: 10, def: 5.79 },
            { sym: "od_drive", name: "Overdrive Drive", min: 0, max: 10, def: 6.0 },
            { sym: "od_level", name: "Overdrive Level", min: 0, max: 10, def: 6.17 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 5.32 },
            { sym: "channel", name: "Overdrive Mode", min: 0, max: 1, def: 0, toggled: true },
            { sym: "clean_treble", name: "Treble", min: 0, max: 10, def: 7.34 },
            { sym: "clean_mid", name: "Middle", min: 0, max: 10, def: 4.69 },
            { sym: "clean_bass", name: "Bass", min: 0, max: 10, def: 5.0 },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 4.2 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 6.88 },
            { sym: "bright", name: "Bright Switch", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mid_boost", name: "Mid Boost", min: 0, max: 1, def: 0, toggled: true },
            { sym: "rock_jazz", name: "Rock / Jazz Voicing", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 1, integer: true, scalePoints: [{ label: "1x12 EV12L Open", value: 0 }, { label: "2x12 Dumble Ported", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 5.8 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-marshall-jtm45", name: "Cyber Marshall JTM45 Plexi", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "⚡", stereo: false, ports: [
            { sym: "high_treb_vol", name: "High Treble Vol", min: 0, max: 10, def: 3.59 },
            { sym: "normal_vol", name: "Normal Vol", min: 0, max: 10, def: 3.44 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.1 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 4.1 },
            { sym: "middle", name: "Middle", min: 0, max: 10, def: 7.6 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 6.88 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 5.9 },
            { sym: "channel_link", name: "Channel Link / Jumper", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 1, integer: true, scalePoints: [{ label: "2x12 Bluesbreaker Alnico", value: 0 }, { label: "4x12 1960A Greenback", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 3.32 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-friedman-be100", name: "Cyber Friedman BE-100 Brown Eye", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🔥", stereo: false, ports: [
            { sym: "be_gain", name: "BE / HBE Gain", min: 0, max: 10, def: 6.4 },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 2.5 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 2.8 },
            { sym: "channel", name: "Channel (Clean / BE)", min: 0, max: 1, def: 0, toggled: true },
            { sym: "hbe_mode", name: "HBE High-Gain Mode", min: 0, max: 1, def: 0, toggled: true },
            { sym: "clean_vol", name: "Clean Volume", min: 0, max: 10, def: 6.7 },
            { sym: "mid", name: "Middle", min: 0, max: 10, def: 6.2 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 6.7 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 5.0 },
            { sym: "c45", name: "C45 Custom Voicing", min: 0, max: 1, def: 1, toggled: true },
            { sym: "fat", name: "FAT Low-End Boost", min: 0, max: 1, def: 1, toggled: true },
            { sym: "sat", name: "SAT Diode Clipping", min: 0, max: 1, def: 0, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "4x12 Friedman V30/G12M", value: 0 }, { label: "2x12 Vintage Greenback", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 10.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-vox-ac30tb", name: "Cyber Vox AC30 Top Boost", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🇬🇧", stereo: false, ports: [
            { sym: "tb_vol", name: "Top Boost Vol", min: 0, max: 10, def: 6.4 },
            { sym: "tb_treble", name: "Treble", min: 0, max: 10, def: 6.9 },
            { sym: "tb_bass", name: "Bass", min: 0, max: 10, def: 5.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 4.7 },
            { sym: "norm_vol", name: "Normal Vol", min: 0, max: 10, def: 6.4 },
            { sym: "cut", name: "Cut Control", min: 0, max: 10, def: 0.0 },
            { sym: "channel_link", name: "Channel Link / Jumper", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "2x12 Celestion Alnico Blue", value: 0 }, { label: "2x12 G12M Greenback", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 5.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-vox-ac30pa", name: "Cyber Vox AC30 Paul's Boutique", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🇬🇧", stereo: false, ports: [
            { sym: "ch1_vol", name: "Ch 1 Vol (Bright)", min: 0, max: 10, def: 6.1 },
            { sym: "ch4_vol", name: "Ch 4 Vol (Lead)", min: 0, max: 10, def: 6.1 },
            { sym: "tone", name: "Master Tone Tilt", min: 0, max: 10, def: 10.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 4.8 },
            { sym: "ch2_vol", name: "Ch 2 Vol (Normal)", min: 0, max: 10, def: 3.6 },
            { sym: "ch3_vol", name: "Ch 3 Vol (Warm)", min: 0, max: 10, def: 3.6 },
            { sym: "cut", name: "Cut Control", min: 0, max: 10, def: 0.0 },
            { sym: "master_link", name: "Master 4-Ch Jumper", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 1, integer: true, scalePoints: [{ label: "2x12 Alnico Blue", value: 0 }, { label: "2x12 Silver Bell", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 6.9 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-matchless-dc30", name: "Cyber Matchless DC-30 Class-A", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "✨", stereo: false, ports: [
            { sym: "ch1_vol", name: "Ch 1 Volume (12AX7)", min: 0, max: 10, def: 10.0 },
            { sym: "ch2_vol", name: "Ch 2 Volume (EF86)", min: 0, max: 10, def: 4.7 },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 10.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 3.7 },
            { sym: "channel", name: "Channel Select (Ch1 / EF86)", min: 0, max: 1, def: 0, toggled: true },
            { sym: "ch1_treble", name: "Ch 1 Treble", min: 0, max: 10, def: 10.0 },
            { sym: "ch1_bass", name: "Ch 1 Bass", min: 0, max: 10, def: 3.5 },
            { sym: "ch2_tone", name: "Ch 2 Tone Rotary", min: 0, max: 5, def: 5.0, integer: true },
            { sym: "cut", name: "Cut Control", min: 0, max: 10, def: 0.0 },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "2x12 G12H30/Greenback", value: 0 }, { label: "2x12 Alnico Blue", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 10.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 0, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-tworock-crs", name: "Cyber Two-Rock Custom Reverb", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "💎", stereo: false, ports: [
            { sym: "gain", name: "Gain", min: 0, max: 10, def: 3.1 },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 6.4 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.8 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 6.9 },
            { sym: "middle", name: "Middle", min: 0, max: 10, def: 5.7 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 6.5 },
            { sym: "contour", name: "Contour", min: 0, max: 10, def: 7.3 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 6.7 },
            { sym: "bright", name: "Bright Switch", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mid_boost", name: "Mid Boost Switch", min: 0, max: 1, def: 1, toggled: true },
            { sym: "deep", name: "Deep Switch", min: 0, max: 1, def: 1, toggled: true },
            { sym: "gain_structure", name: "Gain Structure", min: 0, max: 2, def: 1, integer: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "2x12 Two-Rock 12-65B", value: 0 }, { label: "1x12 EV12L", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 7.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-toneking-imperial", name: "Cyber Tone King Imperial MKII", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "👑", stereo: false, ports: [
            { sym: "rhythm_vol", name: "Rhythm Volume", min: 0, max: 10, def: 1.6 },
            { sym: "lead_vol", name: "Lead Volume", min: 0, max: 10, def: 3.8 },
            { sym: "mid_bite", name: "Mid-Bite Control", min: 0, max: 10, def: 3.3 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 6.2 },
            { sym: "channel", name: "Channel (Rhythm / Lead)", min: 0, max: 1, def: 0, toggled: true },
            { sym: "rhythm_treble", name: "Rhythm Treble", min: 0, max: 10, def: 8.3 },
            { sym: "rhythm_bass", name: "Rhythm Bass", min: 0, max: 10, def: 4.8 },
            { sym: "lead_tone", name: "Lead Tone", min: 0, max: 10, def: 7.1 },
            { sym: "attenuator", name: "Ironman Attenuation", min: 0, max: 5, def: 0.0, integer: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 1, def: 0, integer: true, scalePoints: [{ label: "1x12 Tone King Custom", value: 0 }, { label: "Direct / Bypass Cab", value: 1 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 6.4 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-trainwreck-express", name: "Cyber Trainwreck Express", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🚂", stereo: false, ports: [
            { sym: "volume", name: "Volume", min: 0, max: 10, def: 0.7 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.8 },
            { sym: "middle", name: "Middle", min: 0, max: 10, def: 6.4 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 4.5 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 3.3 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 6.9 },
            { sym: "bright", name: "Bright Switch", min: 0, max: 2, def: 2, integer: true, scalePoints: [{ label: "Off", value: 0 }, { label: "Bright 1 (100pF)", value: 1 }, { label: "Bright 2 (500pF)", value: 2 }] },
            { sym: "edge_mod", name: "Wild Bloom Mod", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "4x12 Celestion Greenback", value: 0 }, { label: "2x12 Vintage G12H30", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 8.1 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-hiwatt-dr504", name: "Cyber Hiwatt DR504 Custom 50", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🔊", stereo: false, ports: [
            { sym: "normal_vol", name: "Normal Volume", min: 0, max: 10, def: 5.0 },
            { sym: "brill_vol", name: "Brilliant Volume", min: 0, max: 10, def: 5.0 },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 5.5 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 3.1 },
            { sym: "input_link", name: "Channel Link / Jumper", min: 0, max: 1, def: 1, toggled: true },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 5.0 },
            { sym: "middle", name: "Middle", min: 0, max: 10, def: 6.0 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 5.5 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 4.5 },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "4x12 Fane Crescendo", value: 0 }, { label: "2x12 Fane Purple", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 7.7 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-orange-or120", name: "Cyber Orange OR120 Pics Only", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🍊", stereo: false, ports: [
            { sym: "gain", name: "Volume / Gain", min: 0, max: 10, def: 3.3 },
            { sym: "hf_drive", name: "H.F. Drive", min: 0, max: 10, def: 8.0 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 6.9 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 3.7 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 2.0 },
            { sym: "fac", name: "F.A.C. Rotary", min: 0, max: 5, def: 0, integer: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "4x12 Orange PPC412", value: 0 }, { label: "2x12 Closed V30", value: 1 }, { label: "Direct / Bypass Cab", value: 2 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 10.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-magnatone-280", name: "Cyber Magnatone 280 Vibrato", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🌀", stereo: false, ports: [
            { sym: "vib_vol", name: "Vibrato Vol", min: 0, max: 10, def: 5.6 },
            { sym: "vib_treble", name: "Vibrato Treble", min: 0, max: 10, def: 5.8 },
            { sym: "vib_depth", name: "Varistor Depth", min: 0, max: 100, def: 33.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 5.2 },
            { sym: "channel", name: "Channel (Normal / Vibrato)", min: 0, max: 1, def: 1, toggled: true },
            { sym: "norm_vol", name: "Normal Vol", min: 0, max: 10, def: 5.0 },
            { sym: "norm_treble", name: "Normal Treble", min: 0, max: 10, def: 5.5 },
            { sym: "norm_bass", name: "Normal Bass", min: 0, max: 10, def: 5.0 },
            { sym: "vib_bass", name: "Vibrato Bass", min: 0, max: 10, def: 5.0 },
            { sym: "vib_speed", name: "Varistor Speed", min: 1, max: 10, def: 6.12 },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 1, def: 0, integer: true, scalePoints: [{ label: "2x12 Magnatone Oxford", value: 0 }, { label: "Direct / Bypass Cab", value: 1 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 10.0 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-roland-jc120", name: "Cyber Roland JC-120 Jazz Chorus", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🌊", stereo: true, ports: [
            { sym: "volume", name: "Volume", min: 0, max: 10, def: 6.9 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 5.5 },
            { sym: "middle", name: "Middle", min: 0, max: 10, def: 5.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 5.0 },
            { sym: "distortion", name: "Distortion", min: 0, max: 10, def: 3.9 },
            { sym: "bright", name: "Bright Switch", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mod_mode", name: "Modulation Mode", min: 0, max: 2, def: 2, integer: true, scalePoints: [{ label: "Off", value: 0 }, { label: "Vibrato", value: 1 }, { label: "Dimensional Chorus", value: 2 }] },
            { sym: "mod_speed", name: "Mod Speed", min: 0.5, max: 10, def: 3.5 },
            { sym: "mod_depth", name: "Mod Depth", min: 0, max: 100, def: 2.0 },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 1, def: 0, integer: true, scalePoints: [{ label: "2x12 Roland Silver Cone", value: 0 }, { label: "Direct / Bypass Cab", value: 1 }] },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 3.5 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-rockman-x100", name: "Cyber Rockman X100 Headphone Amp", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🚀", stereo: true, ports: [
            { sym: "mode", name: "Rockman Mode", min: 0, max: 3, def: 2, integer: true, scalePoints: [{ label: "CLN 2 (Crystal Clean)", value: 0 }, { label: "CLN 1 (Warm Clean)", value: 1 }, { label: "EDGE (Boston Rhythm)", value: 2 }, { label: "DIST (Arena Sustain Lead)", value: 3 }] },
            { sym: "volume", name: "Volume Level", min: 0, max: 10, def: 8.7 },
            { sym: "chorus_depth", name: "Chorus Depth", min: 0, max: 100, def: 75.3 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "chorus", name: "BBD Stereo Chorus", min: 0, max: 1, def: 0, toggled: true },
            { sym: "bass_boost", name: "Bass Boost", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 4.6 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-koch-the-greg", name: "Cyber Koch The Greg Signature", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🎸", stereo: false, ports: [
            { sym: "gain", name: "Drive / Gain", min: 0, max: 10, def: 6.0 },
            { sym: "volume", name: "Volume", min: 0, max: 10, def: 5.0 },
            { sym: "treble", name: "Treble", min: 0, max: 10, def: 5.5 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "channel", name: "Channel", min: 0, max: 2, def: 0, integer: true, scalePoints: [{ label: "Clean", value: 0 }, { label: "Crunch", value: 1 }, { label: "Lead", value: 2 }] },
            { sym: "mid", name: "Middle", min: 0, max: 10, def: 5.5 },
            { sym: "bass", name: "Bass", min: 0, max: 10, def: 5.0 },
            { sym: "ots_drive", name: "OTS Drive", min: 0, max: 10, def: 4.0 },
            { sym: "ots_level", name: "OTS Level", min: 0, max: 10, def: 5.0 },
            { sym: "ots_switch", name: "OTS Circuit", min: 0, max: 1, def: 1, toggled: true },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 5.0 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 5.0 },
            { sym: "trem_speed", name: "Tremolo Speed", min: 0.5, max: 10, def: 2.7 },
            { sym: "trem_depth", name: "Tremolo Depth", min: 0, max: 100, def: 39.3 },
            { sym: "trem_mode", name: "Harmonic Tremolo Mode", min: 0, max: 1, def: 0, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 1, integer: true },
            { sym: "speaker_drive", name: "Cone Stress / Drive", min: 0, max: 10, def: 4.5 },
            { sym: "noise_gate", name: "Zero Noise Suppressor", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "Spectral De-Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "Dynamic De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_level", name: "Noise Depth", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-ycv40", name: "Cyber Traynor YCV40 Tube Combo", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🍁", stereo: false, ports: [
            { sym: "clean_vol", name: "Clean Volume", min: 0, max: 10, def: 5.0 },
            { sym: "lead_gain", name: "Lead Gain", min: 0, max: 10, def: 3.1 },
            { sym: "master", name: "Master Volume", min: 0, max: 10, def: 5.0 },
            { sym: "output_level", name: "Output Level", min: 0, max: 10, def: 5.1 },
            { sym: "channel", name: "Channel (Clean / Lead)", min: 0, max: 1, def: 0, toggled: true },
            { sym: "clean_treble", name: "Clean Treble", min: 0, max: 10, def: 5.0 },
            { sym: "clean_bass", name: "Clean Bass", min: 0, max: 10, def: 5.0 },
            { sym: "bright", name: "Clean Bright", min: 0, max: 1, def: 1, toggled: true },
            { sym: "lead_vol", name: "Lead Volume", min: 0, max: 10, def: 10.0 },
            { sym: "lead_treble", name: "Lead Treble", min: 0, max: 10, def: 5.3 },
            { sym: "lead_mid", name: "Lead Mid", min: 0, max: 10, def: 10.0 },
            { sym: "lead_bass", name: "Lead Bass", min: 0, max: 10, def: 3.3 },
            { sym: "boost", name: "Lead Boost", min: 0, max: 1, def: 1, toggled: true },
            { sym: "scoop", name: "Lead Scoop", min: 0, max: 1, def: 1, toggled: true },
            { sym: "reverb", name: "Spring Reverb", min: 0, max: 10, def: 2.5 },
            { sym: "presence", name: "Presence", min: 0, max: 10, def: 5.0 },
            { sym: "power_tubes", name: "Power Tubes", min: 0, max: 3, def: 3, integer: true, scalePoints: [{ label: "6L6GC Stock", value: 0 }, { label: "EL34 British", value: 1 }, { label: "6V6 Sweet", value: 2 }, { label: "KT88 Big Iron", value: 3 }] },
            { sym: "mod_c10", name: "Mod: C10 Warm Clean", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mod_smooth", name: "Mod: Smooth Dumble Lead", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mod_chime", name: "Mod: Open Raw Chime", min: 0, max: 1, def: 1, toggled: true },
            { sym: "speaker_cab", name: "Speaker Cabinet", min: 0, max: 2, def: 0, integer: true },
            { sym: "speaker_drive", name: "Speaker Stress / Drive", min: 0, max: 10, def: 6.3 },
            { sym: "noise_gate", name: "Smart Noise Suppressor", min: 0, max: 1, def: 1, toggled: true }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro", name: "Cyber Evans Bass Pro Rig", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🎸", stereo: true, ports: [
            { sym: "preset", name: "Bass Preset", min: 0, max: 17, def: 0, integer: true, scalePoints: EVANS_BASS_PRESETS },
            { sym: "wet", name: "Bass Vol", min: 0, max: 10, def: 8.0 },
            { sym: "dry", name: "Dry Level", min: 0, max: 10, def: 5.0 },
            { sym: "output", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "ctrl1", name: "Tone / Attack / Param 1", min: 0, max: 10, def: 5.0 },
            { sym: "ctrl2", name: "Filter / Sub / Param 2", min: 0, max: 10, def: 5.0 },
            { sym: "bass_tone", name: "Bass Tone", min: 0, max: 10, def: 5.0 },
            { sym: "walk_groove", name: "Walk / Groove", min: 0, max: 10, def: 0.0 },
            { sym: "solo_mode", name: "Bass Solo Lead Mode", min: 0, max: 1, def: 0, toggled: true },
            { sym: "fretless_express", name: "Fretless Expression", min: 0, max: 1, def: 0, toggled: true },
            { sym: "bass_mute", name: "Bass Mute (Keep Listening)", min: 0, max: 1, def: 0, toggled: true }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-tone3000", name: "Cyber Tone3000 Neural Rig", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🧠", stereo: false, ports: [
            { sym: "model", name: "NAM Model", min: 0, max: 15, def: 0, integer: true, scalePoints: TONE3000_MODELS },
            { sym: "input_gain", name: "Input Gain", min: -24, max: 24, def: 0.0, unit: "dB" },
            { sym: "treble", name: "Treble", min: -12, max: 12, def: 0.0, unit: "dB" },
            { sym: "output_level", name: "Output Level", min: -24, max: 24, def: 0.0, unit: "dB" },
            { sym: "ir", name: "Cab / Mic IR", min: 0, max: 13, def: 0, integer: true, scalePoints: TONE3000_IRS },
            { sym: "bass", name: "Bass", min: -12, max: 12, def: 0.0, unit: "dB" },
            { sym: "mid", name: "Middle", min: -12, max: 12, def: 0.0, unit: "dB" },
            { sym: "ir_blend", name: "IR Blend", min: 0, max: 100, def: 100.0, unit: "%" },
            { sym: "gate", name: "Noise Gate", min: -80, max: -20, def: -80.0, unit: "dB" }
        ] },
        { uri: "http://moddevices.com/plugins/danny/nam-loader", name: "Cyber NAM Neural Amp Loader (24 Rigs)", category: "Cyber Amps & Cab", catKey: "amp", isCyber: true, icon: "🧠", stereo: false, ports: [
            { sym: "profile_a", name: "Slot A NAM Model", min: 0, max: 23, def: 8, integer: true, scalePoints: NAM_LOADER_PROFILES },
            { sym: "drive", name: "Gain / Drive", min: 0, max: 1, def: 0.5 },
            { sym: "treble", name: "Treble", min: 0, max: 1, def: 0.5 },
            { sym: "volume", name: "Master Volume", min: 0, max: 1, def: 0.5 },
            { sym: "bass", name: "Bass", min: 0, max: 1, def: 0.5 },
            { sym: "mid", name: "Middle", min: 0, max: 1, def: 0.5 },
            { sym: "gate", name: "Noise Gate", min: -80, max: 0, def: -80, unit: "dB" },
            { sym: "cab_enable", name: "Cab IR Simulation", min: 0, max: 1, def: 1, toggled: true },
            { sym: "active_slot", name: "Active Slot (A / B)", min: 0, max: 1, def: 0, toggled: true },
            { sym: "profile_b", name: "Slot B NAM Model", min: 0, max: 23, def: 9, integer: true, scalePoints: NAM_LOADER_PROFILES }
        ] },

        // === USER'S CUSTOM CYBER DRIVES / OVERDRIVES / DISTORTION (Exact 0..100 .ttl Ports) ===
        { uri: "http://cyber-audio.co.uk/plugins/cyber-screamer", name: "Cyber TubeScreamer TS808 Overdrive", category: "Cyber Drive / Distortion", catKey: "drive", isCyber: true, icon: "🔥", stereo: false, ports: [
            { sym: "drive", name: "Drive", min: 0, max: 100, def: 100.0 },
            { sym: "tone", name: "Tone", min: 0, max: 100, def: 37.7 },
            { sym: "level", name: "Level", min: 0, max: 100, def: 70.0 },
            { sym: "sparkle", name: "Glassy Sparkle", min: 0, max: 100, def: 71.5 },
            { sym: "sens", name: "Sensitivity", min: 0, max: 100, def: 50.8 },
            { sym: "bias", name: "Morph Bias", min: 0, max: 100, def: 0.0 },
            { sym: "fat_mod", name: "Fat Mod", min: 0, max: 1, def: 0, toggled: true },
            { sym: "mix", name: "Mix", min: 0, max: 100, def: 100.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-klon", name: "Cyber Klon Centaur Gold Overdrive", category: "Cyber Drive / Distortion", catKey: "drive", isCyber: true, icon: "🐎", stereo: false, ports: [
            { sym: "gain", name: "Gain", min: 0, max: 100, def: 81.1 },
            { sym: "treble", name: "Treble", min: 0, max: 100, def: 65.3 },
            { sym: "output", name: "Output", min: 0, max: 100, def: 70.0 },
            { sym: "sparkle", name: "Glassy Sparkle", min: 0, max: 100, def: 65.6 },
            { sym: "sens", name: "Sensitivity", min: 0, max: 100, def: 0.0 },
            { sym: "bias", name: "Morph Bias", min: 0, max: 100, def: 0.0 },
            { sym: "magic_mod", name: "Magic Diode Mod", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mix", name: "Mix", min: 0, max: 100, def: 100.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-blues-driver", name: "Cyber BD-2 Blues Driver Waza", category: "Cyber Drive / Distortion", catKey: "drive", isCyber: true, icon: "💙", stereo: false, ports: [
            { sym: "gain", name: "Gain", min: 0, max: 100, def: 64.1 },
            { sym: "tone", name: "Tone", min: 0, max: 100, def: 65.8 },
            { sym: "level", name: "Level", min: 0, max: 100, def: 79.5 },
            { sym: "sparkle", name: "Glassy Sparkle", min: 0, max: 100, def: 99.7 },
            { sym: "sens", name: "Sensitivity", min: 0, max: 100, def: 36.0 },
            { sym: "bias", name: "Morph Bias", min: 0, max: 100, def: 0.0 },
            { sym: "phat_mod", name: "Phat Mod", min: 0, max: 1, def: 0, toggled: true },
            { sym: "mix", name: "Mix", min: 0, max: 100, def: 100.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-noble-drive", name: "Cyber ODR-1 Noble Natural Drive", category: "Cyber Drive / Distortion", catKey: "drive", isCyber: true, icon: "💚", stereo: false, ports: [
            { sym: "drive", name: "Drive", min: 0, max: 100, def: 62.2 },
            { sym: "spectrum", name: "Spectrum", min: 0, max: 100, def: 68.6 },
            { sym: "presence", name: "Presence", min: 0, max: 100, def: 68.8 },
            { sym: "volume", name: "Volume", min: 0, max: 100, def: 76.6 },
            { sym: "sparkle", name: "Glassy Sparkle", min: 0, max: 100, def: 78.1 },
            { sym: "bass_cut", name: "Bass Cut", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mix", name: "Mix", min: 0, max: 100, def: 100.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-ds1", name: "Cyber DS-1 Keeley Mod Distortion", category: "Cyber Drive / Distortion", catKey: "drive", isCyber: true, icon: "🟠", stereo: false, ports: [
            { sym: "dist", name: "Distortion", min: 0, max: 100, def: 100.0 },
            { sym: "tone", name: "Tone", min: 0, max: 100, def: 75.1 },
            { sym: "level", name: "Level", min: 0, max: 100, def: 81.3 },
            { sym: "sparkle", name: "Glassy Sparkle", min: 0, max: 100, def: 83.1 },
            { sym: "ultra_mod", name: "Ultra Mod", min: 0, max: 1, def: 1, toggled: true },
            { sym: "mix", name: "Mix", min: 0, max: 100, def: 100.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-guvnor", name: "Cyber Marshall Guv'nor MK1 Drive", category: "Cyber Drive / Distortion", catKey: "drive", isCyber: true, icon: "🇬🇧", stereo: false, ports: [
            { sym: "gain", name: "Gain", min: 0, max: 100, def: 100.0 },
            { sym: "treble", name: "Treble", min: 0, max: 100, def: 70.6 },
            { sym: "middle", name: "Middle", min: 0, max: 100, def: 67.8 },
            { sym: "level", name: "Level", min: 0, max: 100, def: 70.0 },
            { sym: "bass", name: "Bass", min: 0, max: 100, def: 15.0 },
            { sym: "sparkle", name: "Glassy Sparkle", min: 0, max: 100, def: 83.3 },
            { sym: "hot_rod", name: "Hot Rod Mod", min: 0, max: 1, def: 0, toggled: true },
            { sym: "mix", name: "Mix", min: 0, max: 100, def: 100.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-slide-driver", name: "Cyber Slide Driver Sustain Preamp", category: "Cyber Drive / Distortion", catKey: "drive", isCyber: true, icon: "⚡", stereo: false, ports: [
            { sym: "comp_a", name: "Comp A (Peak)", min: 0, max: 100, def: 45.0 },
            { sym: "comp_b", name: "Comp B (Sustain)", min: 0, max: 100, def: 65.0 },
            { sym: "drive", name: "Overdrive", min: 0, max: 100, def: 38.0 },
            { sym: "output", name: "Level", min: 0, max: 100, def: 50.0 },
            { sym: "clean", name: "Clean Headroom", min: 0, max: 100, def: 25.0 },
            { sym: "tone", name: "Tone", min: 0, max: 100, def: 55.0 },
            { sym: "de_ice", name: "De-Ice / Smooth", min: 0, max: 100, def: 40.0 },
            { sym: "mid_boost", name: "Vocal Mids", min: 0, max: 100, def: 60.0 },
            { sym: "bass_tight", name: "Bass Tight", min: 0, max: 100, def: 75.0 },
            { sym: "ratio_b", name: "Comp B Ratio", min: 0, max: 1, def: 0, toggled: true },
            { sym: "zero_noise", name: "Zero Noise", min: 0, max: 1, def: 1, toggled: true },
            { sym: "clean_mode", name: "Clean Comp Mode", min: 0, max: 1, def: 0, toggled: true }
        ] },

        // === USER'S CUSTOM CYBER REVERB & DELAY (Exact .ttl Ports) ===
        { uri: "http://cyberaudio.com/plugins/cyber-spring-reverb", name: "Cyber '63 Outboard Spring Reverb", category: "Cyber Reverb / Space", catKey: "reverb", isCyber: true, icon: "✨", stereo: false, ports: [
            { sym: "dwell", name: "Spring Dwell", min: 0, max: 100, def: 50.0 },
            { sym: "tone", name: "Spring Tone", min: 0, max: 100, def: 60.0 },
            { sym: "drip", name: "Surf Drip", min: 0, max: 100, def: 65.0 },
            { sym: "mix", name: "Wet Mix", min: 0, max: 100, def: 35.0 },
            { sym: "decay", name: "Decay", min: 0, max: 100, def: 50.0 }
        ] },
        { uri: "http://cyberaudio.com/plugins/cyber-spring-reverb-stereo", name: "Cyber '63 Spring Reverb (Stereo)", category: "Cyber Reverb / Space", catKey: "reverb", isCyber: true, icon: "✨", stereo: true, ports: [
            { sym: "dwell", name: "Spring Dwell", min: 0, max: 100, def: 50.0 },
            { sym: "tone", name: "Spring Tone", min: 0, max: 100, def: 60.0 },
            { sym: "drip", name: "Surf Drip", min: 0, max: 100, def: 70.0 },
            { sym: "mix", name: "Wet Mix", min: 0, max: 100, def: 35.0 },
            { sym: "decay", name: "Decay", min: 0, max: 100, def: 65.0 }
        ] },
        { uri: "http://dusk-audio.com/plugins/duskverb", name: "DuskVerb Studio Plate & Hall", category: "Cyber Reverb / Space", catKey: "reverb", isCyber: true, icon: "🏛️", stereo: true, ports: [
            { sym: "algorithm", name: "Algorithm", min: 0, max: 4, def: 1, integer: true, scalePoints: [{ label: "Room", value: 0 }, { label: "Hall", value: 1 }, { label: "Plate", value: 2 }, { label: "Chamber", value: 3 }, { label: "Ambient", value: 4 }] },
            { sym: "decay", name: "Decay Time (s)", min: 0.2, max: 30, def: 3.0 },
            { sym: "predelay", name: "Pre-Delay (ms)", min: 0, max: 250, def: 20.0 },
            { sym: "mix", name: "Wet Mix (%)", min: 0, max: 100, def: 50.0 },
            { sym: "damping", name: "High Damping", min: 0, max: 1, def: 0.3 },
            { sym: "width", name: "Stereo Width (%)", min: 0, max: 200, def: 100.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-cloud-bloom", name: "Cyber Cloud Bloom Ambient Reverb/Delay", category: "Cyber Reverb / Space", catKey: "reverb", isCyber: true, icon: "☁️", stereo: true, ports: [
            { sym: "time", name: "Time (ms)", min: 20, max: 2000, def: 450.0 },
            { sym: "feedback", name: "Repeat (%)", min: 0, max: 100, def: 65.0 },
            { sym: "bloom", name: "Bloom (Rise)", min: 0, max: 100, def: 40.0 },
            { sym: "mix", name: "Mix (%)", min: 0, max: 100, def: 50.0 },
            { sym: "smear", name: "Smear (Diffusion)", min: 0, max: 100, def: 75.0 },
            { sym: "warmth", name: "Warmth", min: 0, max: 100, def: 50.0 },
            { sym: "mod", name: "Tape Mod", min: 0, max: 100, def: 35.0 },
            { sym: "shimmer", name: "Chime Shimmer", min: 0, max: 100, def: 30.0 },
            { sym: "hold", name: "Infinite Freeze", min: 0, max: 100, def: 0.0 }
        ] },
        { uri: "http://dusk-audio.com/plugins/tape-echo-2", name: "Dusk Tape Echo 2 (RE-201 Space Echo)", category: "Cyber Delay / Echo", catKey: "delay", isCyber: true, icon: "📼", stereo: true, ports: [
            { sym: "mode", name: "Head Mode", min: 0, max: 5, def: 1, integer: true },
            { sym: "rate", name: "Repeat Rate (ms)", min: 50, max: 800, def: 300.0 },
            { sym: "intensity", name: "Intensity", min: 0, max: 1.2, def: 0.6 },
            { sym: "echo_vol", name: "Echo Volume", min: 0, max: 1, def: 0.8 },
            { sym: "spring", name: "Spring Reverb", min: 0, max: 1, def: 0.3 },
            { sym: "age", name: "Tape Age", min: 0, max: 1, def: 0.3 },
            { sym: "dry_vol", name: "Dry Volume", min: 0, max: 1, def: 1.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-puresustain-delay", name: "Cyber PureSustain Studio Delay", category: "Cyber Delay / Echo", catKey: "delay", isCyber: true, icon: "⏱️", stereo: true, ports: [
            { sym: "time", name: "Delay Time (ms)", min: 20, max: 2000, def: 400.0 },
            { sym: "feedback", name: "Feedback (%)", min: 0, max: 110, def: 45.0 },
            { sym: "melt", name: "Melt Sustainer", min: 0, max: 100, def: 50.0 },
            { sym: "mix", name: "Mix (%)", min: 0, max: 100, def: 40.0 },
            { sym: "tone", name: "Tone", min: 0, max: 100, def: 70.0 },
            { sym: "duck", name: "Ducker (%)", min: 0, max: 100, def: 0.0 },
            { sym: "mod_on", name: "Mod On", min: 0, max: 1, def: 0, toggled: true },
            { sym: "mod_depth", name: "Mod Depth", min: 0, max: 100, def: 25.0 },
            { sym: "mod_rate", name: "Mod Rate (Hz)", min: 0.1, max: 5, def: 0.8 }
        ] },

        // === USER'S CUSTOM CYBER SYNTH / 9-SERIES / MODULATION ===
        { uri: "http://cyber-audio.co.uk/plugins/cyber-bass9", name: "Cyber BASS9 Bass Machine Synth", category: "Cyber Mod & Synth", catKey: "mod", isCyber: true, icon: "🎸", stereo: false, ports: [
            { sym: "preset", name: "Bass Preset", min: 0, max: 8, def: 0, integer: true },
            { sym: "wet", name: "Bass Vol", min: 0, max: 10, def: 8.0 },
            { sym: "dry", name: "Dry Level", min: 0, max: 10, def: 5.0 },
            { sym: "output", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "ctrl1", name: "Tone / Attack", min: 0, max: 10, def: 5.0 },
            { sym: "ctrl2", name: "Filter / Sub", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-mel9", name: "Cyber MEL9 Tape Replay Mellotron", category: "Cyber Mod & Synth", catKey: "mod", isCyber: true, icon: "🎻", stereo: false, ports: [
            { sym: "preset", name: "Mellotron Instrument", min: 0, max: 8, def: 0, integer: true },
            { sym: "wet", name: "Mel Level", min: 0, max: 10, def: 8.0 },
            { sym: "dry", name: "Dry Level", min: 0, max: 10, def: 5.0 },
            { sym: "output", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "attack", name: "Attack Swell", min: 0, max: 10, def: 2.0 },
            { sym: "sustain", name: "Sustain", min: 0, max: 10, def: 5.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-organ9", name: "Cyber ORGAN9 Tonewheel Organ Synth", category: "Cyber Mod & Synth", catKey: "mod", isCyber: true, icon: "🎹", stereo: false, ports: [
            { sym: "rotary", name: "Rotary Speed", min: 0, max: 2, def: 1, integer: true },
            { sym: "wet", name: "Organ Level", min: 0, max: 10, def: 8.0 },
            { sym: "dry", name: "Dry Level", min: 0, max: 10, def: 5.0 },
            { sym: "output", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "click", name: "Key Click", min: 0, max: 10, def: 3.0 },
            { sym: "db_8", name: "8' Drawbar", min: 0, max: 8, def: 8.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-string9", name: "Cyber STRING9 String Ensemble Synth", category: "Cyber Mod & Synth", catKey: "mod", isCyber: true, icon: "🎻", stereo: false, ports: [
            { sym: "preset", name: "String Ensemble Mode", min: 0, max: 4, def: 0, integer: true },
            { sym: "wet", name: "String Level", min: 0, max: 10, def: 8.0 },
            { sym: "dry", name: "Dry Level", min: 0, max: 10, def: 5.0 },
            { sym: "output", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "ensemble_depth", name: "Ensemble Chorus", min: 0, max: 10, def: 7.0 },
            { sym: "brightness", name: "Brightness", min: 0, max: 10, def: 6.0 }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-synth9", name: "Cyber SYNTH9 Vintage Poly Synthesizer", category: "Cyber Mod & Synth", catKey: "mod", isCyber: true, icon: "🛸", stereo: false, ports: [
            { sym: "preset", name: "Synth Model", min: 0, max: 8, def: 0, integer: true },
            { sym: "wet", name: "Synth Level", min: 0, max: 10, def: 8.0 },
            { sym: "dry", name: "Dry Level", min: 0, max: 10, def: 5.0 },
            { sym: "output", name: "Output Level", min: 0, max: 10, def: 7.0 },
            { sym: "cutoff", name: "Filter Cutoff", min: 0, max: 10, def: 5.0 },
            { sym: "resonance", name: "Filter Resonance", min: 0, max: 10, def: 4.0 }
        ] },
        { uri: "http://moddevices.com/plugins/danny/cyber-stomp-box", name: "Cyber Acoustic Porchboard Stomp Box", category: "Cyber Mod & Synth", catKey: "mod", isCyber: true, icon: "🥾", stereo: false, ports: [
            { sym: "sample_slot", name: "Sample Sound", min: 0, max: 6, def: 0, integer: true },
            { sym: "sub_thump", name: "Sub Thump", min: 0, max: 15, def: 3.0 },
            { sym: "punch", name: "Punch", min: 0, max: 1, def: 0.6 },
            { sym: "stomp_vol", name: "Stomp Level (dB)", min: -60, max: 12, def: 0.0 }
        ] },

        // === USER'S CUSTOM CYBER EQ / FILTER / DYNAMICS / UTILITY ===
        { uri: "http://cyberaudio.com/plugins/cyber-piezo-thump-killer", name: "Cyber Piezo Thump & Quack Killer", category: "Cyber EQ / Filter", catKey: "eq", isCyber: true, icon: "🛡️", stereo: false, ports: [
            { sym: "thump_depth", name: "Thump Depth (dB)", min: 0, max: 24, def: 14.0 },
            { sym: "mud_depth", name: "Mud Depth (dB)", min: 0, max: 18, def: 10.0 },
            { sym: "harmonics", name: "Harmonics (%)", min: 0, max: 100, def: 100.0 },
            { sym: "out_gain", name: "Output Level (dB)", min: -12, max: 6, def: 0.0 }
        ] },
        { uri: "http://moddevices.com/plugins/danny/cyber-hum-killer", name: "Cyber 50/60Hz Hum & Buzz Killer", category: "Cyber EQ / Filter", catKey: "eq", isCyber: true, icon: "⚡", stereo: true, ports: [
            { sym: "reduction", name: "Reduction (%)", min: 0, max: 100, def: 100.0 },
            { sym: "smooth", name: "Smooth (%)", min: 0, max: 100, def: 100.0 },
            { sym: "grid_freq", name: "Grid Frequency (50/60Hz)", min: 0, max: 1, def: 0, toggled: true },
            { sym: "learn", name: "Learn Hum", min: 0, max: 1, def: 0, toggled: true }
        ] },
        { uri: "http://cyber-audio.co.uk/plugins/cyber-acoustic-feedbacker", name: "Cyber Acoustic Feedback Eliminator", category: "Cyber EQ / Filter", catKey: "eq", isCyber: true, icon: "🎯", stereo: true, ports: [
            { sym: "gain", name: "Feedback Gain", min: 0, max: 100, def: 75.0 },
            { sym: "distress", name: "Speaker Distress", min: 0, max: 100, def: 50.0 },
            { sym: "tail", name: "Tail Decay (s)", min: 0.1, max: 5, def: 1.8 },
            { sym: "mix", name: "Feedback Mix (%)", min: 0, max: 100, def: 50.0 }
        ] },
        { uri: "http://moddevices.com/plugins/danny/cyber-denoiser", name: "Cyber-Denoiser PRO (Slot Instance)", category: "Cyber Dynamics", catKey: "dyn", isCyber: true, icon: "🛡️", stereo: true, ports: [
            { sym: "sensitivity", name: "Master Sensitivity", min: -20, max: 20, def: 0.0, unit: "dB" },
            { sym: "reduction", name: "Noise Reduction", min: 0, max: 100, def: 100.0, unit: "%" },
            { sym: "low_cut", name: "40Hz Rumble Cut", min: 0, max: 1, def: 1, toggled: true },
            { sym: "learn", name: "Learn Noise", min: 0, max: 1, def: 0, toggled: true }
        ] },
        { uri: "http://moddevices.com/plugins/danny/smart-fizz-killer", name: "Cyber Smart Fizz Killer", category: "Cyber EQ / Filter", catKey: "eq", isCyber: true, icon: "✨", stereo: true, ports: [
            { sym: "fizz_depth", name: "Fizz Depth (dB)", min: 0, max: 18, def: 6.0 },
            { sym: "body", name: "Warmth Body (dB)", min: 0, max: 12, def: 3.0 },
            { sym: "high_cut", name: "High Cut (Hz)", min: 4000, max: 16000, def: 9000.0 },
            { sym: "output_level", name: "Output (dB)", min: -12, max: 6, def: 0.0 }
        ] },

        // === STANDARD MODEP STUDIO FX ===
        { uri: "http://guitarix.sourceforge.net/plugins/gx_compressor#_compressor", name: "Gx Studio FET Compressor", category: "Dynamics / Compression", catKey: "dyn", icon: "🗜️", stereo: false, ports: [{ sym: "ATTACK", name: "Attack", min: 0.1, max: 50, def: 2 }, { sym: "RELEASE", name: "Release", min: 10, max: 500, def: 80 }, { sym: "THRESHOLD", name: "Threshold", min: -40, max: 0, def: -16 }, { sym: "RATIO", name: "Ratio", min: 1, max: 20, def: 4 }] },
        { uri: "http://github.com/bernhardrusch/molot-lite-mono-lv2", name: "Molot Lite Bus Compressor", category: "Dynamics / Compression", catKey: "dyn", icon: "🗜️", stereo: false, ports: [{ sym: "thresh", name: "Threshold (dB)", min: -40, max: 0, def: -18 }, { sym: "ratio", name: "Ratio", min: 1, max: 10, def: 4 }, { sym: "attack", name: "Attack (ms)", min: 0.1, max: 100, def: 10 }, { sym: "release", name: "Release (ms)", min: 10, max: 1000, def: 120 }, { sym: "makeup", name: "Makeup (dB)", min: -12, max: 24, def: 3 }] },
        { uri: "http://moddevices.com/plugins/mda/DeEss", name: "MDA Vocal De-Esser", category: "Dynamics / Vocal", catKey: "dyn", icon: "🎙️", stereo: false, ports: [{ sym: "thresh", name: "Threshold", min: 0, max: 1, def: 0.5 }, { sym: "freq", name: "Frequency", min: 0, max: 1, def: 0.6 }, { sym: "hf_drive", name: "HF Drive", min: 0, max: 1, def: 0.5 }] },
        { uri: "http://gareus.org/oss/lv2/fil4#stereo", name: "x42 4-Band Parametric EQ", category: "Equalizer / Filter", catKey: "eq", icon: "🎛️", stereo: true, ports: [{ sym: "gain1", name: "Low Gain (dB)", min: -18, max: 18, def: 0 }, { sym: "gain2", name: "Low-Mid (dB)", min: -18, max: 18, def: 0 }, { sym: "gain3", name: "High-Mid (dB)", min: -18, max: 18, def: 0 }, { sym: "gain4", name: "High Gain (dB)", min: -18, max: 18, def: 0 }] },
        { uri: "http://distrho.sf.net/plugins/3BandEQ", name: "DISTRHO 3-Band Console EQ", category: "Equalizer / Filter", catKey: "eq", icon: "🌊", stereo: true, ports: [{ sym: "low", name: "Low (dB)", min: -24, max: 24, def: 0 }, { sym: "mid", name: "Mid (dB)", min: -24, max: 24, def: 0 }, { sym: "high", name: "High (dB)", min: -24, max: 24, def: 0 }, { sym: "master", name: "Output (dB)", min: -24, max: 24, def: 0 }] },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_delay_stereo#_delay_stereo", name: "Gx Stereo Ping-Pong Delay", category: "Delay / Echo", catKey: "delay", icon: "⏱️", stereo: true, ports: [{ sym: "l_delay", name: "Left Delay (ms)", min: 1, max: 2000, def: 350 }, { sym: "r_delay", name: "Right Delay (ms)", min: 1, max: 2000, def: 500 }, { sym: "level", name: "Wet Level", min: 0, max: 100, def: 35 }] },
        { uri: "http://distrho.sf.net/plugins/MVerb", name: "MVerb Studio Plate Reverb", category: "Reverb / Space", catKey: "reverb", icon: "🌌", stereo: true, ports: [{ sym: "size", name: "Size", min: 0, max: 100, def: 75 }, { sym: "decay", name: "Decay", min: 0, max: 100, def: 55 }, { sym: "damping", name: "Damping", min: 0, max: 100, def: 40 }, { sym: "mix", name: "Mix", min: 0, max: 100, def: 35 }] },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_chorus_stereo#_chorus_stereo", name: "Gx Stereo Dimension Chorus", category: "Modulation / Chorus", catKey: "mod", icon: "🌀", stereo: true, ports: [{ sym: "freq", name: "Rate (Hz)", min: 0.1, max: 10, def: 0.8 }, { sym: "depth", name: "Depth", min: 0, max: 1, def: 0.45 }, { sym: "level", name: "Mix", min: 0, max: 1, def: 0.5 }] },
        { uri: "http://guitarix.sourceforge.net/plugins/gx_phaser#_phaser", name: "Gx Vintage Analog Phaser", category: "Modulation / Chorus", catKey: "mod", icon: "🪐", stereo: false, ports: [{ sym: "speed", name: "Rate (Hz)", min: 0.05, max: 10, def: 0.5 }, { sym: "depth", name: "Depth", min: 0, max: 1, def: 0.65 }, { sym: "feedback", name: "Feedback", min: 0, max: 1, def: 0.45 }] },
        { uri: "http://aidadsp.cc/plugins/aidadsp-bundle/rt-neural-generic", name: "AIDA-X Neural Amp Player", category: "Amps & Cab", catKey: "amp", icon: "🎸", stereo: false, ports: [{ sym: "IN_LPF", name: "In LPF (%)", min: 0, max: 100, def: 66.2 }, { sym: "IN_LEVEL", name: "Input (dB)", min: -12, max: 12, def: 0 }, { sym: "BASS", name: "Bass (dB)", min: -8, max: 8, def: 0 }, { sym: "MID", name: "Mid (dB)", min: -8, max: 8, def: 0 }, { sym: "TREBLE", name: "Treble (dB)", min: -8, max: 8, def: 0 }, { sym: "OUT_LEVEL", name: "Output (dB)", min: -12, max: 12, def: 0 }] },
        { uri: "http://github.com/mikeoliphant/neural-amp-modeler-lv2", name: "Neural Amp Modeler (NAM)", category: "Amps & Cab", catKey: "amp", icon: "🎸", stereo: false, ports: [
            { sym: "input_level", name: "Input Level (dB)", min: -20, max: 20, def: 0 },
            { sym: "output_level", name: "Output Level (dB)", min: -20, max: 20, def: 0 },
            { sym: "noise_gate", name: "Noise Gate", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_spectral", name: "De-Hiss", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_defizz", name: "De-Fizz", min: 0, max: 1, def: 1, toggled: true },
            { sym: "noise_sens", name: "Gate Sensitivity", min: 0, max: 10, def: 5 },
            { sym: "quality_scale", name: "Quality Scale", min: 0, max: 1, def: 1 }
        ] }
    ];

    const PICKER_CATEGORIES = [
        { key: "all",    label: "🌐 ALL FX" },
        { key: "cyber",  label: "⭐ MY CYBER FX" },
        { key: "amp",    label: "🎸 AMPS & NAM" },
        { key: "drive",  label: "🔥 DRIVE / DIST" },
        { key: "dyn",    label: "🗜️ DYNAMICS" },
        { key: "eq",     label: "🎛️ EQ / FILTER" },
        { key: "delay",  label: "📼 DELAY / ECHO" },
        { key: "reverb", label: "🏛️ REVERB / SPACE" },
        { key: "mod",    label: "🌀 MOD & SYNTH" }
    ];

    window.channelDualDspState = window.channelDualDspState || {};
    window.channelLv2Slots = window.channelLv2Slots || {};
    let activeSlotsPageChannel = 0;
    let activePickerChannel = 0, activePickerSlot = 0, activePickerCategory = "all", activePickerQuery = "";
    let activeModGuiChannel = 0, activeModGuiSlot = 0, activeModGuiView = "modgui";

    function inferPluginCategoryKey(uri, name, rawCat) {
        const s = `${uri || ""} ${name || ""} ${rawCat || ""}`.toLowerCase();
        if (s.includes("fender") || s.includes("marshall") || s.includes("vox") || s.includes("dumble") ||
            s.includes("friedman") || s.includes("hiwatt") || s.includes("matchless") || s.includes("tworock") ||
            s.includes("toneking") || s.includes("trainwreck") || s.includes("orange") || s.includes("magnatone") ||
            s.includes("roland") || s.includes("rockman") || s.includes("koch") || s.includes("ycv") ||
            s.includes("nam") || s.includes("aida") || s.includes("simulator") || s.includes("amp") || s.includes("cab")) {
            return "amp";
        }
        if (s.includes("screamer") || s.includes("klon") || s.includes("drive") || s.includes("dist") ||
            s.includes("fuzz") || s.includes("guvnor") || s.includes("ds1") || s.includes("muff") || s.includes("boost")) {
            return "drive";
        }
        if (s.includes("reverb") || s.includes("spring") || s.includes("duskverb") || s.includes("mverb") ||
            s.includes("hall") || s.includes("plate") || s.includes("bloom") || s.includes("shimmer")) {
            return "reverb";
        }
        if (s.includes("delay") || s.includes("echo") || s.includes("tape")) {
            return "delay";
        }
        if (s.includes("comp") || s.includes("limit") || s.includes("gate") || s.includes("denoise") ||
            s.includes("deess") || s.includes("dynamics") || s.includes("expander")) {
            return "dyn";
        }
        if (s.includes("eq") || s.includes("filter") || s.includes("hum") || s.includes("thump") ||
            s.includes("feedback") || s.includes("equalizer") || s.includes("wah")) {
            return "eq";
        }
        return "mod";
    }

    function isSwitchLikePort(pt) {
        if (!pt) return false;
        if (pt.toggled) return true;
        const sym = (pt.sym || "").toLowerCase();
        const nm = (pt.name || "").toLowerCase();
        if (pt.integer && pt.min === 0 && pt.max === 1) return true;
        if (sym === ":bypass" || sym === "bypass" || sym === "bright" || sym === "deep" || sym === "fat" ||
            sym === "boost" || sym === "pab" || sym === "rock_jazz" || sym === "voice" || sym === "clip" ||
            sym.includes("switch") || sym.includes("toggle") || nm.includes("switch") || nm.includes("bright") ||
            nm.includes("boost")) {
            return (pt.max - pt.min) <= 2;
        }
        return false;
    }

    function mapModepPorts(item) {
        if (!item || !item.ports || !item.ports.control || !Array.isArray(item.ports.control.input)) return null;
        return item.ports.control.input
            .filter(pt => !Array.isArray(pt.properties) || pt.properties.indexOf("notOnGUI") < 0)
            .map(pt => {
                const props = Array.isArray(pt.properties) ? pt.properties : [];
                const min = (pt.ranges && typeof pt.ranges.minimum === "number") ? pt.ranges.minimum : 0;
                const max = (pt.ranges && typeof pt.ranges.maximum === "number") ? pt.ranges.maximum : 10;
                const def = (pt.ranges && typeof pt.ranges.default === "number") ? pt.ranges.default : ((min + max) * 0.5);
                const portObj = {
                    sym: pt.symbol || pt.shortName || "param",
                    name: pt.name || pt.shortName || pt.symbol || "Param",
                    min: min,
                    max: max,
                    def: def,
                    unit: (pt.units && pt.units.symbol) ? pt.units.symbol : "",
                    toggled: props.indexOf("toggled") >= 0,
                    integer: props.indexOf("integer") >= 0,
                    enumeration: props.indexOf("enumeration") >= 0,
                    scalePoints: Array.isArray(pt.scalePoints) ? pt.scalePoints : []
                };
                if (isSwitchLikePort(portObj)) portObj.toggled = true;
                return portObj;
            });
    }

    // Always use Port :8080 (modep-web-portal.py reverse-proxies /effect/*, /resources/*, and /mixer/modgui-host.html with CORS!)
    function getModepHttpBase() {
        const h = (typeof window.getActivePiHost === "function") ? window.getActivePiHost() : (window.location.hostname || "192.168.1.148");
        if (window.location.protocol === "file:" || !window.location.port || window.location.port !== "8080") {
            return `http://${h}:8080`;
        }
        return "";
    }
    window.getModepHttpBase = getModepHttpBase;

        window.MODEP_USER_FILES = {
        nam: [{"name": "M160 MIDDLE - T 4, I 2, M 5.nam", "label": "M160 MIDDLE - T 4, I 2, M 5", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/M160 MIDDLE - T 4, I 2, M 5.nam"}, {"name": "M160 MIDDLE - T 5, I 3, M 9.nam", "label": "M160 MIDDLE - T 5, I 3, M 9", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/M160 MIDDLE - T 5, I 3, M 9.nam"}, {"name": "M160 MIDDLE - T 6, I 11, M 12.nam", "label": "M160 MIDDLE - T 6, I 11, M 12", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/M160 MIDDLE - T 6, I 11, M 12.nam"}, {"name": "MD 421 CAP_CENTER - T 6, I 11, M 12.nam", "label": "MD 421 CAP_CENTER - T 6, I 11, M 12", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/MD 421 CAP_CENTER - T 6, I 11, M 12.nam"}, {"name": "MD 421 EDGE - T 6, I 11, M 12.nam", "label": "MD 421 EDGE - T 6, I 11, M 12", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/MD 421 EDGE - T 6, I 11, M 12.nam"}, {"name": "MD 421 MIDDLE - T 6, I 11, M 12.nam", "label": "MD 421 MIDDLE - T 6, I 11, M 12", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/MD 421 MIDDLE - T 6, I 11, M 12.nam"}, {"name": "MD 421 OFF AXIS + 45 DEGREES - T 6, I 11, M 12.nam", "label": "MD 421 OFF AXIS + 45 DEGREES - T 6, I 11, M 12", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/MD 421 OFF AXIS + 45 DEGREES - T 6, I 11, M 12.nam"}, {"name": "SM57 CAP EDGE - T 4, I 2, M 5.nam", "label": "SM57 CAP EDGE - T 4, I 2, M 5", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/SM57 CAP EDGE - T 4, I 2, M 5.nam"}, {"name": "SM57 CAP EDGE - T 5, I 3, M 9.nam", "label": "SM57 CAP EDGE - T 5, I 3, M 9", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/SM57 CAP EDGE - T 5, I 3, M 9.nam"}, {"name": "SM57 CAP EDGE - T 6, I 11, M 12.nam", "label": "SM57 CAP EDGE - T 6, I 11, M 12", "category": "1960 Fender Tweed Deluxe 5E3", "path": "/var/modep/user-files/NAM Models/1960 Fender Tweed Deluxe 5E3/SM57 CAP EDGE - T 6, I 11, M 12.nam"}, {"name": "64 Fender VibroVerb N Bright CLN.nam", "label": "64 Fender VibroVerb N Bright CLN", "category": "1964 Fender VibroVerb AA763", "path": "/var/modep/user-files/NAM Models/1964 Fender VibroVerb AA763/64 Fender VibroVerb N Bright CLN.nam"}, {"name": "64 Fender VibroVerb V OD.nam", "label": "64 Fender VibroVerb V OD", "category": "1964 Fender VibroVerb AA763", "path": "/var/modep/user-files/NAM Models/1964 Fender VibroVerb AA763/64 Fender VibroVerb V OD.nam"}, {"name": "MRSH JT45RS I Crunch BAL2 CAB.nam", "label": "MRSH JT45RS I Crunch BAL2 CAB", "category": "1964 Marshall JTM-45 Block Logo Radiospares OT - CRUNCH - A2", "path": "/var/modep/user-files/NAM Models/1964 Marshall JTM-45 Block Logo Radiospares OT - CRUNCH - A2/MRSH JT45RS I Crunch BAL2 CAB.nam"}, {"name": "MAGN 10M I Crunch2 BAL CAB.nam", "label": "MAGN 10M I Crunch2 BAL CAB", "category": "1966 Magnatone M10A Crunch - A2", "path": "/var/modep/user-files/NAM Models/1966 Magnatone M10A Crunch - A2/MAGN 10M I Crunch2 BAL CAB.nam"}, {"name": "BNO JT45DR I Crunch BAL CAB3.nam", "label": "BNO JT45DR I Crunch BAL CAB3", "category": "1966 Marshall 1962 Bluesbreaker (JTM45) with G12 Alnico - CRUNCH - A2", "path": "/var/modep/user-files/NAM Models/1966 Marshall 1962 Bluesbreaker (JTM45) with G12 Alnico - CRUNCH - A2/BNO JT45DR I Crunch BAL CAB3.nam"}, {"name": "Fender - Princeton Clean 3 M160.nam", "label": "Fender - Princeton Clean 3 M160", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton Clean 3 M160.nam"}, {"name": "Fender - Princeton Clean 3 SM57.nam", "label": "Fender - Princeton Clean 3 SM57", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton Clean 3 SM57.nam"}, {"name": "Fender - Princeton Clean 3 SUM M160 + SM57.nam", "label": "Fender - Princeton Clean 3 SUM M160 + SM57", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton Clean 3 SUM M160 + SM57.nam"}, {"name": "Fender - Princeton Crunch 7 SUM M160 + SM57.nam", "label": "Fender - Princeton Crunch 7 SUM M160 + SM57", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton Crunch 7 SUM M160 + SM57.nam"}, {"name": "Fender - Princeton Crunch Vol 7 M160.nam", "label": "Fender - Princeton Crunch Vol 7 M160", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton Crunch Vol 7 M160.nam"}, {"name": "Fender - Princeton Crunch Vol 7 SM57.nam", "label": "Fender - Princeton Crunch Vol 7 SM57", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton Crunch Vol 7 SM57.nam"}, {"name": "Fender - Princeton EOB Vol 5 M160.nam", "label": "Fender - Princeton EOB Vol 5 M160", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton EOB Vol 5 M160.nam"}, {"name": "Fender - Princeton EOB Vol 5 SM57.nam", "label": "Fender - Princeton EOB Vol 5 SM57", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton EOB Vol 5 SM57.nam"}, {"name": "Fender - Princeton EOB Vol 5 SUM M160 + SM57.nam", "label": "Fender - Princeton EOB Vol 5 SUM M160 + SM57", "category": "65 Fender Princeton with Various Mics", "path": "/var/modep/user-files/NAM Models/65 Fender Princeton with Various Mics/Fender - Princeton EOB Vol 5 SUM M160 + SM57.nam"}, {"name": "Clapton's Bassman - Marked Settings (Jumpered).nam", "label": "Clapton's Bassman - Marked Settings (Jumpered)", "category": "Eric Clapton's Fender Bassman 5F6-A", "path": "/var/modep/user-files/NAM Models/Eric Clapton's Fender Bassman 5F6-A/Clapton's Bassman - Marked Settings (Jumpered).nam"}, {"name": "FNDR BFDRI VB Clean BAL2 CAB.nam", "label": "FNDR BFDRI VB Clean BAL2 CAB", "category": "Fender Deluxe Reverb Reissue Iconic Clean A2 (1)", "path": "/var/modep/user-files/NAM Models/Fender Deluxe Reverb Reissue Iconic Clean A2 (1)/FNDR BFDRI VB Clean BAL2 CAB.nam"}, {"name": "FNDR PANO Clean2 BAL 6V6 CAB.nam", "label": "FNDR PANO Clean2 BAL 6V6 CAB", "category": "Fender Pano-Verb", "path": "/var/modep/user-files/NAM Models/Fender Pano-Verb/FNDR PANO Clean2 BAL 6V6 CAB.nam"}, {"name": "FNDR BFPRO VB Clean2 WRM CAB.nam", "label": "FNDR BFPRO VB Clean2 WRM CAB", "category": "Fender Pro Reverb 1967", "path": "/var/modep/user-files/NAM Models/Fender Pro Reverb 1967/FNDR BFPRO VB Clean2 WRM CAB.nam"}, {"name": "Fender Super Reverb_ EQ Flat, Volume 3, AKG 414.nam", "label": "Fender Super Reverb_ EQ Flat, Volume 3, AKG 414", "category": "Fender Super Reverb 1977", "path": "/var/modep/user-files/NAM Models/Fender Super Reverb 1977/Fender Super Reverb_ EQ Flat, Volume 3, AKG 414.nam"}, {"name": "Fender Super Reverb_ EQ Flat, Volume 3, sm57.nam", "label": "Fender Super Reverb_ EQ Flat, Volume 3, sm57", "category": "Fender Super Reverb 1977", "path": "/var/modep/user-files/NAM Models/Fender Super Reverb 1977/Fender Super Reverb_ EQ Flat, Volume 3, sm57.nam"}, {"name": "Fender Super Reverb_ EQ Flat, Volume 3, sm57 and AKG 414.nam", "label": "Fender Super Reverb_ EQ Flat, Volume 3, sm57 and AKG 414", "category": "Fender Super Reverb 1977", "path": "/var/modep/user-files/NAM Models/Fender Super Reverb 1977/Fender Super Reverb_ EQ Flat, Volume 3, sm57 and AKG 414.nam"}, {"name": "64 Fender VibroVerb N Bright CLN.nam", "label": "64 Fender VibroVerb N Bright CLN", "category": "General", "path": "/var/modep/user-files/NAM Models/64 Fender VibroVerb N Bright CLN.nam"}, {"name": "64 Fender VibroVerb V OD.nam", "label": "64 Fender VibroVerb V OD", "category": "General", "path": "/var/modep/user-files/NAM Models/64 Fender VibroVerb V OD.nam"}, {"name": "BNO JT45DR I Crunch BAL CAB3.nam", "label": "BNO JT45DR I Crunch BAL CAB3", "category": "General", "path": "/var/modep/user-files/NAM Models/BNO JT45DR I Crunch BAL CAB3.nam"}, {"name": "Clapton's Bassman - Marked Settings (Jumpered).nam", "label": "Clapton's Bassman - Marked Settings (Jumpered)", "category": "General", "path": "/var/modep/user-files/NAM Models/Clapton's Bassman - Marked Settings (Jumpered).nam"}, {"name": "FNDR BFDRI VB Clean BAL2 CAB.nam", "label": "FNDR BFDRI VB Clean BAL2 CAB", "category": "General", "path": "/var/modep/user-files/NAM Models/FNDR BFDRI VB Clean BAL2 CAB.nam"}, {"name": "FNDR BFPRO VB Clean2 WRM CAB.nam", "label": "FNDR BFPRO VB Clean2 WRM CAB", "category": "General", "path": "/var/modep/user-files/NAM Models/FNDR BFPRO VB Clean2 WRM CAB.nam"}, {"name": "FNDR PANO Clean2 BAL 6V6 CAB.nam", "label": "FNDR PANO Clean2 BAL 6V6 CAB", "category": "General", "path": "/var/modep/user-files/NAM Models/FNDR PANO Clean2 BAL 6V6 CAB.nam"}, {"name": "Fender - Princeton Clean 3 M160.nam", "label": "Fender - Princeton Clean 3 M160", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton Clean 3 M160.nam"}, {"name": "Fender - Princeton Clean 3 SM57.nam", "label": "Fender - Princeton Clean 3 SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton Clean 3 SM57.nam"}, {"name": "Fender - Princeton Clean 3 SUM M160 + SM57.nam", "label": "Fender - Princeton Clean 3 SUM M160 + SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton Clean 3 SUM M160 + SM57.nam"}, {"name": "Fender - Princeton Crunch 7 SUM M160 + SM57.nam", "label": "Fender - Princeton Crunch 7 SUM M160 + SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton Crunch 7 SUM M160 + SM57.nam"}, {"name": "Fender - Princeton Crunch Vol 7 M160.nam", "label": "Fender - Princeton Crunch Vol 7 M160", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton Crunch Vol 7 M160.nam"}, {"name": "Fender - Princeton Crunch Vol 7 SM57.nam", "label": "Fender - Princeton Crunch Vol 7 SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton Crunch Vol 7 SM57.nam"}, {"name": "Fender - Princeton EOB Vol 5 M160.nam", "label": "Fender - Princeton EOB Vol 5 M160", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton EOB Vol 5 M160.nam"}, {"name": "Fender - Princeton EOB Vol 5 SM57.nam", "label": "Fender - Princeton EOB Vol 5 SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton EOB Vol 5 SM57.nam"}, {"name": "Fender - Princeton EOB Vol 5 SUM M160 + SM57.nam", "label": "Fender - Princeton EOB Vol 5 SUM M160 + SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender - Princeton EOB Vol 5 SUM M160 + SM57.nam"}, {"name": "Fender Super Reverb_ EQ Flat, Volume 3, AKG 414.nam", "label": "Fender Super Reverb_ EQ Flat, Volume 3, AKG 414", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender Super Reverb_ EQ Flat, Volume 3, AKG 414.nam"}, {"name": "Fender Super Reverb_ EQ Flat, Volume 3, sm57.nam", "label": "Fender Super Reverb_ EQ Flat, Volume 3, sm57", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender Super Reverb_ EQ Flat, Volume 3, sm57.nam"}, {"name": "Fender Super Reverb_ EQ Flat, Volume 3, sm57 and AKG 414.nam", "label": "Fender Super Reverb_ EQ Flat, Volume 3, sm57 and AKG 414", "category": "General", "path": "/var/modep/user-files/NAM Models/Fender Super Reverb_ EQ Flat, Volume 3, sm57 and AKG 414.nam"}, {"name": "GBSN G19 IA Crunch BAL CAB.nam", "label": "GBSN G19 IA Crunch BAL CAB", "category": "General", "path": "/var/modep/user-files/NAM Models/GBSN G19 IA Crunch BAL CAB.nam"}, {"name": "George B Ceriatone King Kong  KK Ch2 60s solo 80s.nam", "label": "George B Ceriatone King Kong  KK Ch2 60s solo 80s", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong  KK Ch2 60s solo 80s.nam"}, {"name": "George B Ceriatone King Kong  chan2 60s br sw2 L.nam", "label": "George B Ceriatone King Kong  chan2 60s br sw2 L", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong  chan2 60s br sw2 L.nam"}, {"name": "George B Ceriatone King Kong Channel 1 60s mode.nam", "label": "George B Ceriatone King Kong Channel 1 60s mode", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong Channel 1 60s mode.nam"}, {"name": "George B Ceriatone King Kong Channel 2 80s mode both br sw R.nam", "label": "George B Ceriatone King Kong Channel 2 80s mode both br sw R", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong Channel 2 80s mode both br sw R.nam"}, {"name": "George B Ceriatone King Kong NAM Capture Bogner Blue Chase Tone Secret Pre Deco.nam", "label": "George B Ceriatone King Kong NAM Capture Bogner Blue Chase Tone Secret Pre Deco", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong NAM Capture Bogner Blue Chase Tone Secret Pre Deco.nam"}, {"name": "George B Ceriatone King Kong NAM Capture Boosted peace keeper Chase Tone Secret Pre Deco v2.nam", "label": "George B Ceriatone King Kong NAM Capture Boosted peace keeper Chase Tone Secret Pre Deco v2", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong NAM Capture Boosted peace keeper Chase Tone Secret Pre Deco v2.nam"}, {"name": "George B Ceriatone King Kong NAM Capture Broadcast Chase Tone Secret Pre Deco.nam", "label": "George B Ceriatone King Kong NAM Capture Broadcast Chase Tone Secret Pre Deco", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong NAM Capture Broadcast Chase Tone Secret Pre Deco.nam"}, {"name": "George B Ceriatone King Kong NAM Capture Protein Chase Tone Secret Pre Deco .nam", "label": "George B Ceriatone King Kong NAM Capture Protein Chase Tone Secret Pre Deco ", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong NAM Capture Protein Chase Tone Secret Pre Deco .nam"}, {"name": "George B Ceriatone King Kong NAM Capture Turbo Nonna Chase Tone Secret Pre Deco.nam", "label": "George B Ceriatone King Kong NAM Capture Turbo Nonna Chase Tone Secret Pre Deco", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong NAM Capture Turbo Nonna Chase Tone Secret Pre Deco.nam"}, {"name": "George B Ceriatone King Kong chan2 70s both br sw L.nam", "label": "George B Ceriatone King Kong chan2 70s both br sw L", "category": "General", "path": "/var/modep/user-files/NAM Models/George B Ceriatone King Kong chan2 70s both br sw L.nam"}, {"name": "George B V4 Countess 300eps.nam", "label": "George B V4 Countess 300eps", "category": "General", "path": "/var/modep/user-files/NAM Models/George B V4 Countess 300eps.nam"}, {"name": "Helga B 5150 BlockLetter - Boosted.nam", "label": "Helga B 5150 BlockLetter - Boosted", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 5150 BlockLetter - Boosted.nam"}, {"name": "Helga B 5150 BlockLetter - NoBoost.nam", "label": "Helga B 5150 BlockLetter - NoBoost", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 5150 BlockLetter - NoBoost.nam"}, {"name": "Helga B 6505+ Green ch - MXR Drive.nam", "label": "Helga B 6505+ Green ch - MXR Drive", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6505+ Green ch - MXR Drive.nam"}, {"name": "Helga B 6505+ Green ch - NoBoost.nam", "label": "Helga B 6505+ Green ch - NoBoost", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6505+ Green ch - NoBoost.nam"}, {"name": "Helga B 6505+ Red Ch - NoBoost rock.nam", "label": "Helga B 6505+ Red Ch - NoBoost rock", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6505+ Red Ch - NoBoost rock.nam"}, {"name": "Helga B 6505+ Red ch - MXR Drive.nam", "label": "Helga B 6505+ Red ch - MXR Drive", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6505+ Red ch - MXR Drive.nam"}, {"name": "Helga B 6505+ Red ch - MXR Drive V2.nam", "label": "Helga B 6505+ Red ch - MXR Drive V2", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6505+ Red ch - MXR Drive V2.nam"}, {"name": "Helga B 6505+ Red ch - NoBoost.nam", "label": "Helga B 6505+ Red ch - NoBoost", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6505+ Red ch - NoBoost.nam"}, {"name": "Helga B 6534+ MXR M77.nam", "label": "Helga B 6534+ MXR M77", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6534+ MXR M77.nam"}, {"name": "Helga B 6534+ MXR M77 - Helga Behrens.nam", "label": "Helga B 6534+ MXR M77 - Helga Behrens", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6534+ MXR M77 - Helga Behrens.nam"}, {"name": "Helga B 6534+ No Boost.nam", "label": "Helga B 6534+ No Boost", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6534+ No Boost.nam"}, {"name": "Helga B 6534+ OD808.nam", "label": "Helga B 6534+ OD808", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B 6534+ OD808.nam"}, {"name": "Helga B Boss OS-2.nam", "label": "Helga B Boss OS-2", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B Boss OS-2.nam"}, {"name": "Helga B GCI Jugendstil.nam", "label": "Helga B GCI Jugendstil", "category": "General", "path": "/var/modep/user-files/NAM Models/Helga B GCI Jugendstil.nam"}, {"name": "M160 MIDDLE - T 4, I 2, M 5.nam", "label": "M160 MIDDLE - T 4, I 2, M 5", "category": "General", "path": "/var/modep/user-files/NAM Models/M160 MIDDLE - T 4, I 2, M 5.nam"}, {"name": "M160 MIDDLE - T 5, I 3, M 9.nam", "label": "M160 MIDDLE - T 5, I 3, M 9", "category": "General", "path": "/var/modep/user-files/NAM Models/M160 MIDDLE - T 5, I 3, M 9.nam"}, {"name": "M160 MIDDLE - T 6, I 11, M 12.nam", "label": "M160 MIDDLE - T 6, I 11, M 12", "category": "General", "path": "/var/modep/user-files/NAM Models/M160 MIDDLE - T 6, I 11, M 12.nam"}, {"name": "MAGN 10M I Crunch2 BAL CAB.nam", "label": "MAGN 10M I Crunch2 BAL CAB", "category": "General", "path": "/var/modep/user-files/NAM Models/MAGN 10M I Crunch2 BAL CAB.nam"}, {"name": "MD 421 CAP_CENTER - T 6, I 11, M 12.nam", "label": "MD 421 CAP_CENTER - T 6, I 11, M 12", "category": "General", "path": "/var/modep/user-files/NAM Models/MD 421 CAP_CENTER - T 6, I 11, M 12.nam"}, {"name": "MD 421 EDGE - T 6, I 11, M 12.nam", "label": "MD 421 EDGE - T 6, I 11, M 12", "category": "General", "path": "/var/modep/user-files/NAM Models/MD 421 EDGE - T 6, I 11, M 12.nam"}, {"name": "MD 421 MIDDLE - T 6, I 11, M 12.nam", "label": "MD 421 MIDDLE - T 6, I 11, M 12", "category": "General", "path": "/var/modep/user-files/NAM Models/MD 421 MIDDLE - T 6, I 11, M 12.nam"}, {"name": "MD 421 OFF AXIS + 45 DEGREES - T 6, I 11, M 12.nam", "label": "MD 421 OFF AXIS + 45 DEGREES - T 6, I 11, M 12", "category": "General", "path": "/var/modep/user-files/NAM Models/MD 421 OFF AXIS + 45 DEGREES - T 6, I 11, M 12.nam"}, {"name": "MRSH JT45RS I Crunch BAL2 CAB.nam", "label": "MRSH JT45RS I Crunch BAL2 CAB", "category": "General", "path": "/var/modep/user-files/NAM Models/MRSH JT45RS I Crunch BAL2 CAB.nam"}, {"name": "RR Twin65 BRT 160.nam", "label": "RR Twin65 BRT 160", "category": "General", "path": "/var/modep/user-files/NAM Models/RR Twin65 BRT 160.nam"}, {"name": "RR Twin65 NRM 57.nam", "label": "RR Twin65 NRM 57", "category": "General", "path": "/var/modep/user-files/NAM Models/RR Twin65 NRM 57.nam"}, {"name": "RR Twin65 NRM 87.nam", "label": "RR Twin65 NRM 87", "category": "General", "path": "/var/modep/user-files/NAM Models/RR Twin65 NRM 87.nam"}, {"name": "SM57 CAP EDGE - T 4, I 2, M 5.nam", "label": "SM57 CAP EDGE - T 4, I 2, M 5", "category": "General", "path": "/var/modep/user-files/NAM Models/SM57 CAP EDGE - T 4, I 2, M 5.nam"}, {"name": "SM57 CAP EDGE - T 5, I 3, M 9.nam", "label": "SM57 CAP EDGE - T 5, I 3, M 9", "category": "General", "path": "/var/modep/user-files/NAM Models/SM57 CAP EDGE - T 5, I 3, M 9.nam"}, {"name": "SM57 CAP EDGE - T 6, I 11, M 12.nam", "label": "SM57 CAP EDGE - T 6, I 11, M 12", "category": "General", "path": "/var/modep/user-files/NAM Models/SM57 CAP EDGE - T 6, I 11, M 12.nam"}, {"name": "Silvertone Model 1484 - V10 - Mic Sum.nam", "label": "Silvertone Model 1484 - V10 - Mic Sum", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V10 - Mic Sum.nam"}, {"name": "Silvertone Model 1484 - V10 - R121.nam", "label": "Silvertone Model 1484 - V10 - R121", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V10 - R121.nam"}, {"name": "Silvertone Model 1484 - V10 - SM57.nam", "label": "Silvertone Model 1484 - V10 - SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V10 - SM57.nam"}, {"name": "Silvertone Model 1484 - V4 - Mic Sum.nam", "label": "Silvertone Model 1484 - V4 - Mic Sum", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V4 - Mic Sum.nam"}, {"name": "Silvertone Model 1484 - V4 - R121.nam", "label": "Silvertone Model 1484 - V4 - R121", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V4 - R121.nam"}, {"name": "Silvertone Model 1484 - V4 - SM57.nam", "label": "Silvertone Model 1484 - V4 - SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V4 - SM57.nam"}, {"name": "Silvertone Model 1484 - V6 - Mic Sum.nam", "label": "Silvertone Model 1484 - V6 - Mic Sum", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V6 - Mic Sum.nam"}, {"name": "Silvertone Model 1484 - V6 - R121.nam", "label": "Silvertone Model 1484 - V6 - R121", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V6 - R121.nam"}, {"name": "Silvertone Model 1484 - V6 - SM57.nam", "label": "Silvertone Model 1484 - V6 - SM57", "category": "General", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 - V6 - SM57.nam"}, {"name": "TK ROY 67 Drive BAL TMY CAB.nam", "label": "TK ROY 67 Drive BAL TMY CAB", "category": "General", "path": "/var/modep/user-files/NAM Models/TK ROY 67 Drive BAL TMY CAB.nam"}, {"name": "bassman crunch.nam", "label": "bassman crunch", "category": "General", "path": "/var/modep/user-files/NAM Models/bassman crunch.nam"}, {"name": "bassman full bore.nam", "label": "bassman full bore", "category": "General", "path": "/var/modep/user-files/NAM Models/bassman full bore.nam"}, {"name": "bassman full bore v2 400epochs.nam", "label": "bassman full bore v2 400epochs", "category": "General", "path": "/var/modep/user-files/NAM Models/bassman full bore v2 400epochs.nam"}, {"name": "clean prefered setting.nam", "label": "clean prefered setting", "category": "General", "path": "/var/modep/user-files/NAM Models/clean prefered setting.nam"}, {"name": "clean prefered stting baked in eq.nam", "label": "clean prefered stting baked in eq", "category": "General", "path": "/var/modep/user-files/NAM Models/clean prefered stting baked in eq.nam"}, {"name": "mandolin ezac colab.nam", "label": "mandolin ezac colab", "category": "General", "path": "/var/modep/user-files/NAM Models/mandolin ezac colab.nam"}, {"name": "marshall dsl edge of crunch 1000epochs.nam", "label": "marshall dsl edge of crunch 1000epochs", "category": "General", "path": "/var/modep/user-files/NAM Models/marshall dsl edge of crunch 1000epochs.nam"}, {"name": "marshall dsl20 clean 1000epochs.nam", "label": "marshall dsl20 clean 1000epochs", "category": "General", "path": "/var/modep/user-files/NAM Models/marshall dsl20 clean 1000epochs.nam"}, {"name": "marshall dsl20 ultra gain rock monster 1000epochs.nam", "label": "marshall dsl20 ultra gain rock monster 1000epochs", "category": "General", "path": "/var/modep/user-files/NAM Models/marshall dsl20 ultra gain rock monster 1000epochs.nam"}, {"name": "martin d45 -1.nam", "label": "martin d45 -1", "category": "General", "path": "/var/modep/user-files/NAM Models/martin d45 -1.nam"}, {"name": "martin d45 colab.nam", "label": "martin d45 colab", "category": "General", "path": "/var/modep/user-files/NAM Models/martin d45 colab.nam"}, {"name": "most fender blackface baked eq.nam", "label": "most fender blackface baked eq", "category": "General", "path": "/var/modep/user-files/NAM Models/most fender blackface baked eq.nam"}, {"name": "most fender with expremnetal opamp21 and comp added.nam", "label": "most fender with expremnetal opamp21 and comp added", "category": "General", "path": "/var/modep/user-files/NAM Models/most fender with expremnetal opamp21 and comp added.nam"}, {"name": "most fenderish blackface.nam", "label": "most fenderish blackface", "category": "General", "path": "/var/modep/user-files/NAM Models/most fenderish blackface.nam"}, {"name": "most fenderish blackface expriment with dimention D.nam", "label": "most fenderish blackface expriment with dimention D", "category": "General", "path": "/var/modep/user-files/NAM Models/most fenderish blackface expriment with dimention D.nam"}, {"name": "GBSN G19 IA Crunch BAL CAB.nam", "label": "GBSN G19 IA Crunch BAL CAB", "category": "Gibson GA-19 RVT Falcon 1962 - CRUNCH - A2", "path": "/var/modep/user-files/NAM Models/Gibson GA-19 RVT Falcon 1962 - CRUNCH - A2/GBSN G19 IA Crunch BAL CAB.nam"}, {"name": "marshall dsl edge of crunch 1000epochs.nam", "label": "marshall dsl edge of crunch 1000epochs", "category": "MARSHALL DSL20 PACK", "path": "/var/modep/user-files/NAM Models/MARSHALL DSL20 PACK/marshall dsl edge of crunch 1000epochs.nam"}, {"name": "marshall dsl20 clean 1000epochs.nam", "label": "marshall dsl20 clean 1000epochs", "category": "MARSHALL DSL20 PACK", "path": "/var/modep/user-files/NAM Models/MARSHALL DSL20 PACK/marshall dsl20 clean 1000epochs.nam"}, {"name": "marshall dsl20 ultra gain rock monster 1000epochs.nam", "label": "marshall dsl20 ultra gain rock monster 1000epochs", "category": "MARSHALL DSL20 PACK", "path": "/var/modep/user-files/NAM Models/MARSHALL DSL20 PACK/marshall dsl20 ultra gain rock monster 1000epochs.nam"}, {"name": "RR Twin65 BRT 160.nam", "label": "RR Twin65 BRT 160", "category": "RR Twin65", "path": "/var/modep/user-files/NAM Models/RR Twin65/RR Twin65 BRT 160.nam"}, {"name": "RR Twin65 NRM 57.nam", "label": "RR Twin65 NRM 57", "category": "RR Twin65", "path": "/var/modep/user-files/NAM Models/RR Twin65/RR Twin65 NRM 57.nam"}, {"name": "RR Twin65 NRM 87.nam", "label": "RR Twin65 NRM 87", "category": "RR Twin65", "path": "/var/modep/user-files/NAM Models/RR Twin65/RR Twin65 NRM 87.nam"}, {"name": "Silvertone Model 1484 - V10 - Mic Sum.nam", "label": "Silvertone Model 1484 - V10 - Mic Sum", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V10 - Mic Sum.nam"}, {"name": "Silvertone Model 1484 - V10 - R121.nam", "label": "Silvertone Model 1484 - V10 - R121", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V10 - R121.nam"}, {"name": "Silvertone Model 1484 - V10 - SM57.nam", "label": "Silvertone Model 1484 - V10 - SM57", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V10 - SM57.nam"}, {"name": "Silvertone Model 1484 - V4 - Mic Sum.nam", "label": "Silvertone Model 1484 - V4 - Mic Sum", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V4 - Mic Sum.nam"}, {"name": "Silvertone Model 1484 - V4 - R121.nam", "label": "Silvertone Model 1484 - V4 - R121", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V4 - R121.nam"}, {"name": "Silvertone Model 1484 - V4 - SM57.nam", "label": "Silvertone Model 1484 - V4 - SM57", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V4 - SM57.nam"}, {"name": "Silvertone Model 1484 - V6 - Mic Sum.nam", "label": "Silvertone Model 1484 - V6 - Mic Sum", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V6 - Mic Sum.nam"}, {"name": "Silvertone Model 1484 - V6 - R121.nam", "label": "Silvertone Model 1484 - V6 - R121", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V6 - R121.nam"}, {"name": "Silvertone Model 1484 - V6 - SM57.nam", "label": "Silvertone Model 1484 - V6 - SM57", "category": "Silvertone Model 1484 Twin Twelve - Vintage Mid 60s", "path": "/var/modep/user-files/NAM Models/Silvertone Model 1484 Twin Twelve - Vintage Mid 60s/Silvertone Model 1484 - V6 - SM57.nam"}, {"name": "TK ROY 67 Drive BAL TMY CAB.nam", "label": "TK ROY 67 Drive BAL TMY CAB", "category": "Tone King Royalist MKIII Drive", "path": "/var/modep/user-files/NAM Models/Tone King Royalist MKIII Drive/TK ROY 67 Drive BAL TMY CAB.nam"}, {"name": "tw40_blues_deluxe_deerinkstudios.json", "label": "tw40_blues_deluxe_deerinkstudios", "category": "deer ink studios", "path": "/var/modep/lv2/rt-neural-generic.lv2/models/deer ink studios/tw40_blues_deluxe_deerinkstudios.json"}, {"name": "tw40_blues_solo_deerinkstudios.json", "label": "tw40_blues_solo_deerinkstudios", "category": "deer ink studios", "path": "/var/modep/lv2/rt-neural-generic.lv2/models/deer ink studios/tw40_blues_solo_deerinkstudios.json"}, {"name": "tw40_british_lead_deerinkstudios.json", "label": "tw40_british_lead_deerinkstudios", "category": "deer ink studios", "path": "/var/modep/lv2/rt-neural-generic.lv2/models/deer ink studios/tw40_british_lead_deerinkstudios.json"}, {"name": "tw40_british_rhythm_deerinkstudios.json", "label": "tw40_british_rhythm_deerinkstudios", "category": "deer ink studios", "path": "/var/modep/lv2/rt-neural-generic.lv2/models/deer ink studios/tw40_british_rhythm_deerinkstudios.json"}, {"name": "tw40_california_clean_deerinkstudios.json", "label": "tw40_california_clean_deerinkstudios", "category": "deer ink studios", "path": "/var/modep/lv2/rt-neural-generic.lv2/models/deer ink studios/tw40_california_clean_deerinkstudios.json"}, {"name": "tw40_california_crunch_deerinkstudios.json", "label": "tw40_california_crunch_deerinkstudios", "category": "deer ink studios", "path": "/var/modep/lv2/rt-neural-generic.lv2/models/deer ink studios/tw40_california_crunch_deerinkstudios.json"}, {"name": "patchstorage.json", "label": "patchstorage", "category": "neural_amp_modeler.lv2", "path": "/var/modep/lv2/neural_amp_modeler.lv2/patchstorage.json"}, {"name": "patchstorage.json", "label": "patchstorage", "category": "neural_amp_modeler.lv2.bak", "path": "/var/modep/lv2/neural_amp_modeler.lv2.bak/patchstorage.json"}],
        ir: [{"name": "Biomedical Sciences Courtyard UC San Diego California.wav", "label": "Biomedical Sciences Courtyard UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Biomedical Sciences Courtyard UC San Diego California.wav"}, {"name": "Carpenter Center Courtyard UC San Diego California.wav", "label": "Carpenter Center Courtyard UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Carpenter Center Courtyard UC San Diego California.wav"}, {"name": "Galbraith Hall Walkway UC San Diego California.wav", "label": "Galbraith Hall Walkway UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Galbraith Hall Walkway UC San Diego California.wav"}, {"name": "Geisel Library Pedestal UC San Diego California.wav", "label": "Geisel Library Pedestal UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Geisel Library Pedestal UC San Diego California.wav"}, {"name": "Graffiti Hallway UC San Diego California.wav", "label": "Graffiti Hallway UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Graffiti Hallway UC San Diego California.wav"}, {"name": "Humanities and Social Sciences Courtyard California.wav", "label": "Humanities and Social Sciences Courtyard California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Humanities and Social Sciences Courtyard California.wav"}, {"name": "Natural Sciences Courtyard UC San Diego California.wav", "label": "Natural Sciences Courtyard UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Natural Sciences Courtyard UC San Diego California.wav"}, {"name": "Pacific Hall Courtyard UC San Diego California.wav", "label": "Pacific Hall Courtyard UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Pacific Hall Courtyard UC San Diego California.wav"}, {"name": "Pepper Canyon Hall Courtyard UC San Diego California.wav", "label": "Pepper Canyon Hall Courtyard UC San Diego California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/Pepper Canyon Hall Courtyard UC San Diego California.wav"}, {"name": "San Diego Supercomputer Center Outdoor Patio California.wav", "label": "San Diego Supercomputer Center Outdoor Patio California", "category": "EchoThief/EchoThief/Brutalism", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Brutalism/San Diego Supercomputer Center Outdoor Patio California.wav"}, {"name": "Casa Grande Domes Arizona.wav", "label": "Casa Grande Domes Arizona", "category": "EchoThief/EchoThief/Domes", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Domes/Casa Grande Domes Arizona.wav"}, {"name": "Death Valley Charcoal Kilns California.wav", "label": "Death Valley Charcoal Kilns California", "category": "EchoThief/EchoThief/Domes", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Domes/Death Valley Charcoal Kilns California.wav"}, {"name": "Immaculata Dome University of San Diego California.wav", "label": "Immaculata Dome University of San Diego California", "category": "EchoThief/EchoThief/Domes", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Domes/Immaculata Dome University of San Diego California.wav"}, {"name": "Kroc Rotunda University of San Diego California.wav", "label": "Kroc Rotunda University of San Diego California", "category": "EchoThief/EchoThief/Domes", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Domes/Kroc Rotunda University of San Diego California.wav"}, {"name": "Naumburg Bandshell Central Park New York.wav", "label": "Naumburg Bandshell Central Park New York", "category": "EchoThief/EchoThief/Domes", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Domes/Naumburg Bandshell Central Park New York.wav"}, {"name": "Planetarium San Diego State University California.wav", "label": "Planetarium San Diego State University California", "category": "EchoThief/EchoThief/Domes", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Domes/Planetarium San Diego State University California.wav"}, {"name": "Square Victoria Dome Montreal Quebec.wav", "label": "Square Victoria Dome Montreal Quebec", "category": "EchoThief/EchoThief/Domes", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Domes/Square Victoria Dome Montreal Quebec.wav"}, {"name": "Academic Quadrangle Simon Fraser University Vancouver British Columbia.wav", "label": "Academic Quadrangle Simon Fraser University Vancouver British Columbia", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Academic Quadrangle Simon Fraser University Vancouver British Columbia.wav"}, {"name": "Amaranth Michael Hall Sculpture Bloomfield Hills Michigan.wav", "label": "Amaranth Michael Hall Sculpture Bloomfield Hills Michigan", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Amaranth Michael Hall Sculpture Bloomfield Hills Michigan.wav"}, {"name": "Bar Monsieur Ricard Montreal Quebec.wav", "label": "Bar Monsieur Ricard Montreal Quebec", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Bar Monsieur Ricard Montreal Quebec.wav"}, {"name": "Castillo de los Tres Reyes del Morro Arch Havana.wav", "label": "Castillo de los Tres Reyes del Morro Arch Havana", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Castillo de los Tres Reyes del Morro Arch Havana.wav"}, {"name": "Castillo de los Tres Reyes del Morro Courtyard Havana.wav", "label": "Castillo de los Tres Reyes del Morro Courtyard Havana", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Castillo de los Tres Reyes del Morro Courtyard Havana.wav"}, {"name": "Castillo de los Tres Reyes del Morro Havana.wav", "label": "Castillo de los Tres Reyes del Morro Havana", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Castillo de los Tres Reyes del Morro Havana.wav"}, {"name": "Castillo de los Tres Reyes del Morro Salas de Exposicion Havana.wav", "label": "Castillo de los Tres Reyes del Morro Salas de Exposicion Havana", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Castillo de los Tres Reyes del Morro Salas de Exposicion Havana.wav"}, {"name": "Cedar Creek Winery Cellar Wisconsin.wav", "label": "Cedar Creek Winery Cellar Wisconsin", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Cedar Creek Winery Cellar Wisconsin.wav"}, {"name": "Convocation Mall Simpn Fraser University Vancouver British Columbia.wav", "label": "Convocation Mall Simpn Fraser University Vancouver British Columbia", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Convocation Mall Simpn Fraser University Vancouver British Columbia.wav"}, {"name": "Digital Humanities Center San Diego State University California.wav", "label": "Digital Humanities Center San Diego State University California", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Digital Humanities Center San Diego State University California.wav"}, {"name": "Fortaleza de San Carlos de la Cabana Havana.wav", "label": "Fortaleza de San Carlos de la Cabana Havana", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Fortaleza de San Carlos de la Cabana Havana.wav"}, {"name": "Hartwell Tavern Minute Man National Historical Park Lincoln Massachusetts.wav", "label": "Hartwell Tavern Minute Man National Historical Park Lincoln Massachusetts", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Hartwell Tavern Minute Man National Historical Park Lincoln Massachusetts.wav"}, {"name": "Hawxhurst Mansion Ballroom Newport Rhode Island.wav", "label": "Hawxhurst Mansion Ballroom Newport Rhode Island", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Hawxhurst Mansion Ballroom Newport Rhode Island.wav"}, {"name": "Hepner Hall UC San Diego California.wav", "label": "Hepner Hall UC San Diego California", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Hepner Hall UC San Diego California.wav"}, {"name": "Littlefield Concert Hall Lobby Mills College Oakland California.wav", "label": "Littlefield Concert Hall Lobby Mills College Oakland California", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Littlefield Concert Hall Lobby Mills College Oakland California.wav"}, {"name": "Love Library San Diego State University California.wav", "label": "Love Library San Diego State University California", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Love Library San Diego State University California.wav"}, {"name": "Mapparium Christian Science Publishing Society Boston Massachusetts.wav", "label": "Mapparium Christian Science Publishing Society Boston Massachusetts", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Mapparium Christian Science Publishing Society Boston Massachusetts.wav"}, {"name": "Mills Art Museum Oakland California.wav", "label": "Mills Art Museum Oakland California", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Mills Art Museum Oakland California.wav"}, {"name": "Pabellon Cultural de la Republica Cabo San Lucas Baja California Sur.wav", "label": "Pabellon Cultural de la Republica Cabo San Lucas Baja California Sur", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Pabellon Cultural de la Republica Cabo San Lucas Baja California Sur.wav"}, {"name": "Pabst Brewery Tanks Milwaukee Wisconsin.wav", "label": "Pabst Brewery Tanks Milwaukee Wisconsin", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Pabst Brewery Tanks Milwaukee Wisconsin.wav"}, {"name": "Quadracci Pavilion Milwaukee Art Museum Wisconsin.wav", "label": "Quadracci Pavilion Milwaukee Art Museum Wisconsin", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Quadracci Pavilion Milwaukee Art Museum Wisconsin.wav"}, {"name": "Red Bridge Cedarburg Wisconsin.wav", "label": "Red Bridge Cedarburg Wisconsin", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Red Bridge Cedarburg Wisconsin.wav"}, {"name": "Steinman Foundation Recording Suite Millersville University Pennsylvania.wav", "label": "Steinman Foundation Recording Suite Millersville University Pennsylvania", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Steinman Foundation Recording Suite Millersville University Pennsylvania.wav"}, {"name": "Storage Tank No 7 Cedar Creek Winery Wisconsin.wav", "label": "Storage Tank No 7 Cedar Creek Winery Wisconsin", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Storage Tank No 7 Cedar Creek Winery Wisconsin.wav"}, {"name": "Wangenheim Rare Books Room San Diego Public Library California.wav", "label": "Wangenheim Rare Books Room San Diego Public Library California", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Wangenheim Rare Books Room San Diego Public Library California.wav"}, {"name": "Warren Lecture Hall 2005 UC San Diego California.wav", "label": "Warren Lecture Hall 2005 UC San Diego California", "category": "EchoThief/EchoThief/Miscellaneous", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Miscellaneous/Warren Lecture Hall 2005 UC San Diego California.wav"}, {"name": "Annie's Canyon Trail Slot Encinitas California.wav", "label": "Annie's Canyon Trail Slot Encinitas California", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Annie's Canyon Trail Slot Encinitas California.wav"}, {"name": "Byron Glacier Alaska.wav", "label": "Byron Glacier Alaska", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Byron Glacier Alaska.wav"}, {"name": "Cliff of the Dawn Cancun Quintana Roo.wav", "label": "Cliff of the Dawn Cancun Quintana Roo", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Cliff of the Dawn Cancun Quintana Roo.wav"}, {"name": "Cliff of the Dawn Isla Mujeres Quintana Roo.wav", "label": "Cliff of the Dawn Isla Mujeres Quintana Roo", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Cliff of the Dawn Isla Mujeres Quintana Roo.wav"}, {"name": "DevilsPunchbowl.wav", "label": "DevilsPunchbowl", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/DevilsPunchbowl.wav"}, {"name": "DivorceBeach.wav", "label": "DivorceBeach", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/DivorceBeach.wav"}, {"name": "FatMansMisery.wav", "label": "FatMansMisery", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/FatMansMisery.wav"}, {"name": "FatMansSqueeze.wav", "label": "FatMansSqueeze", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/FatMansSqueeze.wav"}, {"name": "Fossil Falls Coso Range California.wav", "label": "Fossil Falls Coso Range California", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Fossil Falls Coso Range California.wav"}, {"name": "Hole in the Rock Phoenix Arizona.wav", "label": "Hole in the Rock Phoenix Arizona", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Hole in the Rock Phoenix Arizona.wav"}, {"name": "Isla Mujeres Cave Quintana Roo.wav", "label": "Isla Mujeres Cave Quintana Roo", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Isla Mujeres Cave Quintana Roo.wav"}, {"name": "Isla Mujeres Small Cave Quintana Roo.wav", "label": "Isla Mujeres Small Cave Quintana Roo", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Isla Mujeres Small Cave Quintana Roo.wav"}, {"name": "Newberry National Volcanic Monument Lava Field Oregon.wav", "label": "Newberry National Volcanic Monument Lava Field Oregon", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Newberry National Volcanic Monument Lava Field Oregon.wav"}, {"name": "Purgatory Chasm Rhode Island.wav", "label": "Purgatory Chasm Rhode Island", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Purgatory Chasm Rhode Island.wav"}, {"name": "Stanley Park Cliffs Vancouver British Columbia.wav", "label": "Stanley Park Cliffs Vancouver British Columbia", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Stanley Park Cliffs Vancouver British Columbia.wav"}, {"name": "Subway Cave Lava Tube Sanctum Lassen National Forest California.wav", "label": "Subway Cave Lava Tube Sanctum Lassen National Forest California", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Subway Cave Lava Tube Sanctum Lassen National Forest California.wav"}, {"name": "Sunset Cliffs Open Ceiling Sea Cave San Diego California.wav", "label": "Sunset Cliffs Open Ceiling Sea Cave San Diego California", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Sunset Cliffs Open Ceiling Sea Cave San Diego California.wav"}, {"name": "The Slot Anza Borrego Desert California.wav", "label": "The Slot Anza Borrego Desert California", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/The Slot Anza Borrego Desert California.wav"}, {"name": "Valley of the Moon Smugglers Cave Jacumba Wilderness California.wav", "label": "Valley of the Moon Smugglers Cave Jacumba Wilderness California", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Valley of the Moon Smugglers Cave Jacumba Wilderness California.wav"}, {"name": "Woodruff Lane Corn Field Marysville California.wav", "label": "Woodruff Lane Corn Field Marysville California", "category": "EchoThief/EchoThief/Nature", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nature/Woodruff Lane Corn Field Marysville California.wav"}, {"name": "B-39 Black Widow Submarine Aft Torpedo Room.wav", "label": "B-39 Black Widow Submarine Aft Torpedo Room", "category": "EchoThief/EchoThief/Nautical", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nautical/B-39 Black Widow Submarine Aft Torpedo Room.wav"}, {"name": "B-39 Black Widow Submarine Engine Room.wav", "label": "B-39 Black Widow Submarine Engine Room", "category": "EchoThief/EchoThief/Nautical", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nautical/B-39 Black Widow Submarine Engine Room.wav"}, {"name": "Californian Tall Ship Deck.wav", "label": "Californian Tall Ship Deck", "category": "EchoThief/EchoThief/Nautical", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nautical/Californian Tall Ship Deck.wav"}, {"name": "Star of India Orlop.wav", "label": "Star of India Orlop", "category": "EchoThief/EchoThief/Nautical", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nautical/Star of India Orlop.wav"}, {"name": "Steam Ferry Berkeley Main Deck.wav", "label": "Steam Ferry Berkeley Main Deck", "category": "EchoThief/EchoThief/Nautical", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nautical/Steam Ferry Berkeley Main Deck.wav"}, {"name": "USS Dolphin Submarine Control Room.wav", "label": "USS Dolphin Submarine Control Room", "category": "EchoThief/EchoThief/Nautical", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Nautical/USS Dolphin Submarine Control Room.wav"}, {"name": "Cranbrook Art Museum Bloomfield Hills Michigan.wav", "label": "Cranbrook Art Museum Bloomfield Hills Michigan", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/Cranbrook Art Museum Bloomfield Hills Michigan.wav"}, {"name": "Hale Holistic Yoga Studio San Diego California.wav", "label": "Hale Holistic Yoga Studio San Diego California", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/Hale Holistic Yoga Studio San Diego California.wav"}, {"name": "International Velodrome at Bloomer Park Rochester Hills Michigan.wav", "label": "International Velodrome at Bloomer Park Rochester Hills Michigan", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/International Velodrome at Bloomer Park Rochester Hills Michigan.wav"}, {"name": "Natatorium UC San Diego California.wav", "label": "Natatorium UC San Diego California", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/Natatorium UC San Diego California.wav"}, {"name": "Outback Rock Climbing Center UC San Diego California.wav", "label": "Outback Rock Climbing Center UC San Diego California", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/Outback Rock Climbing Center UC San Diego California.wav"}, {"name": "Racquetball Court UC San Diego California.wav", "label": "Racquetball Court UC San Diego California", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/Racquetball Court UC San Diego California.wav"}, {"name": "Seward Waterfront Park Stone Shelter Alaska.wav", "label": "Seward Waterfront Park Stone Shelter Alaska", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/Seward Waterfront Park Stone Shelter Alaska.wav"}, {"name": "Sleeping Giant State Park Observation Tower Hamden Connecticut.wav", "label": "Sleeping Giant State Park Observation Tower Hamden Connecticut", "category": "EchoThief/EchoThief/Recreation", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Recreation/Sleeping Giant State Park Observation Tower Hamden Connecticut.wav"}, {"name": "First Church of Christ Scientist Original Edifice Boston Massachusetts.wav", "label": "First Church of Christ Scientist Original Edifice Boston Massachusetts", "category": "EchoThief/EchoThief/Sanctuaries", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Sanctuaries/First Church of Christ Scientist Original Edifice Boston Massachusetts.wav"}, {"name": "Founders Chapel University of San Diego California.wav", "label": "Founders Chapel University of San Diego California", "category": "EchoThief/EchoThief/Sanctuaries", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Sanctuaries/Founders Chapel University of San Diego California.wav"}, {"name": "Immaculata Dome University of San Diego California.wav", "label": "Immaculata Dome University of San Diego California", "category": "EchoThief/EchoThief/Sanctuaries", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Sanctuaries/Immaculata Dome University of San Diego California.wav"}, {"name": "Saint Martin of Tours Parish La Mesa California.wav", "label": "Saint Martin of Tours Parish La Mesa California", "category": "EchoThief/EchoThief/Sanctuaries", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Sanctuaries/Saint Martin of Tours Parish La Mesa California.wav"}, {"name": "St Paul's Cathedral San Diego California.wav", "label": "St Paul's Cathedral San Diego California", "category": "EchoThief/EchoThief/Sanctuaries", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Sanctuaries/St Paul's Cathedral San Diego California.wav"}, {"name": "3000 C Street Garage Stairwell Anchorage Alaska.wav", "label": "3000 C Street Garage Stairwell Anchorage Alaska", "category": "EchoThief/EchoThief/Stairwells", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Stairwells/3000 C Street Garage Stairwell Anchorage Alaska.wav"}, {"name": "CCRMA Stairwell Stanford University California.wav", "label": "CCRMA Stairwell Stanford University California", "category": "EchoThief/EchoThief/Stairwells", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Stairwells/CCRMA Stairwell Stanford University California.wav"}, {"name": "Conrad Prebys Music Center North Stairwell UC San Diego California.wav", "label": "Conrad Prebys Music Center North Stairwell UC San Diego California", "category": "EchoThief/EchoThief/Stairwells", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Stairwells/Conrad Prebys Music Center North Stairwell UC San Diego California.wav"}, {"name": "Exercise and Nutrition Sciences Stairwell San Diego State University California.wav", "label": "Exercise and Nutrition Sciences Stairwell San Diego State University California", "category": "EchoThief/EchoThief/Stairwells", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Stairwells/Exercise and Nutrition Sciences Stairwell San Diego State University California.wav"}, {"name": "San Diego Convention Center Steps California.wav", "label": "San Diego Convention Center Steps California", "category": "EchoThief/EchoThief/Stairwells", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Stairwells/San Diego Convention Center Steps California.wav"}, {"name": "Simon Fraser University Transportation Centre Burnaby British Columbia.wav", "label": "Simon Fraser University Transportation Centre Burnaby British Columbia", "category": "EchoThief/EchoThief/Stairwells", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Stairwells/Simon Fraser University Transportation Centre Burnaby British Columbia.wav"}, {"name": "Strathcona Stairwell McGill University Montreal Quebec.wav", "label": "Strathcona Stairwell McGill University Montreal Quebec", "category": "EchoThief/EchoThief/Stairwells", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Stairwells/Strathcona Stairwell McGill University Montreal Quebec.wav"}, {"name": "Batcave Griffith Park Los Angeles California.wav", "label": "Batcave Griffith Park Los Angeles California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Batcave Griffith Park Los Angeles California.wav"}, {"name": "Battery Benson Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Battery Benson Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Battery Benson Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Battery Brannan Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Battery Brannan Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Battery Brannan Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Battery Powell Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Battery Powell Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Battery Powell Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Battery Quarles Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Battery Quarles Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Battery Quarles Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Battery Randol Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Battery Randol Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Battery Randol Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Battery Tolles Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Battery Tolles Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Battery Tolles Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Cathedral Room Shasta Lake Caverns California.wav", "label": "Cathedral Room Shasta Lake Caverns California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Cathedral Room Shasta Lake Caverns California.wav"}, {"name": "Discovery Room Shasta Lake Caverns California.wav", "label": "Discovery Room Shasta Lake Caverns California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Discovery Room Shasta Lake Caverns California.wav"}, {"name": "Harbor Entrance Command Post Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Harbor Entrance Command Post Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Harbor Entrance Command Post Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Lake Mead Aqueduct Nevada.wav", "label": "Lake Mead Aqueduct Nevada", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Lake Mead Aqueduct Nevada.wav"}, {"name": "Lake Merritt BART Station Oakland California.wav", "label": "Lake Merritt BART Station Oakland California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Lake Merritt BART Station Oakland California.wav"}, {"name": "Lawrence Welk Cave Hidden Meadows California.wav", "label": "Lawrence Welk Cave Hidden Meadows California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Lawrence Welk Cave Hidden Meadows California.wav"}, {"name": "Meadowbrook Tunnel Storm Drain Poway California.wav", "label": "Meadowbrook Tunnel Storm Drain Poway California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Meadowbrook Tunnel Storm Drain Poway California.wav"}, {"name": "Music Concourse Garage Golden Gate Park San Francisco California.wav", "label": "Music Concourse Garage Golden Gate Park San Francisco California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Music Concourse Garage Golden Gate Park San Francisco California.wav"}, {"name": "Nancy Lake State Recreation Area Tunnel Alaska.wav", "label": "Nancy Lake State Recreation Area Tunnel Alaska", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Nancy Lake State Recreation Area Tunnel Alaska.wav"}, {"name": "Pillbox Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Pillbox Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Pillbox Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Port Townsend Skatepark Washington.wav", "label": "Port Townsend Skatepark Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Port Townsend Skatepark Washington.wav"}, {"name": "Portage Creek Tunnel Alaska.wav", "label": "Portage Creek Tunnel Alaska", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Portage Creek Tunnel Alaska.wav"}, {"name": "Qasgiq Semisubterranean Longhouse Anchorage Alaska.wav", "label": "Qasgiq Semisubterranean Longhouse Anchorage Alaska", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Qasgiq Semisubterranean Longhouse Anchorage Alaska.wav"}, {"name": "Salton Sea Drainage Pipe California.wav", "label": "Salton Sea Drainage Pipe California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Salton Sea Drainage Pipe California.wav"}, {"name": "San Diego Civic Center Plaza Parking Garage Basement 3 California.wav", "label": "San Diego Civic Center Plaza Parking Garage Basement 3 California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/San Diego Civic Center Plaza Parking Garage Basement 3 California.wav"}, {"name": "Switzer Canyon Tunnel San Diego California.wav", "label": "Switzer Canyon Tunnel San Diego California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Switzer Canyon Tunnel San Diego California.wav"}, {"name": "Tamarack Beach Drainage Pipe Carlsbad California.wav", "label": "Tamarack Beach Drainage Pipe Carlsbad California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Tamarack Beach Drainage Pipe Carlsbad California.wav"}, {"name": "Tijuana Aqueduct Tunnel Baja California.wav", "label": "Tijuana Aqueduct Tunnel Baja California", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Tijuana Aqueduct Tunnel Baja California.wav"}, {"name": "Tony Knowles Coastal Trail Tunnel Anchorage Alaska.wav", "label": "Tony Knowles Coastal Trail Tunnel Anchorage Alaska", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Tony Knowles Coastal Trail Tunnel Anchorage Alaska.wav"}, {"name": "Tunnel Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Tunnel Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Tunnel Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Tunnel to Heaven Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Tunnel to Heaven Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Tunnel to Heaven Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "Tunnel to Hell Fort Worden Historical State Park Port Townsend Washington.wav", "label": "Tunnel to Hell Fort Worden Historical State Park Port Townsend Washington", "category": "EchoThief/EchoThief/Underground", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underground/Tunnel to Hell Fort Worden Historical State Park Port Townsend Washington.wav"}, {"name": "5 Underpass Commercial Avenue San Diego California.wav", "label": "5 Underpass Commercial Avenue San Diego California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/5 Underpass Commercial Avenue San Diego California.wav"}, {"name": "5 Underpass Valencia California.wav", "label": "5 Underpass Valencia California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/5 Underpass Valencia California.wav"}, {"name": "Arroyo Ensenada Underpass Baja Californa.wav", "label": "Arroyo Ensenada Underpass Baja Californa", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Arroyo Ensenada Underpass Baja Californa.wav"}, {"name": "Art and Anne McDaniel Bridge Underpass San Diego California.wav", "label": "Art and Anne McDaniel Bridge Underpass San Diego California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Art and Anne McDaniel Bridge Underpass San Diego California.wav"}, {"name": "Avenue 52 Underpass Los Angeles River California.wav", "label": "Avenue 52 Underpass Los Angeles River California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Avenue 52 Underpass Los Angeles River California.wav"}, {"name": "Avenue 60 Underpass Los Angeles River California.wav", "label": "Avenue 60 Underpass Los Angeles River California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Avenue 60 Underpass Los Angeles River California.wav"}, {"name": "Boulevard Rosemont Underpass Montreal Quebec.wav", "label": "Boulevard Rosemont Underpass Montreal Quebec", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Boulevard Rosemont Underpass Montreal Quebec.wav"}, {"name": "Cariboo Road Underpass Gaglardi Way Burnabee British Columbia.wav", "label": "Cariboo Road Underpass Gaglardi Way Burnabee British Columbia", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Cariboo Road Underpass Gaglardi Way Burnabee British Columbia.wav"}, {"name": "Cleft Ridge Span Prospect Park Brooklyn New York.wav", "label": "Cleft Ridge Span Prospect Park Brooklyn New York", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Cleft Ridge Span Prospect Park Brooklyn New York.wav"}, {"name": "Dipway Arch Central Park New York.wav", "label": "Dipway Arch Central Park New York", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Dipway Arch Central Park New York.wav"}, {"name": "Echo Bridge Newton Upper Falls Massachusetts.wav", "label": "Echo Bridge Newton Upper Falls Massachusetts", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Echo Bridge Newton Upper Falls Massachusetts.wav"}, {"name": "Fremont Troll Seattle Washington.wav", "label": "Fremont Troll Seattle Washington", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Fremont Troll Seattle Washington.wav"}, {"name": "Hopkins Drive Underpass UC San Diego California.wav", "label": "Hopkins Drive Underpass UC San Diego California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Hopkins Drive Underpass UC San Diego California.wav"}, {"name": "JFK Underpass Golden Gate Park San Francisco California.wav", "label": "JFK Underpass Golden Gate Park San Francisco California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/JFK Underpass Golden Gate Park San Francisco California.wav"}, {"name": "Lions Gate Bridge Vancouver British Columbia.wav", "label": "Lions Gate Bridge Vancouver British Columbia", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Lions Gate Bridge Vancouver British Columbia.wav"}, {"name": "Old South Bridge UnderpassConcord Massachusetts.wav", "label": "Old South Bridge UnderpassConcord Massachusetts", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Old South Bridge UnderpassConcord Massachusetts.wav"}, {"name": "River Mountains Loop Trail Aqueduct Lake Mead California.wav", "label": "River Mountains Loop Trail Aqueduct Lake Mead California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/River Mountains Loop Trail Aqueduct Lake Mead California.wav"}, {"name": "Stanley Park Causeway Vancouver British Columbia.wav", "label": "Stanley Park Causeway Vancouver British Columbia", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Stanley Park Causeway Vancouver British Columbia.wav"}, {"name": "Stanley Park Drive Underpass Vancouver British Columbia.wav", "label": "Stanley Park Drive Underpass Vancouver British Columbia", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Stanley Park Drive Underpass Vancouver British Columbia.wav"}, {"name": "Switzer Street Underpass East Harbor Drive San Diego California.wav", "label": "Switzer Street Underpass East Harbor Drive San Diego California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Switzer Street Underpass East Harbor Drive San Diego California.wav"}, {"name": "Telephone Wash Underpass San Bernardino County California.wav", "label": "Telephone Wash Underpass San Bernardino County California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Telephone Wash Underpass San Bernardino County California.wav"}, {"name": "Walkway Underpass East Campus Drive San Diego State University California.wav", "label": "Walkway Underpass East Campus Drive San Diego State University California", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Walkway Underpass East Campus Drive San Diego State University California.wav"}, {"name": "Waterplace Park Providence Rhode Island.wav", "label": "Waterplace Park Providence Rhode Island", "category": "EchoThief/EchoThief/Underpasses", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Underpasses/Waterplace Park Providence Rhode Island.wav"}, {"name": "City Heights Performance Annex San Diego California.wav", "label": "City Heights Performance Annex San Diego California", "category": "EchoThief/EchoThief/Venues", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Venues/City Heights Performance Annex San Diego California.wav"}, {"name": "Conrad Prebys Concert Hall Seat F111 UC San Diego California.wav", "label": "Conrad Prebys Concert Hall Seat F111 UC San Diego California", "category": "EchoThief/EchoThief/Venues", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Venues/Conrad Prebys Concert Hall Seat F111 UC San Diego California.wav"}, {"name": "Mills Greek Theater Oakland California.wav", "label": "Mills Greek Theater Oakland California", "category": "EchoThief/EchoThief/Venues", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Venues/Mills Greek Theater Oakland California.wav"}, {"name": "Naumburg Bandshell Central Park New York.wav", "label": "Naumburg Bandshell Central Park New York", "category": "EchoThief/EchoThief/Venues", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Venues/Naumburg Bandshell Central Park New York.wav"}, {"name": "Orpheum Theatre Omaha Grand Tier D8 Nebraska.wav", "label": "Orpheum Theatre Omaha Grand Tier D8 Nebraska", "category": "EchoThief/EchoThief/Venues", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Venues/Orpheum Theatre Omaha Grand Tier D8 Nebraska.wav"}, {"name": "Space 4 Art Gallery San Diego California.wav", "label": "Space 4 Art Gallery San Diego California", "category": "EchoThief/EchoThief/Venues", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Venues/Space 4 Art Gallery San Diego California.wav"}, {"name": "Steinman Hall Millersville University Pennsylvania.wav", "label": "Steinman Hall Millersville University Pennsylvania", "category": "EchoThief/EchoThief/Venues", "path": "/var/modep/user-files/Reverb IRs/EchoThief/EchoThief/Venues/Steinman Hall Millersville University Pennsylvania.wav"}, {"name": "IR_AKG_BX25_1,5s.wav", "label": "IR AKG BX25 1,5s", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_1,5s.wav"}, {"name": "IR_AKG_BX25_1,5s_16x_stretch.wav", "label": "IR AKG BX25 1,5s 16x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_1,5s_16x_stretch.wav"}, {"name": "IR_AKG_BX25_1,5s_64x_stretch.wav", "label": "IR AKG BX25 1,5s 64x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_1,5s_64x_stretch.wav"}, {"name": "IR_AKG_BX25_1,5s_8x_stretch.wav", "label": "IR AKG BX25 1,5s 8x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_1,5s_8x_stretch.wav"}, {"name": "IR_AKG_BX25_1,5s_dark.wav", "label": "IR AKG BX25 1,5s dark", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_1,5s_dark.wav"}, {"name": "IR_AKG_BX25_2,5s.wav", "label": "IR AKG BX25 2,5s", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_2,5s.wav"}, {"name": "IR_AKG_BX25_2,5s_dark.wav", "label": "IR AKG BX25 2,5s dark", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_2,5s_dark.wav"}, {"name": "IR_AKG_BX25_3,5s.wav", "label": "IR AKG BX25 3,5s", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_3,5s.wav"}, {"name": "IR_AKG_BX25_3,5s_dark.wav", "label": "IR AKG BX25 3,5s dark", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_AKG_BX25_3,5s_dark.wav"}, {"name": "IR_Dynacord_VRS_reverb_long.wav", "label": "IR Dynacord VRS reverb long", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Dynacord_VRS_reverb_long.wav"}, {"name": "IR_Dynacord_VRS_reverb_medium.wav", "label": "IR Dynacord VRS reverb medium", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Dynacord_VRS_reverb_medium.wav"}, {"name": "IR_Dynacord_VRS_reverb_medium_16x_stretch.wav", "label": "IR Dynacord VRS reverb medium 16x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Dynacord_VRS_reverb_medium_16x_stretch.wav"}, {"name": "IR_Dynacord_VRS_reverb_medium_32x_stretch.wav", "label": "IR Dynacord VRS reverb medium 32x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Dynacord_VRS_reverb_medium_32x_stretch.wav"}, {"name": "IR_Dynacord_VRS_reverb_medium_8x_stretch.wav", "label": "IR Dynacord VRS reverb medium 8x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Dynacord_VRS_reverb_medium_8x_stretch.wav"}, {"name": "IR_Dynacord_VRS_reverb_short.wav", "label": "IR Dynacord VRS reverb short", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Dynacord_VRS_reverb_short.wav"}, {"name": "IR_Korg_GR1.wav", "label": "IR Korg GR1", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Korg_GR1.wav"}, {"name": "IR_Korg_GR1_16x_stretch.wav", "label": "IR Korg GR1 16x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Korg_GR1_16x_stretch.wav"}, {"name": "IR_Korg_GR1_32x_stretch.wav", "label": "IR Korg GR1 32x stretch", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Korg_GR1_32x_stretch.wav"}, {"name": "IR_Roland_RE201_spring_high-drive.wav", "label": "IR Roland RE201 spring high-drive", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE201_spring_high-drive.wav"}, {"name": "IR_Roland_RE201_spring_low-drive.wav", "label": "IR Roland RE201 spring low-drive", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE201_spring_low-drive.wav"}, {"name": "IR_Roland_RE201_spring_med-drive.wav", "label": "IR Roland RE201 spring med-drive", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE201_spring_med-drive.wav"}, {"name": "IR_Roland_RE201_tape_fast.wav", "label": "IR Roland RE201 tape fast", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE201_tape_fast.wav"}, {"name": "IR_Roland_RE201_tape_slow.wav", "label": "IR Roland RE201 tape slow", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE201_tape_slow.wav"}, {"name": "IR_Roland_RE501_spring_high-drive.wav", "label": "IR Roland RE501 spring high-drive", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE501_spring_high-drive.wav"}, {"name": "IR_Roland_RE501_spring_low-drive.wav", "label": "IR Roland RE501 spring low-drive", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE501_spring_low-drive.wav"}, {"name": "IR_Roland_RE501_spring_med-drive.wav", "label": "IR Roland RE501 spring med-drive", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE501_spring_med-drive.wav"}, {"name": "IR_Roland_RE501_tape_drive_fast.wav", "label": "IR Roland RE501 tape drive fast", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE501_tape_drive_fast.wav"}, {"name": "IR_Roland_RE501_tape_drive_slow.wav", "label": "IR Roland RE501 tape drive slow", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE501_tape_drive_slow.wav"}, {"name": "IR_Roland_RE501_tape_fast.wav", "label": "IR Roland RE501 tape fast", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE501_tape_fast.wav"}, {"name": "IR_Roland_RE501_tape_slow.wav", "label": "IR Roland RE501 tape slow", "category": "General", "path": "/var/modep/user-files/Reverb IRs/IR_Roland_RE501_tape_slow.wav"}, {"name": "emt_140_bright_1.wav", "label": "emt 140 bright 1", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_bright_1.wav"}, {"name": "emt_140_bright_2.wav", "label": "emt 140 bright 2", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_bright_2.wav"}, {"name": "emt_140_bright_4.wav", "label": "emt 140 bright 4", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_bright_4.wav"}, {"name": "emt_140_bright_5.wav", "label": "emt 140 bright 5", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_bright_5.wav"}, {"name": "emt_140_dark_1.wav", "label": "emt 140 dark 1", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_dark_1.wav"}, {"name": "emt_140_dark_2.wav", "label": "emt 140 dark 2", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_dark_2.wav"}, {"name": "emt_140_dark_3.wav", "label": "emt 140 dark 3", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_dark_3.wav"}, {"name": "emt_140_dark_4.wav", "label": "emt 140 dark 4", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_dark_4.wav"}, {"name": "emt_140_medium_1.wav", "label": "emt 140 medium 1", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_medium_1.wav"}, {"name": "emt_140_medium_2.wav", "label": "emt 140 medium 2", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_medium_2.wav"}, {"name": "emt_140_medium_3.wav", "label": "emt 140 medium 3", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_medium_3.wav"}, {"name": "emt_140_medium_4.wav", "label": "emt 140 medium 4", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_medium_4.wav"}, {"name": "emt_140_medium_5.wav", "label": "emt 140 medium 5", "category": "General", "path": "/var/modep/user-files/Reverb IRs/emt_140_medium_5.wav"}, {"name": "forward-audio_AliceInBones.wav", "label": "forward-audio AliceInBones", "category": "cabsim-IR-loader.lv2", "path": "/var/modep/lv2/cabsim-IR-loader.lv2/forward-audio_AliceInBones.wav"}]
    };

    window.fetchLiveUserFiles = function() {
        fetch(`${getModepHttpBase()}/effect/files`).then(r => r.json()).then(res => {
            if (res && res.files) {
                if (Array.isArray(res.files.nam) && res.files.nam.length) window.MODEP_USER_FILES.nam = res.files.nam;
                if (Array.isArray(res.files.ir) && res.files.ir.length) window.MODEP_USER_FILES.ir = res.files.ir;
            }
        }).catch(() => {});
    };

    window.fetchLiveModepLv2Plugins = function() {
        fetch(`${getModepHttpBase()}/effect/list`).then(r => r.json()).then(list => {
            if (!Array.isArray(list)) return;
            const byUri = {};
            window.MODEP_LV2_CATALOG.forEach(p => { byUri[p.uri] = p; });
            list.forEach(item => {
                if (!item || !item.uri) return;
                const rawCat = Array.isArray(item.category) ? item.category.join(" / ") : (item.category || "MODEP LV2 Module");
                const mappedPorts = mapModepPorts(item);
                const aOut = (item.ports && item.ports.audio && Array.isArray(item.ports.audio.output)) ? item.ports.audio.output.length : 1;
                const uLower = (item.uri + " " + (item.name || "") + " " + (item.brand || "")).toLowerCase();
                const isCyber = uLower.includes("cyber") || uLower.includes("danny") || uLower.includes("dusk") || uLower.includes("nam");
                const catKey = inferPluginCategoryKey(item.uri, item.name, rawCat);
                if (byUri[item.uri]) {
                    if (mappedPorts && mappedPorts.length) byUri[item.uri].ports = mappedPorts;
                    byUri[item.uri].brand = item.brand || "";
                    byUri[item.uri].stereo = aOut >= 2;
                    byUri[item.uri].isCyber = byUri[item.uri].isCyber || isCyber;
                    if (!byUri[item.uri].catKey) byUri[item.uri].catKey = catKey;
                } else {
                    const fallbackPorts = catKey === "amp" ? AMP_PORTS : (catKey === "drive" ? DRIVE_PORTS : []);
                    const entry = {
                        uri: item.uri,
                        name: item.name || item.label || item.uri.split("/").pop(),
                        brand: item.brand || "",
                        category: rawCat,
                        catKey: catKey,
                        isCyber: isCyber,
                        icon: isCyber ? "⭐" : "🔌",
                        stereo: aOut >= 2,
                        ports: (mappedPorts && mappedPorts.length) ? mappedPorts : fallbackPorts,
                        presets: Array.isArray(item.presets) ? item.presets : []
                    };
                    window.MODEP_LV2_CATALOG.push(entry);
                    byUri[item.uri] = entry;
                }
            });
        }).catch(() => {});
    };

    function createEmptySlot() {
        return { uri: "thru", name: "EMPTY (THRU)", category: "Pass-Through", icon: "➡️", bypassed: true, params: {} };
    }

    function mirrorSlotsForRack(ch) {
        if (!window.channelDualDspState[ch]) return;
        window.channelLv2Slots[ch] = window.channelDualDspState[ch].slots.map((s, idx) => {
            if (!s || !s.uri || s.uri === "thru") return null;
            return { slot: idx, uri: s.uri, name: s.name, bypass: !!s.bypassed, params: s.params || {} };
        });
    }

    function loadDualDspState() {
        try {
            const saved = localStorage.getItem("modep_channel_dual_dsp_v2");
            if (saved) window.channelDualDspState = JSON.parse(saved);
        } catch (e) {}
        for (let ch = 0; ch < 8; ch++) {
            if (!window.channelDualDspState[ch]) {
                window.channelDualDspState[ch] = {
                    mode: "rack",
                    rackPort: (ch === 6 || ch === 7) ? 8881 : ((ch === 0 || ch === 1) ? 8881 : ((ch === 2 || ch === 3) ? 8883 : 8884)),
                    canvasBypass: false,
                    slotFirst: false,
                    modguiFirst: false,
                    insertMode: 1, // 0 = Direct, 1 = 4 Slots Only, 2 = MOD Pedalboard Only (Rule 28)
                    slots: [createEmptySlot(), createEmptySlot(), createEmptySlot(), createEmptySlot()]
                };
            } else {
                if (!Array.isArray(window.channelDualDspState[ch].slots)) {
                    window.channelDualDspState[ch].slots = [createEmptySlot(), createEmptySlot(), createEmptySlot(), createEmptySlot()];
                }
                if (window.channelDualDspState[ch].slotFirst === undefined) window.channelDualDspState[ch].slotFirst = false;
                if (window.channelDualDspState[ch].modguiFirst === undefined) window.channelDualDspState[ch].modguiFirst = false;
                if (window.channelDualDspState[ch].insertMode === undefined) window.channelDualDspState[ch].insertMode = 1;
                window.channelDualDspState[ch].slots.forEach(slot => {
                    if (!slot || !slot.uri || slot.uri === "thru") return;
                    if (!slot.params || typeof slot.params !== "object") slot.params = {};
                    const cat = window.MODEP_LV2_CATALOG.find(p => p.uri === slot.uri);
                    if (cat && Array.isArray(cat.ports) && cat.ports.length > 0) {
                        const validSyms = new Set(cat.ports.map(pt => pt.sym));
                        ["gain", "master", "deep", "mid", "bright", "drive", "tone", "level", "voice"].forEach(oldKey => {
                            if (!validSyms.has(oldKey) && slot.params[oldKey] !== undefined) {
                                delete slot.params[oldKey];
                            }
                        });
                        cat.ports.forEach(pt => {
                            if (slot.params[pt.sym] === undefined) {
                                slot.params[pt.sym] = pt.def;
                            }
                        });
                    }
                });
            }
            mirrorSlotsForRack(ch);
        }
    }

    function saveDualDspState() {
        for (let ch = 0; ch < 8; ch++) mirrorSlotsForRack(ch);
        try { localStorage.setItem("modep_channel_dual_dsp_v2", JSON.stringify(window.channelDualDspState)); } catch (e) {}
        if (!window._applyingRemoteSync && typeof window.schedulePeerUiSnapshotSave === "function") {
            window.schedulePeerUiSnapshotSave();
        }
    }

    window.renderChainFirstButtons = function(ch) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const st = window.channelDualDspState[ch];
        const modeBar = document.getElementById(`dsp-mode-bar-${ch}`);
        if (modeBar && !document.getElementById(`chain-order-bar-${ch}`)) {
            const bar = document.createElement("div");
            bar.className = "chain-order-bar";
            bar.id = `chain-order-bar-${ch}`;
            bar.innerHTML = `
                <button class="btn-chain-first slot-first" id="btn-slot-first-${ch}" onclick="toggleSlotFirst(${ch})" title="Place Insert Slots 1&amp;2 FIRST in the channel chain (before 1176 Preamp, EQ &amp; Comp) for Guitar/Amp use"></button>
                <button class="btn-chain-first mod-first" id="btn-modgui-first-${ch}" onclick="toggleModguiFirst(${ch})" title="Place MOD-GUI Pedalboard (Slots 3&amp;4) FIRST in the channel chain (before 1176 Preamp, EQ &amp; Comp) for Guitar/Amp use"></button>
            `;
            modeBar.insertAdjacentElement("afterend", bar);
        }
        const sBtn = document.getElementById(`btn-slot-first-${ch}`);
        if (sBtn) {
            sBtn.classList.toggle("active", !!st.slotFirst);
            sBtn.innerText = st.slotFirst ? "⚡ SLOT: 1ST" : "SLOT: POST";
        }
        const mBtn = document.getElementById(`btn-modgui-first-${ch}`);
        if (mBtn) {
            mBtn.classList.toggle("active", !!st.modguiFirst);
            mBtn.innerText = st.modguiFirst ? "🎸 MOD: 1ST" : "MOD: POST";
        }
    };

    window.toggleSlotFirst = function(ch) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const st = window.channelDualDspState[ch];
        st.slotFirst = !st.slotFirst;
        saveDualDspState();
        window.renderChainFirstButtons(ch);
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({ type: "dsp_param", channel: ch, param: "slotFirst", value: st.slotFirst ? 1 : 0 }));
        }
        const fsOverlay = document.getElementById("fullscreen-slots-overlay");
        if (fsOverlay && fsOverlay.classList.contains("open") && activeSlotsPageChannel === ch) {
            window.renderFullScreenSlotsPage(ch);
        }
        const rackPage = document.getElementById("dsp-rack-page");
        if (rackPage && rackPage.classList.contains("open") && window.dspRackCurrentChannel === ch && typeof window.showDspRackPage === "function") {
            window.showDspRackPage(ch);
        }
    };

    window.toggleModguiFirst = function(ch) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const st = window.channelDualDspState[ch];
        st.modguiFirst = !st.modguiFirst;
        saveDualDspState();
        window.renderChainFirstButtons(ch);
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({ type: "dsp_param", channel: ch, param: "modguiFirst", value: st.modguiFirst ? 1 : 0 }));
        }
        const fsOverlay = document.getElementById("fullscreen-slots-overlay");
        if (fsOverlay && fsOverlay.classList.contains("open") && activeSlotsPageChannel === ch) {
            window.renderFullScreenSlotsPage(ch);
        }
        const rackPage = document.getElementById("dsp-rack-page");
        if (rackPage && rackPage.classList.contains("open") && window.dspRackCurrentChannel === ch && typeof window.showDspRackPage === "function") {
            window.showDspRackPage(ch);
        }
    };

    window.syncLinearSlotChain = function(ch) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        mirrorSlotsForRack(ch);
        const st = window.channelDualDspState[ch];
        const formattedSlots = [];
        st.slots.forEach((slot, idx) => {
            if (slot && slot.uri && slot.uri !== "thru") {
                formattedSlots.push({
                    slot: idx,
                    uri: slot.uri,
                    name: slot.name || "FX",
                    bypass: !!slot.bypassed,
                    file_path: slot.file_path || (slot.params && slot.params._file_path) || (slot.params && slot.params.model_file) || "",
                    file_label: slot.file_label || (slot.params && slot.params.file_label) || "",
                    params: slot.params || {}
                });
            }
        });
        for (let s = 0; s < 4; s++) window.renderChannelSlotDom(ch, s);
        window.renderChainFirstButtons(ch);
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: "sync_linear_lv2_slots",
                channel: ch,
                ch: ch,
                slots: formattedSlots
            }));
            window.meterWs.send(JSON.stringify({ type: "dsp_param", channel: ch, param: "slotFirst", value: st.slotFirst ? 1 : 0 }));
            window.meterWs.send(JSON.stringify({ type: "dsp_param", channel: ch, param: "modguiFirst", value: st.modguiFirst ? 1 : 0 }));
        }
        // Refresh Fullscreen 4-Slot Rack Page if open for this channel
        const fsOverlay = document.getElementById("fullscreen-slots-overlay");
        if (fsOverlay && fsOverlay.classList.contains("open") && activeSlotsPageChannel === ch) {
            window.renderFullScreenSlotsPage(ch);
        }
        // Refresh DSP Rack Page if open for this channel
        const rackPage = document.getElementById("dsp-rack-page");
        if (rackPage && rackPage.classList.contains("open") && window.dspRackCurrentChannel === ch && typeof window.showDspRackPage === "function") {
            window.showDspRackPage(ch);
        }
    };

    window.syncAllChannelSlotChains = function() {
        for (let ch = 0; ch < 8; ch++) window.syncLinearSlotChain(ch);
    };

    window.renderChannelSlotDom = function(ch, slotIdx) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const slots = window.channelDualDspState[ch].slots;
        const slot = slots[slotIdx] || createEmptySlot();
        const slotEl = document.getElementById(`slot-${ch}-${slotIdx}`);
        const nameEl = document.getElementById(`slot-name-${ch}-${slotIdx}`);
        const bypBtn = document.getElementById(`slot-byp-${ch}-${slotIdx}`);
        const isThru = (!slot.uri || slot.uri === "thru" || slot.name === "EMPTY (THRU)");
        if (nameEl) nameEl.innerText = isThru ? `SLOT ${slotIdx + 1}: EMPTY` : `${slot.icon || "🔌"} ${slot.name}`;
        if (slotEl) {
            slotEl.classList.toggle("empty", isThru);
            slotEl.classList.toggle("occupied", !isThru);
            slotEl.classList.toggle("bypassed", isThru || !!slot.bypassed);
            slotEl.title = `Tap to open 100% Full-Screen 4-Slot Effect Rack for CH ${ch + 1}`;
        }
        if (bypBtn) {
            bypBtn.classList.toggle("active", !isThru && !slot.bypassed);
            bypBtn.innerText = isThru ? "THRU" : (slot.bypassed ? "BYP" : "ON");
        }
    };

    window.setChannelInsertMode = function(ch, modeVal) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const mode = parseInt(modeVal, 10);
        window.channelDualDspState[ch].insertMode = (mode >= 0 && mode <= 2) ? mode : 1;
        saveDualDspState();
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: "set_channel_insert_mode",
                channel: ch,
                ch: ch,
                mode_val: window.channelDualDspState[ch].insertMode,
                mode: window.channelDualDspState[ch].insertMode === 0 ? "direct" : (window.channelDualDspState[ch].insertMode === 2 ? "canvas" : "rack")
            }));
        }
        const fsOverlay = document.getElementById("fullscreen-slots-overlay");
        if (fsOverlay && fsOverlay.classList.contains("open") && activeSlotsPageChannel === ch) {
            window.renderFullScreenSlotsPage(ch);
        }
    };

    window.setChannelDspMode = function(ch, mode) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const wasAlreadyRack = (window.channelDualDspState[ch].mode === "rack" && mode === "rack");
        window.channelDualDspState[ch].mode = mode;
        window.channelDualDspState[ch].insertMode = (mode === "canvas") ? 2 : 1;
        saveDualDspState();
        const btnRack = document.getElementById("btn-mode-rack-" + ch);
        const btnCanvas = document.getElementById("btn-mode-canvas-" + ch);
        if (btnRack) btnRack.classList.toggle("active", mode === "rack");
        if (btnCanvas) btnCanvas.classList.toggle("active", mode === "canvas");
        const rackCont = document.getElementById("dsp-rack-container-" + ch);
        const canvasCont = document.getElementById("dsp-canvas-container-" + ch);
        if (rackCont) rackCont.style.display = (mode === "rack") ? "block" : "none";
        if (canvasCont) canvasCont.style.display = (mode === "canvas") ? "block" : "none";
        if (mode === "rack") window.syncLinearSlotChain(ch);
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: "set_dsp_mode", channel: ch, ch: ch, mode: mode, mode_val: (mode === "canvas" ? 2 : 1), rack_port: window.channelDualDspState[ch].rackPort
            }));
            window.meterWs.send(JSON.stringify({
                action: "set_channel_insert_mode", channel: ch, ch: ch, mode_val: (mode === "canvas" ? 2 : 1), mode: mode
            }));
        }
    };

    window.setChannelCanvasRack = function(ch, port) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        window.channelDualDspState[ch].rackPort = parseInt(port, 10) || 8881;
        saveDualDspState();
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
            window.meterWs.send(JSON.stringify({ action: "set_canvas_bypass", channel: ch, bypass: curr }));
        }
    };

    window.toggleSlotBypass = function(ch, slotIdx) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const slot = window.channelDualDspState[ch].slots[slotIdx];
        if (!slot || !slot.uri || slot.uri === "thru") {
            window.openFullScreenSlotsPage(ch, slotIdx);
            return;
        }
        slot.bypassed = !slot.bypassed;
        saveDualDspState();
        window.syncLinearSlotChain(ch);
        const iframe = document.getElementById("slot-modgui-iframe");
        if (iframe && iframe.contentWindow) {
            iframe.contentWindow.postMessage({ type: "set_bypass", bypassed: !!slot.bypassed }, "*");
        }
    };

    // =========================================================================
    // 1. 100% FULL-SCREEN 4-SLOT CHANNEL EFFECT RACK PAGE
    // =========================================================================
    window.openFullScreenSlotsPage = function(ch, highlightSlotIdx) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        activeSlotsPageChannel = ch;
        const overlay = document.getElementById("fullscreen-slots-overlay");
        if (!overlay) return;
        window.renderFullScreenSlotsPage(ch, highlightSlotIdx);
        overlay.style.display = "flex";
        overlay.classList.add("open");
    };

    window.closeFullScreenSlotsPage = function() {
        const overlay = document.getElementById("fullscreen-slots-overlay");
        if (overlay) {
            overlay.style.display = "none";
            overlay.classList.remove("open");
        }
    };

    window.renderFullScreenSlotsPage = function(ch, highlightSlotIdx) {
        const container = document.getElementById("fullscreen-slots-content");
        if (!container) return;
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const slots = window.channelDualDspState[ch].slots;
        const chName = (window.customChannelNames && window.customChannelNames["ch-" + ch]) || (window.channelNames && window.channelNames[ch]) || `CH ${ch + 1}`;

        const chTabsHtml = [0, 1, 2, 3, 4, 5, 6, 7].map(idx => {
            const nm = (window.customChannelNames && window.customChannelNames["ch-" + idx]) || (window.channelNames && window.channelNames[idx]) || `CH ${idx + 1}`;
            const actCount = (window.channelDualDspState[idx]?.slots || []).filter(s => s && s.uri && s.uri !== "thru").length;
            const isSel = idx === ch;
            return `<button onclick="openFullScreenSlotsPage(${idx})" style="min-height:34px; padding:3px 8px; border-radius:6px; font-weight:900; font-size:0.75rem; cursor:pointer; background:${isSel ? '#059669' : '#111827'}; border:1.5px solid ${isSel ? '#34d399' : '#1e293b'}; color:${isSel ? '#ffffff' : '#94a3b8'}; display:flex; align-items:center; gap:5px; flex-shrink:0;">
                <span>IN ${idx + 1}: ${nm.substring(0, 8)}</span>
                ${actCount > 0 ? `<span style="background:#065f46; color:#6ee7b7; border-radius:8px; padding:0 5px; font-size:0.68rem;">${actCount} FX</span>` : ''}
            </button>`;
        }).join("");

        const slotColumnsHtml = [0, 1, 2, 3].map(sIdx => {
            const slot = slots[sIdx] || createEmptySlot();
            const isThru = (!slot.uri || slot.uri === "thru" || slot.name === "EMPTY (THRU)");
            const isHighlighted = (highlightSlotIdx === sIdx);
            if (isThru) {
                return `
                <div style="background:linear-gradient(180deg, #0d131f 0%, #090d16 100%); border:1.5px dashed ${isHighlighted ? '#38bdf8' : '#334155'}; border-radius:10px; padding:8px 10px; display:flex; flex-direction:column; justify-content:space-between; align-items:stretch; height:100%; box-sizing:border-box; overflow:hidden;">
                    <div>
                        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:6px;">
                            <span style="background:#1e293b; color:#38bdf8; font-weight:900; font-size:0.75rem; padding:2px 8px; border-radius:4px;">SLOT ${sIdx + 1}</span>
                            <span style="color:#64748b; font-size:0.7rem; font-weight:800;">PASS-THROUGH</span>
                        </div>
                        <div style="text-align:center; padding:10px 4px;">
                            <div style="font-size:1.8rem; margin-bottom:4px;">🔌</div>
                            <div style="font-size:0.92rem; font-weight:900; color:#e2e8f0; margin-bottom:2px;">EMPTY SLOT</div>
                            <div style="font-size:0.72rem; color:#94a3b8; line-height:1.2;">Zero-latency clean audio thru.</div>
                        </div>
                    </div>
                    <div style="display:flex; flex-direction:column; gap:5px;">
                        <button onclick="openPluginPickerWithCategory(${ch}, ${sIdx}, 'all')" style="min-height:44px; background:linear-gradient(135deg, #0284c7 0%, #2563eb 100%); border:1.5px solid #38bdf8; color:#ffffff; border-radius:8px; font-size:0.86rem; font-weight:900; cursor:pointer; box-shadow:0 3px 10px rgba(2,132,199,0.35);">
                            ➕ CHOOSE EFFECT
                        </button>
                        <div style="display:grid; grid-template-columns:1fr 1fr; gap:4px;">
                            <button onclick="openPluginPickerWithCategory(${ch}, ${sIdx}, 'cyber')" style="min-height:34px; background:#1e1b4b; border:1px solid #818cf8; color:#e0e7ff; border-radius:6px; font-size:0.72rem; font-weight:900; cursor:pointer;">⭐ CYBER FX</button>
                            <button onclick="openPluginPickerWithCategory(${ch}, ${sIdx}, 'amp')" style="min-height:34px; background:#1f1610; border:1px solid #f59e0b; color:#fef3c7; border-radius:6px; font-size:0.72rem; font-weight:900; cursor:pointer;">🎸 AMPS &amp; NAM</button>
                            <button onclick="openPluginPickerWithCategory(${ch}, ${sIdx}, 'drive')" style="min-height:34px; background:#2a1215; border:1px solid #f87171; color:#fee2e2; border-radius:6px; font-size:0.72rem; font-weight:900; cursor:pointer;">🔥 DRIVE</button>
                            <button onclick="openPluginPickerWithCategory(${ch}, ${sIdx}, 'reverb')" style="min-height:34px; background:#0c2229; border:1px solid #22d3ee; color:#cffafe; border-radius:6px; font-size:0.72rem; font-weight:900; cursor:pointer;">🏛️ REV/DLY</button>
                        </div>
                    </div>
                </div>`;
            }

            let catalogEntry = window.MODEP_LV2_CATALOG.find(p => p.uri === slot.uri) || { ports: DRIVE_PORTS };
            const ports = (catalogEntry.ports && catalogEntry.ports.length) ? catalogEntry.ports : DRIVE_PORTS;
            const scalePort = ports.find(pt => pt.scalePoints && pt.scalePoints.length > 3 && (pt.sym === "profile_a" || pt.sym === "model" || pt.sym === "preset" || pt.sym === "algorithm"));
            const quickPorts = ports.filter(pt => pt.sym !== "bypass" && pt.sym !== ":bypass" && (!scalePort || pt.sym !== scalePort.sym)).slice(0, 4);
            const quickControlsHtml = quickPorts.map(pt => {
                const val = (slot.params && slot.params[pt.sym] !== undefined) ? slot.params[pt.sym] : pt.def;
                if (isSwitchLikePort(pt)) {
                    const isOn = Number(val) >= 0.5;
                    return `
                    <div onclick="updateSlotLv2Param(${ch}, ${sIdx}, '${pt.sym}', ${isOn ? 0 : 1}); renderFullScreenSlotsPage(${ch});" style="background:${isOn ? '#064e3b' : '#111827'}; border:1.5px solid ${isOn ? '#10b981' : '#334155'}; border-radius:6px; padding:4px 8px; min-height:36px; display:flex; justify-content:space-between; align-items:center; cursor:pointer; touch-action:manipulation;">
                        <span style="font-size:0.75rem; font-weight:900; color:#f8fafc;">${pt.name}</span>
                        <span style="background:${isOn ? '#10b981' : '#1e293b'}; color:${isOn ? '#000' : '#94a3b8'}; font-weight:900; font-size:0.72rem; padding:2px 8px; border-radius:4px;">${isOn ? '🟢 ON' : '⚪ OFF'}</span>
                    </div>`;
                }
                const step = pt.integer ? 1 : ((pt.max - pt.min) <= 2 ? 0.01 : ((pt.max - pt.min) <= 25 ? 0.1 : 1));
                const dec = step < 0.1 ? 2 : (step < 1 ? 1 : 0);
                return `
                <div style="background:#111827; border:1px solid #1e293b; border-radius:6px; padding:3px 6px;">
                    <div style="display:flex; justify-content:space-between; font-size:0.72rem; font-weight:800; margin-bottom:1px;">
                        <span style="color:#cbd5e1; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; max-width:65%;">${pt.name}</span>
                        <span id="fs-slot-val-${ch}-${sIdx}-${pt.sym}" style="color:#38bdf8; font-family:monospace;">${Number(val).toFixed(dec)}${pt.unit ? ' ' + pt.unit : ''}</span>
                    </div>
                    <input type="range" min="${pt.min}" max="${pt.max}" step="${step}" value="${val}" data-default="${pt.def}" style="width:100%; height:24px; accent-color:#10b981; cursor:pointer; margin:0; touch-action:manipulation;" oninput="updateSlotLv2Param(${ch}, ${sIdx}, '${pt.sym}', this.value); var el=document.getElementById('fs-slot-val-${ch}-${sIdx}-${pt.sym}'); if(el) el.innerText=Number(this.value).toFixed(${dec});">
                </div>`;
            }).join("");

            const isActive = !slot.bypassed;
            const uriLower = (slot.uri || "").toLowerCase();
            const nameLower = (slot.name || "").toLowerCase();
            const isFileNam = (uriLower.includes("neural-amp-modeler") || uriLower.includes("aidadsp"));
            const isFileIr = (uriLower.includes("convolv") || (uriLower.includes("ir") && !uriLower.includes("tone3000")));
            const fileType = isFileNam ? "nam" : (isFileIr ? "ir" : null);
            let fileControlHtml = "";
            if (scalePort) {
                const curVal = Number((slot.params && slot.params[scalePort.sym] !== undefined) ? slot.params[scalePort.sym] : scalePort.def);
                const curSp = scalePort.scalePoints.find(sp => Math.abs(Number(sp.value) - curVal) < 0.01) || scalePort.scalePoints[0];
                const spJson = JSON.stringify(scalePort.scalePoints).replace(/"/g, '&quot;');
                fileControlHtml = `
                <div style="background:#0f172a; border:1px solid #818cf8; border-radius:6px; padding:3px 6px; margin-bottom:5px; flex-shrink:0;">
                    <div style="font-size:0.7rem; font-weight:800; color:#a5b4fc; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; margin-bottom:3px; display:flex; align-items:center; gap:4px;">
                        <span>🧠</span>
                        <span style="overflow:hidden; text-overflow:ellipsis;">${curSp ? curSp.label : scalePort.name}</span>
                    </div>
                    <div style="display:flex; gap:4px;">
                        <button onclick="stepSlotScalePoint(${ch}, ${sIdx}, '${scalePort.sym}', -1, ${spJson})" style="flex:1; min-height:30px; background:#1e1b4b; border:1px solid #818cf8; color:#ffffff; border-radius:4px; font-weight:900; font-size:0.72rem; cursor:pointer; touch-action:manipulation;">◀ PREV</button>
                        <button onclick="stepSlotScalePoint(${ch}, ${sIdx}, '${scalePort.sym}', 1, ${spJson})" style="flex:1; min-height:30px; background:#1e1b4b; border:1px solid #818cf8; color:#ffffff; border-radius:4px; font-weight:900; font-size:0.72rem; cursor:pointer; touch-action:manipulation;">NEXT ▶</button>
                    </div>
                </div>`;
            } else if (fileType) {
                const curFileLabel = (slot.params && (slot.params.file_label || (slot.params._file_path ? slot.params._file_path.split("/").pop().replace(/\.(nam|wav)$/i, '') : ''))) || (isFileNam ? 'DEFAULT NAM MODEL' : 'DEFAULT IR');
                fileControlHtml = `
                <div style="background:#0f172a; border:1px solid ${isFileNam ? '#818cf8' : '#22d3ee'}; border-radius:6px; padding:3px 6px; margin-bottom:5px; flex-shrink:0;">
                    <div style="font-size:0.7rem; font-weight:800; color:${isFileNam ? '#a5b4fc' : '#67e8f9'}; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; margin-bottom:3px; display:flex; align-items:center; gap:4px;">
                        <span>📂</span>
                        <span style="overflow:hidden; text-overflow:ellipsis;">${curFileLabel}</span>
                    </div>
                    <div style="display:flex; gap:4px;">
                        <button onclick="stepSlotFile(${ch}, ${sIdx}, '${fileType}', -1)" style="flex:1; min-height:30px; background:${isFileNam ? '#1e1b4b' : '#164e63'}; border:1px solid ${isFileNam ? '#818cf8' : '#22d3ee'}; color:#ffffff; border-radius:4px; font-weight:900; font-size:0.72rem; cursor:pointer; touch-action:manipulation;">◀ PREV</button>
                        <button onclick="stepSlotFile(${ch}, ${sIdx}, '${fileType}', 1)" style="flex:1; min-height:30px; background:${isFileNam ? '#1e1b4b' : '#164e63'}; border:1px solid ${isFileNam ? '#818cf8' : '#22d3ee'}; color:#ffffff; border-radius:4px; font-weight:900; font-size:0.72rem; cursor:pointer; touch-action:manipulation;">NEXT ▶</button>
                    </div>
                </div>`;
            }

            return `
            <div style="background:linear-gradient(180deg, ${isActive ? '#071f18' : '#111827'} 0%, #090d16 100%); border:1.5px solid ${isActive ? '#10b981' : '#475569'}; border-radius:10px; padding:8px 10px; display:flex; flex-direction:column; justify-content:space-between; height:100%; box-sizing:border-box; overflow:hidden;">
                <div style="flex:1; min-height:0; display:flex; flex-direction:column; overflow:hidden;">
                    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px; flex-shrink:0;">
                        <span style="background:${isActive ? '#059669' : '#334155'}; color:#fff; font-weight:900; font-size:0.74rem; padding:2px 7px; border-radius:4px;">SLOT ${sIdx + 1}</span>
                        <span style="color:#6ee7b7; font-size:0.68rem; font-weight:800; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; max-width:55%;">${slot.category || 'LV2 FX'}</span>
                    </div>
                    <div style="font-size:0.92rem; font-weight:900; color:#ffffff; margin-bottom:4px; line-height:1.2; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; flex-shrink:0;">${slot.icon || '🎸'} ${slot.name}</div>
                    <button onclick="toggleSlotBypass(${ch}, ${sIdx})" style="width:100%; min-height:36px; border-radius:6px; font-weight:900; font-size:0.78rem; cursor:pointer; margin-bottom:6px; background:${isActive ? '#059669' : '#334155'}; border:1.5px solid ${isActive ? '#34d399' : '#64748b'}; color:#ffffff; touch-action:manipulation; flex-shrink:0;">
                        ${isActive ? '🟢 EFFECT ACTIVE' : '🔴 BYPASSED'}
                    </button>
                    ${fileControlHtml}
                    <div style="display:flex; flex-direction:column; gap:4px; overflow-y:auto; flex:1; min-height:0; padding-right:2px;">
                        ${quickControlsHtml}
                    </div>
                </div>
                <div style="display:flex; flex-direction:column; gap:4px; margin-top:6px; flex-shrink:0;">
                    <button onclick="openSlotModGuiModal(${ch}, ${sIdx}, 'modgui')" style="min-height:38px; background:linear-gradient(135deg, #2563eb 0%, #4f46e5 100%); border:1.5px solid #60a5fa; color:#ffffff; border-radius:6px; font-size:0.82rem; font-weight:900; cursor:pointer;">
                        🎛️ OPEN MOD-GUI &amp; CONTROLS
                    </button>
                    <div style="display:grid; grid-template-columns:1fr 1fr; gap:4px;">
                        <button onclick="openPluginPickerWithCategory(${ch}, ${sIdx}, 'all')" style="min-height:32px; background:#1e293b; border:1px solid #38bdf8; color:#38bdf8; border-radius:6px; font-size:0.74rem; font-weight:900; cursor:pointer;">🔄 CHANGE</button>
                        <button onclick="activePickerChannel=${ch}; activePickerSlot=${sIdx}; assignSlotPlugin(0);" style="min-height:32px; background:#450a0a; border:1px solid #ef4444; color:#fecaca; border-radius:6px; font-size:0.74rem; font-weight:900; cursor:pointer;">🗑️ CLEAR</button>
                    </div>
                </div>
            </div>`;
        }).join("");

        const stObj = window.channelDualDspState[ch];
        const isSlotFirst = !!stObj.slotFirst;
        const isModFirst = !!stObj.modguiFirst;
        const insMode = (stObj.insertMode !== undefined) ? stObj.insertMode : 1; // 0=Direct, 1=Slots, 2=Pedalboard
        const preChainParts = [];
        const postChainParts = [];
        if (isSlotFirst) preChainParts.push("SLOTS 1–2"); else postChainParts.push("SLOTS 1–2");
        if (isModFirst) preChainParts.push("MOD-GUI 3–4"); else postChainParts.push("MOD-GUI 3–4");
        const pathDesc = (insMode === 0) ? "⚡ DIRECT CLEAN (NO INSERT)" :
            ((insMode === 2) ? "🎸 MODEP PEDALBOARD ONLY (EXCLUSIVE LOOP)" :
            `${preChainParts.length ? `[${preChainParts.join(' + ')}] &rarr; ` : ''}1176 &rarr; ${postChainParts.length ? `[${postChainParts.join(' + ')}] &rarr; ` : ''}🛡️ DENOISER &rarr; EQ &rarr; COMP`);

        container.innerHTML = `
            <div style="display:flex; justify-content:space-between; align-items:center; background:#0b111e; padding:6px 14px; border-bottom:1.5px solid #1e293b; flex-shrink:0; gap:8px; flex-wrap:wrap;">
                <div style="display:flex; align-items:center; gap:8px; flex-wrap:wrap;">
                    <span style="font-size:0.95rem; font-weight:900; color:#34d399;">🎸 IN ${ch + 1}: ${chName} — 4-SLOT EFFECT RACK</span>
                    <!-- 3-Way Exclusive Insert Mode Selector (Rule 28) -->
                    <div style="display:flex; background:#090d16; border:1.5px solid #334155; border-radius:6px; overflow:hidden; padding:2px; gap:2px;">
                        <button onclick="setChannelInsertMode(${ch}, 0)" style="min-height:28px; padding:2px 8px; border:none; border-radius:4px; font-weight:900; font-size:0.7rem; cursor:pointer; background:${insMode === 0 ? '#475569' : 'transparent'}; color:${insMode === 0 ? '#ffffff' : '#94a3b8'}; touch-action:manipulation;">⚡ DIRECT</button>
                        <button onclick="setChannelInsertMode(${ch}, 1)" style="min-height:28px; padding:2px 8px; border:none; border-radius:4px; font-weight:900; font-size:0.7rem; cursor:pointer; background:${insMode === 1 ? '#059669' : 'transparent'}; color:${insMode === 1 ? '#ffffff' : '#94a3b8'}; touch-action:manipulation;">🎛️ 4 SLOTS ONLY</button>
                        <button onclick="setChannelInsertMode(${ch}, 2)" style="min-height:28px; padding:2px 8px; border:none; border-radius:4px; font-weight:900; font-size:0.7rem; cursor:pointer; background:${insMode === 2 ? '#b45309' : 'transparent'}; color:${insMode === 2 ? '#ffffff' : '#94a3b8'}; touch-action:manipulation;">🎸 PEDALBOARD ONLY</button>
                    </div>
                    <button onclick="toggleSlotFirst(${ch})" style="min-height:32px; padding:3px 8px; border-radius:6px; font-weight:900; font-size:0.72rem; cursor:pointer; background:${isSlotFirst ? '#065f46' : '#111827'}; border:1.5px solid ${isSlotFirst ? '#34d399' : '#334155'}; color:${isSlotFirst ? '#ecfdf5' : '#94a3b8'};">
                        ⚡ SLOTS 1&amp;2: ${isSlotFirst ? '1ST (PRE-1176)' : 'POST-1176'}
                    </button>
                    <button onclick="toggleModguiFirst(${ch})" style="min-height:32px; padding:3px 8px; border-radius:6px; font-weight:900; font-size:0.72rem; cursor:pointer; background:${isModFirst ? '#78350f' : '#111827'}; border:1.5px solid ${isModFirst ? '#f59e0b' : '#334155'}; color:${isModFirst ? '#fef3c7' : '#94a3b8'};">
                        🎸 MOD-GUI: ${isModFirst ? '1ST (PRE-1176)' : 'POST-1176'}
                    </button>
                    <span style="background:#1e1b4b; border:1px solid #818cf8; color:#e0e7ff; font-size:0.68rem; font-weight:800; padding:2px 6px; border-radius:4px;">
                        PATH: ${pathDesc}
                    </span>
                </div>
                <div style="display:flex; gap:6px; align-items:center;">
                    <button onclick="closeFullScreenSlotsPage(); showDspRackPage(${ch});" style="min-height:34px; padding:4px 10px; background:#1e293b; border:1px solid #38bdf8; color:#38bdf8; border-radius:6px; font-weight:900; font-size:0.78rem; cursor:pointer;">🎛️ DSP RACK</button>
                    <button onclick="closeFullScreenSlotsPage()" style="min-height:34px; padding:4px 12px; background:#dc2626; border:1px solid #f87171; color:#ffffff; border-radius:6px; font-weight:900; font-size:0.8rem; cursor:pointer;">✕ CLOSE</button>
                </div>
            </div>
            <div style="display:flex; gap:5px; padding:4px 12px; background:#090d16; border-bottom:1px solid #1e293b; overflow-x:auto; flex-shrink:0;">
                ${chTabsHtml}
            </div>
            <div style="flex:1; display:grid; grid-template-columns:repeat(4, 1fr); gap:8px; padding:8px 12px; overflow:hidden; min-height:0;">
                ${slotColumnsHtml}
            </div>
        `;
    };

    // 2. 100% FULL-SCREEN EFFECT PICKER PAGE WITH CATEGORY FILTER BUTTONS
    // =========================================================================
    window.openPluginPickerWithCategory = function(ch, slotIdx, categoryKey) {
        activePickerCategory = categoryKey || "all";
        activePickerQuery = "";
        window.openPluginPickerModal(ch, slotIdx, true);
    };

    // When clicking a slot on the channel strip, open the 100% Full-Screen 4-Slot Channel Rack Page!
    // When forcePicker === true, open the 100% Full-Screen Effect Picker Page with Category Buttons!
    window.openPluginPickerModal = function(ch, slotIdx, forcePicker) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        activePickerChannel = ch;
        activePickerSlot = slotIdx;
        if (!forcePicker) {
            window.openFullScreenSlotsPage(ch, slotIdx);
            return;
        }
        const overlay = document.getElementById("plugin-picker-overlay");
        const title = document.getElementById("plugin-picker-title");
        const chName = (window.customChannelNames && window.customChannelNames["ch-" + ch]) || (window.channelNames && window.channelNames[ch]) || `CH ${ch + 1}`;
        if (title) title.innerHTML = `🎸 IN ${ch + 1} (${chName}) &bull; Slot ${slotIdx + 1} — Fullscreen Effect Picker`;
        window.renderPluginPickerList(activePickerQuery, activePickerCategory);
        if (overlay) {
            overlay.style.display = "flex";
            overlay.classList.add("open");
        }
    };

    window.openPluginPicker = function(ch, slotIdx) {
        window.openPluginPickerModal(ch, slotIdx, true);
    };

    window.setPickerCategory = function(catKey) {
        activePickerCategory = catKey;
        const input = document.getElementById("lv2-picker-search");
        const q = input ? input.value : activePickerQuery;
        window.renderPluginPickerList(q, catKey);
    };

    window.renderPluginPickerList = function(query, catKey) {
        const list = document.getElementById("plugin-picker-list");
        if (!list) return;
        if (catKey !== undefined) activePickerCategory = catKey;
        activePickerQuery = (query !== undefined) ? query : activePickerQuery;
        const q = (activePickerQuery || "").trim().toLowerCase();

        const items = window.MODEP_LV2_CATALOG.filter((p, idx) => {
            if (idx === 0) return false; // Handle EMPTY/THRU via dedicated top button
            const pCatKey = p.catKey || inferPluginCategoryKey(p.uri, p.name, p.category);
            const isCyber = !!(p.isCyber || (p.uri && (p.uri.includes("cyber") || p.uri.includes("danny") || p.uri.includes("dusk") || p.uri.includes("nam"))));
            if (activePickerCategory === "cyber" && !isCyber) return false;
            if (activePickerCategory !== "all" && activePickerCategory !== "cyber" && pCatKey !== activePickerCategory) return false;
            if (q) {
                const hay = `${p.name} ${p.category || ""} ${p.uri || ""} ${p.brand || ""}`.toLowerCase();
                if (!hay.includes(q)) return false;
            }
            return true;
        });

        const catBtnsHtml = PICKER_CATEGORIES.map(c => {
            const isSel = (activePickerCategory === c.key);
            return `<button onclick="setPickerCategory('${c.key}')" style="min-height:50px; padding:8px 14px; border-radius:8px; font-weight:900; font-size:0.84rem; cursor:pointer; background:${isSel ? '#0284c7' : '#111827'}; border:2px solid ${isSel ? '#38bdf8' : '#334155'}; color:${isSel ? '#ffffff' : '#cbd5e1'}; white-space:nowrap; touch-action:manipulation;">
                ${c.label}
            </button>`;
        }).join("");

        const cardsHtml = items.slice(0, 120).map(p => {
            const catIdx = window.MODEP_LV2_CATALOG.indexOf(p);
            const isCyber = !!(p.isCyber || (p.uri && p.uri.includes("cyber")));
            const stBadge = p.stereo ? `<span style="background:#0c4a6e; color:#38bdf8; padding:2px 6px; border-radius:4px; font-size:0.68rem; font-weight:900;">STEREO</span>` : `<span style="background:#1e293b; color:#94a3b8; padding:2px 6px; border-radius:4px; font-size:0.68rem; font-weight:800;">MONO</span>`;
            const cyberBadge = isCyber ? `<span style="background:#4c1d95; color:#f5d0fe; padding:2px 7px; border-radius:4px; font-size:0.68rem; font-weight:900;">⭐ CYBER CUSTOM</span>` : "";
            return `
            <div onclick="assignSlotPlugin(${catIdx})" style="background:linear-gradient(180deg, ${isCyber ? '#131129' : '#111827'} 0%, #0b0f19 100%); border:2px solid ${isCyber ? '#6366f1' : '#1e293b'}; border-radius:10px; padding:14px; min-height:88px; display:flex; flex-direction:column; justify-content:space-between; cursor:pointer; touch-action:manipulation; box-sizing:border-box;">
                <div style="font-size:0.98rem; font-weight:900; color:#f8fafc; line-height:1.25;">${p.icon || '🔌'} ${p.name}</div>
                <div style="display:flex; justify-content:space-between; align-items:center; gap:6px; margin-top:8px; flex-wrap:wrap;">
                    <span style="font-size:0.74rem; color:#94a3b8; font-weight:700;">${p.category || 'LV2 Effect'}</span>
                    <div style="display:flex; gap:4px;">${cyberBadge}${stBadge}</div>
                </div>
            </div>`;
        }).join("");

        list.innerHTML = `
            <div style="display:flex; gap:8px; flex-wrap:wrap; margin-bottom:10px; flex-shrink:0;">
                ${catBtnsHtml}
            </div>
            <div style="display:flex; gap:10px; margin-bottom:12px; flex-shrink:0;">
                <input type="text" id="lv2-picker-search" placeholder="🔍 Search effects by name, amp model, or type (e.g. Vibroverb, Dumble, Klon, Screamer, Bass9, DuskVerb)..." value="${activePickerQuery || ''}" oninput="renderPluginPickerList(this.value)" style="flex:1; min-height:48px; background:#0f172a; color:#f8fafc; border:2px solid #334155; border-radius:8px; padding:8px 14px; font-size:0.95rem; font-weight:700;">
                <button onclick="assignSlotPlugin(0)" style="min-height:48px; padding:0 18px; background:#450a0a; border:2px solid #ef4444; color:#fecaca; border-radius:8px; font-weight:900; font-size:0.85rem; cursor:pointer;">🗑️ SET SLOT TO EMPTY (THRU)</button>
            </div>
            <div style="flex:1; overflow-y:auto; display:grid; grid-template-columns:repeat(auto-fill, minmax(265px, 1fr)); gap:10px; align-content:start; padding-right:4px;">
                ${cardsHtml || `<div style="grid-column:1/-1; text-align:center; padding:40px; color:#94a3b8; font-size:1.1rem;">No effects matched "${activePickerQuery}". Tap <strong>🌐 ALL FX</strong> above.</div>`}
            </div>
        `;
        const input = document.getElementById("lv2-picker-search");
        if (input && query) { input.focus(); input.setSelectionRange(query.length, query.length); }
    };

    window.closePluginPickerModal = function() {
        const overlay = document.getElementById("plugin-picker-overlay");
        if (overlay) { overlay.style.display = "none"; overlay.classList.remove("open"); }
    };

    window.assignSlotPlugin = function(catalogIdxOrName, chOpt, slotIdxOpt) {
        const ch = (typeof chOpt === "number") ? chOpt : activePickerChannel;
        const slotIdx = (typeof slotIdxOpt === "number") ? slotIdxOpt : activePickerSlot;
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const plugin = (typeof catalogIdxOrName === "number")
            ? window.MODEP_LV2_CATALOG[catalogIdxOrName]
            : window.MODEP_LV2_CATALOG.find(p => p.name === catalogIdxOrName || p.uri === catalogIdxOrName);
        if (!plugin || plugin.uri === "thru") {
            window.channelDualDspState[ch].slots[slotIdx] = createEmptySlot();
            saveDualDspState();
            window.syncLinearSlotChain(ch);
            window.closePluginPickerModal();
            return;
        }
        const defaultParams = {};
        (plugin.ports || []).forEach(pt => { defaultParams[pt.sym] = pt.def; });
        delete defaultParams["bypass"];
        delete defaultParams[":bypass"];
        let initFilePath = "";
        let initFileLabel = "";
        if (plugin.uri.includes("neural-amp-modeler") || plugin.uri.includes("aidadsp")) {
            if (window.MODEP_USER_FILES && Array.isArray(window.MODEP_USER_FILES.nam) && window.MODEP_USER_FILES.nam.length > 0) {
                const firstNam = window.MODEP_USER_FILES.nam[0];
                initFilePath = (typeof firstNam === "object" && firstNam.path) ? firstNam.path : (typeof firstNam === "string" ? firstNam : "");
                initFileLabel = (typeof firstNam === "object" && firstNam.label) ? firstNam.label : (initFilePath.split("/").pop().replace(/\.nam$/i, ""));
                defaultParams._file_path = initFilePath;
                defaultParams.model_file = initFilePath;
                defaultParams.model = initFilePath;
                defaultParams.file_label = initFileLabel;
            }
        }
        window.channelDualDspState[ch].slots[slotIdx] = {
            uri: plugin.uri,
            name: plugin.name,
            category: plugin.category,
            icon: plugin.icon || "🎸",
            stereo: !!plugin.stereo,
            bypassed: false,
            file_path: initFilePath,
            file_label: initFileLabel,
            params: defaultParams
        };
        saveDualDspState();
        window.syncLinearSlotChain(ch);
        window.closePluginPickerModal();
        window.openSlotModGuiModal(ch, slotIdx, "modgui");
    };

    // =========================================================================
    // 3. 100% FULL-SCREEN MOD-GUI & LARGE TOUCH CONTROL VIEW
    // =========================================================================
    window.selectSlotFile = function(ch, slotIdx, listType, filePath, fileLabel) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const slot = window.channelDualDspState[ch].slots[slotIdx];
        if (!slot) return;
        if (!slot.params) slot.params = {};
        slot.params._file_path = filePath;
        slot.params.model_file = filePath;
        slot.params.file_label = fileLabel || filePath.split("/").pop();
        slot.file_path = filePath;
        slot.file_label = fileLabel || filePath.split("/").pop();
        saveDualDspState();

        const iframe = document.getElementById("slot-modgui-iframe");
        if (iframe && iframe.contentWindow) {
            iframe.contentWindow.postMessage({ type: "set_file", file: filePath, label: fileLabel }, "*");
            iframe.contentWindow.postMessage({ type: "modgui_param_change", symbol: "model_file", value: filePath }, "*");
        }

        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: "set_lv2_slot_file",
                channel: ch,
                ch: ch,
                slot: slotIdx,
                uri: slot.uri,
                file_path: filePath,
                file_label: fileLabel || (slot.params && slot.params.file_label) || filePath.split("/").pop(),
                file_type: listType
            }));
        }
        window.refreshControlStage(ch, slotIdx);
        const fsOverlay = document.getElementById("fullscreen-slots-overlay");
        if (fsOverlay && fsOverlay.classList.contains("open")) {
            window.renderFullScreenSlotsPage(ch);
        }
    };

    window.stepSlotFile = function(ch, slotIdx, listType, delta) {
        const slot = window.channelDualDspState[ch]?.slots[slotIdx];
        if (!slot) return;
        const list = (window.MODEP_USER_FILES && window.MODEP_USER_FILES[listType]) || [];
        if (!list.length) return;
        const curPath = (slot.params && slot.params._file_path) || "";
        let curIdx = list.findIndex(f => f.path === curPath);
        if (curIdx < 0) curIdx = 0;
        let nextIdx = (curIdx + delta + list.length) % list.length;
        const nextItem = list[nextIdx];
        if (nextItem) window.selectSlotFile(ch, slotIdx, listType, nextItem.path, nextItem.label || nextItem.name);
    };

    window.stepSlotScalePoint = function(ch, slotIdx, sym, delta, scalePoints) {
        if (!Array.isArray(scalePoints) || !scalePoints.length) return;
        const slot = window.channelDualDspState[ch]?.slots[slotIdx];
        if (!slot) return;
        const curVal = Number((slot.params && slot.params[sym] !== undefined) ? slot.params[sym] : scalePoints[0].value);
        let curIdx = scalePoints.findIndex(sp => Math.abs(Number(sp.value) - curVal) < 0.01);
        if (curIdx < 0) curIdx = 0;
        let nextIdx = (curIdx + delta + scalePoints.length) % scalePoints.length;
        const nextVal = scalePoints[nextIdx].value;
        window.updateSlotLv2Param(ch, slotIdx, sym, nextVal);
        window.refreshControlStage(ch, slotIdx);
        const fsOverlay = document.getElementById("fullscreen-slots-overlay");
        if (fsOverlay && fsOverlay.classList.contains("open")) {
            window.renderFullScreenSlotsPage(ch);
        }
    };

    function renderControlViewGrid(ch, slotIdx, slot, catalogEntry) {
        const catKey = catalogEntry.catKey || inferPluginCategoryKey(slot.uri, slot.name, slot.category);
        const fallbackPorts = (catKey === "amp") ? AMP_PORTS : ((catKey === "mod") ? SYNTH9_PORTS : DRIVE_PORTS);
        const ports = (catalogEntry.ports && catalogEntry.ports.length) ? catalogEntry.ports : fallbackPorts;
        const presets = catalogEntry.presets || [];
        const uLower = (slot.uri + " " + (slot.name || "")).toLowerCase();

        // 1. Preset Bank Card
        const presetHtml = presets.length ? `
            <div style="display:flex; align-items:center; gap:12px; margin-bottom:12px; background:#0f172a; padding:10px 14px; border-radius:10px; border:1.5px solid #334155;">
                <span style="font-size:0.88rem; font-weight:900; color:#38bdf8;">🎯 PRESET BANK:</span>
                <select onchange="loadSlotLv2Preset(${ch}, ${slotIdx}, this.value)" style="flex:1; min-height:46px; background:#090d16; color:#f8fafc; border:1.5px solid #38bdf8; border-radius:8px; padding:6px 12px; font-size:0.9rem; font-weight:800;">
                    <option value="">-- Select Preset (${presets.length} available) --</option>
                    ${presets.map(pr => `<option value="${pr.uri}" ${slot.preset === pr.uri ? 'selected' : ''}>${pr.label}</option>`).join('')}
                </select>
            </div>` : '';

        // 2. LIVE FILE SELECTOR CARDS (NAM Neural Models & Cabinet/Reverb WAV IRs)
        let fileCardsHtml = "";
        const isNam = uLower.includes("nam") || uLower.includes("neural") || uLower.includes("aida") ||
            (catalogEntry.parameters && catalogEntry.parameters.some(p => (p.fileTypes || []).some(f => f.includes("nam") || f.includes("aida") || f.includes("json"))));
        const isIr = uLower.includes("ir") || uLower.includes("convo") || uLower.includes("cab") || uLower.includes("reverb") ||
            (catalogEntry.parameters && catalogEntry.parameters.some(p => (p.fileTypes || []).some(f => f.includes("ir") || f.includes("cabsim") || f.includes("wav"))));

        if (isNam) {
            const namList = (window.MODEP_USER_FILES && window.MODEP_USER_FILES.nam) || [];
            const curNamPath = (slot.params && slot.params._file_path) || (namList[0] ? namList[0].path : "");
            const curItem = namList.find(f => f.path === curNamPath) || namList[0] || { label: "No NAM models found", category: "Models" };

            // Group by category/folder
            const grouped = {};
            namList.forEach(f => {
                const cat = f.category || "General";
                if (!grouped[cat]) grouped[cat] = [];
                grouped[cat].push(f);
            });

            const optGroupsHtml = Object.keys(grouped).map(cat => {
                const opts = grouped[cat].map(f => `<option value="${f.path}" ${f.path === curNamPath ? 'selected' : ''}>${f.label}</option>`).join("");
                return `<optgroup label="📁 ${cat}">${opts}</optgroup>`;
            }).join("");

            fileCardsHtml += `
            <div style="grid-column:1/-1; background:linear-gradient(135deg, #181028 0%, #0d091a 100%); border:2.5px solid #818cf8; border-radius:12px; padding:10px 14px; margin-bottom:4px; display:flex; flex-direction:column; gap:8px; box-shadow:0 8px 24px rgba(0,0,0,0.6); max-width:100%; overflow:hidden;">
                <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px; min-width:0;">
                    <div style="display:flex; align-items:center; gap:8px; min-width:0;">
                        <span style="font-size:0.95rem; font-weight:900; color:#e0e7ff;">📂 NEURAL AMP / RIG MODEL (.NAM)</span>
                        <span style="background:#4338ca; color:#e0e7ff; font-size:0.72rem; font-weight:900; padding:2px 8px; border-radius:6px;">${namList.length} MODELS</span>
                    </div>
                    <span style="font-size:0.8rem; font-weight:800; color:#a5b4fc; font-family:monospace; background:#1e1b4b; padding:3px 8px; border-radius:6px; max-width:48%; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">ACTIVE: ${curItem.label}</span>
                </div>
                <div style="display:flex; align-items:center; gap:8px; width:100%; min-width:0;">
                    <button onclick="stepSlotFile(${ch}, ${slotIdx}, 'nam', -1)" style="flex:0 0 auto; min-width:86px; min-height:48px; background:#1e1b4b; border:2px solid #818cf8; color:#fff; border-radius:8px; font-weight:900; font-size:0.95rem; cursor:pointer; touch-action:manipulation;">◀ PREV</button>
                    <select onchange="window.selectSlotFile(${ch}, ${slotIdx}, 'nam', this.value, this.options[this.selectedIndex].text)" style="flex:1 1 0%; width:0; min-width:0; max-width:100%; min-height:48px; background:#0f0d24; color:#fef08a; border:2px solid #818cf8; border-radius:8px; padding:6px 10px; font-size:0.9rem; font-weight:900; overflow:hidden; text-overflow:ellipsis;">
                        ${optGroupsHtml || '<option value="">-- No .nam files found --</option>'}
                    </select>
                    <button onclick="stepSlotFile(${ch}, ${slotIdx}, 'nam', 1)" style="flex:0 0 auto; min-width:86px; min-height:48px; background:#1e1b4b; border:2px solid #818cf8; color:#fff; border-radius:8px; font-weight:900; font-size:0.95rem; cursor:pointer; touch-action:manipulation;">NEXT ▶</button>
                </div>
            </div>`;
        }

        if (isIr) {
            const irList = (window.MODEP_USER_FILES && window.MODEP_USER_FILES.ir) || [];
            const curIrPath = (slot.params && slot.params._file_path) || (irList[0] ? irList[0].path : "");
            const curItem = irList.find(f => f.path === curIrPath) || irList[0] || { label: "No IR files found", category: "IRs" };

            // Group by category/folder
            const grouped = {};
            irList.forEach(f => {
                const cat = f.category || "General";
                if (!grouped[cat]) grouped[cat] = [];
                grouped[cat].push(f);
            });

            const optGroupsHtml = Object.keys(grouped).map(cat => {
                const opts = grouped[cat].map(f => `<option value="${f.path}" ${f.path === curIrPath ? 'selected' : ''}>${f.label}</option>`).join("");
                return `<optgroup label="🏛️ ${cat}">${opts}</optgroup>`;
            }).join("");

            fileCardsHtml += `
            <div style="grid-column:1/-1; background:linear-gradient(135deg, #091f24 0%, #061317 100%); border:2.5px solid #22d3ee; border-radius:12px; padding:10px 14px; margin-bottom:4px; display:flex; flex-direction:column; gap:8px; box-shadow:0 8px 24px rgba(0,0,0,0.6); max-width:100%; overflow:hidden;">
                <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px; min-width:0;">
                    <div style="display:flex; align-items:center; gap:8px; min-width:0;">
                        <span style="font-size:0.95rem; font-weight:900; color:#cffafe;">📂 IMPULSE RESPONSE (.WAV IR)</span>
                        <span style="background:#0e7490; color:#cffafe; font-size:0.72rem; font-weight:900; padding:2px 8px; border-radius:6px;">${irList.length} IRS</span>
                    </div>
                    <span style="font-size:0.8rem; font-weight:800; color:#67e8f9; font-family:monospace; background:#164e63; padding:3px 8px; border-radius:6px; max-width:48%; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">ACTIVE: ${curItem.label}</span>
                </div>
                <div style="display:flex; align-items:center; gap:8px; width:100%; min-width:0;">
                    <button onclick="stepSlotFile(${ch}, ${slotIdx}, 'ir', -1)" style="flex:0 0 auto; min-width:86px; min-height:48px; background:#164e63; border:2px solid #22d3ee; color:#fff; border-radius:8px; font-weight:900; font-size:0.95rem; cursor:pointer; touch-action:manipulation;">◀ PREV</button>
                    <select onchange="window.selectSlotFile(${ch}, ${slotIdx}, 'ir', this.value, this.options[this.selectedIndex].text)" style="flex:1 1 0%; width:0; min-width:0; max-width:100%; min-height:48px; background:#082f49; color:#fef08a; border:2px solid #22d3ee; border-radius:8px; padding:6px 10px; font-size:0.9rem; font-weight:900; overflow:hidden; text-overflow:ellipsis;">
                        ${optGroupsHtml || '<option value="">-- No .wav IR files found --</option>'}
                    </select>
                    <button onclick="stepSlotFile(${ch}, ${slotIdx}, 'ir', 1)" style="flex:0 0 auto; min-width:86px; min-height:48px; background:#164e63; border:2px solid #22d3ee; color:#fff; border-radius:8px; font-weight:900; font-size:0.95rem; cursor:pointer; touch-action:manipulation;">NEXT ▶</button>
                </div>
            </div>`;
        }

        // Touch Sub-Menu Pagination when ports.length > 6 so cards fit 100% on 960x600 without scrolling
        const activePage = window._slotControlSubPage || "p1";
        let pageSubMenuHtml = "";
        let visiblePorts = ports;
        if (ports.length > 6) {
            const p1Count = 6;
            const p2Count = ports.length - p1Count;
            if (activePage === "p1") visiblePorts = ports.slice(0, p1Count);
            else if (activePage === "p2") visiblePorts = ports.slice(p1Count);
            pageSubMenuHtml = `
            <div style="display:flex; align-items:center; gap:8px; margin-bottom:10px; background:#0b111e; padding:6px 10px; border-radius:10px; border:1.5px solid #1e293b;">
                <span style="font-size:0.76rem; font-weight:900; color:#94a3b8; letter-spacing:0.6px; margin-right:4px;">🎛️ PARAMETER BANK:</span>
                <button onclick="window.setSlotControlPage(${ch}, ${slotIdx}, 'p1')" style="flex:1; min-height:40px; border-radius:8px; font-weight:900; font-size:0.82rem; cursor:pointer; background:${activePage === 'p1' ? '#0284c7' : '#111827'}; border:2px solid ${activePage === 'p1' ? '#38bdf8' : '#334155'}; color:${activePage === 'p1' ? '#fff' : '#cbd5e1'}; touch-action:manipulation;">🎛️ MAIN CONTROLS (1–${p1Count})</button>
                <button onclick="window.setSlotControlPage(${ch}, ${slotIdx}, 'p2')" style="flex:1; min-height:40px; border-radius:8px; font-weight:900; font-size:0.82rem; cursor:pointer; background:${activePage === 'p2' ? '#0284c7' : '#111827'}; border:2px solid ${activePage === 'p2' ? '#38bdf8' : '#334155'}; color:${activePage === 'p2' ? '#fff' : '#cbd5e1'}; touch-action:manipulation;">🎚️ EXTENDED / TONE (${p1Count + 1}–${ports.length})</button>
                <button onclick="window.setSlotControlPage(${ch}, ${slotIdx}, 'all')" style="flex:0 0 auto; padding:0 14px; min-height:40px; border-radius:8px; font-weight:900; font-size:0.8rem; cursor:pointer; background:${activePage === 'all' ? '#0284c7' : '#111827'}; border:2px solid ${activePage === 'all' ? '#38bdf8' : '#334155'}; color:${activePage === 'all' ? '#fff' : '#94a3b8'}; touch-action:manipulation;">📋 ALL (${ports.length})</button>
            </div>`;
        }

        const cards = visiblePorts.map(pt => {
            const val = (slot.params && slot.params[pt.sym] !== undefined) ? slot.params[pt.sym] : pt.def;

            // Multi-choice Enumeration / ScalePoints
            if (pt.scalePoints && pt.scalePoints.length > 0) {
                // If <= 4 choices, render sleek touch segmented buttons
                if (pt.scalePoints.length <= 4) {
                    const segBtns = pt.scalePoints.map(sp => {
                        const sel = Math.abs(Number(val) - Number(sp.value)) < 0.01;
                        return `<button onclick="updateSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', ${sp.value}); refreshControlStage(${ch}, ${slotIdx});" style="flex:1; min-height:52px; border-radius:8px; font-weight:900; font-size:0.86rem; cursor:pointer; background:${sel ? '#0284c7' : '#090d16'}; border:2px solid ${sel ? '#38bdf8' : '#334155'}; color:${sel ? '#ffffff' : '#94a3b8'}; touch-action:manipulation;">${sp.label}</button>`;
                    }).join("");
                    return `
                    <div style="background:#111827; border:2px solid #1e293b; border-radius:12px; padding:12px; display:flex; flex-direction:column; gap:8px;">
                        <span style="font-size:0.92rem; font-weight:900; color:#e2e8f0; text-transform:uppercase;">📋 ${pt.name}</span>
                        <div style="display:flex; gap:8px; flex-wrap:wrap;">${segBtns}</div>
                    </div>`;
                }

                // If > 4 choices (e.g. NAM Loader 24 models, Evans Bass Pro, GxCabinet), render touch step selector + dropdown!
                const curSp = pt.scalePoints.find(sp => Math.abs(Number(sp.value) - Number(val)) < 0.01) || pt.scalePoints[0];
                const spOptionsHtml = pt.scalePoints.map(sp => {
                    const sel = Math.abs(Number(val) - Number(sp.value)) < 0.01;
                    return `<option value="${sp.value}" ${sel ? 'selected' : ''}>${sp.label}</option>`;
                }).join("");

                return `
                <div style="background:#111827; border:2px solid #0284c7; border-radius:12px; padding:12px; display:flex; flex-direction:column; gap:8px; min-width:0;">
                    <div style="display:flex; justify-content:space-between; align-items:center; gap:6px; min-width:0;">
                        <span style="font-size:0.9rem; font-weight:900; color:#38bdf8; text-transform:uppercase;">📋 ${pt.name}</span>
                        <span style="font-size:0.8rem; font-weight:900; color:#fde68a; font-family:monospace; background:#0c4a6e; padding:3px 8px; border-radius:6px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">${curSp ? curSp.label : ''}</span>
                    </div>
                    <div style="display:flex; align-items:center; gap:8px; width:100%; min-width:0;">
                        <button onclick="stepSlotScalePoint(${ch}, ${slotIdx}, '${pt.sym}', -1, ${JSON.stringify(pt.scalePoints).replace(/"/g, '&quot;')})" style="flex:0 0 auto; min-width:52px; min-height:48px; background:#1e293b; border:1.5px solid #38bdf8; color:#fff; border-radius:8px; font-size:1.1rem; font-weight:900; cursor:pointer; touch-action:manipulation;">◀</button>
                        <select onchange="updateSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', this.value); refreshControlStage(${ch}, ${slotIdx});" style="flex:1 1 0%; width:0; min-width:0; min-height:48px; background:#090d16; color:#f8fafc; border:1.5px solid #38bdf8; border-radius:8px; padding:6px 10px; font-size:0.88rem; font-weight:800; overflow:hidden; text-overflow:ellipsis;">
                            ${spOptionsHtml}
                        </select>
                        <button onclick="stepSlotScalePoint(${ch}, ${slotIdx}, '${pt.sym}', 1, ${JSON.stringify(pt.scalePoints).replace(/"/g, '&quot;')})" style="flex:0 0 auto; min-width:52px; min-height:48px; background:#1e293b; border:1.5px solid #38bdf8; color:#fff; border-radius:8px; font-size:1.1rem; font-weight:900; cursor:pointer; touch-action:manipulation;">▶</button>
                    </div>
                </div>`;
            }

            // Toggle Switch -> MASSIVE Full-Card Touchscreen Hit Target (>= 84px height!)
            if (isSwitchLikePort(pt)) {
                const isOn = Number(val) >= 0.5;
                const nextVal = isOn ? 0 : 1;
                return `
                <div onclick="updateSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', ${nextVal}); refreshControlStage(${ch}, ${slotIdx});" style="background:linear-gradient(135deg, ${isOn ? '#064e3b' : '#111827'} 0%, ${isOn ? '#042f2e' : '#0b0f19'} 100%); border:2.5px solid ${isOn ? '#10b981' : '#334155'}; border-radius:12px; padding:12px 16px; min-height:84px; display:flex; justify-content:space-between; align-items:center; cursor:pointer; touch-action:manipulation; user-select:none; box-sizing:border-box; box-shadow:${isOn ? '0 0 20px rgba(16,185,129,0.28)' : 'none'};">
                    <div style="display:flex; flex-direction:column; gap:4px;">
                        <span style="font-size:0.95rem; font-weight:900; color:#ffffff; text-transform:uppercase;">⚡ ${pt.name}</span>
                        <span style="font-size:0.75rem; color:${isOn ? '#6ee7b7' : '#94a3b8'}; font-weight:800;">TAP ANYWHERE ON CARD TO TOGGLE</span>
                    </div>
                    <div style="min-width:124px; min-height:54px; border-radius:10px; background:${isOn ? '#10b981' : '#1e293b'}; border:2px solid ${isOn ? '#6ee7b7' : '#475569'}; color:${isOn ? '#022c22' : '#cbd5e1'}; font-size:0.92rem; font-weight:900; display:flex; align-items:center; justify-content:center; padding:0 14px;">
                        ${isOn ? '🟢 ON (ACTIVE)' : '⚪ OFF'}
                    </div>
                </div>`;
            }

            // Continuous Knob / Slider -> Large Touch Slider + Fine Step (- / +) Touch Buttons + Double-Tap to Reset
            const step = pt.integer ? 1 : ((pt.max - pt.min) <= 2 ? 0.01 : ((pt.max - pt.min) <= 25 ? 0.1 : 1));
            const dec = step < 0.1 ? 2 : (step < 1 ? 1 : 0);
            const nudgeStep = (pt.max - pt.min) * 0.05;
            return `
            <div ondblclick="updateSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', ${pt.def}); refreshControlStage(${ch}, ${slotIdx});" style="background:#111827; border:2px solid #1e293b; border-radius:12px; padding:12px 14px; display:flex; flex-direction:column; justify-content:space-between; min-height:104px; box-sizing:border-box;">
                <div style="display:flex; justify-content:space-between; width:100%; align-items:center; margin-bottom:6px;">
                    <span style="font-size:0.92rem; font-weight:900; color:#f8fafc; text-transform:uppercase;">${pt.name}</span>
                    <span id="lv2-param-val-${ch}-${slotIdx}-${pt.sym}" onclick="updateSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', ${pt.def}); refreshControlStage(${ch}, ${slotIdx});" title="Tap to reset to default (${pt.def})" style="font-size:1rem; font-weight:900; color:#38bdf8; font-family:monospace; background:#090d16; padding:3px 10px; border-radius:6px; border:1px solid #1e293b; cursor:pointer;">${Number(val).toFixed(dec)}${pt.unit ? ' ' + pt.unit : ''}</span>
                </div>
                <div style="display:flex; align-items:center; gap:10px;">
                    <button onclick="nudgeSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', ${-nudgeStep}, ${pt.min}, ${pt.max}, ${dec})" style="min-width:50px; min-height:48px; background:#1e293b; border:1.5px solid #475569; color:#f8fafc; border-radius:8px; font-size:1.25rem; font-weight:900; cursor:pointer; touch-action:manipulation;">&minus;</button>
                    <input type="range" id="lv2-slider-${ch}-${slotIdx}-${pt.sym}" min="${pt.min}" max="${pt.max}" step="${step}" value="${val}" data-default="${pt.def}" style="flex:1; height:40px; accent-color:#38bdf8; cursor:pointer;" oninput="updateSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', this.value)">
                    <button onclick="nudgeSlotLv2Param(${ch}, ${slotIdx}, '${pt.sym}', ${nudgeStep}, ${pt.min}, ${pt.max}, ${dec})" style="min-width:50px; min-height:48px; background:#1e293b; border:1.5px solid #475569; color:#f8fafc; border-radius:8px; font-size:1.25rem; font-weight:900; cursor:pointer; touch-action:manipulation;">+</button>
                </div>
                <div style="display:flex; justify-content:space-between; width:100%; font-size:0.7rem; color:#64748b; margin-top:4px; font-weight:700;">
                    <span>MIN: ${pt.min}</span>
                    <span style="color:#38bdf8;">DEF: ${pt.def} (TAP VALUE TO RESET)</span>
                    <span>MAX: ${pt.max}${pt.unit ? ' ' + pt.unit : ''}</span>
                </div>
            </div>`;
        }).join('');

        return presetHtml + pageSubMenuHtml + `<div style="display:grid; grid-template-columns:repeat(auto-fill, minmax(290px, 1fr)); gap:10px; padding-bottom:12px;">${fileCardsHtml}${cards}</div>`;
    }

    window.setSlotControlPage = function(ch, slotIdx, pageKey) {
        window._slotControlSubPage = pageKey || "p1";
        window.refreshControlStage(ch, slotIdx);
    };

    window.refreshControlStage = function(ch, slotIdx) {
        const slot = window.channelDualDspState[ch]?.slots[slotIdx];
        if (!slot) return;
        const catalogEntry = window.MODEP_LV2_CATALOG.find(p => p.uri === slot.uri) || { uri: slot.uri, name: slot.name, ports: DRIVE_PORTS };
        const ctlStage = document.getElementById("slot-control-precision-grid");
        if (ctlStage) ctlStage.innerHTML = renderControlViewGrid(ch, slotIdx, slot, catalogEntry);
    };

    window.nudgeSlotLv2Param = function(ch, slotIdx, sym, delta, minV, maxV, dec) {
        const slot = window.channelDualDspState[ch]?.slots[slotIdx];
        if (!slot) return;
        const cur = Number((slot.params && slot.params[sym] !== undefined) ? slot.params[sym] : 0);
        const next = Math.max(minV, Math.min(maxV, Number((cur + delta).toFixed(dec))));
        const sl = document.getElementById(`lv2-slider-${ch}-${slotIdx}-${sym}`);
        if (sl) sl.value = next;
        window.updateSlotLv2Param(ch, slotIdx, sym, next, false);
    };

    window.switchSlotModalView = function(viewMode) {
        activeModGuiView = viewMode;
        const btnGui = document.getElementById("btn-slot-view-modgui");
        const btnCtl = document.getElementById("btn-slot-view-control");
        const btnSplit = document.getElementById("btn-slot-view-split");
        const guiWrap = document.getElementById("slot-modgui-stage-wrap");
        const ctlWrap = document.getElementById("slot-control-stage-wrap");
        const iframe = document.getElementById("slot-modgui-iframe");
        if (btnGui) btnGui.classList.toggle("active", viewMode === "modgui");
        if (btnCtl) btnCtl.classList.toggle("active", viewMode === "control");
        if (btnSplit) btnSplit.classList.toggle("active", viewMode === "split");

        if (viewMode === "modgui") {
            if (guiWrap) { guiWrap.style.display = "flex"; guiWrap.style.flex = "1"; }
            if (ctlWrap) { ctlWrap.style.display = "none"; }
        } else if (viewMode === "control") {
            if (guiWrap) { guiWrap.style.display = "none"; }
            if (ctlWrap) { ctlWrap.style.display = "block"; ctlWrap.style.flex = "1"; }
        } else {
            // Split view: both visible!
            if (guiWrap) { guiWrap.style.display = "flex"; guiWrap.style.flex = "0 0 46%"; }
            if (ctlWrap) { ctlWrap.style.display = "block"; ctlWrap.style.flex = "1"; }
        }
        if (iframe && iframe.contentWindow) {
            iframe.contentWindow.postMessage({ type: "set_view", view: "modgui" }, "*");
        }
    };

    window.openSlotModgui = function(ch, slotIdx) {
        window.openSlotModGuiModal(ch, slotIdx, "modgui");
    };

    window.openSlotModGuiModal = function(ch, slotIdx, preferredView) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const slot = window.channelDualDspState[ch].slots[slotIdx];
        if (!slot || !slot.uri || slot.uri === "thru") {
            window.openPluginPickerModal(ch, slotIdx, true);
            return;
        }
        activeModGuiChannel = ch;
        activeModGuiSlot = slotIdx;
        if (preferredView) activeModGuiView = preferredView;
        let catalogEntry = window.MODEP_LV2_CATALOG.find(p => p.uri === slot.uri);
        if (!catalogEntry) {
            catalogEntry = { uri: slot.uri, name: slot.name, ports: DRIVE_PORTS, presets: [] };
            window.MODEP_LV2_CATALOG.push(catalogEntry);
        }
        if (!catalogEntry._hydrated) {
            fetch(`${getModepHttpBase()}/effect/get?uri=${encodeURIComponent(slot.uri)}`).then(r => r.json()).then(info => {
                if (!info || !info.uri) return;
                catalogEntry._hydrated = true;
                const fullPorts = mapModepPorts(info);
                if (fullPorts && fullPorts.length) catalogEntry.ports = fullPorts;
                catalogEntry.parameters = Array.isArray(info.parameters) ? info.parameters : [];
                catalogEntry.presets = Array.isArray(info.presets) ? info.presets : [];
                catalogEntry.gui = info.gui || {};
                (catalogEntry.ports || []).forEach(pt => { if (slot.params[pt.sym] === undefined) slot.params[pt.sym] = pt.def; });
                const ctlStage = document.getElementById("slot-control-precision-grid");
                if (ctlStage && activeModGuiChannel === ch && activeModGuiSlot === slotIdx) {
                    ctlStage.innerHTML = renderControlViewGrid(ch, slotIdx, slot, catalogEntry);
                }
            }).catch(() => {});
        }
        const overlay = document.getElementById("slot-modgui-overlay");
        const title = document.getElementById("slot-modgui-title");
        const body = document.getElementById("slot-modgui-body");
        if (!overlay || !body) return;
        const chName = (window.customChannelNames && window.customChannelNames["ch-" + ch]) || (window.channelNames && window.channelNames[ch]) || `CH ${ch + 1}`;
        if (title) title.innerHTML = `${slot.icon || "🎸"} IN ${ch + 1} (${chName}) &bull; SLOT ${slotIdx + 1}: ${slot.name}`;

        const slotTabsHtml = [0, 1, 2, 3].map(sIdx => {
            const sObj = window.channelDualDspState[ch].slots[sIdx];
            const hasFx = sObj && sObj.uri && sObj.uri !== "thru";
            const isCur = (sIdx === slotIdx);
            return `<button onclick="${hasFx ? `openSlotModGuiModal(${ch}, ${sIdx}, '${activeModGuiView}')` : `closeSlotModGuiModal(); openPluginPickerModal(${ch}, ${sIdx}, true)`}" style="min-height:44px; padding:6px 12px; border-radius:8px; font-weight:900; font-size:0.8rem; cursor:pointer; background:${isCur ? '#0284c7' : '#1e293b'}; border:2px solid ${isCur ? '#38bdf8' : '#334155'}; color:#fff;">
                S${sIdx + 1}: ${hasFx ? sObj.name.substring(0, 14) : '+ ADD'}
            </button>`;
        }).join("");

        const hostUrl = `${getModepHttpBase()}/mixer/modgui-host.html?uri=${encodeURIComponent(slot.uri)}&view=modgui&bypassed=${slot.bypassed ? 1 : 0}&params=${encodeURIComponent(JSON.stringify(slot.params || {}))}`;
        body.innerHTML = `
            <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px; margin-bottom:8px; background:#090d16; padding:8px 12px; border-radius:8px; border:1px solid #1e293b; flex-shrink:0;">
                <div style="display:flex; gap:6px; align-items:center; flex-wrap:wrap;">
                    ${slotTabsHtml}
                    <span style="width:1px; height:28px; background:#334155; margin:0 4px;"></span>
                    <button class="btn-top ${activeModGuiView === 'modgui' ? 'active' : ''}" id="btn-slot-view-modgui" onclick="switchSlotModalView('modgui')" style="min-height:46px; padding:6px 14px; font-weight:900; font-size:0.86rem;">🎸 PEDAL / RACK MOD-GUI</button>
                    <button class="btn-top ${activeModGuiView === 'control' ? 'active' : ''}" id="btn-slot-view-control" onclick="switchSlotModalView('control')" style="min-height:46px; padding:6px 14px; font-weight:900; font-size:0.86rem;">🎚️ LARGE TOUCH CONTROLS</button>
                    <button class="btn-top ${activeModGuiView === 'split' ? 'active' : ''}" id="btn-slot-view-split" onclick="switchSlotModalView('split')" style="min-height:46px; padding:6px 14px; font-weight:900; font-size:0.86rem;">🔀 SPLIT VIEW</button>
                </div>
                <div style="display:flex; gap:8px; align-items:center;">
                    <button class="btn-top ${!slot.bypassed ? 'active' : ''}" id="modgui-byp-btn" style="min-height:46px; padding:6px 16px; font-weight:900; font-size:0.88rem; background:${!slot.bypassed ? '#059669' : '#334155'}; border-color:${!slot.bypassed ? '#34d399' : '#64748b'};" onclick="toggleSlotBypass(${ch}, ${slotIdx}); openSlotModGuiModal(${ch}, ${slotIdx}, '${activeModGuiView}');">
                        ${slot.bypassed ? "🔴 BYPASSED (TAP TO ENABLE)" : "🟢 EFFECT ACTIVE"}
                    </button>
                    <button class="btn-top" style="min-height:46px; padding:6px 14px; background:#1e3a8a; border-color:#3b82f6; font-weight:900;" onclick="closeSlotModGuiModal(); openPluginPickerModal(${ch}, ${slotIdx}, true);">🔄 CHANGE FX</button>
                    <button class="btn-top" style="min-height:46px; padding:6px 14px; background:#7f1d1d; border-color:#ef4444; font-weight:900;" onclick="activePickerChannel=${ch}; activePickerSlot=${slotIdx}; assignSlotPlugin(0); closeSlotModGuiModal();">🗑️ REMOVE</button>
                </div>
            </div>
            <div id="slot-modgui-stage-wrap" style="background:#070a10; border:1.5px solid #1e293b; border-radius:10px; overflow:hidden; display:${activeModGuiView === 'control' ? 'none' : 'flex'}; flex:${activeModGuiView === 'split' ? '0 0 46%' : '1'}; min-height:0;">
                <iframe id="slot-modgui-iframe" src="${hostUrl}" style="width:100%; height:100%; border:none; background:#070a10;"></iframe>
            </div>
            <div id="slot-control-stage-wrap" style="display:${activeModGuiView === 'modgui' ? 'none' : 'block'}; flex:1; overflow-y:auto; margin-top:${activeModGuiView === 'split' ? '8px' : '0'}; padding-right:4px; min-height:0;">
                <div id="slot-control-precision-grid">${renderControlViewGrid(ch, slotIdx, slot, catalogEntry)}</div>
            </div>
        `;
        overlay.style.display = "flex";
        overlay.classList.add("open");
    };

    window.loadSlotLv2Preset = function(ch, slotIdx, presetUri) {
        if (!presetUri || !window.channelDualDspState[ch]) return;
        const slot = window.channelDualDspState[ch].slots[slotIdx];
        if (!slot) return;
        slot.preset = presetUri;
        saveDualDspState();
        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({ action: "load_lv2_slot_preset", channel: ch, ch: ch, slot: slotIdx, uri: slot.uri, preset_uri: presetUri }));
        }
    };

    window.updateSlotLv2Param = function(ch, slotIdx, sym, rawVal, fromIframe) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const slot = window.channelDualDspState[ch].slots[slotIdx];
        if (!slot || !sym) return;
        if (sym === ":bypass" || sym === "bypass") {
            slot.bypassed = parseFloat(rawVal) >= 0.5;
            saveDualDspState();
            window.syncLinearSlotChain(ch);
            const bypBtn = document.getElementById("modgui-byp-btn");
            if (bypBtn) {
                bypBtn.classList.toggle("active", !slot.bypassed);
                bypBtn.style.background = !slot.bypassed ? "#059669" : "#334155";
                bypBtn.innerText = slot.bypassed ? "🔴 BYPASSED (TAP TO ENABLE)" : "🟢 EFFECT ACTIVE";
            }
            return;
        }
        let numVal = parseFloat(rawVal);
        if (isNaN(numVal)) numVal = 0;
        if (!slot.params) slot.params = {};
        slot.params[sym] = numVal;
        saveDualDspState();

        const el = document.getElementById(`lv2-param-val-${ch}-${slotIdx}-${sym}`);
        if (el) el.innerText = numVal.toFixed(Math.abs(numVal) < 10 && numVal % 1 !== 0 ? 2 : 1);
        const sl = document.getElementById(`lv2-slider-${ch}-${slotIdx}-${sym}`);
        if (sl && fromIframe) sl.value = numVal;

        if (!fromIframe) {
            const iframe = document.getElementById("slot-modgui-iframe");
            if (iframe && iframe.contentWindow) {
                iframe.contentWindow.postMessage({ type: "set_param", symbol: sym, value: numVal }, "*");
                iframe.contentWindow.postMessage({ type: "set_port_value", symbol: sym, value: numVal }, "*");
            }
        } else {
            // If toggle switch was flipped inside MOD-GUI iframe, refresh Control View card state too
            window.refreshControlStage(ch, slotIdx);
        }

        if (window.meterWs && window.meterWs.readyState === WebSocket.OPEN) {
            window.meterWs.send(JSON.stringify({
                action: "set_lv2_slot_param",
                channel: ch,
                ch: ch,
                slot: slotIdx,
                uri: slot.uri,
                symbol: sym,
                value: numVal
            }));
        }
    };

    window.addEventListener("message", function(ev) {
        const d = ev.data;
        if (!d || typeof d !== "object") return;
        if (d.type === "modgui_param_change" && d.symbol) {
            window.updateSlotLv2Param(activeModGuiChannel, activeModGuiSlot, d.symbol, d.value, true);
        } else if (d.type === "modgui_preset_load") {
            window.loadSlotLv2Preset(activeModGuiChannel, activeModGuiSlot, d.preset || d.value);
        } else if (d.type === "modgui_file_selected") {
            const path = d.path || d.file || "";
            const label = d.label || (typeof path === "string" ? path.split("/").pop() : "");
            const isIr = (typeof path === "string") && (path.endsWith(".wav") || path.includes("IR"));
            window.selectSlotFile(activeModGuiChannel, activeModGuiSlot, isIr ? "ir" : "nam", path, label);
        }
    });

    window.closeSlotModGuiModal = function() {
        const overlay = document.getElementById("slot-modgui-overlay");
        const iframe = document.getElementById("slot-modgui-iframe");
        if (iframe) iframe.src = "about:blank";
        if (overlay) { overlay.style.display = "none"; overlay.classList.remove("open"); }
        // Refresh Fullscreen 4-Slot Rack Page if open underneath
        const fsOverlay = document.getElementById("fullscreen-slots-overlay");
        if (fsOverlay && fsOverlay.classList.contains("open")) {
            window.renderFullScreenSlotsPage(activeModGuiChannel);
        }
    };

    let canvasFitMode = true;
    window.toggleCanvasIframeFit = function() {
        canvasFitMode = !canvasFitMode;
        applyCanvasIframeFit();
    };

    function applyCanvasIframeFit() {
        const iframe = document.getElementById("canvas-modal-iframe");
        const btn = document.getElementById("btn-canvas-fit");
        if (!iframe) return;
        if (canvasFitMode) {
            const availW = window.innerWidth;
            const targetW = 1580; // MOD canvas layout width
            const scale = Math.min(1.0, Math.max(0.48, (availW - 32) / targetW));
            iframe.style.width = (100 / scale) + "%";
            iframe.style.height = (100 / scale) + "%";
            iframe.style.transform = `scale(${scale.toFixed(3)})`;
            iframe.style.transformOrigin = "top left";
            if (btn) btn.innerText = "🔍 FIT: ON (" + Math.round(scale * 100) + "%)";
        } else {
            iframe.style.width = "100%";
            iframe.style.height = "100%";
            iframe.style.transform = "none";
            if (btn) btn.innerText = "🔍 100% ZOOM";
        }
    }

    window.openModepCanvasModal = function(ch) {
        if (!window.channelDualDspState[ch]) loadDualDspState();
        const port = (window.channelDualDspState[ch] && window.channelDualDspState[ch].rackPort) || 8881;
        const host = (typeof window.getActivePiHost === "function") ? window.getActivePiHost() : (window.location.hostname || "192.168.1.148");
        const url = `http://${host}:${port}/`;
        const overlay = document.getElementById("canvas-modal-overlay"), title = document.getElementById("canvas-modal-title");
        const iframe = document.getElementById("canvas-modal-iframe"), newTabLink = document.getElementById("canvas-modal-newtab");
        if (title) title.innerHTML = `🎛️ Channel ${typeof ch === 'number' ? ch + 1 : ch} &bull; MODEP Pedalboard Canvas (Port :${port})`;
        if (newTabLink) newTabLink.href = url;
        if (iframe) iframe.src = url;
        if (overlay) { overlay.style.display = "flex"; overlay.classList.add("open"); }
        setTimeout(applyCanvasIframeFit, 100);
    };

    window.closeCanvasModal = function() {
        const overlay = document.getElementById("canvas-modal-overlay"), iframe = document.getElementById("canvas-modal-iframe");
        if (iframe) iframe.src = "about:blank";
        if (overlay) { overlay.style.display = "none"; overlay.classList.remove("open"); }
    };

    window.initDualDspModes = function() {
        loadDualDspState();
        window.fetchLiveModepLv2Plugins();
        window.fetchLiveUserFiles();
        for (let ch = 0; ch < 8; ch++) {
            const state = window.channelDualDspState[ch];
            window.setChannelDspMode(ch, state.mode);
            for (let s = 0; s < 4; s++) window.renderChannelSlotDom(ch, s);
            const sel = document.getElementById(`canvas-rack-select-${ch}`);
            if (sel) sel.value = String(state.rackPort);
            const byp = document.getElementById(`btn-canvas-bypass-${ch}`);
            if (byp) {
                byp.classList.toggle("bypassed", state.canvasBypass);
                byp.innerText = state.canvasBypass ? "CANVAS BYPASS: ACTIVE" : "CANVAS BYPASS: OFF";
            }
        }
        // Ensure all saved slot chains are pushed to C audio engine once WebSocket is connected
        setTimeout(() => { window.syncAllChannelSlotChains(); }, 600);
    };
})(window);
