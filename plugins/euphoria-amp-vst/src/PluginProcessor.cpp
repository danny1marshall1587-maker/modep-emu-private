#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

EuphoriaAmpAudioProcessor::EuphoriaAmpAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    jassert(euphoriaParamTableIsConsistent());
    dspEngine = std::make_unique<EuphoriaDSP::EuphoriaAmpEngine>(48000.0);

    const auto& table = euphoriaParamTable();
    rawParams.reserve(table.size());
    for (const auto& d : table)
        rawParams.push_back(apvts.getRawParameterValue(d.id));
}

// Every control of the Windows standalone is exposed as a host-automatable parameter.
// Stepped controls become choice parameters; the raw value is the step index.
juce::AudioProcessorValueTreeState::ParameterLayout EuphoriaAmpAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    for (const auto& d : euphoriaParamTable()) {
        if (!d.choices.empty()) {
            juce::StringArray names;
            for (auto* c : d.choices) names.add(c);
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{ d.id, 1 }, d.label, names, (int)std::lround(d.defVal)));
        } else {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ d.id, 1 }, d.label,
                juce::NormalisableRange<float>(d.minVal, d.maxVal), d.defVal));
        }
    }
    return { params.begin(), params.end() };
}

void EuphoriaAmpAudioProcessor::prepareToPlay(double sampleRate, int) {
    dspEngine = std::make_unique<EuphoriaDSP::EuphoriaAmpEngine>(sampleRate);
}

void EuphoriaAmpAudioProcessor::releaseResources() {}

