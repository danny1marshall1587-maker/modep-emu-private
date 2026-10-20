#pragma once
#include "dsp_blocks.hpp"
#include <algorithm>
#include <cmath>

namespace ModepDSP {

// Dugan Automatic Mic Mixer (Gain Sharing across 8 active input channels)
class DuganAutoMixer {
public:
    static constexpr int NUM_CHANNELS = 8;

    DuganAutoMixer() : sampleRate(48000.0f), enabled(true) {
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            channelEnvelopes[i] = 0.0f;
            channelWeights[i] = 1.0f; // Default unity weight
            channelGains[i] = 1.0f;
            channelActive[i] = true;
        }
    }

    void init(float sr) {
        sampleRate = sr;
    }

    void setChannelWeight(int ch, float weight) {
        if (ch >= 0 && ch < NUM_CHANNELS) {
            channelWeights[ch] = std::clamp(weight, 0.0f, 2.0f);
        }
    }

    void setChannelActive(int ch, bool active) {
        if (ch >= 0 && ch < NUM_CHANNELS) {
            channelActive[ch] = active;
        }
    }

    void setEnabled(bool e) { enabled = e; }

    // Called once per block or sample to compute Dugan Gain Sharing factors
    void updateGainSharing(const float* currentLevels) {
        if (!enabled) {
            for (int i = 0; i < NUM_CHANNELS; ++i) {
                channelGains[i] = 1.0f;
            }
            return;
        }

        // Fast RMS envelope detector for each channel
        float attackCoeff = 0.05f;
        float releaseCoeff = 0.002f;

        float totalWeightedLevel = 1e-6f; // prevent division by zero

        for (int i = 0; i < NUM_CHANNELS; ++i) {
            if (!channelActive[i]) continue;
            float absIn = std::abs(currentLevels[i]);
            if (absIn > channelEnvelopes[i]) {
                channelEnvelopes[i] += attackCoeff * (absIn - channelEnvelopes[i]);
            } else {
                channelEnvelopes[i] += releaseCoeff * (absIn - channelEnvelopes[i]);
            }
            totalWeightedLevel += channelEnvelopes[i] * channelWeights[i];
        }

        // Calculate Dugan Gain Sharing: Gain_k = (Level_k * Weight_k) / TotalLevel
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            if (!channelActive[i]) {
                channelGains[i] = 1.0f;
                continue;
            }
            float weighted = channelEnvelopes[i] * channelWeights[i];
            float rawGain = weighted / totalWeightedLevel;
            // Floor at -18dB attenuation for background channels to avoid complete muting
            channelGains[i] = std::clamp(rawGain, 0.125f, 1.0f);
        }
    }

    inline float getChannelGain(int ch) const {
        if (ch >= 0 && ch < NUM_CHANNELS) return channelGains[ch];
        return 1.0f;
    }

private:
    float sampleRate;
    bool enabled;
    float channelEnvelopes[NUM_CHANNELS];
    float channelWeights[NUM_CHANNELS];
    float channelGains[NUM_CHANNELS];
    bool channelActive[NUM_CHANNELS];
};

// Dynamic FX Ducker (Allen & Heath style reverb/delay ducking triggered by vocal/guitar)
class DynamicFXDucker {
public:
    DynamicFXDucker() : sampleRate(48000.0f), threshold(0.1f), duckAmount(0.5f), 
                        sidechainEnv(0.0f), currentDuckingGain(1.0f) {}

    void init(float sr) {
        sampleRate = sr;
    }

    void setParams(float threshDB, float duckingDB) {
        threshold = std::pow(10.0f, threshDB / 20.0f);
        duckAmount = std::pow(10.0f, -std::abs(duckingDB) / 20.0f); // target gain when ducked
    }

    // Pass the key/sidechain signal (lead vocal or guitar) to evaluate ducking
    void updateSidechain(float sidechainSample) {
        float absIn = std::abs(sidechainSample);
        float attackCoeff = 0.05f;  // fast clamp on attack
        float releaseCoeff = 0.003f; // musical release (~150ms)

        if (absIn > sidechainEnv) {
            sidechainEnv += attackCoeff * (absIn - sidechainEnv);
        } else {
            sidechainEnv += releaseCoeff * (absIn - sidechainEnv);
        }

        if (sidechainEnv > threshold) {
            float excess = (sidechainEnv - threshold) / threshold;
            float target = std::clamp(1.0f - (excess * (1.0f - duckAmount)), duckAmount, 1.0f);
            currentDuckingGain = target;
        } else {
            currentDuckingGain = 1.0f;
        }
    }

    inline float processFX(float wetSample) const {
        return wetSample * currentDuckingGain;
    }

    float getDuckingGain() const { return currentDuckingGain; }

private:
    float sampleRate;
    float threshold;
    float duckAmount;
    float sidechainEnv;
    float currentDuckingGain;
};

} // namespace ModepDSP
