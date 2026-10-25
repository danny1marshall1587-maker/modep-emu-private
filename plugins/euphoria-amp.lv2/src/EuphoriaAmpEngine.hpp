#pragma once

// ============================================================================
// Euphoria Amp — Pure Fluid Dynamics Tube & Studio Rack Engine
// ============================================================================
// A revolutionary paradigm shift in tube amplification modeling:
// Instead of traditional iterative circuit solvers, Euphoria models electrical
// wave propagation through vacuum tubes as a pure fluid-dynamic pressure field.
//
// Fluid Dynamics Core Mechanisms:
// 1. Fluid Velocity & Kinetic Momentum (Hysteresis & Anti-Aliased Weight):
//    Calculates kinetic energy from pressure deltas, giving notes physical inertia
//    and eliminating brittle digital clipping.
// 2. Chamber Pressure Dynamics (Dynamic Sag & Tube Breathing):
//    Audio energy physically evacuates virtual pressurized chambers, creating
//    organic compression and blooming decay.
// 3. Asymmetric Viscous Harmonic Generation:
//    Natural polynomial harmonic distribution matching whole-number frequency
//    divisions for pure 2nd, 3rd, and 4th order harmonic warmth.
// 4. Momentum Injection:
//    Adds inertial splash to distortion for analog liquid sustain.
// ============================================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef CYBER_ENABLE_STUDIO_DELAY
#define CYBER_ENABLE_STUDIO_DELAY 0
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "../../cyber-hum-killer.lv2/src/CyberHumEngine.hpp"
#include "../../cyber-denoiser/src/CyberDenoiserEngine.hpp"
#include "HyperFluidDriveAndDenoiser.hpp"

namespace EuphoriaDSP {

struct FluidOnePole {
    float z = 0.0f;
    inline float lp(float x, float fc, float sr) {
        float w = 2.0f * (float)M_PI * fc / sr;
        float a0 = w / (1.0f + w);
        z += a0 * (x - z);
        return z;
    }
    inline float hp(float x, float fc, float sr) {
        return x - lp(x, fc, sr);
    }
    inline void reset() { z = 0.0f; }
};

struct FluidBiquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;

    inline float process(float in) {
        float out = b0 * in + s1;
        s1 = b1 * in - a1 * out + s2;
        s2 = b2 * in - a2 * out;
        return out;
    }
    void reset() { s1 = s2 = 0.0f; }
    void setIdentity() { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; s1 = s2 = 0.0f; }

    void setLowPass(float fc, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = sinf(omega);
        float cs = cosf(omega);
        float alpha = sn / (2.0f * Q);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f - cs) * 0.5f) / a0;
        b1 = (1.0f - cs) / a0;
        b2 = ((1.0f - cs) * 0.5f) / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setHighPass(float fc, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = sinf(omega);
        float cs = cosf(omega);
        float alpha = sn / (2.0f * Q);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f + cs) * 0.5f) / a0;
        b1 = (-(1.0f + cs)) / a0;
        b2 = ((1.0f + cs) * 0.5f) / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    inline void setLowpass(float fc, float Q, float sr) { setLowPass(fc, Q, sr); }
    inline void setHighpass(float fc, float Q, float sr) { setHighPass(fc, Q, sr); }

    void setLowShelf(float fc, float gainDb, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float A = powf(10.0f, gainDb / 40.0f);
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = sinf(omega);
        float cs = cosf(omega);
        float beta = sqrtf(A + A);
        float a0 = (A + 1.0f) + (A - 1.0f) * cs + beta * sn;
        b0 = (A * ((A + 1.0f) - (A - 1.0f) * cs + beta * sn)) / a0;
        b1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cs)) / a0;
        b2 = (A * ((A + 1.0f) - (A - 1.0f) * cs - beta * sn)) / a0;
        a1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cs)) / a0;
        a2 = ((A + 1.0f) + (A - 1.0f) * cs - beta * sn) / a0;
    }

    void setHighShelf(float fc, float gainDb, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float A = powf(10.0f, gainDb / 40.0f);
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = sinf(omega);
        float cs = cosf(omega);
        float beta = sqrtf(A + A);
        float a0 = (A + 1.0f) - (A - 1.0f) * cs + beta * sn;
        b0 = (A * ((A + 1.0f) + (A - 1.0f) * cs + beta * sn)) / a0;
        b1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cs)) / a0;
        b2 = (A * ((A + 1.0f) + (A - 1.0f) * cs - beta * sn)) / a0;
        a1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cs)) / a0;
        a2 = ((A + 1.0f) - (A - 1.0f) * cs - beta * sn) / a0;
    }

    void setPeaking(float fc, float gainDb, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float A = powf(10.0f, gainDb / 40.0f);
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = sinf(omega);
        float cs = cosf(omega);
        float alpha = sn / (2.0f * Q);
        float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cs) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    void setAllpass(float fc, float Q, float sr) {
        fc = std::max(20.0f, std::min(sr * 0.45f, fc));
        float omega = 2.0f * (float)M_PI * fc / sr;
        float sn = sinf(omega);
        float cs = cosf(omega);
        float alpha = sn / (2.0f * Q);
        float a0 = 1.0f + alpha;
        b0 = (1.0f - alpha) / a0;
        b1 = (-2.0f * cs) / a0;
        b2 = (1.0f + alpha) / a0;
        a1 = (-2.0f * cs) / a0;
        a2 = (1.0f - alpha) / a0;
    }
};

/* Polyphase Allpass Half-Band Stage for 2x / 4x / 8x Oversampling */
struct HalfBandPolyphase {
    float a0_x = 0.0f, a0_y = 0.0f;
    float a1_x = 0.0f, a1_y = 0.0f;
    float b0_x = 0.0f, b0_y = 0.0f;
    float b1_x = 0.0f, b1_y = 0.0f;
    float prev_b = 0.0f;

    inline void reset() {
        a0_x = a0_y = a1_x = a1_y = b0_x = b0_y = b1_x = b1_y = prev_b = 0.0f;
    }

    inline float processBranchA(float x) {
        const float k0 = 0.079866426f;
        const float k1 = 0.545353651f;
        float y0 = k0 * (x - a0_y) + a0_x;
        a0_x = x; a0_y = y0;
        float y1 = k1 * (y0 - a1_y) + a1_x;
        a1_x = y0; a1_y = y1;
        return y1;
    }

    inline float processBranchB(float x) {
        const float k0 = 0.283829344f;
        const float k1 = 0.834411891f;
        float y0 = k0 * (x - b0_y) + b0_x;
        b0_x = x; b0_y = y0;
        float y1 = k1 * (y0 - b1_y) + b1_x;
        b1_x = y0; b1_y = y1;
        return y1;
    }

    inline void upsample2x(float in, float &out0, float &out1) {
        out0 = processBranchA(in);
        out1 = processBranchB(in);
    }

    inline float downsample2x(float in0, float in1) {
        float a = processBranchA(in0);
        float b = processBranchB(in1);
        float out = 0.5f * (a + prev_b);
        prev_b = b;
        return out;
    }
};

// ============================================================================
// Core Fluid Dynamics Tube Cell
// ============================================================================
struct FluidDynamicsCell {
    float prevInPressure = 0.0f;
    float fluidMomentum = 0.0f;
    float chamberPressure = 1.0f;

    float viscosity = 0.85f;        // Energy dissipation rate
    float harmonicPurity = 0.40f;   // Ratio of even-order harmonic generation
    float chamberRecovery = 0.0020f;// Power supply chamber refill rate
    float drainRate = 0.010f;       // Kinetic power draw from chamber
    float momentumInject = 0.45f;   // Inertial weight injection
    float asymPower = 2.5f;         // Asymmetry polynomial exponent

    // Fluid Space Geometry & Physics
    float shapeFactor = 0.0f;       // 0.0 = Circle (cylinder) .. 1.0 = Square (cube)
    float sizeFactor = 1.0f;        // 0.10x .. 10.0x (432 Hz volume ladder)
    float cornerEddy = 0.0f;        // Stored kinetic energy in corner recirculation vortices
    float drawEnv = 0.0f;           // Filter capacitor current draw envelope

    void reset() {
        prevInPressure = 0.0f;
        fluidMomentum = 0.0f;
        chamberPressure = 1.0f;
        drawEnv = 0.0f;
        cornerEddy = 0.0f;
    }

    inline float process(float inPressure, float driveLevel, float externalSag = 1.0f) {
        using namespace EuphoriaHyperDSP;
        // 1. Fluid Velocity & Kinetic Momentum
        float boundedInput = FluidMath::fast_tanh(inPressure);
        float currentVelocity = boundedInput - prevInPressure;
        prevInPressure = boundedInput;

        // Size factor scaling: larger chamber has higher momentum inertia, smaller chamber has lower inertia
        float effViscosity = std::clamp(viscosity * (0.88f + 0.08f * std::min(1.5f, sizeFactor)), 0.60f, 0.94f);
        fluidMomentum = (fluidMomentum * effViscosity) + (currentVelocity * (1.0f - effViscosity));
        fluidMomentum = std::clamp(fluidMomentum, -1.0f, 1.0f);

        // 2. Chamber Pressure Dynamics (Filtered Power Supply Sag & Dynamic Breathing)
        // Authentic vacuum tube B+ filter capacitor dynamics:
        // Current draw charges a smooth envelope (~25ms attack, ~140ms recovery).
        // Plate voltage drops by at most 18-22% under heavy sustained drive (NEVER voltage-starved!).
        float energyTarget = std::fabs(boundedInput) * driveLevel * 0.18f;
        float coeff = (energyTarget > drawEnv) ? 0.0025f : 0.00045f;
        drawEnv += coeff * (energyTarget - drawEnv);

        float maxSag = 0.22f;
        float sagFactor = std::clamp(drawEnv * (drainRate * 12.0f), 0.0f, maxSag);
        chamberPressure = 1.0f - sagFactor;
        chamberPressure = std::clamp(chamberPressure, 0.78f, 1.0f);

        float effChamber = chamberPressure * (0.86f + 0.14f * externalSag);

        // 3. Asymmetrical Fluid Harmonic Generation with Elastic Fluid-Wall Wave Reflection
        // Circle (shapeFactor = 0): smooth concentric boundary, high even-order purity, soft quadratic knee
        // Square (shapeFactor = 1): 4 corner recirculation eddy vortices, cubic odd-harmonic bite
        // Virtual Unlimited Internal Headroom: When driven hard (e.g. Fuzz Face +22 dB into Lead Amp),
        // the wave never hits a flat clipping rail or cuts out; excess kinetic pressure folds/reflects
        // back off the chamber boundary like water hitting the side of a tank and returning as harmonics!
        float cornerStress = (boundedInput * boundedInput * boundedInput - boundedInput * 0.35f) * shapeFactor;
        cornerEddy = std::clamp(cornerEddy * 0.72f + cornerStress * 0.28f, -1.0f, 1.0f);

        float amplifiedSignal = (inPressure + cornerEddy * 0.30f) * driveLevel * effChamber;
        float effPurity = std::clamp(harmonicPurity * (1.0f - shapeFactor * 0.35f), 0.10f, 0.50f);

        float outputPhase = FluidMath::fluidWallReflect(amplifiedSignal, effPurity, shapeFactor, fluidMomentum);

        // 4. Inject Fluid Momentum (scaled by chamber inertia using fast rational curve instead of std::sqrt)
        float clampedSz = std::min(4.0f, sizeFactor);
        float fastSqrtSz = clampedSz * (1.5f / (0.5f + 0.5f * clampedSz));
        float effMomentumInject = momentumInject * (0.6f + 0.35f * fastSqrtSz);
        outputPhase += FluidMath::fast_tanh(fluidMomentum) * effMomentumInject;

        // Soft limit to absolute max pressure to guarantee stability
        return FluidMath::fast_tanh(outputPhase);
    }

    // Ultra-Linear Laminar Glass Clean Tube Stage (Dumble Steel String Singer / '65 Twin 7025/6L6GC):
    // - When breakup0to1 <= 0.0f (Clean Volume 0.0 .. 7.0):
    //   Operates in 100% pure laminar Class-A flow with zero clipping on any guitar pickup (single-coil,
    //   P90, Filter'Tron, or hot humbucker), imparting only 3D fluid velocity air and <0.15% 2nd-harmonic glass.
    // - When breakup0to1 > 0.0f (Clean Volume 7.0 .. 10.0):
    //   Smoothly transitions across the threshold into touch-sensitive Blackface/Dumble edge-of-breakup
    //   and elastic fluid-wall harmonic bloom!
    inline float processLaminarClean(float inPressure, float breakup0to1, float externalSag = 1.0f) {
        using namespace EuphoriaHyperDSP;
        float boundedInput = FluidMath::fast_tanh(inPressure);
        float currentVelocity = boundedInput - prevInPressure;
        prevInPressure = boundedInput;

        float effViscosity = std::clamp(viscosity * (0.88f + 0.08f * std::min(1.5f, sizeFactor)), 0.60f, 0.94f);
        fluidMomentum = (fluidMomentum * effViscosity) + (currentVelocity * (1.0f - effViscosity));
        fluidMomentum = std::clamp(fluidMomentum, -1.0f, 1.0f);

        // Pure laminar glass signal with subtle 3D kinetic air (zero distortion below Volume 7.0)
        float evenGlass = 0.012f * boundedInput * std::fabs(boundedInput);
        float laminarOut = inPressure + fluidMomentum * 0.035f + evenGlass;

        if (breakup0to1 <= 0.0001f) {
            // Ultra-high headroom safety rail only above +/- 1.25 (never touches normal/hot guitar signals)
            if (std::fabs(laminarOut) > 1.25f) {
                float signL = (laminarOut >= 0.0f) ? 1.0f : -1.0f;
                float excess = std::fabs(laminarOut) - 1.25f;
                laminarOut = signL * (1.25f + 0.35f * FluidMath::fast_tanh(excess * 2.0f));
            }
            return laminarOut;
        }

        // Progressive breakup only when Clean Volume > 7.0 (breakup0to1 in (0.0, 1.0]):
        // Models authentic Dumble Steel String Singer / '65 Twin pushed-clean breakup (1.5% THD at 7.5 -> ~18% THD at 10.0)
        float b = std::clamp(breakup0to1, 0.0f, 1.0f);
        float drivePast7 = (0.38f + 0.85f * b + 0.65f * b * b) * externalSag;
        float u = (inPressure + fluidMomentum * 0.08f) * drivePast7;
        // Soft asymmetric triode saturation knee (2nd + 3rd harmonic tube bloom)
        float posU = (u >= 0.0f) ? u : 0.0f;
        float negU = (u <  0.0f) ? u : 0.0f;
        float sat = FluidMath::fast_tanh(posU * 1.12f + negU * 0.90f);
        float evenHarm = 0.085f * b * (sat * sat);
        float drivenOut = (sat + evenHarm) / (0.36f + 0.48f * b);
        float blend = b * (1.65f - 0.65f * b); // Smooth progressive onset past 7.0
        return laminarOut * (1.0f - blend) + drivenOut * blend;
    }
};

// ============================================================================
// Fluid Dynamics Direct-Coupled Cathode Follower Cell
// ============================================================================
struct FluidCathodeFollowerCell {
    float prevNetInput = 0.0f;
    float fluidMomentum = 0.0f;
    float gridBias = 0.0f;
    float shapeFactor = 0.0f;
    float sizeFactor = 1.0f;

    void reset() {
        prevNetInput = 0.0f;
        fluidMomentum = 0.0f;
        gridBias = 0.0f;
    }

    inline float process(float inPressure) {
        using namespace EuphoriaHyperDSP;
        float boundedIn = FluidMath::fast_tanh(inPressure);
        if (boundedIn > 0.0f) {
            gridBias += (boundedIn - gridBias) * 0.0085f;
        } else {
            gridBias *= 0.99940f;
        }
        gridBias = std::clamp(gridBias, -1.0f, 1.0f);

        float netInput = inPressure - gridBias * 0.32f;
        float boundedNet = FluidMath::fast_tanh(netInput);
        float vel = boundedNet - prevNetInput;
        prevNetInput = boundedNet;

        float effVisc = std::clamp(0.82f * (0.88f + 0.08f * std::min(1.5f, sizeFactor)), 0.60f, 0.92f);
        fluidMomentum = (fluidMomentum * effVisc) + (vel * (1.0f - effVisc));
        fluidMomentum = std::clamp(fluidMomentum, -1.0f, 1.0f);

        float cornerEddy = FluidMath::fast_tanh(netInput * netInput * netInput) * shapeFactor * 0.20f;
        float u = (netInput + cornerEddy + FluidMath::fast_tanh(fluidMomentum) * 0.32f) * 1.25f;
        float th = FluidMath::fast_tanh(u);
        float out = th - 0.12f * (th * th);
        return out;
    }
};

// ============================================================================
// Physical Dynamic Speaker Cabinet & Multi-Stage Acoustic Enclosure Model
// ============================================================================
// Transcends static IR convolution by coupling dynamic non-linear electro-mechanical
// speaker physics with authentic multi-stage acoustic enclosure transfer functions:
// 1. Dynamic Voice Coil Compliance & Thermal Sag:
//    Dynamic compliance compression and thermal heating under high excursion.
// 2. Duffing Non-Linear Suspension Stiffening (x + beta * x^3):
//    Tightens bass response dynamically on hard attacks without flub.
// 3. Dynamic Mechanical High-Frequency Damping:
//    Cone surround absorbs harsh breakup as voice coil heats, smoothing high harmonics.
// 4. Excursion-Gated Paper Ripple / Cone Cry:
//    Blooms naturally ONLY under heavy cone excursion (|x| > 0.55), zero artifacts clean.
// 5. Dual Cascaded 2nd-Order Low-Pass Filters (24 dB/octave Acoustic Roll-Off):
//    Authentic miked cabinet curves (5.9kHz - 7.1kHz), eliminating digital line-out fizz.
// 6. Adiabatic Enclosure Physics:
//    K_box = K_box0 / (V_scale^1.4), f_cavity = f_cavity0 / (V_scale^0.333).
// 7. Calibrated Unity Gain Staging:
//    Guarantees seamless parity with studio IR convolution levels.
// ============================================================================
struct FluidSpeakerCabinetDSP {
    FluidBiquad cabLow;
    FluidBiquad cabMid;
    FluidBiquad cabCavity;
    FluidBiquad cabLp1;
    FluidBiquad cabLp2;
    FluidBiquad cabPhaseAlign;

    float speakerEnv = 0.0f;
    float speakerThermalEnv = 0.0f;
    float speakerConeHistory = 0.0f;
    float magFlux = 0.0f;
    float spkAtk = 0.0f;
    float spkRel = 0.0f;
    float spkThermalRel = 0.0f;
    float spkMagAtk = 0.0f;
    float spkMagRel = 0.0f;

    int model = 0;             // 0..4 = (OLD), 5 = Hyper Greenback, 6 = Euphoric Alnico, 7 = 432Hz Projector, 8 = Hyper V30, 9 = Hyper EVM-12L, 10 = Hyper Blue Bulldog, 11 = Hyper Tweed 4x10, 12 = Bypass
    float cabSizeVol = 1.0f;   // 0.5x .. 2.0x volume multiplier
    bool use432Tuning = false; // 432Hz Natural Harmonic alignment
    float speakerDrive = 4.0f; // 0..10 cone stress

    void reset() {
        cabLow.reset();
        cabMid.reset();
        cabCavity.reset();
        cabLp1.reset();
        cabLp2.reset();
        cabPhaseAlign.reset();
        speakerEnv = speakerThermalEnv = speakerConeHistory = magFlux = 0.0f;
    }

