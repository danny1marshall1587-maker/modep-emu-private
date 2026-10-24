#pragma once

#define CYBER_ENABLE_STUDIO_DELAY 1
#include "EuphoriaAmpEngine.hpp"
#include "EuphoriaParams.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>

// ============================================================================
// Precision Virtual Strobe Tuner Engine (Peterson / Sonic Research Sub-Cent)
// ============================================================================
struct EuphoriaStrobeTunerEngine {
    static constexpr int BUF_SIZE = 4096;
    float m_filtBuf[BUF_SIZE] = {};
    float m_rawBuf[BUF_SIZE]  = {};
    int   m_writeIdx = 0;
    float m_sampleRate = 48000.0f;

    float m_hp_x = 0.0f, m_hp_y = 0.0f;
    float m_lp_y1 = 0.0f, m_lp_y2 = 0.0f;

    float m_displayCents = 0.0f;
    float m_displayFreq = 0.0f;
    int   m_activeMidi = -1;
    int   m_candidateMidi = -1;
    int   m_candidateConfirm = 0;
    int   m_holdFrames = 0;

    std::atomic<float> detectedFreq{0.0f};
    std::atomic<float> detectedPitch{0.0f};
    std::atomic<float> centsDeviation{0.0f};
    std::atomic<float> cents{0.0f};
    std::atomic<float> targetFreq{0.0f};
    std::atomic<float> targetPitch{0.0f};
    std::atomic<float> refFreq{440.0f};
    std::atomic<float> confidence{0.0f};
    std::atomic<float> inRms{0.0f};
    std::atomic<int>   noteIdx{-1};
    std::atomic<int>   octave{0};
    std::atomic<int>   midiNote{-1};
    std::atomic<bool>  hasSignal{false};
    std::atomic<bool>  signalActive{false};
    std::atomic<float> strobeSpeed{0.0f};
    std::atomic<int>   closestStringIdx{-1};
    std::atomic<bool>  highPrecisionMode{true};

    void getNoteName(char* out) const {
        static const char* s_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int idx = noteIdx.load();
        if (idx >= 0 && idx < 12) std::strcpy(out, s_names[idx]);
        else std::strcpy(out, "--");
    }

    void reset(float sRate = 48000.0f) {
        m_sampleRate = (sRate > 20000.0f) ? sRate : 48000.0f;
        std::fill(m_filtBuf, m_filtBuf + BUF_SIZE, 0.0f);
        std::fill(m_rawBuf, m_rawBuf + BUF_SIZE, 0.0f);
        m_writeIdx = 0;
        m_hp_x = m_hp_y = 0.0f;
        m_lp_y1 = m_lp_y2 = 0.0f;
        m_displayCents = 0.0f;
        m_displayFreq = 0.0f;
        m_activeMidi = -1;
        m_candidateMidi = -1;
        m_candidateConfirm = 0;
        m_holdFrames = 0;

        detectedFreq.store(0.0f);
        detectedPitch.store(0.0f);
        centsDeviation.store(0.0f);
        cents.store(0.0f);
        targetFreq.store(0.0f);
        targetPitch.store(0.0f);
        confidence.store(0.0f);
        inRms.store(0.0f);
        noteIdx.store(-1);
        octave.store(0);
        midiNote.store(-1);
        hasSignal.store(false);
        signalActive.store(false);
        strobeSpeed.store(0.0f);
        closestStringIdx.store(-1);
    }

    float getSampleRate() const { return m_sampleRate; }

    void writeSamples(const float* in, int numSamples) {
        float hp_a = 1.0f / (1.0f + 2.0f * 3.14159265f * 30.0f / m_sampleRate);
        float lp_a = (2.0f * 3.14159265f * 700.0f) / (m_sampleRate + 2.0f * 3.14159265f * 700.0f);

        for (int i = 0; i < numSamples; ++i) {
            float s = in[i];
            float hp = hp_a * (m_hp_y + s - m_hp_x);
            m_hp_x = s;
            m_hp_y = hp;

            m_lp_y1 += lp_a * (hp - m_lp_y1);
            m_lp_y2 += lp_a * (m_lp_y1 - m_lp_y2);

            m_rawBuf[m_writeIdx] = s;
            m_filtBuf[m_writeIdx] = m_lp_y2;

            if (++m_writeIdx >= BUF_SIZE) m_writeIdx = 0;
        }
    }

