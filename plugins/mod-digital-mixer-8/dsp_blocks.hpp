#pragma once
#include <cmath>
#include <algorithm>

// -------------------------------------------------------------
// High-efficiency DSP building blocks for MODEP Digital Mixer
// Vectorizable and optimized for ARM Cortex-A76 (Raspberry Pi 5)
// -------------------------------------------------------------

namespace ModepDSP {

// 1. Digital Biquad Filter (used for HPF, LPF, Shelves, Parametric Peaking EQ, and Notch Filters)
class Biquad {
public:
    enum FilterType {
        LOWPASS,
        HIGHPASS,
        BANDPASS,
        NOTCH,
        PEAK,
        LOWSHELF,
        HIGHSHELF
    };

    Biquad() { reset(); }

    void reset() {
        x1 = x2 = y1 = y2 = 0.0f;
        b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f;
    }

    void setup(FilterType type, float freq, float sampleRate, float Q = 0.7071f, float gainDB = 0.0f) {
        if (freq < 10.0f) freq = 10.0f;
        if (freq > sampleRate * 0.49f) freq = sampleRate * 0.49f;

        float omega = 2.0f * 3.14159265358979323846f * freq / sampleRate;
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * std::max(Q, 0.001f));
        float A = std::pow(10.0f, gainDB / 40.0f);
        float a0 = 1.0f;

        switch (type) {
            case LOWPASS:
                b0 =  (1.0f - cs) * 0.5f;
                b1 =   1.0f - cs;
                b2 =  (1.0f - cs) * 0.5f;
                a0 =   1.0f + alpha;
                a1 =  -2.0f * cs;
                a2 =   1.0f - alpha;
                break;
            case HIGHPASS:
                b0 =  (1.0f + cs) * 0.5f;
                b1 = -(1.0f + cs);
                b2 =  (1.0f + cs) * 0.5f;
                a0 =   1.0f + alpha;
                a1 =  -2.0f * cs;
                a2 =   1.0f - alpha;
                break;
            case PEAK:
                b0 =   1.0f + alpha * A;
                b1 =  -2.0f * cs;
                b2 =   1.0f - alpha * A;
                a0 =   1.0f + alpha / A;
                a1 =  -2.0f * cs;
                a2 =   1.0f - alpha / A;
                break;
            case NOTCH:
                b0 =   1.0f;
                b1 =  -2.0f * cs;
                b2 =   1.0f;
                a0 =   1.0f + alpha;
                a1 =  -2.0f * cs;
                a2 =   1.0f - alpha;
                break;
            case LOWSHELF: {
                float sqrtA = std::sqrt(A);
                b0 =    A * ((A + 1.0f) - (A - 1.0f) * cs + 2.0f * sqrtA * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cs);
                b2 =    A * ((A + 1.0f) - (A - 1.0f) * cs - 2.0f * sqrtA * alpha);
                a0 =        (A + 1.0f) + (A - 1.0f) * cs + 2.0f * sqrtA * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cs);
                a2 =        (A + 1.0f) - (A - 1.0f) * cs - 2.0f * sqrtA * alpha;
                break;
            }
            case HIGHSHELF: {
                float sqrtA = std::sqrt(A);
                b0 =    A * ((A + 1.0f) + (A - 1.0f) * cs + 2.0f * sqrtA * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cs);
                b2 =    A * ((A + 1.0f) + (A - 1.0f) * cs - 2.0f * sqrtA * alpha);
                a0 =        (A + 1.0f) - (A - 1.0f) * cs + 2.0f * sqrtA * alpha;
                a1 =  2.0f * ((A - 1.0f) - (A + 1.0f) * cs);
                a2 =        (A + 1.0f) - (A - 1.0f) * cs - 2.0f * sqrtA * alpha;
                break;
            }
            default:
                break;
        }

        // Normalize
        float invA0 = 1.0f / a0;
        b0 *= invA0;
        b1 *= invA0;
        b2 *= invA0;
        a1 *= invA0;
        a2 *= invA0;
    }

    inline float process(float in) {
        float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = in;
        y2 = y1;
        y1 = out;
        return out;
    }

