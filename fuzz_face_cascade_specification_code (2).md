# Fluid Fuzz Face: Direct dB Ratio Scaling & Sustain RMS Calibration

## 1. Why the Previous Scaling & 10-Second Peak Listener Failed
1. **The "Pick-Spike" Trap:** Guitar pick transients are 3x to 4x louder than the sustaining body of a chord. A peak detector (`max()`) over 10 seconds records the single loudest microsecond pick-hit. Immediately after the pick attack, the sustaining chord sits way below that peak—tricking the pedal into backing off the fuzz so it never goes "full bore"!
2. **Linear Voltage vs. Logarithmic Decibels (dB):** Guitar volume pots and human ears operate in **Decibels ($20 \log_{10}$)**, not linear float math. 
3. **Envelope-Driven Gain vs. Direct Input Ratio:** In a real Fuzz Face, rolling your guitar volume knob from 10 to 7 doesn't move an internal envelope knob—it directly changes how hard the raw guitar waveform hits Q1 and Q2! 

---

## 2. The Fix: Dual-Anchor Direct Input Gain + dB Sustain Learner
Instead of capturing a momentary pick spike, the new **Sustain Energy Learner** measures the **Continuous RMS Body** of your guitar while ignoring transient spikes, and gives you a live **Visual Position Meter (`0%` = Clean 7, `100%` = Full Bore 10)**:

1. **Direct Clean Anchor (`calibratedCleanRMS`):** When your guitar is at Volume 7, the pedal scales the input signal so that its RMS energy sits right at **`0.35`** (the exact edge of glassy warmth where `tanh(0.35)` is 96% linear clean).
2. **Direct Full-Bore Anchor (`calibratedMaxRMS`):** When your guitar is at Volume 10, the pedal scales the signal in **Decibels (dB)** so that it slams Space 1 and Space 2 with **`16.0x` to `24.0x` total fluid drive**—giving you massive, thick, endless full-bore fuzz!
3. **Automatic Transient-Proof Auto-Learn:** During the learn window (whether 3 seconds or 10 seconds), the tracker locks onto the **Upper-Average Sustain Body** of your strumming (ignoring silent gaps AND ignoring 2ms pick spikes) so Volume 10 is genuinely Full Bore during the entire chord sustain!

---

## 3. Updated Standalone C++ Implementation (`FluidFuzzFace.hpp`)
Hand this directly to your coding agent to replace the old Fuzz Face class:

