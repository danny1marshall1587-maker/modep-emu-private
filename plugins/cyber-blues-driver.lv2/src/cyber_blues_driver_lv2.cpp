/*
 * Cyber Blues Driver - LV2 Plugin
 * Boss BD-2 Blues Driver + Vintage Fuzz Face Glassy Cleanup Volume Morphing
 *
 * Core Concept:
 *   Dynamic volume-driven circuit morphing:
 *     - Full Guitar Volume / Hard Pick Attack -> Boss BD-2 Blues Driver (multi-stage discrete JFET cascading clipping + active dual tone stack + Phat mod)
 *     - Rolled-Back Volume / Soft Pick Attack  -> Fuzz Face Glassy Cleanup (Germanium chime + treble bleed sparkle)
 *   Controls:
 *     - Bias: Center volume threshold for the morph transition
 *     - Sens: Transition slope/sensitivity
 *     - Sparkle: Glassy chime treble boost
 *     - Phat Mod: Low-end beef / Keeley mod toggle
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "lv2/lv2.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-blues-driver"

enum PortIndex {
    PORT_AUDIO_IN       = 0,
    PORT_AUDIO_OUT      = 1,
    PORT_BYPASS         = 2,
    PORT_GAIN           = 3,
    PORT_TONE           = 4,
    PORT_LEVEL          = 5,
    PORT_BIAS           = 6,
    PORT_SENS           = 7,
    PORT_SPARKLE        = 8,
    PORT_PHAT_MOD       = 9,
    PORT_MIX            = 10,
    PORT_COUNT          = 11
};

// ─── One-Pole IIR Filter ─────────────────────────────────────────────────────
struct OnePole {
    float z1 = 0.0f;

    float lp(float x, float fc, float sr) {
        float w  = 2.0f * (float)M_PI * fc / sr;
        float a0 = w / (1.0f + w);
        float b1 = 1.0f - a0;
        z1 = a0 * x + b1 * z1;
        return z1;
    }

    float hp(float x, float fc, float sr) {
        return x - lp(x, fc, sr);
    }

    void reset() { z1 = 0.0f; }
};

// ─── Direct Form II Transposed Biquad ────────────────────────────────────────
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;

    void reset() { s1 = s2 = 0.0f; }

    float process(float in) {
        float out = b0 * in + s1;
        s1 = b1 * in - a1 * out + s2;
        s2 = b2 * in - a2 * out;
        return out;
    }

    void lowShelf(float fc, float gainDB, float sr) {
        float A       = powf(10.0f, gainDB / 40.0f);
        float w0      = 2.0f * (float)M_PI * fc / sr;
        float cosw    = cosf(w0);
        float sinw    = sinf(w0);
        float alpha   = sinw / (2.0f * 0.707f);
        float a_plus  = (A + 1.0f);
        float a_minus = (A - 1.0f);
        float sqrt2a  = 2.0f * sqrtf(A) * alpha;

        float a0 = a_plus + a_minus*cosw + sqrt2a;
        b0 = (A * (a_plus - a_minus*cosw + sqrt2a)) / a0;
        b1 = (2.0f * A * (a_minus - a_plus*cosw))   / a0;
        b2 = (A * (a_plus - a_minus*cosw - sqrt2a)) / a0;
        a1 = (-2.0f * (a_minus + a_plus*cosw))      / a0;
        a2 = (a_plus + a_minus*cosw - sqrt2a)       / a0;
    }

    void highShelf(float fc, float gainDB, float sr) {
        float A       = powf(10.0f, gainDB / 40.0f);
        float w0      = 2.0f * (float)M_PI * fc / sr;
        float cosw    = cosf(w0);
        float sinw    = sinf(w0);
        float alpha   = sinw / (2.0f * 0.707f);
        float a_plus  = (A + 1.0f);
        float a_minus = (A - 1.0f);
        float sqrt2a  = 2.0f * sqrtf(A) * alpha;

        float a0 = a_plus - a_minus*cosw + sqrt2a;
        b0 = (A * (a_plus + a_minus*cosw + sqrt2a)) / a0;
        b1 = (-2.0f * A * (a_minus + a_plus*cosw))  / a0;
        b2 = (A * (a_plus + a_minus*cosw - sqrt2a)) / a0;
        a1 = (2.0f * (a_minus - a_plus*cosw))       / a0;
        a2 = (a_plus - a_minus*cosw - sqrt2a)       / a0;
    }

    void peaking(float fc, float gainDB, float Q, float sr) {
        float A     = powf(10.0f, gainDB / 40.0f);
        float w0    = 2.0f * (float)M_PI * fc / sr;
        float cosw  = cosf(w0);
        float sinw  = sinf(w0);
        float alpha = sinw / (2.0f * Q);

        float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cosw)     / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * cosw)     / a0;
        a2 = (1.0f - alpha / A) / a0;
    }
};

// ─── Precision Dual-Detector Dynamics Follower ──────────────────────────────
struct DynamicsDetector {
    float env_fast = 0.0f;
    float env_slow = 0.0f;

    float process(float x, float atk_fast, float rel_fast, float rel_slow) {
        float ax = fabsf(x);
        if (ax > env_fast)
            env_fast += atk_fast * (ax - env_fast);
        else
            env_fast += rel_fast * (ax - env_fast);

        if (ax > env_slow)
            env_slow += (atk_fast * 0.4f) * (ax - env_slow);
        else
            env_slow += rel_slow * (ax - env_slow);

        return 0.65f * env_fast + 0.35f * env_slow;
    }

    void reset() { env_fast = env_slow = 0.0f; }
};

// ─── 3-in-1 Master Noise Engine (Traynor YCV40 Circuit) ──────────────────────
// Complete analog noise treatment:
// 1. 10-Band Spectral Phase-Cancellation De-Noise Engine (Subtractive)
//    - Removes continuous op-amp & diode hiss without touching pick dynamics.
// 2. Dynamic De-Fizz (Sliding High-Cut + Expander)
//    - Eliminates low-voltage diode decay fizz & splatter.
// 3. Smart Zero-Floor Clean-Input Sidechain Gate
//    - Keyed off clean guitar input (raw_in) before distortion amplification.
//    - Dead silence between notes with instant pick-attack response.
struct TraynorNoiseEngine {
    double sampleRate = 48000.0;

    // 10-Band Spectral Phase Subtraction
    float specLpStates[9];
    float specLevelStates[10];
    float specCurrentGains[10];
    float specThresholdsDb[10];
    float specTLinear[10];
    float specBCoeffs[9];
    float specLevelAtt = 0.0f;
    float specLevelRel = 0.0f;
    float specAttCoeff = 0.0f;

    // Dynamic De-Fizz
    float defizzEnv = 0.0f;
    float defizzLpState = 0.0f;
    float defizzExpanderGain = 1.0f;
    float defizzAtk = 0.0f;
    float defizzRel = 0.0f;

    // Smart Clean-Input Gate
    OnePole gateScHp;
    OnePole gateScLp;
    float gateEnv = 0.0f;
    float gateGain = 0.0f;
    bool gateIsOpen = false;
    float gateAtk = 0.0f;
    float gateRel = 0.0f;
    float gateAtkSmooth = 0.0f;
    float gateRelSmooth = 0.0f;

    void init(double sr) {
        sampleRate = (sr > 1000.0) ? sr : 48000.0;
        const float sRateF = (float)sampleRate;

        // 1. 10-Band Spectral Engine
        specAttCoeff = 1.0f - expf(-1.0f / (0.0001f * sRateF));
        specLevelAtt = 1.0f - expf(-1.0f / (0.0020f * sRateF));
        specLevelRel = 1.0f - expf(-1.0f / (0.0600f * sRateF));

        const float multi[9] = { 60.0f, 150.0f, 400.0f, 800.0f, 1500.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f };
        for (int i = 0; i < 9; ++i) {
            specLpStates[i] = 0.0f;
            float w = 2.0f * (float)M_PI * multi[i] / sRateF;
            specBCoeffs[i] = std::min(0.99f, 1.0f - expf(-w));
        }

        const float defaultThreshDb[10] = {
            -72.0f, -70.0f, -68.0f, -65.0f, -63.0f,
            -60.0f, -57.0f, -55.0f, -54.0f, -53.0f
        };
        for (int i = 0; i < 10; ++i) {
            specLevelStates[i] = 0.0f;
            specCurrentGains[i] = 1.0f;
            specThresholdsDb[i] = defaultThreshDb[i];
            specTLinear[i] = powf(10.0f, specThresholdsDb[i] * 0.05f);
        }

        // 2. Dynamic De-Fizz
        defizzEnv = 0.0f;
        defizzLpState = 0.0f;
        defizzExpanderGain = 1.0f;
        defizzAtk = 1.0f - expf(-1.0f / (0.002f * sRateF));
        defizzRel = 1.0f - expf(-1.0f / (0.070f * sRateF));

        // 3. Smart Gate
        gateScHp.reset();
        gateScLp.reset();
        gateEnv = 0.0f;
        gateGain = 0.0f;
        gateIsOpen = false;
        gateAtk = 1.0f - expf(-1.0f / (sRateF * 0.0015f));      // 1.5 ms attack
        gateRel = 1.0f - expf(-1.0f / (sRateF * 0.220f));       // 220 ms detector release
        gateAtkSmooth = 1.0f - expf(-1.0f / (sRateF * 0.001f)); // 1.0 ms opening
        gateRelSmooth = 1.0f - expf(-1.0f / (sRateF * 0.180f)); // 180 ms fadeout
    }

    void reset() {
        for (int i = 0; i < 9; ++i) specLpStates[i] = 0.0f;
        for (int i = 0; i < 10; ++i) {
            specLevelStates[i] = 0.0f;
            specCurrentGains[i] = 1.0f;
        }
        defizzEnv = 0.0f;
        defizzLpState = 0.0f;
        defizzExpanderGain = 1.0f;
        gateScHp.reset();
        gateScLp.reset();
        gateEnv = 0.0f;
        gateGain = 0.0f;
        gateIsOpen = false;
    }

    inline float process(float wet, float cleanRawIn) {
        const float sRateF = (float)sampleRate;
        float s = wet;

        // ── Stage 1: 10-Band Spectral Phase-Cancellation De-Noise Engine ──
        float b[10];
        specLpStates[0] += (s - specLpStates[0]) * specBCoeffs[0];
        b[0] = specLpStates[0];
        for (int band = 1; band < 9; ++band) {
            specLpStates[band] += (s - specLpStates[band]) * specBCoeffs[band];
            b[band] = specLpStates[band] - specLpStates[band - 1];
        }
        b[9] = s - specLpStates[8];

        float spectralSum = 0.0f;
        for (int band = 0; band < 10; ++band) {
            float absB = fabsf(b[band]);
            float coeff = (absB > specLevelStates[band]) ? specLevelAtt : specLevelRel;
            specLevelStates[band] += (absB - specLevelStates[band]) * coeff;
            float r = specLevelStates[band];

            float targetG = 1.0f - (specTLinear[band] / (r + 1e-9f));
            targetG = std::max(0.0f, std::min(1.0f, targetG));

            float currentCoeff;
            if (targetG > specCurrentGains[band]) {
                currentCoeff = specAttCoeff;
            } else {
                float tailStab = 1.0f + (1.0f - std::min(1.0f, specCurrentGains[band])) * 8.0f;
                currentCoeff = 1.0f - expf(-1.0f / (0.010f * tailStab * sRateF));
            }
            specCurrentGains[band] += (targetG - specCurrentGains[band]) * currentCoeff;
            spectralSum += b[band] * specCurrentGains[band];
        }
        s = spectralSum;

        // ── Stage 2: Dynamic De-Fizz (Sliding High-Cut + Expander) ──
        float absP = fabsf(s);
        if (absP > defizzEnv) defizzEnv += defizzAtk * (absP - defizzEnv);
        else defizzEnv += defizzRel * (absP - defizzEnv);

        float normLevel = (defizzEnv - 0.0005f) / (0.0300f - 0.0005f);
        normLevel = std::max(0.0f, std::min(1.0f, normLevel));

        float dynCutoff = 2800.0f + normLevel * 11200.0f;
        float defizzW = 2.0f * (float)M_PI * dynCutoff / sRateF;
        float defizzAlpha = defizzW / (1.0f + defizzW);
        defizzLpState += defizzAlpha * (s - defizzLpState);
        s = defizzLpState;

        float expTarget = (normLevel > 0.15f) ? 1.0f : (normLevel / 0.15f);
        defizzExpanderGain += 0.005f * (expTarget - defizzExpanderGain);
        s *= defizzExpanderGain;

        // ── Stage 3: Smart Zero-Floor Clean-Input Sidechain Gate ──
        float sc = gateScLp.lp(gateScHp.hp(cleanRawIn, 100.0f, sRateF), 3200.0f, sRateF);
        float absSc = fabsf(sc);
        if (absSc > gateEnv) gateEnv += gateAtk * (absSc - gateEnv);
        else gateEnv += gateRel * (absSc - gateEnv);

        const float threshOpen  = 0.00080f; // ~ -62 dBFS on clean input
        const float threshClose = 0.00035f; // ~ -69 dBFS on clean input

        if (!gateIsOpen) {
            if (gateEnv >= threshOpen) gateIsOpen = true;
        } else {
            if (gateEnv < threshClose) gateIsOpen = false;
        }

        float targetGain = gateIsOpen ? 1.0f : 0.0f;
        float smoothRate = (targetGain > gateGain) ? gateAtkSmooth : gateRelSmooth;
        gateGain += smoothRate * (targetGain - gateGain);
        s *= gateGain;

        return s;
    }
};


// ─── Nonlinear Saturation Stages ─────────────────────────────────────────────

// Boss BD-2 Multi-Stage Discrete JFET + Diode Clipping
// Features sharp tube-like edge, rich upper harmonics, and subtle second-harmonic asymmetry
static inline float bd2_drive_stage(float x, float gain) {
    float g = 1.0f + gain * 12.0f; // 1..13 gain
    float in_sig = x * g;

    // First FET stage: soft asymmetric compression
    float fet1;
    if (in_sig >= 0.0f) {
        fet1 = tanhf(in_sig * 1.3f) * 0.92f;
    } else {
        fet1 = -atanf(-in_sig * 0.95f) * (2.0f / (float)M_PI) * 1.05f;
    }

    // Second stage: asymmetric clipping diodes with crunchy edge
    float fet2 = fet1 + 0.25f * (fet1 * fet1 - 0.1f);
    if (fet2 > 1.0f)  fet2 = 1.0f  + 0.2f * tanhf(fet2 - 1.0f);
    if (fet2 < -1.0f) fet2 = -1.0f + 0.2f * tanhf(fet2 + 1.0f);

    return fet2 * 0.85f;
}

// Fuzz Face Germanium Dynamic Circuit
static inline float fuzzface_circuit(float x, float gain) {
    float g = 2.0f + gain * 6.5f;
    float gx = x * g;
    if (gx >= 0.0f) {
        return tanhf(gx * 1.25f) * 0.88f;
    } else {
        return -tanhf(-gx * 0.85f) * 0.95f;
    }
}

// ─── Plugin Structure ────────────────────────────────────────────────────────
struct CyberBluesDriver {
    const float* audio_in  = nullptr;
    float*       audio_out = nullptr;

    const float* p_bypass   = nullptr;
    const float* p_gain     = nullptr;
    const float* p_tone     = nullptr;
    const float* p_level    = nullptr;
    const float* p_bias     = nullptr;
    const float* p_sens     = nullptr;
    const float* p_sparkle  = nullptr;
    const float* p_phat_mod = nullptr;
    const float* p_mix      = nullptr;

    double sample_rate = 48000.0;
    TraynorNoiseEngine noise_engine;

    OnePole hp_in;
    OnePole lp_pre;
    OnePole jfet_sag;

    Biquad  phat_boost_eq; // Phat mod low-end boost (120 Hz)
    Biquad  bd2_tone_low;  // BD-2 active tone low-mid shelf
    Biquad  bd2_tone_high; // BD-2 active tone treble shelf
    OnePole treble_sparkle_hp; // Fuzz Face glassy chime (+2.5 kHz)

    DynamicsDetector detector;
    float atk_coeff      = 0.08f;
    float rel_fast_coeff = 0.008f;
    float rel_slow_coeff = 0.002f;

    // Smoothed parameters
    float smooth_gain    = 0.45f;
    float smooth_tone    = 0.50f;
    float smooth_level   = 0.70f;
    float smooth_bias    = 0.50f;
    float smooth_sens    = 0.50f;
    float smooth_sparkle = 0.60f;
    float smooth_mix     = 1.00f;
    float smooth_morph   = 0.50f;

    float prev_tone_gain = 0.0f;
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                               const char*, const LV2_Feature* const*) {
    auto* p = new CyberBluesDriver();
    p->sample_rate = rate > 1000.0 ? rate : 48000.0;

    p->atk_coeff      = 1.0f - expf(-1.0f / ((float)p->sample_rate * 0.012f));
    p->rel_fast_coeff = 1.0f - expf(-1.0f / ((float)p->sample_rate * 0.070f));
    p->rel_slow_coeff = 1.0f - expf(-1.0f / ((float)p->sample_rate * 0.150f));

    p->noise_engine.init(p->sample_rate);
    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle handle, uint32_t port, void* data) {
    auto* p = (CyberBluesDriver*)handle;
    switch (port) {
        case PORT_AUDIO_IN:  p->audio_in  = (const float*)data; break;
        case PORT_AUDIO_OUT: p->audio_out = (float*)data;       break;
        case PORT_BYPASS:    p->p_bypass   = (const float*)data; break;
        case PORT_GAIN:      p->p_gain     = (const float*)data; break;
        case PORT_TONE:      p->p_tone     = (const float*)data; break;
        case PORT_LEVEL:     p->p_level    = (const float*)data; break;
        case PORT_BIAS:      p->p_bias     = (const float*)data; break;
        case PORT_SENS:      p->p_sens     = (const float*)data; break;
        case PORT_SPARKLE:   p->p_sparkle  = (const float*)data; break;
        case PORT_PHAT_MOD:  p->p_phat_mod = (const float*)data; break;
        case PORT_MIX:       p->p_mix      = (const float*)data; break;
    }
}

static void activate(LV2_Handle handle) {
    auto* p = (CyberBluesDriver*)handle;
    p->hp_in.reset();
    p->lp_pre.reset();
    p->jfet_sag.reset();
    p->phat_boost_eq.reset();
    p->bd2_tone_low.reset();
    p->bd2_tone_high.reset();
    p->treble_sparkle_hp.reset();
    p->detector.reset();
    p->noise_engine.reset();

    p->smooth_gain    = 0.45f;
    p->smooth_tone    = 0.50f;
    p->smooth_level   = 0.70f;
    p->smooth_bias    = 0.50f;
    p->smooth_sens    = 0.50f;
    p->smooth_sparkle = 0.60f;
    p->smooth_mix     = 1.00f;
    p->smooth_morph   = 0.50f;
    p->prev_tone_gain = 0.0f;
}

static void run(LV2_Handle handle, uint32_t n_samples) {
    auto* p = (CyberBluesDriver*)handle;
    if (!p->audio_in || !p->audio_out) return;

    const float* in  = p->audio_in;
    float*       out = p->audio_out;

    if (p->p_bypass && *p->p_bypass < 0.5f) {
        if (out != in) {
            std::memcpy(out, in, n_samples * sizeof(float));
        }
        return;
    }

    // Read control ports
    float t_gain    = p->p_gain     ? std::max(0.0f, std::min(100.0f, *p->p_gain)) / 100.0f : 0.45f;
    float t_tone    = p->p_tone     ? std::max(0.0f, std::min(100.0f, *p->p_tone)) / 100.0f : 0.50f;
    float t_level   = p->p_level    ? std::max(0.0f, std::min(100.0f, *p->p_level)) / 100.0f : 0.70f;
    float t_bias    = p->p_bias     ? std::max(0.0f, std::min(100.0f, *p->p_bias)) / 100.0f : 0.50f;
    float t_sens    = p->p_sens     ? std::max(0.0f, std::min(100.0f, *p->p_sens)) / 100.0f : 0.50f;
    float t_sparkle = p->p_sparkle  ? std::max(0.0f, std::min(100.0f, *p->p_sparkle)) / 100.0f : 0.60f;
    bool  phat_on   = p->p_phat_mod ? (*p->p_phat_mod > 0.5f) : false;
    float t_mix     = p->p_mix      ? std::max(0.0f, std::min(100.0f, *p->p_mix)) / 100.0f : 1.00f;

    // Boss BD-2 Active Dual Tone Stack:
    // Sweeps from warm/fat low-mid emphasis to razor-sharp presence bite
    float tone_gain = (t_tone - 0.5f) * 16.0f; // -8 dB .. +8 dB
    if (fabsf(tone_gain - p->prev_tone_gain) > 0.05f) {
        p->bd2_tone_low.lowShelf(400.0f, -tone_gain * 0.5f, (float)p->sample_rate);
        p->bd2_tone_high.highShelf(3500.0f, tone_gain, (float)p->sample_rate);
        p->prev_tone_gain = tone_gain;
    }

    // Keeley Phat Mod: +4.5 dB low shelf at 120 Hz
    if (phat_on) {
        p->phat_boost_eq.lowShelf(120.0f, 4.5f, (float)p->sample_rate);
    }

    float smooth_coeff = 1.0f - expf(-1.0f / ((float)p->sample_rate * 0.010f));
    float morph_coeff  = 1.0f - expf(-1.0f / ((float)p->sample_rate * 0.020f));

    for (uint32_t i = 0; i < n_samples; ++i) {
        p->smooth_gain    += smooth_coeff * (t_gain    - p->smooth_gain);
        p->smooth_tone    += smooth_coeff * (t_tone    - p->smooth_tone);
        p->smooth_level   += smooth_coeff * (t_level   - p->smooth_level);
        p->smooth_bias    += smooth_coeff * (t_bias    - p->smooth_bias);
        p->smooth_sens    += smooth_coeff * (t_sens    - p->smooth_sens);
        p->smooth_sparkle += smooth_coeff * (t_sparkle - p->smooth_sparkle);
        p->smooth_mix     += smooth_coeff * (t_mix     - p->smooth_mix);

        float raw_in = in[i];
        float dry    = raw_in;

        // 1. Input Buffering & Conditioning
        float x = raw_in;
        if (phat_on) {
            x = p->phat_boost_eq.process(x);
        }

        // Discrete FET input coupling
        x = p->hp_in.hp(x, 65.0f, (float)p->sample_rate);
        x = p->lp_pre.lp(x, 9000.0f, (float)p->sample_rate);

        // Power rail sag on hard strumming
        float sag_env = p->jfet_sag.lp(fabsf(x), 30.0f, (float)p->sample_rate);
        x *= (1.0f / (1.0f + sag_env * 0.9f));

        // 2. Dynamic Volume Tracking for Circuit Morphing
        float input_level = p->detector.process(raw_in, p->atk_coeff, p->rel_fast_coeff, p->rel_slow_coeff);

        float center_threshold = 0.015f + p->smooth_bias * 0.22f; // Morph center
        float transition_width = 0.02f + (1.0f - p->smooth_sens) * 0.15f; // Morph width

        float morph_raw = 0.5f + (input_level - center_threshold) / (transition_width * 2.0f);
        float target_morph = std::max(0.0f, std::min(1.0f, morph_raw));

        p->smooth_morph += morph_coeff * (target_morph - p->smooth_morph);
        float morph = p->smooth_morph; // 0 = Glassy Clean, 1 = Boss BD-2 Drive

        // ─── Circuit Path A: Boss BD-2 Blues Driver ───
        float x_bd2 = bd2_drive_stage(x, p->smooth_gain);
        x_bd2 = p->bd2_tone_low.process(x_bd2);
        x_bd2 = p->bd2_tone_high.process(x_bd2);

        // ─── Circuit Path B: Fuzz Face Glassy Cleanup ───
        float x_fuzz = fuzzface_circuit(x, p->smooth_gain);
        float sparkle_hp = p->treble_sparkle_hp.hp(x, 2400.0f, (float)p->sample_rate);
        float sparkle_boost = 1.0f + p->smooth_sparkle * 1.6f;
        float x_glassy_clean = x * 1.08f + sparkle_hp * (0.65f * sparkle_boost);

        float x_fuzz_path = morph * x_fuzz + (1.0f - morph) * x_glassy_clean;

        // ─── Dynamic Volume Crossfade ───
        float x_blended = morph * x_bd2 + (1.0f - morph) * x_fuzz_path;

        // 3. Master Level & Makeup
        float makeup = 1.65f;
        float vol = p->smooth_level * p->smooth_level * makeup;
        float wet = x_blended * vol;
        wet = p->noise_engine.process(wet, raw_in);

        // 4. Dry / Wet Mix Blend
        float final_out = (1.0f - p->smooth_mix) * dry + p->smooth_mix * wet;

        if (final_out > 1.2f)  final_out = 1.2f  - 0.2f * expf(-(final_out - 1.2f));
        if (final_out < -1.2f) final_out = -1.2f + 0.2f * expf(final_out + 1.2f);

        out[i] = final_out;
    }
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle handle) {
    delete (CyberBluesDriver*)handle;
}

static const void* extension_data(const char*) {
    return nullptr;
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