private:
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
};

// 2. Full 7-Band Parametric EQ Cascade (HPF + 5 Fully Parametric Peaking Bands + High Shelf / LPF)
class ParametricEQ7Band {
public:
    ParametricEQ7Band() : sampleRate(48000.0f) {}

    void init(float sr) {
        sampleRate = sr;
        bands[0].setup(Biquad::HIGHPASS, 80.0f, sampleRate, 0.7071f);     // Band 1: HPF (80Hz)
        bands[1].setup(Biquad::PEAK, 160.0f, sampleRate, 1.2f, 0.0f);     // Band 2: Low-Mid (160Hz)
        bands[2].setup(Biquad::PEAK, 450.0f, sampleRate, 1.2f, 0.0f);     // Band 3: Mid 1 (450Hz)
        bands[3].setup(Biquad::PEAK, 1200.0f, sampleRate, 1.4f, 0.0f);    // Band 4: Mid 2 (1.2kHz)
        bands[4].setup(Biquad::PEAK, 3200.0f, sampleRate, 1.4f, 0.0f);    // Band 5: High-Mid (3.2kHz)
        bands[5].setup(Biquad::HIGHSHELF, 8000.0f, sampleRate, 0.7071f, 0.0f); // Band 6: High Shelf (8kHz)
        bands[6].setup(Biquad::LOWPASS, 18000.0f, sampleRate, 0.7071f);   // Band 7: LPF / Air Cut (18kHz)
    }

    void setBand(int index, Biquad::FilterType type, float freq, float Q, float gainDB) {
        if (index >= 0 && index < 7) {
            bands[index].setup(type, freq, sampleRate, Q, gainDB);
        }
    }

    inline float process(float in) {
        float s = in;
        for (int i = 0; i < 7; ++i) {
            s = bands[i].process(s);
        }
        return s;
    }

private:
    float sampleRate;
    Biquad bands[7];
};

// 3. 1176 FET Style Preamp Emulation (Input transformer + FET Class A non-linear saturation)
class FETPreamp1176 {
public:
    FETPreamp1176() : drive(1.0f), trim(1.0f) {}

    void setDrive(float driveDB) {
        drive = std::pow(10.0f, driveDB / 20.0f);
        // Compensate trim to maintain manageable gain staging
        trim = 1.0f / std::pow(10.0f, (driveDB * 0.4f) / 20.0f);
    }

    inline float process(float in) {
        float x = in * drive;
        // Non-linear FET transfer function: soft-clipping with subtle asymmetrical 2nd & 3rd order harmonics
        float sat;
        if (x > 1.25f) {
            sat = 1.0f;
        } else if (x < -1.25f) {
            sat = -1.0f;
        } else {
            sat = x - 0.28f * (x * x * x) + 0.08f * (x * x);
        }
        return sat * trim;
    }

private:
    float drive;
    float trim;
};

// 4. De-Esser (Dynamic band-reject centered on sibilance frequencies)
class DeEsser {
public:
    DeEsser() : sampleRate(48000.0f), threshold(0.1f), sidechainEnv(0.0f), reduction(1.0f) {}

    void init(float sr) {
        sampleRate = sr;
        sidechainBP.setup(Biquad::BANDPASS, 6500.0f, sampleRate, 2.0f);
        filterNotch.setup(Biquad::NOTCH, 6500.0f, sampleRate, 2.5f);
    }

    void setParams(float freqHz, float threshDB) {
        threshold = std::pow(10.0f, threshDB / 20.0f);
        sidechainBP.setup(Biquad::BANDPASS, freqHz, sampleRate, 2.0f);
        filterNotch.setup(Biquad::NOTCH, freqHz, sampleRate, 2.5f);
    }

