#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <iomanip>

struct GoertzelState {
    float coeff;
    float s1, s2;
    float freq;
    void init(float f, float sr) {
        freq = f;
        float w = 2.0f * M_PI * f / sr;
        coeff = 2.0f * cosf(w);
        reset();
    }
    void reset() { s1 = s2 = 0.0f; }
    inline void process(float x) {
        float s0 = x + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    float getEnergy() const {
        return (s1 * s1 + s2 * s2 - s1 * s2 * coeff);
    }
};

struct ChordCycleTracker2 {
    static const int NUM_FILTERS = 36;
    GoertzelState filters[NUM_FILTERS];
    int blockSamples = 0;
    int sampleCountInBlock = 0;
    float smoothedChroma[12] = {};
    int confirmedChordId = -1;

    int barChords[32] = {};
    int barsObserved = 0;
    int detectedRoundBars = 0;
    int roundsCounted = 0;
    bool cycleLocked = false;

    void init(float sr) {
        for (int i = 0; i < NUM_FILTERS; ++i) {
            int midi = 40 + i;
            float freq = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
            filters[i].init(freq, sr);
        }
        blockSamples = int(0.20f * sr); // 200ms blocks
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
            for (int c = 0; c < 12; ++c) if (rawChroma[c] > maxC) maxC = rawChroma[c];
            for (int c = 0; c < 12; ++c) {
                float norm = rawChroma[c] / maxC;
                smoothedChroma[c] = smoothedChroma[c] * 0.5f + norm * 0.5f;
            }

            int bestSemi = -1;
            float bestVal = 0.5f;
            for (int c = 0; c < 12; ++c) {
                float score = smoothedChroma[c] * 1.5f +
                              smoothedChroma[(c + 7) % 12] * 1.0f +
                              std::max(smoothedChroma[(c + 4) % 12], smoothedChroma[(c + 3) % 12]) * 0.8f;
                if (score > bestVal) {
                    bestVal = score;
                    bestSemi = c;
                }
            }
            if (bestSemi >= 0) confirmedChordId = bestSemi;
        }
    }

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
                    // Check distinct chords in cycle
                    int distinct = 0;
                    for (int b = 0; b < L; ++b) {
                        bool found = false;
                        for (int k = 0; k < b; ++k) {
                            if (barChords[barsObserved - L + b] == barChords[barsObserved - L + k]) {
                                found = true; break;
                            }
                        }
                        if (!found) distinct++;
                    }
                    if (distinct < 2) continue; // Must have at least 2 distinct chords!

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

int main() {
    std::ifstream file("/var/modep/user-files/Audio Recordings/lv2record/mod_session003.wav", std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t fsize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buf(fsize);
    file.read(buf.data(), fsize);

    float* audio = nullptr;
    size_t numSamples = 0;
    int sr = 48000;
    for (size_t i = 0; i + 8 < fsize; ++i) {
        if (memcmp(&buf[i], "data", 4) == 0) {
            uint32_t dataSize = *(uint32_t*)&buf[i + 4];
            numSamples = dataSize / 4;
            audio = (float*)&buf[i + 8];
            break;
        }
    }

    ChordCycleTracker2 ct;
    ct.init((float)sr);
    const char* NOTES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

    int barSamples = int(2.264 * sr);
    int bar = 0;
    for (size_t i = 0; i < numSamples; ++i) {
        ct.processSample(audio[i]);
        if ((i > 0 && i % barSamples == 0) || i + 1 == numSamples) {
            ct.onBarComplete();
            int c = ct.barChords[bar];
            std::cout << "Bar " << std::setw(2) << bar
                      << " (t=" << std::fixed << std::setprecision(1) << (float)i/sr << "s): "
                      << (c >= 0 ? NOTES[c] : "None")
                      << " | cycleLocked=" << ct.cycleLocked
                      << " | roundLen=" << ct.detectedRoundBars
                      << " | rounds=" << ct.roundsCounted << std::endl;
            bar++;
        }
    }
    return 0;
}