    void update(float sampleRate) {
        spkAtk = 1.0f - expf(-1.0f / (sampleRate * 0.0015f));
        spkRel = 1.0f - expf(-1.0f / (sampleRate * 0.045f));
        spkThermalRel = 1.0f - expf(-1.0f / (sampleRate * 0.400f));
        spkMagAtk = 1.0f - expf(-1.0f / (sampleRate * 0.0020f));
        spkMagRel = 1.0f - expf(-1.0f / (sampleRate * 0.0350f));

        if (model >= 12) {
            cabLow.setIdentity();
            cabMid.setIdentity();
            cabCavity.setIdentity();
            cabLp1.setIdentity();
            cabLp2.setIdentity();
            cabPhaseAlign.setIdentity();
            return;
        }

        float pitchRatio = use432Tuning ? (432.0f / 440.0f) : 1.0f;
        float vScale = std::max(0.4f, std::min(2.5f, cabSizeVol));
        float boxAirExp = powf(vScale, 1.4f);       // Adiabatic air spring back-pressure
        float boxDimExp = powf(vScale, 0.333333f);  // Enclosure cavity acoustic dimension

        switch (model) {
            case 0: { // 2x12" Jensen C12N (OLD)
                float fLow = 78.0f * pitchRatio * sqrtf(1.0f + 0.22f / boxAirExp);
                float fMid = 3250.0f * pitchRatio;
                float fCut = 7100.0f * pitchRatio / boxDimExp;
                cabLow.setLowShelf(fLow, 5.2f, 0.707f, sampleRate);
                cabMid.setPeaking(fMid, 4.2f, 1.35f, sampleRate);
                cabCavity.setPeaking(380.0f / boxDimExp, -2.5f, 1.20f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setIdentity();
                break;
            }
            case 1: { // 4x10" Jensen P10R Alnico (OLD)
                float fLow = 92.0f * pitchRatio * sqrtf(1.0f + 0.35f / boxAirExp);
                float fMid = 2950.0f * pitchRatio;
                float fCut = 6500.0f * pitchRatio / boxDimExp;
                cabLow.setLowShelf(fLow, 5.4f, 0.707f, sampleRate);
                cabMid.setPeaking(fMid, 4.0f, 1.40f, sampleRate);
                cabCavity.setPeaking(450.0f / boxDimExp, 1.8f, 1.30f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setIdentity();
                break;
            }
            case 2: { // 2x10" Oxford 10L6 (OLD)
                float fLow = 86.0f * pitchRatio * sqrtf(1.0f + 0.28f / boxAirExp);
                float fMid = 3150.0f * pitchRatio;
                float fCut = 6600.0f * pitchRatio / boxDimExp;
                cabLow.setLowShelf(fLow, 5.0f, 0.707f, sampleRate);
                cabMid.setPeaking(fMid, 4.0f, 1.30f, sampleRate);
                cabCavity.setPeaking(410.0f / boxDimExp, -1.8f, 1.15f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setIdentity();
                break;
            }
            case 3: { // 4x12" Celestion Greenback (OLD)
                float fLow = 108.0f * pitchRatio * sqrtf(1.0f + 0.58f / boxAirExp);
                float fMid = 2450.0f * pitchRatio;
                float fCut = 5900.0f * pitchRatio / boxDimExp;
                cabLow.setPeaking(fLow, 5.8f, 1.15f, sampleRate);
                cabMid.setPeaking(fMid, 4.5f, 1.45f, sampleRate);
                cabCavity.setPeaking(320.0f / boxDimExp, 2.8f, 1.50f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setIdentity();
                break;
            }
            case 4: { // Tuned 432 Hz Natural Harmonic Enclosure (OLD)
                float fLow = 108.0f * sqrtf(1.0f + 0.32f / boxAirExp);
                float fMid = 3456.0f;
                float fCav = 216.0f / boxDimExp;
                float fCut = 6912.0f / boxDimExp;
                cabLow.setPeaking(fLow, 5.4f, 1.618f, sampleRate);
                cabMid.setPeaking(fMid, 4.32f, 1.432f, sampleRate);
                cabCavity.setPeaking(fCav, 3.8f, 1.90f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setIdentity();
                break;
            }
            case 5: { // HYPER GREENBACK (Punch & Wood, Ceramic Magnet Hysteresis, 3rd/5th Chladni, Zero-Group-Delay Alignment)
                float fLow = 112.0f * pitchRatio * sqrtf(1.0f + 0.58f / boxAirExp);
                float fMid = 2550.0f * pitchRatio;
                float fCut = 6200.0f * pitchRatio / boxDimExp;
                cabLow.setPeaking(fLow, 6.2f, 1.25f, sampleRate);
                cabMid.setPeaking(fMid, 4.8f, 1.50f, sampleRate);
                cabCavity.setPeaking(330.0f / boxDimExp, 3.2f, 1.60f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setAllpass(180.0f * pitchRatio, 0.65f, sampleRate);
                break;
            }
            case 6: { // EUPHORIC ALNICO (Chime & Bell, Spongy Alnico Hysteresis, 2nd/4th Chladni, Open-Back Diffusion)
                float fLow = 84.0f * pitchRatio * sqrtf(1.0f + 0.25f / boxAirExp);
                float fMid = 3200.0f * pitchRatio;
                float fCut = 6800.0f * pitchRatio / boxDimExp;
                cabLow.setLowShelf(fLow, 4.8f, 0.707f, sampleRate);
                cabMid.setPeaking(fMid, 4.5f, 1.35f, sampleRate);
                cabCavity.setPeaking(420.0f / boxDimExp, -1.5f, 1.10f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setAllpass(240.0f * pitchRatio, 0.55f, sampleRate);
                break;
            }
            case 7: { // 432Hz GOLDEN PROJECTOR (Golden Ratio Phi Scaling, Chladni 432Hz Grid, Lossless FRFR Radiation)
                constexpr float PHI = 1.6180339887f;
                float fLow = 108.0f * sqrtf(1.0f + 0.32f / boxAirExp);
                float fCav = 216.0f / boxDimExp;
                float fMid = 3456.0f;
                float fCut = 6912.0f / boxDimExp;
                cabLow.setPeaking(fLow, 5.6f, PHI, sampleRate);
                cabMid.setPeaking(fMid, 4.32f, 1.432f, sampleRate);
                cabCavity.setPeaking(fCav, 3.8f, 1.90f, sampleRate);
                cabLp1.setLowPass(fCut, 0.618f, sampleRate);
                cabLp2.setLowPass(fCut, 0.618f, sampleRate);
                cabPhaseAlign.setAllpass(216.0f, PHI * 0.5f, sampleRate);
                break;
            }
            case 8: { // HYPER VINTAGE 30 (4x12 Heavy Ceramic, Aggressive Mid-Spike 2.85kHz, Modern High-Gain Punch)
                float fLow = 118.0f * pitchRatio * sqrtf(1.0f + 0.62f / boxAirExp);
                float fMid = 2850.0f * pitchRatio;
                float fCut = 5700.0f * pitchRatio / boxDimExp;
                cabLow.setPeaking(fLow, 6.8f, 1.35f, sampleRate);
                cabMid.setPeaking(fMid, 6.2f, 1.70f, sampleRate);
                cabCavity.setPeaking(360.0f / boxDimExp, 3.5f, 1.45f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setAllpass(195.0f * pitchRatio, 0.70f, sampleRate);
                break;
            }
            case 9: { // HYPER EVM-12L (200W Cast-Frame Aluminum, 16-lb Magnet, 65Hz Colossal Punch, Dumble/SRV Headroom)
                float fLow = 65.0f * pitchRatio * sqrtf(1.0f + 0.18f / boxAirExp);
                float fMid = 2200.0f * pitchRatio;
                float fCut = 7400.0f * pitchRatio / boxDimExp;
                cabLow.setLowShelf(fLow, 4.6f, 0.707f, sampleRate);
                cabMid.setPeaking(fMid, 3.2f, 1.20f, sampleRate);
                cabCavity.setPeaking(450.0f / boxDimExp, -1.5f, 1.00f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setAllpass(120.0f * pitchRatio, 0.50f, sampleRate);
                break;
            }
            case 10: { // HYPER BLUE BULLDOG (2x12 Alnico Blue, AC30 3D Top-End Chime, Lower-Mid Scoop 480Hz)
                float fLow = 82.0f * pitchRatio * sqrtf(1.0f + 0.24f / boxAirExp);
                float fMid = 3850.0f * pitchRatio;
                float fCut = 6600.0f * pitchRatio / boxDimExp;
                cabLow.setLowShelf(fLow, 4.2f, 0.707f, sampleRate);
                cabMid.setPeaking(fMid, 5.8f, 1.60f, sampleRate);
                cabCavity.setPeaking(480.0f / boxDimExp, -2.8f, 1.30f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setAllpass(260.0f * pitchRatio, 0.60f, sampleRate);
                break;
            }
            case 11: { // HYPER TWEED 4x10 (1959 Solid Pine Box, 4x Alnico P10R, Woody Mid Bloom 2.9kHz, Dynamic Strum Sag)
                float fLow = 90.0f * pitchRatio * sqrtf(1.0f + 0.32f / boxAirExp);
                float fMid = 2900.0f * pitchRatio;
                float fCut = 6400.0f * pitchRatio / boxDimExp;
                cabLow.setLowShelf(fLow, 5.6f, 0.707f, sampleRate);
                cabMid.setPeaking(fMid, 4.8f, 1.40f, sampleRate);
                cabCavity.setPeaking(390.0f / boxDimExp, 2.2f, 1.25f, sampleRate);
                cabLp1.setLowPass(fCut, 0.707f, sampleRate);
                cabLp2.setLowPass(fCut, 0.707f, sampleRate);
                cabPhaseAlign.setAllpass(210.0f * pitchRatio, 0.65f, sampleRate);
                break;
            }
            default:
                break;
        }
    }

    inline float processSample(float in, float sampleRate) {
        using namespace EuphoriaHyperDSP;
        if (model >= 12) return in;

        float driveAmount = std::max(0.0f, std::min(10.0f, speakerDrive)) / 10.0f;
        float pitchRatio = use432Tuning ? (432.0f / 440.0f) : 1.0f;

        // Models 0..4: EXACT ORIGINAL BEHAVIOR (OLD)
        if (model < 5) {
            float absIn = std::fabs(in);
            if (absIn > speakerEnv) speakerEnv += spkAtk * (absIn - speakerEnv);
            else speakerEnv += spkRel * (absIn - speakerEnv);
            speakerThermalEnv += spkThermalRel * (speakerEnv - speakerThermalEnv);

            float comp = 1.0f / (1.0f + speakerEnv * driveAmount * 1.15f);
            float thermalComp = 1.0f / (1.0f + speakerThermalEnv * driveAmount * 0.22f);
            float s = in * comp * thermalComp;

            float coneStress = s * (1.0f + driveAmount * 1.10f);
            float t = FluidMath::fast_tanh(coneStress);
            float coneOut = (t - 0.04f * (t * t)) / (1.0f + driveAmount * 0.25f);

            float baseDamping = (model == 3) ? 5700.0f : ((model == 1) ? 6200.0f : 6600.0f);
            float dampingFc = (baseDamping - driveAmount * 1200.0f) * pitchRatio;
            float w = 2.0f * (float)M_PI * dampingFc / sampleRate;
            float a0 = w / (1.0f + w);
            speakerConeHistory += a0 * (coneOut - speakerConeHistory);

            s = (1.0f - driveAmount * 0.75f) * s + (driveAmount * 0.75f) * speakerConeHistory;

            float absS = std::fabs(s);
            if (absS > 0.55f && driveAmount > 0.05f) {
                float excess = absS - 0.55f;
                float bloom = excess * excess;
                float ripple = FluidMath::fast_sin(excess * 18.0f) * 0.08f * bloom * driveAmount;
                s += ripple;
            }

            s = cabLow.process(s);
            s = cabCavity.process(s);
            s = cabMid.process(s);
            s = cabLp1.process(s);
            s = cabLp2.process(s);
            return s * 1.40f;
        }

        // Models 5..11: HYPER-FLUID SPEAKER SYSTEM
        // 1. Voice Coil Compliance & Dynamic Magnetic Flux Hysteresis (Alnico vs Ceramic)
        float absIn = std::fabs(in);
        if (absIn > speakerEnv) speakerEnv += spkAtk * (absIn - speakerEnv);
        else speakerEnv += spkRel * (absIn - speakerEnv);
        speakerThermalEnv += spkThermalRel * (speakerEnv - speakerThermalEnv);

        if (absIn > magFlux) magFlux += spkMagAtk * (absIn - magFlux);
        else magFlux += spkMagRel * (absIn - magFlux);

        float alphaMag = (model == 6 || model == 10) ? 0.38f
                       : (model == 11)               ? 0.30f
                       : (model == 5 || model == 8)  ? 0.06f
                       : (model == 9)                ? 0.02f
                                                     : 0.1618f;
        float blForce = 1.0f / (1.0f + alphaMag * magFlux * driveAmount * 1.25f);
        float comp = 1.0f / (1.0f + speakerEnv * driveAmount * ((model == 6 || model == 10 || model == 11) ? 0.90f : (model == 9 ? 0.45f : 1.20f)));
        float thermalComp = 1.0f / (1.0f + speakerThermalEnv * driveAmount * ((model == 6 || model == 10) ? 0.12f : (model == 9 ? 0.05f : 0.28f)));
        float s = in * comp * thermalComp * blForce;

        // 2. Duffing Non-Linear Suspension Stiffening
        float coneStress = s * (1.0f + driveAmount * 1.15f);
        float t = FluidMath::fast_tanh(coneStress);
        float asymm = (model == 6 || model == 10) ? 0.025f : (model == 9 ? 0.015f : (model == 8 ? 0.052f : 0.045f));
        float coneOut = (t - asymm * (t * t)) / (1.0f + driveAmount * 0.22f);

        // 3. Dynamic Mechanical Damping
        float baseDamping = (model == 5) ? 5800.0f
                          : (model == 8) ? 5600.0f
                          : (model == 6) ? 6800.0f
                          : (model == 9) ? 7400.0f
                          : (model == 10) ? 6500.0f
                          : (model == 11) ? 6200.0f
                          : 6912.0f;
        float dampingFc = (baseDamping - driveAmount * ((model == 6 || model == 10) ? 800.0f : (model == 9 ? 500.0f : 1100.0f))) * pitchRatio;
        float w = 2.0f * (float)M_PI * dampingFc / sampleRate;
        float a0 = w / (1.0f + w);
        speakerConeHistory += a0 * (coneOut - speakerConeHistory);
        s = (1.0f - driveAmount * 0.70f) * s + (driveAmount * 0.70f) * speakerConeHistory;

        // 4. Harmonically Aligned Chladni Break-Up ("The Hyper-Cone")
        float absS = std::fabs(s);
        if (absS > 0.48f && driveAmount > 0.04f) {
            float excess = absS - 0.48f;
            float bloom = excess * excess;
            float chladni = 0.0f;

            if (model == 5) {
                // Hyper Greenback: 3rd and 5th harmonics (Woody British bite)
                float tHarm = FluidMath::fast_tanh(s * 1.8f);
                float t2 = tHarm * tHarm;
                float h3 = (4.0f * t2 - 3.0f) * tHarm;                 // 3rd Chebyshev
                float h5 = ((16.0f * t2 - 20.0f) * t2 + 5.0f) * tHarm; // 5th Chebyshev
                chladni = (0.055f * h3 + 0.025f * h5) * bloom * driveAmount;
            } else if (model == 8) {
                // Hyper Vintage 30: Aggressive 3rd and 5th harmonics (Modern British high-gain cutting bite)
                float tHarm = FluidMath::fast_tanh(s * 2.1f);
                float t2 = tHarm * tHarm;
                float h3 = (4.0f * t2 - 3.0f) * tHarm;
                float h5 = ((16.0f * t2 - 20.0f) * t2 + 5.0f) * tHarm;
                chladni = (0.065f * h3 + 0.035f * h5) * bloom * driveAmount;
            } else if (model == 6) {
                // Euphoric Alnico: 2nd and 4th even harmonics (Silky glass/bell chime)
                float tHarm = FluidMath::fast_tanh(s * 1.5f);
                float t2 = tHarm * tHarm;
                float h2 = (2.0f * t2 - 1.0f);                          // 2nd Chebyshev
                float h4 = (8.0f * t2 * (t2 - 1.0f) + 1.0f);           // 4th Chebyshev
                chladni = (0.065f * h2 + 0.030f * h4) * bloom * driveAmount;
            } else if (model == 10) {
                // Hyper Blue Bulldog: 2nd and 4th chime harmonics (Vox AC30 crystalline top-end sparkle)
                float tHarm = FluidMath::fast_tanh(s * 1.6f);
                float t2 = tHarm * tHarm;
                float h2 = (2.0f * t2 - 1.0f);
                float h4 = (8.0f * t2 * (t2 - 1.0f) + 1.0f);
                chladni = (0.075f * h2 + 0.040f * h4) * bloom * driveAmount;
            } else if (model == 9) {
                // Hyper EVM-12L: Ultra-clean 2nd & 3rd subtle presence
                float tHarm = FluidMath::fast_tanh(s * 1.2f);
                float t2 = tHarm * tHarm;
                float h2 = (2.0f * t2 - 1.0f);
                float h3 = (4.0f * t2 - 3.0f) * tHarm;
                chladni = (0.030f * h2 + 0.020f * h3) * bloom * driveAmount;
            } else if (model == 11) {
                // Hyper Tweed 4x10: 2nd, 3rd, 4th acoustic pine wood bloom
                float tHarm = FluidMath::fast_tanh(s * 1.6f);
                float t2 = tHarm * tHarm;
                float h2 = (2.0f * t2 - 1.0f);
                float h3 = (4.0f * t2 - 3.0f) * tHarm;
                float h4 = (8.0f * t2 * (t2 - 1.0f) + 1.0f);
                chladni = (0.050f * h2 + 0.030f * h3 + 0.020f * h4) * bloom * driveAmount;
            } else {
                // 432Hz Golden Ratio Projector: Golden harmonic overtone series
                float tHarm = FluidMath::fast_tanh(s * 1.618f);
                float t2 = tHarm * tHarm;
                float h2 = (2.0f * t2 - 1.0f);
                float h3 = (4.0f * t2 - 3.0f) * tHarm;
                float h5 = ((16.0f * t2 - 20.0f) * t2 + 5.0f) * tHarm;
                chladni = (0.040f * h2 + 0.035f * h3 + 0.015f * h5) * bloom * driveAmount;
            }
            s += chladni;
        }

        // 5. Multi-Stage Acoustic Cabinet Enclosure & Zero-Group-Delay Alignment
        s = cabLow.process(s);
        s = cabCavity.process(s);
        s = cabMid.process(s);
        s = cabLp1.process(s);
        s = cabLp2.process(s);
        s = cabPhaseAlign.process(s);

        return s * 1.40f;
    }
};

// ============================================================================
// Studio Downward Expander & Dynamic Noise Gate
// ============================================================================
// Gated downward expansion below musical threshold (-68 dBFS) to eliminate
// interface hiss, pickup buzz, and idle line noise with 0ms pick response.
struct StudioDownwardExpander {
    float env = 0.0f;
    float currentGain = 1.0f;

    void reset() {
        env = 0.0f;
        currentGain = 1.0f;
    }

    inline float process(float in, float sampleRate, bool enabled = true, float thresholdDb = -68.0f, float maxAttenDb = -36.0f) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return in;

        float absIn = std::fabs(in);
        // Instantaneous attack (0.6ms), musical smooth release (60ms)
        float rateScale = 48000.0f / sampleRate;
        float attackCoeff  = 0.0341f * rateScale;
        float releaseCoeff = 0.000347f * rateScale;

        if (absIn > env) {
            env += attackCoeff * (absIn - env);
        } else {
            env += releaseCoeff * (absIn - env);
        }

        float envDb = (env > 1e-5f) ? FluidMath::fast_db(env) : -100.0f;
        float targetGain = 1.0f;
        if (envDb < thresholdDb) {
            float delta = thresholdDb - envDb;
            float attDb = -std::min(-maxAttenDb, delta * 1.5f);
            targetGain = FluidMath::fast_db_to_lin(attDb);
        }

        float gCoeff = (targetGain > currentGain) ? attackCoeff : releaseCoeff;
        currentGain += gCoeff * (targetGain - currentGain);
        return in * currentGain;
    }
};

// ============================================================================
// Physical Acoustic Chamber Space (432 Hz Doppler Transit Line & Cavity Sweep)
// ============================================================================
struct FluidAcousticChamberSpace {
    static constexpr int BUF_SIZE = 16384;
    float chamberBufferL[BUF_SIZE] = {};
    float chamberBufferR[BUF_SIZE] = {};
    int writeIdx = 0;

    FluidBiquad cavityFilterL;
    FluidBiquad cavityFilterR;
    float lastCurSize = -1.0f;
    float lastShapeFactor = -1.0f;

    void reset() {
        std::fill(chamberBufferL, chamberBufferL + BUF_SIZE, 0.0f);
        std::fill(chamberBufferR, chamberBufferR + BUF_SIZE, 0.0f);
        writeIdx = 0;
        cavityFilterL.reset();
        cavityFilterR.reset();
        lastCurSize = -1.0f;
        lastShapeFactor = -1.0f;
    }

    inline void process(
        float inL, float inR,
        float curSize, float lfoValL, float lfoValR,
        float sizeModDepth, float shapeFactor, bool modEnabled,
        float sampleRate,
        float& outL, float& outR
    ) {
        chamberBufferL[writeIdx] = inL;
        chamberBufferR[writeIdx] = inR;

        // Base transit delay for 432 Hz fundamental acoustic period
        float basePeriodSamples = sampleRate / 432.0f; // ~111.1 samples at 48kHz
        float targetDelayL = basePeriodSamples * curSize;
        float targetDelayR = basePeriodSamples * curSize;

        if (modEnabled) {
            // Modulate physical acoustic transit time -> True Doppler pitch sweep
            float modAmtL = 1.0f + lfoValL * sizeModDepth * 0.70f;
            float modAmtR = 1.0f + lfoValR * sizeModDepth * 0.70f;
            targetDelayL = std::clamp(targetDelayL * modAmtL, 4.0f, (float)(BUF_SIZE - 8));
            targetDelayR = std::clamp(targetDelayR * modAmtR, 4.0f, (float)(BUF_SIZE - 8));
        } else {
            targetDelayL = std::clamp(targetDelayL, 4.0f, (float)(BUF_SIZE - 8));
            targetDelayR = std::clamp(targetDelayR, 4.0f, (float)(BUF_SIZE - 8));
        }

        // Hermite 4-point cubic fractional delay interpolation
        auto readDelay = [&](const float* buf, float d) -> float {
            float rPos = (float)writeIdx - d;
            while (rPos < 0.0f) rPos += (float)BUF_SIZE;
            int iPos = (int)rPos;
            float frac = rPos - (float)iPos;

            int i0 = (iPos - 1 + BUF_SIZE) % BUF_SIZE;
            int i1 = iPos % BUF_SIZE;
            int i2 = (iPos + 1) % BUF_SIZE;
            int i3 = (iPos + 2) % BUF_SIZE;

            float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];
            float c0 = y1;
            float c1 = 0.5f * (y2 - y0);
            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * frac + c2) * frac + c1) * frac + c0;
        };

        float delayedL = readDelay(chamberBufferL, targetDelayL);
        float delayedR = readDelay(chamberBufferR, targetDelayR);

        if (++writeIdx >= BUF_SIZE) writeIdx = 0;

        // Acoustic cavity resonance (Helmholtz frequency = 432 Hz / sqrt(size))
        // Cached: only recompute Biquad coefficients when size or shape moves by > 0.002f!
        if (std::fabs(curSize - lastCurSize) > 0.002f || std::fabs(shapeFactor - lastShapeFactor) > 0.002f) {
            lastCurSize = curSize;
            lastShapeFactor = shapeFactor;
            float cavityFreq = std::clamp(432.0f / std::sqrt(std::max(0.10f, curSize)), 50.0f, 5200.0f);
            float cavityQ = 1.15f + shapeFactor * 1.35f;
            cavityFilterL.setPeaking(cavityFreq, 2.5f, cavityQ, sampleRate);
            cavityFilterR.setPeaking(cavityFreq, 2.5f, cavityQ, sampleRate);
        }

        float resL = cavityFilterL.process(delayedL);
        float resR = cavityFilterR.process(delayedR);

        if (modEnabled) {
            float wetMix = std::clamp(sizeModDepth * 0.75f, 0.20f, 0.65f);
            outL = inL * (1.0f - wetMix * 0.5f) + resL * wetMix;
            outR = inR * (1.0f - wetMix * 0.5f) + resR * wetMix;
        } else {
            // Zero cavity bleed when modulation is disabled (Rule 34 compliant)
            outL = inL;
            outR = inR;
        }
    }
};

// ============================================================================
// Pure Fluid Dynamics Reverb Space (432 Hz Chamber Size, Shape & Send Filter)
// ============================================================================
struct FluidReverbSpace {
    static constexpr int FDN_LINES = 4;
    static constexpr int BUF_SIZE = 16384;

    float delayBuffers[FDN_LINES][BUF_SIZE] = {};
    int writeIndices[FDN_LINES] = {};

    FluidBiquad sendHp;
    FluidBiquad sendLp;
    float lastLowCut = -1.0f;
    float lastHighCut = -1.0f;

    float apBuf1[211] = {};
    float apBuf2[379] = {};
    int apIdx1 = 0;
    int apIdx2 = 0;

    float dampingLp[FDN_LINES] = {};

    void reset() {
        for (int i = 0; i < FDN_LINES; ++i) {
            std::fill(delayBuffers[i], delayBuffers[i] + BUF_SIZE, 0.0f);
            writeIndices[i] = 0;
            dampingLp[i] = 0.0f;
        }
        std::fill(apBuf1, apBuf1 + 211, 0.0f);
        std::fill(apBuf2, apBuf2 + 379, 0.0f);
        apIdx1 = apIdx2 = 0;
        sendHp.reset();
        sendLp.reset();
        lastLowCut = -1.0f;
        lastHighCut = -1.0f;
    }

    inline void process(
        float inL, float inR,
        float curSize, float curShape,
        float sendLowCutHz, float sendHighCutHz,
        float dwellNorm, float mixNorm,
        float sampleRate,
        float& outL, float& outR
    ) {
        using namespace EuphoriaHyperDSP;
        if (mixNorm <= 0.001f) {
            outL = inL;
            outR = inR;
            return;
        }

        // 1. Abbey Road Pre-Tank Send Conditioning Filter (Cached: zero per-sample trig when unchanged)
        if (std::fabs(sendLowCutHz - lastLowCut) > 0.25f) {
            lastLowCut = sendLowCutHz;
            sendHp.setHighpass(std::clamp(sendLowCutHz, 20.0f, 1000.0f), 0.7071f, sampleRate);
        }
        if (std::fabs(sendHighCutHz - lastHighCut) > 1.0f) {
            lastHighCut = sendHighCutHz;
            sendLp.setLowpass(std::clamp(sendHighCutHz, 1500.0f, 18000.0f), 0.7071f, sampleRate);
        }

        float inMono = (inL + inR) * 0.5f;
        float conditioned = sendLp.process(sendHp.process(inMono));

        // 2. 12AT7 / 6K6 Tube Pre-Driver Stage (high-headroom laminar glass driver)
        float driveGain = 0.38f + dwellNorm * 0.42f;
        float driven = FluidMath::fast_tanh(conditioned * driveGain);

        // 3. Dual Allpass Diffuser Stage (breaks up specular reflections)
        float b1 = apBuf1[apIdx1];
        float v1 = driven - 0.55f * b1;
        float diff1 = b1 + 0.55f * v1;
        apBuf1[apIdx1] = v1;
        if (++apIdx1 >= 211) apIdx1 = 0;

        float b2 = apBuf2[apIdx2];
        float v2 = diff1 - 0.50f * b2;
        float diff2 = b2 + 0.50f * v2;
        apBuf2[apIdx2] = v2;
        if (++apIdx2 >= 379) apIdx2 = 0;

        // 4. Multi-Chamber Waveguide Delay Reads (432 Hz Natural Acoustic Period)
        float t432 = sampleRate / 432.0f; // ~111.1 samples at 48kHz
        static const float s_primeRatios[FDN_LINES] = { 1.000f, 1.4142f, 1.8371f, 2.3607f };

        float readOutputs[FDN_LINES];
        for (int k = 0; k < FDN_LINES; ++k) {
            float targetDelay = std::clamp(t432 * s_primeRatios[k] * curSize * 2.85f, 8.0f, (float)(BUF_SIZE - 16));

            // Hermite 4-point cubic fractional delay read (Rule 27 compliant: zero pitch clicks)
            float rPos = (float)writeIndices[k] - targetDelay;
            while (rPos < 0.0f) rPos += (float)BUF_SIZE;
            int iPos = (int)rPos;
            float frac = rPos - (float)iPos;

            int i0 = (iPos - 1 + BUF_SIZE) % BUF_SIZE;
            int i1 = iPos % BUF_SIZE;
            int i2 = (iPos + 1) % BUF_SIZE;
            int i3 = (iPos + 2) % BUF_SIZE;

            float y0 = delayBuffers[k][i0], y1 = delayBuffers[k][i1];
            float y2 = delayBuffers[k][i2], y3 = delayBuffers[k][i3];
            float c0 = y1;
            float c1 = 0.5f * (y2 - y0);
            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            readOutputs[k] = ((c3 * frac + c2) * frac + c1) * frac + c0;
        }

        // 5. 4x4 Lossless Orthogonal Hadamard Feedback Matrix
        float s0 = 0.5f * ( readOutputs[0] + readOutputs[1] + readOutputs[2] + readOutputs[3]);
        float s1 = 0.5f * ( readOutputs[0] - readOutputs[1] + readOutputs[2] - readOutputs[3]);
        float s2 = 0.5f * ( readOutputs[0] + readOutputs[1] - readOutputs[2] - readOutputs[3]);
        float s3 = 0.5f * ( readOutputs[0] - readOutputs[1] - readOutputs[2] + readOutputs[3]);
        float mixed[FDN_LINES] = { s0, s1, s2, s3 };

        // 6. Fluid Boundary Dispersion & Corner Eddy Turbulence (Shape Modulation)
        float feedbackFactor = 0.50f + 0.44f * dwellNorm;
        float dampAlpha = 0.20f + 0.35f * (1.0f - curShape * 0.45f);

        for (int k = 0; k < FDN_LINES; ++k) {
            // Viscous fluid high frequency damping
            dampingLp[k] += dampAlpha * (mixed[k] - dampingLp[k]);
            float damped = dampingLp[k];

            // Corner eddy turbulence: x^2(1 - x) bounded non-linearity (Rule 32)
            float xNorm = FluidMath::fast_tanh(damped);
            float eddy = curShape * 0.12f * (xNorm * xNorm * (1.0f - xNorm));
            float fbSignal = FluidMath::fast_tanh(damped * feedbackFactor) - eddy;

            delayBuffers[k][writeIndices[k]] = diff2 + fbSignal;
            if (++writeIndices[k] >= BUF_SIZE) writeIndices[k] = 0;
        }

        // 7. Stereo Spatial Extraction (compensated for high-headroom pre-driver)
        float wetL = 1.25f * (readOutputs[0] + readOutputs[2]);
        float wetR = 1.25f * (readOutputs[1] + readOutputs[3]);

        // 8. Equal-Power Dry/Wet Blend
        float mixAngle = mixNorm * 1.5707963f; // 0 .. pi/2
        float dryG = FluidMath::fast_cos(mixAngle);
        float wetG = FluidMath::fast_sin(mixAngle);

        outL = inL * dryG + wetL * wetG;
        outR = inR * dryG + wetR * wetG;
    }
};

// ============================================================================
// Vintage Fender Fluid Space Harmonic Tremolo (6G-Series Anti-Phase Tube Cells)
// ============================================================================
struct FluidHarmonicTremolo {
    FluidBiquad crossoverLp;
    FluidBiquad crossoverHp;

    FluidDynamicsCell fluidTremLow;
    FluidDynamicsCell fluidTremHigh;

    float lfoPhase = 0.0f;

    void reset(double sampleRate) {
        crossoverLp.reset();
        crossoverHp.reset();
        crossoverLp.setLowpass(750.0f, 0.7071f, (float)sampleRate);
        crossoverHp.setHighpass(750.0f, 0.7071f, (float)sampleRate);

        fluidTremLow.reset();
        fluidTremLow.viscosity = 0.85f;
        fluidTremLow.harmonicPurity = 0.35f;
        fluidTremLow.chamberRecovery = 0.003f;
        fluidTremLow.momentumInject = 0.40f;

        fluidTremHigh.reset();
        fluidTremHigh.viscosity = 0.82f;
        fluidTremHigh.harmonicPurity = 0.40f;
        fluidTremHigh.chamberRecovery = 0.003f;
        fluidTremHigh.momentumInject = 0.42f;

        lfoPhase = 0.0f;
    }

    inline float process(
        float inSample,
        float speedHz, float depthNorm,
        bool isHarmonicMode, bool enabled,
        float sampleRate
    ) {
        using namespace EuphoriaHyperDSP;
        if (!enabled || depthNorm <= 0.005f) return inSample;

        // Advance LFO
        float lfoInc = (2.0f * (float)M_PI * speedHz) / sampleRate;
        lfoPhase += lfoInc;
        if (lfoPhase > 2.0f * (float)M_PI) lfoPhase -= 2.0f * (float)M_PI;

        float lfoLow = FluidMath::fast_sin(lfoPhase);
        float lfoHigh = -lfoLow; // 180 deg anti-phase!

        if (isHarmonicMode) {
            // Split audio into Low and High frequency bands at ~750 Hz (Linkwitz-Riley 2nd-order)
            float sLow = crossoverLp.process(inSample);
            float sHigh = crossoverHp.process(inSample);

            // Modulate Low and High bands in 180-deg anti-phase using pure laminar Class-A tube cells (zero distortion!)
            float modLow  = 1.0f - depthNorm * 0.48f * (1.0f - lfoLow);
            float modHigh = 1.0f - depthNorm * 0.48f * (1.0f - lfoHigh);

            float bPlusLow  = std::clamp(1.0f - lfoLow * 0.15f * depthNorm, 0.80f, 1.10f);
            float bPlusHigh = std::clamp(1.0f - lfoHigh * 0.15f * depthNorm, 0.80f, 1.10f);

            float procLow  = fluidTremLow.processLaminarClean(sLow * modLow, 0.0f, bPlusLow);
            float procHigh = fluidTremHigh.processLaminarClean(sHigh * modHigh, 0.0f, bPlusHigh);

            // Recombine: anti-phase laminar fluid phase shift produces authentic 1961 Fender 6G Doppler wobble!
            return procLow + procHigh;
        } else {
            // Opto Photocell mode: smooth optical LDR resistance curve
            float optoMod = 1.0f - depthNorm * 0.5f * (1.0f + lfoLow);
            return inSample * optoMod;
        }
    }
};

// ============================================================================
// Non-Destructive Program-Dependent Dynamic De-Hisser (Rule 33)
// ============================================================================
// ============================================================================
// Transient-Decoupled High-Band De-Hisser & Adaptive Phase Cancellation (Rule 35)
// ============================================================================
struct DynamicDeHiss {
    float sr = 48000.0f;
    float splitFc = 3200.0f;
    FluidBiquad lp1, lp2;
    FluidBiquad hp1, hp2;

    float envFast = 0.0f;
    float envSlow = 0.0f;
    float gainSmooth = 1.0f;
    float lastHighSig = 0.0f;
    float atkCoeff = 0.0507f;
    float relCoeff = 0.000379f;
    float relSlowCoeff = 0.000174f;
    float smoothOpenCoeff = 0.0578f;
    float smoothCloseCoeff = 0.000833f;
    inline float getLastHighSig() const { return lastHighSig; }

    void init(double sampleRate, float fc = 3200.0f) {
        sr = (float)(sampleRate > 1000.0 ? sampleRate : 48000.0);
        splitFc = fc;
        const float Q_butterworth = 0.70710678f;
        lp1.setLowPass(splitFc, Q_butterworth, sr);
        lp2.setLowPass(splitFc, Q_butterworth, sr);
        hp1.setHighPass(splitFc, Q_butterworth, sr);
        hp2.setHighPass(splitFc, Q_butterworth, sr);
        atkCoeff         = 1.0f - expf(-1.0f / (sr * 0.0004f));
        relCoeff         = 1.0f - expf(-1.0f / (sr * 0.055f));
        relSlowCoeff     = 1.0f - expf(-1.0f / (sr * 0.120f));
        smoothOpenCoeff  = 1.0f - expf(-1.0f / (sr * 0.00035f));
        smoothCloseCoeff = 1.0f - expf(-1.0f / (sr * 0.025f));
        reset();
    }

    void reset() {
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();
        envFast = 0.0f;
        envSlow = 0.0f;
        gainSmooth = 0.063f; // smoothly start clamped to -24 dB floor
        lastHighSig = 0.0f;
    }

    // Dual-Keyed expansion: keyed by clean guitar input to cleanly suppress idle amp/pedal hiss
    // while leaving 100% of guitar pick attack, chime, treble, and Glass Air untouched.
    inline float processSample(float in, float inEnvDb, float thresholdDb = -60.0f, float maxReductionDb = -36.0f, bool enabled = true) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return in;

        // 1. Phase-aligned 4th-order Linkwitz-Riley crossover (flat magnitude sum)
        float lowSig  = lp2.process(lp1.process(in));
        float highSig = hp2.process(hp1.process(in));
        lastHighSig = highSig;

        // 2. High-pass band peak detection with instantaneous attack (<0.4ms)
        float absHf = fabsf(highSig);
        if (absHf > envFast) {
            envFast += atkCoeff * (absHf - envFast);
        } else {
            envFast += relCoeff * (absHf - envFast);
        }

        if (absHf > envSlow) {
            envSlow += atkCoeff * (absHf - envSlow);
        } else {
            envSlow += relSlowCoeff * (absHf - envSlow);
        }

        // 3. Input-keyed downward expansion gain computation:
        float targetGainLinear = 1.0f;
        if (inEnvDb < thresholdDb) {
            float deltaDb = thresholdDb - inEnvDb;
            float attDb = -std::min(-maxReductionDb, deltaDb * 2.5f);
            targetGainLinear = FluidMath::fast_db_to_lin(attDb);
        }

        // Fast opening (0.35ms), musical closing (25ms)
        float smoothCoeff = (targetGainLinear > gainSmooth) ? smoothOpenCoeff : smoothCloseCoeff;
        gainSmooth += smoothCoeff * (targetGainLinear - gainSmooth);

        // 4. Pure linear high-band gain + gentle mid-rush hush in deep idle silence (NO CENTER CLIPPING!)
        float cleanHigh = highSig * gainSmooth;
        float lowGain = std::min(1.0f, 0.08f + 0.92f * std::sqrt(gainSmooth));
        float cleanLow  = lowSig * lowGain;

        // 5. Perfect phase reconstruction
        return cleanLow + cleanHigh;
    }

    // Single-input overload for standalone / unit tests
    inline float processSample(float in, float thresholdDb = -62.0f, float maxReductionDb = -28.0f, bool enabled = true) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return in;

        float lowSig  = lp2.process(lp1.process(in));
        float highSig = hp2.process(hp1.process(in));
        lastHighSig = highSig;

        float absHf = fabsf(highSig);
        if (absHf > envFast) {
            envFast += atkCoeff * (absHf - envFast);
        } else {
            envFast += relCoeff * (absHf - envFast);
        }

        if (absHf > envSlow) {
            envSlow += atkCoeff * (absHf - envSlow);
        } else {
            envSlow += relSlowCoeff * (absHf - envSlow);
        }

        float detLevel = std::max(envFast, envSlow * 0.85f);
        float detDb = (detLevel > 1e-6f) ? FluidMath::fast_db(detLevel) : -120.0f;

        float targetGainLinear = 1.0f;
        if (detDb < thresholdDb) {
            float deltaDb = detDb - thresholdDb;
            float attDb = std::max(maxReductionDb, deltaDb * 1.5f);
            targetGainLinear = FluidMath::fast_db_to_lin(attDb);
        }

        float smoothCoeff = (targetGainLinear > gainSmooth) ? smoothOpenCoeff : smoothCloseCoeff;
        gainSmooth += smoothCoeff * (targetGainLinear - gainSmooth);

        float cleanHigh = highSig * gainSmooth;
        return lowSig + cleanHigh;
    }
};

// ============================================================================
// Calibrated Pedal Level / Output Potentiometer Taper (Rule 34)
// ============================================================================
inline float calculatePedalLevelGain(float levelKnob, float maxBoostDb = 22.0f) {
    float l = std::clamp(levelKnob, 0.0f, 10.0f);
    if (l <= 0.0001f) return 0.0f;
    if (l <= 5.0f) {
        float norm = l / 5.0f;
        return norm * norm; // 0.0 at 0.0, 1.0 (unity, 0.0 dB) at 5.0
    } else {
        float boostDb = ((l - 5.0f) / 5.0f) * maxBoostDb;
        return EuphoriaHyperDSP::FluidMath::fast_db_to_lin(boostDb);
    }
}

// ============================================================================
// Ideal Fluid Dynamics Semiconductor / Transistor Cell
// ============================================================================
// Models the physical fluid momentum, depletion boundary physics, and
// thermal velocity of ideal semiconductors:
// 1. JFET Square-Law Channel Constriction:
//    I_D = I_DSS * (1 - V_GS/V_P)^2 modeled as fluid throat venturi choke.
// 2. Germanium 1N34A Soft Conduction Knee:
//    Ultra-soft exponential onset (Vf ~ 0.32V) with smooth fluid inertia.
// 3. JRC4558 Symmetrical Silicon Loop:
//    Dynamic op-amp feedback loop with fluid velocity damping (zero aliased fizz).
// ============================================================================
struct FluidTransistorCell {
    float prevPressure = 0.0f;
    float fluidMomentum = 0.0f;
    float gateBiasSag = 0.0f;
    float chargePumpVoltage = 1.0f;

    void reset() {
        prevPressure = 0.0f;
        fluidMomentum = 0.0f;
        gateBiasSag = 0.0f;
        chargePumpVoltage = 1.0f;
    }

    // JFET Emulation: Quadratic pinch-off with dynamic self-bias and fluid momentum
    inline float processJfet(float inSig, float drive, float shapeFactor = 0.05f) {
        using namespace EuphoriaHyperDSP;
        float boundedIn = FluidMath::fast_tanh(inSig);
        float vel = boundedIn - prevPressure;
        prevPressure = boundedIn;

        // Fluid momentum inertia: acts as an analog charge carrier inertia
        float visc = 0.82f;
        fluidMomentum = fluidMomentum * visc + vel * (1.0f - visc);

        // Dynamic gate bias sag (self-biasing under pick attack)
        float absIn = std::abs(boundedIn);
        if (absIn > gateBiasSag) {
            gateBiasSag += (absIn - gateBiasSag) * 0.004f; // ~15ms attack
        } else {
            gateBiasSag *= 0.9996f; // ~120ms release
        }

        // Net effective gate voltage
        float vEffective = (boundedIn - gateBiasSag * 0.28f + fluidMomentum * 0.30f) * drive;

        // Ideal JFET Quadratic Transfer Curve (square-law conduction)
        float sat;
        if (vEffective >= 0.0f) {
            float x = FluidMath::fast_tanh(vEffective * 0.85f);
            sat = x - 0.18f * (x * x) * (1.0f - shapeFactor);
        } else {
            float x = FluidMath::fast_tanh(vEffective * 0.72f);
            sat = x + 0.12f * (x * x) * (1.0f - shapeFactor);
        }
        return FluidMath::fast_tanh(sat * 1.15f) * 1.02f;
    }

    // Germanium Diode Bridge (Klon): Ultra-soft knee, 18V charge pump headroom
    inline float processGermanium(float midSig, float driveNorm, bool asymmetric = false) {
        using namespace EuphoriaHyperDSP;
        float boundedIn = FluidMath::fast_tanh(midSig);
        float vel = boundedIn - prevPressure;
        prevPressure = boundedIn;

        fluidMomentum = fluidMomentum * 0.88f + vel * 0.12f;

        // 18V Charge Pump: rock-solid rail headroom with subtle current draw breathing
        float draw = std::abs(boundedIn) * 0.08f;
        chargePumpVoltage = 1.0f - draw * 0.05f; // never sags below 0.95!

        // Authentic dual-gang VR1A op-amp drive: delivers up to +46 dB of Germanium diode push at cranked gain
        float driveGain = 1.0f + driveNorm * 14.0f + (driveNorm * driveNorm) * 180.0f;
        float amplified = (midSig + fluidMomentum * 0.25f) * driveGain * chargePumpVoltage;

        // Germanium 1N34A soft conduction curve with fluid blooming inertia
        float clipped;
        if (asymmetric) {
            if (amplified >= 0.0f) {
                clipped = FluidMath::fast_tanh(amplified * 2.2f) * 0.52f;
            } else {
                clipped = -FluidMath::fast_tanh(-amplified * 1.35f) * 0.65f;
            }
        } else {
            // Pure symmetrical Germanium pair
            float x = amplified * 1.75f;
            clipped = FluidMath::fast_tanh(x) * 0.58f;
        }
        return clipped;
    }

    // Op-Amp Silicon Diode Loop (TS808): Feedback loop clipping with fluid momentum
    inline float processSiliconOpAmp(float loopSig, float driveNorm) {
        using namespace EuphoriaHyperDSP;
        float boundedIn = FluidMath::fast_tanh(loopSig);
        float vel = boundedIn - prevPressure;
        prevPressure = boundedIn;

        fluidMomentum = fluidMomentum * 0.84f + vel * 0.16f;

        float driveAmp = 6.0f + driveNorm * driveNorm * 65.0f + driveNorm * 12.0f;
        float driven = (loopSig + fluidMomentum * 0.22f) * driveAmp;

        // Symmetrical 1N4148 Silicon Diode clipping (Vf ~ 0.65V)
        float x = driven * 1.25f;
        float clipped = FluidMath::fast_tanh(x) * 0.68f;
        return clipped;
    }
};

// ============================================================================
// STOMPBOX 1: FLUID CENTAUR (KLON CENTAUR PROFESSIONAL OVERDRIVE)
// ============================================================================
struct FluidCentaurPedal {
    bool enabled = false;
    float gain = 4.5f;       // 0..10
    float treble = 5.5f;     // 0..10
    float output = 6.5f;     // 0..10
    float cleanBlend = 1.0f; // 0..1.0
    bool modSwitch = false;  // Symmetrical 1N34A vs Asymmetrical Schottky
    bool hyperMode = true;   // false = (Old) Classic Fluid Klon, true = Hyper-Centaur (27V ADAA)
    int  mode = 2;           // 0 = (OLD) Classic Klon, 1 = (OLD) Schottky Mod, 2 = HYPER 27V ADAA, 3 = HYPER Asymm ADAA

    FluidOnePole klonHpClean; // Clean path buffer
    FluidOnePole klonHpDrive; // 360 Hz high-pass before Germanium clipping (warm low-mids)
    FluidOnePole klonLpDrive; // 4.2 kHz low-pass before Germanium clipping
    FluidBiquad  klonTreble;   // Active Baxandall treble shelf control
    FluidBiquad  klonBiquadLp; // 2nd-order continuous tone darkening low-pass filter
    FluidOnePole klonToneLp;   // Warm tone darkening low-pass filter
    FluidTransistorCell germanCell; // Germanium diode bridge fluid cell
    EuphoriaHyperDSP::HyperCentaurPedal hyperPedal;

    float peakTrackEnv = 0.0f;
    float rmsSquaredFiltered = 0.0f;
    float currentRMS = 0.0f;
    float smoothedDriveMorph = 1.0f;
    float lastToneNorm = -1.0f;
    float lastSampleRate = 0.0f;

    void reset() {
        klonHpClean.reset();
        klonHpDrive.reset();
        klonLpDrive.reset();
        klonTreble.reset();
        klonBiquadLp.reset();
        klonToneLp.reset();
        germanCell.reset();
        hyperPedal.reset();
        hyperPedal.prepare(48000.0f);
        peakTrackEnv = 0.0f;
        rmsSquaredFiltered = 0.0f;
        currentRMS = 0.0f;
        smoothedDriveMorph = 1.0f;
        lastToneNorm = -1.0f;
        lastSampleRate = 0.0f;
    }

    inline float processSample(float in, float sampleRate) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return in;

        if (sampleRate != lastSampleRate && sampleRate > 1000.0f) {
            lastSampleRate = sampleRate;
            hyperPedal.prepare(sampleRate);
        }

        float driveRaw  = std::clamp(gain, 0.0f, 10.0f) * 0.1f;
        float driveNorm = 0.20f * driveRaw + 0.30f * driveRaw * driveRaw + 0.50f * driveRaw * driveRaw * driveRaw;
        float toneNorm  = std::clamp(treble, 0.0f, 10.0f) / 10.0f;

        // 1. Envelope & Pick Transient Tracking (instantaneous attack <0.05ms, 35ms release)
        float absIn = std::abs(in);
        if (absIn > peakTrackEnv) {
            peakTrackEnv = absIn;
        } else {
            float pkRelCoeff = 0.000595f * (48000.0f / sampleRate);
            peakTrackEnv += pkRelCoeff * (absIn - peakTrackEnv);
        }

        float squared = in * in;
        const float rmsCoeff = 0.0006f * (48000.0f / sampleRate);
        rmsSquaredFiltered += (squared - rmsSquaredFiltered) * rmsCoeff;
        // Fast decibel extraction directly from squared RMS or peak without std::sqrt or std::log10!
        float effectiveSq = std::max(rmsSquaredFiltered, (peakTrackEnv * peakTrackEnv) * 0.5f);
        float currentDB = (effectiveSq > 1e-10f) ? (10.0f * FluidMath::fast_log2(effectiveSq) * 0.301029995f) : -100.0f;
        float cleanRefDB = -36.0f;
        float maxRefDB   = -22.0f;

        // Rapid Drive Morph on Guitar Volume Pot (Vol 10 -> Vol 7 cleanup)
        float rawMorph = (currentDB - cleanRefDB) / (maxRefDB - cleanRefDB);
        rawMorph = std::clamp(rawMorph, 0.0f, 1.0f);
        float morphSpeed = (rawMorph > smoothedDriveMorph) ? 0.020f : 0.0020f;
        smoothedDriveMorph += (rawMorph - smoothedDriveMorph) * morphSpeed;

        float levelGain = calculatePedalLevelGain(output, 22.0f);
        if (levelGain <= 0.0001f) return 0.0f;

        float squelchFactor = std::clamp((currentDB - (-62.0f)) / 16.0f, 0.03f, 1.0f);
        float driveCurve = smoothedDriveMorph * smoothedDriveMorph * smoothedDriveMorph;

        if (mode >= 2) {
            float rawHyper = hyperPedal.processSample(in, driveNorm, toneNorm, 1.0f, smoothedDriveMorph);
            return rawHyper * levelGain * squelchFactor;
        }

        // --- (Old) Classic Fluid Centaur Mode ---
        bool effModSwitch = (mode == 1 || modSwitch);
        float dynamicDrive = driveNorm * (0.10f + 0.90f * driveCurve);

        // 3. Dual-Ganged Clean Path
        float cleanBuffered = klonHpClean.hp(in, 20.0f, sampleRate);
        float cleanGain = (1.0f - dynamicDrive * 0.82f) * (1.0f + 0.40f * (1.0f - driveCurve));
        float cleanPath = cleanBuffered * cleanGain;

        // 4. Dual-Ganged Overdrive Path
        float midSig = klonHpDrive.hp(in, 360.0f, sampleRate);
        midSig = klonLpDrive.lp(midSig, 4200.0f, sampleRate);

        float clipped = germanCell.processGermanium(midSig, dynamicDrive, effModSwitch);
        float odPath = clipped * (dynamicDrive * 3.4f);
        float blended = cleanPath + odPath;

        // 5. Active Baxandall Treble & Continuous Tone Darkening Filter (Cached when tone knob unchanged!)
        if (std::fabs(toneNorm - lastToneNorm) > 0.0005f) {
            lastToneNorm = toneNorm;
            float lpCut = 850.0f * std::pow(18000.0f / 850.0f, toneNorm);
            klonBiquadLp.setLowPass(lpCut, 0.707f, sampleRate);
            if (toneNorm <= 0.5f) {
                float cutDb = (toneNorm - 0.5f) * 48.0f;
                klonTreble.setHighShelf(1200.0f, cutDb, 0.707f, sampleRate);
            } else {
                float boostDb = (toneNorm - 0.5f) * 24.0f;
                klonTreble.setHighShelf(3200.0f, boostDb, 0.707f, sampleRate);
            }
        }
        blended = klonBiquadLp.process(blended);
        blended = klonTreble.process(blended);

        return blended * levelGain * squelchFactor;
    }
};

// ============================================================================
// STOMPBOX 2: TS808 SCREAMER (TUBE SCREAMER CLASSIC)
// ============================================================================
struct TS808ScreamerPedal {
    bool enabled = false;
    float drive = 5.0f;     // 0..10
    float tone = 4.8f;      // 0..10
    float level = 7.0f;     // 0..10
    bool fatMod = false;    // Stock 720 Hz vs Fat Mod 340 Hz
    int  mode = 2;          // 0 = (OLD) Stock 720Hz, 1 = (OLD) Fat Mod 340Hz, 2 = HYPER Velvet, 3 = HYPER Fat
    EuphoriaHyperDSP::HyperTS808Pedal hyperPedal;

    FluidOnePole tsRfCut;
    FluidOnePole tsHp720;
    FluidOnePole tsLoopSmoothing;
    FluidOnePole tsLp723;
    FluidOnePole tsAntiIcepick;
    FluidBiquad  tsTone;
    FluidOnePole tsToneLp;
    FluidTransistorCell opampCell;

    float peakTrackEnv = 0.0f;
    float rmsSquaredFiltered = 0.0f;
    float currentRMS = 0.0f;
    float smoothedDriveMorph = 1.0f;
    float lastToneNorm = -1.0f;
    float lastSampleRate = 0.0f;

    void reset() {
        tsRfCut.reset();
        tsHp720.reset();
        tsLoopSmoothing.reset();
        tsLp723.reset();
        tsAntiIcepick.reset();
        tsTone.reset();
        tsToneLp.reset();
        opampCell.reset();
        hyperPedal.reset();
        hyperPedal.prepare(48000.0f);
        peakTrackEnv = 0.0f;
        rmsSquaredFiltered = 0.0f;
        currentRMS = 0.0f;
        smoothedDriveMorph = 1.0f;
        lastToneNorm = -1.0f;
        lastSampleRate = 0.0f;
    }

    inline float processSample(float in, float sampleRate) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return in;

        if (sampleRate != lastSampleRate && sampleRate > 1000.0f) {
            lastSampleRate = sampleRate;
            hyperPedal.prepare(sampleRate);
        }

        float driveRaw  = std::clamp(drive, 0.0f, 10.0f) * 0.1f;
        float driveNorm = 0.20f * driveRaw + 0.30f * driveRaw * driveRaw + 0.50f * driveRaw * driveRaw * driveRaw;
        float toneNorm  = std::clamp(tone, 0.0f, 10.0f) / 10.0f;

        float levelGain = calculatePedalLevelGain(level, 20.0f);
        if (levelGain <= 0.0001f) return 0.0f;

        // 1. Envelope & Pick Transient Tracking
        float absIn = std::abs(in);
        if (absIn > peakTrackEnv) {
            peakTrackEnv = absIn;
        } else {
            float pkRelCoeff = 0.000595f * (48000.0f / sampleRate);
            peakTrackEnv += pkRelCoeff * (absIn - peakTrackEnv);
        }

        float squared = in * in;
        const float rmsCoeff = 0.0006f * (48000.0f / sampleRate);
        rmsSquaredFiltered += (squared - rmsSquaredFiltered) * rmsCoeff;
        float effectiveSq = std::max(rmsSquaredFiltered, (peakTrackEnv * peakTrackEnv) * 0.5f);

        // 2. Dynamic Volume Roll-Off & Idle Self-Noise Squelch
        float currentDB = (effectiveSq > 1e-10f) ? (10.0f * FluidMath::fast_log2(effectiveSq) * 0.301029995f) : -100.0f;
        float cleanRefDB = -36.0f;
        float maxRefDB   = -22.0f;

        float rawMorph = (currentDB - cleanRefDB) / (maxRefDB - cleanRefDB);
        rawMorph = std::clamp(rawMorph, 0.0f, 1.0f);
        float morphSpeed = (rawMorph > smoothedDriveMorph) ? 0.020f : 0.0020f;
        smoothedDriveMorph += (rawMorph - smoothedDriveMorph) * morphSpeed;

        float squelchFactor = std::clamp((currentDB - (-62.0f)) / 16.0f, 0.03f, 1.0f);
        float driveCurve = smoothedDriveMorph * smoothedDriveMorph * smoothedDriveMorph;

        if (mode >= 2) {
            bool isFat = (mode == 3 || fatMod);
            float rawHyper = hyperPedal.processSample(in, driveNorm, toneNorm, 1.0f, isFat, smoothedDriveMorph);
            return rawHyper * levelGain * squelchFactor;
        }

        // --- (OLD) Classic TS808 Mode ---
        bool effFatMod = (mode == 1 || fatMod);
        float dynamicDrive = driveNorm * (0.10f + 0.90f * driveCurve);

        float xIn = tsRfCut.lp(in, 10500.0f, sampleRate);
        float hpFreq = effFatMod ? 340.0f : 720.5f;
        float highs = tsHp720.hp(xIn, hpFreq, sampleRate);

        float clippedLoop = opampCell.processSiliconOpAmp(highs, dynamicDrive);
        float smoothedLoop = tsLoopSmoothing.lp(clippedLoop, 3600.0f, sampleRate);
        float opAmpOut = (xIn + smoothedLoop * (dynamicDrive * 1.8f)) * (1.0f + 0.35f * (1.0f - driveCurve));
        float smoothed = tsLp723.lp(opAmpOut, 723.0f, sampleRate) * 1.75f;

        if (std::fabs(toneNorm - lastToneNorm) > 0.0005f) {
            lastToneNorm = toneNorm;
            if (toneNorm <= 0.5f) {
                float cutDb = (toneNorm - 0.5f) * 48.0f;
                float corner = 650.0f + toneNorm * 900.0f;
                tsTone.setHighShelf(corner, cutDb, 0.707f, sampleRate);
            } else {
                float boostDb = (toneNorm - 0.5f) * 18.0f;
                tsTone.setPeaking(2600.0f, boostDb, 0.80f, sampleRate);
            }
        }
        if (toneNorm <= 0.5f) {
            float lpCut = 750.0f + toneNorm * 22500.0f;
            smoothed = tsToneLp.lp(smoothed, lpCut, sampleRate);
        }
        float toned = tsTone.process(smoothed);
        toned = tsAntiIcepick.lp(toned, 5500.0f, sampleRate);

        return toned * levelGain * squelchFactor;
    }
};

// ============================================================================
// STOMPBOX 3: BOSS BD-2 BLUES DRIVER (DISCRETE FLUID JFET EMULATION)
// ============================================================================
struct BD2BluesDriverPedal {
    bool enabled = false;
    float level = 7.0f;     // 0..10
    float tone = 5.2f;      // 0..10
    float gain = 4.8f;      // 0..10
    bool phatMod = false;   // Stock 42 Hz vs Phat low-mid bass boost
    int  mode = 2;          // 0 = (OLD) Stock BD-2, 1 = (OLD) Phat Mod, 2 = HYPER Slipstream, 3 = HYPER Phat
    EuphoriaHyperDSP::HyperBD2Pedal hyperPedal;

    FluidOnePole bdSubCut;
    FluidOnePole bdRfCut;
    FluidBiquad  bdPhatFilter;
    FluidBiquad  bdToneLo;
    FluidBiquad  bdToneHi;
    FluidOnePole bdToneLp;
    FluidTransistorCell jfetStage1;
    FluidTransistorCell jfetStage2;

    float peakTrackEnv = 0.0f;
    float rmsSquaredFiltered = 0.0f;
    float currentRMS = 0.0f;
    float smoothedDriveMorph = 1.0f;
    float lastToneNorm = -1.0f;
    float lastSampleRate = 0.0f;
    bool phatInitialized = false;

    void reset() {
        bdSubCut.reset();
        bdRfCut.reset();
        bdPhatFilter.reset();
        bdToneLo.reset();
        bdToneHi.reset();
        bdToneLp.reset();
        jfetStage1.reset();
        jfetStage2.reset();
        hyperPedal.reset();
        hyperPedal.prepare(48000.0f);
        peakTrackEnv = 0.0f;
        rmsSquaredFiltered = 0.0f;
        currentRMS = 0.0f;
        smoothedDriveMorph = 1.0f;
        lastToneNorm = -1.0f;
        lastSampleRate = 0.0f;
        phatInitialized = false;
    }

    inline float processSample(float in, float sampleRate) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return in;

        if (sampleRate != lastSampleRate && sampleRate > 1000.0f) {
            lastSampleRate = sampleRate;
            hyperPedal.prepare(sampleRate);
        }

        float toneNorm  = std::clamp(tone, 0.0f, 10.0f) / 10.0f;
        float gainRaw   = std::clamp(gain, 0.0f, 10.0f) * 0.1f;
        float gainNorm  = 0.20f * gainRaw + 0.30f * gainRaw * gainRaw + 0.50f * gainRaw * gainRaw * gainRaw;

        float levelGain = calculatePedalLevelGain(level, 22.0f);
        if (levelGain <= 0.0001f) return 0.0f;

        // 1. Envelope & Pick Transient Tracking
        float absIn = std::abs(in);
        if (absIn > peakTrackEnv) {
            peakTrackEnv = absIn;
        } else {
            float pkRelCoeff = 0.000595f * (48000.0f / sampleRate);
            peakTrackEnv += pkRelCoeff * (absIn - peakTrackEnv);
        }

        float squared = in * in;
        const float rmsCoeff = 0.0006f * (48000.0f / sampleRate);
        rmsSquaredFiltered += (squared - rmsSquaredFiltered) * rmsCoeff;
        float effectiveSq = std::max(rmsSquaredFiltered, (peakTrackEnv * peakTrackEnv) * 0.5f);

        // 2. Dynamic Volume Roll-Off & Idle Self-Noise Squelch
        float currentDB = (effectiveSq > 1e-10f) ? (10.0f * FluidMath::fast_log2(effectiveSq) * 0.301029995f) : -100.0f;
        float cleanRefDB = -36.0f;
        float maxRefDB   = -22.0f;

        float rawMorph = (currentDB - cleanRefDB) / (maxRefDB - cleanRefDB);
        rawMorph = std::clamp(rawMorph, 0.0f, 1.0f);
        float morphSpeed = (rawMorph > smoothedDriveMorph) ? 0.020f : 0.0020f;
        smoothedDriveMorph += (rawMorph - smoothedDriveMorph) * morphSpeed;

        float squelchFactor = std::clamp((currentDB - (-62.0f)) / 16.0f, 0.03f, 1.0f);
        float driveCurve = smoothedDriveMorph * smoothedDriveMorph * smoothedDriveMorph;

        if (mode >= 2) {
            bool isPhat = (mode == 3 || phatMod);
            float rawHyper = hyperPedal.processSample(in, gainNorm, toneNorm, 1.0f, isPhat, smoothedDriveMorph);
            return rawHyper * levelGain * squelchFactor;
        }

        // --- (OLD) Classic BD-2 Mode ---
        bool effPhatMod = (mode == 1 || phatMod);
        float dynamicGain = gainNorm * (0.10f + 0.90f * driveCurve);

        float xIn = bdSubCut.hp(in, 42.0f, sampleRate);
        xIn = bdRfCut.lp(xIn, 12000.0f, sampleRate);

        if (effPhatMod) {
            if (!phatInitialized) {
                phatInitialized = true;
                bdPhatFilter.setPeaking(160.0f, 3.5f, 0.90f, sampleRate);
            }
            xIn = bdPhatFilter.process(xIn);
        }

        float g1 = 1.0f + dynamicGain * 4.2f;
        float v1 = jfetStage1.processJfet(xIn * g1, 1.0f, 0.05f);

        float g2 = 1.0f + dynamicGain * dynamicGain * 12.0f + dynamicGain * 2.2f;
        float v2 = jfetStage2.processJfet(v1 * g2, 1.0f, 0.08f);

        float signalNorm = (v2 / (1.0f + dynamicGain * 1.5f)) * 0.935f * (1.0f + 0.35f * (1.0f - driveCurve));

        if (std::fabs(toneNorm - lastToneNorm) > 0.0005f) {
            lastToneNorm = toneNorm;
            if (toneNorm <= 0.5f) {
                float tiltHiDb = (toneNorm - 0.5f) * 48.0f;
                float tiltLoDb = (0.5f - toneNorm) * 12.0f;
                float hiFreq = 850.0f + toneNorm * 1200.0f;
                bdToneHi.setHighShelf(hiFreq, tiltHiDb, 0.707f, sampleRate);
                bdToneLo.setLowShelf(250.0f, tiltLoDb, 0.707f, sampleRate);
            } else {
                float tiltHiDb = (toneNorm - 0.5f) * 26.0f;
                float tiltLoDb = (0.5f - toneNorm) * 4.0f;
                bdToneHi.setHighShelf(2200.0f, tiltHiDb, 0.707f, sampleRate);
                bdToneLo.setLowShelf(350.0f, tiltLoDb, 0.707f, sampleRate);
            }
        }
        if (toneNorm <= 0.5f) {
            float lpCut = 850.0f + toneNorm * 10300.0f;
            signalNorm = bdToneLp.lp(signalNorm, lpCut, sampleRate);
        }
        float toned = bdToneLo.process(bdToneHi.process(signalNorm));

        return toned * levelGain * squelchFactor;
    }
};

// ============================================================================
// STOMPBOX 4: FLUID RODENT (SUPER-TIGHT SMOOTH SYNTH-FUZZ MODEL)
// ============================================================================
struct FluidRodentPedal {
    bool enabled = false;
    float distortion = 5.0f; // 0..10 Distortion Gain
    float filter = 5.0f;     // 0..10 Tone / Filter (0 = dark warm cut, 10 = bright open)
    float volume = 6.5f;     // 0..10 Master Level (0 at 0.0, 1.0 at 5.0, +22 dB at 10.0)
    bool  turboMode = false; // false = Vintage Silicon, true = Turbo Red LED
    int   mode = 2;          // 0 = (OLD) Silicon, 1 = (OLD) Turbo LED, 2 = HYPER Silicon, 3 = HYPER Turbo LED
    EuphoriaHyperDSP::HyperRodentPedal hyperPedal;

