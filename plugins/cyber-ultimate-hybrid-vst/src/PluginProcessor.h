#pragma once

#define CYBER_ENABLE_STUDIO_DELAY 1
#include "CyberUltimateHybridEngine.hpp"
#include <juce_audio_processors/juce_audio_processors.h>

class CyberUltimateHybridAudioProcessor : public juce::AudioProcessor {
public:
    CyberUltimateHybridAudioProcessor();
    ~CyberUltimateHybridAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cyber Ultimate Hybrid"; }
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

private:
    std::unique_ptr<CyberDSP::CyberUltimateHybridEngine> dspEngine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CyberUltimateHybridAudioProcessor)
};