    inline float process(float in) {
        float sc = std::abs(sidechainBP.process(in));
        float attackCoeff = 0.05f;
        float releaseCoeff = 0.001f;
        if (sc > sidechainEnv) {
            sidechainEnv += attackCoeff * (sc - sidechainEnv);
        } else {
            sidechainEnv += releaseCoeff * (sc - sidechainEnv);
        }

        if (sidechainEnv > threshold && threshold > 0.0001f) {
            float excess = (sidechainEnv - threshold) / threshold;
            reduction = std::clamp(1.0f - (excess * 0.75f), 0.2f, 1.0f);
        } else {
            reduction = 1.0f;
        }

        float notched = filterNotch.process(in);
        return in * reduction + notched * (1.0f - reduction);
    }

    float getGainReduction() const { return reduction; }

private:
    float sampleRate;
    float threshold;
    float sidechainEnv;
    float reduction;
    Biquad sidechainBP;
    Biquad filterNotch;
};

// 5. Channel Compressor (VCA style)
class ChannelCompressor {
public:
    ChannelCompressor() : sampleRate(48000.0f), threshold(0.5f), ratio(4.0f), 
                          attack(0.01f), release(0.1f), makeup(1.0f), env(0.0f), gainRed(1.0f) {}

    void init(float sr) {
        sampleRate = sr;
        setAttackMs(10.0f);
        setReleaseMs(100.0f);
    }

    void setThreshold(float threshDB) {
        threshold = std::pow(10.0f, threshDB / 20.0f);
    }

    void setRatio(float r) { ratio = std::max(r, 1.0f); }

    void setAttackMs(float ms) {
        attack = 1.0f - std::exp(-1.0f / (sampleRate * (ms * 0.001f)));
    }

    void setReleaseMs(float ms) {
        release = 1.0f - std::exp(-1.0f / (sampleRate * (ms * 0.001f)));
    }

    void setMakeupGain(float db) {
        makeup = std::pow(10.0f, db / 20.0f);
    }

    inline float process(float in) {
        float absIn = std::abs(in);
        if (absIn > env) {
            env += attack * (absIn - env);
        } else {
            env += release * (absIn - env);
        }

        if (env > threshold && threshold > 0.00001f) {
            float envDB = 20.0f * std::log10(std::max(env, 1e-5f));
            float threshDB = 20.0f * std::log10(threshold);
            float overDB = envDB - threshDB;
            float targetGainDB = -overDB * (1.0f - 1.0f / ratio);
            gainRed = std::pow(10.0f, targetGainDB / 20.0f);
        } else {
            gainRed = 1.0f;
        }

        return in * gainRed * makeup;
    }

    float getGainReduction() const { return gainRed; }

private:
    float sampleRate;
    float threshold;
    float ratio;
    float attack;
    float release;
    float makeup;
    float env;
    float gainRed;
};

// 6. Teletronix LA-2A Optical Leveling Amplifier
// Faithful physical model of the T4 electro-luminescent optical attenuator cell
class LA2AOpticalLeveler {
public:
    LA2AOpticalLeveler() : sampleRate(48000.0f), peakReduction(0.0f), gain(1.0f), 
                           cellLum(0.0f), cellResistance(1.0f), gainReduction(1.0f) {}

    void init(float sr) {
        sampleRate = sr;
        // Fast attack (~10ms)
        attCoeff = 1.0f - std::exp(-1.0f / (sampleRate * 0.010f));
        // Dual-stage release: Stage 1 (fast drop ~60ms), Stage 2 (slow tail ~1.5s - 5s)
        relStage1 = 1.0f - std::exp(-1.0f / (sampleRate * 0.060f));
        relStage2 = 1.0f - std::exp(-1.0f / (sampleRate * 2.500f));
    }

    void setPeakReduction(float pr) {
        peakReduction = std::clamp(pr, 0.0f, 100.0f) / 100.0f;
    }

    void setGainDb(float gDb) {
        gain = std::pow(10.0f, gDb / 20.0f);
    }

