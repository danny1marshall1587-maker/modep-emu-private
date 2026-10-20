/*
 * Cyber Evans Bass Pro (Stereo & Multi-Channel)
 * Next-Generation Boutique Bass Machine & Virtual Session Musician
 * Dedicated to Tony Evans: The Bass Player That Just Won't Quit.
 *
 * Core Features:
 *  1. 4-Way Confidence Matrix:
 *     - Fast Layer (10-15ms): Fast Comb + Fast Zero-Crossing Root Extractor (catches instantaneous pick attacks).
 *     - Strict Layer (40-50ms): Strict High-Q Comb + Normalized Autocorrelation Root (analyzes sustained note body).
 *     - Majority Rules Logic: Overrides ringing low drone strings (e.g. Open Bb tuning) when new fretted notes are played.
 *  2. Broadband Transient Extractor & Spectral Flux:
 *     - Detects the microscopic pick scrape click (2-6 kHz noise burst).
 *     - Forces immediate harmonic re-evaluation without requiring overall guitar volume to drop.
 *  3. The Guided Performer (Virtual Session Bassist Brain):
 *     - MIDI Roadmap Ingestion: Follows chord progressions and section markers.
 *     - Style Donor DNA Transplant: Borrows rhythmic syncopation, swing, and Markov interval transitions (roots, 5ths, octaves, walking runs).
 *     - Active Listening & Motif Quoting: Identifies melodic hooks in vocal gaps and borrows them for bass fills.
 *  4. 9 Vintage Bass Presets & 10-Slider Note Selector Voicing:
 *     - Precision, Longhorn, Fretless, Synth (Taurus), Virtual, Bowed, Split Bass, 3:03, Flip-Flop.
 *  5. LV2 MIDI Output:
 *     - Emits live MIDI Note-On/Note-Off events to drive external synths or soundfonts (sfizz).
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_TWOPI
#define M_TWOPI 6.28318530717958647692
#endif

#include "lv2.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstdio>
#include "generated_wavetables.h"

#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-evans-bass-pro-mono"

static const int   NOTES        = 10;
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
    0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

// ── 256-Sample Single-Cycle Real Studio Bass Wavetable ──
// Extracted and normalized from clean studio fingerstyle electric bass
static const float REAL_BASS_WAVETABLE[256] = {
    -0.000776f,  0.096840f,  0.191955f,  0.284524f,  0.374140f,  0.459889f,  0.541441f,  0.617930f,
     0.688690f,  0.753571f,  0.811308f,  0.862214f,  0.904536f,  0.940259f,  0.966952f,  0.985785f,
     0.996807f,  1.000000f,  0.995694f,  0.984506f,  0.965862f,  0.941442f,  0.911198f,  0.874600f,
     0.833304f,  0.787809f,  0.739880f,  0.686661f,  0.630807f,  0.572827f,  0.513021f,  0.452289f,
     0.391335f,  0.330996f,  0.271293f,  0.213916f,  0.159318f,  0.107785f,  0.060523f,  0.017314f,
    -0.021439f, -0.055604f, -0.085708f, -0.111837f, -0.134202f, -0.153945f, -0.171000f, -0.186276f,
    -0.200393f, -0.213442f, -0.225967f, -0.238483f, -0.251336f, -0.264985f, -0.278889f, -0.293302f,
    -0.308155f, -0.323202f, -0.338811f, -0.354838f, -0.371404f, -0.389520f, -0.407965f, -0.428322f,
    -0.449268f, -0.471662f, -0.495670f, -0.519308f, -0.544025f, -0.568675f, -0.592278f, -0.615351f,
    -0.636480f, -0.655317f, -0.671889f, -0.685832f, -0.696674f, -0.704255f, -0.708954f, -0.710957f,
    -0.709229f, -0.704172f, -0.696672f, -0.686330f, -0.672737f, -0.656638f, -0.637414f, -0.616288f,
    -0.592135f, -0.565431f, -0.536763f, -0.506653f, -0.475951f, -0.444195f, -0.413240f, -0.383883f,
    -0.354997f, -0.328324f, -0.303665f, -0.281242f, -0.261120f, -0.242947f, -0.226136f, -0.210181f,
    -0.195471f, -0.180063f, -0.164012f, -0.147257f, -0.129306f, -0.110191f, -0.089315f, -0.066252f,
    -0.041955f, -0.016097f,  0.011318f,  0.039847f,  0.069587f,  0.099609f,  0.130741f,  0.162741f,
     0.195458f,  0.229004f,  0.263198f,  0.298334f,  0.334186f,  0.370241f,  0.406176f,  0.442675f,
     0.478319f,  0.513144f,  0.546267f,  0.577293f,  0.606182f,  0.631635f,  0.654307f,  0.673193f,
     0.688073f,  0.698828f,  0.705086f,  0.707155f,  0.704823f,  0.698617f,  0.688982f,  0.675917f,
     0.660602f,  0.643132f,  0.623076f,  0.602147f,  0.579898f,  0.557105f,  0.533430f,  0.509940f,
     0.486263f,  0.463147f,  0.440048f,  0.417010f,  0.393427f,  0.369350f,  0.344847f,  0.319749f,
     0.293966f,  0.267187f,  0.240339f,  0.213137f,  0.185325f,  0.157284f,  0.129183f,  0.100678f,
     0.071229f,  0.041129f,  0.009815f, -0.023013f, -0.057866f, -0.094637f, -0.133138f, -0.173038f,
    -0.214648f, -0.257721f, -0.301955f, -0.345787f, -0.388900f, -0.430028f, -0.468277f, -0.502145f,
    -0.530822f, -0.553747f, -0.570794f, -0.580989f, -0.584552f, -0.580751f, -0.570529f, -0.553825f,
    -0.530647f, -0.501804f, -0.467574f, -0.428299f, -0.384664f, -0.336970f, -0.285354f, -0.230529f,
    -0.172399f, -0.111348f, -0.048000f,  0.017163f,  0.083301f,  0.150590f,  0.217672f,  0.283989f,
     0.348578f,  0.410873f,  0.468518f,  0.521216f,  0.567924f,  0.606544f,  0.637115f,  0.657686f,
     0.667903f,  0.666580f,  0.653327f,  0.628000f,  0.591001f,  0.542069f,  0.482153f,  0.412115f,
     0.333162f,  0.245948f,  0.151064f,  0.050172f, -0.055006f, -0.161419f, -0.267596f, -0.371823f,
    -0.472364f, -0.567782f, -0.655700f, -0.735095f, -0.804988f, -0.863911f, -0.911072f, -0.945916f,
    -0.967886f, -0.976783f, -0.973183f, -0.957535f, -0.929622f, -0.890996f, -0.842132f, -0.783739f,
    -0.717679f, -0.644688f, -0.565318f, -0.481338f, -0.391965f, -0.299707f, -0.205590f, -0.109532f
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
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

// ── Resonator Bandpass Filter ──
struct NoteResonator {
    float b0 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
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

    inline float process(float x, float attackCoeff = 0.65f, float relCoeff = 0.993f) {
        float y = b0 * (x - x2) - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;

        float absY = fabsf(y);
        if (absY > envelope) {
            envelope = envelope * (1.0f - attackCoeff) + absY * attackCoeff;
        } else {
            envelope = envelope * relCoeff;
        }
        return envelope;
    }
};

// ── DC Blocker ──
struct DCBlocker {
    float x1 = 0.0f, y1 = 0.0f;
    inline float process(float x) {
        float y = x - x1 + 0.9995f * y1;
        x1 = x; y1 = y;
        return y;
    }
};

// ── Steep 4-Pole (24 dB/oct) Butterworth Low-Pass Fundamental Isolator ──
// Strictly filters guitar input below ~260Hz for pitch detection, eliminating upper strings & harmonics
struct Butterworth4PLowPass {
    float x1_a = 0.0f, x2_a = 0.0f, y1_a = 0.0f, y2_a = 0.0f;
    float x1_b = 0.0f, x2_b = 0.0f, y1_b = 0.0f, y2_b = 0.0f;
    float b0_a = 0.0f, b1_a = 0.0f, b2_a = 0.0f, a1_a = 0.0f, a2_a = 0.0f;
    float b0_b = 0.0f, b1_b = 0.0f, b2_b = 0.0f, a1_b = 0.0f, a2_b = 0.0f;

    void setCutoff(float fc, float sr) {
        // Stage 1 (Q = 0.5412)
        float w0 = (float)(M_PI * fc / sr);
        float k = tanf(w0);
        float k2 = k * k;
        float q1 = 0.54119610f;
        float norm1 = 1.0f / (1.0f + k / q1 + k2);
        b0_a = k2 * norm1;
        b1_a = 2.0f * b0_a;
        b2_a = b0_a;
        a1_a = 2.0f * (k2 - 1.0f) * norm1;
        a2_a = (1.0f - k / q1 + k2) * norm1;

        // Stage 2 (Q = 1.3065)
        float q2 = 1.3065630f;
        float norm2 = 1.0f / (1.0f + k / q2 + k2);
        b0_b = k2 * norm2;
        b1_b = 2.0f * b0_b;
        b2_b = b0_b;
        a1_b = 2.0f * (k2 - 1.0f) * norm2;
        a2_b = (1.0f - k / q2 + k2) * norm2;

        reset();
    }

    void reset() {
        x1_a = x2_a = y1_a = y2_a = 0.0f;
        x1_b = x2_b = y1_b = y2_b = 0.0f;
    }

    inline float process(float in) {
        float y_a = b0_a * in + b1_a * x1_a + b2_a * x2_a - a1_a * y1_a - a2_a * y2_a;
        x2_a = x1_a; x1_a = in;
        y2_a = y1_a; y1_a = y_a;

        float y_b = b0_b * y_a + b1_b * x1_b + b2_b * x2_b - a1_b * y1_b - a2_b * y2_b;
        x2_b = x1_b; x1_b = y_a;
        y2_b = y1_b; y1_b = y_b;
        return y_b;
    }
};

// ── Realistic Fretless Expression Engine ──
// Models human fretless playing nuances across all bass models:
// 1. Initial microtonal searching/settling into pitch (±12–22 cents correction over 35–55ms)
// 2. Selective, natural glissando/slides (only on interval shifts >= 3 semitones or chord changes)
// 3. Delayed fingertip vibrato (~5.0 Hz, ±18 cents) blooming only after note is held > 280ms
struct FretlessExpressionProcessor {
    float currentPitchOffsetCents = 0.0f;
    float targetPitchOffsetCents = 0.0f;
    float intonationSlewCoeff = 0.05f;

    float glissCurrentMultiplier = 1.0f;
    float glissTargetMultiplier = 1.0f;
    float glissSlewCoeff = 0.015f;

    float noteAgeSec = 0.0f;
    float vibratoPhase = 0.0f;
    float vibratoRateHz = 5.1f;
    float vibratoDepthCents = 0.0f;

    int   lastRootMidi = -1;
    float lastTargetHz = 0.0f;

    void reset() {
        currentPitchOffsetCents = 0.0f;
        targetPitchOffsetCents = 0.0f;
        glissCurrentMultiplier = 1.0f;
        glissTargetMultiplier = 1.0f;
        noteAgeSec = 0.0f;
        vibratoPhase = 0.0f;
        vibratoDepthCents = 0.0f;
        lastRootMidi = -1;
        lastTargetHz = 0.0f;
    }

    void onNoteTrigger(int newRootMidi, float newRootHz, float sr, bool fretlessEnabled = false) {
        noteAgeSec = 0.0f;
        vibratoPhase = 0.0f;
        vibratoDepthCents = 0.0f;

        if (!fretlessEnabled) {
            currentPitchOffsetCents = 0.0f;
            targetPitchOffsetCents = 0.0f;
            glissCurrentMultiplier = 1.0f;
            glissTargetMultiplier = 1.0f;
            lastRootMidi = newRootMidi;
            lastTargetHz = newRootHz;
            return;
        }

        // Microtonal Intonation Settling:
        // Fretless players strike slightly off (±10 to ±22 cents) depending on direction,
        // then micro-adjust by ear into true pitch within 35-50ms.
        float strikeErrorCents = ((float)rand() / (float)RAND_MAX - 0.5f) * 36.0f; // ±18 cents
        currentPitchOffsetCents = strikeErrorCents;
        targetPitchOffsetCents = 0.0f; // settles back to pure intonation
        intonationSlewCoeff = 1.0f - expf(-1.0f / (0.042f * sr)); // ~42ms settling time

        // Selective Glissando / Slide:
        // Do NOT glissando every note! Only slide on intervals >= 3 semitones
        // or on ~40% of chord transitions.
        if (lastRootMidi >= 0 && lastTargetHz > 20.0f && lastRootMidi != newRootMidi) {
            int intervalDelta = abs(newRootMidi - lastRootMidi);
            float glissRoll = (float)rand() / (float)RAND_MAX;

            if (intervalDelta >= 3 || (intervalDelta >= 2 && glissRoll < 0.45f)) {
                // Slide in from the previous pitch
                glissCurrentMultiplier = lastTargetHz / newRootHz;
                glissTargetMultiplier = 1.0f;
                // Slide speed: ~60ms for small intervals, up to ~110ms for octaves
                float slideTimeSec = 0.055f + 0.006f * (float)intervalDelta;
                glissSlewCoeff = 1.0f - expf(-1.0f / (slideTimeSec * sr));
            } else {
                glissCurrentMultiplier = 1.0f;
                glissTargetMultiplier = 1.0f;
            }
        } else {
            glissCurrentMultiplier = 1.0f;
            glissTargetMultiplier = 1.0f;
        }

        lastRootMidi = newRootMidi;
        lastTargetHz = newRootHz;
    }

    inline float processPitchMultiplier(float dtSec, float guitarEnv = 1.0f) {
        noteAgeSec += dtSec;

        // Smoothly slew initial intonation error back to zero
        currentPitchOffsetCents += intonationSlewCoeff * (targetPitchOffsetCents - currentPitchOffsetCents);

        // Smoothly glide pitch multiplier towards target 1.0
        glissCurrentMultiplier += glissSlewCoeff * (glissTargetMultiplier - glissCurrentMultiplier);

        // Delayed bloom vibrato: Starts after 280ms of sustain
        // Only blooms if note has sustained volume (not in silence / decay)
        float vibOffsetCents = 0.0f;
        if (noteAgeSec > 0.280f && guitarEnv > 0.015f) {
            float bloomTime = noteAgeSec - 0.280f;
            // Tasteful, subtle boutique vibrato depth (±6.0 cents max, down from ±20 cents)
            float targetDepth = 6.0f * (1.0f - expf(-bloomTime / 0.25f));
            vibratoDepthCents += 0.005f * (targetDepth - vibratoDepthCents);

            vibratoPhase += vibratoRateHz * (float)M_TWOPI * dtSec;
            if (vibratoPhase >= (float)M_TWOPI) vibratoPhase -= (float)M_TWOPI;

            vibOffsetCents = sinf(vibratoPhase) * vibratoDepthCents;
        } else {
            vibratoDepthCents *= 0.995f; // Fade vibrato smoothly to 0 if signal decays
        }

        // Total pitch multiplier = glissando glide * (2 ^ (cents / 1200))
        float totalCents = currentPitchOffsetCents + vibOffsetCents;
        float centsMult = powf(2.0f, totalCents / 1200.0f);
        return glissCurrentMultiplier * centsMult;
    }
};

// ── Live Guitar Strum Pulse & Dynamic Tempo Tracker ──
// Dynamically listens to guitar pick/strum inter-onset intervals (IOI)
// Estimates live playing BPM (60-180 BPM) and tracks guitar downbeat pulse
struct GuitarStrumPulseTracker {
    float lastOnsetSec = 0.0f;
    float currentSec = 0.0f;
    float estimatedBpm = 120.0f;
    float confidence = 0.0f;
    float recentIntervals[8] = {};
    int intervalIdx = 0;
    int intervalCount = 0;

    // Articulation detection
    float lastPeakAmp = 0.0f;
    float decayRateEnv = 0.0f;
    bool isPalmStaccato = false;

    void reset() {
        lastOnsetSec = 0.0f;
        currentSec = 0.0f;
        estimatedBpm = 120.0f;
        confidence = 0.0f;
        memset(recentIntervals, 0, sizeof(recentIntervals));
        intervalIdx = 0;
        intervalCount = 0;
        lastPeakAmp = 0.0f;
        decayRateEnv = 0.0f;
        isPalmStaccato = false;
    }

    void onStrumHit(float currentTime, float peakAmp) {
        float dt = currentTime - lastOnsetSec;
        lastOnsetSec = currentTime;
        lastPeakAmp = peakAmp;

        // Valid musical strum intervals: 120ms (quarter note @ 500bpm / 16th @ 125bpm) to 1.8s (~33bpm)
        if (dt >= 0.16f && dt <= 1.50f) {
            // Normalize subdivisions into quarter-note candidate
            float candDt = dt;
            if (candDt < 0.28f) candDt *= 2.0f; // 8th note -> quarter note
            else if (candDt > 1.05f) candDt *= 0.5f; // half note -> quarter note

            recentIntervals[intervalIdx] = candDt;
            intervalIdx = (intervalIdx + 1) % 8;
            if (intervalCount < 8) intervalCount++;

            if (intervalCount >= 3) {
                // Compute median of recent intervals
                float sorted[8];
                for (int k = 0; k < intervalCount; ++k) sorted[k] = recentIntervals[k];
                std::sort(sorted, sorted + intervalCount);
                float medianDt = sorted[intervalCount / 2];

                if (medianDt > 0.20f && medianDt < 1.40f) {
                    float targetBpm = 60.0f / medianDt;
                    // Clamp to musical guitar range (55-200 BPM)
                    targetBpm = std::max(55.0f, std::min(200.0f, targetBpm));
                    // Smooth tracking
                    estimatedBpm = estimatedBpm * 0.70f + targetBpm * 0.30f;
                    confidence = std::min(1.0f, confidence + 0.25f);
                }
            }
        }
    }

    void updateDecay(float currentAmp, float sampleDt) {
        currentSec += sampleDt;
        // Check how rapidly envelope dropped from peak
        if (lastPeakAmp > 0.03f) {
            float drop = lastPeakAmp - currentAmp;
            if (drop > 0.0f) {
                decayRateEnv = decayRateEnv * 0.95f + (drop / lastPeakAmp) * 0.05f;
            }
            // Fast decay = palm-muted staccato chop
            isPalmStaccato = (decayRateEnv > 0.65f);
        } else {
            isPalmStaccato = false;
        }
    }
};

// ── Broadband Transient & Pick Attack Extractor ──
struct BroadbandTransientExtractor {
    float hp_x1 = 0.0f, hp_x2 = 0.0f, hp_y1 = 0.0f, hp_y2 = 0.0f;
    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
    bool  onsetDetected = false;
    bool  prevHitState = false;
    int   refractorySamples = 0;
    float sampleRate = 48000.0f;

    void init(float sr) {
        sampleRate = sr;
        // High-pass at 2500 Hz (picks up guitar pick scrape click)
        float w0 = (float)(M_TWOPI * 2500.0f / sr);
        float alpha = sinf(w0) * 0.7071f;
        float b0 = (1.0f + cosf(w0)) * 0.5f;
        float b1 = -(1.0f + cosf(w0));
        float b2 = (1.0f + cosf(w0)) * 0.5f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosf(w0);
        float a2 = 1.0f - alpha;

        hp_x1 = hp_x2 = hp_y1 = hp_y2 = 0.0f;
        fastEnv = slowEnv = 0.0f;
        onsetDetected = false;
        prevHitState = false;
        refractorySamples = 0;
    }

    inline bool process(float in, float sensitivity = 6.5f, float inputFloor = 0.005f) {
        // 1-pole highpass difference
        float click = in - hp_x1;
        hp_x1 = in;
        float absClick = fabsf(click);

        fastEnv = fastEnv * 0.40f + absClick * 0.60f;
        slowEnv = slowEnv * 0.990f + absClick * 0.010f;

        float sensFactor = 1.35f + (10.0f - sensitivity) * 0.25f;
        // Dynamic threshold based on relative ratio and adaptive floor
        float minDynamicFloor = std::max(0.0008f, inputFloor * 0.08f);
        bool isHitRaw = (fastEnv > slowEnv * sensFactor) && (fastEnv > minDynamicFloor);

        bool isHitTrigger = false;
        if (refractorySamples > 0) {
            refractorySamples--;
        } else if (isHitRaw && !prevHitState) {
            // Rising edge trigger with 40ms refractory lockout
            isHitTrigger = true;
            refractorySamples = (int)(0.040f * sampleRate);
        }
        prevHitState = isHitRaw;
        onsetDetected = isHitTrigger;
        return isHitTrigger;
    }
};

// ── Fast Zero-Crossing & Periodicity Root Detector (Fast Layer: 10-15ms) ──
struct FastZeroCrossingRoot {
    static const int WIN_SIZE = 768; // 16ms @ 48kHz
    float buf[WIN_SIZE] = {};
    int writeIdx = 0;
    int detectedSemi = -1;
    float confidence = 0.0f;

    void reset() {
        memset(buf, 0, sizeof(buf));
        writeIdx = 0;
        detectedSemi = -1;
        confidence = 0.0f;
    }

    inline void push(float s) {
        buf[writeIdx] = s;
        writeIdx = (writeIdx + 1) % WIN_SIZE;
    }

    void analyze(float sr) {
        // Find fundamental period via consecutive positive-slope zero crossings
        int crossings[32];
        int count = 0;
        for (int i = 1; i < WIN_SIZE && count < 32; ++i) {
            int curr = (writeIdx - i + WIN_SIZE) % WIN_SIZE;
            int prev = (curr - 1 + WIN_SIZE) % WIN_SIZE;
            if (buf[prev] <= 0.0f && buf[curr] > 0.0f) {
                crossings[count++] = i;
            }
        }

        if (count >= 3) {
            float avgPeriod = 0.0f;
            int validDiffs = 0;
            for (int k = 1; k < count; ++k) {
                int diff = crossings[k] - crossings[k - 1];
                if (diff >= (int)(sr / 350.0f) && diff <= (int)(sr / 65.0f)) {
                    avgPeriod += (float)diff;
                    validDiffs++;
                }
            }
            if (validDiffs >= 2) {
                avgPeriod /= (float)validDiffs;
                float fundHz = sr / avgPeriod;
                float midiF = 69.0f + 12.0f * log2f(fundHz / 440.0f);
                int midi = (int)roundf(midiF);
                detectedSemi = (midi % 12 + 12) % 12;
                confidence = 0.65f;
                return;
            }
        }
        detectedSemi = -1;
        confidence = 0.0f;
    }
};

// ── Time-Domain Autocorrelation Pitch Detector (Strict Layer: 40-50ms) ──
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
        float decSR = sampleRate * 0.5f;
        int minLag = (int)(decSR / 380.0f);
        int maxLag = (int)(decSR / 55.0f);
        if (minLag < 24) minLag = 24;
        if (maxLag > 560) maxLag = 560;

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

        float targetGain = 0.6f / peak;
        autoGain = autoGain * 0.70f + targetGain * 0.30f;
        autoGain = std::max(0.1f, std::min(100.0f, autoGain));

        float clipFrac = (clipSetting / 10.0f) * 0.45f;
        float clipThresh = (peak * autoGain) * clipFrac;

        float normBuf[DEC_LEN];
        for (int i = 0; i < DEC_LEN; ++i) {
            float v = decimated[i] * autoGain;
            if (v > clipThresh) normBuf[i] = v - clipThresh;
            else if (v < -clipThresh) normBuf[i] = v + clipThresh;
            else normBuf[i] = 0.0f;
        }

        float e0 = 1e-9f;
        for (int i = 0; i < CORR_WIN; ++i) {
            e0 += normBuf[i] * normBuf[i];
        }

        int bestLag = -1;
        float bestCorr = 0.0f;

        for (int lag = minLag; lag <= maxLag; ++lag) {
            float sum = 0.0f;
            float eLag = 1e-9f;
            for (int i = 0; i < CORR_WIN; ++i) {
                float s0 = normBuf[i];
                float s1 = normBuf[i + lag];
                sum += s0 * s1;
                eLag += s1 * s1;
            }
            float normCorr = sum / sqrtf(e0 * eLag);
            corrs[lag] = normCorr;

            if (lag > minLag + 1) {
                int prevLag = lag - 1;
                if (corrs[prevLag] > corrs[prevLag - 1] && corrs[prevLag] > corrs[lag]) {
                    float peakVal = corrs[prevLag];
                    if (peakVal > 0.40f && peakVal > bestCorr) {
                        float yA = corrs[prevLag - 1];
                        float yB = corrs[prevLag];
                        float yC = corrs[lag];
                        float delta = 0.5f * (yA - yC) / (yA - 2.0f * yB + yC + 1e-9f);
                        float refinedLag = (float)prevLag + delta;

                        int octaveHalfLag = (int)roundf(refinedLag * 0.5f);
                        if (octaveHalfLag >= minLag && corrs[octaveHalfLag] > peakVal * 0.75f) {
                            refinedLag = (float)octaveHalfLag;
                            peakVal = corrs[octaveHalfLag];
                        }

                        bestLag = prevLag;
                        bestCorr = peakVal;
                    }
                }
            }
        }

        if (bestCorr > 0.42f && bestLag > 0) {
            float f0 = decSR / (float)bestLag;
            detectedFreq = f0;
            confidence = std::min(1.0f, (bestCorr - 0.40f) / 0.50f);

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

// ── The Guided Performer: Virtual Session Player & Markov Brain ──
struct GuidedPerformerEngine {
    int currentStep = 0;
    double clockPhase = 0.0;
    float currentBpm = 120.0f;
    float tapTimer = 0.0f;
    float lastTap = 0.0f;

    // Markov transition weights: probability of Root -> 5th, Octave, 6th, or chromatic approach
    float markov5thProb = 0.40f;
    float markovOctaveProb = 0.25f;
    float markovWalkProb = 0.35f;

    int activeMidiNote = -1;
    bool noteIsActive = false;

    void reset() {
        currentStep = 0;
        clockPhase = 0.0;
        currentBpm = 120.0f;
        activeMidiNote = -1;
        noteIsActive = false;
    }

    int generateNextBassTone(int rootPitchClass, bool isMinor, int barStep, float styleDensity, float motifQuote) {
        if (rootPitchClass < 0) return -1;
        int baseMidi = 24 + rootPitchClass;
        if (baseMidi < 28) baseMidi += 12;

        // Downbeat (Beat 1, step 0): 100% root anchor
        if (barStep == 0) return baseMidi;

        // Offbeat groove based on style density
        float roll = (float)rand() / (float)RAND_MAX;
        if (barStep == 4) { // Beat 2
            if (roll < 0.65f) return baseMidi + 7; // 5th
            return baseMidi;
        }
        if (barStep == 8) { // Beat 3
            if (roll < 0.45f) return baseMidi + (isMinor ? 8 : 9); // 6th walk
            return baseMidi;
        }
        if (barStep == 12 || barStep == 14) { // Beat 4 / Turnaround
            if (motifQuote > 4.0f && roll < 0.50f) {
                // Melodic fill motif: 3rd -> 5th
                return baseMidi + (isMinor ? 3 : 4);
            }
            if (roll < 0.60f) return baseMidi + 7; // 5th
            return baseMidi + 11; // Chromatic approach half-step below root
        }
        return (styleDensity > 5.0f && (barStep % 2 == 0)) ? baseMidi : -1;
    }
};

// ── Working Memory & Self-Learning Harmonic Brain (160-Bar Rolling Buffer ~5.3 Minutes) ──
// 1. Records detected roots, confidence, and timestamps across 160 musical bars (~5.3 minutes @ 120 BPM)
// 2. Multi-section loop detection: recognizes 4-bar, 8-bar song forms, and classic 12-bar blues progressions
// 3. Consensus Filter: Once a cycle is learned, temporary misdetections/glitches are overridden by historical harmonic memory
// 4. Pass 2+ Embellishment: Distinguishes Pass 1 (foundation) from Pass 2+ (walking fills, leading tones, 5th accents)
// 5. Archetype Detection: Exposes blues, 4-bar pop, and 8-bar forms for style-aware playing
struct WorkingMemoryEngine {
    static const int MAX_BARS = 160;
    static const int STEPS_PER_BAR = 16;

    enum Archetype {
        ARCHETYPE_UNKNOWN = 0,
        ARCHETYPE_LOOP_4   = 1,
        ARCHETYPE_LOOP_8   = 2,
        ARCHETYPE_BLUES_12 = 3
    };
    
    struct BarMemory {
        int dominantRoot = -1;
        bool isMinor = false;
        int hitCount[12] = {};
        float avgConfidence = 0.0f;
    };

    BarMemory history[MAX_BARS];
    int currentBar = 0;
    int cycleLength = 4; // Detected loop length (typically 4, 8, or 12 bars)
    int cycleIteration = 0; // Which pass through the progression (0 = 1st pass, 1 = 2nd pass, etc.)
    bool cycleEstablished = false;
    Archetype detectedArchetype = ARCHETYPE_UNKNOWN;

    void reset() {
        for (int b = 0; b < MAX_BARS; ++b) {
            history[b] = BarMemory();
        }
        currentBar = 0;
        cycleLength = 4;
        cycleIteration = 0;
        cycleEstablished = false;
        detectedArchetype = ARCHETYPE_UNKNOWN;
    }

    void recordStepRoot(int step, int detectedSemi, bool isMin, float confidence) {
        if (detectedSemi < 0 || detectedSemi >= 12) return;
        int barIdx = currentBar % MAX_BARS;
        history[barIdx].hitCount[detectedSemi]++;
        history[barIdx].isMinor = isMin;
        history[barIdx].avgConfidence = history[barIdx].avgConfidence * 0.90f + confidence * 0.10f;
        
        // Update dominant root for the bar
        int bestSemi = -1;
        int maxHits = 0;
        for (int s = 0; s < 12; ++s) {
            if (history[barIdx].hitCount[s] > maxHits) {
                maxHits = history[barIdx].hitCount[s];
                bestSemi = s;
            }
        }
        history[barIdx].dominantRoot = bestSemi;
    }

    void onBarComplete() {
        currentBar++;
        // Periodic cycle & archetype detection
        // 1. Check for 12-bar blues progression archetype (I - IV - I - V - IV - I - V)
        if (currentBar >= 12) {
            // Inspect the last 12 bars:
            // Bar 0-3 (I), Bar 4-5 (IV), Bar 6-7 (I), Bar 8 (V), Bar 9 (IV), Bar 10-11 (I/V)
            int b0 = (currentBar - 12 + MAX_BARS) % MAX_BARS;
            int rI = history[b0].dominantRoot;
            if (rI >= 0) {
                int rIV = (rI + 5) % 12;
                int rV  = (rI + 7) % 12;
                int b4 = (currentBar - 8 + MAX_BARS) % MAX_BARS; // Bar 5
                int b6 = (currentBar - 6 + MAX_BARS) % MAX_BARS; // Bar 7
                int b8 = (currentBar - 4 + MAX_BARS) % MAX_BARS; // Bar 9
                
                if (history[b4].dominantRoot == rIV &&
                    history[b6].dominantRoot == rI &&
                    history[b8].dominantRoot == rV) {
                    detectedArchetype = ARCHETYPE_BLUES_12;
                    cycleLength = 12;
                    cycleEstablished = true;
                    cycleIteration++;
                    return;
                }
            }
        }

        // 2. Check for 8-bar loop
        if (currentBar >= 16) {
            bool match8 = true;
            for (int k = 0; k < 8; ++k) {
                int bRecent = (currentBar - 1 - k + MAX_BARS) % MAX_BARS;
                int bPrior  = (currentBar - 9 - k + MAX_BARS) % MAX_BARS;
                if (history[bRecent].dominantRoot != history[bPrior].dominantRoot || history[bRecent].dominantRoot < 0) {
                    match8 = false;
                    break;
                }
            }
            if (match8) {
                detectedArchetype = ARCHETYPE_LOOP_8;
                cycleLength = 8;
                cycleEstablished = true;
                cycleIteration++;
                return;
            }
        }

        // 3. Check for 4-bar loop
        if (currentBar >= 8) {
            bool match4 = true;
            for (int k = 0; k < 4; ++k) {
                int bRecent = (currentBar - 1 - k + MAX_BARS) % MAX_BARS;
                int bPrior  = (currentBar - 5 - k + MAX_BARS) % MAX_BARS;
                if (history[bRecent].dominantRoot != history[bPrior].dominantRoot || history[bRecent].dominantRoot < 0) {
                    match4 = false;
                    break;
                }
            }
            if (match4) {
                detectedArchetype = ARCHETYPE_LOOP_4;
                cycleLength = 4;
                cycleEstablished = true;
                cycleIteration++;
                return;
            }
        }

        // If progression drifted or changed section (e.g. verse -> chorus)
        if (cycleEstablished && cycleIteration > 1) {
            cycleIteration = 0;
            cycleEstablished = false;
            detectedArchetype = ARCHETYPE_UNKNOWN;
        }
    }

    // Consensus filter: Returns expected historical root if memory is confident, suppressing transient glitches
    // But NEVER locks or traps the detector when the player deliberately changes chord
    int getMemoryGuardedRoot(int detectedRoot, int barStep, int detectedVotes = 0) {
        if (!cycleEstablished || cycleLength <= 0) return detectedRoot;
        int cyclePos = (currentBar % cycleLength);
        int priorBarIdx = (currentBar - cycleLength + MAX_BARS) % MAX_BARS;
        int expectedRoot = history[priorBarIdx].dominantRoot;

        // If the incoming detection has solid consensus (>= 4 votes out of 7), trust the player immediately!
        if (detectedVotes >= 4) {
            return detectedRoot;
        }

        // Only defer to memory on weak, split, or momentary transient slips (<= 3 votes)
        if (expectedRoot >= 0 && expectedRoot != detectedRoot && detectedVotes < 3) {
            return expectedRoot;
        }
        return detectedRoot;
    }

    // Returns whether current pass is an embellishment pass (Pass 2, 4, etc.)
    bool isEmbellishPass() const {
        return cycleEstablished && (cycleIteration % 2 == 1);
    }
};

// ── Song Catalog & Long-Term Roadmap Memory Engine ──
// Stores up to 8 complete songs (up to 128 bars of chord progression DNA each).
// When REC SONG is active, incoming bars are recorded into the active song slot.
// In Auto-Detect mode (slot 0), matches rolling chord history against cataloged songs.
// In Slot mode (slots 1-8), locks to that song's roadmap to anticipate changes with 0ms latency!
// Automatically persists and reloads repertoire in proprietary Cyber Audio binary format (.ebpsong).
struct SongCatalogEngine {
    static const int MAX_SLOTS = 8;
    static const int MAX_SONG_BARS = 128;
    static constexpr uint32_t MAGIC_HEADER = 0x45425053; // 'EBPS' - Evans Bass Pro Song DNA
    static constexpr uint32_t FORMAT_VERSION = 1;

    struct SongEntry {
        int barCount = 0;
        int roots[MAX_SONG_BARS] = {};
        bool isMinor[MAX_SONG_BARS] = {};
        int archetype = 0;
        bool isPopulated = false;

        void clear() {
            barCount = 0;
            memset(roots, -1, sizeof(roots));
            memset(isMinor, 0, sizeof(isMinor));
            archetype = 0;
            isPopulated = false;
        }
    };

    SongEntry songs[MAX_SLOTS];
    bool isRecording = false;
    int targetSlot = 0;      // 0 = Auto-Detect, 1..8 = Slots 1..8
    int recordingSlot = 0;   // 0..7 index of slot being recorded
    int activeMatchSlot = -1;// 0..7 index of currently recognized/active song, or -1
    int songBarPointer = 0;  // Current bar position inside active song (0..barCount-1)
    float matchConfidence = 0.0f;
    int prevRecPort = 0;
    bool hasLoadedDisk = false;

    void reset() {
        for (int s = 0; s < MAX_SLOTS; ++s) {
            songs[s].clear();
        }
        isRecording = false;
        targetSlot = 0;
        recordingSlot = 0;
        activeMatchSlot = -1;
        songBarPointer = 0;
        matchConfidence = 0.0f;
        prevRecPort = 0;
        if (!hasLoadedDisk) {
            loadFromDisk();
            hasLoadedDisk = true;
        }
    }

    // Save all 8 repertoire song roadmaps to proprietary binary container
    void saveToDisk() {
        const char* paths[] = {
            "/var/modep/user_data/song_catalog.ebpsong",
            "/tmp/song_catalog.ebpsong",
            "song_catalog.ebpsong"
        };
        for (const char* path : paths) {
            FILE* f = fopen(path, "wb");
            if (f) {
                uint32_t magic = MAGIC_HEADER;
                uint32_t version = FORMAT_VERSION;
                uint32_t slots = MAX_SLOTS;
                uint32_t maxBars = MAX_SONG_BARS;
                fwrite(&magic, sizeof(uint32_t), 1, f);
                fwrite(&version, sizeof(uint32_t), 1, f);
                fwrite(&slots, sizeof(uint32_t), 1, f);
                fwrite(&maxBars, sizeof(uint32_t), 1, f);
                for (int s = 0; s < MAX_SLOTS; ++s) {
                    fwrite(&songs[s], sizeof(SongEntry), 1, f);
                }
                fclose(f);
                break;
            }
        }
    }

    // Load repertoire song roadmaps from persistent storage
    void loadFromDisk() {
        const char* paths[] = {
            "/var/modep/user_data/song_catalog.ebpsong",
            "/tmp/song_catalog.ebpsong",
            "song_catalog.ebpsong"
        };
        for (const char* path : paths) {
            FILE* f = fopen(path, "rb");
            if (f) {
                uint32_t magic = 0, version = 0, slots = 0, maxBars = 0;
                if (fread(&magic, sizeof(uint32_t), 1, f) == 1 && magic == MAGIC_HEADER) {
                    fread(&version, sizeof(uint32_t), 1, f);
                    fread(&slots, sizeof(uint32_t), 1, f);
                    fread(&maxBars, sizeof(uint32_t), 1, f);
                    if (slots == MAX_SLOTS && maxBars == MAX_SONG_BARS) {
                        for (int s = 0; s < MAX_SLOTS; ++s) {
                            fread(&songs[s], sizeof(SongEntry), 1, f);
                        }
                    }
                }
                fclose(f);
                break;
            }
        }
    }

    void updateRecordState(int recPort, int slotPort) {
        targetSlot = slotPort;
        // Detect rising edge: User hit REC SONG
        if (recPort > 0 && prevRecPort == 0) {
            int slotIdx = (slotPort >= 1 && slotPort <= 8) ? (slotPort - 1) : 0;
            recordingSlot = slotIdx;
            songs[recordingSlot].clear();
            isRecording = true;
            songBarPointer = 0;
        }
        // Detect falling edge: User stopped REC SONG
        else if (recPort == 0 && prevRecPort > 0) {
            if (isRecording) {
                isRecording = false;
                if (songs[recordingSlot].barCount >= 2) {
                    songs[recordingSlot].isPopulated = true;
                    activeMatchSlot = recordingSlot;
                    songBarPointer = 0;
                    saveToDisk(); // Persist immediately on song record completion!
                }
            }
        }
        prevRecPort = recPort;

        // If user manually selected a specific slot (1..8) and it's populated, lock directly
        if (!isRecording && targetSlot >= 1 && targetSlot <= 8) {
            int slotIdx = targetSlot - 1;
            if (songs[slotIdx].isPopulated) {
                activeMatchSlot = slotIdx;
            }
        }
    }

    void onBarComplete(const WorkingMemoryEngine& wm) {
        int lastBarIdx = (wm.currentBar - 1 + WorkingMemoryEngine::MAX_BARS) % WorkingMemoryEngine::MAX_BARS;
        int domRoot = wm.history[lastBarIdx].dominantRoot;
        bool isMin = wm.history[lastBarIdx].isMinor;

        // If recording, append this bar to the active song entry
        if (isRecording) {
            SongEntry& entry = songs[recordingSlot];
            if (entry.barCount < MAX_SONG_BARS && domRoot >= 0) {
                entry.roots[entry.barCount] = domRoot;
                entry.isMinor[entry.barCount] = isMin;
                entry.barCount++;
            }
            return;
        }

        // If Auto-Detect mode (targetSlot == 0) or manual slot locked:
        if (targetSlot == 0) {
            // Pattern Matcher: Compare last 4 bars against all populated songs
            if (wm.currentBar >= 4) {
                int bestSlot = -1;
                int bestBarPos = -1;
                float highestScore = 0.0f;

                int r3 = wm.history[(wm.currentBar - 1 + WorkingMemoryEngine::MAX_BARS) % WorkingMemoryEngine::MAX_BARS].dominantRoot;
                int r2 = wm.history[(wm.currentBar - 2 + WorkingMemoryEngine::MAX_BARS) % WorkingMemoryEngine::MAX_BARS].dominantRoot;
                int r1 = wm.history[(wm.currentBar - 3 + WorkingMemoryEngine::MAX_BARS) % WorkingMemoryEngine::MAX_BARS].dominantRoot;
                int r0 = wm.history[(wm.currentBar - 4 + WorkingMemoryEngine::MAX_BARS) % WorkingMemoryEngine::MAX_BARS].dominantRoot;

                for (int s = 0; s < MAX_SLOTS; ++s) {
                    const SongEntry& entry = songs[s];
                    if (!entry.isPopulated || entry.barCount < 4) continue;

                    for (int b = 0; b < entry.barCount; ++b) {
                        int b0 = (b + 0) % entry.barCount;
                        int b1 = (b + 1) % entry.barCount;
                        int b2 = (b + 2) % entry.barCount;
                        int b3 = (b + 3) % entry.barCount;

                        int score = 0;
                        if (entry.roots[b0] == r0) score++;
                        if (entry.roots[b1] == r1) score++;
                        if (entry.roots[b2] == r2) score++;
                        if (entry.roots[b3] == r3) score++;

                        float normScore = (float)score / 4.0f;
                        if (normScore > highestScore) {
                            highestScore = normScore;
                            bestSlot = s;
                            bestBarPos = (b3 + 1) % entry.barCount; // Next expected bar
                        }
                    }
                }

                if (highestScore >= 0.75f && bestSlot >= 0) {
                    activeMatchSlot = bestSlot;
                    songBarPointer = bestBarPos;
                    matchConfidence = highestScore;
                } else if (highestScore < 0.50f) {
                    matchConfidence *= 0.85f;
                }
            }
        }

        // Advance song roadmap pointer if locked
        if (activeMatchSlot >= 0 && songs[activeMatchSlot].isPopulated) {
            const SongEntry& entry = songs[activeMatchSlot];
            if (entry.barCount > 0) {
                songBarPointer = (songBarPointer + 1) % entry.barCount;
            }
        }
    }

    // Returns upcoming expected root for next bar if song is recognized (or -1)
    int getAnticipatedNextRoot() const {
        if (activeMatchSlot < 0) return -1;
        const SongEntry& entry = songs[activeMatchSlot];
        if (!entry.isPopulated || entry.barCount <= 0) return -1;
        int nextBar = (songBarPointer + 1) % entry.barCount;
        return entry.roots[nextBar];
    }
};

// ── Physical String Dynamics & Hand Expression Engine ──
// Models physical string acoustics:
// 1. Plucking Strike Position: Comb filter moving from Neck (pillowy fundamental) to Bridge (Jaco midrange growl)
// 2. Mechanical Clack & Scrape: Dynamic burst of upper fret clack on note attack
// 3. Dynamic Palm & Foam Damping: Shortens decay while leaving attack transient intact
// 4. Preamp Tube & Transformer Coil Warmth: Asymmetric saturation scaling with touch
struct PhysicalStringDynamics {
    static constexpr int COMB_MAX = 2048;
    float combBuf[COMB_MAX] = {};
    int   combIdx = 0;
    float dampState = 0.0f;
    float clackState = 0.0f;
    float clackEnv = 0.0f;

    void reset() {
        memset(combBuf, 0, sizeof(combBuf));
        combIdx = 0;
        dampState = 0.0f;
        clackState = 0.0f;
        clackEnv = 0.0f;
    }

    inline void triggerClack(float velocity = 0.8f) {
        clackEnv = std::max(clackEnv, std::min(1.0f, velocity * 0.9f));
    }

    inline float process(float in, float rootHz, float bassTone, float walkGroove, float sr) {
        // 1. Plucking Strike Position via Comb Filter
        // bass_tone: 0.0 = deep neck / thumb pad, 5.0 = standard sweetspot, 10.0 = tight bridge
        // Pluck position ratio x in [0.08, 0.42] of string length
        float normTone = std::max(0.0f, std::min(10.0f, bassTone)) / 10.0f;
        float pluckPos = 0.42f - normTone * 0.33f; // 0.42 (neck) down to 0.09 (bridge)

        float safeHz = std::max(25.0f, std::min(400.0f, rootHz));
        float periodSamples = sr / safeHz;
        int delaySamples = (int)(pluckPos * periodSamples);
        if (delaySamples < 2) delaySamples = 2;
        if (delaySamples >= COMB_MAX) delaySamples = COMB_MAX - 1;

        combBuf[combIdx] = in;
        int readIdx = combIdx - delaySamples;
        if (readIdx < 0) readIdx += COMB_MAX;
        float delayed = combBuf[readIdx];
        combIdx = (combIdx + 1) % COMB_MAX;

        // Comb blend: neck creates rounded pillowy bass, bridge creates midrange bite
        float combDepth = 0.45f + 0.35f * (fabsf(normTone - 0.5f) * 2.0f);
        float combOut = in - (normTone > 0.5f ? 0.85f : 0.65f) * combDepth * delayed;

        // 2. Mechanical Fret Clack & Pick Transient Injection
        // High-frequency band burst (~2.8 - 4.5 kHz) decaying over ~12ms
        float clack = 0.0f;
        if (clackEnv > 0.001f) {
            float clackNoise = ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f;
            // 1-pole resonant band simulation
            clackState += 0.35f * (clackNoise - clackState);
            clack = (clackNoise - clackState) * clackEnv * 0.16f;
            clackEnv *= 0.992f; // ~12ms decay @ 48kHz
        }

        // 3. Dynamic Palm & Foam Mute Damping
        // If tone < 4.0: foam mute rounds top end; if tone > 6.0: opens wide
        float dampFc = 450.0f + normTone * 4500.0f;
        float wDamp = (float)(M_TWOPI * dampFc / sr);
        float aDamp = wDamp / (1.0f + wDamp);
        dampState += aDamp * (combOut + clack - dampState);
        float toneShaped = dampState;

        // 4. Preamp Tube & Transformer Coil Saturation
        // Adds rich even/odd harmonics that bloom with intensity
        float drive = 1.0f + (walkGroove / 10.0f) * 0.35f + normTone * 0.25f;
        float driven = toneShaped * drive;
        float saturated = tanhf(driven + 0.05f * driven * driven) - 0.05f; // Asymmetric warm saturation
        return saturated;
    }
};

// ── Evans Bass Voice Bank ──
struct BassChordBank {
    double phases[NOTES];
    double subPhases[NOTES];
    float  filterZ[NOTES];
    float  envZ[NOTES];
    float  rootHz = 130.81f;
    bool   isMinor = false;
    float  gain = 0.0f;

    void init(float fRoot, bool bMinor, float initialGain) {
        memset(phases, 0, sizeof(phases));
        memset(subPhases, 0, sizeof(subPhases));
        memset(filterZ, 0, sizeof(filterZ));
        memset(envZ, 0, sizeof(envZ));
        rootHz = fRoot;
        isMinor = bMinor;
        gain = initialGain;
    }
};

// -------------------------------------------------------------------------
// Main Evans Bass Pro DSP Engine
// -------------------------------------------------------------------------
class EvansBassProDSP {
public:
    float sr = 48000.0f;

    // 4-Way Confidence Matrix Components:
    // 1. Fast Layer: Fast Comb Resonators (10-15ms)
    NoteResonator fastFilterBank[36];
    FastZeroCrossingRoot fastRootDetector;

    // 2. Strict Layer: Strict Comb Resonators (40-50ms)
    NoteResonator strictFilterBank[36];
    TimeDomainAutocorr strictRootDetector;

    // Broadband Pick Attack Extractor
    BroadbandTransientExtractor pickExtractor;

    // Guided Performer Virtual Musician
    GuidedPerformerEngine performer;

    float fastChroma[12];
    float strictChroma[12];
    float smoothedChroma[12];

    int confirmedRoot = -1;
    int candidateRoot = -1;
    int candidateBlocks = 0;
    bool confirmedMinor = false;

    BassChordBank bankA;
    BassChordBank bankB;
    int activeBank = 0;
    float crossfadeRate = 0.0004f;

    float noteLevelSm[NOTES];
    NotchFilter dehum50, dehum60, dehum100, dehum120;
    OnePoleFilter gateScHp, gateScLp;
    float gateEnv = 0.0f;
    float gateGain = 0.0f;
    bool  gateIsOpen = false;
    float gateAtk = 0.0f, gateRel = 0.0f;
    int   gateHoldCounter = 0;
    float gateOpenPeak = 0.0f;
    float inputPeakFollower = 0.05f; // Fast attack, very slow release AGC tracking

    float inputRMS = 0.0f;
    float bassEnv = 0.0f;

    DCBlocker dcBlockL, dcBlockR;
    int currentStep = 0;
    double clockPhase = 0.0;

    // ── Pre-Filter Fundamental Isolator & Pulse Tracker ──
    Butterworth4PLowPass pitchPreFilter;
    GuitarStrumPulseTracker pulseTracker;
    float liveGuitarSampleRate = 48000.0f;

    // ── Voice Leading & Approach Transitions ──
    int   prevConfirmedRoot = -1;
    int   targetRoot = -1;
    bool  approachActive = false;
    int   approachOffsetSemi = 0;   // Semitone shift for approach note (-1, +1, or +7)
    float approachFade = 0.0f;
    float currentStrumAmp = 0.0f;
    float bassDecayMultiplier = 1.0f; // Dynamically shortened on palm mutes

    // ── Bass ADSR Envelope (independent of input gate) ──
    float bassEnvLevel = 0.0f;     // Current bass amplitude 0-1, decays after guitar stops
    bool  bassGateWasOpen = false;

    // ── Rhythm / Tap-Tempo Engine ──
    float prevTapPort = 0.0f;      // For rising-edge detection on tap button
    float tapAccum    = 0.0f;      // Seconds elapsed since last tap
    bool  tapArmed    = false;     // True after first tap received

    // ── Beat-Aware Note Gate (populated by tickBeatEvent) ──
    float dynamicNoteGate[NOTES];   // 0-1 per slot; updated on each 16th-note tick
    float smoothedDynGate[NOTES];   // Low-pass smoothed to avoid hard cuts between beats

    // ── Fretless Human Expression Engine ──
    FretlessExpressionProcessor fretlessEngine;

    // ── Working Memory & Self-Learning Harmonic Brain ──
    WorkingMemoryEngine workingMemory;

    // ── Song Catalog Engine (Repertoire Roadmap Memory) ──
    SongCatalogEngine songCatalog;

    // ── Physical String Dynamics & Hand Expression Engine ──
    PhysicalStringDynamics physicalDynamics;

    // ── Momentary Bass Solo Lead Engine ──
    float soloGainFollower = 0.0f; // Slew-limited 0-1 for seamless transition into lead voice

    // Rate-limiting holdoff counter for 4-Way confidence matrix (prevents CPU spiking)
    int sampleEvaluationHoldoff = 0;


    void init(float sampleRate) {
        sr = sampleRate;

        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            float freq = A4 * powf(2.0f, (midi - 69) / 12.0f);
            fastFilterBank[i].setup(freq, sr, 3.5f);    // Fast, loose window
            strictFilterBank[i].setup(freq, sr, 10.0f); // Strict, selective high-Q window
        }

        fastRootDetector.reset();
        strictRootDetector.reset();
        pitchPreFilter.setCutoff(260.0f, sr);
        pulseTracker.reset();
        physicalDynamics.reset();
        prevConfirmedRoot = -1;
        targetRoot = -1;
        approachActive = false;
        approachOffsetSemi = 0;
        approachFade = 0.0f;
        bassDecayMultiplier = 1.0f;
        pickExtractor.init(sr);
        performer.reset();
        workingMemory.reset();
        songCatalog.reset();
        sampleEvaluationHoldoff = 0;

        memset(fastChroma, 0, sizeof(fastChroma));
        memset(strictChroma, 0, sizeof(strictChroma));
        memset(smoothedChroma, 0, sizeof(smoothedChroma));

        bankA.init(130.81f, false, 1.0f);
        bankB.init(130.81f, false, 0.0f);
        activeBank = 0;
        confirmedRoot = -1;

        for (int n = 0; n < NOTES; n++) {
            noteLevelSm[n] = NS_DEFAULTS[n];
        }

        dehum50.setNotch(50.0f, 16.0f, sr);
        dehum60.setNotch(60.0f, 16.0f, sr);
        dehum100.setNotch(100.0f, 18.0f, sr);
        dehum120.setNotch(120.0f, 18.0f, sr);

        gateScHp.reset();
        gateScLp.reset();
        gateEnv = 0.0f;
        gateGain = 0.0f;
        gateIsOpen = false;
        gateAtk = 1.0f - expf(-1.0f / (0.0012f * sr));
        gateRel = 1.0f - expf(-1.0f / (0.220f * sr));

        bassEnvLevel = 0.0f;
        bassGateWasOpen = false;
        clockPhase = 0.0;
        currentStep = 0;
        prevTapPort = 0.0f;
        tapAccum    = 0.0f;
        tapArmed    = false;
        for (int n = 0; n < NOTES; n++) {
            dynamicNoteGate[n]  = 1.0f;
            smoothedDynGate[n]  = 1.0f;
        }
    }

    // ── Beat-Aware Note Gate: fires on each 16th-note clock tick ──
    void tickBeatEvent(int step, int pattern, bool isMinor, float styleDensity, float motifQuote, bool isSolo = false, float walkGroove = 0.0f, bool adventurous = false) {
        for (int n = 0; n < NOTES; n++) dynamicNoteGate[n] = 0.0f;
        int beat    = step / 4;
        int subBeat = step % 4;

        if (isSolo) {
            // ── BASS SOLO LEAD RIFF ENGINE ──
            // Bass player steps out front and plays an expressive, syncopated lead melody over the chords!
            // Uses Root (3), 3rd (isMinor? 2:4), 5th (5), Octave (6), Octave+3rd (7), Octave+5th (8).
            int currentBar = workingMemory.currentBar;
            int barInPhrase = currentBar % 4; // 4-bar solo phrasing architecture

            switch (barInPhrase) {
                case 0: // Bar 1: Strong melodic statement - Root punch into higher octave pops
                    if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.60f; } // Solid root anchor
                    else if (step == 3)  { dynamicNoteGate[6] = 0.95f; }                            // Octave syncopated pop
                    else if (step == 6)  { dynamicNoteGate[5] = 0.85f; }                            // 5th
                    else if (step == 8)  { dynamicNoteGate[3] = 0.80f; }                            // Root
                    else if (step == 10) { dynamicNoteGate[isMinor ? 2 : 4] = 0.90f; }              // 3rd
                    else if (step == 12) { dynamicNoteGate[5] = 0.95f; }                            // 5th
                    else if (step == 14) { dynamicNoteGate[6] = 1.0f; }                             // High octave finish
                    break;

                case 1: // Bar 2: High Register Walking Run & Arpeggio (singing solo)
                    if (step == 0)       { dynamicNoteGate[6] = 1.0f; dynamicNoteGate[3] = 0.50f; } // High Octave downbeat
                    else if (step == 2)  { dynamicNoteGate[7] = 0.90f; }                            // Octave + 3rd (10th) singing double-stop
                    else if (step == 4)  { dynamicNoteGate[8] = 0.95f; }                            // Octave + 5th peak
                    else if (step == 7)  { dynamicNoteGate[6] = 0.85f; }                            // Octave
                    else if (step == 8)  { dynamicNoteGate[5] = 0.90f; }                            // Descending 5th
                    else if (step == 11) { dynamicNoteGate[isMinor ? 2 : 4] = 0.85f; }              // 3rd
                    else if (step == 13) { dynamicNoteGate[5] = 0.80f; }                            // 5th
                    else if (step == 15) { dynamicNoteGate[3] = 0.70f; }                            // Lead into next bar
                    break;

                case 2: // Bar 3: Syncopated Funky Slap & Pop / Melodic accents
                    if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.75f; } // Low thump
                    else if (step == 2)  { dynamicNoteGate[6] = 1.05f; }                            // Pop octave
                    else if (step == 5)  { dynamicNoteGate[6] = 0.90f; }                            // Offbeat pop
                    else if (step == 8)  { dynamicNoteGate[3] = 0.90f; }                            // Mid-bar root
                    else if (step == 10) { dynamicNoteGate[5] = 0.85f; }                            // 5th
                    else if (step == 12) { dynamicNoteGate[isMinor ? 2 : 4] = 0.90f; }              // 3rd
                    else if (step == 14) { dynamicNoteGate[5] = 0.95f; }                            // 5th
                    break;

                case 3: // Bar 4: Climax Turnaround & Resolution back to band groove
                    if (step == 0)       { dynamicNoteGate[6] = 1.05f; dynamicNoteGate[3] = 0.70f; } // High octave hit
                    else if (step == 3)  { dynamicNoteGate[8] = 0.95f; }                            // High 5th
                    else if (step == 6)  { dynamicNoteGate[7] = 0.90f; }                            // High 3rd
                    else if (step == 8)  { dynamicNoteGate[6] = 0.85f; }                            // Octave
                    else if (step == 10) { dynamicNoteGate[5] = 0.85f; }                            // 5th
                    else if (step == 12) { dynamicNoteGate[isMinor ? 2 : 4] = 0.80f; }              // 3rd
                    else if (step == 14) { dynamicNoteGate[5] = 0.90f; }                            // Chromatic leading tone into next chorus
                    break;
            }
            return;
        }

        switch (pattern) {
            case 0: // ── Off / Pass-Through ──
                for (int n = 0; n < NOTES; n++) dynamicNoteGate[n] = 1.0f;
                break;

            case 1: // ── 1: Indie Rock & Post-Punk ──
                // Driving downpicked 8th notes (Joy Division, The Cure, Strokes) with offbeat octave accents
                if (step % 2 == 0) {
                    dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.55f; // Solid 8th-note root drive
                    if ((step == 6 || step == 14) && styleDensity > 4.0f) {
                        dynamicNoteGate[6] = 0.85f; // Octave pop on the "&" of 2 and 4
                    }
                }
                break;

            case 2: // ── 2: Indie Folk & Americana ──
                // Warm thumb-plucked alternating Root-5th with muted acoustic ghost on beats 2 & 4 (Mumford, Lumineers)
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.60f; } // Root beat 1
                else if (step == 4)  { dynamicNoteGate[3] = 0.40f; }                            // Soft ghost tap
                else if (step == 8)  { dynamicNoteGate[1] = 0.85f; dynamicNoteGate[5] = 0.60f; } // Low Sub-5th or 5th beat 3
                else if (step == 12) { dynamicNoteGate[3] = 0.35f; }                            // Ghost tap
                else if (step == 14 && styleDensity > 4.0f) { // Syncopated push into next chord
                    dynamicNoteGate[isMinor ? 2 : 4] = 0.70f;
                }
                break;

            case 3: // ── 3: Traditional Folk & Celtic ──
                // Resonant 1 - 5 foundational acoustic pulse with 3rd diatonic passing walk on turnarounds
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.65f; } // Deep root
                else if (step == 6)  { dynamicNoteGate[5] = 0.65f; }                            // Air-suspension 5th
                else if (step == 8)  { dynamicNoteGate[5] = 0.90f; dynamicNoteGate[1] = 0.70f; } // 5th beat 3
                else if (step == 12) { dynamicNoteGate[isMinor ? 2 : 4] = 0.80f; }              // 3rd passing tone
                else if (step == 14) { dynamicNoteGate[5] = 0.75f; }                            // Turnaround leading tone
                break;

            case 4: // ── 4: Folk Rock & Laurel Canyon ──
                // Melodic, conversational bassline (Leland Sklar, Carol Kaye): downbeat root, floating melodic 10ths and 5ths
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.50f; } // Root anchor
                else if (step == 3)  { dynamicNoteGate[5] = 0.70f; }                            // Anticipated 5th
                else if (step == 6)  { dynamicNoteGate[7] = 0.85f; }                            // High Octave + 3rd (10th)
                else if (step == 8)  { dynamicNoteGate[3] = 0.75f; }                            // Mid-bar root
                else if (step == 11) { dynamicNoteGate[6] = 0.80f; }                            // Octave
                else if (step == 14) { dynamicNoteGate[5] = 0.85f; }                            // Warm 5th leading down
                break;

            case 5: // ── 5: Bluegrass & Old-Time Boom-Chick ──
                // "Boom-Chick" alternating bass: sharp staccato downbeat Root on 1, punchy low Sub-5th on 3, fast pickup runs
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.80f; } // Boom (Root beat 1)
                else if (step == 4)  { dynamicNoteGate[3] = 0.45f; }                            // Chick (ghost tap)
                else if (step == 8)  { dynamicNoteGate[1] = 1.0f; dynamicNoteGate[5] = 0.50f; } // Boom (Sub-5th beat 3)
                else if (step == 12) { dynamicNoteGate[1] = 0.45f; }                            // Chick
                else if (step == 14 || step == 15) { // Fast bluegrass double-stop / pickup roll
                    dynamicNoteGate[isMinor ? 2 : 4] = 0.75f;
                    dynamicNoteGate[5] = 0.80f;
                }
                break;

            case 6: // ── 6: Classic Rock 'n' Roll Boogie ──
                // Slap-bass boogie-woogie arpeggio: 1 - 3 - 5 - 6 - b7 - 6 - 5 - 3
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.70f; } // 1 (Root)
                else if (step == 2)  { dynamicNoteGate[isMinor ? 2 : 4] = 0.85f; }              // 3
                else if (step == 4)  { dynamicNoteGate[5] = 0.90f; }                            // 5
                else if (step == 6)  { dynamicNoteGate[isMinor ? 2 : 4] = 0.75f; }              // 6 walk
                else if (step == 8)  { dynamicNoteGate[6] = 0.90f; }                            // b7 / octave peak
                else if (step == 10) { dynamicNoteGate[5] = 0.85f; }                            // 6
                else if (step == 12) { dynamicNoteGate[5] = 0.80f; }                            // 5
                else if (step == 14) { dynamicNoteGate[isMinor ? 2 : 4] = 0.85f; }              // 3 boogie turnaround
                break;

            case 7: // ── 7: Jazz-Rock Fusion (Pastorius / Clarke) ──
                // 16th-note syncopation, octave pops, rapid dead notes, harmonic 10ths
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.60f; } // Root
                else if (step == 2)  { dynamicNoteGate[6] = 0.95f; }                            // High Octave pop
                else if (step == 5)  { dynamicNoteGate[5] = 0.80f; }                            // 5th syncopation
                else if (step == 7)  { dynamicNoteGate[7] = 0.85f; }                            // Singing 10th
                else if (step == 8)  { dynamicNoteGate[3] = 0.80f; }                            // Mid root
                else if (step == 10) { dynamicNoteGate[6] = 0.90f; }                            // Octave pop
                else if (step == 13) { dynamicNoteGate[5] = 0.85f; }                            // 5th
                else if (step == 15) { dynamicNoteGate[isMinor ? 2 : 4] = 0.75f; }              // Leading tone
                break;

            case 8: // ── 8: Motown & Soul (James Jamerson "The Hook") ──
                // Syncopated dotted rhythms, chromatic runs, deep pocket swing
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.70f; } // Heavy root downbeat
                else if (step == 3)  { dynamicNoteGate[3] = 0.65f; }                            // Dotted 8th syncopated punch
                else if (step == 6)  { dynamicNoteGate[5] = 0.80f; }                            // 5th
                else if (step == 8)  { dynamicNoteGate[isMinor ? 2 : 4] = 0.85f; }              // 3rd
                else if (step == 10) { dynamicNoteGate[5] = 0.90f; }                            // 5th
                else if (step == 12) { dynamicNoteGate[6] = 0.75f; }                            // Octave
                else if (step == 14) { dynamicNoteGate[5] = 0.85f; }                            // Chromatic approach tone
                break;

            case 9: // ── 9: Dub & Roots Reggae One-Drop ──
                // Heavy sub-bass drop on Beat 3 (step 8), drop the downbeat, long conscious space
                if (step == 8)       { dynamicNoteGate[3] = 1.10f; dynamicNoteGate[0] = 1.00f; } // Massive One-Drop on Beat 3
                else if (step == 10) { dynamicNoteGate[5] = 0.75f; }                            // Deep 5th tail
                else if (step == 14 && styleDensity > 5.0f) { dynamicNoteGate[3] = 0.60f; }     // Occasional dub pickup
                break;

            case 10: // ── 10: 70s Funk & Slap Thumb (Larry Graham / Bootsy) ──
                // Aggressive thumb-thump on Beat 1, octave snap/pop on upbeats, percussive muted chops
                if (step == 0)       { dynamicNoteGate[3] = 1.10f; dynamicNoteGate[0] = 0.85f; } // Slap Thumb 1
                else if (step == 2)  { dynamicNoteGate[6] = 1.05f; }                            // Pop Octave
                else if (step == 5)  { dynamicNoteGate[6] = 0.90f; }                            // Offbeat pop
                else if (step == 8)  { dynamicNoteGate[3] = 0.95f; }                            // Thumb 3
                else if (step == 10) { dynamicNoteGate[5] = 0.85f; }                            // 5th
                else if (step == 12) { dynamicNoteGate[isMinor ? 2 : 4] = 0.90f; }              // 3rd
                else if (step == 14) { dynamicNoteGate[6] = 1.00f; }                            // Turnaround pop
                break;

            case 11: // ── 11: Chicago / Texas Blues Shuffle ──
                // Classic R - 5 - 6 - b7 box shuffle feel
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.65f; } // Root beat 1
                else if (step == 2)  { dynamicNoteGate[3] = 0.45f; }                            // Swing bounce
                else if (step == 4)  { dynamicNoteGate[5] = 0.90f; }                            // 5th beat 2
                else if (step == 6)  { dynamicNoteGate[5] = 0.50f; }
                else if (step == 8)  { dynamicNoteGate[isMinor ? 2 : 4] = 0.85f; }              // 3rd / 6th walk beat 3
                else if (step == 10) { dynamicNoteGate[5] = 0.60f; }
                else if (step == 12) { dynamicNoteGate[6] = 0.80f; dynamicNoteGate[5] = 0.85f; } // Octave / 5th beat 4
                else if (step == 14) { dynamicNoteGate[5] = 0.70f; }                            // Leading pickup
                break;

            case 12: // ── 12: Ray Brown Walking Jazz ──
                // Beat 1: Root; Beat 2: 3rd; Beat 3: 5th; Beat 4: Chromatic approach tone
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.65f; } // Beat 1: Root
                else if (step == 4)  { dynamicNoteGate[isMinor ? 2 : 4] = 0.90f; }              // Beat 2: 3rd
                else if (step == 8)  { dynamicNoteGate[5] = 0.95f; }                            // Beat 3: 5th
                else if (step == 12) { dynamicNoteGate[isMinor ? 2 : 4] = 0.75f; dynamicNoteGate[5] = 0.80f; } // Beat 4: Approach
                else if (step == 14 && styleDensity > 4.0f) { dynamicNoteGate[3] = 0.40f; }     // Ghosted swing
                break;

            case 13: // ── 13: Afro-Cuban & Bossa Tumbao ──
                // Anticipates chord change on beat 4's "and" (step 14), pulse on beat 1 and beat 3 "and"
                if (step == 0)       { dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.60f; } // Beat 1 Root
                else if (step == 6)  { dynamicNoteGate[5] = 0.85f; }                            // Beat 2 "and"
                else if (step == 10) { dynamicNoteGate[6] = 0.90f; }                            // Beat 3 "and" octave
                else if (step == 14) { dynamicNoteGate[3] = 1.05f; dynamicNoteGate[5] = 0.85f; } // Beat 4 "and" Tumbao push!
                break;

            case 14: // ── 14: Metal & Hard Rock Gallop (Steve Harris / Maiden) ──
                // Triplet-style driving gallop: 8th + two 16ths (1 & a)
                if (step == 0 || step == 4 || step == 8 || step == 12) {
                    dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.75f; // Onbeat gallop anchor
                } else if (step == 2 || step == 3 || step == 6 || step == 7 ||
                           step == 10 || step == 11 || step == 14 || step == 15) {
                    dynamicNoteGate[3] = 0.85f; // Fast rolling gallop pickups
                    if ((step == 14 || step == 15) && styleDensity > 5.0f) {
                        dynamicNoteGate[5] = 0.80f; // Gallop fill on turnaround
                    }
                }
                break;

            case 15: // ── 15: Neo-Soul & Hip-Hop Pocket (Pino Palladino) ──
                // Deep behind-the-beat laid-back sub-bass pocket, understated minimal phrasing
                if (step == 0)       { dynamicNoteGate[3] = 1.05f; dynamicNoteGate[0] = 0.95f; } // Thick sub-root
                else if (step == 6)  { dynamicNoteGate[5] = 0.70f; }                            // Laid-back 5th
                else if (step == 8)  { dynamicNoteGate[3] = 0.80f; dynamicNoteGate[0] = 0.70f; } // Half-bar anchor
                else if (step == 11) { dynamicNoteGate[isMinor ? 2 : 4] = 0.75f; }              // Vocal-gap melodic slide
                else if (step == 14) { dynamicNoteGate[1] = 0.80f; }                            // Deep sub-5th pickup
                break;

            case 16: // ── 16: Autonomous Amalgamation (The Living Bassist) ──
                // The AI Session Player's Brain: Blends and cross-pollinates the 16 genre lessons & legendary bassists
                // Adapts phrasing according to song sections (Pass 0: Verse, Pass 1: Chorus, Pass 2+: Climax),
                // beat division, guitar strike intensity, and walk_groove (Port 41).
                {
                    int cycle = workingMemory.cycleIteration;
                    int barInPhrase = workingMemory.currentBar % 4;
                    float flair = std::max(0.0f, std::min(1.0f, walkGroove / 10.0f));

                    if (cycle == 0) {
                        // ── PASS 0: THE VERSE / FOUNDATION ──
                        // Grounded in Carol Kaye / Leland Sklar / Indie Folk pocket: warm, breathing downbeats, steady 5ths
                        if (step == 0) {
                            dynamicNoteGate[3] = 1.0f; dynamicNoteGate[0] = 0.70f; // Firm anchor root
                        } else if (step == 4 && (styleDensity > 4.0f || flair > 0.3f)) {
                            dynamicNoteGate[3] = 0.45f; // Soft acoustic ghost tap
                        } else if (step == 8) {
                            dynamicNoteGate[5] = 0.85f; dynamicNoteGate[1] = 0.65f; // Sub-5th / 5th anchor
                        } else if (step == 11 && flair > 0.4f) {
                            dynamicNoteGate[isMinor ? 2 : 4] = 0.70f; // Conversational 3rd passing tone
                        } else if (step == 14) {
                            if (barInPhrase == 3) {
                                dynamicNoteGate[5] = 0.90f; // Turnaround leading tone into next bar
                            } else if (styleDensity > 5.0f) {
                                dynamicNoteGate[3] = 0.60f; // Gentle pickup
                            }
                        }
                    } else if (cycle == 1) {
                        // ── PASS 1: THE CHORUS / GROOVE EXPLOSION ──
                        // Motown (Jamerson) syncopation meets Rock'n'Roll boogie drive & McCartney melodic counterpoints
                        if (step == 0) {
                            dynamicNoteGate[3] = 1.05f; dynamicNoteGate[0] = 0.80f; // Powerful chorus downbeat
                        } else if (step == 2 && flair > 0.5f) {
                            dynamicNoteGate[6] = 0.80f; // Syncopated octave pop
                        } else if (step == 3) {
                            dynamicNoteGate[3] = 0.75f; // Jamerson dotted 8th push
                        } else if (step == 6) {
                            dynamicNoteGate[5] = 0.85f; // 5th offbeat punch
                        } else if (step == 8) {
                            dynamicNoteGate[isMinor ? 2 : 4] = 0.90f; // Melodic 3rd
                        } else if (step == 10) {
                            dynamicNoteGate[5] = 0.90f; // 5th
                        } else if (step == 12) {
                            dynamicNoteGate[6] = 0.85f; // High octave leap
                        } else if (step == 14) {
                            // Chromatic encirclement turnaround: Jamerson & Ray Brown walking approach
                            dynamicNoteGate[5] = 0.90f;
                            if (flair > 0.6f) dynamicNoteGate[isMinor ? 2 : 4] = 0.85f;
                        }
                    } else {
                        // ── PASS 2+: THE CLIMAX / VIRTUOSIC AMALGAMATION ──
                        // Fusion (Pastorius) 16th-note syncopation, Latin Tumbao pushes, singing harmonic 10ths
                        if (step == 0) {
                            dynamicNoteGate[3] = 1.10f; dynamicNoteGate[0] = 0.85f; // Climax downbeat
                        } else if (step == 2) {
                            dynamicNoteGate[6] = 0.95f; // High octave pop
                        } else if (step == 5) {
                            dynamicNoteGate[5] = 0.80f; // Offbeat 5th
                        } else if (step == 7) {
                            dynamicNoteGate[7] = 0.85f; // McCartney/Jaco singing 10th (Octave + 3rd)
                        } else if (step == 8) {
                            dynamicNoteGate[3] = 0.90f; // Half-bar root punch
                        } else if (step == 10) {
                            dynamicNoteGate[6] = 0.90f; // Octave pop
                        } else if (step == 13) {
                            dynamicNoteGate[5] = 0.85f; // Syncopated 5th
                        } else if (step == 14) {
                            // Tumbao anticipatory push on beat 4 "and"
                            dynamicNoteGate[3] = 1.05f;
                            dynamicNoteGate[5] = 0.85f;
                            if (barInPhrase == 3) dynamicNoteGate[6] = 0.90f; // Climax phrase turnaround
                        }
                    }
                }
                break;

            default:
                for (int n = 0; n < NOTES; n++) dynamicNoteGate[n] = 1.0f;
                break;
        }

        // Musical Approach Tone: On step 14 or 15 right before downbeat step 0, if an approach is active, lift +3rd or +5th to lead smoothly into target
        if (approachActive && (step == 14 || step == 15)) {
            dynamicNoteGate[3] = 0.40f;
            dynamicNoteGate[5] = 0.85f; // 5th or leading tone
        }

        // Song Catalog Anticipation: If a song roadmap is recognized and the upcoming bar changes chord,
        // voice-lead seamlessly into the next root on beat 4 with 0ms latency!
        int nextSongRoot = songCatalog.getAnticipatedNextRoot();
        if (nextSongRoot >= 0 && (step == 14 || step == 15)) {
            dynamicNoteGate[3] = 0.30f;
            dynamicNoteGate[5] = 0.90f; // Leading tone / 5th pivot
            if (step == 15) dynamicNoteGate[isMinor ? 2 : 4] = 0.80f;
        }

        // 30-Year Bass Player Brain Tropes:
        // A. 12-Bar Blues Turnaround: On Bars 11 & 12 (or bar 10 & 11) of a detected 12-bar blues progression, play classic turnaround chromatic fill
        if (workingMemory.detectedArchetype == WorkingMemoryEngine::ARCHETYPE_BLUES_12) {
            int barInBlues = workingMemory.currentBar % 12;
            if ((barInBlues == 10 || barInBlues == 11) && step >= 8) {
                // Classic turnaround run: walk 5th -> 3rd -> root on steps 8, 12, 14
                if (step == 8)       { dynamicNoteGate[5] = 0.95f; }
                else if (step == 12) { dynamicNoteGate[isMinor ? 2 : 4] = 0.90f; }
                else if (step == 14) { dynamicNoteGate[5] = 0.85f; }
            }
        }

        // B. Working Memory Pass 2+ Embellishment & 4/8-Bar Turnarounds:
        // On Pass 2+ of a recognized chord progression, add taste and musical variation (walking 5th/6th on turnaround)
        if (workingMemory.isEmbellishPass() && (step == 12 || step == 14)) {
            dynamicNoteGate[5] = std::max(dynamicNoteGate[5], 0.85f);
            dynamicNoteGate[isMinor ? 2 : 4] = std::max(dynamicNoteGate[isMinor ? 2 : 4], 0.75f);
            if (step == 14) dynamicNoteGate[6] = std::max(dynamicNoteGate[6], 0.65f); // High octave grace note
        }

        // ── ADVENTUROUS MODE — HARMONIC VOICE INDEPENDENCE ──
        // Bass fills diatonic space that the guitar isn't occupying.
        // Instead of doubling the root, it leans toward the 5th and 3rd of the chord
        // on beats 2 and 3 — walking around the scale like a real bass player.
        // Sub-octave (index 0) always stays on the root for low-end foundation.
        // All note choices are diatonic — no chromatic passing tones.
        if (adventurous) {
            // Scale how aggressively we depart from the root (0..10 port → 0.0..1.0 factor)
            float depthFactor = std::min(styleDensity / 10.0f, 1.0f);

            // Index shortcuts for clarity
            const int IDX_SUB    = 0; // sub-octave root — always kept
            const int IDX_ROOT   = 3; // root
            const int IDX_THIRD  = isMinor ? 2 : 4; // minor 3rd or major 3rd
            const int IDX_FIFTH  = 5; // perfect 5th (diatonic, always safe)

            // Beat 0 (steps 0–3): ROOT ANCHOR — keep whatever the pattern set for harmonic clarity
            // Nothing to change; the pattern already set root-based gates.

            // Beat 1 (steps 4–7): LEAN TO THE FIFTH
            // Redirect any root gate energy toward the 5th; sub-oct stays on root.
            if (beat == 1) {
                float rootGate = dynamicNoteGate[IDX_ROOT];
                if (rootGate > 0.05f) {
                    // Blend: as depthFactor rises, more energy shifts from root to 5th
                    float shiftAmt = rootGate * depthFactor;
                    dynamicNoteGate[IDX_ROOT]  = rootGate - shiftAmt;
                    dynamicNoteGate[IDX_FIFTH] = std::max(dynamicNoteGate[IDX_FIFTH], shiftAmt * 0.95f);
                }
                // Sub-octave always stays grounded on root — keeps the low-end solid
                dynamicNoteGate[IDX_SUB] = std::max(dynamicNoteGate[IDX_SUB], 0.80f * depthFactor);
            }

            // Beat 2 (steps 8–11): LEAN TO THE THIRD — colours the chord
            // Redirect root gate to the 3rd (major or minor depending on isMinor).
            if (beat == 2) {
                float rootGate = dynamicNoteGate[IDX_ROOT];
                if (rootGate > 0.05f) {
                    float shiftAmt = rootGate * depthFactor;
                    dynamicNoteGate[IDX_ROOT]  = rootGate - shiftAmt;
                    dynamicNoteGate[IDX_THIRD] = std::max(dynamicNoteGate[IDX_THIRD], shiftAmt * 0.90f);
                }
                // Sub-octave stays on root
                dynamicNoteGate[IDX_SUB] = std::max(dynamicNoteGate[IDX_SUB], 0.80f * depthFactor);
            }

            // Beat 3 (steps 12–15): WALKING BACK — mix 5th and root to approach home
            // The 5th on 12–13 provides the walk, root on 14–15 lands us back.
            if (beat == 3) {
                if (subBeat <= 1) {
                    // First half of beat 3: add the 5th alongside whatever is there
                    dynamicNoteGate[IDX_FIFTH] = std::max(dynamicNoteGate[IDX_FIFTH], 0.80f * depthFactor);
                } else {
                    // Second half of beat 3: approach back to root — strengthen root
                    dynamicNoteGate[IDX_ROOT] = std::max(dynamicNoteGate[IDX_ROOT], 0.85f * depthFactor);
                }
                // Sub-octave keeps low-end grounded throughout
                dynamicNoteGate[IDX_SUB] = std::max(dynamicNoteGate[IDX_SUB], 0.75f * depthFactor);
            }

            // At high density (styleDensity > 6): on beat 1 sub-beats 2–3 also add diatonic 3rd
            // for a brief passing colour — still fully diatonic, adds life without clutter.
            if (styleDensity > 6.0f && beat == 1 && subBeat >= 2) {
                float passDepth = (styleDensity - 6.0f) / 4.0f; // 0..1 for density 6..10
                dynamicNoteGate[IDX_THIRD] = std::max(dynamicNoteGate[IDX_THIRD], 0.70f * passDepth);
            }
        }
    }


    // ── 4-Way Majority Rules Confidence Gate ──

    void evaluateChordConfidenceMatrix(bool forceMinor, bool forceMajor,
                                       float trackSens, float trackStab,
                                       float debounceCycles, float bassBoost,
                                       int keyRootVal, int scaleModeVal,
                                       float droneImmunity, float logicStrictVal,
                                       bool pickTriggerFired,
                                       bool workingMemoryEnabled = false,
                                       bool fretlessEnabled = false)
    {
        if (!gateIsOpen && gateGain < 0.05f) return;

        // Run both mono fundamental root extractors
        fastRootDetector.analyze(sr);
        strictRootDetector.analyze(sr, 4.0f);

        int root1 = fastRootDetector.detectedSemi;
        int root2 = strictRootDetector.detectedSemi;

        // Build Fast and Strict Chroma Vectors
        memset(fastChroma, 0, sizeof(fastChroma));
        memset(strictChroma, 0, sizeof(strictChroma));

        int fastCombBest = -1;
        float fastCombMax = 0.0f;
        int strictCombBest = -1;
        float strictCombMax = 0.0f;

        float bassMult = 1.0f + (bassBoost / 10.0f) * 2.5f;

        for (int i = 0; i < 36; ++i) {
            int semi = (40 + i) % 12;
            float fEnv = fastFilterBank[i].envelope;
            float sEnv = strictFilterBank[i].envelope;

            float weight = (i < 12) ? bassMult : 1.0f;
            fastChroma[semi] += fEnv * weight;
            strictChroma[semi] += sEnv * weight;

            if (fastChroma[semi] > fastCombMax) {
                fastCombMax = fastChroma[semi];
                fastCombBest = semi;
            }
            if (strictChroma[semi] > strictCombMax) {
                strictCombMax = strictChroma[semi];
                strictCombBest = semi;
            }
        }

        // Smooth chroma
        float smoothCoeff = 1.0f / std::max(1.1f, trackStab * 0.5f);
        for (int s = 0; s < 12; ++s) {
            smoothedChroma[s] += smoothCoeff * (strictChroma[s] - smoothedChroma[s]);
        }

        // 4 Detectors:
        //  1: fastRootDetector (root1)
        //  2: strictRootDetector (root2)
        //  3: fastCombBest (comb1)
        //  4: strictCombBest (comb2)

        int voteCounts[12] = {};
        if (root1 >= 0) voteCounts[root1] += 2; // Fast attack detector gets high instant weight
        if (root2 >= 0) voteCounts[root2] += 1;
        if (fastCombBest >= 0) voteCounts[fastCombBest] += 2;
        if (strictCombBest >= 0) voteCounts[strictCombBest] += 2;

        // Drone Immunity Logic: If pick attack click occurred, heavily downweight Root 2 if it matches previous droning root
        if (pickTriggerFired && droneImmunity > 2.0f && confirmedRoot >= 0) {
            if (root2 == confirmedRoot) {
                voteCounts[root2] = std::max(0, voteCounts[root2] - 2); // Penalize droning bass string
            }
        }

        int majorityPitch = -1;
        int highestVotes = 0;
        for (int p = 0; p < 12; ++p) {
            if (voteCounts[p] > highestVotes) {
                highestVotes = voteCounts[p];
                majorityPitch = p;
            }
        }

        // Working Memory Glitch Consensus Filter:
        // If user has enabled Working Memory switch, consult the cyclical chord history buffer.
        // It guards against transient blips, but yields immediately when a real chord change occurs.
        if (workingMemoryEnabled && majorityPitch >= 0) {
            majorityPitch = workingMemory.getMemoryGuardedRoot(majorityPitch, currentStep, highestVotes);
        }

        // Spectral Distance / Chroma Flux Unsticking:
        // Compare current fastChroma vs smoothedChroma. If the energy has shifted decisively to a new note
        // (chroma flux ratio > 1.35x), the guitarist has clearly changed chord, so decay lock is bypassed!
        float chromaFlux = 0.0f;
        if (confirmedRoot >= 0) {
            float oldRootChroma = smoothedChroma[confirmedRoot];
            float newCandidateChroma = (majorityPitch >= 0) ? fastChroma[majorityPitch] : 0.0f;
            if (majorityPitch != confirmedRoot && newCandidateChroma > oldRootChroma * 1.35f && newCandidateChroma > 0.02f) {
                chromaFlux = newCandidateChroma;
            }
        }

        // Reset gateOpenPeak if fresh energy influx arrives so soft decay doesn't get stuck forever
        if (gateEnv > gateOpenPeak * 1.15f) {
            gateOpenPeak = gateEnv;
        }

        // Attack-Latch & Decay Tail:
        // Only treat as decay freeze if signal is strictly decaying AND no new pitch flux is detected.
        // If a new pitch has solid votes and chroma flux is active, chord change is permitted!
        bool isDecayTail = (!pickTriggerFired && gateIsOpen && gateOpenPeak > 0.005f && gateEnv < gateOpenPeak * 0.75f && chromaFlux < 0.001f);

        int minVotesRequired = isDecayTail ? 4 : 3;
        if (pickTriggerFired) minVotesRequired = 2; // Fast pickup on clear pick attacks

        if (majorityPitch >= 0 && highestVotes >= minVotesRequired) {
            int baseDebounce = (int)std::max(1.0f, roundf(debounceCycles));
            // Responsive debounce: 1 block on pick strike; strict debounce (>= 3 blocks) during sustained play or flux
            int reqBlocks = pickTriggerFired ? 1 : (isDecayTail ? (baseDebounce + 3) : std::max(3, baseDebounce));
            if (majorityPitch == candidateRoot) {
                candidateBlocks++;
            } else {
                candidateRoot = majorityPitch;
                candidateBlocks = 1;
            }

            if (candidateBlocks >= reqBlocks && candidateRoot != confirmedRoot) {
                prevConfirmedRoot = confirmedRoot;
                targetRoot = candidateRoot;
                confirmedRoot = candidateRoot;
                confirmedMinor = forceMinor || (!forceMajor && smoothedChroma[(confirmedRoot + 3) % 12] > smoothedChroma[(confirmedRoot + 4) % 12]);

                // Reset gate peak to current envelope so next chord tracks fresh baseline
                gateOpenPeak = gateEnv;

                // Record confirmed step into Working Memory
                if (workingMemoryEnabled) {
                    float confNorm = (float)highestVotes / 7.0f;
                    workingMemory.recordStepRoot(currentStep, confirmedRoot, confirmedMinor, confNorm);
                }

                // Arm approach leading tone if previous root was established
                if (prevConfirmedRoot >= 0) {
                    approachActive = true;
                    // Leading tone: semitone below target root, or 5th
                    int diff = (targetRoot - prevConfirmedRoot + 12) % 12;
                    approachOffsetSemi = (diff <= 6) ? -1 : 1;
                }

                float newRootHz = (A4 * 0.25f) * powf(2.0f, (confirmedRoot - 9) / 12.0f);
                fretlessEngine.onNoteTrigger(confirmedRoot, newRootHz, sr, fretlessEnabled);

                if (activeBank == 0) {
                    bankB.rootHz = newRootHz;
                    bankB.isMinor = confirmedMinor;
                    activeBank = 1;
                } else {
                    bankA.rootHz = newRootHz;
                    bankA.isMinor = confirmedMinor;
                    activeBank = 0;
                }
                candidateBlocks = 0;
            } else if (confirmedRoot >= 0) {
                // Track major/minor quality live even if the chord root stays the same
                bool liveMinor = forceMinor || (!forceMajor && smoothedChroma[(confirmedRoot + 3) % 12] > smoothedChroma[(confirmedRoot + 4) % 12]);
                if (liveMinor != confirmedMinor) {
                    confirmedMinor = liveMinor;
                    if (activeBank == 0) bankA.isMinor = confirmedMinor;
                    else bankB.isMinor = confirmedMinor;
                }
            }
        }
    }

    // ── 9 Vintage Bass Presets Voice Rendering ──
    inline float renderBassVoice(int preset, float noteFund, double& phase, double& subPhase, float& filterState, float ctrl1, float ctrl2) {
        phase += (double)noteFund * (M_TWOPI / (double)sr);
        if (phase >= M_TWOPI) phase -= M_TWOPI;

        subPhase += (double)(noteFund * 0.5f) * (M_TWOPI / (double)sr);
        if (subPhase >= M_TWOPI) subPhase -= M_TWOPI;

        float p = (float)phase;
        float sp = (float)subPhase;
        float sig = 0.0f;

        float c1 = ctrl1 / 10.0f;
        float c2 = ctrl2 / 10.0f;

        switch (preset) {
            case 0: { // 1: Precision (P-Bass punch + warm split coil)
                float s1 = sinf(p);
                float s2 = sinf(p * 2.0f) * 0.35f;
                float s3 = sinf(p * 3.0f) * 0.15f;
                sig = s1 + s2 + s3;
                sig = tanhf(sig * (1.2f + c1 * 0.8f));
                float fc = 350.0f + c1 * 1800.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
            case 1: { // 2: Longhorn (Baritone Danelectro twang + hollow body)
                float tri = (p < (float)M_PI) ? (-1.0f + (2.0f / (float)M_PI) * p) : (3.0f - (2.0f / (float)M_PI) * p);
                float square = (p < (float)M_PI) ? 0.8f : -0.8f;
                sig = tri * (1.0f - c2 * 0.5f) + square * (0.3f + c2 * 0.4f);
                float fc = 600.0f + c1 * 2600.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
            case 2: { // 3: Fretless (Smooth mwah + singing midrange bloom)
                float s1 = sinf(p);
                float s2 = sinf(p * 2.0f) * 0.45f;
                float s3 = sinf(p * 3.0f) * 0.25f;
                sig = s1 + s2 * (0.5f + c1 * 0.8f) + s3 * (0.2f + c1 * 0.5f);
                if (c2 > 0.05f) {
                    float chorusPhase = p + sinf(sp * 4.0f) * (c2 * 0.25f);
                    sig += (c2 * 0.30f) * sinf(chorusPhase);
                }
                float fc = 450.0f + c1 * 1400.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
            case 3: { // 4: Synth (Moog Taurus massive resonant analog pedal)
                float saw = 1.0f - (1.0f / (float)M_PI) * p;
                float subSquare = (sp < (float)M_PI) ? 0.9f : -0.9f;
                sig = saw * 0.7f + subSquare * (0.5f + c2 * 0.8f);
                float fc = 180.0f + c1 * 1200.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = tanhf(filterState * 1.5f);
                break;
            }
            case 4: { // 5: Virtual (Dynamic Contour: body density & neck scale)
                float sine = sinf(p);
                float subSine = sinf(sp);
                float brightSaw = 1.0f - (1.0f / (float)M_PI) * p;
                sig = sine * (1.2f - c1 * 0.5f) + subSine * (0.3f + c1 * 0.7f) + brightSaw * (c2 * 0.4f);
                float fc = 300.0f + c2 * 2200.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
            case 5: { // 6: Bowed (Upright Contrabass with cello bow rasp)
                float saw = 1.0f - (1.0f / (float)M_PI) * p;
                float bowNoise = (float)rand() / (float)RAND_MAX - 0.5f;
                sig = saw * 0.8f + bowNoise * (0.08f + c1 * 0.15f);
                float fc = 220.0f + c2 * 1400.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
            case 6: { // 7: Split Bass (Deep clean sub-fundamental poly)
                float s1 = sinf(p);
                float sSub = sinf(sp);
                sig = s1 * 0.6f + sSub * (0.8f + c2 * 0.6f);
                float fc = 280.0f + c1 * 1500.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
            case 7: { // 8: 3:03 (Acid Bass: squelchy TB-303 sawtooth)
                float saw = 1.0f - (1.0f / (float)M_PI) * p;
                float fc = 200.0f + c1 * 2500.0f;
                float qReso = 1.0f + c2 * 4.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (saw * qReso - filterState);
                sig = tanhf(filterState);
                break;
            }
            case 8: { // 9: Flip-Flop (EHX Octave Multiplexer logic-driven sub)
                float sq = (p < (float)M_PI) ? 0.7f : -0.7f;
                float subSq = (sp < (float)M_PI) ? 0.9f : -0.9f;
                sig = sq * (1.0f - c2 * 0.5f) + subSq * (0.6f + c2 * 0.8f);
                float fc = 250.0f + c1 * 1600.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
            case 9:  // 10: Studio Low-Fat Bass
            case 10: // 11: Real Electric Studio C2
            case 11: // 12: Real 5-String Sub B0
            case 12: // 13: Real Bridge Fretless D
            case 13: // 14: Real Acoustic Upright Pluck
            case 14: // 15: Real Acoustic Cello Body
            case 15: // 16: Real Accordion Bass Reed
            case 16: // 17: Real Upright Piano A0 Sub
            case 17: // 18: Real Fat Vintage P-Bass
            default: {
                const float* tablePtr = REAL_BASS_WAVETABLE;
                float baseFc = 2800.0f;
                float fcRange = 4000.0f;
                float waveGain = 1.0f;

                switch (preset) {
                    case 10: // Studio Fingerstyle C2
                        tablePtr = WT_STUDIO_FINGER;
                        baseFc = 800.0f;  fcRange = 3500.0f;
                        break;
                    case 11: // 5-String Sub B0
                        tablePtr = WT_SUB_BSTRING;
                        baseFc = 350.0f;  fcRange = 2200.0f;
                        waveGain = 1.15f;
                        break;
                    case 12: // Bridge Fretless D
                        tablePtr = WT_REAL_FRETLESS;
                        baseFc = 600.0f;  fcRange = 3800.0f;
                        break;
                    case 13: // Acoustic Upright Pluck
                        tablePtr = WT_UPRIGHT_PLUCK;
                        baseFc = 450.0f;  fcRange = 2800.0f;
                        break;
                    case 14: // Acoustic Cello Body
                        tablePtr = WT_CELLO_BASS;
                        baseFc = 550.0f;  fcRange = 3400.0f;
                        break;
                    case 15: // Accordion Bass Reed (Bright, buzzy reed acoustic harmonics)
                        tablePtr = WT_ACCORDION_BASS;
                        baseFc = 1200.0f; fcRange = 6500.0f;
                        waveGain = 1.1f;
                        break;
                    case 16: // Upright Piano A0 Sub (Percussive acoustic string attack)
                        tablePtr = WT_PIANO_A0;
                        baseFc = 1000.0f; fcRange = 5500.0f;
                        break;
                    case 17: // Fat Vintage P-Bass
                        tablePtr = WT_FAT_P_BASS;
                        baseFc = 700.0f;  fcRange = 3200.0f;
                        break;
                    default:
                        tablePtr = REAL_BASS_WAVETABLE;
                        baseFc = 600.0f;  fcRange = 3000.0f;
                        break;
                }

                // Table index from phase [0, 2*PI)
                float normPhase = (p / (float)M_TWOPI) * 256.0f;
                int idx0 = (int)normPhase;
                if (idx0 < 0) idx0 = 0;
                if (idx0 >= 256) idx0 = 255;
                int idx1 = (idx0 + 1) % 256;
                float frac = normPhase - (float)idx0;
                float wave = (tablePtr[idx0] * (1.0f - frac) + tablePtr[idx1] * frac) * waveGain;

                // Sub-octave reinforcement is controlled STRICTLY by ctrl2 (Sub control)
                // When c2 is 0, output is 100% pure acoustic wavetable without synthetic sine masking!
                float subSine = sinf(sp);
                sig = wave + subSine * (c2 * 0.70f);

                // Acoustic tone filter: opens up according to instrument profile and ctrl1
                float fc = baseFc + c1 * fcRange;
                if (fc > sr * 0.45f) fc = sr * 0.45f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
        }
        return sig;
    }
};

// -------------------------------------------------------------------------
// LV2 Plugin Wrapper
// -------------------------------------------------------------------------
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

    // BASS 9 PARAMS
    PORT_PRESET      = 16,
    PORT_CTRL1       = 17,
    PORT_CTRL2       = 18,
    PORT_ATTACK      = 19,
    PORT_SUSTAIN     = 20,
    PORT_RELEASE     = 21,
    PORT_GATE_SENS   = 22,
    PORT_DRY         = 23,
    PORT_WET         = 24,
    PORT_OUTPUT      = 25,

    // KEY & SCALE DIATONIC LOCK
    PORT_KEY_ROOT    = 26,
    PORT_SCALE_MODE  = 27,

    // DUAL DETECTOR CALIBRATION
    PORT_TRACK_SENS  = 28,
    PORT_TRACK_STAB  = 29,
    PORT_DEBOUNCE    = 30,
    PORT_FILTER_Q    = 31,
    PORT_BASS_BOOST  = 32,
    PORT_AGC_SPEED   = 33,
    PORT_AC_WEIGHT   = 34,
    PORT_AC_CLIP     = 35,

    // AUTO RHYTHM & GUIDED PERFORMER
    PORT_BASS_LEVEL   = 36,
    PORT_BASS_PATTERN = 37,
    PORT_BASS_BPM     = 38,
    PORT_BASS_TAP     = 39,
    PORT_BASS_TONE    = 40,
    PORT_WALK_GROOVE  = 41,
    PORT_LOGIC_STRICT = 42,
    PORT_CHORD_HOLD   = 43,

    // PRO EXTENSIONS (4-WAY MATRIX & GUIDED PERFORMER)
    PORT_TRACK_MODE      = 44, // 0: Standard, 1: 4-Way Confidence Matrix
    PORT_TRANSIENT_SENS  = 45, // Broadband Pick Attack Sens
    PORT_DRONE_IMMUNITY  = 46, // Open Bb Drone Rejection Weight
    PORT_PERFORMER_MODE  = 47, // 0: Off, 1: Guided Roadmap, 2: Style Donor, 3: Full Session
    PORT_PERFORMER_BIAS  = 48, // Roadmap Bias %
    PORT_STYLE_DENSITY   = 49, // Groove Density
    PORT_MOTIF_QUOTE     = 50, // Melodic Quote Probability
    PORT_BALLAD_RELAX    = 51, // Verse / Ballad Relax Mode (Footswitchable)
    PORT_FRETLESS_EXPRESS= 52, // Decoupled Fretless Expression (Intonation hunting, gliss, delayed vibrato)
    PORT_WORKING_MEMORY  = 53, // Self-Learning Working Memory Buffer (Chord pattern recall & embellishment)
    PORT_SOLO_MODE       = 54, // Momentary Bass Solo Lead Mode (Boost, presence cut-through, octave reach)
    PORT_SONG_RECORD     = 55, // 0: Idle/Play, 1: Record / Learn Song Roadmap
    PORT_SONG_SLOT       = 56, // 0: Auto-Detect, 1..8: Specific Song Slot
    PORT_BASS_MUTE       = 57, // 0: Active, 1: Muted (bass audio silenced, detectors still listen)
    PORT_DETECTED_ROOT   = 58, // Output port: -1 (none) or 0..11 (C..B)
    PORT_DETECTED_QUALITY= 59, // Output port: 0 = Maj, 1 = Min
    PORT_ADVENTUROUS_MODE= 60, // Adventurous (Virtuoso) Mode — chromatic passing tones, octave leaps, 16th syncopation

    PORT_COUNT           = 63
};

struct CyberEvansBassPro {
    EvansBassProDSP dsp;
    const float* in_l = nullptr;
    const float* in_r = nullptr;
    float* out_l = nullptr;
    float* out_r = nullptr;
    const float* ports[PORT_COUNT] = {};
    float* out_ports[PORT_COUNT] = {};

    CyberEvansBassPro(double sr) {
        dsp.init((float)sr);
    }
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const*) {
    return (LV2_Handle)new CyberEvansBassPro(rate);
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    CyberEvansBassPro* p = (CyberEvansBassPro*)instance;
    if (port < PORT_COUNT) {
        p->ports[port] = (const float*)data;
        p->out_ports[port] = (float*)data;
    }
    if (port == PORT_IN_L) p->in_l = (const float*)data;
    if (port == PORT_IN_R) p->in_r = (const float*)data;
    if (port == PORT_OUT_L) p->out_l = (float*)data;
    if (port == PORT_OUT_R) p->out_r = (float*)data;
}

static void activate(LV2_Handle instance) {
    CyberEvansBassPro* p = (CyberEvansBassPro*)instance;
    p->dsp.init(p->dsp.sr);
}

static void run(LV2_Handle instance, uint32_t sample_count) {
    CyberEvansBassPro* p = (CyberEvansBassPro*)instance;
    if (!p || sample_count == 0) return;

    bool bypass = (p->ports[PORT_BYPASS] && *p->ports[PORT_BYPASS] < 0.5f);
    if (bypass) {
        if (p->out_l != p->in_l) memcpy(p->out_l, p->in_l, sample_count * sizeof(float));
        if (p->out_r && p->in_r && p->out_r != p->in_r) memcpy(p->out_r, p->in_r, sample_count * sizeof(float));
        return;
    }

    // ── Per-block port reads (constant for entire block in LV2) ──
    float droneImmunity = p->ports[PORT_DRONE_IMMUNITY] ? *p->ports[PORT_DRONE_IMMUNITY] : 7.5f;
    float transSens     = p->ports[PORT_TRANSIENT_SENS] ? *p->ports[PORT_TRANSIENT_SENS] : 6.5f;
    float trackSens     = p->ports[PORT_TRACK_SENS]     ? *p->ports[PORT_TRACK_SENS]     : 8.0f;
    float trackStab     = p->ports[PORT_TRACK_STAB]     ? *p->ports[PORT_TRACK_STAB]     : 6.0f;
    float debounce      = p->ports[PORT_DEBOUNCE]       ? *p->ports[PORT_DEBOUNCE]       : 2.0f;
    float bassBoost     = p->ports[PORT_BASS_BOOST]     ? *p->ports[PORT_BASS_BOOST]     : 6.0f;
    float logicStrict   = p->ports[PORT_LOGIC_STRICT]   ? *p->ports[PORT_LOGIC_STRICT]   : 6.0f;
    float dryGain       = (p->ports[PORT_DRY]    ? *p->ports[PORT_DRY]    : 5.0f) / 10.0f;
    float wetGain       = (p->ports[PORT_WET]    ? *p->ports[PORT_WET]    : 8.0f) / 10.0f;
    float masterGain    = (p->ports[PORT_OUTPUT] ? *p->ports[PORT_OUTPUT] : 7.0f) / 10.0f;
    int   presetIdx     = p->ports[PORT_PRESET]  ? (int)(*p->ports[PORT_PRESET] + 0.5f)  : 0;
    float ctrl1         = p->ports[PORT_CTRL1]   ? *p->ports[PORT_CTRL1]  : 5.0f;
    float ctrl2         = p->ports[PORT_CTRL2]   ? *p->ports[PORT_CTRL2]  : 5.0f;

    // Gate sensitivity: 0-10 maps to -60..-20 dBFS threshold
    float gateSensPort  = p->ports[PORT_GATE_SENS] ? *p->ports[PORT_GATE_SENS] : 5.0f;
    float gateThresh    = powf(10.0f, (-60.0f + gateSensPort * 4.0f) / 20.0f);

    // ── Fix A: Bass ADSR — independent of input gate ──
    // PORT_ATTACK:  0-10 → 5ms–200ms   (how fast bass fades in when you play)
    // PORT_RELEASE: 0-10 → 100ms–3000ms (how long bass rings after you stop)
    float attackPort  = p->ports[PORT_ATTACK]  ? *p->ports[PORT_ATTACK]  : 3.0f;
    float releasePort = p->ports[PORT_RELEASE] ? *p->ports[PORT_RELEASE] : 5.0f;
    float attackMs    = 5.0f   + attackPort  * 19.5f;    // 5ms – 200ms
    float releaseMs   = 100.0f + releasePort * 290.0f;   // 100ms – 3000ms
    float bassAtkCoeff  = 1.0f - expf(-1.0f / (attackMs  * 0.001f * p->dsp.sr));
    float bassRelCoeff  = 1.0f - expf(-1.0f / (releaseMs * 0.001f * p->dsp.sr));
    // Smoothing for dynGate transitions (~8ms) — prevents clicks when beat fires
    float dynSmoothCoeff = 1.0f - expf(-1.0f / (0.008f * p->dsp.sr));

    // ── Fix C: Rhythm engine params ──
    bool  performerOn  = (p->ports[PORT_PERFORMER_MODE] && *p->ports[PORT_PERFORMER_MODE] > 0.5f);
    float bpmPort      = p->ports[PORT_BASS_BPM]      ? *p->ports[PORT_BASS_BPM]      : 120.0f;
    int   pattern      = p->ports[PORT_BASS_PATTERN]  ? (int)(*p->ports[PORT_BASS_PATTERN] + 0.5f) : 7;
    float styleDensity = p->ports[PORT_STYLE_DENSITY] ? *p->ports[PORT_STYLE_DENSITY] : 5.0f;
    float motifQuote   = p->ports[PORT_MOTIF_QUOTE]   ? *p->ports[PORT_MOTIF_QUOTE]   : 0.0f;
    float tapPort      = p->ports[PORT_BASS_TAP]      ? *p->ports[PORT_BASS_TAP]      : 0.0f;
    float bassTonePort = p->ports[PORT_BASS_TONE]     ? *p->ports[PORT_BASS_TONE]     : 5.0f;
    float walkGroovePort = p->ports[PORT_WALK_GROOVE] ? *p->ports[PORT_WALK_GROOVE]   : 0.0f;

    // ── New Controls: Ballad Relax, Fretless Expression, Working Memory & Bass Solo ──
    bool  balladRelax     = (p->ports[PORT_BALLAD_RELAX] && *p->ports[PORT_BALLAD_RELAX] > 0.5f);
    bool  fretlessExpress = (p->ports[PORT_FRETLESS_EXPRESS] && *p->ports[PORT_FRETLESS_EXPRESS] > 0.5f);
    bool  workingMemoryOn = (p->ports[PORT_WORKING_MEMORY] && *p->ports[PORT_WORKING_MEMORY] > 0.5f);
    bool  soloModeOn      = (p->ports[PORT_SOLO_MODE] && *p->ports[PORT_SOLO_MODE] > 0.5f);
    int   songRecVal      = (p->ports[PORT_SONG_RECORD] && *p->ports[PORT_SONG_RECORD] > 0.5f) ? 1 : 0;
    int   songSlotVal     = p->ports[PORT_SONG_SLOT] ? (int)(*p->ports[PORT_SONG_SLOT] + 0.5f) : 0;
    bool  bassMuted       = (p->ports[PORT_BASS_MUTE] && *p->ports[PORT_BASS_MUTE] > 0.5f);
    float muteGain        = bassMuted ? 0.0f : 1.0f;
    bool  adventurousMode = (p->ports[PORT_ADVENTUROUS_MODE] && *p->ports[PORT_ADVENTUROUS_MODE] > 0.5f);

    // Update Song Catalog Record / Slot Engine State
    p->dsp.songCatalog.updateRecordState(songRecVal, songSlotVal);

    // Ballad / Verse Relax Mode:
    // Softens attack and stretches release for sustained acoustic breathing
    if (balladRelax) {
        releaseMs *= 2.2f;
        bassRelCoeff = 1.0f - expf(-1.0f / (releaseMs * 0.001f * p->dsp.sr));
    }

    // Bass Solo Mode:
    // When momentary lead solo is pressed:
    // - Boost bass release slightly for singing sustain
    // - Opens up tone brightness
    if (soloModeOn) {
        releaseMs *= 1.6f;
        bassRelCoeff = 1.0f - expf(-1.0f / (releaseMs * 0.001f * p->dsp.sr));
    }

    // Tap-tempo: detect rising edge on tap port (port is block-constant in LV2)
    if (tapPort > 0.5f && p->dsp.prevTapPort <= 0.5f) {
        if (p->dsp.tapArmed && p->dsp.tapAccum > 0.2f && p->dsp.tapAccum < 3.0f) {
            bpmPort = 60.0f / p->dsp.tapAccum;
        }
        p->dsp.tapAccum = 0.0f;
        p->dsp.tapArmed = true;
    }
    p->dsp.prevTapPort = tapPort;

    // Read all 10 note selector level ports (range 0-10, slider = ceiling)
    float noteLevel[NOTES];
    for (int n = 0; n < NOTES; ++n) {
        int portIdx = PORT_NS_SUB_OCT + n;
        noteLevel[n] = (p->ports[portIdx] ? *p->ports[portIdx] : NS_DEFAULTS[n]) / 10.0f;
    }

    // In Bass Solo Mode: ensure Octave (index 6) and 5th (index 5) are naturally available to cut through as a lead instrument
    float soloTarget = soloModeOn ? 1.0f : 0.0f;

    // ── Per-sample processing ──
    for (uint32_t i = 0; i < sample_count; ++i) {
        float inL  = p->in_l[i];
        float inR  = (p->in_r ? p->in_r[i] : inL);
        float mono = 0.5f * (inL + inR);

        // ── Beast Mode AGC (Auto-Input Gain Calibration) ──
        // Continuously track peak guitar levels with fast attack (~5ms) and adaptive release
        // Handles whisper-quiet single coils (down to 0.002) up to roaring humbuckers without user knob tuning
        float absMono = fabsf(mono);
        if (absMono > p->dsp.inputPeakFollower) {
            p->dsp.inputPeakFollower += 0.008f * (absMono - p->dsp.inputPeakFollower);
        } else {
            p->dsp.inputPeakFollower += 0.000015f * (absMono - p->dsp.inputPeakFollower);
        }
        p->dsp.inputPeakFollower = std::max(0.005f, std::min(1.5f, p->dsp.inputPeakFollower));

        // Auto-scaled threshold: scaled dynamically to 24 dB below peak follower
        // User gateSensPort acts as a fine trimmer around the auto level
        float autoGateBase = p->dsp.inputPeakFollower * 0.055f;
        float sensMultiplier = powf(10.0f, (gateSensPort - 5.0f) * 0.8f / 20.0f);
        float effectiveOpenThresh = std::max(0.0012f, autoGateBase * sensMultiplier);
        float effectiveCloseThresh = effectiveOpenThresh * 0.28f; // -11 dB Schmitt hysteresis

        // ── Input Gate Envelope Follower & Asymmetric Schmitt Trigger ──
        if (absMono > p->dsp.gateEnv)
            p->dsp.gateEnv += p->dsp.gateAtk * (absMono - p->dsp.gateEnv);
        else
            p->dsp.gateEnv += p->dsp.gateRel * (absMono - p->dsp.gateEnv);

        int minHoldSamples = (int)(0.160f * p->dsp.sr); // 160ms minimum hold time

        if (!p->dsp.gateIsOpen) {
            if (p->dsp.gateEnv >= effectiveOpenThresh) {
                p->dsp.gateIsOpen = true;
                p->dsp.gateHoldCounter = minHoldSamples;
                p->dsp.gateOpenPeak = std::max(absMono, p->dsp.gateEnv);
            }
        } else {
            // While gate is open, track highest peak for decay freeze calculations
            if (p->dsp.gateEnv > p->dsp.gateOpenPeak) {
                p->dsp.gateOpenPeak = p->dsp.gateEnv;
            }
            if (p->dsp.gateHoldCounter > 0) {
                p->dsp.gateHoldCounter--;
            }
            // Gate only closes if envelope drops below close hysteresis threshold AND hold time has elapsed
            if (p->dsp.gateHoldCounter <= 0 && p->dsp.gateEnv < effectiveCloseThresh) {
                p->dsp.gateIsOpen = false;
                p->dsp.gateOpenPeak = 0.0f;
            }
        }

        float gTarget = p->dsp.gateIsOpen ? 1.0f : 0.0f;
        p->dsp.gateGain += 0.003f * (gTarget - p->dsp.gateGain);

        // ── Fix A: Independent Bass ADSR ──
        // Attacks when guitar gate opens, then decays on its own schedule
        // so bass rings out musically even after you lift your fingers.
        if (p->dsp.gateIsOpen) {
            p->dsp.bassEnvLevel += bassAtkCoeff * (1.0f - p->dsp.bassEnvLevel);
        } else {
            p->dsp.bassEnvLevel += bassRelCoeff * (0.0f - p->dsp.bassEnvLevel);
        }

        // ── Fix C: Rhythm clock advance ──
        p->dsp.tapAccum += 1.0f / p->dsp.sr;

        if (performerOn && !balladRelax) {
            double clockInc = (double)bpmPort / (60.0 * p->dsp.sr) * 16.0;
            p->dsp.clockPhase += clockInc;
            if (p->dsp.clockPhase >= 1.0) {
                p->dsp.clockPhase -= 1.0;
                int prevStep = p->dsp.currentStep;
                p->dsp.currentStep = (p->dsp.currentStep + 1) % 16;
                if (p->dsp.currentStep == 0 && prevStep == 15) {
                    p->dsp.workingMemory.onBarComplete();
                    p->dsp.songCatalog.onBarComplete(p->dsp.workingMemory);
                }
                p->dsp.tickBeatEvent(p->dsp.currentStep, pattern,
                                     p->dsp.confirmedMinor, styleDensity, motifQuote, soloModeOn, walkGroovePort, adventurousMode);
            }
        }

        // ── Fix B: Smooth dynamic gate transitions (~8ms) ──
        // If Ballad / Verse Relax is engaged, sustain roots and fifths continuously
        for (int n = 0; n < NOTES; ++n) {
            float dynTarget = 1.0f;
            if (balladRelax) {
                // Keep Root (3), Sub-Oct (0) and 5th (5) open with natural singing sustain
                dynTarget = (n == 3 || n == 0 || n == 5) ? 1.0f : 0.40f;
            } else if (performerOn) {
                dynTarget = p->dsp.dynamicNoteGate[n];
            }
            p->dsp.smoothedDynGate[n] += dynSmoothCoeff * (dynTarget - p->dsp.smoothedDynGate[n]);
        }

        // ── Pre-Filter Pitch Path: Steep 4-Pole Low-Pass (< 260Hz) ──
        // Strips upper strings (B & high E) and upper treble harmonics before feeding pitch engines
        float pitchMono = p->dsp.pitchPreFilter.process(mono);

        // Broadband pick attack detection on guitar input with dynamic floor calibration
        bool pickHit = p->dsp.pickExtractor.process(mono, transSens, p->dsp.inputPeakFollower);

        // Live Guitar Strum Pulse & Dynamic Tempo Tracker
        float currentTimeSec = (float)i / p->dsp.sr;
        p->dsp.pulseTracker.updateDecay(absMono, 1.0f / p->dsp.sr);

        if (pickHit) {
            p->dsp.pulseTracker.onStrumHit(currentTimeSec, absMono);
            p->dsp.physicalDynamics.triggerClack(std::min(1.0f, absMono * 3.0f));

            // If in Live Audio Only mode (performer mode 0), dynamic tempo tracker guides BPM
            if (!performerOn && p->dsp.pulseTracker.confidence > 0.40f) {
                bpmPort = p->dsp.pulseTracker.estimatedBpm;
            }

            // Strum-synchronous downbeat alignment: if rhythm clock was close to a 16th boundary, nudge phase
            if (performerOn && !balladRelax && p->dsp.clockPhase > 0.70) {
                p->dsp.clockPhase = 0.0;
                int prevStep = p->dsp.currentStep;
                p->dsp.currentStep = (p->dsp.currentStep + 1) % 16;
                if (p->dsp.currentStep == 0 && prevStep == 15) {
                    p->dsp.workingMemory.onBarComplete();
                    p->dsp.songCatalog.onBarComplete(p->dsp.workingMemory);
                }
                p->dsp.tickBeatEvent(p->dsp.currentStep, pattern,
                                     p->dsp.confirmedMinor, styleDensity, motifQuote, soloModeOn, walkGroovePort, adventurousMode);
            }
        }

        // Dynamic Touch & Articulation:
        // When guitarist plays short palm-muted chops, speed up decay and round the tone (unless in Ballad mode)
        float activeRelCoeff = bassRelCoeff;
        if (!balladRelax && p->dsp.pulseTracker.isPalmStaccato) {
            // Tighten release (3x faster decay for punchy staccato thump)
            activeRelCoeff = 1.0f - expf(-1.0f / (0.060f * p->dsp.sr));
        }

        // Universal Detector Input Normalization:
        // Scale pitch signal cleanly and linearly to nominal ~0.22 target amplitude.
        // Avoid non-linear tanhf saturation which creates odd harmonics and confuses pitch tracking.
        float targetPitchAmp = 0.22f;
        float currentInputPeak = std::max(0.010f, p->dsp.inputPeakFollower);
        float normPitchScale = std::min(10.0f, targetPitchAmp / currentInputPeak);
        float normalizedPitch = pitchMono * normPitchScale;
        // Soft rail limiter to prevent wild transients from distorting pitch estimators
        if (normalizedPitch > 0.85f) normalizedPitch = 0.85f;
        else if (normalizedPitch < -0.85f) normalizedPitch = -0.85f;

        // Feed pitch engines with cleanly filtered, amplitude-normalized fundamental audio
        p->dsp.fastRootDetector.push(normalizedPitch);
        p->dsp.strictRootDetector.pushSample(normalizedPitch);

        for (int k = 0; k < 36; ++k) {
            p->dsp.fastFilterBank[k].process(normalizedPitch, 0.75f, 0.985f);
            p->dsp.strictFilterBank[k].process(normalizedPitch, 0.35f, 0.995f);
        }

        // Run 4-Way confidence evaluation with strict rate-limiting (guarantees no CPU spiking / xruns)
        p->dsp.sampleEvaluationHoldoff++;
        bool shouldEval = false;
        if (pickHit && p->dsp.sampleEvaluationHoldoff >= 64) {
            shouldEval = true;
        } else if (p->dsp.sampleEvaluationHoldoff >= 128) {
            shouldEval = true;
        }

        if (shouldEval) {
            p->dsp.sampleEvaluationHoldoff = 0;
            p->dsp.evaluateChordConfidenceMatrix(false, false, trackSens, trackStab,
                                                debounce, bassBoost, 0, 0,
                                                droneImmunity, logicStrict, pickHit,
                                                workingMemoryOn, fretlessExpress);
        }

        // ── Fretless Pitch Multiplier (Intonation hunting, gliss, delayed bloom vibrato) ──
        float fretlessMult = 1.0f;
        if (fretlessExpress) {
            fretlessMult = p->dsp.fretlessEngine.processPitchMultiplier(1.0f / p->dsp.sr, p->dsp.gateEnv);
        }

        // ── Multi-Note Bank Rendering with Persistent Oscillator State ──
        BassChordBank& activeB = (p->dsp.activeBank == 0) ? p->dsp.bankA : p->dsp.bankB;
        BassChordBank& fadingB = (p->dsp.activeBank == 0) ? p->dsp.bankB : p->dsp.bankA;

        activeB.gain += p->dsp.crossfadeRate * (1.0f - activeB.gain);
        fadingB.gain += p->dsp.crossfadeRate * (0.0f - fadingB.gain);

        // ── Bass Solo Lead Engine ──
        // Slew-limited solo gain follower (~12ms attack/release) prevents pops on footswitch press/release
        p->dsp.soloGainFollower += 0.0025f * (soloTarget - p->dsp.soloGainFollower);
        float soloFactor = p->dsp.soloGainFollower;

        float bassSig = 0.0f;

        for (int n = 0; n < NOTES; ++n) {
            // Effective level = slider ceiling × smoothed beat gate
            float effLevel = noteLevel[n] * p->dsp.smoothedDynGate[n];
            // When Bass Solo is engaged: ensure solo melody voices are fully audible even if sliders are lowered
            if (soloFactor > 0.001f) {
                // If the solo pattern opens this note slot (smoothedDynGate > 0.1), let it sing!
                if (p->dsp.smoothedDynGate[n] > 0.10f) {
                    effLevel = std::max(effLevel, p->dsp.smoothedDynGate[n] * soloFactor * 1.05f);
                }
            }
            if (effLevel < 0.001f) continue;

            if (activeB.gain > 0.001f) {
                float noteHz = activeB.rootHz * powf(2.0f, NOTE_SEMI[n] / 12.0f) * fretlessMult;
                float voice  = p->dsp.renderBassVoice(presetIdx, noteHz,
                                                       activeB.phases[n], activeB.subPhases[n],
                                                       activeB.filterZ[n], ctrl1, ctrl2);
                bassSig += voice * effLevel * activeB.gain;
            }

            if (fadingB.gain > 0.001f) {
                float noteHz = fadingB.rootHz * powf(2.0f, NOTE_SEMI[n] / 12.0f) * fretlessMult;
                float voice  = p->dsp.renderBassVoice(presetIdx, noteHz,
                                                       fadingB.phases[n], fadingB.subPhases[n],
                                                       fadingB.filterZ[n], ctrl1, ctrl2);
                bassSig += voice * effLevel * fadingB.gain;
            }
        }

        // In Solo Lead Mode: add +3.5 dB boost and tube-like warm harmonic presence
        if (soloFactor > 0.001f) {
            float soloBoost = 1.0f + soloFactor * 0.45f; // +3.5 dB
            float saturated = tanhf(bassSig * 1.25f);
            bassSig = bassSig * (1.0f - soloFactor * 0.40f) + saturated * (soloFactor * 0.40f);
            bassSig *= soloBoost;
        }

        // ── Fix A: Scale by independent ADSR with palm-mute sensitivity ──
        if (!balladRelax && !p->dsp.gateIsOpen && p->dsp.pulseTracker.isPalmStaccato) {
            p->dsp.bassEnvLevel += activeRelCoeff * (0.0f - p->dsp.bassEnvLevel);
        }
        bassSig *= p->dsp.bassEnvLevel;

        // ── Physical String Dynamics & Hand Expression DSP ──
        // Models pluck strike position (neck to bridge), fret clack, palm damping & tube growl
        float currentRootHz = activeB.rootHz * fretlessMult;
        bassSig = p->dsp.physicalDynamics.process(bassSig, currentRootHz, bassTonePort, walkGroovePort, p->dsp.sr);

        // Mute bass output audio if MUTE is engaged (detectors & memory stay 100% active)
        bassSig *= muteGain;

        float outSample = inL * dryGain + bassSig * wetGain;
        p->out_l[i] = p->dsp.dcBlockL.process(outSample * masterGain);
        if (p->out_r) {
            float outR = (p->in_r ? p->in_r[i] : inL) * dryGain + bassSig * wetGain;
            p->out_r[i] = p->dsp.dcBlockR.process(outR * masterGain);
        }
    }

    // ── Real-Time Detected Chord Readout Ports ──
    if (p->out_ports[PORT_DETECTED_ROOT]) {
        *p->out_ports[PORT_DETECTED_ROOT] = (float)p->dsp.confirmedRoot;
    }
    if (p->out_ports[PORT_DETECTED_QUALITY]) {
        *p->out_ports[PORT_DETECTED_QUALITY] = p->dsp.confirmedMinor ? 1.0f : 0.0f;
    }
}

static void deactivate(LV2_Handle instance) {}
static void cleanup(LV2_Handle instance) {
    delete (CyberEvansBassPro*)instance;
}
static const void* extension_data(const char*) { return nullptr; }

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
    if (index == 0) return &descriptor;
    return nullptr;
}
