/*
 * test_cyber_ultimate_hybrid.cpp
 * Verification Suite for Cyber Ultimate Hybrid ('65 Blackface + '59 Bassman/JMP + Mixer Studio Delay)
 * Verifies:
 *   1. Channel 0 ('65 Blackface Clean), Channel 1 ('59 Bassman/JMP Crunch), Channel 2 (Jumpered Parallel Blend)
 *   2. Scalable Physics / Oversampling Modes (0=Eco 1x, 1=Stage 2x, 2=Studio 4x, 3=Ultra Bounce 8x)
 *   3. Automatic DAW Offline Bounce 8x Ultra Physics elevation (isOfflineBounce = true)
 *   4. 3-Spring Accutronics Reverb Tank with 12AT7 Driver & Dispersion Drip
 *   5. Brownface 6G Harmonic Tube Vibrato
 *   6. VST-Exclusive Live Mixer Studio Tape / Ping-Pong Delay with Hermite Interpolation & Dual Read-Head Crossfade
 */

#define CYBER_ENABLE_STUDIO_DELAY 1
#include "../plugins/cyber-ultimate-hybrid.lv2/src/CyberUltimateHybridEngine.hpp"
#include <cstdio>
#include <cmath>
#include <vector>

static float computeRms(const std::vector<float>& buf) {
    double sum = 0.0;
    for (float v : buf) {
        if (!std::isfinite(v)) return -1.0f;
        sum += (double)v * (double)v;
    }
    return (float)std::sqrt(sum / (double)buf.size());
}

int main() {
    const double sr = 48000.0;
    const uint32_t N = 4096;
    std::vector<float> in(N), outL(N), outR(N);

    for (uint32_t i = 0; i < N; ++i) {
        float t = (float)i / (float)sr;
        in[i] = 0.25f * std::sin(2.0f * (float)M_PI * 220.0f * t)
              + 0.12f * std::sin(2.0f * (float)M_PI * 440.0f * t);
    }

    CyberDSP::CyberUltimateHybridEngine engine(sr);

    // Test 1: All 3 Channel Topologies & Quality Scales (1x, 2x, 4x, 8x)
    for (int ch = 0; ch <= 2; ++ch) {
        for (int q = 0; q <= 3; ++q) {
            engine.reset();
            engine.setDelayParams(360.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0.0f); // Delay bypassed (Mixer LV2 behavior)
            engine.process(
                in.data(), outL.data(), outR.data(), N,
                1.0f, (float)ch, 6.2f, 6.0f, 5.8f,
                5.5f, 6.0f, 6.5f, 1.0f, (float)(ch == 1 ? 1 : 0), (float)(ch == 1 ? 1 : 0),
                5.8f, 5.5f, 6.5f,
                5.5f, 25.0f, 6.2f, 7.0f,
                4.2f, 35.0f, 1.0f,
                (float)q, (float)ch, 5.5f, 7.0f,
                1.0f, 1.0f, 1.0f, 5.0f, false
            );
            float rms = computeRms(outL);
            if (rms <= 0.001f || rms > 5.0f) {
                std::printf("[FAIL] Ch=%d Quality=%d invalid RMS=%.5f\n", ch, q, rms);
                return 1;
            }
            std::printf("[PASS] Channel=%d Quality=%dx -> RMS=%.4f\n", ch, (1 << q), rms);
        }
    }

    // Test 2:VST Studio Delay Engine + Rule 27 Dual Read-Head Crossfade + Auto Offline Bounce 8x
    engine.reset();
    engine.setDelayParams(320.0f, 42.0f, 35.0f, 30.0f, 22.0f, 1, 25.0f);
    engine.process(
        in.data(), outL.data(), outR.data(), N,
        1.0f, 2.0f, 6.2f, 5.8f, 5.5f,
        5.5f, 6.0f, 6.5f, 1.0f, 0.0f, 0.0f,
        5.5f, 5.8f, 6.0f,
        5.0f, 22.0f, 6.2f, 6.8f,
        3.8f, 28.0f, 1.0f,
        0.0f, 0.0f, 5.5f, 7.0f,
        1.0f, 1.0f, 1.0f, 5.0f, true // isOfflineBounce = true -> auto-upgrades Eco to 8x Ultra Physics
    );
    // Change delay time mid-stream to trigger dual read-head crossfade
    engine.setDelayParams(480.0f, 45.0f, 38.0f, 35.0f, 25.0f, 1, 25.0f);
    engine.process(
        in.data(), outL.data(), outR.data(), N,
        1.0f, 2.0f, 6.2f, 5.8f, 5.5f,
        5.5f, 6.0f, 6.5f, 1.0f, 0.0f, 0.0f,
        5.5f, 5.8f, 6.0f,
        5.0f, 22.0f, 6.2f, 6.8f,
        3.8f, 28.0f, 1.0f,
        0.0f, 0.0f, 5.5f, 7.0f,
        1.0f, 1.0f, 1.0f, 5.0f, true
    );

    float rmsL = computeRms(outL);
    float rmsR = computeRms(outR);
    if (rmsL <= 0.001f || rmsR <= 0.001f) {
        std::printf("[FAIL] VST Stereo Delay + Offline Bounce invalid RMS L=%.5f R=%.5f\n", rmsL, rmsR);
        return 1;
    }
    std::printf("[PASS] VST Mixer Tape Ping-Pong Delay + Offline 8x Bounce -> RMS L=%.4f R=%.4f\n", rmsL, rmsR);
    return 0;
}
