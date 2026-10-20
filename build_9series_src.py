import os

# Common header & DSP definitions (pitch detector, bass player, DC blocker, notch, etc.)
COMMON_HEADER = """
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
    0.0f, 0.0f, 0.0f, 8.0f, 7.0f, 6.0f, 5.0f, 0.0f, 0.0f, 0.0f
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

    inline float process(float x) {
        float y = b0 * (x - x2) - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;

        float absY = fabsf(y);
        if (absY > envelope) {
            envelope = envelope * 0.35f + absY * 0.65f;
        } else {
            envelope = envelope * 0.993f;
        }
        return envelope;
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
        int minLag = (int)(decSR / 900.0f);
        int maxLag = (int)(decSR / 65.0f);
        if (minLag < 12) minLag = 12;
        if (maxLag > 520) maxLag = 520;

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

// ── Auto Lowest-Note Rhythm Bass Player ──
struct BassRhythmEngine {
    double phase16 = 0.0;
    double phaseSub = 0.0;
    float  noteEnv = 0.0f;
    float  currentHz = 55.0f;
    float  targetHz = 55.0f;

    double clockPhase = 0.0;
    int    currentStep = 0;
    float  lastTapVal = 0.0f;
    uint32_t tapSampleTimer = 0;
    float  detectedBpm = 120.0f;
    OnePoleFilter lpFilter;

    static int getPatternStep(int pattern, int step, bool isMinor) {
        if (pattern <= 0) return 0;
        step = step % 16;
        switch (pattern) {
            case 1: return (step == 0) ? 1 : 99;
            case 2: return (step % 2 == 0) ? 1 : 0;
            case 3:
                if (step == 0 || step == 8) return 1;
                if (step == 6 || step == 14) return 3;
                if (step == 2 || step == 10) return 1;
                return 0;
            case 4:
                if (step == 0 || step == 8) return 1;
                if (step == 4 || step == 12) return 2;
                return 0;
            case 5:
                if (step == 0) return 1;
                if (step == 4) return isMinor ? 103 : 104;
                if (step == 8) return 2;
                if (step == 12) return isMinor ? 110 : 111;
                return 0;
            case 6:
                if (step == 4 || step == 10 || step == 12) return 1;
                return 0;
            default: return (step % 4 == 0) ? 1 : 0;
        }
    }

    void reset() {
        phase16 = phaseSub = 0.0;
        noteEnv = 0.0f;
        currentHz = targetHz = 55.0f;
        clockPhase = 0.0;
        currentStep = 0;
        lastTapVal = 0.0f;
        tapSampleTimer = 0;
        detectedBpm = 120.0f;
        lpFilter.reset();
    }

    inline float processSample(float sr, float bpmSetting, float tapTrigger,
                               int pattern, float level, float tone,
                               int rootPitchClass, bool isMinor, bool gateActive)
    {
        if (level < 0.01f || pattern <= 0 || rootPitchClass < 0 || !gateActive) {
            noteEnv *= 0.996f;
            if (noteEnv < 0.0001f) return 0.0f;
        }

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

        float effectiveBpm = bpmSetting;
        if (fabsf(bpmSetting - 120.0f) < 0.5f && detectedBpm > 40.0f && detectedBpm < 240.0f) {
            effectiveBpm = detectedBpm;
        }

        double stepsPerSec = (double)effectiveBpm * (4.0 / 60.0);
        clockPhase += stepsPerSec / (double)sr;
        if (clockPhase >= 1.0) {
            clockPhase -= 1.0;
            currentStep = (currentStep + 1) % 16;
            int action = getPatternStep(pattern, currentStep, isMinor);
            if (action == 1) {
                int midiBass = 24 + rootPitchClass;
                if (midiBass < 28) midiBass += 12;
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 1.0f;
            } else if (action == 2) {
                int midiBass = 24 + rootPitchClass + 7;
                if (midiBass > 55) midiBass -= 12;
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 0.95f;
            } else if (action == 3) {
                int midiBass = 24 + rootPitchClass + 12;
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 0.90f;
            } else if (action >= 100) {
                int semiOffset = action - 100;
                int midiBass = 24 + rootPitchClass + semiOffset;
                while (midiBass > 52) midiBass -= 12;
                targetHz = 440.0f * powf(2.0f, (midiBass - 69) / 12.0f);
                noteEnv = 0.90f;
            } else if (action == 99) {
            } else {
                if (pattern != 1) noteEnv *= 0.85f;
            }
        }

        currentHz += 0.005f * (targetHz - currentHz);
        float decayRate = (pattern == 1) ? 0.99995f : 0.9992f;
        noteEnv *= decayRate;

        phase16 += (double)currentHz * (M_TWOPI / (double)sr);
        if (phase16 >= M_TWOPI) phase16 -= M_TWOPI;
        phaseSub += (double)(currentHz * 0.5f) * (M_TWOPI / (double)sr);
        if (phaseSub >= M_TWOPI) phaseSub -= M_TWOPI;

        float s16 = sinf((float)phase16);
        float sSub = sinf((float)phaseSub);
        float rawBass = sSub * 0.65f + s16 * 0.45f + 0.08f * (s16 * s16 * s16);
        float lpCutoff = 90.0f + (tone / 10.0f) * 560.0f;
        float filteredBass = lpFilter.lp(rawBass, lpCutoff, sr);

        return filteredBass * noteEnv * (level / 10.0f);
    }
};
"""

print("Common components prepared!")