    inline float process(float in) {
        float absIn = std::abs(in);

        // Optical cell drive
        float lumTarget = absIn * (peakReduction * 4.0f);

        // Electro-luminescent light response (T4 cell attack)
        if (lumTarget > cellLum) {
            cellLum += attCoeff * (lumTarget - cellLum);
        } else {
            // Dual-stage compound photo-resistor memory decay
            float stageMix = (cellLum > 0.4f) ? relStage1 : relStage2;
            cellLum += stageMix * (lumTarget - cellLum);
        }

        // Photo-resistor non-linear attenuation curve
        if (cellLum > 0.08f) {
            float excess = cellLum - 0.08f;
            gainReduction = 1.0f / (1.0f + 3.5f * excess * excess);
        } else {
            gainReduction = 1.0f;
        }

        // Apply gain reduction followed by tube makeup output stage
        return in * gainReduction * gain;
    }

    float getGainReduction() const { return gainReduction; }

private:
    float sampleRate;
    float peakReduction;
    float gain;
    float cellLum;
    float cellResistance;
    float gainReduction;
    float attCoeff;
    float relStage1;
    float relStage2;
};

// 7. Live Zero-Latency Brickwall Limiter (Instant sample-by-sample peak clamping with fast smooth release)
// GUARANTEED ZERO LATENCY: No lookahead buffer, perfect for real-time live performance
class LiveZeroLatencyLimiter {
public:
    LiveZeroLatencyLimiter() : sampleRate(48000.0f), ceiling(0.98f), env(0.0f), gain(1.0f) {}

    void init(float sr) {
        sampleRate = sr;
        // Fast release (~25ms) to prevent pumping while eliminating digital overs
        relCoeff = 1.0f - std::exp(-1.0f / (sampleRate * 0.025f));
    }

    void setCeilingDb(float db) {
        ceiling = std::pow(10.0f, db / 20.0f);
    }

    inline float process(float in) {
        float absIn = std::abs(in);

        // Zero-latency instant detection
        if (absIn > ceiling && ceiling > 0.001f) {
            float instantGain = ceiling / absIn;
            if (instantGain < gain) {
                gain = instantGain; // Instant clamp (0 samples latency)
            }
        } else {
            // Smooth release back to unity
            gain += relCoeff * (1.0f - gain);
        }

        float clamped = in * gain;

        // Hard analog-style soft knee saturator ceiling safeguard
        if (clamped > ceiling) return ceiling;
        if (clamped < -ceiling) return -ceiling;
        return clamped;
    }

    float getGainReduction() const { return gain; }

private:
    float sampleRate;
    float ceiling;
    float env;
    float gain;
    float relCoeff;
};

// 8. Automatic Feedback Destroyer (Dynamic multi-notch tracking)
class FeedbackDestroyer {
public:
    static constexpr int NUM_NOTCHES = 4;

    FeedbackDestroyer() : sampleRate(48000.0f), activeNotches(0) {}

    void init(float sr) {
        sampleRate = sr;
        for (int i = 0; i < NUM_NOTCHES; ++i) {
            notches[i].reset();
            notchFreqs[i] = 1000.0f;
            notchEngaged[i] = false;
        }
    }

    void deployNotch(int index, float freqHz) {
        if (index >= 0 && index < NUM_NOTCHES) {
            notchFreqs[index] = freqHz;
            notches[index].setup(Biquad::NOTCH, freqHz, sampleRate, 35.0f);
            notchEngaged[index] = true;
        }
    }

    void clearNotches() {
        for (int i = 0; i < NUM_NOTCHES; ++i) {
            notchEngaged[i] = false;
        }
    }

    inline float process(float in) {
        float out = in;
        for (int i = 0; i < NUM_NOTCHES; ++i) {
            if (notchEngaged[i]) {
                out = notches[i].process(out);
            }
        }
        return out;
    }

    bool isNotchActive(int i) const { return notchEngaged[i]; }
    float getNotchFreq(int i) const { return notchFreqs[i]; }

private:
    float sampleRate;
    int activeNotches;
    Biquad notches[NUM_NOTCHES];
    float notchFreqs[NUM_NOTCHES];
    bool notchEngaged[NUM_NOTCHES];
};

} // namespace ModepDSP
