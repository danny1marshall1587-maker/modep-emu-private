#include "PluginProcessor.h"
#include "PluginEditor.h"

CyberUltimateHybridAudioProcessor::CyberUltimateHybridAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    dspEngine = std::make_unique<CyberDSP::CyberUltimateHybridEngine>(48000.0);
}

juce::AudioProcessorValueTreeState::ParameterLayout CyberUltimateHybridAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Preamp & Topology
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"channel_mode", 1}, "Channel Topology",
        juce::StringArray{"'65 Blackface Clean", "'59 Bassman / JMP Crunch", "Jumpered Parallel Blend"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"clean_vol", 1}, "Blackface Clean Vol", 0.0f, 10.0f, 6.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"drive_gain", 1}, "Bassman / JMP Gain", 0.0f, 10.0f, 5.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"drive_vol", 1}, "Bassman / JMP Vol", 0.0f, 10.0f, 5.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"bass", 1}, "Bass", 0.0f, 10.0f, 5.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mid", 1}, "Middle", 0.0f, 10.0f, 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"treble", 1}, "Treble", 0.0f, 10.0f, 6.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bright_sw", 1}, "120pF Bright Switch", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"slope_mode", 1}, "Tone Stack Slope",
        juce::StringArray{"56k Fender Bassman", "33k Marshall JMP"}, 0));

    // 2. Power Section & Sag
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"power_tubes", 1}, "Power Tubes",
        juce::StringArray{"2x 6L6GC American", "2x EL34 British"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"master", 1}, "Master Volume", 0.0f, 10.0f, 5.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"presence", 1}, "Presence", 0.0f, 10.0f, 5.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"sag_amount", 1}, "B+ Tube Sag", 0.0f, 10.0f, 6.0f));

    // 3. 3-Spring Accutronics Reverb Tank
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"rev_dwell", 1}, "Spring Dwell (12AT7)", 0.0f, 10.0f, 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"rev_mix", 1}, "Spring Reverb Mix", 0.0f, 100.0f, 22.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"rev_tone", 1}, "Spring Reverb Tone", 0.0f, 10.0f, 6.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"rev_drip", 1}, "Spring Drip Dispersion", 0.0f, 10.0f, 6.8f));

    // 4. Brownface 6G Harmonic Tube Vibrato
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"trem_speed", 1}, "Vibrato Speed (Hz)", 0.5f, 12.0f, 3.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"trem_depth", 1}, "Vibrato Intensity (%)", 0.0f, 100.0f, 28.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"trem_mode", 1}, "Brownface Harmonic Vibrato", true));

    // 5. Scalable Physics / CPU Mode & Speaker Cab
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"quality_mode", 1}, "Physics / CPU Scale",
        juce::StringArray{"Eco Live (1x Fast)", "Stage (2x Oversample)", "Studio (4x High-Precision)", "Ultra Bounce (8x Pure Physics)"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"speaker_cab", 1}, "Speaker Cabinet",
        juce::StringArray{"2x12 '65 Twin Jensen C12N", "4x10 '59 Bassman Jensen P10R", "4x12 '68 Marshall Greenback", "Direct Out / Cab Bypass"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"speaker_drive", 1}, "Cone Stress / Drive", 0.0f, 10.0f, 5.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"output_level", 1}, "Output Level", 0.0f, 10.0f, 7.0f));

    // 6. VST-Exclusive Live Mixer Studio Tape / Ping-Pong Delay
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dly_time_ms", 1}, "Delay Time (ms)", 10.0f, 1800.0f, 360.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dly_feedback", 1}, "Delay Feedback (%)", 0.0f, 95.0f, 34.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dly_mix", 1}, "Delay Wet Mix (%)", 0.0f, 100.0f, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dly_tape_drive", 1}, "15/30 IPS Tape Oxide Drive (%)", 0.0f, 100.0f, 28.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dly_flutter", 1}, "Tape Wow & Flutter (%)", 0.0f, 100.0f, 18.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"dly_ping_pong", 1}, "Stereo Ping-Pong Mode", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dly_ducking", 1}, "Dynamic Auto-Ducking (%)", 0.0f, 100.0f, 25.0f));

    return { params.begin(), params.end() };
}

void CyberUltimateHybridAudioProcessor::prepareToPlay(double sampleRate, int) {
    dspEngine = std::make_unique<CyberDSP::CyberUltimateHybridEngine>(sampleRate);
}

void CyberUltimateHybridAudioProcessor::releaseResources() {}

bool CyberUltimateHybridAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& outSet = layouts.getMainOutputChannelSet();
    const auto& inSet  = layouts.getMainInputChannelSet();
    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;
    if (inSet != juce::AudioChannelSet::mono() && inSet != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void CyberUltimateHybridAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || !dspEngine) return;

    dspEngine->setDelayParams(
        apvts.getRawParameterValue("dly_time_ms")->load(),
        apvts.getRawParameterValue("dly_feedback")->load(),
        apvts.getRawParameterValue("dly_mix")->load(),
        apvts.getRawParameterValue("dly_tape_drive")->load(),
        apvts.getRawParameterValue("dly_flutter")->load(),
        apvts.getRawParameterValue("dly_ping_pong")->load() > 0.5f ? 1 : 0,
        apvts.getRawParameterValue("dly_ducking")->load()
    );

    const float* inPtr = buffer.getReadPointer(0);
    float* outL = buffer.getWritePointer(0);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1) : nullptr;

    // Automatically upgrade to 8x Ultra Physics when DAW renders/bounces offline!
    const bool offlineBounce = isNonRealtime();

    dspEngine->process(
        inPtr, outL, outR, (uint32_t)numSamples,
        1.0f,
        apvts.getRawParameterValue("channel_mode")->load(),
        apvts.getRawParameterValue("clean_vol")->load(),
        apvts.getRawParameterValue("drive_gain")->load(),
        apvts.getRawParameterValue("drive_vol")->load(),
        apvts.getRawParameterValue("bass")->load(),
        apvts.getRawParameterValue("mid")->load(),
        apvts.getRawParameterValue("treble")->load(),
        apvts.getRawParameterValue("bright_sw")->load(),
        apvts.getRawParameterValue("slope_mode")->load(),
        apvts.getRawParameterValue("power_tubes")->load(),
        apvts.getRawParameterValue("master")->load(),
        apvts.getRawParameterValue("presence")->load(),
        apvts.getRawParameterValue("sag_amount")->load(),
        apvts.getRawParameterValue("rev_dwell")->load(),
        apvts.getRawParameterValue("rev_mix")->load(),
        apvts.getRawParameterValue("rev_tone")->load(),
        apvts.getRawParameterValue("rev_drip")->load(),
        apvts.getRawParameterValue("trem_speed")->load(),
        apvts.getRawParameterValue("trem_depth")->load(),
        apvts.getRawParameterValue("trem_mode")->load(),
        apvts.getRawParameterValue("quality_mode")->load(),
        apvts.getRawParameterValue("speaker_cab")->load(),
        apvts.getRawParameterValue("speaker_drive")->load(),
        apvts.getRawParameterValue("output_level")->load(),
        1.0f, 1.0f, 1.0f, 5.0f,
        offlineBounce
    );
}

juce::AudioProcessorEditor* CyberUltimateHybridAudioProcessor::createEditor() {
    return new CyberUltimateHybridAudioProcessorEditor(*this);
}

void CyberUltimateHybridAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CyberUltimateHybridAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new CyberUltimateHybridAudioProcessor();
}