def write_mel9_source():
    mel_body = '''

#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-mel9"

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

    // MELLOTRON PARAMS
    PORT_PRESET      = 16,
    PORT_ATTACK      = 17,
    PORT_SUSTAIN     = 18,
    PORT_RELEASE     = 19,
    PORT_FLUTTER     = 20,
    PORT_WARMTH      = 21,
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

    // AUTO LOWEST-NOTE RHYTHM BASS PLAYER
    PORT_BASS_LEVEL   = 36,
    PORT_BASS_PATTERN = 37,
    PORT_BASS_BPM     = 38,
    PORT_BASS_TAP     = 39,
    PORT_BASS_TONE    = 40,

    PORT_COUNT        = 41
};

struct MelChordBank {
    double phases[NOTES][4];
    float  rootHz = 261.63f;
    bool   isMinor = false;
    float  gain = 0.0f;

    void init(float fRoot, bool bMinor, float initialGain) {
        memset(phases, 0, sizeof(phases));
        rootHz = fRoot;
        isMinor = bMinor;
        gain = initialGain;
    }
};

class CyberMel9DSP {
public:
    float sr = 44100.0f;

    NoteResonator filterBank[36];
    float chromaVector[12];
    float smoothedChroma[12];

    int   confirmedRoot = -1;
    int   candidateRoot = -1;
    int   candidateBlocks = 0;
    bool  confirmedMinor = false;
    int   blockSampleCounter = 0;

    MelChordBank bankA;
    MelChordBank bankB;
    int          activeBank = 0;
    float        crossfadeRate = 0.0003f;

    float noteLevelSm[NOTES];
    float agcPeak = 0.0f;
    float agcGain = 1.0f;

    NotchFilter dehum50, dehum60, dehum100, dehum120;
    OnePoleFilter gateScHp, gateScLp;
    float gateEnv = 0.0f;
    float gateGain = 0.0f;
    bool  gateIsOpen = false;
    float gateAtk = 0.0f, gateRel = 0.0f, gateSmoothRate = 0.0f;

    float inputRMS = 0.0f;
    float melEnv = 0.0f;

    float flutterBufferL[2048] = {};
    float flutterBufferR[2048] = {};
    int   flutterWrite = 0;
    double wowPhase = 0.0;
    double flutterPhase = 0.0;

    DCBlocker dcBlockL, dcBlockR;
    TimeDomainAutocorr timeDomainPitch;
    BassRhythmEngine   bassPlayer;
    float currentFilterQ = 7.0f;

    void init(float sampleRate) {
        sr = sampleRate;
        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            float freq = A4 * powf(2.0f, (midi - 69) / 12.0f);
            filterBank[i].setup(freq, sr, 8.0f);
        }

        memset(chromaVector, 0, sizeof(chromaVector));
        memset(smoothedChroma, 0, sizeof(smoothedChroma));
        memset(flutterBufferL, 0, sizeof(flutterBufferL));
        memset(flutterBufferR, 0, sizeof(flutterBufferR));

        bankA.init(261.63f, false, 1.0f);
        bankB.init(261.63f, false, 0.0f);
        activeBank = 0;
        confirmedRoot = 0;

        for (int i = 0; i < NOTES; i++) noteLevelSm[i] = NS_DEFAULTS[i] / 10.0f;

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

        bassPlayer.reset();
    }

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

        timeDomainPitch.analyze(sr, acClipVal);
        int autoCorrSemi = timeDomainPitch.detectedSemi;
        float autoCorrConf = timeDomainPitch.confidence;

        memset(chromaVector, 0, sizeof(chromaVector));
        float maxBass = 0.0f;
        int bassSemi = -1;
        float bassMult = 1.0f + (bassBoost / 10.0f) * 2.5f;
        float baseThresh = 0.015f / std::max(0.05f, trackSens * 0.2f);

        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            int semi = midi % 12;
            float env = filterBank[i].envelope;
            if (i < 12 && env > maxBass && env > baseThresh) {
                maxBass = env;
                bassSemi = semi;
            }
            chromaVector[semi] += env * (i < 12 ? bassMult : 1.0f);
        }

        if (autoCorrSemi >= 0 && autoCorrConf > 0.35f) {
            float acBoost = (acWeightVal / 10.0f) * autoCorrConf * 4.0f;
            chromaVector[autoCorrSemi] += acBoost;
            chromaVector[(autoCorrSemi + 7) % 12] += acBoost * 0.45f;
        }

        float maxChroma = 1e-9f;
        for (int c = 0; c < 12; ++c) {
            if (chromaVector[c] > maxChroma) maxChroma = chromaVector[c];
        }
        float smoothCoeff = std::max(0.05f, std::min(0.96f, 0.10f + (trackStab / 10.0f) * 0.43f));
        float newWeight = 1.0f - smoothCoeff;
        for (int c = 0; c < 12; ++c) {
            float norm = chromaVector[c] / maxChroma;
            smoothedChroma[c] = smoothedChroma[c] * smoothCoeff + norm * newWeight;
        }
        int keyBase = (keyRootVal > 0) ? (keyRootVal - 1) : -1;

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

        int bestRoot = -1;
        bool bestIsMinor = false;
        float bestScore = -1.0f;

        for (int r = 0; r < 12; ++r) {
            float cRoot = smoothedChroma[r];
            float c5th  = smoothedChroma[(r + 7) % 12];
            float cMaj3 = smoothedChroma[(r + 4) % 12];
            float cMin3 = smoothedChroma[(r + 3) % 12];
            float c4th  = smoothedChroma[(r + 5) % 12];

            float majScore = cRoot * 2.0f + c5th * 1.5f + cMaj3 * 1.3f + c4th * 0.4f;
            float minScore = cRoot * 2.0f + c5th * 1.5f + cMin3 * 1.3f + c4th * 0.4f;

            if (keyBase >= 0) {
                int interval = (r - keyBase + 12) % 12;
                if (scaleModeVal == 2) {
                    if (interval == 0 || interval == 5 || interval == 7) {
                        minScore *= 1.35f;
                    } else if (interval == 3 || interval == 8 || interval == 10) {
                        majScore *= 1.25f;
                    } else if (interval == 2) {
                        minScore *= 1.15f;
                    } else {
                        majScore *= 0.60f; minScore *= 0.60f;
                    }
                } else {
                    if (interval == 0 || interval == 5 || interval == 7) {
                        majScore *= 1.35f;
                    } else if (interval == 2 || interval == 4 || interval == 9) {
                        minScore *= 1.25f;
                    } else {
                        majScore *= 0.60f; minScore *= 0.60f;
                    }
                }
            }

            if (forceMajor) minScore = -1.0f;
            if (forceMinor) majScore = -1.0f;

            if (majScore > minScore) {
                if (majScore > bestScore) {
                    bestScore = majScore;
                    bestRoot = r;
                    bestIsMinor = false;
                }
            } else {
                if (minScore > bestScore) {
                    bestScore = minScore;
                    bestRoot = r;
                    bestIsMinor = true;
                }
            }
        }

        if (bassSemi >= 0 && smoothedChroma[bassSemi] > 0.03f) {
            if (bestRoot >= 0 && bestRoot != bassSemi) {
                int dist = (bestRoot - bassSemi + 12) % 12;
                if (dist == 4 || dist == 3 || dist == 7) {
                    bestRoot = bassSemi;
                }
            }
        }

        float scoreMinReq = 0.015f / std::max(0.1f, trackSens * 0.25f);
        int requiredBlocks = std::max(1, (int)(debounceCycles + 0.5f));

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
                candidateBlocks = 0;
            }
        }
    }

    inline float renderMellotronVoice(int preset, float noteFund, double phaseAcc[4]) {
        float harmWeights[9][4] = {
            { 0.70f, 0.45f, 0.35f, 0.20f }, // Orchestra
            { 0.85f, 0.55f, 0.25f, 0.10f }, // Cello
            { 0.65f, 0.50f, 0.40f, 0.30f }, // Strings
            { 0.95f, 0.15f, 0.05f, 0.00f }, // Flutes
            { 0.75f, 0.05f, 0.50f, 0.05f }, // Clarinet
            { 0.60f, 0.70f, 0.45f, 0.25f }, // Saxophone
            { 0.55f, 0.65f, 0.50f, 0.35f }, // Brass
            { 0.80f, 0.40f, 0.20f, 0.10f }, // Low Choir
            { 0.70f, 0.30f, 0.40f, 0.15f }  // High Choir
        };

        float out = 0.0f;
        for (int h = 0; h < 4; ++h) {
            float mult = (float)(h + 1);
            if (preset == 4 && h == 1) mult = 3.0f;
            float f = noteFund * mult;
            if (f > sr * 0.48f) continue;

            phaseAcc[h] += (double)f * (M_TWOPI / (double)sr);
            if (phaseAcc[h] >= M_TWOPI) phaseAcc[h] -= M_TWOPI;

            float s = sinf((float)phaseAcc[h]);
            s = s - 0.06f * s * s * s;
            out += s * harmWeights[preset][h];
        }
        return out;
    }

    void process(
        const float* inL, const float* inR,
        float* outL, float* outR,
        uint32_t nSamples,
        float bypass, float thirdMode,
        const float* nsLevels,
        int presetIdx, float atkVal, float susVal, float relVal,
        float flutterVal, float warmthVal, float gateSensVal,
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

        presetIdx = std::max(0, std::min(8, presetIdx));

        float atkSec = 0.01f + (atkVal / 10.0f) * 0.50f;
        float attCoeff = 1.0f - expf(-1.0f / (atkSec * sr));
        float sustainHold = susVal / 10.0f;

        float relSec = 0.05f + (relVal / 10.0f) * 2.95f;
        float relCoeff = 1.0f - expf(-1.0f / (relSec * sr));

        float xfadeSec = 0.15f + (relVal / 10.0f) * 0.55f;
        crossfadeRate = 1.0f / (xfadeSec * sr);

        float sensMult = std::max(0.1f, gateSensVal / 5.0f);
        float gateThreshOpen  = 0.0020f * sensMult;
        float gateThreshClose = 0.0006f * sensMult;

        float dryGain    = (dryVal / 10.0f);
        float wetGain    = (wetVal / 10.0f) * 1.15f;
        float masterGain = (outGainVal / 10.0f) * 1.25f;

        for (int i = 0; i < NOTES; i++) {
            float target = nsLevels[i] / 10.0f;
            noteLevelSm[i] += 0.002f * (target - noteLevelSm[i]);
        }

        float wowInc = (float)(M_TWOPI * 0.6 / sr);
        float flutInc = (float)(M_TWOPI * 4.5 / sr);
        float wowDepth = (flutterVal / 10.0f) * 0.0025f * sr;

        for (uint32_t i = 0; i < nSamples; ++i) {
            float sMono = 0.5f * (inL[i] + (inR ? inR[i] : inL[i]));
            float absMono = fabsf(sMono);

            timeDomainPitch.pushSample(sMono);

            if (agcSpeedVal > 0.05f) {
                float agcAttack = 1.0f - expf(-1.0f / (0.002f * sr));
                float agcRelease = 1.0f - expf(-1.0f / ((2.5f / std::max(0.1f, agcSpeedVal)) * sr));
                if (absMono > agcPeak) agcPeak += agcAttack * (absMono - agcPeak);
                else agcPeak += agcRelease * (absMono - agcPeak);

                float targetGain = 0.40f / std::max(0.005f, agcPeak);
                targetGain = std::max(0.2f, std::min(15.0f, targetGain));
                agcGain += 0.001f * (targetGain - agcGain);
            } else {
                agcGain = 1.0f;
            }

            float sLevel = sMono * agcGain;
            float sClean = dehum120.process(dehum100.process(dehum60.process(dehum50.process(sLevel))));

            float scSig = gateScLp.lp(gateScHp.hp(sMono, 75.0f, sr), 3500.0f, sr);
            float scAbs = fabsf(scSig);
            if (scAbs > gateEnv) gateEnv += gateAtk * (scAbs - gateEnv);
            else gateEnv += gateRel * (scAbs - gateEnv);

            if (!gateIsOpen && gateEnv > gateThreshOpen) gateIsOpen = true;
            else if (gateIsOpen && gateEnv < gateThreshClose) gateIsOpen = false;

            float targetGateGain = gateIsOpen ? 1.0f : 0.0f;
            gateGain += gateSmoothRate * (targetGateGain - gateGain);

            for (int r = 0; r < 36; ++r) {
                filterBank[r].process(sClean);
            }

            blockSampleCounter++;
            if (blockSampleCounter >= 256) {
                blockSampleCounter = 0;
                evaluateChord(forceMinor, forceMajor,
                              trackSens, trackStab, debounceVal, bassBoostVal,
                              keyRootVal, scaleModeVal, acWeightVal, acClipVal);
            }

            inputRMS += 0.001f * (absMono - inputRMS);
            float targetEnv = (gateIsOpen || gateGain > 0.05f) ? (0.35f * sustainHold + 0.65f * (inputRMS * 5.0f)) : 0.0f;
            targetEnv = std::min(1.0f, targetEnv * gateGain);

            if (targetEnv > melEnv) melEnv += attCoeff * (targetEnv - melEnv);
            else melEnv += relCoeff * (targetEnv - melEnv);

            if (activeBank == 1) {
                bankB.gain = std::min(1.0f, bankB.gain + crossfadeRate);
                bankA.gain = std::max(0.0f, bankA.gain - crossfadeRate);
            } else {
                bankA.gain = std::min(1.0f, bankA.gain + crossfadeRate);
                bankB.gain = std::max(0.0f, bankB.gain - crossfadeRate);
            }

            float mixA = 0.0f;
            int voicesA = 0;
            if (bankA.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesA++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankA.isMinor ? -3 : -4;
                    if (n == 4) semi = bankA.isMinor ?  3 :  4;
                    if (n == 7) semi = bankA.isMinor ? 15 : 16;

                    float noteFund = bankA.rootHz * powf(2.0f, semi / 12.0f);
                    mixA += renderMellotronVoice(presetIdx, noteFund, bankA.phases[n]) * nVol;
                }
                if (voicesA > 1) mixA /= sqrtf((float)voicesA);
            }

            float mixB = 0.0f;
            int voicesB = 0;
            if (bankB.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesB++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankB.isMinor ? -3 : -4;
                    if (n == 4) semi = bankB.isMinor ?  3 :  4;
                    if (n == 7) semi = bankB.isMinor ? 15 : 16;

                    float noteFund = bankB.rootHz * powf(2.0f, semi / 12.0f);
                    mixB += renderMellotronVoice(presetIdx, noteFund, bankB.phases[n]) * nVol;
                }
                if (voicesB > 1) mixB /= sqrtf((float)voicesB);
            }

            float blendA = sqrtf(bankA.gain);
            float blendB = sqrtf(bankB.gain);
            float melMix = (mixA * blendA + mixB * blendB) * melEnv;

            float drive = 1.0f + (warmthVal / 10.0f) * 2.0f;
            float satMel = tanhf(melMix * drive) / drive;

            wowPhase += wowInc;
            if (wowPhase >= M_TWOPI) wowPhase -= M_TWOPI;
            flutterPhase += flutInc;
            if (flutterPhase >= M_TWOPI) flutterPhase -= M_TWOPI;

            float tapeModL = (sinf((float)wowPhase) * 0.7f + sinf((float)flutterPhase) * 0.3f) * wowDepth + 40.0f;
            float tapeModR = (sinf((float)wowPhase + 1.2f) * 0.7f + sinf((float)flutterPhase + 1.2f) * 0.3f) * wowDepth + 40.0f;

            flutterBufferL[flutterWrite] = satMel;
            flutterBufferR[flutterWrite] = satMel;

            int readL = (flutterWrite - (int)tapeModL + 2048) % 2048;
            int readR = (flutterWrite - (int)tapeModR + 2048) % 2048;
            flutterWrite = (flutterWrite + 1) % 2048;

            float wetL = flutterBufferL[readL];
            float wetR = flutterBufferR[readR];

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

struct CyberMel9Plugin {
    CyberMel9DSP* dsp = nullptr;
    const float* audioIn[2] = {};
    float* audioOut[2] = {};
    const float* ports[PORT_COUNT] = {};
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const*) {
    CyberMel9Plugin* p = new CyberMel9Plugin();
    p->dsp = new CyberMel9DSP();
    p->dsp->init((float)rate);
    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    CyberMel9Plugin* p = (CyberMel9Plugin*)instance;
    if (port < PORT_COUNT) p->ports[port] = (const float*)data;
    if (port == PORT_IN_L) p->audioIn[0] = (const float*)data;
    if (port == PORT_IN_R) p->audioIn[1] = (const float*)data;
    if (port == PORT_OUT_L) p->audioOut[0] = (float*)data;
    if (port == PORT_OUT_R) p->audioOut[1] = (float*)data;
}

static void activate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t n_samples) {
    CyberMel9Plugin* p = (CyberMel9Plugin*)instance;
    if (!p || !p->dsp || !p->audioOut[0]) return;

    float ns[NOTES];
    for (int i = 0; i < NOTES; i++) {
        ns[i] = p->ports[PORT_NS_SUB_OCT + i] ? *p->ports[PORT_NS_SUB_OCT + i] : NS_DEFAULTS[i];
    }

    p->dsp->process(
        p->audioIn[0], p->audioIn[1],
        p->audioOut[0], p->audioOut[1],
        n_samples,
        p->ports[PORT_BYPASS]      ? *p->ports[PORT_BYPASS]      : 1.0f,
        p->ports[PORT_THIRD_MODE]  ? *p->ports[PORT_THIRD_MODE]  : 0.0f,
        ns,
        p->ports[PORT_PRESET]      ? (int)(*p->ports[PORT_PRESET] + 0.5f) : 0,
        p->ports[PORT_ATTACK]      ? *p->ports[PORT_ATTACK]      : 2.0f,
        p->ports[PORT_SUSTAIN]     ? *p->ports[PORT_SUSTAIN]     : 5.0f,
        p->ports[PORT_RELEASE]     ? *p->ports[PORT_RELEASE]     : 5.0f,
        p->ports[PORT_FLUTTER]     ? *p->ports[PORT_FLUTTER]     : 3.0f,
        p->ports[PORT_WARMTH]      ? *p->ports[PORT_WARMTH]      : 4.0f,
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
    CyberMel9Plugin* p = (CyberMel9Plugin*)instance;
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
'''
    with open('plugins/cyber-mel9.lv2/src/cyber_mel9_lv2.cpp', 'w', encoding='utf-8') as f:
        f.write(COMMON_HEADER + mel_body)
    print("cyber-mel9 source generated successfully!")

