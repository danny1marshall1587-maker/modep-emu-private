#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#include "../plugins/cyber-feedback-super.lv2/src/cyber_feedback_super.cpp"

int main() {
    double sampleRate = 48000.0;
    CyberFeedbackSuper pedal(sampleRate);

    const int N = 48000 * 3; // 3 seconds
    std::vector<float> in_l(N, 0.0f);
    std::vector<float> in_r(N, 0.0f);
    std::vector<float> out_l(N, 0.0f);
    std::vector<float> out_r(N, 0.0f);

    float bypass = 1.0f;
    float feedback = 0.0f; // Heel down initially
    float sustain_level = 1.0f;
    float crossfade_ms = 20.0f;
    float loop_len_min = 30.0f;
    float loop_nudge = 0.0f;
    float release_ms = 300.0f;
    float waver_depth = 1.5f;
    float waver_rate = 1.2f;
    float speaker_distress = 0.35f;
    float room_in_loop = 1.0f;
    float room_size = 180.0f;
    float room_decay = 2.0f;
    float room_mix = 25.0f;

    pedal.connect_port(PORT_AUDIO_IN_L, in_l.data());
    pedal.connect_port(PORT_AUDIO_IN_R, in_r.data());
    pedal.connect_port(PORT_AUDIO_OUT_L, out_l.data());
    pedal.connect_port(PORT_AUDIO_OUT_R, out_r.data());
    pedal.connect_port(PORT_BYPASS, &bypass);
    pedal.connect_port(PORT_FEEDBACK, &feedback);
    pedal.connect_port(PORT_SUSTAIN_LEVEL, &sustain_level);
    pedal.connect_port(PORT_CROSSFADE_MS, &crossfade_ms);
    pedal.connect_port(PORT_LOOP_LEN_MIN, &loop_len_min);
    pedal.connect_port(PORT_LOOP_NUDGE, &loop_nudge);
    pedal.connect_port(PORT_RELEASE_MS, &release_ms);
    pedal.connect_port(PORT_WAVER_DEPTH, &waver_depth);
    pedal.connect_port(PORT_WAVER_RATE, &waver_rate);
    pedal.connect_port(PORT_SPEAKER_DISTRESS, &speaker_distress);
    pedal.connect_port(PORT_ROOM_IN_LOOP, &room_in_loop);
    pedal.connect_port(PORT_ROOM_SIZE, &room_size);
    pedal.connect_port(PORT_ROOM_DECAY, &room_decay);
    pedal.connect_port(PORT_ROOM_MIX, &room_mix);

    // Scenario: Play an A2 guitar note (110.0 Hz) decaying over 0.5s
    for (int i = 0; i < (int)(48000 * 0.5); ++i) {
        float t = (float)i / 48000.0f;
        float env = expf(-t * 4.0f);
        in_l[i] = in_r[i] = 0.5f * env * sinf(2.0f * (float)M_PI * 110.0f * t);
    }

    const int blockSize = 128;
    // Step 1: Run first 0.25s with heel down (0 feedback)
    for (int i = 0; i < 48000 * 0.25; i += blockSize) {
        pedal.connect_port(PORT_AUDIO_IN_L, &in_l[i]);
        pedal.connect_port(PORT_AUDIO_IN_R, &in_r[i]);
        pedal.connect_port(PORT_AUDIO_OUT_L, &out_l[i]);
        pedal.connect_port(PORT_AUDIO_OUT_R, &out_r[i]);
        pedal.run(blockSize);
    }

    // Step 2: Push expression pedal to toe (feedback = 1.0)
    feedback = 1.0f;
    for (int i = 48000 * 0.25; i < 48000 * 2.0; i += blockSize) {
        pedal.connect_port(PORT_AUDIO_IN_L, &in_l[i]);
        pedal.connect_port(PORT_AUDIO_IN_R, &in_r[i]);
        pedal.connect_port(PORT_AUDIO_OUT_L, &out_l[i]);
        pedal.connect_port(PORT_AUDIO_OUT_R, &out_r[i]);
        pedal.run(blockSize);
    }

    // Check RMS at 1.5s (dry input has completely decayed to 0.0, sustain should be active and clean)
    float sustain_rms = 0.0f;
    int check_start = 48000 * 1.5;
    int check_end = 48000 * 1.8;
    for (int i = check_start; i < check_end; ++i) {
        sustain_rms += out_l[i] * out_l[i];
    }
    sustain_rms = sqrtf(sustain_rms / (float)(check_end - check_start));
    std::cout << "Sustain phase RMS (dry note decayed): " << sustain_rms << std::endl;
    assert(sustain_rms > 0.05f && "Sustainer failed to lock and sustain note!");

    // Step 3: Back off pedal to heel (feedback = 0.0) and verify clean decay
    feedback = 0.0f;
    for (int i = 48000 * 2.0; i < N; i += blockSize) {
        pedal.connect_port(PORT_AUDIO_IN_L, &in_l[i]);
        pedal.connect_port(PORT_AUDIO_IN_R, &in_r[i]);
        pedal.connect_port(PORT_AUDIO_OUT_L, &out_l[i]);
        pedal.connect_port(PORT_AUDIO_OUT_R, &out_r[i]);
        pedal.run(blockSize);
    }

    float final_rms = 0.0f;
    int final_start = 48000 * 2.8;
    int final_end = N;
    for (int i = final_start; i < final_end; ++i) {
        final_rms += out_l[i] * out_l[i];
    }
    final_rms = sqrtf(final_rms / (float)(final_end - final_start));
    std::cout << "Final released RMS (after release_ms): " << final_rms << std::endl;
    assert(final_rms < 0.02f && "Release failed to decay properly!");

    std::cout << "ALL DSP SIMULATION TESTS PASSED PERFECTLY!" << std::endl;
    return 0;
}
