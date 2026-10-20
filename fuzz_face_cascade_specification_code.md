# Fluid Fuzz Face: Cascading Space Interruption & Input Calibration

## 1. Architectural Concept: Interrupting Laminar Flow
Instead of writing artificial waveshaping or static clipping code for the Fuzz Face, this design preserves the 100% unified physics engine by instantiating **two cascading `FluidTubeDSP` chambers** with mismatched physical geometries (acting like the Q1 and Q2 transistors in a vintage circuit).

### How it Creates Authentic Fuzz & Note Clashing:
*   **Chamber 1 (The Driver - `0.6x` Space):** Configured with a medium chamber volume and fast recovery. It takes the normalized guitar signal and pushes a strong, wide laminar wave forward.
*   **Chamber 2 (The Bottleneck - `0.12x` Space):** Configured as a tiny, constricted chamber with slow recovery and a deep negative starvation floor (`-0.45`). 
*   **Hydrodynamic Cavitation (Full Volume = 10):** When the guitar volume is on 10, the massive wave from Chamber 1 slams into the tiny `0.12x` bottleneck of Chamber 2. Simultaneously, the **Harmonic Slipstream Polarity** flips from `+1.0` to `-2.5`. Instead of the harmonics aligning in synergy, the overtones from Chamber 1 collide head-on into Chamber 2. This interrupts the laminar flow, causing organic phase inversion, bias starvation (Velcro sag), and classic intermodulation note-clashing on chords.
*   **Laminar Restoration (Rolled-Back Volume = 7):** When you roll your guitar volume knob back, the fluid pressure drops below the bottleneck threshold of Chamber 2, and the slipstream polarity returns to `+1.0`. The water flows gently through the tiny pipe without choking, naturally restoring a glassy, compressed clean tone.

---

## 2. The Two-Button "Volume Learn" Calibration System
Because audio interface Hi-Z preamps do not physically load down passive guitar pickups like a real Fuzz Face circuit does, fixed digital input gains fail to capture the razor-sharp cleanup between 7 and 10 on a guitar's volume knob. Furthermore, boosting the soundcard preamp to satisfy the fuzz ruins the gain staging for the clean amp.

### UI & Calibration Workflow for the Agent:
To solve this without touching the soundcard gain, the Fuzz pedal UI requires two calibration buttons:
1.  **`[Learn Max (10)]` Button:** The user puts their guitar volume knob at **10**, clicks this button, and strums firmly for a second. The pedal records the peak envelope of the pickups (`calibratedMaxLevel`).
2.  **`[Learn Clean (7)]` Button:** The user rolls their guitar volume knob back to their preferred sweet spot (e.g., **7**), clicks this button, and strums normally. The pedal records this lower threshold (`calibratedCleanLevel`).
3.  **Dynamic Cubic Scaling:** The DSP continuously tracks the guitar's input envelope between these two exact learned boundaries and applies a steep **cubic curve** (`x^3`). 
    *   At **Clean (7)**, the internal cascade drive drops to `0.8x` (pure laminar flow).
    *   At **Max (10)**, the internal cascade drive scales up to `8.8x` (severe bottleneck collision), while the final output is trimmed so the clean amp downstream receives the exact unity level it expects.

---

## 3. Standalone C++ Implementation (`FluidFuzzFace.hpp`)
You can hand this self-contained header/implementation directly to your coding agent to wire into the pedalboard module and UI buttons.

