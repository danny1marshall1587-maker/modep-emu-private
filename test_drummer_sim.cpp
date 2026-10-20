#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <iomanip>

// Include plugin source directly
#include "cyber_evans_drummer_pro.cpp"

// Simple WAV reader for 32-bit float or 16-bit PCM WAV files
bool loadWavFloat(const std::string& path, std::vector<float>& outAudio, int& outSampleRate) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char riff[4];
    file.read(riff, 4);
    if (strncmp(riff, "RIFF", 4) != 0) return false;

    uint32_t fileSize;
    file.read((char*)&fileSize, 4);

    char wave[4];
    file.read(wave, 4);
    if (strncmp(wave, "WAVE", 4) != 0) return false;

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<char> dataChunk;

    while (file) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        file.read(chunkId, 4);
        file.read((char*)&chunkSize, 4);
        if (!file) break;

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            file.read((char*)&audioFormat, 2);
            file.read((char*)&numChannels, 2);
            file.read((char*)&sampleRate, 4);
            uint32_t byteRate;
            file.read((char*)&byteRate, 4);
            uint16_t blockAlign;
            file.read((char*)&blockAlign, 2);
            file.read((char*)&bitsPerSample, 2);
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }
        } else if (strncmp(chunkId, "data", 4) == 0) {
            dataChunk.resize(chunkSize);
            file.read(dataChunk.data(), chunkSize);
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    outSampleRate = sampleRate;
    size_t numSamples = 0;

    if (audioFormat == 3 && bitsPerSample == 32) { // IEEE Float
        numSamples = dataChunk.size() / (sizeof(float) * numChannels);
        const float* fptr = (const float*)dataChunk.data();
        outAudio.resize(numSamples);
        for (size_t i = 0; i < numSamples; ++i) {
            outAudio[i] = fptr[i * numChannels]; // Mono or Left channel
        }
    } else if (audioFormat == 1 && bitsPerSample == 16) { // 16-bit PCM
        numSamples = dataChunk.size() / (sizeof(int16_t) * numChannels);
        const int16_t* iptr = (const int16_t*)dataChunk.data();
        outAudio.resize(numSamples);
        for (size_t i = 0; i < numSamples; ++i) {
            outAudio[i] = iptr[i * numChannels] / 32768.0f;
        }
    } else {
        std::cerr << "Unsupported format: format=" << audioFormat << ", bits=" << bitsPerSample << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    std::string wavPath = "guitar_test_recording.wav";
    if (argc > 1) wavPath = argv[1];

    std::vector<float> audio;
    int sr = 48000;
    if (!loadWavFloat(wavPath, audio, sr)) {
        std::cerr << "Failed to load WAV file: " << wavPath << std::endl;
        return 1;
    }

    // Simple URID map for offline simulation
    static auto urid_map_fn = [](LV2_URID_Map_Handle handle, const char* uri) -> LV2_URID {
        static uint32_t counter = 1;
        return counter++;
    };
    LV2_URID_Map dummyMap = { nullptr, urid_map_fn };

    CyberEvansDrummerPro drummer(sr, &dummyMap);

    // Setup ports
    float bypass = 1.0f;
    float style = 0.0f; // Classic Rock
    float bpmOverride = 0.0f; // auto
    float accentSens = 5.0f;
    float ghostDensity = 3.0f;
    float fillDensity = 5.0f;
    float groove = 4.0f;
    float overallVel = 7.0f;
    float openHhThresh = 6.0f;
    float crashOnChorus = 1.0f;
    float recordSong = 0.0f;
    float mute = 0.0f;
    float adventurous = 0.0f;
    float detectedBpm = 120.0f;
    float currentSection = 0.0f;
    float energyLevel = 0.0f;
    float strumOnlyMode = 0.0f;
    float kitVol = 100.0f;
    float stopCrash = 0.0f;
    float fillNow = 0.0f;
    float manualChorus = 0.0f;

    drummer.ports[PORT_BYPASS] = &bypass;
    drummer.ports[PORT_STYLE] = &style;
    drummer.ports[PORT_BPM_OVERRIDE] = &bpmOverride;
    drummer.ports[PORT_ACCENT_SENS] = &accentSens;
    drummer.ports[PORT_GHOST_DENSITY] = &ghostDensity;
    drummer.ports[PORT_FILL_DENSITY] = &fillDensity;
    drummer.ports[PORT_GROOVE] = &groove;
    drummer.ports[PORT_OVERALL_VEL] = &overallVel;
    drummer.ports[PORT_OPEN_HH_THRESH] = &openHhThresh;
    drummer.ports[PORT_CRASH_ON_CHORUS] = &crashOnChorus;
    drummer.ports[PORT_RECORD_SONG] = &recordSong;
    drummer.ports[PORT_MUTE] = &mute;
    drummer.ports[PORT_ADVENTUROUS] = &adventurous;
    drummer.ports[PORT_STRUM_ONLY_MODE] = &strumOnlyMode;
    drummer.ports[PORT_KIT_MASTER_VOL] = &kitVol;
    drummer.ports[PORT_STOP_CRASH] = &stopCrash;
    drummer.ports[PORT_FILL_NOW] = &fillNow;
    drummer.ports[PORT_MANUAL_CHORUS] = &manualChorus;

    drummer.out_ports[PORT_DETECTED_BPM] = &detectedBpm;
    drummer.out_ports[PORT_CURRENT_SECTION] = &currentSection;
    drummer.out_ports[PORT_ENERGY_LEVEL] = &energyLevel;

    // Buffer processing
    const size_t blockSize = 128;
    std::vector<float> blockBuf(blockSize, 0.0f);
    std::vector<float> outAudio(blockSize, 0.0f);

    drummer.out_audio = outAudio.data();

    // Allocate MIDI sequence buffer
    std::vector<uint8_t> midiBuf(8192, 0);
    LV2_Atom_Sequence* seq = (LV2_Atom_Sequence*)midiBuf.data();
    seq->atom.type = 0;
    seq->atom.size = (uint32_t)(midiBuf.size() - sizeof(LV2_Atom));
    drummer.out_midi = seq;

    activate(&drummer);

    int totalHits = 0;
    int totalKicks = 0;
    int totalSnares = 0;
    int totalHats = 0;
    double lastPrintSec = 0.0;
    int totalAutoFills = 0;
    bool prevFillActive = false;
    int prevSection = 0;

    std::cout << "\n=== SIMULATING GUITAR RECORDING THROUGH DRUMMER ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    for (size_t offset = 0; offset < audio.size(); offset += blockSize) {
        size_t samplesThisBlock = std::min(blockSize, audio.size() - offset);
        drummer.in_audio = &audio[offset];

        // Reset atom forge
        seq->atom.size = (uint32_t)(midiBuf.size() - sizeof(LV2_Atom));

        // Note counting: track active notes generated
        size_t prevActiveNotes = drummer.activeNotes.size();

        run(&drummer, (uint32_t)samplesThisBlock);

        // Count newly added notes
        for (size_t n = prevActiveNotes; n < drummer.activeNotes.size(); ++n) {
            uint8_t note = drummer.activeNotes[n].note;
            totalHits++;
            if (note == GM_BASS_DRUM) totalKicks++;
            else if (note == GM_SNARE) totalSnares++;
            else if (note == GM_CLOSED_HH || note == GM_OPEN_HH || note == GM_RIDE_1) totalHats++;
        }

        // Detect fill fire events (false -> true transition)
        if (drummer.fillActive && !prevFillActive) {
            double curSec2 = (double)offset / sr;
            const char* fillType = (drummer.activeSection != prevSection) ? "SECTION-CHANGE" : "ENERGY-CLIMB";
            std::cout << "  *** AUTO-FILL [" << curSec2 << "s] Bar:" << drummer.currentBar
                      << " Section:" << drummer.activeSection
                      << " risingBeats:" << drummer.risingEnergyBeats
                      << " Type:" << fillType << " ***" << std::endl;
            totalAutoFills++;
        }
        prevFillActive = drummer.fillActive;
        prevSection    = drummer.activeSection;

        double curSec = (double)offset / sr;
        if (curSec - lastPrintSec >= 2.0 || offset + blockSize >= audio.size()) {
            lastPrintSec = curSec;
            std::cout << "[" << std::setw(5) << curSec << "s] "
                      << "BPM: " << std::setw(5) << detectedBpm
                      << " | Started: " << (drummer.hasStartedPlayback ? "YES" : "NO ")
                      << " | Step: " << std::setw(2) << drummer.currentStep
                      << " | Bar: " << std::setw(3) << drummer.currentBar
                      << " | RMS: " << std::setw(5) << drummer.rollingRms500ms
                      << " | Sec: " << drummer.activeSection
                      << " | Fills: " << totalAutoFills
                      << " | Hits: " << totalHits << " (K:" << totalKicks << " S:" << totalSnares << " H:" << totalHats << ")"
                      << std::endl;
        }
    }

    std::cout << "\n=== SIMULATION COMPLETE ===" << std::endl;
    std::cout << "Total Audio Time: " << (float)audio.size() / sr << " s" << std::endl;
    std::cout << "Total Drum Hits: " << totalHits << " (Kick: " << totalKicks << ", Snare: " << totalSnares << ", Cymbals/Hats: " << totalHats << ")" << std::endl;
    std::cout << "Total Auto-Fills Fired: " << totalAutoFills << std::endl;

    return 0;
}
