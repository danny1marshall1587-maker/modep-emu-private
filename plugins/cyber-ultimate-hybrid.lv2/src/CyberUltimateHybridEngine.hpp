/*
 * CyberUltimateHybridEngine.hpp
 *
 * "The Ultimate American/British Hybrid" - Scalable Physical Tube Amplifier Engine
 *
 * Shared C++17 Real-Time DSP Core for:
 *   1. MODEP / Live Mixer LV2 Plugin (CYBER_ENABLE_STUDIO_DELAY = 0)
 *      - Uses the Live Mixer's Aux Send 3-4 for delay; keeps channel insert lean.
 *   2. Desktop / DAW Plugin: VST3, AU (Apple Silicon/Intel), Standalone (CYBER_ENABLE_STUDIO_DELAY = 1)
 *      - Embeds the exact Live Mixer Studio Tape / Ping-Pong Delay engine
 *        (Hermite 4-point cubic interpolation, dual read-head 45ms equal-power crossfade,
 *         15/30 IPS Tape Oxide saturation, independent dual-harmonic Wow/Flutter, Auto-Ducking)
 *      - Automatically switches to 8x Ultra Physics mode when the DAW bounces offline (isNonRealtime).
 *
 * Circuit Topology:
 *   - Channel 1 (Blackface AB763): 12AX7 V1A -> Plate-Driven Scooped Tone Stack + 120pF Bright Cap -> V2A Recovery
 *   - Channel 2 (5F6-A Bassman / 1987 JMP): 12AY7/12AX7 -> V2A Gain -> DC-Coupled Cathode Follower ->
 *     Cathode-Driven Tone Stack (Switchable 56k Fender Tweed vs 33k Marshall JMP Slope Voicing)
 *   - Channel 3 (Jumpered Parallel Blend): Phase-aligned parallel blend of Glassy Blackface + Chewy Bassman/JMP
 *   - 3-Spring Accutronics Tube Reverb Tank: 12AT7 Driver Saturation + 3 Cross-Coupled Springs + 16-Stage Allpass "Drip"
 *   - Brownface 6G-Series / Koch "The Greg" Harmonic Tube Vibrato: Crossover Band-Split Counter-Phase Triode Modulator
 *   - Push-Pull Power Section (Switchable 6L6GC American vs EL34 British) + Dynamic B+ Rail Sag + Output Transformer Hysteresis
 *   - Scalable Physics Engine (Quality Mode: 0=Eco 1x, 1=Stage 2x, 2=Studio 4x, 3=Ultra Bounce 8x)
 */

#ifndef CYBER_ULTIMATE_HYBRID_ENGINE_HPP
#define CYBER_ULTIMATE_HYBRID_ENGINE_HPP

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

namespace CyberDSP {

struct HybridOnePole {
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

struct HybridBiquad {
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
    void setIdentity() { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }

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

class CyberUltimateHybridEngine {
public:
    CyberUltimateHybridEngine(double rate) : sampleRate(rate) {
        reset();
    }

