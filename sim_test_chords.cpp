#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <iomanip>

#include "cyber_evans_drummer_pro.cpp"

bool loadWavFloat(const std::string& path, std::vector<float>& outAudio, int& outSampleRate) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    size_t fsize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buf(fsize);
    file.read(buf.data(), fsize);

    outSampleRate = 48000;
    for (size_t i = 0; i + 8 < fsize; ++i) {
        if (memcmp(&buf[i], "fmt ", 4) == 0) {
            uint32_t sr = *(uint32_t*)&buf[i + 12];
            outSampleRate = sr;
        } else if (memcmp(&buf[i], "data", 4) == 0) {
            uint32_t dataSize = *(uint32_t*)&buf[i + 4];
            size_t numSamples = dataSize / 4;
            float* sptr = (float*)&buf[i + 8];
            outAudio.assign(sptr, sptr + numSamples);
            return true;
        }
    }
    return false;
}

int main() {
    std::vector<float> audio;
    int sr = 48000;
    loadWavFloat("/var/modep/user-files/Audio Recordings/lv2record/mod_session003.wav", audio, sr);

    ChordCycleTracker ct;
    ct.init((float)sr);

    const char* PITCH_NAMES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

    int barSamples = int(2.264 * sr);
    int bar = 0;
    for (size_t i = 0; i < audio.size(); ++i) {
        ct.processSample(audio[i]);
        if ((i > 0 && i % barSamples == 0) || i + 1 == audio.size()) {
            ct.onBarComplete();
            int c = ct.barChords[bar];
            std::cout << "Bar " << bar << " (t=" << (float)i/sr << "s): "
                      << (c >= 0 ? PITCH_NAMES[c] : "None")
                      << " | cycleLocked=" << ct.cycleLocked
                      << " | roundLen=" << ct.detectedRoundBars
                      << " | rounds=" << ct.roundsCounted << std::endl;
            bar++;
        }
    }
    return 0;
}
