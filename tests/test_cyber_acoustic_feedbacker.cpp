#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

// Include the plugin implementation directly for DSP unit testing
#include "../plugins/cyber-acoustic-feedbacker.lv2/src/cyber_acoustic_feedbacker.cpp"

int main() {
    std::cout << "Running DSP Unit Tests for Cyber Acoustic Feedbacker..." << std::endl;
    double sampleRate = 48000.0;
    CyberAcousticFeedbacker pedal(sampleRate);

    const int blockSize = 128;
    std::vector<float> in_l(blockSize, 0.0f);
    std::vector<float> in_r(blockSize, 0.0f);
    std::vector<float> out_l(blockSize, 0.0f);
    std::vector<float> out_r(blockSize, 0.0f);

    float bypass = 1.0f; // Not bypassed
    float trigger = 0.0f; // Heel down (no feedback)
    float mode = 0.0f;
    float bloom = 0.5f;
    float gain = 80.0f;
    float harmonic = 2.0f; // Octave
    float warmth = 60.0f;  // Speaker distress
    float vibrato = 80.0f; // Leaky loop depth
    float mix = 50.0f;

    pedal.connect_port(PORT_AUDIO_IN_L, in_l.data());
    pedal.connect_port(PORT_AUDIO_IN_R, in_r.data());
    pedal.connect_port(PORT_AUDIO_OUT_L, out_l.data());
    pedal.connect_port(PORT_AUDIO_OUT_R, out_r.data());
    pedal.connect_port(PORT_BYPASS, &bypass);
    pedal.connect_port(PORT_TRIGGER, &trigger);
    pedal.connect_port(PORT_MODE, &mode);
    pedal.connect_port(PORT_BLOOM, &bloom);
    pedal.connect_port(PORT_GAIN, &gain);
    pedal.connect_port(PORT_HARMONIC, &harmonic);
    pedal.connect_port(PORT_WARMTH, &warmth);
    pedal.connect_port(PORT_VIBRATO, &vibrato);
    pedal.connect_port(PORT_MIX, &mix);

    // TEST 1: Heel down (trigger = 0.0) -> Output must be 100% pristine dry guitar
    for (int i = 0; i < blockSize; ++i) {
        in_l[i] = in_r[i] = 0.4f * sinf(2.0f * (float)M_PI * 440.0f * i / 48000.0f);
    }
    pedal.run(blockSize);
    float max_diff_heel = 0.0f;
    for (int i = 0; i < blockSize; ++i) {
        max_diff_heel = std::max(max_diff_heel, fabsf(out_l[i] - in_l[i]));
    }
    std::cout << "[TEST 1] Heel down dry transparency max difference: " << max_diff_heel << std::endl;
    assert(max_diff_heel < 1e-5f);
    std::cout << "  -> PASS: 100% Clean dry passthrough verified!" << std::endl;

    // TEST 2: Toe down (trigger = 1.0) -> Feedback blooms and sustains
    trigger = 1.0f;
    float max_peak_toe = 0.0f;
    // Run for 1.5 seconds to let feedback bloom
    for (int block = 0; block < 600; ++block) {
        // Guitar note decays naturally
        float decay = expf(-((float)block * blockSize) / (48000.0f * 0.8f));
        for (int i = 0; i < blockSize; ++i) {
            in_l[i] = in_r[i] = 0.3f * decay * sinf(2.0f * (float)M_PI * 220.0f * (block * blockSize + i) / 48000.0f);
        }
        pedal.run(blockSize);
        for (int i = 0; i < blockSize; ++i) {
            max_peak_toe = std::max(max_peak_toe, fabsf(out_l[i]));
        }
    }
    std::cout << "[TEST 2] Toe down maximum feedback peak: " << max_peak_toe << std::endl;
    assert(max_peak_toe > 0.1f); // Must generate singing feedback
    assert(max_peak_toe < 1.4f); // Speaker distress & limiter must prevent clipping
    std::cout << "  -> PASS: Sustained feedback generated and clamped by speaker distress without runaway clipping!" << std::endl;

    // TEST 3: Infinite sustain check when string dies out completely
    for (int i = 0; i < blockSize; ++i) in_l[i] = in_r[i] = 0.0f;
    float sustained_energy = 0.0f;
    for (int block = 0; block < 100; ++block) {
        pedal.run(blockSize);
        for (int i = 0; i < blockSize; ++i) {
            sustained_energy += out_l[i] * out_l[i];
        }
    }
    std::cout << "[TEST 3] Sustained loop energy after string decay: " << sustained_energy << std::endl;
    assert(sustained_energy > 0.01f);
    std::cout << "  -> PASS: Infinite sustain maintained in the acoustic loop!" << std::endl;

    // TEST 4: Heel down shutoff
    trigger = 0.0f;
    for (int block = 0; block < 200; ++block) {
        pedal.run(blockSize);
    }
    float shutoff_peak = 0.0f;
    for (int i = 0; i < blockSize; ++i) {
        shutoff_peak = std::max(shutoff_peak, fabsf(out_l[i]));
    }
    std::cout << "[TEST 4] Heel down shutoff output peak: " << shutoff_peak << std::endl;
    assert(shutoff_peak < 0.001f);
    std::cout << "  -> PASS: Feedback completely silenced on heel down!" << std::endl;

    std::cout << "\nALL 4 DSP TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
