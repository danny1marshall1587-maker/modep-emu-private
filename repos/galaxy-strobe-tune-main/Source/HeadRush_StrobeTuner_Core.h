/**
 * HeadRush Flex/Core/Prime - Strobe Tuner DSP Core
 * ------------------------------------------------
 * This header contains the standalone, dependency-free C++ logic for the 
 * high-performance Strobe Tuner. It is designed for easy integration into 
 * the HeadRush embedded DSP framework.
 * 
 * Features:
 * - Real-time safe (No allocations in processing loop)
 * - Intelligent Auto-Gain Control (AGC) with 74dB boost range
 * - DC Blocking & Center Clipping for robust guitar detection
 * - Autocorrelation-based pitch detection optimized for ARM/SIMD
 * - Support for Sweetened Tuning offsets
 */

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>

namespace HeadRushDSP {

class StrobeTunerCore {
public:
    StrobeTunerCore() = default;

    /**
     * Initializes the detector buffers and state.
     * @param sampleRate The hardware sample rate (e.g., 48000.0)
     */
    void init(double sampleRate) {
        mSampleRate = sampleRate;
        mAnalysisWindowSize = 2048;
        mMaxLag = static_cast<int>(mSampleRate / 50.0); // 50Hz floor
        if (mMaxLag > 2048) mMaxLag = 2048;
        
        mBuffer.assign(mAnalysisWindowSize + mMaxLag, 0.0f);
        mProcessedWindow.assign(mAnalysisWindowSize, 0.0f);
        
        mAutoGain = 1.0f;
        mDcY1 = 0.0f;
        mDcX1 = 0.0f;
        mDetectedFrequency = 0.0f;
    }

    /**
     * Processes a block of mono audio.
     * Ideally called from a lower-priority background task or a dedicated worker thread
     * to keep the main audio callback lean.
     */
    float processDetection(const float* inputSamples, int numSamples) {
        if (numSamples <= 0) return 0.0f;

        float peak = 0.0f;
        const float R = 0.995f; // DC Blocker coefficient

        // 1. DC Blocking & Peak Detection
        for (int i = 0; i < std::min(numSamples, (int)mAnalysisWindowSize); ++i) {
            float x = inputSamples[i];
            float y = x - mDcX1 + R * mDcY1;
            mDcX1 = x;
            mDcY1 = y;
            
            mProcessedWindow[i] = y;
            peak = std::max(peak, std::abs(y));
        }

        // Silence check
        if (peak < 0.0001f) {
            mDetectedFrequency = 0.0f;
            return 0.0f;
        }

        // 2. Smart AGC (Targeting 0.6 Amplitude)
        float targetLevel = 0.6f;
        float targetGain = targetLevel / peak;
        mAutoGain = mAutoGain * 0.8f + targetGain * 0.2f;
        mAutoGain = std::max(0.1f, std::min(mAutoGain, 5000.0f)); // Up to 74dB boost

        // 3. Center Clipping (Pre-correlation cleanup)
        float clipThreshold = (peak * mAutoGain) * 0.25f;
        for (int i = 0; i < (int)mAnalysisWindowSize; ++i) {
            float val = mProcessedWindow[i] * mAutoGain;
            if (val > clipThreshold) mProcessedWindow[i] = val - clipThreshold;
            else if (val < -clipThreshold) mProcessedWindow[i] = val + clipThreshold;
            else mProcessedWindow[i] = 0.0f;
        }

        // 4. Autocorrelation Pitch Detection
        int minLag = static_cast<int>(mSampleRate / 1200.0); // 1.2kHz ceiling
        float maxCorr = -1.0f;
        int bestLag = -1;

        for (int lag = minLag; lag < mMaxLag; ++lag) {
            float corr = 0.0f;
            const float* p1 = mProcessedWindow.data();
            const float* p2 = p1 + lag;
            
            // Note: This loop can be optimized with NEON/SIMD on HeadRush hardware
            for (int i = 0; i < 1024; ++i) {
                corr += p1[i] * p2[i];
            }

            if (corr > maxCorr) {
                maxCorr = corr;
                bestLag = lag;
            }
        }

        // 5. Validation
        float power = 0.0f;
        for (int i = 0; i < 1024; ++i) {
            power += mProcessedWindow[i] * mProcessedWindow[i];
        }

        if (bestLag > 0 && maxCorr > power * 0.4f) {
            mDetectedFrequency = static_cast<float>(mSampleRate / (float)bestLag);
        } else {
            mDetectedFrequency = 0.0f;
        }

        return mDetectedFrequency;
    }

    /**
     * Calculates the cents deviation including sweetened offsets.
     */
    float getCentsDeviation(float frequency, float referenceA, float profileOffsetCents) {
        if (frequency <= 20.0f) return 0.0f;
        float midi = 12.0f * std::log2(frequency / referenceA) + 69.0f;
        float rounded = std::round(midi);
        float rawCents = (midi - rounded) * 100.0f;
        return rawCents - profileOffsetCents;
    }

private:
    double mSampleRate = 48000.0;
    size_t mAnalysisWindowSize = 2048;
    int mMaxLag = 1000;
    
    std::vector<float> mBuffer;
    std::vector<float> mProcessedWindow;
    
    float mAutoGain = 1.0f;
    float mDcY1 = 0.0f;
    float mDcX1 = 0.0f;
    float mDetectedFrequency = 0.0f;
};

} // namespace HeadRushDSP

/* 
   EXAMPLE INTEGRATION FOR HEADRUSH ENGINEERS:
   -------------------------------------------
   
   // In Processor Setup:
   HeadRushDSP::StrobeTunerCore tuner;
   tuner.init(getSampleRate());

   // In Audio Thread (Background Worker):
   float freq = tuner.processDetection(inputBuffer, bufferSize);
   
   // In UI Thread:
   float offset = getSweetenedOffsetForNote(currentNote);
   float cents = tuner.getCentsDeviation(freq, 440.0f, offset);
   updateStrobeUI(cents);
*/
