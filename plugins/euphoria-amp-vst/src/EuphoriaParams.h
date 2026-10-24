#pragma once
// ============================================================================
// Euphoria Amp - shared parameter table for the JUCE build (VST3 / AU / LV2 /
// Standalone on macOS, Windows, Linux).
//
// This mirrors the g_knobs[] table of the Windows standalone
// (StandaloneWinMain.cpp) 1:1 - same controls, same order, same ranges and
// defaults - so both front-ends drive the shared EuphoriaAmpEngine identically.
// Keep the enum and the table in the same order (see the static_assert below).
// ============================================================================
#include <vector>
#include <cstddef>

enum EuphoriaParamIndex {
    K_CH_MODE = 0, K_QUALITY, K_CAB_SIM, K_CAB_SIZE, K_TUNING_REF, K_PICKUP_VOICE,
    // Page 0: Supreme Glass Clean
    K_CLEAN_VOL, K_CLEAN_BRIGHT, K_CLEAN_DEEP, K_CLEAN_BASS, K_CLEAN_MID, K_CLEAN_MIDSHIFT,
    K_CLEAN_TREBLE, K_CLEAN_GLASS, K_CLEAN_PRESENCE, K_CLEAN_MASTER, K_CLEAN_OUT,
    // Page 1: '59 Bassman / '68 JMP Crunch
    K_BM_DRIVE, K_BM_VOL, K_BM_BASS, K_BM_MID, K_BM_TREBLE, K_BM_PRESENCE, K_BM_SLOPE, K_BM_TUBES, K_BM_SAG, K_BM_SPKDRV,
    // Page 2: BE-100 / JCM 800 Lead
    K_LEAD_GAIN, K_LEAD_VOL, K_LEAD_BASS, K_LEAD_MID, K_LEAD_TREBLE, K_LEAD_PRESENCE, K_LEAD_HBE, K_LEAD_C45, K_LEAD_SAT, K_LEAD_DEHISS,
    // Page 3: five independent series pedals, each with its own (OLD)/HYPER circuit switch
    K_KLON_ON, K_KLON_GAIN, K_KLON_TREBLE, K_KLON_OUTPUT, K_KLON_BLEND, K_KLON_HYPER,
    K_TS_ON, K_TS_DRIVE, K_TS_TONE, K_TS_LEVEL, K_TS_MOD, K_TS_HYPER,
    K_BD_ON, K_BD_GAIN, K_BD_TONE, K_BD_LEVEL, K_BD_PHAT, K_BD_HYPER,
    K_RAT_ON, K_RAT_DIST, K_RAT_FILTER, K_RAT_VOL, K_RAT_TURBO, K_RAT_HYPER,
    K_FUZZ_ON, K_FUZZ_GAIN, K_FUZZ_SAG, K_FUZZ_LEVEL, K_FUZZ_TRANSISTOR, K_FUZZ_HYPER,
    // Page 4: Fluid Space Chamber
    K_SPACE_SHAPE, K_SPACE_SIZE, K_SPACE_MOD_ON, K_SPACE_MOD_RATE,
    K_SPACE_SHAPE_DEP, K_SPACE_SIZE_DEP, K_SPACE_WAVEFORM, K_SPACE_SPREAD,
    // Page 5: Fluid Reverb + Fender Harmonic Tremolo
    K_REV_ON, K_REV_SHAPE, K_REV_SIZE, K_REV_LOWCUT, K_REV_HIGHCUT, K_REV_DWELL, K_REV_MIX,
    K_VIB_ON, K_VIB_MODE, K_VIB_SPEED, K_VIB_DEPTH,
    // Page 6: Studio Tape Delay
    K_DLY_ON, K_DLY_PINGPONG, K_DLY_TIME, K_DLY_FB, K_DLY_MIX, K_DLY_DRIVE, K_DLY_FLUTTER, K_DLY_DUCK,
    K_NUM_PARAMS
};

struct EuphoriaParamDef {
    const char* id;
    const char* label;
    int   page;               // -1 = global header, 0..6 = page
    float minVal, maxVal, defVal;
    std::vector<const char*> choices; // non-empty => stepped / choice parameter
};

static const char* const kShapeNames[11] = {
    "CIRCLE TUBE", "24-GON", "16-GON", "12-DODECAGON", "10-DECAGON", "9-NONAGON",
    "8-OCTAGON", "7-HEPTAGON", "6-HEXAGON", "5-PENTAGON", "4-SQUARE CUBE" };