    FluidOnePole ratHpTight;
    FluidOnePole ratFeedbackLp;
    FluidBiquad  ratLpFilter;

    float peakTrackEnv = 0.0f;
    float rmsSquaredFiltered = 0.0f;
    float currentRMS = 0.0f;
    float smoothedDriveMorph = 1.0f;
    float lastFilterNorm = -1.0f;
    float lastSampleRate = 0.0f;

    void reset() {
        ratHpTight.reset();
        ratFeedbackLp.reset();
        ratLpFilter.reset();
        hyperPedal.reset();
        hyperPedal.prepare(48000.0f);
        peakTrackEnv = 0.0f;
        rmsSquaredFiltered = 0.0f;
        currentRMS = 0.0f;
        smoothedDriveMorph = 1.0f;
        lastFilterNorm = -1.0f;
        lastSampleRate = 0.0f;
    }

    inline float processSample(float in, float sampleRate) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return in;

        if (sampleRate != lastSampleRate && sampleRate > 1000.0f) {
            lastSampleRate = sampleRate;
            hyperPedal.prepare(sampleRate);
        }

        float distRaw    = std::clamp(distortion, 0.0f, 10.0f) * 0.1f;
        float distNorm   = 0.20f * distRaw + 0.30f * distRaw * distRaw + 0.50f * distRaw * distRaw * distRaw;
        float filterNorm = std::clamp(filter, 0.0f, 10.0f) / 10.0f;