write_mel9_source()

# ── CYBER SYNTH 9 ───────────────────────────────────────────────────────────
def write_synth9_source():
    synth_body = '''
#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-synth9"

// Moog-style 4-pole (24 dB/oct) resonant lowpass ladder filter
struct MoogLadderFilter {
    float s[4] = {};
    void reset() { memset(s, 0, sizeof(s)); }

    inline float process(float in, float cutoffHz, float resonance, float sr) {
        cutoffHz = std::max(20.0f, std::min(cutoffHz, sr * 0.45f));
        float f = 2.0f * cutoffHz / sr;
        float k = 3.6f * f - 1.6f * f * f - 1.0f;
        float p = (k + 1.0f) * 0.5f;
        float scale = expf((1.0f - p) * 1.386249f);
        float r = resonance * scale * 0.95f;

        float x = in - r * s[3];
        x = tanhf(x); // Saturation in ladder feedback loop

        s[0] = s[0] + p * (x - s[0]);
        s[1] = s[1] + p * (s[0] - s[1]);
        s[2] = s[2] + p * (s[1] - s[2]);
        s[3] = s[3] + p * (s[2] - s[3]);
        return s[3];
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

    // SYNTH PARAMS
    PORT_PRESET      = 16,
    PORT_CUTOFF      = 17,
    PORT_RESONANCE   = 18,
    PORT_ENV_MOD     = 19,
    PORT_ATTACK      = 20,
    PORT_SUSTAIN     = 21,
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

    // AUTO LOWEST-NOTE RHYTHM BASS PLAYER
    PORT_BASS_LEVEL   = 36,
    PORT_BASS_PATTERN = 37,
    PORT_BASS_BPM     = 38,
    PORT_BASS_TAP     = 39,
    PORT_BASS_TONE    = 40,

    PORT_COUNT        = 41
};

struct SynthChordBank {
    double phases[NOTES];
    double subPhases[NOTES];
    float  rootHz = 261.63f;
    bool   isMinor = false;
    float  gain = 0.0f;

    void init(float fRoot, bool bMinor, float initialGain) {
        memset(phases, 0, sizeof(phases));
        memset(subPhases, 0, sizeof(subPhases));
        rootHz = fRoot;
        isMinor = bMinor;
        gain = initialGain;
    }
};

class CyberSynth9DSP {
public:
    float sr = 44100.0f;

    NoteResonator filterBank[36];
    float chromaVector[12];
    float smoothedChroma[12];

    int   confirmedRoot = -1;
    int   candidateRoot = -1;
    int   candidateBlocks = 0;
    bool  confirmedMinor = false;
    int   blockSampleCounter = 0;

    SynthChordBank bankA;
    SynthChordBank bankB;
    int            activeBank = 0;
    float          crossfadeRate = 0.0003f;

    float noteLevelSm[NOTES];
    float agcPeak = 0.0f;
    float agcGain = 1.0f;

    NotchFilter dehum50, dehum60, dehum100, dehum120;
    OnePoleFilter gateScHp, gateScLp;
    float gateEnv = 0.0f;
    float gateGain = 0.0f;
    bool  gateIsOpen = false;
    float gateAtk = 0.0f, gateRel = 0.0f, gateSmoothRate = 0.0f;

    float inputRMS = 0.0f;
    float synthEnv = 0.0f;
    float filterEnv = 0.0f;

    MoogLadderFilter ladderL;
    MoogLadderFilter ladderR;

    DCBlocker dcBlockL, dcBlockR;
    TimeDomainAutocorr timeDomainPitch;
    BassRhythmEngine   bassPlayer;
    float currentFilterQ = 7.0f;

    void init(float sampleRate) {
        sr = sampleRate;
        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            float freq = A4 * powf(2.0f, (midi - 69) / 12.0f);
            filterBank[i].setup(freq, sr, 8.0f);
        }

        memset(chromaVector, 0, sizeof(chromaVector));
        memset(smoothedChroma, 0, sizeof(smoothedChroma));

        bankA.init(261.63f, false, 1.0f);
        bankB.init(261.63f, false, 0.0f);
        activeBank = 0;
        confirmedRoot = 0;

        for (int i = 0; i < NOTES; i++) noteLevelSm[i] = NS_DEFAULTS[i] / 10.0f;

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

        ladderL.reset();
        ladderR.reset();
        bassPlayer.reset();
    }

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

        timeDomainPitch.analyze(sr, acClipVal);
        int autoCorrSemi = timeDomainPitch.detectedSemi;
        float autoCorrConf = timeDomainPitch.confidence;

        memset(chromaVector, 0, sizeof(chromaVector));
        float maxBass = 0.0f;
        int bassSemi = -1;
        float bassMult = 1.0f + (bassBoost / 10.0f) * 2.5f;
        float baseThresh = 0.015f / std::max(0.05f, trackSens * 0.2f);

        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            int semi = midi % 12;
            float env = filterBank[i].envelope;
            if (i < 12 && env > maxBass && env > baseThresh) {
                maxBass = env;
                bassSemi = semi;
            }
            chromaVector[semi] += env * (i < 12 ? bassMult : 1.0f);
        }

        if (autoCorrSemi >= 0 && autoCorrConf > 0.35f) {
            float acBoost = (acWeightVal / 10.0f) * autoCorrConf * 4.0f;
            chromaVector[autoCorrSemi] += acBoost;
            chromaVector[(autoCorrSemi + 7) % 12] += acBoost * 0.45f;
        }

        float maxChroma = 1e-9f;
        for (int c = 0; c < 12; ++c) {
            if (chromaVector[c] > maxChroma) maxChroma = chromaVector[c];
        }
        float smoothCoeff = std::max(0.05f, std::min(0.96f, 0.10f + (trackStab / 10.0f) * 0.43f));
        float newWeight = 1.0f - smoothCoeff;
        for (int c = 0; c < 12; ++c) {
            float norm = chromaVector[c] / maxChroma;
            smoothedChroma[c] = smoothedChroma[c] * smoothCoeff + norm * newWeight;
        }
        int keyBase = (keyRootVal > 0) ? (keyRootVal - 1) : -1;

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

        int bestRoot = -1;
        bool bestIsMinor = false;
        float bestScore = -1.0f;

        for (int r = 0; r < 12; ++r) {
            float cRoot = smoothedChroma[r];
            float c5th  = smoothedChroma[(r + 7) % 12];
            float cMaj3 = smoothedChroma[(r + 4) % 12];
            float cMin3 = smoothedChroma[(r + 3) % 12];
            float c4th  = smoothedChroma[(r + 5) % 12];

            float majScore = cRoot * 2.0f + c5th * 1.5f + cMaj3 * 1.3f + c4th * 0.4f;
            float minScore = cRoot * 2.0f + c5th * 1.5f + cMin3 * 1.3f + c4th * 0.4f;

            if (keyBase >= 0) {
                int interval = (r - keyBase + 12) % 12;
                if (scaleModeVal == 2) {
                    if (interval == 0 || interval == 5 || interval == 7) {
                        minScore *= 1.35f;
                    } else if (interval == 3 || interval == 8 || interval == 10) {
                        majScore *= 1.25f;
                    } else if (interval == 2) {
                        minScore *= 1.15f;
                    } else {
                        majScore *= 0.60f; minScore *= 0.60f;
                    }
                } else {
                    if (interval == 0 || interval == 5 || interval == 7) {
                        majScore *= 1.35f;
                    } else if (interval == 2 || interval == 4 || interval == 9) {
                        minScore *= 1.25f;
                    } else {
                        majScore *= 0.60f; minScore *= 0.60f;
                    }
                }
            }

            if (forceMajor) minScore = -1.0f;
            if (forceMinor) majScore = -1.0f;

            if (majScore > minScore) {
                if (majScore > bestScore) {
                    bestScore = majScore;
                    bestRoot = r;
                    bestIsMinor = false;
                }
            } else {
                if (minScore > bestScore) {
                    bestScore = minScore;
                    bestRoot = r;
                    bestIsMinor = true;
                }
            }
        }

        if (bassSemi >= 0 && smoothedChroma[bassSemi] > 0.03f) {
            if (bestRoot >= 0 && bestRoot != bassSemi) {
                int dist = (bestRoot - bassSemi + 12) % 12;
                if (dist == 4 || dist == 3 || dist == 7) {
                    bestRoot = bassSemi;
                }
            }
        }

        float scoreMinReq = 0.015f / std::max(0.1f, trackSens * 0.25f);
        int requiredBlocks = std::max(1, (int)(debounceCycles + 0.5f));

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
                filterEnv = 1.0f; // Trigger synth filter envelope sweep on chord change
                candidateBlocks = 0;
            }
        }
    }

    // Classic Analog Waveform Generator (Saw, Pulse/Square, Sync, Sub)
    // Presets: 0=OBX, 1=Profit 5, 2=Moog Lead, 3=Moodys, 4=String Synth, 5=Poly VI, 6=Mini Mood, 7=Laser, 8=Micro
    inline float renderSynthOsc(int preset, float freq, double& p, double& pSub) {
        if (freq > sr * 0.48f) return 0.0f;
        p += (double)freq * (M_TWOPI / (double)sr);
        if (p >= M_TWOPI) p -= M_TWOPI;

        pSub += (double)(freq * 0.5f) * (M_TWOPI / (double)sr);
        if (pSub >= M_TWOPI) pSub -= M_TWOPI;

        float normP = (float)(p / M_TWOPI);
        float saw = 2.0f * normP - 1.0f;
        float sqr = (normP > 0.5f) ? 1.0f : -1.0f;
        float pulse = (normP > 0.30f) ? 1.0f : -1.0f;
        float sub = sinf((float)pSub);

        switch (preset) {
            case 0: return saw * 0.70f + sqr * 0.30f;                  // OBX fat dual
            case 1: return pulse * 0.65f + saw * 0.35f;                // Profit 5 brassy pulse
            case 2: return saw * 0.60f + sqr * 0.30f + sub * 0.35f;    // Moog Lead with heavy sub
            case 3: return saw * 0.85f + 0.15f * sinf(3.0f * (float)p);// Moodys bright sweep
            case 4: return saw * 0.90f + 0.10f * sqr;                  // String Synth raw saw
            case 5: return pulse * 0.75f + sub * 0.25f;                // Poly VI lush pulse
            case 6: return sqr * 0.60f + sub * 0.50f;                  // Mini Mood square bass
            case 7: return sinf((float)p * 2.0f) * 0.8f + saw * 0.3f;  // Laser sync FM
            case 8: return sqr * 0.80f + saw * 0.20f;                  // Micro punchy lead
            default: return saw;
        }
    }

    void process(
        const float* inL, const float* inR,
        float* outL, float* outR,
        uint32_t nSamples,
        float bypass, float thirdMode,
        const float* nsLevels,
        int presetIdx, float cutoffVal, float resVal, float envModVal,
        float atkVal, float susVal, float gateSensVal,
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

        presetIdx = std::max(0, std::min(8, presetIdx));

        float atkSec = 0.005f + (atkVal / 10.0f) * 0.45f;
        float attCoeff = 1.0f - expf(-1.0f / (atkSec * sr));
        float sustainHold = susVal / 10.0f;
        float relCoeff = 1.0f - expf(-1.0f / (0.35f * sr));

        crossfadeRate = 1.0f / (0.18f * sr); // Snappy synth transition

        float sensMult = std::max(0.1f, gateSensVal / 5.0f);
        float gateThreshOpen  = 0.0020f * sensMult;
        float gateThreshClose = 0.0006f * sensMult;

        float dryGain    = (dryVal / 10.0f);
        float wetGain    = (wetVal / 10.0f) * 1.10f;
        float masterGain = (outGainVal / 10.0f) * 1.25f;

        for (int i = 0; i < NOTES; i++) {
            float target = nsLevels[i] / 10.0f;
            noteLevelSm[i] += 0.002f * (target - noteLevelSm[i]);
        }

        // Base cutoff: 200 Hz to 12 kHz
        float baseCutoff = 200.0f * powf(60.0f, cutoffVal / 10.0f);
        float ladderRes = (resVal / 10.0f) * 0.90f;
        float envModAmt = (envModVal / 10.0f) * 6000.0f;

        for (uint32_t i = 0; i < nSamples; ++i) {
            float sMono = 0.5f * (inL[i] + (inR ? inR[i] : inL[i]));
            float absMono = fabsf(sMono);

            timeDomainPitch.pushSample(sMono);

            if (agcSpeedVal > 0.05f) {
                float agcAttack = 1.0f - expf(-1.0f / (0.002f * sr));
                float agcRelease = 1.0f - expf(-1.0f / ((2.5f / std::max(0.1f, agcSpeedVal)) * sr));
                if (absMono > agcPeak) agcPeak += agcAttack * (absMono - agcPeak);
                else agcPeak += agcRelease * (absMono - agcPeak);

                float targetGain = 0.40f / std::max(0.005f, agcPeak);
                targetGain = std::max(0.2f, std::min(15.0f, targetGain));
                agcGain += 0.001f * (targetGain - agcGain);
            } else {
                agcGain = 1.0f;
            }

            float sLevel = sMono * agcGain;
            float sClean = dehum120.process(dehum100.process(dehum60.process(dehum50.process(sLevel))));

            float scSig = gateScLp.lp(gateScHp.hp(sMono, 75.0f, sr), 3500.0f, sr);
            float scAbs = fabsf(scSig);
            if (scAbs > gateEnv) gateEnv += gateAtk * (scAbs - gateEnv);
            else gateEnv += gateRel * (scAbs - gateEnv);

            if (!gateIsOpen && gateEnv > gateThreshOpen) gateIsOpen = true;
            else if (gateIsOpen && gateEnv < gateThreshClose) gateIsOpen = false;

            float targetGateGain = gateIsOpen ? 1.0f : 0.0f;
            gateGain += gateSmoothRate * (targetGateGain - gateGain);

            for (int r = 0; r < 36; ++r) {
                filterBank[r].process(sClean);
            }

            blockSampleCounter++;
            if (blockSampleCounter >= 256) {
                blockSampleCounter = 0;
                evaluateChord(forceMinor, forceMajor,
                              trackSens, trackStab, debounceVal, bassBoostVal,
                              keyRootVal, scaleModeVal, acWeightVal, acClipVal);
            }

            inputRMS += 0.001f * (absMono - inputRMS);
            float targetEnv = (gateIsOpen || gateGain > 0.05f) ? (0.40f * sustainHold + 0.60f * (inputRMS * 5.0f)) : 0.0f;
            targetEnv = std::min(1.0f, targetEnv * gateGain);

            if (targetEnv > synthEnv) synthEnv += attCoeff * (targetEnv - synthEnv);
            else synthEnv += relCoeff * (targetEnv - synthEnv);

            filterEnv *= 0.9994f; // Filter envelope decay

            if (activeBank == 1) {
                bankB.gain = std::min(1.0f, bankB.gain + crossfadeRate);
                bankA.gain = std::max(0.0f, bankA.gain - crossfadeRate);
            } else {
                bankA.gain = std::min(1.0f, bankA.gain + crossfadeRate);
                bankB.gain = std::max(0.0f, bankB.gain - crossfadeRate);
            }

            float mixA = 0.0f;
            int voicesA = 0;
            if (bankA.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesA++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankA.isMinor ? -3 : -4;
                    if (n == 4) semi = bankA.isMinor ?  3 :  4;
                    if (n == 7) semi = bankA.isMinor ? 15 : 16;

                    float noteFund = bankA.rootHz * powf(2.0f, semi / 12.0f);
                    mixA += renderSynthOsc(presetIdx, noteFund, bankA.phases[n], bankA.subPhases[n]) * nVol;
                }
                if (voicesA > 1) mixA /= sqrtf((float)voicesA);
            }

            float mixB = 0.0f;
            int voicesB = 0;
            if (bankB.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesB++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankB.isMinor ? -3 : -4;
                    if (n == 4) semi = bankB.isMinor ?  3 :  4;
                    if (n == 7) semi = bankB.isMinor ? 15 : 16;

                    float noteFund = bankB.rootHz * powf(2.0f, semi / 12.0f);
                    mixB += renderSynthOsc(presetIdx, noteFund, bankB.phases[n], bankB.subPhases[n]) * nVol;
                }
                if (voicesB > 1) mixB /= sqrtf((float)voicesB);
            }

            float blendA = sqrtf(bankA.gain);
            float blendB = sqrtf(bankB.gain);
            float rawSynth = (mixA * blendA + mixB * blendB) * synthEnv;

            // Apply 4-pole Moog Ladder Filter with envelope sweep
            float dynamicCutoff = baseCutoff + filterEnv * envModAmt;
            float filteredL = ladderL.process(rawSynth, dynamicCutoff, ladderRes, sr);
            float filteredR = ladderR.process(rawSynth, dynamicCutoff * 1.02f, ladderRes, sr);

            // Auto Lowest-Note Rhythm Bass Player
            float bassSample = bassPlayer.processSample(
                sr, bassBpmVal, bassTapVal,
                (int)(bassPatternVal + 0.5f), bassLevelVal, bassToneVal,
                confirmedRoot, confirmedMinor, (gateIsOpen || gateGain > 0.05f)
            );

            float wetL = dcBlockL.process(filteredL + bassSample);
            float wetR = dcBlockR.process(filteredR + bassSample);

            float dryL = inL[i] * dryGain;
            float dryR = (inR ? inR[i] : inL[i]) * dryGain;

            outL[i] = (dryL + wetL * wetGain) * masterGain;
            if (outR) {
                outR[i] = (dryR + wetR * wetGain) * masterGain;
            }
        }
    }
};

struct CyberSynth9Plugin {
    CyberSynth9DSP* dsp = nullptr;
    const float* audioIn[2] = {};
    float* audioOut[2] = {};
    const float* ports[PORT_COUNT] = {};
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const*) {
    CyberSynth9Plugin* p = new CyberSynth9Plugin();
    p->dsp = new CyberSynth9DSP();
    p->dsp->init((float)rate);
    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    CyberSynth9Plugin* p = (CyberSynth9Plugin*)instance;
    if (port < PORT_COUNT) p->ports[port] = (const float*)data;
    if (port == PORT_IN_L) p->audioIn[0] = (const float*)data;
    if (port == PORT_IN_R) p->audioIn[1] = (const float*)data;
    if (port == PORT_OUT_L) p->audioOut[0] = (float*)data;
    if (port == PORT_OUT_R) p->audioOut[1] = (float*)data;
}

static void activate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t n_samples) {
    CyberSynth9Plugin* p = (CyberSynth9Plugin*)instance;
    if (!p || !p->dsp || !p->audioOut[0]) return;

    float ns[NOTES];
    for (int i = 0; i < NOTES; i++) {
        ns[i] = p->ports[PORT_NS_SUB_OCT + i] ? *p->ports[PORT_NS_SUB_OCT + i] : NS_DEFAULTS[i];
    }

    p->dsp->process(
        p->audioIn[0], p->audioIn[1],
        p->audioOut[0], p->audioOut[1],
        n_samples,
        p->ports[PORT_BYPASS]      ? *p->ports[PORT_BYPASS]      : 1.0f,
        p->ports[PORT_THIRD_MODE]  ? *p->ports[PORT_THIRD_MODE]  : 0.0f,
        ns,
        p->ports[PORT_PRESET]      ? (int)(*p->ports[PORT_PRESET] + 0.5f) : 0,
        p->ports[PORT_CUTOFF]      ? *p->ports[PORT_CUTOFF]      : 5.0f,
        p->ports[PORT_RESONANCE]   ? *p->ports[PORT_RESONANCE]   : 4.0f,
        p->ports[PORT_ENV_MOD]     ? *p->ports[PORT_ENV_MOD]     : 5.0f,
        p->ports[PORT_ATTACK]      ? *p->ports[PORT_ATTACK]      : 2.0f,
        p->ports[PORT_SUSTAIN]     ? *p->ports[PORT_SUSTAIN]     : 5.0f,
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
    CyberSynth9Plugin* p = (CyberSynth9Plugin*)instance;
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
'''
    with open('plugins/cyber-synth9.lv2/src/cyber_synth9_lv2.cpp', 'w', encoding='utf-8') as f:
        f.write(COMMON_HEADER + synth_body)
    print("cyber-synth9 source generated successfully!")