    void reset() {
        bfBass.setIdentity(); bfMid.setIdentity(); bfTreble.setIdentity(); bfBrightShelf.setIdentity();
        bmBass.setIdentity(); bmMid.setIdentity(); bmTreble.setIdentity();
        presenceFilter.setIdentity();
        tremLp.setIdentity();

        cabTwinLow.setIdentity(); cabTwinMid.setIdentity(); cabTwinLp1.setIdentity(); cabTwinLp2.setIdentity();
        cabP10Low.setIdentity();  cabP10Mid.setIdentity();  cabP10Lp1.setIdentity();  cabP10Lp2.setIdentity();
        cab412Low.setIdentity();  cab412Mid.setIdentity();  cab412Lp1.setIdentity();  cab412Lp2.setIdentity();

        dcBlockerPre.reset(); dcBlockerPost.reset();
        dcBlockerBf.reset();  dcBlockerBm.reset();
        snubberBf.reset();    snubberBm.reset();    snubberPower.reset();

        upStage1.reset(); upStage2[0].reset(); upStage2[1].reset();
        for (int i = 0; i < 4; ++i) upStage3[i].reset();
        dnStage1.reset(); dnStage2[0].reset(); dnStage2[1].reset();
        for (int i = 0; i < 4; ++i) dnStage3[i].reset();

        sagVoltage = 1.0f;
        sagScreenGrid = 1.0f;
        sagCounter = 0;

        gridCapBf1 = gridCapBf2 = 0.0f;
        gridCapBm1 = gridCapBm2 = gridCapCf = 0.0f;
        platePrevBf1 = platePrevBf2 = platePrevBm1 = platePrevBm2 = cfPrev = 0.0f;
        otFlux = 0.0f;
        otPrevIn = 0.0f;

        lfoPhase = 0.0f;
        vibTriodeBiasL = vibTriodeBiasH = 0.0f;

        // 3-Spring Accutronics Reverb Tank Initialization
        springDelays[0] = std::max(16, std::min(SPRING_MAX_DELAY - 1, (int)(0.0335 * sampleRate)));
        springDelays[1] = std::max(16, std::min(SPRING_MAX_DELAY - 1, (int)(0.0373 * sampleRate)));
        springDelays[2] = std::max(16, std::min(SPRING_MAX_DELAY - 1, (int)(0.0418 * sampleRate)));
        for (int k = 0; k < NUM_SPRINGS; ++k) {
            std::memset(springBuffers[k], 0, sizeof(springBuffers[k]));
            springWritePos[k] = 0;
            springLp[k] = 0.0f;
            for (int st = 0; st < DISP_STAGES; ++st) {
                dispX[k][st] = 0.0f;
                dispY[k][st] = 0.0f;
            }
        }
        std::memset(revDiffBuf1, 0, sizeof(revDiffBuf1));
        std::memset(revDiffBuf2, 0, sizeof(revDiffBuf2));
        revDiffIdx1 = revDiffIdx2 = 0;
        revOutToneLp = 0.0f;
        revDriverBias = 0.0f;
        smoothedRevMix = 0.25f;
        smoothedRevDecay = 0.78f;

        // Speaker Stress State
        speakerEnv = speakerThermalEnv = speakerConeHistory = 0.0f;
        const float sRateF = (float)sampleRate;
        spkAtk = 1.0f - expf(-1.0f / (sRateF * 0.0015f));
        spkRel = 1.0f - expf(-1.0f / (sRateF * 0.045f));
        spkThermalRel = 1.0f - expf(-1.0f / (sRateF * 0.400f));

        // Noise Suppressor Lab State
        gateScHp.reset(); gateScLp.reset();
        gateEnv = 0.0f; gateGain = 0.0f; gateIsOpen = false;
        gateAtk = 1.0f - expf(-1.0f / (sRateF * 0.0015f));
        gateRel = 1.0f - expf(-1.0f / (sRateF * 0.220f));
        gateAtkSmooth = 1.0f - expf(-1.0f / (sRateF * 0.001f));
        gateRelSmooth = 1.0f - expf(-1.0f / (sRateF * 0.180f));

        specAttCoeff = 1.0f - expf(-1.0f / (0.0001f * sRateF));
        specLevelAtt = 1.0f - expf(-1.0f / (0.0020f * sRateF));
        specLevelRel = 1.0f - expf(-1.0f / (0.0600f * sRateF));

        const float multi[9] = { 60.0f, 150.0f, 400.0f, 800.0f, 1500.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f };
        for (int i = 0; i < 9; ++i) {
            specLpStates[i] = 0.0f;
            float w = 2.0f * (float)M_PI * multi[i] / sRateF;
            specBCoeffs[i] = std::min(0.99f, 1.0f - expf(-w));
        }

        const float defaultThreshDb[10] = {
            -72.0f, -70.0f, -68.0f, -65.0f, -63.0f,
            -60.0f, -57.0f, -55.0f, -54.0f, -53.0f
        };
        for (int i = 0; i < 10; ++i) {
            specLevelStates[i] = 0.0f;
            specCurrentGains[i] = 1.0f;
            specThresholdsDb[i] = defaultThreshDb[i];
            specTLinear[i] = powf(10.0f, specThresholdsDb[i] * 0.05f);
        }

        defizzEnv = 0.0f;
        defizzLpState = 0.0f;
        defizzExpanderGain = 1.0f;
        defizzAtk = 1.0f - expf(-1.0f / (0.002f * sRateF));
        defizzRel = 1.0f - expf(-1.0f / (0.070f * sRateF));

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
        const float sr = (float)sampleRate;
        // Cab 0: '65 Blackface Twin 2x12 Jensen C12N (Deep 75Hz bloom, scooped 480Hz, airy 3.4kHz sparkle)
        cabTwinLow.setLowShelf(78.0f, 5.2f, 0.707f, sr);
        cabTwinMid.setPeaking(3250.0f, 4.2f, 1.35f, sr);
        cabTwinLp1.setLowPass(7200.0f, 0.707f, sr);
        cabTwinLp2.setLowPass(7200.0f, 0.707f, sr);

        // Cab 1: '59 Tweed Bassman 4x10 Jensen P10R Alnico (Tight 92Hz punch, woody 2.8kHz chime)
        cabP10Low.setLowShelf(92.0f, 5.4f, 0.707f, sr);
        cabP10Mid.setPeaking(2950.0f, 4.0f, 1.40f, sr);
        cabP10Lp1.setLowPass(6700.0f, 0.707f, sr);
        cabP10Lp2.setLowPass(6700.0f, 0.707f, sr);

        // Cab 2: '68 Marshall JMP 4x12 Celestion G12M-25 Greenback (110Hz chest thump, 2.4kHz vocal bark)
        cab412Low.setPeaking(108.0f, 5.8f, 1.15f, sr);
        cab412Mid.setPeaking(2450.0f, 4.5f, 1.45f, sr);
        cab412Lp1.setLowPass(6100.0f, 0.707f, sr);
        cab412Lp2.setLowPass(6100.0f, 0.707f, sr);
    }

    /*
     * 12AX7 Nonlinear Triode Load-Line Solver
     * Solves grid current rectification, cathode bias shift, and soft asymmetric triode saturation.
     * Miller high-frequency roll-off is handled as a physical plate snubber RC filter (snubberBf / snubberBm),
     * preventing any discrete-time feedback limit cycles or parasitic ringing.
     */
    inline float solveTriodeNR(float in, float &gridBias, float &platePrev, float gainFactor, float asym, float sagBPlus, int nrIterations) {
        // Grid conduction rectifies positive peaks and dynamically shifts grid bias
        if (in > 0.0f) {
            gridBias += (in - gridBias) * 0.0035f;
        } else {
            gridBias *= 0.99980f;
        }
        gridBias = std::max(0.0f, std::min(4.0f, gridBias));

        float vGrid = (in - gridBias * 0.18f) * gainFactor;
        vGrid = std::max(-5.5f, std::min(5.5f, vGrid));
        float effBPlus = std::max(0.64f, sagBPlus);

        float u = vGrid / effBPlus;
        float th = tanhf(u);
        float target = (th - asym * (th * th)) * effBPlus;

        // Smooth physical plate response (models plate capacitance / inter-electrode damping)
        platePrev = 0.82f * target + 0.18f * platePrev;
        return platePrev;
    }

    /*
     * Direct-Coupled 12AX7 Cathode Follower (V2B in 5F6-A Bassman & Marshall JMP)
     * Draws heavy grid current on positive peaks, creating the signature warm, chewy compression
     * before driving the passive tone stack.
     */
    inline float solveCathodeFollowerNR(float in, float &gridBias, float &cfState, int nrIterations) {
        if (in > 0.0f) {
            gridBias += (in - gridBias) * 0.0095f;
        } else {
            gridBias *= 0.99935f;
        }
        float x = in - gridBias * 0.34f;
        float y = cfState;
        const float kCathodeDeg = 0.22f;

        for (int it = 0; it < nrIterations; ++it) {
            float u = (x - kCathodeDeg * y) * 1.32f;
            float th = tanhf(u);
            float sech2 = 1.0f - th * th;
            float target = th - 0.13f * (th * th);
            float f = y - target;
            if (fabsf(f) < 1e-6f) break;
            float df = 1.0f + kCathodeDeg * 1.32f * sech2 * (1.0f - 0.26f * th);
            y -= f / std::max(0.25f, df);
        }
        cfState = y;
        return y;
    }