static const char* const kSizeNames[19] = {
    "/10 (0.1x)", "/9 (0.11x)", "/8 (0.12x)", "/7 (0.14x)", "/6 (0.17x)", "/5 (0.2x)", "/4 (0.25x)", "/3 (0.33x)", "/2 (0.5x)",
    "1x (432 Hz)", "x2 (2.0x)", "x3 (3.0x)", "x4 (4.0x)", "x5 (5.0x)", "x6 (6.0x)", "x7 (7.0x)", "x8 (8.0x)", "x9 (9.0x)", "x10 (10x)" };

inline const std::vector<EuphoriaParamDef>& euphoriaParamTable() {
    static const std::vector<EuphoriaParamDef> t = {
        // ---- Global header ----
        {"ch_mode",    "ROUTING",          -1, 0, 3, 0, {"CH1 SUPREME GLASS", "CH2 '59/'68 CRUNCH", "CH3 BE-100 LEAD", "JUMPERED BLEND"}},
        {"quality",    "FLUID OVERSAMPLE", -1, 0, 3, 2, {"ECO LIVE 1x", "STAGE 2x", "STUDIO 4x", "ULTRA 8x"}},
        {"cab_sim",    "PHYSICAL SPEAKER", -1, 0, 12, 0, {"2x12 JENSEN (OLD)", "4x10 BASSMAN (OLD)", "2x10 OXFORD (OLD)", "4x12 GREENBACK (OLD)", "432Hz HARMONIC (OLD)", "HYPER GREENBACK", "EUPHORIC ALNICO", "432Hz PROJECTOR", "HYPER VINTAGE 30", "HYPER EVM-12L", "HYPER BLUE BULLDOG", "HYPER TWEED 4x10", "BYPASS CAB"}},
        {"cab_size",   "CAB SIZE / VOL",   -1, 0, 4, 2, {"0.6x COMPACT", "0.8x VINTAGE", "1.0x STANDARD", "1.3x OVERSIZED", "1.8x STADIUM"}},
        {"tuning_ref", "TUNING REF",       -1, 0, 1, 0, {"440 Hz STD", "432 Hz NAT"}},
        {"pickup",     "PICKUP LOAD",      -1, 0, 2, 0, {"1M STUDIO", "STRAT / SC", "GRETSCH / HB"}},

        // ---- Page 0: Supreme Glass Clean ----
        {"clean_vol",      "VOLUME",        0, 0, 10, 5.2f, {}},
        {"clean_bright",   "BRIGHT VOICING",0, 0, 2, 1, {"WARM / OFF", "BRIGHT GLASS", "ULTRA CHIME"}},
        {"clean_deep",     "DEEP CONTOUR",  0, 0, 1, 0, {"TIGHT (GRETSCH)", "DEEP 3D (STRAT)"}},
        {"clean_bass",     "BASS",          0, 0, 10, 5.0f, {}},
        {"clean_mid",      "MIDDLE",        0, 0, 10, 5.0f, {}},
        {"clean_midshift", "MID SHIFT",     0, 0, 2, 0, {"450Hz (STRAT)", "750Hz (BODY)", "1.15kHz (GRETSCH)"}},
        {"clean_treble",   "TREBLE",        0, 0, 10, 5.8f, {}},
        {"clean_glass",    "GLASS AIR",     0, 0, 10, 6.5f, {}},
        {"clean_presence", "PRESENCE",      0, 0, 10, 5.5f, {}},
        {"clean_master",   "MASTER",        0, 0, 10, 5.5f, {}},
        {"clean_out",      "MASTER VOL",    0, 0, 10, 5.0f, {}},

        // ---- Page 1: '59 Bassman / '68 JMP Crunch ----
        {"bm_drive",    "PREAMP DRIVE",  1, 0, 10, 5.8f, {}},
        {"bm_vol",      "CHANNEL VOL",   1, 0, 10, 5.5f, {}},
        {"bm_bass",     "BASS",          1, 0, 10, 5.5f, {}},
        {"bm_mid",      "MIDDLE",        1, 0, 10, 6.0f, {}},
        {"bm_treble",   "TREBLE",        1, 0, 10, 5.5f, {}},
        {"bm_presence", "PRESENCE",      1, 0, 10, 5.2f, {}},
        {"bm_slope",    "SLOPE VOICING", 1, 0, 1, 0, {"56k '59 TWEED", "33k '68 JMP"}},
        {"bm_tubes",    "POWER TUBES",   1, 0, 1, 0, {"2x 6L6GC USA", "2x EL34 UK"}},
        {"bm_sag",      "FLUID SAG",     1, 0, 10, 4.5f, {}},
        {"bm_spkdrv",   "CONE DRIVE",    1, 0, 10, 4.0f, {}},

        // ---- Page 2: BE-100 / JCM 800 Lead ----
        {"lead_gain",     "LEAD GAIN",       2, 0, 10, 6.5f, {}},
        {"lead_vol",      "LEAD VOL",        2, 0, 10, 5.5f, {}},
        {"lead_bass",     "BASS",            2, 0, 10, 5.0f, {}},
        {"lead_mid",      "MIDDLE",          2, 0, 10, 5.5f, {}},
        {"lead_treble",   "TREBLE",          2, 0, 10, 6.0f, {}},
        {"lead_presence", "PRESENCE",        2, 0, 10, 5.8f, {}},
        {"lead_hbe",      "HBE CASCADE",     2, 0, 1, 0, {"OFF (BE-100)", "ON (HBE LEAD)"}},
        {"lead_c45",      "C45 VOICING",     2, 0, 1, 1, {"OFF (STOCK)", "C45 MID PUNCH"}},
        {"lead_sat",      "SAT CLIPPING",    2, 0, 1, 0, {"OFF (OPEN)", "SAT DIODE"}},
        {"lead_dehiss",   "DYNAMIC DE-HISS", 2, 0, 10, 5.0f, {}},

        // ---- Page 3: Pedals (each pedal: footswitch, 3-4 knobs, voicing switch, (OLD)/HYPER circuit switch) ----
        {"klon_on",     "CENTAUR ON",   3, 0, 1, 0, {"BYPASSED", "ACTIVE ON"}},
        {"klon_gain",   "CENTAUR GAIN", 3, 0, 10, 4.0f, {}},
        {"klon_treble", "CENTAUR TREBLE", 3, 0, 10, 5.5f, {}},
        {"klon_output", "CENTAUR OUTPUT", 3, 0, 10, 6.5f, {}},
        {"klon_blend",  "CENTAUR CLEAN BLEND", 3, 0, 100, 35.0f, {}},
        {"klon_hyper",  "CENTAUR CIRCUIT", 3, 0, 1, 0, {"(OLD) 18V", "HYPER 27V"}},

        {"ts_on",     "TS808 ON",    3, 0, 1, 0, {"BYPASSED", "ACTIVE ON"}},
        {"ts_drive",  "TS808 OVERDRIVE", 3, 0, 10, 4.5f, {}},
        {"ts_tone",   "TS808 TONE",  3, 0, 10, 4.8f, {}},
        {"ts_level",  "TS808 LEVEL", 3, 0, 10, 7.0f, {}},
        {"ts_mod",    "TS808 VOICING", 3, 0, 1, 0, {"STOCK 720Hz", "FAT MOD 340Hz"}},
        {"ts_hyper",  "TS808 CIRCUIT", 3, 0, 1, 0, {"(OLD) TS808", "HYPER VELVET"}},

        {"bd_on",     "BLUES DRV ON", 3, 0, 1, 0, {"BYPASSED", "ACTIVE ON"}},
        {"bd_gain",   "BD-2 GAIN",    3, 0, 10, 5.0f, {}},
        {"bd_tone",   "BD-2 TONE",    3, 0, 10, 5.2f, {}},
        {"bd_level",  "BD-2 LEVEL",   3, 0, 10, 6.8f, {}},
        {"bd_phat",   "BD-2 VOICING", 3, 0, 1, 0, {"STOCK BD-2", "PHAT MOD"}},
        {"bd_hyper",  "BD-2 CIRCUIT", 3, 0, 1, 0, {"(OLD) BD-2", "HYPER JFET"}},

        {"rat_on",     "RODENT ON",   3, 0, 1, 0, {"BYPASSED", "ACTIVE ON"}},
        {"rat_dist",   "RODENT DISTORT", 3, 0, 10, 5.0f, {}},
        {"rat_filter", "RODENT FILTER",  3, 0, 10, 5.0f, {}},
        {"rat_vol",    "RODENT VOLUME",  3, 0, 10, 6.5f, {}},
        {"rat_turbo",  "RODENT CLIPPING", 3, 0, 1, 0, {"SILICON", "TURBO LED"}},
        {"rat_hyper",  "RODENT CIRCUIT",  3, 0, 1, 0, {"(OLD) RODENT", "HYPER LM308"}},

        {"fuzz_on",         "FUZZ FACE ON", 3, 0, 1, 0, {"BYPASSED", "ACTIVE ON"}},
        {"fuzz_gain",       "FUZZ",         3, 0, 10, 6.5f, {}},
        {"fuzz_sag",        "FUZZ AMP SAG", 3, 0, 10, 5.0f, {}},
        {"fuzz_level",      "FUZZ LEVEL",   3, 0, 10, 7.0f, {}},
        {"fuzz_transistor", "FUZZ TRANSISTOR", 3, 0, 1, 0, {"NKT275 GE", "BC108 SI"}},
        {"fuzz_hyper",      "FUZZ CIRCUIT",    3, 0, 1, 0, {"(OLD) FUZZ", "HYPER 432Hz"}},

        // ---- Page 4: Fluid Space Chamber ----
        {"space_shape",   "CHAMBER SHAPE",    4, 0, 10, 0, std::vector<const char*>(kShapeNames, kShapeNames + 11)},
        {"space_size",    "432Hz SIZE / VOL", 4, 0, 18, 9, std::vector<const char*>(kSizeNames, kSizeNames + 19)},
        {"space_mod_on",  "SPACE MOD",        4, 0, 1, 0, {"BYPASSED", "ACTIVE ON"}},
        {"space_mod_rate","MOD RATE Hz",      4, 0.05f, 8.0f, 0.75f, {}},
        {"space_shape_dep","SHAPE MOD %",     4, 0, 100, 40.0f, {}},
        {"space_size_dep","SIZE MOD %",       4, 0, 100, 35.0f, {}},
        {"space_waveform","MOD WAVEFORM",     4, 0, 3, 0, {"SINE WAVE", "TRIANGLE", "432Hz GOLDEN", "SMOOTH DRIFT"}},
        {"space_spread",  "STEREO SPREAD",    4, 0, 180, 90.0f, {}},

        // ---- Page 5: Reverb + Tremolo ----
        {"rev_on",      "REVERB SWITCH", 5, 0, 1, 1, {"BYPASSED", "ACTIVE ON"}},
        {"rev_shape",   "REV SHAPE",     5, 0, 10, 0, std::vector<const char*>(kShapeNames, kShapeNames + 11)},
        {"rev_size",    "REV 432Hz SIZE",5, 0, 18, 9, std::vector<const char*>(kSizeNames, kSizeNames + 19)},
        {"rev_lowcut",  "SEND LOW-CUT",  5, 40.0f, 600.0f, 120.0f, {}},
        {"rev_highcut", "SEND HIGH-CUT", 5, 2500.0f, 14000.0f, 6500.0f, {}},
        {"rev_dwell",   "FLUID DWELL",   5, 0, 10, 5.0f, {}},
        {"rev_mix",     "REVERB MIX %",  5, 0, 100, 28.0f, {}},
        {"vib_on",      "VIB SWITCH",    5, 0, 1, 1, {"BYPASSED", "ACTIVE ON"}},
        {"vib_mode",    "VIB CIRCUIT",   5, 0, 1, 0, {"HARMONIC TUBE", "OPTO PHOTOCELL"}},
        {"vib_speed",   "SPEED (Hz)",    5, 0.5f, 12.0f, 4.2f, {}},
        {"vib_depth",   "INTENSITY",     5, 0, 10, 5.5f, {}},

        // ---- Page 6: Tape Delay ----
        {"dly_on",      "TAPE BYPASS",   6, 0, 1, 1, {"BYPASSED", "ACTIVE ON"}},
        {"dly_pingpong","ROUTING MODE",  6, 0, 1, 1, {"15/30 MONO", "PING-PONG"}},
        {"dly_time",    "DELAY TIME ms", 6, 20, 1500, 360.0f, {}},
        {"dly_fb",      "FEEDBACK %",    6, 0, 95, 34.0f, {}},
        {"dly_mix",     "WET MIX %",     6, 0, 100, 22.0f, {}},
        {"dly_drive",   "OXIDE DRIVE %", 6, 0, 100, 28.0f, {}},
        {"dly_flutter", "WOW & FLUTTER", 6, 0, 100, 18.0f, {}},
        {"dly_duck",    "AUTO DUCKING",  6, 0, 100, 25.0f, {}},
    };
    return t;
}

// Checked with jassert in the processor constructor so enum/table drift is caught in debug builds.
inline bool euphoriaParamTableIsConsistent() {
    return euphoriaParamTable().size() == (size_t)K_NUM_PARAMS;
}