write_synth9_source()

# ── CYBER STRING 9 ──────────────────────────────────────────────────────────
def write_string9_source():
    string_body = '''
#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-string9"

// Solina Triple BBD Bucket-Brigade Ensemble Chorus
// 2 LFOs: Slow (0.6 Hz) + Fast (6.0 Hz), 3 delay lines 120 degrees out of phase
struct SolinaEnsembleChorus {
    static constexpr int BBD_LEN = 2048;
    float delayBuffer[3][BBD_LEN] = {};
    int   writePos = 0;
    double lfoSlowPhase = 0.0;
    double lfoFastPhase = 0.0;

    void reset() {
        memset(delayBuffer, 0, sizeof(delayBuffer));
        writePos = 0;
        lfoSlowPhase = 0.0;
        lfoFastPhase = 0.0;
    }

    inline void process(float inL, float inR, float& outL, float& outR, float depth, float sr) {
        double slowInc = M_TWOPI * 0.60 / (double)sr;
        double fastInc = M_TWOPI * 5.90 / (double)sr;

        lfoSlowPhase += slowInc;
        if (lfoSlowPhase >= M_TWOPI) lfoSlowPhase -= M_TWOPI;

        lfoFastPhase += fastInc;
        if (lfoFastPhase >= M_TWOPI) lfoFastPhase -= M_TWOPI;

        float monoIn = 0.5f * (inL + inR);
        delayBuffer[0][writePos] = monoIn;
        delayBuffer[1][writePos] = monoIn;
        delayBuffer[2][writePos] = monoIn;

        float baseDelay = 0.007f * sr; // ~7ms baseline BBD delay
        float modDepth  = (depth / 10.0f) * 0.0025f * sr; // up to ~2.5ms excursion

        // 3 BBD lines 120 degrees (2*PI/3) apart
        float mod0 = (sinf((float)lfoSlowPhase) * 0.75f + sinf((float)lfoFastPhase) * 0.25f) * modDepth;
        float mod1 = (sinf((float)lfoSlowPhase + 2.094395f) * 0.75f + sinf((float)lfoFastPhase + 2.094395f) * 0.25f) * modDepth;
        float mod2 = (sinf((float)lfoSlowPhase + 4.188790f) * 0.75f + sinf((float)lfoFastPhase + 4.188790f) * 0.25f) * modDepth;

        int r0 = (writePos - (int)(baseDelay + mod0) + BBD_LEN) % BBD_LEN;
        int r1 = (writePos - (int)(baseDelay + mod1) + BBD_LEN) % BBD_LEN;
        int r2 = (writePos - (int)(baseDelay + mod2) + BBD_LEN) % BBD_LEN;

        writePos = (writePos + 1) % BBD_LEN;

        float bbd0 = delayBuffer[0][r0];
        float bbd1 = delayBuffer[1][r1];
        float bbd2 = delayBuffer[2][r2];

        // Solina stereo matrix: Left = BBD0 + 0.5*BBD1, Right = BBD2 + 0.5*BBD1
        outL = bbd0 * 0.7f + bbd1 * 0.4f;
        outR = bbd2 * 0.7f + bbd1 * 0.4f;
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

    // STRING PARAMS
    PORT_PRESET      = 16,
    PORT_ENSEMBLE    = 17,
    PORT_BRIGHTNESS  = 18,
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

    // AUTO LOWEST-NOTE RHYTHM BASS PLAYER
    PORT_BASS_LEVEL   = 36,
    PORT_BASS_PATTERN = 37,
    PORT_BASS_BPM     = 38,
    PORT_BASS_TAP     = 39,
    PORT_BASS_TONE    = 40,

    PORT_COUNT        = 41
};

struct StringChordBank {
    double phases[NOTES];
    double phasesOct[NOTES];
    float  rootHz = 261.63f;
    bool   isMinor = false;
    float  gain = 0.0f;

    void init(float fRoot, bool bMinor, float initialGain) {
        memset(phases, 0, sizeof(phases));
        memset(phasesOct, 0, sizeof(phasesOct));
        rootHz = fRoot;
        isMinor = bMinor;
        gain = initialGain;
    }
};

class CyberString9DSP {
public:
    float sr = 44100.0f;

    NoteResonator filterBank[36];
    float chromaVector[12];
    float smoothedChroma[12];

    int   confirmedRoot = -1;
    int   candidateRoot = -1;
    int   candidateBlocks = 0;
    bool  confirmedMinor = false;
    int   blockSampleCounter = 0;

    StringChordBank bankA;
    StringChordBank bankB;
    int             activeBank = 0;
    float           crossfadeRate = 0.0003f;

    float noteLevelSm[NOTES];
    float agcPeak = 0.0f;
    float agcGain = 1.0f;

    NotchFilter dehum50, dehum60, dehum100, dehum120;
    OnePoleFilter gateScHp, gateScLp;
    float gateEnv = 0.0f;
    float gateGain = 0.0f;
    bool  gateIsOpen = false;
    float gateAtk = 0.0f, gateRel = 0.0f, gateSmoothRate = 0.0f;

    float inputRMS = 0.0f;
    float stringEnv = 0.0f;

    SolinaEnsembleChorus ensemble;
    OnePoleFilter toneFilterL;
    OnePoleFilter toneFilterR;

    DCBlocker dcBlockL, dcBlockR;
    TimeDomainAutocorr timeDomainPitch;
    BassRhythmEngine   bassPlayer;
    float currentFilterQ = 7.0f;

    void init(float sampleRate) {
        sr = sampleRate;
        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            float freq = A4 * powf(2.0f, (midi - 69) / 12.0f);
            filterBank[i].setup(freq, sr, 8.0f);
        }

        memset(chromaVector, 0, sizeof(chromaVector));
        memset(smoothedChroma, 0, sizeof(smoothedChroma));

        bankA.init(261.63f, false, 1.0f);
        bankB.init(261.63f, false, 0.0f);
        activeBank = 0;
        confirmedRoot = 0;

        for (int i = 0; i < NOTES; i++) noteLevelSm[i] = NS_DEFAULTS[i] / 10.0f;

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

        ensemble.reset();
        bassPlayer.reset();
    }

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

        timeDomainPitch.analyze(sr, acClipVal);
        int autoCorrSemi = timeDomainPitch.detectedSemi;
        float autoCorrConf = timeDomainPitch.confidence;

        memset(chromaVector, 0, sizeof(chromaVector));
        float maxBass = 0.0f;
        int bassSemi = -1;
        float bassMult = 1.0f + (bassBoost / 10.0f) * 2.5f;
        float baseThresh = 0.015f / std::max(0.05f, trackSens * 0.2f);

        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            int semi = midi % 12;
            float env = filterBank[i].envelope;
            if (i < 12 && env > maxBass && env > baseThresh) {
                maxBass = env;
                bassSemi = semi;
            }
            chromaVector[semi] += env * (i < 12 ? bassMult : 1.0f);
        }

        if (autoCorrSemi >= 0 && autoCorrConf > 0.35f) {
            float acBoost = (acWeightVal / 10.0f) * autoCorrConf * 4.0f;
            chromaVector[autoCorrSemi] += acBoost;
            chromaVector[(autoCorrSemi + 7) % 12] += acBoost * 0.45f;
        }

        float maxChroma = 1e-9f;
        for (int c = 0; c < 12; ++c) {
            if (chromaVector[c] > maxChroma) maxChroma = chromaVector[c];
        }
        float smoothCoeff = std::max(0.05f, std::min(0.96f, 0.10f + (trackStab / 10.0f) * 0.43f));
        float newWeight = 1.0f - smoothCoeff;
        for (int c = 0; c < 12; ++c) {
            float norm = chromaVector[c] / maxChroma;
            smoothedChroma[c] = smoothedChroma[c] * smoothCoeff + norm * newWeight;
        }
        int keyBase = (keyRootVal > 0) ? (keyRootVal - 1) : -1;

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

        int bestRoot = -1;
        bool bestIsMinor = false;
        float bestScore = -1.0f;

        for (int r = 0; r < 12; ++r) {
            float cRoot = smoothedChroma[r];
            float c5th  = smoothedChroma[(r + 7) % 12];
            float cMaj3 = smoothedChroma[(r + 4) % 12];
            float cMin3 = smoothedChroma[(r + 3) % 12];
            float c4th  = smoothedChroma[(r + 5) % 12];

            float majScore = cRoot * 2.0f + c5th * 1.5f + cMaj3 * 1.3f + c4th * 0.4f;
            float minScore = cRoot * 2.0f + c5th * 1.5f + cMin3 * 1.3f + c4th * 0.4f;

            if (keyBase >= 0) {
                int interval = (r - keyBase + 12) % 12;
                if (scaleModeVal == 2) {
                    if (interval == 0 || interval == 5 || interval == 7) {
                        minScore *= 1.35f;
                    } else if (interval == 3 || interval == 8 || interval == 10) {
                        majScore *= 1.25f;
                    } else if (interval == 2) {
                        minScore *= 1.15f;
                    } else {
                        majScore *= 0.60f; minScore *= 0.60f;
                    }
                } else {
                    if (interval == 0 || interval == 5 || interval == 7) {
                        majScore *= 1.35f;
                    } else if (interval == 2 || interval == 4 || interval == 9) {
                        minScore *= 1.25f;
                    } else {
                        majScore *= 0.60f; minScore *= 0.60f;
                    }
                }
            }

            if (forceMajor) minScore = -1.0f;
            if (forceMinor) majScore = -1.0f;

            if (majScore > minScore) {
                if (majScore > bestScore) {
                    bestScore = majScore;
                    bestRoot = r;
                    bestIsMinor = false;
                }
            } else {
                if (minScore > bestScore) {
                    bestScore = minScore;
                    bestRoot = r;
                    bestIsMinor = true;
                }
            }
        }

        if (bassSemi >= 0 && smoothedChroma[bassSemi] > 0.03f) {
            if (bestRoot >= 0 && bestRoot != bassSemi) {
                int dist = (bestRoot - bassSemi + 12) % 12;
                if (dist == 4 || dist == 3 || dist == 7) {
                    bestRoot = bassSemi;
                }
            }
        }

        float scoreMinReq = 0.015f / std::max(0.1f, trackSens * 0.25f);
        int requiredBlocks = std::max(1, (int)(debounceCycles + 0.5f));

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
                candidateBlocks = 0;
            }
        }
    }

    // Authentic Solina Divider String Oscillator (Warm 8' + 4' Sawtooth with gentle bandpass)
    inline float renderStringVoice(float freq, double& p8, double& p4) {
        if (freq > sr * 0.48f) return 0.0f;
        p8 += (double)freq * (M_TWOPI / (double)sr);
        if (p8 >= M_TWOPI) p8 -= M_TWOPI;

        p4 += (double)(freq * 2.0f) * (M_TWOPI / (double)sr);
        if (p4 >= M_TWOPI) p4 -= M_TWOPI;

        float saw8 = 2.0f * (float)(p8 / M_TWOPI) - 1.0f;
        float saw4 = 2.0f * (float)(p4 / M_TWOPI) - 1.0f;
        return saw8 * 0.70f + saw4 * 0.35f;
    }

    void process(
        const float* inL, const float* inR,
        float* outL, float* outR,
        uint32_t nSamples,
        float bypass, float thirdMode,
        const float* nsLevels,
        int presetIdx, float ensDepthVal, float brightVal,
        float atkVal, float susVal, float relVal, float gateSensVal,
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

        float atkSec = 0.02f + (atkVal / 10.0f) * 0.75f;
        float attCoeff = 1.0f - expf(-1.0f / (atkSec * sr));
        float sustainHold = susVal / 10.0f;

        float relSec = 0.05f + (relVal / 10.0f) * 3.20f;
        float relCoeff = 1.0f - expf(-1.0f / (relSec * sr));

        float xfadeSec = 0.20f + (relVal / 10.0f) * 0.60f;
        crossfadeRate = 1.0f / (xfadeSec * sr);

        float sensMult = std::max(0.1f, gateSensVal / 5.0f);
        float gateThreshOpen  = 0.0020f * sensMult;
        float gateThreshClose = 0.0006f * sensMult;

        float dryGain    = (dryVal / 10.0f);
        float wetGain    = (wetVal / 10.0f) * 1.15f;
        float masterGain = (outGainVal / 10.0f) * 1.25f;

        for (int i = 0; i < NOTES; i++) {
            float target = nsLevels[i] / 10.0f;
            noteLevelSm[i] += 0.002f * (target - noteLevelSm[i]);
        }

        float cutoffTone = 1200.0f + (brightVal / 10.0f) * 8000.0f;

        for (uint32_t i = 0; i < nSamples; ++i) {
            float sMono = 0.5f * (inL[i] + (inR ? inR[i] : inL[i]));
            float absMono = fabsf(sMono);

            timeDomainPitch.pushSample(sMono);

            if (agcSpeedVal > 0.05f) {
                float agcAttack = 1.0f - expf(-1.0f / (0.002f * sr));
                float agcRelease = 1.0f - expf(-1.0f / ((2.5f / std::max(0.1f, agcSpeedVal)) * sr));
                if (absMono > agcPeak) agcPeak += agcAttack * (absMono - agcPeak);
                else agcPeak += agcRelease * (absMono - agcPeak);

                float targetGain = 0.40f / std::max(0.005f, agcPeak);
                targetGain = std::max(0.2f, std::min(15.0f, targetGain));
                agcGain += 0.001f * (targetGain - agcGain);
            } else {
                agcGain = 1.0f;
            }

            float sLevel = sMono * agcGain;
            float sClean = dehum120.process(dehum100.process(dehum60.process(dehum50.process(sLevel))));

            float scSig = gateScLp.lp(gateScHp.hp(sMono, 75.0f, sr), 3500.0f, sr);
            float scAbs = fabsf(scSig);
            if (scAbs > gateEnv) gateEnv += gateAtk * (scAbs - gateEnv);
            else gateEnv += gateRel * (scAbs - gateEnv);

            if (!gateIsOpen && gateEnv > gateThreshOpen) gateIsOpen = true;
            else if (gateIsOpen && gateEnv < gateThreshClose) gateIsOpen = false;

            float targetGateGain = gateIsOpen ? 1.0f : 0.0f;
            gateGain += gateSmoothRate * (targetGateGain - gateGain);

            for (int r = 0; r < 36; ++r) {
                filterBank[r].process(sClean);
            }

            blockSampleCounter++;
            if (blockSampleCounter >= 256) {
                blockSampleCounter = 0;
                evaluateChord(forceMinor, forceMajor,
                              trackSens, trackStab, debounceVal, bassBoostVal,
                              keyRootVal, scaleModeVal, acWeightVal, acClipVal);
            }

            inputRMS += 0.001f * (absMono - inputRMS);
            float targetEnv = (gateIsOpen || gateGain > 0.05f) ? (0.35f * sustainHold + 0.65f * (inputRMS * 5.0f)) : 0.0f;
            targetEnv = std::min(1.0f, targetEnv * gateGain);

            if (targetEnv > stringEnv) stringEnv += attCoeff * (targetEnv - stringEnv);
            else stringEnv += relCoeff * (targetEnv - stringEnv);

            if (activeBank == 1) {
                bankB.gain = std::min(1.0f, bankB.gain + crossfadeRate);
                bankA.gain = std::max(0.0f, bankA.gain - crossfadeRate);
            } else {
                bankA.gain = std::min(1.0f, bankA.gain + crossfadeRate);
                bankB.gain = std::max(0.0f, bankB.gain - crossfadeRate);
            }

            float mixA = 0.0f;
            int voicesA = 0;
            if (bankA.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesA++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankA.isMinor ? -3 : -4;
                    if (n == 4) semi = bankA.isMinor ?  3 :  4;
                    if (n == 7) semi = bankA.isMinor ? 15 : 16;

                    float noteFund = bankA.rootHz * powf(2.0f, semi / 12.0f);
                    mixA += renderStringVoice(noteFund, bankA.phases[n], bankA.phasesOct[n]) * nVol;
                }
                if (voicesA > 1) mixA /= sqrtf((float)voicesA);
            }

            float mixB = 0.0f;
            int voicesB = 0;
            if (bankB.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesB++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankB.isMinor ? -3 : -4;
                    if (n == 4) semi = bankB.isMinor ?  3 :  4;
                    if (n == 7) semi = bankB.isMinor ? 15 : 16;

                    float noteFund = bankB.rootHz * powf(2.0f, semi / 12.0f);
                    mixB += renderStringVoice(noteFund, bankB.phases[n], bankB.phasesOct[n]) * nVol;
                }
                if (voicesB > 1) mixB /= sqrtf((float)voicesB);
            }

            float blendA = sqrtf(bankA.gain);
            float blendB = sqrtf(bankB.gain);
            float rawStrings = (mixA * blendA + mixB * blendB) * stringEnv;

            // Triple BBD Solina Ensemble Chorus
            float chorusL = 0.0f, chorusR = 0.0f;
            ensemble.process(rawStrings, rawStrings, chorusL, chorusR, ensDepthVal, sr);

            chorusL = toneFilterL.lp(chorusL, cutoffTone, sr);
            chorusR = toneFilterR.lp(chorusR, cutoffTone, sr);

            // Auto Lowest-Note Rhythm Bass Player
            float bassSample = bassPlayer.processSample(
                sr, bassBpmVal, bassTapVal,
                (int)(bassPatternVal + 0.5f), bassLevelVal, bassToneVal,
                confirmedRoot, confirmedMinor, (gateIsOpen || gateGain > 0.05f)
            );

            float wetL = dcBlockL.process(chorusL + bassSample);
            float wetR = dcBlockR.process(chorusR + bassSample);

            float dryL = inL[i] * dryGain;
            float dryR = (inR ? inR[i] : inL[i]) * dryGain;

            outL[i] = (dryL + wetL * wetGain) * masterGain;
            if (outR) {
                outR[i] = (dryR + wetR * wetGain) * masterGain;
            }
        }
    }
};

struct CyberString9Plugin {
    CyberString9DSP* dsp = nullptr;
    const float* audioIn[2] = {};
    float* audioOut[2] = {};
    const float* ports[PORT_COUNT] = {};
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const*) {
    CyberString9Plugin* p = new CyberString9Plugin();
    p->dsp = new CyberString9DSP();
    p->dsp->init((float)rate);
    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    CyberString9Plugin* p = (CyberString9Plugin*)instance;
    if (port < PORT_COUNT) p->ports[port] = (const float*)data;
    if (port == PORT_IN_L) p->audioIn[0] = (const float*)data;
    if (port == PORT_IN_R) p->audioIn[1] = (const float*)data;
    if (port == PORT_OUT_L) p->audioOut[0] = (float*)data;
    if (port == PORT_OUT_R) p->audioOut[1] = (float*)data;
}

static void activate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t n_samples) {
    CyberString9Plugin* p = (CyberString9Plugin*)instance;
    if (!p || !p->dsp || !p->audioOut[0]) return;

    float ns[NOTES];
    for (int i = 0; i < NOTES; i++) {
        ns[i] = p->ports[PORT_NS_SUB_OCT + i] ? *p->ports[PORT_NS_SUB_OCT + i] : NS_DEFAULTS[i];
    }

    p->dsp->process(
        p->audioIn[0], p->audioIn[1],
        p->audioOut[0], p->audioOut[1],
        n_samples,
        p->ports[PORT_BYPASS]      ? *p->ports[PORT_BYPASS]      : 1.0f,
        p->ports[PORT_THIRD_MODE]  ? *p->ports[PORT_THIRD_MODE]  : 0.0f,
        ns,
        p->ports[PORT_PRESET]      ? (int)(*p->ports[PORT_PRESET] + 0.5f) : 0,
        p->ports[PORT_ENSEMBLE]    ? *p->ports[PORT_ENSEMBLE]    : 7.0f,
        p->ports[PORT_BRIGHTNESS]  ? *p->ports[PORT_BRIGHTNESS]  : 6.0f,
        p->ports[PORT_ATTACK]      ? *p->ports[PORT_ATTACK]      : 4.0f,
        p->ports[PORT_SUSTAIN]     ? *p->ports[PORT_SUSTAIN]     : 5.0f,
        p->ports[PORT_RELEASE]     ? *p->ports[PORT_RELEASE]     : 5.0f,
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
    CyberString9Plugin* p = (CyberString9Plugin*)instance;
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
'''
    with open('plugins/cyber-string9.lv2/src/cyber_string9_lv2.cpp', 'w', encoding='utf-8') as f:
        f.write(COMMON_HEADER + string_body)
    print("cyber-string9 source generated successfully!")