    /*
     * Push-Pull Power Section (6L6GC American vs EL34 British) + Output Transformer Hysteresis
     */
    inline float processPowerStage(float in, float masterGain, int powerTubeType, float sag, int qualityMode) {
        float effectiveBPlus = std::max(0.64f, sag);
        float push = in * masterGain;
        float pull = -in * masterGain * 0.985f;

        auto evalTube = [&](float v) -> float {
            float x = v / effectiveBPlus;
            if (powerTubeType == 1) {
                // EL34 British Pentode: earlier screen-grid knee, stronger odd-harmonic snarl
                if (x < -0.62f) {
                    return (-0.62f - 0.22f * tanhf((x + 0.62f) * 1.55f)) * effectiveBPlus;
                }
                float t = tanhf(x * 1.48f);
                return (t - 0.075f * (t * t) - 0.045f * (t * t * t)) * effectiveBPlus;
            } else {
                // 6L6GC American Beam Tetrode: glassy headroom, firm low-end punch
                if (x < -0.74f) {
                    return (-0.74f - 0.18f * tanhf((x + 0.74f) * 1.35f)) * effectiveBPlus;
                }
                float t = tanhf(x * 1.24f);
                return (t - 0.045f * (t * t)) * effectiveBPlus;
            }
        };

        float pOut = (evalTube(push) - evalTube(pull)) * 0.5f;

        // Output Transformer Core Hysteresis (Jiles-Atherton style differential core memory in Studio/Ultra modes)
        if (qualityMode >= 2) {
            float dH = pOut - otPrevIn;
            otPrevIn = pOut;
            float m_an = tanhf(pOut * 1.15f);
            float delta = (dH >= 0.0f) ? 1.0f : -1.0f;
            float diff = m_an - otFlux;
            float dM = (diff * delta > 0.0f) ? (0.35f * diff * dH) : (0.08f * diff * dH);
            otFlux = 0.992f * (otFlux + dM) + 0.008f * m_an;
            pOut = 0.72f * pOut + 0.28f * otFlux;
        } else {
            // Fast rational saturation in Live Eco mode
            otFlux = 0.995f * otFlux + 0.005f * pOut;
            pOut = pOut / (1.0f + 0.12f * otFlux * otFlux);
        }

        return pOut;
    }

    /*
     * 3-Spring Accutronics Reverb Tank with 12AT7 Tube Driver & 16-Stage Allpass "Drip"
     */
    inline float processSpringReverb(float inSample, float revDwell, float revMix, float revTone, float revDrip) {
        if (revMix <= 0.005f) return inSample;

        float driveGain = 0.55f + 2.85f * (revDwell * revDwell);
        float toneFreq = 1600.0f + 6600.0f * (revTone * revTone);
        float toneAlpha = 1.0f - expf(-2.0f * (float)M_PI * toneFreq / (float)sampleRate);
        float dispA = -0.36f - 0.39f * revDrip;
        float targetFeedback = 0.70f + 0.21f * revDwell;

        smoothedRevMix += 0.004f * (revMix - smoothedRevMix);
        smoothedRevDecay += 0.004f * (targetFeedback - smoothedRevDecay);

        // 1. 12AT7 Reverb Driver Tube Saturation + Transducer Splash
        float driven = solveTriodeNR(inSample * driveGain, revDriverBias, revOutToneLp, 1.35f, 0.06f, 1.0f, 1);

        // 2. Transducer gap diffusion (2 cascaded allpasses)
        float d1_delayed = revDiffBuf1[revDiffIdx1];
        float d1_out = -0.6f * driven + d1_delayed;
        revDiffBuf1[revDiffIdx1] = driven + 0.6f * d1_out;
        revDiffIdx1 = (revDiffIdx1 + 1) % REV_DIFF_DELAY1;

        float d2_delayed = revDiffBuf2[revDiffIdx2];
        float d2_out = 0.6f * d1_out + d2_delayed;
        revDiffBuf2[revDiffIdx2] = d1_out - 0.6f * d2_out;
        revDiffIdx2 = (revDiffIdx2 + 1) % REV_DIFF_DELAY2;

        // 3. Read 3 physical spring lines
        float s0 = springBuffers[0][springWritePos[0]];
        float s1 = springBuffers[1][springWritePos[1]];
        float s2 = springBuffers[2][springWritePos[2]];

        // 4. Mechanical end-plate cross-coupling matrix
        float coupled[NUM_SPRINGS] = {
             0.60f * s0 + 0.40f * s1 - 0.35f * s2,
            -0.35f * s0 + 0.60f * s1 + 0.40f * s2,
             0.40f * s0 - 0.35f * s1 + 0.60f * s2
        };

        for (int k = 0; k < NUM_SPRINGS; ++k) {
            float springIn = d2_out + coupled[k] * smoothedRevDecay;
            if (springIn > 1.5f) springIn = 1.0f;
            else if (springIn < -1.5f) springIn = -1.0f;
            else springIn = springIn * (1.5f - 0.5f * springIn * springIn);

            springLp[k] += toneAlpha * (springIn - springLp[k]);
            float apSig = springLp[k];

            // 16-stage dispersive allpass chain ("The Drip / Chirp")
            for (int st = 0; st < DISP_STAGES; ++st) {
                float apIn = apSig;
                float apOut = dispA * apIn + dispX[k][st] - dispA * dispY[k][st];
                dispX[k][st] = apIn;
                dispY[k][st] = apOut;
                apSig = apOut;
            }

            springBuffers[k][springWritePos[k]] = apSig;
            springWritePos[k] = (springWritePos[k] + 1) % springDelays[k];
        }

        float rawWet = (s0 - 0.85f * s1 + 0.90f * s2) * 0.58f;
        return (1.0f - smoothedRevMix * 0.35f) * inSample + smoothedRevMix * rawWet;
    }