```cpp
#pragma once
#include <cmath>
#include <algorithm>

// ============================================================================
// CORE FLUID CHAMBER (Shared Laminar/Turbulent Physics Engine)
// ============================================================================
class FluidChamberDSP {
private:
    float sampleRate = 48000.0f;
    float previousPressure = 0.0f;
    float fluidMomentum = 0.0f;
    float chamberPressure = 1.0f;
    
    // Physical Space & Geometry Parameters
    float fluidViscosity = 0.85f;     // Inertia / slew rate
    float harmonicPurity = 0.4f;      // Even vs Odd geometry
    float chamberRecovery = 0.002f;   // Sag refill speed
    float chamberSizeScalar = 1.0f;   // X0.1 to X10 space multiplier
    float minStarvationFloor = 0.1f;  // Choke limit (< 0.0 causes phase sputter)
    
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
        generatedHarmonics = 0.0f;
    }

    // synergyPolarity: +1.0 = Laminar Synergy, Negative (-2.5) = Flow Interruption / Note Clash
    float processSample(float inputPressure, float driveLevel, float incomingHarmonics = 0.0f, float synergyPolarity = 1.0f) {
        float currentVelocity = inputPressure - previousPressure;
        fluidMomentum = (fluidMomentum * fluidViscosity) + (currentVelocity * (1.0f - fluidViscosity));

        // Smaller chamberSizeScalar (e.g., 0.12x) drains rapidly under high drive, choking the flow
        float energyDraw = (std::abs(inputPressure) * driveLevel) / std::max(0.05f, chamberSizeScalar);
        chamberPressure -= energyDraw * 0.01f;
        chamberPressure += (1.0f - chamberPressure) * chamberRecovery;
        chamberPressure = std::max(minStarvationFloor, std::min(chamberPressure, 1.0f));

        // Amplify and apply chamber compression/starvation
        float amplifiedSignal = inputPressure * driveLevel * chamberPressure;
        
        // Inject slipstream harmonics from previous space
        amplifiedSignal += incomingHarmonics * 0.35f * synergyPolarity;

        float outputPhase;
        if (amplifiedSignal > 0.0f) {
            outputPhase = std::tanh(amplifiedSignal) + (harmonicPurity * std::pow(amplifiedSignal, 2.0f) * 0.5f);
        } else {
            outputPhase = std::tanh(amplifiedSignal) - (harmonicPurity * std::pow(std::abs(amplifiedSignal), 2.5f) * 0.2f);
        }

        generatedHarmonics = outputPhase - std::tanh(amplifiedSignal);

        outputPhase += fluidMomentum * 0.5f;
        float finalOutput = std::tanh(outputPhase);
        previousPressure = finalOutput;

        return finalOutput;
    }

    float getGeneratedHarmonics() const { return generatedHarmonics; }
};

// ============================================================================
// CASCADING 2-SPACE FUZZ FACE PEDAL (With 2-Button Input Level Learn)
// ============================================================================
class FluidFuzzFacePedal {
private:
    FluidChamberDSP space1_Q1; // Wide driver space (0.60x)
    FluidChamberDSP space2_Q2; // Constricted bottleneck space (0.12x)

    // User-Calibrated Guitar Volume Thresholds
    float calibratedMaxLevel = 0.25f;   // Default fallback for Volume 10
    float calibratedCleanLevel = 0.06f; // Default fallback for Volume 7
    float currentEnvelope = 0.0f;

    // Learn State Flags for UI Buttons
    bool isLearningMax = false;
    bool isLearningClean = false;
    float learnPeakTracker = 0.0f;

public:
    void prepare(float sampleRate) {
        space1_Q1.prepare(sampleRate);
        space2_Q2.prepare(sampleRate);
        currentEnvelope = 0.0f;

        // SPACE 1 (Q1): 0.6x size, fast recovery, feeds strong laminar wave into Space 2
        space1_Q1.configureSpace(0.60f, 0.2f, 0.010f, 0.60f, 0.10f);

        // SPACE 2 (Q2): Tiny 0.12x bottleneck, slow recovery, deep starvation (-0.45)
        // Chokes and inverts when slammed by Space 1, but flows cleanly at low pressure!
        space2_Q2.configureSpace(0.30f, 0.6f, 0.004f, 0.12f, -0.45f);
    }

    // ------------------------------------------------------------------------
    // UI BUTTON BINDINGS FOR ANTIGRAVITY / PLUGIN INTERFACE
    // ------------------------------------------------------------------------
    
    // Call when user presses "Learn Max (Vol 10)" button
    void triggerLearnMaxVolume() {
        isLearningMax = true;
        isLearningClean = false;
        learnPeakTracker = 0.0001f;
    }

    // Call when user presses "Learn Clean (Vol 7)" button
    void triggerLearnCleanVolume() {
        isLearningClean = true;
        isLearningMax = false;
        learnPeakTracker = 0.0001f;
    }

    // Call when user releases button or clicks "Save Calibration"
    void finishLearning() {
        if (isLearningMax) {
            calibratedMaxLevel = std::max(0.005f, learnPeakTracker);
            // Safety guard: ensure Clean threshold is always below Max
            if (calibratedCleanLevel >= calibratedMaxLevel) {
                calibratedCleanLevel = calibratedMaxLevel * 0.3f;
            }
        } else if (isLearningClean) {
            calibratedCleanLevel = std::max(0.001f, learnPeakTracker);
            // Safety guard: ensure Max threshold is always above Clean
            if (calibratedMaxLevel <= calibratedCleanLevel) {
                calibratedMaxLevel = calibratedCleanLevel * 3.5f;
            }
        }
        isLearningMax = false;
        isLearningClean = false;
    }

    // Optional getters so the UI can display the learned threshold values or save them in presets
    float getCalibratedMax() const { return calibratedMaxLevel; }
    float getCalibratedClean() const { return calibratedCleanLevel; }
    void setCalibration(float maxVal, float cleanVal) {
        calibratedMaxLevel = maxVal;
        calibratedCleanLevel = cleanVal;
    }

    // ------------------------------------------------------------------------
    // AUDIO PROCESSING
    // ------------------------------------------------------------------------
    float processSample(float inputSignal, float fuzzKnob = 1.0f, float outputVolumeKnob = 0.75f) {
        float absInput = std::abs(inputSignal);

        // 1. Envelope Follower (Tracks Guitar Volume Knob Position)
        const float attack = 0.015f;
        const float release = 0.0004f;
        if (absInput > currentEnvelope) {
            currentEnvelope += (absInput - currentEnvelope) * attack;
        } else {
            currentEnvelope += (absInput - currentEnvelope) * release;
        }

        // Capture peaks if either Learn Button is currently active
        if (isLearningMax || isLearningClean) {
            if (currentEnvelope > learnPeakTracker) {
                learnPeakTracker = currentEnvelope;
            }
        }

        // 2. Calculate Dynamic Guitar Volume Cleanup Scalar (0.0 = Clean at 7, 1.0 = Fuzz at 10)
        float dynamicRange = std::max(0.001f, calibratedMaxLevel - calibratedCleanLevel);
        float normalizedGuitarVol = (currentEnvelope - calibratedCleanLevel) / dynamicRange;
        normalizedGuitarVol = std::max(0.0f, std::min(normalizedGuitarVol, 1.5f));

        // Cubic taper mimics the razor-sharp impedance drop of a real Strat volume pot
        float cleanupCurve = std::pow(normalizedGuitarVol, 3.0f);

        // Normalize raw soundcard input against the user's learned Max guitar level
        float normalizedSignal = inputSignal / std::max(0.01f, calibratedMaxLevel);

        // 3. Scale Drive & Flow Interruption Across the Two Cascading Spaces
        float space1Drive = 0.8f + (cleanupCurve * fuzzKnob * 5.0f);
        float space2Drive = 0.8f + (cleanupCurve * fuzzKnob * 8.0f);

        // Run Space 1 (0.60x Driver Chamber)
        float space1Out = space1_Q1.processSample(normalizedSignal, space1Drive, 0.0f, 1.0f);
        float space1Harmonics = space1_Q1.getGeneratedHarmonics();

        // Calculate Slipstream Interruption Polarity:
        // At Guitar Vol 7 (cleanupCurve == 0): Polarity is +1.0 (Laminar glassy flow)
        // At Guitar Vol 10 (cleanupCurve == 1): Polarity flips to -2.5 (Harmonics collide head-on!)
        float slipstreamPolarity = 1.0f - (cleanupCurve * 3.5f);

        // Run Space 2 (0.12x Bottleneck Chamber)
        float space2Out = space2_Q2.processSample(space1Out, space2Drive, space1Harmonics, slipstreamPolarity);

        // Return level-matched output ready for the Clean Amp input
        return space2Out * outputVolumeKnob;
    }
};
```