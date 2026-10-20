#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_TWOPI
#define M_TWOPI 6.28318530717958647692
#endif

// Include DSP directly from cyber_evans_bass_pro.cpp
#define PLUGIN_URI "test"
#include "../plugins/cyber-evans-bass-pro.lv2/src/cyber_evans_bass_pro.cpp"

int main() {
    std::cout << "=== EVANS BASS PRO DSP SIMULATION TEST ===" << std::endl;
    float sr = 48000.0f;
    EvansBassProDSP dsp;
    dsp.init(sr);

    // Test 1: Broadband Transient Pick Extractor
    std::cout << "[*] Testing Broadband Pick Transient Detector..." << std::endl;
    // Feed 100 samples of silence, then a sharp pick click (high-frequency spike)
    bool clickDetected = false;
    for (int i = 0; i < 200; ++i) {
        float sample = 0.0f;
        if (i == 100) sample = 0.8f; // Sharp pick click
        else if (i == 101) sample = -0.6f;
        else if (i == 102) sample = 0.3f;

        if (dsp.pickExtractor.process(sample, 6.5f)) {
            clickDetected = true;
            std::cout << " [+] Pick click detected at sample " << i << std::endl;
        }
    }
    assert(clickDetected && "Pick extractor failed to detect pick attack spike!");

    // Test 2: Open Bb Drone Immunity & Chord Change to Eb
    // Guitar tuning Open Bb: Low Bb string ~ 116.54 Hz (Bb2) or 58.27 Hz (Bb1)
    // When playing chord 1: Bb major (Bb + D + F)
    // When playing chord 2: Eb major (Eb + G + Bb) while low Bb string CONTINUES to ring at -12 dB!
    std::cout << "[*] Testing Open Bb Drone Immunity during Eb chord change..." << std::endl;

    float freqBb = 116.54f; // Bb2 (pitch class 10)
    float freqD  = 146.83f; // D3
    float freqF  = 174.61f; // F3
    float freqEb = 155.56f; // Eb3 (pitch class 3)
    float freqG  = 196.00f; // G3

    // Phase accumulators
    double pBb = 0.0, pD = 0.0, pF = 0.0, pEb = 0.0, pG = 0.0;

    // Simulate 0.5s of Bb chord
    int bbSamples = (int)(0.5f * sr);
    for (int i = 0; i < bbSamples; ++i) {
        float guitarAudio = 0.4f * sin(pBb) + 0.3f * sin(pD) + 0.3f * sin(pF);
        pBb += 2.0 * M_PI * freqBb / sr;
        pD  += 2.0 * M_PI * freqD / sr;
        pF  += 2.0 * M_PI * freqF / sr;

        dsp.fastRootDetector.push(guitarAudio);
        dsp.strictRootDetector.pushSample(guitarAudio);
        for (int k = 0; k < 36; ++k) {
            dsp.fastFilterBank[k].process(guitarAudio, 0.75f, 0.985f);
            dsp.strictFilterBank[k].process(guitarAudio, 0.35f, 0.995f);
        }
        if (i % 64 == 0) {
            dsp.gateIsOpen = true;
            dsp.gateGain = 1.0f;
            dsp.evaluateChordConfidenceMatrix(false, false, 8.0f, 6.0f, 2.0f, 6.0f, 0, 0, 7.5f, 6.0f, false);
        }
    }
    std::cout << " [+] After Bb chord, Confirmed Root Pitch Class = " << dsp.confirmedRoot << " (expected 10=Bb)" << std::endl;

    // Now chord transitions to Eb major, but the low Bb drone continues to ring loudly!
    int ebSamples = (int)(0.2f * sr); // 200ms
    int detectedEbAt = -1;

    for (int i = 0; i < ebSamples; ++i) {
        // High fretted Eb and G are played with a pick click, while Bb drone rings at 0.3 amplitude
        float pickClick = (i < 3) ? (0.7f - i * 0.2f) : 0.0f;
        float guitarAudio = 0.25f * sin(pBb) + 0.45f * sin(pEb) + 0.35f * sin(pG) + pickClick;
        pBb += 2.0 * M_PI * freqBb / sr;
        pEb += 2.0 * M_PI * freqEb / sr;
        pG  += 2.0 * M_PI * freqG / sr;

        bool pickHit = dsp.pickExtractor.process(guitarAudio, 6.5f);
        dsp.fastRootDetector.push(guitarAudio);
        dsp.strictRootDetector.pushSample(guitarAudio);
        for (int k = 0; k < 36; ++k) {
            dsp.fastFilterBank[k].process(guitarAudio, 0.75f, 0.985f);
            dsp.strictFilterBank[k].process(guitarAudio, 0.35f, 0.995f);
        }

        if (pickHit || (i % 64 == 0)) {
            dsp.evaluateChordConfidenceMatrix(false, false, 8.0f, 6.0f, 2.0f, 6.0f, 0, 0, 7.5f, 6.0f, pickHit);
            if (dsp.confirmedRoot == 3 && detectedEbAt < 0) { // 3 = Eb
                detectedEbAt = i;
            }
        }
    }

    float msToEb = (float)detectedEbAt * 1000.0f / sr;
    std::cout << " [+] Confirmed Root after transition = " << dsp.confirmedRoot << " (3=Eb)" << std::endl;
    std::cout << " [+] Detected Eb chord transition in " << msToEb << " ms despite continuous low Bb drone!" << std::endl;
    assert(dsp.confirmedRoot == 3 && "Failed to switch to Eb in presence of Bb drone!");

    // Test 3: Guided Performer Markov Bass Tone Generation
    std::cout << "[*] Testing Guided Performer Groove & Walk..." << std::endl;
    int note1 = dsp.performer.generateNextBassTone(3, false, 0, 5.0f, 6.0f); // Beat 1: Root anchor
    int note2 = dsp.performer.generateNextBassTone(3, false, 4, 5.0f, 6.0f); // Beat 2: 5th or root
    std::cout << " [+] Beat 1 note: MIDI " << note1 << " (Root Eb)" << std::endl;
    std::cout << " [+] Beat 2 note: MIDI " << note2 << std::endl;
    assert(note1 % 12 == 3 && "Downbeat must anchor to root pitch class!");

    // Test 4: Vintage Bass Voice Rendering
    std::cout << "[*] Testing Vintage Bass Models (all 9 voices)..." << std::endl;
    for (int preset = 0; preset < 9; ++preset) {
        double p = 0.0, sp = 0.0;
        float filt = 0.0f;
        float out = dsp.renderBassVoice(preset, 155.56f, p, sp, filt, 5.0f, 5.0f);
        assert(!std::isnan(out) && !std::isinf(out) && "Voice render produced NaN/Inf!");
    }
    std::cout << " [+] All 9 Vintage Bass voices rendered clean non-NaN audio!" << std::endl;

    std::cout << "\n>>> ALL EVANS BASS PRO DSP TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
