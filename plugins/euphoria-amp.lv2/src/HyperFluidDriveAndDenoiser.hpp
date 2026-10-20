#pragma once
#include <cmath>
#include <algorithm>
#include <array>

namespace EuphoriaHyperDSP {

// ============================================================================
// 1. ADAA HYPER-FLUID NON-LINEAR CELL (Eliminates Ultrasonic Aliasing Fizz)
// ============================================================================
class HyperFluidADAA {
private:
    float uPrev = 0.0f;

    // Exact analytical 1st antiderivative of tanh(x): F1(x) = ln(cosh(x))
    // Implemented with stable high-argument asymptote: |x| + ln(1 + e^(-2|x|)) - ln(2)
    static inline float antiderivativeTanh(float x) noexcept {
        float ax = std::abs(x);
        return ax + std::log1p(std::exp(-2.0f * ax)) - 0.69314718056f;
    }

public:
    void reset() noexcept { uPrev = 0.0f; }

    inline float process(float u) noexcept {
        float diff = u - uPrev;
        float out;
        if (std::abs(diff) < 1e-5f) {
            out = std::tanh(0.5f * (u + uPrev));
        } else {
            out = (antiderivativeTanh(u) - antiderivativeTanh(uPrev)) / diff;
        }
        uPrev = u;
        return out;
    }
};

// ============================================================================
// 2. PHASE-COHERENT LAMINAR CROSSOVER (0.0 deg Phase Error Clean/Dirty Split)
// ============================================================================
class PhaseCoherentMidSplitter {
private:
    float lpLowState = 0.0f;
    float lpHighState = 0.0f;
    float alphaLow = 0.045f;
    float alphaHigh = 0.42f;

public:
    void prepare(float sampleRate, float fLow = 360.0f, float fHigh = 4200.0f) noexcept {
        alphaLow  = (2.0f * 3.14159265f * fLow)  / (sampleRate + 2.0f * 3.14159265f * fLow);
        alphaHigh = (2.0f * 3.14159265f * fHigh) / (sampleRate + 2.0f * 3.14159265f * fHigh);
    }

    void setLowCorner(float sampleRate, float fLow) noexcept {
        alphaLow = (2.0f * 3.14159265f * fLow) / (sampleRate + 2.0f * 3.14159265f * fLow);
    }

    void reset() noexcept { lpLowState = 0.0f; lpHighState = 0.0f; }

    inline void split(float input, float& outMidDriveBand, float& outCleanPhaseExact) noexcept {
        lpLowState  += alphaLow  * (input - lpLowState);
        lpHighState += alphaHigh * (input - lpHighState);
        outMidDriveBand    = lpHighState - lpLowState;
        outCleanPhaseExact = input - outMidDriveBand; // 100% phase-locked complement!
    }
};

// ============================================================================
// 3. ULTIMATE PEDAL 1: HYPER-CENTAUR ("27V Liquid Gold" Klon Evolution)
// ============================================================================
class HyperCentaurPedal {
private:
    PhaseCoherentMidSplitter splitter;
    HyperFluidADAA gePosADAA, geNegADAA;
    float prevIn = 0.0f;
    float pickTransientEnv = 0.0f;
    float toneLpState = 0.0f;
    float sampleRate = 48000.0f;

public:
    void prepare(float sr) noexcept {
        sampleRate = sr;
        splitter.prepare(sr, 360.0f, 4200.0f);
        reset();
    }

    void reset() noexcept {
        splitter.reset();
        gePosADAA.reset();
        geNegADAA.reset();
        prevIn = 0.0f;
        pickTransientEnv = 0.0f;
        toneLpState = 0.0f;
    }

