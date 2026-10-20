#include "PluginProcessor.h"
#include "PluginEditor.h"

GalaxyStrobeTuneAudioProcessor::GalaxyStrobeTuneAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    setupDefaultProfiles();
}

GalaxyStrobeTuneAudioProcessor::~GalaxyStrobeTuneAudioProcessor()
{
    worker.stopThread(2000);
}

void GalaxyStrobeTuneAudioProcessor::setupDefaultProfiles()
{
    profiles.clear();
    
    TuningProfile equal;
    equal.name = "Equal Temperament";
    for (int i = 0; i < 12; ++i) equal.offsets[i] = 0.0f;
    profiles.push_back(equal);

    TuningProfile jt;
    jt.name = "James Taylor";
    for (int i = 0; i < 12; ++i) jt.offsets[i] = 0.0f;
    jt.offsets[4] = -12.0f; jt.offsets[9] = -10.0f; jt.offsets[2] = -8.0f; 
    jt.offsets[7] = -4.0f; jt.offsets[11] = -6.0f; 
    profiles.push_back(jt);

    TuningProfile bfts;
    bfts.name = "Buzz Feiten";
    for (int i = 0; i < 12; ++i) bfts.offsets[i] = 0.0f;
    bfts.offsets[4] = -2.0f; bfts.offsets[9] = -2.0f; bfts.offsets[2] = -2.0f; 
    bfts.offsets[7] = -2.0f; bfts.offsets[11] = 1.0f; 
    profiles.push_back(bfts);

    TuningProfile gtr;
    gtr.name = "Peterson GTR Style";
    for (int i = 0; i < 12; ++i) gtr.offsets[i] = 0.0f;
    gtr.offsets[4] = -2.3f; gtr.offsets[9] = -2.1f; gtr.offsets[2] = -0.4f; 
    gtr.offsets[7] = 0.0f;  gtr.offsets[11] = -0.6f; 
    profiles.push_back(gtr);

    TuningProfile opend;
    opend.name = "Open D Sweetened";
    for (int i = 0; i < 12; ++i) opend.offsets[i] = 0.0f;
    opend.offsets[2] = -2.0f; opend.offsets[9] = -1.5f; opend.offsets[6] = -4.0f; 
    profiles.push_back(opend);

    TuningProfile openg;
    openg.name = "Open G Sweetened";
    for (int i = 0; i < 12; ++i) openg.offsets[i] = 0.0f;
    openg.offsets[7] = -2.0f; openg.offsets[2] = -1.5f; openg.offsets[11] = -4.0f; 
    profiles.push_back(openg);

    for (int i = 1; i <= 3; ++i)
    {
        TuningProfile user;
        user.name = "User Profile " + juce::String(i);
        for (int j = 0; j < 12; ++j) user.offsets[j] = 0.0f;
        profiles.push_back(user);
    }
}

void GalaxyStrobeTuneAudioProcessor::saveUserProfile(int index, const float* newOffsets)
{
    if (index >= 0 && index < (int)profiles.size())
    {
        for (int i = 0; i < 12; ++i)
            profiles[index].offsets[i] = newOffsets[i];
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout GalaxyStrobeTuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("refA", "Reference A (Hz)", 400.0f, 480.0f, 440.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("smoothing", "Stability", 0.0f, 0.99f, 0.99f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("profile", "Tuning Profile", juce::StringArray{"Equal", "James Taylor", "Buzz Feiten", "Peterson GTR", "Open D", "Open G", "User 1", "User 2", "User 3"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("capo", "Capo Fret", 0, 12, 0));
    return { params.begin(), params.end() };
}

void GalaxyStrobeTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    worker.prepare(sampleRate);
    monoBuffer.setSize(1, samplesPerBlock);
}

void GalaxyStrobeTuneAudioProcessor::releaseResources() {}

bool GalaxyStrobeTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto mainIn = layouts.getMainInputChannelSet();
    auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo()) return false;
    return mainIn == mainOut;
}

void GalaxyStrobeTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto numSamples = buffer.getNumSamples();
    auto numChannels = getTotalNumInputChannels();
    currentLevel.store(buffer.getRMSLevel(0, 0, numSamples));
    if (monoBuffer.getNumSamples() < numSamples) monoBuffer.setSize(1, numSamples);
    if (numChannels > 0)
    {
        monoBuffer.clear();
        for (int ch = 0; ch < numChannels; ++ch)
            monoBuffer.addFrom(0, 0, buffer.getReadPointer(ch), numSamples, 1.0f / numChannels);
        worker.pushSamples(monoBuffer.getReadPointer(0), numSamples);
    }
}

juce::AudioProcessorEditor* GalaxyStrobeTuneAudioProcessor::createEditor()
{
    return new GalaxyStrobeTuneAudioProcessorEditor (*this);
}

void GalaxyStrobeTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = 0; i < (int)profiles.size(); ++i)
    {
        juce::ValueTree p("Profile" + juce::String(i));
        for (int j = 0; j < 12; ++j)
            p.setProperty("offset" + juce::String(j), profiles[i].offsets[j], nullptr);
        state.addChild(p, -1, nullptr);
    }
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GalaxyStrobeTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(tree);
        for (int i = 0; i < (int)profiles.size(); ++i)
        {
            auto p = tree.getChildWithName("Profile" + juce::String(i));
            if (p.isValid())
            {
                for (int j = 0; j < 12; ++j)
                    profiles[i].offsets[j] = p.getProperty("offset" + juce::String(j));
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GalaxyStrobeTuneAudioProcessor();
}