```cpp
#pragma once
#include <cmath>
#include <algorithm>

// ============================================================================
// CORE FLUID CHAMBER (High-Gain Capable Laminar/Turbulent Physics Engine)
// ============================================================================
class FluidChamberDSP {
private:
    float sampleRate = 48000.0f;
    float previousPressure = 0.0f;
    float fluidMomentum = 0.0f;
    float chamberPressure = 1.0f;
    float dcBlockerX1 = 0.0f;
    float dcBlockerY1 = 0.0f;
    
    // Physical Space & Geometry Parameters
    float fluidViscosity = 0.75f;     
    float harmonicPurity = 0.45f;     
    float chamberRecovery = 0.005f;   
    float chamberSizeScalar = 1.0f;   
    float minStarvationFloor = 0.28f; // Positive cushion: rich compression, zero Velcro gating
    
    float generatedHarmonics = 0.0f;

public:
    void prepare(float newSampleRate) {
        sampleRate = newSampleRate;
        reset();
    }

    void configureSpace(float viscosity, float purity, float recovery, float sizeScalar, float starvationFloor) {
        fluidViscosity = viscosity;
        harmonicPurity = purity;
        chamberRecovery = recovery;
        chamberSizeScalar = sizeScalar;
        minStarvationFloor = starvationFloor;
    }

    void reset() {
        previousPressure = 0.0f;
        fluidMomentum = 0.0f;
        chamberPressure = 1.0f;
        dcBlockerX1 = 0.0f;
        dcBlockerY1 = 0.0f;
        generatedHarmonics = 0.0f;
    }

    float processSample(float inputPressure, float driveLevel, float incomingHarmonics = 0.0f, float synergyPolarity = 1.0f) {
        float currentVelocity = inputPressure - previousPressure;
        fluidMomentum = (fluidMomentum * fluidViscosity) + (currentVelocity * (1.0f - fluidViscosity));

        // Dynamic fluid sag (bounded so high drive never chokes the volume!)
        float boundedInput = std::min(std::abs(inputPressure), 1.5f);
        float energyDraw = (boundedInput * std::min(driveLevel, 4.0f)) / std::max(0.20f, chamberSizeScalar);
        chamberPressure -= energyDraw * 0.0025f;
        chamberPressure += (1.0f - chamberPressure) * chamberRecovery;
        chamberPressure = std::max(minStarvationFloor, std::min(chamberPressure, 1.0f));

        // Pre-gain + Slipstream injection
        float amplifiedSignal = (inputPressure * driveLevel * chamberPressure) + (incomingHarmonics * 0.35f * synergyPolarity);

        // Rich Asymmetrical Germanium Saturation Curve
        float outputPhase;
        if (amplifiedSignal >= 0.0f) {
            // Positive lobe saturates hard and warm into thick singing sustain
            outputPhase = std::tanh(amplifiedSignal * 1.15f) + (harmonicPurity * std::tanh(amplifiedSignal * amplifiedSignal) * 0.40f);
        } else {
            // Negative lobe stays slightly rounder (classic Q1/Q2 bias asymmetry)
            outputPhase = std::tanh(amplifiedSignal * 0.85f) - (harmonicPurity * std::tanh(amplifiedSignal * amplifiedSignal) * 0.22f);
        }

        generatedHarmonics = outputPhase - std::tanh(amplifiedSignal);

        // High-frequency fluid damping (turns harsh digital fizz into thick creamy lead tone)
        float viscosityCoeff = fluidViscosity * 0.42f;
        outputPhase = (outputPhase * (1.0f - viscosityCoeff)) + (previousPressure * viscosityCoeff);
        
        // Remove DC offset built up by asymmetrical saturation so the next stage hits full-bore symmetrically!
        float dcFiltered = outputPhase - dcBlockerX1 + 0.995f * dcBlockerY1;
        dcBlockerX1 = outputPhase;
        dcBlockerY1 = dcFiltered;

        float finalOutput = std::tanh(dcFiltered * 1.25f);
        previousPressure = finalOutput;

        return finalOutput;
    }

    float getGeneratedHarmonics() const { return generatedHarmonics; }
};

// ============================================================================
// CASCADING 2-SPACE CLASSIC FUZZ FACE (Sustain-RMS Learn + Decibel Scaling)
// ============================================================================
class FluidFuzzFacePedal {
private:
    FluidChamberDSP space1_Q1; // Q1: High-sensitivity input space
    FluidChamberDSP space2_Q2; // Q2: Full-bore Germanium saturation space

    float sampleRate = 48000.0f;

    // Calibrated RMS Sustain Anchors (NOT raw pick-transients!)
    float calibratedMaxRMS   = 0.080f; // Typical guitar sustaining chord at Vol 10
    float calibratedCleanRMS = 0.018f; // Typical guitar sustaining chord at Vol 7

    // True RMS Envelope (Ignores 2ms pick spikes, tracks the real body of the note)
    float rmsSquaredFiltered = 0.0f;
    float currentRMS = 0.0f;
    float smoothedFuzzMorph = 0.0f; // 0.0 = Clean (Vol 7), 1.0 = Full Bore (Vol 10)

    // Timed Auto-Learn State (e.g. 5-second or 10-second listening window)
    bool isLearningMax = false;
    bool isLearningClean = false;
    int learnSamplesRemaining = 0;
    float learnSustainAccumulator = 0.0f;
    int learnActiveSampleCount = 0;
    float learnGateThreshold = 0.0015f;

public:
    void prepare(float sr) {
        sampleRate = sr;
        space1_Q1.prepare(sr);
        space2_Q2.prepare(sr);
        rmsSquaredFiltered = 0.0f;
        currentRMS = 0.0f;
        smoothedFuzzMorph = 0.0f;

        // SPACE 1 (Q1): Medium-wide chamber (0.75x), warm viscosity (0.68)
        space1_Q1.configureSpace(0.68f, 0.40f, 0.008f, 0.75f, 0.38f);

        // SPACE 2 (Q2): 0.42x compression chamber, creamy viscosity (0.76), positive 0.28 floor
        space2_Q2.configureSpace(0.76f, 0.50f, 0.005f, 0.42f, 0.28f);
    }

    // Call this when user clicks "Learn Max (10)" — listens for `seconds` (default 4.0s)
    void triggerLearnMaxVolume(float seconds = 4.0f) {
        isLearningMax = true;
        isLearningClean = false;
        learnSamplesRemaining = static_cast<int>(sampleRate * seconds);
        learnSustainAccumulator = 0.0f;
        learnActiveSampleCount = 0;
    }

    // Call this when user clicks "Learn Clean (7)" — listens for `seconds` (default 4.0s)
    void triggerLearnCleanVolume(float seconds = 4.0f) {
        isLearningClean = true;
        isLearningMax = false;
        learnSamplesRemaining = static_cast<int>(sampleRate * seconds);
        learnSustainAccumulator = 0.0f;
        learnActiveSampleCount = 0;
    }

    void finishLearning() {
        if (learnActiveSampleCount > 100) {
            // Multiply average sustain RMS by 1.15 so normal playing easily hits 100% Full Bore!
            float measuredSustainRMS = (learnSustainAccumulator / static_cast<float>(learnActiveSampleCount)) * 1.15f;

            if (isLearningMax) {
                calibratedMaxRMS = std::max(0.004f, measuredSustainRMS);
                // Ensure Clean RMS is always at least 8 dB (2.5x) below Max RMS
                if (calibratedCleanRMS > calibratedMaxRMS * 0.45f) {
                    calibratedCleanRMS = calibratedMaxRMS * 0.25f;
                }
            } else if (isLearningClean) {
                calibratedCleanRMS = std::max(0.0008f, measuredSustainRMS);
                // Ensure Max RMS is always at least 8 dB (2.5x) above Clean RMS
                if (calibratedMaxRMS < calibratedCleanRMS * 2.2f) {
                    calibratedMaxRMS = calibratedCleanRMS * 4.0f;
                }
            }
        }
        isLearningMax = false;
        isLearningClean = false;
        learnSamplesRemaining = 0;
    }

    bool isCurrentlyLearning() const { return isLearningMax || isLearningClean; }
    float getCurrentFuzzMorph() const { return smoothedFuzzMorph; } // 0.0 (Clean) to 1.0 (Full Bore) for UI meter!
    float getCalibratedMax() const { return calibratedMaxRMS; }
    float getCalibratedClean() const { return calibratedCleanRMS; }
    void setCalibration(float maxRMS, float cleanRMS) {
        calibratedMaxRMS = maxRMS;
        calibratedCleanRMS = cleanRMS;
    }

    // ------------------------------------------------------------------------
    // AUDIO PROCESSING (Logarithmic dB Mapping + True Full-Bore Cascade)
    // ------------------------------------------------------------------------
    float processSample(float inputSignal, float fuzzKnob = 1.0f, float outputVolumeKnob = 0.80f) {
        // 1. Calculate True Sustain RMS (35ms window Smoothly ignores microsecond pick spikes!)
        float squared = inputSignal * inputSignal;
        const float rmsCoeff = 0.0006f; 
        rmsSquaredFiltered += (squared - rmsSquaredFiltered) * rmsCoeff;
        currentRMS = std::sqrt(std::max(0.0f, rmsSquaredFiltered));

        // 2. If Learning is active, accumulate the sustaining body of the guitar signal
        if (isLearningMax || isLearningClean) {
            if (currentRMS > learnGateThreshold) {
                learnSustainAccumulator += currentRMS;
                learnActiveSampleCount++;
            }
            if (--learnSamplesRemaining <= 0) {
                finishLearning();
            }
        }

        // 3. Convert current RMS, Clean RMS, and Max RMS into Decibels (dB)
        // Because guitar volume pots are logarithmic, dB mapping makes Vol 7 -> Vol 10 feel 100% natural!
        float currentDB = 20.0f * std::log10(std::max(0.00001f, currentRMS));
        float cleanDB   = 20.0f * std::log10(std::max(0.0001f, calibratedCleanRMS));
        float maxDB     = 20.0f * std::log10(std::max(0.0005f, calibratedMaxRMS));

        float dbSpan = std::max(4.0f, maxDB - cleanDB); // Minimum 4 dB window between 7 and 10
        float rawMorph = (currentDB - cleanDB) / dbSpan;
        rawMorph = std::max(0.0f, std::min(rawMorph, 1.0f)); // 0.0 at Vol 7, 1.0 at Vol 10!

        // Smooth morph transitions (fast rise on strum, smooth bloom on decay)
        float morphSpeed = (rawMorph > smoothedFuzzMorph) ? 0.015f : 0.0015f;
        smoothedFuzzMorph += (rawMorph - smoothedFuzzMorph) * morphSpeed;

        // 4. Direct Input Normalization (Fixes Quiet Interface Preamp Levels Automatically!)
        // Scales input so that when guitar is at Clean (Vol 7), peak wave is ~0.35 (sparkling clean),
        // and when guitar is at Max (Vol 10), wave is boosted into true Full-Bore saturation!
        float baseNormalization = 0.32f / std::max(0.001f, calibratedCleanRMS);
        float normalizedWave = inputSignal * baseNormalization;

        // 5. True Dual-Stage Gain Scaling (Clean Sparkle at 0.0 -> Massive 22x Fuzz at 1.0!)
        float activeFuzz = std::max(0.1f, fuzzKnob);
        // At Morph 0.0 (Vol 7): Space 1 Drive = 0.85x, Space 2 Drive = 0.90x (Glassy Warm Clean!)
        // At Morph 1.0 (Vol 10): Space 1 Drive = 4.20x, Space 2 Drive = 5.50x (Total ~23x Cascaded Full-Bore Fuzz!)
        float space1Drive = 0.85f + (smoothedFuzzMorph * smoothedFuzzMorph * activeFuzz * 3.35f);
        float space2Drive = 0.90f + (smoothedFuzzMorph * smoothedFuzzMorph * activeFuzz * 4.60f);

        float space1Out = space1_Q1.processSample(normalizedWave, space1Drive, 0.0f, 1.0f);
        float space1Harmonics = space1_Q1.getGeneratedHarmonics();

        // Slipstream glides from +1.0 (Laminar Clean) to -0.75 (Rich Germanium Grind)
        float slipstreamPolarity = 1.0f - (smoothedFuzzMorph * 1.75f);

        float space2Out = space2_Q2.processSample(space1Out, space2Drive, space1Harmonics, slipstreamPolarity);

        // 6. Clean-to-Fuzz Volume Compensation
        // Because Vol 7 is un-clipped (0.32 peak) and Vol 10 is hard-saturated (0.95 peak),
        // we boost the output when smoothedFuzzMorph is near 0.0 so Clean (7) is just as loud as Full Fuzz (10)!
        float cleanLevelBoost = 2.1f - (1.1f * smoothedFuzzMorph);

        return space2Out * cleanLevelBoost * outputVolumeKnob;
    }
};
```