write_string9_source()

def write_bass9_source():
    bass_body = '''
#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-bass9"

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

    // AUTO LOWEST-NOTE RHYTHM BASS PLAYER
    PORT_BASS_LEVEL   = 36,
    PORT_BASS_PATTERN = 37,
    PORT_BASS_BPM     = 38,
    PORT_BASS_TAP     = 39,
    PORT_BASS_TONE    = 40,

    PORT_COUNT        = 41
};

struct BassChordBank {
    double phases[NOTES];
    double subPhases[NOTES];
    float  filterZ[NOTES];
    float  envZ[NOTES];
    float  rootHz = 130.81f; // One octave lower baseline for bass
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

class CyberBass9DSP {
public:
    float sr = 44100.0f;

    NoteResonator filterBank[36];
    float chromaVector[12];
    float smoothedChroma[12];

    int   confirmedRoot = -1;
    int   candidateRoot = -1;
    int   candidateBlocks = 0;
    bool  confirmedMinor = false;
    int   blockSampleCounter = 0;

    BassChordBank bankA;
    BassChordBank bankB;
    int           activeBank = 0;
    float         crossfadeRate = 0.0004f;

    float noteLevelSm[NOTES];
    float agcPeak = 0.0f;
    float agcGain = 1.0f;

    NotchFilter dehum50, dehum60, dehum100, dehum120;
    OnePoleFilter gateScHp, gateScLp;
    float gateEnv = 0.0f;
    float gateGain = 0.0f;
    bool  gateIsOpen = false;
    float gateAtk = 0.0f, gateRel = 0.0f, gateSmoothRate = 0.0f;

    float inputRMS = 0.0f;
    float bassEnv = 0.0f;

    DCBlocker dcBlockL, dcBlockR;
    TimeDomainAutocorr timeDomainPitch;
    BassRhythmEngine   bassPlayer;
    float currentFilterQ = 7.0f;

    void init(float sampleRate) {
        sr = sampleRate;
        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            float freq = A4 * powf(2.0f, (midi - 69) / 12.0f);
            filterBank[i].setup(freq, sr, 8.0f);
        }

        memset(chromaVector, 0, sizeof(chromaVector));
        memset(smoothedChroma, 0, sizeof(smoothedChroma));

        bankA.init(130.81f, false, 1.0f);
        bankB.init(130.81f, false, 0.0f);
        activeBank = 0;
        confirmedRoot = 0;

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
        gateSmoothRate = 1.0f - expf(-1.0f / (0.005f * sr));

        bassPlayer.reset();
    }

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

        timeDomainPitch.analyze(sr, acClipVal);
        int autoCorrSemi = timeDomainPitch.detectedSemi;
        float autoCorrConf = timeDomainPitch.confidence;

        memset(chromaVector, 0, sizeof(chromaVector));
        float maxBass = 0.0f;
        int bassSemi = -1;
        float bassMult = 1.0f + (bassBoost / 10.0f) * 2.5f;
        float baseThresh = 0.015f / std::max(0.05f, trackSens * 0.2f);

        for (int i = 0; i < 36; ++i) {
            int midi = 40 + i;
            int semi = midi % 12;
            float env = filterBank[i].envelope;
            if (i < 12 && env > maxBass && env > baseThresh) {
                maxBass = env;
                bassSemi = semi;
            }
            float weight = (i < 12) ? bassMult : (1.0f / (1.0f + 0.035f * (i - 12)));
            chromaVector[semi] += env * weight;
        }

        float totalChroma = 0.0f;
        for (int s = 0; s < 12; s++) {
            totalChroma += chromaVector[s];
        }

        if (totalChroma < baseThresh * 2.0f && autoCorrConf < 0.35f) return;

        float acWeight = acWeightVal / 10.0f;
        if (autoCorrConf > 0.40f && autoCorrSemi >= 0) {
            chromaVector[autoCorrSemi] += autoCorrConf * totalChroma * acWeight * 1.5f;
        }

        float smoothCoeff = 1.0f / std::max(1.1f, trackStab * 0.5f);
        for (int s = 0; s < 12; s++) {
            smoothedChroma[s] += smoothCoeff * (chromaVector[s] - smoothedChroma[s]);
        }

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

        float bestScore = 0.0f;
        int bestRoot = -1;
        bool bestIsMinor = false;

        for (int r = 0; r < 12; ++r) {
            if (keyRootVal > 0) {
                int selectedKey = (keyRootVal - 1) % 12;
                int interval = (r - selectedKey + 12) % 12;
                bool inScale = false;
                if (scaleModeVal == 1) { // Major
                    inScale = (interval == 0 || interval == 2 || interval == 4 || interval == 5 || interval == 7 || interval == 9 || interval == 11);
                } else if (scaleModeVal == 2) { // Minor
                    inScale = (interval == 0 || interval == 2 || interval == 3 || interval == 5 || interval == 7 || interval == 8 || interval == 10);
                } else {
                    inScale = (interval == 0 || interval == 2 || interval == 4 || interval == 5 || interval == 7 || interval == 9 || interval == 11);
                }
                if (!inScale) continue;
            }

            float rE = smoothedChroma[r];
            float fE = smoothedChroma[(r + 7) % 12];
            float m3E = smoothedChroma[(r + 3) % 12];
            float M3E = smoothedChroma[(r + 4) % 12];

            float majScore = rE * 1.5f + fE * 1.0f + M3E * 0.8f;
            float minScore = rE * 1.5f + fE * 1.0f + m3E * 0.8f;

            if (forceMajor) minScore = 0.0f;
            if (forceMinor) majScore = 0.0f;

            if (r == bassSemi) {
                majScore *= 1.25f;
                minScore *= 1.25f;
            }

            if (r == autoCorrSemi && autoCorrConf > 0.50f) {
                majScore *= (1.0f + autoCorrConf * 0.6f);
                minScore *= (1.0f + autoCorrConf * 0.6f);
            }

            if (majScore > bestScore) {
                bestScore = majScore;
                bestRoot = r;
                bestIsMinor = false;
            }
            if (minScore > bestScore) {
                bestScore = minScore;
                bestRoot = r;
                bestIsMinor = true;
            }
        }

        if (bestRoot >= 0) {
            int reqBlocks = (int)std::max(1.0f, roundf(debounceCycles));
            if (bestRoot == candidateRoot) {
                candidateBlocks++;
            } else {
                candidateRoot = bestRoot;
                candidateBlocks = 1;
            }

            if (candidateBlocks >= reqBlocks && candidateRoot != confirmedRoot) {
                confirmedRoot = candidateRoot;
                confirmedMinor = bestIsMinor;

                // Sub-octave root frequency (one octave below standard guitar fundamental)
                float newRootHz = (A4 * 0.25f) * powf(2.0f, (confirmedRoot - 9) / 12.0f);

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

        float c1 = ctrl1 / 10.0f; // 0..1
        float c2 = ctrl2 / 10.0f; // 0..1

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
                float chorusPhase = p + sinf(sp * 4.0f) * (0.15f + c2 * 0.35f);
                sig += 0.35f * sinf(chorusPhase);
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
            case 8: // 9: Flip-Flop (EHX Octave Multiplexer logic-driven sub)
            default: {
                float sq = (p < (float)M_PI) ? 0.7f : -0.7f;
                float subSq = (sp < (float)M_PI) ? 0.9f : -0.9f;
                sig = sq * (1.0f - c2 * 0.5f) + subSq * (0.6f + c2 * 0.8f);
                float fc = 250.0f + c1 * 1600.0f;
                float w = (float)(M_TWOPI * fc / sr);
                filterState += (w / (1.0f + w)) * (sig - filterState);
                sig = filterState;
                break;
            }
        }
        return sig;
    }

    void process(
        const float* inL, const float* inR,
        float* outL, float* outR,
        uint32_t nSamples,
        float bypass, float thirdMode,
        const float* nsLevels,
        int presetIdx, float ctrl1Val, float ctrl2Val,
        float atkVal, float susVal, float relVal, float gateSensVal,
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

        float attCoeff = 1.0f - expf(-1.0f / (std::max(0.003f, atkVal * 0.050f) * sr));
        float relCoeff = 1.0f - expf(-1.0f / (std::max(0.010f, relVal * 0.120f) * sr));
        float sustainHold = susVal / 10.0f;

        float gateThreshOpen = 0.003f * powf(10.0f, (gateSensVal - 5.0f) * 0.25f);
        float gateThreshClose = gateThreshOpen * 0.55f;

        float dryGain = dryVal / 10.0f;
        float wetGain = (wetVal / 10.0f) * 1.35f;
        float masterGain = outGainVal / 7.0f;

        for (int n = 0; n < NOTES; n++) {
            noteLevelSm[n] += 0.05f * (nsLevels[n] - noteLevelSm[n]);
        }

        for (uint32_t i = 0; i < nSamples; ++i) {
            float sMono = 0.5f * (inL[i] + (inR ? inR[i] : inL[i]));
            float absMono = fabsf(sMono);

            timeDomainPitch.pushSample(sMono);

            if (agcSpeedVal > 0.05f) {
                float agcAttack = 1.0f - expf(-1.0f / (0.002f * sr));
                float agcRelease = 1.0f - expf(-1.0f / ((2.5f / std::max(0.1f, agcSpeedVal)) * sr));
                if (absMono > agcPeak) agcPeak += agcAttack * (absMono - agcPeak);
                else agcPeak += agcRelease * (absMono - agcPeak);

                float targetGain = 0.40f / std::max(0.005f, agcPeak);
                targetGain = std::max(0.2f, std::min(15.0f, targetGain));
                agcGain += 0.001f * (targetGain - agcGain);
            } else {
                agcGain = 1.0f;
            }

            float sLevel = sMono * agcGain;
            float sClean = dehum120.process(dehum100.process(dehum60.process(dehum50.process(sLevel))));

            float scSig = gateScLp.lp(gateScHp.hp(sMono, 75.0f, sr), 3500.0f, sr);
            float scAbs = fabsf(scSig);
            if (scAbs > gateEnv) gateEnv += gateAtk * (scAbs - gateEnv);
            else gateEnv += gateRel * (scAbs - gateEnv);

            if (!gateIsOpen && gateEnv > gateThreshOpen) gateIsOpen = true;
            else if (gateIsOpen && gateEnv < gateThreshClose) gateIsOpen = false;

            float targetGateGain = gateIsOpen ? 1.0f : 0.0f;
            gateGain += gateSmoothRate * (targetGateGain - gateGain);

            for (int r = 0; r < 36; ++r) {
                filterBank[r].process(sClean);
            }

            blockSampleCounter++;
            if (blockSampleCounter >= 256) {
                blockSampleCounter = 0;
                evaluateChord(forceMinor, forceMajor,
                              trackSens, trackStab, debounceVal, bassBoostVal,
                              keyRootVal, scaleModeVal, acWeightVal, acClipVal);
            }

            inputRMS += 0.001f * (absMono - inputRMS);
            float targetEnv = (gateIsOpen || gateGain > 0.05f) ? (0.35f * sustainHold + 0.65f * (inputRMS * 5.0f)) : 0.0f;
            targetEnv = std::min(1.0f, targetEnv * gateGain);

            if (targetEnv > bassEnv) bassEnv += attCoeff * (targetEnv - bassEnv);
            else bassEnv += relCoeff * (targetEnv - bassEnv);

            if (activeBank == 1) {
                bankB.gain = std::min(1.0f, bankB.gain + crossfadeRate);
                bankA.gain = std::max(0.0f, bankA.gain - crossfadeRate);
            } else {
                bankA.gain = std::min(1.0f, bankA.gain + crossfadeRate);
                bankB.gain = std::max(0.0f, bankB.gain - crossfadeRate);
            }

            float mixA = 0.0f;
            int voicesA = 0;
            if (bankA.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesA++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankA.isMinor ? -3 : -4;
                    if (n == 4) semi = bankA.isMinor ?  3 :  4;
                    if (n == 7) semi = bankA.isMinor ? 15 : 16;

                    float noteFund = bankA.rootHz * powf(2.0f, semi / 12.0f);
                    mixA += renderBassVoice(presetIdx, noteFund, bankA.phases[n], bankA.subPhases[n], bankA.filterZ[n], ctrl1Val, ctrl2Val) * nVol;
                }
                if (voicesA > 1) mixA /= sqrtf((float)voicesA);
            }

            float mixB = 0.0f;
            int voicesB = 0;
            if (bankB.gain > 0.001f) {
                for (int n = 0; n < NOTES; n++) {
                    float nVol = noteLevelSm[n];
                    if (nVol < 0.005f) continue;
                    voicesB++;
                    int semi = NOTE_SEMI[n];
                    if (n == 2) semi = bankB.isMinor ? -3 : -4;
                    if (n == 4) semi = bankB.isMinor ?  3 :  4;
                    if (n == 7) semi = bankB.isMinor ? 15 : 16;

                    float noteFund = bankB.rootHz * powf(2.0f, semi / 12.0f);
                    mixB += renderBassVoice(presetIdx, noteFund, bankB.phases[n], bankB.subPhases[n], bankB.filterZ[n], ctrl1Val, ctrl2Val) * nVol;
                }
                if (voicesB > 1) mixB /= sqrtf((float)voicesB);
            }

            float blendA = sqrtf(bankA.gain);
            float blendB = sqrtf(bankB.gain);
            float rawBass = (mixA * blendA + mixB * blendB) * bassEnv;

            // Auto Lowest-Note Rhythm Bass Player
            float bassSample = bassPlayer.processSample(
                sr, bassBpmVal, bassTapVal,
                (int)(bassPatternVal + 0.5f), bassLevelVal, bassToneVal,
                confirmedRoot, confirmedMinor, (gateIsOpen || gateGain > 0.05f)
            );

            float wetL = dcBlockL.process(rawBass + bassSample);
            float wetR = dcBlockR.process(rawBass + bassSample);

            float dryL = inL[i] * dryGain;
            float dryR = (inR ? inR[i] : inL[i]) * dryGain;

            outL[i] = (dryL + wetL * wetGain) * masterGain;
            if (outR) {
                outR[i] = (dryR + wetR * wetGain) * masterGain;
            }
        }
    }
};

struct CyberBass9Plugin {
    CyberBass9DSP* dsp = nullptr;
    const float* audioIn[2] = {};
    float* audioOut[2] = {};
    const float* ports[PORT_COUNT] = {};
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const*) {
    CyberBass9Plugin* p = new CyberBass9Plugin();
    p->dsp = new CyberBass9DSP();
    p->dsp->init((float)rate);
    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    CyberBass9Plugin* p = (CyberBass9Plugin*)instance;
    if (port < PORT_COUNT) p->ports[port] = (const float*)data;
    if (port == PORT_IN_L) p->audioIn[0] = (const float*)data;
    if (port == PORT_IN_R) p->audioIn[1] = (const float*)data;
    if (port == PORT_OUT_L) p->audioOut[0] = (float*)data;
    if (port == PORT_OUT_R) p->audioOut[1] = (float*)data;
}

static void activate(LV2_Handle) {}

static void run(LV2_Handle instance, uint32_t n_samples) {
    CyberBass9Plugin* p = (CyberBass9Plugin*)instance;
    if (!p || !p->dsp || !p->audioOut[0]) return;

    float ns[NOTES];
    for (int n = 0; n < NOTES; n++) {
        ns[n] = p->ports[PORT_NS_SUB_OCT + n] ? *p->ports[PORT_NS_SUB_OCT + n] : NS_DEFAULTS[n];
    }

    p->dsp->process(
        p->audioIn[0],
        p->audioIn[1],
        p->audioOut[0],
        p->audioOut[1],
        n_samples,
        p->ports[PORT_BYPASS]      ? *p->ports[PORT_BYPASS]      : 1.0f,
        p->ports[PORT_THIRD_MODE]  ? *p->ports[PORT_THIRD_MODE]  : 0.0f,
        ns,
        p->ports[PORT_PRESET]      ? (int)(*p->ports[PORT_PRESET] + 0.5f) : 0,
        p->ports[PORT_CTRL1]       ? *p->ports[PORT_CTRL1]       : 5.0f,
        p->ports[PORT_CTRL2]       ? *p->ports[PORT_CTRL2]       : 5.0f,
        p->ports[PORT_ATTACK]      ? *p->ports[PORT_ATTACK]      : 2.0f,
        p->ports[PORT_SUSTAIN]     ? *p->ports[PORT_SUSTAIN]     : 5.0f,
        p->ports[PORT_RELEASE]     ? *p->ports[PORT_RELEASE]     : 5.0f,
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
    CyberBass9Plugin* p = (CyberBass9Plugin*)instance;
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
'''
    with open('plugins/cyber-bass9.lv2/src/cyber_bass9_lv2.cpp', 'w', encoding='utf-8') as f:
        f.write(COMMON_HEADER + bass_body)
    print("cyber-bass9 source generated successfully!")

write_bass9_source()