    inline float processSpeaker(float in, int cabType, float driveVal) {
        if (cabType == 3) return in; // Direct / Cab Bypass

        float s = in;
        if (driveVal > 0.05f) {
            float driveAmount = driveVal / 10.0f;
            float rect = fabsf(s);
            if (rect > speakerEnv) speakerEnv += spkAtk * (rect - speakerEnv);
            else speakerEnv += spkRel * (rect - speakerEnv);
            speakerThermalEnv += spkThermalRel * (speakerEnv - speakerThermalEnv);

            float comp = 1.0f / (1.0f + speakerEnv * driveAmount * 1.35f);
            float thermalComp = 1.0f / (1.0f + speakerThermalEnv * driveAmount * 0.28f);
            s = s * comp * thermalComp;

            float coneStress = s * (1.0f + driveAmount * 1.25f);
            float t = tanhf(coneStress);
            float coneOut = (t - 0.04f * (t * t)) / (1.0f + driveAmount * 0.28f);

            float dampingFc = 6500.0f - driveAmount * 1500.0f;
            float w = 2.0f * (float)M_PI * dampingFc / (float)sampleRate;
            float a0 = w / (1.0f + w);
            speakerConeHistory += a0 * (coneOut - speakerConeHistory);

            s = (1.0f - driveAmount * 0.82f) * s + (driveAmount * 0.82f) * speakerConeHistory;
            s *= (1.0f + driveAmount * 0.26f);
        }

        if (cabType == 0) {
            s = cabTwinLow.process(s);
            s = cabTwinMid.process(s);
            s = cabTwinLp1.process(s);
            s = cabTwinLp2.process(s);
            return s;
        } else if (cabType == 1) {
            s = cabP10Low.process(s);
            s = cabP10Mid.process(s);
            s = cabP10Lp1.process(s);
            s = cabP10Lp2.process(s);
            return s * 1.02f;
        } else {
            s = cab412Low.process(s);
            s = cab412Mid.process(s);
            s = cab412Lp1.process(s);
            s = cab412Lp2.process(s);
            return s * 1.04f;
        }
    }

#if CYBER_ENABLE_STUDIO_DELAY
    /*
     * Exact Mixer Studio Tape / Ping-Pong Delay Engine (Ported from src/mixer_meter_daemon.c)
     * Features:
     *   - Hermite 4-Point Cubic Fractional Delay Interpolation
     *   - Rule 27 Dual Read-Head 45ms Equal-Power Crossfade (zero pitch dive-bomb on time changes)
     *   - Independent 1x and 2.7x Wow/Flutter phase accumulators
     *   - 15/30 IPS Tape Oxide Feedback Saturation & Input/Output HPF/LPF
     *   - Envelope Auto-Ducking
     */
    struct StudioDelayState {
        static const int BUF_LEN = 96000;
        float buf_l[BUF_LEN];
        float buf_r[BUF_LEN];
        int   write_idx;
        float smooth_samp;
        float smooth_samp_r;
        float in_hp_l, in_hp_r;
        float in_lp_l, in_lp_r;
        float fb_hp_l, fb_hp_r;
        float fb_lp_l, fb_lp_r;
        float out_hp_l, out_hp_r;
        float out_lp_l, out_lp_r;
        float ap_l[512];
        float ap_r[512];
        int   ap_idx;
        float mod_phase;
        float mod_phase2;
        float duck_env;
        float time_ms;
        float feedback;
        float diffuse;
        float wet;
        int   ping_pong;
        float in_lowcut;
        float in_highcut;
        float out_lowcut;
        float out_highcut;
        float drive;
        float flutter;
        float mod_rate;
        float mod_depth;
        float ducking;
        float stereo;
        float spread;
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
        StudioDelayState *dl = &dlyUnit;
        if (dl->wet <= 0.05f && (!dl->sm_init || dl->sm_wet <= 0.0005f)) {
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
        float dly_in_hp_a  = 1.0f - expf(-2.0f * (float)M_PI * std::max(20.0f, dl->in_lowcut) / fs);
        float dly_in_lp_a  = 1.0f - expf(-2.0f * (float)M_PI * std::min(18000.0f, std::max(800.0f, dl->in_highcut)) / fs);
        float dly_out_hp_a = 1.0f - expf(-2.0f * (float)M_PI * std::max(20.0f, dl->out_lowcut) / fs);
        float dly_out_lp_a = 1.0f - expf(-2.0f * (float)M_PI * std::min(18000.0f, std::max(800.0f, dl->out_highcut)) / fs);
        float dly_drive_amt = dl->drive * 0.01f;
        float dly_mod_inc   = (0.22f + (dl->mod_rate * 0.038f) + (dl->flutter * 0.042f)) * 2.0f * (float)M_PI / fs;
        float dly_mod_dep   = ((dl->mod_depth * 0.01f) * 16.0f) + ((dl->flutter * 0.01f) * 9.0f);
        float dly_duck_amt  = dl->ducking * 0.01f;

        const float fx_sm_k = 1.0f - expf(-1.0f / (0.020f * fs));
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
        float wow_l = sinf(dl->mod_phase) * dly_mod_dep + 0.35f * sinf(dl->mod_phase2) * dly_mod_dep;
        float wow_r = cosf(dl->mod_phase) * dly_mod_dep + 0.35f * cosf(dl->mod_phase2) * dly_mod_dep;

        int w_idx = dl->write_idx;
        float d_samp_l = dl->smooth_samp + wow_l;
        float d_samp_r = dl->smooth_samp_r + wow_r;

        float dly_out_l = readFracHermite(dl->buf_l, StudioDelayState::BUF_LEN, w_idx, d_samp_l);
        float dly_out_r = readFracHermite(dl->buf_r, StudioDelayState::BUF_LEN, w_idx, d_samp_r);
        if (dl->xf_active) {
            float n_l = readFracHermite(dl->buf_l, StudioDelayState::BUF_LEN, w_idx, dl->xf_target + wow_l);
            float n_r = readFracHermite(dl->buf_r, StudioDelayState::BUF_LEN, w_idx, dl->xf_target_r + wow_r);
            float th = dl->xf_pos * 1.5707963f;
            float g_old = cosf(th), g_new = sinf(th);
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
            float norm_drv = 1.0f / tanhf(drv);
            fb_l = tanhf(fb_l * drv) * norm_drv;
            fb_r = tanhf(fb_r * drv) * norm_drv;
        }

        if (dl->ping_pong) {
            float mono_in = 0.5f * (dly_in_l + dly_in_r);
            dl->buf_l[w_idx] = mono_in + fb_r * dl->sm_fb;
            dl->buf_r[w_idx] = fb_l * dl->sm_fb;
        } else {
            dl->buf_l[w_idx] = dly_in_l + fb_l * dl->sm_fb;
            dl->buf_r[w_idx] = dly_in_r + fb_r * dl->sm_fb;
        }
        if (++dl->write_idx >= StudioDelayState::BUF_LEN) dl->write_idx = 0;

        dl->out_hp_l += dl->sm_out_hp_a * (dly_out_l - dl->out_hp_l);
        dl->out_hp_r += dl->sm_out_hp_a * (dly_out_r - dl->out_hp_r);
        dly_out_l -= dl->out_hp_l;
        dly_out_r -= dl->out_hp_r;
        dl->out_lp_l += dl->sm_out_lp_a * (dly_out_l - dl->out_lp_l);
        dl->out_lp_r += dl->sm_out_lp_a * (dly_out_r - dl->out_lp_r);
        dly_out_l = dl->out_lp_l;
        dly_out_r = dl->out_lp_r;

        float dly_in_pk = std::max(fabsf(inL), fabsf(inR));
        if (dly_in_pk > dl->duck_env) dl->duck_env = 0.99f * dl->duck_env + 0.01f * dly_in_pk;
        else dl->duck_env = 0.9995f * dl->duck_env + 0.0005f * dly_in_pk;
        float dly_duck_g = 1.0f - std::min(0.85f, dl->duck_env * dly_duck_amt * 2.5f);

        float dly_mid  = 0.5f * (dly_out_l + dly_out_r);
        float dly_side = 0.5f * (dly_out_l - dly_out_r) * dl->sm_width;
        outL = inL + (dly_mid + dly_side) * dly_duck_g * dl->sm_wet;
        outR = inR + (dly_mid - dly_side) * dly_duck_g * dl->sm_wet;
    }