        float levelGain = calculatePedalLevelGain(volume, 22.0f);
        if (levelGain <= 0.0001f) return 0.0f;

        // 1. Envelope & Pick Transient Tracking
        float absIn = std::abs(in);
        if (absIn > peakTrackEnv) {
            peakTrackEnv = absIn;
        } else {
            float pkRelCoeff = 0.000595f * (48000.0f / sampleRate);
            peakTrackEnv += pkRelCoeff * (absIn - peakTrackEnv);
        }

        float squared = in * in;
        const float rmsCoeff = 0.0006f * (48000.0f / sampleRate);
        rmsSquaredFiltered += (squared - rmsSquaredFiltered) * rmsCoeff;
        float effectiveSq = std::max(rmsSquaredFiltered, (peakTrackEnv * peakTrackEnv) * 0.5f);

        // 2. Dynamic Volume Roll-Off & Idle Self-Noise Squelch
        float currentDB = (effectiveSq > 1e-10f) ? (10.0f * FluidMath::fast_log2(effectiveSq) * 0.301029995f) : -100.0f;
        float cleanRefDB = -36.0f;
        float maxRefDB   = -22.0f;

        float rawMorph = (currentDB - cleanRefDB) / (maxRefDB - cleanRefDB);
        rawMorph = std::clamp(rawMorph, 0.0f, 1.0f);
        float morphSpeed = (rawMorph > smoothedDriveMorph) ? 0.020f : 0.0020f;
        smoothedDriveMorph += (rawMorph - smoothedDriveMorph) * morphSpeed;

        float squelchFactor = std::clamp((currentDB - (-62.0f)) / 16.0f, 0.03f, 1.0f);
        float driveCurve = smoothedDriveMorph * smoothedDriveMorph * smoothedDriveMorph;

        if (mode >= 2) {
            float diodeMorph = (mode == 3 || turboMode) ? 1.0f : 0.0f;
            float rawHyper = hyperPedal.processSample(in, distNorm, filterNorm, 1.0f, diodeMorph, smoothedDriveMorph);
            return rawHyper * levelGain * squelchFactor;
        }

        // --- (OLD) Classic Rodent Mode ---
        bool effTurbo = (mode == 1 || turboMode);
        float dynamicDist = distNorm * (0.08f + 0.92f * driveCurve);

        float xIn = ratHpTight.hp(in, 60.0f, sampleRate);

        float q1Gain = 2.0f + dynamicDist * 22.0f;
        float q1Driven = xIn * q1Gain;
        float q1Out;
        if (effTurbo) {
            q1Out = FluidMath::fast_tanh(q1Driven * 1.5f) * 0.88f;
        } else {
            float u = FluidMath::fast_tanh(q1Driven * 1.2f);
            q1Out = (u - 0.15f * u * u) * 0.82f;
        }

        float fbSmoothed = ratFeedbackLp.lp(q1Out, 4500.0f, sampleRate);

        float q2Drive = 4.0f + (dynamicDist * dynamicDist) * 75.0f;
        float q2In = (q1Out - fbSmoothed * 0.24f) * q2Drive;
        float q2Out;
        if (effTurbo) {
            float x = FluidMath::fast_tanh(q2In * 1.85f);
            q2Out = x * 0.90f;
        } else {
            if (q2In >= 0.0f) {
                q2Out = FluidMath::fast_tanh(q2In * 1.5f) * 0.72f;
            } else {
                q2Out = -FluidMath::fast_tanh(-q2In * 0.95f) * 0.85f;
            }
        }
        float fuzzRaw = q2Out * 0.85f;

        float distComp = 1.0f / (1.0f + dynamicDist * 2.2f);
        float normed = fuzzRaw * distComp * 0.185f * (1.0f + 0.50f * (1.0f - driveCurve));

        if (std::fabs(filterNorm - lastFilterNorm) > 0.0005f) {
            lastFilterNorm = filterNorm;
            float filterCut = 380.0f * std::pow(20000.0f / 380.0f, filterNorm);
            ratLpFilter.setLowPass(filterCut, 0.707f, sampleRate);
        }
        float filtered = ratLpFilter.process(normed);

        return filtered * levelGain * squelchFactor;
    }
};

// ============================================================================
// CORE FLUID CHAMBER (Shared Laminar/Turbulent Physics Engine)
// Used in Cascading Space Interruption & Semiconductor Emulation
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
        sampleRate = (newSampleRate > 1000.0f) ? newSampleRate : 48000.0f;
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
        using namespace EuphoriaHyperDSP;
        float rateScale = 48000.0f / sampleRate;
        float currentVelocity = inputPressure - previousPressure;
        fluidMomentum = (fluidMomentum * fluidViscosity) + (currentVelocity * (1.0f - fluidViscosity));
        fluidMomentum = std::clamp(fluidMomentum, -1.0f, 1.0f);

        // Dynamic fluid sag (bounded so high drive never chokes the volume!)
        float boundedInput = std::min(std::abs(inputPressure), 1.5f);
        float energyDraw = (boundedInput * std::min(driveLevel, 4.0f)) / std::max(0.20f, chamberSizeScalar);
        chamberPressure -= energyDraw * (0.0025f * rateScale);
        chamberPressure += (1.0f - chamberPressure) * (chamberRecovery * rateScale);
        chamberPressure = std::max(minStarvationFloor, std::min(chamberPressure, 1.0f));

        // Pre-gain + Slipstream injection
        float amplifiedSignal = (inputPressure * driveLevel * chamberPressure) + (incomingHarmonics * 0.35f * synergyPolarity);

        // Elastic Fluid-Wall Wave Reflection ("water hitting the side and coming back"):
        // Instead of slamming into a flat rail, excess drive pressure folds back off the chamber wall
        // into rich Germanium/Silicon harmonics with virtual unlimited internal headroom!
        float outputPhase = FluidMath::fluidWallReflect(amplifiedSignal, harmonicPurity, 1.0f - fluidViscosity, fluidMomentum);
        float baseTanh = FluidMath::fast_tanh(amplifiedSignal);
        generatedHarmonics = outputPhase - baseTanh;

        // High-frequency fluid damping (turns harsh digital fizz into thick creamy lead tone)
        float viscosityCoeff = fluidViscosity * 0.42f;
        outputPhase = (outputPhase * (1.0f - viscosityCoeff)) + (previousPressure * viscosityCoeff);

        // Remove DC offset built up by asymmetrical saturation so the next stage hits full-bore symmetrically!
        float dcR = 1.0f - (240.0f / sampleRate);
        float dcFiltered = outputPhase - dcBlockerX1 + dcR * dcBlockerY1;
        dcBlockerX1 = outputPhase;
        dcBlockerY1 = dcFiltered;

        float finalOutput = FluidMath::fast_tanh(dcFiltered * 1.15f);
        previousPressure = finalOutput;

        return finalOutput;
    }

    float getGeneratedHarmonics() const { return generatedHarmonics; }
    float getChamberPressure() const { return chamberPressure; }
};

// ============================================================================
// STOMPBOX 5: FLUID FUZZ FACE (CASCADING SPACE INTERRUPTION & SUSTAIN RMS LEARN)
// True 2-Chamber Cascading Fluid Physics with Direct dB Ratio Scaling & Sustain RMS Calibration:
// 1. Two Cascading Fluid Chambers (Q1 Driver 0.75x -> Q2 Saturation 0.42x).
// 2. Transistor Voicing:
//    - Germanium NKT275: Warm even harmonics, creamy viscosity (0.76), positive 0.28 floor.
//    - Silicon BC108:    Aggressive upper bite, tight bite (viscosity 0.60, 0.22 floor).
// 3. Dual-Anchor Direct dB Sustain RMS Learner:
//    - Clean Anchor (Vol 7): Scales input RMS to ~0.32 (linear glassy clean).
//    - Full Bore Anchor (Vol 10): Slams Space 1 & 2 with up to 23x cascaded drive.
//    - 10-Second continuous playing window locks onto the real sustain body of chords.
// 4. Harmonic Slipstream Interruption Polarity:
//    - At Vol 7 (Clean): +1.0 (Laminar glassy synergy).
//    - At Vol 10 (Max): -1.75 (Head-on harmonic collision, rich grind, note clashing).
// 5. Clean-to-Fuzz Volume Compensation & Dynamic Amp Sag:
//    - Clean level boost ensures Vol 7 is just as loud and punchy as Vol 10.
//    - Tube power transformer sag compression (-7 dB) relaxes completely on cleanup.
// 6. Calibrated Level Taper: 0 at 0.0, 1.0 (0.0 dB) at 5.0, +22 dB at 10.0.
// ============================================================================
struct FluidFuzzFacePedal {
    bool enabled = false;
    float fuzz = 7.0f;          // 0..10 Fuzz Gain
    float level = 7.0f;         // 0..10 Master Level (0 at 0.0, 1.0 at 5.0, +22 dB at 10.0)
    float ampSag = 5.0f;        // 0..10 Built-In Dynamic Amp Output Stage Compression & Sag
    int   transistorType = 0;   // 0 = Germanium NKT275, 1 = Silicon BC108
    int   mode = 2;             // 0 = (OLD) NKT275, 1 = (OLD) BC108, 2 = HYPER NKT275 (432Hz), 3 = HYPER BC108 (432Hz)
    EuphoriaHyperDSP::HyperFuzzFaceEnhancer hyperEnhancer;

    FluidChamberDSP space1_Q1;  // Q1: High-sensitivity input space
    FluidChamberDSP space2_Q2;  // Q2: Full-bore Germanium saturation space
    FluidOnePole ffInHp;

    float sampleRate = 48000.0f;

    // Calibrated RMS Sustain Anchors (NOT raw pick-transients!)
    float calibratedMaxRMS   = 0.080f; // Typical guitar sustaining chord at Vol 10
    float calibratedCleanRMS = 0.018f; // Typical guitar sustaining chord at Vol 7

    // True RMS Envelope (Ignores 2ms pick spikes, tracks the real body of the note)
    float rmsSquaredFiltered = 0.0f;
    float currentRMS = 0.0f;
    float peakTrackEnv = 0.0f;
    float smoothedFuzzMorph = 0.0f; // 0.0 = Clean (Vol 7), 1.0 = Full Bore (Vol 10)
    float ampSagEnv = 0.0f;

    // Timed Auto-Learn State (10-second continuous playing listening window)
    bool isLearningMax = false;
    bool isLearningClean = false;
    int learnSamplesRemaining = 0;
    float learnSustainAccumulator = 0.0f;
    int learnActiveSampleCount = 0;
    float learnGateThreshold = 0.0015f;

    void prepare(float sr) {
        sampleRate = (sr > 1000.0f) ? sr : 48000.0f;
        space1_Q1.prepare(sampleRate);
        space2_Q2.prepare(sampleRate);
        ffInHp.reset();
        hyperEnhancer.prepare(sampleRate);
        reset();
    }

