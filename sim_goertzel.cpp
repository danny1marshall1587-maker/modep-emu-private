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
    float getMag() const {
        return sqrtf(s1 * s1 + s2 * s2 - s1 * s2 * coeff);
    }
};

int main() {
    std::ifstream file("/var/modep/user-files/Audio Recordings/lv2record/mod_session003.wav", std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open WAV\n";
        return 1;
    }
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

    if (!audio) {
        std::cerr << "No data chunk\n";
        return 1;
    }

    // Standard 12 chromatic frequencies from A2 (110 Hz) to G#3 (207.65 Hz)
    const char* NOTES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    GoertzelState filters[36]; // 3 octaves: MIDI 40 (E2, 82.4Hz) to MIDI 75 (Eb5)
    for (int i = 0; i < 36; ++i) {
        int midi = 40 + i;
        float f = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
        filters[i].init(f, (float)sr);
    }

    int blockLen = int(0.25f * sr); // 250ms analysis frames
    int hop = int(0.25f * sr);

    std::cout << "Testing chromatic detection across mod_session003.wav...\n";
    for (size_t offset = 0; offset + blockLen <= numSamples; offset += hop) {
        float sec = (float)offset / sr;
        if (sec < 3.0f || sec > 48.0f) continue;

        for (int i = 0; i < 36; ++i) filters[i].reset();
        for (int s = 0; s < blockLen; ++s) {
            float smp = audio[offset + s];
            for (int i = 0; i < 36; ++i) filters[i].process(smp);
        }

        float chroma[12] = {};
        for (int i = 0; i < 36; ++i) {
            int semi = (40 + i) % 12;
            chroma[semi] += filters[i].getMag();
        }

        float maxC = 1e-6f;
        for (int c = 0; c < 12; ++c) if (chroma[c] > maxC) maxC = chroma[c];
        for (int c = 0; c < 12; ++c) chroma[c] /= maxC;

        int bestRoot = -1;
        float bestScore = -1.0f;
        for (int c = 0; c < 12; ++c) {
            float sc = chroma[c] * 1.5f + chroma[(c + 7) % 12] * 1.0f + std::max(chroma[(c + 4) % 12], chroma[(c + 3) % 12]) * 0.8f;
            if (sc > bestScore) {
                bestScore = sc;
                bestRoot = c;
            }
        }

        if (int(sec * 4) % 4 == 0) { // every 1.0 second
            std::cout << "t=" << std::fixed << std::setprecision(1) << sec << "s: "
                      << std::setw(3) << NOTES[bestRoot] << " (sc=" << bestScore << ") [";
            for (int c = 0; c < 12; ++c) {
                if (chroma[c] > 0.5f) std::cout << NOTES[c] << " ";
            }
            std::cout << "]\n";
        }
    }
    return 0;
}