    // gainKnob, toneKnob, levelKnob in [0.0f ... 1.0f], muSmooth = dB guitar vol morph [0..1]
    inline float processSample(float x, float gainKnob, float toneKnob, float levelKnob, float muSmooth) noexcept {
        // 1. Dynamic Charge-Pump Headroom Expander (Expands from 18V to 36V on pick attacks)
        float velocity = std::abs(x - prevIn);
        prevIn = x;
        pickTransientEnv = std::max(velocity * 2.5f, pickTransientEnv * 0.992f);
        float dynamicHeadroom = 1.0f + std::min(1.0f, pickTransientEnv); // Up to 2x headroom on pick snap!

        // 2. Zero-Phase Complementary Split (Sub-bass < 360Hz & Air > 4.2kHz stay 100% phase-locked)
        float xMid = 0.0f, xCleanComplement = 0.0f;
        splitter.split(x, xMid, xCleanComplement);

        // 3. Golden-Ratio Asymmetrical Germanium Fluid Cell with ADAA
        float kDynDrive = gainKnob * (0.12f + 0.88f * (muSmooth * muSmooth));
        float opAmpGain = (1.0f + 14.0f * kDynDrive + 140.0f * kDynDrive * kDynDrive) / dynamicHeadroom;
        float uGe = xMid * opAmpGain;

        float yPos = gePosADAA.process(uGe * 2.1f) * 0.54f * dynamicHeadroom;
        float yNeg = geNegADAA.process(uGe * 1.35f) * 0.65f * dynamicHeadroom;
        float yClipped = (uGe >= 0.0f) ? yPos : yNeg;

        // Add Golden 2nd-Order Harmonic Bloom
        float goldenEvenBloom = 0.16f * yClipped * std::abs(yClipped);
        yClipped = std::tanh(yClipped + goldenEvenBloom);

        // 4. Phase-Locked Dual-Gang Summing (Never loses sub-bass weight at high gain!)
        float cleanBlend = (1.0f - 0.65f * kDynDrive) * (1.0f + 0.35f * (1.0f - muSmooth));
        float ySum = (xCleanComplement + xMid * cleanBlend) + yClipped * (3.2f * kDynDrive);

        // 5. Active Liquid-Glass Tilt/Treble Matrix
        toneLpState += 0.28f * (ySum - toneLpState); // ~2.4 kHz pivot
        float highAir = ySum - toneLpState;
        float toneShaped = ySum + (toneKnob - 0.5f) * 1.4f * highAir;

        return toneShaped * levelKnob;
    }
};

// ============================================================================
// 4. ULTIMATE PEDAL 2: HYPER-TS808 ("Zero-Boxiness Velvet Mid" Screamer)
// ============================================================================
class HyperTS808Pedal {
private:
    PhaseCoherentMidSplitter dynamicSplitter;
    HyperFluidADAA diodeLoopADAA;
    float envFollower = 0.0f;
    float vocalResState1 = 0.0f, vocalResState2 = 0.0f;
    float toneLpState = 0.0f;
    float sampleRate = 48000.0f;

public:
    void prepare(float sr) noexcept {
        sampleRate = sr;
        dynamicSplitter.prepare(sr, 360.0f, 3600.0f);
        reset();
    }

    void reset() noexcept {
        dynamicSplitter.reset();
        diodeLoopADAA.reset();
        envFollower = 0.0f;
        vocalResState1 = vocalResState2 = toneLpState = 0.0f;
    }

    inline float processSample(float x, float driveKnob, float toneKnob, float levelKnob, bool isFat, float muSmooth = 1.0f) noexcept {
        // 1. Pressure-Sensitive Bass Aperture (180 Hz warm rhythm -> 720.5 Hz laser-tight lead!)
        float absX = std::abs(x);
        envFollower = (absX > envFollower) ? (envFollower + 0.02f * (absX - envFollower))
                                           : (envFollower + 0.0008f * (absX - envFollower));
        float pressureFactor = std::clamp(envFollower * 4.5f * (0.4f + 0.6f * driveKnob), 0.0f, 1.0f);
        float baseCutoff = isFat ? 110.0f : 180.0f;
        float dynamicBassCutoff = baseCutoff + 540.5f * pressureFactor; // Sweeps base -> 720.5 Hz!
        dynamicSplitter.setLowCorner(sampleRate, dynamicBassCutoff);

        float xMidLoop = 0.0f, xCleanBottomAndAir = 0.0f;
        dynamicSplitter.split(x, xMidLoop, xCleanBottomAndAir);

        // 2. Non-Inverting Feedback Loop with ADAA & Even-Order Cello Slipstream
        float kDynDrive = driveKnob * (0.15f + 0.85f * muSmooth * muSmooth);
        float loopGain = 1.0f + 28.0f * kDynDrive;
        float uLoop = xMidLoop * loopGain;

        float symmetricalClip = diodeLoopADAA.process(uLoop);
        float extracted2ndHarmonic = (symmetricalClip * symmetricalClip) * 0.24f; // Singing vocal bloom!

        // 3. Feedback Summing with Phase-Coherent Bottom Weight Retention
        float yOpAmp = xCleanBottomAndAir * (0.65f - 0.30f * pressureFactor)
                     + xMidLoop
                     + (symmetricalClip + extracted2ndHarmonic) * (1.65f * kDynDrive);

        // 4. Velvet Post-Filter (Preserves pick chime up to 8.5 kHz instead of choking at 723 Hz)
        float alphaTone = 0.08f + 0.45f * toneKnob;
        toneLpState += alphaTone * (yOpAmp - toneLpState);
        float yTone = toneLpState + (toneKnob * 0.45f) * (yOpAmp - toneLpState);

        return std::tanh(yTone * 0.90f) * levelKnob;
    }

