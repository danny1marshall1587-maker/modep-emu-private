#ifndef CYBER_DENOISER_ENGINE_HPP
#define CYBER_DENOISER_ENGINE_HPP

#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace AudioDSP {

constexpr float PI_FLOAT = 3.14159265358979323846f;
constexpr int NUM_DENOISER_BANDS = 10;

/**
 * @brief Cyber-Denoiser PRO: Forensic Subtractive Spectral Phase-Cancellation Engine.
 *
 * Ported faithfully from the working JUCE/AU VST at:
 *   https://github.com/danny1marshall1587-maker/de-noise-pro-vst
 *
 * Architecture:
 *   10-band parallel 1st-order filter bank (zero latency, perfect reconstruction).
 *   Per-band spectral subtraction: gain = 1 - noise/signal.
 *   Dual-rate envelope with adaptive tail-stabilized release (prevents chirping).
 *   Learn mode captures peak noise floor over configurable duration.
 */
class CyberDenoiserEngine {
public:
    CyberDenoiserEngine() = default;
    ~CyberDenoiserEngine() = default;

    void prepare(double sampleRate) {
        mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        const float sRate = static_cast<float>(mSampleRate);

        mAttCoeff = 1.0f - std::exp(-1.0f / (0.1f * sRate / 1000.0f));
        mLevelAtt = 1.0f - std::exp(-1.0f / (2.0f * sRate / 1000.0f));
        mLevelRel = 1.0f - std::exp(-1.0f / (60.0f * sRate / 1000.0f));

        auto getCoeff = [sRate](float freq) {
            return 1.0f - std::exp(-2.0f * PI_FLOAT * freq / std::max(1.0f, sRate));
        };

        mHpF = getCoeff(40.0f);
        mLpF_cut = getCoeff(18000.0f);

        const float multi[9] = { 60.0f, 150.0f, 400.0f, 800.0f, 1500.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f };
        for (int i = 0; i < 9; ++i) {
            mBCoeffs[i] = std::min(0.99f, getCoeff(multi[i]));
        }

        reset();
    }

