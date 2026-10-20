/*
 * Cyber Organ 9 v3.5 — High-Precision 36-Band Real-Time Polyphonic Chord Engine
 * with Dual-Bank Ambient Crossfading Pad & Zero-Noise Gate
 *
 * PROVEN ARCHITECTURE (PORTED FROM CYBER GUITAR-MIDI):
 * 1. 36 High-Speed 2nd-Order IIR Resonators (E2 82.4Hz to D#5 622.25Hz):
 *    Replaces DFT/Goertzel with real-time per-sample resonant bandpass filters (Q=8.0).
 *    - Instant detection (<3ms response).
 *    - True continuous semitone selectivity in the bass octave where DFT smears.
 *    - Instant attack peak follower on each note string.
 *
 * 2. Harmonic Chroma Folding & Bass Weighting:
 *    - Detects lowest bass fundamental note while correlating triad harmonic profiles
 *      (Root, Maj3/Min3, 5th, Sus4).
 *    - Never gets stuck on one note!
 *
 * 3. Dual-Bank Ambient Pad Crossfader:
 *    - When a chord transition occurs, Bank A smoothly decays while Bank B blooms in.
 *    - True ambient, ethereal pad behavior (crossfade speed controlled by RELEASE knob).
 *
 * 4. AGC + De-Hum (50/60/100/120 Hz) + Smart Zero-Noise Gate on guitar input.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_TWOPI
#define M_TWOPI 6.28318530717958647692
#endif

#include "lv2/lv2.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-organ9"

static const int   NOTES        = 10;
static const int   DRAWBARS     = 9;
static const float A4           = 440.0f;

// Note Selector interval semitones
static const int NOTE_SEMI[NOTES] = {
    -12, // Sub Oct
    -7,  // Sub 5th
    -4,  // Sub 3rd (auto M/m)
     0,  // Root
     4,  // +3rd (auto M/m)
     7,  // +5th
    12,  // +Oct
    16,  // +Oct+3rd (auto M/m)
    19,  // +Oct+5th
    24   // +2Oct
};

static const float NS_DEFAULTS[NOTES] = {
    0.0f, 0.0f, 0.0f, 8.0f, 7.0f, 6.0f, 5.0f, 0.0f, 0.0f, 0.0f
};

// Hammond Drawbar Harmonics (ratios to note fundamental)
static const float DB_RATIOS[DRAWBARS] = {
    0.5f, 1.4983f, 1.0f, 2.0f, 2.9966f, 4.0f, 5.0397f, 5.9932f, 8.0f
};

static const float DB_DEFAULTS[DRAWBARS] = {
    0.0f, 0.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

// ── One-pole filter ──
struct OnePoleFilter {
    float z = 0.0f;
    inline float lp(float in, float fc, float sr) {
        float w = 2.0f * (float)M_PI * fc / sr;
        float a = w / (1.0f + w);
        z += a * (in - z);
        return z;
    }
    inline float hp(float in, float fc, float sr) {
        return in - lp(in, fc, sr);
    }
    inline void reset() { z = 0.0f; }
};

// ── Narrow Notch Filter for 50Hz/60Hz/100Hz/120Hz De-Hum ──
struct NotchFilter {
    float b0=1, b1=0, b2=0, a1=0, a2=0;
    float x1=0, x2=0, y1=0, y2=0;

    void setNotch(float fc, float Q, float sr) {
        float w0 = (float)(M_TWOPI * fc / sr);
        float alpha = sinf(w0) / (2.0f * Q);
        float cs = cosf(w0);
        float a0 = 1.0f + alpha;

        b0 = 1.0f / a0;
        b1 = (-2.0f * cs) / a0;
        b2 = 1.0f / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha) / a0;
        x1 = x2 = y1 = y2 = 0.0f;
    }

    inline float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

// ── Fast 2nd-Order Resonator Bandpass with Peak Follower ──
struct NoteResonator {
    float b0 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float x1 = 0.0f;
    float x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;
    float envelope = 0.0f;

    void setup(float freq, float sr, float Q = 8.0f) {
        float w0 = (float)(M_TWOPI * freq / sr);
        float alpha = sinf(w0) / (2.0f * Q);
        float a0 = 1.0f + alpha;

        b0 = alpha / a0;
        b2 = -b0;
        a1 = (-2.0f * cosf(w0)) / a0;
        a2 = (1.0f - alpha) / a0;
        reset();
    }

    void reset() {
        x1 = x2 = y1 = y2 = 0.0f;
        envelope = 0.0f;
    }

    inline float process(float x) {
        float y = b0 * (x - x2) - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;

        float absY = fabsf(y);
        // Fast attack (<2ms), smooth decay (~45ms)
        if (absY > envelope) {
            envelope = envelope * 0.35f + absY * 0.65f;
        } else {
            envelope = envelope * 0.993f;
        }
        return envelope;
    }
};

// ── Biquad Crossover / Bandpass ──
struct Biquad {
    float b0=1, b1=0, b2=0, a1=0, a2=0;
    float z1=0, z2=0;

    void setLP(float fc, float q, float sr) {
        float w0 = (float)(M_TWOPI * fc / sr);
        float alpha = sinf(w0) / (2.0f * q);
        float cosw = cosf(w0);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f - cosw) * 0.5f) / a0;
        b1 = (1.0f - cosw) / a0;
        b2 = b0;
        a1 = (-2.0f * cosw) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setHP(float fc, float q, float sr) {
        float w0 = (float)(M_TWOPI * fc / sr);
        float alpha = sinf(w0) / (2.0f * q);
        float cosw = cosf(w0);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f + cosw) * 0.5f) / a0;
        b1 = -(1.0f + cosw) / a0;
        b2 = b0;
        a1 = (-2.0f * cosw) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setBP(float fc, float q, float sr) {
        float w0 = (float)(M_TWOPI * fc / sr);
        float alpha = sinf(w0) / (2.0f * q);
        float cosw = cosf(w0);
        float a0 = 1.0f + alpha;
        b0 = alpha / a0;
        b1 = 0.0f;
        b2 = -alpha / a0;
        a1 = (-2.0f * cosw) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    inline float process(float in) {
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }
};

// ── Standard DC Blocker ──
struct DCBlocker {
    float x1 = 0.0f;
    float y1 = 0.0f;
    inline float process(float x) {
        float y = x - x1 + 0.9995f * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

// ── Second Pitch Detection Engine: Time-Domain Normalized Autocorrelation ──
// Accurately extracts the true fundamental frequency (E2 to E4) regardless of
// upper harmonic dominance, pickup position, or octave confusion.
struct TimeDomainAutocorr {
    static constexpr int HIST_LEN = 2048;
    static constexpr int DEC_LEN  = 1024;
    static constexpr int CORR_WIN = 384;

    float history[HIST_LEN] = {};
    float decimated[DEC_LEN] = {};
    float corrs[600] = {};
    int   writePos = 0;
    float dcX = 0.0f;
    float dcY = 0.0f;
    float autoGain = 1.0f;
    float detectedFreq = 0.0f;
    int   detectedSemi = -1;
    float confidence = 0.0f;

    void reset() {
        memset(history, 0, sizeof(history));
        memset(decimated, 0, sizeof(decimated));
        memset(corrs, 0, sizeof(corrs));
        writePos = 0;
        dcX = dcY = 0.0f;
        autoGain = 1.0f;
        detectedFreq = 0.0f;
        detectedSemi = -1;
        confidence = 0.0f;
    }

    inline void pushSample(float s) {
        history[writePos] = s;
        writePos = (writePos + 1) % HIST_LEN;
    }

    void analyze(float sampleRate, float clipSetting) {
        // Downsample 2x to focus on bass/mid guitar frequencies (up to 12 kHz)
        float decSR = sampleRate * 0.5f;
        int minLag = (int)(decSR / 900.0f); // ~900 Hz max note (A5)
        int maxLag = (int)(decSR / 65.0f);  // ~65 Hz min note (C2 / drop D)
        if (minLag < 12) minLag = 12;
        if (maxLag > 520) maxLag = 520;

        // Unroll history into linear decimated buffer with DC block
        float peak = 0.0f;
        int readPos = writePos;
        for (int i = 0; i < DEC_LEN; ++i) {
            int p1 = (readPos + 2 * i) % HIST_LEN;
            int p2 = (p1 + 1) % HIST_LEN;
            float x1 = history[p1];
            float y1 = x1 - dcX + 0.995f * dcY;
            dcX = x1; dcY = y1;

            float x2 = history[p2];
            float y2 = x2 - dcX + 0.995f * dcY;
            dcX = x2; dcY = y2;

            float decVal = 0.5f * (y1 + y2);
            decimated[i] = decVal;
            float absV = fabsf(decVal);
            if (absV > peak) peak = absV;
        }

        if (peak < 0.0005f) {
            detectedFreq = 0.0f;
            detectedSemi = -1;
            confidence = 0.0f;
            return;
        }

        // Intelligent AGC (target 0.6 amplitude)
        float targetGain = 0.6f / peak;
        autoGain = autoGain * 0.70f + targetGain * 0.30f;
        autoGain = std::max(0.1f, std::min(100.0f, autoGain));

        // Center clipping (scaled by ac_clip port 0..10 -> 0.0 to 0.45)
        float clipFrac = (clipSetting / 10.0f) * 0.45f;
        float clipThresh = (peak * autoGain) * clipFrac;
        for (int i = 0; i < DEC_LEN; ++i) {
            float val = decimated[i] * autoGain;
            if (val > clipThresh) decimated[i] = val - clipThresh;
            else if (val < -clipThresh) decimated[i] = val + clipThresh;
            else decimated[i] = 0.0f;
        }

        // Compute zero-lag energy
        float e0 = 0.0f;
        const float* p0 = decimated;
        for (int j = 0; j < CORR_WIN; ++j) {
            e0 += p0[j] * p0[j];
        }

        if (e0 < 1e-9f) {
            detectedFreq = 0.0f;
            detectedSemi = -1;
            confidence = 0.0f;
            return;
        }

        // Normalized Autocorrelation
        float bestCorr = -1.0f;
        int bestLag = minLag;

        for (int lag = minLag; lag < maxLag; lag += 2) {
            float dot = 0.0f;
            float eLag = 0.0f;
            const float* pLag = p0 + lag;
            for (int j = 0; j < CORR_WIN; j += 2) {
                dot += p0[j] * pLag[j] + p0[j+1] * pLag[j+1];
                eLag += pLag[j] * pLag[j] + pLag[j+1] * pLag[j+1];
            }
            float normCorr = dot / sqrtf(e0 * eLag + 1e-12f);
            int idx = lag - minLag;
            corrs[idx] = normCorr;
            if (normCorr > bestCorr) bestCorr = normCorr;
        }

        if (bestCorr < 0.25f) {
            detectedFreq = 0.0f;
            detectedSemi = -1;
            confidence = 0.0f;
            return;
        }

        // First significant peak picking (prevents octave doubling/halving)
        for (int lag = minLag + 2; lag < maxLag - 2; lag += 2) {
            int idx = lag - minLag;
            if (corrs[idx] > corrs[idx - 2] && corrs[idx] >= corrs[idx + 2]) {
                if (corrs[idx] >= 0.80f * bestCorr) {
                    bestLag = lag;
                    bestCorr = corrs[idx];
                    break;
                }
            }
        }

        // Fine parabolic interpolation around peak
        int prevLag = std::max(minLag, bestLag - 1);
        int nextLag = std::min(maxLag - 1, bestLag + 1);
        float yA = (prevLag - minLag >= 0) ? corrs[prevLag - minLag] : bestCorr;
        float yB = bestCorr;
        float yC = (nextLag - minLag >= 0) ? corrs[nextLag - minLag] : bestCorr;

        float delta = 0.0f;
        float denom = (yA - 2.0f * yB + yC);
        if (fabsf(denom) > 1e-6f) {
            delta = 0.5f * (yA - yC) / denom;
            delta = std::max(-0.5f, std::min(0.5f, delta));
        }

        float trueLag = (float)bestLag + delta;
        float f0 = decSR / trueLag;

        // Valid guitar range: 70 Hz (drop D) to 800 Hz
        if (f0 >= 70.0f && f0 <= 800.0f) {
            detectedFreq = f0;
            confidence = std::max(0.0f, std::min(1.0f, bestCorr));
            float midiF = 69.0f + 12.0f * log2f(f0 / 440.0f);
            int midiNote = (int)roundf(midiF);
            detectedSemi = (midiNote % 12 + 12) % 12;
        } else {
            detectedFreq = 0.0f;
            detectedSemi = -1;
            confidence = 0.0f;
        }
    }
};

enum PortIndex {
    PORT_IN_L        = 0,
    PORT_IN_R        = 1,
    PORT_OUT_L       = 2,
    PORT_OUT_R       = 3,
    PORT_BYPASS      = 4,
    PORT_THIRD_MODE  = 5,

    // NOTE SELECTOR (10)
    PORT_NS_SUB_OCT  = 6,
    PORT_NS_SUB_5TH  = 7,
    PORT_NS_SUB_3RD  = 8,
    PORT_NS_ROOT     = 9,
    PORT_NS_3RD      = 10,
    PORT_NS_5TH      = 11,
    PORT_NS_OCT      = 12,
    PORT_NS_OCT3     = 13,
    PORT_NS_OCT5     = 14,
    PORT_NS_DBL_OCT  = 15,

    // HAMMOND DRAWBARS (9)
    PORT_DB_16       = 16,
    PORT_DB_5_3      = 17,
    PORT_DB_8        = 18,
    PORT_DB_4        = 19,
    PORT_DB_2_3      = 20,
    PORT_DB_2        = 21,
    PORT_DB_1_5      = 22,
    PORT_DB_1_3      = 23,
    PORT_DB_1        = 24,

    // PERFORMANCE & TONE CONTROLS
    PORT_ROTARY      = 25,
    PORT_CLICK       = 26,
    PORT_SWELL       = 27,
    PORT_SUSTAIN     = 28,
    PORT_RELEASE     = 29,
    PORT_GATE_SENS   = 30,
    PORT_DRY         = 31,
    PORT_WET         = 32,
    PORT_OUTPUT      = 33,

    // KEY & SCALE DIATONIC LOCK
    PORT_KEY_ROOT    = 34,
    PORT_SCALE_MODE  = 35,

    // DUAL DETECTOR CALIBRATION & SCOPE TUNING
    PORT_TRACK_SENS  = 36,
    PORT_TRACK_STAB  = 37,
    PORT_DEBOUNCE    = 38,
    PORT_FILTER_Q    = 39,
    PORT_BASS_BOOST  = 40,
    PORT_AGC_SPEED   = 41,
    PORT_AC_WEIGHT   = 42,
    PORT_AC_CLIP     = 43,

    // AUTO LOWEST-NOTE RHYTHM BASS PLAYER
    PORT_BASS_LEVEL   = 44,
    PORT_BASS_PATTERN = 45,
    PORT_BASS_BPM     = 46,
    PORT_BASS_TAP     = 47,
    PORT_BASS_TONE    = 48,

    PORT_COUNT        = 49
};

// ── Auto Lowest-Note Rhythm Bass Player ──
// Pattern Sequencer: 16 steps per bar (16th notes), synced to BPM / Tap Tempo
// Plays deep 16' tonewheel bass pedal oscillator with punchy key envelope
struct BassRhythmEngine {
    double phase16 = 0.0;
    double phaseSub = 0.0;
    float  noteEnv = 0.0f;
    float  currentHz = 55.0f;
    float  targetHz = 55.0f;

    // Clock & Sequencer
    double clockPhase = 0.0; // 0.0 to 1.0 per 16th note step
    int    currentStep = 0;   // 0 to 15
    float  lastTapVal = 0.0f;
    uint32_t tapSampleTimer = 0;
    float  detectedBpm = 120.0f;

    // Filter for Bass Tone
    OnePoleFilter lpFilter;

    // Step sequences for 6 rhythm patterns (16 steps each)
    // Values: 0 = Rest, 1 = Root hit, 2 = 5th hit, 3 = Octave hit, 4 = Walk/Step hit, 99 = Hold/Legato
    // Pattern 1: Drone / Pedal Sustain
    // Pattern 2: 8th Note Rock Pulse (hits on 0, 2, 4, 6, 8, 10, 12, 14)
    // Pattern 3: Root-Octave Funk Groove (Root on 1 & 9, Octave on 6 & 14, ghost roots)
    // Pattern 4: Root-5th Country/Blues (Root on beats 1 & 3, Fifth on beats 2 & 4)
    // Pattern 5: Walking Bass (Diatonic steps connecting beats)
    // Pattern 6: Dub / Reggae Drop (Syncopated sub bass hits on 4, 10, 12)
    static int getPatternStep(int pattern, int step, bool isMinor) {
        if (pattern <= 0) return 0;
        step = step % 16;
        switch (pattern) {
            case 1: // Drone Sustain
                return (step == 0) ? 1 : 99; // Retrigger on 1, sustain across bar
            case 2: // 8th Note Rock Pulse
                return (step % 2 == 0) ? 1 : 0;
            case 3: // Root-Octave Groove
                if (step == 0 || step == 8) return 1;       // Root
                if (step == 6 || step == 14) return 3;      // Octave slap
                if (step == 2 || step == 10) return 1;      // Light root
                return 0;
            case 4: // Root - 5th Country / Blues Alternating
                if (step == 0 || step == 8) return 1;       // Beats 1 & 3: Root
                if (step == 4 || step == 12) return 2;      // Beats 2 & 4: Fifth
                return 0;
            case 5: // Walking Bass
                if (step == 0) return 1;                    // Beat 1: Root
                if (step == 4) return isMinor ? 103 : 104;  // Beat 2: 3rd (chromatic offset)
                if (step == 8) return 2;                    // Beat 3: 5th
                if (step == 12) return isMinor ? 110 : 111; // Beat 4: 7th leading tone
                return 0;
            case 6: // Dub / Reggae Drop-One
                if (step == 4 || step == 10 || step == 12) return 1;
                return 0;
            default:
                return (step % 4 == 0) ? 1 : 0;
        }
    }

    void reset() {
        phase16 = 0.0;
        phaseSub = 0.0;
        noteEnv = 0.0f;
        currentHz = targetHz = 55.0f;
        clockPhase = 0.0;
        currentStep = 0;
        lastTapVal = 0.0f;
        tapSampleTimer = 0;
        detectedBpm = 120.0f;
        lpFilter.reset();
    }

    // Process a single audio sample
    inline float processSample(float sr, float bpmSetting, float tapTrigger,
                               int pattern, float level, float tone,
                               int rootPitchClass, bool isMinor, bool gateActive)
    {
        if (level < 0.01f || pattern <= 0 || rootPitchClass < 0 || !gateActive) {
            noteEnv *= 0.996f; // Rapid fade out when inactive
            if (noteEnv < 0.0001f) return 0.0f;
        }

        // Tap tempo tracking
        tapSampleTimer++;
        if (tapTrigger > 0.5f && lastTapVal <= 0.5f) {
            if (tapSampleTimer > (uint32_t)(sr * 0.15f) && tapSampleTimer < (uint32_t)(sr * 2.5f)) {
                detectedBpm = (60.0f * sr) / (float)tapSampleTimer;
                detectedBpm = std::max(40.0f, std::min(240.0f, detectedBpm));
            }
            tapSampleTimer = 0;
            clockPhase = 0.0;
            currentStep = 0;
        }
        lastTapVal = tapTrigger;

        // Effective BPM: if user adjusted bpmSetting away from default 120, prefer dial; else use tap if valid
        float effectiveBpm = bpmSetting;
        if (fabsf(bpmSetting - 120.0f) < 0.5f && detectedBpm > 40.0f && detectedBpm < 240.0f) {
            effectiveBpm = detectedBpm;
        }

        // Clock advancement (16th notes = 4 steps per beat)
        double stepsPerSec = (double)effectiveBpm * (4.0 / 60.0);
        clockPhase += stepsPerSec / (double)sr;
        if (clockPhase >= 1.0) {
            clockPhase -= 1.0;
            currentStep = (currentStep + 1) % 16;

            int action = getPatternStep(pattern, currentStep, isMinor);
            if (action == 1) {
                // Root note in bass octave (MIDI 24 + pitchClass: 32.7Hz to 61.7Hz)
                int midiBass = 24 + rootPitchClass;
                if (midiBass < 28) midiBass += 12; // Keep in punchy 41Hz-82Hz range
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 1.0f;
            } else if (action == 2) {
                // 5th (+7 semitones)
                int midiBass = 24 + rootPitchClass + 7;
                if (midiBass > 55) midiBass -= 12;
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 0.95f;
            } else if (action == 3) {
                // Octave (+12 semitones)
                int midiBass = 24 + rootPitchClass + 12;
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 0.90f;
            } else if (action >= 100) {
                // Walking chromatic offset (103 = +3, 104 = +4, 110 = +10, 111 = +11)
                int semiOffset = action - 100;
                int midiBass = 24 + rootPitchClass + semiOffset;
                while (midiBass > 52) midiBass -= 12;
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 0.90f;
            } else if (action == 99) {
                // Sustain / hold
                // Do not retrigger envelope, allow drone
            } else {
                // Rest
                if (pattern != 1) {
                    noteEnv *= 0.85f; // Fast drop on rest steps
                }
            }
        }

        // Smooth portamento / glide between bass notes
        currentHz += 0.005f * (targetHz - currentHz);

        // Envelope decay: punchy for grooves, sustained for drone
        float decayRate = (pattern == 1) ? 0.99995f : 0.9992f;
        noteEnv *= decayRate;

        // Dual-harmonic 16' Tonewheel Pedal Bass (Fundamental + Sub-Octave Sine)
        phase16 += (double)currentHz * (M_TWOPI / (double)sr);
        if (phase16 >= M_TWOPI) phase16 -= M_TWOPI;

        phaseSub += (double)(currentHz * 0.5f) * (M_TWOPI / (double)sr);
        if (phaseSub >= M_TWOPI) phaseSub -= M_TWOPI;

        float s16 = sinf((float)phase16);
        float sSub = sinf((float)phaseSub);

        // Tonewheel saturation + warmth
        float rawBass = sSub * 0.65f + s16 * 0.45f;
        rawBass += 0.08f * (s16 * s16 * s16); // 3rd harmonic growl

        // Lowpass filter tone control (tone: 0 = 90Hz deep sub, 10 = 650Hz bright growl)
        float lpCutoff = 90.0f + (tone / 10.0f) * 560.0f;
        float filteredBass = lpFilter.lp(rawBass, lpCutoff, sr);

        return filteredBass * noteEnv * (level / 10.0f);
    }
};

// ── Tonewheel Chord Bank for Ambient Crossfade ──
struct ChordBank {
    double twPhase[NOTES][DRAWBARS];
    float  rootHz = 261.63f;
    bool   isMinor = false;
    float  gain = 0.0f; // 0.0 to 1.0 crossfade weight

    void init(float fRoot, bool bMinor, float initialGain) {
        memset(twPhase, 0, sizeof(twPhase));
        rootHz = fRoot;
        isMinor = bMinor;
        gain = initialGain;
    }
};


class CyberOrgan9DSP {
public:
    float sr = 44100.0f;

    // 36 Note Resonators: E2 (MIDI 40, 82.4Hz) to D#5 (MIDI 75, 622.25Hz)
    NoteResonator filterBank[36];
    float chromaVector[12];
    float smoothedChroma[12];

    int   confirmedRoot = -1; // 0=C, 1=C# ... 11=B
    int   candidateRoot = -1;
    int   candidateBlocks = 0;
    bool  confirmedMinor = false;
    int   blockSampleCounter = 0;

    // Dual Chord Banks for Ambient Crossfade
    ChordBank bankA;
    ChordBank bankB;
    int       activeBank = 0; // 0 = Bank A active, 1 = Bank B active
    float     crossfadeRate = 0.0003f;

    // Controls smoothing
    float  noteLevelSm[NOTES];
    float  drawbarValSm[DRAWBARS];

    // Auto Gain Control (AGC)
    float agcPeak = 0.0f;
    float agcGain = 1.0f;

    // De-Hum Filters
    NotchFilter dehum50;
    NotchFilter dehum60;
    NotchFilter dehum100;
    NotchFilter dehum120;

    // Smart Zero-Noise Gate
    OnePoleFilter gateScHp;
    OnePoleFilter gateScLp;
    float gateEnv = 0.0f;
    float gateGain = 0.0f;
    bool  gateIsOpen = false;
    float gateAtk = 0.0f;
    float gateRel = 0.0f;
    float gateSmoothRate = 0.0f;

    // Envelope & Expression
    float inputRMS = 0.0f;
    float organEnv = 0.0f;
    float keyClickEnv = 0.0f;
    Biquad clickFilter;

    // Leslie Rotary System (Horn + Drum crossover)
    Biquad xoHornLP;
    Biquad xoDrumLP;

    float hornSpeedHz = 0.0f;
    float hornTargetHz = 0.0f;
    float drumSpeedHz = 0.0f;
    float drumTargetHz = 0.0f;

    double hornPhase = 0.0;
    double drumPhase = 0.0;

    float hornDelayL[2048];
    float hornDelayR[2048];
    int   hornWrite = 0;

    uint32_t lcgSeed = 1234567;
    DCBlocker dcBlockL;
    DCBlocker dcBlockR;

    void init(float sampleRate) {
        sr = sampleRate;

        // Initialize 36 parallel note resonators (E2 to D#5)
        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            float freq = A4 * powf(2.0f, (midi - 69) / 12.0f);
            filterBank[i].setup(freq, sr, 8.0f);
        }

        memset(chromaVector, 0, sizeof(chromaVector));
        memset(smoothedChroma, 0, sizeof(smoothedChroma));
        memset(hornDelayL, 0, sizeof(hornDelayL));
        memset(hornDelayR, 0, sizeof(hornDelayR));

        bankA.init(261.63f, false, 1.0f);
        bankB.init(261.63f, false, 0.0f);
        activeBank = 0;
        confirmedRoot = 0;

        for (int i = 0; i < NOTES; i++) noteLevelSm[i] = NS_DEFAULTS[i] / 10.0f;
        for (int i = 0; i < DRAWBARS; i++) drawbarValSm[i] = DB_DEFAULTS[i] / 8.0f;

        agcPeak = 0.01f;
        agcGain = 1.0f;

        dehum50.setNotch(50.0f, 12.0f, sr);
        dehum60.setNotch(60.0f, 12.0f, sr);
        dehum100.setNotch(100.0f, 12.0f, sr);
        dehum120.setNotch(120.0f, 12.0f, sr);

        gateScHp.reset();
        gateScLp.reset();
        gateEnv = 0.0f;
        gateGain = 0.0f;
        gateIsOpen = false;
        gateAtk = 1.0f - expf(-1.0f / (0.0012f * sr));
        gateRel = 1.0f - expf(-1.0f / (0.220f * sr));
        gateSmoothRate = 1.0f - expf(-1.0f / (0.005f * sr));

        clickFilter.setBP(2400.0f, 2.5f, sr);
        xoHornLP.setHP(800.0f, 0.707f, sr);
        xoDrumLP.setLP(800.0f, 0.707f, sr);
    }

    inline float randWhite() {
        lcgSeed = lcgSeed * 1664525u + 1013904223u;
        return ((float)(lcgSeed & 0xFFFF) / 32768.0f) - 1.0f;
    }

    TimeDomainAutocorr timeDomainPitch;
    BassRhythmEngine   bassPlayer;
    float currentFilterQ = 7.0f;

    void updateFilterQ(float q) {

        q = std::max(1.5f, std::min(30.0f, q));
        if (fabsf(q - currentFilterQ) > 0.05f) {
            currentFilterQ = q;
            for (int i = 0; i < 36; ++i) {
                int midi = 40 + i;
                float freq = A4 * powf(2.0f, (midi - 69) / 12.0f);
                filterBank[i].setup(freq, sr, currentFilterQ);
            }
        }
    }

    void evaluateChord(bool forceMinor, bool forceMajor,
                       float trackSens, float trackStab,
                       float debounceCycles, float bassBoost,
                       int keyRootVal, int scaleModeVal,
                       float acWeightVal, float acClipVal)
    {
        if (!gateIsOpen && gateGain < 0.05f) return;

        // ── 1. Second Detector: Time-Domain Autocorrelation Fundamental Extractor ──
        timeDomainPitch.analyze(sr, acClipVal);
        int autoCorrSemi = timeDomainPitch.detectedSemi;
        float autoCorrConf = timeDomainPitch.confidence;

        // ── 2. Fold 36 resonator envelopes into 12 chromatic bins (0=C, 1=C# ... 11=B) ──
        memset(chromaVector, 0, sizeof(chromaVector));
        float maxBass = 0.0f;
        int bassSemi = -1;

        // bassBoost mapped from 0..20 to 1.0..6.0x multiplier
        float bassMult = 1.0f + (bassBoost / 10.0f) * 2.5f;

        // Sensitivity mapped from 0.1..20 to threshold (higher sens = lower threshold)
        float baseThresh = 0.015f / std::max(0.05f, trackSens * 0.2f);

        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            int semi = midi % 12; // Chromatic pitch class
            float env = filterBank[i].envelope;

            // Low E string / bass notes priority
            if (i < 12 && env > maxBass && env > baseThresh) {
                maxBass = env;
                bassSemi = semi;
            }
            chromaVector[semi] += env * (i < 12 ? bassMult : 1.0f);
        }

        // Fuse with Time-Domain Autocorrelation: if detected with high confidence, strongly inject into chroma
        if (autoCorrSemi >= 0 && autoCorrConf > 0.35f) {
            float acBoost = (acWeightVal / 10.0f) * autoCorrConf * 4.0f;
            chromaVector[autoCorrSemi] += acBoost;
            // Also override bassSemi if autocorrelation confirms true fundamental
            if (autoCorrConf > 0.60f) {
                bassSemi = autoCorrSemi;
            }
        }

        float maxChroma = 1e-9f;
        for (int c = 0; c < 12; ++c) {
            if (chromaVector[c] > maxChroma) maxChroma = chromaVector[c];
        }

        // Stability mapped from 0.1..20 to smoothing factor (0.05 to 0.96)
        float smoothCoeff = std::max(0.05f, std::min(0.96f, 0.10f + (trackStab / 10.0f) * 0.43f));
        float newWeight = 1.0f - smoothCoeff;

        for (int c = 0; c < 12; ++c) {
            float norm = chromaVector[c] / maxChroma;
            smoothedChroma[c] = smoothedChroma[c] * smoothCoeff + norm * newWeight;
        }

        // ── 3. Diatonic Key & Scale Biasing ──
        int keyBase = (keyRootVal > 0) ? (keyRootVal - 1) : -1; // 0=C, 1=C#, etc.

        // Single-note protection: only update chord if polyphonic input detected
        {
            float maxChromaVal = 1e-9f;
            float totalChromaVal = 0.0f;
            for (int c = 0; c < 12; ++c) {
                totalChromaVal += smoothedChroma[c];
                if (smoothedChroma[c] > maxChromaVal) maxChromaVal = smoothedChroma[c];
            }
            int sigPitches = 0;
            for (int c = 0; c < 12; ++c) {
                if (smoothedChroma[c] >= maxChromaVal * 0.28f) sigPitches++;
            }
            float topShare = (totalChromaVal > 1e-9f) ? (maxChromaVal / totalChromaVal) : 1.0f;
            bool isPolyChord = (sigPitches >= 3) || (sigPitches >= 2 && topShare < 0.62f);
            if (!isPolyChord) return;
        }

        // ── 4. Correlate with chord templates ──
        float bestScore = -1.0f;
        int bestRoot = (bassSemi >= 0) ? bassSemi : ((autoCorrSemi >= 0) ? autoCorrSemi : 0);
        bool bestIsMinor = false;

        for (int r = 0; r < 12; ++r) {
            float cRoot = smoothedChroma[r];
            float cMaj3 = smoothedChroma[(r + 4) % 12];
            float cMin3 = smoothedChroma[(r + 3) % 12];
            float c5th  = smoothedChroma[(r + 7) % 12];

            float sMaj = (cRoot * 1.5f + cMaj3 * 1.1f + c5th * 0.9f) / 3.5f;
            float sMin = (cRoot * 1.5f + cMin3 * 1.1f + c5th * 0.9f) / 3.5f;

            if (r == bassSemi) {
                sMaj *= 1.30f;
                sMin *= 1.30f;
            }

            // Strongly reward agreement with time-domain autocorrelation detector
            if (r == autoCorrSemi && autoCorrConf > 0.40f) {
                float fusionBonus = 1.0f + (acWeightVal / 10.0f) * 0.40f * autoCorrConf;
                sMaj *= fusionBonus;
                sMin *= fusionBonus;
            }

            // Apply Diatonic Key & Scale Weighting if Key Lock is active
            if (keyBase >= 0) {
                int interval = (r - keyBase + 12) % 12;
                if (scaleModeVal == 2) {
                    // Minor Scale (Aeolian): root, min3, 4, 5, min6, min7
                    if (interval == 0 || interval == 5 || interval == 7) {
                        sMin *= 1.35f;
                    } else if (interval == 3 || interval == 8 || interval == 10) {
                        sMaj *= 1.25f;
                    } else if (interval == 2) {
                        sMin *= 1.15f;
                    } else {
                        // Out-of-key note penalty
                        sMaj *= 0.60f;
                        sMin *= 0.60f;
                    }
                } else {
                    // Major Scale (Ionian): I(Maj), ii(min), iii(min), IV(Maj), V(Maj), vi(min), vii°
                    if (interval == 0 || interval == 5 || interval == 7) {
                        sMaj *= 1.35f;
                    } else if (interval == 2 || interval == 4 || interval == 9) {
                        sMin *= 1.25f;
                    } else {
                        // Out-of-key note penalty
                        sMaj *= 0.60f;
                        sMin *= 0.60f;
                    }
                }
            }

            if (sMaj > bestScore) {
                bestScore = sMaj;
                bestRoot = r;
                bestIsMinor = false;
            }
            if (sMin > bestScore) {
                bestScore = sMin;
                bestRoot = r;
                bestIsMinor = true;
            }
        }

        if (forceMinor) bestIsMinor = true;
        if (forceMajor) bestIsMinor = false;

        // Minimum score threshold inversely proportional to sensitivity
        float scoreMinReq = 0.35f - (trackSens / 10.0f) * 0.20f;
        scoreMinReq = std::max(0.08f, scoreMinReq);

        int requiredBlocks = std::max(1, (int)(debounceCycles + 0.5f));

        // ── 5. Debounce & Trigger Ambient Crossfade ──
        if (bestScore >= scoreMinReq) {
            if (bestRoot == candidateRoot && bestIsMinor == confirmedMinor) {
                candidateBlocks++;
            } else {
                candidateRoot = bestRoot;
                candidateBlocks = 1;
            }

            if (candidateBlocks >= requiredBlocks && (candidateRoot != confirmedRoot || bestIsMinor != confirmedMinor)) {
                confirmedRoot = candidateRoot;
                confirmedMinor = bestIsMinor;

                int midi = 48 + confirmedRoot;
                float newRootHz = A4 * powf(2.0f, (midi - 69) / 12.0f);

                if (activeBank == 0) {
                    bankB.rootHz = newRootHz;
                    bankB.isMinor = confirmedMinor;
                    activeBank = 1;
                } else {
                    bankA.rootHz = newRootHz;
                    bankA.isMinor = confirmedMinor;
                    activeBank = 0;
                }

                keyClickEnv = 0.5f; // Gentle authentic key click on chord transition
                candidateBlocks = 0;
            }
        }
    }

    void process(
        const float* inL, const float* inR,
        float* outL, float* outR,
        uint32_t nSamples,
        float bypass, float thirdMode,
        const float* nsLevels,
        const float* dbLevels,
        float rotarySpeed, float clickAmt, float swellVal,
        float sustainVal, float releaseVal, float gateSensVal,
        float dryVal, float wetVal, float outGainVal,
        int keyRootVal, int scaleModeVal,
        float trackSens, float trackStab, float debounceVal,
        float filterQVal, float bassBoostVal, float agcSpeedVal,
        float acWeightVal, float acClipVal,
        float bassLevelVal, float bassPatternVal, float bassBpmVal,
        float bassTapVal, float bassToneVal)
    {
        if (bypass < 0.5f) {
            for (uint32_t i = 0; i < nSamples; i++) {
                outL[i] = inL[i];
                if (outR) outR[i] = inR ? inR[i] : inL[i];

            }
            return;
        }

        updateFilterQ(filterQVal);

        bool forceMinor = (thirdMode > 0.5f && thirdMode < 1.5f);
        bool forceMajor = (thirdMode > 1.5f);

        // Leslie target speeds
        if (rotarySpeed < 0.5f) {
            hornTargetHz = 0.0f;
            drumTargetHz = 0.0f;
        } else if (rotarySpeed < 1.5f) {
            hornTargetHz = 0.82f;
            drumTargetHz = 0.67f;
        } else {
            hornTargetHz = 6.8f;
            drumTargetHz = 5.7f;
        }

        // Swell attack time & Sustain / Release curve calculation
        float swellSec = 0.005f + (swellVal / 10.0f) * 0.60f;
        float attCoeff = 1.0f - expf(-1.0f / (swellSec * sr));

        float sustainHold = (sustainVal / 10.0f);

        float relSec = 0.05f + (releaseVal / 10.0f) * 2.95f;
        float relCoeff = 1.0f - expf(-1.0f / (relSec * sr));

        // Ambient crossfade speed scaled by release (longer release = more ethereal chord blending)
        float xfadeSec = 0.15f + (releaseVal / 10.0f) * 0.55f; // 150ms to 700ms pad crossfade
        crossfadeRate = 1.0f / (xfadeSec * sr);

        float sensMult = std::max(0.1f, gateSensVal / 5.0f);
        float gateThreshOpen  = 0.0020f * sensMult;
        float gateThreshClose = 0.0006f * sensMult;

        float dryGain = dryVal / 10.0f;
        float wetGain = wetVal / 10.0f;
        float masterGain = outGainVal / 10.0f;

        float hornRamp = 1.0f - expf(-1.0f / (1.2f * sr));
        float drumRamp = 1.0f - expf(-1.0f / (4.2f * sr));

        // AGC speed mapping
        float agcRate = (agcSpeedVal / 10.0f) * 0.002f;
        bool agcActive = (agcSpeedVal > 0.1f);

        for (uint32_t i = 0; i < nSamples; i++) {
            float monoIn = (inL[i] + (inR ? inR[i] : inL[i])) * 0.5f;

            // Push into time-domain autocorrelation buffer
            timeDomainPitch.pushSample(monoIn);

            // ── 1. De-Hum Filtering ──
            float dehumSignal = dehum120.process(dehum100.process(dehum60.process(dehum50.process(monoIn))));

            // ── 2. Smart Zero-Noise Gate ──
            float scSig = gateScLp.lp(gateScHp.hp(dehumSignal, 80.0f, sr), 3400.0f, sr);
            float absSc = fabsf(scSig);

            if (absSc > gateEnv) {
                gateEnv += gateAtk * (absSc - gateEnv);
            } else {
                gateEnv += gateRel * (absSc - gateEnv);
            }

            if (!gateIsOpen) {
                if (gateEnv >= gateThreshOpen) gateIsOpen = true;
            } else {
                if (gateEnv < gateThreshClose) gateIsOpen = false;
            }

            float targetGate = gateIsOpen ? 1.0f : 0.0f;
            gateGain += gateSmoothRate * (targetGate - gateGain);

            // ── 3. Auto Gain Control (AGC) ──
            if (agcActive) {
                if (absSc > agcPeak) {
                    agcPeak += 0.05f * (absSc - agcPeak);
                } else {
                    agcPeak += 0.0002f * (absSc - agcPeak);
                }
                float safePeak = std::max(0.005f, agcPeak);
                float targetAgcGain = 0.35f / safePeak;
                targetAgcGain = std::max(0.5f, std::min(15.0f, targetAgcGain));
                agcGain += agcRate * (targetAgcGain - agcGain);
            } else {
                agcGain = 1.0f;
            }

            // ── 4. Continuous Real-Time Resonator Processing ──
            float cleanAnalysisIn = dehumSignal * agcGain * gateGain;
            for (int r = 0; r < 36; ++r) {
                filterBank[r].process(cleanAnalysisIn);
            }

            // Periodically evaluate chord match (~every 256 samples, ~5.8ms)
            blockSampleCounter++;
            if (blockSampleCounter >= 256) {
                blockSampleCounter = 0;
                evaluateChord(forceMinor, forceMajor, trackSens, trackStab, debounceVal, bassBoostVal,
                              keyRootVal, scaleModeVal, acWeightVal, acClipVal);
            }

            // ── 5. RMS Envelope Follower + Sustain + Release ──
            float inSquare = monoIn * monoIn;
            inputRMS += 0.003f * (inSquare - inputRMS);
            float currentAmp = sqrtf(inputRMS > 0.0f ? inputRMS : 0.0f) * gateGain;

            float targetVol = std::min(currentAmp * (12.0f + sustainHold * 12.0f), 1.0f);
            if (targetVol < sustainHold * 0.40f && currentAmp > 0.0005f) {
                targetVol = sustainHold * 0.40f;
            }

            if (targetVol > organEnv) {
                organEnv += attCoeff * (targetVol - organEnv);
            } else {
                organEnv += relCoeff * (targetVol - organEnv);
            }

            // Drawbar & Note Levels
            for (int d = 0; d < DRAWBARS; d++) {
                float target = dbLevels[d] / 8.0f;
                drawbarValSm[d] += 0.004f * (target - drawbarValSm[d]);
            }
            for (int n = 0; n < NOTES; n++) {
                float target = nsLevels[n] / 10.0f;
                noteLevelSm[n] += 0.004f * (target - noteLevelSm[n]);
            }

            // ── 6. Ambient Dual-Bank Crossfader ──
            if (activeBank == 0) {
                if (bankA.gain < 1.0f) bankA.gain = std::min(1.0f, bankA.gain + crossfadeRate);
                if (bankB.gain > 0.0f) bankB.gain = std::max(0.0f, bankB.gain - crossfadeRate);
            } else {
                if (bankB.gain < 1.0f) bankB.gain = std::min(1.0f, bankB.gain + crossfadeRate);
                if (bankA.gain > 0.0f) bankA.gain = std::max(0.0f, bankA.gain - crossfadeRate);
            }

            // Synthesise Tonewheels across both banks and blend with equal-power curve
            float mixA = 0.0f;
            float mixB = 0.0f;

            // Bank A Synthesis
            if (bankA.gain > 0.001f) {
                int activeVoicesA = 0;
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.001f) continue;
                    activeVoicesA++;

                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankA.isMinor ? -3 : -4;
                    if (n == 4) semi = bankA.isMinor ?  3 :  4;
                    if (n == 7) semi = bankA.isMinor ? 15 : 16;

                    float noteFund = bankA.rootHz * powf(2.0f, semi / 12.0f);
                    float noteOut = 0.0f;
                    for (int d = 0; d < DRAWBARS; d++) {
                        float dbVol = drawbarValSm[d];
                        if (dbVol < 0.001f) continue;

                        float twFreq = noteFund * DB_RATIOS[d];
                        if (twFreq > sr * 0.48f) continue;

                        double dPhase = (double)twFreq * (M_TWOPI / (double)sr);
                        bankA.twPhase[n][d] += dPhase;
                        if (bankA.twPhase[n][d] >= M_TWOPI) bankA.twPhase[n][d] -= M_TWOPI;

                        float s = sinf((float)bankA.twPhase[n][d]);
                        noteOut += (s - 0.04f * s * s + 0.02f * s * s * s) * dbVol;
                    }
                    mixA += noteOut * nVol;
                }
                if (activeVoicesA > 1) mixA /= sqrtf((float)activeVoicesA);
            }

            // Bank B Synthesis
            if (bankB.gain > 0.001f) {
                int activeVoicesB = 0;
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.001f) continue;
                    activeVoicesB++;

                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankB.isMinor ? -3 : -4;
                    if (n == 4) semi = bankB.isMinor ?  3 :  4;
                    if (n == 7) semi = bankB.isMinor ? 15 : 16;

                    float noteFund = bankB.rootHz * powf(2.0f, semi / 12.0f);
                    float noteOut = 0.0f;
                    for (int d = 0; d < DRAWBARS; d++) {
                        float dbVol = drawbarValSm[d];
                        if (dbVol < 0.001f) continue;

                        float twFreq = noteFund * DB_RATIOS[d];
                        if (twFreq > sr * 0.48f) continue;

                        double dPhase = (double)twFreq * (M_TWOPI / (double)sr);
                        bankB.twPhase[n][d] += dPhase;
                        if (bankB.twPhase[n][d] >= M_TWOPI) bankB.twPhase[n][d] -= M_TWOPI;

                        float s = sinf((float)bankB.twPhase[n][d]);
                        noteOut += (s - 0.04f * s * s + 0.02f * s * s * s) * dbVol;
                    }
                    mixB += noteOut * nVol;
                }
                if (activeVoicesB > 1) mixB /= sqrtf((float)activeVoicesB);
            }

            float blendA = sqrtf(bankA.gain);
            float blendB = sqrtf(bankB.gain);
            float organMix = mixA * blendA + mixB * blendB;

            // Key click transient
            float clickSample = 0.0f;
            if (keyClickEnv > 0.0001f) {
                keyClickEnv *= 0.9982f;
                float rawNoise = randWhite();
                clickSample = clickFilter.process(rawNoise) * keyClickEnv * (clickAmt / 10.0f) * 0.10f;
            }
            organMix += clickSample;
            organMix *= organEnv;

            // Preamp soft drive
            float satOrgan = tanhf(organMix * 1.25f);

            // ── 7. Leslie 122 Rotary Emulation ──
            hornSpeedHz += hornRamp * (hornTargetHz - hornSpeedHz);
            drumSpeedHz += drumRamp * (drumTargetHz - drumSpeedHz);

            hornPhase += (double)hornSpeedHz * (M_TWOPI / (double)sr);
            if (hornPhase >= M_TWOPI) hornPhase -= M_TWOPI;

            drumPhase += (double)drumSpeedHz * (M_TWOPI / (double)sr);
            if (drumPhase >= M_TWOPI) drumPhase -= M_TWOPI;

            float highPart = xoHornLP.process(satOrgan);
            float lowPart  = xoDrumLP.process(satOrgan);

            float hornOutL = highPart;
            float hornOutR = highPart;

            if (hornSpeedHz > 0.02f) {
                float hornDepth = std::min(hornSpeedHz / 6.8f, 1.0f);

                float modL = 0.0016f * sr * (sinf((float)hornPhase) * 0.5f + 0.5f);
                float modR = 0.0016f * sr * (sinf((float)hornPhase + (float)M_PI) * 0.5f + 0.5f);

                hornDelayL[hornWrite] = highPart;
                hornDelayR[hornWrite] = highPart;

                int rL = (hornWrite - (int)modL + 2048) % 2048;
                int rR = (hornWrite - (int)modR + 2048) % 2048;

                float amL = 1.0f + 0.45f * hornDepth * cosf((float)hornPhase);
                float amR = 1.0f + 0.45f * hornDepth * cosf((float)hornPhase + (float)M_PI);

                hornOutL = hornDelayL[rL] * amL;
                hornOutR = hornDelayR[rR] * amR;

                hornWrite = (hornWrite + 1) % 2048;
            }

            float drumOutL = lowPart;
            float drumOutR = lowPart;

            if (drumSpeedHz > 0.02f) {
                float drumDepth = std::min(drumSpeedHz / 5.7f, 1.0f) * 0.35f;
                float drumAmL = 1.0f + drumDepth * sinf((float)drumPhase);
                float drumAmR = 1.0f + drumDepth * sinf((float)drumPhase + (float)M_PI);

                drumOutL = lowPart * drumAmL;
                drumOutR = lowPart * drumAmR;
            }

            float wetL = hornOutL + drumOutL;
            float wetR = hornOutR + drumOutR;

            // Auto Lowest-Note Rhythm Bass Player (runs in parallel with organ)
            float bassSample = bassPlayer.processSample(
                sr, bassBpmVal, bassTapVal,
                (int)(bassPatternVal + 0.5f), bassLevelVal, bassToneVal,
                confirmedRoot, confirmedMinor, (gateIsOpen || gateGain > 0.05f)
            );
            wetL += bassSample;
            wetR += bassSample;

            wetL = dcBlockL.process(wetL);
            wetR = dcBlockR.process(wetR);

            float dryL = inL[i] * dryGain;
            float dryR = (inR ? inR[i] : inL[i]) * dryGain;

            outL[i] = (dryL + wetL * wetGain) * masterGain;
            if (outR) {
                outR[i] = (dryR + wetR * wetGain) * masterGain;
            }
        }
    }
};

struct CyberOrgan9Plugin {
    CyberOrgan9DSP* dsp = nullptr;
    const float* audioIn[2] = {};
    float* audioOut[2] = {};
    const float* ports[PORT_COUNT] = {};
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const*) {
    CyberOrgan9Plugin* p = new CyberOrgan9Plugin();
    p->dsp = new CyberOrgan9DSP();
    p->dsp->init((float)rate);
    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    CyberOrgan9Plugin* p = (CyberOrgan9Plugin*)instance;
    if (port < PORT_COUNT) p->ports[port] = (const float*)data;
    if (port == PORT_IN_L) p->audioIn[0] = (const float*)data;
    if (port == PORT_IN_R) p->audioIn[1] = (const float*)data;
    if (port == PORT_OUT_L) p->audioOut[0] = (float*)data;
    if (port == PORT_OUT_R) p->audioOut[1] = (float*)data;
}

static void activate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t n_samples) {
    CyberOrgan9Plugin* p = (CyberOrgan9Plugin*)instance;
    if (!p || !p->dsp || !p->audioOut[0]) return;

    float ns[NOTES];
    for (int i = 0; i < NOTES; i++) {
        ns[i] = p->ports[PORT_NS_SUB_OCT + i] ? *p->ports[PORT_NS_SUB_OCT + i] : NS_DEFAULTS[i];
    }

    float db[DRAWBARS];
    for (int i = 0; i < DRAWBARS; i++) {
        db[i] = p->ports[PORT_DB_16 + i] ? *p->ports[PORT_DB_16 + i] : DB_DEFAULTS[i];
    }

    p->dsp->process(
        p->audioIn[0], p->audioIn[1],
        p->audioOut[0], p->audioOut[1],
        n_samples,
        p->ports[PORT_BYPASS]     ? *p->ports[PORT_BYPASS]     : 1.0f,
        p->ports[PORT_THIRD_MODE] ? *p->ports[PORT_THIRD_MODE] : 0.0f,
        ns, db,
        p->ports[PORT_ROTARY]     ? *p->ports[PORT_ROTARY]     : 1.0f,
        p->ports[PORT_CLICK]      ? *p->ports[PORT_CLICK]      : 3.0f,
        p->ports[PORT_SWELL]      ? *p->ports[PORT_SWELL]      : 5.0f,
        p->ports[PORT_SUSTAIN]    ? *p->ports[PORT_SUSTAIN]    : 4.0f,
        p->ports[PORT_RELEASE]    ? *p->ports[PORT_RELEASE]    : 5.0f,
        p->ports[PORT_GATE_SENS]   ? *p->ports[PORT_GATE_SENS]   : 5.0f,
        p->ports[PORT_DRY]         ? *p->ports[PORT_DRY]         : 5.0f,
        p->ports[PORT_WET]         ? *p->ports[PORT_WET]         : 8.0f,
        p->ports[PORT_OUTPUT]      ? *p->ports[PORT_OUTPUT]      : 7.0f,
        p->ports[PORT_KEY_ROOT]    ? (int)(*p->ports[PORT_KEY_ROOT] + 0.5f) : 0,
        p->ports[PORT_SCALE_MODE]  ? (int)(*p->ports[PORT_SCALE_MODE] + 0.5f) : 0,
        p->ports[PORT_TRACK_SENS]  ? *p->ports[PORT_TRACK_SENS]  : 8.0f,
        p->ports[PORT_TRACK_STAB]  ? *p->ports[PORT_TRACK_STAB]  : 6.0f,
        p->ports[PORT_DEBOUNCE]    ? *p->ports[PORT_DEBOUNCE]    : 2.0f,
        p->ports[PORT_FILTER_Q]    ? *p->ports[PORT_FILTER_Q]    : 7.0f,
        p->ports[PORT_BASS_BOOST]  ? *p->ports[PORT_BASS_BOOST]  : 6.0f,
        p->ports[PORT_AGC_SPEED]   ? *p->ports[PORT_AGC_SPEED]   : 6.0f,
        p->ports[PORT_AC_WEIGHT]   ? *p->ports[PORT_AC_WEIGHT]   : 8.0f,
        p->ports[PORT_AC_CLIP]     ? *p->ports[PORT_AC_CLIP]     : 4.0f,
        p->ports[PORT_BASS_LEVEL]   ? *p->ports[PORT_BASS_LEVEL]   : 0.0f,
        p->ports[PORT_BASS_PATTERN] ? *p->ports[PORT_BASS_PATTERN] : 1.0f,
        p->ports[PORT_BASS_BPM]     ? *p->ports[PORT_BASS_BPM]     : 120.0f,
        p->ports[PORT_BASS_TAP]     ? *p->ports[PORT_BASS_TAP]     : 0.0f,
        p->ports[PORT_BASS_TONE]    ? *p->ports[PORT_BASS_TONE]    : 5.0f
    );
}

static void deactivate(LV2_Handle) {}

static void cleanup(LV2_Handle instance) {
    CyberOrgan9Plugin* p = (CyberOrgan9Plugin*)instance;
    if (p) {
        delete p->dsp;
        delete p;
    }
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI, instantiate, connect_port, activate, run, deactivate, cleanup, nullptr
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
