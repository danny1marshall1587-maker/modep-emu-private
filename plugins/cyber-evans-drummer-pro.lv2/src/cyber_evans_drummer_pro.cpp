/*
 * Cyber Evans Drummer Pro (LV2 Audio-to-MIDI Virtual Drummer)
 * Listens to live guitar audio transients, accents, and dynamics to improvise,
 * follow song roadmaps, and output authentic General MIDI drum performances.
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_TWOPI
#define M_TWOPI 6.28318530717958647692
#endif

#include "lv2.h"
#include <atom.h>
#include <forge.h>
#include <midi.h>
#include <urid.h>

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/cyber-evans-drummer-pro"

// General MIDI Drum Note Constants
static const uint8_t GM_BASS_DRUM   = 36;
static const uint8_t GM_SNARE       = 38;
static const uint8_t GM_SNARE_RIM   = 40;
static const uint8_t GM_CLOSED_HH   = 42;
static const uint8_t GM_PEDAL_HH    = 44;
static const uint8_t GM_OPEN_HH     = 46;
static const uint8_t GM_LOW_TOM     = 45;
static const uint8_t GM_MID_TOM     = 47;
static const uint8_t GM_HIGH_TOM    = 50;
static const uint8_t GM_CRASH_1     = 49;
static const uint8_t GM_RIDE_1      = 51;

enum PortIndex {
    PORT_AUDIO_IN        = 0,
    PORT_AUDIO_THRU      = 1,
    PORT_BYPASS          = 2,
    PORT_STYLE           = 3,
    PORT_BPM_OVERRIDE    = 4,
    PORT_ACCENT_SENS     = 5,
    PORT_GHOST_DENSITY   = 6,
    PORT_FILL_DENSITY    = 7,
    PORT_GROOVE          = 8,
    PORT_OVERALL_VEL     = 9,
    PORT_OPEN_HH_THRESH  = 10,
    PORT_CRASH_ON_CHORUS = 11,
    PORT_RECORD_SONG     = 12,
    PORT_MUTE            = 13,
    PORT_ADVENTUROUS     = 14,
    PORT_DETECTED_BPM    = 15,
    PORT_CURRENT_SECTION = 16,
    PORT_ENERGY_LEVEL    = 17,
    PORT_STRUM_ONLY_MODE = 18,
    PORT_KIT_MASTER_VOL  = 19,
    PORT_STOP_CRASH      = 20,
    PORT_FILL_NOW        = 21,
    PORT_MANUAL_CHORUS   = 22,
    PORT_CONTROL         = 23,
    PORT_MIDI_OUT        = 24,
    PORT_TAP_TEMPO       = 25, // Momentary: tap to set rough BPM seed
    PORT_SILENCE_GATE    = 26, // Knob: seconds of silence before auto-stop (0=disabled, 1-10s)
    PORT_RESET_LISTEN    = 27, // Momentary: reset drummer to listen state for a new song start
    PORT_START_PLAY      = 28, // Start Playback on downbeat
    PORT_COUNT           = 29
};

// ── Broadband Transient & Pick Attack Extractor ──
struct BroadbandTransientExtractor {
    float hp_x1 = 0.0f;
    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
    bool  onsetDetected = false;
    bool  prevHitState = false;
    int   refractorySamples = 0;
    float sampleRate = 48000.0f;

    void init(float sr) {
        sampleRate = sr;
        hp_x1 = 0.0f;
        fastEnv = slowEnv = 0.0f;
        onsetDetected = false;
        prevHitState = false;
        refractorySamples = 0;
    }

    inline bool process(float in, float sensitivity = 5.0f, float inputFloor = 0.005f) {
        float click = in - hp_x1;
        hp_x1 = in;
        float absClick = fabsf(click);

        fastEnv = fastEnv * 0.40f + absClick * 0.60f;
        slowEnv = slowEnv * 0.990f + absClick * 0.010f;

        float sensFactor = 1.35f + (10.0f - sensitivity) * 0.25f;
        float minDynamicFloor = std::max(0.0008f, inputFloor * 0.08f);
        bool isHitRaw = (fastEnv > slowEnv * sensFactor) && (fastEnv > minDynamicFloor);

        bool isHitTrigger = false;
        if (refractorySamples > 0) {
            refractorySamples--;
        } else if (isHitRaw && !prevHitState) {
            isHitTrigger = true;
            refractorySamples = (int)(0.090f * sampleRate); // 90ms lockout groups individual string picks into one chord strum
        }
        prevHitState = isHitRaw;
        onsetDetected = isHitTrigger;
        return isHitTrigger;
    }
};

/// ── Real-Time Guitar Chroma Resonator & Chord Cycle Tracker ──
// Detects pitch chroma energy across 12 pitch classes, tracks chord changes,
// and identifies when a progression cycle (e.g. 3, 4, 5, 6, 8 bars) repeats.
struct GoertzelResonator {
    float coeff = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
    float freq = 0.0f;

    void setup(float f, float sr) {
        freq = f;
        float w = 2.0f * (float)M_PI * f / sr;
        coeff = 2.0f * cosf(w);
        reset();
    }

    void reset() {
        s1 = s2 = 0.0f;
    }

    inline void process(float x) {
        float s0 = x + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    inline float getEnergy() const {
        return (s1 * s1 + s2 * s2 - s1 * s2 * coeff);
    }
};

struct ChordCycleTracker {
    static const int NUM_FILTERS = 36; // 3 octaves (E2 82.4Hz to Eb5 622.25Hz)
    GoertzelResonator filters[NUM_FILTERS];
    int blockSamples = 9600; // ~200ms blocks at 48kHz
    int sampleCountInBlock = 0;
    float smoothedChroma[12] = {};

    int confirmedChordId = -1;

    // History of chords per bar (up to 32 bars)
    int barChords[32] = {};
    int barsObserved = 0;

    // Progression cycle length detected (in bars, e.g. 4, 5, 6, 8)
    int detectedRoundBars = 0;
    int roundsCounted = 0;
    bool cycleLocked = false;

    void init(float sr) {
        for (int i = 0; i < NUM_FILTERS; ++i) {
            int midi = 40 + i; // 40 = E2 (82.41 Hz) up to 75 = Eb5
            float freq = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
            filters[i].setup(freq, sr);
        }
        blockSamples = (int)(0.20f * sr);
        reset();
    }

    void reset() {
        for (int i = 0; i < NUM_FILTERS; ++i) filters[i].reset();
        sampleCountInBlock = 0;
        memset(smoothedChroma, 0, sizeof(smoothedChroma));
        confirmedChordId = -1;
        memset(barChords, -1, sizeof(barChords));
        barsObserved = 0;
        detectedRoundBars = 0;
        roundsCounted = 0;
        cycleLocked = false;
    }

    // Process audio sample through resonator bank
    inline void processSample(float inSample) {
        for (int i = 0; i < NUM_FILTERS; ++i) {
            filters[i].process(inSample);
        }
        sampleCountInBlock++;
        if (sampleCountInBlock >= blockSamples) {
            sampleCountInBlock = 0;
            float rawChroma[12] = {};
            for (int i = 0; i < NUM_FILTERS; ++i) {
                int semi = (40 + i) % 12;
                rawChroma[semi] += sqrtf(filters[i].getEnergy());
                filters[i].reset();
            }

            float maxC = 1e-6f;
            for (int c = 0; c < 12; ++c) {
                if (rawChroma[c] > maxC) maxC = rawChroma[c];
            }
            for (int c = 0; c < 12; ++c) {
                float norm = rawChroma[c] / maxC;
                smoothedChroma[c] = smoothedChroma[c] * 0.5f + norm * 0.5f;
            }

            int bestSemi = -1;
            float bestVal = 0.5f;
            for (int c = 0; c < 12; ++c) {
                // Root + 5th + 3rd harmonic profile
                float score = smoothedChroma[c] * 1.5f +
                              smoothedChroma[(c + 7) % 12] * 1.0f +
                              std::max(smoothedChroma[(c + 4) % 12], smoothedChroma[(c + 3) % 12]) * 0.8f;
                if (score > bestVal) {
                    bestVal = score;
                    bestSemi = c;
                }
            }
            if (bestSemi >= 0) {
                confirmedChordId = bestSemi;
            }
        }
    }

    // Call at end of each bar during listening mode
    void onBarComplete() {
        if (barsObserved < 32) {
            barChords[barsObserved] = confirmedChordId;
            barsObserved++;
        }

        // Test cycle lengths: only check progression lengths >= 3 bars (e.g. 5, 4, 6, 8, 3)
        // Disallow 1 or 2 bars to prevent locking prematurely on repeated chords!
        if (!cycleLocked && barsObserved >= 6) {
            int candidateLengths[] = { 5, 4, 6, 8, 3 };
            for (int ci = 0; ci < 5; ++ci) {
                int L = candidateLengths[ci];
                if (barsObserved >= L * 2) {
                    // Check distinct chords in candidate cycle
                    int distinct = 0;
                    for (int b = 0; b < L; ++b) {
                        bool found = false;
                        for (int k = 0; k < b; ++k) {
                            if (barChords[barsObserved - L + b] == barChords[barsObserved - L + k]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) distinct++;
                    }
                    if (distinct < 2) continue; // Must have at least 2 distinct chords in progression!

                    // Compare round 1 vs round 2
                    int diffCount = 0;
                    for (int b = 0; b < L; ++b) {
                        int c1 = barChords[barsObserved - 2 * L + b];
                        int c2 = barChords[barsObserved - L + b];
                        if (c1 >= 0 && c2 >= 0 && c1 != c2) diffCount++;
                    }
                    if (diffCount <= 1) { // allow at most 1 passing chord difference
                        detectedRoundBars = L;
                        cycleLocked = true;
                        roundsCounted = 2;
                        break;
                    }
                }
            }
        } else if (cycleLocked) {
            roundsCounted++;
        }
    }
};

// ── Live Guitar Strum Pulse & Dynamic Tempo Tracker ──
struct GuitarStrumPulseTracker {
    float lastOnsetSec = 0.0f;
    float currentSec = 0.0f;
    float estimatedBpm = 120.0f;
    float confidence = 0.0f;
    bool  hasLockedTempo = false;

    // History of raw onset timestamps
    static const int MAX_ONSETS = 32;
    float onsetTimes[MAX_ONSETS] = {};
    int   onsetCount = 0;
    int   strumsSinceReset = 0;

    void reset() {
        lastOnsetSec = 0.0f;
        currentSec = 0.0f;
        estimatedBpm = 120.0f;
        confidence = 0.0f;
        hasLockedTempo = false;
        memset(onsetTimes, 0, sizeof(onsetTimes));
        onsetCount = 0;
        strumsSinceReset = 0;
    }

    void onStrumHit(float peakAmp) {
        float dt = currentSec - lastOnsetSec;
        lastOnsetSec = currentSec;
        strumsSinceReset++;

        // Store onset timestamp
        if (onsetCount < MAX_ONSETS) {
            onsetTimes[onsetCount++] = currentSec;
        } else {
            // Shift left
            for (int k = 0; k < MAX_ONSETS - 1; ++k) {
                onsetTimes[k] = onsetTimes[k + 1];
            }
            onsetTimes[MAX_ONSETS - 1] = currentSec;
        }

        // Only evaluate if we have at least 4 onsets
        if (onsetCount < 4) return;

        // Compute all pairwise IOIs (intervals) between recent onsets within 3.5 seconds
        // (covers consecutive hits, alternate hits, bar pulses, and syncopated accents)
        float pairwiseIois[128];
        int numIois = 0;

        int searchStart = std::max(0, onsetCount - 16);
        for (int i = searchStart; i < onsetCount; ++i) {
            for (int j = i + 1; j < onsetCount; ++j) {
                float interval = onsetTimes[j] - onsetTimes[i];
                if (interval >= 0.18f && interval <= 3.20f) {
                    if (numIois < 128) {
                        pairwiseIois[numIois++] = interval;
                    }
                }
            }
        }

        if (numIois < 5) return;

        // Evaluate candidate quarter-note beat periods from 55 BPM to 195 BPM
        // Period range: 0.307s (195 BPM) to 1.091s (55 BPM) in fine increments
        float bestPeriod = 0.50f;
        float bestScore = -1.0f;

        // Step by 5ms intervals (about 1-2 BPM resolution)
        for (float testT = 0.30f; testT <= 1.10f; testT += 0.005f) {
            float score = 0.0f;

            for (int k = 0; k < numIois; ++k) {
                float ioi = pairwiseIois[k];
                // How many multiples/fractions of testT is this IOI?
                // Test for matches to: 0.5x (8th note), 1x (quarter), 1.5x (dotted quarter),
                // 2x (half note), 3x (dotted half), 4x (whole note / full bar)
                float ratios[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };
                float weights[] = { 0.8f, 1.5f, 0.9f, 1.3f, 0.7f, 1.1f };

                for (int r = 0; r < 6; ++r) {
                    float target = testT * ratios[r];
                    float diff = fabsf(ioi - target);
                    float tolerance = 0.055f * ratios[r]; // scaled tolerance
                    if (diff < tolerance) {
                        // Gaussian-like peak scoring
                        float resonance = 1.0f - (diff / tolerance);
                        score += resonance * weights[r];
                        break;
                    }
                }
            }

            // Slight prior favoring human natural tempo center (85 - 135 BPM)
            float bpmCenterDist = fabsf((60.0f / testT) - 110.0f);
            float tempoPrior = 1.0f - (bpmCenterDist / 250.0f) * 0.15f;
            score *= std::max(0.5f, tempoPrior);

            if (score > bestScore) {
                bestScore = score;
                bestPeriod = testT;
            }
        }

        if (bestScore > 3.0f && bestPeriod >= 0.28f && bestPeriod <= 1.20f) {
            float detectedBpm = 60.0f / bestPeriod;
            detectedBpm = std::max(50.0f, std::min(220.0f, detectedBpm));

            if (!hasLockedTempo) {
                // Initial fast lock on first solid rhythm detection
                estimatedBpm = detectedBpm;
                hasLockedTempo = true;
                confidence = 0.70f;
            } else {
                // Adaptive smoothing: if guitar tempo subtly drifts, follow smoothly.
                // If detected tempo is an exact octave/double/half harmonic, reject jumping
                float ratio = detectedBpm / estimatedBpm;
                if (ratio > 1.85f && ratio < 2.15f) {
                    detectedBpm *= 0.5f; // prevent accidental octave doubling
                } else if (ratio > 0.45f && ratio < 0.55f) {
                    detectedBpm *= 2.0f; // prevent accidental octave halving
                }

                float alpha = (strumsSinceReset < 12) ? 0.30f : 0.08f;
                estimatedBpm = estimatedBpm * (1.0f - alpha) + detectedBpm * alpha;
                confidence = std::min(1.0f, confidence + 0.08f);
            }
        }
    }

    void update(float sampleDt) {
        currentSec += sampleDt;
    }
};

// ── Persistent Song Library ──
// Each learned song is saved as a small binary file in the songs/ sub-directory
// of the plugin bundle. On startup all files are loaded. When a new song is
// learned the file is written. During playback the live BPM + bar-energy profile
// is compared against every stored song; a close match auto-loads that roadmap
// so the auto-fill and section detection work from the very first bar.

#define SONG_DB_DIR "/var/modep/lv2/cyber-evans-drummer-pro.lv2/songs/"
#define MAX_SAVED_SONGS 32
#define SONG_FILE_MAGIC  0x43455350u  // "CESP"
#define SONG_FILE_VER    2

struct SavedSong {
    uint32_t magic;            // SONG_FILE_MAGIC
    uint32_t version;          // SONG_FILE_VER
    float    bpm;              // Recorded tempo
    int      barCount;         // Number of bars recorded (up to 64)
    float    verseMeanEnergy;
    float    chorusMeanEnergy;
    float    barEnergy[64];    // Normalised RMS energy per bar
    int      barHitCount[64];  // Guitar transient hits per bar
    char     name[64];         // Auto-name: "Song_001", "Song_002", …
};

struct SongLibrary {
    SavedSong songs[MAX_SAVED_SONGS];
    int       count = 0;

    // ── Ensure songs directory exists ──
    void ensureDir() {
        mkdir(SONG_DB_DIR, 0755);
    }

    // ── Load all .dat files from the songs directory ──
    void loadAll() {
        ensureDir();
        count = 0;
        DIR* d = opendir(SONG_DB_DIR);
        if (!d) return;
        struct dirent* ent;
        while ((ent = readdir(d)) && count < MAX_SAVED_SONGS) {
            // Only process .dat files
            const char* nm = ent->d_name;
            int len = (int)strlen(nm);
            if (len < 5 || strcmp(nm + len - 4, ".dat") != 0) continue;
            char path[256];
            snprintf(path, sizeof(path), "%s%s", SONG_DB_DIR, nm);
            FILE* f = fopen(path, "rb");
            if (!f) continue;
            SavedSong s;
            size_t got = fread(&s, 1, sizeof(SavedSong), f);
            fclose(f);
            if (got == sizeof(SavedSong) &&
                s.magic == SONG_FILE_MAGIC && s.version == SONG_FILE_VER &&
                s.barCount >= 4) {
                songs[count++] = s;
            }
        }
        closedir(d);
    }

    // ── Save a newly learned song ──
    // Scans existing files to find the highest index and creates the next one.
    void save(const SavedSong& s) {
        ensureDir();
        // Find next free index
        int nextIdx = 1;
        DIR* d = opendir(SONG_DB_DIR);
        if (d) {
            struct dirent* ent;
            while ((ent = readdir(d))) {
                int idx = 0;
                if (sscanf(ent->d_name, "song_%d.dat", &idx) == 1) {
                    if (idx >= nextIdx) nextIdx = idx + 1;
                }
            }
            closedir(d);
        }
        char path[256];
        snprintf(path, sizeof(path), "%ssong_%03d.dat", SONG_DB_DIR, nextIdx);
        FILE* f = fopen(path, "wb");
        if (!f) return;
        fwrite(&s, 1, sizeof(SavedSong), f);
        fclose(f);
        // Also add to in-memory library if space
        if (count < MAX_SAVED_SONGS) songs[count++] = s;
    }

    // ── Try to match live play against stored songs ──
    // liveBpm: current estimated BPM
    // liveEnergy[]: bar energies recorded so far (barsRecorded bars)
    // barsRecorded: how many bars of live data we have
    // Returns index into songs[] of best match, or -1 if no match found.
    int tryMatch(float liveBpm, const float* liveEnergy, int barsRecorded) const {
        if (barsRecorded < 4 || count == 0) return -1;

        int best = -1;
        float bestScore = 0.0f;

        for (int si = 0; si < count; ++si) {
            const SavedSong& s = songs[si];
            // ① BPM must be within ±10 BPM
            if (fabsf(liveBpm - s.bpm) > 10.0f) continue;
            // ② Energy correlation across the available bars
            int n = std::min(barsRecorded, s.barCount);
            if (n < 4) continue;
            // Compute means
            float meanL = 0, meanS = 0;
            for (int b = 0; b < n; ++b) {
                meanL += liveEnergy[b];
                meanS += s.barEnergy[b];
            }
            meanL /= n; meanS /= n;
            // Pearson numerator and denominators
            float num = 0, denL = 0, denS = 0;
            for (int b = 0; b < n; ++b) {
                float dl = liveEnergy[b] - meanL;
                float ds = s.barEnergy[b] - meanS;
                num  += dl * ds;
                denL += dl * dl;
                denS += ds * ds;
            }
            float den = sqrtf(denL * denS);
            if (den < 1e-6f) continue;
            float corr = num / den;  // -1..+1
            // Combine correlation with BPM closeness
            float bpmScore = 1.0f - fabsf(liveBpm - s.bpm) / 10.0f;
            float score = corr * 0.7f + bpmScore * 0.3f;
            if (score > bestScore && corr > 0.55f) {
                bestScore = score;
                best = si;
            }
        }
        return best;
    }
};

// ── Song Roadmap Memory Engine ──

struct SongMemoryEngine {
    static const int MAX_RECORDED_BARS = 64;
    float barEnergy[MAX_RECORDED_BARS] = {};
    int barHitCount[MAX_RECORDED_BARS] = {};
    int recordedBars = 0;
    bool isRecording = false;
    bool hasLearned = false;

    float verseMeanEnergy = 0.2f;
    float chorusMeanEnergy = 0.6f;

    void startRecording() {
        recordedBars = 0;
        isRecording = true;
        hasLearned = false;
        memset(barEnergy, 0, sizeof(barEnergy));
        memset(barHitCount, 0, sizeof(barHitCount));
    }

    void stopRecording() {
        isRecording = false;
        if (recordedBars >= 4) {
            hasLearned = true;
            // Classify sections: calculate lowest 40% vs highest 40%
            std::vector<float> sorted(barEnergy, barEnergy + recordedBars);
            std::sort(sorted.begin(), sorted.end());
            verseMeanEnergy = sorted[recordedBars * 2 / 10];
            chorusMeanEnergy = sorted[recordedBars * 8 / 10];
        }
    }

    void onBarFinish(float avgEnergy, int hits) {
        if (isRecording && recordedBars < MAX_RECORDED_BARS) {
            barEnergy[recordedBars] = avgEnergy;
            barHitCount[recordedBars] = hits;
            recordedBars++;
        }
    }

    int predictSection(float currentEnergy) {
        if (!hasLearned) {
            if (currentEnergy > 0.55f) return 1; // Chorus
            if (currentEnergy < 0.25f) return 0; // Verse
            return 0;
        }
        float dVerse = fabsf(currentEnergy - verseMeanEnergy);
        float dChorus = fabsf(currentEnergy - chorusMeanEnergy);
        return (dChorus < dVerse) ? 1 : 0;
    }
};

// ── 16-Step Pattern Definition Structure ──
struct DrumPattern {
    uint16_t kick;
    uint16_t snare;
    uint16_t hihat;
    uint16_t openHH;
    uint16_t ride;
    uint16_t crash;
    uint16_t toms;
};

// 17 Styles (including Indie Folk, Folk Rock, Fingerstyle, Grunge, Boogie, Punk, and AI Hybrid)
// 17 Styles × 2 Sections (Verse / Chorus) × 4 Variations = 136 Core Patterns
static const DrumPattern PATTERN_TABLE[17][2][4] = {
    // 0: Classic Rock
    {
        // Verse
        {
            { 0x8200, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8080, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8280, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8208, 0x0820, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus (Rides + Open HH + Punchy Kicks)
        {
            { 0x8A82, 0x0820, 0x0000, 0x0400, 0xAAAA, 0x8000, 0x0000 },
            { 0x8A8A, 0x0820, 0x0000, 0x0404, 0xAAAA, 0x0000, 0x0000 },
            { 0x828A, 0x0828, 0x0000, 0x0004, 0xAAAA, 0x8000, 0x0000 },
            { 0xAA82, 0x0820, 0x0000, 0x0400, 0xAAAA, 0x0000, 0x0000 }
        }
    },
    // 1: Pop / Modern Rock
    {
        // Verse
        {
            { 0x8080, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8082, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8090, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8280, 0x0820, 0xAAAA, 0x0200, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8A82, 0x0820, 0xEEEE, 0x0101, 0x0000, 0x8000, 0x0000 },
            { 0x8A92, 0x0820, 0xEEEE, 0x0100, 0x0000, 0x0000, 0x0000 },
            { 0x828A, 0x0822, 0xEEEE, 0x0001, 0x0000, 0x8000, 0x0000 },
            { 0xAA82, 0x0820, 0xEEEE, 0x0101, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 2: Funk / Syncopated
    {
        // Verse
        {
            { 0x9042, 0x0824, 0xEEEE, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x9102, 0x0824, 0xEEEE, 0x0040, 0x0000, 0x0000, 0x0000 },
            { 0x9048, 0x0824, 0xEEEE, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x9242, 0x0824, 0xEEEE, 0x0400, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0xB242, 0x0825, 0xEEEE, 0x0202, 0x0000, 0x8000, 0x0000 },
            { 0x9342, 0x0825, 0xEEEE, 0x0404, 0x0000, 0x0000, 0x0000 },
            { 0xB04A, 0x0825, 0xEEEE, 0x0200, 0x0000, 0x8000, 0x0000 },
            { 0x9A42, 0x0825, 0xEEEE, 0x0402, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 3: Blues Shuffle
    {
        // Verse
        {
            { 0x8080, 0x0820, 0x2492, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8082, 0x0820, 0x2492, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8280, 0x0820, 0x2492, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8088, 0x0820, 0x2492, 0x0040, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8888, 0x0820, 0x0000, 0x0000, 0x2492, 0x8000, 0x0000 },
            { 0x8A88, 0x0820, 0x0000, 0x0040, 0x2492, 0x0000, 0x0000 },
            { 0x888A, 0x0828, 0x0000, 0x0000, 0x2492, 0x8000, 0x0000 },
            { 0x8A8A, 0x0820, 0x0000, 0x0040, 0x2492, 0x0000, 0x0000 }
        }
    },
    // 4: Motown / R&B
    {
        // Verse
        {
            { 0x8282, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x828A, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8A82, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8282, 0x0828, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus (Four on the Floor Snare & Tambourine feel)
        {
            { 0x8888, 0x2A2A, 0xAAAA, 0x0000, 0x0000, 0x8000, 0x0000 },
            { 0x8A88, 0x2A2A, 0xAAAA, 0x0200, 0x0000, 0x0000, 0x0000 },
            { 0x888A, 0x2A2A, 0xAAAA, 0x0000, 0x0000, 0x8000, 0x0000 },
            { 0x8A8A, 0x2A2A, 0xAAAA, 0x0202, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 5: Jazz / Swing
    {
        // Verse
        {
            { 0x8000, 0x0020, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 },
            { 0x8002, 0x0020, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 },
            { 0x8000, 0x0220, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 },
            { 0x8008, 0x0020, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8202, 0x0820, 0x0000, 0x0000, 0x24B2, 0x8000, 0x0000 },
            { 0x8082, 0x0820, 0x0000, 0x0040, 0x24B2, 0x0000, 0x0000 },
            { 0x8280, 0x0822, 0x0000, 0x0000, 0x24B2, 0x8000, 0x0000 },
            { 0x8282, 0x0820, 0x0000, 0x0040, 0x24B2, 0x0000, 0x0000 }
        }
    },
    // 6: Bossa / Latin
    {
        // Verse
        {
            { 0x8484, 0x0421, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8484, 0x0424, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8484, 0x0421, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8484, 0x04A4, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8C8C, 0x0425, 0xAAAA, 0x0100, 0x0000, 0x8000, 0x0000 },
            { 0x8C8C, 0x04A5, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8C8C, 0x042D, 0xAAAA, 0x0100, 0x0000, 0x8000, 0x0000 },
            { 0x8C8C, 0x04A5, 0xAAAA, 0x0200, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 7: Country Train
    {
        // Verse
        {
            { 0x8080, 0x2A2A, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 },
            { 0x8082, 0x2A2A, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 },
            { 0x8280, 0x2A2A, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 },
            { 0x8088, 0x2A2A, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8888, 0x2A2A, 0x0000, 0x0000, 0xEEEE, 0x8000, 0x0000 },
            { 0x8A88, 0x2A2A, 0x0000, 0x0200, 0xEEEE, 0x0000, 0x0000 },
            { 0x888A, 0x2A2A, 0x0000, 0x0000, 0xEEEE, 0x8000, 0x0000 },
            { 0x8A8A, 0x2A2A, 0x0000, 0x0202, 0xEEEE, 0x0000, 0x0000 }
        }
    },
    // 8: Reggae One Drop
    {
        // Verse (Empty beat 1, heavy drop on beat 3)
        {
            { 0x0080, 0x0080, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x0082, 0x0080, 0xAAAA, 0x0200, 0x0000, 0x0000, 0x0000 },
            { 0x0080, 0x0080, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x0088, 0x0080, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x0280, 0x0080, 0xEEEE, 0x0200, 0x0000, 0x8000, 0x0000 },
            { 0x0082, 0x0080, 0xEEEE, 0x0400, 0x0000, 0x0000, 0x0000 },
            { 0x0282, 0x0080, 0xEEEE, 0x0200, 0x0000, 0x8000, 0x0000 },
            { 0x0088, 0x0080, 0xEEEE, 0x0404, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 9: Half-Time Heavy
    {
        // Verse
        {
            { 0x8000, 0x0080, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8002, 0x0080, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8200, 0x0080, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8008, 0x0080, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8802, 0x0080, 0x0000, 0x0400, 0xAAAA, 0x8000, 0x0000 },
            { 0x8A02, 0x0080, 0x0000, 0x0404, 0xAAAA, 0x0000, 0x0000 },
            { 0x880A, 0x0080, 0x0000, 0x0004, 0xAAAA, 0x8000, 0x0000 },
            { 0x8A0A, 0x0080, 0x0000, 0x0400, 0xAAAA, 0x0000, 0x0000 }
        }
    },
    // 10: Indie Folk / Strum (Driving kick + floor tom/tambourine feel, sparse snare)
    {
        // Verse
        {
            { 0x8888, 0x0020, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8A88, 0x0020, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x888A, 0x0020, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8A8A, 0x0022, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus (Big stomping kick + tambourine-accent open hats + rimshot)
        {
            { 0xAAAA, 0x0820, 0xEEEE, 0x0202, 0x0000, 0x8000, 0x0000 },
            { 0xAAAA, 0x0824, 0xEEEE, 0x0404, 0x0000, 0x0000, 0x0000 },
            { 0xBAAA, 0x0820, 0xEEEE, 0x0202, 0x0000, 0x8000, 0x0000 },
            { 0xAAAA, 0x0828, 0xEEEE, 0x0404, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 11: Folk Rock 6/8 Ballad (Triplet lilt, rolling ride + cross-stick)
    {
        // Verse
        {
            { 0x8040, 0x0400, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 },
            { 0x8042, 0x0400, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 },
            { 0x8040, 0x0404, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 },
            { 0x8048, 0x0400, 0x0000, 0x0000, 0x2492, 0x0000, 0x0000 }
        },
        // Chorus (Full 6/8 heavy snare crash on beat 4, rolling toms)
        {
            { 0x8242, 0x0404, 0x0000, 0x0040, 0x24B2, 0x8000, 0x0000 },
            { 0x824A, 0x0404, 0x0000, 0x0000, 0x24B2, 0x0000, 0x0040 },
            { 0x8A42, 0x0404, 0x0000, 0x0040, 0x24B2, 0x8000, 0x0000 },
            { 0x824E, 0x0404, 0x0000, 0x0000, 0x24B2, 0x0000, 0x0080 }
        }
    },
    // 12: Acoustic Fingerstyle (Subtle brushed snare, gentle kick, pedal hi-hat)
    {
        // Verse
        {
            { 0x8000, 0x0020, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 },
            { 0x8002, 0x0020, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 },
            { 0x8000, 0x0020, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 },
            { 0x8004, 0x0020, 0x0000, 0x0000, 0xAAAA, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8080, 0x0820, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 },
            { 0x8082, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8280, 0x0820, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 },
            { 0x8088, 0x0820, 0xAAAA, 0x0200, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 13: Grunge / Alt 90s (Quiet hi-hat verse, massive sludgy chorus)
    {
        // Verse
        {
            { 0x8080, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8082, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8280, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8088, 0x0820, 0xAAAA, 0x0080, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus (Grohl-style heavy crashes on every beat + syncopated doubles)
        {
            { 0x8A82, 0x0820, 0x0000, 0x0000, 0x0000, 0x8888, 0x0000 },
            { 0x8A8A, 0x0820, 0x0000, 0x0000, 0x0000, 0x8888, 0x0000 },
            { 0x828A, 0x0828, 0x0000, 0x0000, 0x0000, 0x8888, 0x0000 },
            { 0xAA82, 0x0820, 0x0000, 0x0000, 0x0000, 0x8888, 0x0000 }
        }
    },
    // 14: Southern Rock Boogie (Four-on-floor kick + driving shuffle ride)
    {
        // Verse
        {
            { 0x8888, 0x0820, 0x2492, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8A88, 0x0820, 0x2492, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x888A, 0x0820, 0x2492, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8A8A, 0x0828, 0x2492, 0x0040, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0x8888, 0x0820, 0x0000, 0x0000, 0x24B2, 0x8000, 0x0000 },
            { 0x8A88, 0x0820, 0x0000, 0x0040, 0x24B2, 0x0000, 0x0000 },
            { 0x888A, 0x0828, 0x0000, 0x0000, 0x24B2, 0x8000, 0x0000 },
            { 0x8A8A, 0x0820, 0x0000, 0x0040, 0x24B2, 0x0000, 0x0000 }
        }
    },
    // 15: Punk / Energetic Drive (Fast 8th-note kick + continuous snare drive)
    {
        // Verse
        {
            { 0xAAAA, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0xAAAA, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0xAAAA, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0xAAAA, 0x0828, 0xAAAA, 0x0100, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus
        {
            { 0xAAAA, 0x0820, 0xEEEE, 0x0202, 0x0000, 0x8000, 0x0000 },
            { 0xBAAA, 0x0820, 0xEEEE, 0x0404, 0x0000, 0x0000, 0x0000 },
            { 0xAAAA, 0x0828, 0xEEEE, 0x0202, 0x0000, 0x8000, 0x0000 },
            { 0xBAAA, 0x0820, 0xEEEE, 0x0404, 0x0000, 0x0000, 0x0000 }
        }
    },
    // 16: AI Hybrid / Adaptive Brain (Dynamic morphing pattern that adapts to song input)
    {
        // Verse (Starts clean, syncopates with guitar)
        {
            { 0x8080, 0x0820, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x9042, 0x0824, 0xEEEE, 0x0040, 0x0000, 0x0000, 0x0000 },
            { 0x8280, 0x0820, 0x2492, 0x0000, 0x0000, 0x0000, 0x0000 },
            { 0x8484, 0x0421, 0xAAAA, 0x0000, 0x0000, 0x0000, 0x0000 }
        },
        // Chorus (Pushes energy, switches between heavy rock, funk accents, and half-time)
        {
            { 0x8A82, 0x0820, 0x0000, 0x0400, 0xAAAA, 0x8000, 0x0000 },
            { 0xB242, 0x0825, 0xEEEE, 0x0202, 0x0000, 0x8000, 0x0000 },
            { 0x8802, 0x0080, 0x0000, 0x0400, 0xAAAA, 0x8000, 0x0000 },
            { 0x8888, 0x2A2A, 0xAAAA, 0x0200, 0x0000, 0x8000, 0x0000 }
        }
    }
};

// ── 16 Dedicated Dynamic Drum Fills ──
// Grouped musically by character:
// [0..5]   BALLAD / GENTLE FILLS: Sparse, tasteful, musical pickups on beats 3 & 4 (cross-stick / rim / ride / soft tom / gentle snare). NO machine-gun rolls!
// [6..11]  MID / GROOVE FILLS: Motown, funk staccato, syncopated toms, blues turnaround.
// [12..15] DRIVING / HEAVY FILLS: Half-bar slams, descending kit runs, energetic climax.
static const DrumPattern FILL_TABLE[16] = {
    // ── BALLAD / GENTLE FILLS (Indexes 0..5) ──
    // 0: Ballad Soft Snare Pickup (gentle snare on step 10 & 14, soft kick on 8)
    { 0x0080, 0x0022, 0x0000, 0x0000, 0x00AA, 0x0000, 0x0000 },
    // 1: Ballad Snare & Low Tom Pickup (snare on 10, gentle low floor tom on 14)
    { 0x0080, 0x0020, 0x0000, 0x0000, 0x00AA, 0x0000, 0x0002 },
    // 2: Ballad Ride Bell Lilt with Kick on 8 and Soft Snare tap on 14 ("and" of 4)
    { 0x0080, 0x0002, 0x0000, 0x0000, 0x00AA, 0x0000, 0x0000 },
    // 3: Ballad Gentle Mid-Tom & Snare (step 12 & 14)
    { 0x0080, 0x0002, 0x0000, 0x0000, 0x00A0, 0x0000, 0x0008 },
    // 4: Ballad Tasteful Ghost Triplet Pickup (light ghost taps on steps 11, 13, 14)
    { 0x0080, 0x0016, 0x0000, 0x0000, 0x0080, 0x0000, 0x0000 },
    // 5: Ballad Subtle Hi-Hat Open Splash & Cross-Stick (open HH splash on 14, kick on 8)
    { 0x0080, 0x0020, 0x0000, 0x0002, 0x00A8, 0x0000, 0x0000 },

    // ── MID-ENERGY GROOVE FILLS (Indexes 6..11) ──
    // 6: Funk Ghost Staccato
    { 0x9042, 0x087F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    // 7: Motown 8th Roll
    { 0x8888, 0x00AA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    // 8: Syncopated Tom Roll
    { 0x8002, 0x0033, 0x0000, 0x0000, 0x0000, 0x0000, 0x00CC },
    // 9: Blues Turnaround Shuffle
    { 0x8080, 0x0066, 0x0000, 0x0000, 0x2492, 0x0000, 0x0099 },
    // 10: Bossa Double-Hit Syncopation
    { 0x8484, 0x005A, 0x0000, 0x0000, 0x0000, 0x0000, 0x00A5 },
    // 11: Classic Rock Snare & Kick
    { 0x8282, 0x08A8, 0x0000, 0x0000, 0x0000, 0x0000, 0x0055 },

    // ── DRIVING / HEAVY FILLS (Indexes 12..15) ──
    // 12: High Tom -> Low Tom fill
    { 0x8000, 0x00F0, 0x0000, 0x0000, 0x0000, 0x0000, 0x000F },
    // 13: Heavy Half-Bar Slam
    { 0x8000, 0x00AA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0055 },
    // 14: Descending All-Kit Explosion
    { 0x8088, 0x00C0, 0x0000, 0x0000, 0x0000, 0x0000, 0x003F },
    // 15: Snare 16th roll down
    { 0x8000, 0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }
};

// ── URID Struct for LV2 Atom MIDI communication ──
struct DrummerURIDs {
    LV2_URID atom_Sequence;
    LV2_URID midi_Event;

    void map(LV2_URID_Map* map) {
        atom_Sequence = map->map(map->handle, LV2_ATOM__Sequence);
        midi_Event    = map->map(map->handle, LV2_MIDI__MidiEvent);
    }
};

// ── Plugin State & Execution Context ──
struct CyberEvansDrummerPro {
    LV2_URID_Map* map = nullptr;
    DrummerURIDs urids;
    LV2_Atom_Forge forge;

    const float* in_audio = nullptr;
    float* out_audio = nullptr;
    const LV2_Atom_Sequence* in_control = nullptr;
    LV2_Atom_Sequence* out_midi = nullptr;

    const float* ports[PORT_COUNT] = {};
    float* out_ports[PORT_COUNT] = {};

    double sampleRate = 48000.0;
    BroadbandTransientExtractor pickExtractor;
    GuitarStrumPulseTracker pulseTracker;
    SongMemoryEngine songMemory;

    // Rhythmic Clock
    double clockPhase = 0.0;
    int currentStep = 0;
    int currentBar = 0;
    float smoothedEffectiveBpm = 120.0f;

    // Guitar Dynamics Tracking
    float rollingRms50ms = 0.0f;
    float rollingRms500ms = 0.0f;
    float rollingRms4s = 0.0f;
    float prevEnergy500ms = 0.0f;
    float energySlope = 0.0f;
    int risingEnergyBeats = 0;
    // Guitar Input Gate & AGC (from Bass Player)
    float inputPeakFollower = 0.05f;
    float gateEnv = 0.0f;
    bool  guitarGateIsOpen = false;
    int   gateHoldCounter = 0;
    int   strumHoldStepsRemaining = 0;
    bool  singleStrumEndingFired = false;

    // Smart Musical Articulation & Ring-Out Detection
    int   strumCountInBar = 0;
    int   lastHitStep = -1;
    bool  ringingSustainState = false;

    // Accent Map per Bar (16 16th slots)
    float accentMap[16] = {};
    float prevBarAccentMap[16] = {}; // Learned from previous bar for guitar-syncopated improv
    int barHitCount = 0;
    int prevBarHitCount = 0;

    // Momentary Button Triggers
    bool prevStopBtn = false;
    bool prevFillBtn = false;
    bool prevChorusBtn = false;
    bool prevTapBtn  = false;
    bool prevResetBtn = false;
    bool prevStartBtn = false;
    bool startArmed   = false;
    bool manualStopActive = false;
    bool manualFillActive = false;
    bool hasStartedPlayback = false;
    bool pendingStopCrash = false;

    // ── Count-In Listener & Chord Progression Tracker ──
    // When the guitarist first plays, the drummer listens to gauge tempo AND chord
    // progression cycle length, waiting for two rounds of the progression to repeat
    // before entering cleanly on the next downbeat with a musical fill.
    ChordCycleTracker chordTracker;
    bool isListening          = false; // True while counting the pre-entry rounds
    int  listenBarsElapsed    = 0;     // Bars counted while listening
    int  listenTargetBars     = 4;     // Dynamically set to 2 rounds of detected chord progression (or default 4 bars)
    int  listenStepsRemaining = 0;     // Fallback step countdown if needed

    // Fill Engine
    bool fillActive = false;
    int fillIndex = 0;
    int fillStartStep = 12; // Start on beat 4 (step 12) or beat 3 (step 8)
    bool crashPendingNextBar = false;
    int barsSinceAutoFill = 0; // Bars elapsed since last auto-fill (prevents over-filling)
    bool isBalladFeel = false; // Detected gentle / ballad playing character

    // Pick fill index matching song style & guitar dynamics:
    int pickFillIndex(int effStyle) const {
        if (isBalladFeel || effStyle == 10 || effStyle == 11 || effStyle == 12) {
            // Pick from gentle ballad fills [0..5] (sparse pickups, soft snare/tom/ride)
            return rand() % 6;
        } else if (effStyle == 13 || effStyle == 15 || rollingRms500ms > 0.40f) {
            // Rock climax / Punk / Grunge: pick from heavy fills [12..15]
            return 12 + (rand() % 4);
        } else {
            // Groove / Funk / Pop / Rock: pick from mid-energy groove fills [6..11]
            return 6 + (rand() % 6);
        }
    }

    // Section Management
    int activeSection = 0; // 0 = Verse, 1 = Chorus
    int prevSection = 0;

    // Strum-Only Transient Gate
    int strumWindowSamplesRemaining = 0;
    float lastSentKitVol = -1.0f;

    // ── Tap Tempo Engine ──
    // Stores up to 4 inter-tap intervals (in samples). Circular buffer.
    static const int TAP_MAX = 4;
    int   tapIntervals[TAP_MAX] = {};
    int   tapCount      = 0;       // How many valid taps so far (max TAP_MAX)
    int   tapSinceLastSamples = 0; // Samples elapsed since last tap
    bool  tapActive     = false;   // True while taps are being collected
    float tapSeededBpm  = 0.0f;    // Last BPM seeded from tap (debug)

    // ── Silence Auto-Stop Engine ──
    int  silenceSamples     = 0;       // Samples of continuous silence so far
    bool silenceStopped     = false;   // True if we auto-stopped due to silence
    int  samplesSinceLastHit = 0;      // Samples since last guitar transient (for silence gate)

    // ── Song Library (Persistent) ──
    SongLibrary songLib;
    float  liveMatchEnergy[64] = {};   // Bar energies accumulated for song recognition
    int    liveMatchBarCount   = 0;    // Bars accumulated so far (resets on new recording)
    int    songMatchedIdx      = -1;   // Index in songLib.songs[] if matched, else -1

    // Note Off Queuing (to emit proper MIDI Note-Offs)
    struct ActiveNote {
        uint8_t note;
        int framesRemaining;
    };
    std::vector<ActiveNote> activeNotes;
    uint32_t lastMidiFrameOffset = 0;

    void resetToListen() {
        clockPhase = 0.0;
        currentStep = 0;
        currentBar = 0;
        hasStartedPlayback = false;
        startArmed = false;
        manualStopActive = false;
        pendingStopCrash = false;
        fillActive = false;
        isListening = false;
        listenBarsElapsed = 0;
        listenTargetBars = 10;
        chordTracker.reset();
        pulseTracker.reset();
        silenceStopped = false;
        silenceSamples = 0;
        samplesSinceLastHit = 0;
        barHitCount = 0;
        strumCountInBar = 0;
        barsSinceAutoFill = 0;
        isBalladFeel = false;
        liveMatchBarCount = 0;
        songMatchedIdx = -1;
        memset(accentMap, 0, sizeof(accentMap));
        memset(prevBarAccentMap, 0, sizeof(prevBarAccentMap));
    }

    CyberEvansDrummerPro(double rate, LV2_URID_Map* uridMap) {
        sampleRate = rate;
        map = uridMap;
        if (map) urids.map(map);
        lv2_atom_forge_init(&forge, map);

        pickExtractor.init((float)rate);
        chordTracker.init((float)rate);
        pulseTracker.reset();
    }

    void queueNote(uint8_t note, uint8_t velocity, uint32_t frameOffset) {
        if (!out_midi) return;
        if (frameOffset < lastMidiFrameOffset) {
            frameOffset = lastMidiFrameOffset;
        }
        lastMidiFrameOffset = frameOffset;

        uint8_t msg[3] = { 0x99, note, velocity }; // Channel 10 (0x99)
        lv2_atom_forge_frame_time(&forge, frameOffset);
        lv2_atom_forge_atom(&forge, 3, urids.midi_Event);
        lv2_atom_forge_write(&forge, msg, 3);

        ActiveNote an;
        an.note = note;
        an.framesRemaining = (int)(0.060 * sampleRate); // 60ms gate duration
        activeNotes.push_back(an);
    }

    void queueCC(uint8_t cc, uint8_t val, uint32_t frameOffset) {
        if (!out_midi) return;
        uint8_t msg[3] = { 0xB9, cc, val }; // Channel 10 (0xB9) Control Change
        lv2_atom_forge_frame_time(&forge, frameOffset);
        lv2_atom_forge_atom(&forge, 3, urids.midi_Event);
        lv2_atom_forge_write(&forge, msg, 3);
    }

    void processNoteOffs(uint32_t sampleCount) {
        if (!out_midi) return;
        for (auto it = activeNotes.begin(); it != activeNotes.end(); ) {
            it->framesRemaining -= sampleCount;
            if (it->framesRemaining <= 0) {
                uint8_t msg[3] = { 0x89, it->note, 0x00 }; // Note-Off Channel 10
                lv2_atom_forge_frame_time(&forge, 0);
                lv2_atom_forge_atom(&forge, 3, urids.midi_Event);
                lv2_atom_forge_write(&forge, msg, 3);
                it = activeNotes.erase(it);
            } else {
                ++it;
            }
        }
    }
};

static LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*, const LV2_Feature* const* features) {
    LV2_URID_Map* map = nullptr;
    for (int i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            map = (LV2_URID_Map*)features[i]->data;
            break;
        }
    }
    CyberEvansDrummerPro* p = new CyberEvansDrummerPro(rate, map);
    p->songLib.loadAll();
    return (LV2_Handle)p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    CyberEvansDrummerPro* p = (CyberEvansDrummerPro*)instance;
    if (!p) return;

    if (port < PORT_COUNT) {
        p->ports[port] = (const float*)data;
        p->out_ports[port] = (float*)data;
    }
    if (port == PORT_AUDIO_IN)   p->in_audio = (const float*)data;
    if (port == PORT_AUDIO_THRU) p->out_audio = (float*)data;
    if (port == PORT_CONTROL)    p->in_control = (const LV2_Atom_Sequence*)data;
    if (port == PORT_MIDI_OUT)   p->out_midi = (LV2_Atom_Sequence*)data;
}

static void activate(LV2_Handle instance) {
    CyberEvansDrummerPro* p = (CyberEvansDrummerPro*)instance;
    if (!p) return;
    p->resetToListen();
}

static void run(LV2_Handle instance, uint32_t sample_count) {
    CyberEvansDrummerPro* p = (CyberEvansDrummerPro*)instance;
    if (!p || sample_count == 0) return;

    // Passthrough guitar audio if wired
    if (p->out_audio && p->in_audio && p->out_audio != p->in_audio) {
        memcpy(p->out_audio, p->in_audio, sample_count * sizeof(float));
    }

    bool bypass = (p->ports[PORT_BYPASS] && *p->ports[PORT_BYPASS] < 0.5f);
    if (bypass) return;

    // Prepare Atom Forge for MIDI out
    LV2_Atom_Forge_Frame seqFrame;
    const uint32_t capacity = p->out_midi ? p->out_midi->atom.size : 0;
    if (p->out_midi) {
        lv2_atom_forge_set_buffer(&p->forge, (uint8_t*)p->out_midi, capacity);
        lv2_atom_forge_sequence_head(&p->forge, &seqFrame, 0);
    }
    p->lastMidiFrameOffset = 0;

    // Process queued Note-Offs
    p->processNoteOffs(sample_count);

    // Read control parameters
    int   styleIndex    = p->ports[PORT_STYLE] ? (int)(*p->ports[PORT_STYLE] + 0.5f) : 0;
    styleIndex = std::max(0, std::min(17, styleIndex));
    float bpmOverride   = p->ports[PORT_BPM_OVERRIDE] ? *p->ports[PORT_BPM_OVERRIDE] : 0.0f;
    float accentSens    = p->ports[PORT_ACCENT_SENS] ? *p->ports[PORT_ACCENT_SENS] : 5.0f;
    float ghostDensity  = p->ports[PORT_GHOST_DENSITY] ? *p->ports[PORT_GHOST_DENSITY] : 3.0f;
    float fillFreq      = p->ports[PORT_FILL_DENSITY] ? *p->ports[PORT_FILL_DENSITY] : 5.0f;
    float groovePort    = p->ports[PORT_GROOVE] ? *p->ports[PORT_GROOVE] : 4.0f;
    float masterVel     = p->ports[PORT_OVERALL_VEL] ? *p->ports[PORT_OVERALL_VEL] : 7.0f;
    float openHhThresh  = p->ports[PORT_OPEN_HH_THRESH] ? *p->ports[PORT_OPEN_HH_THRESH] : 6.0f;
    bool  crashOnSec    = (!p->ports[PORT_CRASH_ON_CHORUS] || *p->ports[PORT_CRASH_ON_CHORUS] > 0.5f);
    bool  recordSong    = (p->ports[PORT_RECORD_SONG] && *p->ports[PORT_RECORD_SONG] > 0.5f);
    bool  muteDrums     = (p->ports[PORT_MUTE] && *p->ports[PORT_MUTE] > 0.5f);
    bool  adventurous   = (p->ports[PORT_ADVENTUROUS] && *p->ports[PORT_ADVENTUROUS] > 0.5f);
    bool  strumOnlyMode = (p->ports[PORT_STRUM_ONLY_MODE] && *p->ports[PORT_STRUM_ONLY_MODE] > 0.5f);
    float kitVolPort    = p->ports[PORT_KIT_MASTER_VOL] ? *p->ports[PORT_KIT_MASTER_VOL] : 100.0f;
    bool  stopBtnNow    = (p->ports[PORT_STOP_CRASH] && *p->ports[PORT_STOP_CRASH] > 0.5f);
    bool  fillBtnNow    = (p->ports[PORT_FILL_NOW] && *p->ports[PORT_FILL_NOW] > 0.5f);
    bool  chorusBtnNow  = (p->ports[PORT_MANUAL_CHORUS] && *p->ports[PORT_MANUAL_CHORUS] > 0.5f);
    bool  tapBtnNow     = (p->ports[PORT_TAP_TEMPO] && *p->ports[PORT_TAP_TEMPO] > 0.5f);
    bool  resetBtnNow   = (p->ports[PORT_RESET_LISTEN] && *p->ports[PORT_RESET_LISTEN] > 0.5f);
    bool  startBtnNow   = (p->ports[PORT_START_PLAY] && *p->ports[PORT_START_PLAY] > 0.5f);
    float silenceGate   = p->ports[PORT_SILENCE_GATE] ? *p->ports[PORT_SILENCE_GATE] : 0.0f;
    // silenceGate: 0 = disabled, 1–10 = seconds of silence before auto-stop

    // ── MOMENTARY BUTTON ACTIONS ──
    // Reset Button: Arm reset back to count-in listen state for a new song
    if (resetBtnNow && !p->prevResetBtn) {
        p->resetToListen();
    }
    p->prevResetBtn = resetBtnNow;

    // Start Button: Arm drummer to enter cleanly on next downbeat
    if (startBtnNow && !p->prevStartBtn) {
        if (!p->hasStartedPlayback) {
            p->startArmed = true;
            p->manualStopActive = false;
        }
    }
    p->prevStartBtn = startBtnNow;

    // Stop Button: Edge-detect tap or hold
    if (stopBtnNow && !p->prevStopBtn) {
        p->startArmed = false;
        if (!p->manualStopActive) {
            // User requested STOP: Arm clean stop with Crash & Kick on the next step 0 / downbeat (or immediately if not yet playing)
            if (!p->hasStartedPlayback) {
                p->manualStopActive = true;
            } else {
                p->pendingStopCrash = true;
            }
        } else {
            // User pressed STOP again: arm start for next downbeat
            p->startArmed = true;
        }
    }
    p->prevStopBtn = stopBtnNow;

    // Fill Button: User pressed FILL HERE button -> arm immediate dynamic fill
    if (fillBtnNow && !p->prevFillBtn) {
        p->fillActive = true;
        p->fillIndex = p->pickFillIndex(styleIndex);
        p->fillStartStep = (p->currentStep < 8) ? 8 : (p->currentStep + 1);
    }
    p->prevFillBtn = fillBtnNow;

    // Chorus Button: Manually switch section between Verse (0) and Chorus (1)
    if (chorusBtnNow != p->prevChorusBtn) {
        int targetSec = chorusBtnNow ? 1 : 0;
        if (p->activeSection != targetSec) {
            p->prevSection = p->activeSection;
            p->activeSection = targetSec;
            if (crashOnSec && targetSec == 1) {
                p->crashPendingNextBar = true; // Cymbal crash entering chorus
            }
        }
    }
    p->prevChorusBtn = chorusBtnNow;

    // ── TAP TEMPO ──
    // Each button press edge records the gap since the last tap.
    // After 2+ taps, average the intervals → seed estimatedBpm.
    // Taps expire after 3 seconds (reset the collection).
    p->tapSinceLastSamples += (int)sample_count;
    if (p->tapSinceLastSamples > (int)(3.0f * p->sampleRate)) {
        // Long gap — next tap starts a fresh collection
        p->tapCount = 0;
        p->tapActive = false;
    }
    if (tapBtnNow && !p->prevTapBtn) {
        int gap = p->tapSinceLastSamples;
        p->tapSinceLastSamples = 0;
        // Only count if gap is in musical range (40–240 BPM = 0.25–1.5s)
        int minGap = (int)(0.25f * p->sampleRate);
        int maxGap = (int)(1.5f  * p->sampleRate);
        if (gap >= minGap && gap <= maxGap && p->tapActive) {
            // Store interval (circular, cap at TAP_MAX)
            int slot = p->tapCount % p->TAP_MAX;
            p->tapIntervals[slot] = gap;
            p->tapCount = std::min(p->tapCount + 1, p->TAP_MAX);
            // Average available intervals → BPM
            int count = std::min(p->tapCount, p->TAP_MAX);
            float avgGap = 0.0f;
            for (int t = 0; t < count; ++t) avgGap += p->tapIntervals[t];
            avgGap /= (float)count;
            float tapBpm = (p->sampleRate * 60.0f) / avgGap;
            tapBpm = std::max(40.0f, std::min(240.0f, tapBpm));
            // Seed the pulse tracker: hard-set BPM and mark as warmed up
            p->pulseTracker.estimatedBpm = tapBpm;
            p->pulseTracker.strumsSinceReset = 8; // Skip slow-start alpha ramp
            p->tapSeededBpm = tapBpm;
        }
        p->tapActive = true; // Mark that at least one tap has landed
    }
    p->prevTapBtn = tapBtnNow;

    // ── SILENCE AUTO-STOP ──
    // Counts samples since the last guitar transient (samplesSinceLastHit).
    // This is reset to 0 on every pick/strum, so it measures true silence from
    // the last note — NOT from when the 3s strum window expires.
    // When silence exceeds the gate threshold, stop the drummer immediately and
    // silently (no crash or kick). Guitar strumming again restarts the drummer.
    p->samplesSinceLastHit += (int)sample_count;
    if (silenceGate > 0.5f && p->hasStartedPlayback && !p->manualStopActive) {
        int thresholdSamples = (int)(silenceGate * p->sampleRate);
        if (p->samplesSinceLastHit >= thresholdSamples) {
            if (!p->silenceStopped) {
                // Stop immediately, no crash, no kick
                p->manualStopActive = true;
                p->silenceStopped   = true;
            }
        }
    }
    // Resume: if guitar comes back in while silence-stopped, clear the flag
    if (p->silenceStopped && p->samplesSinceLastHit == 0) {
        p->silenceStopped   = false;
        p->silenceSamples   = 0;
        p->manualStopActive = false;
    }

    // Send MIDI CC 7 (Channel Volume) if changed to control Black Pearl / Red Zeppelin level
    uint8_t kitVolCC = (uint8_t)std::max(0.0f, std::min(127.0f, kitVolPort));
    if ((float)kitVolCC != p->lastSentKitVol) {
        p->queueCC(7, kitVolCC, 0); // CC 7 = Master Volume
        p->lastSentKitVol = (float)kitVolCC;
    }

    // AI Hybrid Style Logic: If style == 16 (Hybrid Brain), dynamically select
    // based on tempo and playing density learned from the guitar
    int effectiveStyle = styleIndex;
    if (styleIndex == 16) {
        float bpm = (bpmOverride >= 50.0f) ? bpmOverride : p->pulseTracker.estimatedBpm;
        if (p->isBalladFeel || (p->rollingRms500ms < 0.08f && p->prevBarHitCount <= 4)) {
            // Ballad strumming detected: select gentle styles regardless of raw pulse tempo
            if (bpm < 90.0f) {
                effectiveStyle = 11; // 6/8 Folk Ballad
            } else if (bpm < 125.0f) {
                effectiveStyle = 12; // Acoustic Fingerstyle
            } else {
                effectiveStyle = 10; // Indie Folk / Strum
            }
        } else if (bpm < 82.0f) {
            effectiveStyle = (p->rollingRms500ms > 0.35f) ? 9 : 11; // Half-time heavy or 6/8 Folk Ballad
        } else if (bpm < 108.0f) {
            effectiveStyle = (p->rollingRms500ms < 0.20f) ? 10 : 2; // Indie Folk or Funk Syncopated
        } else if (bpm < 135.0f) {
            effectiveStyle = (p->rollingRms500ms > 0.40f) ? 13 : 0; // Grunge Alt or Classic Rock
        } else if (bpm < 160.0f) {
            effectiveStyle = 14; // Southern Rock Boogie
        } else {
            effectiveStyle = 15; // Punk Drive
        }
    }

    // Manage Song Record toggle
    if (recordSong && !p->songMemory.isRecording) {
        p->songMemory.startRecording();
        // Reset live match accumulator when a new recording starts
        p->liveMatchBarCount = 0;
        p->songMatchedIdx    = -1;
    } else if (!recordSong && p->songMemory.isRecording) {
        p->songMemory.stopRecording();
        // Save to disk if recording produced usable data
        if (p->songMemory.hasLearned) {
            SavedSong ss = {};
            ss.magic   = SONG_FILE_MAGIC;
            ss.version = SONG_FILE_VER;
            ss.bpm     = p->pulseTracker.estimatedBpm;
            ss.barCount = p->songMemory.recordedBars;
            ss.verseMeanEnergy  = p->songMemory.verseMeanEnergy;
            ss.chorusMeanEnergy = p->songMemory.chorusMeanEnergy;
            memcpy(ss.barEnergy,   p->songMemory.barEnergy,   sizeof(ss.barEnergy));
            for (int b = 0; b < 64; ++b) ss.barHitCount[b] = (uint8_t)p->songMemory.barHitCount[b];
            snprintf(ss.name, sizeof(ss.name), "Song_%03d", p->songLib.count + 1);
            p->songLib.save(ss);
            // Reset live match so we can recognise this song immediately next time
            p->liveMatchBarCount = 0;
            p->songMatchedIdx    = -1;
        }
    }

    // Determine target BPM from pulseTracker or BPM override
    float targetBpm = 120.0f;
    if (bpmOverride >= 50.0f) {
        targetBpm = bpmOverride;
    } else {
        targetBpm = p->pulseTracker.estimatedBpm;
    }
    targetBpm = std::max(50.0f, std::min(240.0f, targetBpm));

    // Smooth tempo changes smoothly across audio blocks so the drummer never jerks or skips
    float bpmSmoothing = (p->hasStartedPlayback) ? 0.05f : 0.25f;
    p->smoothedEffectiveBpm = p->smoothedEffectiveBpm * (1.0f - bpmSmoothing) + targetBpm * bpmSmoothing;
    float effectiveBpm = p->smoothedEffectiveBpm;

    // Dynamic swing delay calculation
    float swingAmount = (groovePort / 10.0f) * 0.35f; // up to 35% swing offset on odd 16ths

    // Process audio samples for guitar onset & dynamic envelope
    const float* inBuf = p->in_audio;
    float blockEnergySum = 0.0f;

    for (uint32_t i = 0; i < sample_count; ++i) {
        float inSample = inBuf ? inBuf[i] : 0.0f;
        float absSample = fabsf(inSample);
        blockEnergySum += absSample;

        p->pulseTracker.update(1.0f / (float)p->sampleRate);
        p->chordTracker.processSample(inSample);

        // ── Beast Mode AGC & Guitar Input Gate (like Bass Player) ──
        if (absSample > p->inputPeakFollower) {
            p->inputPeakFollower += 0.008f * (absSample - p->inputPeakFollower);
        } else {
            p->inputPeakFollower += 0.000015f * (absSample - p->inputPeakFollower);
        }
        p->inputPeakFollower = std::max(0.005f, std::min(1.5f, p->inputPeakFollower));

        float autoGateBase = p->inputPeakFollower * 0.055f;
        float sensMultiplier = powf(10.0f, (accentSens - 5.0f) * 0.8f / 20.0f);
        float effectiveOpenThresh = std::max(0.0012f, autoGateBase * sensMultiplier);
        float effectiveCloseThresh = effectiveOpenThresh * 0.28f; // -11 dB hysteresis

        // Envelope Follower
        if (absSample > p->gateEnv)
            p->gateEnv += 0.005f * (absSample - p->gateEnv);
        else
            p->gateEnv += 0.0002f * (absSample - p->gateEnv);

        int minHoldSamples = (int)(0.160f * p->sampleRate);

        if (!p->guitarGateIsOpen) {
            if (p->gateEnv >= effectiveOpenThresh) {
                p->guitarGateIsOpen = true;
                p->gateHoldCounter = minHoldSamples;
            }
        } else {
            if (p->gateHoldCounter > 0) p->gateHoldCounter--;
            if (p->gateHoldCounter <= 0 && p->gateEnv < effectiveCloseThresh) {
                p->guitarGateIsOpen = false;
            }
        }

        // Pick transient detector
        bool hit = p->pickExtractor.process(inSample, accentSens, 0.005f);
        if (hit) {
            p->pulseTracker.onStrumHit(absSample);

            // Record raw attack strength
            float strength = std::min(1.0f, absSample * 3.5f);
            p->accentMap[p->currentStep] = std::max(p->accentMap[p->currentStep], strength);
            p->barHitCount++;

            // Count strums within this current bar
            if (p->currentStep != p->lastHitStep) {
                p->strumCountInBar++;
                p->lastHitStep = p->currentStep;
            }

            // Strumming Window:
            // Generous window (3.0 seconds) so spaced chord changes or pauses never cut playback
            p->strumWindowSamplesRemaining = (int)(3.000f * p->sampleRate);
            p->samplesSinceLastHit = 0; // Reset silence gate timer on every transient

            // Listening and tracking always active so tempo, clock, and chords are ready in background
            if (!p->hasStartedPlayback && !p->isListening) {
                p->isListening          = true;
                p->listenBarsElapsed    = 0;
                p->chordTracker.reset();
                p->manualStopActive     = false;
                p->clockPhase           = 0.0;
                p->currentStep          = 0;
            }
        }

        if (p->strumWindowSamplesRemaining > 0) {
            p->strumWindowSamplesRemaining--;
        }

        // Clock Progression (16th-note ticks)
        double clockInc = ((double)effectiveBpm / (60.0 * p->sampleRate)) * 4.0; // 16th clock
        p->clockPhase += clockInc;

        if (p->clockPhase >= 1.0) {
            p->clockPhase -= 1.0;
            int prevStep = p->currentStep;
            p->currentStep = (p->currentStep + 1) % 16;

            if (p->strumHoldStepsRemaining > 0) {
                p->strumHoldStepsRemaining--;
            }

            // Update energy slope once per quarter-note beat (every 4 steps)
            if (p->currentStep % 4 == 0) {
                float dEnergy = p->rollingRms500ms - p->prevEnergy500ms;
                p->prevEnergy500ms = p->rollingRms500ms;
                if (dEnergy > 0.005f) {
                    p->risingEnergyBeats = std::min(8, p->risingEnergyBeats + 1);
                } else if (dEnergy < -0.005f) {
                    p->risingEnergyBeats = std::max(0, p->risingEnergyBeats - 1);
                }
            }

            // Bar Completion Boundary (Downbeat entrance on step 0)
            if (p->currentStep == 0 && prevStep == 15) {
                p->currentBar++;

                // Track chord progression and bars continuously in the background
                p->chordTracker.onBarComplete();
                p->listenBarsElapsed++;

                // If START button was pressed / armed, enter cleanly right now on the downbeat!
                if (p->startArmed) {
                    p->startArmed         = false;
                    p->hasStartedPlayback = true;
                    p->manualStopActive   = false;
                    p->isListening        = false;
                    p->fillActive         = false; // Clean groove entry on downbeat
                    p->barsSinceAutoFill  = 0;
                }

                p->songMemory.onBarFinish(p->rollingRms500ms, p->barHitCount);

                // ── SONG LIBRARY: accumulate + recognise ──
                if (!p->songMemory.isRecording && p->songMatchedIdx < 0 && p->liveMatchBarCount < 64) {
                    p->liveMatchEnergy[p->liveMatchBarCount++] = p->rollingRms500ms;
                    if (p->liveMatchBarCount >= 4) {
                        int idx = p->songLib.tryMatch(p->pulseTracker.estimatedBpm,
                                                      p->liveMatchEnergy, p->liveMatchBarCount);
                        if (idx >= 0) {
                            const SavedSong& s = p->songLib.songs[idx];
                            p->songMemory.verseMeanEnergy  = s.verseMeanEnergy;
                            p->songMemory.chorusMeanEnergy = s.chorusMeanEnergy;
                            memcpy(p->songMemory.barEnergy,   s.barEnergy,   sizeof(p->songMemory.barEnergy));
                            for (int b = 0; b < 64; ++b) p->songMemory.barHitCount[b] = s.barHitCount[b];
                            p->songMemory.recordedBars = s.barCount;
                            p->songMemory.hasLearned   = true;
                            p->songMatchedIdx          = idx;
                        }
                    }
                }
                // If user just started a new recording, reset the accumulator
                if (p->songMemory.isRecording) {
                    p->liveMatchBarCount = 0;
                    p->songMatchedIdx    = -1;
                }

                // Section Assessment (only auto-predict if manual chorus button is off and Song Memory is active)
                if (!chorusBtnNow && p->songMemory.hasLearned) {
                    p->prevSection = p->activeSection;
                    p->activeSection = p->songMemory.predictSection(p->rollingRms500ms);
                    if (crashOnSec && (p->activeSection == 1 && p->prevSection == 0)) {
                        p->crashPendingNextBar = true;
                    }
                }

                // ── BALLAD / GENTLE PLAYING DETECTION ──
                // If guitar strumming is sparse (<= 3 strums per bar) or gentle (RMS < 0.08),
                // detect ballad feel so fills are soft & musical rather than rock rolls!
                if (p->barHitCount <= 3 || p->rollingRms500ms < 0.08f) {
                    p->isBalladFeel = true;
                } else if (p->barHitCount >= 6 && p->rollingRms500ms > 0.18f) {
                    p->isBalladFeel = false;
                }

                // ── AUTO-FILL ENGINE ──
                // Auto-fills fire at bar boundaries under two conditions:
                //   1) Section transition  (verse↔chorus): always fires a fill going into new section.
                //   2) Energy climb fill   (risingEnergyBeats >= 4): fires periodically when the
                //      music is building. Fill Density knob (fillFreq 0-10) controls spacing:
                //      0 = disabled, 10 = every 2 bars while climbing. Min 2-bar gap always enforced.
                // Manual FILL HERE button always wins and is never blocked by this logic.

                p->barsSinceAutoFill++;

                bool autoFillThisBar = false;

                if (!p->fillActive && p->hasStartedPlayback) {

                    // Condition 1: Section transition fill (verse↔chorus or chorus↔verse)
                    bool sectionJustChanged = (p->songMemory.hasLearned &&
                                               p->activeSection != p->prevSection);
                    if (sectionJustChanged && p->barsSinceAutoFill >= 2) {
                        autoFillThisBar = true;
                    }

                    // Condition 2: Energy-climb fill (fillFreq controls how often)
                    if (!autoFillThisBar && fillFreq > 0.5f) {
                        // In ballad mode, require stronger climb and more bar space
                        int minBarGap = (int)(14.0f - fillFreq);
                        if (p->isBalladFeel) minBarGap += 2;
                        if (minBarGap < 2) minBarGap = 2;
                        int minRising = p->isBalladFeel ? 5 : 4;
                        if (p->risingEnergyBeats >= minRising &&
                            p->barsSinceAutoFill >= minBarGap) {
                            autoFillThisBar = true;
                        }
                    }

                    if (autoFillThisBar) {
                        p->fillActive    = true;
                        p->fillIndex     = p->pickFillIndex(effectiveStyle);
                        // Beat 3 (step 8): fill runs back half of bar,
                        // resolves cleanly into next bar's downbeat.
                        p->fillStartStep = 8;
                        p->barsSinceAutoFill = 0;
                    }
                }

                // Clear any fill that ran last bar (manual or auto) — never loop fills
                if (!autoFillThisBar) {
                    p->fillActive = false;
                }

                // Learn from this bar's rhythm into previous bar memory for live guitar improv:
                memcpy(p->prevBarAccentMap, p->accentMap, sizeof(p->accentMap));
                p->prevBarHitCount = p->barHitCount;

                p->barHitCount = 0;
                p->strumCountInBar = 0;
                p->lastHitStep = -1;
                p->ringingSustainState = false;
                memset(p->accentMap, 0, sizeof(p->accentMap));
            }

            // ── TRIGGER DRUM HITS FOR THIS 16TH STEP ──
            bool isGuitarActive = p->guitarGateIsOpen;
            bool isStrummingActive = (p->strumWindowSamplesRemaining > 0);

            // Handle pending stop on phrase / beat boundary
            if (p->pendingStopCrash && (p->currentStep == 0 || p->currentStep % 4 == 0)) {
                p->pendingStopCrash = false;
                p->manualStopActive = true;
                p->queueNote(GM_CRASH_1, 118, i);
                p->queueNote(GM_BASS_DRUM, 110, i);
            }

            // Determine if drummer should play:
            // 1. If user presses STOP button, manualStopActive becomes true -> silent.
            // 2. Playback only begins once the guitarist strums/plays (hasStartedPlayback = true).
            // 3. While isListening (one-bar count-in), drummer is silent even though clock runs.
            // 4. Once started, the drummer keeps time steadily and continuously without stuttering or stopping,
            //    unless the STOP button is pressed or Mute is toggled.
            bool shouldPlay = false;
            if (p->manualStopActive || !p->hasStartedPlayback || p->isListening) {
                shouldPlay = false;
            } else if (strumOnlyMode) {
                shouldPlay = isStrummingActive;
            } else {
                // Rock-solid continuous groove locked to clock
                shouldPlay = true;
            }

            if (!muteDrums && shouldPlay) {
                int step = p->currentStep;
                uint16_t mask = (1 << (15 - step));

                // Select Pattern (base or fill) using effectiveStyle
                DrumPattern pat;
                bool isFillingStep = (p->fillActive && step >= p->fillStartStep);

                if (isFillingStep) {
                    pat = FILL_TABLE[p->fillIndex];
                } else if (effectiveStyle == 17) {
                    // ── 17: GUITAR-DRIVEN IMPROV (Live Syncopated Mirror) ──
                    // Derives the drum performance directly from the guitar's strumming syncopation!
                    // 1. Kick: Mirrors intentional guitar accents (max 3-4 kicks per bar to keep a solid groove)
                    uint16_t kickMask = 0x8000; // Always anchor Beat 1
                    // Only place kick on primary rhythmic pockets (steps 2, 6, 8, 10) when guitar actually accents them
                    int kicksAdded = 1;
                    int prioritySteps[] = { 8, 6, 10, 2, 14 }; // beat 3 downbeat, 'and' of 2, 'and' of 3, 'and' of 1
                    for (int idx = 0; idx < 5; ++idx) {
                        int s = prioritySteps[idx];
                        float learnedAcc = p->prevBarAccentMap[s];
                        float liveAcc = p->accentMap[s];
                        if (kicksAdded < 4 && (learnedAcc > 0.70f || liveAcc > 0.75f)) {
                            kickMask |= (1 << (15 - s));
                            kicksAdded++;
                        }
                    }
                    // 2. Snare: Anchors backbeat on beats 2 & 4 (steps 4 & 12)
                    uint16_t snareMask = 0x0880; // Beat 2 & Beat 4
                    // If guitar has syncopated push before beat 3 (step 6) or beat 4 (step 10/14):
                    if (p->prevBarAccentMap[6] > 0.65f) snareMask |= (1 << (15 - 6));
                    if (p->prevBarAccentMap[14] > 0.65f) snareMask |= (1 << (15 - 14));

                    // 3. Cymbals: Decided by guitar sustain & energy
                    uint16_t hatMask = 0xAAAA; // 8th note pulse
                    uint16_t rideMask = 0x0000;
                    uint16_t openHhMask = 0x0000;
                    if (p->activeSection == 1 || p->rollingRms500ms > 0.38f) {
                        // High energy / Chorus / Jam peak: Ride cymbal wash!
                        rideMask = 0xAAAA;
                        hatMask = 0x0000;
                    } else if (p->rollingRms500ms > 0.22f) {
                        // Driving mid-energy: Sizzling open hi-hat on upbeats
                        openHhMask = 0x2222;
                    }

                    pat.kick = kickMask;
                    pat.snare = snareMask;
                    pat.hihat = hatMask;
                    pat.openHH = openHhMask;
                    pat.ride = rideMask;
                    pat.crash = (step == 0 && (p->currentBar % 4 == 0)) ? 0x8000 : 0x0000;
                    pat.toms = 0x0000;
                } else {
                    int varIdx = (p->currentBar % 4);
                    pat = PATTERN_TABLE[effectiveStyle][p->activeSection][varIdx];
                }

                // Calculate Base Velocity scaled by Master Dynamics
                // Dynamic ceiling clamped to 1.15f so high strum energy never causes drums or fills to jump out disproportionately loud
                float dynamicMultiplier = 0.5f + (p->rollingRms500ms * 2.0f);
                dynamicMultiplier = std::max(0.6f, std::min(1.15f, dynamicMultiplier));
                // In real life, drummers play fills musically and blend them into the groove rather than suddenly getting louder
                float fillTrim = 1.0f;
                if (isFillingStep) {
                    fillTrim = p->isBalladFeel ? 0.78f : 0.88f; // Even gentler in ballad passages
                }
                float velScale = (masterVel / 10.0f) * dynamicMultiplier * fillTrim;

                // Micro-timing Swing offset
                uint32_t triggerOffset = i;
                if ((step % 2) != 0) {
                    triggerOffset += (uint32_t)(swingAmount * (p->sampleRate / (effectiveBpm * 4.0 / 60.0)));
                    triggerOffset = std::min(triggerOffset, sample_count - 1);
                }

                // 1. CRASH CYMBAL
                if (step == 0 && p->crashPendingNextBar) {
                    p->queueNote(GM_CRASH_1, (uint8_t)std::min(127, (int)(110 * velScale)), triggerOffset);
                    p->crashPendingNextBar = false;
                } else if (pat.crash & mask) {
                    p->queueNote(GM_CRASH_1, (uint8_t)std::min(127, (int)(100 * velScale)), triggerOffset);
                }

                // 2. KICK DRUM (Groove Anchored)
                bool kickFires = (pat.kick & mask);
                // Musical Accent: Only reinforce kick if the pattern calls for it OR strong intentional downbeat
                if (!kickFires && (step == 0 || step == 8) && p->accentMap[step] > 0.80f) {
                    kickFires = true;
                }
                if (kickFires) {
                    float accentBoost = isFillingStep ? 1.0f : (1.0f + (p->accentMap[step] * (accentSens / 10.0f) * 0.15f));
                    uint8_t vel = (uint8_t)std::min(127, (int)(95.0f * velScale * accentBoost));
                    p->queueNote(GM_BASS_DRUM, vel, triggerOffset);
                }

                // 3. SNARE (Musical Backbeat)
                bool snareFires = (pat.snare & mask);
                // Musical Snare Accent: DO NOT fire extra snares on random strums!
                // Only fire extra snare if the pattern has it, or on explicit backbeat pushes (steps 6 or 14) with huge energy
                if (!snareFires && !isFillingStep && (step == 6 || step == 14) && (p->accentMap[step] > 0.92f) && (accentSens > 7.0f)) {
                    snareFires = true;
                }
                if (snareFires) {
                    float accentBoost = isFillingStep ? 1.0f : (1.0f + (p->accentMap[step] * (accentSens / 10.0f) * 0.20f));
                    uint8_t vel = (uint8_t)std::min(127, (int)(100.0f * velScale * accentBoost));
                    p->queueNote(GM_SNARE, vel, triggerOffset);
                } else if (!isFillingStep && ghostDensity > 2.0f && (step % 2 != 0) && (rand() % 100 < (int)(ghostDensity * 3.0f))) {
                    // Snare Ghost Note - tasteful, subtle, and only if ghost density turned up
                    uint8_t vel = (uint8_t)std::min(50, (int)(28.0f * velScale));
                    p->queueNote(GM_SNARE, vel, triggerOffset);
                }

                // 4. HI-HAT / RIDE
                if (pat.ride & mask) {
                    uint8_t vel = (uint8_t)std::min(127, (int)(85.0f * velScale));
                    p->queueNote(GM_RIDE_1, vel, triggerOffset);
                } else if (pat.openHH & mask || (p->accentMap[step] > (openHhThresh / 10.0f) && (pat.hihat & mask))) {
                    uint8_t vel = (uint8_t)std::min(127, (int)(92.0f * velScale));
                    p->queueNote(GM_OPEN_HH, vel, triggerOffset);
                } else if (pat.hihat & mask) {
                    uint8_t vel = (uint8_t)std::min(127, (int)(80.0f * velScale));
                    p->queueNote(GM_CLOSED_HH, vel, triggerOffset);
                }

                // 5. TOMS (in fills) — play at a similar level to snare, not louder
                if (pat.toms & mask) {
                    uint8_t tomNote = (step < 12) ? GM_HIGH_TOM : GM_LOW_TOM;
                    uint8_t vel = (uint8_t)std::min(127, (int)(87.0f * velScale)); // slightly softer than snare
                    p->queueNote(tomNote, vel, triggerOffset);
                }

                // 6. ADVENTUROUS / JAM IMPROV
                if (adventurous && (step % 4 == 2) && (rand() % 100 < 25)) {
                    if (rand() % 2 == 0) {
                        p->queueNote(GM_OPEN_HH, (uint8_t)(75 * velScale), triggerOffset);
                    } else {
                        p->queueNote(GM_BASS_DRUM, (uint8_t)(80 * velScale), triggerOffset);
                    }
                }
            }
        }
    }

    // Update Guitar RMS Tracking Windows
    float blockRms = blockEnergySum / (float)sample_count;
    p->rollingRms50ms  = p->rollingRms50ms * 0.90f + blockRms * 0.10f;
    p->rollingRms500ms = p->rollingRms500ms * 0.985f + blockRms * 0.015f;
    p->rollingRms4s    = p->rollingRms4s * 0.998f + blockRms * 0.002f;

    // Write Output Monitored Ports
    if (p->out_ports[PORT_DETECTED_BPM]) {
        *p->out_ports[PORT_DETECTED_BPM] = effectiveBpm;
    }
    if (p->out_ports[PORT_CURRENT_SECTION]) {
        *p->out_ports[PORT_CURRENT_SECTION] = (float)p->activeSection;
    }
    if (p->out_ports[PORT_ENERGY_LEVEL]) {
        *p->out_ports[PORT_ENERGY_LEVEL] = std::min(1.0f, p->rollingRms500ms * 5.0f);
    }
    // Pop Atom Sequence frame
    if (p->out_midi) {
        lv2_atom_forge_pop(&p->forge, &seqFrame);
    }
}

static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    delete (CyberEvansDrummerPro*)instance;
}

static const void* extension_data(const char* uri) {
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

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
