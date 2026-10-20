#define CYBER_ENABLE_STUDIO_DELAY 1
#include "CyberUltimateHybridEngine.hpp"
#include <cstdio>
#include <cmath>
#include <vector>

int main() {
    double sr = 48000.0;
    CyberDSP::CyberUltimateHybridEngine engine(sr);
    const int N = 48000;
    std::vector<float> in(N, 0.0f);
    // Put a brief pulse at the start, then silence
    in[10] = 0.5f;

    std::vector<float> outL(N, 0.0f), outR(N, 0.0f);

    for (int q = 0; q <= 3; ++q) {
        engine.reset();
        engine.process(
            in.data(), outL.data(), outR.data(), N,
            1.0f,
            0.0f, // Channel 0: Blackface
            10.0f, // Clean Vol = 10 (MAX)
            5.0f, 5.0f,
            5.0f, 5.0f,
            10.0f, // Treble = 10 (MAX)
            1.0f, // Bright = ON
            0.0f, 0.0f,
            10.0f, // Master = 10
            5.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, // Reverb OFF
            4.0f, 0.0f, 0.0f, // Vibrato OFF
            (float)q, // Quality
            3.0f, // Cab bypass to hear raw amp
            0.0f, 7.0f,
            0.0f, 0.0f, 0.0f, 0.0f, false // No gate
        );

        // Check if there is sustained oscillation after sample 5000 (0.1s after pulse)
        float maxLate = 0.0f;
        int zeroCrossings = 0;
        for (int i = 5000; i < N; ++i) {
            if (fabsf(outL[i]) > maxLate) maxLate = fabsf(outL[i]);
            if (i > 5000 && ((outL[i] > 0) != (outL[i-1] > 0))) zeroCrossings++;
        }
        printf("Quality %dx: Max amplitude after pulse = %.6f, Zero-crossings = %d\n", (1 << q), maxLate, zeroCrossings);
    }

    // Now test with low-level guitar-like continuous sine
    printf("\nContinuous signal test with Clean Vol = 10:\n");
    for (int q = 0; q <= 3; ++q) {
        for (int i = 0; i < N; ++i) {
            float t = (float)i / (float)sr;
            in[i] = 0.05f * sinf(2.0f * (float)M_PI * 440.0f * t);
        }
        engine.reset();
        engine.process(
            in.data(), outL.data(), outR.data(), N,
            1.0f,
            0.0f, // Channel 0: Blackface
            10.0f, // Clean Vol = 10 (MAX)
            5.0f, 5.0f,
            5.0f, 5.0f,
            10.0f, // Treble = 10 (MAX)
            1.0f, // Bright = ON
            0.0f, 0.0f,
            10.0f, // Master = 10
            5.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            4.0f, 0.0f, 0.0f,
            (float)q,
            3.0f,
            0.0f, 7.0f,
            0.0f, 0.0f, 0.0f, 0.0f, false
        );
        // Measure high-frequency energy ratio (above 8kHz vs fundamental)
        double hfEnergy = 0.0, totalEnergy = 0.0;
        for (int i = 1000; i < N; ++i) {
            float diff = outL[i] - outL[i-1];
            hfEnergy += (double)(diff * diff);
            totalEnergy += (double)(outL[i] * outL[i]);
        }
        printf("Quality %dx: Total Energy = %.4f, Diff Energy = %.4f, Ratio = %.4f\n",
               (1 << q), totalEnergy, hfEnergy, hfEnergy / (totalEnergy + 1e-9));
    }

    return 0;
}