    void reset() {
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < 9; ++i) lpStates[ch][i] = 0.0f;
            for (int i = 0; i < NUM_DENOISER_BANDS; ++i) {
                levelStates[ch][i] = 0.0f;
                currentGains[ch][i] = 1.0f;
            }
            hpCutStat[ch] = 0.0f;
            lpCutStat[ch] = 0.0f;
        }

        for (int i = 0; i < NUM_DENOISER_BANDS; ++i) {
            thresholdsDb[i] = -84.0f;  // Studio baseline default: -84 dBFS
            peakLevels[i] = 1e-6f;
            updateLinearThreshold(i);
        }

        mLearnActive = false;
        mLearnSamplesElapsed = 0;
        mLearnSamplesTotal = static_cast<uint32_t>(mSampleRate * 5.0);
    }

    bool mHarmonicShield = false;

    void setHarmonicShield(bool enabled) { mHarmonicShield = enabled; }
    bool getHarmonicShield() const { return mHarmonicShield; }

    void setFloor(float f) {
        mFloor = std::clamp(f, 0.0f, 1.0f);
    }

    float getFloor() const {
        return mFloor;
    }

    void setThresholdDb(int band, float db) {
        if (band >= 0 && band < NUM_DENOISER_BANDS) {
            thresholdsDb[band] = std::clamp(db, -100.0f, 0.0f);
            updateLinearThreshold(band);
        }
    }

    float getThresholdDb(int band) const {
        if (band >= 0 && band < NUM_DENOISER_BANDS) return thresholdsDb[band];
        return -100.0f;
    }

    float getBandLevel(int band) const {
        if (band >= 0 && band < NUM_DENOISER_BANDS) {
            return std::max(levelStates[0][band], levelStates[1][band]);
        }
        return 0.0f;
    }

    void startLearn(float durationSeconds = 1.2f) {
        mLearnActive = true;
        mLearnSamplesElapsed = 0;
        mLearnSamplesTotal = static_cast<uint32_t>(mSampleRate * std::max(0.2f, durationSeconds));
        for (int i = 0; i < NUM_DENOISER_BANDS; ++i) {
            peakLevels[i] = 1e-6f;
        }
    }

    void cancelLearn() { mLearnActive = false; }
    bool isLearning() const { return mLearnActive; }

    float getLearnProgress() const {
        if (!mLearnActive || mLearnSamplesTotal == 0) return 0.0f;
        return std::clamp(static_cast<float>(mLearnSamplesElapsed) / mLearnSamplesTotal, 0.0f, 1.0f);
    }

    void setReductionAmount(float reductionPercent) {
        mReductionScale = std::clamp(reductionPercent / 100.0f, 0.0f, 1.0f);
    }

    void setThresholdOffsetDb(float offsetDb) {
        mThresholdOffsetDb = std::clamp(offsetDb, -20.0f, 20.0f);
        for (int i = 0; i < NUM_DENOISER_BANDS; ++i) {
            updateLinearThreshold(i);
        }
    }

    void setListenNoise(bool listen) { mListenNoise = listen; }
    void setLowCut(bool enable) { mLowCut = enable; }

    /**
     * @brief Process single sample — continuous zero-latency subtractive spectral phase cancellation.
     * With non-zero floor (mFloor = 0.15f), notes sustain naturally into -inf with zero gating!
     */
    inline float processSample(float inSample, int ch = 0) {
        float s0 = inSample;
        if (mLowCut) {
            hpCutStat[ch] += (s0 - hpCutStat[ch]) * mHpF;
            s0 -= hpCutStat[ch];
        }
        lpCutStat[ch] += (s0 - lpCutStat[ch]) * mLpF_cut;
        s0 = lpCutStat[ch];

        // 10-Band Filter Bank (1st order parallel subtraction, perfect reconstruction)
        float b[NUM_DENOISER_BANDS];
        float currentIn = s0;

        lpStates[ch][0] += (currentIn - lpStates[ch][0]) * mBCoeffs[0];
        b[0] = lpStates[ch][0];

        for (int i = 1; i < 9; ++i) {
            lpStates[ch][i] += (currentIn - lpStates[ch][i]) * mBCoeffs[i];
            b[i] = lpStates[ch][i] - lpStates[ch][i - 1];
        }
        b[9] = currentIn - lpStates[ch][8];

        float outSum = 0.0f;
        const float floorV = mFloor;

        float fundamentalActivity = 0.0f;
        if (mHarmonicShield) {
            for (int fBand = 1; fBand <= 3; ++fBand) {
                float rf = levelStates[ch][fBand];
                float rawG = 1.0f - (mLinearThresholds[fBand] / (rf + 1e-9f));
                fundamentalActivity = std::max(fundamentalActivity, std::clamp(rawG, 0.0f, 1.0f));
            }
        }

        for (int i = 0; i < NUM_DENOISER_BANDS; ++i) {
            float absVal = std::abs(b[i]);
            float coeff = (absVal > levelStates[ch][i]) ? mLevelAtt : mLevelRel;
            levelStates[ch][i] += (absVal - levelStates[ch][i]) * coeff;
            float r = levelStates[ch][i];

            if (mLearnActive && r > peakLevels[i]) {
                peakLevels[i] = r;
            }

            // Spectral Subtraction Gain: gain = 1 - noise/signal
            float effectiveThresh = mLinearThresholds[i];
            if (mHarmonicShield && i >= 4) {
                effectiveThresh *= (1.0f - 0.65f * fundamentalActivity);
            }
            float targetG = 1.0f - (effectiveThresh / (r + 1e-9f));
            targetG = std::max(floorV, std::min(1.0f, targetG));

            float scaledTargetG = 1.0f - mReductionScale * (1.0f - targetG);

            float currentCoeff;
            if (scaledTargetG > currentGains[ch][i]) {
                currentCoeff = mAttCoeff;
            } else {
                float tailDist = (currentGains[ch][i] - floorV) / (1.01f - floorV);
                float tailStab = 1.0f + (1.0f - std::min(1.0f, std::max(0.0f, tailDist))) * 8.0f;
                currentCoeff = mLevelRel / tailStab;
            }
            currentGains[ch][i] += (scaledTargetG - currentGains[ch][i]) * currentCoeff;

            outSum += b[i] * currentGains[ch][i];
        }

        return mListenNoise ? (s0 - outSum) : outSum;
    }

    /**
     * @brief Process block — faithful port of the VST's processBlock().
     *
     * Key differences from a noise gate:
     *   - Gain is computed as 1 - noise/signal (continuous spectral subtraction).
     *   - When guitar plays ABOVE threshold, gain approaches 1.0 but the noise
     *     estimate (tLinear) is STILL subtracted from the band, so noise never
     *     leaks through with the note.
     *   - When signal is BELOW threshold, gain approaches floor (0.0 = silence).
     *   - Adaptive tail-stabilized release prevents chirping at the noise floor.
     */
    void processBlock(const float* inL, const float* inR, float* outL, float* outR, uint32_t numSamples) {
        const float floorV = mFloor;

        bool isStereo = (inR != nullptr && inR != inL && outR != nullptr && outR != outL);
        int numChannels = isStereo ? 2 : 1;

        const float* inputs[2] = { inL, isStereo ? inR : inL };
        float* outputs[2] = { outL, isStereo ? outR : outL };

        for (int ch = 0; ch < numChannels; ++ch) {
            const float* inBuf = inputs[ch];
            float* outBuf = outputs[ch];

            for (uint32_t s = 0; s < numSamples; ++s) {
                float s0 = inBuf[s];

                // 1. Low Cut / High Cut filters (1st order, identical to VST)
                if (mLowCut) {
                    hpCutStat[ch] += (s0 - hpCutStat[ch]) * mHpF;
                    s0 -= hpCutStat[ch];
                }
                lpCutStat[ch] += (s0 - lpCutStat[ch]) * mLpF_cut;
                s0 = lpCutStat[ch];

                // 2. 10-Band Filter Bank (1st order parallel subtraction)
                //    sum(b[0..9]) == s0 perfectly (zero-latency reconstruction)
                float b[NUM_DENOISER_BANDS];
                float currentIn = s0;

                lpStates[ch][0] += (currentIn - lpStates[ch][0]) * mBCoeffs[0];
                b[0] = lpStates[ch][0];

                for (int i = 1; i < 9; ++i) {
                    lpStates[ch][i] += (currentIn - lpStates[ch][i]) * mBCoeffs[i];
                    b[i] = lpStates[ch][i] - lpStates[ch][i - 1];
                }
                b[9] = currentIn - lpStates[ch][8];

                // 3. Level Detection & Spectral Subtraction (VST core algorithm)
                float outSum = 0.0f;

                float fundamentalActivity = 0.0f;
                if (mHarmonicShield) {
                    for (int fBand = 1; fBand <= 3; ++fBand) {
                        float rf = levelStates[ch][fBand];
                        float rawG = 1.0f - (mLinearThresholds[fBand] / (rf + 1e-9f));
                        fundamentalActivity = std::max(fundamentalActivity, std::clamp(rawG, 0.0f, 1.0f));
                    }
                }

                for (int i = 0; i < NUM_DENOISER_BANDS; ++i) {
                    // Dual-rate peak follower: 2ms attack, 60ms decay
                    float absVal = std::abs(b[i]);
                    float coeff = (absVal > levelStates[ch][i]) ? mLevelAtt : mLevelRel;
                    levelStates[ch][i] += (absVal - levelStates[ch][i]) * coeff;
                    float r = levelStates[ch][i];

                    // Learn mode: capture peak noise envelope
                    if (mLearnActive && r > peakLevels[i]) {
                        peakLevels[i] = r;
                    }

                    // Spectral Subtraction Gain: gain = 1 - noise/signal
                    // When signal > noise: gain -> 1 (guitar passes, noise subtracted)
                    // When signal <= noise: gain -> floor (non-zero floor preserves decay tails)
                    float effectiveThresh = mLinearThresholds[i];
                    if (mHarmonicShield && i >= 4) {
                        effectiveThresh *= (1.0f - 0.65f * fundamentalActivity);
                    }
                    float targetG = 1.0f - (effectiveThresh / (r + 1e-9f));
                    targetG = std::max(floorV, std::min(1.0f, targetG));

                    // Scale by reduction amount (user depth control)
                    // reduction=100%: full noise cancellation
                    // reduction=0%: no noise cancellation (gain stays 1.0)
                    float scaledTargetG = 1.0f - mReductionScale * (1.0f - targetG);

                    // Adaptive Release with Tail Stabilization (VST's key anti-chirp trick)
                    float currentCoeff;
                    if (scaledTargetG > currentGains[ch][i]) {
                        currentCoeff = mAttCoeff;  // Fast attack (0.1ms)
                    } else {
                        // Slow down release as we approach the noise floor
                        // This prevents the "chirping" / "pumping" artifacts
                        float tailDist = (currentGains[ch][i] - floorV) / (1.01f - floorV);
                        float tailStab = 1.0f + (1.0f - std::min(1.0f, std::max(0.0f, tailDist))) * 8.0f;
                        currentCoeff = mLevelRel / tailStab;
                    }
                    currentGains[ch][i] += (scaledTargetG - currentGains[ch][i]) * currentCoeff;

                    // 4. Subtractive Phase Cancellation
                    //    noiseComponent = b[i] * (1 - gain)  [the part we remove]
                    //    cleanBand = b[i] - noiseComponent = b[i] * gain
                    outSum += b[i] * currentGains[ch][i];
                }

                // 5. Output: clean signal or listen to cancelled noise (delta mode)
                outBuf[s] = mListenNoise ? (s0 - outSum) : outSum;
            }
        }

        if (!isStereo && outR && outR != outL) {
            std::memcpy(outR, outL, numSamples * sizeof(float));
        }

        // Handle learn completion (VST: peak + 3dB headroom)
        if (mLearnActive) {
            mLearnSamplesElapsed += numSamples;
            if (mLearnSamplesElapsed >= mLearnSamplesTotal) {
                for (int i = 0; i < NUM_DENOISER_BANDS; ++i) {
                    float peak = peakLevels[i];
                    float db = 20.0f * std::log10(peak + 1e-6f) + 3.0f;
                    thresholdsDb[i] = std::clamp(db, -100.0f, -38.0f);
                    updateLinearThreshold(i);
                    currentGains[0][i] = 1.0f;
                    currentGains[1][i] = 1.0f;
                }
                mLearnActive = false;
            }
        }
    }

