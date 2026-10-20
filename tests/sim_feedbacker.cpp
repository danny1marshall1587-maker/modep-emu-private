#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <iomanip>

#include "../plugins/cyber-acoustic-feedbacker.lv2/src/cyber_acoustic_feedbacker.cpp"

int main() {
    double sampleRate = 48000.0;
    CyberAcousticFeedbacker pedal(sampleRate);

    const int N = 48000 * 3; // 3 seconds
    std::vector<float> in_l(N, 0.0f);
    std::vector<float> in_r(N, 0.0f);
    std::vector<float> out_l(N, 0.0f);
    std::vector<float> out_r(N, 0.0f);

    float bypass = 1.0f;
    float trigger = 1.0f; // Feedback engaged
    float mode = 0.0f;
    float bloom = 0.2f;
    float gain = 75.0f;
    float harmonic = 2.0f; // Octave
    float warmth = 50.0f;
    float vibrato = 75.0f;
    float mix = 100.0f; // 100% wet

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

    // Scenario: Pluck an E2 guitar note (82.4 Hz), decaying over 0.5s
    for (int i = 0; i < 48000 * 0.5; ++i) {
        float t = (float)i / 48000.0f;
        float env = expf(-t * 6.0f);
        in_l[i] = in_r[i] = 0.4f * env * sinf(2.0f * (float)M_PI * 82.4f * t);
    }
    // After 0.5s, guitar is completely silent

    const int blockSize = 128;
    for (int i = 0; i < N; i += blockSize) {
        pedal.connect_port(PORT_AUDIO_IN_L, &in_l[i]);
        pedal.connect_port(PORT_AUDIO_IN_R, &in_r[i]);
        pedal.connect_port(PORT_AUDIO_OUT_L, &out_l[i]);
        pedal.connect_port(PORT_AUDIO_OUT_R, &out_r[i]);
        pedal.run(blockSize);
    }

    // Measure zero crossings / dominant frequency between t = 1.5s and 2.5s (during sustain)
    int start_samp = 48000 * 1.5;
    int end_samp = 48000 * 2.5;
    int zero_crossings = 0;
    float peak_val = 0.0f;

    for (int i = start_samp; i < end_samp; ++i) {
        peak_val = std::max(peak_val, fabsf(out_l[i]));
        if ((out_l[i] >= 0.0f && out_l[i-1] < 0.0f) || (out_l[i] < 0.0f && out_l[i-1] >= 0.0f)) {
            zero_crossings++;
        }
    }

    float approx_freq = (float)zero_crossings * 0.5f; // Zero crossings per second = 2 * freq
    std::cout << "=== AUDIO ANALYSIS OF SUSTAINED FEEDBACK ===" << std::endl;
    std::cout << "Guitar Note Played: E2 (82.4 Hz)" << std::endl;
    std::cout << "Harmonic Setting: Octave (+12 st -> should be 164.8 Hz)" << std::endl;
    std::cout << "Peak Amplitude during sustain: " << peak_val << std::endl;
    std::cout << "Dominant Whistle Frequency Measured: " << approx_freq << " Hz" << std::endl;

    // Also let's print a slice of 30 samples to see the waveform
    std::cout << "\nWaveform samples around 2.0s:" << std::endl;
    for (int i = 48000 * 2; i < 48000 * 2 + 25; ++i) {
        std::cout << std::fixed << std::setprecision(4) << out_l[i] << " ";
    }
    std::cout << std::endl;

    return 0;
}
