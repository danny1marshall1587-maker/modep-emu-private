#pragma once
#include <JuceHeader.h>

//==============================================================================
class TunerBackgroundWorker : public juce::Thread
{
public:
    TunerBackgroundWorker() : juce::Thread("TunerWorker") {}

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        fifoSize = 8192;
        audioFifo.assign(fifoSize, 0.0f);
        analysisBuffer.assign(4096, 0.0f);
        autoGain = 1.0f;
        dcY1 = 0.0f; dcX1 = 0.0f;
        startThread();
    }

    void pushSamples(const float* data, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            audioFifo[writeIndex] = data[i];
            writeIndex = (writeIndex + 1) % fifoSize;
        }
        samplesAvailable.fetch_add(numSamples);
        if (samplesAvailable.load() > fifoSize) samplesAvailable.store(fifoSize);
        notify();
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            if (samplesAvailable.load() >= 2048)
            {
                performAnalysis();
                samplesAvailable.fetch_sub(1024);
            }
            else
            {
                wait(10);
            }
        }
    }

    float getFrequency() const { return frequency.load(); }

private:
    double sr = 44100.0;
    std::vector<float> audioFifo;
    int writeIndex = 0, readIndex = 0, fifoSize = 8192;
    std::atomic<int> samplesAvailable { 0 };

    std::vector<float> analysisBuffer;
    std::atomic<float> frequency { 0.0f };
    float autoGain = 1.0f;
    float dcY1 = 0.0f, dcX1 = 0.0f;

    void performAnalysis()
    {
        int tempReadIndex = readIndex;
        float peak = 0.0f;
        const float R = 0.995f;
        
        std::vector<float> rawWindow(2048);
        for (int i = 0; i < 2048; ++i)
        {
            float x = audioFifo[tempReadIndex];
            float y = x - dcX1 + R * dcY1;
            dcX1 = x; dcY1 = y;
            rawWindow[i] = y;
            peak = std::max(peak, std::abs(y));
            tempReadIndex = (tempReadIndex + 1) % fifoSize;
        }
        readIndex = (readIndex + 1024) % fifoSize;

        if (peak < 0.0001f) { 
            frequency.store(0.0f); 
            return; 
        }

        float targetLevel = 0.6f;
        float targetGain = targetLevel / peak;
        autoGain = autoGain * 0.8f + targetGain * 0.2f;
        autoGain = std::clamp(autoGain, 0.1f, 5000.0f);
        
        for (int i = 0; i < 2048; ++i) analysisBuffer[i] = rawWindow[i] * autoGain;

        detectPitch();
    }

    void detectPitch()
    {
        int minLag = static_cast<int>(sr / 1200.0);
        int maxLag = static_cast<int>(sr / 40.0);
        int correlationWindow = 1024;
        int requiredSize = maxLag + correlationWindow + 10;
        if (requiredSize > (int)analysisBuffer.size()) requiredSize = (int)analysisBuffer.size();

        std::vector<float> clipped(requiredSize, 0.0f);
        float clipThreshold = 0.25f;
        for (int i = 0; i < requiredSize; ++i) {
            float val = analysisBuffer[i];
            if (val > clipThreshold) clipped[i] = val - clipThreshold;
            else if (val < -clipThreshold) clipped[i] = val + clipThreshold;
            else clipped[i] = 0.0f;
        }

        float maxCorr = -1.0f; int bestLag = -1;
        for (int lag = minLag; lag < maxLag; ++lag) {
            if (lag + correlationWindow > requiredSize) break;
            float corr = 0.0f;
            for (int i = 0; i < correlationWindow; ++i) corr += clipped[i] * clipped[i+lag];
            if (corr > maxCorr) { maxCorr = corr; bestLag = lag; }
        }
        
        float power = 0.0f;
        for (int i = 0; i < correlationWindow; ++i) power += clipped[i] * clipped[i];
        if (bestLag > 0 && maxCorr > power * 0.4f)
            frequency.store(static_cast<float>(sr / bestLag));
        else
            frequency.store(0.0f);
    }
};

//==============================================================================
class GalaxyStrobeTuneAudioProcessor  : public juce::AudioProcessor
{
public:
    GalaxyStrobeTuneAudioProcessor();
    ~GalaxyStrobeTuneAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Galaxy Strobe Tune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getCurrentFrequency() const { return worker.getFrequency(); }
    float getCurrentLevel() const { return currentLevel.load(); }

    struct TuningProfile {
        juce::String name;
        float offsets[12];
    };
    std::vector<TuningProfile>& getProfiles() { return profiles; }
    void saveUserProfile(int index, const float* newOffsets);

private:
    TunerBackgroundWorker worker;
    std::atomic<float> currentLevel { 0.0f };
    juce::AudioBuffer<float> monoBuffer;
    
    std::vector<TuningProfile> profiles;
    void setupDefaultProfiles();

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GalaxyStrobeTuneAudioProcessor)
};