private:
    void updateLinearThreshold(int band) {
        if (band >= 0 && band < NUM_DENOISER_BANDS) {
            float effThresholdDb = thresholdsDb[band] + mThresholdOffsetDb;
            mLinearThresholds[band] = std::pow(10.0f, effThresholdDb * 0.05f);
        }
    }

    double mSampleRate = 48000.0;

    // Filter states [Channel][Band]
    float lpStates[2][9] = {};
    float hpCutStat[2] = {};
    float lpCutStat[2] = {};

    // Level detection & gain [Channel][Band]
    float levelStates[2][NUM_DENOISER_BANDS] = {};
    float currentGains[2][NUM_DENOISER_BANDS] = {};

    // 10-Band threshold settings in dB & linear
    float thresholdsDb[NUM_DENOISER_BANDS] = {};
    float mLinearThresholds[NUM_DENOISER_BANDS] = {};
    float peakLevels[NUM_DENOISER_BANDS] = {};

    // Cached filter & envelope coefficients
    float mAttCoeff = 0.0f;
    float mLevelAtt = 0.0f;
    float mLevelRel = 0.0f;
    float mHpF = 0.0f;
    float mLpF_cut = 0.0f;
    float mBCoeffs[9] = {};

    // Learn mode
    bool mLearnActive = false;
    uint32_t mLearnSamplesElapsed = 0;
    uint32_t mLearnSamplesTotal = 0;

    // User settings
    float mFloor = 0.05f;             // -26 dB non-zero spectral floor (deep studio noise reduction, zero gating)
    float mReductionScale = 1.0f;     // 100% depth
    float mThresholdOffsetDb = 0.0f;  // 0 dB fine trim
    bool  mListenNoise = false;
    bool  mLowCut = true;
};

} // namespace AudioDSP

#endif // CYBER_DENOISER_ENGINE_HPP