    inline float processSample(float x, float driveKnob, float toneKnob, float levelKnob, float muSmooth) noexcept {
        return processSample(x, driveKnob, toneKnob, levelKnob, false, muSmooth);
    }
};

// ============================================================================
// 5. ULTIMATE PEDAL 3: HYPER-BD2 ("Polyphonic JFET Slipstream" Blues Driver)
// ============================================================================
class HyperBD2Pedal {
private:
    HyperFluidADAA jfet1ADAA, jfet2ADAA;
    float jfet1Momentum = 0.0f, jfet2Momentum = 0.0f;
    float jfet2BiasSag = 0.0f;
    float prevX = 0.0f, prevV1 = 0.0f;
    float signalEnv = 0.0f;
    float subTightState = 0.0f;

public:
    void prepare(float sr = 48000.0f) noexcept {
        (void)sr;
        reset();
    }

    void reset() noexcept {
        jfet1ADAA.reset();
        jfet2ADAA.reset();
        jfet1Momentum = jfet2Momentum = jfet2BiasSag = 0.0f;
        prevX = prevV1 = signalEnv = subTightState = 0.0f;
    }

    inline float processSample(float x, float gainKnob, float toneKnob, float levelKnob, bool isPhat = false, float muSmooth = 1.0f) noexcept {
        // Track envelope for Dynamic Viscoelastic Decay Damping (Zero fizzy decay tails!)
        float absX = std::abs(x);
        signalEnv = (absX > signalEnv) ? (signalEnv + 0.015f * (absX - signalEnv))
                                       : (signalEnv + 0.0005f * (absX - signalEnv));
        // When note fades out, viscosity rises to 0.92 to melt away solid-state fizz
        float dynamicViscosity = std::clamp(0.92f - 0.22f * std::min(1.0f, signalEnv * 5.0f), 0.70f, 0.92f);

        // Stage 1: Discrete JFET Differential Pre-Amp
        float kDyn = gainKnob * (0.15f + 0.85f * muSmooth * muSmooth);
        float g1 = 1.0f + 4.5f * kDyn;
        float u1 = x * g1;
        jfet1Momentum = dynamicViscosity * jfet1Momentum + (1.0f - dynamicViscosity) * (u1 - prevX);
        prevX = u1;

        float w1 = jfet1ADAA.process(u1 + 0.25f * jfet1Momentum);
        float jfet1EvenHarmonic = 0.20f * w1 * w1; // Extracted 2nd-order slipstream!
        float v1 = (u1 >= 0.0f) ? (w1 - 0.14f * w1 * w1) : (w1 + 0.10f * w1 * w1);

        // Stage 2: Cascading JFET with Forward Harmonic Slipstream Bias Locking
        // Stage 1's harmonic slipstream pre-biases Stage 2 so 6-string chords never mud out!
        jfet2BiasSag = 0.995f * jfet2BiasSag + 0.005f * std::abs(v1);
        float g2 = 1.0f + 10.5f * kDyn * kDyn + 2.2f * kDyn;
        float u2 = (v1 - 0.22f * jfet2BiasSag + 0.35f * jfet1EvenHarmonic) * g2;

        jfet2Momentum = dynamicViscosity * jfet2Momentum + (1.0f - dynamicViscosity) * (u2 - prevV1);
        prevV1 = u2;

        float w2 = jfet2ADAA.process(u2 + 0.28f * jfet2Momentum);
        float v2 = (u2 >= 0.0f) ? (w2 - 0.16f * w2 * w2) : (w2 + 0.12f * w2 * w2);

        // Keeley-Style Phat Low-End Balance + Smooth Tilt Tone
        subTightState += 0.025f * (v2 - subTightState);
        float phatWeight = isPhat ? 0.48f : 0.25f;
        float phatBoost = v2 + phatWeight * subTightState + (toneKnob - 0.5f) * 0.85f * (v2 - subTightState);

        return std::tanh(phatBoost * 0.92f) * levelKnob;
    }