bool EuphoriaAmpAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& outSet = layouts.getMainOutputChannelSet();
    const auto& inSet  = layouts.getMainInputChannelSet();
    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;
    if (inSet != juce::AudioChannelSet::mono() && inSet != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

// Mirrors runEngineBlock() of StandaloneWinMain.cpp so the plugin and the
// standalone configure the shared engine identically.
void EuphoriaAmpAudioProcessor::pushParamsToEngine(uint32_t) {
    auto& e = *dspEngine;
    auto on = [this](int i) { return V(i) > 0.5f; };
    auto idx = [this](int i, int hi) { return std::max(0, std::min(hi, (int)std::lround(V(i)))); };

    // Tape delay
    const bool dlyOn = on(K_DLY_ON);
    e.setDelayParams(V(K_DLY_TIME), V(K_DLY_FB), dlyOn ? V(K_DLY_MIX) : 0.0f,
                     V(K_DLY_DRIVE), V(K_DLY_FLUTTER), on(K_DLY_PINGPONG) ? 1 : 0,
                     V(K_DLY_DUCK), dlyOn);

    // Fluid space chamber
    static const float shapeFactors[11] = { 0.00f, 0.08f, 0.16f, 0.28f, 0.40f, 0.50f, 0.60f, 0.72f, 0.82f, 0.92f, 1.00f };
    static const float sizeFactors[19]  = { 0.100f, 0.111f, 0.125f, 0.143f, 0.167f, 0.200f, 0.250f, 0.333f, 0.500f,
                                            1.000f, 2.000f, 3.000f, 4.000f, 5.000f, 6.000f, 7.000f, 8.000f, 9.000f, 10.00f };
    e.setFluidSpaceParams(shapeFactors[idx(K_SPACE_SHAPE, 10)], sizeFactors[idx(K_SPACE_SIZE, 18)],
                          on(K_SPACE_MOD_ON), V(K_SPACE_MOD_RATE),
                          V(K_SPACE_SHAPE_DEP) * 0.01f, V(K_SPACE_SIZE_DEP) * 0.01f,
                          idx(K_SPACE_WAVEFORM, 3), V(K_SPACE_SPREAD));

    // Fluid reverb + harmonic tremolo
    e.setFluidReverbParams(shapeFactors[idx(K_REV_SHAPE, 10)], sizeFactors[idx(K_REV_SIZE, 18)],
                           V(K_REV_LOWCUT), V(K_REV_HIGHCUT), V(K_REV_DWELL), V(K_REV_MIX));
    e.setEffectBypasses(on(K_REV_ON), on(K_VIB_ON));

    // Cabinet + pickup
    static const float cabSizeFactors[5] = { 0.6f, 0.8f, 1.0f, 1.3f, 1.8f };
    const int cabModel = idx(K_CAB_SIM, 8);
    e.setCabParams(cabModel, cabSizeFactors[idx(K_CAB_SIZE, 4)], on(K_TUNING_REF), V(K_BM_SPKDRV));
    e.setPickupVoicing(idx(K_PICKUP_VOICE, 2));

    // Five independent series pedals: every pedal has a dedicated voicing switch
    // and a dedicated (OLD) vs HYPER circuit switch (mode = hyper*2 + voicing).
    const bool klonHyper = on(K_KLON_HYPER);
    e.setCentaurParams(on(K_KLON_ON), V(K_KLON_GAIN), V(K_KLON_TREBLE), V(K_KLON_OUTPUT), V(K_KLON_BLEND),
                       false, klonHyper ? 2 : 0);

    const bool tsFat = on(K_TS_MOD), tsHyper = on(K_TS_HYPER);
    e.setTS808Params(on(K_TS_ON), V(K_TS_DRIVE), V(K_TS_TONE), V(K_TS_LEVEL), tsFat,
                     (tsHyper ? 2 : 0) + (tsFat ? 1 : 0));

    const bool bdPhat = on(K_BD_PHAT), bdHyper = on(K_BD_HYPER);
    e.setBD2Params(on(K_BD_ON), V(K_BD_LEVEL), V(K_BD_TONE), V(K_BD_GAIN), bdPhat,
                   (bdHyper ? 2 : 0) + (bdPhat ? 1 : 0));

    const bool ratTurbo = on(K_RAT_TURBO), ratHyper = on(K_RAT_HYPER);
    e.setRatParams(on(K_RAT_ON), V(K_RAT_DIST), V(K_RAT_FILTER), V(K_RAT_VOL), ratTurbo,
                   (ratHyper ? 2 : 0) + (ratTurbo ? 1 : 0));

    const bool fuzzSi = on(K_FUZZ_TRANSISTOR), fuzzHyper = on(K_FUZZ_HYPER);
    e.setFuzzFaceParams(on(K_FUZZ_ON), V(K_FUZZ_GAIN), V(K_FUZZ_LEVEL), V(K_FUZZ_SAG),
                        fuzzSi ? 1 : 0, (fuzzHyper ? 2 : 0) + (fuzzSi ? 1 : 0));

    // Amp channels
    e.setBassmanToneParams(V(K_BM_BASS), V(K_BM_MID), V(K_BM_TREBLE));
    e.setLeadParams(V(K_LEAD_GAIN), V(K_LEAD_VOL), V(K_LEAD_BASS), V(K_LEAD_MID), V(K_LEAD_TREBLE),
                    V(K_LEAD_PRESENCE), on(K_LEAD_HBE), on(K_LEAD_C45), on(K_LEAD_SAT), V(K_LEAD_DEHISS));
    e.setCleanToneParams(V(K_CLEAN_BRIGHT), V(K_CLEAN_DEEP), V(K_CLEAN_MIDSHIFT), V(K_CLEAN_GLASS),
                         V(K_CLEAN_PRESENCE), V(K_CLEAN_VOL), V(K_CLEAN_BASS), V(K_CLEAN_MID), V(K_CLEAN_TREBLE));
}

void EuphoriaAmpAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || !dspEngine) return;

    pushParamsToEngine((uint32_t)numSamples);

    const float* inPtr = buffer.getReadPointer(0);
    float* outL = buffer.getWritePointer(0);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1) : nullptr;

    dspEngine->process(
        inPtr, outL, outR, (uint32_t)numSamples,
        1.0f,
        V(K_CH_MODE),
        V(K_CLEAN_VOL),
        V(K_BM_DRIVE),
        V(K_BM_VOL),
        V(K_CLEAN_BASS),
        V(K_CLEAN_MID),
        V(K_CLEAN_TREBLE),
        V(K_CLEAN_BRIGHT),
        V(K_BM_SLOPE),
        V(K_BM_TUBES),
        V(K_CLEAN_MASTER),
        V(K_BM_PRESENCE),
        V(K_BM_SAG),
        V(K_REV_DWELL),
        V(K_REV_MIX),
        0.0f, // reverb tone (unused by the fluid reverb)
        0.0f, // reverb drip (unused by the fluid reverb)
        V(K_VIB_SPEED),
        V(K_VIB_DEPTH),
        V(K_VIB_MODE),
        V(K_QUALITY),
        (float)std::max(0, std::min(8, (int)std::lround(V(K_CAB_SIM)))),
        V(K_BM_SPKDRV),
        V(K_CLEAN_OUT),
        1.0f, 1.0f, 1.0f, 5.0f,
        isNonRealtime(),
        V(K_BM_BASS),
        V(K_BM_MID),
        V(K_BM_TREBLE)
    );
}

juce::AudioProcessorEditor* EuphoriaAmpAudioProcessor::createEditor() {
    return new EuphoriaAmpAudioProcessorEditor(*this);
}

void EuphoriaAmpAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EuphoriaAmpAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new EuphoriaAmpAudioProcessor();
}