    void setDelayParams(float timeMs, float feedbackPct, float mixPct, float tapeDrivePct, float flutterPct, int pingPong, float duckingPct) {
        dlyUnit.time_ms   = std::max(10.0f, std::min(1800.0f, timeMs));
        dlyUnit.feedback  = std::max(0.0f, std::min(95.0f, feedbackPct));
        dlyUnit.wet       = std::max(0.0f, std::min(100.0f, mixPct));
        dlyUnit.drive     = std::max(0.0f, std::min(100.0f, tapeDrivePct));
        dlyUnit.flutter   = std::max(0.0f, std::min(100.0f, flutterPct));
        dlyUnit.ping_pong = pingPong ? 1 : 0;
        dlyUnit.ducking   = std::max(0.0f, std::min(100.0f, duckingPct));
    }
#endif

    inline float audioTaper(float val0to10, float maxGain) {
        if (val0to10 <= 0.05f) return 0.0f;
        float norm = val0to10 / 10.0f;
        return (0.22f * norm + 0.78f * norm * norm * norm) * (maxGain * 1.45f);
    }

    void updateToneStacks(
        float bass, float mid, float treb, float pres,
        bool brightSwitch, float cleanVol, int slopeMode, float osRate
    ) {
        // 1. Blackface AB763 Plate-Driven Tone Stack (Scooped 480Hz mids, shimmering highs)
        float bfBassDb = -3.0f + (bass / 10.0f) * 14.5f;
        bfBass.setLowShelf(105.0f, bfBassDb, 0.707f, osRate);

        float bfMidDb = -7.5f + (mid / 10.0f) * 9.5f; // Classic AB763 mid scoop
        bfMid.setPeaking(480.0f, bfMidDb, 1.15f, osRate);

        float bfTrebDb = -2.0f + (treb / 10.0f) * 15.0f;
        bfTreble.setHighShelf(2800.0f, bfTrebDb, 0.707f, osRate);

        // Authentic Fender Blackface 120pF Bright Cap:
        // In real AB763 circuitry, the 120pF cap is bridged across the volume pot.
        // At low volume, it adds sparkle; as the volume pot turns up toward 10, the wiper shorts the cap out.
        float brightBoost = (brightSwitch) ? (4.2f * std::max(0.0f, 1.0f - (cleanVol / 10.0f))) : 0.0f;
        if (brightBoost > 0.05f) {
            bfBrightShelf.setHighShelf(3400.0f, brightBoost, 0.707f, osRate);
        } else {
            bfBrightShelf.setIdentity();
        }

        // 2. 5F6-A Bassman / 1987 JMP Cathode-Follower Tone Stack
        // slopeMode 0 = 56k Fender Bassman (woody 580Hz), slopeMode 1 = 33k Marshall JMP (forward 780Hz)
        float bmBassFreq = (slopeMode == 1) ? 135.0f : 112.0f;
        float bmMidFreq  = (slopeMode == 1) ? 780.0f : 580.0f;
        float bmMidBoost = (slopeMode == 1) ? 2.4f   : 0.8f;

        float bmBassDb = -2.0f + (bass / 10.0f) * 14.0f;
        bmBass.setLowShelf(bmBassFreq, bmBassDb, 0.707f, osRate);

        float bmMidDb = bmMidBoost + (mid - 5.0f) * 2.5f;
        bmMid.setPeaking(bmMidFreq, bmMidDb, 1.10f, osRate);

        float bmTrebDb = (treb - 4.5f) * 2.6f;
        bmTreble.setHighShelf(2500.0f, bmTrebDb, 0.707f, osRate);

        // 3. Negative Feedback Presence Filter
        float presDb = (pres / 10.0f) * 7.5f;
        presenceFilter.setHighShelf(3600.0f, presDb, 0.707f, (float)sampleRate);

        // 4. Brownface Harmonic Vibrato Crossover at 700 Hz
        tremLp.setLowPass(700.0f, 0.707f, (float)sampleRate);
    }

    /*
     * Single-Sub-Sample Core Nonlinear Tube Preamp + Power Stage Solver
     * Runs at 1x, 2x, 4x, or 8x oversampled rate depending on Quality Mode.
     */
    inline float processNonlinearCoreSubSample(
        float s, int chMode, float cleanGain, float driveGain, float driveVol,
        float masterGain, int powerTubes, int nrIters, int qualityMode, float osRate
    ) {
        float ch1Out = 0.0f;
        float ch2Out = 0.0f;

        // Path A: Channel 1 - '65 Blackface AB763 Glassy Clean
        if (chMode == 0 || chMode == 2) {
            float bf = solveTriodeNR(s * 1.15f, gridCapBf1, platePrevBf1, 1.35f, 0.05f, sagVoltage, nrIters);
            bf = bfBrightShelf.process(bf);
            bf = bfBass.process(bf);
            bf = bfMid.process(bf);
            bf = bfTreble.process(bf);
            bf = dcBlockerBf.hp(bf, 14.0f, osRate); // Interstage AC coupling capacitor (0.022uF)
            bf *= cleanGain;
            bf = solveTriodeNR(bf, gridCapBf2, platePrevBf2, 1.20f, 0.04f, sagVoltage, nrIters);
            ch1Out = snubberBf.lp(bf, 12500.0f, osRate);
        }

        // Path B: Channel 2 - '59 Tweed Bassman 5F6-A / Marshall JMP Cathode-Follower Crunch
        if (chMode == 1 || chMode == 2) {
            float bm = solveTriodeNR(s * 1.25f, gridCapBm1, platePrevBm1, 1.55f, 0.08f, sagVoltage, nrIters);
            bm = dcBlockerBm.hp(bm, 14.0f, osRate); // Interstage AC coupling capacitor
            bm *= driveGain;
            bm = solveTriodeNR(bm, gridCapBm2, platePrevBm2, 1.95f, 0.10f, sagVoltage, nrIters);
            // Direct-Coupled 12AX7 Cathode Follower driving the Tone Stack
            bm = solveCathodeFollowerNR(bm, gridCapCf, cfPrev, nrIters);
            bm = bmBass.process(bm);
            bm = bmMid.process(bm);
            bm = bmTreble.process(bm);
            bm *= driveVol;
            ch2Out = snubberBm.lp(bm, 11200.0f, osRate);
        }

        float mixedPre = 0.0f;
        if (chMode == 0) {
            mixedPre = ch1Out;
        } else if (chMode == 1) {
            mixedPre = ch2Out;
        } else {
            // Jumpered Parallel Blend (Glassy Blackface bottom/top + Chewy Bassman midrange)
            mixedPre = (ch1Out * 0.78f + ch2Out * 0.78f);
        }

        // Push-Pull Power Section (6L6GC or EL34) + Output Transformer Hysteresis
        float pOut = processPowerStage(mixedPre, masterGain, powerTubes, sagVoltage, qualityMode);
        return pOut;
    }