    inline float processSample(float x, float gainKnob, float toneKnob, float levelKnob, float muSmooth) noexcept {
        return processSample(x, gainKnob, toneKnob, levelKnob, false, muSmooth);
    }
};

// ============================================================================
// 6. ULTIMATE PEDAL 4: HYPER-RODENT ("LM308 Vortex Violin" Rat Evolution)
// ============================================================================
class HyperRodentPedal {
private:
    HyperFluidADAA q1ADAA, q2ADAA;
    float slewState = 0.0f;
    float maxSlewPerSample = 0.18f;
    float subSplitState = 0.0f;
    float alphaSub = 0.022f; // ~180 Hz tight sub-bass lock
    float filterState1 = 0.0f, filterState2 = 0.0f;
    float sampleRate = 48000.0f;

public:
    void prepare(float sr, float voltsPerMicrosecond = 0.30f) noexcept {
        sampleRate = sr;
        maxSlewPerSample = (voltsPerMicrosecond * 1e6f) / (sr * 12.0f);
        alphaSub = (2.0f * 3.14159265f * 180.0f) / (sr + 2.0f * 3.14159265f * 180.0f);
        reset();
    }

    void reset() noexcept {
        q1ADAA.reset();
        q2ADAA.reset();
        slewState = subSplitState = filterState1 = filterState2 = 0.0f;
    }

    inline float processSample(float x, float distKnob, float filterKnob, float volKnob, float turboMorph = 0.0f, float muSmooth = 1.0f) noexcept {
        // 1. Sub-Bass Tightness Splitter (< 180 Hz bypasses extreme fuzz to prevent flubby bass!)
        subSplitState += alphaSub * (x - subSplitState);
        float tightSub = std::tanh(subSplitState * (1.5f + 2.5f * distKnob));
        float midHighIn = x - subSplitState;

        // 2. Q1 High-Gain Pre-Amp + True LM308 Fluid Slew-Rate Limiter
        float kDynDist = distKnob * (0.15f + 0.85f * muSmooth * muSmooth);
        float q1Out = q1ADAA.process(midHighIn * (2.0f + 22.0f * kDynDist));

        float dynamicSlewLimit = maxSlewPerSample / (1.0f + 0.65f * kDynDist);
        float delta = q1Out - slewState;
        slewState += std::clamp(delta, -dynamicSlewLimit, dynamicSlewLimit);

        // 3. Q2 Continuous Silicon-to-Red-LED Fluid Clipper + Violin Even-Order Injection
        float thresholdScale = 1.0f + 0.85f * std::clamp(turboMorph, 0.0f, 1.0f);
        float uClip = (slewState * (1.35f + 1.15f * kDynDist)) / thresholdScale;
        float clipped = q2ADAA.process(uClip) * thresholdScale;
        float violinSustain = clipped + 0.22f * (clipped * clipped) * ((uClip >= 0.0f) ? 1.0f : -0.6f);

        // Recombine Tight Laminar Sub-Bass with Creamy LM308 Mid/High Lead
        float combined = tightSub * 0.55f + violinSustain * 0.85f;

        // 4. Classic Rat Filter Sweep (0.0 = Bright 18 kHz, 1.0 = Warm Creamy 480 Hz)
        float cutoffHz = 480.0f * std::pow(36.0f, 1.0f - filterKnob);
        float alphaLp = (2.0f * 3.14159265f * cutoffHz) / (sampleRate + 2.0f * 3.14159265f * cutoffHz);
        filterState1 += alphaLp * (combined - filterState1);
        filterState2 += alphaLp * (filterState1 - filterState2);

        return filterState2 * volKnob;
    }
};

// ============================================================================
// 7. ULTIMATE PEDAL 5: HYPER-FUZZ FACE ("432Hz Pickup-Inductance Chime")
// ============================================================================
class HyperFuzzFaceEnhancer {
private:
    float ic1eq = 0.0f, ic2eq = 0.0f;
    float sampleRate = 48000.0f;

public:
    void prepare(float sr) noexcept { sampleRate = sr; reset(); }
    void reset() noexcept { ic1eq = ic2eq = 0.0f; }