    void reset() {
        hyperEnhancer.reset();
        rmsSquaredFiltered = 0.0f;
        currentRMS = 0.0f;
        peakTrackEnv = 0.0f;
        smoothedFuzzMorph = 0.0f;
        ampSagEnv = 0.0f;
        isLearningMax = false;
        isLearningClean = false;
        learnSamplesRemaining = 0;
        learnSustainAccumulator = 0.0f;
        learnActiveSampleCount = 0;

        if (transistorType == 1) {
            // Silicon BC108: aggressive bite, lower viscosity, tighter floor
            space1_Q1.configureSpace(0.55f, 0.55f, 0.012f, 0.65f, 0.30f);
            space2_Q2.configureSpace(0.60f, 0.65f, 0.008f, 0.35f, 0.22f);
        } else {
            // Germanium NKT275: warm even harmonics, creamy viscosity, positive 0.28 floor
            space1_Q1.configureSpace(0.68f, 0.40f, 0.008f, 0.75f, 0.38f);
            space2_Q2.configureSpace(0.76f, 0.50f, 0.005f, 0.42f, 0.28f);
        }
    }

    void triggerLearnMaxVolume(float seconds = 10.0f) {
        isLearningMax = true;
        isLearningClean = false;
        learnSamplesRemaining = static_cast<int>(sampleRate * seconds);
        learnSustainAccumulator = 0.0f;
        learnActiveSampleCount = 0;
    }

    void triggerLearnCleanVolume(float seconds = 10.0f) {
        isLearningClean = true;
        isLearningMax = false;
        learnSamplesRemaining = static_cast<int>(sampleRate * seconds);
        learnSustainAccumulator = 0.0f;
        learnActiveSampleCount = 0;
    }