    void process(
        const float* input, float* outputL, float* outputR, uint32_t n_samples,
        float bypass,
        float pChannelMode,   // 0 = Blackface Clean, 1 = Bassman/JMP Drive, 2 = Jumpered Blend
        float pCleanVol,      // 0..10 Blackface Clean Volume
        float pDriveGain,     // 0..10 Bassman/JMP Preamp Gain
        float pDriveVol,      // 0..10 Bassman/JMP Channel Volume
        float pBass,          // 0..10
        float pMid,           // 0..10
        float pTreble,        // 0..10
        float pBrightSwitch,  // 0/1 Blackface 120pF Bright Cap
        float pSlopeMode,     // 0 = 56k Fender Bassman, 1 = 33k Marshall JMP
        float pPowerTubes,    // 0 = 6L6GC American, 1 = EL34 British
        float pMaster,        // 0..10 Power Amp Master Volume
        float pPresence,      // 0..10 Negative Feedback Presence
        float pSagAmount,     // 0..10 B+ Tube Rectifier Sag Depth
        float pRevDwell,      // 0..10 12AT7 Spring Reverb Driver Dwell
        float pRevMix,        // 0..100% Spring Reverb Wet Level
        float pRevTone,       // 0..10 Spring Reverb Tone
        float pRevDrip,       // 0..10 Spring Dispersion Drip
        float pTremSpeed,     // 0.5..12 Hz Harmonic Tube Vibrato Speed
        float pTremDepth,     // 0..100% Harmonic Tube Vibrato Intensity
        float pTremMode,      // 0 = Opto Bias Tremolo, 1 = Brownface Harmonic Tube Vibrato
        float pQualityMode,   // 0 = Live Eco (1x), 1 = Stage (2x), 2 = Studio (4x), 3 = Ultra Bounce (8x)
        float pSpeakerCab,    // 0 = 2x12 Twin C12N, 1 = 4x10 Bassman P10R, 2 = 4x12 Greenback, 3 = Bypass
        float pSpeakerDrive,  // 0..10 Speaker Cone Stress
        float pOutputLevel,   // 0..10 Final Attenuator
        float pNoiseGate = 1.0f,
        float pNoiseSpectral = 1.0f,
        float pNoiseDefizz = 1.0f,
        float pNoiseLevel = 5.0f,
        bool isOfflineBounce = false
    ) {
        if (bypass < 0.5f) {
            if (outputL != input) std::memcpy(outputL, input, n_samples * sizeof(float));
            if (outputR && outputR != input) std::memcpy(outputR, input, n_samples * sizeof(float));
            return;
        }

        int chMode = std::max(0, std::min(2, (int)lroundf(pChannelMode)));
        int slopeMode = (pSlopeMode > 0.5f) ? 1 : 0;
        int powerTubes = (pPowerTubes > 0.5f) ? 1 : 0;
        int cabType = std::max(0, std::min(3, (int)lroundf(pSpeakerCab)));
        bool brightSw = (pBrightSwitch > 0.5f);
        bool isHarmonicVib = (pTremMode > 0.5f);
        bool useGate = (pNoiseGate > 0.5f);

        // Scalable Quality / Oversampling Mode (Auto-Elevates to 3 = 8x Ultra Physics during DAW Offline Bounce!)
        int qMode = isOfflineBounce ? 3 : std::max(0, std::min(3, (int)lroundf(pQualityMode)));
        int osFactor = (qMode == 3) ? 8 : ((qMode == 2) ? 4 : ((qMode == 1) ? 2 : 1));
        int nrIters  = (qMode == 3) ? 8 : ((qMode == 2) ? 5 : ((qMode == 1) ? 3 : 2));
        float osRate = (float)sampleRate * (float)osFactor;

        updateToneStacks(pBass, pMid, pTreble, pPresence, brightSw, pCleanVol, slopeMode, osRate);

        float cleanGain  = audioTaper(pCleanVol, 2.35f);
        float driveGain  = audioTaper(pDriveGain, (slopeMode == 1) ? 3.85f : 3.10f);
        float driveVol   = audioTaper(pDriveVol, 2.20f);
        float masterGain = audioTaper(pMaster, 2.95f);
        // Automatic inverse loudness compensation on Master so cranking power tubes doesn't blow out downstream
        float masterComp = 1.0f / (1.0f + 0.32f * std::max(0.0f, pMaster - 4.0f));
        float sagDepth   = std::max(0.0f, std::min(10.0f, pSagAmount)) / 10.0f;

        float revDwellNorm = std::max(0.0f, std::min(10.0f, pRevDwell)) * 0.1f;
        float revMixNorm   = std::max(0.0f, std::min(100.0f, pRevMix)) * 0.01f;
        float revToneNorm  = std::max(0.0f, std::min(10.0f, pRevTone)) * 0.1f;
        float revDripNorm  = std::max(0.0f, std::min(10.0f, pRevDrip)) * 0.1f;

        float lfoInc  = (2.0f * (float)M_PI * std::max(0.5f, std::min(12.0f, pTremSpeed))) / (float)sampleRate;
        float tremAmt = std::max(0.0f, std::min(100.0f, pTremDepth)) * 0.01f;

        float outTrim = 1.0f;
        if (pOutputLevel <= 0.05f) {
            outTrim = 0.0f;
        } else {
            float db = (pOutputLevel - 7.0f) * 3.5f;
            outTrim = powf(10.0f, db / 20.0f);
        }

        for (uint32_t i = 0; i < n_samples; ++i) {
            float rawIn = input[i];
            float s = dcBlockerPre.hp(rawIn, 24.0f, (float)sampleRate);

            // Stage 1 & 2: Polyphase Oversampled Nonlinear Preamp + Cathode Follower + Power Stage
            float pOut = 0.0f;
            if (osFactor == 1) {
                pOut = processNonlinearCoreSubSample(s, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
            } else if (osFactor == 2) {
                float u0, u1;
                upStage1.upsample2x(s, u0, u1);
                float y0 = processNonlinearCoreSubSample(u0, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
                float y1 = processNonlinearCoreSubSample(u1, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
                pOut = dnStage1.downsample2x(y0, y1);
            } else if (osFactor == 4) {
                float u0, u1, u00, u01, u10, u11;
                upStage1.upsample2x(s, u0, u1);
                upStage2[0].upsample2x(u0, u00, u01);
                upStage2[1].upsample2x(u1, u10, u11);
                float y00 = processNonlinearCoreSubSample(u00, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
                float y01 = processNonlinearCoreSubSample(u01, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
                float y10 = processNonlinearCoreSubSample(u10, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
                float y11 = processNonlinearCoreSubSample(u11, chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
                float d0 = dnStage2[0].downsample2x(y00, y01);
                float d1 = dnStage2[1].downsample2x(y10, y11);
                pOut = dnStage1.downsample2x(d0, d1);
            } else {
                // 8x Ultra Oversampling (Studio Bounce Mode)
                float u2[2], u4[4], u8[8], y8[8], d4[4], d2[2];
                upStage1.upsample2x(s, u2[0], u2[1]);
                for (int k = 0; k < 2; ++k) upStage2[k].upsample2x(u2[k], u4[k * 2], u4[k * 2 + 1]);
                for (int k = 0; k < 4; ++k) upStage3[k].upsample2x(u4[k], u8[k * 2], u8[k * 2 + 1]);
                for (int k = 0; k < 8; ++k) {
                    y8[k] = processNonlinearCoreSubSample(u8[k], chMode, cleanGain, driveGain, driveVol, masterGain, powerTubes, nrIters, qMode, osRate);
                }
                for (int k = 0; k < 4; ++k) d4[k] = dnStage3[k].downsample2x(y8[k * 2], y8[k * 2 + 1]);
                for (int k = 0; k < 2; ++k) d2[k] = dnStage2[k].downsample2x(d4[k * 2], d4[k * 2 + 1]);
                pOut = dnStage1.downsample2x(d2[0], d2[1]);
            }

            pOut *= masterComp;
            pOut = snubberPower.lp(pOut, 11200.0f, (float)sampleRate);

            // Stage 3: Dynamic B+ Power Rail Sag (Decimated 4x in Eco mode, per-sample in Studio mode)
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

            pOut = presenceFilter.process(pOut);
            pOut = dcBlockerPost.hp(pOut, 22.0f, (float)sampleRate);

            // Stage 4: 3-Spring Accutronics Tube Reverb Tank (with 12AT7 Driver & Dispersion Drip)
            pOut = processSpringReverb(pOut, revDwellNorm, revMixNorm, revToneNorm, revDripNorm);

            // Stage 5: Brownface 6G-Series / Koch "The Greg" Harmonic Tube Vibrato
            if (tremAmt > 0.005f) {
                float lfo = sinf(lfoPhase);
                lfoPhase += lfoInc;
                if (lfoPhase > 2.0f * (float)M_PI) lfoPhase -= 2.0f * (float)M_PI;

                if (isHarmonicVib) {
                    // Split into Low (<700Hz) and High (>700Hz) bands and modulate through counter-phase triodes
                    float sLow = tremLp.process(pOut);
                    float sHigh = pOut - sLow;
                    float modL = 1.0f + (lfo * 0.68f * tremAmt);
                    float modH = 1.0f - (lfo * 0.68f * tremAmt);
                    float vLow  = tanhf(sLow * modL * 1.12f) * 0.92f;
                    float vHigh = tanhf(sHigh * modH * 1.12f) * 0.92f;
                    pOut = (vLow + vHigh) * 0.94f;
                } else {
                    // Classic Blackface Opto / Bias Tremolo
                    float mod = 1.0f - (tremAmt * 0.55f * (1.0f + lfo));
                    pOut *= mod;
                }
            }

            // Stage 6: Speaker Cabinet & Dynamic Cone Stress
            pOut = processSpeaker(pOut, cabType, pSpeakerDrive);

            // Stage 7: Smart Zero-Floor Noise Suppressor Lab
            float noiseSens = std::max(0.0f, std::min(10.0f, pNoiseLevel)) / 5.0f;
            if (pNoiseSpectral > 0.5f && noiseSens > 0.01f) {
                float b[10];
                specLpStates[0] += (pOut - specLpStates[0]) * specBCoeffs[0];
                b[0] = specLpStates[0];
                for (int band = 1; band < 9; ++band) {
                    specLpStates[band] += (pOut - specLpStates[band]) * specBCoeffs[band];
                    b[band] = specLpStates[band] - specLpStates[band - 1];
                }
                b[9] = pOut - specLpStates[8];

                float spectralSum = 0.0f;
                for (int band = 0; band < 10; ++band) {
                    float absB = fabsf(b[band]);
                    float coeff = (absB > specLevelStates[band]) ? specLevelAtt : specLevelRel;
                    specLevelStates[band] += (absB - specLevelStates[band]) * coeff;
                    float r = specLevelStates[band];

                    float targetG = 1.0f - (specTLinear[band] * noiseSens / (r + 1e-9f));
                    targetG = std::max(0.0f, std::min(1.0f, targetG));

                    float currentCoeff;
                    if (targetG > specCurrentGains[band]) {
                        currentCoeff = specAttCoeff;
                    } else {
                        float tailStab = 1.0f + (1.0f - std::min(1.0f, specCurrentGains[band])) * 8.0f;
                        currentCoeff = 1.0f - expf(-1.0f / (0.010f * tailStab * (float)sampleRate));
                    }
                    specCurrentGains[band] += (targetG - specCurrentGains[band]) * currentCoeff;
                    spectralSum += b[band] * specCurrentGains[band];
                }
                pOut = spectralSum;
            }

            if (pNoiseDefizz > 0.5f && noiseSens > 0.01f) {
                float absP = fabsf(pOut);
                if (absP > defizzEnv) defizzEnv += defizzAtk * (absP - defizzEnv);
                else defizzEnv += defizzRel * (absP - defizzEnv);

                float normLevel = (defizzEnv - 0.0005f) / (0.0300f - 0.0005f);
                normLevel = std::max(0.0f, std::min(1.0f, normLevel));

                float dynCutoff = 2800.0f + normLevel * 11200.0f;
                float defizzW = 2.0f * (float)M_PI * dynCutoff / (float)sampleRate;
                float defizzAlpha = defizzW / (1.0f + defizzW);
                defizzLpState += defizzAlpha * (pOut - defizzLpState);
                pOut = defizzLpState;

                float expTarget = (normLevel > 0.15f) ? 1.0f : (1.0f - (1.0f - (normLevel / 0.15f)) * std::min(1.0f, noiseSens));
                defizzExpanderGain += 0.005f * (expTarget - defizzExpanderGain);
                pOut *= defizzExpanderGain;
            }

            if (useGate && noiseSens > 0.01f) {
                float sc = gateScLp.lp(gateScHp.hp(rawIn, 100.0f, (float)sampleRate), 3200.0f, (float)sampleRate);
                float absSc = fabsf(sc);
                if (absSc > gateEnv) gateEnv += gateAtk * (absSc - gateEnv);
                else gateEnv += gateRel * (absSc - gateEnv);

                float threshOpen  = 0.00100f * noiseSens;
                float threshClose = 0.00045f * noiseSens;

                if (!gateIsOpen) {
                    if (gateEnv >= threshOpen) gateIsOpen = true;
                } else {
                    if (gateEnv < threshClose) gateIsOpen = false;
                }

                float targetGain = gateIsOpen ? 1.0f : 0.0f;
                float smoothRate = (targetGain > gateGain) ? gateAtkSmooth : gateRelSmooth;
                gateGain += smoothRate * (targetGain - gateGain);
                pOut *= gateGain;
            } else {
                gateGain = 1.0f;
                gateIsOpen = true;
            }

            pOut *= outTrim;

            float finalL = pOut;
            float finalR = pOut;

#if CYBER_ENABLE_STUDIO_DELAY
            processStudioDelaySample(pOut, pOut, finalL, finalR);
#endif

            outputL[i] = finalL;
            if (outputR) outputR[i] = finalR;
        }
    }

private:
    double sampleRate;

    // Preamp Tone Stacks (Blackface AB763 & Bassman/JMP 5F6-A)
    HybridBiquad bfBass, bfMid, bfTreble, bfBrightShelf;
    HybridBiquad bmBass, bmMid, bmTreble;
    HybridBiquad presenceFilter;
    HybridBiquad tremLp;

    // Speaker Cabinets (Twin 2x12 C12N, Bassman 4x10 P10R, Marshall 4x12 G12M)
    HybridBiquad cabTwinLow, cabTwinMid, cabTwinLp1, cabTwinLp2;
    HybridBiquad cabP10Low,  cabP10Mid,  cabP10Lp1,  cabP10Lp2;
    HybridBiquad cab412Low,  cab412Mid,  cab412Lp1,  cab412Lp2;

    // Polyphase Half-Band Oversampling Filter Tree (1x / 2x / 4x / 8x)
    HalfBandPolyphase upStage1, upStage2[2], upStage3[4];
    HalfBandPolyphase dnStage1, dnStage2[2], dnStage3[4];

    HybridOnePole snubberBf, snubberBm, snubberPower;
    HybridOnePole dcBlockerPre, dcBlockerPost, dcBlockerBf, dcBlockerBm;

    // Tube & Transformer State
    float sagVoltage, sagScreenGrid;
    uint32_t sagCounter;
    float gridCapBf1, gridCapBf2;
    float gridCapBm1, gridCapBm2, gridCapCf;
    float platePrevBf1, platePrevBf2, platePrevBm1, platePrevBm2, cfPrev;
    float otFlux, otPrevIn;

    // Harmonic Vibrato State
    float lfoPhase;
    float vibTriodeBiasL, vibTriodeBiasH;

    // 3-Spring Accutronics Reverb Tank State
    static const int NUM_SPRINGS = 3;
    static const int SPRING_MAX_DELAY = 16384;
    static const int REV_DIFF_DELAY1 = 128;
    static const int REV_DIFF_DELAY2 = 192;
    static const int DISP_STAGES = 16;
    float springBuffers[NUM_SPRINGS][SPRING_MAX_DELAY];
    int   springDelays[NUM_SPRINGS];
    int   springWritePos[NUM_SPRINGS];
    float springLp[NUM_SPRINGS];
    float dispX[NUM_SPRINGS][DISP_STAGES];
    float dispY[NUM_SPRINGS][DISP_STAGES];
    float revDiffBuf1[REV_DIFF_DELAY1];
    float revDiffBuf2[REV_DIFF_DELAY2];
    int   revDiffIdx1, revDiffIdx2;
    float revOutToneLp, revDriverBias;
    float smoothedRevMix, smoothedRevDecay;

    // Dynamic Speaker Cone State
    float speakerEnv, speakerThermalEnv, speakerConeHistory;
    float spkAtk, spkRel, spkThermalRel;

    // Smart Zero-Floor Noise Suppressor Lab
    HybridOnePole gateScHp, gateScLp;
    float gateEnv, gateGain, gateAtk, gateRel, gateAtkSmooth, gateRelSmooth;
    bool  gateIsOpen;
    float specLpStates[9];
    float specLevelStates[10];
    float specCurrentGains[10];
    float specThresholdsDb[10];
    float specTLinear[10];
    float specBCoeffs[9];
    float specLevelAtt, specLevelRel, specAttCoeff;
    float defizzEnv, defizzLpState, defizzExpanderGain, defizzAtk, defizzRel;

#if CYBER_ENABLE_STUDIO_DELAY
    StudioDelayState dlyUnit;
#endif
};

} // namespace CyberDSP

#endif // CYBER_ULTIMATE_HYBRID_ENGINE_HPP