    void process(float refA4 = 440.0f) {
        float effectiveRef = (refFreq.load() > 400.0f) ? refFreq.load() : refA4;
        constexpr int N = 2048;
        float x[N];
        int rIdx = (m_writeIdx - N + BUF_SIZE) % BUF_SIZE;

        float sumSq = 0.0f;
        for (int i = 0; i < N; ++i) {
            int idx = (rIdx + i) % BUF_SIZE;
            x[i] = m_filtBuf[idx];
            float raw = m_rawBuf[idx];
            sumSq += raw * raw;
        }

        float rms = std::sqrt(sumSq / float(N));
        inRms.store(rms);

        if (rms < 0.0025f) {
            if (m_holdFrames > 0) {
                m_holdFrames--;
                strobeSpeed.store(strobeSpeed.load() * 0.90f);
                return;
            }
            hasSignal.store(false);
            signalActive.store(false);
            strobeSpeed.store(0.0f);
            confidence.store(0.0f);
            return;
        }

        m_holdFrames = 18;
        hasSignal.store(true);
        signalActive.store(true);

        int minLag = int(m_sampleRate / 1050.0f);
        int maxLag = int(m_sampleRate / 30.0f);
        if (maxLag > N / 2) maxLag = N / 2;

        std::vector<float> d(maxLag + 1, 0.0f);
        d[0] = 1.0f;
        float runningSum = 0.0f;

        for (int tau = 1; tau <= maxLag; ++tau) {
            float sum = 0.0f;
            for (int j = 0; j < N - tau; ++j) {
                float delta = x[j] - x[j + tau];
                sum += delta * delta;
            }
            runningSum += sum;
            d[tau] = (runningSum > 1e-9f) ? (sum * float(tau) / runningSum) : 1.0f;
        }

        constexpr float YIN_THRESH = 0.15f;
        int bestTau = -1;
        float minValFound = 1.0f;
        for (int tau = minLag; tau <= maxLag - 1; ++tau) {
            if (d[tau] < YIN_THRESH) {
                while (tau + 1 <= maxLag && d[tau + 1] < d[tau]) tau++;
                bestTau = tau;
                minValFound = d[tau];
                break;
            }
        }

        if (bestTau < 0) {
            float minVal = 1.0f;
            for (int tau = minLag; tau <= maxLag; ++tau) {
                if (d[tau] < minVal) {
                    minVal = d[tau];
                    bestTau = tau;
                }
            }
            minValFound = minVal;
            if (minVal > 0.40f) {
                confidence.store(0.0f);
                return;
            }
        }

        int subTau = bestTau * 2;
        if (subTau <= maxLag) {
            int minK = std::max(minLag, subTau - 4);
            int maxK = std::min(maxLag, subTau + 4);
            int subBest = subTau;
            float subMin = d[subTau];
            for (int k = minK; k <= maxK; ++k) {
                if (d[k] < subMin) {
                    subMin = d[k];
                    subBest = k;
                }
            }
            if (subMin < 0.32f && subMin < d[bestTau] * 1.35f) {
                bestTau = subBest;
                minValFound = subMin;
            }
        }

        confidence.store(std::max(0.0f, std::min(1.0f, 1.0f - minValFound)));

        float s0 = (bestTau > 0) ? d[bestTau - 1] : d[bestTau];
        float s1 = d[bestTau];
        float s2 = (bestTau < maxLag) ? d[bestTau + 1] : d[bestTau];
        float curvature = s0 - 2.0f * s1 + s2;
        float deltaTau = 0.0f;
        if (curvature > 1e-6f) {
            deltaTau = 0.5f * (s0 - s2) / curvature;
            deltaTau = std::clamp(deltaTau, -0.5f, 0.5f);
        }
        float trueTau = float(bestTau) + deltaTau;

        float rawFreq = m_sampleRate / trueTau;
        if (rawFreq < 30.0f || rawFreq > 1200.0f) return;

        float semitones = 12.0f * std::log2(rawFreq / effectiveRef);
        float exactMidi = semitones + 69.0f;
        int rawMidi = int(std::round(exactMidi));

        if (m_activeMidi < 0) {
            m_activeMidi = rawMidi;
            m_displayFreq = rawFreq;
        } else if (rawMidi != m_activeMidi) {
            if (rawMidi == m_candidateMidi) {
                if (++m_candidateConfirm >= 2) {
                    m_activeMidi = rawMidi;
                    m_candidateConfirm = 0;
                    m_displayFreq = rawFreq;
                }
            } else {
                m_candidateMidi = rawMidi;
                m_candidateConfirm = 1;
            }
        } else {
            m_candidateConfirm = 0;
        }

        m_displayFreq = m_displayFreq * 0.40f + rawFreq * 0.60f;
        detectedFreq.store(m_displayFreq);
        detectedPitch.store(m_displayFreq);

        float tFreq = effectiveRef * std::pow(2.0f, float(m_activeMidi - 69) / 12.0f);
        targetFreq.store(tFreq);
        targetPitch.store(tFreq);

        float rawCents = 1200.0f * std::log2(m_displayFreq / tFreq);
        m_displayCents = m_displayCents * 0.35f + rawCents * 0.65f;
        centsDeviation.store(m_displayCents);
        cents.store(m_displayCents);

        int nIdx = (m_activeMidi % 12 + 12) % 12;
        noteIdx.store(nIdx);
        octave.store((m_activeMidi / 12) - 1);
        midiNote.store(m_activeMidi);

        float spd = 0.0f;
        if (std::abs(m_displayCents) > 0.15f) {
            float mult = highPrecisionMode.load() ? 0.045f : 0.025f;
            spd = std::clamp(m_displayCents * mult, -3.5f, 3.5f);
        }
        strobeSpeed.store(spd);
    }
};