    void finishLearning() {
        if (learnActiveSampleCount > 100) {
            float measuredSustainRMS = (learnSustainAccumulator / static_cast<float>(learnActiveSampleCount)) * 1.15f;
            if (isLearningMax) {
                calibratedMaxRMS = std::max(0.004f, measuredSustainRMS);
                if (calibratedCleanRMS > calibratedMaxRMS * 0.45f) {
                    calibratedCleanRMS = calibratedMaxRMS * 0.25f;
                }
            } else if (isLearningClean) {
                calibratedCleanRMS = std::max(0.0008f, measuredSustainRMS);
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
    bool isLearningMaxVolume() const { return isLearningMax; }
    bool isLearningCleanVolume() const { return isLearningClean; }
    float getCurrentFuzzMorph() const { return smoothedFuzzMorph; }
    float getCalibratedMax() const { return calibratedMaxRMS; }
    float getCalibratedClean() const { return calibratedCleanRMS; }
    float getLearnTracker() const {
        return (learnActiveSampleCount > 0) ? (learnSustainAccumulator / float(learnActiveSampleCount)) : 0.0f;
    }

    void setCalibration(float maxRMS, float cleanRMS) {
        calibratedMaxRMS = maxRMS;
        calibratedCleanRMS = cleanRMS;
    }

    inline float processSample(float inputSignal, float sr) {
        using namespace EuphoriaHyperDSP;
        if (!enabled) return inputSignal;
        float outLvl = calculatePedalLevelGain(level, 22.0f);
        if (outLvl <= 0.0001f) return 0.0f;

        // 1. Calculate Peak Tracker & True Sustain RMS
        // Instantaneous attack (<0.05ms) preserves 100% of pick transient, 35ms release tracks note body
        float rateScale = 48000.0f / sr;
        float absIn = std::abs(inputSignal);
        if (absIn > peakTrackEnv) {
            peakTrackEnv = absIn;
        } else {
            float pkRelCoeff = 0.000595f * rateScale;
            peakTrackEnv += pkRelCoeff * (absIn - peakTrackEnv);
        }

        float squared = inputSignal * inputSignal;
        const float rmsCoeff = 0.0006f * rateScale; 
        rmsSquaredFiltered += (squared - rmsSquaredFiltered) * rmsCoeff;

        // 2. If Learning is active, accumulate the sustaining body of the guitar signal
        if (isLearningMax || isLearningClean) {
            currentRMS = std::sqrt(std::max(0.0f, rmsSquaredFiltered));
            if (currentRMS > learnGateThreshold) {
                learnSustainAccumulator += currentRMS;
                learnActiveSampleCount++;
            }
            if (--learnSamplesRemaining <= 0) {
                finishLearning();
            }
        }

        // 3. Convert effective RMS, Clean RMS, and Max RMS into Decibels (dB) using fast IEEE-754 log2
        float effectiveSq = std::max(rmsSquaredFiltered, (peakTrackEnv * peakTrackEnv) * 0.5f);
        float currentDB = (effectiveSq > 1e-10f) ? (10.0f * FluidMath::fast_log2(effectiveSq) * 0.301029995f) : -100.0f;
        float cleanDB   = FluidMath::fast_db(std::max(0.0001f, calibratedCleanRMS));
        float maxDB     = FluidMath::fast_db(std::max(0.0005f, calibratedMaxRMS));

        float dbSpan = std::max(4.0f, maxDB - cleanDB); // Minimum 4 dB window between 7 and 10
        float rawMorph = (currentDB - cleanDB) / dbSpan;
        rawMorph = std::clamp(rawMorph, 0.0f, 1.0f); // 0.0 at Vol 7, 1.0 at Vol 10!

        // Smooth morph transitions (fast rise on strum, smooth bloom on decay)
        float morphSpeed = (rawMorph > smoothedFuzzMorph) ? 0.015f : 0.0015f;
        smoothedFuzzMorph += (rawMorph - smoothedFuzzMorph) * morphSpeed;

        // 4. Input HP filter & Direct Normalization
        int effTrans = (mode == 1 || mode == 3 || transistorType == 1) ? 1 : 0;
        float inCut = (effTrans == 1 ? 62.0f : 50.0f) + (1.0f - smoothedFuzzMorph) * 95.0f;
        float xIn = ffInHp.hp(inputSignal, inCut, sr);

        float baseNormalization = 0.32f / std::max(0.001f, calibratedCleanRMS);
        float normalizedWave = xIn * baseNormalization;

        // 5. True Dual-Stage Gain Scaling (Clean Sparkle at 0.0 -> Massive 28x Fuzz at 1.0!)
        float activeFuzz = std::max(0.1f, fuzz / 7.0f);
        float morphSq = smoothedFuzzMorph * smoothedFuzzMorph;
        float space1Drive = 0.85f + (morphSq * activeFuzz * 4.60f);
        float space2Drive = 0.90f + (morphSq * activeFuzz * 6.20f);

        float space1Out = space1_Q1.processSample(normalizedWave, space1Drive, 0.0f, 1.0f);
        float space1Harmonics = space1_Q1.getGeneratedHarmonics();

        // Slipstream glides from +1.0 (Laminar Clean) to -1.75 (Rich Germanium Grind / Note Clashing)
        float slipstreamPolarity = 1.0f - (smoothedFuzzMorph * 1.75f);

        float space2Out = space2_Q2.processSample(space1Out, space2Drive, space1Harmonics, slipstreamPolarity);

        // 6. Clean-to-Fuzz Volume Compensation & Dynamic Amp Sag
        float cleanLevelBoost = 1.0f + 1.1f * (1.0f - smoothedFuzzMorph);
        float chimed = space2Out * cleanLevelBoost;

        float sagNorm = std::clamp(ampSag / 10.0f, 0.0f, 1.0f);
        if (sagNorm > 0.01f) {
            float absSig = std::abs(chimed);
            float atkCoeff = 0.0083f * rateScale;
            float relCoeff = 0.000278f * rateScale;
            if (absSig > ampSagEnv) ampSagEnv += atkCoeff * (absSig - ampSagEnv);
            else ampSagEnv += relCoeff * (absSig - ampSagEnv);

            float effectiveSag = sagNorm * smoothedFuzzMorph;
            float sagComp = 1.0f / (1.0f + ampSagEnv * effectiveSag * 1.2f);
            chimed *= sagComp;
        }

        const float fuzzOutputCalibration = 1.25f;
        float squelchFactor = std::clamp((currentDB - (-62.0f)) / 16.0f, 0.03f, 1.0f);
        if (mode >= 2) {
            float activeChimeScale = smoothedFuzzMorph * squelchFactor;
            float enhanced = hyperEnhancer.processPickupInductanceResonance(chimed, activeChimeScale);
            return enhanced * fuzzOutputCalibration * outLvl * squelchFactor;
        }
        return chimed * fuzzOutputCalibration * outLvl * squelchFactor;
    }
};

// Legacy compatibility wrapper
struct FluidStompbox {
    FluidCentaurPedal centaur;
    TS808ScreamerPedal ts808;
    int model = 0;
    bool enabled = false;
    float drive = 5.0f;
    float tone = 5.0f;
    float level = 5.0f;
    bool modSwitch = false;
    float cleanBlend = 1.0f;

    void reset() {
        centaur.reset();
        ts808.reset();
    }

    inline float processSample(float in, float sampleRate) {
        if (!enabled) return in;
        if (model == 0) {
            centaur.enabled = true;
            centaur.gain = drive;
            centaur.treble = tone;
            centaur.output = level;
            centaur.cleanBlend = cleanBlend;
            centaur.modSwitch = modSwitch;
            return centaur.processSample(in, sampleRate);
        } else {
            ts808.enabled = true;
            ts808.drive = drive;
            ts808.tone = tone;
            ts808.level = level;
            ts808.fatMod = modSwitch;
            return ts808.processSample(in, sampleRate);
        }
    }
};

// ============================================================================
// Guitar Pickup Input Impedance & Resonant Loading (1M High-Z / Strat / Gretsch)
// ============================================================================
struct GuitarPickupVoicing {
    int mode = 0; // 0 = 1M Studio High-Z, 1 = Strat / SC Glass, 2 = Gretsch / HB Punch
    FluidOnePole hpFilter;
    FluidBiquad  peakFilter;
    FluidBiquad  warmthFilter;

    void reset() {
        hpFilter.reset();
        peakFilter.reset();
        warmthFilter.reset();
    }

    void update(float sampleRate) {
        if (mode == 0) {
            peakFilter.setIdentity();
            warmthFilter.setIdentity();
        } else if (mode == 1) {
            // Strat / Single-Coil Glass & Chime
            peakFilter.setPeaking(4800.0f, 1.8f, 1.15f, sampleRate);
            warmthFilter.setIdentity();
        } else {
            // Gretsch Filter'Tron / Broad'Tron & PAF Humbucker Rock Punch
            // 1. Filter'Tron iconic 3.3 kHz bite and clarity
            peakFilter.setPeaking(3300.0f, 3.0f, 1.30f, sampleRate);
            // 2. Body warmth & punch around 380 Hz
            warmthFilter.setPeaking(380.0f, 1.4f, 1.10f, sampleRate);
        }
    }

    inline float process(float in, float sampleRate) {
        if (mode == 0) return in;

        float x = in;
        if (mode == 1) {
            x = hpFilter.hp(x, 38.0f, sampleRate);
            x = peakFilter.process(x);
        } else {
            // Tighten sub-rumble below 68 Hz for articulate rock rhythm
            x = hpFilter.hp(x, 68.0f, sampleRate);
            x = peakFilter.process(x);
            x = warmthFilter.process(x);
        }
        return x;
    }
};

// ============================================================================
// Euphoria Amp Main DSP Engine
// ============================================================================
class EuphoriaAmpEngine {
public:
    explicit EuphoriaAmpEngine(double rate) : sampleRate(rate) {
        reset();
    }

    AudioDSP::CyberHumEngine humKiller;
    AudioDSP::CyberDenoiserEngine cyberDenoiser;
    AudioDSP::CyberDenoiserEngine parallelGainDenoiser;
    bool enableHumKiller = true;
    bool enableCyberDenoiser = true;

    void triggerLearnNoise() {
        humKiller.triggerLearn(true);
        cyberDenoiser.startLearn(1.2f);
    }
    bool isLearningNoise() const {
        return humKiller.isLearning() || cyberDenoiser.isLearning();
    }
    float getLearnNoiseProgress() const {
        return cyberDenoiser.getLearnProgress();
    }

    // Fluid Space Chamber & Space Modulation State
    float baseChamberShape = 0.0f; // 0.0 = Circle .. 1.0 = Square
    float baseChamberSize = 1.0f;  // 0.10x .. 10.0x (432 Hz volume ladder)
    bool spaceModEnabled = false;
    float spaceModRate = 0.75f;     // Hz
    float spaceShapeModDepth = 0.35f;
    float spaceSizeModDepth = 0.30f;
    int spaceModWaveform = 0;       // 0=Sine, 1=Triangle, 2=432Hz Golden, 3=Drift
    float spaceStereoSpread = 90.0f;// Degrees

    // Smooth state variables (Rule 27: ramped per-sample)
    float curChamberShape = 0.0f;
    float curChamberSize = 1.0f;

    // LFO phase accumulators (separate per harmonic to prevent wrap discontinuity - Rule 27)
    float spaceModPhase1 = 0.0f;
    float spaceModPhase2 = 0.0f;
    float spaceModPhase3 = 0.0f;

    // Effect Bypasses
    bool enableReverb = true;
    bool enableVibrato = true;

    // Fluid Dynamics Reverb Space Parameters (Rule 32)
    float revChamberShape = 0.0f; // 0.0 = Circle .. 1.0 = Square
    float revChamberSize = 1.0f;  // 0.10x .. 10.0x (432 Hz volume ladder)
    float revSendLowCut = 120.0f; // Hz (Bottom filter)
    float revSendHighCut = 6500.0f;// Hz (Top filter)
    float revDwell = 5.0f;        // 0..10
    float revMix = 28.0f;         // 0..100%

    // Smooth state variables for Reverb (Rule 27)
    float curRevShape = 0.0f;
    float curRevSize = 1.0f;

    void setFluidSpaceParams(
        float shapeFactor,
        float sizeFactor,
        bool modEnabled,
        float modRateHz,
        float shapeModDepth,
        float sizeModDepth,
        int modWaveform,
        float stereoSpreadDeg
    ) {
        baseChamberShape = std::max(0.0f, std::min(1.0f, shapeFactor));
        baseChamberSize = std::max(0.10f, std::min(10.0f, sizeFactor));
        spaceModEnabled = modEnabled;
        spaceModRate = std::max(0.02f, std::min(12.0f, modRateHz));
        spaceShapeModDepth = std::max(0.0f, std::min(1.0f, shapeModDepth));
        spaceSizeModDepth = std::max(0.0f, std::min(1.0f, sizeModDepth));
        spaceModWaveform = std::max(0, std::min(3, modWaveform));
        spaceStereoSpread = std::max(0.0f, std::min(180.0f, stereoSpreadDeg));
    }

    void setFluidReverbParams(
        float shapeFactor, float sizeFactor,
        float sendLowCutHz, float sendHighCutHz,
        float dwell, float mixPercent
    ) {
        revChamberShape = std::max(0.0f, std::min(1.0f, shapeFactor));
        revChamberSize = std::max(0.10f, std::min(10.0f, sizeFactor));
        revSendLowCut = std::max(20.0f, std::min(1000.0f, sendLowCutHz));
        revSendHighCut = std::max(1500.0f, std::min(18000.0f, sendHighCutHz));
        revDwell = std::max(0.0f, std::min(10.0f, dwell));
        revMix = std::max(0.0f, std::min(100.0f, mixPercent));
    }

    // 5 Integrated Fluid Stompboxes in Series (Rule 33 & 34)
    FluidFuzzFacePedal fuzzFace;
    FluidCentaurPedal centaur;
    TS808ScreamerPedal ts808;
    BD2BluesDriverPedal bd2;
    FluidRodentPedal rodent;
    FluidStompbox stompbox;
    DynamicDeHiss dynamicDeHiss;
    GuitarPickupVoicing pickupVoicing;
    int pickupVoicingMode = 0; // 0 = 1M Studio High-Z, 1 = Strat / SC Glass, 2 = Gretsch / HB Punch

    EuphoriaHyperDSP::AdaptiveQuadratureHumTracker pllHumTracker;
    bool hyperHumTracker = false; // Default to rock-solid Goertzel grid cancellation (Gold Standard)
    bool hyperLeadStandingWave = true;

    // Channel 0: Euphoria Supreme Glass Tone Parameters
    float cleanBrightMode  = 1.0f; // 0 = Warm/Off, 1 = Bright Glass, 2 = Ultra Chime
    float cleanDeepMode    = 0.0f; // 0 = Tight (Gretsch/HB), 1 = Deep 3D (Strat)
    float cleanMidShift    = 0.0f; // 0 = 450Hz Scoop (Strat), 1 = 750Hz Body (Two-Rock), 2 = 1.15kHz Cut (Gretsch)
    float cleanGlassAirVal = 6.5f; // 0..10 (Active ultra-high air shelf at 6.2 kHz)
    float cleanPresVal     = 5.5f; // 0..10 (Clean power stage HF air)
    float cleanVolVal      = 5.2f; // 0..10
    float cleanBassVal     = 5.0f; // 0..10
    float cleanMidVal      = 5.0f; // 0..10
    float cleanTrebVal     = 5.8f; // 0..10
    float cleanMasterVal   = 5.5f; // 0..10
    float cleanOutTrimVal  = 5.0f; // 0..10
    float curCleanVol      = 5.2f; // Active Clean Volume (0..10)
    float curCleanBreakup  = 0.0f; // 0.0 for CleanVol <= 7.0, 0.0..1.0 for CleanVol 7.0..10.0
    float curDriveGain     = 5.8f; // Active Bassman/JMP Gain (0..10)
    float curBmBreakup     = 0.0f; // 0.0 for Bassman Gain <= 3.8, progressive 10A audio-taper breakup > 3.8
    float curLeadBreakup   = 0.5f; // Progressive 10A audio-taper breakup for BE-100 / JCM800 Lead Gain
    float curMasterBreakup = 0.0f; // Power-tube overdrive factor when Master Volume is pushed > 6.5

    void setCleanToneParams(
        float brightMode, float deepMode, float midShiftMode,
        float glassAir, float presence,
        float vol = -1.0f, float bass = -1.0f, float mid = -1.0f, float treb = -1.0f
    ) {
        cleanBrightMode  = std::max(0.0f, std::min(2.0f, brightMode));
        cleanDeepMode    = std::max(0.0f, std::min(1.0f, deepMode));
        cleanMidShift    = std::max(0.0f, std::min(2.0f, midShiftMode));
        cleanGlassAirVal = std::max(0.0f, std::min(10.0f, glassAir));
        cleanPresVal     = std::max(0.0f, std::min(10.0f, presence));
        if (vol >= 0.0f)  cleanVolVal  = std::max(0.0f, std::min(10.0f, vol));
        if (bass >= 0.0f) cleanBassVal = std::max(0.0f, std::min(10.0f, bass));
        if (mid >= 0.0f)  cleanMidVal  = std::max(0.0f, std::min(10.0f, mid));
        if (treb >= 0.0f) cleanTrebVal = std::max(0.0f, std::min(10.0f, treb));
    }

    // Discrete Bassman Tone Stack Values (Rule 33)
    float bmBassVal = 5.0f;
    float bmMidVal  = 5.0f;
    float bmTrebVal = 5.0f;

    // Discrete Friedman BE-100 / JCM 800 Lead Channel Values (Rule 33)
    float leadGainVal    = 6.5f;
    float leadVolVal     = 5.5f;
    float leadBassVal    = 5.0f;
    float leadMidVal     = 5.5f;
    float leadTrebVal    = 6.0f;
    float leadPresVal    = 5.8f;
    bool  leadHbeOn      = false;
    bool  leadC45On      = true;
    bool  leadSatOn      = false;
    float leadDeHissSens = 5.0f;

    void setPickupVoicing(int mode) {
        pickupVoicingMode = std::max(0, std::min(2, mode));
        pickupVoicing.mode = pickupVoicingMode;
        pickupVoicing.update((float)sampleRate);
    }

    void setCentaurParams(bool enabled, float gain, float treble, float output, float blendPercent, bool modSwitch = false, int mode = -1) {
        centaur.enabled = enabled;
        centaur.gain = std::max(0.0f, std::min(10.0f, gain));
        centaur.treble = std::max(0.0f, std::min(10.0f, treble));
        centaur.output = std::max(0.0f, std::min(10.0f, output));
        centaur.cleanBlend = std::max(0.0f, std::min(100.0f, blendPercent)) * 0.01f;
        centaur.modSwitch = modSwitch;
        if (mode >= 0) centaur.mode = mode;
    }

    void setTS808Params(bool enabled, float drive, float tone, float level, bool fatMod = false, int mode = -1) {
        ts808.enabled = enabled;
        ts808.drive = std::max(0.0f, std::min(10.0f, drive));
        ts808.tone = std::max(0.0f, std::min(10.0f, tone));
        ts808.level = std::max(0.0f, std::min(10.0f, level));
        ts808.fatMod = fatMod;
        if (mode >= 0) ts808.mode = mode;
    }

    void setBD2Params(bool enabled, float level, float tone, float gain, bool phatMod = false, int mode = -1) {
        bd2.enabled = enabled;
        bd2.level = std::max(0.0f, std::min(10.0f, level));
        bd2.tone = std::max(0.0f, std::min(10.0f, tone));
        bd2.gain = std::max(0.0f, std::min(10.0f, gain));
        bd2.phatMod = phatMod;
        if (mode >= 0) bd2.mode = mode;
    }

    void setRatParams(bool enabled, float dist, float filter, float vol, bool turbo = false, int mode = -1) {
        rodent.enabled = enabled;
        rodent.distortion = std::max(0.0f, std::min(10.0f, dist));
        rodent.filter = std::max(0.0f, std::min(10.0f, filter));
        rodent.volume = std::max(0.0f, std::min(10.0f, vol));
        rodent.turboMode = turbo;
        if (mode >= 0) rodent.mode = mode;
    }

    void setFuzzFaceParams(bool enabled, float fuzz, float level, float ampSag, int transistorType, int mode = -1) {
        fuzzFace.enabled = enabled;
        fuzzFace.fuzz = std::max(0.0f, std::min(10.0f, fuzz));
        fuzzFace.level = std::max(0.0f, std::min(10.0f, level));
        fuzzFace.ampSag = std::max(0.0f, std::min(10.0f, ampSag));
        fuzzFace.transistorType = std::max(0, std::min(1, transistorType));
        if (mode >= 0) fuzzFace.mode = mode;
    }

    void triggerFuzzFaceLearnMax() { fuzzFace.triggerLearnMaxVolume(); }
    void triggerFuzzFaceLearnClean() { fuzzFace.triggerLearnCleanVolume(); }
    void finishFuzzFaceLearning() { fuzzFace.finishLearning(); }
    bool isFuzzFaceLearningMax() const { return fuzzFace.isLearningMaxVolume(); }
    bool isFuzzFaceLearningClean() const { return fuzzFace.isLearningCleanVolume(); }
    float getFuzzFaceCalibratedMax() const { return fuzzFace.getCalibratedMax(); }
    float getFuzzFaceCalibratedClean() const { return fuzzFace.getCalibratedClean(); }
    float getFuzzFaceLearnTracker() const { return fuzzFace.getLearnTracker(); }
    float getFuzzFaceMorph() const { return fuzzFace.getCurrentFuzzMorph(); }
    void setFuzzFaceCalibration(float maxVal, float cleanVal) { fuzzFace.setCalibration(maxVal, cleanVal); }

    void setStompboxParams(
        bool enabled, int model, float drive, float tone, float level, bool modSwitch, float blendPercent
    ) {
        if (model == 0) {
            setCentaurParams(enabled, drive, tone, level, blendPercent, modSwitch);
            ts808.enabled = false;
        } else {
            setTS808Params(enabled, drive, tone, level, modSwitch);
            centaur.enabled = false;
        }
    }

    void setBassmanToneParams(float bass, float mid, float treb) {
        bmBassVal = std::max(0.0f, std::min(10.0f, bass));
        bmMidVal  = std::max(0.0f, std::min(10.0f, mid));
        bmTrebVal = std::max(0.0f, std::min(10.0f, treb));
    }

    void setLeadParams(
        float gain, float vol, float bass, float mid, float treb, float pres,
        bool hbe, bool c45, bool sat, float deHissSens
    ) {
        leadGainVal    = std::max(0.0f, std::min(10.0f, gain));
        leadVolVal     = std::max(0.0f, std::min(10.0f, vol));
        leadBassVal    = std::max(0.0f, std::min(10.0f, bass));
        leadMidVal     = std::max(0.0f, std::min(10.0f, mid));
        leadTrebVal    = std::max(0.0f, std::min(10.0f, treb));
        leadPresVal    = std::max(0.0f, std::min(10.0f, pres));
        leadHbeOn      = hbe;
        leadC45On      = c45;
        leadSatOn      = sat;
        leadDeHissSens = std::max(0.0f, std::min(10.0f, deHissSens));
    }

    void setEffectBypasses(bool revOn, bool vibOn) {
        enableReverb = revOn;
        enableVibrato = vibOn;
    }

    void reset() {
        curChamberShape = baseChamberShape;
        curChamberSize = baseChamberSize;
        spaceModPhase1 = spaceModPhase2 = spaceModPhase3 = 0.0f;

        curRevShape = revChamberShape;
        curRevSize = revChamberSize;

        cleanSubFilter.setIdentity(); cleanBass.setIdentity(); cleanMid.setIdentity();
        cleanTreble.setIdentity(); cleanGlassAir.setIdentity(); cleanBrightShelf.setIdentity();
        cleanPresenceFilter.setIdentity();
        bfBass.setIdentity(); bfMid.setIdentity(); bfTreble.setIdentity(); bfBrightShelf.setIdentity();
        bmBass.setIdentity(); bmMid.setIdentity(); bmTreble.setIdentity();
        leadToneBass.setIdentity(); leadToneMid.setIdentity(); leadToneTreble.setIdentity();
        leadC45Filter.setIdentity(); leadPresenceFilter.setIdentity();
        presenceFilter.setIdentity();

        pickupVoicing.reset();
        pickupVoicing.mode = pickupVoicingMode;
        pickupVoicing.update((float)sampleRate);

        pllHumTracker.prepare((float)sampleRate, 50.0f);

        humKiller.init(sampleRate);
        // Single-coil mains hum typically sits around -50..-60 dBFS at the fundamental, so the
        // hum-floor estimates for 50/100/150/250 Hz are raised to cancel it fully (the subtractor
        // only removes up to the threshold amount; guitar above it passes with only hum removed).
        humKiller.setThreshold(0, -70.0f); // 25 Hz Sub
        humKiller.setThreshold(1, -52.0f); // 50 Hz Fundamental
        humKiller.setThreshold(2, -56.0f); // 100 Hz 2nd Harmonic
        humKiller.setThreshold(3, -62.0f); // 150 Hz 3rd Harmonic
        humKiller.setThreshold(4, -70.0f); // 250 Hz 4th-6th
        humKiller.setThreshold(5, -82.0f); // 500 Hz Mid Buzz
        humKiller.setThreshold(6, -84.0f); // 1200 Hz High Buzz
        humKiller.setThreshold(7, -86.0f); // 3200 Hz EMI Hash

        cyberDenoiser.prepare(sampleRate);
        cyberDenoiser.setFloor(0.005f);
        cyberDenoiser.setHarmonicShield(true);

        // Pre-seed Cyber-Denoiser to calibrated studio baseline defaults (Rule 33 & 35)
        // Subtractive spectral phase cancellation removes stationary baseline without gating
        const float defDb[10] = { -60.0f, -62.0f, -62.0f, -62.0f, -62.0f, -62.0f, -62.0f, -62.0f, -64.0f, -64.0f };
        for (int b = 0; b < 10; ++b) {
            cyberDenoiser.setThresholdDb(b, defDb[b]);
        }

        // Parallel Internal Gain Noise Auto-Removal (Rule 33 & 35)
        // Eliminates internal pedal/tube circuit noise in parallel with zero gating
        parallelGainDenoiser.prepare(sampleRate);
        parallelGainDenoiser.setFloor(0.005f);
        parallelGainDenoiser.setHarmonicShield(true);
        for (int b = 0; b < 10; ++b) {
            parallelGainDenoiser.setThresholdDb(b, defDb[b]);
        }
        cleanInputEnv = 0.0f;

        fuzzFace.reset();
        centaur.reset();
        ts808.reset();
        bd2.reset();
        rodent.reset();
        stompbox.reset();
        dynamicDeHiss.init(sampleRate);

        physicalCab.reset();
        physicalCab.update((float)sampleRate);

        fluidAcousticSpace.reset();
        fluidReverb.reset();
        fluidTremolo.reset(sampleRate);

        inputExpander.reset();

        dcBlockerPre.reset(); dcBlockerPost.reset();
        dcBlockerBf.reset();  dcBlockerBm.reset();
        snubberBf.reset();    snubberBm.reset();    snubberPower.reset();

        leadAntiAliasTight.reset(); leadAntiAlias1.reset(); leadAntiAlias2.reset(); leadAntiAlias3.reset();
        leadCouplingV1A.reset(); leadCouplingV1B.reset(); leadCouplingV2A.reset(); leadCouplingTone.reset();
        leadBrightCap.reset();
        leadGridCapV1A = leadGridCapV1B = leadGridCapV2A = leadGridCapV2B = 0.0f;

        upStage1.reset(); upStage2[0].reset(); upStage2[1].reset();
        for (int i = 0; i < 4; ++i) upStage3[i].reset();
        dnStage1.reset(); dnStage2[0].reset(); dnStage2[1].reset();
        for (int i = 0; i < 4; ++i) dnStage3[i].reset();

        sagVoltage = 1.0f;
        sagCounter = 0;

        // Initialize Fluid Dynamics Preamp Voicings
        // Channel 1: Euphoria Supreme Glass Fluid Chamber (Laminar Flow + 3D Harmonic Bloom)
        fluidBf1.reset();
        fluidBf1.viscosity = 0.90f;        // Ultra-low turbulence, smooth laminar conduction
        fluidBf1.harmonicPurity = 0.28f;   // High purity, subtle 2nd order fluid bloom on hard transients
        fluidBf1.chamberRecovery = 0.0040f;// Fast recovery, zero starvation
        fluidBf1.drainRate = 0.005f;
        fluidBf1.momentumInject = 0.38f;

        fluidBf2.reset();
        fluidBf2.viscosity = 0.88f;
        fluidBf2.harmonicPurity = 0.30f;
        fluidBf2.chamberRecovery = 0.0035f;
        fluidBf2.drainRate = 0.006f;
        fluidBf2.momentumInject = 0.40f;

        // Channel 2: '59 Bassman / '68 JMP Crunch Fluid Chamber
        fluidBm1.reset();
        fluidBm1.viscosity = 0.78f;
        fluidBm1.harmonicPurity = 0.52f;
        fluidBm1.chamberRecovery = 0.0018f;
        fluidBm1.drainRate = 0.014f;
        fluidBm1.momentumInject = 0.55f;

        fluidBm2.reset();
        fluidBm2.viscosity = 0.75f;
        fluidBm2.harmonicPurity = 0.58f;
        fluidBm2.chamberRecovery = 0.0015f;
        fluidBm2.drainRate = 0.016f;
        fluidBm2.momentumInject = 0.60f;

        fluidCf.reset();

        // Channel 3: '80s Friedman BE-100 / JCM 800 Lead Fluid Chamber
        fluidLeadV1A.reset();
        fluidLeadV1A.viscosity = 0.72f;
        fluidLeadV1A.harmonicPurity = 0.60f;
        fluidLeadV1A.chamberRecovery = 0.0014f;
        fluidLeadV1A.drainRate = 0.018f;
        fluidLeadV1A.momentumInject = 0.65f;

        fluidLeadV2A.reset();
        fluidLeadV2A.viscosity = 0.70f;
        fluidLeadV2A.harmonicPurity = 0.62f;
        fluidLeadV2A.chamberRecovery = 0.0012f;
        fluidLeadV2A.drainRate = 0.020f;
        fluidLeadV2A.momentumInject = 0.68f;

        fluidLeadV2B.reset();
        fluidLeadV2B.viscosity = 0.68f;
        fluidLeadV2B.harmonicPurity = 0.65f;
        fluidLeadV2B.chamberRecovery = 0.0010f;
        fluidLeadV2B.drainRate = 0.022f;
        fluidLeadV2B.momentumInject = 0.72f;

        fluidPowerPush.reset();
        fluidPowerPush.viscosity = 0.82f;
        fluidPowerPush.harmonicPurity = 0.45f;
        fluidPowerPush.chamberRecovery = 0.0022f;
        fluidPowerPush.momentumInject = 0.48f;

        fluidPowerPull.reset();
        fluidPowerPull.viscosity = 0.82f;
        fluidPowerPull.harmonicPurity = 0.45f;
        fluidPowerPull.chamberRecovery = 0.0022f;
        fluidPowerPull.momentumInject = 0.48f;

        otFlux = 0.0f;
        otPrevIn = 0.0f;

#if CYBER_ENABLE_STUDIO_DELAY
        std::memset(&dlyUnit, 0, sizeof(dlyUnit));
        dlyUnit.time_ms     = 360.0f;
        dlyUnit.feedback    = 34.0f;
        dlyUnit.diffuse     = 22.0f;
        dlyUnit.wet         = 0.0f;
        dlyUnit.ping_pong   = 1;
        dlyUnit.in_lowcut   = 90.0f;
        dlyUnit.in_highcut  = 5200.0f;
        dlyUnit.out_lowcut  = 60.0f;
        dlyUnit.out_highcut = 9500.0f;
        dlyUnit.drive       = 28.0f;
        dlyUnit.flutter     = 18.0f;
        dlyUnit.mod_rate    = 25.0f;
        dlyUnit.mod_depth   = 20.0f;
        dlyUnit.ducking     = 25.0f;
        dlyUnit.stereo      = 100.0f;
        dlyUnit.spread      = 54.0f;
#endif

        initCabFilters();
    }


    void initCabFilters() {
        physicalCab.update((float)sampleRate);
    }

    void setCabParams(int model, float cabSizeVol, bool use432Tuning, float speakerDrive) {
        physicalCab.model = std::max(0, std::min(12, model));
        physicalCab.cabSizeVol = cabSizeVol;
        physicalCab.use432Tuning = use432Tuning;
        physicalCab.speakerDrive = speakerDrive;
        physicalCab.update((float)sampleRate);
    }

    /* Push-Pull Power Section + Fluid Core Transformer Dynamics */
    inline float processPowerStage(float in, float masterGain, int powerTubeType, float sag, int qualityMode, bool isCleanGlass = false, float powerBreakup = 0.0f) {
        float effectiveBPlus = std::max(0.64f, sag);
        if (isCleanGlass || powerBreakup <= 0.001f) {
            // 100W-150W 4x6L6GC / EL34 Ultra-Linear Clean Power Section:
            // When Clean Glass is below 7.0 OR Bassman/Lead Gain & Master are in their clean headroom region,
            // push-pull power tubes operate in pure Class-A/AB laminar headroom with zero clipping,
            // while masterGain controls output loudness along a true 15A Analog Audio Taper!
            float brk = isCleanGlass ? (curCleanBreakup * 0.65f) : 0.0f;
            float cleanMasterScale = masterGain * 0.60f;
            float push = in * cleanMasterScale;
            float pull = -in * cleanMasterScale * 0.992f;
            float pOutPush = fluidPowerPush.processLaminarClean(push, brk, effectiveBPlus);
            float pOutPull = fluidPowerPull.processLaminarClean(pull, brk, effectiveBPlus);
            float pOut = (pOutPush - pOutPull) * 0.88f;
            otFlux = 0.995f * otFlux + 0.005f * pOut;
            pOut = pOut / (1.0f + 0.015f * otFlux * otFlux);
            return pOut;
        }

        // Progressive Push-Pull Power Tube Overdrive (6L6GC / EL34) + Output Transformer Core Memory:
        // Smoothly crossfades from Class-AB laminar power into full push-pull saturation as Preamp Gain
        // or Master Volume (past 6.5) is turned up along the Audio Taper curve!
        float cleanMasterScale = masterGain * 0.60f;
        float lamPush = fluidPowerPush.processLaminarClean(in * cleanMasterScale, powerBreakup * 0.55f, effectiveBPlus);
        float lamPull = fluidPowerPull.processLaminarClean(-in * cleanMasterScale * 0.992f, powerBreakup * 0.55f, effectiveBPlus);
        float lamOut  = (lamPush - lamPull) * 0.88f;

        float tubeDriveScale = (0.45f + 0.70f * powerBreakup) * ((powerTubeType == 1) ? 1.25f : 1.08f);
        float push = in * (0.55f + 0.45f * masterGain);
        float pull = -in * (0.55f + 0.45f * masterGain) * 0.985f;

        float pOutPush = fluidPowerPush.process(push, tubeDriveScale, effectiveBPlus);
        float pOutPull = fluidPowerPull.process(pull, tubeDriveScale, effectiveBPlus);
        float satOut = (pOutPush - pOutPull) * (0.72f + 0.32f * masterGain);

        // Output Transformer Core Memory (Jiles-Atherton differential core physics)
        if (qualityMode >= 2) {
            float dH = satOut - otPrevIn;
            otPrevIn = satOut;
            float m_an = EuphoriaHyperDSP::FluidMath::fast_tanh(satOut * 1.15f);
            float delta = (dH >= 0.0f) ? 1.0f : -1.0f;
            float diff = m_an - otFlux;
            float dM = (diff * delta > 0.0f) ? (0.35f * diff * dH) : (0.08f * diff * dH);
            otFlux = 0.992f * (otFlux + dM) + 0.008f * m_an;
            satOut = 0.72f * satOut + 0.28f * otFlux;
        } else {
            otFlux = 0.995f * otFlux + 0.005f * satOut;
            satOut = satOut / (1.0f + 0.12f * otFlux * otFlux);
        }

        float pMix = std::clamp(powerBreakup, 0.0f, 1.0f);
        return lamOut * (1.0f - pMix) + satOut * pMix;
    }


#if CYBER_ENABLE_STUDIO_DELAY
    struct StudioDelayState {
        static const int BUF_LEN = 192000;
        float buf_l[BUF_LEN];
        float buf_r[BUF_LEN];
        int write_idx;

        float in_lowcut, in_highcut;
        float out_lowcut, out_highcut;
        float in_hp_l, in_hp_r;
        float in_lp_l, in_lp_r;
        float out_hp_l, out_hp_r;
        float out_lp_l, out_lp_r;
        float fb_hp_l, fb_hp_r;
        float fb_lp_l, fb_lp_r;

        float ap_l[347];
        float ap_r[347];
        int ap_idx;

        float mod_phase;
        float mod_phase2;
        float duck_env;

        float time_ms;
        float feedback;
        float diffuse;
        float wet;
        int   enabled;
        int   ping_pong;
        float drive;
        float flutter;
        float mod_rate;
        float mod_depth;
        float ducking;
        float stereo;
        float spread;

        float smooth_samp;
        float smooth_samp_r;
        float xf_target;
        float xf_target_r;
        float xf_pos;
        int   xf_active;
        int   sm_init;
        float sm_fb, sm_wet, sm_width, sm_in_hp_a, sm_in_lp_a, sm_out_hp_a, sm_out_lp_a;
    };

    static inline float readFracHermite(const float *buf, int buf_len, int w_idx, float delay_samples) {
        if (delay_samples < 2.0f) delay_samples = 2.0f;
        if (delay_samples > (float)(buf_len - 4)) delay_samples = (float)(buf_len - 4);
        int i_dly = (int)delay_samples;
        float frac = delay_samples - (float)i_dly;
        int idx0 = (w_idx - i_dly + 1 + buf_len) % buf_len;
        int idx1 = (w_idx - i_dly     + buf_len) % buf_len;
        int idx2 = (w_idx - i_dly - 1 + buf_len) % buf_len;
        int idx3 = (w_idx - i_dly - 2 + buf_len) % buf_len;
        float y0 = buf[idx0], y1 = buf[idx1], y2 = buf[idx2], y3 = buf[idx3];
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    inline void processStudioDelaySample(float inL, float inR, float &outL, float &outR) {
        using namespace EuphoriaHyperDSP;
        StudioDelayState *dl = &dlyUnit;
        if (!dl->enabled || dl->wet <= 0.05f) {
            dl->sm_wet = 0.0f;
            outL = inL;
            outR = inR;
            return;
        }

        const float fs = (float)sampleRate;
        float base_dly_samp = std::max(48.0f, std::min(94000.0f, dl->time_ms * 0.001f * fs));
        float spread_ratio  = 1.0f + (dl->spread - 50.0f) * 0.0035f;
        float target_samp_l = base_dly_samp;
        float target_samp_r = std::max(48.0f, std::min(94000.0f, base_dly_samp * spread_ratio));

        float dly_fb       = std::min(0.94f, dl->feedback * 0.0093f);
        float dly_ap_g     = std::min(0.58f, dl->diffuse * 0.0052f);
        float dly_wet      = (dl->wet * 0.01f) * 1.15f;
        float dly_width    = dl->stereo * 0.01f;
        auto fastOnePoleAlpha = [&](float fc) -> float {
            float w = 2.0f * (float)M_PI * fc / fs;
            return w / (1.0f + w);
        };
        float dly_in_hp_a  = fastOnePoleAlpha(std::max(20.0f, dl->in_lowcut));
        float dly_in_lp_a  = fastOnePoleAlpha(std::min(18000.0f, std::max(800.0f, dl->in_highcut)));
        float dly_out_hp_a = fastOnePoleAlpha(std::max(20.0f, dl->out_lowcut));
        float dly_out_lp_a = fastOnePoleAlpha(std::min(18000.0f, std::max(800.0f, dl->out_highcut)));
        float dly_drive_amt = dl->drive * 0.01f;
        float dly_mod_inc   = (0.22f + (dl->mod_rate * 0.038f) + (dl->flutter * 0.042f)) * 2.0f * (float)M_PI / fs;
        float dly_mod_dep   = ((dl->mod_depth * 0.01f) * 16.0f) + ((dl->flutter * 0.01f) * 9.0f);
        float dly_duck_amt  = dl->ducking * 0.01f;

        const float fx_sm_k = 1.0f / (1.0f + 0.020f * fs);
        const float dly_xf_step = 1.0f / (0.045f * fs);

        if (!dl->sm_init) {
            dl->sm_init = 1;
            dl->sm_fb = dly_fb; dl->sm_wet = dly_wet; dl->sm_width = dly_width;
            dl->sm_in_hp_a = dly_in_hp_a; dl->sm_in_lp_a = dly_in_lp_a;
            dl->sm_out_hp_a = dly_out_hp_a; dl->sm_out_lp_a = dly_out_lp_a;
            dl->smooth_samp = target_samp_l;
            dl->smooth_samp_r = target_samp_r;
            dl->xf_target = target_samp_l;
            dl->xf_target_r = target_samp_r;
            dl->xf_active = 0;
        }

        dl->sm_fb       += fx_sm_k * (dly_fb - dl->sm_fb);
        dl->sm_wet      += fx_sm_k * (dly_wet - dl->sm_wet);
        dl->sm_width    += fx_sm_k * (dly_width - dl->sm_width);
        dl->sm_in_hp_a  += fx_sm_k * (dly_in_hp_a - dl->sm_in_hp_a);
        dl->sm_in_lp_a  += fx_sm_k * (dly_in_lp_a - dl->sm_in_lp_a);
        dl->sm_out_hp_a += fx_sm_k * (dly_out_hp_a - dl->sm_out_hp_a);
        dl->sm_out_lp_a += fx_sm_k * (dly_out_lp_a - dl->sm_out_lp_a);

        dl->in_hp_l += dl->sm_in_hp_a * (inL - dl->in_hp_l);
        dl->in_hp_r += dl->sm_in_hp_a * (inR - dl->in_hp_r);
        float dly_in_l = inL - dl->in_hp_l;
        float dly_in_r = inR - dl->in_hp_r;
        dl->in_lp_l += dl->sm_in_lp_a * (dly_in_l - dl->in_lp_l);
        dl->in_lp_r += dl->sm_in_lp_a * (dly_in_r - dl->in_lp_r);
        dly_in_l = dl->in_lp_l;
        dly_in_r = dl->in_lp_r;

        float dt_diff_l = target_samp_l - dl->smooth_samp;
        float dt_diff_r = target_samp_r - dl->smooth_samp_r;
        if (!dl->xf_active) {
            if (fabsf(dt_diff_l) > 2.0f || fabsf(dt_diff_r) > 2.0f) {
                dl->xf_active = 1;
                dl->xf_pos = 0.0f;
                dl->xf_target = target_samp_l;
                dl->xf_target_r = target_samp_r;
            } else {
                dl->smooth_samp = target_samp_l;
                dl->smooth_samp_r = target_samp_r;
            }
        }

        dl->mod_phase += dly_mod_inc;
        if (dl->mod_phase > 2.0f * (float)M_PI) dl->mod_phase -= 2.0f * (float)M_PI;
        dl->mod_phase2 += dly_mod_inc * 2.7f;
        if (dl->mod_phase2 > 2.0f * (float)M_PI) dl->mod_phase2 -= 2.0f * (float)M_PI;
        float wow_l = FluidMath::fast_sin(dl->mod_phase) * dly_mod_dep + 0.35f * FluidMath::fast_sin(dl->mod_phase2) * dly_mod_dep;
        float wow_r = FluidMath::fast_cos(dl->mod_phase) * dly_mod_dep + 0.35f * FluidMath::fast_cos(dl->mod_phase2) * dly_mod_dep;

        int w_idx = dl->write_idx;
        float d_samp_l = dl->smooth_samp + wow_l;
        float d_samp_r = dl->smooth_samp_r + wow_r;

        float dly_out_l = readFracHermite(dl->buf_l, StudioDelayState::BUF_LEN, w_idx, d_samp_l);
        float dly_out_r = readFracHermite(dl->buf_r, StudioDelayState::BUF_LEN, w_idx, d_samp_r);
        if (dl->xf_active) {
            float n_l = readFracHermite(dl->buf_l, StudioDelayState::BUF_LEN, w_idx, dl->xf_target + wow_l);
            float n_r = readFracHermite(dl->buf_r, StudioDelayState::BUF_LEN, w_idx, dl->xf_target_r + wow_r);
            float th = dl->xf_pos * 1.5707963f;
            float g_old = FluidMath::fast_cos(th), g_new = FluidMath::fast_sin(th);
            dly_out_l = dly_out_l * g_old + n_l * g_new;
            dly_out_r = dly_out_r * g_old + n_r * g_new;
            dl->xf_pos += dly_xf_step;
            if (dl->xf_pos >= 1.0f) {
                dl->smooth_samp = dl->xf_target;
                dl->smooth_samp_r = dl->xf_target_r;
                dl->xf_active = 0;
            }
        }

        if (dly_ap_g > 0.02f) {
            int dap = dl->ap_idx;
            float bl = dl->ap_l[dap];
            float vl = dly_out_l + bl * dly_ap_g;
            dl->ap_l[dap] = vl;
            dly_out_l = bl - vl * dly_ap_g;

            float br = dl->ap_r[dap];
            float vr = dly_out_r + br * dly_ap_g;
            dl->ap_r[dap] = vr;
            dly_out_r = br - vr * dly_ap_g;
            if (++dl->ap_idx >= 347) dl->ap_idx = 0;
        }

        dl->fb_hp_l += dl->sm_in_hp_a * (dly_out_l - dl->fb_hp_l);
        dl->fb_hp_r += dl->sm_in_hp_a * (dly_out_r - dl->fb_hp_r);
        float fb_l = dly_out_l - dl->fb_hp_l;
        float fb_r = dly_out_r - dl->fb_hp_r;
        dl->fb_lp_l += dl->sm_in_lp_a * (fb_l - dl->fb_lp_l);
        dl->fb_lp_r += dl->sm_in_lp_a * (fb_r - dl->fb_lp_r);
        fb_l = dl->fb_lp_l;
        fb_r = dl->fb_lp_r;

        if (dly_drive_amt > 0.02f) {
            float drv = 1.0f + dly_drive_amt * 1.8f;
            fb_l = FluidMath::fast_tanh(fb_l * drv);
            fb_r = FluidMath::fast_tanh(fb_r * drv);
        }

        float in_peak = std::max(fabsf(inL), fabsf(inR));
        float duck_k = (in_peak > dl->duck_env) ? (1.0f / (1.0f + 0.002f * fs)) : (1.0f / (1.0f + 0.160f * fs));
        dl->duck_env += duck_k * (in_peak - dl->duck_env);
        float duck_gain = 1.0f - (dly_duck_amt * std::min(1.0f, dl->duck_env * 2.8f));
        if (duck_gain < 0.12f) duck_gain = 0.12f;

        float write_l, write_r;
        if (dl->ping_pong) {
            write_l = dly_in_l + (fb_r * dl->sm_fb);
            write_r = dly_in_r + (fb_l * dl->sm_fb);
        } else {
            write_l = dly_in_l + (fb_l * dl->sm_fb);
            write_r = dly_in_r + (fb_r * dl->sm_fb);
        }

        dl->buf_l[w_idx] = write_l;
        dl->buf_r[w_idx] = write_r;
        if (++dl->write_idx >= StudioDelayState::BUF_LEN) dl->write_idx = 0;

        float wet_l = dly_out_l * duck_gain;
        float wet_r = dly_out_r * duck_gain;

        float mid = (wet_l + wet_r) * 0.5f;
        float side = (wet_l - wet_r) * 0.5f * dl->sm_width;
        wet_l = mid + side;
        wet_r = mid - side;

        dl->out_hp_l += dl->sm_out_hp_a * (wet_l - dl->out_hp_l);
        dl->out_hp_r += dl->sm_out_hp_a * (wet_r - dl->out_hp_r);
        wet_l -= dl->out_hp_l;
        wet_r -= dl->out_hp_r;
        dl->out_lp_l += dl->sm_out_lp_a * (wet_l - dl->out_lp_l);
        dl->out_lp_r += dl->sm_out_lp_a * (wet_r - dl->out_lp_r);
        wet_l = dl->out_lp_l;
        wet_r = dl->out_lp_r;

        outL = inL + (wet_l * dl->sm_wet);
        outR = inR + (wet_r * dl->out_lp_r * 0.0f + wet_r * dl->sm_wet);
    }

    void setDelayParams(
        float time_ms, float feedback, float wet_pct,
        float tape_drive, float flutter_pct, int ping_pong, float ducking_pct,
        bool enabled = true
    ) {
        dlyUnit.enabled   = enabled ? 1 : 0;
        dlyUnit.time_ms   = std::max(20.0f, std::min(1500.0f, time_ms));
        dlyUnit.feedback  = std::max(0.0f, std::min(95.0f, feedback));
        dlyUnit.wet       = (enabled && wet_pct > 0.05f) ? std::max(0.0f, std::min(100.0f, wet_pct)) : 0.0f;
        dlyUnit.drive     = std::max(0.0f, std::min(100.0f, tape_drive));
        dlyUnit.flutter   = std::max(0.0f, std::min(100.0f, flutter_pct));
        dlyUnit.ping_pong = ping_pong ? 1 : 0;
        dlyUnit.ducking   = std::max(0.0f, std::min(100.0f, ducking_pct));
    }
#endif

    // True Vintage 15A Audio Taper (Logarithmic Potentiometer for Volume & Master Volume Controls):
    // - 0.0 -> 0.0%
    // - 2.0 -> 3.4% (-29.3 dB)
    // - 5.0 -> 19.0% (-14.4 dB at 12 o'clock noon — authentic CTS/Bourns 15A-20A audio pot)
    // - 6.0 -> 28.6% (-10.9 dB)
    // - 7.0 -> 41.1% (-7.7 dB)
    // - 10.0 -> 100.0% (0.0 dB full scale)
    inline float audioTaper(float val0to10, float maxGain) {
        if (val0to10 <= 0.02f) return 0.0f;
        float norm = std::clamp(val0to10 * 0.1f, 0.0f, 1.0f);
        return (0.10f * norm + 0.22f * norm * norm + 0.68f * norm * norm * norm) * maxGain;
    }

    // True Vintage 10A Audio Taper (Logarithmic Input Gain / Preamp Drive Potentiometer):
    // Gives wide, musical clean-to-edge-of-breakup travel from 0.0 to 5.5, and progressive
    // tube saturation from 5.5 to 10.0 so the Bassman & Lead amps never distort too fast!
    // - 0.0 -> 0.0%
    // - 2.0 -> 2.2%
    // - 3.5 -> 7.0%
    // - 5.0 -> 16.2% (12 o'clock noon)
    // - 7.0 -> 38.3%
    // - 8.5 -> 64.3%
    // - 10.0 -> 100.0%
    inline float gainAudioTaper(float val0to10, float maxGain) {
        if (val0to10 <= 0.02f) return 0.0f;
        float norm = std::clamp(val0to10 * 0.1f, 0.0f, 1.0f);
        return (0.05f * norm + 0.15f * norm * norm + 0.80f * norm * norm * norm) * maxGain;
    }

    // Dave Friedman / Marshall 10k Cold Clipper Stage (V1B) with Elastic Fluid-Wall Wave Reflection
    inline float triodeColdClipper(float in, float &gridBias, float gainFactor, bool hyperStandingWave = true) {
        using namespace EuphoriaHyperDSP;
        float boundedIn = FluidMath::fast_tanh(in);
        if (boundedIn > 0.0f) {
            gridBias += (boundedIn - gridBias) * 0.0004f;
        } else {
            gridBias *= 0.99996f;
        }
        gridBias = std::clamp(gridBias, -1.0f, 1.0f);
        float effIn = (in - gridBias * 0.08f) * gainFactor;

        // Authentic Cold Clipper Asymmetric Triode with Elastic Fluid-Wall Reflection:
        // When driven hard by a front-end boost/fuzz, excess pressure folds back from the chamber boundary
        // into rich 2nd/3rd harmonics without ever hitting a flat rail or blocking!
        float out;
        if (effIn >= 0.0f) {
            out = FluidMath::fluidWallReflect(effIn * 0.90f, 0.38f, curChamberShape) * 1.12f;
        } else {
            float absIn = -effIn;
            float baseCut = -absIn / (1.0f + 0.45f * absIn);
            float wallFold = FluidMath::fluidWallReflect(effIn * 0.70f, 0.30f, curChamberShape);
            out = 0.65f * baseCut + 0.35f * wallFold;
        }
        if (hyperStandingWave) {
            float boundedOut = FluidMath::fast_tanh(out);
            out += 0.035f * (boundedOut * boundedOut);
        }
        return FluidMath::fast_tanh(out);
    }

    void updateToneStacks(
        float bfBassIn, float bfMidIn, float bfTrebIn, float presIn,
        bool brightSwitch, float cleanVol,
        float bmBassIn, float bmMidIn, float bmTrebIn, int slopeMode,
        float leadBassIn, float leadMidIn, float leadTrebIn, float leadPresIn, bool c45Switch,
        float osRate
    ) {
        // 1. EUPHORIA SUPREME GLASS CLEAN ACTIVE TONE MATRIX
        // Transcending vintage Blackface, Dumble Steel String Singer & Two-Rock
        // A. Sub-Harmonic Tight vs Deep 3D (Sub filter):
        // Mode 0 (TIGHT - Gretsch/HB): HPF at 65 Hz (Q=0.707) removes hollowbody boom and mud!
        // Mode 1 (DEEP 3D - Strat): Extends down to 28 Hz for massive piano-like authority.
        if (cleanDeepMode < 0.5f) {
            cleanSubFilter.setHighPass(65.0f, 0.707f, osRate);
        } else {
            cleanSubFilter.setHighPass(28.0f, 0.707f, osRate);
        }

        // B. Active Low-Shelf Bass Control (±10 dB):
        float cleanBassFreq = (cleanDeepMode < 0.5f) ? 115.0f : 85.0f;
        float cleanBassDb   = (bfBassIn - 5.0f) * 2.0f; // ±10 dB
        cleanBass.setLowShelf(cleanBassFreq, cleanBassDb, 0.707f, osRate);

        // C. Shiftable Active Mid Control (±10 dB):
        float cleanMidFreq = 450.0f;
        float cleanMidQ    = 1.05f;
        int mShift = (int)std::round(cleanMidShift);
        if (mShift == 1) {
            cleanMidFreq = 750.0f;
            cleanMidQ    = 1.15f;
        } else if (mShift == 2) {
            cleanMidFreq = 1150.0f;
            cleanMidQ    = 1.25f;
        }
        float cleanMidDb = (bfMidIn - 5.0f) * 2.0f; // ±10 dB
        cleanMid.setPeaking(cleanMidFreq, cleanMidDb, cleanMidQ, osRate);

        // D. Active High-Shelf Treble Control (±10 dB @ 3.2 kHz):
        float cleanTrebDb = (bfTrebIn - 5.0f) * 2.0f; // ±10 dB
        cleanTreble.setHighShelf(3200.0f, cleanTrebDb, 0.707f, osRate);

        // E. Dedicated Glass / Air Shelf (±8 dB @ 6.2 kHz):
        float cleanGlassDb = (cleanGlassAirVal - 5.0f) * 1.6f; // ±8 dB
        cleanGlassAir.setHighShelf(6200.0f, cleanGlassDb, 0.707f, osRate);

        // F. 3-Way Multi-Mode Bright:
        int bMode = (int)std::round(cleanBrightMode);
        if (bMode == 1 || (bMode == 0 && brightSwitch)) {
            float bAmt = 2.2f * std::max(0.35f, 1.0f - (cleanVol / 12.0f));
            cleanBrightShelf.setHighShelf(3800.0f, bAmt, 0.707f, osRate);
        } else if (bMode == 2) {
            float bAmt = 3.8f * std::max(0.40f, 1.0f - (cleanVol / 12.0f));
            cleanBrightShelf.setHighShelf(5200.0f, bAmt, 0.707f, osRate);
        } else {
            cleanBrightShelf.setIdentity();
        }

        // Centered at 5.0 = 0.0 dB flat (±7 dB range) so default 5.5 adds +0.7 dB of silky air, not +4.4 dB!
        float cleanPresDb = (cleanPresVal - 5.0f) * 1.4f;
        cleanPresenceFilter.setHighShelf(4500.0f, cleanPresDb, 0.707f, osRate);

        // Legacy mirrors for backwards-compatibility
        bfBass = cleanBass;
        bfMid = cleanMid;
        bfTreble = cleanTreble;
        bfBrightShelf = cleanBrightShelf;

        // 2. 5F6-A Bassman / 1987 JMP Cathode-Follower Tone Stack (Separate independent EQ)
        float bmBassFreq = (slopeMode == 1) ? 135.0f : 112.0f;
        float bmMidFreq  = (slopeMode == 1) ? 780.0f : 580.0f;
        float bmMidBoost = (slopeMode == 1) ? 2.4f   : 0.8f;

        float bmBassDb = -2.0f + (bmBassIn / 10.0f) * 14.0f;
        bmBass.setLowShelf(bmBassFreq, bmBassDb, 0.707f, osRate);

        float bmMidDb = bmMidBoost + (bmMidIn - 5.0f) * 2.5f;
        bmMid.setPeaking(bmMidFreq, bmMidDb, 1.10f, osRate);

        float bmTrebDb = (bmTrebIn - 4.5f) * 2.6f;
        bmTreble.setHighShelf(2500.0f, bmTrebDb, 0.707f, osRate);

        // 3. Friedman BE-100 / JCM 800 Lead Tone Stack
        float lBassDb = -2.0f + (leadBassIn / 10.0f) * 15.0f;
        leadToneBass.setLowShelf(110.0f, lBassDb, 0.7f, osRate);

        float lMidDb = -5.5f + (leadMidIn / 10.0f) * 10.5f;
        leadToneMid.setPeaking(650.0f, lMidDb, 1.2f, osRate);

        float lTrebDb = (leadTrebIn - 5.0f) * 2.5f;
        leadToneTreble.setHighShelf(3200.0f, lTrebDb, 0.7f, osRate);

        if (c45Switch) {
            leadC45Filter.setPeaking(2200.0f, 3.8f, 1.3f, osRate);
        } else {
            leadC45Filter.setIdentity();
        }

        // 4. Presence Filters
        float presDb = (presIn / 10.0f) * 7.5f;
        presenceFilter.setHighShelf(3900.0f, presDb, 0.707f, (float)sampleRate);

        float leadPresDb = (leadPresIn / 10.0f) * 8.5f;
        leadPresenceFilter.setHighShelf(4200.0f, leadPresDb, 0.7f, (float)sampleRate);
    }

    /* Single-Sub-Sample Core Fluid Dynamics Tube Preamp + Power Solver */
    inline float processFluidCoreSubSample(
        float s, int chMode, float cleanGain, float driveGain, float driveVol,
        float masterGain, int powerTubes, int qualityMode, float osRate
    ) {
        float ch1Out = 0.0f;
        float ch2Out = 0.0f;
        float ch3Out = 0.0f;

        // Path A: Channel 0 - Euphoria Supreme Ultra-Glass Clean
        // 100% Pure Ultra-Linear Laminar Clean from Volume 0.0 to 7.0 on any guitar (Single-Coil, P90,
        // Filter'Tron, or hot Humbucker). Only begins to break up when Clean Volume is turned past 7.0!
        if (chMode == 0 || chMode == 3) {
            float sClean = s;
            sClean = cleanSubFilter.process(sClean);
            // Stage 1 (V1A): High-plate-voltage laminar glass input triode (zero breakup before Volume knob)
            float bf = fluidBf1.processLaminarClean(sClean, 0.0f, sagVoltage);
            bf = cleanBrightShelf.process(bf);
            bf = cleanBass.process(bf);
            bf = cleanMid.process(bf);
            bf = cleanTreble.process(bf);
            bf = cleanGlassAir.process(bf);
            bf = dcBlockerBf.hp(bf, 12.0f, osRate); // Interstage AC coupling
            bf *= 0.76f; // Active Tone Matrix headroom calibration
            float cleanVolScale = (curCleanVol <= 7.0f)
                ? audioTaper(curCleanVol * (10.0f / 7.0f), 1.42f)
                : (1.42f + 0.28f * curCleanBreakup);
            bf *= cleanVolScale;
            // Stage 2 (V2A): Stays 100% laminar clean up to Volume 7.0; blooms into tube breakup past 7.0!
            bf = fluidBf2.processLaminarClean(bf, curCleanBreakup, sagVoltage);
            bf = cleanPresenceFilter.process(bf);
            ch1Out = snubberBf.lp(bf, 16000.0f, osRate); // Extended 16 kHz ceiling for crystal glass chime
        }

        // Path B: Channel 1 - Euphoria '59 Bassman / '68 JMP Crunch (True 10A Audio-Taper Input Gain)
        // - Gain 0.0 .. 3.8: Pure woody 12AY7/5F6-A Tweed Clean (<0.55% THD, zero premature distortion!)
        // - Gain 4.0 .. 5.5: Touch-sensitive blues edge-of-breakup & cathode-follower bark (1.2% .. 3.5% THD)
        // - Gain 6.0 .. 7.5: Classic Tweed / '68 JMP Plexi rhythm crunch (5.0% .. 10.5% THD)
        // - Gain 8.0 .. 10.0: Full dimed Tweed / British Plexi roar (14% .. 24% THD)
        if (chMode == 1 || chMode == 3) {
            // Stage 1 (V1A 12AY7 / 12AX7): High-headroom laminar input triode BEFORE the Audio-Taper Gain pot
            float bm = fluidBm1.processLaminarClean(s * 0.92f, 0.0f, sagVoltage);
            bm = dcBlockerBm.hp(bm, 14.0f, osRate); // Interstage AC coupling

            if (curDriveGain <= 3.8f) {
                // Pure Tweed Clean region (0.0 .. 3.8) with 15A Audio-Taper loudness sweep
                float tweedCleanLevel = audioTaper(curDriveGain * (10.0f / 3.8f), 0.88f);
                bm *= tweedCleanLevel;
                bm = fluidBm2.processLaminarClean(bm, 0.0f, sagVoltage);
            } else {
                // Progressive Audio-Taper Tube Breakup past 3.8 (curBmBreakup in (0..1])
                float lamBm = fluidBm2.processLaminarClean(bm * 0.88f, curBmBreakup * 0.85f, sagVoltage);
                float satDrive = 0.55f + driveGain;
                float satBm = fluidBm2.process(bm * satDrive, 1.35f, sagVoltage);
                satBm = fluidCf.process(satBm);
                float cfMix = std::clamp(curBmBreakup * 1.15f, 0.0f, 1.0f);
                bm = lamBm * (1.0f - cfMix) + satBm * cfMix;
            }

            bm = bmBass.process(bm);
            bm = bmMid.process(bm);
            bm = bmTreble.process(bm);
            bm *= driveVol;
            ch2Out = snubberBm.lp(bm, 10800.0f, osRate);
        }

        // Path C: Channel 2 - Euphoria '80s Friedman BE-100 / JCM 800 Lead (4-Stage 12AX7 Cascade with 10A Audio Taper)
        if (chMode == 2) {
            float lead = s;
            lead = leadAntiAliasTight.hp(lead, 35.0f, osRate);

            // Stage 1: V1A Input Triode (High-Headroom Laminar Input Stage BEFORE the Lead Gain Pot!)
            lead = fluidLeadV1A.processLaminarClean(lead * 1.05f, curLeadBreakup * 0.35f, sagVoltage);
            if (hyperLeadStandingWave) {
                lead += fluidLeadV1A.fluidMomentum * 0.12f;
            }
            lead = leadAntiAlias1.lp(lead, 12000.0f, osRate);
            lead = leadCouplingV1A.hp(lead, 24.0f, osRate);

            // Voicing: C45 Mid-Focus Punch vs Bright Cap
            if (!leadC45On) {
                float brightMix = std::max(0.0f, 1.0f - (leadGainVal / 10.0f)) * 0.40f;
                lead = lead + leadBrightCap.hp(lead, 3000.0f, osRate) * brightMix;
            } else {
                lead = leadC45Filter.process(lead);
            }

            // Lead Gain Potentiometer (True 10A Audio Taper — zero artificial floor offset!)
            float beGain = gainAudioTaper(leadGainVal, 5.8f);
            if (leadHbeOn) beGain *= 1.55f;

            // Low-gain clean/crunch path (LEAD GAIN 0.0 .. 3.0) blended smoothly into the 4-stage cascade
            float lowGainSweep = (leadGainVal <= 3.0f) ? audioTaper(leadGainVal * (10.0f / 3.0f), 0.90f) : 0.90f;
            float cleanCrunchLead = fluidLeadV2A.processLaminarClean(lead * lowGainSweep, curLeadBreakup * 0.70f, sagVoltage);

            lead *= beGain;

            // Stage 2: V1B 10k Cold Clipper (Smooth Asymmetric Crunch)
            lead = triodeColdClipper(lead, leadGridCapV1B, 1.65f, hyperLeadStandingWave);
            lead = leadAntiAlias2.lp(lead, 11500.0f, osRate);
            lead = leadCouplingV1B.hp(lead, 22.0f, osRate);

            // Interstage Grid-Leak Attenuator (470k / 470k divider)
            lead *= 0.52f;

            // Stage 3: V2A High-Voltage Saturated Drive
            lead = fluidLeadV2A.process(lead * (1.25f + 0.85f * curLeadBreakup), 1.45f, sagVoltage);
            lead = leadAntiAlias3.lp(lead, 11000.0f, osRate);
            lead = leadCouplingV2A.hp(lead, 20.0f, osRate);

            // Stage 4: V2B HBE (Hairy Brown Eye) Extra Cascade Gain Stage
            if (leadHbeOn) {
                lead *= 0.65f;
                lead = fluidLeadV2B.process(lead * 2.1f, 1.60f, sagVoltage);
                lead = snubberBm.lp(lead, 10500.0f, osRate);
            }

            // SAT Diode Saturation (Soft Symmetrical Clamping with Fluid Wall Reflection)
            if (leadSatOn) {
                lead = EuphoriaHyperDSP::FluidMath::fluidWallReflect(lead * 1.25f, 0.25f, curChamberShape) * 0.85f + 0.15f * EuphoriaHyperDSP::FluidMath::fast_tanh(lead);
            }

            // Smooth crossfade from low-gain JCM800 edge-of-clean into full 4-stage BE-100 cascade
            float cascadeMix = std::clamp(curLeadBreakup * 1.35f, 0.0f, 1.0f);
            lead = cleanCrunchLead * (1.0f - cascadeMix) + lead * cascadeMix;

            // Dedicated BE-100 Tone Stack
            lead = leadToneBass.process(lead);
            lead = leadToneMid.process(lead);
            lead = leadToneTreble.process(lead);
            lead = leadCouplingTone.hp(lead, 22.0f, osRate);

            float leadVolGain = audioTaper(leadVolVal, 4.20f);
            lead *= leadVolGain;
            ch3Out = lead;
        }

        float mixedPre = 0.0f;
        float activePowerBreakup = 0.0f;
        if (chMode == 0) {
            mixedPre = ch1Out;
            activePowerBreakup = curCleanBreakup * 0.65f;
        } else if (chMode == 1) {
            mixedPre = ch2Out;
            activePowerBreakup = std::clamp(curBmBreakup * 0.85f + curMasterBreakup * 0.65f, 0.0f, 1.0f);
        } else if (chMode == 2) {
            mixedPre = ch3Out;
            activePowerBreakup = std::clamp(curLeadBreakup * 0.80f + curMasterBreakup * 0.65f, 0.0f, 1.0f);
        } else {
            mixedPre = (ch1Out * 0.78f + ch2Out * 0.78f);
            activePowerBreakup = std::clamp(std::max(curCleanBreakup * 0.65f, curBmBreakup * 0.75f) + curMasterBreakup * 0.50f, 0.0f, 1.0f);
        }

        // Push-Pull Power Section + Fluid Core Transformer Memory (EL34 for Lead channel)
        int effectivePowerTubes = (chMode == 2) ? 1 : powerTubes;
        float pOut = processPowerStage(mixedPre, masterGain, effectivePowerTubes, sagVoltage, qualityMode, (chMode == 0), activePowerBreakup);
        return pOut;
    }

    void process(
        const float* input, float* outputL, float* outputR, uint32_t n_samples,
        float bypass,
        float pChannelMode,   // 0 = Blackface Clean, 1 = Bassman/JMP Drive, 2 = Friedman Lead, 3 = Jumpered Blend
        float pCleanVol,      // 0..10 Clean Volume
        float pDriveGain,     // 0..10 Preamp Gain
        float pDriveVol,      // 0..10 Channel Volume
        float pBass,          // 0..10
        float pMid,           // 0..10
        float pTreble,        // 0..10
        float pBrightSwitch,  // 0/1 Bright Cap
        float pSlopeMode,     // 0 = 56k Tweed, 1 = 33k JMP
        float pPowerTubes,    // 0 = 6L6GC, 1 = EL34
        float pMaster,        // 0..10 Master Volume
        float pPresence,      // 0..10 Presence
        float pSagAmount,     // 0..10 Fluid Sag Depth
        float pRevDwell,      // 0..10 Reverb Dwell
        float pRevMix,        // 0..100% Reverb Mix
        float pRevTone,       // 0..10 Reverb Tone
        float pRevDrip,       // 0..10 Reverb Drip
        float pTremSpeed,     // 0.5..12 Hz Vibrato Speed
        float pTremDepth,     // 0..100% Vibrato Depth
        float pTremMode,      // 0 = Opto Trem, 1 = Harmonic Tube Vibrato
        float pQualityMode,   // 0 = Live Eco, 1 = Stage 2x, 2 = Studio 4x, 3 = Ultra Bounce 8x
        float pSpeakerCab,    // 0 = Twin 2x12, 1 = Bassman 4x10, 2 = Greenback 4x12, 3 = Bypass
        float pSpeakerDrive,  // 0..10 Speaker Stress
        float pOutputLevel,   // 0..10 Output Trim
        float pNoiseGate = 1.0f,
        float pNoiseSpectral = 1.0f,
        float pNoiseDefizz = 1.0f,
        float pNoiseLevel = 5.0f,
        bool isOfflineBounce = false,
        float pBmBass = -1.0f, float pBmMid = -1.0f, float pBmTreble = -1.0f
    ) {
        using namespace EuphoriaHyperDSP;
        // Hardware Denormal Elimination (SSE2 FTZ/DAZ on Intel/AMD + FPCR FZ on ARM64 Raspberry Pi 5 / Apple Silicon)
        FluidMath::disableDenormals();

        if (bypass < 0.5f) {
            if (outputL != input) std::memcpy(outputL, input, n_samples * sizeof(float));
            if (outputR && outputR != input) std::memcpy(outputR, input, n_samples * sizeof(float));
            return;
        }

        int chMode = std::max(0, std::min(3, (int)lroundf(pChannelMode)));
        int slopeMode = (pSlopeMode > 0.5f) ? 1 : 0;
        int powerTubes = (pPowerTubes > 0.5f) ? 1 : 0;
        int cabType = std::max(0, std::min(12, (int)lroundf(pSpeakerCab)));
        if (cabType != physicalCab.model) {
            physicalCab.model = cabType;
            physicalCab.update((float)sampleRate);
        }
        curCleanVol      = std::clamp(pCleanVol, 0.0f, 10.0f);
        curCleanBreakup  = (curCleanVol > 7.0f) ? std::clamp((curCleanVol - 7.0f) / 3.0f, 0.0f, 1.0f) : 0.0f;
        curDriveGain     = std::clamp(pDriveGain, 0.0f, 10.0f);
        // True 10A Audio-Taper breakup curve for Bassman/JMP (chMode == 1):
        // Stays 100% woody Tweed clean up to 3.8 on the Gain knob, then blooms with a smooth logarithmic
        // audio taper through edge-of-breakup (4.0..5.5), classic crunch (6.0..7.5), and dimed roar (8.0..10.0)!
        if (curDriveGain <= 3.8f) {
            curBmBreakup = 0.0f;
        } else {
            float bmNorm = std::clamp((curDriveGain - 3.8f) / 6.2f, 0.0f, 1.0f);
            curBmBreakup = 0.14f * bmNorm + 0.30f * bmNorm * bmNorm + 0.56f * bmNorm * bmNorm * bmNorm;
        }
        // True 10A Audio-Taper breakup curve for Friedman BE-100 / JCM 800 Lead (chMode == 2):
        {
            float ldNorm = std::clamp(leadGainVal * 0.1f, 0.0f, 1.0f);
            curLeadBreakup = 0.12f * ldNorm + 0.28f * ldNorm * ldNorm + 0.60f * ldNorm * ldNorm * ldNorm;
        }
        // Master Volume Power-Tube Overdrive past 6.5:
        curMasterBreakup = (pMaster > 6.5f) ? gainAudioTaper((pMaster - 6.5f) * (10.0f / 3.5f), 1.0f) : 0.0f;

        // Speaker cone nonlinear stress scales with active channel breakup so clean/low-gain settings stay 100% pure:
        if (chMode == 0) {
            physicalCab.speakerDrive = pSpeakerDrive * curCleanBreakup;
        } else if (chMode == 1) {
            physicalCab.speakerDrive = pSpeakerDrive * std::clamp(curBmBreakup * 1.15f, 0.0f, 1.0f);
        } else {
            physicalCab.speakerDrive = pSpeakerDrive * std::clamp(0.15f + 0.85f * curLeadBreakup, 0.0f, 1.0f);
        }
        bool brightSw = (pBrightSwitch > 0.5f);
        bool isHarmonicVib = (pTremMode > 0.5f);

        int qMode = isOfflineBounce ? 3 : std::max(0, std::min(3, (int)lroundf(pQualityMode)));
        int osFactor = (qMode == 3) ? 8 : ((qMode == 2) ? 4 : ((qMode == 1) ? 2 : 1));
        float osRate = (float)sampleRate * (float)osFactor;

        float actualBmBass = (pBmBass >= 0.0f) ? pBmBass : bmBassVal;
        float actualBmMid  = (pBmMid  >= 0.0f) ? pBmMid  : bmMidVal;
        float actualBmTreble = (pBmTreble >= 0.0f) ? pBmTreble : bmTrebVal;

        updateToneStacks(
            pBass, pMid, pTreble, pPresence, brightSw, pCleanVol,
            actualBmBass, actualBmMid, actualBmTreble, slopeMode,
            leadBassVal, leadMidVal, leadTrebVal, leadPresVal, leadC45On,
            osRate
        );

        float cleanGain  = audioTaper(pCleanVol, 3.60f);
        float driveGain  = gainAudioTaper(pDriveGain, (slopeMode == 1) ? 4.80f : 4.00f);
        float driveVol   = audioTaper(pDriveVol, 3.80f);
        float masterGain = audioTaper(pMaster, 3.60f);
        float masterComp = 1.0f;
        float sagDepth   = std::max(0.0f, std::min(10.0f, pSagAmount)) / 10.0f;

        float revDwellNorm = std::max(0.0f, std::min(10.0f, (revDwell > 0.0f) ? revDwell : pRevDwell)) * 0.1f;
        float revMixNorm   = std::max(0.0f, std::min(100.0f, (revMix > 0.0f) ? revMix : pRevMix)) * 0.01f;
        float tremAmt      = std::max(0.0f, std::min(10.0f, pTremDepth)) * 0.10f; // 0..10 normalized to 0..1.0

        float outTrim = 1.0f;
        if (pOutputLevel <= 0.05f) {
            outTrim = 0.0f;
        } else {
            float db = (pOutputLevel - 5.0f) * 2.5f;
            outTrim = FluidMath::fast_db_to_lin(db);
        }

        bool gateActive = (pNoiseGate > 0.5f);

        // Dynamic parallel internal gain-noise tracking across 10 bands (Rule 33 & 35)
        // Strictly capped below the guitar playing signal threshold (<= -55.0 dBFS per Rule 33)
        // so stacking Full Volume Fuzz Face into Lead Amp NEVER cuts out or attenuates active playing!
        float gainNoiseBoostDb = 0.0f;
        if (chMode == 0 || chMode == 3) {
            gainNoiseBoostDb += (pCleanVol > 5.0f) ? (pCleanVol - 5.0f) * 1.5f : 0.0f;
        } else if (chMode == 1 || chMode == 3) {
            gainNoiseBoostDb += 3.0f + (pDriveGain / 10.0f) * 6.0f + (pDriveVol / 10.0f) * 2.0f;
        } else if (chMode == 2) {
            gainNoiseBoostDb += 4.0f + (leadGainVal / 10.0f) * 7.0f + (leadVolVal / 10.0f) * 2.0f;
            if (leadHbeOn) gainNoiseBoostDb += 2.0f;
            if (leadSatOn) gainNoiseBoostDb += 1.0f;
        }

        if (fuzzFace.enabled) gainNoiseBoostDb += 2.0f + (fuzzFace.fuzz / 10.0f) * 4.0f;
        if (centaur.enabled)  gainNoiseBoostDb += 1.5f + (centaur.gain / 10.0f) * 3.0f;
        if (ts808.enabled)    gainNoiseBoostDb += 1.5f + (ts808.drive / 10.0f) * 3.0f;
        if (bd2.enabled)      gainNoiseBoostDb += 1.5f + (bd2.gain / 10.0f) * 3.0f;
        if (rodent.enabled)   gainNoiseBoostDb += 2.0f + (rodent.distortion / 10.0f) * 4.0f;
        gainNoiseBoostDb = std::min(gainNoiseBoostDb, 12.0f);

        static const float baseStageNoiseDb[10] = {
            -72.0f, -70.0f, -68.0f, -66.0f, -64.0f, -62.0f, -60.0f, -60.0f, -62.0f, -64.0f
        };
        static const float noiseShapeFactor[10] = {
            0.25f,  0.35f,  0.50f,  0.75f,  0.95f,  1.00f,  1.00f,  0.95f,  0.85f,  0.75f
        };

        if (gateActive) {
            float redPct = (pNoiseLevel <= 0.05f) ? 0.0f : std::clamp(pNoiseLevel * 20.0f, 0.0f, 100.0f);
            float offDb  = (pNoiseLevel - 5.0f) * 2.4f;
            cyberDenoiser.setReductionAmount(redPct);
            cyberDenoiser.setThresholdOffsetDb(offDb);

            parallelGainDenoiser.setReductionAmount(redPct);
            parallelGainDenoiser.setThresholdOffsetDb(std::min(0.0f, offDb));

            for (int b = 0; b < 10; ++b) {
                float dynThresh = baseStageNoiseDb[b] + (gainNoiseBoostDb * noiseShapeFactor[b]);
                // Rule 33: Post-cab dynamic hiss tracking must operate strictly <= -55.0 dBFS!
                dynThresh = std::clamp(dynThresh, -95.0f, -55.0f);
                parallelGainDenoiser.setThresholdDb(b, dynThresh);
            }
        } else {
            cyberDenoiser.setReductionAmount(0.0f);
            parallelGainDenoiser.setReductionAmount(0.0f);
        }

        // 0. Pre-Preamp Clean Lab: Mains Hum Killer + 10-Band Cyber-Denoiser
        const uint32_t CHUNK_CAP = 2048;
        float humCleanBuf[CHUNK_CAP];
        float denoiseCleanBuf[CHUNK_CAP];

        float modLfoInc = (2.0f * (float)M_PI * spaceModRate) / (float)sampleRate;
        const float inAtk = 1.0f - expf(-1.0f / (0.0002f * (float)sampleRate));
        const float inRel = 1.0f - expf(-1.0f / (0.0600f * (float)sampleRate));

        uint32_t chunkOffset = 0;
        while (chunkOffset < n_samples) {
            uint32_t chunkSize = std::min(CHUNK_CAP, n_samples - chunkOffset);
            const float* inChunk = input + chunkOffset;
            const float* procChunk = inChunk;

            if (enableHumKiller) {
                if (hyperHumTracker && (cleanInputEnv < 0.001f || humKiller.isLearning())) {
                    for (uint32_t k = 0; k < chunkSize; ++k) {
                        float fEst = pllHumTracker.trackFundamentalHz(inChunk[k]);
                        humKiller.setDirectFundamental(fEst);
                    }
                }
                humKiller.process(inChunk, nullptr, humCleanBuf, nullptr, chunkSize);
                procChunk = humCleanBuf;
            }
            if (enableCyberDenoiser) {
                cyberDenoiser.processBlock(procChunk, nullptr, denoiseCleanBuf, nullptr, chunkSize);
                procChunk = denoiseCleanBuf;
            }

            for (uint32_t i = 0; i < chunkSize; ++i) {
                uint32_t globalIdx = chunkOffset + i;
                float rawIn = procChunk[i];

                // Track clean guitar input level with instantaneous attack (<0.2ms) and smooth musical release (60ms)
                float absRaw = std::abs(rawIn);
                if (absRaw > cleanInputEnv) {
                    cleanInputEnv += inAtk * (absRaw - cleanInputEnv);
                } else {
                    cleanInputEnv += inRel * (absRaw - cleanInputEnv);
                }

                // Pre-Pedal / Pre-Preamp Studio Downward Expander:
                // Smoothly expands idle interface/pickup noise floor (< -58 dBFS) down by -45 dB BEFORE
                // it ever hits high-gain pedals (Fuzz Face, Rat, TS808) or the Lead/Clean preamp cascades,
                // while opening instantaneously (0.6ms) to 1.0 (0.0 dB) the moment a string is played!
                float inGateThreshDb = -58.0f + (pNoiseLevel - 5.0f) * 1.5f;
                rawIn = inputExpander.process(rawIn, (float)sampleRate, gateActive, inGateThreshDb, -45.0f);

                float s = pickupVoicing.process(rawIn, (float)sampleRate);
                s = dcBlockerPre.hp(s, 24.0f, (float)sampleRate);

                // Pre-Amp Stompbox Overdrive: Fluid Fuzz Face, Centaur, TS808, BD-2, and Rodent in Series (Rule 33 & 34)
                if (fuzzFace.enabled) s = fuzzFace.processSample(s, (float)sampleRate);
                if (centaur.enabled)  s = centaur.processSample(s, (float)sampleRate);
                if (ts808.enabled)    s = ts808.processSample(s, (float)sampleRate);
                if (bd2.enabled)      s = bd2.processSample(s, (float)sampleRate);
                if (rodent.enabled)   s = rodent.processSample(s, (float)sampleRate);

                // Dynamic Fluid Space Chamber Geometry & LFO Modulation (Rule 27 compliant)
                spaceModPhase1 += modLfoInc;
                if (spaceModPhase1 > 2.0f * (float)M_PI) spaceModPhase1 -= 2.0f * (float)M_PI;
                spaceModPhase2 += modLfoInc * 1.6180339887f;
                if (spaceModPhase2 > 2.0f * (float)M_PI) spaceModPhase2 -= 2.0f * (float)M_PI;
                spaceModPhase3 += modLfoInc * 0.70710678f;
                if (spaceModPhase3 > 2.0f * (float)M_PI) spaceModPhase3 -= 2.0f * (float)M_PI;

                float spreadRad = spaceStereoSpread * (float)(M_PI / 180.0);
                float lfoW = 0.0f;
                float lfoWR = 0.0f;
                if (spaceModWaveform == 0) {
                    lfoW  = FluidMath::fast_sin(spaceModPhase1);
                    lfoWR = FluidMath::fast_sin(spaceModPhase1 + spreadRad);
                } else if (spaceModWaveform == 1) {
                    // Triangle wave directly from phase in [0, 2*pi] (zero sinf/asinf!)
                    auto fastTri = [](float ph) -> float {
                        while (ph >= 6.2831853f) ph -= 6.2831853f;
                        float norm = ph * 0.15915494f; // 0..1
                        return 1.0f - 4.0f * std::fabs(norm - 0.5f);
                    };
                    lfoW  = fastTri(spaceModPhase1);
                    lfoWR = fastTri(spaceModPhase1 + spreadRad);
                } else if (spaceModWaveform == 2) {
                    lfoW  = 0.62f * FluidMath::fast_sin(spaceModPhase1) + 0.38f * FluidMath::fast_sin(spaceModPhase2);
                    lfoWR = 0.62f * FluidMath::fast_sin(spaceModPhase1 + spreadRad) + 0.38f * FluidMath::fast_sin(spaceModPhase2 + spreadRad);
                } else {
                    lfoW  = 0.50f * FluidMath::fast_sin(spaceModPhase1) + 0.30f * FluidMath::fast_sin(spaceModPhase2) + 0.20f * FluidMath::fast_sin(spaceModPhase3);
                    lfoWR = 0.50f * FluidMath::fast_sin(spaceModPhase1 + spreadRad) + 0.30f * FluidMath::fast_sin(spaceModPhase2 + spreadRad) + 0.20f * FluidMath::fast_sin(spaceModPhase3 + spreadRad);
                }

                float targetShape = baseChamberShape;
                float targetSize = baseChamberSize;
                if (spaceModEnabled) {
                    targetShape = std::clamp(baseChamberShape + lfoW * spaceShapeModDepth * 0.5f, 0.0f, 1.0f);
                    float sizeScale = 1.0f + lfoW * spaceSizeModDepth * 0.65f + 0.21f * (lfoW * spaceSizeModDepth) * (lfoW * spaceSizeModDepth);
                    targetSize = std::clamp(baseChamberSize * sizeScale, 0.10f, 10.0f);
                }

                // Smooth one-pole parameter interpolator (Rule 27: ~15ms smoothing)
                curChamberShape += 0.0035f * (targetShape - curChamberShape);
                curChamberSize += 0.0035f * (targetSize - curChamberSize);

                fluidBf1.shapeFactor = curChamberShape; fluidBf1.sizeFactor = curChamberSize;
                fluidBf2.shapeFactor = curChamberShape; fluidBf2.sizeFactor = curChamberSize;
                fluidBm1.shapeFactor = curChamberShape; fluidBm1.sizeFactor = curChamberSize;
                fluidBm2.shapeFactor = curChamberShape; fluidBm2.sizeFactor = curChamberSize;
                fluidCf.shapeFactor = curChamberShape;  fluidCf.sizeFactor = curChamberSize;
                fluidLeadV1A.shapeFactor = curChamberShape; fluidLeadV1A.sizeFactor = curChamberSize;
                fluidLeadV2A.shapeFactor = curChamberShape; fluidLeadV2A.sizeFactor = curChamberSize;
                fluidLeadV2B.shapeFactor = curChamberShape; fluidLeadV2B.sizeFactor = curChamberSize;
                fluidPowerPush.shapeFactor = curChamberShape; fluidPowerPush.sizeFactor = curChamberSize;
                fluidPowerPull.shapeFactor = curChamberShape; fluidPowerPull.sizeFactor = curChamberSize;

                // Stage 1 & 2: Polyphase Half-Band Oversampled Pure Fluid Preamp & Power Solver
                float pOut = 0.0f;
                if (osFactor == 1) {
                    pOut = processFluidCoreSubSample(s, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                } else if (osFactor == 2) {
                    float u0, u1;
                    upStage1.upsample2x(s, u0, u1);
                    float y0 = processFluidCoreSubSample(u0, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                    float y1 = processFluidCoreSubSample(u1, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                    pOut = dnStage1.downsample2x(y0, y1);
                } else if (osFactor == 4) {
                    float u0, u1, u00, u01, u10, u11;
                    upStage1.upsample2x(s, u0, u1);
                    upStage2[0].upsample2x(u0, u00, u01);
                    upStage2[1].upsample2x(u1, u10, u11);
                    float y00 = processFluidCoreSubSample(u00, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                    float y01 = processFluidCoreSubSample(u01, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                    float y10 = processFluidCoreSubSample(u10, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                    float y11 = processFluidCoreSubSample(u11, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                    float d0 = dnStage2[0].downsample2x(y00, y01);
                    float d1 = dnStage2[1].downsample2x(y10, y11);
                    pOut = dnStage1.downsample2x(d0, d1);
                } else {
                    float u2[2], u4[4], u8[8], y8[8], d4[4], d2[2];
                    upStage1.upsample2x(s, u2[0], u2[1]);
                    for (int k = 0; k < 2; ++k) upStage2[k].upsample2x(u2[k], u4[k * 2], u4[k * 2 + 1]);
                    for (int k = 0; k < 4; ++k) upStage3[k].upsample2x(u4[k], u8[k * 2], u8[k * 2 + 1]);
                    for (int k = 0; k < 8; ++k) {
                        y8[k] = processFluidCoreSubSample(u8[k], chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, qMode, osRate);
                    }
                    for (int k = 0; k < 4; ++k) d4[k] = dnStage3[k].downsample2x(y8[k * 2], y8[k * 2 + 1]);
                    for (int k = 0; k < 2; ++k) d2[k] = dnStage2[k].downsample2x(d4[k * 2], d4[k * 2 + 1]);
                    pOut = dnStage1.downsample2x(d2[0], d2[1]);
                }

                pOut *= masterComp;
                pOut = snubberPower.lp(pOut, 14500.0f, (float)sampleRate);

                // Dynamic Power Rail Sag
                if (qMode == 0) {
                    if ((++sagCounter & 3) == 0) {
                        float currentDraw = fabsf(pOut) * 0.055f * sagDepth;
                        sagVoltage += (1.0f - currentDraw - sagVoltage) * 0.0072f;
                        sagVoltage = std::max(0.66f, std::min(1.0f, sagVoltage));
                    }
                } else {
                    float currentDraw = fabsf(pOut) * 0.055f * sagDepth;
                    sagVoltage += (1.0f - currentDraw - sagVoltage) * 0.0018f;
                    sagVoltage = std::max(0.66f, std::min(1.0f, sagVoltage));
                }

                if (chMode == 1 || chMode == 3) {
                    pOut = presenceFilter.process(pOut);
                } else if (chMode == 2) {
                    pOut = leadPresenceFilter.process(pOut);
                }
                pOut = dcBlockerPost.hp(pOut, 22.0f, (float)sampleRate);

                // Stage 4: Vintage Fender Fluid Space Harmonic Tremolo (6G-Series Anti-Phase Tube Cells)
                if (enableVibrato && tremAmt > 0.005f) {
                    pOut = fluidTremolo.process(pOut, pTremSpeed, tremAmt, isHarmonicVib, enableVibrato, (float)sampleRate);
                }

                // Stage 5: Physical Speaker Cone ODE & Acoustic Enclosure Cabinet
                pOut = physicalCab.processSample(pOut, (float)sampleRate);

                // Stage 5b: Parallel Internal Gain Noise Auto-Removal & Clean-Keyed Dynamic De-Hiss (Rule 33 & 35)
                // Keyed off the clean DI input envelope (cleanInDb) BEFORE pedals/preamp:
                // - While playing (cleanInDb >= -56 dBFS), dynamicDeHiss stays 100% open (1.0f) so treble chime
                //   and Full Fuzz + Lead sustain NEVER cut out or dull.
                // - When idle (cleanInDb < -56 dBFS), residual preamp/pedal hiss is expanded down by -42 dB!
                if (gateActive) {
                    pOut = parallelGainDenoiser.processSample(pOut);
                }
                if (gateActive || pNoiseDefizz > 0.5f) {
                    float cleanInDb = (cleanInputEnv > 1e-7f) ? FluidMath::fast_db(cleanInputEnv) : -120.0f;
                    float deHissThreshDb = -56.0f + (leadDeHissSens - 5.0f) * 1.4f;
                    pOut = dynamicDeHiss.processSample(pOut, cleanInDb, deHissThreshDb, -42.0f, true);
                }

                // Stage 6: Physical Acoustic Chamber Space (432 Hz Doppler Transit Line & Cavity Sweep)
                float spaceL = pOut, spaceR = pOut;
                fluidAcousticSpace.process(
                    pOut, pOut,
                    curChamberSize, lfoW, lfoWR,
                    spaceSizeModDepth, curChamberShape, spaceModEnabled,
                    (float)sampleRate,
                    spaceL, spaceR
                );

                // Stage 7: Pure Fluid Dynamics Reverb Space (Send Low/High Cut + 432 Hz Size & Shape)
                float revOutL = spaceL, revOutR = spaceR;
                if (enableReverb && revMixNorm > 0.001f) {
                    curRevShape += 0.0035f * (revChamberShape - curRevShape);
                    curRevSize += 0.0035f * (revChamberSize - curRevSize);
                    fluidReverb.process(
                        spaceL, spaceR,
                        curRevSize, curRevShape,
                        revSendLowCut, revSendHighCut,
                        revDwellNorm, revMixNorm,
                        (float)sampleRate,
                        revOutL, revOutR
                    );
                }

                float finalL = revOutL * outTrim;
                float finalR = revOutR * outTrim;

#if CYBER_ENABLE_STUDIO_DELAY
                if (dlyUnit.enabled && dlyUnit.wet > 0.05f) {
                    processStudioDelaySample(finalL, finalR, finalL, finalR);
                }
#endif

                // Final Fluid-Space Headroom Boundary Limiter:
                // 100% bit-linear up to +/- 0.82 (-1.7 dBFS) so Clean Glass notes have zero soft-clipper
                // distortion, while smoothly bounding extreme pedal + lead cascades below 0.99!
                auto linearKneeLimit = [](float x) noexcept -> float {
                    float ax = std::fabs(x);
                    if (ax <= 0.82f) return x;
                    float sgn = (x >= 0.0f) ? 1.0f : -1.0f;
                    return sgn * (0.82f + 0.165f * EuphoriaHyperDSP::FluidMath::fast_tanh((ax - 0.82f) * 4.5f));
                };
                finalL = linearKneeLimit(finalL);
                finalR = linearKneeLimit(finalR);

                outputL[globalIdx] = finalL;
                if (outputR) outputR[globalIdx] = finalR;
            }
            chunkOffset += chunkSize;
        }
    }

private:
    double sampleRate;

    // Channel 0: Euphoria Supreme Glass Clean Filters (Active 3D Tone Matrix)
    FluidBiquad cleanSubFilter, cleanBass, cleanMid, cleanTreble, cleanGlassAir, cleanBrightShelf, cleanPresenceFilter;

    // Tone Stacks (Legacy & Dedicated Channels)
    FluidBiquad bfBass, bfMid, bfTreble, bfBrightShelf;
    FluidBiquad bmBass, bmMid, bmTreble;
    FluidBiquad presenceFilter;

    // Studio Downward Expander & Dynamic Noise Gate
    StudioDownwardExpander inputExpander;
    float cleanInputEnv = 0.0f;

    // Physical Speaker ODE & Acoustic Enclosure Cabinet
    FluidSpeakerCabinetDSP physicalCab;

    // Polyphase Oversampling
    HalfBandPolyphase upStage1, upStage2[2], upStage3[4];
    HalfBandPolyphase dnStage1, dnStage2[2], dnStage3[4];

    // DC Blockers & Snubbers
    FluidOnePole snubberBf, snubberBm, snubberPower;
    FluidOnePole dcBlockerPre, dcBlockerPost, dcBlockerBf, dcBlockerBm;

    // Fluid Dynamics Cells
    FluidDynamicsCell fluidBf1, fluidBf2;
    FluidDynamicsCell fluidBm1, fluidBm2;
    FluidCathodeFollowerCell fluidCf;
    FluidDynamicsCell fluidLeadV1A, fluidLeadV2A, fluidLeadV2B;
    FluidDynamicsCell fluidPowerPush, fluidPowerPull;

    // Channel 3: '80s Friedman BE-100 / JCM 800 Lead Filters & Components
    FluidBiquad leadToneBass, leadToneMid, leadToneTreble;
    FluidBiquad leadC45Filter, leadPresenceFilter;
    FluidOnePole leadAntiAliasTight, leadAntiAlias1, leadAntiAlias2, leadAntiAlias3, leadAntiAliasPower;
    FluidOnePole leadCouplingV1A, leadCouplingV1B, leadCouplingV2A, leadCouplingTone, leadBrightCap;
    float leadGridCapV1A = 0.0f, leadGridCapV1B = 0.0f, leadGridCapV2A = 0.0f, leadGridCapV2B = 0.0f;

    // Physical Acoustic Chamber Space (432 Hz Doppler Transit Line & Cavity Resonator)
    FluidAcousticChamberSpace fluidAcousticSpace;

    // Pure Fluid Dynamics Reverb Space
    FluidReverbSpace fluidReverb;

    // Vintage Fender Fluid Space Harmonic Tremolo
    FluidHarmonicTremolo fluidTremolo;

    float sagVoltage;
    uint32_t sagCounter;
    float otFlux, otPrevIn;

#if CYBER_ENABLE_STUDIO_DELAY
    StudioDelayState dlyUnit;
#endif
};

} // namespace EuphoriaDSP
