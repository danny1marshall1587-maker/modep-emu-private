#pragma once

#define CYBER_ENABLE_STUDIO_DELAY 1
#include "EuphoriaAmpEngine.hpp"
#include "EuphoriaParams.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <vector>

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    EuphoriaDSP::EuphoriaAmpEngine* getEngine() const noexcept { return dspEngine.get(); }

private:
    // Raw (denormalised) parameter value by table index - same numbers the
    // Windows standalone reads from g_knobs[i].curVal.
    float V(int idx) const noexcept { return rawParams[(size_t)idx]->load(); }
    void pushParamsToEngine(uint32_t numSamples);

    std::unique_ptr<EuphoriaDSP::EuphoriaAmpEngine> dspEngine;
    std::vector<std::atomic<float>*> rawParams;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuphoriaAmpAudioProcessor)
};