struct EuphoriaTestGuitarSynth {
    double phase = 0.0;
    double noteEnv = 0.0;
    int sampleCounter = 0;
    int stepIdx = 0;
    double sampleRate = 48000.0;

    float nextSample() {
        static const double chordNotes[] = { 82.41, 110.0, 146.83, 196.00, 246.94, 329.63, 220.0, 164.81 };
        const int stepSamples = std::max(1, int(sampleRate / 3.0));
        if (sampleCounter % stepSamples == 0) {
            noteEnv = 1.0;
            stepIdx = (stepIdx + 1) % 8;
        }
        sampleCounter++;
        const double freq = chordNotes[stepIdx];
        phase += freq / sampleRate;
        if (phase >= 1.0) phase -= 1.0;
        noteEnv *= 0.99988;
        const double p2 = std::fmod(phase * 2.0, 1.0);
        const double p3 = std::fmod(phase * 3.0, 1.0);
        const double raw = (std::sin(phase * 6.2831853) * 0.65
                          + std::sin(p2 * 6.2831853) * 0.25
                          + std::sin(p3 * 6.2831853) * 0.10) * noteEnv;
        return float(raw * 0.45);
    }
};

class EuphoriaAmpAudioProcessor : public juce::AudioProcessor {
public:
    EuphoriaAmpAudioProcessor();
    ~EuphoriaAmpAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Euphoria Amp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPresetIdx; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    EuphoriaDSP::EuphoriaAmpEngine* getEngine() const noexcept { return dspEngine.get(); }

    // Parameter access helpers for the 3D Hardware GUI
    float getParamValue(int idx) const noexcept { return V(idx); }
    void setParamValueFromGui(int idx, float denormVal);

    // Preset system matching StandaloneWinMain.cpp
    struct PresetItem {
        juce::String name;
        float vals[K_NUM_PARAMS];
    };
    std::vector<PresetItem> presets;
    int currentPresetIdx = 0;
    void initFactoryPresets();
    void applyPreset(int idx);
    void saveUserPreset(const juce::String& name);

    // Live hardware telemetry & tuner
    EuphoriaStrobeTunerEngine tuner;
    EuphoriaTestGuitarSynth   riffSynth;
    std::atomic<bool>  testRiffActive{false};
    std::atomic<bool>  tunerMute{false};
    std::atomic<float> inMeterPeak{0.0f};
    std::atomic<float> outMeterPeakL{0.0f};
    std::atomic<float> outMeterPeakR{0.0f};
    std::atomic<float> dspCpuPercent{1.8f};
    std::atomic<float> dspTimeUs{95.0f};
    std::atomic<int>   dspInternalRate{192000};
    std::atomic<int>   currentBlockSize{256};

private:
    float V(int idx) const noexcept { return rawParams[(size_t)idx]->load(); }
    void pushParamsToEngine(uint32_t numSamples);

    std::unique_ptr<EuphoriaDSP::EuphoriaAmpEngine> dspEngine;
    std::vector<std::atomic<float>*> rawParams;
    std::vector<float> monoInScratch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuphoriaAmpAudioProcessor)
};
