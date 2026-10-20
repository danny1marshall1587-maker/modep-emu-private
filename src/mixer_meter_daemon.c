#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <jack/jack.h>
#include <libwebsockets.h>

/* Minimal LV2 C ABI & URID / Atom Extension Definitions for Native .so Slot Hosting */
typedef void* LV2_Handle;
typedef struct _LV2_Feature {
    const char *URI;
    void       *data;
} LV2_Feature;

typedef uint32_t LV2_URID;
typedef void*    LV2_URID_Map_Handle;
typedef struct {
    LV2_URID_Map_Handle handle;
    LV2_URID (*map)(LV2_URID_Map_Handle handle, const char *uri);
} LV2_URID_Map;

typedef struct {
    uint32_t size;
    uint32_t type;
    uint32_t unit;
    uint32_t pad;
    uint8_t  body[4080];
} Lv2EmptyAtomSeq;

typedef struct _LV2_Descriptor {
    const char *URI;
    LV2_Handle (*instantiate)(const struct _LV2_Descriptor *descriptor,
                              double sample_rate,
                              const char *bundle_path,
                              const LV2_Feature *const *features);
    void (*connect_port)(LV2_Handle instance, uint32_t port, void *data_location);
    void (*activate)(LV2_Handle instance);
    void (*run)(LV2_Handle instance, uint32_t sample_count);
    void (*deactivate)(LV2_Handle instance);
    void (*cleanup)(LV2_Handle instance);
    const void* (*extension_data)(const char *uri);
} LV2_Descriptor;

typedef const LV2_Descriptor* (*LV2_Descriptor_Function)(uint32_t index);

#define MAX_LV2_PORTS 64
#define MAX_SAVED_SLOT_PARAMS 48
#define LV2_BLOCK_MAX 2048

#define NUM_CHANNELS     8
#define NUM_MONITORS     6
#define NUM_FX           4
#define NUM_EQ_BANDS     8
#define NUM_RTA_BINS     64
#define MAX_TAPE_TRACKS  32
#define TAPE_LOOP_FRAMES (48000 * 30) /* Fast RAM playback cache */
#define DISK_RING_FRAMES 65536        /* Lock-free SPSC ringbuffer for continuous direct-to-disk WAV streaming */
#define METER_PORT       8089

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void isolate_mod_monitor_passthrough(jack_client_t *client);

typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} BiquadFilter;

/* Stage 4 & Stage 6: Cytomic Zero-Delay Feedback SVF Band + Tukan Green Dynamic EQ S2 & FreeEQ8 SVF Sidechain Tap */
typedef struct {
    int   enabled;
    int   type;   /* 0=Peak, 1=LowCut, 2=LowCutBW, 3=LowShelf, 4=HighShelf, 5=HighCut, 6=HighCutBW, 7=Notch, 8=BandPass, 9=Tilt */
    int   slope;  /* 1..4 (12/24/36/48 dB/oct) */
    float freq;
    float gain;
    float q;
    /* Stage 6: Tukan Studios Green Dynamic EQ S2 / FreeEQ8 Dynamic SVF Extension */
    int   dyn_enabled;   /* 0 = Static EQ, 1 = Tukan Dynamic EQ S2 Active */
    float dyn_thresh_db; /* -48.0 .. 0.0 dBFS (default -20.0) */
    float dyn_range_db;  /* -18.0 .. +12.0 dB max dynamic excursion (default -6.0) */
    float dyn_ratio;     /* 1.2 .. 10.0 : 1 (default 3.0) */
    float dyn_env;       /* Internal SVF bandpass (k*v1) envelope follower */
    float dyn_gain_db;   /* Instantaneous dynamic EQ gain modulation (dB) */
    float k_base;        /* 1/Q for sidechain bandpass tap */
    int   ctrl_cnt;      /* 16-sample control-rate SVF mix update counter */
    /* Rule 27: the WS thread only writes targets (freq/gain/q) and sets param_dirty.
     * The JACK thread owns sm_* and is the ONLY writer of a1..m2, ramping toward targets
     * at 3 kHz control rate so EQ drags never zipper, click or read a torn coefficient set. */
    float sm_freq, sm_gain, sm_q;
    volatile int param_dirty;
    float a1, a2, a3;
    float m0, m1, m2;
    float ic1eq[4];
    float ic2eq[4];
    float ic1eq_r[4];
    float ic2eq_r[4];
} CytomicSvfBand;

typedef struct {
    int            adaptive_room_mode; /* 1 = Tukan Adaptive Room-Curve Loudness Flatness Lock */
    CytomicSvfBand bands[NUM_EQ_BANDS];
} EqStrip8;

/* Stage 6: 64-bit (double) Precision Surgical -Infinity dB Auto-Feedback Destroyer (AFD) Notch Bank */
#define NUM_AFD_NOTCHES 12
typedef struct {
    int    active;    /* 0 = idle, 1 = locked (fixed soundcheck), 2 = clamped (live floating/tracking) */
    int    inf_null;  /* 1 = True -Infinity dB spectral zero null, 0 = dB cut */
    double freq;      /* Center frequency (Hz) */
    double depth_db;  /* -3.0 .. -96.0 dB (or -Infinity when inf_null=1) */
    double q;         /* Surgical Q: 14.0 (Auto-Q wide moving bass) .. 120.0 (1/80th oct razor) */
    double a1, a2, a3;
    double m0, m1, m2;
    double ic1_l, ic2_l;
    double ic1_r, ic2_r;
} AfdNotchFilter64;

typedef struct {
    int              auto_hunting; /* 1 = Live Hunting (CH 1-8 default), 0 = Soundcheck Lock (Buses default) */
    AfdNotchFilter64 notches[NUM_AFD_NOTCHES];
} AfdNotchBank;

typedef struct {
    int   comp_model;     /* 0 = fetish76, 1 = lala, 2 = dual (76->LA), 3 = buster limiter */
    int   comp_bypass;    /* 1 = Master Compressor Bypass engaged, 0 = Active */
    float bypass_mix;     /* Smoothed 0.0 (bypass) .. 1.0 (active) click-free equal-power crossfader */
    /* FETISH 76 state */
    int   fet_power;
    float fet_input_db;   /* 0..60 dB (default 18 = 0 dB unity) */
    float fet_output_db;  /* 0..48 dB (default 20 = 0 dB unity) */
    int   fet_attack;     /* 1..7 (7 = fastest) */
    int   fet_release;    /* 1..7 (7 = fastest) */
    float fet_ratio;      /* 4, 8, 12, 20, or 50 (ALL IN) */
    float fet_det_env;    /* Fast peak-rectifier detector envelope */
    float fet_env_db;
    /* LALA Optical state */
    int   lala_power;
    float lala_peak_red;  /* 0..100 (default 18) */
    float lala_gain;      /* 0..100 (default 32 = 0 dB unity) */
    float lala_mix;       /* 0..100 % (default 100) */
    int   lala_limit;     /* 0 = Compress (3:1), 1 = Limit (10:1) */
    float lala_det_env;   /* Optical cell peak-rectifier detector envelope */
    float lala_env_db;
    /* BUSTER Limiter state */
    int   lim_power;
    float lim_thresh_db;  /* -18..0 dBFS (default -1.5) */
    float lim_ceil_lin;   /* -6..0 dBFS linear ceiling (default 0.89125f = -1.0 dBFS) */
    float lim_drive_lin;  /* 0..12 dB linear drive */
    float lim_rel_coeff;
    float lim_env_db;
    /* Precalculated linear coefficients (Rule 20 & Rule 27) */
    float fet_drive_lin;
    float fet_makeup_lin;
    float lala_knee_lin;
    float lala_makeup_lin;
} DynamicsSlot;

#define NUM_CH_SLOTS 4
#define SLOT_DLY_MAX 24000

typedef struct {
    float b0, b1, b2, a1, a2;
} BiquadCoeffs;

typedef struct {
    int   active;         /* 0 = empty, 1 = loaded */
    int   bypass;         /* 0 = active, 1 = bypassed */
    int   fx_class;       /* 1=Drive, 2=Amp/NAM, 3=Delay, 4=Reverb, 5=Mod, 6=Synth, 7=Comp, 8=EQ/Filter */
    char  uri[160];
    char  name[80];
    char  file_path[256];
    char  file_label[128];
    int   model_preset_id;
    /* Native Compiled LV2 .so Plugin Instance State (100% Authentic MODEP Parity) */
    volatile int lv2_ready;
    void *dl_handle;
    const LV2_Descriptor *lv2_desc;
    LV2_Handle lv2_handle_l;
    LV2_Handle lv2_handle_r;
    uint32_t lv2_num_ports;
    int   num_audio_in;
    int   num_audio_out;
    int   audio_in_idx[4];
    int   audio_out_idx[4];
    int   bypass_port_idx;
    char  lv2_port_syms[MAX_LV2_PORTS][32];
    float lv2_port_defs[MAX_LV2_PORTS];
    float lv2_port_vals[MAX_LV2_PORTS];
    float lv2_port_out_vals[MAX_LV2_PORTS];
    uint8_t lv2_port_is_ctrl[MAX_LV2_PORTS];
    uint8_t lv2_port_is_atom[MAX_LV2_PORTS];
    uint8_t lv2_port_is_input[MAX_LV2_PORTS];
    float buf_in_l[LV2_BLOCK_MAX];
    float buf_in_r[LV2_BLOCK_MAX];
    float buf_out_l[LV2_BLOCK_MAX];
    float buf_out_r[LV2_BLOCK_MAX];
    Lv2EmptyAtomSeq atom_in_buf;
    Lv2EmptyAtomSeq atom_out_buf;
    /* Exact Saved Port Symbol->Value Table for Persistence & Reloads */
    int   num_saved_params;
    char  saved_param_syms[MAX_SAVED_SLOT_PARAMS][32];
    float saved_param_vals[MAX_SAVED_SLOT_PARAMS];
    /* Normalized 0..1 DSP Parameters mapped from any LV2/MOD-GUI symbol (Fallback Engine) */
    float p_drive;        /* Drive / Gain / Sustain / Distortion (default 0.55) */
    float p_tone;         /* Tone / Cutoff / Filter / Damping (default 0.55) */
    float p_bass;         /* Bass / Low / Thump (default 0.50) */
    float p_mid;          /* Mid / Body / Presence (default 0.50) */
    float p_treble;       /* Treble / High / Bright (default 0.55) */
    float p_level;        /* Output Level / Master / Volume (default 0.70 -> unity/boost) */
    float p_mix;          /* Dry/Wet Mix / Blend (default 0.50 for time/mod FX, 1.0 for drive/amp) */
    float p_time;         /* Delay Time / Reverb Size / Decay (default 0.45) */
    float p_feedback;     /* Feedback / Regen / Resonance (default 0.40) */
    float p_rate;         /* LFO Rate / Speed (default 0.35) */
    float p_depth;        /* LFO Depth / Intensity / Octave / Synth (default 0.50) */
    int   sw_bright;      /* Toggle Switch 1: Bright / Fat / Boost / PAB */
    int   sw_mode;        /* Toggle Switch 2: Voice / Clipping / Mode / Deep */
    /* Per-slot filter & state variables */
    float lp1_l, lp1_r;
    float lp2_l, lp2_r;
    float hp1_l, hp1_r;
    float env_l, lfo_phase;
    float sub_sign_l, sub_prev_l;
    /* Dynamic Tube / Sag / Speaker State (Rule 29: exact C++ cyber amp parity) */
    float grid_bias1_l, grid_bias1_r;
    float grid_bias2_l, grid_bias2_r;
    float grid_bias_cf_l, grid_bias_cf_r;
    float sag_voltage_l, sag_voltage_r;
    float speaker_env_l, speaker_env_r;
    float speaker_thermal_l, speaker_thermal_r;
    float speaker_cone_l, speaker_cone_r;
    /* Multi-pole cabinet filter coefficients & states (Rule 29: authentic speaker transfer function) */
    BiquadCoeffs cab_ls;
    BiquadCoeffs cab_mid;
    BiquadCoeffs cab_lp1;
    BiquadCoeffs cab_lp2;
    float cab_ls_s1_l,  cab_ls_s2_l;
    float cab_ls_s1_r,  cab_ls_s2_r;
    float cab_mid_s1_l, cab_mid_s2_l;
    float cab_mid_s1_r, cab_mid_s2_r;
    float cab_lp1_s1_l, cab_lp1_s2_l;
    float cab_lp1_s1_r, cab_lp1_s2_r;
    float cab_lp2_s1_l, cab_lp2_s2_l;
    float cab_lp2_s1_r, cab_lp2_s2_r;
    int   dly_w;
    float dly_l[SLOT_DLY_MAX];
    float dly_r[SLOT_DLY_MAX];
} SlotDspUnit;

typedef struct ReverbUnit {
    float predly_l[24000];
    float predly_r[24000];
    int   predly_w;
    float fdn_buf[4][4096];
    int   fdn_w[4];
    float fdn_lp[4];
    float fdn_dc[4];
    float ap_l[2][1024];
    float ap_r[2][1024];
    int   ap_idx_l[2];
    int   ap_idx_r[2];
    float in_hp_l, in_hp_r;
    float in_lp_l, in_lp_r;
    float out_hp_l, out_hp_r;
    float out_lp_l, out_lp_r;
    float mod_phase;
    float duck_env;
    int   sr_cnt;
    float hold_l, hold_r;
    float decay;         /* 0..100% RT60 tail length */
    float size;          /* 0..100% Room geometry size */
    float damp;          /* 0..100% */
    float predelay;      /* 0..500 ms */
    float diffuse;       /* 0..100% */
    float ir_mix;        /* 0..100% Early Reflections IR body */
    float stereo;        /* 0..100% */
    float dry;           /* 0..100% */
    float wet;           /* 0..100% */
    float in_lowcut;     /* 20..1000 Hz Input HPF */
    float in_highcut;    /* 1000..18000 Hz Input LPF */
    float out_lowcut;    /* 20..1000 Hz Output Return HPF */
    float out_highcut;   /* 1000..18000 Hz Output Return LPF */
    float mod_rate;      /* 0..100% */
    float mod_amount;    /* 0..100% */
    float ducking;       /* 0..100% */
    float lofi_bits;     /* 4..24 bits */
    float lofi_sr;       /* 4000..48000 Hz */
    /* Rule 27: per-sample ~20 ms smoothed DSP values + dual read-head crossfade for Size & Pre-Delay (zero Doppler pitch-warp) */
    int   sm_init;
    float sm_size_scale, sm_fb, sm_damp, sm_wet, sm_ap_g, sm_er_mix, sm_width, sm_predly;
    float sm_in_hp_a, sm_in_lp_a, sm_out_hp_a, sm_out_lp_a;
    int   xf_active;
    float xf_pos;
    float xf_size_scale;
    float xf_predly;
} ReverbUnit;

typedef struct DelayUnit {
    float buf_l[96000];
    float buf_r[96000];
    int   write_idx;
    float smooth_samp;
    float smooth_samp_r;
    float in_hp_l, in_hp_r;
    float in_lp_l, in_lp_r;
    float fb_hp_l, fb_hp_r;
    float fb_lp_l, fb_lp_r;
    float out_hp_l, out_hp_r;
    float out_lp_l, out_lp_r;
    float ap_l[512];
    float ap_r[512];
    int   ap_idx;
    float mod_phase;
    float duck_env;
    float time_ms;       /* 10..1800 ms */
    float feedback;      /* 0..95% */
    float diffuse;       /* 0..100% */
    float dry;           /* 0..100% */
    float wet;           /* 0..100% */
    int   ping_pong;     /* 0=Stereo, 1=Ping-Pong */
    float in_lowcut;     /* 20..1000 Hz Input HPF */
    float in_highcut;    /* 1000..18000 Hz Input LPF */
    float out_lowcut;    /* 20..1000 Hz Output Return HPF */
    float out_highcut;   /* 1000..18000 Hz Output Return LPF */
    float drive;         /* 0..100% Tape Oxide Drive */
    float flutter;       /* 0..100% Tape Wow/Flutter */
    float mod_rate;      /* 0..100% Chorus Rate */
    float mod_depth;     /* 0..100% Chorus Depth */
    float ducking;       /* 0..100% Vocal Auto-Ducking */
    float stereo;        /* 0..100% Stereo Width */
    float spread;        /* 0..100% L/R Slap/Offset Spread */
    /* Rule 27: dual read-head equal-power crossfade for Delay Time, Tap Tempo & L/R Spread (no pitch dive-bomb) */
    float xf_target;     /* new L head delay (samples) while crossfading */
    float xf_target_r;   /* new R head delay (samples) while crossfading */
    float xf_pos;        /* 0..1 crossfade progress */
    int   xf_active;
    float mod_phase2;    /* independent 2.7x wow harmonic phase (no wrap discontinuity) */
    int   sm_init;
    float sm_fb, sm_wet, sm_width, sm_spread, sm_in_hp_a, sm_in_lp_a, sm_out_hp_a, sm_out_lp_a;
} DelayUnit;

typedef struct {
    jack_port_t* in_ports[NUM_CHANNELS];
    jack_port_t* in_ports_r[NUM_CHANNELS];
    int          ch_stereo_active[NUM_CHANNELS];
    jack_port_t* master_l;
    jack_port_t* master_r;
    jack_port_t* out_l;
    jack_port_t* out_r;
    jack_port_t* mon_ports[NUM_MONITORS];
    float sample_rate;

    /* Stage 1: 1176 FET Class-A Preamp Emulation (Precedes Stage 2 Regain) */
    int   preamp_enabled[NUM_CHANNELS];
    float preamp_drive_db[NUM_CHANNELS];
    float preamp_drive_lin[NUM_CHANNELS];
    float preamp_auto_comp[NUM_CHANNELS]; /* Precomputed inverse loudness compensation (Rule 20) */
    float preamp_trim_db[NUM_CHANNELS];
    float preamp_trim_lin[NUM_CHANNELS];
    int   preamp_clip_hard[NUM_CHANNELS]; /* 0 = Soft FET Tanh Sat, 1 = Hard Clip */
    float preamp_in_peak[NUM_CHANNELS];   /* Pre-Stage-2 raw input peak for 1176 telemetry */
    int   preamp_clip_status[NUM_CHANNELS]; /* 0 = CLEAN, 1 = FET SAT, 2 = CLIP! */

    /* Stage 2: Digital Pre-Gain, Polarity Invert (Ø) & 80Hz HPF */
    float ch_pregain_lin[NUM_CHANNELS];
    int   ch_phase_inv[NUM_CHANNELS];
    int   ch_hpf80[NUM_CHANNELS];
    BiquadFilter hpf_filters[NUM_CHANNELS];
    BiquadFilter hpf_filters_r[NUM_CHANNELS];

    /* Stage 3A: 4x Channel LV2/MOD-GUI Effect Insert Slots + Exclusive 3-Way Insert Routing + Auto-Gain Staging */
    SlotDspUnit ch_slots[NUM_CHANNELS][NUM_CH_SLOTS];
    int   slot_first[NUM_CHANNELS];       /* 1 = Insert Slots 1&2 (0..1) placed FIRST in channel chain */
    int   modgui_first[NUM_CHANNELS];     /* 1 = MOD-GUI Pedalboard (2..3) placed FIRST in channel chain */
    int   ch_insert_mode[NUM_CHANNELS];   /* 0 = Direct Clean Strip, 1 = 4 Slots Only, 2 = MOD Pedalboard Only (Rule 28) */
    float stage_env_in[NUM_CHANNELS][6];  /* Per-stage input RMS tracker for automatic unity gain staging */
    float stage_env_out[NUM_CHANNELS][6]; /* Per-stage output RMS tracker for automatic unity gain staging */
    float stage_gain[NUM_CHANNELS][6];    /* Smoothed per-stage automatic unity compensation gain */

    /* Stage 3B: Forensic Cyber-Denoiser PRO (10-Band Subtractive Spectral Phase-Cancellation Engine from de-noise-pro-vst) */
    int   gate_enabled[NUM_CHANNELS];
    float gate_thresh_lin[NUM_CHANNELS];
    float gate_floor_lin[NUM_CHANNELS];
    float gate_env[NUM_CHANNELS];
    float gate_gain[NUM_CHANNELS];
    int   gate_open[NUM_CHANNELS];
    float denoise_thresh_db[NUM_CHANNELS][10];   /* 10 bands: 60, 150, 400, 800, 1.5k, 3k, 5k, 8k, 12k, 16k (-100..0 dB) */
    float denoise_sensitivity_db[NUM_CHANNELS];  /* Master Sensitivity Offset (-20..+20 dB) */
    float denoise_reduction_pct[NUM_CHANNELS];   /* Reduction Depth (0..100%) */
    int   denoise_low_cut[NUM_CHANNELS];         /* 40Hz Sub-Rumble Filter (0/1) */
    int   denoise_listen[NUM_CHANNELS];          /* Delta Mode: Listen to Canceled Noise Stream (0/1) */
    float denoise_lp_l[NUM_CHANNELS][9];
    float denoise_lp_r[NUM_CHANNELS][9];
    float denoise_lc_lp_l[NUM_CHANNELS];
    float denoise_lc_lp_r[NUM_CHANNELS];
    float denoise_hc_lp_l[NUM_CHANNELS];
    float denoise_hc_lp_r[NUM_CHANNELS];
    float denoise_rms_sq[NUM_CHANNELS][10];
    float denoise_current_gains[NUM_CHANNELS][10];
    float denoise_band_levels[NUM_CHANNELS][10];
    float denoise_learn_peaks[NUM_CHANNELS][10];
    int   denoise_learn_active[NUM_CHANNELS];
    int   denoise_learn_samples_left[NUM_CHANNELS];
    int   denoise_learn_done_ch;

    /* Stage 4: 8-Band Cytomic SVF Equaliser per Channel, Monitor, Send, Return, and Master */
    EqStrip8 ch_eq[NUM_CHANNELS];
    EqStrip8 mon_eq[NUM_MONITORS];
    EqStrip8 send_eq[NUM_FX];
    EqStrip8 ret_eq[NUM_FX];
    EqStrip8 master_eq;

    /* Stage 6: 64-Bit Surgical -Infinity dB AFD Notch Banks (CH 1-8 Live + MON 1-6 & Master Lock) */
    AfdNotchBank ch_afd[NUM_CHANNELS];
    AfdNotchBank mon_afd[NUM_MONITORS];
    AfdNotchBank master_afd;

    /* Live 64-Band Logarithmic RTA Analyzer (20 Hz .. 20 kHz) */
    int   rta_target_kind; /* 1 = channel, 2 = bus */
    int   rta_target_ch;
    char  rta_target_bus[32];
    CytomicSvfBand rta_filters[NUM_RTA_BINS];
    float rta_env[NUM_RTA_BINS];
    float rta_db[NUM_RTA_BINS];

    /* Stage 5: Unified Dynamics Engine (Channels 0..7, Monitors 0..5 [Slot A + Slot B], Sends 0..3, Returns 0..3, Master [Slot A + Slot B]) */
    DynamicsSlot ch_dyn[NUM_CHANNELS];
    DynamicsSlot mon_dyn[NUM_MONITORS];   /* Monitor Slot A: Bus Compressor */
    DynamicsSlot mon_dyn2[NUM_MONITORS];  /* Monitor Slot B: Speaker Compressor / Brickwall Limiter */
    DynamicsSlot send_dyn[NUM_FX];
    DynamicsSlot ret_dyn[NUM_FX];
    DynamicsSlot master_dyn;              /* Master Slot A: Bus Glue Compressor */
    DynamicsSlot master_dyn2;             /* Master Slot B: Speaker Compressor / Brickwall Limiter */

    /* Stage 3A+: Per-Channel Real-Time 60FPS Cyber Tuner Pitch Detector & Mute-to-Tune */
    float tuner_lp[NUM_CHANNELS];
    float tuner_dc[NUM_CHANNELS];
    float tuner_prev[NUM_CHANNELS];
    float tuner_zc_samples[NUM_CHANNELS];
    float tuner_period[NUM_CHANNELS];
    float ch_pitch_hz[NUM_CHANNELS];
    int   ch_tuner_mute[NUM_CHANNELS];

    /* Stage 6: Dual-Band Zero-Phase Cytomic SVF Dynamic Sibilance & Harshness De-Esser */
    int   deesser_enabled[NUM_CHANNELS];     /* 1 = Active, 0 = Bypassed */
    float deesser_thresh_lin[NUM_CHANNELS];  /* Band 1 linear threshold (vocal sibilance 2k-8kHz) */
    float deesser_thresh1_db[NUM_CHANNELS];  /* Band 1 threshold in dB (-48..0 dB, default -22) */
    float deesser_freq1[NUM_CHANNELS];       /* Band 1 center frequency (2000..8500 Hz, default 4800) */
    float deesser_q1[NUM_CHANNELS];          /* Band 1 bandwidth Q (0.7..6.0, default 1.8) */
    float deesser_max1_db[NUM_CHANNELS];     /* Band 1 max reduction dB (0..24 dB, default 14) */
    float deesser_thresh2_lin[NUM_CHANNELS]; /* Band 2 linear threshold (air/harshness 5k-16kHz) */
    float deesser_thresh2_db[NUM_CHANNELS];  /* Band 2 threshold in dB (-48..0 dB, default -24) */
    float deesser_freq2[NUM_CHANNELS];       /* Band 2 center frequency (4500..16000 Hz, default 9500) */
    float deesser_q2[NUM_CHANNELS];          /* Band 2 bandwidth Q (0.7..6.0, default 1.6) */
    float deesser_max2_db[NUM_CHANNELS];     /* Band 2 max reduction dB (0..24 dB, default 12) */
    int   deesser_band2_en[NUM_CHANNELS];    /* 1 = Band 2 active (default 1) */
    float deesser_rel_coeff[NUM_CHANNELS];
    int   deesser_wide[NUM_CHANNELS];
    int   deesser_listen[NUM_CHANNELS];      /* 0=Off, 1=Solo Sibilance (B1+B2), 2=Solo B1, 3=Solo B2 */
    CytomicSvfBand deess_svf1[NUM_CHANNELS];
    CytomicSvfBand deess_svf2[NUM_CHANNELS];
    BiquadFilter deesser_hp_filters[NUM_CHANNELS];
    BiquadFilter deesser_hp_filters_r[NUM_CHANNELS];
    float deesser_env[NUM_CHANNELS];
    float deesser_env2[NUM_CHANNELS];
    float deesser_gr1_db[NUM_CHANNELS];      /* Live 60FPS Band 1 Gain Reduction (0..24 dB) */
    float deesser_gr2_db[NUM_CHANNELS];      /* Live 60FPS Band 2 Gain Reduction (0..24 dB) */
    float deesser_det1_db[NUM_CHANNELS];     /* Live 60FPS Band 1 Sibilance Detector Level dBFS */
    float deesser_det2_db[NUM_CHANNELS];     /* Live 60FPS Band 2 Harshness Detector Level dBFS */

    /* Channel Fader, Constant-Power Pan, Mute & Solo */
    float ch_fader_gain[NUM_CHANNELS];
    float ch_pan[NUM_CHANNELS]; /* -1.0 (L) to +1.0 (R), 0.0 = Center */
    int   ch_mute[NUM_CHANNELS];
    int   ch_solo[NUM_CHANNELS];

    /* Sends on Fader Matrix */
    float sof_mon_gain[NUM_MONITORS][NUM_CHANNELS];
    float sof_fx_gain[NUM_FX][NUM_CHANNELS];

    /* Bus Faders, Pans, Mutes & Solos */
    float mon_fader_gain[NUM_MONITORS];
    int   mon_mute[NUM_MONITORS];
    float send_fader_gain[NUM_FX];
    int   send_mute[NUM_FX];
    int   send_solo[NUM_FX];
    float ret_fader_gain[NUM_FX];
    float ret_pan[NUM_FX];
    int   ret_mute[NUM_FX];
    int   ret_solo[NUM_FX];

    float master_fader_gain;
    int   master_mute;

    /* Stage 6: Master Bus Analog Console Summing & Transformer Emulation */
    int   summing_enabled;      /* 1 = Active, 0 = Bypass */
    int   summing_console;      /* 0 = Neve 8048, 1 = API 2520, 2 = SSL 4000E, 3 = Germanium Tube */
    float summing_drive_db;     /* 0..12 dB (default 2.5) */
    float summing_warmth_db;    /* 0..10 dB low-end iron bloom (default 3.0) */
    float summing_crosstalk_db; /* -90..-40 dB stereo bleed (default -60.0) */
    float sum_lp_l, sum_lp_r;   /* 120 Hz transformer iron warmth state */

    /* Stage 6: Real -18 dBFS Pink Noise Room Calibration Engine */
    int   room_cal_active;
    char  room_cal_bus[32];
    int   room_cal_samples_left;
    int   room_cal_last_sec;
    int   room_cal_done_flag;
    float pink_b0, pink_b1, pink_b2, pink_b3, pink_b4, pink_b5, pink_b6;
    uint32_t rng_seed;

    /* Stage 5B: 2 Independent Stereo Reverbs (Send 1 -> Return 1: Instrument Room/Plate, Send 2 -> Return 2: Vocal Hall/Plate) */
    ReverbUnit rev[2];

    /* Stage 5B: 2 Independent Stereo Delays (Send 3 -> Return 3: Tape Slapback Engine, Send 4 -> Return 4: Modulated Vocal Delay) */
    DelayUnit dly[2];

    /* Live Peak & GR Telemetry */
    float peak_levels[NUM_CHANNELS];
    float ch_pre_levels[NUM_CHANNELS];
    float aux_peaks[NUM_MONITORS];
    float send_peaks[NUM_FX];
    float ret_peaks[NUM_FX];
    float master_peak_l;
    float master_peak_r;

    /* Live Gain Reduction Telemetry (dB positive reduction) */
    float ch_gr_db[NUM_CHANNELS];
    float mon_gr_db[NUM_MONITORS];
    float mon_gr2_db[NUM_MONITORS];
    float send_gr_db[NUM_FX];
    float ret_gr_db[NUM_FX];
    float master_gr_db;
    float master_gr2_db;

    /* Stage 7: 2" Analog Multitrack Tape Machine, 0 VU = -18 dBFS Metering & Song Folder Engine */
    int   tape_transport;     /* 0 = STOP, 1 = PLAY, 2 = RECORD */
    long  tape_frame_pos;     /* Current tape head frame index */
    long  tape_max_frames;    /* Recorded song length in frames */
    int   tape_ips;           /* 15 IPS (65Hz bump + 14kHz rolloff) or 30 IPS (110Hz bump + 18.5kHz rolloff) */
    int   tape_emu_enabled;   /* 1 = 2" Tape Saturation & Head-Bump active on Monitor Playback */
    int   tape_print_wav;     /* 1 = Bake 2" Tape Emulation into exported WAV stems */
    float tape_master_vol;    /* Stereo Playback Output level to MAIN L/R (0..2.0, default 1.0) */
    float tape_mon_send[NUM_MONITORS]; /* Stereo Playback Send level to MON 1-6 (0..1.5) */
    char  tape_base_dir[192]; /* Base recording directory: /var/modep/recordings (SD) or /media/... (USB SSD) */
    int   tape_dir_manual;    /* 1 = user manually set path, 0 = auto-switch to USB SSD when plugged in */
    float tape_free_gb;       /* Free disk space in GB on active recording drive */
    char  tape_drives_json[512]; /* Detected SD + USB drives JSON array */
    char  tape_song_name[64]; /* Current Song Folder name in <tape_base_dir>/<song_name>/ */
    int   tape_num_tracks;    /* Default 20 (0..7 Ch Post, 8..11 FX 1-4, 12..19 Ch Post-Preamp Direct) + Paired */
    int   tape_trk_src[MAX_TAPE_TRACKS];     /* Source channel index 0..7 (or FX 0..3) */
    int   tape_trk_is_direct[MAX_TAPE_TRACKS]; /* 0 = Processed/FX (Main View), 1 = Post-Preamp Direct (Sub-Page) */
    float tape_trk_vol[MAX_TAPE_TRACKS];     /* Monitor Playback Fader (1.0f default) */
    float tape_trk_pan[MAX_TAPE_TRACKS];     /* Monitor Playback Pan (-1..+1) */
    float tape_trk_drive_db[MAX_TAPE_TRACKS];/* Per-track 2" Tape Input Drive (-6..+12 dB) */
    int   tape_trk_mute[MAX_TAPE_TRACKS];    /* 0 for Processed/FX, 1 (muted) by default for Direct Dry */
    int   tape_trk_solo[MAX_TAPE_TRACKS];
    int   tape_trk_rec[MAX_TAPE_TRACKS];     /* Record Arm */
    float tape_trk_hb_lp[MAX_TAPE_TRACKS];   /* 15/30 IPS Head-Bump resonator state */
    float tape_trk_hf_lp[MAX_TAPE_TRACKS];   /* 15/30 IPS Oxide HF rolloff state */
    float tape_trk_vu_db[MAX_TAPE_TRACKS];   /* Calibrated VU meter: 0.0 VU = -18.0 dBFS! */
    int   tape_trk_clip[MAX_TAPE_TRACKS];    /* Digital -1.0 dBFS overload indicator */
    float *tape_play_ring[MAX_TAPE_TRACKS];  /* Lock-free SPSC ringbuffer for unlimited-length direct-from-disk playback */
    float *tape_disk_ring[MAX_TAPE_TRACKS];  /* Lock-free SPSC ringbuffer for unlimited-length direct-to-disk recording */
    volatile uint32_t ring_write_pos;
    volatile uint32_t ring_read_pos;
    volatile uint32_t play_write_pos;
    volatile uint32_t play_read_pos;
    volatile int      tape_seek_requested;
    FILE  *tape_disk_fp[MAX_TAPE_TRACKS];
    FILE  *tape_play_fp[MAX_TAPE_TRACKS];
    uint32_t tape_disk_frames_written;

    /* Stage 8: Universal Audio Interface Auto-Detection, Smart I/O Map & Solo/Cue Routing */
    int   mon_solo[NUM_MONITORS];
    int   solo_cue_to_main;     /* 1 = Route Soloed Monitor Bus (MON 1-6) to Main Out 1/2 for 2-out interfaces like EVO 4 */
    char  hw_card_id[64];       /* e.g. "EVO4", "UMC1820", "Dummy" */
    char  hw_card_name[128];    /* e.g. "AUDIENT EVO 4", "BEHRINGER UMC1820" */
    int   hw_in_count;          /* Physical capture port count */
    int   hw_out_count;         /* Physical playback port count */
    int   hw_buffer_size;       /* Active JACK period size (e.g. 128) */
    char  hw_cards_json[1024];  /* Detected ALSA soundcards JSON array */
    int   ch_input_patch[NUM_CHANNELS]; /* 0 = None/Virtual, 1..N = system:capture_N */
    volatile uint32_t jack_heartbeat;
    /* Live Pi & JACK Real-Time Telemetry */
    volatile uint32_t jack_xruns;
    float jack_dsp_load;        /* 0..100% JACK real-time DSP load */
    float sys_cpu_load;         /* 0..100% Raspberry Pi system CPU load */
    float pi_temp_c;            /* Raspberry Pi SoC temperature in Celsius */
    char  net_mode[16];         /* "wifi" or "hotspot" */
    char  net_ssid[64];         /* Active SSID ("VM99103602.4g" or "patchos") */
    char  net_ip[32];           /* Active IPv4 ("192.168.1.148" or "172.24.1.1") */
    char  net_policy[16];       /* "auto", "wifi", or "hotspot" */
} MeterState;

static MeterState g_state;
static jack_client_t *g_jack_client = NULL;
static volatile int g_jack_restart_requested = 0;
static struct lws_context *g_context = NULL;
static struct lws *g_wsi_list[32];
static int g_num_wsi = 0;
#define SYNC_RING_SIZE 256
static char g_sync_ring[SYNC_RING_SIZE][2048];
static struct lws *g_sync_sender[SYNC_RING_SIZE];
static unsigned int g_sync_head = 0;
static unsigned int g_wsi_sync_seq[32];
static int g_wsi_needs_init[32];
static int g_wsi_needs_meters[32];
static char g_latest_ui_snapshot[65536] = {0};
static volatile int g_state_save_dirty = 0;
static volatile long long g_last_state_change_ms = 0;
static int g_state_loaded_from_disk = 0;

static inline void mark_live_state_dirty(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_last_state_change_ms = (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000LL);
    g_state_save_dirty = 1;
}

static void init_hpf_biquad(BiquadFilter *f, float fc, float fs) {
    float q = 0.70710678f;
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * q);
    float a0 = 1.0f + alpha;
    f->b0 = ((1.0f + cos_w0) * 0.5f) / a0;
    f->b1 = (-(1.0f + cos_w0)) / a0;
    f->b2 = ((1.0f + cos_w0) * 0.5f) / a0;
    f->a1 = (-2.0f * cos_w0) / a0;
    f->a2 = (1.0f - alpha) / a0;
    f->z1 = 0.0f;
    f->z2 = 0.0f;
}

static inline float process_biquad(BiquadFilter *f, float x) {
    float y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}

/* Update Cytomic SVF coefficients matching ReEQ JSFX filter types 0..9 + Tukan Dynamic EQ S2 */
static void update_cytomic_svf_coeffs_with_gain(CytomicSvfBand *b, float fs, float eff_gain) {
    if (b->sm_freq <= 0.0f) { b->sm_freq = b->freq; b->sm_q = b->q; b->sm_gain = b->gain; }
    float f = b->sm_freq;
    if (f < 10.0f) f = 10.0f;
    if (f > 22000.0f) f = 22000.0f;
    float q = b->sm_q;
    if (q < 0.1f) q = 0.1f;
    if (q > 50.0f) q = 50.0f;
    b->k_base = 1.0f / q;
    int type = b->type;
    if (b->slope < 1) b->slope = 1;
    if (b->slope > 4) b->slope = 4;

    float g = tanf((float)M_PI * f / fs);
    float A = powf(10.0f, eff_gain / 40.0f);
    float k = 1.0f / q;
    float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;

    if (type == 0) { /* Peak / Bell */
        k = 1.0f / (q * A);
        m0 = 1.0f; m1 = k * (A * A - 1.0f); m2 = 0.0f;
    } else if (type == 1 || type == 2) { /* Low Cut / High Pass */
        k = (type == 2) ? 1.41421356f : (1.0f / q);
        m0 = 1.0f; m1 = -k; m2 = -1.0f;
    } else if (type == 3) { /* Low Shelf */
        g = g / sqrtf(A);
        k = 1.0f / q;
        m0 = 1.0f; m1 = k * (A - 1.0f); m2 = A * A - 1.0f;
    } else if (type == 4 || type == 9) { /* High Shelf / Tilt */
        g = g * sqrtf(A);
        k = 1.0f / q;
        m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A;
    } else if (type == 5 || type == 6) { /* High Cut / Low Pass */
        k = (type == 6) ? 1.41421356f : (1.0f / q);
        m0 = 0.0f; m1 = 0.0f; m2 = 1.0f;
    } else if (type == 7) { /* Notch */
        k = 1.0f / q;
        m0 = 1.0f; m1 = -k; m2 = 0.0f;
    } else if (type == 8) { /* Band Pass */
        k = 1.0f / q;
        m0 = 0.0f; m1 = k; m2 = 0.0f;
    }

    b->a1 = 1.0f / (1.0f + g * (g + k));
    b->a2 = g * b->a1;
    b->a3 = g * b->a2;
    b->m0 = m0;
    b->m1 = m1;
    b->m2 = m2;
}

static void update_cytomic_svf_coeffs(CytomicSvfBand *b, float fs) {
    if (b->sm_freq <= 0.0f) { b->sm_freq = b->freq; b->sm_q = b->q; b->sm_gain = b->gain; }
    update_cytomic_svf_coeffs_with_gain(b, fs, b->sm_gain + (b->dyn_enabled ? b->dyn_gain_db : 0.0f));
}

/* Rule 27: audio-thread control-rate ramp of EQ targets -> smoothed params -> coefficients.
 * Frequency glides in the log domain (musical, no lurch at low freqs); ~7 ms time constant. */
static inline void ramp_svf_band_params(CytomicSvfBand *b, float fs) {
    if (!b->param_dirty) return;
    const float k = 0.05f; /* per 16-sample tick @48k (~3 kHz) => tau ~6.7 ms */
    float raw_f = isfinite(b->freq) ? b->freq : 1000.0f;
    float raw_q = isfinite(b->q) ? b->q : 0.707f;
    float raw_g = isfinite(b->gain) ? b->gain : 0.0f;
    float tf = raw_f < 10.0f ? 10.0f : (raw_f > 22000.0f ? 22000.0f : raw_f);
    float tq = raw_q < 0.1f ? 0.1f : (raw_q > 50.0f ? 50.0f : raw_q);
    if (!isfinite(b->sm_freq) || b->sm_freq <= 0.0f) { b->sm_freq = tf; b->sm_q = tq; b->sm_gain = raw_g; }
    if (!isfinite(b->sm_q) || b->sm_q <= 0.0f) b->sm_q = tq;
    if (!isfinite(b->sm_gain)) b->sm_gain = raw_g;
    float lf = logf(b->sm_freq);
    lf += k * (logf(tf) - lf);
    b->sm_freq = expf(lf);
    b->sm_q += k * (tq - b->sm_q);
    b->sm_gain += k * (raw_g - b->sm_gain);
    int done = fabsf(b->sm_freq - tf) < tf * 0.0005f && fabsf(b->sm_q - tq) < 0.002f && fabsf(b->sm_gain - raw_g) < 0.01f;
    if (done) { b->sm_freq = tf; b->sm_q = tq; b->sm_gain = raw_g; b->param_dirty = 0; }
    update_cytomic_svf_coeffs_with_gain(b, fs, b->sm_gain + (b->dyn_enabled ? b->dyn_gain_db : 0.0f));
}

static void init_eq_strip(EqStrip8 *eq, float fs) {
    memset(eq, 0, sizeof(EqStrip8));
    eq->adaptive_room_mode = 0;
    const int   def_type[8]  = { 1, 3, 0, 0, 0, 0, 4, 5 };
    const float def_freq[8]  = { 30.0f, 100.0f, 250.0f, 800.0f, 2500.0f, 5000.0f, 10000.0f, 18000.0f };
    const float def_gain[8]  = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    const float def_q[8]     = { 0.707f, 0.707f, 1.41f, 1.0f, 1.2f, 1.41f, 0.707f, 0.707f };
    const int   def_slope[8] = { 2, 1, 1, 1, 1, 1, 1, 2 };
    const int   def_en[8]    = { 0, 1, 1, 1, 1, 1, 1, 0 };
    for (int i = 0; i < NUM_EQ_BANDS; i++) {
        eq->bands[i].enabled       = def_en[i];
        eq->bands[i].type          = def_type[i];
        eq->bands[i].freq          = def_freq[i];
        eq->bands[i].gain          = def_gain[i];
        eq->bands[i].q             = def_q[i];
        eq->bands[i].slope         = def_slope[i];
        eq->bands[i].dyn_enabled   = 0;
        eq->bands[i].dyn_thresh_db = -20.0f;
        eq->bands[i].dyn_range_db  = -6.0f;
        eq->bands[i].dyn_ratio     = 3.0f;
        eq->bands[i].dyn_env       = 0.0f;
        eq->bands[i].dyn_gain_db   = 0.0f;
        update_cytomic_svf_coeffs(&eq->bands[i], fs);
    }
}

static inline float process_cytomic_svf_band(CytomicSvfBand *b, float x, int right_ch, int adaptive_room, float fs) {
    if (!b->enabled) return x;
    int ctrl_tick = 0;
    if (!right_ch && ((++b->ctrl_cnt & 15) == 0)) {
        ctrl_tick = 1;
        ramp_svf_band_params(b, fs); /* Rule 27: glitch-free EQ moves, audio thread owns coeffs */
    }
    int stages = (b->type == 1 || b->type == 2 || b->type == 5 || b->type == 6) ? b->slope : 1;
    if (stages < 1) stages = 1;
    if (stages > 4) stages = 4;
    float v0 = x;
    float *ic1 = right_ch ? b->ic1eq_r : b->ic1eq;
    float *ic2 = right_ch ? b->ic2eq_r : b->ic2eq;
    int is_dyn_band = (b->dyn_enabled || adaptive_room) && (b->type == 0 || b->type == 3 || b->type == 4 || b->type == 9);
    for (int s = 0; s < stages; s++) {
        float v3 = v0 - ic2[s];
        float v1 = b->a1 * ic1[s] + b->a2 * v3;
        float v2 = ic2[s] + b->a2 * ic1[s] + b->a3 * v3;
        ic1[s] = 2.0f * v1 - ic1[s];
        ic2[s] = 2.0f * v2 - ic2[s];
        if (s == 0 && !right_ch && is_dyn_band) {
            /* Tukan Green Dynamic EQ S2 / FreeEQ8 SVF Sidechain Tap: k * v1 is the exact Band-Pass output */
            float bp_sc = fabsf(b->k_base * v1);
            if (!isfinite(bp_sc)) bp_sc = 0.0f;
            if (bp_sc > b->dyn_env) b->dyn_env = 0.995f * b->dyn_env + 0.005f * bp_sc;
            else b->dyn_env = 0.9996f * b->dyn_env + 0.0004f * bp_sc;
            if (ctrl_tick) {
                float env_db = (b->dyn_env > 0.00001f) ? (20.0f * log10f(b->dyn_env)) : -90.0f;
                float target_dyn = 0.0f;
                float thresh = isfinite(b->dyn_thresh_db) ? b->dyn_thresh_db : -20.0f;
                float range = isfinite(b->dyn_range_db) ? b->dyn_range_db : -6.0f;
                float ratio = (isfinite(b->dyn_ratio) && b->dyn_ratio > 1.1f) ? b->dyn_ratio : 3.0f;
                if (env_db > thresh) {
                    float over = env_db - thresh;
                    float comp = over * (1.0f - (1.0f / ratio));
                    if (range < 0.0f) target_dyn = -fminf(-range, comp);
                    else target_dyn = fminf(range, comp);
                } else if (adaptive_room && env_db < (thresh - 6.0f) && env_db > -68.0f) {
                    /* Fletcher-Munson Quiet-Volume Room Curve Support (Low Warmth <=180Hz & Air >=7.5kHz) */
                    if (b->freq <= 180.0f || b->freq >= 7500.0f) {
                        float under = (thresh - 6.0f) - env_db;
                        target_dyn = fminf(2.5f, under * 0.12f);
                    }
                }
                b->dyn_gain_db = 0.85f * b->dyn_gain_db + 0.15f * target_dyn;
                if (!isfinite(b->dyn_gain_db)) b->dyn_gain_db = 0.0f;
                update_cytomic_svf_coeffs_with_gain(b, fs, b->sm_gain + b->dyn_gain_db);
            }
        }
        v0 = b->m0 * v0 + b->m1 * v1 + b->m2 * v2;
    }
    if (!isfinite(v0)) {
        memset(ic1, 0, 4 * sizeof(float));
        memset(ic2, 0, 4 * sizeof(float));
        b->dyn_env = 0.0f;
        b->dyn_gain_db = 0.0f;
        v0 = 0.0f;
    }
    return v0;
}

static inline float process_eq_strip(EqStrip8 *eq, float x, int right_ch) {
    float fs = g_state.sample_rate > 0.0f ? g_state.sample_rate : 48000.0f;
    for (int i = 0; i < NUM_EQ_BANDS; i++) {
        if (eq->bands[i].enabled) {
            x = process_cytomic_svf_band(&eq->bands[i], x, right_ch, eq->adaptive_room_mode, fs);
        }
    }
    return x;
}

/* Stage 6: 64-Bit (double) Surgical -Infinity dB Notch Filter Bank (Zero Bus Stacking Required) */
static void update_afd_notch64_coeffs(AfdNotchFilter64 *n, double fs) {
    if (!n->active || n->freq < 20.0 || n->freq > 21000.0) {
        n->active = 0;
        return;
    }
    double q = n->q;
    if (q < 2.0) q = 2.0;
    if (q > 140.0) q = 140.0;
    double g = tan(M_PI * n->freq / fs);
    double k = 1.0 / q;
    if (n->inf_null || n->depth_db <= -48.0) {
        /* True -Infinity dB Zero-Delay Feedback SVF Spectral Null (m0=1, m1=-k, m2=0) */
        n->a1 = 1.0 / (1.0 + g * (g + k));
        n->a2 = g * n->a1;
        n->a3 = g * n->a2;
        n->m0 = 1.0;
        n->m1 = -k;
        n->m2 = 0.0;
    } else {
        /* Deep 64-bit Surgical Bell Cut (-3 dB down to -48 dB) */
        double A = pow(10.0, n->depth_db / 40.0);
        double k_bell = 1.0 / (q * A);
        n->a1 = 1.0 / (1.0 + g * (g + k_bell));
        n->a2 = g * n->a1;
        n->a3 = g * n->a2;
        n->m0 = 1.0;
        n->m1 = k_bell * (A * A - 1.0);
        n->m2 = 0.0;
    }
}

static inline float process_afd_bank64(AfdNotchBank *bank, float x, int right_ch) {
    if (!bank) return x;
    /* Fast check: skip entire notch bank if no notches are active */
    int any_active = 0;
    for (int i = 0; i < NUM_AFD_NOTCHES; i++) {
        if (bank->notches[i].active) { any_active = 1; break; }
    }
    if (!any_active) return x;

    double v0 = (double)x;
    for (int i = 0; i < NUM_AFD_NOTCHES; i++) {
        AfdNotchFilter64 *n = &bank->notches[i];
        if (!n->active) continue;
        double *ic1 = right_ch ? &n->ic1_r : &n->ic1_l;
        double *ic2 = right_ch ? &n->ic2_r : &n->ic2_l;
        double v3 = v0 - (*ic2);
        double v1 = n->a1 * (*ic1) + n->a2 * v3;
        double v2 = (*ic2) + n->a2 * (*ic1) + n->a3 * v3;
        *ic1 = 2.0 * v1 - (*ic1);
        *ic2 = 2.0 * v2 - (*ic2);
        v0 = n->m0 * v0 + n->m1 * v1 + n->m2 * v2;
    }
    return (float)v0;
}

static void init_rta_bank(MeterState *st, float fs) {
    st->rta_target_kind = 1;
    st->rta_target_ch = 0;
    strcpy(st->rta_target_bus, "master");
    for (int b = 0; b < NUM_RTA_BINS; b++) {
        float fc = 20.0f * powf(1000.0f, (float)b / (float)(NUM_RTA_BINS - 1));
        memset(&st->rta_filters[b], 0, sizeof(CytomicSvfBand));
        st->rta_filters[b].enabled = 1;
        st->rta_filters[b].type = 8; /* Band Pass */
        st->rta_filters[b].freq = fc;
        st->rta_filters[b].gain = 0.0f;
        st->rta_filters[b].q = 4.2f;
        st->rta_filters[b].slope = 1;
        update_cytomic_svf_coeffs(&st->rta_filters[b], fs);
        st->rta_env[b] = 0.0f;
        st->rta_db[b] = -80.0f;
    }
}

static inline void feed_rta_sample(MeterState *st, float x) {
    for (int b = 0; b < NUM_RTA_BINS; b++) {
        CytomicSvfBand *flt = &st->rta_filters[b];
        float v3 = x - flt->ic2eq[0];
        float v1 = flt->a1 * flt->ic1eq[0] + flt->a2 * v3;
        float v2 = flt->ic2eq[0] + flt->a2 * flt->ic1eq[0] + flt->a3 * v3;
        flt->ic1eq[0] = 2.0f * v1 - flt->ic1eq[0];
        flt->ic2eq[0] = 2.0f * v2 - flt->ic2eq[0];
        float bp = flt->m1 * v1;
        float av = fabsf(bp) * 1.75f; /* Compensate narrow bandpass energy density for true 0 dBFS peak readout */
        if (av > st->rta_env[b]) {
            st->rta_env[b] = 0.50f * st->rta_env[b] + 0.50f * av;
        } else {
            st->rta_env[b] = 0.99935f * st->rta_env[b] + 0.00065f * av;
        }
    }
}

static inline void update_dynamics_slot_coeffs(DynamicsSlot *d) {
    float drive_db = (d->fet_input_db - 18.0f) * 0.65f;
    d->fet_drive_lin = powf(10.0f, drive_db / 20.0f);
    float makeup_db = (d->fet_output_db - 20.0f) * 0.45f;
    d->fet_makeup_lin = powf(10.0f, makeup_db / 20.0f);

    float thresh_db = -10.0f - (d->lala_peak_red * 0.45f);
    d->lala_knee_lin = powf(10.0f, (thresh_db - 4.0f) / 20.0f);
    float la_makeup_db = (d->lala_gain - 32.0f) * 0.30f;
    d->lala_makeup_lin = powf(10.0f, la_makeup_db / 20.0f);
}

static void init_dynamics_slot(DynamicsSlot *d, int default_model, float fs) {
    memset(d, 0, sizeof(DynamicsSlot));
    d->comp_model = default_model;
    d->comp_bypass = 0;
    d->bypass_mix = 1.0f;
    d->fet_power = 1;
    d->fet_input_db = 18.0f;  /* 0.0 dB unity input drive (no unwanted default saturation/compression) */
    d->fet_output_db = 20.0f; /* 0.0 dB unity makeup */
    d->fet_attack = 3;
    d->fet_release = 4;
    d->fet_ratio = 4.0f;
    d->fet_det_env = 0.0f;
    d->fet_env_db = 0.0f;

    d->lala_power = 1;
    d->lala_peak_red = 15.0f; /* Gentle transparent optical threshold */
    d->lala_gain = 32.0f;     /* 0.0 dB unity optical makeup */
    d->lala_mix = 100.0f;
    d->lala_limit = 0;
    d->lala_det_env = 0.0f;
    d->lala_env_db = 0.0f;

    d->lim_power = 1;
    d->lim_thresh_db = -1.0f;
    d->lim_ceil_lin = 0.94406f; /* -0.5 dBFS transparent ceiling */
    d->lim_drive_lin = 1.0f;    /* 0.0 dB unity limiter input */
    d->lim_rel_coeff = expf(-1.0f / (0.025f * fs));
    d->lim_env_db = 0.0f;
    update_dynamics_slot_coeffs(d);
}

/* Hermite 4-Point Cubic Fractional Delay Interpolation:
   Eliminates 100% of integer-truncation zipper noise & metallic stepping when delay time or LFO moves! */
static inline float read_frac_hermite(const float *buf, int buf_len, int w_idx, float delay_samples) {
    if (delay_samples < 2.0f) delay_samples = 2.0f;
    if (delay_samples > (float)(buf_len - 4)) delay_samples = (float)(buf_len - 4);
    int i_dly = (int)delay_samples;
    float frac = delay_samples - (float)i_dly;
    int idx0 = w_idx - i_dly;
    while (idx0 < 0) idx0 += buf_len;
    while (idx0 >= buf_len) idx0 -= buf_len;
    int idx_m1 = (idx0 + 1 < buf_len) ? (idx0 + 1) : 0;
    int idx1   = (idx0 - 1 >= 0) ? (idx0 - 1) : (buf_len - 1);
    int idx2   = (idx1 - 1 >= 0) ? (idx1 - 1) : (buf_len - 1);

    float ym1 = buf[idx_m1];
    float y0  = buf[idx0];
    float y1  = buf[idx1];
    float y2  = buf[idx2];

    float c0 = y0;
    float c1 = 0.5f * (y1 - ym1);
    float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

/* Update Zero-Phase Cytomic SVF Band-Pass Detector & Subtractive Tap for Dual-Band De-Esser */
static void update_deesser_svf_band(CytomicSvfBand *b, float freq, float q, float fs) {
    if (fs < 8000.0f) fs = 48000.0f;
    if (freq < 1200.0f) freq = 1200.0f;
    if (freq > 18000.0f) freq = 18000.0f;
    if (q < 0.4f) q = 0.4f;
    if (q > 8.0f) q = 8.0f;
    b->enabled = 1;
    b->type = 8; /* Band Pass */
    b->freq = freq;
    b->q = q;
    if (b->sm_freq <= 0.0f) b->sm_freq = freq;
    if (b->sm_q <= 0.0f) b->sm_q = q;
    b->k_base = 1.0f / b->sm_q;
    float g = tanf((float)M_PI * b->sm_freq / fs);
    float k = 1.0f / b->sm_q;
    b->a1 = 1.0f / (1.0f + g * (g + k));
    b->a2 = g * b->a1;
    b->a3 = g * b->a2;
    b->m0 = 0.0f;
    b->m1 = k; /* Normalized unity-peak Band-Pass output = k * v1 */
    b->m2 = 0.0f;
}

static inline void ramp_deesser_svf_band(CytomicSvfBand *b, float fs) {
    if (!b->param_dirty) return;
    const float k = 0.06f;
    float tf = b->freq < 1200.0f ? 1200.0f : (b->freq > 18000.0f ? 18000.0f : b->freq);
    float tq = b->q < 0.4f ? 0.4f : (b->q > 8.0f ? 8.0f : b->q);
    if (b->sm_freq <= 0.0f) { b->sm_freq = tf; b->sm_q = tq; }
    float lf = logf(b->sm_freq);
    lf += k * (logf(tf) - lf);
    b->sm_freq = expf(lf);
    b->sm_q += k * (tq - b->sm_q);
    if (fabsf(b->sm_freq - tf) < tf * 0.0008f && fabsf(b->sm_q - tq) < 0.002f) {
        b->sm_freq = tf;
        b->sm_q = tq;
        b->param_dirty = 0;
    }
    update_deesser_svf_band(b, b->freq, b->q, fs);
}

/* Stage 1: Single-Knob Auto-Compensated 1176 Class-A FET Preamp Emulation
 * Models UTC O-12 Input Transformer + Class-A FET asymmetric 2nd/3rd-order harmonic saturation.
 * Automatically calculates inverse loudness compensation (`auto_comp`) for `drive_lin` so sweeping
 * DRIVE from 0 dB to +24 dB adds rich analog harmonic density and transient warmth without changing
 * overall volume or clipping Stage 2! */
static inline float process_1176_preamp_sample(float x, float drive_lin, float trim_lin, float auto_comp, int hard_clip, int *status_out) {
    if (drive_lin < 1.0f) drive_lin = 1.0f;
    float d = x * drive_lin;
    float abs_d = fabsf(d);
    if (hard_clip) {
        if (abs_d >= 0.89125f) {
            if (*status_out < 2) *status_out = 2; /* CLIP! */
            d = (d > 0.0f) ? 0.89125f : -0.89125f;
        } else if (abs_d >= 0.22f) {
            if (*status_out < 1) *status_out = 1; /* FET SAT */
        }
    } else {
        if (abs_d >= 0.75f) {
            if (*status_out < 2) *status_out = 2; /* CLIP! */
            float asym = d + 0.075f * d * abs_d;
            d = 0.89125f * tanhf(asym / 0.89125f);
        } else if (abs_d >= 0.16f) {
            if (*status_out < 1) *status_out = 1; /* FET SAT */
            float asym = d + 0.075f * d * abs_d;
            d = 0.89125f * tanhf(asym / 0.89125f);
        } else {
            /* Small-signal FET linear region (< -16 dBFS): tanhf is 99.8% linear, zero transcendental cost! */
            float asym = d + 0.075f * d * abs_d;
            d = asym;
        }
    }
    return d * auto_comp * trim_lin;
}

/* Stage 5A: FETISH 76 Fast FET Peak Compressor */
static inline float process_fet76_sample(DynamicsSlot *d, float x, float *gr_out) {
    if (!d->fet_power) {
        d->fet_det_env = 0.0f;
        d->fet_env_db = 0.0f;
        *gr_out = 0.0f;
        return x;
    }
    /* Precomputed linear input drive (Rule 20: zero per-sample powf) */
    float driven_x = x * d->fet_drive_lin;
    float abs_x = fabsf(driven_x);

    /* Fast Peak-Rectifier Detector Envelope (holds AC waveform peaks across zero-crossings) */
    if (abs_x > d->fet_det_env) {
        d->fet_det_env = abs_x;
    } else {
        d->fet_det_env = d->fet_det_env * 0.9985f + abs_x * 0.0015f;
    }

    float target_gr = 0.0f;
    /* -33 dBFS linear knee onset threshold is ~0.022387f (skip log10f when below knee) */
    if (d->fet_det_env > 0.022387f) {
        float in_db = 20.0f * log10f(d->fet_det_env);
        float thresh_db = -30.0f;
        float over_db = in_db - thresh_db;
        float ratio = (d->fet_ratio >= 1.5f) ? d->fet_ratio : 4.0f;
        float slope = 1.0f - (1.0f / ratio);
        if (over_db > -3.0f) {
            if (over_db < 3.0f) {
                float k = over_db + 3.0f;
                target_gr = (k * k / 12.0f) * slope;
            } else {
                target_gr = over_db * slope;
            }
        }
        if (target_gr > 24.0f) target_gr = 24.0f;
    }

    /* 1176 Attack (1=slowest, 7=fastest) & Release (1=slowest, 7=fastest) */
    float atk_alpha = 0.008f * (float)(d->fet_attack >= 1 ? d->fet_attack : 3);
    float rel_alpha = 0.00045f * (float)(d->fet_release >= 1 ? d->fet_release : 4);
    if (target_gr > d->fet_env_db) {
        d->fet_env_db += atk_alpha * (target_gr - d->fet_env_db);
    } else {
        d->fet_env_db += rel_alpha * (target_gr - d->fet_env_db);
        if (d->fet_env_db < 0.01f) d->fet_env_db = 0.0f;
    }
    *gr_out = d->fet_env_db;
    float total_gain = (d->fet_env_db > 0.005f) ? (d->fet_makeup_lin * powf(10.0f, -d->fet_env_db / 20.0f)) : d->fet_makeup_lin;
    return driven_x * total_gain;
}

/* Stage 5B: LALA Optical Leveling Amplifier */
static inline float process_lala_sample(DynamicsSlot *d, float x, float *gr_out) {
    if (!d->lala_power) {
        d->lala_det_env = 0.0f;
        d->lala_env_db = 0.0f;
        *gr_out = 0.0f;
        return x;
    }
    float abs_x = fabsf(x);
    if (abs_x > d->lala_det_env) {
        d->lala_det_env = d->lala_det_env * 0.92f + abs_x * 0.08f;
    } else {
        d->lala_det_env = d->lala_det_env * 0.9990f + abs_x * 0.0010f;
    }

    float target_gr = 0.0f;
    /* Skip log10f when below precalculated soft-knee onset linear threshold */
    if (d->lala_det_env > d->lala_knee_lin && d->lala_knee_lin > 0.00001f) {
        float in_db = 20.0f * log10f(d->lala_det_env);
        float thresh_db = -10.0f - (d->lala_peak_red * 0.45f);
        float over_db = in_db - thresh_db;
        float ratio = d->lala_limit ? 10.0f : 3.2f;
        float slope = 1.0f - (1.0f / ratio);
        if (over_db > -4.0f) {
            if (over_db < 4.0f) {
                float k = over_db + 4.0f;
                target_gr = (k * k / 16.0f) * slope;
            } else {
                target_gr = over_db * slope;
            }
        }
        if (target_gr > 24.0f) target_gr = 24.0f;
    }

    /* Smooth T4B Optical Cell Attack (~10 ms) & Release (~60 ms) */
    if (target_gr > d->lala_env_db) {
        d->lala_env_db += 0.0045f * (target_gr - d->lala_env_db);
    } else {
        d->lala_env_db += 0.00055f * (target_gr - d->lala_env_db);
        if (d->lala_env_db < 0.01f) d->lala_env_db = 0.0f;
    }
    *gr_out = d->lala_env_db;
    float wet_gain = (d->lala_env_db > 0.005f) ? (d->lala_makeup_lin * powf(10.0f, -d->lala_env_db / 20.0f)) : d->lala_makeup_lin;
    float wet = x * wet_gain;
    float mix = d->lala_mix * 0.01f;
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    return x * (1.0f - mix) + wet * mix;
}

/* Unified Channel / Bus Dynamics Processor (FET 76 | LALA | DUAL 76->LA | BUSTER Limiter)
 * Features click-free equal-power crossfading on Master Compressor Bypass (`comp_bypass`) */
static inline float process_dynamics_slot(DynamicsSlot *d, float x, float *gr_out) {
    float target_mix = d->comp_bypass ? 0.0f : 1.0f;
    d->bypass_mix += 0.008f * (target_mix - d->bypass_mix);
    if (d->bypass_mix < 0.0005f) {
        d->bypass_mix = 0.0f;
        d->fet_det_env = 0.0f;
        d->fet_env_db = 0.0f;
        d->lala_det_env = 0.0f;
        d->lala_env_db = 0.0f;
        d->lim_env_db = 0.0f;
        *gr_out = 0.0f;
        return x;
    }

    float wet = x;
    float raw_gr = 0.0f;
    if (d->comp_model == 0) {
        wet = process_fet76_sample(d, x, &raw_gr);
    } else if (d->comp_model == 1) {
        wet = process_lala_sample(d, x, &raw_gr);
    } else if (d->comp_model == 2) {
        /* DUAL SERIES: Stage A (FETISH 76 Peak Catcher) -> Stage B (LALA Optical Leveler) */
        float gr_fet = 0.0f, gr_la = 0.0f;
        float mid = process_fet76_sample(d, x, &gr_fet);
        wet = process_lala_sample(d, mid, &gr_la);
        raw_gr = gr_fet + gr_la;
        if (raw_gr > 24.0f) raw_gr = 24.0f;
    } else {
        /* BUSTER Brickwall Limiter */
        if (!d->lim_power) {
            raw_gr = 0.0f;
            wet = x;
        } else {
            float driven = x * d->lim_drive_lin;
            float abs_v = fabsf(driven);
            float ceil_v = d->lim_ceil_lin;
            float gr_db = 0.0f;
            if (abs_v > ceil_v && ceil_v > 0.01f) {
                gr_db = 20.0f * log10f(abs_v / ceil_v);
                driven = (driven > 0.0f) ? ceil_v : -ceil_v;
            }
            if (gr_db > d->lim_env_db) d->lim_env_db = gr_db;
            else d->lim_env_db = d->lim_env_db * 0.998f + gr_db * 0.002f;
            raw_gr = d->lim_env_db;
            wet = driven;
        }
    }
    *gr_out = raw_gr * d->bypass_mix;
    return x * (1.0f - d->bypass_mix) + wet * d->bypass_mix;
}

/* Stage 3: Authentic 10-Band Cyber-Denoiser PRO Engine (Ported from CyberDenoiserEngine.hpp / de-noise-pro-vst)
 * 10-Band Parallel 1st-Order Crossover Bank: 60Hz, 150Hz, 400Hz, 800Hz, 1.5kHz, 3kHz, 5kHz, 8kHz, 12kHz, 16kHz
 * + 40Hz Sub-Rumble Low Cut + 18kHz High Cut + Adaptive Tail-Stabilized Anti-Chirp Release + Delta Listen Mode */
static inline void process_cyber_denoiser_sample(MeterState *st, int ch, float *s_l, float *s_r, int is_stereo, float fs) {
    static const float xover_freqs[9] = { 60.0f, 150.0f, 400.0f, 800.0f, 1500.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f };
    static float cached_fs = 0.0f;
    static float lc_a = 0.9947f, hc_a = 0.095f, atk_coeff = 0.95f;
    static float xover_a[9];
    if (fs != cached_fs && fs > 8000.0f) {
        cached_fs = fs;
        lc_a = expf(-2.0f * (float)M_PI * 40.0f / fs);
        hc_a = expf(-2.0f * (float)M_PI * 18000.0f / fs);
        atk_coeff = expf(-1.0f / (fs * 0.0005f)); /* 0.5ms sub-millisecond transient attack */
        for (int b = 0; b < 9; b++) {
            xover_a[b] = expf(-2.0f * (float)M_PI * xover_freqs[b] / fs);
        }
    }

    if (!st->gate_enabled[ch] && !st->denoise_learn_active[ch]) {
        st->gate_open[ch] = 1;
        return;
    }

    float in0 = *s_l;
    float in1 = is_stereo ? *s_r : in0;
    if (fabsf(in0) < 1e-6f && (!is_stereo || fabsf(in1) < 1e-6f) && !st->denoise_learn_active[ch]) {
        st->gate_open[ch] = 0;
        return;
    }

    /* 1. 40Hz Sub-Rumble Low-Cut (1st-order HPF) */
    st->denoise_lc_lp_l[ch] = (1.0f - lc_a) * in0 + lc_a * st->denoise_lc_lp_l[ch];
    st->denoise_lc_lp_r[ch] = (1.0f - lc_a) * in1 + lc_a * st->denoise_lc_lp_r[ch];
    float s0 = st->denoise_low_cut[ch] ? (in0 - st->denoise_lc_lp_l[ch]) : in0;
    float s1 = st->denoise_low_cut[ch] ? (in1 - st->denoise_lc_lp_r[ch]) : in1;

    /* 2. 18kHz Ultrasonic High-Cut (1st-order LPF) */
    st->denoise_hc_lp_l[ch] = (1.0f - hc_a) * s0 + hc_a * st->denoise_hc_lp_l[ch];
    st->denoise_hc_lp_r[ch] = (1.0f - hc_a) * s1 + hc_a * st->denoise_hc_lp_r[ch];
    s0 = st->denoise_hc_lp_l[ch];
    s1 = st->denoise_hc_lp_r[ch];

    /* 3. Split into 10 Phase-Coherent Parallel Frequency Bands via 1st-Order Subtraction */
    float b0[10], b1[10];
    float rem0 = s0, rem1 = s1;
    for (int b = 0; b < 9; b++) {
        float a = xover_a[b];
        float lp0 = (1.0f - a) * rem0 + a * st->denoise_lp_l[ch][b];
        float lp1 = (1.0f - a) * rem1 + a * st->denoise_lp_r[ch][b];
        st->denoise_lp_l[ch][b] = lp0;
        st->denoise_lp_r[ch][b] = lp1;
        b0[b] = lp0;
        b1[b] = lp1;
        rem0 -= lp0;
        rem1 -= lp1;
    }
    b0[9] = rem0;
    b1[9] = rem1;

    /* 4. Per-Band Fast Transient-Assisted RMS Detection, Forensic Noise Learn, and Spectral Subtraction */
    float out0 = 0.0f, out1 = 0.0f;
    float sens_db = st->denoise_sensitivity_db[ch];
    float red_norm = st->denoise_reduction_pct[ch] * 0.01f;
    if (red_norm < 0.0f) red_norm = 0.0f;
    if (red_norm > 1.0f) red_norm = 1.0f;
    int any_band_open = 0;
    float inv_fs = 1.0f / (fs > 8000.0f ? fs : 48000.0f);

    /* Cache linear band thresholds per channel whenever threshold/sensitivity changes */
    static float cached_t_lin[NUM_CHANNELS][10];
    static float cached_eff_db[NUM_CHANNELS][10];

    for (int b = 0; b < 10; b++) {
        float pwr = 0.5f * (b0[b] * b0[b] + b1[b] * b1[b]);
        /* Instantaneous peak-assisted envelope so transients open the band in <0.5ms with zero swell lag */
        if (pwr > st->denoise_rms_sq[ch][b]) {
            st->denoise_rms_sq[ch][b] = 0.20f * st->denoise_rms_sq[ch][b] + 0.80f * pwr;
        } else {
            st->denoise_rms_sq[ch][b] = 0.990f * st->denoise_rms_sq[ch][b] + 0.010f * pwr;
        }
        float r = sqrtf(st->denoise_rms_sq[ch][b] + 1e-12f);

        if (st->denoise_learn_active[ch]) {
            if (r > st->denoise_learn_peaks[ch][b]) {
                st->denoise_learn_peaks[ch][b] = r;
            }
        }

        float eff_thresh_db = st->denoise_thresh_db[ch][b] + sens_db;
        if (eff_thresh_db != cached_eff_db[ch][b] || cached_t_lin[ch][b] <= 0.0f) {
            cached_eff_db[ch][b] = eff_thresh_db;
            cached_t_lin[ch][b] = powf(10.0f, eff_thresh_db / 20.0f);
        }
        float t_lin = cached_t_lin[ch][b];
        float target_g = (r > t_lin * 0.5f) ? (1.0f - (t_lin * 0.5f) / (r + 1e-9f)) : 0.0f;
        if (target_g < 0.0f) target_g = 0.0f;
        if (target_g > 1.0f) target_g = 1.0f;
        target_g = target_g * red_norm + (1.0f - red_norm);
        if (target_g > 0.45f) any_band_open = 1;

        /* Fast rational release approximation (eliminates 3.84M expf() calls/sec!) */
        float cur_g = st->denoise_current_gains[ch][b];
        if (target_g > cur_g) {
            cur_g = atk_coeff * cur_g + (1.0f - atk_coeff) * target_g;
        } else {
            float rel_time = 0.012f + (1.0f - target_g) * 0.065f;
            float rel_step = inv_fs / rel_time;
            if (rel_step > 0.05f) rel_step = 0.05f;
            cur_g += (target_g - cur_g) * rel_step;
        }
        st->denoise_current_gains[ch][b] = cur_g;
        out0 += b0[b] * cur_g;
        out1 += b1[b] * cur_g;
    }

    /* Finalize Learn Noise Window if countdown finished */
    if (st->denoise_learn_active[ch]) {
        if (--st->denoise_learn_samples_left[ch] <= 0) {
            st->denoise_learn_active[ch] = 0;
            float avg_t = 0.0f;
            for (int b = 0; b < 10; b++) {
                float pk = st->denoise_learn_peaks[ch][b];
                float learned_db = 20.0f * log10f(pk + 1e-6f) + 2.5f; /* +2.5dB headroom above noise floor */
                /* If input is completely silent (-117 dB), set a realistic studio noise profile (-58..-48 dB) */
                if (pk < 0.000015f) {
                    static const float default_profile[10] = { -48.0f, -50.0f, -54.0f, -56.0f, -58.0f, -58.0f, -56.0f, -54.0f, -52.0f, -50.0f };
                    learned_db = default_profile[b];
                }
                if (learned_db < -90.0f) learned_db = -90.0f;
                /* Safe studio noise-floor ceiling (-38 dBFS max) so accidental speech/signal during Learn Noise never gates the channel! */
                if (learned_db > -38.0f) learned_db = -38.0f;
                st->denoise_thresh_db[ch][b] = learned_db;
                st->denoise_current_gains[ch][b] = 1.0f;
                avg_t += learned_db;
            }
            avg_t *= 0.1f;
            st->gate_thresh_lin[ch] = powf(10.0f, avg_t / 20.0f);
            st->denoise_learn_done_ch = ch;
        }
    }

    if (!st->gate_enabled[ch]) {
        st->gate_open[ch] = 1;
        return;
    }

    st->gate_open[ch] = any_band_open;
    if (st->denoise_listen[ch]) {
        /* Delta Mode: Audition only the removed noise signal */
        *s_l = s0 - out0;
        if (is_stereo) *s_r = s1 - out1;
    } else {
        *s_l = out0;
        if (is_stereo) *s_r = out1;
    }
}

static int str_icontains(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 0;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i]) {
            char c1 = p[i], c2 = needle[i];
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            if (c1 != c2) break;
            i++;
        }
        if (i == nlen) return 1;
    }
    return 0;
}

static inline float process_biquad_transposed(float in, float b0, float b1, float b2, float a1, float a2, float *s1, float *s2) {
    float out = b0 * in + *s1;
    *s1 = b1 * in - a1 * out + *s2;
    *s2 = b2 * in - a2 * out;
    return out;
}

static inline void calc_biquad_lp(BiquadCoeffs *bc, float fc, float Q, float sr) {
    if (sr < 8000.0f) sr = 48000.0f;
    if (fc > sr * 0.45f) fc = sr * 0.45f;
    float omega = 6.2831853f * fc / sr;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * Q);
    float a0_inv = 1.0f / (1.0f + alpha);
    bc->b0 = ((1.0f - cs) * 0.5f) * a0_inv;
    bc->b1 = (1.0f - cs) * a0_inv;
    bc->b2 = ((1.0f - cs) * 0.5f) * a0_inv;
    bc->a1 = (-2.0f * cs) * a0_inv;
    bc->a2 = (1.0f - alpha) * a0_inv;
}

static inline void calc_biquad_lowshelf(BiquadCoeffs *bc, float fc, float gainDb, float Q, float sr) {
    if (sr < 8000.0f) sr = 48000.0f;
    if (fc > sr * 0.45f) fc = sr * 0.45f;
    float A = powf(10.0f, gainDb * 0.025f);
    float omega = 6.2831853f * fc / sr;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * Q);
    float beta = 2.0f * sqrtf(A) * alpha;
    float a0_inv = 1.0f / ((A + 1.0f) + (A - 1.0f) * cs + beta);
    bc->b0 = (A * ((A + 1.0f) - (A - 1.0f) * cs + beta)) * a0_inv;
    bc->b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cs)) * a0_inv;
    bc->b2 = (A * ((A + 1.0f) - (A - 1.0f) * cs - beta)) * a0_inv;
    bc->a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cs)) * a0_inv;
    bc->a2 = ((A + 1.0f) + (A - 1.0f) * cs - beta) * a0_inv;
}

static inline void calc_biquad_peaking(BiquadCoeffs *bc, float fc, float gainDb, float Q, float sr) {
    if (sr < 8000.0f) sr = 48000.0f;
    if (fc > sr * 0.45f) fc = sr * 0.45f;
    float A = powf(10.0f, gainDb * 0.025f);
    float omega = 6.2831853f * fc / sr;
    float sn = sinf(omega);
    float cs = cosf(omega);
    float alpha = sn / (2.0f * Q);
    float a0_inv = 1.0f / (1.0f + alpha / A);
    bc->b0 = (1.0f + alpha * A) * a0_inv;
    bc->b1 = (-2.0f * cs) * a0_inv;
    bc->b2 = (1.0f - alpha * A) * a0_inv;
    bc->a1 = (-2.0f * cs) * a0_inv;
    bc->a2 = (1.0f - alpha / A) * a0_inv;
}

static void update_slot_cab_filters(SlotDspUnit *u, float sr) {
    if (!u) return;
    if (sr < 8000.0f) sr = 48000.0f;
    switch (u->model_preset_id) {
        case 1: /* Fender Clean Twin Reverb (Jensen C12N 2x12) */
            calc_biquad_lowshelf(&u->cab_ls, 80.0f, 3.5f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 3200.0f, 2.5f, 1.2f, sr);
            calc_biquad_lp(&u->cab_lp1, 7200.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 7200.0f, 0.707f, sr);
            break;
        case 2: /* Marshall JTM45 Plexi (1960A Celestion 20W Greenbacks) */
            calc_biquad_lowshelf(&u->cab_ls, 90.0f, 5.5f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2400.0f, 4.2f, 1.3f, sr);
            calc_biquad_lp(&u->cab_lp1, 6800.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6800.0f, 0.707f, sr);
            break;
        case 3: /* Friedman BE-100 Brown Eye (4x12 Celestion V30/Greenback) */
            calc_biquad_lowshelf(&u->cab_ls, 100.0f, 6.0f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2200.0f, 4.5f, 1.3f, sr);
            calc_biquad_lp(&u->cab_lp1, 6500.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6500.0f, 0.707f, sr);
            break;
        case 4: /* Silvertone 1484 Twin Twelve (Jensen C12Q 2x12) */
            calc_biquad_lowshelf(&u->cab_ls, 95.0f, 4.5f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2600.0f, 3.8f, 1.4f, sr);
            calc_biquad_lp(&u->cab_lp1, 6200.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6200.0f, 0.707f, sr);
            break;
        case 5: /* Fender '59 Bassman Tweed (4x10 Jensen P10R Alnico) */
            calc_biquad_lowshelf(&u->cab_ls, 85.0f, 5.0f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2800.0f, 3.5f, 1.4f, sr);
            calc_biquad_lp(&u->cab_lp1, 6400.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6400.0f, 0.707f, sr);
            break;
        case 7: /* Vox AC30 Top Boost (2x12 Celestion Alnico Blue) */
            calc_biquad_lowshelf(&u->cab_ls, 85.0f, 4.2f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2900.0f, 4.8f, 1.4f, sr);
            calc_biquad_lp(&u->cab_lp1, 6500.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6500.0f, 0.707f, sr);
            break;
        case 8: /* Orange OR120 (4x12 Celestion V30) */
            calc_biquad_lowshelf(&u->cab_ls, 110.0f, 6.5f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2100.0f, 4.2f, 1.3f, sr);
            calc_biquad_lp(&u->cab_lp1, 6200.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6200.0f, 0.707f, sr);
            break;
        case 9: /* Dumble ODS Overdrive Special (1x12 EVM-12L) */
            calc_biquad_lowshelf(&u->cab_ls, 85.0f, 4.0f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2400.0f, 3.2f, 1.3f, sr);
            calc_biquad_lp(&u->cab_lp1, 6800.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6800.0f, 0.707f, sr);
            break;
        default: /* Preset 0 / Default Guitar Amp Cab */
            calc_biquad_lowshelf(&u->cab_ls, 90.0f, 5.0f, 0.707f, sr);
            calc_biquad_peaking(&u->cab_mid, 2500.0f, 4.0f, 1.3f, sr);
            calc_biquad_lp(&u->cab_lp1, 6600.0f, 0.707f, sr);
            calc_biquad_lp(&u->cab_lp2, 6600.0f, 0.707f, sr);
            break;
    }
}

/* Classify any LV2 URI or plugin name into one of 8 real-time DSP archetypes */
static int classify_slot_uri(const char *uri, const char *name) {
    const char *u = uri ? uri : "";
    const char *n = name ? name : "";

    /* Rule 29: Check Guitar Amp / Cab / NAM plugins FIRST to prevent misclassification as simple drive pedals */
    if (str_icontains(u, "marshall") || str_icontains(u, "friedman") || str_icontains(u, "fender") ||
        str_icontains(u, "bassman") || str_icontains(u, "jtm45") || str_icontains(u, "be100") ||
        str_icontains(u, "vox") || str_icontains(u, "ac30") || str_icontains(u, "orange") ||
        str_icontains(u, "or120") || str_icontains(u, "dumble") || str_icontains(u, "ods") ||
        str_icontains(u, "supro") || str_icontains(u, "hiwatt") || str_icontains(u, "ampeg") ||
        str_icontains(u, "twin") || str_icontains(u, "vibroverb") || str_icontains(u, "deluxe") ||
        str_icontains(u, "silvertone") || str_icontains(u, "1484") || str_icontains(u, "nam") ||
        str_icontains(u, "ultimate-hybrid") || str_icontains(u, "koch") || str_icontains(u, "tworock") ||
        str_icontains(u, "amp") || str_icontains(u, "cab") || str_icontains(n, "amp") ||
        str_icontains(n, "nam") || str_icontains(n, "marshall") || str_icontains(n, "friedman")) {
        return 2; /* Class 2: Tube Guitar/Bass Amp & Cab / NAM */
    }
    if (str_icontains(u, "screamer") || str_icontains(u, "klon") || str_icontains(u, "drive") ||
        str_icontains(u, "dist") || str_icontains(u, "fuzz") || str_icontains(u, "muff") ||
        str_icontains(u, "boost") || str_icontains(u, "rat") || str_icontains(n, "drive") ||
        str_icontains(n, "screamer") || str_icontains(n, "klon") || str_icontains(n, "fuzz") ||
        str_icontains(n, "dist") || str_icontains(n, "boost")) {
        return 1; /* Class 1: Overdrive / Distortion / Klon / Screamer / Fuzz */
    }
    if (str_icontains(u, "delay") || str_icontains(u, "echo") || str_icontains(u, "tape") ||
        str_icontains(u, "cloud-bloom") || str_icontains(n, "delay") || str_icontains(n, "echo")) {
        return 3; /* Class 3: Delay / Tape Echo / Cloud Bloom */
    }
    if (str_icontains(u, "reverb") || str_icontains(u, "spring") || str_icontains(u, "duskverb") ||
        str_icontains(u, "mverb") || str_icontains(u, "hall") || str_icontains(u, "plate") ||
        str_icontains(u, "shimmer") || str_icontains(n, "reverb") || str_icontains(n, "verb") || str_icontains(n, "spring")) {
        return 4; /* Class 4: Reverb / Spring / DuskVerb / Hall */
    }
    if (str_icontains(u, "synth") || str_icontains(u, "bass9") || str_icontains(u, "mel9") ||
        str_icontains(u, "organ9") || str_icontains(u, "string9") || str_icontains(u, "octave") ||
        str_icontains(u, "poly") || str_icontains(u, "pitch") || str_icontains(n, "synth") ||
        str_icontains(n, "bass9") || str_icontains(n, "mel9") || str_icontains(n, "organ") || str_icontains(n, "octave")) {
        return 6; /* Class 6: Poly / Octave / Bass9 / Mel9 / Organ9 / String9 Synth */
    }
    if (str_icontains(u, "chorus") || str_icontains(u, "flanger") || str_icontains(u, "phaser") ||
        str_icontains(u, "tremolo") || str_icontains(u, "vibrato") || str_icontains(u, "rotary") ||
        str_icontains(u, "univibe") || str_icontains(u, "mod") || str_icontains(n, "chorus") ||
        str_icontains(n, "phaser") || str_icontains(n, "flanger") || str_icontains(n, "tremolo")) {
        return 5; /* Class 5: Chorus / Phaser / Flanger / Tremolo Modulation */
    }
    if (str_icontains(u, "comp") || str_icontains(u, "sustain") || str_icontains(u, "limit") ||
        str_icontains(u, "gate") || str_icontains(u, "1176") || str_icontains(u, "la2a") ||
        str_icontains(n, "comp") || str_icontains(n, "limit") || str_icontains(n, "sustain")) {
        return 7; /* Class 7: Pedal/Rack Compressor & Sustainer */
    }
    if (str_icontains(u, "eq") || str_icontains(u, "filter") || str_icontains(u, "hum") ||
        str_icontains(u, "thump") || str_icontains(u, "wah") || str_icontains(u, "tone") ||
        str_icontains(u, "sculpt") || str_icontains(n, "eq") || str_icontains(n, "filter") ||
        str_icontains(n, "hum") || str_icontains(n, "thump")) {
        return 8; /* Class 8: EQ / Filter / Thump-Killer / Hum-Killer */
    }
    return 1; /* Default fallback: Warm Analog Saturation/Tone stage */
}

/* Dynamically adapt audio DSP voicing & tone-stack parameters based on loaded NAM/IR model character */
static void apply_slot_model_voicing(SlotDspUnit *u, const char *fpath, const char *flabel) {
    if (!u) return;
    if (u->p_level <= 0.05f) u->p_level = 0.72f;
    if (u->p_tone <= 0.05f) u->p_tone = 0.58f;
    char combined[512];
    snprintf(combined, sizeof(combined), "%s %s", fpath ? fpath : "", flabel ? flabel : "");
    for (int i = 0; combined[i]; i++) {
        if (combined[i] >= 'A' && combined[i] <= 'Z') combined[i] += 32;
    }

    if (strstr(combined, "acoustic") || strstr(combined, "d45") || strstr(combined, "mandolin")) {
        /* Pristine Acoustic Guitar / Body Air Shimmer: Linear transparent response, ~350Hz warmth, ~10kHz air */
        u->p_drive = 0.12f;
        u->p_tone = 0.72f;
        u->p_bass = 0.46f;
        u->p_mid = 0.48f;
        u->p_treble = 0.78f;
        u->sw_bright = 1;
        u->sw_mode = 0;
        u->model_preset_id = 6;
    } else if (strstr(combined, "be100") || strstr(combined, "friedman") || strstr(combined, "rock monster") || strstr(combined, "ultra gain") || strstr(combined, "lead") || strstr(combined, "heavy") || strstr(combined, "fuzz")) {
        /* High-Gain British Lead Monster: Cascaded 12AX7 high-gain saturation, Dave Friedman cold clipper */
        u->p_drive = 0.82f;
        u->p_tone = 0.66f;
        u->p_bass = 0.58f;
        u->p_mid = 0.70f;
        u->p_treble = 0.64f;
        u->sw_bright = 1;
        u->sw_mode = 1;
        u->model_preset_id = 3;
    } else if (strstr(combined, "jtm45") || strstr(combined, "marshall") || strstr(combined, "plexi") || strstr(combined, "dsl") || strstr(combined, "crunch") || strstr(combined, "edge")) {
        /* British Plexi / JTM45 Edge of Crunch: Tight dynamic pick attack, rich harmonic overtones, 4x12 cab body */
        u->p_drive = 0.66f;
        u->p_tone = 0.60f;
        u->p_bass = 0.54f;
        u->p_mid = 0.66f;
        u->p_treble = 0.58f;
        u->sw_bright = 1;
        u->sw_mode = 0;
        u->model_preset_id = 2;
    } else if (strstr(combined, "bassman") || strstr(combined, "5f6") || strstr(combined, "bass")) {
        /* Bassman / Low-End Foundation: Massive low punch, warm saturated mids */
        u->p_drive = 0.58f;
        u->p_tone = 0.52f;
        u->p_bass = 0.76f;
        u->p_mid = 0.54f;
        u->p_treble = 0.48f;
        u->sw_bright = 0;
        u->sw_mode = 0;
        u->model_preset_id = 5;
    } else if (strstr(combined, "vox") || strstr(combined, "ac30")) {
        /* Vox AC30 Top Boost: Chime & glassy upper mids into singing EL84 breakup */
        u->p_drive = 0.62f;
        u->p_tone = 0.65f;
        u->p_bass = 0.48f;
        u->p_mid = 0.58f;
        u->p_treble = 0.68f;
        u->sw_bright = 1;
        u->sw_mode = 0;
        u->model_preset_id = 7;
    } else if (strstr(combined, "orange") || strstr(combined, "or120")) {
        /* Orange OR120: F.A.C. low-end roar, heavy British tube crunch */
        u->p_drive = 0.70f;
        u->p_tone = 0.58f;
        u->p_bass = 0.68f;
        u->p_mid = 0.62f;
        u->p_treble = 0.55f;
        u->sw_bright = 0;
        u->sw_mode = 0;
        u->model_preset_id = 8;
    } else if (strstr(combined, "dumble") || strstr(combined, "ods")) {
        /* Dumble ODS Overdrive Special: Rich vocal sustain and bloom */
        u->p_drive = 0.68f;
        u->p_tone = 0.62f;
        u->p_bass = 0.52f;
        u->p_mid = 0.64f;
        u->p_treble = 0.60f;
        u->sw_bright = 1;
        u->sw_mode = 0;
        u->model_preset_id = 9;
    } else if (strstr(combined, "clean") || strstr(combined, "twin") || strstr(combined, "blackface") || strstr(combined, "california") || strstr(combined, "deluxe")) {
        /* Sparkling 60s American Clean (Twin Reverb / Blackface): High headroom, open-back 2x12 chime */
        u->p_drive = 0.35f;
        u->p_tone = 0.62f;
        u->p_bass = 0.52f;
        u->p_mid = 0.46f;
        u->p_treble = 0.68f;
        u->sw_bright = 1;
        u->sw_mode = 0;
        u->model_preset_id = 1;
    } else if (strstr(combined, "vibroverb") || strstr(combined, "aa763")) {
        /* Fender VibroVerb: 15-inch speaker deep punch, bright glassy bell clean, singing Texas blues lead */
        u->p_drive = 0.44f;
        u->p_tone = 0.64f;
        u->p_bass = 0.60f;
        u->p_mid = 0.52f;
        u->p_treble = 0.68f;
        u->sw_bright = 1;
        u->sw_mode = 0;
        u->model_preset_id = 1;
    } else if (strstr(combined, "princeton")) {
        /* Fender '65 Princeton: Warm dynamic 10-inch Jensen breakup, sweet edge-of-breakup compression */
        u->p_drive = 0.48f;
        u->p_tone = 0.58f;
        u->p_bass = 0.54f;
        u->p_mid = 0.50f;
        u->p_treble = 0.64f;
        u->sw_bright = 0;
        u->sw_mode = 0;
        u->model_preset_id = 1;
    } else if (strstr(combined, "5150") || strstr(combined, "6505") || strstr(combined, "6534") || strstr(combined, "blockletter")) {
        /* Peavey 5150 / 6534: Extreme high-gain modern lead, massive low-end chunk, tight percussive palm mutes */
        u->p_drive = 0.88f;
        u->p_tone = 0.66f;
        u->p_bass = 0.64f;
        u->p_mid = 0.70f;
        u->p_treble = 0.65f;
        u->sw_bright = 1;
        u->sw_mode = 1;
        u->model_preset_id = 3;
    } else if (strstr(combined, "ceriatone") || strstr(combined, "king kong")) {
        /* Ceriatone King Kong: Hot-rodded British Plexi / Jose Arredondo high-gain cascade */
        u->p_drive = 0.78f;
        u->p_tone = 0.65f;
        u->p_bass = 0.56f;
        u->p_mid = 0.68f;
        u->p_treble = 0.62f;
        u->sw_bright = 1;
        u->sw_mode = 1;
        u->model_preset_id = 3;
    } else if (strstr(combined, "magnatone") || strstr(combined, "m10a") || strstr(combined, "280")) {
        /* Magnatone M10A / 280: Sweet vintage vibrato, rich harmonically complex tube saturation */
        u->p_drive = 0.56f;
        u->p_tone = 0.62f;
        u->p_bass = 0.52f;
        u->p_mid = 0.60f;
        u->p_treble = 0.66f;
        u->sw_bright = 1;
        u->sw_mode = 0;
        u->model_preset_id = 7;
    } else if (strstr(combined, "5e3") || strstr(combined, "tweed deluxe") || (strstr(combined, "tweed") && !strstr(combined, "bassman"))) {
        /* 1960 Fender Tweed Deluxe 5E3: Raw interactive volume/tone breakup, spongy sag, fat midrange growl */
        u->p_drive = 0.62f;
        u->p_tone = 0.55f;
        u->p_bass = 0.66f;
        u->p_mid = 0.58f;
        u->p_treble = 0.52f;
        u->sw_bright = 0;
        u->sw_mode = 0;
        u->model_preset_id = 5;
    } else if (strstr(combined, "silvertone") || strstr(combined, "1484") || strstr(combined, "vintage")) {
        /* Vintage Silvertone 1484 Twin Twelve: Lo-fi garage rock warmth, sweet compressed midrange */
        u->p_drive = 0.62f;
        u->p_tone = 0.50f;
        u->p_bass = 0.56f;
        u->p_mid = 0.62f;
        u->p_treble = 0.52f;
        u->sw_bright = 0;
        u->sw_mode = 0;
        u->model_preset_id = 4;
    } else {
        /* Default Guitar Amp Voicing */
        u->p_drive = 0.58f;
        u->p_tone = 0.58f;
        u->p_bass = 0.52f;
        u->p_mid = 0.54f;
        u->p_treble = 0.56f;
        u->model_preset_id = 0;
    }
    update_slot_cab_filters(u, 48000.0f);

    /* Also map model/file character onto native LV2 ports for nam-loader and cyber-tone3000 */
    if (u->lv2_num_ports > 0) {
        int nam_prof = 0;
        int t3k_mod = 0;
        if (u->model_preset_id == 1) { nam_prof = 0; t3k_mod = 0; }
        else if (u->model_preset_id == 5) { nam_prof = 1; t3k_mod = 1; }
        else if (u->model_preset_id == 9) { nam_prof = 2; t3k_mod = 4; }
        else if (u->model_preset_id == 7) { nam_prof = 4; t3k_mod = 1; }
        else if (u->model_preset_id == 2) { nam_prof = 6; t3k_mod = 2; }
        else if (u->model_preset_id == 8) { nam_prof = 7; t3k_mod = 2; }
        else if (u->model_preset_id == 3) { nam_prof = 12; t3k_mod = 3; }
        else if (u->model_preset_id == 6) { nam_prof = 18; t3k_mod = 0; }
        for (uint32_t p = 0; p < u->lv2_num_ports && p < MAX_LV2_PORTS; p++) {
            if (strcmp(u->lv2_port_syms[p], "profile_a") == 0) u->lv2_port_vals[p] = (float)nam_prof;
            else if (strcmp(u->lv2_port_syms[p], "model") == 0) u->lv2_port_vals[p] = (float)t3k_mod;
        }
    }
}

/* Native LV2 URID Map callback for plugins requiring http://lv2plug.in/ns/ext/urid#map (e.g. nam-loader.lv2) */
static LV2_URID lv2_urid_map_fn(LV2_URID_Map_Handle handle, const char *uri) {
    (void)handle;
    if (!uri) return 0;
    if (strcmp(uri, "http://lv2plug.in/ns/ext/atom#Sequence") == 0) return 1;
    if (strcmp(uri, "http://lv2plug.in/ns/ext/midi#MidiEvent") == 0) return 2;
    uint32_t h = 2166136261u;
    for (const char *p = uri; *p; p++) {
        h ^= (uint8_t)(*p);
        h *= 16777619u;
    }
    return 3u + (h % 10000u);
}

typedef void* LV2_Worker_Schedule_Handle;
typedef enum { LV2_WORKER_SUCCESS = 0, LV2_WORKER_ERR_UNKNOWN = 1, LV2_WORKER_ERR_NO_SPACE = 2 } LV2_Worker_Status;
typedef struct {
    LV2_Worker_Schedule_Handle handle;
    LV2_Worker_Status (*schedule_work)(LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data);
} LV2_Worker_Schedule;

static LV2_Worker_Status dummy_schedule_work(LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data) {
    (void)handle; (void)size; (void)data;
    return LV2_WORKER_SUCCESS;
}

static LV2_URID_Map g_lv2_urid_map = { NULL, lv2_urid_map_fn };
static const LV2_Feature g_lv2_urid_feature = { "http://lv2plug.in/ns/ext/urid#map", &g_lv2_urid_map };
static LV2_Worker_Schedule g_lv2_worker_sched = { NULL, dummy_schedule_work };
static const LV2_Feature g_lv2_worker_feature = { "http://lv2plug.in/ns/ext/worker#schedule", &g_lv2_worker_sched };
static const LV2_Feature g_lv2_hard_rt_feature = { "http://lv2plug.in/ns/ext/buf-size#boundedBlockLength", NULL };
static const LV2_Feature* const g_lv2_features[] = { &g_lv2_urid_feature, &g_lv2_worker_feature, &g_lv2_hard_rt_feature, NULL };

static void unload_native_lv2_slot(SlotDspUnit *u) {
    if (!u) return;
    u->lv2_ready = 0;
    __sync_synchronize();
    if (u->dl_handle || u->lv2_handle_l || u->lv2_handle_r) {
        usleep(2500); /* Ensure any active JACK 128-sample callback finishes before freeing */
    }
    if (u->lv2_desc) {
        if (u->lv2_handle_l) {
            if (u->lv2_desc->deactivate) u->lv2_desc->deactivate(u->lv2_handle_l);
            if (u->lv2_desc->cleanup) u->lv2_desc->cleanup(u->lv2_handle_l);
            u->lv2_handle_l = NULL;
        }
        if (u->lv2_handle_r) {
            if (u->lv2_desc->deactivate) u->lv2_desc->deactivate(u->lv2_handle_r);
            if (u->lv2_desc->cleanup) u->lv2_desc->cleanup(u->lv2_handle_r);
            u->lv2_handle_r = NULL;
        }
        u->lv2_desc = NULL;
    }
    if (u->dl_handle) {
        dlclose(u->dl_handle);
        u->dl_handle = NULL;
    }
    u->lv2_num_ports = 0;
    u->num_audio_in = 0;
    u->num_audio_out = 0;
    u->bypass_port_idx = -1;
}

static void record_saved_slot_param(SlotDspUnit *u, const char *sym, float val) {
    if (!u || !sym || !*sym || sym[0] == ':') return;
    for (int i = 0; i < u->num_saved_params && i < MAX_SAVED_SLOT_PARAMS; i++) {
        if (strcasecmp(u->saved_param_syms[i], sym) == 0) {
            u->saved_param_vals[i] = val;
            return;
        }
    }
    if (u->num_saved_params < MAX_SAVED_SLOT_PARAMS) {
        int idx = u->num_saved_params++;
        snprintf(u->saved_param_syms[idx], sizeof(u->saved_param_syms[idx]), "%s", sym);
        u->saved_param_vals[idx] = val;
    }
}

static int set_native_lv2_port_by_sym(SlotDspUnit *u, const char *sym, float val) {
    if (!u || !sym || !*sym || u->lv2_num_ports == 0) return 0;
    for (uint32_t p = 0; p < u->lv2_num_ports && p < MAX_LV2_PORTS; p++) {
        if (u->lv2_port_is_ctrl[p] && u->lv2_port_is_input[p] && strcasecmp(u->lv2_port_syms[p], sym) == 0) {
            u->lv2_port_vals[p] = val;
            return 1;
        }
    }
    return 0;
}

static int find_lv2_bundle_dir(const char *uri, char *bundle_dir_out, size_t dir_len) {
    if (!uri || !*uri || strcmp(uri, "thru") == 0) return 0;
    static const char *search_roots[] = {
        "/usr/modep/lv2",
        "/var/modep/lv2",
        "/home/patch/plugins",
        "/usr/lib/lv2",
        "/usr/local/lib/lv2",
        "plugins",
        NULL
    };
    /* 1. Fast O(1) lookup by URI trailing slug (<slug>.lv2) */
    const char *last_slash = strrchr(uri, '/');
    const char *slug = last_slash ? (last_slash + 1) : uri;
    char clean_slug[128];
    snprintf(clean_slug, sizeof(clean_slug), "%s", slug);
    char *hash = strchr(clean_slug, '#');
    if (hash) *hash = '\0';

    if (clean_slug[0] != '\0') {
        for (int r = 0; search_roots[r]; r++) {
            char cand[512], mf[600];
            snprintf(cand, sizeof(cand), "%s/%s.lv2", search_roots[r], clean_slug);
            snprintf(mf, sizeof(mf), "%s/manifest.ttl", cand);
            struct stat st;
            if (stat(mf, &st) == 0) {
                snprintf(bundle_dir_out, dir_len, "%s", cand);
                return 1;
            }
        }
    }
    /* 2. Scan manifest.ttl files across LV2 roots for exact <uri> match */
    char uri_bracket[200];
    snprintf(uri_bracket, sizeof(uri_bracket), "<%s>", uri);
    for (int r = 0; search_roots[r]; r++) {
        DIR *d = opendir(search_roots[r]);
        if (!d) continue;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            size_t nlen = strlen(de->d_name);
            if (nlen < 5 || strcmp(de->d_name + nlen - 4, ".lv2") != 0) continue;
            char cand[512], mf[600];
            snprintf(cand, sizeof(cand), "%s/%s", search_roots[r], de->d_name);
            snprintf(mf, sizeof(mf), "%s/manifest.ttl", cand);
            FILE *fp = fopen(mf, "rb");
            if (!fp) continue;
            char buf[8192];
            size_t nr = fread(buf, 1, sizeof(buf) - 1, fp);
            fclose(fp);
            buf[nr] = '\0';
            if (strstr(buf, uri_bracket) || strstr(buf, uri)) {
                closedir(d);
                snprintf(bundle_dir_out, dir_len, "%s", cand);
                return 1;
            }
        }
        closedir(d);
    }
    return 0;
}

static int load_native_lv2_slot(SlotDspUnit *u, float fs) {
    if (!u) return 0;
    unload_native_lv2_slot(u);
    if (!u->active || !u->uri[0] || strcmp(u->uri, "thru") == 0) return 0;

    char bundle_dir[512] = {0};
    if (!find_lv2_bundle_dir(u->uri, bundle_dir, sizeof(bundle_dir))) {
        return 0;
    }

    /* Read all non-modgui .ttl files in the bundle directory to extract port metadata & binary name */
    static char ttl_buf[131072];
    size_t ttl_len = 0;
    ttl_buf[0] = '\0';
    DIR *bd = opendir(bundle_dir);
    if (!bd) return 0;
    struct dirent *de;
    char first_so_path[600] = {0};
    while ((de = readdir(bd)) != NULL) {
        size_t nl = strlen(de->d_name);
        if (nl > 3 && strcmp(de->d_name + nl - 3, ".so") == 0 && first_so_path[0] == '\0') {
            snprintf(first_so_path, sizeof(first_so_path), "%s/%s", bundle_dir, de->d_name);
        }
        if (nl > 4 && strcmp(de->d_name + nl - 4, ".ttl") == 0 && strstr(de->d_name, "modgui") == NULL) {
            char tpath[600];
            snprintf(tpath, sizeof(tpath), "%s/%s", bundle_dir, de->d_name);
            FILE *fp = fopen(tpath, "rb");
            if (fp) {
                size_t avail = sizeof(ttl_buf) - ttl_len - 2;
                if (avail > 0) {
                    size_t nr = fread(ttl_buf + ttl_len, 1, avail, fp);
                    ttl_len += nr;
                    ttl_buf[ttl_len++] = '\n';
                    ttl_buf[ttl_len] = '\0';
                }
                fclose(fp);
            }
        }
    }
    closedir(bd);
    if (first_so_path[0] == '\0') return 0;

    /* Parse lv2:port [ ... ] blocks for this plugin URI */
    memset(u->lv2_port_syms, 0, sizeof(u->lv2_port_syms));
    memset(u->lv2_port_defs, 0, sizeof(u->lv2_port_defs));
    memset(u->lv2_port_vals, 0, sizeof(u->lv2_port_vals));
    memset(u->lv2_port_out_vals, 0, sizeof(u->lv2_port_out_vals));
    memset(u->lv2_port_is_ctrl, 0, sizeof(u->lv2_port_is_ctrl));
    memset(u->lv2_port_is_atom, 0, sizeof(u->lv2_port_is_atom));
    memset(u->lv2_port_is_input, 0, sizeof(u->lv2_port_is_input));
    u->lv2_num_ports = 0;
    u->num_audio_in = 0;
    u->num_audio_out = 0;
    u->bypass_port_idx = -1;

    const char *port_sec = strstr(ttl_buf, "lv2:port");
    while (port_sec) {
        const char *p = port_sec + 8;
        int depth = 0;
        const char *blk_start = NULL;
        while (*p) {
            if (*p == '[') {
                if (depth == 0) blk_start = p + 1;
                depth++;
            } else if (*p == ']') {
                depth--;
                if (depth == 0 && blk_start) {
                    size_t blen = (size_t)(p - blk_start);
                    char blk[4096];
                    if (blen >= sizeof(blk)) blen = sizeof(blk) - 1;
                    memcpy(blk, blk_start, blen);
                    blk[blen] = '\0';

                    const char *idx_p = strstr(blk, "lv2:index");
                    if (idx_p) {
                        idx_p += 9;
                        while (*idx_p == ' ' || *idx_p == '\t' || *idx_p == '\r' || *idx_p == '\n') idx_p++;
                        int idx = atoi(idx_p);
                        if (idx >= 0 && idx < MAX_LV2_PORTS) {
                            if ((uint32_t)(idx + 1) > u->lv2_num_ports) u->lv2_num_ports = (uint32_t)(idx + 1);
                            int is_in  = (strstr(blk, "InputPort") != NULL) ? 1 : 0;
                            int is_aud = (strstr(blk, "AudioPort") != NULL) ? 1 : 0;
                            int is_ctl = (strstr(blk, "ControlPort") != NULL) ? 1 : 0;
                            int is_atm = (strstr(blk, "AtomPort") != NULL) ? 1 : 0;
                            u->lv2_port_is_input[idx] = (uint8_t)is_in;
                            u->lv2_port_is_ctrl[idx]  = (uint8_t)is_ctl;
                            u->lv2_port_is_atom[idx]  = (uint8_t)is_atm;
                            if (is_aud) {
                                if (is_in && u->num_audio_in < 4) u->audio_in_idx[u->num_audio_in++] = idx;
                                else if (!is_in && u->num_audio_out < 4) u->audio_out_idx[u->num_audio_out++] = idx;
                            }
                            const char *sym_p = strstr(blk, "lv2:symbol");
                            if (sym_p) {
                                const char *q1 = strchr(sym_p, '"');
                                const char *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
                                if (q1 && q2 && q2 > q1) {
                                    size_t slen = (size_t)(q2 - (q1 + 1));
                                    if (slen >= sizeof(u->lv2_port_syms[idx])) slen = sizeof(u->lv2_port_syms[idx]) - 1;
                                    memcpy(u->lv2_port_syms[idx], q1 + 1, slen);
                                    u->lv2_port_syms[idx][slen] = '\0';
                                }
                            }
                            float def_v = 0.0f;
                            const char *def_p = strstr(blk, "lv2:default");
                            if (def_p) {
                                def_p += 11;
                                while (*def_p == ' ' || *def_p == '\t' || *def_p == '\r' || *def_p == '\n') def_p++;
                                def_v = strtof(def_p, NULL);
                            }
                            u->lv2_port_defs[idx] = def_v;
                            u->lv2_port_vals[idx] = def_v;
                            if (strstr(blk, "lv2:enabled") || strcasecmp(u->lv2_port_syms[idx], "bypass") == 0 || strcasecmp(u->lv2_port_syms[idx], "enabled") == 0) {
                                u->bypass_port_idx = idx;
                                u->lv2_port_vals[idx] = 1.0f; /* Active by default */
                            }
                        }
                    }
                    blk_start = NULL;
                }
            } else if (depth == 0 && (*p == '.' || *p == ';')) {
                break;
            }
            p++;
        }
        port_sec = strstr(p, "lv2:port");
    }

    if (u->lv2_num_ports == 0 || u->num_audio_in == 0 || u->num_audio_out == 0) {
        return 0;
    }

    void *dl = dlopen(first_so_path, RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
        fprintf(stderr, "[LV2-Loader] dlopen failed for %s: %s\n", first_so_path, dlerror());
        return 0;
    }
    LV2_Descriptor_Function dfn = (LV2_Descriptor_Function)dlsym(dl, "lv2_descriptor");
    if (!dfn) {
        dlclose(dl);
        return 0;
    }
    const LV2_Descriptor *desc = NULL;
    for (uint32_t d_i = 0; ; d_i++) {
        const LV2_Descriptor *cand = dfn(d_i);
        if (!cand) break;
        if (!desc) desc = cand;
        if (cand->URI && strcmp(cand->URI, u->uri) == 0) {
            desc = cand;
            break;
        }
    }
    if (!desc || !desc->instantiate || !desc->connect_port || !desc->run) {
        dlclose(dl);
        return 0;
    }

    char bundle_path_slash[520];
    snprintf(bundle_path_slash, sizeof(bundle_path_slash), "%s/", bundle_dir);
    double sr = (fs > 8000.0f) ? (double)fs : 48000.0;
    LV2_Handle hl = desc->instantiate(desc, sr, bundle_path_slash, g_lv2_features);
    if (!hl) {
        dlclose(dl);
        return 0;
    }
    LV2_Handle hr = NULL;
    if (u->num_audio_in == 1 && u->num_audio_out == 1) {
        hr = desc->instantiate(desc, sr, bundle_path_slash, g_lv2_features);
    }

    u->atom_in_buf.size = 8;
    u->atom_in_buf.type = 1;
    u->atom_in_buf.unit = 0;
    u->atom_in_buf.pad  = 0;
    u->atom_out_buf.size = sizeof(u->atom_out_buf.body);
    u->atom_out_buf.type = 0;
    u->atom_out_buf.unit = 0;
    u->atom_out_buf.pad  = 0;

    for (uint32_t p = 0; p < u->lv2_num_ports && p < MAX_LV2_PORTS; p++) {
        if (u->num_audio_in >= 1 && (int)p == u->audio_in_idx[0]) {
            desc->connect_port(hl, p, u->buf_in_l);
            if (hr) desc->connect_port(hr, p, u->buf_in_r);
        } else if (u->num_audio_in >= 2 && (int)p == u->audio_in_idx[1]) {
            desc->connect_port(hl, p, u->buf_in_r);
        } else if (u->num_audio_out >= 1 && (int)p == u->audio_out_idx[0]) {
            desc->connect_port(hl, p, u->buf_out_l);
            if (hr) desc->connect_port(hr, p, u->buf_out_r);
        } else if (u->num_audio_out >= 2 && (int)p == u->audio_out_idx[1]) {
            desc->connect_port(hl, p, u->buf_out_r);
        } else if (u->lv2_port_is_atom[p]) {
            void *abuf = u->lv2_port_is_input[p] ? (void*)&u->atom_in_buf : (void*)&u->atom_out_buf;
            desc->connect_port(hl, p, abuf);
            if (hr) desc->connect_port(hr, p, abuf);
        } else if (u->lv2_port_is_input[p]) {
            desc->connect_port(hl, p, &u->lv2_port_vals[p]);
            if (hr) desc->connect_port(hr, p, &u->lv2_port_vals[p]);
        } else {
            desc->connect_port(hl, p, &u->lv2_port_out_vals[p]);
            if (hr) desc->connect_port(hr, p, &u->lv2_port_out_vals[p]);
        }
    }

    if (desc->activate) {
        desc->activate(hl);
        if (hr) desc->activate(hr);
    }

    /* Re-apply any saved user parameters to the freshly loaded LV2 control ports */
    for (int k = 0; k < u->num_saved_params && k < MAX_SAVED_SLOT_PARAMS; k++) {
        set_native_lv2_port_by_sym(u, u->saved_param_syms[k], u->saved_param_vals[k]);
    }

    u->dl_handle = dl;
    u->lv2_desc = desc;
    u->lv2_handle_l = hl;
    u->lv2_handle_r = hr;
    __sync_synchronize();
    u->lv2_ready = 1;
    fprintf(stderr, "[LV2-Loader] Loaded native .so for '%s' (%s, %u ports, %d in / %d out)\n",
        u->uri, first_so_path, u->lv2_num_ports, u->num_audio_in, u->num_audio_out);
    return 1;
}

static void init_slot_dsp_unit(SlotDspUnit *u, const char *uri, const char *name) {
    int prev_w = u->dly_w;
    unload_native_lv2_slot(u);
    u->num_saved_params = 0;
    u->active = (uri && *uri && strcmp(uri, "thru") != 0) ? 1 : 0;
    u->bypass = 0;
    snprintf(u->uri, sizeof(u->uri), "%s", uri ? uri : "");
    snprintf(u->name, sizeof(u->name), "%s", name ? name : "");
    u->file_path[0] = '\0';
    u->file_label[0] = '\0';
    u->fx_class = u->active ? classify_slot_uri(u->uri, u->name) : 0;
    u->p_drive = 0.58f;
    u->p_tone = 0.58f;
    u->p_bass = 0.52f;
    u->p_mid = 0.54f;
    u->p_treble = 0.56f;
    u->p_level = 0.72f;
    u->p_mix = (u->fx_class == 3 || u->fx_class == 4 || u->fx_class == 5) ? 0.42f : 1.0f;
    u->p_time = 0.45f;
    u->p_feedback = 0.42f;
    u->p_rate = 0.35f;
    u->p_depth = 0.50f;
    u->sw_bright = 0;
    u->sw_mode = 0;
    u->model_preset_id = 0;
    u->dly_w = (prev_w >= 0 && prev_w < SLOT_DLY_MAX) ? prev_w : 0;

    /* Rule 29: Identify authentic cyber amp model directly from plugin URI or name */
    if (u->fx_class == 2) {
        char combined[512];
        snprintf(combined, sizeof(combined), "%s %s", u->uri, u->name);
        for (int i = 0; combined[i]; i++) {
            if (combined[i] >= 'A' && combined[i] <= 'Z') combined[i] += 32;
        }
        if (strstr(combined, "be100") || strstr(combined, "friedman") || strstr(combined, "rock monster") || strstr(combined, "lead")) {
            u->model_preset_id = 3; /* Friedman BE-100 Brown Eye */
            u->p_drive = 0.82f; u->p_bass = 0.56f; u->p_mid = 0.68f; u->p_treble = 0.64f;
            u->sw_bright = 1; u->sw_mode = 1;
        } else if (strstr(combined, "jtm45") || strstr(combined, "marshall") || strstr(combined, "plexi") || strstr(combined, "dsl") || strstr(combined, "crunch")) {
            u->model_preset_id = 2; /* Marshall JTM45 Plexi */
            u->p_drive = 0.66f; u->p_bass = 0.54f; u->p_mid = 0.66f; u->p_treble = 0.58f;
            u->sw_bright = 1; u->sw_mode = 0;
        } else if (strstr(combined, "bassman") || strstr(combined, "5f6") || strstr(combined, "bass")) {
            u->model_preset_id = 5; /* Fender '59 Bassman Tweed */
            u->p_drive = 0.58f; u->p_bass = 0.74f; u->p_mid = 0.54f; u->p_treble = 0.48f;
            u->sw_bright = 0; u->sw_mode = 0;
        } else if (strstr(combined, "vox") || strstr(combined, "ac30")) {
            u->model_preset_id = 7; /* Vox AC30 Top Boost */
            u->p_drive = 0.62f; u->p_bass = 0.48f; u->p_mid = 0.58f; u->p_treble = 0.68f;
            u->sw_bright = 1; u->sw_mode = 0;
        } else if (strstr(combined, "orange") || strstr(combined, "or120")) {
            u->model_preset_id = 8; /* Orange OR120 */
            u->p_drive = 0.70f; u->p_bass = 0.68f; u->p_mid = 0.62f; u->p_treble = 0.55f;
            u->sw_bright = 0; u->sw_mode = 0;
        } else if (strstr(combined, "dumble") || strstr(combined, "ods")) {
            u->model_preset_id = 9; /* Dumble ODS */
            u->p_drive = 0.68f; u->p_bass = 0.52f; u->p_mid = 0.64f; u->p_treble = 0.60f;
            u->sw_bright = 1; u->sw_mode = 0;
        } else if (strstr(combined, "clean") || strstr(combined, "twin") || strstr(combined, "blackface") || strstr(combined, "deluxe") || strstr(combined, "california")) {
            u->model_preset_id = 1; /* Fender Clean Twin / Blackface */
            u->p_drive = 0.35f; u->p_bass = 0.52f; u->p_mid = 0.46f; u->p_treble = 0.68f;
            u->sw_bright = 1; u->sw_mode = 0;
        } else if (strstr(combined, "silvertone") || strstr(combined, "1484") || strstr(combined, "vintage")) {
            u->model_preset_id = 4; /* Silvertone 1484 */
            u->p_drive = 0.62f; u->p_bass = 0.56f; u->p_mid = 0.62f; u->p_treble = 0.52f;
            u->sw_bright = 0; u->sw_mode = 0;
        } else if (strstr(combined, "acoustic") || strstr(combined, "d45")) {
            u->model_preset_id = 6; /* Acoustic Body Air */
            u->p_drive = 0.12f; u->p_bass = 0.46f; u->p_mid = 0.48f; u->p_treble = 0.78f;
            u->sw_bright = 1; u->sw_mode = 0;
        }
    }
    update_slot_cab_filters(u, 48000.0f);
    if (u->active) {
        load_native_lv2_slot(u, 48000.0f);
    }
}

/* Authentic built-in NAM models mapper for nam-loader.lv2 (profile_a & profile_b 0..23) */
static void apply_slot_nam_profile(SlotDspUnit *u, int profile_id) {
    if (!u) return;
    if (profile_id < 0) profile_id = 0;
    if (profile_id > 23) profile_id = 23;
    u->active = 1;
    u->bypass = 0;
    if (u->p_level <= 0.05f) u->p_level = 0.72f;
    if (u->p_tone <= 0.05f) u->p_tone = 0.58f;
    if (profile_id <= 18) {
        /* Amplifier and Preamp models */
        u->fx_class = 2;
        switch (profile_id) {
            case 0: /* Fender '65 Deluxe Reverb */
                u->model_preset_id = 1;
                u->p_drive = 0.38f; u->p_bass = 0.52f; u->p_mid = 0.46f; u->p_treble = 0.68f;
                u->sw_bright = 1; u->sw_mode = 0;
                break;
            case 1: /* Fender '59 Tweed Bassman */
                u->model_preset_id = 5;
                u->p_drive = 0.58f; u->p_bass = 0.74f; u->p_mid = 0.54f; u->p_treble = 0.48f;
                u->sw_bright = 0; u->sw_mode = 0;
                break;
            case 2: /* Dumble ODS Clean Channel */
                u->model_preset_id = 9;
                u->p_drive = 0.45f; u->p_bass = 0.52f; u->p_mid = 0.60f; u->p_treble = 0.64f;
                u->sw_bright = 1; u->sw_mode = 0;
                break;
            case 3: /* Matchless DC-30 Clean */
                u->model_preset_id = 7;
                u->p_drive = 0.42f; u->p_bass = 0.50f; u->p_mid = 0.56f; u->p_treble = 0.72f;
                u->sw_bright = 1; u->sw_mode = 0;
                break;
            case 4: /* Vox AC30 Top Boost 1964 */
                u->model_preset_id = 7;
                u->p_drive = 0.62f; u->p_bass = 0.48f; u->p_mid = 0.58f; u->p_treble = 0.68f;
                u->sw_bright = 1; u->sw_mode = 0;
                break;
            case 5: /* Marshall Bluesbreaker 1962 */
                u->model_preset_id = 2;
                u->p_drive = 0.60f; u->p_bass = 0.56f; u->p_mid = 0.64f; u->p_treble = 0.56f;
                u->sw_bright = 1; u->sw_mode = 0;
                break;
            case 6: /* Marshall JTM45 Plexi */
                u->model_preset_id = 2;
                u->p_drive = 0.66f; u->p_bass = 0.54f; u->p_mid = 0.66f; u->p_treble = 0.58f;
                u->sw_bright = 1; u->sw_mode = 0;
                break;
            case 7: /* Orange Rockerverb 50 */
                u->model_preset_id = 8;
                u->p_drive = 0.72f; u->p_bass = 0.66f; u->p_mid = 0.62f; u->p_treble = 0.55f;
                u->sw_bright = 0; u->sw_mode = 0;
                break;
            case 8: /* Marshall JCM800 2203 */
                u->model_preset_id = 2;
                u->p_drive = 0.75f; u->p_bass = 0.54f; u->p_mid = 0.68f; u->p_treble = 0.62f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 9: /* Soldano SLO-100 Super Lead */
                u->model_preset_id = 3;
                u->p_drive = 0.84f; u->p_bass = 0.60f; u->p_mid = 0.68f; u->p_treble = 0.66f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 10: /* Peavey 5150 Block Letter */
                u->model_preset_id = 3;
                u->p_drive = 0.88f; u->p_bass = 0.64f; u->p_mid = 0.70f; u->p_treble = 0.65f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 11: /* Mesa Dual Rectifier Multi-Watt */
                u->model_preset_id = 3;
                u->p_drive = 0.86f; u->p_bass = 0.72f; u->p_mid = 0.52f; u->p_treble = 0.64f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 12: /* Friedman BE-100 Brown Eye */
                u->model_preset_id = 3;
                u->p_drive = 0.82f; u->p_bass = 0.56f; u->p_mid = 0.68f; u->p_treble = 0.64f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 13: /* Bogner Ecstasy 101B Red */
                u->model_preset_id = 3;
                u->p_drive = 0.80f; u->p_bass = 0.58f; u->p_mid = 0.72f; u->p_treble = 0.60f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 14: /* ENGL Powerball II Lead */
                u->model_preset_id = 3;
                u->p_drive = 0.86f; u->p_bass = 0.64f; u->p_mid = 0.66f; u->p_treble = 0.68f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 15: /* Diezel VH4 Channel 3 */
                u->model_preset_id = 3;
                u->p_drive = 0.85f; u->p_bass = 0.66f; u->p_mid = 0.68f; u->p_treble = 0.62f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 16: /* Ampeg SVT-CL Classic Stack */
                u->model_preset_id = 5;
                u->p_drive = 0.55f; u->p_bass = 0.78f; u->p_mid = 0.56f; u->p_treble = 0.44f;
                u->sw_bright = 0; u->sw_mode = 0;
                break;
            case 17: /* Darkglass Microtubes B7K */
                u->model_preset_id = 5;
                u->p_drive = 0.68f; u->p_bass = 0.72f; u->p_mid = 0.62f; u->p_treble = 0.58f;
                u->sw_bright = 1; u->sw_mode = 1;
                break;
            case 18: /* Acoustic Tube DI Preamp */
                u->model_preset_id = 6;
                u->p_drive = 0.12f; u->p_bass = 0.46f; u->p_mid = 0.48f; u->p_treble = 0.78f;
                u->sw_bright = 1; u->sw_mode = 0;
                break;
        }
    } else {
        /* Overdrive / Distortion / Fuzz Pedals (Class 1) */
        u->fx_class = 1;
        u->model_preset_id = 0;
        switch (profile_id) {
            case 19: /* Klon Centaur Gold Horsie */
                u->p_drive = 0.55f; u->p_tone = 0.60f; u->p_level = 0.72f; u->sw_bright = 1;
                break;
            case 20: /* Ibanez TS808 Tube Screamer */
                u->p_drive = 0.58f; u->p_tone = 0.52f; u->p_level = 0.70f; u->sw_bright = 0;
                break;
            case 21: /* ProCo Rat 2 Vintage LM308 */
                u->p_drive = 0.68f; u->p_tone = 0.48f; u->p_level = 0.68f; u->sw_bright = 0;
                break;
            case 22: /* Electro-Harmonix Big Muff Pi */
                u->p_drive = 0.82f; u->p_tone = 0.50f; u->p_level = 0.65f; u->sw_bright = 0;
                break;
            case 23: /* King of Tone High Gain Side */
                u->p_drive = 0.64f; u->p_tone = 0.58f; u->p_level = 0.70f; u->sw_bright = 1;
                break;
        }
    }
    update_slot_cab_filters(u, 48000.0f);
}

/* Context-aware parameter normalizer: handles dB ranges, 0..10 cyber knobs, frequencies, and 0..1 normalized values */
static inline float norm_slot_val(float v, const char *sym, const char *uri) {
    int is_cyber = (uri && strstr(uri, "cyber")) ? 1 : 0;
    const char *s = sym ? sym : "";

    /* 1. Explicit dB ranges for Noise Gate / Threshold */
    if (str_icontains(s, "gate") || str_icontains(s, "noise_gate")) {
        if (v <= -80.0f) return 0.0f;
        if (v >= 0.0f) return 1.0f;
        return (v + 80.0f) / 80.0f;
    }

    /* 2. dB gain / level ranges (-20 .. +20 dB e.g. NAM input_level/output_level) */
    if (str_icontains(s, "level") || str_icontains(s, "output") || str_icontains(s, "input") || str_icontains(s, "drive")) {
        if (v < 0.0f && v >= -35.0f) {
            float n = (v + 20.0f) / 40.0f;
            return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
        }
    }

    /* 3. Negative dB generic ranges */
    if (v < 0.0f) {
        if (v < -40.0f) {
            float n = (v + 80.0f) / 80.0f;
            return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
        } else {
            float n = (v + 24.0f) / 48.0f;
            return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
        }
    }

    /* 4. Cyber amp & pedal knobs: continuous 0..10 scale */
    if (is_cyber && v >= 0.0f && v <= 10.0f) {
        return v * 0.1f;
    }

    /* 5. Values already in 0.0 .. 1.0 normalized range */
    if (v >= 0.0f && v <= 1.0f) {
        return v;
    }

    /* 6. Non-cyber 0..10 knobs */
    if (v > 1.0f && v <= 10.0f) {
        return v * 0.1f;
    }

    /* 7. Percentage (0..100) range */
    if (v > 10.0f && v <= 100.0f) {
        return v * 0.01f;
    }

    /* 8. Frequency or millisecond ranges */
    if (v > 100.0f) {
        if (v <= 2000.0f) return fminf(1.0f, v / 1000.0f);
        return fminf(1.0f, log10f(v / 20.0f) / 3.0f);
    }

    return 0.5f;
}

static void apply_slot_param(SlotDspUnit *u, const char *sym, float val) {
    if (!u || !sym || !*sym) return;
    if (strcmp(sym, ":bypass") == 0) {
        /* Direct UI bypass toggle: 1.0 = bypassed, 0.0 = enabled */
        u->bypass = (val >= 0.5f) ? 1 : 0;
        if (u->bypass_port_idx >= 0 && u->bypass_port_idx < (int)u->lv2_num_ports) {
            u->lv2_port_vals[u->bypass_port_idx] = u->bypass ? 0.0f : 1.0f;
        }
        return;
    }
    if (str_icontains(sym, "bypass")) {
        /* LV2 bypass port (lv2:designation lv2:enabled): 1.0 = active/enabled, 0.0 = bypassed */
        u->bypass = (val < 0.5f) ? 1 : 0;
        if (u->bypass_port_idx >= 0 && u->bypass_port_idx < (int)u->lv2_num_ports) {
            u->lv2_port_vals[u->bypass_port_idx] = u->bypass ? 0.0f : 1.0f;
        }
        return;
    }
    if (strcmp(sym, "enabled") == 0 || strcmp(sym, "on") == 0 || strcmp(sym, "power") == 0) {
        u->bypass = (val < 0.5f) ? 1 : 0;
        if (u->bypass_port_idx >= 0 && u->bypass_port_idx < (int)u->lv2_num_ports) {
            u->lv2_port_vals[u->bypass_port_idx] = u->bypass ? 0.0f : 1.0f;
        }
        return;
    }

    /* Persist and apply exact symbol->value directly to native LV2 control port */
    record_saved_slot_param(u, sym, val);
    set_native_lv2_port_by_sym(u, sym, val);

    if (strcmp(sym, "profile_a") == 0 || strcmp(sym, "profile_b") == 0 || strcmp(sym, "profile") == 0 || strcmp(sym, "model") == 0) {
        apply_slot_nam_profile(u, (int)(val + 0.5f));
        return;
    }
    if (strcmp(sym, "active_slot") == 0 || str_icontains(sym, "denoise") || str_icontains(sym, "learn") || str_icontains(sym, "gate")) {
        return;
    }
    if (strcmp(sym, "cab_enable") == 0 || strcmp(sym, "cab") == 0) {
        u->sw_bright = (val >= 0.5f) ? 1 : 0;
        return;
    }
    if (str_icontains(sym, "bright") || str_icontains(sym, "fat") || str_icontains(sym, "boost") ||
        str_icontains(sym, "pab") || str_icontains(sym, "tight") || str_icontains(sym, "c45")) {
        u->sw_bright = (val >= 0.5f) ? 1 : 0;
        return;
    }
    if (str_icontains(sym, "voice") || str_icontains(sym, "clip") || str_icontains(sym, "mode") ||
        str_icontains(sym, "deep") || str_icontains(sym, "rock") || str_icontains(sym, "switch") ||
        str_icontains(sym, "sat") || str_icontains(sym, "channel") || str_icontains(sym, "hbe")) {
        u->sw_mode = (val >= 0.5f) ? (int)(val + 0.5f) : 0;
        return;
    }
    float nv = norm_slot_val(val, sym, u->uri);

    /* Master Output Level vs Drive/Preamp:
     * "volume", "master", "level", "output", "output_level", "out" -> u->p_level
     * "drive", "gain", "dist", "sustain", "input", "input_level", "high_treb_vol", "normal_vol", "preamp" -> u->p_drive
     */
    if (strcmp(sym, "volume") == 0 || str_icontains(sym, "master") || str_icontains(sym, "output") ||
        strcmp(sym, "level") == 0 || strcmp(sym, "out") == 0 || str_icontains(sym, "make")) {
        u->p_level = nv;
    } else if (!str_icontains(sym, "speaker") && (str_icontains(sym, "drive") || str_icontains(sym, "gain") || str_icontains(sym, "dist") ||
               str_icontains(sym, "sustain") || str_icontains(sym, "input") || str_icontains(sym, "preamp") ||
               str_icontains(sym, "thresh") || str_icontains(sym, "comp") || str_icontains(sym, "sens") ||
               str_icontains(sym, "vol"))) {
        u->p_drive = nv;
    } else if (str_icontains(sym, "tone") || str_icontains(sym, "filter") || str_icontains(sym, "cutoff") ||
               str_icontains(sym, "damp") || str_icontains(sym, "highcut") || str_icontains(sym, "color")) {
        u->p_tone = nv;
    } else if (str_icontains(sym, "bass") || str_icontains(sym, "low") || str_icontains(sym, "thump") ||
               str_icontains(sym, "sub") || str_icontains(sym, "bottom")) {
        u->p_bass = nv;
    } else if (str_icontains(sym, "mid") || str_icontains(sym, "middle") || str_icontains(sym, "body") ||
               str_icontains(sym, "contour") || str_icontains(sym, "focus") || str_icontains(sym, "q") || str_icontains(sym, "res")) {
        u->p_mid = nv;
    } else if (str_icontains(sym, "treble") || str_icontains(sym, "treb") || str_icontains(sym, "high") ||
               str_icontains(sym, "pres") || str_icontains(sym, "presence") || str_icontains(sym, "air") ||
               str_icontains(sym, "cut") || str_icontains(sym, "shimmer")) {
        u->p_treble = nv;
    } else if (str_icontains(sym, "mix") || str_icontains(sym, "wet") || str_icontains(sym, "blend") ||
               str_icontains(sym, "effect") || str_icontains(sym, "synth") || str_icontains(sym, "dwell")) {
        u->p_mix = nv;
    } else if (str_icontains(sym, "dry") || str_icontains(sym, "direct")) {
        u->p_mix = 1.0f - nv * 0.5f;
    } else if (str_icontains(sym, "time") || str_icontains(sym, "delay") || str_icontains(sym, "decay") ||
               str_icontains(sym, "size") || str_icontains(sym, "room") || str_icontains(sym, "length")) {
        u->p_time = nv;
    } else if (str_icontains(sym, "feed") || str_icontains(sym, "regen") || str_icontains(sym, "repeat") ||
               str_icontains(sym, "bloom")) {
        u->p_feedback = nv;
    } else if (str_icontains(sym, "rate") || str_icontains(sym, "speed") || str_icontains(sym, "freq") ||
               str_icontains(sym, "attack")) {
        u->p_rate = nv;
    } else if (str_icontains(sym, "depth") || str_icontains(sym, "amount") || str_icontains(sym, "intens") ||
               str_icontains(sym, "width") || str_icontains(sym, "mod") || str_icontains(sym, "oct") ||
               str_icontains(sym, "flutter") || str_icontains(sym, "wow") || str_icontains(sym, "release")) {
        u->p_depth = nv;
    }
}

/* Rule 25: Automatic Per-Module Unity Gain Staging & -0.5 dBFS (0.944f) Soft-Ceiling Guard
 * Ensures no DSP module (Preamp, Insert Slots, MOD-GUI, Denoise, EQ, Comp, De-Esser) can overload
 * the next module in the signal chain or cause sudden volume jumps when tone/drive is adjusted. */
static inline float soft_ceiling_guard(float x) {
    float ax = fabsf(x);
    if (ax <= 0.7079f) return x; /* 100% linear up to -3.0 dBFS */
    float sign = (x >= 0.0f) ? 1.0f : -1.0f;
    return sign * (0.7079f + 0.2361f * tanhf((ax - 0.7079f) / 0.2361f)); /* Asymptotic soft-ceiling at -0.5 dBFS (0.944f) */
}

/* Helper to detect whether an effect slot range hosts active overdrive pedals or guitar amps */
static inline int slot_range_has_drive_or_amp(MeterState *st, int ch, int start_slot, int end_slot) {
    for (int s = start_slot; s < end_slot && s < NUM_CH_SLOTS; s++) {
        SlotDspUnit *u = &st->ch_slots[ch][s];
        if (u->active && !u->bypass && (u->fx_class == 1 || u->fx_class == 2)) {
            return 1;
        }
    }
    return 0;
}

static inline int slot_range_has_native_cyber_amp(MeterState *st, int ch, int start_slot, int end_slot) {
    for (int s = start_slot; s < end_slot && s < NUM_CH_SLOTS; s++) {
        SlotDspUnit *u = &st->ch_slots[ch][s];
        if (u->active && !u->bypass && u->lv2_ready && u->fx_class == 2 && strstr(u->uri, "cyber")) {
            return 1;
        }
    }
    return 0;
}

static inline void apply_module_autogain(float in_l, float in_r, float *out_l, float *out_r, int is_stereo, float *env_in, float *env_out, float *gain_smooth, int allow_attenuation) {
    if (*gain_smooth <= 0.05f || *gain_smooth > 2.0f) *gain_smooth = 1.0f;
    if (allow_attenuation) {
        float p_in  = is_stereo ? 0.5f * (in_l * in_l + in_r * in_r) : (in_l * in_l);
        float p_out = is_stereo ? 0.5f * ((*out_l) * (*out_l) + (*out_r) * (*out_r)) : ((*out_l) * (*out_l));
        /* Rule 25 + Rule 27: Gated slow ~300 ms RMS integration (never tracks bass half-cycles or snaps in silence/tails) */
        if (p_in > 0.00004f && p_out > 0.00004f) {
            *env_in  = 0.99993f * (*env_in)  + 0.00007f * p_in;
            *env_out = 0.99993f * (*env_out) + 0.00007f * p_out;
            float target_g = 1.0f;
            if (*env_out > (*env_in) * 1.12f && *env_out > 0.0001f) {
                target_g = sqrtf((*env_in) / (*env_out));
                if (target_g < 0.35f) target_g = 0.35f; /* Up to -9 dB automatic overload trim */
                if (target_g > 1.00f) target_g = 1.00f; /* Never fight compressor/denoiser downward action */
            }
            *gain_smooth = 0.99985f * (*gain_smooth) + 0.00015f * target_g;
        }
        *out_l = soft_ceiling_guard((*out_l) * (*gain_smooth));
        if (is_stereo) {
            *out_r = soft_ceiling_guard((*out_r) * (*gain_smooth));
        }
    } else {
        /* Amp / Drive mode: 100% transparent pass-through up to -0.3 dBFS (0.966f) so native LV2 amp tone is never squashed! */
        *gain_smooth = 0.99985f * (*gain_smooth) + 0.00015f * 1.0f;
        if (fabsf(*out_l) > 0.966f) *out_l = (*out_l > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(*out_l) - 0.966f) / 0.033f));
        if (is_stereo && fabsf(*out_r) > 0.966f) *out_r = (*out_r > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(*out_r) - 0.966f) / 0.033f));
    }
}

/* Rule 29: Ported from authentic C++ sources in /home/patch/plugins/cyber-*.lv2/src/ */
static inline float triode_12ax7_stage(float in, float *grid_bias, float gain_factor, float asym) {
    if (in > 0.0f) {
        *grid_bias += (in - *grid_bias) * 0.006f;
    } else {
        *grid_bias *= 0.9997f;
    }
    float eff_in = (in - *grid_bias * 0.20f) * gain_factor;
    float t = tanhf(eff_in);
    return t - asym * (t * t);
}

/* Dave Friedman / Marshall 10k Cold Clipper Stage (V1B) from cyber_friedman_be100_lv2.cpp */
static inline float triode_cold_clipper(float in, float *grid_bias, float gain_factor) {
    if (in > 0.0f) {
        *grid_bias += (in - *grid_bias) * 0.002f;
    } else {
        *grid_bias *= 0.9998f;
    }
    float eff_in = (in - *grid_bias * 0.12f) * gain_factor;
    float out;
    if (eff_in < -0.45f) {
        out = -0.45f - 0.12f * tanhf((eff_in + 0.45f) * 1.5f);
    } else {
        out = tanhf(eff_in * 0.95f) * 1.15f;
    }
    return out;
}

static inline float cathode_follower_stage(float in, float *grid_bias) {
    if (in > 0.0f) {
        *grid_bias += (in - *grid_bias) * 0.009f;
    } else {
        *grid_bias *= 0.9994f;
    }
    float x = in - *grid_bias * 0.30f;
    float t = tanhf(x * 1.25f);
    return t - 0.10f * (t * t);
}

static inline float power_tube_stage(float in, float *sag_voltage) {
    if (*sag_voltage <= 0.2f || *sag_voltage > 1.2f) *sag_voltage = 1.0f;
    float rec = fabsf(in);
    if (rec > 0.4f) {
        *sag_voltage -= (rec - 0.4f) * 0.0025f;
        if (*sag_voltage < 0.68f) *sag_voltage = 0.68f;
    } else {
        *sag_voltage += (1.0f - *sag_voltage) * 0.0012f;
    }
    float eff_bplus = *sag_voltage;
    float x = in / eff_bplus;
    if (x < -0.70f) {
        return (-0.70f - 0.20f * tanhf((x + 0.70f) * 1.4f)) * eff_bplus;
    }
    float t = tanhf(x * 1.30f);
    return (t - 0.05f * (t * t)) * eff_bplus;
}

static inline float process_speaker_stress(float in, float drive_val, float *spk_env, float *spk_thermal, float *spk_cone) {
    if (drive_val <= 0.05f) return in;
    float s = in;
    float drive_amt = drive_val / 10.0f;
    float rect = fabsf(s);
    if (rect > *spk_env) *spk_env += 0.015f * (rect - *spk_env);
    else *spk_env += 0.0025f * (rect - *spk_env);
    *spk_thermal += 0.00025f * (*spk_env - *spk_thermal);

    float comp = 1.0f / (1.0f + *spk_env * drive_amt * 1.4f);
    float therm_comp = 1.0f / (1.0f + *spk_thermal * drive_amt * 0.32f);
    s = s * comp * therm_comp;

    float cone_stress = s * (1.0f + drive_amt * 1.35f);
    float t = tanhf(cone_stress);
    float cone_out = (t - 0.045f * (t * t)) / (1.0f + drive_amt * 0.28f);

    *spk_cone += 0.35f * (cone_out - *spk_cone);
    s = (1.0f - drive_amt * 0.85f) * s + (drive_amt * 0.85f) * (*spk_cone);
    return s * (1.0f + drive_amt * 0.28f);
}

/* Stage 3A: Execute a range of Channel LV2 Insert Effect Slots (0..2 = Insert Slots 1&2, 2..4 = MOD-GUI Pedalboard) */
static inline void process_channel_effect_slots_range(MeterState *st, int ch, int start_slot, int end_slot, float *s_l, float *s_r, int is_stereo, float fs) {
    for (int s = start_slot; s < end_slot && s < NUM_CH_SLOTS; s++) {
        SlotDspUnit *u = &st->ch_slots[ch][s];
        if (!u->active || u->bypass) continue;

        float xl = *s_l;
        float xr = is_stereo ? *s_r : xl;
        float out_l = xl, out_r = xr;
        float out_gain = 0.25f + u->p_level * 1.45f; /* 0.72 default -> ~1.3x */

        switch (u->fx_class) {
            case 1: /* Overdrive / Distortion / Klon / Screamer / Fuzz */
            {
                /* Tight input high-pass (~120Hz) */
                u->hp1_l += 0.016f * (xl - u->hp1_l);
                float tight_l = xl - u->hp1_l * (u->sw_bright ? 0.45f : 0.75f);
                /* Audio taper: 1.0x at zero drive (clean boost), up to 22x when cranked */
                float norm_pdrv = u->p_drive;
                if (norm_pdrv < 0.0f) norm_pdrv = 0.0f; else if (norm_pdrv > 1.0f) norm_pdrv = 1.0f;
                float drv = 1.0f + (0.35f * norm_pdrv + 0.65f * norm_pdrv * norm_pdrv) * 21.0f * (u->sw_bright ? 1.30f : 1.0f);
                float d_l = tight_l * drv;
                float clip_l = (u->sw_mode > 0)
                    ? tanhf(d_l * 1.15f) /* Hard/LED clipping */
                    : tanhf(d_l + 0.15f * d_l * fabsf(d_l)); /* Warm asymmetric germanium/silicon */
                /* Tone Low-Pass control (800 Hz .. 7.5 kHz) */
                float tone_alpha = 0.08f + u->p_tone * 0.65f + (u->sw_bright ? 0.15f : 0.0f);
                if (tone_alpha > 0.92f) tone_alpha = 0.92f;
                u->lp1_l += tone_alpha * (clip_l - u->lp1_l);
                /* Klon-style clean blend + calibrated output level (unity at p_level=0.72) */
                float clean_blend = (1.0f - norm_pdrv) * 0.35f;
                float ped_out_scale = 0.35f + u->p_level * 0.85f;
                out_l = (u->lp1_l * (1.0f - clean_blend) + xl * clean_blend) * ped_out_scale;

                if (is_stereo) {
                    u->hp1_r += 0.016f * (xr - u->hp1_r);
                    float tight_r = xr - u->hp1_r * (u->sw_bright ? 0.45f : 0.75f);
                    float d_r = tight_r * drv;
                    float clip_r = (u->sw_mode > 0) ? tanhf(d_r * 1.15f) : tanhf(d_r + 0.15f * d_r * fabsf(d_r));
                    u->lp1_r += tone_alpha * (clip_r - u->lp1_r);
                    out_r = (u->lp1_r * (1.0f - clean_blend) + xr * clean_blend) * ped_out_scale;
                } else {
                    out_r = out_l;
                }
                break;
            }
            case 2: /* Tube Guitar/Bass Amp & Cab / NAM Neural Amp */
            {
                if (u->model_preset_id == 6) {
                    /* Pristine Acoustic Guitar / Body Air Shimmer: Linear transparent response, ~350Hz warmth, ~10kHz air */
                    u->lp1_l += 0.045f * (xl - u->lp1_l); /* ~350Hz body */
                    u->lp2_l += 0.42f * (xl - u->lp2_l);  /* ~8kHz clarity */
                    float ac_body = u->lp1_l * (0.50f + u->p_bass * 0.70f);
                    float ac_clarity = (u->lp2_l - u->lp1_l) * (0.65f + u->p_mid * 0.55f);
                    float ac_air = (xl - u->lp2_l) * (0.60f + u->p_treble * 0.90f);
                    float ac_out_scale = 0.45f + u->p_level * 0.65f; /* Unity at p_level=0.72 */
                    out_l = (ac_body + ac_clarity + ac_air) * ac_out_scale;
                    if (is_stereo) {
                        u->lp1_r += 0.045f * (xr - u->lp1_r);
                        u->lp2_r += 0.42f * (xr - u->lp2_r);
                        out_r = (u->lp1_r * (0.50f + u->p_bass * 0.70f) +
                                 (u->lp2_r - u->lp1_r) * (0.65f + u->p_mid * 0.55f) +
                                 (xr - u->lp2_r) * (0.60f + u->p_treble * 0.90f)) * ac_out_scale;
                    } else {
                        out_r = out_l;
                    }
                    break;
                }

                /* Preamp Drive Audio Taper: Logarithmic response matching cyber-*.lv2 C++ sources
                 * Low knob (0..2): Clean headroom, sparkle, zero clipping
                 * Mid knob (3..6): Dynamic touch sensitivity, singing edge-of-breakup
                 * High knob (7..10): Authentic tube saturation, harmonic bloom, classic crunch / lead */
                float norm_drv = u->p_drive;
                if (norm_drv < 0.0f) norm_drv = 0.0f; else if (norm_drv > 1.0f) norm_drv = 1.0f;
                float drv_taper = 0.42f * norm_drv + 0.58f * norm_drv * norm_drv;

                /* Input DC Blocker / High-Pass (~22 Hz) */
                u->hp1_l += 0.003f * (xl - u->hp1_l);
                float in_clean_l = xl - u->hp1_l;

                float tonestack_l = 0.0f;
                float pwr_drive = 1.4f;

                if (u->model_preset_id == 3) {
                    /* Friedman BE-100 High-Gain Lead Monster (ported from cyber_friedman_be100_lv2.cpp) */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.85f, 0.08f);
                    float be_gain = 0.50f + drv_taper * 5.5f;
                    if (u->sw_mode > 0) be_gain *= 1.65f; /* HBE lead boost */
                    float v2_l = triode_cold_clipper(v1_l * be_gain * 0.65f, &u->grid_bias2_l, 2.2f);
                    float v3_l = triode_12ax7_stage(v2_l * (0.85f + drv_taper * 0.65f), &u->grid_bias_cf_l, 2.2f, 0.12f);
                    /* Marshall interactive tone stack with 650Hz scoop */
                    u->lp1_l += 0.024f * (v3_l - u->lp1_l);
                    u->lp2_l += (0.28f + u->p_treble * 0.35f) * (v3_l - u->lp2_l);
                    tonestack_l = u->lp1_l * (0.35f + u->p_bass * 1.15f) + (u->lp2_l - u->lp1_l) * (0.35f + u->p_mid * 1.05f) + (v3_l - u->lp2_l) * (0.45f + u->p_treble * 1.25f);
                    pwr_drive = 1.2f + drv_taper * 0.9f;
                } else if (u->model_preset_id == 2) {
                    /* Marshall JTM45 Plexi (ported from cyber_marshall_jtm45_lv2.cpp) */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.50f, 0.08f);
                    float high_gain = 0.35f + drv_taper * 4.2f * (u->sw_bright ? 1.30f : 1.0f);
                    float cf_l = cathode_follower_stage(v1_l * high_gain * 0.75f, &u->grid_bias_cf_l);
                    u->lp1_l += 0.028f * (cf_l - u->lp1_l);
                    u->lp2_l += (0.30f + u->p_treble * 0.32f + (u->sw_bright ? 0.10f : 0.0f)) * (cf_l - u->lp2_l);
                    tonestack_l = u->lp1_l * (0.38f + u->p_bass * 1.20f) + (u->lp2_l - u->lp1_l) * (0.45f + u->p_mid * 1.15f) + (cf_l - u->lp2_l) * (0.40f + u->p_treble * 1.15f);
                    pwr_drive = 1.4f + drv_taper * 1.1f;
                } else if (u->model_preset_id == 5) {
                    /* Fender '59 Bassman Tweed (ported from cyber_fender_bassman59_lv2.cpp) */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.35f, 0.06f);
                    float vol_gain = 0.35f + drv_taper * 3.8f * (u->sw_bright ? 1.25f : 1.0f);
                    float cf_l = cathode_follower_stage(v1_l * vol_gain * 0.75f, &u->grid_bias_cf_l);
                    u->lp1_l += 0.020f * (cf_l - u->lp1_l);
                    u->lp2_l += (0.28f + u->p_treble * 0.32f) * (cf_l - u->lp2_l);
                    tonestack_l = u->lp1_l * (0.45f + u->p_bass * 1.35f) + (u->lp2_l - u->lp1_l) * (0.45f + u->p_mid * 1.10f) + (cf_l - u->lp2_l) * (0.38f + u->p_treble * 1.10f);
                    pwr_drive = 1.3f + drv_taper * 1.0f;
                } else if (u->model_preset_id == 7) {
                    /* Vox AC30 Top Boost (ported from cyber_vox_ac30tb_lv2.cpp) */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.55f, 0.07f);
                    u->lp1_l += 0.032f * (v1_l - u->lp1_l);
                    u->lp2_l += (0.34f + u->p_treble * 0.38f) * (v1_l - u->lp2_l);
                    float tone_l = u->lp1_l * (0.35f + u->p_bass * 1.15f) + (u->lp2_l - u->lp1_l) * (0.48f + u->p_mid * 1.10f) + (v1_l - u->lp2_l) * (0.45f + u->p_treble * 1.30f);
                    float tb_gain = 0.35f + drv_taper * 4.0f;
                    float v2_l = triode_12ax7_stage(tone_l * tb_gain * 0.75f, &u->grid_bias2_l, 1.45f, 0.08f);
                    tonestack_l = v2_l;
                    pwr_drive = 1.4f + drv_taper * 1.0f;
                } else if (u->model_preset_id == 8) {
                    /* Orange OR120 (ported from cyber_orange_or120_lv2.cpp) */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.55f, 0.08f);
                    u->lp1_l += 0.022f * (v1_l - u->lp1_l);
                    u->lp2_l += (0.30f + u->p_treble * 0.36f) * (v1_l - u->lp2_l);
                    float tone_l = u->lp1_l * (0.42f + u->p_bass * 1.35f) + (u->lp2_l - u->lp1_l) * (0.42f + u->p_mid * 1.15f) + (v1_l - u->lp2_l) * (0.38f + u->p_treble * 1.25f);
                    float pre_gain = 0.40f + drv_taper * 4.8f;
                    float v2_l = triode_12ax7_stage(tone_l * pre_gain * 0.75f, &u->grid_bias2_l, 1.60f, 0.10f);
                    tonestack_l = v2_l;
                    pwr_drive = 1.5f + drv_taper * 1.1f;
                } else if (u->model_preset_id == 9) {
                    /* Dumble ODS Overdrive Special (ported from cyber_dumble_ods_lv2.cpp) */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.50f, 0.06f);
                    u->lp1_l += 0.026f * (v1_l - u->lp1_l);
                    u->lp2_l += (0.32f + u->p_treble * 0.34f) * (v1_l - u->lp2_l);
                    float tone_l = u->lp1_l * (0.38f + u->p_bass * 1.15f) + (u->lp2_l - u->lp1_l) * (0.48f + u->p_mid * 1.20f) + (v1_l - u->lp2_l) * (0.40f + u->p_treble * 1.15f);
                    float od_gain = 0.40f + drv_taper * 4.4f;
                    float v2_l = triode_12ax7_stage(tone_l * od_gain * 0.70f, &u->grid_bias2_l, 1.75f, 0.10f);
                    float cf_l = cathode_follower_stage(v2_l * 1.05f, &u->grid_bias_cf_l);
                    tonestack_l = cf_l;
                    pwr_drive = 1.3f + drv_taper * 1.0f;
                } else if (u->model_preset_id == 1) {
                    /* Fender Clean Twin / Blackface: High Headroom Pristine Chime */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.35f, 0.04f);
                    float clean_gain = 0.40f + drv_taper * 2.8f * (u->sw_bright ? 1.20f : 1.0f);
                    float cf_l = cathode_follower_stage(v1_l * clean_gain * 0.70f, &u->grid_bias_cf_l);
                    u->lp1_l += 0.028f * (cf_l - u->lp1_l);
                    u->lp2_l += (0.32f + u->p_treble * 0.36f + (u->sw_bright ? 0.12f : 0.0f)) * (cf_l - u->lp2_l);
                    tonestack_l = u->lp1_l * (0.40f + u->p_bass * 1.20f) + (u->lp2_l - u->lp1_l) * (0.40f + u->p_mid * 1.00f) + (cf_l - u->lp2_l) * (0.48f + u->p_treble * 1.25f);
                    pwr_drive = 1.2f + drv_taper * 0.6f;
                } else {
                    /* Default Vintage Guitar Amp */
                    float v1_l = triode_12ax7_stage(in_clean_l, &u->grid_bias1_l, 1.45f, 0.07f);
                    float vol_gain = 0.35f + drv_taper * 3.8f;
                    float cf_l = cathode_follower_stage(v1_l * vol_gain * 0.75f, &u->grid_bias_cf_l);
                    u->lp1_l += 0.028f * (cf_l - u->lp1_l);
                    u->lp2_l += (0.32f + u->p_treble * 0.35f) * (cf_l - u->lp2_l);
                    tonestack_l = u->lp1_l * (0.38f + u->p_bass * 1.25f) + (u->lp2_l - u->lp1_l) * (0.45f + u->p_mid * 1.15f) + (cf_l - u->lp2_l) * (0.40f + u->p_treble * 1.15f);
                    pwr_drive = 1.3f + drv_taper * 1.0f;
                }

                /* Power Tube Push-Pull Stage with Dynamic Sag */
                float push_l = tonestack_l * pwr_drive;
                float pull_l = -tonestack_l * pwr_drive * 0.985f; /* 1.5% phase inverter imbalance for even harmonic bloom */
                float pwr_l = (power_tube_stage(push_l, &u->sag_voltage_l) - power_tube_stage(pull_l, &u->sag_voltage_l)) * 0.5f;

                /* Dynamic Speaker Stress & Cone Compression */
                float spk_l = process_speaker_stress(pwr_l, norm_drv * 6.0f, &u->speaker_env_l, &u->speaker_thermal_l, &u->speaker_cone_l);

                /* Authentic 4-Pole Guitar Speaker Cabinet Transfer Function */
                float cab_l = spk_l;
                cab_l = process_biquad_transposed(cab_l, u->cab_ls.b0, u->cab_ls.b1, u->cab_ls.b2, u->cab_ls.a1, u->cab_ls.a2, &u->cab_ls_s1_l, &u->cab_ls_s2_l);
                cab_l = process_biquad_transposed(cab_l, u->cab_mid.b0, u->cab_mid.b1, u->cab_mid.b2, u->cab_mid.a1, u->cab_mid.a2, &u->cab_mid_s1_l, &u->cab_mid_s2_l);
                cab_l = process_biquad_transposed(cab_l, u->cab_lp1.b0, u->cab_lp1.b1, u->cab_lp1.b2, u->cab_lp1.a1, u->cab_lp1.a2, &u->cab_lp1_s1_l, &u->cab_lp1_s2_l);
                cab_l = process_biquad_transposed(cab_l, u->cab_lp2.b0, u->cab_lp2.b1, u->cab_lp2.b2, u->cab_lp2.a1, u->cab_lp2.a2, &u->cab_lp2_s1_l, &u->cab_lp2_s2_l);

                /* Master Output Level: calibrated so nominal speaker output sits at -6 dBFS with p_level=0.72 */
                float amp_trim = 0.48f;
                float out_vol = (0.25f + u->p_level * 0.95f);
                out_l = cab_l * (amp_trim * out_vol);

                if (is_stereo) {
                    u->hp1_r += 0.003f * (xr - u->hp1_r);
                    float in_clean_r = xr - u->hp1_r;
                    float tonestack_r = 0.0f;
                    float pwr_drive_r = 1.4f;

                    if (u->model_preset_id == 3) {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.85f, 0.08f);
                        float be_gain = 0.50f + drv_taper * 5.5f;
                        if (u->sw_mode > 0) be_gain *= 1.65f;
                        float v2_r = triode_cold_clipper(v1_r * be_gain * 0.65f, &u->grid_bias2_r, 2.2f);
                        float v3_r = triode_12ax7_stage(v2_r * (0.85f + drv_taper * 0.65f), &u->grid_bias_cf_r, 2.2f, 0.12f);
                        u->lp1_r += 0.024f * (v3_r - u->lp1_r);
                        u->lp2_r += (0.28f + u->p_treble * 0.35f) * (v3_r - u->lp2_r);
                        tonestack_r = u->lp1_r * (0.35f + u->p_bass * 1.15f) + (u->lp2_r - u->lp1_r) * (0.35f + u->p_mid * 1.05f) + (v3_r - u->lp2_r) * (0.45f + u->p_treble * 1.25f);
                        pwr_drive_r = 1.2f + drv_taper * 0.9f;
                    } else if (u->model_preset_id == 2) {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.50f, 0.08f);
                        float high_gain = 0.35f + drv_taper * 4.2f * (u->sw_bright ? 1.30f : 1.0f);
                        float cf_r = cathode_follower_stage(v1_r * high_gain * 0.75f, &u->grid_bias_cf_r);
                        u->lp1_r += 0.028f * (cf_r - u->lp1_r);
                        u->lp2_r += (0.30f + u->p_treble * 0.32f + (u->sw_bright ? 0.10f : 0.0f)) * (cf_r - u->lp2_r);
                        tonestack_r = u->lp1_r * (0.38f + u->p_bass * 1.20f) + (u->lp2_r - u->lp1_r) * (0.45f + u->p_mid * 1.15f) + (cf_r - u->lp2_r) * (0.40f + u->p_treble * 1.15f);
                        pwr_drive_r = 1.4f + drv_taper * 1.1f;
                    } else if (u->model_preset_id == 5) {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.35f, 0.06f);
                        float vol_gain = 0.35f + drv_taper * 3.8f * (u->sw_bright ? 1.25f : 1.0f);
                        float cf_r = cathode_follower_stage(v1_r * vol_gain * 0.75f, &u->grid_bias_cf_r);
                        u->lp1_r += 0.020f * (cf_r - u->lp1_r);
                        u->lp2_r += (0.28f + u->p_treble * 0.32f) * (cf_r - u->lp2_r);
                        tonestack_r = u->lp1_r * (0.45f + u->p_bass * 1.35f) + (u->lp2_r - u->lp1_r) * (0.45f + u->p_mid * 1.10f) + (cf_r - u->lp2_r) * (0.38f + u->p_treble * 1.10f);
                        pwr_drive_r = 1.3f + drv_taper * 1.0f;
                    } else if (u->model_preset_id == 7) {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.55f, 0.07f);
                        u->lp1_r += 0.032f * (v1_r - u->lp1_r);
                        u->lp2_r += (0.34f + u->p_treble * 0.38f) * (v1_r - u->lp2_r);
                        float tone_r = u->lp1_r * (0.35f + u->p_bass * 1.15f) + (u->lp2_r - u->lp1_r) * (0.48f + u->p_mid * 1.10f) + (v1_r - u->lp2_r) * (0.45f + u->p_treble * 1.30f);
                        float tb_gain = 0.35f + drv_taper * 4.0f;
                        float v2_r = triode_12ax7_stage(tone_r * tb_gain * 0.75f, &u->grid_bias2_r, 1.45f, 0.08f);
                        tonestack_r = v2_r;
                        pwr_drive_r = 1.4f + drv_taper * 1.0f;
                    } else if (u->model_preset_id == 8) {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.55f, 0.08f);
                        u->lp1_r += 0.022f * (v1_r - u->lp1_r);
                        u->lp2_r += (0.30f + u->p_treble * 0.36f) * (v1_r - u->lp2_r);
                        float tone_r = u->lp1_r * (0.42f + u->p_bass * 1.35f) + (u->lp2_r - u->lp1_r) * (0.42f + u->p_mid * 1.15f) + (v1_r - u->lp2_r) * (0.38f + u->p_treble * 1.25f);
                        float pre_gain = 0.40f + drv_taper * 4.8f;
                        float v2_r = triode_12ax7_stage(tone_r * pre_gain * 0.75f, &u->grid_bias2_r, 1.60f, 0.10f);
                        tonestack_r = v2_r;
                        pwr_drive_r = 1.5f + drv_taper * 1.1f;
                    } else if (u->model_preset_id == 9) {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.50f, 0.06f);
                        u->lp1_r += 0.026f * (v1_r - u->lp1_r);
                        u->lp2_r += (0.32f + u->p_treble * 0.34f) * (v1_r - u->lp2_r);
                        float tone_r = u->lp1_r * (0.38f + u->p_bass * 1.15f) + (u->lp2_r - u->lp1_r) * (0.48f + u->p_mid * 1.20f) + (v1_r - u->lp2_r) * (0.40f + u->p_treble * 1.15f);
                        float od_gain = 0.40f + drv_taper * 4.4f;
                        float v2_r = triode_12ax7_stage(tone_r * od_gain * 0.70f, &u->grid_bias2_r, 1.75f, 0.10f);
                        float cf_r = cathode_follower_stage(v2_r * 1.05f, &u->grid_bias_cf_r);
                        tonestack_r = cf_r;
                        pwr_drive_r = 1.3f + drv_taper * 1.0f;
                    } else if (u->model_preset_id == 1) {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.35f, 0.04f);
                        float clean_gain = 0.40f + drv_taper * 2.8f * (u->sw_bright ? 1.20f : 1.0f);
                        float cf_r = cathode_follower_stage(v1_r * clean_gain * 0.70f, &u->grid_bias_cf_r);
                        u->lp1_r += 0.028f * (cf_r - u->lp1_r);
                        u->lp2_r += (0.32f + u->p_treble * 0.36f + (u->sw_bright ? 0.12f : 0.0f)) * (cf_r - u->lp2_r);
                        tonestack_r = u->lp1_r * (0.40f + u->p_bass * 1.20f) + (u->lp2_r - u->lp1_r) * (0.40f + u->p_mid * 1.00f) + (cf_r - u->lp2_r) * (0.48f + u->p_treble * 1.25f);
                        pwr_drive_r = 1.2f + drv_taper * 0.6f;
                    } else {
                        float v1_r = triode_12ax7_stage(in_clean_r, &u->grid_bias1_r, 1.45f, 0.07f);
                        float vol_gain = 0.35f + drv_taper * 3.8f;
                        float cf_r = cathode_follower_stage(v1_r * vol_gain * 0.75f, &u->grid_bias_cf_r);
                        u->lp1_r += 0.028f * (cf_r - u->lp1_r);
                        u->lp2_r += (0.32f + u->p_treble * 0.35f) * (cf_r - u->lp2_r);
                        tonestack_r = u->lp1_r * (0.38f + u->p_bass * 1.25f) + (u->lp2_r - u->lp1_r) * (0.45f + u->p_mid * 1.15f) + (cf_r - u->lp2_r) * (0.40f + u->p_treble * 1.15f);
                        pwr_drive_r = 1.3f + drv_taper * 1.0f;
                    }

                    float push_r = tonestack_r * pwr_drive_r;
                    float pull_r = -tonestack_r * pwr_drive_r * 0.985f;
                    float pwr_r = (power_tube_stage(push_r, &u->sag_voltage_r) - power_tube_stage(pull_r, &u->sag_voltage_r)) * 0.5f;
                    float spk_r = process_speaker_stress(pwr_r, norm_drv * 6.0f, &u->speaker_env_r, &u->speaker_thermal_r, &u->speaker_cone_r);
                    float cab_r = spk_r;
                    cab_r = process_biquad_transposed(cab_r, u->cab_ls.b0, u->cab_ls.b1, u->cab_ls.b2, u->cab_ls.a1, u->cab_ls.a2, &u->cab_ls_s1_r, &u->cab_ls_s2_r);
                    cab_r = process_biquad_transposed(cab_r, u->cab_mid.b0, u->cab_mid.b1, u->cab_mid.b2, u->cab_mid.a1, u->cab_mid.a2, &u->cab_mid_s1_r, &u->cab_mid_s2_r);
                    cab_r = process_biquad_transposed(cab_r, u->cab_lp1.b0, u->cab_lp1.b1, u->cab_lp1.b2, u->cab_lp1.a1, u->cab_lp1.a2, &u->cab_lp1_s1_r, &u->cab_lp1_s2_r);
                    cab_r = process_biquad_transposed(cab_r, u->cab_lp2.b0, u->cab_lp2.b1, u->cab_lp2.b2, u->cab_lp2.a1, u->cab_lp2.a2, &u->cab_lp2_s1_r, &u->cab_lp2_s2_r);

                    out_r = cab_r * (amp_trim * out_vol);
                } else {
                    out_r = out_l;
                }
                break;
            }
            case 3: /* Delay / Tape Echo / Cloud Bloom */
            {
                float target_dly = (0.04f + u->p_time * 0.44f) * fs;
                if (u->hp1_l < 48.0f) u->hp1_l = target_dly;
                u->hp1_l += 0.0015f * (target_dly - u->hp1_l);
                /* Subtle tape wow/flutter modulation with Hermite fractional interpolation (Rule 27) */
                u->lfo_phase += (0.5f + u->p_rate * 3.5f) / fs;
                if (u->lfo_phase >= 1.0f) u->lfo_phase -= 1.0f;
                float wow = sinf(u->lfo_phase * 6.2831853f) * (u->p_depth * 36.0f);
                float r_offset = u->hp1_l + wow;
                if (r_offset < 64.0f) r_offset = 64.0f;
                if (r_offset >= (float)(SLOT_DLY_MAX - 4)) r_offset = (float)(SLOT_DLY_MAX - 4);

                float tap_l = read_frac_hermite(u->dly_l, SLOT_DLY_MAX, u->dly_w, r_offset);
                float tap_r = read_frac_hermite(u->dly_r, SLOT_DLY_MAX, u->dly_w, r_offset);
                float damp_a = 0.12f + u->p_tone * 0.65f;
                u->lp1_l += damp_a * (tap_l - u->lp1_l);
                u->lp1_r += damp_a * (tap_r - u->lp1_r);
                float fb = fminf(0.88f, u->p_feedback * 0.92f);
                u->dly_l[u->dly_w] = tanhf(xl + u->lp1_l * fb);
                u->dly_r[u->dly_w] = tanhf(xr + u->lp1_r * fb);
                if (++u->dly_w >= SLOT_DLY_MAX) u->dly_w = 0;

                float mix = u->p_mix;
                out_l = (xl * (1.0f - mix * 0.5f) + u->lp1_l * mix) * out_gain;
                out_r = (xr * (1.0f - mix * 0.5f) + u->lp1_r * mix) * out_gain;
                break;
            }
            case 4: /* Reverb / Spring / DuskVerb / Hall / Shimmer */
            {
                /* Multi-tap recirculating tank reverb using slot circular buffer */
                int w = u->dly_w;
                int t1 = w - 1493; if (t1 < 0) t1 += SLOT_DLY_MAX;
                int t2 = w - 3467; if (t2 < 0) t2 += SLOT_DLY_MAX;
                int t3 = w - 6829; if (t3 < 0) t3 += SLOT_DLY_MAX;
                int t4 = w - 11777; if (t4 < 0) t4 += SLOT_DLY_MAX;
                float tank_l = 0.35f * u->dly_l[t1] - 0.28f * u->dly_l[t2] + 0.24f * u->dly_l[t3] + 0.20f * u->dly_l[t4];
                float tank_r = 0.35f * u->dly_r[t2] + 0.28f * u->dly_r[t1] - 0.24f * u->dly_r[t4] + 0.20f * u->dly_r[t3];
                float damp = 0.15f + u->p_tone * 0.65f + (u->sw_bright ? 0.15f : 0.0f);
                u->lp1_l += damp * (tank_l - u->lp1_l);
                u->lp1_r += damp * (tank_r - u->lp1_r);
                float decay = 0.45f + fminf(0.47f, u->p_time * 0.48f);
                u->dly_l[w] = xl * 0.5f + u->lp1_l * decay;
                u->dly_r[w] = xr * 0.5f + u->lp1_r * decay;
                if (++u->dly_w >= SLOT_DLY_MAX) u->dly_w = 0;

                float mix = u->p_mix;
                out_l = (xl * (1.0f - mix * 0.4f) + u->lp1_l * mix * 1.35f) * out_gain;
                out_r = (xr * (1.0f - mix * 0.4f) + u->lp1_r * mix * 1.35f) * out_gain;
                break;
            }
            case 5: /* Modulation: Chorus / Flanger / Phaser / Tremolo */
            {
                u->lfo_phase += (0.25f + u->p_rate * 6.5f) / fs;
                if (u->lfo_phase >= 1.0f) u->lfo_phase -= 1.0f;
                float lfo = 0.5f + 0.5f * sinf(u->lfo_phase * 6.2831853f);
                int w = u->dly_w;
                float mod_dly = 48.0f + lfo * (80.0f + u->p_depth * 620.0f);
                float mod_l = read_frac_hermite(u->dly_l, SLOT_DLY_MAX, w, mod_dly);
                float mod_r = read_frac_hermite(u->dly_r, SLOT_DLY_MAX, w, mod_dly);
                u->dly_l[w] = xl + mod_l * (u->p_feedback * 0.45f);
                u->dly_r[w] = xr + mod_r * (u->p_feedback * 0.45f);
                if (++u->dly_w >= SLOT_DLY_MAX) u->dly_w = 0;

                /* Also apply gentle amplitude tremolo if URI is tremolo */
                float trem = str_icontains(u->uri, "trem") ? (1.0f - u->p_depth * 0.65f * lfo) : 1.0f;
                float mix = u->p_mix;
                out_l = ((xl * (1.0f - mix * 0.5f) + mod_l * mix * 0.75f) * trem) * out_gain;
                out_r = ((xr * (1.0f - mix * 0.5f) + mod_r * mix * 0.75f) * trem) * out_gain;
                break;
            }
            case 6: /* Polyphonic / Octave / Synth / Bass9 / Mel9 / Organ9 / String9 */
            {
                /* Zero-crossing sub-octave flip-flop + full-wave octave-up + resonant synth envelope filter */
                if ((xl >= 0.0f && u->sub_prev_l < 0.0f)) {
                    u->sub_sign_l = (u->sub_sign_l >= 0.0f) ? -1.0f : 1.0f;
                }
                u->sub_prev_l = xl;
                float sub_oct = xl * u->sub_sign_l * 1.35f;
                float oct_up = (fabsf(xl) - 0.15f * fabsf(u->lp1_l)) * 1.8f;
                float abs_in = fabsf(xl);
                u->env_l = (abs_in > u->env_l) ? (0.98f * u->env_l + 0.02f * abs_in) : (0.9992f * u->env_l + 0.0008f * abs_in);
                float synth_raw = sub_oct * (0.4f + u->p_bass * 0.8f) + oct_up * (0.3f + u->p_treble * 0.8f + u->p_depth * 0.5f);
                float cutoff = fminf(0.85f, 0.04f + u->p_tone * 0.45f + u->env_l * 1.4f);
                u->lp1_l += cutoff * (synth_raw - u->lp1_l);
                u->lp2_l += cutoff * (u->lp1_l - u->lp2_l);
                float mix = u->p_mix;
                out_l = (xl * (1.0f - mix * 0.6f) + u->lp2_l * mix * 1.4f) * out_gain;
                out_r = is_stereo ? ((xr * (1.0f - mix * 0.6f) + u->lp2_l * mix * 1.4f) * out_gain) : out_l;
                break;
            }
            case 7: /* Pedal / Studio Compressor & Sustainer */
            {
                float abs_in = fmaxf(fabsf(xl), fabsf(xr));
                u->env_l = (abs_in > u->env_l) ? (0.96f * u->env_l + 0.04f * abs_in) : (0.9990f * u->env_l + 0.0010f * abs_in);
                float thresh = 0.04f + (1.0f - u->p_drive) * 0.35f;
                float comp_g = 1.0f;
                if (u->env_l > thresh && u->env_l > 0.0001f) {
                    comp_g = powf(thresh / u->env_l, 0.65f);
                }
                float makeup = 1.0f + u->p_drive * 1.6f;
                float wet_l = xl * comp_g * makeup;
                float wet_r = xr * comp_g * makeup;
                out_l = (xl * (1.0f - u->p_mix) + wet_l * u->p_mix) * out_gain;
                out_r = (xr * (1.0f - u->p_mix) + wet_r * u->p_mix) * out_gain;
                break;
            }
            case 8: /* EQ / Filter / Thump-Killer / Hum-Killer / Sculpt */
            default:
            {
                /* High-pass thump filter + 3-band tone sculpt */
                float hp_a = 0.005f + (1.0f - u->p_bass) * 0.045f;
                u->hp1_l += hp_a * (xl - u->hp1_l);
                float hpf_l = xl - u->hp1_l;
                u->lp1_l += (0.12f + u->p_tone * 0.55f) * (hpf_l - u->lp1_l);
                float hi_l = hpf_l - u->lp1_l;
                out_l = (u->lp1_l * (0.5f + u->p_bass * 1.1f) + hi_l * (0.5f + u->p_treble * 1.2f + (u->sw_bright ? 0.5f : 0.0f))) * out_gain;
                if (is_stereo) {
                    u->hp1_r += hp_a * (xr - u->hp1_r);
                    float hpf_r = xr - u->hp1_r;
                    u->lp1_r += (0.12f + u->p_tone * 0.55f) * (hpf_r - u->lp1_r);
                    float hi_r = hpf_r - u->lp1_r;
                    out_r = (u->lp1_r * (0.5f + u->p_bass * 1.1f) + hi_r * (0.5f + u->p_treble * 1.2f + (u->sw_bright ? 0.5f : 0.0f))) * out_gain;
                } else {
                    out_r = out_l;
                }
                break;
            }
        }

        /* Transparent headroom protection keeping peak <= 0.966f (-0.3 dBFS) */
        if (fabsf(out_l) > 0.966f) out_l = (out_l > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(out_l) - 0.966f) / 0.033f));
        if (fabsf(out_r) > 0.966f) out_r = (out_r > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(out_r) - 0.966f) / 0.033f));
        *s_l = out_l;
        if (is_stereo) *s_r = out_r;
    }
}

/* Block-wise LV2 Slot Processor:
 * Executes compiled native LV2 .so plugins directly via lv2_desc->run(handle, nframes) for 100% authentic MODEP parity,
 * falling back to process_channel_effect_slots_range only when a .so binary is unavailable. */
static inline void process_channel_effect_slots_block(MeterState *st, int ch, int start_slot, int end_slot, float *buf_l, float *buf_r, int is_stereo, uint32_t nframes, float fs) {
    if (nframes == 0 || nframes > LV2_BLOCK_MAX) return;
    for (int s = start_slot; s < end_slot && s < NUM_CH_SLOTS; s++) {
        SlotDspUnit *u = &st->ch_slots[ch][s];
        if (!u->active || u->bypass) continue;
        if (u->lv2_ready && u->lv2_desc && u->lv2_desc->run && u->lv2_handle_l) {
            if (u->bypass_port_idx >= 0 && u->bypass_port_idx < (int)u->lv2_num_ports) {
                u->lv2_port_vals[u->bypass_port_idx] = u->bypass ? 0.0f : 1.0f;
            }
            u->atom_in_buf.size = 8;
            u->atom_in_buf.type = 1;
            u->atom_in_buf.unit = 0;
            u->atom_in_buf.pad  = 0;
            u->atom_out_buf.size = sizeof(u->atom_out_buf.body);
            u->atom_out_buf.type = 0;
            u->atom_out_buf.unit = 0;
            u->atom_out_buf.pad  = 0;

            memcpy(u->buf_in_l, buf_l, nframes * sizeof(float));
            if (is_stereo && buf_r) {
                memcpy(u->buf_in_r, buf_r, nframes * sizeof(float));
            } else {
                memcpy(u->buf_in_r, buf_l, nframes * sizeof(float));
            }

            u->lv2_desc->run(u->lv2_handle_l, nframes);
            if (is_stereo && u->num_audio_in == 1 && u->num_audio_out == 1 && u->lv2_handle_r) {
                u->lv2_desc->run(u->lv2_handle_r, nframes);
            }

            for (uint32_t i = 0; i < nframes; i++) {
                float ol = u->buf_out_l[i];
                if (!isfinite(ol)) ol = 0.0f;
                else if (fabsf(ol) > 0.966f) ol = (ol > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(ol) - 0.966f) / 0.033f));
                buf_l[i] = ol;
                if (is_stereo && buf_r) {
                    float or_val = (u->num_audio_out >= 2 || u->lv2_handle_r) ? u->buf_out_r[i] : ol;
                    if (!isfinite(or_val)) or_val = 0.0f;
                    else if (fabsf(or_val) > 0.966f) or_val = (or_val > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(or_val) - 0.966f) / 0.033f));
                    buf_r[i] = or_val;
                }
            }
        } else {
            for (uint32_t i = 0; i < nframes; i++) {
                float sl = buf_l[i];
                float sr = (is_stereo && buf_r) ? buf_r[i] : sl;
                process_channel_effect_slots_range(st, ch, s, s + 1, &sl, &sr, is_stereo, fs);
                buf_l[i] = sl;
                if (is_stereo && buf_r) buf_r[i] = sr;
            }
        }
    }
}

static void init_default_mixer_state(MeterState *st, float fs) {
    memset(st, 0, sizeof(MeterState));
    st->sample_rate = fs;
    st->denoise_learn_done_ch = -1;
    int default_models[NUM_CHANNELS] = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* Default all 8 channels to FETISH 76 to match UI */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        /* Stage 1: 1176 FET Preamp defaults (0.0 dB clean unity drive, 0.0 dB trim) */
        st->preamp_enabled[i] = 1;
        st->preamp_drive_db[i] = 0.0f;
        st->preamp_drive_lin[i] = 1.0f;
        st->preamp_auto_comp[i] = 1.0f;
        st->preamp_trim_db[i] = 0.0f;
        st->preamp_trim_lin[i] = 1.0f;
        st->preamp_clip_hard[i] = 0;

        /* Stage 2: Digital Pre-Gain, Polarity & 80Hz HPF */
        st->ch_stereo_active[i] = 0;
        st->ch_pregain_lin[i] = 1.0f;
        st->ch_phase_inv[i] = 0;
        st->ch_hpf80[i] = 0;
        st->ch_insert_mode[i] = 1; /* Default to 1 = 4 Slots Only (Rule 28) */
        init_hpf_biquad(&st->hpf_filters[i], 80.0f, fs);
        init_hpf_biquad(&st->hpf_filters_r[i], 80.0f, fs);

        /* Stage 3: 10-Band Cyber-Denoiser PRO (Matched to CyberDenoiserEngine.hpp) */
        st->gate_enabled[i] = 1;
        st->gate_thresh_lin[i] = powf(10.0f, -60.0f / 20.0f);
        st->gate_floor_lin[i]  = powf(10.0f, -18.0f / 20.0f);
        st->gate_env[i] = 0.0f;
        st->gate_gain[i] = 1.0f;
        st->gate_open[i] = 1;
        st->denoise_sensitivity_db[i] = 0.0f;
        st->denoise_reduction_pct[i] = 85.0f;
        st->denoise_low_cut[i] = 1;
        st->denoise_listen[i] = 0;
        for (int b = 0; b < 10; b++) {
            st->denoise_thresh_db[i][b] = -60.0f;
            st->denoise_current_gains[i][b] = 1.0f;
            st->denoise_band_levels[i][b] = 0.0f;
        }

        /* Stage 4: 8-Band Cytomic SVF Equaliser */
        init_eq_strip(&st->ch_eq[i], fs);

        /* Stage 5: Unified Channel Compressor (clean unity defaults) */
        init_dynamics_slot(&st->ch_dyn[i], default_models[i], fs);

        /* Stage 6: Dual-Band Zero-Phase Cytomic SVF De-Esser (Off by default so guitars/instruments stay bright) */
        st->deesser_enabled[i] = 0;
        st->deesser_thresh1_db[i] = -22.0f;
        st->deesser_thresh_lin[i] = powf(10.0f, -22.0f / 20.0f);
        st->deesser_freq1[i] = 4800.0f;
        st->deesser_q1[i] = 1.8f;
        st->deesser_max1_db[i] = 14.0f;
        st->deesser_band2_en[i] = 1;
        st->deesser_thresh2_db[i] = -24.0f;
        st->deesser_thresh2_lin[i] = powf(10.0f, -24.0f / 20.0f);
        st->deesser_freq2[i] = 9500.0f;
        st->deesser_q2[i] = 1.6f;
        st->deesser_max2_db[i] = 12.0f;
        st->deesser_rel_coeff[i] = expf(-1.0f / (0.045f * fs));
        st->deesser_wide[i] = 0;
        st->deesser_listen[i] = 0;
        update_deesser_svf_band(&st->deess_svf1[i], st->deesser_freq1[i], st->deesser_q1[i], fs);
        update_deesser_svf_band(&st->deess_svf2[i], st->deesser_freq2[i], st->deesser_q2[i], fs);
        init_hpf_biquad(&st->deesser_hp_filters[i], 4800.0f, fs);
        init_hpf_biquad(&st->deesser_hp_filters_r[i], 4800.0f, fs);
        st->deesser_env[i] = 0.0f;
        st->deesser_env2[i] = 0.0f;
        st->deesser_gr1_db[i] = 0.0f;
        st->deesser_gr2_db[i] = 0.0f;
        st->deesser_det1_db[i] = -80.0f;
        st->deesser_det2_db[i] = -80.0f;

        st->ch_fader_gain[i] = 1.0f; /* 0.0 dB unity at 75% */
        st->ch_pan[i] = 0.0f;        /* Center */
        st->ch_mute[i] = 0;
        st->ch_solo[i] = 0;
        st->ch_gr_db[i] = 0.0f;
        st->ch_afd[i].auto_hunting = 0; /* Off by default so sustained guitar notes are never notched */
    }
    for (int m = 0; m < NUM_MONITORS; m++) {
        st->mon_fader_gain[m] = 1.0f;
        st->mon_mute[m] = 0;
        init_eq_strip(&st->mon_eq[m], fs);
        init_dynamics_slot(&st->mon_dyn[m], 0, fs);  /* Monitor Slot A: FETISH 76 Bus Compressor */
        init_dynamics_slot(&st->mon_dyn2[m], 3, fs); /* Monitor Slot B: BUSTER Speaker Limiter at -1.0 dBFS */
        st->mon_afd[m].auto_hunting = 0; /* Soundcheck Ring-Out Lock on Monitors 1-6 */
        for (int c = 0; c < NUM_CHANNELS; c++) {
            st->sof_mon_gain[m][c] = 0.0f;
        }
    }
    for (int fx = 0; fx < NUM_FX; fx++) {
        st->send_fader_gain[fx] = 1.0f;
        st->ret_fader_gain[fx] = 1.0f;
        st->ret_pan[fx] = 0.0f; /* True Stereo Returns 1-4 centered by default */
        init_eq_strip(&st->send_eq[fx], fs);
        init_eq_strip(&st->ret_eq[fx], fs);
        init_dynamics_slot(&st->send_dyn[fx], 0, fs); /* FET 76 Ducking Comp */
        init_dynamics_slot(&st->ret_dyn[fx], 1, fs);  /* LALA Glue Comp */
        for (int c = 0; c < NUM_CHANNELS; c++) {
            st->sof_fx_gain[fx][c] = 0.0f;
        }
    }

    /* Reverb 1 (Send 1 -> Return 1): Instrument Room / Plate Hybrid */
    st->rev[0].decay       = 58.0f;
    st->rev[0].size        = 68.0f;
    st->rev[0].damp        = 36.0f;
    st->rev[0].predelay    = 14.0f;
    st->rev[0].diffuse     = 88.0f;
    st->rev[0].ir_mix      = 54.0f;
    st->rev[0].stereo      = 100.0f;
    st->rev[0].dry         = 0.0f;
    st->rev[0].wet         = 85.0f;
    st->rev[0].in_lowcut   = 110.0f;
    st->rev[0].in_highcut  = 9500.0f;
    st->rev[0].out_lowcut  = 90.0f;
    st->rev[0].out_highcut = 11000.0f;
    st->rev[0].mod_rate    = 18.0f;
    st->rev[0].mod_amount  = 28.0f;
    st->rev[0].ducking     = 0.0f;

    /* Reverb 2 (Send 2 -> Return 2): Vocal-Focused Lush Hall / Abbey Plate Hybrid */
    st->rev[1].decay       = 78.0f;
    st->rev[1].size        = 84.0f;
    st->rev[1].damp        = 24.0f;
    st->rev[1].predelay    = 36.0f;
    st->rev[1].diffuse     = 95.0f;
    st->rev[1].ir_mix      = 42.0f;
    st->rev[1].stereo      = 100.0f;
    st->rev[1].dry         = 0.0f;
    st->rev[1].wet         = 88.0f;
    st->rev[1].in_lowcut   = 180.0f;
    st->rev[1].in_highcut  = 7200.0f;
    st->rev[1].out_lowcut  = 160.0f;
    st->rev[1].out_highcut = 6800.0f;
    st->rev[1].mod_rate    = 24.0f;
    st->rev[1].mod_amount  = 42.0f;
    st->rev[1].ducking     = 22.0f;

    /* Delay 1 (Send 3 -> Return 3): Perfect 15/30 IPS Tape Slapback Engine */
    st->dly[0].time_ms     = 115.0f;
    st->dly[0].smooth_samp = 115.0f * 0.001f * fs;
    st->dly[0].feedback    = 18.0f;
    st->dly[0].diffuse     = 24.0f;
    st->dly[0].dry         = 0.0f;
    st->dly[0].wet         = 85.0f;
    st->dly[0].ping_pong   = 0;
    st->dly[0].in_lowcut   = 140.0f;
    st->dly[0].in_highcut  = 5200.0f;
    st->dly[0].out_lowcut  = 120.0f;
    st->dly[0].out_highcut = 4800.0f;
    st->dly[0].drive       = 42.0f;
    st->dly[0].flutter     = 28.0f;
    st->dly[0].mod_rate    = 18.0f;
    st->dly[0].mod_depth   = 16.0f;
    st->dly[0].ducking     = 0.0f;
    st->dly[0].stereo      = 100.0f;
    st->dly[0].spread      = 58.0f;

    /* Delay 2 (Send 4 -> Return 4): Perfect Modulated Vocal Ping-Pong Delay Engine */
    st->dly[1].time_ms     = 385.0f;
    st->dly[1].smooth_samp = 385.0f * 0.001f * fs;
    st->dly[1].feedback    = 42.0f;
    st->dly[1].diffuse     = 34.0f;
    st->dly[1].dry         = 0.0f;
    st->dly[1].wet         = 85.0f;
    st->dly[1].ping_pong   = 1;
    st->dly[1].in_lowcut   = 180.0f;
    st->dly[1].in_highcut  = 6400.0f;
    st->dly[1].out_lowcut  = 160.0f;
    st->dly[1].out_highcut = 5800.0f;
    st->dly[1].drive       = 22.0f;
    st->dly[1].flutter     = 14.0f;
    st->dly[1].mod_rate    = 28.0f;
    st->dly[1].mod_depth   = 45.0f;
    st->dly[1].ducking     = 28.0f;
    st->dly[1].stereo      = 100.0f;
    st->dly[1].spread      = 54.0f;

    init_eq_strip(&st->master_eq, fs);
    st->master_eq.adaptive_room_mode = 0; /* Flat Master EQ by default until Room Calibration is run */
    for (int b = 1; b <= 6; b++) {
        st->master_eq.bands[b].dyn_enabled = 0;
    }
    st->master_afd.auto_hunting = 0; /* Soundcheck Ring-Out Lock on Master L/R */
    st->summing_enabled = 1;
    st->summing_console = 0; /* Neve 8048 */
    st->summing_drive_db = 0.0f;  /* 0.0 dB unity Master Summing Drive by default */
    st->summing_warmth_db = 0.0f; /* 0.0 dB flat Master Summing Warmth by default */
    st->summing_crosstalk_db = -60.0f;
    st->rng_seed = 0x13579BDFu;
    init_rta_bank(st, fs);
    init_dynamics_slot(&st->master_dyn, 1, fs);  /* Master Slot A: LALA Optical Bus Glue */
    init_dynamics_slot(&st->master_dyn2, 3, fs); /* Master Slot B: BUSTER Speaker Brickwall Limiter */
    st->master_dyn.lim_ceil_lin = 0.97724f;
    st->master_dyn2.lim_ceil_lin = 0.97724f; /* -0.2 dBFS Master Brickwall Ceiling */
    st->master_fader_gain = 1.0f;
    st->master_mute = 0;

    /* Stage 7: Initialize 2" Analog Multitrack Tape Machine (20 Pre-Wired + Unlimited Paired Tracks) */
    st->tape_transport = 0; /* STOP */
    st->tape_frame_pos = 0;
    st->tape_max_frames = 0;
    st->tape_ips = 15;          /* 15 IPS (65Hz fat head-bump + 14kHz warm oxide rolloff) */
    st->tape_emu_enabled = 1;   /* 2" Tape Emulation active on Monitor Playback */
    st->tape_print_wav = 0;     /* Non-destructive WAV recording by default */
    st->tape_master_vol = 1.0f; /* 0 dB stereo playback to MAIN L/R */
    for (int m = 0; m < NUM_MONITORS; m++) st->tape_mon_send[m] = 0.0f;
    snprintf(st->tape_song_name, sizeof(st->tape_song_name), "Song_01_Live_Take");
    snprintf(st->tape_base_dir, sizeof(st->tape_base_dir), "/var/modep/recordings");
    st->tape_dir_manual = 0;
    st->tape_free_gb = 16.0f;
    snprintf(st->tape_drives_json, sizeof(st->tape_drives_json), "[{\"label\":\"SD Card (/var/modep/recordings)\",\"path\":\"/var/modep/recordings\",\"free\":16.0}]");
    st->ring_write_pos = 0;
    st->ring_read_pos = 0;
    st->play_write_pos = 0;
    st->play_read_pos = 0;
    st->tape_seek_requested = 1;
    st->tape_disk_frames_written = 0;
    st->tape_num_tracks = 20;
    for (int t = 0; t < MAX_TAPE_TRACKS; t++) {
        st->tape_play_ring[t] = (float*)calloc(DISK_RING_FRAMES, sizeof(float));
        st->tape_disk_ring[t] = (float*)calloc(DISK_RING_FRAMES, sizeof(float));
        st->tape_disk_fp[t] = NULL;
        st->tape_play_fp[t] = NULL;
        st->tape_trk_vol[t] = 1.0f;
        st->tape_trk_pan[t] = 0.0f;
        st->tape_trk_drive_db[t] = 1.5f; /* +1.5 dB into 0 VU sweet spot */
        st->tape_trk_rec[t] = 1;
        st->tape_trk_solo[t] = 0;
        st->tape_trk_vu_db[t] = -30.0f;
        st->tape_trk_clip[t] = 0;
        if (t < 8) {
            /* Tracks 1-8: Ch 1-8 Processed Outs (Main View, Unmuted) */
            st->tape_trk_src[t] = t;
            st->tape_trk_is_direct[t] = 0;
            st->tape_trk_mute[t] = 0;
        } else if (t < 12) {
            /* Tracks 9-12: FX 1-4 Returns (Main View, Unmuted) */
            st->tape_trk_src[t] = t - 8;
            st->tape_trk_is_direct[t] = 0;
            st->tape_trk_mute[t] = 0;
            st->tape_trk_pan[t] = ((t & 1) == 0) ? -0.45f : 0.45f;
        } else if (t < 20) {
            /* Tracks 13-20: Ch 1-8 Post-Preamp (1176+HPF) Direct Safety Tracks (Direct Sub-Page, Muted on Playback by default!) */
            st->tape_trk_src[t] = t - 12;
            st->tape_trk_is_direct[t] = 1;
            st->tape_trk_mute[t] = 1;
        } else {
            st->tape_trk_src[t] = (t - 20) % 8;
            st->tape_trk_is_direct[t] = (t & 1);
            st->tape_trk_mute[t] = (t & 1);
        }
    }

    /* Stage 8: Universal Audio Interface Defaults */
    for (int m = 0; m < NUM_MONITORS; m++) st->mon_solo[m] = 0;
    st->solo_cue_to_main = 1;
    snprintf(st->hw_card_id, sizeof(st->hw_card_id), "auto");
    snprintf(st->hw_card_name, sizeof(st->hw_card_name), "AUTO-DETECT USB AUDIO");
    st->hw_in_count = 8;
    st->hw_out_count = 8;
    st->hw_buffer_size = 128;
    snprintf(st->hw_cards_json, sizeof(st->hw_cards_json), "[]");
    for (int c = 0; c < NUM_CHANNELS; c++) st->ch_input_patch[c] = c + 1;
    st->jack_heartbeat = 0;
}

/* Stage 8: Scan /proc/asound/cards to discover all connected USB / ALSA audio interfaces */
static void scan_audio_interfaces(MeterState *st) {
    FILE *fp = fopen("/proc/asound/cards", "r");
    char jbuf[1024];
    int n = snprintf(jbuf, sizeof(jbuf), "[{\"id\":\"auto\",\"name\":\"Auto-Detect USB/ALSA Interface\"}");
    char detected_usb_id[64] = {0};
    char detected_usb_name[128] = {0};

    if (fp) {
        char line1[256], line2[256];
        while (fgets(line1, sizeof(line1), fp)) {
            int card_num = -1;
            char id[64] = {0}, drv[64] = {0}, desc[128] = {0};
            if (sscanf(line1, " %d [%63[^]]]: %63s - %127[^\n]", &card_num, id, drv, desc) >= 3) {
                /* Trim trailing whitespace from id */
                for (int k = (int)strlen(id) - 1; k >= 0 && (id[k] == ' ' || id[k] == '\t'); k--) id[k] = '\0';
                for (int k = (int)strlen(desc) - 1; k >= 0 && (desc[k] == ' ' || desc[k] == '\r' || desc[k] == '\n'); k--) desc[k] = '\0';
                if (fgets(line2, sizeof(line2), fp) == NULL) line2[0] = '\0';
                /* Skip internal Raspberry Pi HDMI playback-only devices in main USB list unless needed */
                if (strncmp(id, "vc4hdmi", 7) == 0) continue;
                /* Sanitize quotes */
                for (int k = 0; desc[k]; k++) { if (desc[k] == '"' || desc[k] == '\\') desc[k] = ' '; }
                if (n < (int)sizeof(jbuf) - 160) {
                    n += snprintf(jbuf + n, sizeof(jbuf) - n, ",{\"id\":\"%s\",\"name\":\"%s (%s)\"}", id, desc, id);
                }
                if (detected_usb_id[0] == '\0' && (strstr(drv, "USB-Audio") != NULL || strstr(id, "EVO") != NULL || strstr(id, "UMC") != NULL || strstr(id, "pisound") != NULL)) {
                    snprintf(detected_usb_id, sizeof(detected_usb_id), "%s", id);
                    snprintf(detected_usb_name, sizeof(detected_usb_name), "%s", desc);
                }
            }
        }
        fclose(fp);
    }
    if (n < (int)sizeof(jbuf) - 80) {
        n += snprintf(jbuf + n, sizeof(jbuf) - n, ",{\"id\":\"dummy\",\"name\":\"Virtual Dummy Engine (18x20 No Hardware)\"}]");
    } else {
        snprintf(jbuf + n, sizeof(jbuf) - n, "]");
    }
    snprintf(st->hw_cards_json, sizeof(st->hw_cards_json), "%s", jbuf);

    if (detected_usb_id[0] != '\0') {
        snprintf(st->hw_card_id, sizeof(st->hw_card_id), "%s", detected_usb_id);
        /* Uppercase display name for header badge */
        for (int k = 0; detected_usb_name[k]; k++) {
            if (detected_usb_name[k] >= 'a' && detected_usb_name[k] <= 'z') detected_usb_name[k] -= 32;
        }
        snprintf(st->hw_card_name, sizeof(st->hw_card_name), "%s", detected_usb_name);
    } else if (st->hw_in_count >= 18) {
        snprintf(st->hw_card_id, sizeof(st->hw_card_id), "dummy");
        snprintf(st->hw_card_name, sizeof(st->hw_card_name), "VIRTUAL 18x20 ENGINE");
    }
}

/* Stage 7: Airwindows ToTape6 / Tukan Tape S2 2" Magnetic Tape Emulation
 * Calibrated around 0 VU = -18.0 dBFS (0.1258925f linear).
 * - 15 IPS: +1.8 dB 65 Hz head-bump + smooth 14 kHz warm oxide rolloff
 * - 30 IPS: +1.0 dB 110 Hz head-bump + 18.5 kHz open top-end
 * - 0 to +3 VU (-18 to -15 dBFS): Golden Sweet Spot soft magnetic compression
 * - +3 to +6 VU (-15 to -12 dBFS): Warm 3rd-harmonic Studer/Ampex oxide saturation */
static inline float process_tape_2inch_sample(MeterState *st, int t, float x, float fs) {
    if (!st->tape_emu_enabled) return x;
    float drive_lin = powf(10.0f, st->tape_trk_drive_db[t] / 20.0f);
    float hb_fc = (st->tape_ips == 30) ? 110.0f : 65.0f;
    float hb_gain = (st->tape_ips == 30) ? 0.14f : 0.24f;
    float hf_fc = (st->tape_ips == 30) ? 18500.0f : 14000.0f;
    float hb_a = 1.0f - expf(-2.0f * (float)M_PI * hb_fc / fs);
    float hf_a = 1.0f - expf(-2.0f * (float)M_PI * hf_fc / fs);

    st->tape_trk_hb_lp[t] += hb_a * (x - st->tape_trk_hb_lp[t]);
    float bumped = x + st->tape_trk_hb_lp[t] * hb_gain;
    st->tape_trk_hf_lp[t] += hf_a * (bumped - st->tape_trk_hf_lp[t]);
    float rolled = st->tape_trk_hf_lp[t];

    /* Normalize to 0 VU = -18.0 dBFS (0.1258925f) so 1.0 in x_vu equals 0 VU */
    float x_vu = (rolled / 0.1258925f) * drive_lin;
    float abs_vu = fabsf(x_vu);
    if (abs_vu > 1.0f) {
        /* Above 0 VU (-18 dBFS): Soft magnetic knee & 3rd-harmonic hysteresis */
        float over = abs_vu - 1.0f;
        float sat = 1.0f + 2.2f * tanhf(over / 2.2f);
        x_vu = (x_vu >= 0.0f) ? sat : -sat;
    } else {
        /* Subtle even/odd harmonic warmth below 0 VU */
        x_vu = x_vu - 0.035f * x_vu * x_vu * x_vu;
    }
    return x_vu * 0.1258925f;
}

static void mkdir_recursive(const char *path) {
    char tmp[384];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

#define LIVE_STATE_MAGIC   0x4D4F4450u
#define LIVE_STATE_VERSION 6u
#define LIVE_STATE_PATH    "/var/modep/last_live_mixer_state.bin"
#define LIVE_STATE_TMP     "/var/modep/last_live_mixer_state.bin.tmp"

typedef struct {
    int   active, bypass, fx_class;
    char  uri[160];
    char  name[80];
    float p_drive, p_tone, p_bass, p_mid, p_treble, p_level, p_mix, p_time, p_feedback, p_rate, p_depth;
    int   sw_bright, sw_mode;
} PersistentSlotParamV4;

typedef struct {
    int   active, bypass, fx_class;
    char  uri[160];
    char  name[80];
    float p_drive, p_tone, p_bass, p_mid, p_treble, p_level, p_mix, p_time, p_feedback, p_rate, p_depth;
    int   sw_bright, sw_mode;
    char  file_path[256];
    char  file_label[128];
    int   model_preset_id;
} PersistentSlotParam;

typedef struct {
    float decay, size, damp, predelay, diffuse, ir_mix, stereo, dry, wet;
    float in_lowcut, in_highcut, out_lowcut, out_highcut;
    float mod_rate, mod_amount, ducking, lofi_bits, lofi_sr;
} PersistentReverbParam;

typedef struct {
    float time_ms, feedback, diffuse, dry, wet;
    int   ping_pong;
    float in_lowcut, in_highcut, out_lowcut, out_highcut;
    float drive, flutter, mod_rate, mod_depth, ducking, stereo, spread;
} PersistentDelayParam;

typedef struct {
    uint32_t magic;
    uint32_t version;
    int   preamp_enabled[NUM_CHANNELS];
    float preamp_drive_db[NUM_CHANNELS];
    float preamp_drive_lin[NUM_CHANNELS];
    float preamp_trim_db[NUM_CHANNELS];
    float preamp_trim_lin[NUM_CHANNELS];
    int   preamp_clip_hard[NUM_CHANNELS];
    float ch_pregain_lin[NUM_CHANNELS];
    int   ch_phase_inv[NUM_CHANNELS];
    int   ch_hpf80[NUM_CHANNELS];
    int   slot_first[NUM_CHANNELS];
    int   modgui_first[NUM_CHANNELS];
    int   ch_insert_mode[NUM_CHANNELS]; /* 0 = Direct Clean Strip, 1 = 4 Slots Only, 2 = MOD Pedalboard Only (Rule 28) */
    PersistentSlotParam slots[NUM_CHANNELS][NUM_CH_SLOTS];
    int   gate_enabled[NUM_CHANNELS];
    float gate_thresh_lin[NUM_CHANNELS];
    float gate_floor_lin[NUM_CHANNELS];
    float denoise_thresh_db[NUM_CHANNELS][10];
    float denoise_sensitivity_db[NUM_CHANNELS];
    float denoise_reduction_pct[NUM_CHANNELS];
    int   denoise_low_cut[NUM_CHANNELS];
    int   denoise_listen[NUM_CHANNELS];
    EqStrip8 ch_eq[NUM_CHANNELS];
    EqStrip8 mon_eq[NUM_MONITORS];
    EqStrip8 send_eq[NUM_FX];
    EqStrip8 ret_eq[NUM_FX];
    EqStrip8 master_eq;
    AfdNotchBank ch_afd[NUM_CHANNELS];
    AfdNotchBank mon_afd[NUM_MONITORS];
    AfdNotchBank master_afd;
    DynamicsSlot ch_dyn[NUM_CHANNELS];
    DynamicsSlot mon_dyn[NUM_MONITORS];
    DynamicsSlot mon_dyn2[NUM_MONITORS];
    DynamicsSlot send_dyn[NUM_FX];
    DynamicsSlot ret_dyn[NUM_FX];
    DynamicsSlot master_dyn;
    DynamicsSlot master_dyn2;
    int   deesser_enabled[NUM_CHANNELS];
    float deesser_thresh_lin[NUM_CHANNELS];
    float deesser_thresh1_db[NUM_CHANNELS];
    float deesser_freq1[NUM_CHANNELS];
    float deesser_q1[NUM_CHANNELS];
    float deesser_max1_db[NUM_CHANNELS];
    float deesser_thresh2_lin[NUM_CHANNELS];
    float deesser_thresh2_db[NUM_CHANNELS];
    float deesser_freq2[NUM_CHANNELS];
    float deesser_q2[NUM_CHANNELS];
    float deesser_max2_db[NUM_CHANNELS];
    int   deesser_band2_en[NUM_CHANNELS];
    float deesser_rel_coeff[NUM_CHANNELS];
    int   deesser_wide[NUM_CHANNELS];
    int   deesser_listen[NUM_CHANNELS];
    float ch_fader_gain[NUM_CHANNELS];
    float ch_pan[NUM_CHANNELS];
    int   ch_mute[NUM_CHANNELS];
    int   ch_solo[NUM_CHANNELS];
    int   ch_tuner_mute[NUM_CHANNELS];
    float sof_mon_gain[NUM_MONITORS][NUM_CHANNELS];
    float sof_fx_gain[NUM_FX][NUM_CHANNELS];
    float mon_fader_gain[NUM_MONITORS];
    int   mon_mute[NUM_MONITORS];
    int   mon_solo[NUM_MONITORS];
    float send_fader_gain[NUM_FX];
    int   send_mute[NUM_FX];
    int   send_solo[NUM_FX];
    float ret_fader_gain[NUM_FX];
    float ret_pan[NUM_FX];
    int   ret_mute[NUM_FX];
    int   ret_solo[NUM_FX];
    float master_fader_gain;
    int   master_mute;
    int   summing_enabled;
    int   summing_console;
    float summing_drive_db;
    float summing_warmth_db;
    float summing_crosstalk_db;
    PersistentReverbParam rev[2];
    PersistentDelayParam  dly[2];
    int   tape_ips;
    int   tape_emu_enabled;
    int   tape_print_wav;
    float tape_master_vol;
    float tape_mon_send[NUM_MONITORS];
    char  tape_base_dir[192];
    int   tape_dir_manual;
    char  tape_song_name[64];
    int   tape_num_tracks;
    int   tape_trk_src[MAX_TAPE_TRACKS];
    int   tape_trk_is_direct[MAX_TAPE_TRACKS];
    float tape_trk_vol[MAX_TAPE_TRACKS];
    float tape_trk_pan[MAX_TAPE_TRACKS];
    float tape_trk_drive_db[MAX_TAPE_TRACKS];
    int   tape_trk_mute[MAX_TAPE_TRACKS];
    int   tape_trk_solo[MAX_TAPE_TRACKS];
    int   tape_trk_rec[MAX_TAPE_TRACKS];
    int   solo_cue_to_main;
    int   ch_input_patch[NUM_CHANNELS];
    char  ui_snapshot[65536];
} LivePersistentState;

typedef struct {
    uint32_t magic;
    uint32_t version;
    int   preamp_enabled[NUM_CHANNELS];
    float preamp_drive_db[NUM_CHANNELS];
    float preamp_drive_lin[NUM_CHANNELS];
    float preamp_trim_db[NUM_CHANNELS];
    float preamp_trim_lin[NUM_CHANNELS];
    int   preamp_clip_hard[NUM_CHANNELS];
    float ch_pregain_lin[NUM_CHANNELS];
    int   ch_phase_inv[NUM_CHANNELS];
    int   ch_hpf80[NUM_CHANNELS];
    int   slot_first[NUM_CHANNELS];
    int   modgui_first[NUM_CHANNELS];
    PersistentSlotParamV4 slots[NUM_CHANNELS][NUM_CH_SLOTS];
    int   gate_enabled[NUM_CHANNELS];
    float gate_thresh_lin[NUM_CHANNELS];
    float gate_floor_lin[NUM_CHANNELS];
    float denoise_thresh_db[NUM_CHANNELS][10];
    float denoise_sensitivity_db[NUM_CHANNELS];
    float denoise_reduction_pct[NUM_CHANNELS];
    int   denoise_low_cut[NUM_CHANNELS];
    int   denoise_listen[NUM_CHANNELS];
    EqStrip8 ch_eq[NUM_CHANNELS];
    EqStrip8 mon_eq[NUM_MONITORS];
    EqStrip8 send_eq[NUM_FX];
    EqStrip8 ret_eq[NUM_FX];
    EqStrip8 master_eq;
    AfdNotchBank ch_afd[NUM_CHANNELS];
    AfdNotchBank mon_afd[NUM_MONITORS];
    AfdNotchBank master_afd;
    DynamicsSlot ch_dyn[NUM_CHANNELS];
    DynamicsSlot mon_dyn[NUM_MONITORS];
    DynamicsSlot mon_dyn2[NUM_MONITORS];
    DynamicsSlot send_dyn[NUM_FX];
    DynamicsSlot ret_dyn[NUM_FX];
    DynamicsSlot master_dyn;
    DynamicsSlot master_dyn2;
    int   deesser_enabled[NUM_CHANNELS];
    float deesser_thresh_lin[NUM_CHANNELS];
    float deesser_thresh1_db[NUM_CHANNELS];
    float deesser_freq1[NUM_CHANNELS];
    float deesser_q1[NUM_CHANNELS];
    float deesser_max1_db[NUM_CHANNELS];
    float deesser_thresh2_lin[NUM_CHANNELS];
    float deesser_thresh2_db[NUM_CHANNELS];
    float deesser_freq2[NUM_CHANNELS];
    float deesser_q2[NUM_CHANNELS];
    float deesser_max2_db[NUM_CHANNELS];
    int   deesser_band2_en[NUM_CHANNELS];
    float deesser_rel_coeff[NUM_CHANNELS];
    int   deesser_wide[NUM_CHANNELS];
    int   deesser_listen[NUM_CHANNELS];
    float ch_fader_gain[NUM_CHANNELS];
    float ch_pan[NUM_CHANNELS];
    int   ch_mute[NUM_CHANNELS];
    int   ch_solo[NUM_CHANNELS];
    int   ch_tuner_mute[NUM_CHANNELS];
    float sof_mon_gain[NUM_MONITORS][NUM_CHANNELS];
    float sof_fx_gain[NUM_FX][NUM_CHANNELS];
    float mon_fader_gain[NUM_MONITORS];
    int   mon_mute[NUM_MONITORS];
    int   mon_solo[NUM_MONITORS];
    float send_fader_gain[NUM_FX];
    int   send_mute[NUM_FX];
    int   send_solo[NUM_FX];
    float ret_fader_gain[NUM_FX];
    float ret_pan[NUM_FX];
    int   ret_mute[NUM_FX];
    int   ret_solo[NUM_FX];
    float master_fader_gain;
    int   master_mute;
    int   summing_enabled;
    int   summing_console;
    float summing_drive_db;
    float summing_warmth_db;
    float summing_crosstalk_db;
    PersistentReverbParam rev[2];
    PersistentDelayParam  dly[2];
    int   tape_ips;
    int   tape_emu_enabled;
    int   tape_print_wav;
    float tape_master_vol;
    float tape_mon_send[NUM_MONITORS];
    char  tape_base_dir[192];
    int   tape_dir_manual;
    char  tape_song_name[64];
    int   tape_num_tracks;
    int   tape_trk_src[MAX_TAPE_TRACKS];
    int   tape_trk_is_direct[MAX_TAPE_TRACKS];
    float tape_trk_vol[MAX_TAPE_TRACKS];
    float tape_trk_pan[MAX_TAPE_TRACKS];
    float tape_trk_drive_db[MAX_TAPE_TRACKS];
    int   tape_trk_mute[MAX_TAPE_TRACKS];
    int   tape_trk_solo[MAX_TAPE_TRACKS];
    int   tape_trk_rec[MAX_TAPE_TRACKS];
    int   solo_cue_to_main;
    int   ch_input_patch[NUM_CHANNELS];
    char  ui_snapshot[65536];
} LivePersistentStateV4;

typedef struct {
    uint32_t magic;
    uint32_t version;
    int   preamp_enabled[NUM_CHANNELS];
    float preamp_drive_db[NUM_CHANNELS];
    float preamp_drive_lin[NUM_CHANNELS];
    float preamp_trim_db[NUM_CHANNELS];
    float preamp_trim_lin[NUM_CHANNELS];
    int   preamp_clip_hard[NUM_CHANNELS];
    float ch_pregain_lin[NUM_CHANNELS];
    int   ch_phase_inv[NUM_CHANNELS];
    int   ch_hpf80[NUM_CHANNELS];
    int   slot_first[NUM_CHANNELS];
    int   modgui_first[NUM_CHANNELS];
    PersistentSlotParam slots[NUM_CHANNELS][NUM_CH_SLOTS];
    int   gate_enabled[NUM_CHANNELS];
    float gate_thresh_lin[NUM_CHANNELS];
    float gate_floor_lin[NUM_CHANNELS];
    float denoise_thresh_db[NUM_CHANNELS][10];
    float denoise_sensitivity_db[NUM_CHANNELS];
    float denoise_reduction_pct[NUM_CHANNELS];
    int   denoise_low_cut[NUM_CHANNELS];
    int   denoise_listen[NUM_CHANNELS];
    EqStrip8 ch_eq[NUM_CHANNELS];
    EqStrip8 mon_eq[NUM_MONITORS];
    EqStrip8 send_eq[NUM_FX];
    EqStrip8 ret_eq[NUM_FX];
    EqStrip8 master_eq;
    AfdNotchBank ch_afd[NUM_CHANNELS];
    AfdNotchBank mon_afd[NUM_MONITORS];
    AfdNotchBank master_afd;
    DynamicsSlot ch_dyn[NUM_CHANNELS];
    DynamicsSlot mon_dyn[NUM_MONITORS];
    DynamicsSlot mon_dyn2[NUM_MONITORS];
    DynamicsSlot send_dyn[NUM_FX];
    DynamicsSlot ret_dyn[NUM_FX];
    DynamicsSlot master_dyn;
    DynamicsSlot master_dyn2;
    int   deesser_enabled[NUM_CHANNELS];
    float deesser_thresh_lin[NUM_CHANNELS];
    float deesser_thresh1_db[NUM_CHANNELS];
    float deesser_freq1[NUM_CHANNELS];
    float deesser_q1[NUM_CHANNELS];
    float deesser_max1_db[NUM_CHANNELS];
    float deesser_thresh2_lin[NUM_CHANNELS];
    float deesser_thresh2_db[NUM_CHANNELS];
    float deesser_freq2[NUM_CHANNELS];
    float deesser_q2[NUM_CHANNELS];
    float deesser_max2_db[NUM_CHANNELS];
    int   deesser_band2_en[NUM_CHANNELS];
    float deesser_rel_coeff[NUM_CHANNELS];
    int   deesser_wide[NUM_CHANNELS];
    int   deesser_listen[NUM_CHANNELS];
    float ch_fader_gain[NUM_CHANNELS];
    float ch_pan[NUM_CHANNELS];
    int   ch_mute[NUM_CHANNELS];
    int   ch_solo[NUM_CHANNELS];
    int   ch_tuner_mute[NUM_CHANNELS];
    float sof_mon_gain[NUM_MONITORS][NUM_CHANNELS];
    float sof_fx_gain[NUM_FX][NUM_CHANNELS];
    float mon_fader_gain[NUM_MONITORS];
    int   mon_mute[NUM_MONITORS];
    int   mon_solo[NUM_MONITORS];
    float send_fader_gain[NUM_FX];
    int   send_mute[NUM_FX];
    int   send_solo[NUM_FX];
    float ret_fader_gain[NUM_FX];
    float ret_pan[NUM_FX];
    int   ret_mute[NUM_FX];
    int   ret_solo[NUM_FX];
    float master_fader_gain;
    int   master_mute;
    int   summing_enabled;
    int   summing_console;
    float summing_drive_db;
    float summing_warmth_db;
    float summing_crosstalk_db;
    PersistentReverbParam rev[2];
    PersistentDelayParam  dly[2];
    int   tape_ips;
    int   tape_emu_enabled;
    int   tape_print_wav;
    float tape_master_vol;
    float tape_mon_send[NUM_MONITORS];
    char  tape_base_dir[192];
    int   tape_dir_manual;
    char  tape_song_name[64];
    int   tape_num_tracks;
    int   tape_trk_src[MAX_TAPE_TRACKS];
    int   tape_trk_is_direct[MAX_TAPE_TRACKS];
    float tape_trk_vol[MAX_TAPE_TRACKS];
    float tape_trk_pan[MAX_TAPE_TRACKS];
    float tape_trk_drive_db[MAX_TAPE_TRACKS];
    int   tape_trk_mute[MAX_TAPE_TRACKS];
    int   tape_trk_solo[MAX_TAPE_TRACKS];
    int   tape_trk_rec[MAX_TAPE_TRACKS];
    int   solo_cue_to_main;
    int   ch_input_patch[NUM_CHANNELS];
    char  ui_snapshot[65536];
} LivePersistentStateV5;

static void sanitize_loaded_eq_strip(EqStrip8 *eq, float fs) {
    const int   def_type[8]  = { 1, 3, 0, 0, 0, 0, 4, 5 };
    const float def_freq[8]  = { 30.0f, 100.0f, 250.0f, 800.0f, 2500.0f, 5000.0f, 10000.0f, 18000.0f };
    const float def_q[8]     = { 0.707f, 0.707f, 1.41f, 1.0f, 1.2f, 1.41f, 0.707f, 0.707f };
    const int   def_slope[8] = { 2, 1, 1, 1, 1, 1, 1, 2 };
    const int   def_en[8]    = { 0, 1, 1, 1, 1, 1, 1, 0 };
    if (eq->adaptive_room_mode != 0 && eq->adaptive_room_mode != 1) eq->adaptive_room_mode = 0;
    for (int b = 0; b < NUM_EQ_BANDS; b++) {
        CytomicSvfBand *bd = &eq->bands[b];
        memset(bd->ic1eq, 0, sizeof(bd->ic1eq));
        memset(bd->ic2eq, 0, sizeof(bd->ic2eq));
        memset(bd->ic1eq_r, 0, sizeof(bd->ic1eq_r));
        memset(bd->ic2eq_r, 0, sizeof(bd->ic2eq_r));
        bd->dyn_env = 0.0f;
        bd->dyn_gain_db = 0.0f;
        if (bd->enabled != 0 && bd->enabled != 1) bd->enabled = def_en[b];
        if (bd->type < 0 || bd->type > 9) bd->type = def_type[b];
        if (bd->slope >= 12) bd->slope = bd->slope / 12;
        if (bd->slope < 1 || bd->slope > 4) bd->slope = def_slope[b];
        if (!isfinite(bd->freq) || bd->freq < 10.0f || bd->freq > 22000.0f) bd->freq = def_freq[b];
        if (!isfinite(bd->gain) || bd->gain < -36.0f || bd->gain > 36.0f) bd->gain = 0.0f;
        if (!isfinite(bd->q) || bd->q < 0.1f || bd->q > 50.0f) bd->q = def_q[b];
        if (bd->dyn_enabled != 0 && bd->dyn_enabled != 1) bd->dyn_enabled = 0;
        if (!isfinite(bd->dyn_thresh_db) || bd->dyn_thresh_db < -80.0f || bd->dyn_thresh_db > 12.0f) bd->dyn_thresh_db = -20.0f;
        if (!isfinite(bd->dyn_range_db) || bd->dyn_range_db < -36.0f || bd->dyn_range_db > 36.0f) bd->dyn_range_db = -6.0f;
        if (!isfinite(bd->dyn_ratio) || bd->dyn_ratio < 1.0f || bd->dyn_ratio > 20.0f) bd->dyn_ratio = 3.0f;
        bd->sm_freq = bd->freq;
        bd->sm_gain = bd->gain;
        bd->sm_q    = bd->q;
        bd->param_dirty = 1;
        update_cytomic_svf_coeffs(bd, fs);
    }
}

static void sanitize_loaded_afd_bank(AfdNotchBank *bank, double fs) {
    for (int i = 0; i < NUM_AFD_NOTCHES; i++) {
        AfdNotchFilter64 *n = &bank->notches[i];
        n->ic1_l = 0.0; n->ic2_l = 0.0;
        n->ic1_r = 0.0; n->ic2_r = 0.0;
        if (n->active) update_afd_notch64_coeffs(n, fs);
    }
}

static void sanitize_loaded_dyn_slot(DynamicsSlot *d, float fs) {
    d->fet_det_env = 0.0f;
    d->fet_env_db  = 0.0f;
    d->lala_det_env = 0.0f;
    d->lala_env_db  = 0.0f;
    d->lim_env_db   = 0.0f;
    d->bypass_mix   = d->comp_bypass ? 0.0f : 1.0f;
    if (d->lim_rel_coeff <= 0.0f || d->lim_rel_coeff >= 1.0f) {
        d->lim_rel_coeff = expf(-1.0f / (0.025f * fs));
    }
    update_dynamics_slot_coeffs(d);
}

/* Atomic non-volatile state persistence ("Pi is King of Sync")
 * Writes to /var/modep/last_live_mixer_state.bin.tmp, fsyncs, and renames atomically
 * so power-outs during a live gig never corrupt or lose mixer settings. */
static void save_persistent_mixer_state(const MeterState *st) {
    static LivePersistentState ps;
    memset(&ps, 0, sizeof(ps));
    ps.magic   = LIVE_STATE_MAGIC;
    ps.version = LIVE_STATE_VERSION;

    memcpy(ps.preamp_enabled,   st->preamp_enabled,   sizeof(ps.preamp_enabled));
    memcpy(ps.preamp_drive_db,  st->preamp_drive_db,  sizeof(ps.preamp_drive_db));
    memcpy(ps.preamp_drive_lin, st->preamp_drive_lin, sizeof(ps.preamp_drive_lin));
    memcpy(ps.preamp_trim_db,   st->preamp_trim_db,   sizeof(ps.preamp_trim_db));
    memcpy(ps.preamp_trim_lin,  st->preamp_trim_lin,  sizeof(ps.preamp_trim_lin));
    memcpy(ps.preamp_clip_hard, st->preamp_clip_hard, sizeof(ps.preamp_clip_hard));
    memcpy(ps.ch_pregain_lin,   st->ch_pregain_lin,   sizeof(ps.ch_pregain_lin));
    memcpy(ps.ch_phase_inv,     st->ch_phase_inv,     sizeof(ps.ch_phase_inv));
    memcpy(ps.ch_hpf80,         st->ch_hpf80,         sizeof(ps.ch_hpf80));
    memcpy(ps.slot_first,       st->slot_first,       sizeof(ps.slot_first));
    memcpy(ps.modgui_first,     st->modgui_first,     sizeof(ps.modgui_first));
    memcpy(ps.ch_insert_mode,   st->ch_insert_mode,   sizeof(ps.ch_insert_mode));

    for (int c = 0; c < NUM_CHANNELS; c++) {
        for (int s = 0; s < NUM_CH_SLOTS; s++) {
            const SlotDspUnit *u = &st->ch_slots[c][s];
            PersistentSlotParam *dst = &ps.slots[c][s];
            dst->active = u->active;
            dst->bypass = u->bypass;
            dst->fx_class = u->fx_class;
            memcpy(dst->uri, u->uri, sizeof(dst->uri));
            memcpy(dst->name, u->name, sizeof(dst->name));
            dst->p_drive = u->p_drive; dst->p_tone = u->p_tone;
            dst->p_bass = u->p_bass; dst->p_mid = u->p_mid; dst->p_treble = u->p_treble;
            dst->p_level = u->p_level; dst->p_mix = u->p_mix;
            dst->p_time = u->p_time; dst->p_feedback = u->p_feedback;
            dst->p_rate = u->p_rate; dst->p_depth = u->p_depth;
            dst->sw_bright = u->sw_bright; dst->sw_mode = u->sw_mode;
            memcpy(dst->file_path, u->file_path, sizeof(dst->file_path));
            memcpy(dst->file_label, u->file_label, sizeof(dst->file_label));
            dst->model_preset_id = u->model_preset_id;
        }
    }

    memcpy(ps.gate_enabled,           st->gate_enabled,           sizeof(ps.gate_enabled));
    memcpy(ps.gate_thresh_lin,        st->gate_thresh_lin,        sizeof(ps.gate_thresh_lin));
    memcpy(ps.gate_floor_lin,         st->gate_floor_lin,         sizeof(ps.gate_floor_lin));
    memcpy(ps.denoise_thresh_db,      st->denoise_thresh_db,      sizeof(ps.denoise_thresh_db));
    memcpy(ps.denoise_sensitivity_db, st->denoise_sensitivity_db, sizeof(ps.denoise_sensitivity_db));
    memcpy(ps.denoise_reduction_pct,  st->denoise_reduction_pct,  sizeof(ps.denoise_reduction_pct));
    memcpy(ps.denoise_low_cut,        st->denoise_low_cut,        sizeof(ps.denoise_low_cut));
    memcpy(ps.denoise_listen,         st->denoise_listen,         sizeof(ps.denoise_listen));

    memcpy(ps.ch_eq,      st->ch_eq,      sizeof(ps.ch_eq));
    memcpy(ps.mon_eq,     st->mon_eq,     sizeof(ps.mon_eq));
    memcpy(ps.send_eq,    st->send_eq,    sizeof(ps.send_eq));
    memcpy(ps.ret_eq,     st->ret_eq,     sizeof(ps.ret_eq));
    ps.master_eq = st->master_eq;

    memcpy(ps.ch_afd,     st->ch_afd,     sizeof(ps.ch_afd));
    memcpy(ps.mon_afd,    st->mon_afd,    sizeof(ps.mon_afd));
    ps.master_afd = st->master_afd;

    memcpy(ps.ch_dyn,     st->ch_dyn,     sizeof(ps.ch_dyn));
    memcpy(ps.mon_dyn,    st->mon_dyn,    sizeof(ps.mon_dyn));
    memcpy(ps.mon_dyn2,   st->mon_dyn2,   sizeof(ps.mon_dyn2));
    memcpy(ps.send_dyn,   st->send_dyn,   sizeof(ps.send_dyn));
    memcpy(ps.ret_dyn,    st->ret_dyn,    sizeof(ps.ret_dyn));
    ps.master_dyn  = st->master_dyn;
    ps.master_dyn2 = st->master_dyn2;

    memcpy(ps.deesser_enabled,     st->deesser_enabled,     sizeof(ps.deesser_enabled));
    memcpy(ps.deesser_thresh_lin,  st->deesser_thresh_lin,  sizeof(ps.deesser_thresh_lin));
    memcpy(ps.deesser_thresh1_db,  st->deesser_thresh1_db,  sizeof(ps.deesser_thresh1_db));
    memcpy(ps.deesser_freq1,       st->deesser_freq1,       sizeof(ps.deesser_freq1));
    memcpy(ps.deesser_q1,          st->deesser_q1,          sizeof(ps.deesser_q1));
    memcpy(ps.deesser_max1_db,     st->deesser_max1_db,     sizeof(ps.deesser_max1_db));
    memcpy(ps.deesser_thresh2_lin, st->deesser_thresh2_lin, sizeof(ps.deesser_thresh2_lin));
    memcpy(ps.deesser_thresh2_db,  st->deesser_thresh2_db,  sizeof(ps.deesser_thresh2_db));
    memcpy(ps.deesser_freq2,       st->deesser_freq2,       sizeof(ps.deesser_freq2));
    memcpy(ps.deesser_q2,          st->deesser_q2,          sizeof(ps.deesser_q2));
    memcpy(ps.deesser_max2_db,     st->deesser_max2_db,     sizeof(ps.deesser_max2_db));
    memcpy(ps.deesser_band2_en,    st->deesser_band2_en,    sizeof(ps.deesser_band2_en));
    memcpy(ps.deesser_rel_coeff,   st->deesser_rel_coeff,   sizeof(ps.deesser_rel_coeff));
    memcpy(ps.deesser_wide,        st->deesser_wide,        sizeof(ps.deesser_wide));
    memcpy(ps.deesser_listen,      st->deesser_listen,      sizeof(ps.deesser_listen));

    memcpy(ps.ch_fader_gain,  st->ch_fader_gain,  sizeof(ps.ch_fader_gain));
    memcpy(ps.ch_pan,         st->ch_pan,         sizeof(ps.ch_pan));
    memcpy(ps.ch_mute,        st->ch_mute,        sizeof(ps.ch_mute));
    memcpy(ps.ch_solo,        st->ch_solo,        sizeof(ps.ch_solo));
    memcpy(ps.ch_tuner_mute,  st->ch_tuner_mute,  sizeof(ps.ch_tuner_mute));
    memcpy(ps.sof_mon_gain,   st->sof_mon_gain,   sizeof(ps.sof_mon_gain));
    memcpy(ps.sof_fx_gain,    st->sof_fx_gain,    sizeof(ps.sof_fx_gain));
    memcpy(ps.mon_fader_gain, st->mon_fader_gain, sizeof(ps.mon_fader_gain));
    memcpy(ps.mon_mute,       st->mon_mute,       sizeof(ps.mon_mute));
    memcpy(ps.mon_solo,       st->mon_solo,       sizeof(ps.mon_solo));
    memcpy(ps.send_fader_gain,st->send_fader_gain,sizeof(ps.send_fader_gain));
    memcpy(ps.send_mute,      st->send_mute,      sizeof(ps.send_mute));
    memcpy(ps.send_solo,      st->send_solo,      sizeof(ps.send_solo));
    memcpy(ps.ret_fader_gain, st->ret_fader_gain, sizeof(ps.ret_fader_gain));
    memcpy(ps.ret_pan,        st->ret_pan,        sizeof(ps.ret_pan));
    memcpy(ps.ret_mute,       st->ret_mute,       sizeof(ps.ret_mute));
    memcpy(ps.ret_solo,       st->ret_solo,       sizeof(ps.ret_solo));
    ps.master_fader_gain = st->master_fader_gain;
    ps.master_mute       = st->master_mute;

    ps.summing_enabled      = st->summing_enabled;
    ps.summing_console      = st->summing_console;
    ps.summing_drive_db     = st->summing_drive_db;
    ps.summing_warmth_db    = st->summing_warmth_db;
    ps.summing_crosstalk_db = st->summing_crosstalk_db;

    for (int u = 0; u < 2; u++) {
        ps.rev[u].decay = st->rev[u].decay; ps.rev[u].size = st->rev[u].size;
        ps.rev[u].damp = st->rev[u].damp; ps.rev[u].predelay = st->rev[u].predelay;
        ps.rev[u].diffuse = st->rev[u].diffuse; ps.rev[u].ir_mix = st->rev[u].ir_mix;
        ps.rev[u].stereo = st->rev[u].stereo; ps.rev[u].dry = st->rev[u].dry; ps.rev[u].wet = st->rev[u].wet;
        ps.rev[u].in_lowcut = st->rev[u].in_lowcut; ps.rev[u].in_highcut = st->rev[u].in_highcut;
        ps.rev[u].out_lowcut = st->rev[u].out_lowcut; ps.rev[u].out_highcut = st->rev[u].out_highcut;
        ps.rev[u].mod_rate = st->rev[u].mod_rate; ps.rev[u].mod_amount = st->rev[u].mod_amount;
        ps.rev[u].ducking = st->rev[u].ducking; ps.rev[u].lofi_bits = st->rev[u].lofi_bits; ps.rev[u].lofi_sr = st->rev[u].lofi_sr;

        ps.dly[u].time_ms = st->dly[u].time_ms; ps.dly[u].feedback = st->dly[u].feedback;
        ps.dly[u].diffuse = st->dly[u].diffuse; ps.dly[u].dry = st->dly[u].dry; ps.dly[u].wet = st->dly[u].wet;
        ps.dly[u].ping_pong = st->dly[u].ping_pong;
        ps.dly[u].in_lowcut = st->dly[u].in_lowcut; ps.dly[u].in_highcut = st->dly[u].in_highcut;
        ps.dly[u].out_lowcut = st->dly[u].out_lowcut; ps.dly[u].out_highcut = st->dly[u].out_highcut;
        ps.dly[u].drive = st->dly[u].drive; ps.dly[u].flutter = st->dly[u].flutter;
        ps.dly[u].mod_rate = st->dly[u].mod_rate; ps.dly[u].mod_depth = st->dly[u].mod_depth;
        ps.dly[u].ducking = st->dly[u].ducking; ps.dly[u].stereo = st->dly[u].stereo; ps.dly[u].spread = st->dly[u].spread;
    }

    ps.tape_ips         = st->tape_ips;
    ps.tape_emu_enabled = st->tape_emu_enabled;
    ps.tape_print_wav   = st->tape_print_wav;
    ps.tape_master_vol  = st->tape_master_vol;
    memcpy(ps.tape_mon_send,       st->tape_mon_send,       sizeof(ps.tape_mon_send));
    memcpy(ps.tape_base_dir,       st->tape_base_dir,       sizeof(ps.tape_base_dir));
    ps.tape_dir_manual  = st->tape_dir_manual;
    memcpy(ps.tape_song_name,      st->tape_song_name,      sizeof(ps.tape_song_name));
    ps.tape_num_tracks  = st->tape_num_tracks;
    memcpy(ps.tape_trk_src,        st->tape_trk_src,        sizeof(ps.tape_trk_src));
    memcpy(ps.tape_trk_is_direct,  st->tape_trk_is_direct,  sizeof(ps.tape_trk_is_direct));
    memcpy(ps.tape_trk_vol,        st->tape_trk_vol,        sizeof(ps.tape_trk_vol));
    memcpy(ps.tape_trk_pan,        st->tape_trk_pan,        sizeof(ps.tape_trk_pan));
    memcpy(ps.tape_trk_drive_db,   st->tape_trk_drive_db,   sizeof(ps.tape_trk_drive_db));
    memcpy(ps.tape_trk_mute,       st->tape_trk_mute,       sizeof(ps.tape_trk_mute));
    memcpy(ps.tape_trk_solo,       st->tape_trk_solo,       sizeof(ps.tape_trk_solo));
    memcpy(ps.tape_trk_rec,        st->tape_trk_rec,        sizeof(ps.tape_trk_rec));
    ps.solo_cue_to_main = st->solo_cue_to_main;
    memcpy(ps.ch_input_patch,      st->ch_input_patch,      sizeof(ps.ch_input_patch));
    memcpy(ps.ui_snapshot,         g_latest_ui_snapshot,    sizeof(ps.ui_snapshot));

    mkdir_recursive("/var/modep");
    FILE *fp = fopen(LIVE_STATE_TMP, "wb");
    if (fp) {
        size_t nw = fwrite(&ps, sizeof(LivePersistentState), 1, fp);
        fflush(fp);
        fsync(fileno(fp));
        fclose(fp);
        if (nw == 1) {
            rename(LIVE_STATE_TMP, LIVE_STATE_PATH);
        }
    }

    /* Also persist exact LV2 symbol->value port tables per slot so native .so plugins restore 100% of their knobs & switches */
    FILE *lfp = fopen("/var/modep/last_live_lv2_slot_params.bin.tmp", "wb");
    if (lfp) {
        uint32_t lmagic = 0x4C563250u; /* 'LV2P' */
        fwrite(&lmagic, sizeof(uint32_t), 1, lfp);
        for (int c = 0; c < NUM_CHANNELS; c++) {
            for (int s = 0; s < NUM_CH_SLOTS; s++) {
                const SlotDspUnit *u = &st->ch_slots[c][s];
                fwrite(&u->num_saved_params, sizeof(int), 1, lfp);
                fwrite(u->saved_param_syms, sizeof(u->saved_param_syms), 1, lfp);
                fwrite(u->saved_param_vals, sizeof(u->saved_param_vals), 1, lfp);
            }
        }
        fflush(lfp);
        fsync(fileno(lfp));
        fclose(lfp);
        rename("/var/modep/last_live_lv2_slot_params.bin.tmp", "/var/modep/last_live_lv2_slot_params.bin");
    }
}

static int load_persistent_mixer_state(MeterState *st) {
    FILE *fp = fopen(LIVE_STATE_PATH, "rb");
    if (!fp) return 0;
    uint32_t header[2] = {0, 0};
    if (fread(header, sizeof(uint32_t), 2, fp) != 2 || header[0] != LIVE_STATE_MAGIC) {
        fclose(fp);
        return 0;
    }
    rewind(fp);

    static LivePersistentState ps;
    memset(&ps, 0, sizeof(ps));

    if (header[1] == LIVE_STATE_VERSION) {
        size_t nr = fread(&ps, sizeof(LivePersistentState), 1, fp);
        fclose(fp);
        if (nr != 1) return 0;
    } else if (header[1] == 5u) {
        static LivePersistentStateV5 v5;
        size_t nr = fread(&v5, sizeof(LivePersistentStateV5), 1, fp);
        fclose(fp);
        if (nr != 1) return 0;
        ps.magic = v5.magic;
        ps.version = 6u;
        memcpy(ps.preamp_enabled, v5.preamp_enabled, sizeof(ps.preamp_enabled));
        memcpy(ps.preamp_drive_db, v5.preamp_drive_db, sizeof(ps.preamp_drive_db));
        memcpy(ps.preamp_drive_lin, v5.preamp_drive_lin, sizeof(ps.preamp_drive_lin));
        memcpy(ps.preamp_trim_db, v5.preamp_trim_db, sizeof(ps.preamp_trim_db));
        memcpy(ps.preamp_trim_lin, v5.preamp_trim_lin, sizeof(ps.preamp_trim_lin));
        memcpy(ps.preamp_clip_hard, v5.preamp_clip_hard, sizeof(ps.preamp_clip_hard));
        memcpy(ps.ch_pregain_lin, v5.ch_pregain_lin, sizeof(ps.ch_pregain_lin));
        memcpy(ps.ch_phase_inv, v5.ch_phase_inv, sizeof(ps.ch_phase_inv));
        memcpy(ps.ch_hpf80, v5.ch_hpf80, sizeof(ps.ch_hpf80));
        memcpy(ps.slot_first, v5.slot_first, sizeof(ps.slot_first));
        memcpy(ps.modgui_first, v5.modgui_first, sizeof(ps.modgui_first));
        for (int c = 0; c < NUM_CHANNELS; c++) {
            ps.ch_insert_mode[c] = 1; /* Default to 4 Slots Only */
            for (int s = 0; s < NUM_CH_SLOTS; s++) {
                ps.slots[c][s] = v5.slots[c][s];
            }
        }
        memcpy(ps.gate_enabled, v5.gate_enabled, sizeof(ps.gate_enabled));
        memcpy(ps.gate_thresh_lin, v5.gate_thresh_lin, sizeof(ps.gate_thresh_lin));
        memcpy(ps.gate_floor_lin, v5.gate_floor_lin, sizeof(ps.gate_floor_lin));
        memcpy(ps.denoise_thresh_db, v5.denoise_thresh_db, sizeof(ps.denoise_thresh_db));
        memcpy(ps.denoise_sensitivity_db, v5.denoise_sensitivity_db, sizeof(ps.denoise_sensitivity_db));
        memcpy(ps.denoise_reduction_pct, v5.denoise_reduction_pct, sizeof(ps.denoise_reduction_pct));
        memcpy(ps.denoise_low_cut, v5.denoise_low_cut, sizeof(ps.denoise_low_cut));
        memcpy(ps.denoise_listen, v5.denoise_listen, sizeof(ps.denoise_listen));
        memcpy(ps.ch_eq, v5.ch_eq, sizeof(ps.ch_eq));
        memcpy(ps.mon_eq, v5.mon_eq, sizeof(ps.mon_eq));
        memcpy(ps.send_eq, v5.send_eq, sizeof(ps.send_eq));
        memcpy(ps.ret_eq, v5.ret_eq, sizeof(ps.ret_eq));
        ps.master_eq = v5.master_eq;
        memcpy(ps.ch_afd, v5.ch_afd, sizeof(ps.ch_afd));
        memcpy(ps.mon_afd, v5.mon_afd, sizeof(ps.mon_afd));
        ps.master_afd = v5.master_afd;
        memcpy(ps.ch_dyn, v5.ch_dyn, sizeof(ps.ch_dyn));
        memcpy(ps.mon_dyn, v5.mon_dyn, sizeof(ps.mon_dyn));
        memcpy(ps.mon_dyn2, v5.mon_dyn2, sizeof(ps.mon_dyn2));
        memcpy(ps.send_dyn, v5.send_dyn, sizeof(ps.send_dyn));
        memcpy(ps.ret_dyn, v5.ret_dyn, sizeof(ps.ret_dyn));
        ps.master_dyn = v5.master_dyn;
        ps.master_dyn2 = v5.master_dyn2;
        memcpy(ps.deesser_enabled, v5.deesser_enabled, sizeof(ps.deesser_enabled));
        memcpy(ps.deesser_thresh_lin, v5.deesser_thresh_lin, sizeof(ps.deesser_thresh_lin));
        memcpy(ps.deesser_thresh1_db, v5.deesser_thresh1_db, sizeof(ps.deesser_thresh1_db));
        memcpy(ps.deesser_freq1, v5.deesser_freq1, sizeof(ps.deesser_freq1));
        memcpy(ps.deesser_q1, v5.deesser_q1, sizeof(ps.deesser_q1));
        memcpy(ps.deesser_max1_db, v5.deesser_max1_db, sizeof(ps.deesser_max1_db));
        memcpy(ps.deesser_thresh2_lin, v5.deesser_thresh2_lin, sizeof(ps.deesser_thresh2_lin));
        memcpy(ps.deesser_thresh2_db, v5.deesser_thresh2_db, sizeof(ps.deesser_thresh2_db));
        memcpy(ps.deesser_freq2, v5.deesser_freq2, sizeof(ps.deesser_freq2));
        memcpy(ps.deesser_q2, v5.deesser_q2, sizeof(ps.deesser_q2));
        memcpy(ps.deesser_max2_db, v5.deesser_max2_db, sizeof(ps.deesser_max2_db));
        memcpy(ps.deesser_band2_en, v5.deesser_band2_en, sizeof(ps.deesser_band2_en));
        memcpy(ps.deesser_rel_coeff, v5.deesser_rel_coeff, sizeof(ps.deesser_rel_coeff));
        memcpy(ps.deesser_wide, v5.deesser_wide, sizeof(ps.deesser_wide));
        memcpy(ps.deesser_listen, v5.deesser_listen, sizeof(ps.deesser_listen));
        memcpy(ps.ch_fader_gain, v5.ch_fader_gain, sizeof(ps.ch_fader_gain));
        memcpy(ps.ch_pan, v5.ch_pan, sizeof(ps.ch_pan));
        memcpy(ps.ch_mute, v5.ch_mute, sizeof(ps.ch_mute));
        memcpy(ps.ch_solo, v5.ch_solo, sizeof(ps.ch_solo));
        memcpy(ps.ch_tuner_mute, v5.ch_tuner_mute, sizeof(ps.ch_tuner_mute));
        memcpy(ps.sof_mon_gain, v5.sof_mon_gain, sizeof(ps.sof_mon_gain));
        memcpy(ps.sof_fx_gain, v5.sof_fx_gain, sizeof(ps.sof_fx_gain));
        memcpy(ps.mon_fader_gain, v5.mon_fader_gain, sizeof(ps.mon_fader_gain));
        memcpy(ps.mon_mute, v5.mon_mute, sizeof(ps.mon_mute));
        memcpy(ps.mon_solo, v5.mon_solo, sizeof(ps.mon_solo));
        memcpy(ps.send_fader_gain, v5.send_fader_gain, sizeof(ps.send_fader_gain));
        memcpy(ps.send_mute, v5.send_mute, sizeof(ps.send_mute));
        memcpy(ps.send_solo, v5.send_solo, sizeof(ps.send_solo));
        memcpy(ps.ret_fader_gain, v5.ret_fader_gain, sizeof(ps.ret_fader_gain));
        memcpy(ps.ret_pan, v5.ret_pan, sizeof(ps.ret_pan));
        memcpy(ps.ret_mute, v5.ret_mute, sizeof(ps.ret_mute));
        memcpy(ps.ret_solo, v5.ret_solo, sizeof(ps.ret_solo));
        ps.master_fader_gain = v5.master_fader_gain;
        ps.master_mute = v5.master_mute;
        ps.summing_enabled = v5.summing_enabled;
        ps.summing_console = v5.summing_console;
        ps.summing_drive_db = v5.summing_drive_db;
        ps.summing_warmth_db = v5.summing_warmth_db;
        ps.summing_crosstalk_db = v5.summing_crosstalk_db;
        memcpy(ps.rev, v5.rev, sizeof(ps.rev));
        memcpy(ps.dly, v5.dly, sizeof(ps.dly));
        ps.tape_ips = v5.tape_ips;
        ps.tape_emu_enabled = v5.tape_emu_enabled;
        ps.tape_print_wav = v5.tape_print_wav;
        ps.tape_master_vol = v5.tape_master_vol;
        memcpy(ps.tape_mon_send, v5.tape_mon_send, sizeof(ps.tape_mon_send));
        memcpy(ps.tape_base_dir, v5.tape_base_dir, sizeof(ps.tape_base_dir));
        ps.tape_dir_manual = v5.tape_dir_manual;
        memcpy(ps.tape_song_name, v5.tape_song_name, sizeof(ps.tape_song_name));
        ps.tape_num_tracks = v5.tape_num_tracks;
        memcpy(ps.tape_trk_src, v5.tape_trk_src, sizeof(ps.tape_trk_src));
        memcpy(ps.tape_trk_is_direct, v5.tape_trk_is_direct, sizeof(ps.tape_trk_is_direct));
        memcpy(ps.tape_trk_vol, v5.tape_trk_vol, sizeof(ps.tape_trk_vol));
        memcpy(ps.tape_trk_pan, v5.tape_trk_pan, sizeof(ps.tape_trk_pan));
        memcpy(ps.tape_trk_drive_db, v5.tape_trk_drive_db, sizeof(ps.tape_trk_drive_db));
        memcpy(ps.tape_trk_mute, v5.tape_trk_mute, sizeof(ps.tape_trk_mute));
        memcpy(ps.tape_trk_solo, v5.tape_trk_solo, sizeof(ps.tape_trk_solo));
        memcpy(ps.tape_trk_rec, v5.tape_trk_rec, sizeof(ps.tape_trk_rec));
        ps.solo_cue_to_main = v5.solo_cue_to_main;
        memcpy(ps.ch_input_patch, v5.ch_input_patch, sizeof(ps.ch_input_patch));
        memcpy(ps.ui_snapshot, v5.ui_snapshot, sizeof(ps.ui_snapshot));
    } else if (header[1] == 4u) {
        static LivePersistentStateV4 v4;
        size_t nr = fread(&v4, sizeof(LivePersistentStateV4), 1, fp);
        fclose(fp);
        if (nr != 1) return 0;
        ps.magic = v4.magic;
        ps.version = 6u;
        memcpy(ps.preamp_enabled, v4.preamp_enabled, sizeof(ps.preamp_enabled));
        memcpy(ps.preamp_drive_db, v4.preamp_drive_db, sizeof(ps.preamp_drive_db));
        memcpy(ps.preamp_drive_lin, v4.preamp_drive_lin, sizeof(ps.preamp_drive_lin));
        memcpy(ps.preamp_trim_db, v4.preamp_trim_db, sizeof(ps.preamp_trim_db));
        memcpy(ps.preamp_trim_lin, v4.preamp_trim_lin, sizeof(ps.preamp_trim_lin));
        memcpy(ps.preamp_clip_hard, v4.preamp_clip_hard, sizeof(ps.preamp_clip_hard));
        memcpy(ps.ch_pregain_lin, v4.ch_pregain_lin, sizeof(ps.ch_pregain_lin));
        memcpy(ps.ch_phase_inv, v4.ch_phase_inv, sizeof(ps.ch_phase_inv));
        memcpy(ps.ch_hpf80, v4.ch_hpf80, sizeof(ps.ch_hpf80));
        memcpy(ps.slot_first, v4.slot_first, sizeof(ps.slot_first));
        memcpy(ps.modgui_first, v4.modgui_first, sizeof(ps.modgui_first));
        for (int c = 0; c < NUM_CHANNELS; c++) {
            ps.ch_insert_mode[c] = 1; /* Default to 4 Slots Only */
            for (int s = 0; s < NUM_CH_SLOTS; s++) {
                ps.slots[c][s].active = v4.slots[c][s].active;
                ps.slots[c][s].bypass = v4.slots[c][s].bypass;
                ps.slots[c][s].fx_class = v4.slots[c][s].fx_class;
                memcpy(ps.slots[c][s].uri, v4.slots[c][s].uri, sizeof(ps.slots[c][s].uri));
                memcpy(ps.slots[c][s].name, v4.slots[c][s].name, sizeof(ps.slots[c][s].name));
                ps.slots[c][s].p_drive = v4.slots[c][s].p_drive;
                ps.slots[c][s].p_tone = v4.slots[c][s].p_tone;
                ps.slots[c][s].p_bass = v4.slots[c][s].p_bass;
                ps.slots[c][s].p_mid = v4.slots[c][s].p_mid;
                ps.slots[c][s].p_treble = v4.slots[c][s].p_treble;
                ps.slots[c][s].p_level = v4.slots[c][s].p_level;
                ps.slots[c][s].p_mix = v4.slots[c][s].p_mix;
                ps.slots[c][s].p_time = v4.slots[c][s].p_time;
                ps.slots[c][s].p_feedback = v4.slots[c][s].p_feedback;
                ps.slots[c][s].p_rate = v4.slots[c][s].p_rate;
                ps.slots[c][s].p_depth = v4.slots[c][s].p_depth;
                ps.slots[c][s].sw_bright = v4.slots[c][s].sw_bright;
                ps.slots[c][s].sw_mode = v4.slots[c][s].sw_mode;
            }
        }
        memcpy(ps.gate_enabled, v4.gate_enabled, sizeof(ps.gate_enabled));
        memcpy(ps.gate_thresh_lin, v4.gate_thresh_lin, sizeof(ps.gate_thresh_lin));
        memcpy(ps.gate_floor_lin, v4.gate_floor_lin, sizeof(ps.gate_floor_lin));
        memcpy(ps.denoise_thresh_db, v4.denoise_thresh_db, sizeof(ps.denoise_thresh_db));
        memcpy(ps.denoise_sensitivity_db, v4.denoise_sensitivity_db, sizeof(ps.denoise_sensitivity_db));
        memcpy(ps.denoise_reduction_pct, v4.denoise_reduction_pct, sizeof(ps.denoise_reduction_pct));
        memcpy(ps.denoise_low_cut, v4.denoise_low_cut, sizeof(ps.denoise_low_cut));
        memcpy(ps.denoise_listen, v4.denoise_listen, sizeof(ps.denoise_listen));
        memcpy(ps.ch_eq, v4.ch_eq, sizeof(ps.ch_eq));
        memcpy(ps.mon_eq, v4.mon_eq, sizeof(ps.mon_eq));
        memcpy(ps.send_eq, v4.send_eq, sizeof(ps.send_eq));
        memcpy(ps.ret_eq, v4.ret_eq, sizeof(ps.ret_eq));
        ps.master_eq = v4.master_eq;
        memcpy(ps.ch_afd, v4.ch_afd, sizeof(ps.ch_afd));
        memcpy(ps.mon_afd, v4.mon_afd, sizeof(ps.mon_afd));
        ps.master_afd = v4.master_afd;
        memcpy(ps.ch_dyn, v4.ch_dyn, sizeof(ps.ch_dyn));
        memcpy(ps.mon_dyn, v4.mon_dyn, sizeof(ps.mon_dyn));
        memcpy(ps.mon_dyn2, v4.mon_dyn2, sizeof(ps.mon_dyn2));
        memcpy(ps.send_dyn, v4.send_dyn, sizeof(ps.send_dyn));
        memcpy(ps.ret_dyn, v4.ret_dyn, sizeof(ps.ret_dyn));
        ps.master_dyn = v4.master_dyn;
        ps.master_dyn2 = v4.master_dyn2;
        memcpy(ps.deesser_enabled, v4.deesser_enabled, sizeof(ps.deesser_enabled));
        memcpy(ps.deesser_thresh_lin, v4.deesser_thresh_lin, sizeof(ps.deesser_thresh_lin));
        memcpy(ps.deesser_thresh1_db, v4.deesser_thresh1_db, sizeof(ps.deesser_thresh1_db));
        memcpy(ps.deesser_freq1, v4.deesser_freq1, sizeof(ps.deesser_freq1));
        memcpy(ps.deesser_q1, v4.deesser_q1, sizeof(ps.deesser_q1));
        memcpy(ps.deesser_max1_db, v4.deesser_max1_db, sizeof(ps.deesser_max1_db));
        memcpy(ps.deesser_thresh2_lin, v4.deesser_thresh2_lin, sizeof(ps.deesser_thresh2_lin));
        memcpy(ps.deesser_thresh2_db, v4.deesser_thresh2_db, sizeof(ps.deesser_thresh2_db));
        memcpy(ps.deesser_freq2, v4.deesser_freq2, sizeof(ps.deesser_freq2));
        memcpy(ps.deesser_q2, v4.deesser_q2, sizeof(ps.deesser_q2));
        memcpy(ps.deesser_max2_db, v4.deesser_max2_db, sizeof(ps.deesser_max2_db));
        memcpy(ps.deesser_band2_en, v4.deesser_band2_en, sizeof(ps.deesser_band2_en));
        memcpy(ps.deesser_rel_coeff, v4.deesser_rel_coeff, sizeof(ps.deesser_rel_coeff));
        memcpy(ps.deesser_wide, v4.deesser_wide, sizeof(ps.deesser_wide));
        memcpy(ps.deesser_listen, v4.deesser_listen, sizeof(ps.deesser_listen));
        memcpy(ps.ch_fader_gain, v4.ch_fader_gain, sizeof(ps.ch_fader_gain));
        memcpy(ps.ch_pan, v4.ch_pan, sizeof(ps.ch_pan));
        memcpy(ps.ch_mute, v4.ch_mute, sizeof(ps.ch_mute));
        memcpy(ps.ch_solo, v4.ch_solo, sizeof(ps.ch_solo));
        memcpy(ps.ch_tuner_mute, v4.ch_tuner_mute, sizeof(ps.ch_tuner_mute));
        memcpy(ps.sof_mon_gain, v4.sof_mon_gain, sizeof(ps.sof_mon_gain));
        memcpy(ps.sof_fx_gain, v4.sof_fx_gain, sizeof(ps.sof_fx_gain));
        memcpy(ps.mon_fader_gain, v4.mon_fader_gain, sizeof(ps.mon_fader_gain));
        memcpy(ps.mon_mute, v4.mon_mute, sizeof(ps.mon_mute));
        memcpy(ps.mon_solo, v4.mon_solo, sizeof(ps.mon_solo));
        memcpy(ps.send_fader_gain, v4.send_fader_gain, sizeof(ps.send_fader_gain));
        memcpy(ps.send_mute, v4.send_mute, sizeof(ps.send_mute));
        memcpy(ps.send_solo, v4.send_solo, sizeof(ps.send_solo));
        memcpy(ps.ret_fader_gain, v4.ret_fader_gain, sizeof(ps.ret_fader_gain));
        memcpy(ps.ret_pan, v4.ret_pan, sizeof(ps.ret_pan));
        memcpy(ps.ret_mute, v4.ret_mute, sizeof(ps.ret_mute));
        memcpy(ps.ret_solo, v4.ret_solo, sizeof(ps.ret_solo));
        ps.master_fader_gain = v4.master_fader_gain;
        ps.master_mute = v4.master_mute;
        ps.summing_enabled = v4.summing_enabled;
        ps.summing_console = v4.summing_console;
        ps.summing_drive_db = v4.summing_drive_db;
        ps.summing_warmth_db = v4.summing_warmth_db;
        ps.summing_crosstalk_db = v4.summing_crosstalk_db;
        memcpy(ps.rev, v4.rev, sizeof(ps.rev));
        memcpy(ps.dly, v4.dly, sizeof(ps.dly));
        ps.tape_ips = v4.tape_ips;
        ps.tape_emu_enabled = v4.tape_emu_enabled;
        ps.tape_print_wav = v4.tape_print_wav;
        ps.tape_master_vol = v4.tape_master_vol;
        memcpy(ps.tape_mon_send, v4.tape_mon_send, sizeof(ps.tape_mon_send));
        memcpy(ps.tape_base_dir, v4.tape_base_dir, sizeof(ps.tape_base_dir));
        ps.tape_dir_manual = v4.tape_dir_manual;
        memcpy(ps.tape_song_name, v4.tape_song_name, sizeof(ps.tape_song_name));
        ps.tape_num_tracks = v4.tape_num_tracks;
        memcpy(ps.tape_trk_src, v4.tape_trk_src, sizeof(ps.tape_trk_src));
        memcpy(ps.tape_trk_is_direct, v4.tape_trk_is_direct, sizeof(ps.tape_trk_is_direct));
        memcpy(ps.tape_trk_vol, v4.tape_trk_vol, sizeof(ps.tape_trk_vol));
        memcpy(ps.tape_trk_pan, v4.tape_trk_pan, sizeof(ps.tape_trk_pan));
        memcpy(ps.tape_trk_drive_db, v4.tape_trk_drive_db, sizeof(ps.tape_trk_drive_db));
        memcpy(ps.tape_trk_mute, v4.tape_trk_mute, sizeof(ps.tape_trk_mute));
        memcpy(ps.tape_trk_solo, v4.tape_trk_solo, sizeof(ps.tape_trk_solo));
        memcpy(ps.tape_trk_rec, v4.tape_trk_rec, sizeof(ps.tape_trk_rec));
        ps.solo_cue_to_main = v4.solo_cue_to_main;
        memcpy(ps.ch_input_patch, v4.ch_input_patch, sizeof(ps.ch_input_patch));
        memcpy(ps.ui_snapshot, v4.ui_snapshot, sizeof(ps.ui_snapshot));
    } else {
        fclose(fp);
        return 0;
    }
    float fs = (st->sample_rate > 8000.0f) ? st->sample_rate : 48000.0f;

    memcpy(st->preamp_enabled,   ps.preamp_enabled,   sizeof(st->preamp_enabled));
    memcpy(st->preamp_drive_db,  ps.preamp_drive_db,  sizeof(st->preamp_drive_db));
    memcpy(st->preamp_drive_lin, ps.preamp_drive_lin, sizeof(st->preamp_drive_lin));
    memcpy(st->preamp_trim_db,   ps.preamp_trim_db,   sizeof(st->preamp_trim_db));
    memcpy(st->preamp_trim_lin,  ps.preamp_trim_lin,  sizeof(st->preamp_trim_lin));
    memcpy(st->preamp_clip_hard, ps.preamp_clip_hard, sizeof(st->preamp_clip_hard));
    for (int c = 0; c < NUM_CHANNELS; c++) {
        if (fabsf(st->preamp_drive_db[c] - 6.0f) < 0.01f && fabsf(st->preamp_trim_db[c]) < 0.01f) {
            /* Migrate old +6 dB baked-in preamp default to clean 0.0 dB unity */
            st->preamp_drive_db[c] = 0.0f;
            st->preamp_drive_lin[c] = 1.0f;
            st->preamp_auto_comp[c] = 1.0f;
        } else {
            float ref_in = 0.25f * (st->preamp_drive_lin[c] > 0.01f ? st->preamp_drive_lin[c] : 1.0f);
            st->preamp_auto_comp[c] = 1.0f / fmaxf(0.25f, (0.89125f * tanhf(ref_in / 0.89125f)) / 0.25f);
        }
    }
    memcpy(st->ch_pregain_lin,   ps.ch_pregain_lin,   sizeof(st->ch_pregain_lin));
    memcpy(st->ch_phase_inv,     ps.ch_phase_inv,     sizeof(st->ch_phase_inv));
    memcpy(st->ch_hpf80,         ps.ch_hpf80,         sizeof(st->ch_hpf80));
    memcpy(st->slot_first,       ps.slot_first,       sizeof(st->slot_first));
    memcpy(st->modgui_first,     ps.modgui_first,     sizeof(st->modgui_first));
    memcpy(st->ch_insert_mode,   ps.ch_insert_mode,   sizeof(st->ch_insert_mode));

    FILE *lfp = fopen("/var/modep/last_live_lv2_slot_params.bin", "rb");
    int has_lv2_param_file = 0;
    if (lfp) {
        uint32_t lmagic = 0;
        if (fread(&lmagic, sizeof(uint32_t), 1, lfp) == 1 && lmagic == 0x4C563250u) {
            has_lv2_param_file = 1;
        } else {
            fclose(lfp);
            lfp = NULL;
        }
    }

    for (int c = 0; c < NUM_CHANNELS; c++) {
        for (int s = 0; s < NUM_CH_SLOTS; s++) {
            SlotDspUnit *u = &st->ch_slots[c][s];
            const PersistentSlotParam *src = &ps.slots[c][s];
            unload_native_lv2_slot(u);
            memset(u, 0, sizeof(SlotDspUnit));
            u->bypass_port_idx = -1;
            u->active = src->active;
            u->bypass = src->bypass;
            u->fx_class = src->fx_class;
            memcpy(u->uri, src->uri, sizeof(u->uri));
            memcpy(u->name, src->name, sizeof(u->name));
            u->p_drive = src->p_drive; u->p_tone = src->p_tone;
            u->p_bass = src->p_bass; u->p_mid = src->p_mid; u->p_treble = src->p_treble;
            u->p_level = src->p_level; u->p_mix = src->p_mix;
            u->p_time = src->p_time; u->p_feedback = src->p_feedback;
            u->p_rate = src->p_rate; u->p_depth = src->p_depth;
            u->sw_bright = src->sw_bright; u->sw_mode = src->sw_mode;
            memcpy(u->file_path, src->file_path, sizeof(u->file_path));
            memcpy(u->file_label, src->file_label, sizeof(u->file_label));
            u->model_preset_id = src->model_preset_id;
            if (has_lv2_param_file && lfp) {
                int n_sav = 0;
                if (fread(&n_sav, sizeof(int), 1, lfp) == 1 &&
                    fread(u->saved_param_syms, sizeof(u->saved_param_syms), 1, lfp) == 1 &&
                    fread(u->saved_param_vals, sizeof(u->saved_param_vals), 1, lfp) == 1) {
                    if (n_sav >= 0 && n_sav <= MAX_SAVED_SLOT_PARAMS) u->num_saved_params = n_sav;
                }
            }
            if (u->active && u->uri[0] != '\0' && strcmp(u->uri, "thru") != 0) {
                load_native_lv2_slot(u, fs);
                if (u->file_path[0] != '\0' || u->file_label[0] != '\0') {
                    apply_slot_model_voicing(u, u->file_path, u->file_label);
                }
                update_slot_cab_filters(u, fs);
            }
        }
    }
    if (lfp) fclose(lfp);

    memcpy(st->gate_enabled,           ps.gate_enabled,           sizeof(st->gate_enabled));
    memcpy(st->gate_thresh_lin,        ps.gate_thresh_lin,        sizeof(st->gate_thresh_lin));
    memcpy(st->gate_floor_lin,         ps.gate_floor_lin,         sizeof(st->gate_floor_lin));
    memcpy(st->denoise_thresh_db,      ps.denoise_thresh_db,      sizeof(st->denoise_thresh_db));
    memcpy(st->denoise_sensitivity_db, ps.denoise_sensitivity_db, sizeof(st->denoise_sensitivity_db));
    memcpy(st->denoise_reduction_pct,  ps.denoise_reduction_pct,  sizeof(st->denoise_reduction_pct));
    memcpy(st->denoise_low_cut,        ps.denoise_low_cut,        sizeof(st->denoise_low_cut));
    memcpy(st->denoise_listen,         ps.denoise_listen,         sizeof(st->denoise_listen));

    memcpy(st->ch_eq,   ps.ch_eq,   sizeof(st->ch_eq));
    memcpy(st->mon_eq,  ps.mon_eq,  sizeof(st->mon_eq));
    memcpy(st->send_eq, ps.send_eq, sizeof(st->send_eq));
    memcpy(st->ret_eq,  ps.ret_eq,  sizeof(st->ret_eq));
    st->master_eq = ps.master_eq;
    st->master_eq.adaptive_room_mode = 0; /* Keep Master EQ linear unless Room Calibration is run */
    for (int i = 0; i < NUM_CHANNELS; i++) sanitize_loaded_eq_strip(&st->ch_eq[i], fs);
    for (int i = 0; i < NUM_MONITORS; i++) sanitize_loaded_eq_strip(&st->mon_eq[i], fs);
    for (int i = 0; i < NUM_FX; i++) {
        sanitize_loaded_eq_strip(&st->send_eq[i], fs);
        sanitize_loaded_eq_strip(&st->ret_eq[i], fs);
    }
    sanitize_loaded_eq_strip(&st->master_eq, fs);

    memcpy(st->ch_afd,  ps.ch_afd,  sizeof(st->ch_afd));
    memcpy(st->mon_afd, ps.mon_afd, sizeof(st->mon_afd));
    st->master_afd = ps.master_afd;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (slot_range_has_drive_or_amp(st, i, 0, NUM_CH_SLOTS)) {
            st->ch_afd[i].auto_hunting = 0;
            for (int n = 0; n < NUM_AFD_NOTCHES; n++) st->ch_afd[i].notches[n].active = 0;
        }
        sanitize_loaded_afd_bank(&st->ch_afd[i], (double)fs);
    }
    for (int i = 0; i < NUM_MONITORS; i++) sanitize_loaded_afd_bank(&st->mon_afd[i], (double)fs);
    sanitize_loaded_afd_bank(&st->master_afd, (double)fs);

    memcpy(st->ch_dyn,   ps.ch_dyn,   sizeof(st->ch_dyn));
    memcpy(st->mon_dyn,  ps.mon_dyn,  sizeof(st->mon_dyn));
    memcpy(st->mon_dyn2, ps.mon_dyn2, sizeof(st->mon_dyn2));
    memcpy(st->send_dyn, ps.send_dyn, sizeof(st->send_dyn));
    memcpy(st->ret_dyn,  ps.ret_dyn,  sizeof(st->ret_dyn));
    st->master_dyn  = ps.master_dyn;
    st->master_dyn2 = ps.master_dyn2;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (fabsf(st->ch_dyn[i].fet_input_db - 30.0f) < 0.01f && fabsf(st->ch_dyn[i].fet_output_db - 20.0f) < 0.01f) {
            st->ch_dyn[i].fet_input_db = 18.0f; /* Migrate old +7.8 dB default drive to 0 dB unity */
        }
        sanitize_loaded_dyn_slot(&st->ch_dyn[i], fs);
    }
    for (int i = 0; i < NUM_MONITORS; i++) {
        if (fabsf(st->mon_dyn[i].fet_input_db - 30.0f) < 0.01f) st->mon_dyn[i].fet_input_db = 18.0f;
        sanitize_loaded_dyn_slot(&st->mon_dyn[i], fs);
        sanitize_loaded_dyn_slot(&st->mon_dyn2[i], fs);
    }
    for (int i = 0; i < NUM_FX; i++) {
        if (fabsf(st->send_dyn[i].fet_input_db - 30.0f) < 0.01f) st->send_dyn[i].fet_input_db = 18.0f;
        sanitize_loaded_dyn_slot(&st->send_dyn[i], fs);
        sanitize_loaded_dyn_slot(&st->ret_dyn[i], fs);
    }
    if (fabsf(st->master_dyn.lala_peak_red - 42.0f) < 0.01f && fabsf(st->master_dyn.lala_gain - 40.0f) < 0.01f) {
        st->master_dyn.lala_peak_red = 15.0f;
        st->master_dyn.lala_gain = 32.0f;
    }
    sanitize_loaded_dyn_slot(&st->master_dyn, fs);
    sanitize_loaded_dyn_slot(&st->master_dyn2, fs);

    memcpy(st->deesser_enabled,     ps.deesser_enabled,     sizeof(st->deesser_enabled));
    memcpy(st->deesser_thresh_lin,  ps.deesser_thresh_lin,  sizeof(st->deesser_thresh_lin));
    memcpy(st->deesser_thresh1_db,  ps.deesser_thresh1_db,  sizeof(st->deesser_thresh1_db));
    memcpy(st->deesser_freq1,       ps.deesser_freq1,       sizeof(ps.deesser_freq1));
    memcpy(st->deesser_q1,          ps.deesser_q1,          sizeof(st->deesser_q1));
    memcpy(st->deesser_max1_db,     ps.deesser_max1_db,     sizeof(st->deesser_max1_db));
    memcpy(st->deesser_thresh2_lin, ps.deesser_thresh2_lin, sizeof(st->deesser_thresh2_lin));
    memcpy(st->deesser_thresh2_db,  ps.deesser_thresh2_db,  sizeof(st->deesser_thresh2_db));
    memcpy(st->deesser_freq2,       ps.deesser_freq2,       sizeof(st->deesser_freq2));
    memcpy(st->deesser_q2,          ps.deesser_q2,          sizeof(st->deesser_q2));
    memcpy(st->deesser_max2_db,     ps.deesser_max2_db,     sizeof(st->deesser_max2_db));
    memcpy(st->deesser_band2_en,    ps.deesser_band2_en,    sizeof(st->deesser_band2_en));
    memcpy(st->deesser_rel_coeff,   ps.deesser_rel_coeff,   sizeof(st->deesser_rel_coeff));
    memcpy(st->deesser_wide,        ps.deesser_wide,        sizeof(st->deesser_wide));
    memcpy(st->deesser_listen,      ps.deesser_listen,      sizeof(st->deesser_listen));
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (slot_range_has_drive_or_amp(st, i, 0, NUM_CH_SLOTS)) {
            st->deesser_enabled[i] = 0; /* Never muffle guitar amp channels with vocal de-essing */
        }
        update_deesser_svf_band(&st->deess_svf1[i], st->deesser_freq1[i], st->deesser_q1[i], fs);
        update_deesser_svf_band(&st->deess_svf2[i], st->deesser_freq2[i], st->deesser_q2[i], fs);
    }

    memcpy(st->ch_fader_gain,  ps.ch_fader_gain,  sizeof(st->ch_fader_gain));
    memcpy(st->ch_pan,         ps.ch_pan,         sizeof(st->ch_pan));
    memcpy(st->ch_mute,        ps.ch_mute,        sizeof(st->ch_mute));
    memcpy(st->ch_solo,        ps.ch_solo,        sizeof(st->ch_solo));
    memcpy(st->ch_tuner_mute,  ps.ch_tuner_mute,  sizeof(st->ch_tuner_mute));
    memcpy(st->sof_mon_gain,   ps.sof_mon_gain,   sizeof(st->sof_mon_gain));
    memcpy(st->sof_fx_gain,    ps.sof_fx_gain,    sizeof(st->sof_fx_gain));
    memcpy(st->mon_fader_gain, ps.mon_fader_gain, sizeof(st->mon_fader_gain));
    memcpy(st->mon_mute,       ps.mon_mute,       sizeof(st->mon_mute));
    memcpy(st->mon_solo,       ps.mon_solo,       sizeof(st->mon_solo));
    memcpy(st->send_fader_gain,ps.send_fader_gain,sizeof(st->send_fader_gain));
    memcpy(st->send_mute,      ps.send_mute,      sizeof(st->send_mute));
    memcpy(st->send_solo,      ps.send_solo,      sizeof(st->send_solo));
    memcpy(st->ret_fader_gain, ps.ret_fader_gain, sizeof(st->ret_fader_gain));
    memcpy(st->ret_pan,        ps.ret_pan,        sizeof(st->ret_pan));
    memcpy(st->ret_mute,       ps.ret_mute,       sizeof(st->ret_mute));
    memcpy(st->ret_solo,       ps.ret_solo,       sizeof(st->ret_solo));
    st->master_fader_gain = ps.master_fader_gain;
    st->master_mute       = ps.master_mute;

    st->summing_enabled      = ps.summing_enabled;
    st->summing_console      = ps.summing_console;
    st->summing_drive_db     = ps.summing_drive_db;
    st->summing_warmth_db    = ps.summing_warmth_db;
    st->summing_crosstalk_db = ps.summing_crosstalk_db;
    if (fabsf(st->summing_drive_db - 2.5f) < 0.01f && fabsf(st->summing_warmth_db - 3.0f) < 0.01f) {
        st->summing_drive_db = 0.0f;
        st->summing_warmth_db = 0.0f;
    }

    for (int u = 0; u < 2; u++) {
        st->rev[u].decay = ps.rev[u].decay; st->rev[u].size = ps.rev[u].size;
        st->rev[u].damp = ps.rev[u].damp; st->rev[u].predelay = ps.rev[u].predelay;
        st->rev[u].diffuse = ps.rev[u].diffuse; st->rev[u].ir_mix = ps.rev[u].ir_mix;
        st->rev[u].stereo = ps.rev[u].stereo; st->rev[u].dry = ps.rev[u].dry; st->rev[u].wet = ps.rev[u].wet;
        st->rev[u].in_lowcut = ps.rev[u].in_lowcut; st->rev[u].in_highcut = ps.rev[u].in_highcut;
        st->rev[u].out_lowcut = ps.rev[u].out_lowcut; st->rev[u].out_highcut = ps.rev[u].out_highcut;
        st->rev[u].mod_rate = ps.rev[u].mod_rate; st->rev[u].mod_amount = ps.rev[u].mod_amount;
        st->rev[u].ducking = ps.rev[u].ducking; st->rev[u].lofi_bits = ps.rev[u].lofi_bits; st->rev[u].lofi_sr = ps.rev[u].lofi_sr;
        st->rev[u].sm_init = 0; st->rev[u].xf_active = 0;

        st->dly[u].time_ms = ps.dly[u].time_ms; st->dly[u].feedback = ps.dly[u].feedback;
        st->dly[u].diffuse = ps.dly[u].diffuse; st->dly[u].dry = ps.dly[u].dry; st->dly[u].wet = ps.dly[u].wet;
        st->dly[u].ping_pong = ps.dly[u].ping_pong;
        st->dly[u].in_lowcut = ps.dly[u].in_lowcut; st->dly[u].in_highcut = ps.dly[u].in_highcut;
        st->dly[u].out_lowcut = ps.dly[u].out_lowcut; st->dly[u].out_highcut = ps.dly[u].out_highcut;
        st->dly[u].drive = ps.dly[u].drive; st->dly[u].flutter = ps.dly[u].flutter;
        st->dly[u].mod_rate = ps.dly[u].mod_rate; st->dly[u].mod_depth = ps.dly[u].mod_depth;
        st->dly[u].ducking = ps.dly[u].ducking; st->dly[u].stereo = ps.dly[u].stereo; st->dly[u].spread = ps.dly[u].spread;
        st->dly[u].smooth_samp = st->dly[u].time_ms * 0.001f * fs;
        st->dly[u].sm_init = 0; st->dly[u].xf_active = 0;
    }

    st->tape_ips         = (ps.tape_ips == 30) ? 30 : 15;
    st->tape_emu_enabled = ps.tape_emu_enabled;
    st->tape_print_wav   = ps.tape_print_wav;
    st->tape_master_vol  = ps.tape_master_vol;
    memcpy(st->tape_mon_send,      ps.tape_mon_send,      sizeof(st->tape_mon_send));
    if (ps.tape_base_dir[0] == '/') {
        memcpy(st->tape_base_dir,  ps.tape_base_dir,      sizeof(st->tape_base_dir));
    }
    st->tape_dir_manual  = ps.tape_dir_manual;
    if (ps.tape_song_name[0] != '\0') {
        memcpy(st->tape_song_name, ps.tape_song_name,     sizeof(st->tape_song_name));
    }
    if (ps.tape_num_tracks >= 20 && ps.tape_num_tracks <= MAX_TAPE_TRACKS) {
        st->tape_num_tracks = ps.tape_num_tracks;
    }
    memcpy(st->tape_trk_src,       ps.tape_trk_src,       sizeof(st->tape_trk_src));
    memcpy(st->tape_trk_is_direct, ps.tape_trk_is_direct, sizeof(st->tape_trk_is_direct));
    memcpy(st->tape_trk_vol,       ps.tape_trk_vol,       sizeof(st->tape_trk_vol));
    memcpy(st->tape_trk_pan,       ps.tape_trk_pan,       sizeof(st->tape_trk_pan));
    memcpy(st->tape_trk_drive_db,  ps.tape_trk_drive_db,  sizeof(st->tape_trk_drive_db));
    memcpy(st->tape_trk_mute,      ps.tape_trk_mute,      sizeof(st->tape_trk_mute));
    memcpy(st->tape_trk_solo,      ps.tape_trk_solo,      sizeof(st->tape_trk_solo));
    memcpy(st->tape_trk_rec,       ps.tape_trk_rec,       sizeof(st->tape_trk_rec));
    st->solo_cue_to_main = ps.solo_cue_to_main;
    memcpy(st->ch_input_patch,     ps.ch_input_patch,     sizeof(st->ch_input_patch));
    memcpy(g_latest_ui_snapshot,   ps.ui_snapshot,        sizeof(g_latest_ui_snapshot));
    st->tape_seek_requested = 1;
    g_state_loaded_from_disk = 1;
    fprintf(stderr, "[modep-meter-daemon] Restored last live mixer state from %s (song='%s', master=%.3f)\n",
        LIVE_STATE_PATH, st->tape_song_name, st->master_fader_gain);
    return 1;
}

static float get_free_gb(const char *path) {
    struct statvfs vfs;
    if (statvfs(path, &vfs) == 0) {
        double bytes = (double)vfs.f_bavail * (double)vfs.f_frsize;
        return (float)(bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return 0.0f;
}

/* Auto-detect Internal SD Card + any mounted External USB SSDs (/media/patch/, /media/, /mnt/) */
static void scan_recording_drives(MeterState *st) {
    scan_audio_interfaces(st);
    mkdir_recursive("/var/modep/recordings");
    float sd_free = get_free_gb("/var/modep/recordings");
    struct statvfs root_vfs;
    unsigned long root_fsid = 0;
    if (statvfs("/", &root_vfs) == 0) root_fsid = root_vfs.f_fsid;

    char jbuf[512];
    int n = snprintf(jbuf, sizeof(jbuf), "[{\"label\":\"SD Card (/var/modep/recordings)\",\"path\":\"/var/modep/recordings\",\"free\":%.1f}", sd_free);
    char auto_usb_path[192] = {0};

    const char *parents[] = { "/media/patch", "/media", "/mnt" };
    for (int m = 0; m < 3; m++) {
        DIR *d = opendir(parents[m]);
        if (!d) continue;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            if (m == 1 && strcmp(de->d_name, "patch") == 0) continue;
            char mpath[256];
            snprintf(mpath, sizeof(mpath), "%s/%s", parents[m], de->d_name);
            struct stat st_dir;
            if (stat(mpath, &st_dir) == 0 && S_ISDIR(st_dir.st_mode)) {
                struct statvfs mvfs;
                if (statvfs(mpath, &mvfs) == 0 && mvfs.f_blocks > 0) {
                    if (mvfs.f_fsid != root_fsid || strncmp(de->d_name, "usb", 3) == 0 || strncmp(de->d_name, "ssd", 3) == 0 || strncmp(de->d_name, "SSD", 3) == 0) {
                        char rec_dir[256];
                        snprintf(rec_dir, sizeof(rec_dir), "%s/modep_recordings", mpath);
                        float ufree = get_free_gb(mpath);
                        if (n < (int)sizeof(jbuf) - 140) {
                            n += snprintf(jbuf + n, sizeof(jbuf) - n, ",{\"label\":\"USB SSD (%s)\",\"path\":\"%s\",\"free\":%.1f}", de->d_name, rec_dir, ufree);
                        }
                        if (auto_usb_path[0] == '\0') {
                            snprintf(auto_usb_path, sizeof(auto_usb_path), "%s", rec_dir);
                        }
                    }
                }
            }
        }
        closedir(d);
    }
    snprintf(jbuf + n, sizeof(jbuf) - n, "]");
    snprintf(st->tape_drives_json, sizeof(st->tape_drives_json), "%s", jbuf);

    /* Automatically prefer external USB SSD when plugged in unless user manually chose a path */
    if (!st->tape_dir_manual && st->tape_transport != 2) {
        if (auto_usb_path[0] != '\0') {
            mkdir_recursive(auto_usb_path);
            snprintf(st->tape_base_dir, sizeof(st->tape_base_dir), "%s", auto_usb_path);
        } else {
            snprintf(st->tape_base_dir, sizeof(st->tape_base_dir), "/var/modep/recordings");
        }
    }
    mkdir_recursive(st->tape_base_dir);
    st->tape_free_gb = get_free_gb(st->tape_base_dir);
}

static void write_wav_header(FILE *fp, uint32_t frames, uint32_t sr) {
    if (!fp) return;
    fseek(fp, 0, SEEK_SET);
    uint32_t data_bytes = frames * 2u;
    uint32_t riff_size = 36u + data_bytes;
    uint32_t fmt_size = 16u, byte_rate = sr * 2u;
    uint16_t audio_fmt = 1u, num_ch = 1u, block_align = 2u, bits = 16u;
    fwrite("RIFF", 1, 4, fp); fwrite(&riff_size, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
    fwrite(&fmt_size, 4, 1, fp); fwrite(&audio_fmt, 2, 1, fp); fwrite(&num_ch, 2, 1, fp);
    fwrite(&sr, 4, 1, fp); fwrite(&byte_rate, 4, 1, fp); fwrite(&block_align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&data_bytes, 4, 1, fp);
}

static void resolve_track_wav_path(MeterState *st, int t, char *out_path, size_t out_sz, int find_existing) {
    char dir_path[384];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", st->tape_base_dir, st->tape_song_name);
    const char *tag = (t < 8) ? "Ch_Processed" : ((t < 12) ? "FX_Return" : (st->tape_trk_is_direct[t] ? "Ch_DirectPreamp" : "Added_Track"));
    snprintf(out_path, out_sz, "%s/%02d_%s_Src%d.wav", dir_path, t + 1, tag, st->tape_trk_src[t] + 1);
    if (find_existing) {
        struct stat st_f;
        if (stat(out_path, &st_f) == 0 && st_f.st_size > 44) return;
        DIR *d = opendir(dir_path);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.' || strstr(de->d_name, ".wav") == NULL) continue;
                if (atoi(de->d_name) == t + 1) {
                    snprintf(out_path, out_sz, "%s/%s", dir_path, de->d_name);
                    break;
                }
            }
            closedir(d);
        }
    }
}

static void refill_playback_ring(MeterState *st, uint32_t max_chunks) {
    int16_t pcm_buf[2048];
    int open_ntrk = st->tape_num_tracks;
    if (open_ntrk > MAX_TAPE_TRACKS) open_ntrk = MAX_TAPE_TRACKS;
    uint32_t chunks_done = 0;

    while ((st->play_write_pos - st->play_read_pos) <= (DISK_RING_FRAMES - 2048) && chunks_done < max_chunks) {
        uint32_t wpos = st->play_write_pos;
        for (int t = 0; t < open_ntrk; t++) {
            float *ring = st->tape_play_ring[t];
            if (!ring) continue;
            FILE *fp = st->tape_play_fp[t];
            size_t nread = 0;
            if (fp) {
                nread = fread(pcm_buf, sizeof(int16_t), 2048, fp);
            }
            for (size_t k = 0; k < nread; k++) {
                ring[(wpos + (uint32_t)k) & (DISK_RING_FRAMES - 1)] = (float)pcm_buf[k] * (1.0f / 32767.0f);
            }
            for (size_t k = nread; k < 2048; k++) {
                ring[(wpos + (uint32_t)k) & (DISK_RING_FRAMES - 1)] = 0.0f;
            }
        }
        st->play_write_pos = wpos + 2048;
        chunks_done++;
    }
}

static void prepare_playback_streams(MeterState *st) {
    for (int t = 0; t < MAX_TAPE_TRACKS; t++) {
        if (st->tape_play_fp[t]) {
            fclose(st->tape_play_fp[t]);
            st->tape_play_fp[t] = NULL;
        }
    }
    char dir_path[384];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", st->tape_base_dir, st->tape_song_name);
    mkdir_recursive(dir_path);

    long max_disk_frames = 0;
    int max_trk_idx = st->tape_num_tracks;
    DIR *d = opendir(dir_path);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.' || strstr(de->d_name, ".wav") == NULL) continue;
            int trk_num = atoi(de->d_name);
            if (trk_num > max_trk_idx && trk_num <= MAX_TAPE_TRACKS) max_trk_idx = trk_num;
            char fpath[512];
            snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, de->d_name);
            struct stat st_f;
            if (stat(fpath, &st_f) == 0 && st_f.st_size > 44) {
                long f_frames = (long)((st_f.st_size - 44) / 2);
                if (f_frames > max_disk_frames) max_disk_frames = f_frames;
            }
        }
        closedir(d);
    }
    if (max_trk_idx > st->tape_num_tracks && max_trk_idx <= MAX_TAPE_TRACKS) {
        st->tape_num_tracks = max_trk_idx;
    }
    st->tape_max_frames = max_disk_frames;

    long start_frame = st->tape_frame_pos;
    if (start_frame < 0) start_frame = 0;
    int open_ntrk = st->tape_num_tracks;
    if (open_ntrk > MAX_TAPE_TRACKS) open_ntrk = MAX_TAPE_TRACKS;

    for (int t = 0; t < open_ntrk; t++) {
        char fpath[448];
        resolve_track_wav_path(st, t, fpath, sizeof(fpath), 1);
        FILE *fp = fopen(fpath, "rb");
        if (fp) {
            long byte_off = 44L + start_frame * 2L;
            if (fseek(fp, byte_off, SEEK_SET) != 0) {
                fclose(fp);
                fp = NULL;
            }
        }
        st->tape_play_fp[t] = fp;
    }
    st->play_read_pos = 0;
    st->play_write_pos = 0;
    /* Pre-buffer 16 x 2048 = 32,768 frames (~0.68s) into RAM ringbuffer for instant 0ms PLAY/OVERDUB start */
    refill_playback_ring(st, 16);
}

/* Background Direct-to-Disk & Direct-from-Disk Streaming Thread (Unlimited Song/Album Length)
 * - Streams armed tracks continuously to SD/SSD WAV files with periodic header commits (zero 30s limit).
 * - Streams un-armed tracks continuously from disk into `tape_play_ring` for true full-length DAW overdubbing! */
static void* tape_disk_writer_thread(void *arg) {
    MeterState *st = (MeterState*)arg;
    int was_recording = 0;
    int open_ntrk = 0;
    int scan_counter = 0;
    uint32_t last_header_commit = 0;
    int16_t chunk_pcm[512];

    prepare_playback_streams(st);
    st->tape_seek_requested = 0;

    while (1) {
        if (++scan_counter >= 200) {
            scan_counter = 0;
            scan_recording_drives(st);
        }
        int is_rec = (st->tape_transport == 2);
        uint32_t sr = (uint32_t)(st->sample_rate > 8000.0f ? st->sample_rate : 48000.0f);

        if (!is_rec && !was_recording && st->tape_seek_requested) {
            st->tape_seek_requested = 0;
            prepare_playback_streams(st);
        }

        if (is_rec && !was_recording) {
            st->tape_seek_requested = 0;
            /* Pre-buffer un-armed tracks at current tape_frame_pos for full-length overdub monitoring */
            prepare_playback_streams(st);
            open_ntrk = st->tape_num_tracks;
            if (open_ntrk > MAX_TAPE_TRACKS) open_ntrk = MAX_TAPE_TRACKS;
            long start_frame = st->tape_frame_pos;
            if (start_frame < 0) start_frame = 0;
            st->tape_disk_frames_written = (uint32_t)start_frame;
            last_header_commit = st->tape_disk_frames_written;

            for (int t = 0; t < open_ntrk; t++) {
                st->tape_disk_fp[t] = NULL;
                if (!st->tape_trk_rec[t]) continue;
                /* Close playback stream on armed tracks so we write cleanly */
                if (st->tape_play_fp[t]) {
                    fclose(st->tape_play_fp[t]);
                    st->tape_play_fp[t] = NULL;
                }
                char fname[448];
                resolve_track_wav_path(st, t, fname, sizeof(fname), 0);
                FILE *fp = NULL;
                if (start_frame > 0) {
                    fp = fopen(fname, "r+b");
                    if (fp) {
                        if (fseek(fp, 44L + start_frame * 2L, SEEK_SET) != 0) {
                            fclose(fp);
                            fp = NULL;
                        }
                    }
                }
                if (!fp) {
                    fp = fopen(fname, "wb");
                    if (fp) write_wav_header(fp, 0, sr);
                }
                st->tape_disk_fp[t] = fp;
            }
            was_recording = 1;
        }

        if (was_recording) {
            uint32_t wpos = st->ring_write_pos;
            uint32_t rpos = st->ring_read_pos;
            uint32_t avail = wpos - rpos;
            if (avail > DISK_RING_FRAMES) {
                rpos = wpos - DISK_RING_FRAMES;
                avail = DISK_RING_FRAMES;
            }
            while (avail > 0) {
                uint32_t batch = (avail > 512) ? 512 : avail;
                for (int t = 0; t < open_ntrk; t++) {
                    FILE *fp = st->tape_disk_fp[t];
                    if (!fp || !st->tape_disk_ring[t]) continue;
                    for (uint32_t k = 0; k < batch; k++) {
                        float s = st->tape_disk_ring[t][(rpos + k) & (DISK_RING_FRAMES - 1)];
                        if (st->tape_print_wav && !st->tape_trk_is_direct[t]) {
                            s = process_tape_2inch_sample(st, t, s, (float)sr);
                        }
                        if (s > 0.99f) s = 0.99f;
                        if (s < -0.99f) s = -0.99f;
                        chunk_pcm[k] = (int16_t)(s * 32767.0f);
                    }
                    fwrite(chunk_pcm, sizeof(int16_t), batch, fp);
                }
                rpos += batch;
                st->ring_read_pos = rpos;
                st->tape_disk_frames_written += batch;
                if ((long)st->tape_disk_frames_written > st->tape_max_frames) {
                    st->tape_max_frames = (long)st->tape_disk_frames_written;
                }
                avail -= batch;
            }

            /* Keep un-armed overdub playback tracks streaming from disk */
            refill_playback_ring(st, 4);

            /* Periodically commit WAV headers every ~2s so power loss never corrupts a long song take */
            if (st->tape_disk_frames_written - last_header_commit >= 96000) {
                last_header_commit = st->tape_disk_frames_written;
                uint32_t hdr_frames = (uint32_t)((long)st->tape_disk_frames_written > st->tape_max_frames ? st->tape_disk_frames_written : st->tape_max_frames);
                for (int t = 0; t < open_ntrk; t++) {
                    FILE *fp = st->tape_disk_fp[t];
                    if (fp) {
                        long cur_p = ftell(fp);
                        write_wav_header(fp, hdr_frames, sr);
                        if (cur_p >= 44) fseek(fp, cur_p, SEEK_SET);
                        fflush(fp);
                    }
                }
            }

            if (!is_rec) {
                uint32_t final_frames = (uint32_t)((long)st->tape_disk_frames_written > st->tape_max_frames ? st->tape_disk_frames_written : st->tape_max_frames);
                for (int t = 0; t < open_ntrk; t++) {
                    if (st->tape_disk_fp[t]) {
                        write_wav_header(st->tape_disk_fp[t], final_frames, sr);
                        fflush(st->tape_disk_fp[t]);
                        fclose(st->tape_disk_fp[t]);
                        st->tape_disk_fp[t] = NULL;
                    }
                }
                was_recording = 0;
                st->tape_free_gb = get_free_gb(st->tape_base_dir);
                prepare_playback_streams(st);
            }
        } else if (st->tape_transport == 1) {
            /* Continuous Direct-from-Disk Playback Streaming */
            refill_playback_ring(st, 4);
        }
        if (g_state_save_dirty) {
            static long long last_state_save_ms = 0;
            struct timespec ts_now;
            clock_gettime(CLOCK_MONOTONIC, &ts_now);
            long long cur_ms = (long long)ts_now.tv_sec * 1000LL + (long long)(ts_now.tv_nsec / 1000000LL);
            if ((cur_ms - g_last_state_change_ms) >= 120 || (cur_ms - last_state_save_ms) >= 800) {
                g_state_save_dirty = 0;
                last_state_save_ms = cur_ms;
                save_persistent_mixer_state(st);
            }
        }
        usleep(8000);
    }
    return NULL;
}

/* Flush & verify full-length WAV stems on disk (never truncates to a 30s RAM buffer!) */
static void write_song_folder_wav_stems(MeterState *st) {
    mkdir_recursive(st->tape_base_dir);
    char dir_path[384];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", st->tape_base_dir, st->tape_song_name);
    mkdir_recursive(dir_path);
    uint32_t sr = (uint32_t)(st->sample_rate > 8000.0f ? st->sample_rate : 48000.0f);

    DIR *d = opendir(dir_path);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.' || strstr(de->d_name, ".wav") == NULL) continue;
            char fpath[512];
            snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, de->d_name);
            struct stat st_f;
            if (stat(fpath, &st_f) == 0 && st_f.st_size > 44) {
                uint32_t f_frames = (uint32_t)((st_f.st_size - 44) / 2);
                FILE *fp = fopen(fpath, "r+b");
                if (fp) {
                    write_wav_header(fp, f_frames, sr);
                    fclose(fp);
                }
            }
        }
        closedir(d);
    }
    st->tape_free_gb = get_free_gb(st->tape_base_dir);
}

/* Prepare full-length WAV stems from <tape_base_dir>/<song_name>/ for direct-from-disk streaming */
static void load_song_folder_wav_stems(MeterState *st) {
    st->tape_transport = 0;
    st->tape_frame_pos = 0;
    st->tape_seek_requested = 1;
}

/* Broadcast DAW Session & File Browser directory listing to all connected clients */
static void broadcast_tape_browser_state(MeterState *st, const char *browse_path) {
    const char *base = (browse_path && browse_path[0] == '/') ? browse_path : st->tape_base_dir;
    unsigned int slot = g_sync_head % SYNC_RING_SIZE;
    char *out = g_sync_ring[slot];
    int sz = (int)sizeof(g_sync_ring[slot]);
    int n = snprintf(out, sz, "{\"action\":\"tape_browser_data\",\"baseDir\":\"%s\",\"currentSong\":\"%s\",\"freeGb\":%.1f,\"songs\":[",
        st->tape_base_dir, st->tape_song_name, st->tape_free_gb);

    DIR *bd = opendir(st->tape_base_dir);
    if (bd) {
        int first = 1;
        struct dirent *de;
        while ((de = readdir(bd)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char spath[448];
            snprintf(spath, sizeof(spath), "%s/%s", st->tape_base_dir, de->d_name);
            struct stat st_s;
            if (stat(spath, &st_s) == 0 && S_ISDIR(st_s.st_mode)) {
                int wav_cnt = 0;
                double total_mb = 0.0;
                DIR *sd = opendir(spath);
                if (sd) {
                    struct dirent *fe;
                    while ((fe = readdir(sd)) != NULL) {
                        if (strstr(fe->d_name, ".wav") != NULL) {
                            wav_cnt++;
                            char fpath[512];
                            snprintf(fpath, sizeof(fpath), "%s/%s", spath, fe->d_name);
                            struct stat st_f;
                            if (stat(fpath, &st_f) == 0) total_mb += (double)st_f.st_size / (1024.0 * 1024.0);
                        }
                    }
                    closedir(sd);
                }
                if (n < sz - 240) {
                    n += snprintf(out + n, sz - n, "%s{\"name\":\"%s\",\"tracks\":%d,\"sizeMb\":%.1f}",
                        first ? "" : ",", de->d_name, wav_cnt, total_mb);
                    first = 0;
                }
            }
        }
        closedir(bd);
    }

    n += snprintf(out + n, sz - n, "],\"files\":[");
    char cur_song_dir[448];
    snprintf(cur_song_dir, sizeof(cur_song_dir), "%s/%s", st->tape_base_dir, st->tape_song_name);
    DIR *sd = opendir(cur_song_dir);
    if (sd) {
        int first = 1;
        struct dirent *fe;
        while ((fe = readdir(sd)) != NULL) {
            if (fe->d_name[0] == '.' || strstr(fe->d_name, ".wav") == NULL) continue;
            char fpath[512];
            snprintf(fpath, sizeof(fpath), "%s/%s", cur_song_dir, fe->d_name);
            struct stat st_f;
            double fmb = 0.0;
            if (stat(fpath, &st_f) == 0) fmb = (double)st_f.st_size / (1024.0 * 1024.0);
            if (n < sz - 160) {
                n += snprintf(out + n, sz - n, "%s{\"name\":\"%s\",\"sizeMb\":%.2f}",
                    first ? "" : ",", fe->d_name, fmb);
                first = 0;
            }
        }
        closedir(sd);
    }

    n += snprintf(out + n, sz - n, "],\"browsePath\":\"%s\",\"dirs\":[", base);
    DIR *pd = opendir(base);
    if (pd) {
        int first = 1;
        struct dirent *de;
        while ((de = readdir(pd)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char dpath[448];
            snprintf(dpath, sizeof(dpath), "%s/%s%s", (strcmp(base, "/") == 0 ? "" : base), de->d_name, "");
            struct stat st_d;
            if (stat(dpath, &st_d) == 0 && S_ISDIR(st_d.st_mode)) {
                if (n < sz - 120) {
                    n += snprintf(out + n, sz - n, "%s\"%s\"", first ? "" : ",", de->d_name);
                    first = 0;
                }
            }
        }
        closedir(pd);
    }
    snprintf(out + n, sz - n, "]}");
    g_sync_sender[slot] = NULL;
    g_sync_head++;
    for (int i = 0; i < g_num_wsi; i++) lws_callback_on_writable(g_wsi_list[i]);
}

/* Paul Kellet -18.0 dBFS Pink Noise Generator for Acoustic Room Calibration */
static inline float next_pink_noise_sample(MeterState *st) {
    st->rng_seed = st->rng_seed * 1664525u + 1013904223u;
    float white = ((float)(int32_t)st->rng_seed) * (1.0f / 2147483648.0f);
    st->pink_b0 = 0.99886f * st->pink_b0 + white * 0.0555179f;
    st->pink_b1 = 0.99332f * st->pink_b1 + white * 0.0750759f;
    st->pink_b2 = 0.96900f * st->pink_b2 + white * 0.1538520f;
    st->pink_b3 = 0.86650f * st->pink_b3 + white * 0.3104856f;
    st->pink_b4 = 0.55000f * st->pink_b4 + white * 0.5329522f;
    st->pink_b5 = -0.7616f * st->pink_b5 - white * 0.0168980f;
    float pink = (st->pink_b0 + st->pink_b1 + st->pink_b2 + st->pink_b3 + st->pink_b4 + st->pink_b5 + st->pink_b6 + white * 0.5362f) * 0.04f;
    st->pink_b6 = white * 0.115926f;
    return pink * 0.12589f; /* -18.0 dBFS reference level */
}

static int jack_process(jack_nframes_t nframes, void *arg) {
    MeterState *state = (MeterState*)arg;
    state->jack_heartbeat++;

    jack_default_audio_sample_t *out_l = state->out_l ? (jack_default_audio_sample_t*)jack_port_get_buffer(state->out_l, nframes) : NULL;
    jack_default_audio_sample_t *out_r = state->out_r ? (jack_default_audio_sample_t*)jack_port_get_buffer(state->out_r, nframes) : NULL;
    if (out_l) memset(out_l, 0, sizeof(jack_default_audio_sample_t) * nframes);
    if (out_r) memset(out_r, 0, sizeof(jack_default_audio_sample_t) * nframes);

    jack_default_audio_sample_t *mon_bufs[NUM_MONITORS];
    for (int m = 0; m < NUM_MONITORS; m++) {
        mon_bufs[m] = state->mon_ports[m] ? (jack_default_audio_sample_t*)jack_port_get_buffer(state->mon_ports[m], nframes) : NULL;
        if (mon_bufs[m]) memset(mon_bufs[m], 0, sizeof(jack_default_audio_sample_t) * nframes);
    }

    static float ch_raw_buf[NUM_CHANNELS][2048];
    static float ch_post_buf[NUM_CHANNELS][2048];
    static float fx_ret_buf[NUM_FX][2048];
    static float fx_send_l[NUM_FX][2048];
    static float fx_send_r[NUM_FX][2048];
    jack_nframes_t safe_frames = (nframes <= 2048) ? nframes : 2048;
    for (int fx = 0; fx < NUM_FX; fx++) {
        memset(fx_send_l[fx], 0, sizeof(float) * safe_frames);
        memset(fx_send_r[fx], 0, sizeof(float) * safe_frames);
        memset(fx_ret_buf[fx], 0, sizeof(float) * safe_frames);
    }

    /* Check if any channel has SOLO engaged */
    int any_solo = 0;
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        if (state->ch_solo[ch]) {
            any_solo = 1;
            break;
        }
    }

    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        if (!state->in_ports[ch]) continue;
        jack_default_audio_sample_t *in = (jack_default_audio_sample_t*)jack_port_get_buffer(state->in_ports[ch], nframes);
        jack_default_audio_sample_t *in_r = state->in_ports_r[ch] ? (jack_default_audio_sample_t*)jack_port_get_buffer(state->in_ports_r[ch], nframes) : NULL;
        int is_stereo = state->ch_stereo_active[ch] || (state->in_ports_r[ch] && jack_port_connected(state->in_ports_r[ch]) > 0);
        state->ch_stereo_active[ch] = is_stereo ? 1 : 0;

        float max_val = 0.0f;
        float raw_in_max = 0.0f;
        float block_gr_max = 0.0f;
        int block_clip_status = 0;

        int pre_en = state->preamp_enabled[ch];
        float pre_drive = state->preamp_drive_lin[ch];
        float pre_trim = state->preamp_trim_lin[ch];
        float pre_comp = state->preamp_auto_comp[ch] > 0.01f ? state->preamp_auto_comp[ch] : 1.0f;
        int pre_hard = state->preamp_clip_hard[ch];

        float pregain = state->ch_pregain_lin[ch] * (state->ch_phase_inv[ch] ? -1.0f : 1.0f);
        int use_hpf = state->ch_hpf80[ch];

        float fs_ch = (state->sample_rate > 8000.0f) ? state->sample_rate : 48000.0f;

        int ds_en = state->deesser_enabled[ch];
        int ds_b2_en = state->deesser_band2_en[ch];
        float ds_thresh1 = state->deesser_thresh_lin[ch];
        float ds_thresh2 = state->deesser_thresh2_lin[ch];
        float ds_thresh1_db = state->deesser_thresh1_db[ch];
        float ds_thresh2_db = state->deesser_thresh2_db[ch];
        float ds_max1_db = fmaxf(0.0f, state->deesser_max1_db[ch]);
        float ds_max2_db = fmaxf(0.0f, state->deesser_max2_db[ch]);
        float ds_knee_lin1 = ds_thresh1 * 0.794328f; /* -2.0 dB soft-knee onset below Band 1 threshold */
        float ds_knee_lin2 = ds_thresh2 * 0.794328f; /* -2.0 dB soft-knee onset below Band 2 threshold */
        float ds_rel = state->deesser_rel_coeff[ch];
        int ds_wide = state->deesser_wide[ch];
        int ds_listen = state->deesser_listen[ch];
        float block_ds_gr1 = 0.0f, block_ds_gr2 = 0.0f;
        float block_ds_det1 = 0.0f, block_ds_det2 = 0.0f;
        float cur_ds_gain1 = 1.0f, cur_ds_gain2 = 1.0f;

        int audible = (!state->ch_mute[ch]) && (!state->ch_tuner_mute[ch]) && (!any_solo || state->ch_solo[ch]);
        int is_rta_ch = (state->rta_target_kind == 1 && state->rta_target_ch == ch);

        /* Mono Constant-Power Pan Law vs True Stereo Balance Law */
        float pan = state->ch_pan[ch];
        if (pan < -1.0f) pan = -1.0f;
        if (pan >  1.0f) pan =  1.0f;
        float theta = (pan + 1.0f) * (float)(M_PI * 0.25);
        float f_gain = audible ? state->ch_fader_gain[ch] : 0.0f;
        float gain_l = is_stereo ? (pan <= 0.0f ? f_gain : f_gain * (1.0f - pan)) : (f_gain * cosf(theta) * 1.41421356f);
        float gain_r = is_stereo ? (pan >= 0.0f ? f_gain : f_gain * (1.0f + pan)) : (f_gain * sinf(theta) * 1.41421356f);

        int is_slot_first = state->slot_first[ch];
        int is_mod_first  = state->modgui_first[ch];
        int ins_mode = state->ch_insert_mode[ch]; /* 0=Direct, 1=4 Slots Only, 2=MOD Pedalboard Only (Rule 28) */
        int has_amp_or_drive0 = (ins_mode == 1) ? slot_range_has_drive_or_amp(state, ch, 0, 2) : 0;
        int has_amp_or_drive1 = (ins_mode == 1) ? slot_range_has_drive_or_amp(state, ch, 2, 4) : 0;
        int ch_has_tube_amp = has_amp_or_drive0 || has_amp_or_drive1;
        int ch_has_native_cyber_amp = (ins_mode == 1) ? slot_range_has_native_cyber_amp(state, ch, 0, 4) : 0;

        float blk_l[2048];
        float blk_r[2048];
        float blk_in_l[2048];
        float blk_in_r[2048];
        uint32_t blk_frames = (nframes <= 2048) ? (uint32_t)nframes : 2048;

        for (uint32_t i = 0; i < blk_frames; i++) {
            float raw_l = in[i];
            float raw_r = (is_stereo && in_r) ? in_r[i] : raw_l;
            float raw_abs = fmaxf(fabsf(raw_l), fabsf(raw_r));
            if (raw_abs > raw_in_max) raw_in_max = raw_abs;
            blk_l[i] = raw_l;
            blk_r[i] = raw_r;

            /* Real-Time Fundamental Pitch Detector (35..1100 Hz) for Cyber Tuner / Strobe Tuner (works even while muted!) */
            state->tuner_dc[ch] += 0.002f * (raw_l - state->tuner_dc[ch]);
            float t_ac = raw_l - state->tuner_dc[ch];
            state->tuner_lp[ch] += 0.045f * (t_ac - state->tuner_lp[ch]);
            float t_cur = state->tuner_lp[ch];
            float t_prv = state->tuner_prev[ch];
            state->tuner_zc_samples[ch] += 1.0f;
            if (t_prv < 0.0f && t_cur >= 0.0f && raw_abs > 0.002f) {
                float frac = (-t_prv) / (t_cur - t_prv + 1e-9f);
                float period = state->tuner_zc_samples[ch] - 1.0f + frac;
                state->tuner_zc_samples[ch] = 1.0f - frac;
                if (period > (fs_ch / 1100.0f) && period < (fs_ch / 35.0f)) {
                    if (state->tuner_period[ch] <= 1.0f || fabsf(period - state->tuner_period[ch]) > state->tuner_period[ch] * 0.25f) {
                        state->tuner_period[ch] = period;
                    } else {
                        state->tuner_period[ch] = 0.85f * state->tuner_period[ch] + 0.15f * period;
                    }
                    state->ch_pitch_hz[ch] = fs_ch / state->tuner_period[ch];
                }
            } else if (state->tuner_zc_samples[ch] > (fs_ch * 0.12f)) {
                state->ch_pitch_hz[ch] = 0.0f;
            }
            state->tuner_prev[ch] = t_cur;
        }

        /* STAGE 0: Independent '1ST IN CHAIN' Routing for Insert Slots 1&2 and/or Slots 3&4 (Block LV2 + C Fallback) */
        if (ins_mode == 1) {
            if (is_slot_first) {
                memcpy(blk_in_l, blk_l, blk_frames * sizeof(float));
                memcpy(blk_in_r, blk_r, blk_frames * sizeof(float));
                process_channel_effect_slots_block(state, ch, 0, 2, blk_l, blk_r, is_stereo, blk_frames, fs_ch);
                for (uint32_t i = 0; i < blk_frames; i++) {
                    apply_module_autogain(blk_in_l[i], blk_in_r[i], &blk_l[i], &blk_r[i], is_stereo, &state->stage_env_in[ch][0], &state->stage_env_out[ch][0], &state->stage_gain[ch][0], !has_amp_or_drive0);
                }
            }
            if (is_mod_first) {
                memcpy(blk_in_l, blk_l, blk_frames * sizeof(float));
                memcpy(blk_in_r, blk_r, blk_frames * sizeof(float));
                process_channel_effect_slots_block(state, ch, 2, 4, blk_l, blk_r, is_stereo, blk_frames, fs_ch);
                for (uint32_t i = 0; i < blk_frames; i++) {
                    apply_module_autogain(blk_in_l[i], blk_in_r[i], &blk_l[i], &blk_r[i], is_stereo, &state->stage_env_in[ch][1], &state->stage_env_out[ch][1], &state->stage_gain[ch][1], !has_amp_or_drive1);
                }
            }
        }

        /* STAGE 1 & STAGE 2: Auto-Compensated 1176 Preamp + Digital Pre-Gain, Polarity Invert (Ø) & 80Hz HPF + Direct Tape Tap */
        for (uint32_t i = 0; i < blk_frames; i++) {
            float s_l = blk_l[i];
            float s_r = blk_r[i];

            s_l = pre_en ? process_1176_preamp_sample(s_l, pre_drive, pre_trim, pre_comp, pre_hard, &block_clip_status) : s_l;
            s_r = (is_stereo && pre_en) ? process_1176_preamp_sample(s_r, pre_drive, pre_trim, pre_comp, pre_hard, &block_clip_status) : s_r;
            s_l = soft_ceiling_guard(s_l);
            if (is_stereo) s_r = soft_ceiling_guard(s_r);

            s_l *= pregain;
            if (use_hpf) s_l = process_biquad(&state->hpf_filters[ch], s_l);
            s_l = soft_ceiling_guard(s_l);
            if (is_stereo) {
                s_r *= pregain;
                if (use_hpf) s_r = process_biquad(&state->hpf_filters_r[ch], s_r);
                s_r = soft_ceiling_guard(s_r);
            }

            if (i < safe_frames) {
                ch_raw_buf[ch][i] = is_stereo ? 0.5f * (s_l + s_r) : s_l;
            }
            blk_l[i] = s_l;
            blk_r[i] = s_r;
        }

        /* STAGE 3A: Normal Post-Preamp Insert Slots 1&2 and/or Slots 3&4 (Block LV2 + C Fallback) */
        if (ins_mode == 1) {
            if (!is_slot_first) {
                memcpy(blk_in_l, blk_l, blk_frames * sizeof(float));
                memcpy(blk_in_r, blk_r, blk_frames * sizeof(float));
                process_channel_effect_slots_block(state, ch, 0, 2, blk_l, blk_r, is_stereo, blk_frames, fs_ch);
                for (uint32_t i = 0; i < blk_frames; i++) {
                    apply_module_autogain(blk_in_l[i], blk_in_r[i], &blk_l[i], &blk_r[i], is_stereo, &state->stage_env_in[ch][0], &state->stage_env_out[ch][0], &state->stage_gain[ch][0], !has_amp_or_drive0);
                }
            }
            if (!is_mod_first) {
                memcpy(blk_in_l, blk_l, blk_frames * sizeof(float));
                memcpy(blk_in_r, blk_r, blk_frames * sizeof(float));
                process_channel_effect_slots_block(state, ch, 2, 4, blk_l, blk_r, is_stereo, blk_frames, fs_ch);
                for (uint32_t i = 0; i < blk_frames; i++) {
                    apply_module_autogain(blk_in_l[i], blk_in_r[i], &blk_l[i], &blk_r[i], is_stereo, &state->stage_env_in[ch][1], &state->stage_env_out[ch][1], &state->stage_gain[ch][1], !has_amp_or_drive1);
                }
            }
        }

        for (jack_nframes_t i = 0; i < nframes; i++) {
            float s_l = (i < blk_frames) ? blk_l[i] : in[i];
            float s_r = (i < blk_frames) ? blk_r[i] : ((is_stereo && in_r) ? in_r[i] : s_l);

            /* STAGE 3B: Authentic 10-Band Cyber-Denoiser PRO
             * Note: Native Cyber Amps already include their own internal 6-band dynamic denoiser & gate.
             * Running a second 10-band crossover denoiser on top causes phase smearing unless the user explicitly turned up channel denoise sensitivity or Learn Noise. */
            if (!ch_has_native_cyber_amp || state->denoise_learn_active[ch] || state->denoise_listen[ch] || state->denoise_sensitivity_db[ch] > 0.1f) {
                process_cyber_denoiser_sample(state, ch, &s_l, &s_r, is_stereo, fs_ch);
            }
            if (ch_has_tube_amp) {
                /* Tube amp mode: preserve full dynamic range of native LV2 amp output up to -0.3 dBFS (0.966f) */
                if (fabsf(s_l) > 0.966f) s_l = (s_l > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(s_l) - 0.966f) / 0.033f));
                if (is_stereo && fabsf(s_r) > 0.966f) s_r = (s_r > 0.0f ? 1.0f : -1.0f) * (0.966f + 0.033f * tanhf((fabsf(s_r) - 0.966f) / 0.033f));
            } else {
                s_l = soft_ceiling_guard(s_l);
                if (is_stereo) s_r = soft_ceiling_guard(s_r);
            }

            /* STAGE 4: 8-Band Cytomic SVF Equaliser + 64-Bit Surgical -Infinity dB AFD Bank + Auto-Gain Staging + Live RTA */
            {
                float eq_in_l = s_l, eq_in_r = s_r;
                s_l = process_eq_strip(&state->ch_eq[ch], s_l, 0);
                s_l = process_afd_bank64(&state->ch_afd[ch], s_l, 0);
                if (is_stereo) {
                    s_r = process_eq_strip(&state->ch_eq[ch], s_r, 1);
                    s_r = process_afd_bank64(&state->ch_afd[ch], s_r, 1);
                }
                apply_module_autogain(eq_in_l, eq_in_r, &s_l, &s_r, is_stereo, &state->stage_env_in[ch][2], &state->stage_env_out[ch][2], &state->stage_gain[ch][2], !ch_has_tube_amp);
            }
            if (is_rta_ch) {
                feed_rta_sample(state, is_stereo ? 0.5f * (s_l + s_r) : s_l);
            }

            /* STAGE 5: Stereo-Linked Per-Channel World-Class Compressor (FETISH 76 | LALA | DUAL 76->LA | BUSTER) + Auto-Gain Staging */
            float sample_gr = 0.0f;
            {
                float cmp_in_l = s_l, cmp_in_r = s_r;
                if (!is_stereo) {
                    s_l = process_dynamics_slot(&state->ch_dyn[ch], s_l, &sample_gr);
                } else {
                    float link_in = (fabsf(s_l) >= fabsf(s_r)) ? s_l : s_r;
                    float link_out = process_dynamics_slot(&state->ch_dyn[ch], link_in, &sample_gr);
                    float cg = (fabsf(link_in) > 0.00001f) ? (link_out / link_in) : 1.0f;
                    s_l *= cg;
                    s_r *= cg;
                }
                apply_module_autogain(cmp_in_l, cmp_in_r, &s_l, &s_r, is_stereo, &state->stage_env_in[ch][3], &state->stage_env_out[ch][3], &state->stage_gain[ch][3], !ch_has_tube_amp);
            }
            if (sample_gr > block_gr_max) block_gr_max = sample_gr;

            /* STAGE 6: Dual-Band Zero-Phase Cytomic SVF Dynamic Sibilance & Harshness De-Esser
             * Bypassed automatically when a guitar/bass amp slot is active so vocal sibilance ducking never dulls guitar pick attack! */
            if ((ds_en && !ch_has_tube_amp) || ds_listen) {
                CytomicSvfBand *svf1 = &state->deess_svf1[ch];
                CytomicSvfBand *svf2 = &state->deess_svf2[ch];
                if ((i & 15) == 0) {
                    ramp_deesser_svf_band(svf1, fs_ch);
                    ramp_deesser_svf_band(svf2, fs_ch);
                }
                float v3_1l = s_l - svf1->ic2eq[0];
                float v1_1l = svf1->a1 * svf1->ic1eq[0] + svf1->a2 * v3_1l;
                float v2_1l = svf1->ic2eq[0] + svf1->a2 * svf1->ic1eq[0] + svf1->a3 * v3_1l;
                svf1->ic1eq[0] = 2.0f * v1_1l - svf1->ic1eq[0];
                svf1->ic2eq[0] = 2.0f * v2_1l - svf1->ic2eq[0];
                float bp1_l = svf1->m1 * v1_1l;
                float bp1_r = bp1_l;
                if (is_stereo) {
                    float v3_1r = s_r - svf1->ic2eq_r[0];
                    float v1_1r = svf1->a1 * svf1->ic1eq_r[0] + svf1->a2 * v3_1r;
                    float v2_1r = svf1->ic2eq_r[0] + svf1->a2 * svf1->ic1eq_r[0] + svf1->a3 * v3_1r;
                    svf1->ic1eq_r[0] = 2.0f * v1_1r - svf1->ic1eq_r[0];
                    svf1->ic2eq_r[0] = 2.0f * v2_1r - svf1->ic2eq_r[0];
                    bp1_r = svf1->m1 * v1_1r;
                }

                float v3_2l = s_l - svf2->ic2eq[0];
                float v1_2l = svf2->a1 * svf2->ic1eq[0] + svf2->a2 * v3_2l;
                float v2_2l = svf2->ic2eq[0] + svf2->a2 * svf2->ic1eq[0] + svf2->a3 * v3_2l;
                svf2->ic1eq[0] = 2.0f * v1_2l - svf2->ic1eq[0];
                svf2->ic2eq[0] = 2.0f * v2_2l - svf2->ic2eq[0];
                float bp2_l = svf2->m1 * v1_2l;
                float bp2_r = bp2_l;
                if (is_stereo) {
                    float v3_2r = s_r - svf2->ic2eq_r[0];
                    float v1_2r = svf2->a1 * svf2->ic1eq_r[0] + svf2->a2 * v3_2r;
                    float v2_2r = svf2->ic2eq_r[0] + svf2->a2 * svf2->ic1eq_r[0] + svf2->a3 * v3_2r;
                    svf2->ic1eq_r[0] = 2.0f * v1_2r - svf2->ic1eq_r[0];
                    svf2->ic2eq_r[0] = 2.0f * v2_2r - svf2->ic2eq_r[0];
                    bp2_r = svf2->m1 * v1_2r;
                }

                float abs_b1 = is_stereo ? fmaxf(fabsf(bp1_l), fabsf(bp1_r)) : fabsf(bp1_l);
                float abs_b2 = is_stereo ? fmaxf(fabsf(bp2_l), fabsf(bp2_r)) : fabsf(bp2_l);
                if (abs_b1 > block_ds_det1) block_ds_det1 = abs_b1;
                if (abs_b2 > block_ds_det2) block_ds_det2 = abs_b2;

                if (abs_b1 > state->deesser_env[ch]) {
                    state->deesser_env[ch] = 0.94f * state->deesser_env[ch] + 0.06f * abs_b1;
                } else {
                    state->deesser_env[ch] = ds_rel * state->deesser_env[ch] + (1.0f - ds_rel) * abs_b1;
                }
                if (abs_b2 > state->deesser_env2[ch]) {
                    state->deesser_env2[ch] = 0.94f * state->deesser_env2[ch] + 0.06f * abs_b2;
                } else {
                    state->deesser_env2[ch] = ds_rel * state->deesser_env2[ch] + (1.0f - ds_rel) * abs_b2;
                }

                if (ds_en) {
                    if (state->deesser_env[ch] <= ds_knee_lin1 || ds_max1_db <= 0.05f) {
                        cur_ds_gain1 = 1.0f;
                    } else if ((i & 3) == 0 || cur_ds_gain1 == 1.0f) {
                        float in1_db = 20.0f * log10f(fmaxf(0.00001f, state->deesser_env[ch]));
                        float diff1 = in1_db - ds_thresh1_db;
                        float gr1 = 0.0f;
                        if (diff1 > -2.0f) {
                            if (diff1 < 2.0f) {
                                float x1 = diff1 + 2.0f;
                                gr1 = (0.85f * x1 * x1) * 0.125f; /* 4.0 dB soft knee */
                            } else {
                                gr1 = 0.85f * diff1; /* ~6.5:1 high-ratio sibilance compressor */
                            }
                        }
                        if (gr1 > ds_max1_db) gr1 = ds_max1_db;
                        if (gr1 > block_ds_gr1) block_ds_gr1 = gr1;
                        cur_ds_gain1 = powf(10.0f, -gr1 * 0.05f);
                    }
                    if (!ds_b2_en || state->deesser_env2[ch] <= ds_knee_lin2 || ds_max2_db <= 0.05f) {
                        cur_ds_gain2 = 1.0f;
                    } else if ((i & 3) == 0 || cur_ds_gain2 == 1.0f) {
                        float in2_db = 20.0f * log10f(fmaxf(0.00001f, state->deesser_env2[ch]));
                        float diff2 = in2_db - ds_thresh2_db;
                        float gr2 = 0.0f;
                        if (diff2 > -2.0f) {
                            if (diff2 < 2.0f) {
                                float x2 = diff2 + 2.0f;
                                gr2 = (0.85f * x2 * x2) * 0.125f;
                            } else {
                                gr2 = 0.85f * diff2;
                            }
                        }
                        if (gr2 > ds_max2_db) gr2 = ds_max2_db;
                        if (gr2 > block_ds_gr2) block_ds_gr2 = gr2;
                        cur_ds_gain2 = powf(10.0f, -gr2 * 0.05f);
                    }
                } else {
                    cur_ds_gain1 = 1.0f;
                    cur_ds_gain2 = 1.0f;
                }
                float ds_gain1 = cur_ds_gain1;
                float ds_gain2 = cur_ds_gain2;

                if (ds_listen == 1) {
                    s_l = bp1_l + (ds_b2_en ? bp2_l : 0.0f);
                    if (is_stereo) s_r = bp1_r + (ds_b2_en ? bp2_r : 0.0f);
                } else if (ds_listen == 2) {
                    s_l = bp1_l;
                    if (is_stereo) s_r = bp1_r;
                } else if (ds_listen == 3) {
                    s_l = bp2_l;
                    if (is_stereo) s_r = bp2_r;
                } else if (ds_en) {
                    if (ds_wide) {
                        float wg = fminf(ds_gain1, ds_gain2);
                        s_l *= wg;
                        if (is_stereo) s_r *= wg;
                    } else {
                        float cut1 = 1.0f - ds_gain1;
                        float cut2 = ds_b2_en ? (1.0f - ds_gain2) : 0.0f;
                        s_l = s_l - bp1_l * cut1 - bp2_l * cut2;
                        if (is_stereo) s_r = s_r - bp1_r * cut1 - bp2_r * cut2;
                    }
                }
            }

            float abs_val = is_stereo ? fmaxf(fabsf(s_l), fabsf(s_r)) : fabsf(s_l);
            if (abs_val > max_val) max_val = abs_val;
            if (i < safe_frames) {
                ch_post_buf[ch][i] = (is_stereo ? 0.5f * (s_l + s_r) : s_l) * state->ch_fader_gain[ch];
            }

            if (audible) {
                if (out_l) out_l[i] += s_l * gain_l;
                if (out_r) out_r[i] += (is_stereo ? s_r : s_l) * gain_r;
                float mono_sum = is_stereo ? 0.5f * (s_l + s_r) : s_l;
                for (int m = 0; m < NUM_MONITORS; m++) {
                    if (mon_bufs[m] && !state->mon_mute[m] && state->sof_mon_gain[m][ch] > 0.0001f) {
                        mon_bufs[m][i] += mono_sum * state->sof_mon_gain[m][ch];
                    }
                }
                if (i < safe_frames) {
                    /* Route channel stereo signal into 4 True Independent Stereo FX Sends (Send 1..4) */
                    for (int fx = 0; fx < NUM_FX; fx++) {
                        if (!state->send_mute[fx] && state->sof_fx_gain[fx][ch] > 0.0001f) {
                            float sg = state->sof_fx_gain[fx][ch];
                            fx_send_l[fx][i] += s_l * sg;
                            fx_send_r[fx][i] += (is_stereo ? s_r : s_l) * sg;
                        }
                    }
                }
            }
        }

        /* Update 60FPS Dual-Band De-Esser Gain Reduction & Detector Telemetry */
        state->deesser_gr1_db[ch] = (block_ds_gr1 >= state->deesser_gr1_db[ch]) ? block_ds_gr1 : (state->deesser_gr1_db[ch] * 0.82f + block_ds_gr1 * 0.18f);
        if (state->deesser_gr1_db[ch] < 0.05f) state->deesser_gr1_db[ch] = 0.0f;
        state->deesser_gr2_db[ch] = (block_ds_gr2 >= state->deesser_gr2_db[ch]) ? block_ds_gr2 : (state->deesser_gr2_db[ch] * 0.82f + block_ds_gr2 * 0.18f);
        if (state->deesser_gr2_db[ch] < 0.05f) state->deesser_gr2_db[ch] = 0.0f;
        float det1_db = (block_ds_det1 > 0.00003f) ? (20.0f * log10f(block_ds_det1)) : -80.0f;
        float det2_db = (block_ds_det2 > 0.00003f) ? (20.0f * log10f(block_ds_det2)) : -80.0f;
        state->deesser_det1_db[ch] = 0.75f * state->deesser_det1_db[ch] + 0.25f * det1_db;
        state->deesser_det2_db[ch] = 0.75f * state->deesser_det2_db[ch] + 0.25f * det2_db;

        /* Update 60FPS Cyber-Denoiser 10-Band UI Meter Levels once per JACK block */
        for (int b = 0; b < 10; b++) {
            float r = sqrtf(state->denoise_rms_sq[ch][b] + 1e-12f);
            float db_lvl = (r > 0.00003f) ? (20.0f * log10f(r)) : -90.0f;
            float norm_lvl = (db_lvl + 90.0f) * (1.0f / 90.0f);
            if (norm_lvl < 0.0f) norm_lvl = 0.0f;
            if (norm_lvl > 1.0f) norm_lvl = 1.0f;
            if (norm_lvl >= state->denoise_band_levels[ch][b]) {
                state->denoise_band_levels[ch][b] = norm_lvl;
            } else {
                state->denoise_band_levels[ch][b] = fmaxf(norm_lvl, state->denoise_band_levels[ch][b] * 0.85f);
                if (state->denoise_band_levels[ch][b] < 0.005f) state->denoise_band_levels[ch][b] = 0.0f;
            }
        }

        /* Update Stage 1 1176 Preamp input peak & saturation status */
        float pre_in_scaled = raw_in_max * fabsf(pregain);
        state->preamp_in_peak[ch] = fmaxf(pre_in_scaled, state->preamp_in_peak[ch] * 0.85f);
        if (state->preamp_in_peak[ch] < 0.0003f) state->preamp_in_peak[ch] = 0.0f;
        state->preamp_clip_status[ch] = block_clip_status;

        /* Update Pre-Fader (Post-DSP) Channel Level for Sends on Fader (SOF) Metering */
        float pre_fader_val = audible ? max_val : 0.0f;
        state->ch_pre_levels[ch] = fmaxf(pre_fader_val, state->ch_pre_levels[ch] * 0.85f);
        if (state->ch_pre_levels[ch] < 0.0003f) state->ch_pre_levels[ch] = 0.0f;

        /* Update Channel Post-Strip Peak Meter */
        float metered_val = audible ? (max_val * state->ch_fader_gain[ch]) : 0.0f;
        state->peak_levels[ch] = fmaxf(metered_val, state->peak_levels[ch] * 0.85f);
        if (state->peak_levels[ch] < 0.0003f) state->peak_levels[ch] = 0.0f;

        /* Update Channel Gain Reduction Telemetry from real audio compressor envelope */
        if (block_gr_max >= state->ch_gr_db[ch]) {
            state->ch_gr_db[ch] = block_gr_max;
        } else {
            state->ch_gr_db[ch] = state->ch_gr_db[ch] * 0.84f + block_gr_max * 0.16f;
            if (state->ch_gr_db[ch] < 0.03f) state->ch_gr_db[ch] = 0.0f;
        }
    }

    /* Process 4 True Independent Stereo FX Sends & Returns:
       - Send 1 (Stereo) -> Reverb 1 (Instrument Room/Plate) -> Return 1 (Stereo)
       - Send 2 (Stereo) -> Reverb 2 (Vocal Hall/Abbey Plate) -> Return 2 (Stereo)
       - Send 3 (Stereo) -> Delay 1  (15/30 IPS Tape Slapback) -> Return 3 (Stereo)
       - Send 4 (Stereo) -> Delay 2  (Modulated Vocal Delay)  -> Return 4 (Stereo) */
    if (out_l && out_r) {
        static const float fdn_lens[2][4] = {
            { 1597.0f, 2053.0f, 2617.0f, 3301.0f }, /* Reverb 1: Mutually-Prime Hall / Acoustic Chamber / Room / Plate */
            { 1699.0f, 2137.0f, 2689.0f, 3371.0f }  /* Reverb 2: Lush Vocal Hall / Abbey Plate / Cathedral */
        };
        static const int   er_taps_l[4] = { 283, 739, 1543, 2713 };
        static const int   er_taps_r[4] = { 467, 1103, 2089, 3467 };
        static const float er_gains[4]  = { 0.42f, 0.31f, 0.22f, 0.15f };
        static const int   ap_lens_l[2] = { 225, 556 };
        static const int   ap_lens_r[2] = { 248, 579 };
        float fs = (state->sample_rate > 8000.0f) ? state->sample_rate : 48000.0f;
        float send_pk[NUM_FX] = {0.0f, 0.0f, 0.0f, 0.0f};
        float ret_pk[NUM_FX]  = {0.0f, 0.0f, 0.0f, 0.0f};
        float send_gr_max[NUM_FX] = {0.0f, 0.0f, 0.0f, 0.0f};
        float ret_gr_max[NUM_FX]  = {0.0f, 0.0f, 0.0f, 0.0f};

        /* Precompute coefficients for both Stereo Reverbs (u = 0..1) */
        float rev_fb[2], rev_size_scale[2], rev_damp[2], rev_ap_g[2], rev_er_mix[2], rev_wet[2], rev_width[2];
        int   predly_samp[2], rev_sr_step[2];
        float rev_in_hp_a[2], rev_in_lp_a[2], rev_out_hp_a[2], rev_out_lp_a[2];
        float rev_mod_inc[2], rev_mod_dep[2], rev_duck_amt[2], rev_bit_levels[2];
        for (int u = 0; u < 2; u++) {
            ReverbUnit *rv = &state->rev[u];
            float dec_pct = (rv->decay > 0.1f) ? rv->decay : rv->size;
            rev_fb[u] = 0.24f + (dec_pct * 0.00748f);
            if (rev_fb[u] > 0.988f) rev_fb[u] = 0.988f;
            rev_size_scale[u] = 0.34f + (rv->size * 0.0078f);
            if (rev_size_scale[u] < 0.30f) rev_size_scale[u] = 0.30f;
            if (rev_size_scale[u] > 1.12f) rev_size_scale[u] = 1.12f;
            rev_damp[u] = 0.06f + (rv->damp * 0.0062f);
            if (rev_damp[u] > 0.75f) rev_damp[u] = 0.75f;
            rev_ap_g[u] = 0.25f + (rv->diffuse * 0.0042f);
            if (rev_ap_g[u] > 0.68f) rev_ap_g[u] = 0.68f;
            rev_er_mix[u] = (rv->ir_mix > 0.1f ? rv->ir_mix : 45.0f) * 0.01f;
            rev_wet[u] = (rv->wet * 0.01f) * 1.65f;
            rev_width[u] = rv->stereo * 0.01f;
            predly_samp[u] = (int)(rv->predelay * 0.001f * fs);
            if (predly_samp[u] < 0) predly_samp[u] = 0;
            if (predly_samp[u] > 19500) predly_samp[u] = 19500;
            rev_in_hp_a[u]  = 1.0f - expf(-2.0f * (float)M_PI * fmaxf(20.0f, rv->in_lowcut) / fs);
            rev_in_lp_a[u]  = 1.0f - expf(-2.0f * (float)M_PI * fminf(18000.0f, fmaxf(800.0f, rv->in_highcut)) / fs);
            rev_out_hp_a[u] = 1.0f - expf(-2.0f * (float)M_PI * fmaxf(20.0f, rv->out_lowcut) / fs);
            rev_out_lp_a[u] = 1.0f - expf(-2.0f * (float)M_PI * fminf(18000.0f, fmaxf(800.0f, rv->out_highcut)) / fs);
            rev_mod_inc[u]  = (0.18f + (rv->mod_rate * 0.028f)) * 2.0f * (float)M_PI / fs;
            rev_mod_dep[u]  = (rv->mod_amount * 0.01f) * 16.0f;
            rev_duck_amt[u] = rv->ducking * 0.01f;
            rev_sr_step[u]  = (rv->lofi_sr > 1000.0f && rv->lofi_sr < 44000.0f) ? (int)(fs / rv->lofi_sr) : 1;
            if (rev_sr_step[u] < 1) rev_sr_step[u] = 1;
            rev_bit_levels[u] = (rv->lofi_bits > 3.5f && rv->lofi_bits < 20.0f) ? powf(2.0f, rv->lofi_bits - 1.0f) : 0.0f;
        }

        /* Precompute coefficients for both Stereo Delays (u = 0..1) */
        float base_dly_samp[2], spread_ratio[2], dly_fb[2], dly_ap_g[2], dly_wet[2], dly_width[2];
        float dly_in_hp_a[2], dly_in_lp_a[2], dly_out_hp_a[2], dly_out_lp_a[2];
        float dly_drive_amt[2], dly_mod_inc[2], dly_mod_dep[2], dly_duck_amt[2];
        for (int u = 0; u < 2; u++) {
            DelayUnit *dl = &state->dly[u];
            base_dly_samp[u] = dl->time_ms * 0.001f * fs;
            if (base_dly_samp[u] < 48.0f) base_dly_samp[u] = 48.0f;
            if (base_dly_samp[u] > 94000.0f) base_dly_samp[u] = 94000.0f;
            if (dl->smooth_samp < 48.0f) dl->smooth_samp = base_dly_samp[u];
            spread_ratio[u] = 1.0f + (dl->spread - 50.0f) * 0.0035f;
            dly_fb[u] = dl->feedback * 0.0093f;
            if (dly_fb[u] > 0.94f) dly_fb[u] = 0.94f;
            dly_ap_g[u] = dl->diffuse * 0.0052f;
            if (dly_ap_g[u] > 0.58f) dly_ap_g[u] = 0.58f;
            dly_wet[u] = (dl->wet * 0.01f) * 1.45f;
            dly_width[u] = dl->stereo * 0.01f;
            dly_in_hp_a[u]  = 1.0f - expf(-2.0f * (float)M_PI * fmaxf(20.0f, dl->in_lowcut) / fs);
            dly_in_lp_a[u]  = 1.0f - expf(-2.0f * (float)M_PI * fminf(18000.0f, fmaxf(800.0f, dl->in_highcut)) / fs);
            dly_out_hp_a[u] = 1.0f - expf(-2.0f * (float)M_PI * fmaxf(20.0f, dl->out_lowcut) / fs);
            dly_out_lp_a[u] = 1.0f - expf(-2.0f * (float)M_PI * fminf(18000.0f, fmaxf(800.0f, dl->out_highcut)) / fs);
            dly_drive_amt[u] = dl->drive * 0.01f;
            dly_mod_inc[u] = (0.22f + (dl->mod_rate * 0.038f) + (dl->flutter * 0.042f)) * 2.0f * (float)M_PI / fs;
            dly_mod_dep[u] = ((dl->mod_depth * 0.01f) * 16.0f) + ((dl->flutter * 0.01f) * 9.0f);
            dly_duck_amt[u] = dl->ducking * 0.01f;
        }
        /* Rule 27: one-pole per-sample smoothing coefficient (~20 ms) + delay head crossfade step (~45 ms) */
        const float fx_sm_k = 1.0f - expf(-1.0f / (0.020f * fs));
        const float dly_xf_step = 1.0f / (0.045f * fs);

        for (jack_nframes_t i = 0; i < safe_frames; i++) {
            float send_sig_l[NUM_FX];
            float send_sig_r[NUM_FX];
            for (int fx = 0; fx < NUM_FX; fx++) {
                float sg = state->send_mute[fx] ? 0.0f : state->send_fader_gain[fx];
                float sl = process_eq_strip(&state->send_eq[fx], fx_send_l[fx][i] * sg, 0);
                float sr = process_eq_strip(&state->send_eq[fx], fx_send_r[fx][i] * sg, 1);
                float s_gr = 0.0f;
                float link_in = (fabsf(sl) >= fabsf(sr)) ? sl : sr;
                float link_out = process_dynamics_slot(&state->send_dyn[fx], link_in, &s_gr);
                float cg = (fabsf(link_in) > 0.00001f) ? (link_out / link_in) : 1.0f;
                sl *= cg;
                sr *= cg;
                if (s_gr > send_gr_max[fx]) send_gr_max[fx] = s_gr;
                float asv = fmaxf(fabsf(sl), fabsf(sr));
                if (asv > send_pk[fx]) send_pk[fx] = asv;
                send_sig_l[fx] = sl;
                send_sig_r[fx] = sr;
            }

            float ret_raw_l[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float ret_raw_r[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            /* 1. Process Stereo Reverb 1 (Send 1 -> Return 1) & Stereo Reverb 2 (Send 2 -> Return 2) */
            for (int u = 0; u < 2; u++) {
                ReverbUnit *rv = &state->rev[u];
                /* Rule 27: glide gains/filters per sample (~20 ms) and use dual read-head equal-power crossfade (~45 ms)
                 * for Size & Pre-Delay so preset/fader changes never Doppler pitch-warp the FDN feedback tank */
                if (!rv->sm_init) {
                    rv->sm_init = 1;
                    rv->sm_size_scale = rev_size_scale[u]; rv->sm_fb = rev_fb[u]; rv->sm_damp = rev_damp[u];
                    rv->sm_wet = rev_wet[u]; rv->sm_ap_g = rev_ap_g[u]; rv->sm_er_mix = rev_er_mix[u];
                    rv->sm_width = rev_width[u]; rv->sm_predly = (float)predly_samp[u];
                    rv->sm_in_hp_a = rev_in_hp_a[u]; rv->sm_in_lp_a = rev_in_lp_a[u];
                    rv->sm_out_hp_a = rev_out_hp_a[u]; rv->sm_out_lp_a = rev_out_lp_a[u];
                    rv->xf_active = 0;
                    rv->xf_pos = 0.0f;
                    rv->xf_size_scale = rev_size_scale[u];
                    rv->xf_predly = (float)predly_samp[u];
                }
                if (!rv->xf_active) {
                    float sz_diff = fabsf(rev_size_scale[u] - rv->sm_size_scale);
                    float pd_diff = fabsf((float)predly_samp[u] - rv->sm_predly);
                    if (sz_diff > 0.0015f || pd_diff > 2.0f) {
                        rv->xf_active = 1;
                        rv->xf_pos = 0.0f;
                        rv->xf_size_scale = rev_size_scale[u];
                        rv->xf_predly = (float)predly_samp[u];
                    } else {
                        rv->sm_size_scale = rev_size_scale[u];
                        rv->sm_predly = (float)predly_samp[u];
                    }
                }
                rv->sm_fb        += fx_sm_k * (rev_fb[u] - rv->sm_fb);
                rv->sm_damp      += fx_sm_k * (rev_damp[u] - rv->sm_damp);
                rv->sm_wet       += fx_sm_k * (rev_wet[u] - rv->sm_wet);
                rv->sm_ap_g      += fx_sm_k * (rev_ap_g[u] - rv->sm_ap_g);
                rv->sm_er_mix    += fx_sm_k * (rev_er_mix[u] - rv->sm_er_mix);
                rv->sm_width     += fx_sm_k * (rev_width[u] - rv->sm_width);
                rv->sm_in_hp_a   += fx_sm_k * (rev_in_hp_a[u] - rv->sm_in_hp_a);
                rv->sm_in_lp_a   += fx_sm_k * (rev_in_lp_a[u] - rv->sm_in_lp_a);
                rv->sm_out_hp_a  += fx_sm_k * (rev_out_hp_a[u] - rv->sm_out_hp_a);
                rv->sm_out_lp_a  += fx_sm_k * (rev_out_lp_a[u] - rv->sm_out_lp_a);
                float raw_rev_l = send_sig_l[u] * 0.70f;
                float raw_rev_r = send_sig_r[u] * 0.70f;

                /* Input Low-Cut (HPF) & High-Cut (LPF) BEFORE Pre-Delay & Tank */
                rv->in_hp_l += rv->sm_in_hp_a * (raw_rev_l - rv->in_hp_l);
                rv->in_hp_r += rv->sm_in_hp_a * (raw_rev_r - rv->in_hp_r);
                float in_l = raw_rev_l - rv->in_hp_l;
                float in_r = raw_rev_r - rv->in_hp_r;
                rv->in_lp_l += rv->sm_in_lp_a * (in_l - rv->in_lp_l);
                rv->in_lp_r += rv->sm_in_lp_a * (in_r - rv->in_lp_r);
                in_l = rv->in_lp_l;
                in_r = rv->in_lp_r;

                float rv_g_old = 1.0f, rv_g_new = 0.0f;
                if (rv->xf_active) {
                    float r_th = rv->xf_pos * 1.5707963f;
                    rv_g_old = cosf(r_th);
                    rv_g_new = sinf(r_th);
                }

                int pw = rv->predly_w;
                rv->predly_l[pw] = in_l;
                rv->predly_r[pw] = in_r;
                /* Dual read-head pre-delay read: zero pitch shift during Pre-Delay or Preset changes */
                int   pd_i = (int)rv->sm_predly;
                float pd_f = rv->sm_predly - (float)pd_i;
                int pr = pw - pd_i;
                if (pr < 0) pr += 24000;
                int pr2 = pr - 1;
                if (pr2 < 0) pr2 += 24000;
                float rev_in_l = rv->predly_l[pr] + pd_f * (rv->predly_l[pr2] - rv->predly_l[pr]);
                float rev_in_r = rv->predly_r[pr] + pd_f * (rv->predly_r[pr2] - rv->predly_r[pr]);
                if (rv->xf_active) {
                    int   pd_nb_i = (int)rv->xf_predly;
                    float pd_nb_f = rv->xf_predly - (float)pd_nb_i;
                    int pr_b = pw - pd_nb_i;
                    if (pr_b < 0) pr_b += 24000;
                    int pr_b2 = pr_b - 1;
                    if (pr_b2 < 0) pr_b2 += 24000;
                    float rev_b_l = rv->predly_l[pr_b] + pd_nb_f * (rv->predly_l[pr_b2] - rv->predly_l[pr_b]);
                    float rev_b_r = rv->predly_r[pr_b] + pd_nb_f * (rv->predly_r[pr_b2] - rv->predly_r[pr_b]);
                    rev_in_l = rev_in_l * rv_g_old + rev_b_l * rv_g_new;
                    rev_in_r = rev_in_r * rv_g_old + rev_b_r * rv_g_new;
                }

                float er_l = 0.0f, er_r = 0.0f;
                for (int t = 0; t < 4; t++) {
                    int idx_el = pr - er_taps_l[t];
                    if (idx_el < 0) idx_el += 24000;
                    int idx_er = pr - er_taps_r[t];
                    if (idx_er < 0) idx_er += 24000;
                    er_l += rv->predly_l[idx_el] * er_gains[t];
                    er_r += rv->predly_r[idx_er] * er_gains[t];
                }
                if (++rv->predly_w >= 24000) rv->predly_w = 0;

                float diff_l = rev_in_l + er_l * (rv->sm_er_mix * 0.35f);
                float diff_r = rev_in_r + er_r * (rv->sm_er_mix * 0.35f);
                for (int a = 0; a < 2; a++) {
                    int ia_l = rv->ap_idx_l[a];
                    float buf_l = rv->ap_l[a][ia_l];
                    float vn_l = diff_l + buf_l * rv->sm_ap_g;
                    rv->ap_l[a][ia_l] = vn_l;
                    diff_l = buf_l - vn_l * rv->sm_ap_g;
                    if (++rv->ap_idx_l[a] >= ap_lens_l[a]) rv->ap_idx_l[a] = 0;

                    int ia_r = rv->ap_idx_r[a];
                    float buf_r = rv->ap_r[a][ia_r];
                    float vn_r = diff_r + buf_r * rv->sm_ap_g;
                    rv->ap_r[a][ia_r] = vn_r;
                    diff_r = buf_r - vn_r * rv->sm_ap_g;
                    if (++rv->ap_idx_r[a] >= ap_lens_r[a]) rv->ap_idx_r[a] = 0;
                }

                rv->mod_phase += rev_mod_inc[u];
                if (rv->mod_phase > 2.0f * (float)M_PI) rv->mod_phase -= 2.0f * (float)M_PI;
                float lfo_s = sinf(rv->mod_phase) * rev_mod_dep[u];
                float lfo_c = cosf(rv->mod_phase) * rev_mod_dep[u];
                float mod_offs[4] = { lfo_s, lfo_c, -lfo_s * 0.85f, -lfo_c * 0.85f };

                float d_out[4];
                for (int b = 0; b < 4; b++) {
                    float raw_d = read_frac_hermite(rv->fdn_buf[b], 4096, rv->fdn_w[b], fdn_lens[u][b] * rv->sm_size_scale + mod_offs[b]);
                    if (rv->xf_active) {
                        float raw_d_new = read_frac_hermite(rv->fdn_buf[b], 4096, rv->fdn_w[b], fdn_lens[u][b] * rv->xf_size_scale + mod_offs[b]);
                        raw_d = raw_d * rv_g_old + raw_d_new * rv_g_new;
                    }
                    rv->fdn_lp[b] = raw_d * (1.0f - rv->sm_damp) + rv->fdn_lp[b] * rv->sm_damp;
                    rv->fdn_dc[b] += 0.0015f * (rv->fdn_lp[b] - rv->fdn_dc[b]);
                    d_out[b] = rv->fdn_lp[b] - rv->fdn_dc[b];
                }
                if (rv->xf_active) {
                    rv->xf_pos += dly_xf_step;
                    if (rv->xf_pos >= 1.0f) {
                        rv->sm_size_scale = rv->xf_size_scale;
                        rv->sm_predly = rv->xf_predly;
                        rv->xf_active = 0;
                    }
                }

                float s01 = d_out[0] + d_out[1];
                float d01 = d_out[0] - d_out[1];
                float s23 = d_out[2] + d_out[3];
                float d23 = d_out[2] - d_out[3];
                float h_vec[4] = {
                    0.5f * (s01 + s23),
                    0.5f * (d01 + d23),
                    0.5f * (s01 - s23),
                    0.5f * (d01 - d23)
                };
                float tank_in[4] = { diff_l, diff_r, diff_l * 0.707f + diff_r * 0.707f, diff_l * 0.707f - diff_r * 0.707f };
                for (int b = 0; b < 4; b++) {
                    int fw = rv->fdn_w[b];
                    float next_v = tank_in[b] * 0.55f + h_vec[b] * rv->sm_fb;
                    if (next_v > 1.5f || next_v < -1.5f) next_v = 1.5f * tanhf(next_v * 0.666f);
                    rv->fdn_buf[b][fw] = next_v;
                    rv->fdn_w[b] = (fw + 1) & 4095;
                }

                float rev_out_l = er_l * rv->sm_er_mix * 0.65f + (d_out[0] - d_out[2] + 0.65f * (d_out[1] - d_out[3])) * 0.65f;
                float rev_out_r = er_r * rv->sm_er_mix * 0.65f + (d_out[1] + d_out[3] + 0.65f * (d_out[0] + d_out[2])) * 0.65f;

                if (rev_sr_step[u] > 1) {
                    if (++rv->sr_cnt >= rev_sr_step[u]) {
                        rv->sr_cnt = 0;
                        rv->hold_l = rev_out_l;
                        rv->hold_r = rev_out_r;
                    } else {
                        rev_out_l = rv->hold_l;
                        rev_out_r = rv->hold_r;
                    }
                }
                if (rev_bit_levels[u] > 1.0f) {
                    rev_out_l = roundf(rev_out_l * rev_bit_levels[u]) / rev_bit_levels[u];
                    rev_out_r = roundf(rev_out_r * rev_bit_levels[u]) / rev_bit_levels[u];
                }

                /* Output Return Low-Cut (HPF) & High-Cut (LPF) */
                rv->out_hp_l += rv->sm_out_hp_a * (rev_out_l - rv->out_hp_l);
                rv->out_hp_r += rv->sm_out_hp_a * (rev_out_r - rv->out_hp_r);
                rev_out_l -= rv->out_hp_l;
                rev_out_r -= rv->out_hp_r;
                rv->out_lp_l += rv->sm_out_lp_a * (rev_out_l - rv->out_lp_l);
                rv->out_lp_r += rv->sm_out_lp_a * (rev_out_r - rv->out_lp_r);
                rev_out_l = rv->out_lp_l;
                rev_out_r = rv->out_lp_r;

                float rev_in_pk = fmaxf(fabsf(raw_rev_l), fabsf(raw_rev_r)) * 3.5f;
                if (rev_in_pk > rv->duck_env) rv->duck_env = 0.99f * rv->duck_env + 0.01f * rev_in_pk;
                else rv->duck_env = 0.9995f * rv->duck_env + 0.0005f * rev_in_pk;
                float rev_duck_g = 1.0f - fminf(0.85f, rv->duck_env * rev_duck_amt[u] * 2.2f);
                float rev_mid = 0.5f * (rev_out_l + rev_out_r);
                float rev_side = 0.5f * (rev_out_l - rev_out_r) * rv->sm_width;
                ret_raw_l[u] = (rev_mid + rev_side) * rev_duck_g * rv->sm_wet;
                ret_raw_r[u] = (rev_mid - rev_side) * rev_duck_g * rv->sm_wet;
            }

            /* 2. Process Stereo Delay 1 (Send 3 -> Return 3) & Stereo Delay 2 (Send 4 -> Return 4) */
            for (int u = 0; u < 2; u++) {
                DelayUnit *dl = &state->dly[u];
                int fx_idx = 2 + u;
                float raw_dly_l = send_sig_l[fx_idx];
                float raw_dly_r = send_sig_r[fx_idx];
                float target_samp_l = base_dly_samp[u];
                float target_samp_r = fminf(94000.0f, fmaxf(48.0f, base_dly_samp[u] * spread_ratio[u]));

                /* Rule 27: per-sample ~20 ms smoothing of feedback / wet / width / filter coefficients */
                if (!dl->sm_init) {
                    dl->sm_init = 1;
                    dl->sm_fb = dly_fb[u]; dl->sm_wet = dly_wet[u]; dl->sm_width = dly_width[u];
                    dl->sm_in_hp_a = dly_in_hp_a[u]; dl->sm_in_lp_a = dly_in_lp_a[u];
                    dl->sm_out_hp_a = dly_out_hp_a[u]; dl->sm_out_lp_a = dly_out_lp_a[u];
                    dl->sm_spread = spread_ratio[u];
                    dl->smooth_samp = target_samp_l;
                    dl->smooth_samp_r = target_samp_r;
                    dl->xf_target = target_samp_l;
                    dl->xf_target_r = target_samp_r;
                    dl->xf_active = 0;
                }
                if (dl->smooth_samp_r < 48.0f) dl->smooth_samp_r = target_samp_r;
                dl->sm_fb       += fx_sm_k * (dly_fb[u] - dl->sm_fb);
                dl->sm_wet      += fx_sm_k * (dly_wet[u] - dl->sm_wet);
                dl->sm_width    += fx_sm_k * (dly_width[u] - dl->sm_width);
                dl->sm_in_hp_a  += fx_sm_k * (dly_in_hp_a[u] - dl->sm_in_hp_a);
                dl->sm_in_lp_a  += fx_sm_k * (dly_in_lp_a[u] - dl->sm_in_lp_a);
                dl->sm_out_hp_a += fx_sm_k * (dly_out_hp_a[u] - dl->sm_out_hp_a);
                dl->sm_out_lp_a += fx_sm_k * (dly_out_lp_a[u] - dl->sm_out_lp_a);

                /* Input Low-Cut (HPF) & High-Cut (LPF) BEFORE Delay Line */
                dl->in_hp_l += dl->sm_in_hp_a * (raw_dly_l - dl->in_hp_l);
                dl->in_hp_r += dl->sm_in_hp_a * (raw_dly_r - dl->in_hp_r);
                float dly_in_l = raw_dly_l - dl->in_hp_l;
                float dly_in_r = raw_dly_r - dl->in_hp_r;
                dl->in_lp_l += dl->sm_in_lp_a * (dly_in_l - dl->in_lp_l);
                dl->in_lp_r += dl->sm_in_lp_a * (dly_in_r - dl->in_lp_r);
                dly_in_l = dl->in_lp_l;
                dly_in_r = dl->in_lp_r;

                /* Rule 27: Delay Time, Tap Tempo & L/R Spread => dual read-head ~45 ms equal-power crossfade.
                 * Neither L nor R delay tap ever slides its read pointer, eliminating Doppler pitch-warps. */
                float dt_diff_l = target_samp_l - dl->smooth_samp;
                float dt_diff_r = target_samp_r - dl->smooth_samp_r;
                if (!dl->xf_active) {
                    if (fabsf(dt_diff_l) > 2.0f || fabsf(dt_diff_r) > 2.0f) {
                        dl->xf_active = 1;
                        dl->xf_pos = 0.0f;
                        dl->xf_target = target_samp_l;
                        dl->xf_target_r = target_samp_r;
                    } else {
                        dl->smooth_samp = target_samp_l;
                        dl->smooth_samp_r = target_samp_r;
                    }
                }

                /* Independent LFO phase accumulators (no 2.7x harmonic jump at the 2*PI wrap) */
                dl->mod_phase += dly_mod_inc[u];
                if (dl->mod_phase > 2.0f * (float)M_PI) dl->mod_phase -= 2.0f * (float)M_PI;
                dl->mod_phase2 += dly_mod_inc[u] * 2.7f;
                if (dl->mod_phase2 > 2.0f * (float)M_PI) dl->mod_phase2 -= 2.0f * (float)M_PI;
                float wow_l = sinf(dl->mod_phase) * dly_mod_dep[u] + 0.35f * sinf(dl->mod_phase2) * dly_mod_dep[u];
                float wow_r = cosf(dl->mod_phase) * dly_mod_dep[u] + 0.35f * cosf(dl->mod_phase2) * dly_mod_dep[u];

                int w_idx = dl->write_idx;
                float d_samp_l = dl->smooth_samp + wow_l;
                float d_samp_r = dl->smooth_samp_r + wow_r;

                float dly_out_l = read_frac_hermite(dl->buf_l, 96000, w_idx, d_samp_l);
                float dly_out_r = read_frac_hermite(dl->buf_r, 96000, w_idx, d_samp_r);
                if (dl->xf_active) {
                    float n_l = read_frac_hermite(dl->buf_l, 96000, w_idx, dl->xf_target + wow_l);
                    float n_r = read_frac_hermite(dl->buf_r, 96000, w_idx, dl->xf_target_r + wow_r);
                    float th = dl->xf_pos * 1.5707963f;
                    float g_old = cosf(th), g_new = sinf(th);
                    dly_out_l = dly_out_l * g_old + n_l * g_new;
                    dly_out_r = dly_out_r * g_old + n_r * g_new;
                    dl->xf_pos += dly_xf_step;
                    if (dl->xf_pos >= 1.0f) {
                        dl->smooth_samp = dl->xf_target;
                        dl->smooth_samp_r = dl->xf_target_r;
                        dl->xf_active = 0;
                    }
                }

                if (dly_ap_g[u] > 0.02f) {
                    int dap = dl->ap_idx;
                    float bl = dl->ap_l[dap];
                    float vl = dly_out_l + bl * dly_ap_g[u];
                    dl->ap_l[dap] = vl;
                    dly_out_l = bl - vl * dly_ap_g[u];

                    float br = dl->ap_r[dap];
                    float vr = dly_out_r + br * dly_ap_g[u];
                    dl->ap_r[dap] = vr;
                    dly_out_r = br - vr * dly_ap_g[u];
                    if (++dl->ap_idx >= 347) dl->ap_idx = 0;
                }

                /* Tape/Head Feedback Loop Filtering & Soft Tape Saturation */
                dl->fb_hp_l += dl->sm_in_hp_a * (dly_out_l - dl->fb_hp_l);
                dl->fb_hp_r += dl->sm_in_hp_a * (dly_out_r - dl->fb_hp_r);
                float fb_l = dly_out_l - dl->fb_hp_l;
                float fb_r = dly_out_r - dl->fb_hp_r;
                dl->fb_lp_l += dl->sm_in_lp_a * (fb_l - dl->fb_lp_l);
                dl->fb_lp_r += dl->sm_in_lp_a * (fb_r - dl->fb_lp_r);
                fb_l = dl->fb_lp_l;
                fb_r = dl->fb_lp_r;
                if (dly_drive_amt[u] > 0.02f) {
                    float drv = 1.0f + dly_drive_amt[u] * 1.8f;
                    float norm_drv = 1.0f / tanhf(drv);
                    fb_l = tanhf(fb_l * drv) * norm_drv;
                    fb_r = tanhf(fb_r * drv) * norm_drv;
                }

                if (dl->ping_pong) {
                    float mono_in = 0.5f * (dly_in_l + dly_in_r);
                    dl->buf_l[w_idx] = mono_in + fb_r * dl->sm_fb;
                    dl->buf_r[w_idx] = fb_l * dl->sm_fb;
                } else {
                    dl->buf_l[w_idx] = dly_in_l + fb_l * dl->sm_fb;
                    dl->buf_r[w_idx] = dly_in_r + fb_r * dl->sm_fb;
                }
                if (++dl->write_idx >= 96000) dl->write_idx = 0;

                /* Output Return Low-Cut (HPF) & High-Cut (LPF) */
                dl->out_hp_l += dl->sm_out_hp_a * (dly_out_l - dl->out_hp_l);
                dl->out_hp_r += dl->sm_out_hp_a * (dly_out_r - dl->out_hp_r);
                dly_out_l -= dl->out_hp_l;
                dly_out_r -= dl->out_hp_r;
                dl->out_lp_l += dl->sm_out_lp_a * (dly_out_l - dl->out_lp_l);
                dl->out_lp_r += dl->sm_out_lp_a * (dly_out_r - dl->out_lp_r);
                dly_out_l = dl->out_lp_l;
                dly_out_r = dl->out_lp_r;

                float dly_in_pk = fmaxf(fabsf(raw_dly_l), fabsf(raw_dly_r));
                if (dly_in_pk > dl->duck_env) dl->duck_env = 0.99f * dl->duck_env + 0.01f * dly_in_pk;
                else dl->duck_env = 0.9995f * dl->duck_env + 0.0005f * dly_in_pk;
                float dly_duck_g = 1.0f - fminf(0.85f, dl->duck_env * dly_duck_amt[u] * 2.5f);
                float dly_mid = 0.5f * (dly_out_l + dly_out_r);
                float dly_side = 0.5f * (dly_out_l - dly_out_r) * dl->sm_width;
                ret_raw_l[fx_idx] = (dly_mid + dly_side) * dly_duck_g * dl->sm_wet;
                ret_raw_r[fx_idx] = (dly_mid - dly_side) * dly_duck_g * dl->sm_wet;
            }

            /* 3. Process 4 True Stereo FX Returns (RET 1: Reverb 1, RET 2: Reverb 2, RET 3: Delay 1, RET 4: Delay 2) */
            for (int r = 0; r < NUM_FX; r++) {
                if (state->ret_mute[r]) {
                    fx_ret_buf[r][i] = 0.0f;
                    continue;
                }
                float rl = process_eq_strip(&state->ret_eq[r], ret_raw_l[r], 0);
                float rr = process_eq_strip(&state->ret_eq[r], ret_raw_r[r], 1);
                float r_gr = 0.0f;
                float link_in = (fabsf(rl) >= fabsf(rr)) ? rl : rr;
                float link_out = process_dynamics_slot(&state->ret_dyn[r], link_in, &r_gr);
                float cg = (fabsf(link_in) > 0.00001f) ? (link_out / link_in) : 1.0f;
                float rg = state->ret_fader_gain[r] * cg;
                rl *= rg;
                rr *= rg;
                if (r_gr > ret_gr_max[r]) ret_gr_max[r] = r_gr;
                float arv = fmaxf(fabsf(rl), fabsf(rr));
                if (arv > ret_pk[r]) ret_pk[r] = arv;
                fx_ret_buf[r][i] = 0.5f * (rl + rr);

                float r_pan = state->ret_pan[r];
                float bal_l = (r_pan <= 0.0f) ? 1.0f : (1.0f - r_pan);
                float bal_r = (r_pan >= 0.0f) ? 1.0f : (1.0f + r_pan);
                out_l[i] += rl * bal_l;
                out_r[i] += rr * bal_r;
            }
        }
        for (int fx = 0; fx < NUM_FX; fx++) {
            state->send_peaks[fx] = fmaxf(send_pk[fx], state->send_peaks[fx] * 0.85f);
            if (state->send_peaks[fx] < 0.0003f) state->send_peaks[fx] = 0.0f;
            state->ret_peaks[fx] = fmaxf(ret_pk[fx], state->ret_peaks[fx] * 0.85f);
            if (state->ret_peaks[fx] < 0.0003f) state->ret_peaks[fx] = 0.0f;
            state->send_gr_db[fx] = state->send_gr_db[fx] * 0.84f + send_gr_max[fx] * 0.16f;
            state->ret_gr_db[fx]  = state->ret_gr_db[fx]  * 0.84f + ret_gr_max[fx]  * 0.16f;
        }
    }

    /* Stage 7: 2" Analog Multitrack Tape Machine Record/Play/Overdub, 0 VU = -18 dBFS Metering & Stereo Monitor Mixer */
    {
        float fs = (state->sample_rate > 8000.0f) ? state->sample_rate : 48000.0f;
        int ntrk = state->tape_num_tracks;
        if (ntrk > MAX_TAPE_TRACKS) ntrk = MAX_TAPE_TRACKS;
        int any_tape_solo = 0;
        float trk_pk[MAX_TAPE_TRACKS];
        for (int t = 0; t < ntrk; t++) {
            trk_pk[t] = 0.0f;
            if (state->tape_trk_solo[t]) any_tape_solo = 1;
        }

        for (jack_nframes_t i = 0; i < safe_frames; i++) {
            float tape_bus_l = 0.0f, tape_bus_r = 0.0f;
            uint32_t pr_idx = state->play_read_pos & (DISK_RING_FRAMES - 1);

            for (int t = 0; t < ntrk; t++) {
                int src_idx = state->tape_trk_src[t];
                float src_s = 0.0f;
                if (t >= 8 && t < 12) {
                    src_s = fx_ret_buf[t - 8][i];
                } else if (state->tape_trk_is_direct[t]) {
                    src_s = ch_raw_buf[src_idx & 7][i];
                } else {
                    src_s = ch_post_buf[src_idx & 7][i];
                }

                float head_in = src_s;
                float disk_play_s = (state->tape_play_ring[t]) ? state->tape_play_ring[t][pr_idx] : 0.0f;

                if (state->tape_transport == 2) {
                    /* True Unlimited-Length Direct-to-Disk DAW Overdubbing:
                       - Armed tracks (tape_trk_rec[t] == 1) stream live audio to disk via tape_disk_ring[t].
                       - Un-Armed tracks (tape_trk_rec[t] == 0) stream their existing full-length WAV from disk via tape_play_ring[t]! */
                    if (state->tape_trk_rec[t]) {
                        if (state->tape_disk_ring[t]) {
                            state->tape_disk_ring[t][state->ring_write_pos & (DISK_RING_FRAMES - 1)] = src_s;
                        }
                        head_in = src_s;
                    } else {
                        if (state->tape_disk_ring[t]) {
                            state->tape_disk_ring[t][state->ring_write_pos & (DISK_RING_FRAMES - 1)] = 0.0f;
                        }
                        head_in = (state->tape_frame_pos < state->tape_max_frames) ? disk_play_s : 0.0f;
                    }
                } else if (state->tape_transport == 1) {
                    head_in = (state->tape_frame_pos < state->tape_max_frames) ? disk_play_s : 0.0f;
                }

                float head_out = process_tape_2inch_sample(state, t, head_in, fs);
                float abs_ho = fabsf(head_out);
                if (abs_ho > trk_pk[t]) trk_pk[t] = abs_ho;

                /* Sum playback tracks into Tape Monitor Bus during PLAY (all audible tracks) AND during RECORD (un-armed playback tracks) */
                if (state->tape_transport == 1 || (state->tape_transport == 2 && !state->tape_trk_rec[t])) {
                    int t_audible = (!state->tape_trk_mute[t]) && (!any_tape_solo || state->tape_trk_solo[t]);
                    if (t_audible) {
                        float tv = head_out * state->tape_trk_vol[t];
                        float t_theta = (state->tape_trk_pan[t] + 1.0f) * (float)(M_PI * 0.25);
                        tape_bus_l += tv * cosf(t_theta) * 1.41421356f;
                        tape_bus_r += tv * sinf(t_theta) * 1.41421356f;
                    }
                }
            }

            if (state->tape_transport == 1 || state->tape_transport == 2) {
                /* 2" Master Tape Bus Summing Glue */
                if (state->tape_emu_enabled) {
                    tape_bus_l = 0.25f * tanhf(tape_bus_l / 0.25f);
                    tape_bus_r = 0.25f * tanhf(tape_bus_r / 0.25f);
                }
                if (out_l) out_l[i] += tape_bus_l * state->tape_master_vol;
                if (out_r) out_r[i] += tape_bus_r * state->tape_master_vol;
                float tape_mono = 0.5f * (tape_bus_l + tape_bus_r);
                for (int m = 0; m < NUM_MONITORS; m++) {
                    if (mon_bufs[m] && state->tape_mon_send[m] > 0.0001f) {
                        mon_bufs[m][i] += tape_mono * state->tape_mon_send[m];
                    }
                }
            }

            if (state->tape_transport == 1 || state->tape_transport == 2) {
                if (state->tape_transport == 2) state->ring_write_pos++;
                state->play_read_pos++;
                state->tape_frame_pos++;
                if (state->tape_transport == 2 && state->tape_frame_pos > state->tape_max_frames) {
                    state->tape_max_frames = state->tape_frame_pos;
                }
                /* Stop cleanly at the end of a recorded song during PLAY instead of looping back to 0 */
                if (state->tape_transport == 1 && state->tape_max_frames > 4800 && state->tape_frame_pos >= state->tape_max_frames) {
                    state->tape_transport = 0;
                    state->tape_frame_pos = 0;
                    state->tape_seek_requested = 1;
                }
            }
        }

        /* Update Calibrated 0 VU = -18.0 dBFS Track VU Meters & -1.0 dBFS Digital Clip LEDs */
        for (int t = 0; t < ntrk; t++) {
            float dbfs = (trk_pk[t] > 0.00001f) ? (20.0f * log10f(trk_pk[t])) : -60.0f;
            float vu_target = dbfs + 18.0f; /* 0.0 VU = -18.0 dBFS Digital Standard */
            if (vu_target < -24.0f) vu_target = -24.0f;
            if (vu_target > 9.0f) vu_target = 9.0f;
            if (vu_target > state->tape_trk_vu_db[t]) state->tape_trk_vu_db[t] = vu_target;
            else state->tape_trk_vu_db[t] = state->tape_trk_vu_db[t] * 0.88f + vu_target * 0.12f;
            state->tape_trk_clip[t] = (trk_pk[t] >= 0.89125f) ? 1 : 0;
        }
    }

    /* Finalize 6 Stage Monitor Outputs with 8-Band EQ, 64-Bit Surgical AFD & Dual-Slot Series Dynamics (Slot A + Slot B) */
    int any_mon_solo = 0;
    for (int m = 0; m < NUM_MONITORS; m++) {
        if (state->mon_solo[m]) any_mon_solo = 1;
        float mon_max = 0.0f;
        float mon_gr_max = 0.0f;
        float mon_gr2_max = 0.0f;
        float m_gain = state->mon_mute[m] ? 0.0f : state->mon_fader_gain[m];
        char m_name[16];
        snprintf(m_name, sizeof(m_name), "mon-%d", m + 1);
        int is_rta_mon = (state->rta_target_kind == 2 && strcmp(state->rta_target_bus, m_name) == 0);
        int is_cal_mon = (state->room_cal_active && strcmp(state->room_cal_bus, m_name) == 0);
        if (mon_bufs[m]) {
            for (jack_nframes_t i = 0; i < nframes; i++) {
                float v = mon_bufs[m][i] * m_gain;
                if (is_cal_mon && state->room_cal_samples_left > 0) {
                    float pn = next_pink_noise_sample(state);
                    v += pn;
                    if (--state->room_cal_samples_left <= 0) {
                        state->room_cal_active = 0;
                        state->room_cal_done_flag = 1;
                    }
                    feed_rta_sample(state, v);
                }
                v = process_eq_strip(&state->mon_eq[m], v, 0);
                v = process_afd_bank64(&state->mon_afd[m], v, 0);
                if (is_rta_mon) feed_rta_sample(state, v);
                /* Slot A: Monitor Bus Compressor */
                float sample_gr1 = 0.0f;
                v = process_dynamics_slot(&state->mon_dyn[m], v, &sample_gr1);
                /* Slot B: Monitor Speaker Protection Limiter / Second Compressor */
                float sample_gr2 = 0.0f;
                v = process_dynamics_slot(&state->mon_dyn2[m], v, &sample_gr2);
                /* Hard safety ceiling at -1.0 dBFS (0.89125f) */
                if (v >  0.89125f) v =  0.89125f;
                if (v < -0.89125f) v = -0.89125f;
                mon_bufs[m][i] = v;
                if (sample_gr1 > mon_gr_max) mon_gr_max = sample_gr1;
                if (sample_gr2 > mon_gr2_max) mon_gr2_max = sample_gr2;
                float av = fabsf(v);
                if (av > mon_max) mon_max = av;
            }
        }
        state->aux_peaks[m] = fmaxf(mon_max, state->aux_peaks[m] * 0.85f);
        if (state->aux_peaks[m] < 0.0003f) state->aux_peaks[m] = 0.0f;
        state->mon_gr_db[m] = state->mon_gr_db[m] * 0.84f + mon_gr_max * 0.16f;
        state->mon_gr2_db[m] = state->mon_gr2_db[m] * 0.84f + mon_gr2_max * 0.16f;
    }

    /* Stage 8: Monitor Solo / Cue-to-Out-1/2 Auditioning (Ideal for 2x2 interfaces like Audient EVO 4) */
    if (any_mon_solo && state->solo_cue_to_main && out_l && out_r) {
        for (jack_nframes_t i = 0; i < nframes; i++) {
            float cue_v = 0.0f;
            for (int m = 0; m < NUM_MONITORS; m++) {
                if (state->mon_solo[m] && mon_bufs[m]) cue_v += mon_bufs[m][i];
            }
            out_l[i] = cue_v;
            out_r[i] = cue_v;
        }
    }

    /* Apply Master Bus Analog Summing, 8-Band Tukan Dynamic EQ, 64-Bit AFD, Dual-Slot Dynamics & Master Fader */
    float m_gain = state->master_mute ? 0.0f : state->master_fader_gain;
    float max_l = 0.0f, max_r = 0.0f;
    float master_gr_max = 0.0f;
    float master_gr2_max = 0.0f;
    int is_rta_master = (state->rta_target_kind == 2 && strcmp(state->rta_target_bus, "master") == 0);
    int is_cal_master = (state->room_cal_active && strcmp(state->room_cal_bus, "master") == 0);
    float sum_drive_lin = powf(10.0f, state->summing_drive_db / 20.0f);
    float sum_warmth_g  = (powf(10.0f, state->summing_warmth_db / 20.0f) - 1.0f) * 0.45f;
    float sum_xtalk_lin = powf(10.0f, state->summing_crosstalk_db / 20.0f);
    float sum_lp_a      = 1.0f - expf(-2.0f * (float)M_PI * 120.0f / (state->sample_rate > 0.0f ? state->sample_rate : 48000.0f));

    if (out_l && out_r) {
        for (jack_nframes_t i = 0; i < nframes; i++) {
            float l = out_l[i] * m_gain;
            float r = out_r[i] * m_gain;
            if (is_cal_master && state->room_cal_samples_left > 0) {
                float pn = next_pink_noise_sample(state);
                l += pn;
                r += pn;
                if (--state->room_cal_samples_left <= 0) {
                    state->room_cal_active = 0;
                    state->room_cal_done_flag = 1;
                }
            }
            /* Stage 6: Master Bus Analog Console Summing & Transformer Emulation */
            if (state->summing_enabled) {
                float xl = l + r * sum_xtalk_lin;
                float xr = r + l * sum_xtalk_lin;
                state->sum_lp_l += sum_lp_a * (xl - state->sum_lp_l);
                state->sum_lp_r += sum_lp_a * (xr - state->sum_lp_r);
                xl = (xl + state->sum_lp_l * sum_warmth_g) * sum_drive_lin;
                xr = (xr + state->sum_lp_r * sum_warmth_g) * sum_drive_lin;
                if (state->summing_console == 0) {
                    l = tanhf(xl * 1.15f + 0.015f * xl * xl) / 1.12f;
                    r = tanhf(xr * 1.15f + 0.015f * xr * xr) / 1.12f;
                } else if (state->summing_console == 1) {
                    l = xl / (1.0f + 0.28f * xl * xl);
                    r = xr / (1.0f + 0.28f * xr * xr);
                } else if (state->summing_console == 2) {
                    l = tanhf(xl);
                    r = tanhf(xr);
                } else {
                    l = tanhf(xl + 0.04f * xl * xl) * 0.96f;
                    r = tanhf(xr + 0.04f * xr * xr) * 0.96f;
                }
            }
            l = process_eq_strip(&state->master_eq, l, 0);
            r = process_eq_strip(&state->master_eq, r, 1);
            l = process_afd_bank64(&state->master_afd, l, 0);
            r = process_afd_bank64(&state->master_afd, r, 1);
            if (is_rta_master || is_cal_master) feed_rta_sample(state, 0.5f * (l + r));

            /* Slot A: Master Bus Compressor (Stereo-Linked) */
            float gr1 = 0.0f;
            float link1_in = (fabsf(l) >= fabsf(r)) ? l : r;
            float link1_out = process_dynamics_slot(&state->master_dyn, link1_in, &gr1);
            float cg1 = (fabsf(link1_in) > 0.00001f) ? (link1_out / link1_in) : 1.0f;
            l *= cg1;
            r *= cg1;

            /* Slot B: Master Speaker Limiter / Second Bus Compressor (Stereo-Linked) */
            float gr2 = 0.0f;
            float link2_in = (fabsf(l) >= fabsf(r)) ? l : r;
            float link2_out = process_dynamics_slot(&state->master_dyn2, link2_in, &gr2);
            float cg2 = (fabsf(link2_in) > 0.00001f) ? (link2_out / link2_in) : 1.0f;
            l *= cg2;
            r *= cg2;

            /* Brickwall Master Ceiling */
            float ceil_v = fminf(state->master_dyn.lim_ceil_lin, state->master_dyn2.lim_ceil_lin);
            if (ceil_v < 0.1f) ceil_v = 0.89125f;
            if (l >  ceil_v) l =  ceil_v;
            if (l < -ceil_v) l = -ceil_v;
            if (r >  ceil_v) r =  ceil_v;
            if (r < -ceil_v) r = -ceil_v;
            out_l[i] = l;
            out_r[i] = r;
            if (gr1 > master_gr_max) master_gr_max = gr1;
            if (gr2 > master_gr2_max) master_gr2_max = gr2;
            float al = fabsf(l);
            float ar = fabsf(r);
            if (al > max_l) max_l = al;
            if (ar > max_r) max_r = ar;
        }
    }
    if (state->master_l && !state->master_mute) {
        jack_default_audio_sample_t *in_l = (jack_default_audio_sample_t*)jack_port_get_buffer(state->master_l, nframes);
        for (jack_nframes_t i = 0; i < nframes; i++) {
            float abs_val = fabsf(in_l[i]) * m_gain;
            if (abs_val > max_l) max_l = abs_val;
        }
    }
    if (state->master_r && !state->master_mute) {
        jack_default_audio_sample_t *in_r = (jack_default_audio_sample_t*)jack_port_get_buffer(state->master_r, nframes);
        for (jack_nframes_t i = 0; i < nframes; i++) {
            float abs_val = fabsf(in_r[i]) * m_gain;
            if (abs_val > max_r) max_r = abs_val;
        }
    }
    state->master_peak_l = fmaxf(max_l, state->master_peak_l * 0.85f);
    if (state->master_peak_l < 0.0003f) state->master_peak_l = 0.0f;
    state->master_peak_r = fmaxf(max_r, state->master_peak_r * 0.85f);
    if (state->master_peak_r < 0.0003f) state->master_peak_r = 0.0f;

    state->master_gr_db = state->master_gr_db * 0.84f + master_gr_max * 0.16f;
    if (state->master_gr_db < 0.03f) state->master_gr_db = 0.0f;
    state->master_gr2_db = state->master_gr2_db * 0.84f + master_gr2_max * 0.16f;
    if (state->master_gr2_db < 0.03f) state->master_gr2_db = 0.0f;

    /* Convert 64-band RTA linear envelopes to dBFS (-80..0 dB) once per JACK block */
    for (int b = 0; b < NUM_RTA_BINS; b++) {
        float env = state->rta_env[b];
        float db = (env > 0.0001f) ? (20.0f * log10f(env)) : -80.0f;
        if (db < -80.0f) db = -80.0f;
        if (db > 0.0f) db = 0.0f;
        state->rta_db[b] = db;
    }

    return 0;
}

/* Lightweight JSON field extractors for real-time WebSocket control messages */
static int json_get_string(const char *json, const char *key, char *out, size_t out_sz) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < out_sz) out[i++] = *p++;
        out[i] = '\0';
        return 1;
    }
    return 0;
}

static int json_get_float(const char *json, const char *key, float *out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '+') {
        *out = strtof(p, NULL);
        return 1;
    }
    return 0;
}

static int json_get_bool_or_int(const char *json, const char *key, int *out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "true", 4) == 0) { *out = 1; return 1; }
    if (strncmp(p, "false", 5) == 0) { *out = 0; return 1; }
    if ((*p >= '0' && *p <= '9') || *p == '-') { *out = atoi(p); return 1; }
    return 0;
}

static int parse_target_channel(const char *json, char *str_target, size_t str_sz, int *ch_out) {
    if (json_get_string(json, "target", str_target, str_sz) || json_get_string(json, "channel", str_target, str_sz)) {
        if (str_target[0] >= '0' && str_target[0] <= '7' && str_target[1] == '\0') {
            *ch_out = str_target[0] - '0';
            return 1;
        }
        return 2; /* Named bus target in str_target */
    }
    if (json_get_bool_or_int(json, "target", ch_out) || json_get_bool_or_int(json, "channel", ch_out)) {
        if (*ch_out >= 0 && *ch_out < NUM_CHANNELS) return 1;
    }
    return 0;
}

static EqStrip8* resolve_eq_strip(int target_kind, int ch, const char *target_str) {
    if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) return &g_state.ch_eq[ch];
    if (target_kind == 2 && target_str) {
        if (strcmp(target_str, "master") == 0) return &g_state.master_eq;
        if (strncmp(target_str, "mon-", 4) == 0 || strncmp(target_str, "mon", 3) == 0) {
            const char *num = (target_str[3] == '-') ? (target_str + 4) : (target_str + 3);
            int m = atoi(num) - 1;
            if (m >= 0 && m < NUM_MONITORS) return &g_state.mon_eq[m];
        }
        if (strncmp(target_str, "send-", 5) == 0 || strncmp(target_str, "send", 4) == 0) {
            const char *num = (target_str[4] == '-') ? (target_str + 5) : (target_str + 4);
            int s = atoi(num) - 1;
            if (s >= 0 && s < NUM_FX) return &g_state.send_eq[s];
        }
        if (strncmp(target_str, "ret-", 4) == 0 || strncmp(target_str, "ret", 3) == 0) {
            const char *num = (target_str[3] == '-') ? (target_str + 4) : (target_str + 3);
            int r = atoi(num) - 1;
            if (r >= 0 && r < NUM_FX) return &g_state.ret_eq[r];
        }
    }
    return NULL;
}

static AfdNotchBank* resolve_afd_bank(int target_kind, int ch, const char *target_str) {
    if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) return &g_state.ch_afd[ch];
    if (target_kind == 2 && target_str) {
        if (strcmp(target_str, "master") == 0) return &g_state.master_afd;
        if (strncmp(target_str, "mon-", 4) == 0 || strncmp(target_str, "mon", 3) == 0) {
            const char *num = (target_str[3] == '-') ? (target_str + 4) : (target_str + 3);
            int m = atoi(num) - 1;
            if (m >= 0 && m < NUM_MONITORS) return &g_state.mon_afd[m];
        }
    }
    return NULL;
}

static DynamicsSlot* resolve_dynamics_slot(int target_kind, int ch, const char *target_str, const char *json) {
    int slot_idx = 1;
    if (json) json_get_bool_or_int(json, "slot", &slot_idx);
    if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) return &g_state.ch_dyn[ch];
    if (target_kind == 2 && target_str) {
        int is_slot_b = (slot_idx == 2 || strstr(target_str, "-b") != NULL || strstr(target_str, "_b") != NULL || strstr(target_str, "slotB") != NULL);
        if (strncmp(target_str, "master", 6) == 0) {
            return is_slot_b ? &g_state.master_dyn2 : &g_state.master_dyn;
        }
        if (strncmp(target_str, "mon-", 4) == 0 || strncmp(target_str, "mon", 3) == 0) {
            const char *num = (target_str[3] == '-') ? (target_str + 4) : (target_str + 3);
            int m = atoi(num) - 1;
            if (m >= 0 && m < NUM_MONITORS) {
                return is_slot_b ? &g_state.mon_dyn2[m] : &g_state.mon_dyn[m];
            }
        }
        if (strncmp(target_str, "send-", 5) == 0 || strncmp(target_str, "send", 4) == 0) {
            const char *num = (target_str[4] == '-') ? (target_str + 5) : (target_str + 4);
            int s = atoi(num) - 1;
            if (s >= 0 && s < NUM_FX) return &g_state.send_dyn[s];
        }
        if (strncmp(target_str, "ret-", 4) == 0 || strncmp(target_str, "ret", 3) == 0) {
            const char *num = (target_str[3] == '-') ? (target_str + 4) : (target_str + 3);
            int r = atoi(num) - 1;
            if (r >= 0 && r < NUM_FX) return &g_state.ret_dyn[r];
        }
    }
    return NULL;
}

static void apply_dynamics_param(DynamicsSlot *slot, const char *param, const char *json) {
    if (!slot) return;
    float fv = 0.0f;
    int iv = 0;
    char sv[32] = {0};

    if (strcmp(param, "compModel") == 0) {
        if (json_get_string(json, "value", sv, sizeof(sv))) {
            if (strcmp(sv, "lala") == 0 || strcmp(sv, "la2a") == 0) slot->comp_model = 1;
            else if (strcmp(sv, "dual") == 0) slot->comp_model = 2;
            else if (strcmp(sv, "limiter") == 0 || strcmp(sv, "buster") == 0) slot->comp_model = 3;
            else slot->comp_model = 0;
        }
    } else if (strcmp(param, "compBypass") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->comp_bypass = iv ? 1 : 0;
    } else if (strcmp(param, "compEnabled") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->comp_bypass = iv ? 0 : 1;
    } else if (strcmp(param, "fetish76_power") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->fet_power = iv ? 1 : 0;
    } else if (strcmp(param, "fetish76_input") == 0 || strcmp(param, "compInput") == 0) {
        if (json_get_float(json, "value", &fv)) slot->fet_input_db = fv;
    } else if (strcmp(param, "fetish76_output") == 0 || strcmp(param, "compOutput") == 0) {
        if (json_get_float(json, "value", &fv)) slot->fet_output_db = fv;
    } else if (strcmp(param, "fetish76_attack") == 0 || strcmp(param, "compAttack") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->fet_attack = iv;
    } else if (strcmp(param, "fetish76_release") == 0 || strcmp(param, "compRelease") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->fet_release = iv;
    } else if (strcmp(param, "fetRatio") == 0 || strcmp(param, "fetish76_ratio") == 0) {
        if (json_get_string(json, "value", sv, sizeof(sv))) {
            if (strcmp(sv, "ALL") == 0) slot->fet_ratio = 50.0f;
            else slot->fet_ratio = strtof(sv, NULL);
        } else if (json_get_float(json, "value", &fv)) {
            slot->fet_ratio = fv;
        }
    } else if (strcmp(param, "fetAllButtons") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->fet_ratio = iv ? 50.0f : 4.0f;
    } else if (strcmp(param, "lala_power") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->lala_power = iv;
    } else if (strcmp(param, "lala_peakReduction") == 0) {
        if (json_get_float(json, "value", &fv)) slot->lala_peak_red = fv;
    } else if (strcmp(param, "lala_gain") == 0) {
        if (json_get_float(json, "value", &fv)) slot->lala_gain = fv;
    } else if (strcmp(param, "lala_mix") == 0) {
        if (json_get_float(json, "value", &fv)) slot->lala_mix = fv;
    } else if (strcmp(param, "lalaMode") == 0) {
        if (json_get_string(json, "value", sv, sizeof(sv))) {
            slot->lala_limit = (strcmp(sv, "limit") == 0) ? 1 : 0;
        }
    } else if (strcmp(param, "limiter_power") == 0) {
        if (json_get_bool_or_int(json, "value", &iv)) slot->lim_power = iv;
    } else if (strcmp(param, "limiter_threshold") == 0 || strcmp(param, "buster_threshold") == 0) {
        if (json_get_float(json, "value", &fv)) slot->lim_thresh_db = fv;
    } else if (strcmp(param, "limiter_ceiling") == 0 || strcmp(param, "buster_ceiling") == 0) {
        if (json_get_float(json, "value", &fv)) slot->lim_ceil_lin = powf(10.0f, fv / 20.0f);
    } else if (strcmp(param, "limiter_drive") == 0 || strcmp(param, "buster_drive") == 0) {
        if (json_get_float(json, "value", &fv)) slot->lim_drive_lin = powf(10.0f, fv / 20.0f);
    } else if (strcmp(param, "limiter_release") == 0) {
        if (json_get_float(json, "value", &fv) && fv >= 1.0f) {
            slot->lim_rel_coeff = expf(-1.0f / (fv * 0.001f * g_state.sample_rate));
        }
    }
    update_dynamics_slot_coeffs(slot);
}

static void handle_ws_control_message(const char *json) {
    char action[64] = {0};
    char type[64] = {0};
    json_get_string(json, "action", action, sizeof(action));
    json_get_string(json, "type", type, sizeof(type));

    char target_str[64] = {0};
    int ch = -1;
    int target_kind = parse_target_channel(json, target_str, sizeof(target_str), &ch);

    if (strcmp(action, "reset_xruns") == 0) {
        g_state.jack_xruns = 0;
        return;
    }
    if (strcmp(action, "set_network_mode") == 0) {
        char mode[32] = {0};
        if (json_get_string(json, "mode", mode, sizeof(mode))) {
            if (strcmp(mode, "wifi") == 0) {
                snprintf(g_state.net_policy, sizeof(g_state.net_policy), "wifi");
                int rc = system("nohup sudo /usr/local/bin/patchbox-net-switch.sh wifi </dev/null >/tmp/net-switch.log 2>&1 &");
                (void)rc;
            } else if (strcmp(mode, "hotspot") == 0) {
                snprintf(g_state.net_policy, sizeof(g_state.net_policy), "hotspot");
                int rc = system("nohup sudo /usr/local/bin/patchbox-net-switch.sh hotspot </dev/null >/tmp/net-switch.log 2>&1 &");
                (void)rc;
            } else if (strcmp(mode, "auto") == 0) {
                snprintf(g_state.net_policy, sizeof(g_state.net_policy), "auto");
                int rc = system("nohup sudo /usr/local/bin/patchbox-net-switch.sh auto </dev/null >/tmp/net-switch.log 2>&1 &");
                (void)rc;
            }
        }
        return;
    }
    if (strcmp(action, "set_fader") == 0) {
        float gain = 1.0f;
        if (!json_get_float(json, "gain", &gain)) return;
        char sof[32] = {0};
        if (json_get_string(json, "sof", sof, sizeof(sof)) && ch >= 0 && ch < NUM_CHANNELS) {
            if (strncmp(sof, "mon", 3) == 0) {
                const char *num_p = (sof[3] == '-') ? (sof + 4) : (sof + 3);
                int m = atoi(num_p) - 1;
                if (m >= 0 && m < NUM_MONITORS) g_state.sof_mon_gain[m][ch] = gain;
                return;
            } else if (strncmp(sof, "fx", 2) == 0) {
                const char *num_p = (sof[2] == '-') ? (sof + 3) : (sof + 2);
                int fx = atoi(num_p) - 1;
                if (fx >= 0 && fx < NUM_FX) g_state.sof_fx_gain[fx][ch] = gain;
                return;
            } else if (strncmp(sof, "send", 4) == 0) {
                const char *num_p = (sof[4] == '-') ? (sof + 5) : (sof + 4);
                int fx = atoi(num_p) - 1;
                if (fx >= 0 && fx < NUM_FX) g_state.sof_fx_gain[fx][ch] = gain;
                return;
            }
        }
        if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) {
            g_state.ch_fader_gain[ch] = gain;
            g_state.peak_levels[ch] = g_state.ch_mute[ch] ? 0.0f : (g_state.ch_pre_levels[ch] * gain);
        } else if (target_kind == 2) {
            if (strcmp(target_str, "master") == 0) {
                float old_g = g_state.master_fader_gain;
                g_state.master_fader_gain = gain;
                if (old_g > 0.0001f) {
                    float ratio = gain / old_g;
                    g_state.master_peak_l *= ratio;
                    g_state.master_peak_r *= ratio;
                } else if (gain <= 0.0001f) {
                    g_state.master_peak_l = 0.0f;
                    g_state.master_peak_r = 0.0f;
                }
            } else if (strncmp(target_str, "mon-", 4) == 0) {
                int m = atoi(target_str + 4) - 1;
                if (m >= 0 && m < NUM_MONITORS) {
                    float old_g = g_state.mon_fader_gain[m];
                    g_state.mon_fader_gain[m] = gain;
                    if (old_g > 0.0001f) g_state.aux_peaks[m] *= (gain / old_g);
                    else if (gain <= 0.0001f) g_state.aux_peaks[m] = 0.0f;
                }
            } else if (strncmp(target_str, "send-", 5) == 0) {
                int s = atoi(target_str + 5) - 1;
                if (s >= 0 && s < NUM_FX) {
                    float old_g = g_state.send_fader_gain[s];
                    g_state.send_fader_gain[s] = gain;
                    if (old_g > 0.0001f) g_state.send_peaks[s] *= (gain / old_g);
                    else if (gain <= 0.0001f) g_state.send_peaks[s] = 0.0f;
                }
            } else if (strncmp(target_str, "ret-", 4) == 0) {
                int r = atoi(target_str + 4) - 1;
                if (r >= 0 && r < NUM_FX) {
                    float old_g = g_state.ret_fader_gain[r];
                    g_state.ret_fader_gain[r] = gain;
                    if (old_g > 0.0001f) g_state.ret_peaks[r] *= (gain / old_g);
                    else if (gain <= 0.0001f) g_state.ret_peaks[r] = 0.0f;
                }
            }
        }
    } else if (strcmp(action, "set_pan") == 0) {
        float pan = 0.0f;
        if (!json_get_float(json, "pan", &pan)) return;
        if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) {
            g_state.ch_pan[ch] = pan;
        } else if (target_kind == 2 && strncmp(target_str, "ret-", 4) == 0) {
            int r = atoi(target_str + 4) - 1;
            if (r >= 0 && r < NUM_FX) g_state.ret_pan[r] = pan;
        }
    } else if (strcmp(action, "set_mute") == 0) {
        int muted = 0;
        if (!json_get_bool_or_int(json, "muted", &muted)) return;
        if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) {
            g_state.ch_mute[ch] = muted;
            if (muted) {
                g_state.peak_levels[ch] = 0.0f;
                g_state.ch_pre_levels[ch] = 0.0f;
            }
        } else if (target_kind == 2) {
            if (strcmp(target_str, "master") == 0) {
                g_state.master_mute = muted;
                if (muted) { g_state.master_peak_l = 0.0f; g_state.master_peak_r = 0.0f; }
            } else if (strncmp(target_str, "mon-", 4) == 0) {
                int m = atoi(target_str + 4) - 1;
                if (m >= 0 && m < NUM_MONITORS) {
                    g_state.mon_mute[m] = muted;
                    if (muted) g_state.aux_peaks[m] = 0.0f;
                }
            } else if (strncmp(target_str, "send-", 5) == 0) {
                int s = atoi(target_str + 5) - 1;
                if (s >= 0 && s < NUM_FX) {
                    g_state.send_mute[s] = muted;
                    if (muted) g_state.send_peaks[s] = 0.0f;
                }
            } else if (strncmp(target_str, "ret-", 4) == 0) {
                int r = atoi(target_str + 4) - 1;
                if (r >= 0 && r < NUM_FX) {
                    g_state.ret_mute[r] = muted;
                    if (muted) g_state.ret_peaks[r] = 0.0f;
                }
            }
        }
    } else if (strcmp(action, "set_solo") == 0) {
        int solo = 0;
        if (!json_get_bool_or_int(json, "solo", &solo)) return;
        if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) {
            g_state.ch_solo[ch] = solo;
        } else if (target_kind == 2) {
            if (strncmp(target_str, "mon-", 4) == 0) {
                int m = atoi(target_str + 4) - 1;
                if (m >= 0 && m < NUM_MONITORS) g_state.mon_solo[m] = solo;
            } else if (strncmp(target_str, "send-", 5) == 0) {
                int s = atoi(target_str + 5) - 1;
                if (s >= 0 && s < NUM_FX) g_state.send_solo[s] = solo;
            } else if (strncmp(target_str, "ret-", 4) == 0) {
                int r = atoi(target_str + 4) - 1;
                if (r >= 0 && r < NUM_FX) g_state.ret_solo[r] = solo;
            }
        }
    } else if (strcmp(action, "clear_all_solos") == 0) {
        for (int i = 0; i < NUM_CHANNELS; i++) g_state.ch_solo[i] = 0;
        for (int i = 0; i < NUM_MONITORS; i++) g_state.mon_solo[i] = 0;
        for (int i = 0; i < NUM_FX; i++) { g_state.send_solo[i] = 0; g_state.ret_solo[i] = 0; }
    } else if (strcmp(action, "patch_input") == 0) {
        int patch_ch = ch;
        if (target_kind != 1) {
            json_get_bool_or_int(json, "channel", &patch_ch);
        }
        int hw_in = -1;
        char cap_str[64] = {0};
        if (json_get_string(json, "capture_port", cap_str, sizeof(cap_str)) && cap_str[0] != '\0') {
            if (strcmp(cap_str, "none") == 0 || strcmp(cap_str, "virtual") == 0) {
                hw_in = 0;
            } else {
                const char *us = strrchr(cap_str, '_');
                if (us) hw_in = atoi(us + 1);
            }
        } else {
            json_get_bool_or_int(json, "hw_in", &hw_in);
        }
        if (patch_ch >= 0 && patch_ch < NUM_CHANNELS && hw_in >= 0 && hw_in <= 32) {
            g_state.ch_input_patch[patch_ch] = hw_in;
            if (g_jack_client) {
                char my_port[48];
                snprintf(my_port, sizeof(my_port), "modep-meter-daemon:in_%d", patch_ch + 1);
                for (int p = 1; p <= 32; p++) {
                    char sys_p[48];
                    snprintf(sys_p, sizeof(sys_p), "system:capture_%d", p);
                    jack_disconnect(g_jack_client, sys_p, my_port);
                }
                if (hw_in > 0) {
                    char sys_p[48];
                    snprintf(sys_p, sizeof(sys_p), "system:capture_%d", hw_in);
                    jack_connect(g_jack_client, sys_p, my_port);
                }
            }
        }
    } else if (strcmp(action, "set_audio_interface") == 0) {
        char card_id[64] = {0};
        int rate = (int)(g_state.sample_rate > 8000.0f ? g_state.sample_rate : 48000.0f);
        int buf_sz = g_state.hw_buffer_size > 0 ? g_state.hw_buffer_size : 128;
        int cue_en = g_state.solo_cue_to_main;
        json_get_string(json, "card", card_id, sizeof(card_id));
        json_get_bool_or_int(json, "rate", &rate);
        json_get_bool_or_int(json, "buffer", &buf_sz);
        if (json_get_bool_or_int(json, "soloCue", &cue_en)) {
            g_state.solo_cue_to_main = cue_en ? 1 : 0;
        }
        if (card_id[0] != '\0') {
            for (int k = 0; card_id[k]; k++) {
                char c = card_id[k];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
                    card_id[k] = '\0';
                    break;
                }
            }
            if (rate != 44100 && rate != 48000 && rate != 96000) rate = 48000;
            if (buf_sz != 64 && buf_sz != 128 && buf_sz != 256 && buf_sz != 512) buf_sz = 128;
            FILE *cf = fopen("/etc/modep-audio.conf", "w");
            if (cf) {
                fprintf(cf, "MODEP_CARD=\"%s\"\nMODEP_RATE=\"%d\"\nMODEP_BUFFER=\"%d\"\n", card_id, rate, buf_sz);
                fclose(cf);
            }
            g_jack_restart_requested = 1;
        }
    } else if (strcmp(action, "set_hpf80") == 0) {
        int enabled = 0;
        if (json_get_bool_or_int(json, "channel", &ch) && json_get_bool_or_int(json, "enabled", &enabled)) {
            if (ch >= 0 && ch < NUM_CHANNELS) g_state.ch_hpf80[ch] = enabled;
        }
    } else if (strcmp(action, "set_ch_stereo") == 0) {
        int st_en = 0;
        if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS && json_get_bool_or_int(json, "stereo", &st_en)) {
            g_state.ch_stereo_active[ch] = st_en ? 1 : 0;
        }
    } else if (strcmp(action, "set_rta_target") == 0) {
        if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) {
            g_state.rta_target_kind = 1;
            g_state.rta_target_ch = ch;
        } else if (target_kind == 2) {
            g_state.rta_target_kind = 2;
            snprintf(g_state.rta_target_bus, sizeof(g_state.rta_target_bus), "%s", target_str);
        }
    } else if (strcmp(action, "set_eq_band") == 0) {
        EqStrip8 *eq = resolve_eq_strip(target_kind, ch, target_str);
        int band_idx = 0;
        if (eq && json_get_bool_or_int(json, "band", &band_idx) && band_idx >= 0 && band_idx < NUM_EQ_BANDS) {
            CytomicSvfBand *b = &eq->bands[band_idx];
            int en = b->enabled, tp = b->type, sl = b->slope, dyn_en = b->dyn_enabled;
            float fr = b->freq, gn = b->gain, qv = b->q;
            float d_th = b->dyn_thresh_db, d_rg = b->dyn_range_db, d_rt = b->dyn_ratio;
            if (json_get_bool_or_int(json, "enabled", &en)) b->enabled = en ? 1 : 0;
            if (json_get_bool_or_int(json, "type", &tp)) {
                if (tp < 0) tp = 0;
                if (tp > 9) tp = 9;
                b->type = tp;
            }
            if (json_get_bool_or_int(json, "slope", &sl)) {
                if (sl >= 12) sl = sl / 12;
                if (sl < 1) sl = 1;
                if (sl > 4) sl = 4;
                b->slope = sl;
            }
            if (json_get_float(json, "freq", &fr) && isfinite(fr)) b->freq = fmaxf(10.0f, fminf(22000.0f, fr));
            if (json_get_float(json, "gain", &gn) && isfinite(gn)) b->gain = fmaxf(-30.0f, fminf(30.0f, gn));
            if (json_get_float(json, "q", &qv) && isfinite(qv)) b->q = fmaxf(0.1f, fminf(50.0f, qv));
            if (json_get_bool_or_int(json, "dynEnabled", &dyn_en)) {
                b->dyn_enabled = dyn_en ? 1 : 0;
                if (!dyn_en) b->dyn_gain_db = 0.0f;
            }
            if (json_get_float(json, "dynThresh", &d_th) && isfinite(d_th)) b->dyn_thresh_db = fmaxf(-80.0f, fminf(12.0f, d_th));
            if (json_get_float(json, "dynRange", &d_rg) && isfinite(d_rg)) b->dyn_range_db = fmaxf(-36.0f, fminf(36.0f, d_rg));
            if (json_get_float(json, "dynRatio", &d_rt) && isfinite(d_rt)) b->dyn_ratio = fmaxf(1.0f, fminf(20.0f, d_rt));
            b->param_dirty = 1; /* Rule 27: JACK thread ramps + recomputes coeffs */
        }
    } else if (strcmp(action, "set_adaptive_room_dyneq") == 0) {
        EqStrip8 *eq = resolve_eq_strip(target_kind, ch, target_str);
        if (!eq) eq = &g_state.master_eq;
        int en = 1;
        if (json_get_bool_or_int(json, "enabled", &en)) {
            eq->adaptive_room_mode = en ? 1 : 0;
            for (int b = 1; b <= 6; b++) {
                eq->bands[b].dyn_enabled = en ? 1 : 0;
                if (!en) eq->bands[b].dyn_gain_db = 0.0f;
                eq->bands[b].param_dirty = 1; /* Rule 27 */
            }
        }
    } else if (strcmp(action, "set_afd_notch") == 0) {
        AfdNotchBank *bank = resolve_afd_bank(target_kind, ch, target_str);
        int idx = 0;
        if (bank && json_get_bool_or_int(json, "notch", &idx) && idx >= 0 && idx < NUM_AFD_NOTCHES) {
            AfdNotchFilter64 *n = &bank->notches[idx];
            int act = n->active, inf_n = n->inf_null;
            float fr = (float)n->freq, dp = (float)n->depth_db, qv = (float)n->q;
            if (json_get_bool_or_int(json, "active", &act)) n->active = act;
            if (json_get_bool_or_int(json, "infNull", &inf_n)) n->inf_null = inf_n;
            if (json_get_float(json, "freq", &fr)) n->freq = (double)fr;
            if (json_get_float(json, "depth", &dp)) n->depth_db = (double)dp;
            if (json_get_float(json, "q", &qv)) n->q = (double)qv;
            update_afd_notch64_coeffs(n, (double)(g_state.sample_rate > 0.0f ? g_state.sample_rate : 48000.0f));
        }
    } else if (strcmp(action, "clear_afd_notches") == 0) {
        AfdNotchBank *bank = resolve_afd_bank(target_kind, ch, target_str);
        if (bank) {
            for (int i = 0; i < NUM_AFD_NOTCHES; i++) {
                memset(&bank->notches[i], 0, sizeof(AfdNotchFilter64));
            }
        }
    } else if (strcmp(action, "denoise_learn") == 0) {
        if (ch < 0 || ch >= NUM_CHANNELS) {
            if (!json_get_bool_or_int(json, "channel", &ch)) ch = 0;
        }
        if (ch >= 0 && ch < NUM_CHANNELS) {
            for (int b = 0; b < 10; b++) g_state.denoise_learn_peaks[ch][b] = 0.0f;
            g_state.denoise_learn_samples_left[ch] = (int)((g_state.sample_rate > 8000.0f ? g_state.sample_rate : 48000.0f) * 1.1f);
            g_state.denoise_learn_active[ch] = 1;
        }
    } else if (strcmp(action, "start_room_cal") == 0) {
        char bus[32] = "master";
        json_get_string(json, "bus", bus, sizeof(bus));
        snprintf(g_state.room_cal_bus, sizeof(g_state.room_cal_bus), "%s", bus);
        g_state.room_cal_samples_left = (int)((g_state.sample_rate > 0.0f ? g_state.sample_rate : 48000.0f) * 2.0f);
        g_state.room_cal_done_flag = 0;
        g_state.room_cal_active = 1;
    } else if (strcmp(action, "tape_transport") == 0) {
        char cmd[32] = {0};
        json_get_string(json, "command", cmd, sizeof(cmd));
        if (strcmp(cmd, "rec") == 0) {
            g_state.ring_read_pos = g_state.ring_write_pos;
            g_state.tape_transport = 2;
        } else if (strcmp(cmd, "play") == 0) {
            g_state.tape_transport = 1;
        } else if (strcmp(cmd, "rtz") == 0) {
            g_state.tape_frame_pos = 0;
            g_state.tape_seek_requested = 1;
        } else if (strcmp(cmd, "stop") == 0) {
            g_state.tape_transport = 0;
            g_state.tape_seek_requested = 1;
        }
    } else if (strcmp(action, "tape_set_storage") == 0) {
        char path[192] = {0};
        int is_auto = 0;
        if (json_get_bool_or_int(json, "auto", &is_auto) && is_auto) {
            g_state.tape_dir_manual = 0;
            scan_recording_drives(&g_state);
        } else if (json_get_string(json, "path", path, sizeof(path)) && path[0] == '/') {
            for (int k = 0; path[k]; k++) {
                char c = path[k];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/' || c == '.')) {
                    path[k] = '_';
                }
            }
            g_state.tape_dir_manual = 1;
            mkdir_recursive(path);
            snprintf(g_state.tape_base_dir, sizeof(g_state.tape_base_dir), "%s", path);
            g_state.tape_free_gb = get_free_gb(g_state.tape_base_dir);
        }
        g_state.tape_seek_requested = 1;
        broadcast_tape_browser_state(&g_state, NULL);
    } else if (strcmp(action, "tape_set_track") == 0) {
        int trk = -1;
        if (json_get_bool_or_int(json, "track", &trk) && trk >= 0 && trk < MAX_TAPE_TRACKS) {
            float fv = 0.0f;
            int iv = 0;
            if (json_get_float(json, "vol", &fv)) g_state.tape_trk_vol[trk] = (fv < 0.0f ? 0.0f : (fv > 2.0f ? 2.0f : fv));
            if (json_get_float(json, "pan", &fv)) g_state.tape_trk_pan[trk] = (fv < -1.0f ? -1.0f : (fv > 1.0f ? 1.0f : fv));
            if (json_get_float(json, "drive", &fv)) g_state.tape_trk_drive_db[trk] = (fv < -6.0f ? -6.0f : (fv > 18.0f ? 18.0f : fv));
            if (json_get_bool_or_int(json, "mute", &iv)) g_state.tape_trk_mute[trk] = iv ? 1 : 0;
            if (json_get_bool_or_int(json, "solo", &iv)) g_state.tape_trk_solo[trk] = iv ? 1 : 0;
            if (json_get_bool_or_int(json, "rec", &iv))  g_state.tape_trk_rec[trk]  = iv ? 1 : 0;
        }
    } else if (strcmp(action, "tape_set_machine") == 0) {
        int iv = 0;
        float fv = 0.0f;
        if (json_get_bool_or_int(json, "ips", &iv)) g_state.tape_ips = (iv == 30) ? 30 : 15;
        if (json_get_bool_or_int(json, "emuEnabled", &iv)) g_state.tape_emu_enabled = iv ? 1 : 0;
        if (json_get_bool_or_int(json, "printWav", &iv)) g_state.tape_print_wav = iv ? 1 : 0;
        if (json_get_float(json, "masterVol", &fv)) g_state.tape_master_vol = (fv < 0.0f ? 0.0f : (fv > 2.0f ? 2.0f : fv));
        if (json_get_bool_or_int(json, "monIdx", &iv) && iv >= 0 && iv < NUM_MONITORS && json_get_float(json, "monSend", &fv)) {
            g_state.tape_mon_send[iv] = (fv < 0.0f ? 0.0f : (fv > 2.0f ? 2.0f : fv));
        }
    } else if (strcmp(action, "tape_add_track") == 0) {
        int src_ch = 0;
        json_get_bool_or_int(json, "sourceCh", &src_ch);
        if (src_ch < 0 || src_ch >= NUM_CHANNELS) src_ch = 0;
        /* Smart Overdub Arming: disarm existing tracks so they play back safely, and arm only the newly added track pair! */
        if (g_state.tape_num_tracks + 2 <= MAX_TAPE_TRACKS) {
            for (int t = 0; t < g_state.tape_num_tracks; t++) {
                g_state.tape_trk_rec[t] = 0;
            }
            int tp = g_state.tape_num_tracks;
            int td = g_state.tape_num_tracks + 1;
            g_state.tape_trk_src[tp] = src_ch;
            g_state.tape_trk_is_direct[tp] = 0;
            g_state.tape_trk_mute[tp] = 0;
            g_state.tape_trk_rec[tp] = 1;
            g_state.tape_trk_vol[tp] = 1.0f;
            g_state.tape_trk_drive_db[tp] = 1.5f;

            g_state.tape_trk_src[td] = src_ch;
            g_state.tape_trk_is_direct[td] = 1;
            g_state.tape_trk_mute[td] = 1; /* Direct safety track muted by default on playback */
            g_state.tape_trk_rec[td] = 1;
            g_state.tape_trk_vol[td] = 1.0f;
            g_state.tape_trk_drive_db[td] = 0.0f;
            g_state.tape_num_tracks += 2;
        }
    } else if (strcmp(action, "tape_song_op") == 0) {
        char op[32] = {0};
        char sname[64] = {0};
        char bpath[256] = {0};
        json_get_string(json, "op", op, sizeof(op));
        json_get_string(json, "path", bpath, sizeof(bpath));
        if (json_get_string(json, "song", sname, sizeof(sname)) && sname[0] != '\0') {
            for (int k = 0; sname[k]; k++) {
                char c = sname[k];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
                    sname[k] = '_';
                }
            }
            if (strcmp(op, "delete_song") != 0) {
                snprintf(g_state.tape_song_name, sizeof(g_state.tape_song_name), "%s", sname);
            }
        }
        if (strcmp(op, "list") == 0 || strcmp(op, "browse") == 0) {
            broadcast_tape_browser_state(&g_state, bpath[0] ? bpath : NULL);
        } else if (strcmp(op, "new") == 0) {
            g_state.tape_transport = 0;
            g_state.tape_frame_pos = 0;
            g_state.tape_max_frames = 0;
            for (int t = 0; t < MAX_TAPE_TRACKS; t++) {
                if (g_state.tape_play_ring[t]) memset(g_state.tape_play_ring[t], 0, sizeof(float) * DISK_RING_FRAMES);
            }
            char song_dir[256];
            snprintf(song_dir, sizeof(song_dir), "%s/%s", g_state.tape_base_dir, g_state.tape_song_name);
            mkdir_recursive(song_dir);
            g_state.tape_seek_requested = 1;
            broadcast_tape_browser_state(&g_state, NULL);
        } else if (strcmp(op, "bounce") == 0 || strcmp(op, "save") == 0) {
            write_song_folder_wav_stems(&g_state);
            broadcast_tape_browser_state(&g_state, NULL);
        } else if (strcmp(op, "load") == 0) {
            load_song_folder_wav_stems(&g_state);
            broadcast_tape_browser_state(&g_state, NULL);
        } else if (strcmp(op, "delete_song") == 0 && sname[0] != '\0') {
            char song_dir[256];
            snprintf(song_dir, sizeof(song_dir), "%s/%s", g_state.tape_base_dir, sname);
            DIR *d = opendir(song_dir);
            if (d) {
                struct dirent *de;
                while ((de = readdir(d)) != NULL) {
                    if (de->d_name[0] == '.') continue;
                    char fp[320];
                    snprintf(fp, sizeof(fp), "%s/%s", song_dir, de->d_name);
                    unlink(fp);
                }
                closedir(d);
                rmdir(song_dir);
            }
            if (strcmp(g_state.tape_song_name, sname) == 0) {
                snprintf(g_state.tape_song_name, sizeof(g_state.tape_song_name), "Song_01");
                g_state.tape_seek_requested = 1;
            }
            broadcast_tape_browser_state(&g_state, NULL);
        } else if (strcmp(op, "delete_file") == 0) {
            char fname[128] = {0};
            if (json_get_string(json, "file", fname, sizeof(fname)) && fname[0] != '\0' && strchr(fname, '/') == NULL) {
                char fp[320];
                snprintf(fp, sizeof(fp), "%s/%s/%s", g_state.tape_base_dir, g_state.tape_song_name, fname);
                unlink(fp);
                g_state.tape_seek_requested = 1;
                broadcast_tape_browser_state(&g_state, NULL);
            }
        }
    } else if (strcmp(type, "dsp_param") == 0) {
        char param[64] = {0};
        if (!json_get_string(json, "param", param, sizeof(param))) return;

        /* Route Unified Dynamics / Limiter parameters for Channels AND Dual-Slot Buses */
        DynamicsSlot *dyn_slot = resolve_dynamics_slot(target_kind, ch, target_str, json);
        if (dyn_slot) {
            apply_dynamics_param(dyn_slot, param, json);
        }

        /* Stage 5B Dual Stereo Reverbs (rev[0..1]), Dual Stereo Delays (dly[0..1]) & Stage 6 Master Console Summing */
        float fv = 0.0f;
        int   iv = 0;
        char  sv[32] = {0};
        int unit_idx = 0;
        if (!json_get_bool_or_int(json, "unit", &unit_idx)) {
            if (strcmp(target_str, "send-2") == 0 || strcmp(target_str, "ret-2") == 0 ||
                strcmp(target_str, "send-4") == 0 || strcmp(target_str, "ret-4") == 0 ||
                strcmp(target_str, "reverb2") == 0 || strcmp(target_str, "delay2") == 0) {
                unit_idx = 1;
            }
        }
        int u = (unit_idx == 1) ? 1 : 0;
        struct ReverbUnit *rv = &g_state.rev[u];
        struct DelayUnit  *dl = &g_state.dly[u];

        #define NORM_PCT(v) (((v) > 0.0f && (v) <= 1.0f) ? ((v) * 100.0f) : (v))
        #define CLAMP_PCT(v) ((v) < 0.0f ? 0.0f : ((v) > 100.0f ? 100.0f : (v)))

        if (strcmp(param, "summingEnabled") == 0 && json_get_bool_or_int(json, "value", &iv)) {
            g_state.summing_enabled = iv ? 1 : 0;
        } else if (strcmp(param, "summingConsole") == 0 && json_get_string(json, "value", sv, sizeof(sv))) {
            if (strcmp(sv, "api") == 0) g_state.summing_console = 1;
            else if (strcmp(sv, "ssl") == 0) g_state.summing_console = 2;
            else if (strcmp(sv, "tube") == 0) g_state.summing_console = 3;
            else g_state.summing_console = 0;
        } else if (strcmp(param, "summingDrive") == 0 && json_get_float(json, "value", &fv)) {
            g_state.summing_drive_db = (fv < 0.0f ? 0.0f : (fv > 12.0f ? 12.0f : fv));
        } else if (strcmp(param, "summingWarmth") == 0 && json_get_float(json, "value", &fv)) {
            g_state.summing_warmth_db = (fv < 0.0f ? 0.0f : (fv > 10.0f ? 10.0f : fv));
        } else if (strcmp(param, "summingCrosstalk") == 0 && json_get_float(json, "value", &fv)) {
            g_state.summing_crosstalk_db = (fv < -90.0f ? -90.0f : (fv > -30.0f ? -30.0f : fv));
        } else if (strcmp(param, "reverbDecay") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->decay = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbSize") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->size = CLAMP_PCT(fv);
            if (rv->decay <= 0.1f) rv->decay = rv->size;
        } else if (strcmp(param, "reverbDamp") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->damp = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbPreDelay") == 0 && json_get_float(json, "value", &fv)) {
            rv->predelay = (fv < 0.0f ? 0.0f : (fv > 500.0f ? 500.0f : fv));
        } else if (strcmp(param, "reverbDiffuse") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->diffuse = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbIrMix") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->ir_mix = CLAMP_PCT(fv);
        } else if ((strcmp(param, "reverbStereo") == 0 || strcmp(param, "reverbWidth") == 0) && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->stereo = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbDry") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->dry = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbWet") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->wet = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbPower") == 0 && json_get_bool_or_int(json, "value", &iv)) {
            g_state.ret_mute[u] = iv ? 0 : 1;
        } else if ((strcmp(param, "reverbLowCut") == 0 || strcmp(param, "reverbInLowCut") == 0) && json_get_float(json, "value", &fv)) {
            rv->in_lowcut = (fv < 20.0f ? 20.0f : (fv > 1000.0f ? 1000.0f : fv));
        } else if ((strcmp(param, "reverbHighCut") == 0 || strcmp(param, "reverbInHighCut") == 0) && json_get_float(json, "value", &fv)) {
            rv->in_highcut = (fv < 800.0f ? 800.0f : (fv > 18000.0f ? 18000.0f : fv));
        } else if (strcmp(param, "reverbOutLowCut") == 0 && json_get_float(json, "value", &fv)) {
            rv->out_lowcut = (fv < 20.0f ? 20.0f : (fv > 1000.0f ? 1000.0f : fv));
        } else if (strcmp(param, "reverbOutHighCut") == 0 && json_get_float(json, "value", &fv)) {
            rv->out_highcut = (fv < 800.0f ? 800.0f : (fv > 18000.0f ? 18000.0f : fv));
        } else if (strcmp(param, "reverbModRate") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->mod_rate = CLAMP_PCT(fv);
        } else if ((strcmp(param, "reverbModAmount") == 0 || strcmp(param, "reverbModDepth") == 0) && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->mod_amount = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbDucking") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            rv->ducking = CLAMP_PCT(fv);
        } else if (strcmp(param, "reverbLofiBits") == 0 && json_get_float(json, "value", &fv)) {
            rv->lofi_bits = (fv < 4.0f ? 4.0f : (fv > 24.0f ? 24.0f : fv));
        } else if (strcmp(param, "reverbLofiSR") == 0 && json_get_float(json, "value", &fv)) {
            rv->lofi_sr = (fv < 4000.0f ? 4000.0f : (fv > 48000.0f ? 48000.0f : fv));
        } else if ((strcmp(param, "delayTimeMs") == 0 || strcmp(param, "delayTime") == 0) && json_get_float(json, "value", &fv)) {
            dl->time_ms = (fv < 10.0f ? 10.0f : (fv > 1800.0f ? 1800.0f : fv));
        } else if (strcmp(param, "delayFeedback") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->feedback = (fv < 0.0f ? 0.0f : (fv > 95.0f ? 95.0f : fv));
        } else if (strcmp(param, "delayDiffuse") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->diffuse = CLAMP_PCT(fv);
        } else if (strcmp(param, "delayDry") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->dry = CLAMP_PCT(fv);
        } else if (strcmp(param, "delayWet") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->wet = CLAMP_PCT(fv);
        } else if (strcmp(param, "delayPower") == 0 && json_get_bool_or_int(json, "value", &iv)) {
            g_state.ret_mute[2 + u] = iv ? 0 : 1;
        } else if (strcmp(param, "delayPingPong") == 0 && json_get_bool_or_int(json, "value", &iv)) {
            dl->ping_pong = iv ? 1 : 0;
        } else if ((strcmp(param, "delayLowCut") == 0 || strcmp(param, "delayInLowCut") == 0) && json_get_float(json, "value", &fv)) {
            dl->in_lowcut = (fv < 20.0f ? 20.0f : (fv > 1000.0f ? 1000.0f : fv));
        } else if ((strcmp(param, "delayHighCut") == 0 || strcmp(param, "delayInHighCut") == 0) && json_get_float(json, "value", &fv)) {
            dl->in_highcut = (fv < 800.0f ? 800.0f : (fv > 18000.0f ? 18000.0f : fv));
        } else if (strcmp(param, "delayOutLowCut") == 0 && json_get_float(json, "value", &fv)) {
            dl->out_lowcut = (fv < 20.0f ? 20.0f : (fv > 1000.0f ? 1000.0f : fv));
        } else if (strcmp(param, "delayOutHighCut") == 0 && json_get_float(json, "value", &fv)) {
            dl->out_highcut = (fv < 800.0f ? 800.0f : (fv > 18000.0f ? 18000.0f : fv));
        } else if (strcmp(param, "delayDrive") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->drive = CLAMP_PCT(fv);
        } else if (strcmp(param, "delayFlutter") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->flutter = CLAMP_PCT(fv);
        } else if (strcmp(param, "delayModRate") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->mod_rate = CLAMP_PCT(fv);
        } else if (strcmp(param, "delayModDepth") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->mod_depth = CLAMP_PCT(fv);
        } else if (strcmp(param, "delayDucking") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->ducking = CLAMP_PCT(fv);
        } else if ((strcmp(param, "delayStereo") == 0 || strcmp(param, "delayWidth") == 0) && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->stereo = CLAMP_PCT(fv);
        } else if (strcmp(param, "delaySpread") == 0 && json_get_float(json, "value", &fv)) {
            fv = NORM_PCT(fv);
            dl->spread = CLAMP_PCT(fv);
        }

        if (target_kind == 1 && ch >= 0 && ch < NUM_CHANNELS) {
            if (strcmp(param, "tunerMute") == 0) {
                int tm = 0;
                if (json_get_bool_or_int(json, "value", &tm)) g_state.ch_tuner_mute[ch] = tm ? 1 : 0;
            } else if (strcmp(param, "slotFirst") == 0) {
                int sf = 0;
                if (json_get_bool_or_int(json, "value", &sf)) g_state.slot_first[ch] = sf ? 1 : 0;
            } else if (strcmp(param, "modguiFirst") == 0) {
                int mf = 0;
                if (json_get_bool_or_int(json, "value", &mf)) g_state.modgui_first[ch] = mf ? 1 : 0;
            }
            /* Stage 1: Auto-Compensated 1176 FET Class-A Preamp Emulation Parameters */
            else if (strcmp(param, "preamp1176Enabled") == 0) {
                int en = 1;
                if (json_get_bool_or_int(json, "value", &en)) g_state.preamp_enabled[ch] = en;
            } else if (strcmp(param, "preamp1176Drive") == 0) {
                float db = 6.0f;
                if (json_get_float(json, "value", &db)) {
                    if (db < 0.0f) db = 0.0f;
                    if (db > 24.0f) db = 24.0f;
                    g_state.preamp_drive_db[ch] = db;
                    g_state.preamp_drive_lin[ch] = powf(10.0f, db / 20.0f);
                    float ref_in = 0.25f * g_state.preamp_drive_lin[ch];
                    g_state.preamp_auto_comp[ch] = 1.0f / fmaxf(0.25f, (0.89125f * tanhf(ref_in / 0.89125f)) / 0.25f);
                }
            } else if (strcmp(param, "preamp1176Trim") == 0) {
                float db = 0.0f;
                if (json_get_float(json, "value", &db)) {
                    if (db < -24.0f) db = -24.0f;
                    if (db > 12.0f) db = 12.0f;
                    g_state.preamp_trim_db[ch] = db;
                    g_state.preamp_trim_lin[ch] = powf(10.0f, db / 20.0f);
                }
            } else if (strcmp(param, "preamp1176ClipMode") == 0) {
                char mode_str[32] = {0};
                int mode_int = 0;
                if (json_get_string(json, "value", mode_str, sizeof(mode_str))) {
                    g_state.preamp_clip_hard[ch] = (strcmp(mode_str, "hard") == 0) ? 1 : 0;
                } else if (json_get_bool_or_int(json, "value", &mode_int)) {
                    g_state.preamp_clip_hard[ch] = mode_int ? 1 : 0;
                }
            }
            /* Stage 2: Digital Pre-Gain & Polarity Invert */
            else if (strcmp(param, "phaseInvert") == 0) {
                int inv = 0;
                if (json_get_bool_or_int(json, "value", &inv)) g_state.ch_phase_inv[ch] = inv;
            } else if (strcmp(param, "pregainDb") == 0) {
                float db = 0.0f;
                if (json_get_float(json, "value", &db)) g_state.ch_pregain_lin[ch] = powf(10.0f, db / 20.0f);
            }
            /* Stage 3: 10-Band Cyber-Denoiser PRO (Matched to CyberDenoiserEngine.hpp) */
            else if (strcmp(param, "denoiseBypass") == 0) {
                int byp = 0;
                if (json_get_bool_or_int(json, "value", &byp)) g_state.gate_enabled[ch] = !byp;
            } else if (strcmp(param, "denoiseSensitivity") == 0) {
                float db = 0.0f;
                if (json_get_float(json, "value", &db)) {
                    if (db < -20.0f) db = -20.0f;
                    if (db > 20.0f) db = 20.0f;
                    g_state.denoise_sensitivity_db[ch] = db;
                }
            } else if (strcmp(param, "denoiseThresh") == 0) {
                float db = -45.0f;
                if (json_get_float(json, "value", &db)) {
                    g_state.gate_thresh_lin[ch] = powf(10.0f, db / 20.0f);
                    float sens = (db + 45.0f) * 0.55f;
                    if (sens < -20.0f) sens = -20.0f;
                    if (sens > 20.0f) sens = 20.0f;
                    g_state.denoise_sensitivity_db[ch] = sens;
                }
            } else if (strcmp(param, "denoiseReduction") == 0) {
                float pct = 85.0f;
                if (json_get_float(json, "value", &pct)) {
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 100.0f) pct = 100.0f;
                    g_state.denoise_reduction_pct[ch] = pct;
                }
            } else if (strcmp(param, "denoiseReduc") == 0) {
                float val = -18.0f;
                if (json_get_float(json, "value", &val)) {
                    if (val <= 0.0f) {
                        g_state.gate_floor_lin[ch] = powf(10.0f, val / 20.0f);
                        float pct = (-val / 40.0f) * 100.0f;
                        if (pct < 0.0f) pct = 0.0f;
                        if (pct > 100.0f) pct = 100.0f;
                        g_state.denoise_reduction_pct[ch] = pct;
                    } else {
                        if (val > 100.0f) val = 100.0f;
                        g_state.denoise_reduction_pct[ch] = val;
                    }
                }
            } else if (strcmp(param, "denoiseLowCut") == 0) {
                int lc = 1;
                if (json_get_bool_or_int(json, "value", &lc)) g_state.denoise_low_cut[ch] = lc ? 1 : 0;
            } else if (strcmp(param, "denoiseListen") == 0) {
                int lsn = 0;
                if (json_get_bool_or_int(json, "value", &lsn)) g_state.denoise_listen[ch] = lsn ? 1 : 0;
            } else if (strncmp(param, "denoiseBand_", 12) == 0) {
                int b_idx = atoi(param + 12);
                float t_db = -60.0f;
                if (b_idx >= 0 && b_idx < 10 && json_get_float(json, "value", &t_db)) {
                    if (t_db < -100.0f) t_db = -100.0f;
                    if (t_db > 0.0f) t_db = 0.0f;
                    g_state.denoise_thresh_db[ch][b_idx] = t_db;
                }
            }
            /* Stage 6: Dual-Band Zero-Phase Cytomic SVF De-Esser */
            else if (strcmp(param, "deesserEnabled") == 0 || strcmp(param, "loadesEnabled") == 0) {
                int en = 1;
                if (json_get_bool_or_int(json, "value", &en)) g_state.deesser_enabled[ch] = en ? 1 : 0;
            } else if (strcmp(param, "loadesThresh") == 0 || strcmp(param, "loadesThresh1") == 0) {
                float db = -22.0f;
                if (json_get_float(json, "value", &db)) {
                    g_state.deesser_thresh1_db[ch] = db;
                    g_state.deesser_thresh_lin[ch] = powf(10.0f, db / 20.0f);
                }
            } else if (strcmp(param, "loadesFreq1") == 0) {
                float fq = 4800.0f;
                if (json_get_float(json, "value", &fq)) {
                    g_state.deesser_freq1[ch] = fq;
                    g_state.deess_svf1[ch].freq = fq;
                    g_state.deess_svf1[ch].param_dirty = 1;
                }
            } else if (strcmp(param, "loadesQ1") == 0) {
                float qv = 1.8f;
                if (json_get_float(json, "value", &qv)) {
                    g_state.deesser_q1[ch] = qv;
                    g_state.deess_svf1[ch].q = qv;
                    g_state.deess_svf1[ch].param_dirty = 1;
                }
            } else if (strcmp(param, "loadesMax1") == 0 || strcmp(param, "loadesMaxGr1") == 0) {
                float mdb = 14.0f;
                if (json_get_float(json, "value", &mdb)) g_state.deesser_max1_db[ch] = mdb;
            } else if (strcmp(param, "loadesBand2En") == 0) {
                int b2 = 1;
                if (json_get_bool_or_int(json, "value", &b2)) g_state.deesser_band2_en[ch] = b2 ? 1 : 0;
            } else if (strcmp(param, "loadesThresh2") == 0) {
                float db = -24.0f;
                if (json_get_float(json, "value", &db)) {
                    g_state.deesser_thresh2_db[ch] = db;
                    g_state.deesser_thresh2_lin[ch] = powf(10.0f, db / 20.0f);
                }
            } else if (strcmp(param, "loadesFreq2") == 0) {
                float fq = 9500.0f;
                if (json_get_float(json, "value", &fq)) {
                    g_state.deesser_freq2[ch] = fq;
                    g_state.deess_svf2[ch].freq = fq;
                    g_state.deess_svf2[ch].param_dirty = 1;
                }
            } else if (strcmp(param, "loadesQ2") == 0) {
                float qv = 1.6f;
                if (json_get_float(json, "value", &qv)) {
                    g_state.deesser_q2[ch] = qv;
                    g_state.deess_svf2[ch].q = qv;
                    g_state.deess_svf2[ch].param_dirty = 1;
                }
            } else if (strcmp(param, "loadesMax2") == 0 || strcmp(param, "loadesMaxGr2") == 0) {
                float mdb = 12.0f;
                if (json_get_float(json, "value", &mdb)) g_state.deesser_max2_db[ch] = mdb;
            } else if (strcmp(param, "loadesRelease") == 0) {
                float ms = 50.0f;
                if (json_get_float(json, "value", &ms) && ms >= 5.0f) {
                    g_state.deesser_rel_coeff[ch] = expf(-1.0f / (ms * 0.001f * g_state.sample_rate));
                }
            } else if (strcmp(param, "loadesMode") == 0) {
                char mode_str[32] = {0};
                if (json_get_string(json, "value", mode_str, sizeof(mode_str))) {
                    g_state.deesser_wide[ch] = (strcmp(mode_str, "wide") == 0) ? 1 : 0;
                }
            } else if (strcmp(param, "loadesListen") == 0) {
                int listen = 0;
                if (json_get_bool_or_int(json, "value", &listen)) g_state.deesser_listen[ch] = listen;
            }
        }
    } else if (strcmp(action, "sync_linear_lv2_slots") == 0) {
        int ch = -1;
        if (!json_get_bool_or_int(json, "channel", &ch)) json_get_bool_or_int(json, "ch", &ch);
        if (ch >= 0 && ch < NUM_CHANNELS) {
            int seen[NUM_CH_SLOTS] = {0, 0, 0, 0};
            const char *slots_arr = strstr(json, "\"slots\"");
            if (slots_arr) {
                const char *p = strchr(slots_arr, '[');
                if (p) {
                    p++;
                    while (*p && *p != ']') {
                        const char *obj_start = strchr(p, '{');
                        if (!obj_start) break;
                        int depth = 0;
                        const char *q = obj_start;
                        while (*q) {
                            if (*q == '{') depth++;
                            else if (*q == '}') {
                                depth--;
                                if (depth == 0) break;
                            }
                            q++;
                        }
                        if (*q != '}') break;
                        size_t obj_len = (size_t)(q - obj_start + 1);
                        char obj_buf[2048];
                        if (obj_len >= sizeof(obj_buf)) obj_len = sizeof(obj_buf) - 1;
                        memcpy(obj_buf, obj_start, obj_len);
                        obj_buf[obj_len] = '\0';

                        int s_idx = -1, byp = 0;
                        char uri_str[160] = {0}, name_str[80] = {0};
                        json_get_bool_or_int(obj_buf, "slot", &s_idx);
                        json_get_string(obj_buf, "uri", uri_str, sizeof(uri_str));
                        json_get_string(obj_buf, "name", name_str, sizeof(name_str));
                        json_get_bool_or_int(obj_buf, "bypass", &byp);

                        if (s_idx >= 0 && s_idx < NUM_CH_SLOTS && uri_str[0] != '\0') {
                            seen[s_idx] = 1;
                            SlotDspUnit *u = &g_state.ch_slots[ch][s_idx];
                            if (!u->active || strcmp(u->uri, uri_str) != 0) {
                                init_slot_dsp_unit(u, uri_str, name_str);
                            }
                            u->bypass = byp ? 1 : 0;
                            char fpath_buf[256] = {0}, flabel_buf[128] = {0};
                            if (json_get_string(obj_buf, "file_path", fpath_buf, sizeof(fpath_buf)) ||
                                json_get_string(obj_buf, "_file_path", fpath_buf, sizeof(fpath_buf))) {
                                json_get_string(obj_buf, "file_label", flabel_buf, sizeof(flabel_buf));
                                snprintf(u->file_path, sizeof(u->file_path), "%s", fpath_buf);
                                snprintf(u->file_label, sizeof(u->file_label), "%s", flabel_buf);
                                apply_slot_model_voicing(u, fpath_buf, flabel_buf);
                            }
                            const char *prm = strstr(obj_buf, "\"params\"");
                            if (prm) {
                                const char *pb = strchr(prm, '{');
                                const char *pe = pb ? strchr(pb, '}') : NULL;
                                if (pb && pe && pe > pb) {
                                    const char * cur = pb + 1;
                                    while (cur < pe) {
                                        const char *k1 = strchr(cur, '"');
                                        if (!k1 || k1 >= pe) break;
                                        const char *k2 = strchr(k1 + 1, '"');
                                        if (!k2 || k2 >= pe) break;
                                        char sym[64] = {0};
                                        size_t klen = (size_t)(k2 - (k1 + 1));
                                        if (klen >= sizeof(sym)) klen = sizeof(sym) - 1;
                                        memcpy(sym, k1 + 1, klen);
                                        const char *colon = strchr(k2, ':');
                                        if (!colon || colon >= pe) break;
                                        const char *vstart = colon + 1;
                                        while (vstart < pe && (*vstart == ' ' || *vstart == '\t')) vstart++;
                                        float fval = 0.0f;
                                        if (strncmp(vstart, "true", 4) == 0) fval = 1.0f;
                                        else if (strncmp(vstart, "false", 5) == 0) fval = 0.0f;
                                        else fval = strtof(vstart, NULL);
                                        apply_slot_param(u, sym, fval);
                                        const char *comma = strchr(vstart, ',');
                                        if (!comma || comma >= pe) break;
                                        cur = comma + 1;
                                    }
                                }
                            }
                        }
                        p = q + 1;
                    }
                }
            }
            for (int s = 0; s < NUM_CH_SLOTS; s++) {
                if (!seen[s]) {
                    unload_native_lv2_slot(&g_state.ch_slots[ch][s]);
                    g_state.ch_slots[ch][s].active = 0;
                    g_state.ch_slots[ch][s].uri[0] = '\0';
                }
            }
        }
    } else if (strcmp(action, "set_lv2_slot_param") == 0) {
        int ch = -1, s_idx = -1;
        if (!json_get_bool_or_int(json, "channel", &ch)) json_get_bool_or_int(json, "ch", &ch);
        json_get_bool_or_int(json, "slot", &s_idx);
        if (ch >= 0 && ch < NUM_CHANNELS && s_idx >= 0 && s_idx < NUM_CH_SLOTS) {
            char uri_str[160] = {0}, sym_str[64] = {0};
            json_get_string(json, "uri", uri_str, sizeof(uri_str));
            json_get_string(json, "symbol", sym_str, sizeof(sym_str));
            SlotDspUnit *u = &g_state.ch_slots[ch][s_idx];
            if (!u->active && uri_str[0] != '\0') {
                init_slot_dsp_unit(u, uri_str, uri_str);
            }
            float val = 0.0f;
            int bval = 0;
            if (json_get_float(json, "value", &val)) {
                apply_slot_param(u, sym_str, val);
            } else if (json_get_bool_or_int(json, "value", &bval)) {
                apply_slot_param(u, sym_str, (float)bval);
            }
            mark_live_state_dirty();
        }
    } else if (strcmp(action, "load_lv2_slot_preset") == 0) {
        int ch = -1, s_idx = -1;
        if (!json_get_bool_or_int(json, "channel", &ch)) json_get_bool_or_int(json, "ch", &ch);
        json_get_bool_or_int(json, "slot", &s_idx);
        if (ch >= 0 && ch < NUM_CHANNELS && s_idx >= 0 && s_idx < NUM_CH_SLOTS) {
            char preset_uri[160] = {0};
            json_get_string(json, "preset_uri", preset_uri, sizeof(preset_uri));
            SlotDspUnit *u = &g_state.ch_slots[ch][s_idx];
            if (str_icontains(preset_uri, "lead") || str_icontains(preset_uri, "high") || str_icontains(preset_uri, "heavy") || str_icontains(preset_uri, "fuzz")) {
                u->p_drive = 0.82f; u->p_tone = 0.65f; u->sw_bright = 1;
            } else if (str_icontains(preset_uri, "clean") || str_icontains(preset_uri, "warm") || str_icontains(preset_uri, "soft")) {
                u->p_drive = 0.32f; u->p_tone = 0.45f; u->sw_bright = 0;
            } else {
                u->p_drive = 0.60f; u->p_tone = 0.58f;
            }
            mark_live_state_dirty();
        }
    } else if (strcmp(action, "set_lv2_slot_file") == 0) {
        int ch = -1, s_idx = -1;
        if (!json_get_bool_or_int(json, "channel", &ch)) json_get_bool_or_int(json, "ch", &ch);
        json_get_bool_or_int(json, "slot", &s_idx);
        if (ch >= 0 && ch < NUM_CHANNELS && s_idx >= 0 && s_idx < NUM_CH_SLOTS) {
            char fpath[256] = {0}, flabel[128] = {0};
            if (!json_get_string(json, "file_path", fpath, sizeof(fpath))) {
                if (!json_get_string(json, "_file_path", fpath, sizeof(fpath))) {
                    json_get_string(json, "path", fpath, sizeof(fpath));
                }
            }
            if (!json_get_string(json, "file_label", flabel, sizeof(flabel))) {
                json_get_string(json, "label", flabel, sizeof(flabel));
            }
            SlotDspUnit *u = &g_state.ch_slots[ch][s_idx];
            u->active = 1;
            u->bypass = 0;
            if (u->fx_class == 0) u->fx_class = 2; /* Class 2: Amp/NAM */
            snprintf(u->file_path, sizeof(u->file_path), "%s", fpath);
            snprintf(u->file_label, sizeof(u->file_label), "%s", flabel);
            apply_slot_model_voicing(u, fpath, flabel);
            mark_live_state_dirty();
        }
    } else if (strcmp(action, "set_channel_insert_mode") == 0 || strcmp(action, "set_dsp_mode") == 0) {
        int ch = -1;
        if (!json_get_bool_or_int(json, "channel", &ch)) json_get_bool_or_int(json, "ch", &ch);
        int mode_val = 1;
        char mode_str[32] = {0};
        if (json_get_string(json, "mode", mode_str, sizeof(mode_str))) {
            if (strcmp(mode_str, "direct") == 0 || strcmp(mode_str, "off") == 0) mode_val = 0;
            else if (strcmp(mode_str, "canvas") == 0 || strcmp(mode_str, "pedalboard") == 0 || strcmp(mode_str, "mod") == 0) mode_val = 2;
            else mode_val = 1; /* "rack" or "slots" */
        } else {
            json_get_bool_or_int(json, "mode_val", &mode_val);
        }
        if (ch >= 0 && ch < NUM_CHANNELS) {
            g_state.ch_insert_mode[ch] = (mode_val >= 0 && mode_val <= 2) ? mode_val : 1;
            mark_live_state_dirty();
        }
    }
}

static int callback_meters(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    (void)user;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            int fd = lws_get_socket_fd(wsi);
            if (fd >= 0) {
                int nodelay = 1;
                setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
                int sndbuf = 65536;
                setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
            }
            if (g_num_wsi < 32) {
                int idx = g_num_wsi++;
                g_wsi_list[idx] = wsi;
                g_wsi_sync_seq[idx] = g_sync_head;
                g_wsi_needs_init[idx] = 1;
                g_wsi_needs_meters[idx] = 1;
                if (g_num_wsi > 1) {
                    unsigned int slot = g_sync_head % SYNC_RING_SIZE;
                    snprintf(g_sync_ring[slot], sizeof(g_sync_ring[slot]), "{\"action\":\"request_peer_snapshot\"}");
                    g_sync_sender[slot] = wsi;
                    g_sync_head++;
                    for (int i = 0; i < g_num_wsi; i++) {
                        if (g_wsi_list[i] != wsi) lws_callback_on_writable(g_wsi_list[i]);
                    }
                }
            }
            lws_callback_on_writable(wsi);
            break;
        }
        case LWS_CALLBACK_CLOSED:
            for (int i = 0; i < g_num_wsi; i++) {
                if (g_wsi_list[i] == wsi) {
                    int last = --g_num_wsi;
                    g_wsi_list[i] = g_wsi_list[last];
                    g_wsi_sync_seq[i] = g_wsi_sync_seq[last];
                    g_wsi_needs_init[i] = g_wsi_needs_init[last];
                    g_wsi_needs_meters[i] = g_wsi_needs_meters[last];
                    break;
                }
            }
            break;
        case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
            for (int i = 0; i < g_num_wsi; i++) {
                if (g_wsi_list[i] && !lws_send_pipe_choked(g_wsi_list[i])) {
                    if (g_wsi_sync_seq[i] < g_sync_head) {
                        lws_callback_on_writable(g_wsi_list[i]);
                    }
                }
            }
            break;
        case LWS_CALLBACK_RECEIVE: {
            static char msg_buf[65536];
            static size_t msg_accum_len = 0;
            if (lws_is_first_fragment(wsi)) {
                msg_accum_len = 0;
            }
            if (msg_accum_len + len < sizeof(msg_buf) - 1) {
                memcpy(msg_buf + msg_accum_len, in, len);
                msg_accum_len += len;
                msg_buf[msg_accum_len] = '\0';
            }
            if (!lws_is_final_fragment(wsi)) {
                break;
            }
            size_t copy_len = msg_accum_len;
            msg_accum_len = 0;
            if (strstr(msg_buf, "\"peer_ui_snapshot\"") != NULL) {
                if (copy_len > 2 && msg_buf[0] == '{' && msg_buf[copy_len - 1] == '}') {
                    snprintf(g_latest_ui_snapshot, sizeof(g_latest_ui_snapshot), "%s", msg_buf);
                    mark_live_state_dirty();
                    unsigned int slot = g_sync_head % SYNC_RING_SIZE;
                    snprintf(g_sync_ring[slot], sizeof(g_sync_ring[slot]), "{\"action\":\"peer_ui_snapshot_updated\"}");
                    g_sync_sender[slot] = wsi;
                    g_sync_head++;
                    for (int i = 0; i < g_num_wsi; i++) {
                        if (g_wsi_list[i] != wsi) lws_callback_on_writable(g_wsi_list[i]);
                    }
                }
                break;
            }
            handle_ws_control_message(msg_buf);
            if (strstr(msg_buf, "\"set_rta_target\"") == NULL) {
                mark_live_state_dirty();
                if (copy_len < sizeof(g_sync_ring[0]) - 1) {
                    unsigned int slot = g_sync_head % SYNC_RING_SIZE;
                    memcpy(g_sync_ring[slot], msg_buf, copy_len + 1);
                    g_sync_sender[slot] = wsi;
                    g_sync_head++;
                }
            }
            for (int i = 0; i < g_num_wsi; i++) {
                if (g_wsi_list[i] && !lws_send_pipe_choked(g_wsi_list[i])) {
                    g_wsi_needs_meters[i] = 1;
                    lws_callback_on_writable(g_wsi_list[i]);
                }
            }
            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            if (lws_send_pipe_choked(wsi)) break;
            int c_idx = -1;
            for (int i = 0; i < g_num_wsi; i++) {
                if (g_wsi_list[i] == wsi) { c_idx = i; break; }
            }
            if (c_idx >= 0 && g_wsi_needs_init[c_idx]) {
                g_wsi_needs_init[c_idx] = 0;
                static unsigned char sbuf[LWS_PRE + 98304];
                char *sp = (char*)&sbuf[LWS_PRE];
                int cap = 98304;
                float pg_db[8], gt_db[8], gf_db[8];
                for (int i = 0; i < 8; i++) {
                    pg_db[i] = 20.0f * log10f(fmaxf(0.0001f, fabsf(g_state.ch_pregain_lin[i])));
                    gt_db[i] = 20.0f * log10f(fmaxf(0.00001f, g_state.gate_thresh_lin[i]));
                    gf_db[i] = -20.0f * log10f(fmaxf(0.00001f, g_state.gate_floor_lin[i]));
                }
                int sn = snprintf(sp, cap,
                    "{\"type\":\"full_state_sync\","
                    "\"ch_fader\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],"
                    "\"ch_pan\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f],"
                    "\"ch_mute\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"ch_solo\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"ch_hpf80\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"ch_pregain\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
                    "\"ch_phase\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"slot_first\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"modgui_first\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"insert_mode\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"preamp_en\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"preamp_drive\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
                    "\"preamp_trim\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
                    "\"preamp_clip\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"gate_en\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"gate_thresh\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
                    "\"gate_floor\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
                    "\"denoise_sens\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
                    "\"denoise_red\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],"
                    "\"denoise_lc\":[%d,%d,%d,%d,%d,%d,%d,%d],"
                    "\"master_fader\":%.4f,\"master_mute\":%d,"
                    "\"mon_fader\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],"
                    "\"mon_mute\":[%d,%d,%d,%d,%d,%d],"
                    "\"mon_solo\":[%d,%d,%d,%d,%d,%d],"
                    "\"send_fader\":[%.4f,%.4f,%.4f,%.4f],"
                    "\"send_mute\":[%d,%d,%d,%d],"
                    "\"send_solo\":[%d,%d,%d,%d],"
                    "\"ret_fader\":[%.4f,%.4f,%.4f,%.4f],"
                    "\"ret_pan\":[%.3f,%.3f,%.3f,%.3f],"
                    "\"ret_mute\":[%d,%d,%d,%d],"
                    "\"ret_solo\":[%d,%d,%d,%d],"
                    "\"sof_mon\":[[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]],"
                    "\"sof_fx\":[[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]],"
                    "\"reverb\":{\"size\":%.1f,\"damp\":%.1f,\"predelay\":%.1f,\"diffuse\":%.1f,\"ir_mix\":%.1f,\"stereo\":%.1f,\"dry\":%.1f,\"wet\":%.1f,\"lowcut\":%.1f,\"highcut\":%.1f,\"out_lowcut\":%.1f,\"out_highcut\":%.1f,\"mod_rate\":%.1f,\"mod_amount\":%.1f,\"ducking\":%.1f,\"bits\":%.1f,\"sr\":%.1f},"
                    "\"reverb2\":{\"size\":%.1f,\"damp\":%.1f,\"predelay\":%.1f,\"diffuse\":%.1f,\"ir_mix\":%.1f,\"stereo\":%.1f,\"dry\":%.1f,\"wet\":%.1f,\"lowcut\":%.1f,\"highcut\":%.1f,\"out_lowcut\":%.1f,\"out_highcut\":%.1f,\"mod_rate\":%.1f,\"mod_amount\":%.1f,\"ducking\":%.1f,\"bits\":%.1f,\"sr\":%.1f},"
                    "\"delay\":{\"time\":%.1f,\"fb\":%.1f,\"diffuse\":%.1f,\"dry\":%.1f,\"wet\":%.1f,\"pp\":%d,\"lowcut\":%.1f,\"highcut\":%.1f,\"out_lowcut\":%.1f,\"out_highcut\":%.1f,\"drive\":%.1f,\"flutter\":%.1f,\"mod_rate\":%.1f,\"mod_depth\":%.1f,\"ducking\":%.1f,\"stereo\":%.1f,\"spread\":%.1f},"
                    "\"delay2\":{\"time\":%.1f,\"fb\":%.1f,\"diffuse\":%.1f,\"dry\":%.1f,\"wet\":%.1f,\"pp\":%d,\"lowcut\":%.1f,\"highcut\":%.1f,\"out_lowcut\":%.1f,\"out_highcut\":%.1f,\"drive\":%.1f,\"flutter\":%.1f,\"mod_rate\":%.1f,\"mod_depth\":%.1f,\"ducking\":%.1f,\"stereo\":%.1f,\"spread\":%.1f},"
                    "\"summing\":{\"on\":%d,\"console\":%d,\"drive\":%.1f,\"warmth\":%.1f,\"xtalk\":%.1f},"
                    "\"hw\":{\"id\":\"%s\",\"name\":\"%s\",\"ins\":%d,\"outs\":%d,\"sr\":%d,\"buf\":%d,\"cue\":%d,\"patch\":[%d,%d,%d,%d,%d,%d,%d,%d],\"cards\":%s}",
                    g_state.ch_fader_gain[0], g_state.ch_fader_gain[1], g_state.ch_fader_gain[2], g_state.ch_fader_gain[3],
                    g_state.ch_fader_gain[4], g_state.ch_fader_gain[5], g_state.ch_fader_gain[6], g_state.ch_fader_gain[7],
                    g_state.ch_pan[0], g_state.ch_pan[1], g_state.ch_pan[2], g_state.ch_pan[3],
                    g_state.ch_pan[4], g_state.ch_pan[5], g_state.ch_pan[6], g_state.ch_pan[7],
                    g_state.ch_mute[0], g_state.ch_mute[1], g_state.ch_mute[2], g_state.ch_mute[3],
                    g_state.ch_mute[4], g_state.ch_mute[5], g_state.ch_mute[6], g_state.ch_mute[7],
                    g_state.ch_solo[0], g_state.ch_solo[1], g_state.ch_solo[2], g_state.ch_solo[3],
                    g_state.ch_solo[4], g_state.ch_solo[5], g_state.ch_solo[6], g_state.ch_solo[7],
                    g_state.ch_hpf80[0], g_state.ch_hpf80[1], g_state.ch_hpf80[2], g_state.ch_hpf80[3],
                    g_state.ch_hpf80[4], g_state.ch_hpf80[5], g_state.ch_hpf80[6], g_state.ch_hpf80[7],
                    pg_db[0], pg_db[1], pg_db[2], pg_db[3], pg_db[4], pg_db[5], pg_db[6], pg_db[7],
                    g_state.ch_phase_inv[0], g_state.ch_phase_inv[1], g_state.ch_phase_inv[2], g_state.ch_phase_inv[3],
                    g_state.ch_phase_inv[4], g_state.ch_phase_inv[5], g_state.ch_phase_inv[6], g_state.ch_phase_inv[7],
                    g_state.slot_first[0], g_state.slot_first[1], g_state.slot_first[2], g_state.slot_first[3],
                    g_state.slot_first[4], g_state.slot_first[5], g_state.slot_first[6], g_state.slot_first[7],
                    g_state.modgui_first[0], g_state.modgui_first[1], g_state.modgui_first[2], g_state.modgui_first[3],
                    g_state.modgui_first[4], g_state.modgui_first[5], g_state.modgui_first[6], g_state.modgui_first[7],
                    g_state.ch_insert_mode[0], g_state.ch_insert_mode[1], g_state.ch_insert_mode[2], g_state.ch_insert_mode[3],
                    g_state.ch_insert_mode[4], g_state.ch_insert_mode[5], g_state.ch_insert_mode[6], g_state.ch_insert_mode[7],
                    g_state.preamp_enabled[0], g_state.preamp_enabled[1], g_state.preamp_enabled[2], g_state.preamp_enabled[3],
                    g_state.preamp_enabled[4], g_state.preamp_enabled[5], g_state.preamp_enabled[6], g_state.preamp_enabled[7],
                    g_state.preamp_drive_db[0], g_state.preamp_drive_db[1], g_state.preamp_drive_db[2], g_state.preamp_drive_db[3],
                    g_state.preamp_drive_db[4], g_state.preamp_drive_db[5], g_state.preamp_drive_db[6], g_state.preamp_drive_db[7],
                    g_state.preamp_trim_db[0], g_state.preamp_trim_db[1], g_state.preamp_trim_db[2], g_state.preamp_trim_db[3],
                    g_state.preamp_trim_db[4], g_state.preamp_trim_db[5], g_state.preamp_trim_db[6], g_state.preamp_trim_db[7],
                    g_state.preamp_clip_hard[0], g_state.preamp_clip_hard[1], g_state.preamp_clip_hard[2], g_state.preamp_clip_hard[3],
                    g_state.preamp_clip_hard[4], g_state.preamp_clip_hard[5], g_state.preamp_clip_hard[6], g_state.preamp_clip_hard[7],
                    g_state.gate_enabled[0], g_state.gate_enabled[1], g_state.gate_enabled[2], g_state.gate_enabled[3],
                    g_state.gate_enabled[4], g_state.gate_enabled[5], g_state.gate_enabled[6], g_state.gate_enabled[7],
                    gt_db[0], gt_db[1], gt_db[2], gt_db[3], gt_db[4], gt_db[5], gt_db[6], gt_db[7],
                    gf_db[0], gf_db[1], gf_db[2], gf_db[3], gf_db[4], gf_db[5], gf_db[6], gf_db[7],
                    g_state.denoise_sensitivity_db[0], g_state.denoise_sensitivity_db[1], g_state.denoise_sensitivity_db[2], g_state.denoise_sensitivity_db[3],
                    g_state.denoise_sensitivity_db[4], g_state.denoise_sensitivity_db[5], g_state.denoise_sensitivity_db[6], g_state.denoise_sensitivity_db[7],
                    g_state.denoise_reduction_pct[0], g_state.denoise_reduction_pct[1], g_state.denoise_reduction_pct[2], g_state.denoise_reduction_pct[3],
                    g_state.denoise_reduction_pct[4], g_state.denoise_reduction_pct[5], g_state.denoise_reduction_pct[6], g_state.denoise_reduction_pct[7],
                    g_state.denoise_low_cut[0], g_state.denoise_low_cut[1], g_state.denoise_low_cut[2], g_state.denoise_low_cut[3],
                    g_state.denoise_low_cut[4], g_state.denoise_low_cut[5], g_state.denoise_low_cut[6], g_state.denoise_low_cut[7],
                    g_state.master_fader_gain, g_state.master_mute,
                    g_state.mon_fader_gain[0], g_state.mon_fader_gain[1], g_state.mon_fader_gain[2],
                    g_state.mon_fader_gain[3], g_state.mon_fader_gain[4], g_state.mon_fader_gain[5],
                    g_state.mon_mute[0], g_state.mon_mute[1], g_state.mon_mute[2],
                    g_state.mon_mute[3], g_state.mon_mute[4], g_state.mon_mute[5],
                    g_state.mon_solo[0], g_state.mon_solo[1], g_state.mon_solo[2],
                    g_state.mon_solo[3], g_state.mon_solo[4], g_state.mon_solo[5],
                    g_state.send_fader_gain[0], g_state.send_fader_gain[1], g_state.send_fader_gain[2], g_state.send_fader_gain[3],
                    g_state.send_mute[0], g_state.send_mute[1], g_state.send_mute[2], g_state.send_mute[3],
                    g_state.send_solo[0], g_state.send_solo[1], g_state.send_solo[2], g_state.send_solo[3],
                    g_state.ret_fader_gain[0], g_state.ret_fader_gain[1], g_state.ret_fader_gain[2], g_state.ret_fader_gain[3],
                    g_state.ret_pan[0], g_state.ret_pan[1], g_state.ret_pan[2], g_state.ret_pan[3],
                    g_state.ret_mute[0], g_state.ret_mute[1], g_state.ret_mute[2], g_state.ret_mute[3],
                    g_state.ret_solo[0], g_state.ret_solo[1], g_state.ret_solo[2], g_state.ret_solo[3],
                    g_state.sof_mon_gain[0][0], g_state.sof_mon_gain[0][1], g_state.sof_mon_gain[0][2], g_state.sof_mon_gain[0][3], g_state.sof_mon_gain[0][4], g_state.sof_mon_gain[0][5], g_state.sof_mon_gain[0][6], g_state.sof_mon_gain[0][7],
                    g_state.sof_mon_gain[1][0], g_state.sof_mon_gain[1][1], g_state.sof_mon_gain[1][2], g_state.sof_mon_gain[1][3], g_state.sof_mon_gain[1][4], g_state.sof_mon_gain[1][5], g_state.sof_mon_gain[1][6], g_state.sof_mon_gain[1][7],
                    g_state.sof_mon_gain[2][0], g_state.sof_mon_gain[2][1], g_state.sof_mon_gain[2][2], g_state.sof_mon_gain[2][3], g_state.sof_mon_gain[2][4], g_state.sof_mon_gain[2][5], g_state.sof_mon_gain[2][6], g_state.sof_mon_gain[2][7],
                    g_state.sof_mon_gain[3][0], g_state.sof_mon_gain[3][1], g_state.sof_mon_gain[3][2], g_state.sof_mon_gain[3][3], g_state.sof_mon_gain[3][4], g_state.sof_mon_gain[3][5], g_state.sof_mon_gain[3][6], g_state.sof_mon_gain[3][7],
                    g_state.sof_mon_gain[4][0], g_state.sof_mon_gain[4][1], g_state.sof_mon_gain[4][2], g_state.sof_mon_gain[4][3], g_state.sof_mon_gain[4][4], g_state.sof_mon_gain[4][5], g_state.sof_mon_gain[4][6], g_state.sof_mon_gain[4][7],
                    g_state.sof_mon_gain[5][0], g_state.sof_mon_gain[5][1], g_state.sof_mon_gain[5][2], g_state.sof_mon_gain[5][3], g_state.sof_mon_gain[5][4], g_state.sof_mon_gain[5][5], g_state.sof_mon_gain[5][6], g_state.sof_mon_gain[5][7],
                    g_state.sof_fx_gain[0][0], g_state.sof_fx_gain[0][1], g_state.sof_fx_gain[0][2], g_state.sof_fx_gain[0][3], g_state.sof_fx_gain[0][4], g_state.sof_fx_gain[0][5], g_state.sof_fx_gain[0][6], g_state.sof_fx_gain[0][7],
                    g_state.sof_fx_gain[1][0], g_state.sof_fx_gain[1][1], g_state.sof_fx_gain[1][2], g_state.sof_fx_gain[1][3], g_state.sof_fx_gain[1][4], g_state.sof_fx_gain[1][5], g_state.sof_fx_gain[1][6], g_state.sof_fx_gain[1][7],
                    g_state.sof_fx_gain[2][0], g_state.sof_fx_gain[2][1], g_state.sof_fx_gain[2][2], g_state.sof_fx_gain[2][3], g_state.sof_fx_gain[2][4], g_state.sof_fx_gain[2][5], g_state.sof_fx_gain[2][6], g_state.sof_fx_gain[2][7],
                    g_state.sof_fx_gain[3][0], g_state.sof_fx_gain[3][1], g_state.sof_fx_gain[3][2], g_state.sof_fx_gain[3][3], g_state.sof_fx_gain[3][4], g_state.sof_fx_gain[3][5], g_state.sof_fx_gain[3][6], g_state.sof_fx_gain[3][7],
                    g_state.rev[0].size, g_state.rev[0].damp, g_state.rev[0].predelay, g_state.rev[0].diffuse, g_state.rev[0].ir_mix,
                    g_state.rev[0].stereo, g_state.rev[0].dry, g_state.rev[0].wet, g_state.rev[0].in_lowcut,
                    g_state.rev[0].in_highcut, g_state.rev[0].out_lowcut, g_state.rev[0].out_highcut,
                    g_state.rev[0].mod_rate, g_state.rev[0].mod_amount, g_state.rev[0].ducking, g_state.rev[0].lofi_bits, g_state.rev[0].lofi_sr,
                    g_state.rev[1].size, g_state.rev[1].damp, g_state.rev[1].predelay, g_state.rev[1].diffuse, g_state.rev[1].ir_mix,
                    g_state.rev[1].stereo, g_state.rev[1].dry, g_state.rev[1].wet, g_state.rev[1].in_lowcut,
                    g_state.rev[1].in_highcut, g_state.rev[1].out_lowcut, g_state.rev[1].out_highcut,
                    g_state.rev[1].mod_rate, g_state.rev[1].mod_amount, g_state.rev[1].ducking, g_state.rev[1].lofi_bits, g_state.rev[1].lofi_sr,
                    g_state.dly[0].time_ms, g_state.dly[0].feedback, g_state.dly[0].diffuse, g_state.dly[0].dry,
                    g_state.dly[0].wet, g_state.dly[0].ping_pong, g_state.dly[0].in_lowcut, g_state.dly[0].in_highcut,
                    g_state.dly[0].out_lowcut, g_state.dly[0].out_highcut,
                    g_state.dly[0].drive, g_state.dly[0].flutter, g_state.dly[0].mod_rate, g_state.dly[0].mod_depth,
                    g_state.dly[0].ducking, g_state.dly[0].stereo, g_state.dly[0].spread,
                    g_state.dly[1].time_ms, g_state.dly[1].feedback, g_state.dly[1].diffuse, g_state.dly[1].dry,
                    g_state.dly[1].wet, g_state.dly[1].ping_pong, g_state.dly[1].in_lowcut, g_state.dly[1].in_highcut,
                    g_state.dly[1].out_lowcut, g_state.dly[1].out_highcut,
                    g_state.dly[1].drive, g_state.dly[1].flutter, g_state.dly[1].mod_rate, g_state.dly[1].mod_depth,
                    g_state.dly[1].ducking, g_state.dly[1].stereo, g_state.dly[1].spread,
                    g_state.summing_enabled, g_state.summing_console, g_state.summing_drive_db,
                    g_state.summing_warmth_db, g_state.summing_crosstalk_db,
                    g_state.hw_card_id, g_state.hw_card_name, g_state.hw_in_count, g_state.hw_out_count,
                    (int)g_state.sample_rate, g_state.hw_buffer_size, g_state.solo_cue_to_main,
                    g_state.ch_input_patch[0], g_state.ch_input_patch[1], g_state.ch_input_patch[2], g_state.ch_input_patch[3],
                    g_state.ch_input_patch[4], g_state.ch_input_patch[5], g_state.ch_input_patch[6], g_state.ch_input_patch[7],
                    g_state.hw_cards_json
                );
                /* Append Dual-Band De-Esser, Tape Machine, Channel EQs, and Channel Compressors */
                sn += snprintf(sp + sn, cap - sn,
                    ",\"deesser\":{\"en\":[%d,%d,%d,%d,%d,%d,%d,%d],\"th1\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],\"fq1\":[%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f],\"q1\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"mx1\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],\"b2\":[%d,%d,%d,%d,%d,%d,%d,%d],\"th2\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],\"fq2\":[%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f],\"q2\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"mx2\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],\"wide\":[%d,%d,%d,%d,%d,%d,%d,%d]}",
                    g_state.deesser_enabled[0], g_state.deesser_enabled[1], g_state.deesser_enabled[2], g_state.deesser_enabled[3], g_state.deesser_enabled[4], g_state.deesser_enabled[5], g_state.deesser_enabled[6], g_state.deesser_enabled[7],
                    g_state.deesser_thresh1_db[0], g_state.deesser_thresh1_db[1], g_state.deesser_thresh1_db[2], g_state.deesser_thresh1_db[3], g_state.deesser_thresh1_db[4], g_state.deesser_thresh1_db[5], g_state.deesser_thresh1_db[6], g_state.deesser_thresh1_db[7],
                    g_state.deesser_freq1[0], g_state.deesser_freq1[1], g_state.deesser_freq1[2], g_state.deesser_freq1[3], g_state.deesser_freq1[4], g_state.deesser_freq1[5], g_state.deesser_freq1[6], g_state.deesser_freq1[7],
                    g_state.deesser_q1[0], g_state.deesser_q1[1], g_state.deesser_q1[2], g_state.deesser_q1[3], g_state.deesser_q1[4], g_state.deesser_q1[5], g_state.deesser_q1[6], g_state.deesser_q1[7],
                    g_state.deesser_max1_db[0], g_state.deesser_max1_db[1], g_state.deesser_max1_db[2], g_state.deesser_max1_db[3], g_state.deesser_max1_db[4], g_state.deesser_max1_db[5], g_state.deesser_max1_db[6], g_state.deesser_max1_db[7],
                    g_state.deesser_band2_en[0], g_state.deesser_band2_en[1], g_state.deesser_band2_en[2], g_state.deesser_band2_en[3], g_state.deesser_band2_en[4], g_state.deesser_band2_en[5], g_state.deesser_band2_en[6], g_state.deesser_band2_en[7],
                    g_state.deesser_thresh2_db[0], g_state.deesser_thresh2_db[1], g_state.deesser_thresh2_db[2], g_state.deesser_thresh2_db[3], g_state.deesser_thresh2_db[4], g_state.deesser_thresh2_db[5], g_state.deesser_thresh2_db[6], g_state.deesser_thresh2_db[7],
                    g_state.deesser_freq2[0], g_state.deesser_freq2[1], g_state.deesser_freq2[2], g_state.deesser_freq2[3], g_state.deesser_freq2[4], g_state.deesser_freq2[5], g_state.deesser_freq2[6], g_state.deesser_freq2[7],
                    g_state.deesser_q2[0], g_state.deesser_q2[1], g_state.deesser_q2[2], g_state.deesser_q2[3], g_state.deesser_q2[4], g_state.deesser_q2[5], g_state.deesser_q2[6], g_state.deesser_q2[7],
                    g_state.deesser_max2_db[0], g_state.deesser_max2_db[1], g_state.deesser_max2_db[2], g_state.deesser_max2_db[3], g_state.deesser_max2_db[4], g_state.deesser_max2_db[5], g_state.deesser_max2_db[6], g_state.deesser_max2_db[7],
                    g_state.deesser_wide[0], g_state.deesser_wide[1], g_state.deesser_wide[2], g_state.deesser_wide[3], g_state.deesser_wide[4], g_state.deesser_wide[5], g_state.deesser_wide[6], g_state.deesser_wide[7]
                );
                int t_ntrk = g_state.tape_num_tracks;
                if (t_ntrk > MAX_TAPE_TRACKS) t_ntrk = MAX_TAPE_TRACKS;
                sn += snprintf(sp + sn, cap - sn,
                    ",\"tape_cfg\":{\"ips\":%d,\"emu\":%d,\"prt\":%d,\"mvol\":%.3f,\"msend\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f],\"song\":\"%s\",\"dir\":\"%s\",\"ntrk\":%d,\"vol\":[",
                    g_state.tape_ips, g_state.tape_emu_enabled, g_state.tape_print_wav, g_state.tape_master_vol,
                    g_state.tape_mon_send[0], g_state.tape_mon_send[1], g_state.tape_mon_send[2], g_state.tape_mon_send[3], g_state.tape_mon_send[4], g_state.tape_mon_send[5],
                    g_state.tape_song_name, g_state.tape_base_dir, t_ntrk);
                for (int t = 0; t < t_ntrk; t++) sn += snprintf(sp + sn, cap - sn, "%s%.3f", (t == 0 ? "" : ","), g_state.tape_trk_vol[t]);
                sn += snprintf(sp + sn, cap - sn, "],\"pan\":[");
                for (int t = 0; t < t_ntrk; t++) sn += snprintf(sp + sn, cap - sn, "%s%.3f", (t == 0 ? "" : ","), g_state.tape_trk_pan[t]);
                sn += snprintf(sp + sn, cap - sn, "],\"drv\":[");
                for (int t = 0; t < t_ntrk; t++) sn += snprintf(sp + sn, cap - sn, "%s%.1f", (t == 0 ? "" : ","), g_state.tape_trk_drive_db[t]);
                sn += snprintf(sp + sn, cap - sn, "],\"mute\":[");
                for (int t = 0; t < t_ntrk; t++) sn += snprintf(sp + sn, cap - sn, "%s%d", (t == 0 ? "" : ","), g_state.tape_trk_mute[t]);
                sn += snprintf(sp + sn, cap - sn, "],\"solo\":[");
                for (int t = 0; t < t_ntrk; t++) sn += snprintf(sp + sn, cap - sn, "%s%d", (t == 0 ? "" : ","), g_state.tape_trk_solo[t]);
                sn += snprintf(sp + sn, cap - sn, "],\"rec\":[");
                for (int t = 0; t < t_ntrk; t++) sn += snprintf(sp + sn, cap - sn, "%s%d", (t == 0 ? "" : ","), g_state.tape_trk_rec[t]);
                sn += snprintf(sp + sn, cap - sn, "]},\"ch_eq\":[");
                for (int c = 0; c < NUM_CHANNELS; c++) {
                    sn += snprintf(sp + sn, cap - sn, "%s[", (c == 0 ? "" : ","));
                    for (int b = 0; b < NUM_EQ_BANDS; b++) {
                        const CytomicSvfBand *bd = &g_state.ch_eq[c].bands[b];
                        int sl_st = bd->slope >= 12 ? (bd->slope / 12) : bd->slope;
                        if (sl_st < 1 || sl_st > 4) sl_st = 1;
                        sn += snprintf(sp + sn, cap - sn, "%s{\"en\":%d,\"t\":%d,\"f\":%.1f,\"g\":%.2f,\"q\":%.2f,\"s\":%d,\"de\":%d,\"dt\":%.1f,\"dr\":%.1f,\"rt\":%.1f}",
                            (b == 0 ? "" : ","), bd->enabled ? 1 : 0, (bd->type >= 0 && bd->type <= 9) ? bd->type : 0,
                            isfinite(bd->freq) ? bd->freq : 1000.0f, isfinite(bd->gain) ? bd->gain : 0.0f, isfinite(bd->q) ? bd->q : 0.71f,
                            sl_st, bd->dyn_enabled ? 1 : 0,
                            isfinite(bd->dyn_thresh_db) ? bd->dyn_thresh_db : -20.0f,
                            isfinite(bd->dyn_range_db) ? bd->dyn_range_db : -6.0f,
                            isfinite(bd->dyn_ratio) ? bd->dyn_ratio : 3.0f);
                    }
                    sn += snprintf(sp + sn, cap - sn, "]");
                }
                sn += snprintf(sp + sn, cap - sn, "],\"master_eq\":[");
                for (int b = 0; b < NUM_EQ_BANDS; b++) {
                    const CytomicSvfBand *bd = &g_state.master_eq.bands[b];
                    int sl_st = bd->slope >= 12 ? (bd->slope / 12) : bd->slope;
                    if (sl_st < 1 || sl_st > 4) sl_st = 1;
                    sn += snprintf(sp + sn, cap - sn, "%s{\"en\":%d,\"t\":%d,\"f\":%.1f,\"g\":%.2f,\"q\":%.2f,\"s\":%d,\"de\":%d,\"dt\":%.1f,\"dr\":%.1f,\"rt\":%.1f}",
                        (b == 0 ? "" : ","), bd->enabled ? 1 : 0, (bd->type >= 0 && bd->type <= 9) ? bd->type : 0,
                        isfinite(bd->freq) ? bd->freq : 1000.0f, isfinite(bd->gain) ? bd->gain : 0.0f, isfinite(bd->q) ? bd->q : 0.71f,
                        sl_st, bd->dyn_enabled ? 1 : 0,
                        isfinite(bd->dyn_thresh_db) ? bd->dyn_thresh_db : -20.0f,
                        isfinite(bd->dyn_range_db) ? bd->dyn_range_db : -6.0f,
                        isfinite(bd->dyn_ratio) ? bd->dyn_ratio : 3.0f);
                }
                sn += snprintf(sp + sn, cap - sn, "],\"ch_comp\":[");
                for (int c = 0; c < NUM_CHANNELS; c++) {
                    const DynamicsSlot *d = &g_state.ch_dyn[c];
                    sn += snprintf(sp + sn, cap - sn, "%s{\"m\":%d,\"byp\":%d,\"fp\":%d,\"fi\":%.1f,\"fo\":%.1f,\"fa\":%d,\"fr\":%d,\"frt\":%.1f,\"lp\":%d,\"lpk\":%.1f,\"lg\":%.1f,\"lmx\":%.1f,\"ll\":%d}",
                        (c == 0 ? "" : ","), d->comp_model, d->comp_bypass, d->fet_power, d->fet_input_db, d->fet_output_db, d->fet_attack, d->fet_release, d->fet_ratio, d->lala_power, d->lala_peak_red, d->lala_gain, d->lala_mix, d->lala_limit);
                }
                size_t snap_len = strlen(g_latest_ui_snapshot);
                int valid_snap = (snap_len > 2 && g_latest_ui_snapshot[0] == '{' && g_latest_ui_snapshot[snap_len - 1] == '}');
                sn += snprintf(sp + sn, cap - sn, "],\"snapshot\":%s}",
                    valid_snap ? g_latest_ui_snapshot : "null"
                );
                lws_write(wsi, (unsigned char*)sp, sn, LWS_WRITE_TEXT);
                lws_callback_on_writable(wsi);
                break;
            }
            if (c_idx >= 0 && g_wsi_sync_seq[c_idx] < g_sync_head) {
                if (g_sync_head - g_wsi_sync_seq[c_idx] > SYNC_RING_SIZE) {
                    g_wsi_sync_seq[c_idx] = g_sync_head - SYNC_RING_SIZE;
                }
                while (g_wsi_sync_seq[c_idx] < g_sync_head) {
                    unsigned int slot = g_wsi_sync_seq[c_idx] % SYNC_RING_SIZE;
                    g_wsi_sync_seq[c_idx]++;
                    if (g_sync_sender[slot] == wsi) continue;
                    static unsigned char ebuf[LWS_PRE + 65536];
                    char *ep = (char*)&ebuf[LWS_PRE];
                    int en = 0;
                    if (strstr(g_sync_ring[slot], "\"peer_ui_snapshot_updated\"") != NULL && g_latest_ui_snapshot[0] != '\0') {
                        en = snprintf(ep, 65536, "{\"type\":\"sync_event\",\"payload\":%s}", g_latest_ui_snapshot);
                    } else {
                        en = snprintf(ep, 65536, "{\"type\":\"sync_event\",\"payload\":%s}", g_sync_ring[slot]);
                    }
                    lws_write(wsi, (unsigned char*)ep, en, LWS_WRITE_TEXT);
                    if (g_wsi_sync_seq[c_idx] < g_sync_head || g_wsi_needs_meters[c_idx]) {
                        lws_callback_on_writable(wsi);
                    }
                    return 0;
                }
            }
            if (c_idx >= 0) g_wsi_needs_meters[c_idx] = 0;
            unsigned char buf[LWS_PRE + 4096];
            char *p = (char*)&buf[LWS_PRE];
            if (g_state.denoise_learn_done_ch >= 0) {
                int lch = g_state.denoise_learn_done_ch;
                g_state.denoise_learn_done_ch = -1;
                mark_live_state_dirty();
                float avg_t = 0.0f;
                for (int b = 0; b < 10; b++) avg_t += g_state.denoise_thresh_db[lch][b];
                avg_t *= 0.1f;
                unsigned int slot = g_sync_head % SYNC_RING_SIZE;
                snprintf(g_sync_ring[slot], sizeof(g_sync_ring[slot]),
                    "{\"action\":\"denoise_learned\",\"ch\":%d,\"channel\":%d,\"thresh\":%.1f,\"thresholds\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f]}",
                    lch, lch, avg_t,
                    g_state.denoise_thresh_db[lch][0], g_state.denoise_thresh_db[lch][1],
                    g_state.denoise_thresh_db[lch][2], g_state.denoise_thresh_db[lch][3],
                    g_state.denoise_thresh_db[lch][4], g_state.denoise_thresh_db[lch][5],
                    g_state.denoise_thresh_db[lch][6], g_state.denoise_thresh_db[lch][7],
                    g_state.denoise_thresh_db[lch][8], g_state.denoise_thresh_db[lch][9]);
                g_sync_sender[slot] = NULL;
                g_sync_head++;
                for (int i = 0; i < g_num_wsi; i++) lws_callback_on_writable(g_wsi_list[i]);
            }
            if (g_state.room_cal_done_flag) {
                g_state.room_cal_done_flag = 0;
                static const int cal_bins[8]  = { 10, 19, 28, 37, 45, 51, 56, 60 };
                static const int cal_freqs[8] = { 60, 150, 400, 1000, 2500, 5000, 8000, 12000 };
                float avg_db = 0.0f;
                for (int k = 0; k < 8; k++) avg_db += g_state.rta_db[cal_bins[k]];
                avg_db *= 0.125f;
                int cn = snprintf(p, 4096, "{\"action\":\"room_cal_complete\",\"bus\":\"%s\",\"bands\":[", g_state.room_cal_bus);
                for (int k = 0; k < 8; k++) {
                    float diff = avg_db - g_state.rta_db[cal_bins[k]];
                    if (diff < -6.0f) diff = -6.0f;
                    if (diff > 4.5f)  diff = 4.5f;
                    if (avg_db <= -75.0f) {
                        static const float fb_g[8] = { -3.5f, -4.0f, 2.0f, -1.5f, 2.5f, -2.0f, 1.5f, 1.0f };
                        diff = fb_g[k];
                    }
                    cn += snprintf(p + cn, 4096 - cn, "%s{\"band\":%d,\"freq\":%d,\"gain\":%.1f,\"q\":1.41}",
                        (k == 0 ? "" : ","), k, cal_freqs[k], diff);
                }
                cn += snprintf(p + cn, 4096 - cn, "]}");
                lws_write(wsi, (unsigned char*)p, cn, LWS_WRITE_TEXT);
                if (c_idx >= 0 && g_wsi_sync_seq[c_idx] < g_sync_head) lws_callback_on_writable(wsi);
                break;
            }
            EqStrip8 *active_eq = resolve_eq_strip(g_state.rta_target_kind, g_state.rta_target_ch, g_state.rta_target_bus);
            if (!active_eq) active_eq = &g_state.master_eq;
            int dn_ch = (g_state.rta_target_kind == 1 && g_state.rta_target_ch >= 0 && g_state.rta_target_ch < NUM_CHANNELS) ? g_state.rta_target_ch : 0;
            float tape_sec = (float)g_state.tape_frame_pos / (g_state.sample_rate > 8000.0f ? g_state.sample_rate : 48000.0f);
            static unsigned int s_meter_tick = 0;
            int send_slow_meta = ((s_meter_tick++ % 30) == 1);
            int n = snprintf(p, 4096,
                "{\"type\":\"meters\",\"ch\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],\"ch_pre\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],\"master\":[%.4f,%.4f],\"aux\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],\"send\":[%.4f,%.4f,%.4f,%.4f],\"ret\":[%.4f,%.4f,%.4f,%.4f],\"gr\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"master_gr\":%.2f,\"master_gr2\":%.2f,\"mon_gr\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"mon_gr2\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"send_gr\":[%.2f,%.2f,%.2f,%.2f],\"ret_gr\":[%.2f,%.2f,%.2f,%.2f],\"ds_gr1\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"ds_gr2\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"ds_det1\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],\"ds_det2\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],\"pitch\":[%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f],\"sys\":{\"dsp\":%.1f,\"cpu\":%.1f,\"temp\":%.1f,\"xruns\":%u},\"preamp_in\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f],\"preamp_status\":[%d,%d,%d,%d,%d,%d,%d,%d],\"gate\":[%d,%d,%d,%d,%d,%d,%d,%d],\"denoise_l\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f],\"stereo\":[%d,%d,%d,%d,%d,%d,%d,%d],\"dyn_eq\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]",
                g_state.peak_levels[0], g_state.peak_levels[1], g_state.peak_levels[2], g_state.peak_levels[3],
                g_state.peak_levels[4], g_state.peak_levels[5], g_state.peak_levels[6], g_state.peak_levels[7],
                g_state.ch_pre_levels[0], g_state.ch_pre_levels[1], g_state.ch_pre_levels[2], g_state.ch_pre_levels[3],
                g_state.ch_pre_levels[4], g_state.ch_pre_levels[5], g_state.ch_pre_levels[6], g_state.ch_pre_levels[7],
                g_state.master_peak_l, g_state.master_peak_r,
                g_state.aux_peaks[0], g_state.aux_peaks[1], g_state.aux_peaks[2],
                g_state.aux_peaks[3], g_state.aux_peaks[4], g_state.aux_peaks[5],
                g_state.send_peaks[0], g_state.send_peaks[1], g_state.send_peaks[2], g_state.send_peaks[3],
                g_state.ret_peaks[0], g_state.ret_peaks[1], g_state.ret_peaks[2], g_state.ret_peaks[3],
                g_state.ch_gr_db[0], g_state.ch_gr_db[1], g_state.ch_gr_db[2], g_state.ch_gr_db[3],
                g_state.ch_gr_db[4], g_state.ch_gr_db[5], g_state.ch_gr_db[6], g_state.ch_gr_db[7],
                g_state.master_gr_db, g_state.master_gr2_db,
                g_state.mon_gr_db[0], g_state.mon_gr_db[1], g_state.mon_gr_db[2],
                g_state.mon_gr_db[3], g_state.mon_gr_db[4], g_state.mon_gr_db[5],
                g_state.mon_gr2_db[0], g_state.mon_gr2_db[1], g_state.mon_gr2_db[2],
                g_state.mon_gr2_db[3], g_state.mon_gr2_db[4], g_state.mon_gr2_db[5],
                g_state.send_gr_db[0], g_state.send_gr_db[1], g_state.send_gr_db[2], g_state.send_gr_db[3],
                g_state.ret_gr_db[0], g_state.ret_gr_db[1], g_state.ret_gr_db[2], g_state.ret_gr_db[3],
                g_state.deesser_gr1_db[0], g_state.deesser_gr1_db[1], g_state.deesser_gr1_db[2], g_state.deesser_gr1_db[3],
                g_state.deesser_gr1_db[4], g_state.deesser_gr1_db[5], g_state.deesser_gr1_db[6], g_state.deesser_gr1_db[7],
                g_state.deesser_gr2_db[0], g_state.deesser_gr2_db[1], g_state.deesser_gr2_db[2], g_state.deesser_gr2_db[3],
                g_state.deesser_gr2_db[4], g_state.deesser_gr2_db[5], g_state.deesser_gr2_db[6], g_state.deesser_gr2_db[7],
                g_state.deesser_det1_db[0], g_state.deesser_det1_db[1], g_state.deesser_det1_db[2], g_state.deesser_det1_db[3],
                g_state.deesser_det1_db[4], g_state.deesser_det1_db[5], g_state.deesser_det1_db[6], g_state.deesser_det1_db[7],
                g_state.deesser_det2_db[0], g_state.deesser_det2_db[1], g_state.deesser_det2_db[2], g_state.deesser_det2_db[3],
                g_state.deesser_det2_db[4], g_state.deesser_det2_db[5], g_state.deesser_det2_db[6], g_state.deesser_det2_db[7],
                g_state.ch_pitch_hz[0], g_state.ch_pitch_hz[1], g_state.ch_pitch_hz[2], g_state.ch_pitch_hz[3],
                g_state.ch_pitch_hz[4], g_state.ch_pitch_hz[5], g_state.ch_pitch_hz[6], g_state.ch_pitch_hz[7],
                g_state.jack_dsp_load, g_state.sys_cpu_load, g_state.pi_temp_c, (unsigned int)g_state.jack_xruns,
                g_state.preamp_in_peak[0], g_state.preamp_in_peak[1], g_state.preamp_in_peak[2], g_state.preamp_in_peak[3],
                g_state.preamp_in_peak[4], g_state.preamp_in_peak[5], g_state.preamp_in_peak[6], g_state.preamp_in_peak[7],
                g_state.preamp_clip_status[0], g_state.preamp_clip_status[1], g_state.preamp_clip_status[2], g_state.preamp_clip_status[3],
                g_state.preamp_clip_status[4], g_state.preamp_clip_status[5], g_state.preamp_clip_status[6], g_state.preamp_clip_status[7],
                g_state.gate_open[0], g_state.gate_open[1], g_state.gate_open[2], g_state.gate_open[3],
                g_state.gate_open[4], g_state.gate_open[5], g_state.gate_open[6], g_state.gate_open[7],
                g_state.denoise_band_levels[dn_ch][0], g_state.denoise_band_levels[dn_ch][1],
                g_state.denoise_band_levels[dn_ch][2], g_state.denoise_band_levels[dn_ch][3],
                g_state.denoise_band_levels[dn_ch][4], g_state.denoise_band_levels[dn_ch][5],
                g_state.denoise_band_levels[dn_ch][6], g_state.denoise_band_levels[dn_ch][7],
                g_state.denoise_band_levels[dn_ch][8], g_state.denoise_band_levels[dn_ch][9],
                g_state.ch_stereo_active[0], g_state.ch_stereo_active[1], g_state.ch_stereo_active[2], g_state.ch_stereo_active[3],
                g_state.ch_stereo_active[4], g_state.ch_stereo_active[5], g_state.ch_stereo_active[6], g_state.ch_stereo_active[7],
                isfinite(active_eq->bands[0].dyn_gain_db) ? active_eq->bands[0].dyn_gain_db : 0.0f,
                isfinite(active_eq->bands[1].dyn_gain_db) ? active_eq->bands[1].dyn_gain_db : 0.0f,
                isfinite(active_eq->bands[2].dyn_gain_db) ? active_eq->bands[2].dyn_gain_db : 0.0f,
                isfinite(active_eq->bands[3].dyn_gain_db) ? active_eq->bands[3].dyn_gain_db : 0.0f,
                isfinite(active_eq->bands[4].dyn_gain_db) ? active_eq->bands[4].dyn_gain_db : 0.0f,
                isfinite(active_eq->bands[5].dyn_gain_db) ? active_eq->bands[5].dyn_gain_db : 0.0f,
                isfinite(active_eq->bands[6].dyn_gain_db) ? active_eq->bands[6].dyn_gain_db : 0.0f,
                isfinite(active_eq->bands[7].dyn_gain_db) ? active_eq->bands[7].dyn_gain_db : 0.0f);
            if (send_slow_meta) {
                n += snprintf(p + n, 4096 - n,
                    ",\"net\":{\"mode\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"policy\":\"%s\"},\"hw\":{\"id\":\"%s\",\"name\":\"%s\",\"ins\":%d,\"outs\":%d,\"sr\":%d,\"buf\":%d,\"cue\":%d},\"tape\":{\"st\":%d,\"sec\":%.3f,\"ips\":%d,\"emu\":%d,\"prt\":%d,\"ntrk\":%d,\"song\":\"%s\",\"dir\":\"%s\",\"free\":%.1f,\"drives\":%s,\"vu\":[",
                    g_state.net_mode[0] ? g_state.net_mode : "wifi",
                    g_state.net_ssid[0] ? g_state.net_ssid : "VM99103602.4g",
                    g_state.net_ip[0]   ? g_state.net_ip   : "192.168.1.148",
                    g_state.net_policy[0] ? g_state.net_policy : "auto",
                    g_state.hw_card_id, g_state.hw_card_name, g_state.hw_in_count, g_state.hw_out_count, (int)g_state.sample_rate, g_state.hw_buffer_size, g_state.solo_cue_to_main,
                    g_state.tape_transport, tape_sec, g_state.tape_ips, g_state.tape_emu_enabled, g_state.tape_print_wav, g_state.tape_num_tracks, g_state.tape_song_name, g_state.tape_base_dir, g_state.tape_free_gb, g_state.tape_drives_json);
            } else {
                n += snprintf(p + n, 4096 - n,
                    ",\"tape\":{\"st\":%d,\"sec\":%.3f,\"ips\":%d,\"emu\":%d,\"prt\":%d,\"ntrk\":%d,\"vu\":[",
                    g_state.tape_transport, tape_sec, g_state.tape_ips, g_state.tape_emu_enabled, g_state.tape_print_wav, g_state.tape_num_tracks);
            }
            int ntrk = g_state.tape_num_tracks;
            if (ntrk > MAX_TAPE_TRACKS) ntrk = MAX_TAPE_TRACKS;
            for (int t = 0; t < ntrk; t++) {
                n += snprintf(p + n, 4096 - n, "%s%.1f", (t == 0 ? "" : ","), g_state.tape_trk_vu_db[t]);
            }
            n += snprintf(p + n, 4096 - n, "],\"clip\":[");
            for (int t = 0; t < ntrk; t++) {
                n += snprintf(p + n, 4096 - n, "%s%d", (t == 0 ? "" : ","), g_state.tape_trk_clip[t]);
            }
            n += snprintf(p + n, 4096 - n, "]},\"rta\":[");
            for (int b = 0; b < NUM_RTA_BINS; b++) {
                n += snprintf(p + n, 4096 - n, "%s%.1f", (b == 0 ? "" : ","), g_state.rta_db[b]);
            }
            n += snprintf(p + n, 4096 - n, "]}");
            lws_write(wsi, (unsigned char*)p, n, LWS_WRITE_TEXT);
            if (c_idx >= 0 && g_wsi_sync_seq[c_idx] < g_sync_head) {
                lws_callback_on_writable(wsi);
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    { "ws-meters", callback_meters, 0, 65536 },
    { NULL, NULL, 0, 0 }
};

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000LL);
}

/* Dedicated 60FPS (16.6ms) wakeup thread:
   In libwebsockets >= 3.2, lws_service(context, 0) ignores the 0 timeout and blocks inside ppoll()
   for several seconds unless woken by lws_cancel_service(context). Calling lws_cancel_service()
   every 16ms guarantees rock-solid 60FPS meter & RTA frames with zero sleep delay. */
static void *ws_60fps_wakeup_thread(void *arg) {
    (void)arg;
    while (1) {
        usleep(16000);
        if (g_context) {
            lws_cancel_service(g_context);
        }
    }
    return NULL;
}

static void *udp_discovery_thread(void *arg) {
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return NULL;
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(METER_PORT);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }
    char rbuf[128];
    struct sockaddr_in cli;
    socklen_t clilen = sizeof(cli);
    while (1) {
        clilen = sizeof(cli);
        ssize_t n = recvfrom(sock, rbuf, sizeof(rbuf) - 1, 0, (struct sockaddr*)&cli, &clilen);
        if (n > 0) {
            rbuf[n] = '\0';
            if (strstr(rbuf, "PATCHBOX_DISCOVER") != NULL) {
                const char *reply = "PATCHBOX_HERE:8089";
                sendto(sock, reply, strlen(reply), 0, (struct sockaddr*)&cli, clilen);
            }
        }
    }
    return NULL;
}

static int jack_xrun_cb(void *arg) {
    MeterState *st = (MeterState*)arg;
    if (st) st->jack_xruns++;
    return 0;
}

static volatile int g_isolate_mod_monitor_needed = 0;

/* Rule 23A: Isolate hardware outputs from MODEP's default mod-monitor parallel passthrough
   so muted channels never bleed dry audio to headphones/main outputs and never comb-filter! */
static void isolate_mod_monitor_passthrough(jack_client_t *client) {
    if (!client) return;
    jack_disconnect(client, "mod-monitor:out_1", "system:playback_1");
    jack_disconnect(client, "mod-monitor:out_2", "system:playback_2");
    jack_disconnect(client, "system:capture_1", "mod-monitor:in_1");
    jack_disconnect(client, "system:capture_2", "mod-monitor:in_2");
    jack_disconnect(client, "system:capture_1", "system:playback_1");
    jack_disconnect(client, "system:capture_2", "system:playback_2");
}

static void jack_port_connect_cb(jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {
    if (connect) {
        g_isolate_mod_monitor_needed = 1;
    }
}

/* Sample Raspberry Pi System CPU %, Core Temp (°C), JACK Real-Time DSP %, and Network Mode */
static void update_pi_system_telemetry(MeterState *st, jack_client_t *client) {
    if (client) {
        st->jack_dsp_load = jack_cpu_load(client);
        if (g_isolate_mod_monitor_needed) {
            g_isolate_mod_monitor_needed = 0;
            isolate_mod_monitor_passthrough(client);
        }
    }
    static unsigned long long prev_total = 0, prev_idle = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (fp) {
        unsigned long long user = 0, nice = 0, sys = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
        if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
            unsigned long long cur_idle = idle + iowait;
            unsigned long long cur_total = user + nice + sys + cur_idle + irq + softirq + steal;
            unsigned long long d_total = cur_total - prev_total;
            unsigned long long d_idle  = cur_idle - prev_idle;
            if (prev_total > 0 && d_total > 0) {
                float pct = (float)(d_total - d_idle) * 100.0f / (float)d_total;
                if (pct < 0.0f) pct = 0.0f;
                if (pct > 100.0f) pct = 100.0f;
                st->sys_cpu_load = 0.65f * st->sys_cpu_load + 0.35f * pct;
            }
            prev_total = cur_total;
            prev_idle  = cur_idle;
        }
        fclose(fp);
    }
    FILE *tf = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (tf) {
        int millideg = 0;
        if (fscanf(tf, "%d", &millideg) == 1 && millideg > 0) {
            st->pi_temp_c = (float)millideg * 0.001f;
        }
        fclose(tf);
    }
    FILE *nf = fopen("/tmp/patchbox-net-status", "r");
    if (nf) {
        char line[192] = {0};
        if (fgets(line, sizeof(line), nf)) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            char *p1 = strtok(line, "|");
            char *p2 = p1 ? strtok(NULL, "|") : NULL;
            char *p3 = p2 ? strtok(NULL, "|") : NULL;
            char *p4 = p3 ? strtok(NULL, "|") : NULL;
            if (p1 && p1[0]) snprintf(st->net_mode, sizeof(st->net_mode), "%s", p1);
            if (p2 && p2[0]) snprintf(st->net_ssid, sizeof(st->net_ssid), "%s", p2);
            if (p3 && p3[0]) snprintf(st->net_ip, sizeof(st->net_ip), "%s", p3);
            if (p4 && p4[0]) snprintf(st->net_policy, sizeof(st->net_policy), "%s", p4);
        }
        fclose(nf);
    }
}

static jack_client_t *try_attach_jack(void) {
    jack_client_t *client = jack_client_open("modep-meter-daemon", JackNoStartServer, NULL);
    if (!client) return NULL;

    float fs = (float)jack_get_sample_rate(client);
    if (fs > 8000.0f) g_state.sample_rate = fs;
    g_state.hw_buffer_size = (int)jack_get_buffer_size(client);
    scan_audio_interfaces(&g_state);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "in_%d", i + 1);
        g_state.in_ports[i] = jack_port_register(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        snprintf(name, sizeof(name), "in_%d_r", i + 1);
        g_state.in_ports_r[i] = jack_port_register(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    }
    g_state.master_l = jack_port_register(client, "master_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    g_state.master_r = jack_port_register(client, "master_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    g_state.out_l = jack_port_register(client, "out_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    g_state.out_r = jack_port_register(client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    for (int m = 0; m < NUM_MONITORS; m++) {
        char name[32];
        snprintf(name, sizeof(name), "mon_%d", m + 1);
        g_state.mon_ports[m] = jack_port_register(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    }

    jack_set_process_callback(client, jack_process, &g_state);
    jack_set_xrun_callback(client, jack_xrun_cb, &g_state);
    jack_set_port_connect_callback(client, jack_port_connect_cb, client);
    if (jack_activate(client) != 0) {
        jack_client_close(client);
        return NULL;
    }

    /* Immediately sever mod-monitor parallel dry passthrough so Mute is 100% silent & Aux FX have zero comb-filtering */
    isolate_mod_monitor_passthrough(client);

    /* Count actual hardware capture & playback ports */
    int cap_count = 0, play_count = 0;
    const char **cap_ports = jack_get_ports(client, "^system:capture_", NULL, JackPortIsOutput);
    if (cap_ports) {
        while (cap_ports[cap_count]) cap_count++;
        jack_free(cap_ports);
    }
    const char **play_ports = jack_get_ports(client, "^system:playback_", NULL, JackPortIsInput);
    if (play_ports) {
        while (play_ports[play_count]) play_count++;
        jack_free(play_ports);
    }
    if (cap_count > 0) g_state.hw_in_count = cap_count;
    if (play_count > 0) g_state.hw_out_count = play_count;

    /* Smart I/O Auto-Map (or honor restored persistent input patches if loaded from disk) */
    int auto_inputs = (cap_count <= 4 && strstr(g_state.hw_card_id, "EVO4") != NULL) ? 2 : cap_count;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char my_port[48];
        snprintf(my_port, sizeof(my_port), "modep-meter-daemon:in_%d", i + 1);
        int patch_in = g_state_loaded_from_disk ? g_state.ch_input_patch[i] : ((i + 1 <= auto_inputs) ? (i + 1) : 0);
        if (patch_in > 0 && patch_in <= cap_count) {
            char sys_port[48];
            snprintf(sys_port, sizeof(sys_port), "system:capture_%d", patch_in);
            jack_connect(client, sys_port, my_port);
            g_state.ch_input_patch[i] = patch_in;
        } else if (!g_state_loaded_from_disk) {
            g_state.ch_input_patch[i] = 0; /* Virtual / Tape only */
        }
    }

    jack_connect(client, "modep-meter-daemon:out_l", "system:playback_1");
    jack_connect(client, "modep-meter-daemon:out_r", "system:playback_2");
    if (play_count >= 4 && strstr(g_state.hw_card_id, "EVO4") == NULL) {
        for (int m = 0; m < NUM_MONITORS && (m + 3) <= play_count; m++) {
            char mon_p[48], sys_p[48];
            snprintf(mon_p, sizeof(mon_p), "modep-meter-daemon:mon_%d", m + 1);
            snprintf(sys_p, sizeof(sys_p), "system:playback_%d", m + 3);
            jack_connect(client, mon_p, sys_p);
        }
    }
    jack_connect(client, "system:playback_1", "modep-meter-daemon:master_l");
    jack_connect(client, "system:playback_2", "modep-meter-daemon:master_r");
    g_jack_client = client;
    fprintf(stderr, "[modep-meter-daemon] Attached to JACK (%s: %d in / %d out) at %.0f Hz (mod-monitor isolated)\n",
        g_state.hw_card_name, g_state.hw_in_count, g_state.hw_out_count, g_state.sample_rate);
    return client;
}

static int is_alsa_usb_card_present(const char *card_id) {
    FILE *fp = fopen("/proc/asound/cards", "r");
    if (!fp) return 0;
    char buf[2048];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    if (card_id && card_id[0] && strcmp(card_id, "dummy") != 0) {
        if (strstr(buf, card_id) != NULL) return 1;
    }
    return (strstr(buf, "USB-Audio") != NULL || strstr(buf, "EVO4") != NULL || strstr(buf, "UMC1820") != NULL);
}

/* Dedicated JACK & USB Audio Hotplug/Zombie Watchdog Thread:
 * Keeps try_attach_jack() (which can block up to 5s inside libjack if jackd is in zombie state)
 * 100% off the 60FPS lws_service WebSocket thread, and automatically restarts jack.service if
 * a USB audio glitch stops the ALSA driver ("Driver is not running" / frozen jack_heartbeat). */
static void *jack_watchdog_thread(void *arg) {
    (void)arg;
    g_jack_client = try_attach_jack();
    unsigned int prev_hb = g_state.jack_heartbeat;
    int stall_ticks = 0;
    int attach_fail_ticks = 0;

    while (1) {
        usleep(1000000); /* 1.0s watchdog cadence */
        if (g_jack_restart_requested) {
            g_jack_restart_requested = 0;
            jack_client_t *old_c = g_jack_client;
            g_jack_client = NULL;
            if (old_c) jack_client_close(old_c);
            int rc = system("sudo systemctl restart jack.service");
            (void)rc;
            usleep(800000);
            g_jack_client = try_attach_jack();
            prev_hb = g_state.jack_heartbeat;
            stall_ticks = 0;
            attach_fail_ticks = 0;
            continue;
        }

        if (g_jack_client) {
            unsigned int cur_hb = g_state.jack_heartbeat;
            if (cur_hb == prev_hb) {
                stall_ticks++;
                if (stall_ticks >= 3 && is_alsa_usb_card_present(g_state.hw_card_id)) {
                    fprintf(stderr, "[modep-meter-daemon] JACK process callback stalled for %ds while ALSA card is present — self-healing jack.service\n", stall_ticks);
                    g_jack_client = NULL;
                    int rc = system("sudo killall -9 jackd jackdmp 2>/dev/null; sudo systemctl restart jack.service");
                    (void)rc;
                    usleep(1000000);
                    g_jack_client = try_attach_jack();
                    prev_hb = g_state.jack_heartbeat;
                    stall_ticks = 0;
                }
            } else {
                prev_hb = cur_hb;
                stall_ticks = 0;
            }
        } else {
            g_jack_client = try_attach_jack();
            if (g_jack_client) {
                prev_hb = g_state.jack_heartbeat;
                stall_ticks = 0;
                attach_fail_ticks = 0;
            } else {
                attach_fail_ticks++;
                if (attach_fail_ticks >= 2 && is_alsa_usb_card_present(g_state.hw_card_id)) {
                    fprintf(stderr, "[modep-meter-daemon] Cannot attach to JACK while ALSA card is present — restarting zombie jack.service\n");
                    int rc = system("sudo killall -9 jackd jackdmp 2>/dev/null; sudo systemctl restart jack.service");
                    (void)rc;
                    usleep(1000000);
                    g_jack_client = try_attach_jack();
                    attach_fail_ticks = 0;
                }
            }
        }
    }
    return NULL;
}

static void handle_shutdown_signal(int sig) {
    (void)sig;
    save_persistent_mixer_state(&g_state);
    _exit(0);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    init_default_mixer_state(&g_state, 48000.0f);
    load_persistent_mixer_state(&g_state);
    scan_recording_drives(&g_state);
    signal(SIGTERM, handle_shutdown_signal);
    signal(SIGINT,  handle_shutdown_signal);

    pthread_t disk_tid;
    pthread_create(&disk_tid, NULL, tape_disk_writer_thread, &g_state);
    pthread_detach(disk_tid);

    pthread_t udp_tid;
    pthread_create(&udp_tid, NULL, udp_discovery_thread, NULL);
    pthread_detach(udp_tid);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = METER_PORT;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    g_context = lws_create_context(&info);
    if (!g_context) {
        fprintf(stderr, "Failed to create libwebsockets context\n");
        return 1;
    }

    pthread_t wakeup_tid;
    pthread_create(&wakeup_tid, NULL, ws_60fps_wakeup_thread, NULL);
    pthread_detach(wakeup_tid);

    pthread_t jack_wd_tid;
    pthread_create(&jack_wd_tid, NULL, jack_watchdog_thread, NULL);
    pthread_detach(jack_wd_tid);

    printf("MODEP Real-Time Audio Matrix & Meter Daemon (60FPS + Auto-Save) running on ws://0.0.0.0:%d\n", METER_PORT);

    long long last_broadcast = now_ms();
    long long last_sys_poll = now_ms();
    unsigned int last_hb = g_state.jack_heartbeat;

    while (1) {
        lws_service(g_context, 0);
        long long t = now_ms();

        /* Every 1000ms: sample lightweight Pi CPU / JACK DSP / Temp */
        if (t - last_sys_poll >= 1000) {
            last_sys_poll = t;
            update_pi_system_telemetry(&g_state, g_jack_client);
        }

        /* 60FPS (16ms) broadcast loop across all connected tablets & Web Portal clients */
        if (t - last_broadcast >= 15) {
            long long dt_ms = t - last_broadcast;
            last_broadcast = t;
            /* Fallback wall-clock tape counter advance if JACK callback is temporarily offline */
            unsigned int cur_hb = g_state.jack_heartbeat;
            if (cur_hb == last_hb && (g_state.tape_transport == 1 || g_state.tape_transport == 2)) {
                int add_frames = (int)((dt_ms * (g_state.sample_rate > 8000.0f ? g_state.sample_rate : 48000.0f)) / 1000.0f);
                g_state.tape_frame_pos += add_frames;
            }
            last_hb = cur_hb;
            for (int i = 0; i < g_num_wsi; i++) {
                if (g_wsi_list[i] && !lws_send_pipe_choked(g_wsi_list[i])) {
                    g_wsi_needs_meters[i] = 1;
                    lws_callback_on_writable(g_wsi_list[i]);
                }
            }
        } else {
            usleep(1500);
        }
    }

    if (g_jack_client) jack_client_close(g_jack_client);
    lws_context_destroy(g_context);
    return 0;
}