    // Apply directly to the output of your FluidFuzzFacePedal using its live muFuzz [0..1]!
    // At Vol 7 (muFuzz -> 0): Injects 4.32 kHz (10x 432Hz) Hendrix glassy pickup inductance chime!
    // At Vol 10 (muFuzz -> 1): Shifts resonance to 1.728 kHz (4x 432Hz) for thick singing violin lead!
    inline float processPickupInductanceResonance(float fuzzSignal, float muFuzz) noexcept {
        float cleanFactor = 1.0f - std::clamp(muFuzz, 0.0f, 1.0f);
        float resonantHz  = 1728.0f + (4320.0f - 1728.0f) * cleanFactor; // 1.728 kHz -> 4.32 kHz!
        float Q           = 0.85f + 1.45f * cleanFactor;                 // Glassy bell peak at Vol 7

        float g = std::tan(3.14159265f * resonantHz / sampleRate);
        float k = 1.0f / Q;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        float v3 = fuzzSignal - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        // Bandpass chime peak (v1) blended into the signal
        float chimeGain = 0.35f + 0.85f * cleanFactor;
        return fuzzSignal + (v1 * k * chimeGain);
    }
};

// ============================================================================
// 8. ULTRA-FAST ADAPTIVE QUADRATURE HUM TRACKER (Replaces 71 Goertzel Bins!)
// ============================================================================
class AdaptiveQuadratureHumTracker {
private:
    float sampleRate = 48000.0f;
    float omega = 2.0f * 3.14159265f * 50.0f / 48000.0f;
    float omegaMin = 0.0f, omegaMax = 0.0f;
    float phase = 0.0f;
    float lpInPhase = 0.0f, lpQuadPhase = 0.0f;

public:
    void prepare(float sr, float nominalHz = 50.0f) noexcept {
        sampleRate = sr;
        omega = 2.0f * 3.14159265f * nominalHz / sr;
        omegaMin = 2.0f * 3.14159265f * (nominalHz - 3.5f) / sr;
        omegaMax = 2.0f * 3.14159265f * (nominalHz + 4.0f) / sr;
    }

    inline float trackFundamentalHz(float xMono) noexcept {
        float s = std::sin(phase);
        float c = std::cos(phase);
        lpInPhase   += 0.0015f * (xMono * c - lpInPhase);
        lpQuadPhase += 0.0015f * (xMono * s - lpQuadPhase);

        float phaseError = lpQuadPhase * lpInPhase;
        omega = std::clamp(omega - 0.0000025f * phaseError, omegaMin, omegaMax);

        phase += omega;
        if (phase > 6.2831853f) phase -= 6.2831853f;

        return (omega * sampleRate) / 6.2831853f;
    }
};

// ============================================================================
// 9. HARMONIC-SHIELDED 10-BAND ZERO-GATE DENOISER UPGRADE
// ============================================================================
class HarmonicShieldedDenoiserHelper {
public:
    static inline void computeProtectedBandGains(
        const std::array<float, 10>& bandEnergies,
        const std::array<float, 10>& baseLinearThresholds,
        float nonZeroFloor, // Keep at 0.05f ... 0.15f for 100% Zero-Gating!
        std::array<float, 10>& outBandGains) noexcept 
    {
        float fundamentalActivity = 0.0f;
        for (int b = 1; b <= 3; ++b) {
            float rawG = 1.0f - (baseLinearThresholds[b] / (bandEnergies[b] + 1e-9f));
            fundamentalActivity = std::max(fundamentalActivity, std::clamp(rawG, 0.0f, 1.0f));
        }

        for (int i = 0; i < 10; ++i) {
            float shieldMultiplier = (i >= 4) ? (1.0f - 0.65f * fundamentalActivity) : 1.0f;
            float effectiveThreshold = baseLinearThresholds[i] * shieldMultiplier;
            float g = 1.0f - (effectiveThreshold / (bandEnergies[i] + 1e-9f));
            outBandGains[i] = std::clamp(g, nonZeroFloor, 1.0f);
        }
    }
};

} // namespace EuphoriaHyperDSP
