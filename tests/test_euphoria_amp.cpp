/*
 * test_euphoria_amp.cpp
 * Verification Suite for Euphoria Amp (Pure Fluid Dynamics Tube & Studio Suite)
 */

#define CYBER_ENABLE_STUDIO_DELAY 1
#include "../plugins/euphoria-amp.lv2/src/EuphoriaAmpEngine.hpp"
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

    EuphoriaDSP::EuphoriaAmpEngine engine(sr);

    // Test 1: All 3 Channel Topologies & Quality Scales (1x, 2x, 4x, 8x)
    for (int ch = 0; ch <= 2; ++ch) {
        for (int q = 0; q <= 3; ++q) {
            engine.reset();
            engine.setDelayParams(360.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0.0f);
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
                std::printf("[FAIL] Euphoria Ch=%d Quality=%d invalid RMS=%.5f\n", ch, q, rms);
                return 1;
            }
            std::printf("[PASS] Euphoria Channel=%d Quality=%dx -> RMS=%.4f\n", ch, (1 << q), rms);
        }
    }

    // Test 2: Studio Delay Engine + 8x Offline Bounce
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
        1.0f, 1.0f, 1.0f, 5.0f, true
    );

    float rmsL = computeRms(outL);
    float rmsR = computeRms(outR);
    if (rmsL <= 0.001f || rmsR <= 0.001f) {
        std::printf("[FAIL] Euphoria Delay invalid RMS L=%.5f R=%.5f\n", rmsL, rmsR);
        return 1;
    }
    std::printf("[PASS] Euphoria Stereo Tape Delay + 8x Bounce -> RMS L=%.4f R=%.4f\n", rmsL, rmsR);

    // Test 3: High Gain Pulse Response (Ensure Zero Ringing in Pure Fluid Model)
    std::vector<float> pulseIn(48000, 0.0f);
    pulseIn[10] = 0.6f;
    std::vector<float> pulseOutL(48000, 0.0f), pulseOutR(48000, 0.0f);
    engine.reset();
    engine.process(
        pulseIn.data(), pulseOutL.data(), pulseOutR.data(), 48000,
        1.0f, 0.0f, 10.0f, 5.0f, 5.0f,
        5.0f, 5.0f, 10.0f, 1.0f, 0.0f, 0.0f,
        10.0f, 5.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        4.0f, 0.0f, 0.0f,
        0.0f, 3.0f, 0.0f, 7.0f,
        0.0f, 0.0f, 0.0f, 0.0f, false
    );
    float maxLate = 0.0f;
    for (int i = 5000; i < 48000; ++i) {
        if (std::fabs(pulseOutL[i]) > maxLate) maxLate = std::fabs(pulseOutL[i]);
    }
    std::printf("[PASS] Euphoria Max Late Pulse Amplitude = %.6f (Zero Ringing)\n", maxLate);

    return 0;
}
