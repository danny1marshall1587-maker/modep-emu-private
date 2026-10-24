#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <chrono>
#include <cmath>

EuphoriaAmpAudioProcessor::EuphoriaAmpAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    jassert(euphoriaParamTableIsConsistent());
    dspEngine = std::make_unique<EuphoriaDSP::EuphoriaAmpEngine>(48000.0);
    tuner.reset(48000.0f);

    const auto& table = euphoriaParamTable();
    rawParams.reserve(table.size());
    for (const auto& d : table)
        rawParams.push_back(apvts.getRawParameterValue(d.id));

    initFactoryPresets();
}

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

void EuphoriaAmpAudioProcessor::setParamValueFromGui(int idx, float denormVal) {
    const auto& table = euphoriaParamTable();
    if (idx < 0 || idx >= (int)table.size()) return;
    const auto& d = table[(size_t)idx];
    float clamped = std::clamp(denormVal, d.minVal, d.maxVal);
    if (auto* p = apvts.getParameter(d.id)) {
        float norm = p->convertTo0to1(clamped);
        p->beginChangeGesture();
        p->setValueNotifyingHost(norm);
        p->endChangeGesture();
    }
}

void EuphoriaAmpAudioProcessor::initFactoryPresets() {
    presets.clear();
    const auto& table = euphoriaParamTable();
    auto makeDefault = [&table](const char* name) -> PresetItem {
        PresetItem p;
        p.name = name;
        for (size_t i = 0; i < K_NUM_PARAMS; ++i)
            p.vals[i] = table[i].defVal;
        return p;
    };

    // 1. default
    {
        auto p = makeDefault("default");
        p.vals[K_CLEAN_VOL] = 5.2f; p.vals[K_CLEAN_BRIGHT] = 1.0f; p.vals[K_CLEAN_DEEP] = 1.0f;
        p.vals[K_CLEAN_BASS] = 5.2f; p.vals[K_CLEAN_MID] = 4.8f; p.vals[K_CLEAN_MIDSHIFT] = 0.0f;
        p.vals[K_CLEAN_TREBLE] = 6.0f; p.vals[K_CLEAN_GLASS] = 6.6f; p.vals[K_CLEAN_PRESENCE] = 5.5f;
        p.vals[K_CLEAN_MASTER] = 6.0f; p.vals[K_CLEAN_OUT] = 5.0f;
        presets.push_back(p);
    }
    // 2. Euphoria Supreme Glass (Gretsch)
    {
        auto p = makeDefault("Euphoria Supreme Glass (Gretsch)");
        p.vals[K_CH_MODE] = 0.0f; p.vals[K_CAB_SIM] = 0.0f; p.vals[K_PICKUP_VOICE] = 2.0f;
        p.vals[K_CLEAN_VOL] = 5.5f; p.vals[K_CLEAN_BRIGHT] = 2.0f; p.vals[K_CLEAN_DEEP] = 0.0f;
        p.vals[K_CLEAN_BASS] = 4.8f; p.vals[K_CLEAN_MID] = 5.6f; p.vals[K_CLEAN_MIDSHIFT] = 2.0f;
        p.vals[K_CLEAN_TREBLE] = 6.2f; p.vals[K_CLEAN_GLASS] = 7.2f; p.vals[K_CLEAN_PRESENCE] = 6.0f;
        p.vals[K_CLEAN_MASTER] = 6.0f; p.vals[K_CLEAN_OUT] = 5.0f;
        presets.push_back(p);
    }
    // 3. Two-Rock Vocal Clean
    {
        auto p = makeDefault("Two-Rock Vocal Clean");
        p.vals[K_CH_MODE] = 0.0f; p.vals[K_CAB_SIM] = 2.0f;
        p.vals[K_CLEAN_VOL] = 5.6f; p.vals[K_CLEAN_BRIGHT] = 1.0f; p.vals[K_CLEAN_DEEP] = 1.0f;
        p.vals[K_CLEAN_BASS] = 5.4f; p.vals[K_CLEAN_MID] = 6.2f; p.vals[K_CLEAN_MIDSHIFT] = 1.0f;
        p.vals[K_CLEAN_TREBLE] = 5.8f; p.vals[K_CLEAN_GLASS] = 6.0f; p.vals[K_CLEAN_PRESENCE] = 5.2f;
        p.vals[K_CLEAN_MASTER] = 5.8f;
        presets.push_back(p);
    }
    // 4. Tweed Bassman Crunch
    {
        auto p = makeDefault("Tweed Bassman Crunch");
        p.vals[K_CH_MODE] = 1.0f; p.vals[K_CAB_SIM] = 1.0f; p.vals[K_CAB_SIZE] = 2.0f;
        p.vals[K_BM_DRIVE] = 6.2f; p.vals[K_BM_VOL] = 5.8f; p.vals[K_BM_BASS] = 5.8f;
        p.vals[K_BM_MID] = 6.2f; p.vals[K_BM_TREBLE] = 5.6f; p.vals[K_BM_SLOPE] = 0.0f;
        p.vals[K_BM_TUBES] = 0.0f; p.vals[K_BM_PRESENCE] = 5.8f; p.vals[K_BM_SAG] = 4.8f;
        p.vals[K_BM_SPKDRV] = 4.5f; p.vals[K_SPACE_SHAPE] = 0.0f; p.vals[K_SPACE_SIZE] = 9.0f;
        presets.push_back(p);
    }
    // 5. JMP British Roar
    {
        auto p = makeDefault("JMP British Roar");
        p.vals[K_CH_MODE] = 1.0f; p.vals[K_CAB_SIM] = 3.0f; p.vals[K_CAB_SIZE] = 3.0f;
        p.vals[K_BM_DRIVE] = 7.8f; p.vals[K_BM_VOL] = 6.0f; p.vals[K_BM_BASS] = 5.0f;
        p.vals[K_BM_MID] = 6.8f; p.vals[K_BM_TREBLE] = 6.5f; p.vals[K_BM_SLOPE] = 1.0f;
        p.vals[K_BM_TUBES] = 1.0f; p.vals[K_BM_PRESENCE] = 6.5f; p.vals[K_BM_SAG] = 5.5f;
        p.vals[K_BM_SPKDRV] = 6.2f; p.vals[K_SPACE_SHAPE] = 4.0f; p.vals[K_SPACE_SIZE] = 10.0f;
        presets.push_back(p);
    }
    // 6. Brown Sound BE-100 Lead
    {
        auto p = makeDefault("Brown Sound BE-100 Lead");
        p.vals[K_CH_MODE] = 2.0f; p.vals[K_CAB_SIM] = 3.0f; p.vals[K_CAB_SIZE] = 3.0f;
        p.vals[K_LEAD_GAIN] = 7.5f; p.vals[K_LEAD_VOL] = 5.8f; p.vals[K_LEAD_BASS] = 5.2f;
        p.vals[K_LEAD_MID] = 6.2f; p.vals[K_LEAD_TREBLE] = 6.5f; p.vals[K_LEAD_PRESENCE] = 6.0f;
        p.vals[K_LEAD_HBE] = 0.0f; p.vals[K_LEAD_C45] = 1.0f; p.vals[K_LEAD_SAT] = 0.0f;
        p.vals[K_LEAD_DEHISS] = 5.5f;
        presets.push_back(p);
    }
    // 7. Hot-Rodded Screamer Lead
    {
        auto p = makeDefault("Hot-Rodded Screamer Lead");
        p.vals[K_CH_MODE] = 2.0f; p.vals[K_CAB_SIM] = 3.0f;
        p.vals[K_TS_ON] = 1.0f; p.vals[K_TS_DRIVE] = 2.5f; p.vals[K_TS_TONE] = 6.2f; p.vals[K_TS_LEVEL] = 8.8f;
        p.vals[K_LEAD_GAIN] = 7.0f; p.vals[K_LEAD_VOL] = 5.5f; p.vals[K_LEAD_BASS] = 4.8f;
        p.vals[K_LEAD_MID] = 6.0f; p.vals[K_LEAD_TREBLE] = 6.8f; p.vals[K_LEAD_PRESENCE] = 6.2f;
        p.vals[K_LEAD_HBE] = 1.0f; p.vals[K_LEAD_C45] = 1.0f; p.vals[K_LEAD_SAT] = 1.0f;
        p.vals[K_LEAD_DEHISS] = 6.0f;
        presets.push_back(p);
    }
    // 8. Centaur Transparent Boost
    {
        auto p = makeDefault("Centaur Transparent Boost");
        p.vals[K_CH_MODE] = 0.0f; p.vals[K_CAB_SIM] = 0.0f; p.vals[K_CLEAN_VOL] = 5.2f;
        p.vals[K_CLEAN_BRIGHT] = 1.0f; p.vals[K_KLON_ON] = 1.0f; p.vals[K_KLON_GAIN] = 2.8f;
        p.vals[K_KLON_TREBLE] = 6.0f; p.vals[K_KLON_OUTPUT] = 8.5f; p.vals[K_KLON_BLEND] = 45.0f;
        presets.push_back(p);
    }
    // 9. Blues Driver Touch Dynamic
    {
        auto p = makeDefault("Blues Driver Touch Dynamic");
        p.vals[K_CH_MODE] = 0.0f; p.vals[K_CAB_SIM] = 0.0f; p.vals[K_CLEAN_VOL] = 5.0f;
        p.vals[K_CLEAN_BRIGHT] = 1.0f; p.vals[K_BD_ON] = 1.0f; p.vals[K_BD_LEVEL] = 7.8f;
        p.vals[K_BD_TONE] = 5.4f; p.vals[K_BD_GAIN] = 4.8f; p.vals[K_BD_PHAT] = 1.0f;
        presets.push_back(p);
    }
    // 10. 432Hz Golden Harmonic
    {
        auto p = makeDefault("432Hz Golden Harmonic");
        p.vals[K_CH_MODE] = 3.0f; p.vals[K_CAB_SIM] = 4.0f; p.vals[K_TUNING_REF] = 1.0f;
        p.vals[K_CLEAN_VOL] = 5.0f; p.vals[K_BM_DRIVE] = 4.5f;
        p.vals[K_SPACE_SHAPE] = 0.0f; p.vals[K_SPACE_SIZE] = 9.0f; p.vals[K_SPACE_MOD_ON] = 1.0f;
        p.vals[K_SPACE_MOD_RATE] = 0.432f; p.vals[K_SPACE_SHAPE_DEP] = 30.0f; p.vals[K_SPACE_SIZE_DEP] = 35.0f;
        p.vals[K_SPACE_WAVEFORM] = 2.0f; p.vals[K_SPACE_SPREAD] = 90.0f;
        p.vals[K_REV_ON] = 1.0f; p.vals[K_REV_SHAPE] = 0.0f; p.vals[K_REV_SIZE] = 9.0f;
        p.vals[K_REV_LOWCUT] = 140.0f; p.vals[K_REV_HIGHCUT] = 7000.0f; p.vals[K_REV_DWELL] = 6.0f;
        p.vals[K_REV_MIX] = 32.0f; p.vals[K_VIB_ON] = 1.0f; p.vals[K_VIB_MODE] = 0.0f;
        p.vals[K_VIB_SPEED] = 4.32f; p.vals[K_VIB_DEPTH] = 6.5f; p.vals[K_DLY_ON] = 1.0f; p.vals[K_DLY_MIX] = 24.0f;
        presets.push_back(p);
    }
    // 11. Surf Spring Dream
    {
        auto p = makeDefault("Surf Spring Dream");
        p.vals[K_CH_MODE] = 0.0f; p.vals[K_CAB_SIM] = 0.0f; p.vals[K_CLEAN_VOL] = 4.6f; p.vals[K_CLEAN_BRIGHT] = 1.0f;
        p.vals[K_REV_ON] = 1.0f; p.vals[K_REV_SHAPE] = 0.0f; p.vals[K_REV_SIZE] = 10.0f;
        p.vals[K_REV_LOWCUT] = 90.0f; p.vals[K_REV_HIGHCUT] = 8500.0f; p.vals[K_REV_DWELL] = 7.5f; p.vals[K_REV_MIX] = 45.0f;
        p.vals[K_VIB_ON] = 1.0f; p.vals[K_VIB_MODE] = 0.0f; p.vals[K_VIB_SPEED] = 5.2f; p.vals[K_VIB_DEPTH] = 7.0f;
        presets.push_back(p);
    }
    // 12. Ambient Tape Echo
    {
        auto p = makeDefault("Ambient Tape Echo");
        p.vals[K_CH_MODE] = 3.0f; p.vals[K_CAB_SIM] = 2.0f;
        p.vals[K_REV_ON] = 1.0f; p.vals[K_REV_SHAPE] = 3.0f; p.vals[K_REV_SIZE] = 12.0f;
        p.vals[K_REV_LOWCUT] = 120.0f; p.vals[K_REV_HIGHCUT] = 6000.0f; p.vals[K_REV_DWELL] = 6.8f; p.vals[K_REV_MIX] = 35.0f;
        p.vals[K_DLY_ON] = 1.0f; p.vals[K_DLY_PINGPONG] = 1.0f; p.vals[K_DLY_TIME] = 480.0f;
        p.vals[K_DLY_FB] = 48.0f; p.vals[K_DLY_MIX] = 32.0f; p.vals[K_DLY_FLUTTER] = 22.0f;
        presets.push_back(p);
    }
    // 13. Gretsch CB Junior Rock
    {
        auto p = makeDefault("Gretsch CB Junior Rock");
        p.vals[K_CH_MODE] = 2.0f; p.vals[K_CAB_SIM] = 3.0f; p.vals[K_CAB_SIZE] = 2.0f; p.vals[K_PICKUP_VOICE] = 2.0f;
        p.vals[K_LEAD_GAIN] = 6.8f; p.vals[K_LEAD_VOL] = 5.5f; p.vals[K_LEAD_BASS] = 5.2f;
        p.vals[K_LEAD_MID] = 6.0f; p.vals[K_LEAD_TREBLE] = 6.2f; p.vals[K_LEAD_PRESENCE] = 5.8f;
        p.vals[K_LEAD_HBE] = 0.0f; p.vals[K_LEAD_C45] = 1.0f; p.vals[K_LEAD_SAT] = 0.0f; p.vals[K_LEAD_DEHISS] = 5.0f;
        presets.push_back(p);
    }
    // 14. Hyper Greenback Punch
    {
        auto p = makeDefault("Hyper Greenback Punch");
        p.vals[K_CH_MODE] = 1.0f; p.vals[K_CAB_SIM] = 5.0f; p.vals[K_CAB_SIZE] = 3.0f;
        p.vals[K_BM_DRIVE] = 7.2f; p.vals[K_BM_VOL] = 5.8f; p.vals[K_BM_BASS] = 5.2f;
        p.vals[K_BM_MID] = 6.5f; p.vals[K_BM_TREBLE] = 6.2f; p.vals[K_BM_SLOPE] = 1.0f;
        p.vals[K_BM_TUBES] = 1.0f; p.vals[K_BM_PRESENCE] = 6.0f; p.vals[K_BM_SAG] = 5.0f; p.vals[K_BM_SPKDRV] = 6.5f;
        presets.push_back(p);
    }
    // 15. Euphoric Alnico Chime
    {
        auto p = makeDefault("Euphoric Alnico Chime");
        p.vals[K_CH_MODE] = 0.0f; p.vals[K_CAB_SIM] = 6.0f; p.vals[K_CAB_SIZE] = 2.0f; p.vals[K_PICKUP_VOICE] = 1.0f;
        p.vals[K_CLEAN_VOL] = 5.4f; p.vals[K_CLEAN_BRIGHT] = 1.0f; p.vals[K_CLEAN_DEEP] = 1.0f;
        p.vals[K_CLEAN_BASS] = 5.0f; p.vals[K_CLEAN_MID] = 5.2f; p.vals[K_CLEAN_MIDSHIFT] = 0.0f;
        p.vals[K_CLEAN_TREBLE] = 6.2f; p.vals[K_CLEAN_GLASS] = 7.0f; p.vals[K_CLEAN_PRESENCE] = 5.8f;
        p.vals[K_CLEAN_MASTER] = 6.2f; p.vals[K_BM_SPKDRV] = 5.5f;
        presets.push_back(p);
    }
    // 16. 432Hz Golden Projector
    {
        auto p = makeDefault("432Hz Golden Projector");
        p.vals[K_CH_MODE] = 3.0f; p.vals[K_CAB_SIM] = 7.0f; p.vals[K_TUNING_REF] = 1.0f;
        p.vals[K_CLEAN_VOL] = 5.2f; p.vals[K_BM_DRIVE] = 5.0f; p.vals[K_BM_SPKDRV] = 5.8f;
        p.vals[K_SPACE_SHAPE] = 0.0f; p.vals[K_SPACE_SIZE] = 9.0f; p.vals[K_SPACE_MOD_ON] = 1.0f;
        p.vals[K_SPACE_MOD_RATE] = 0.432f; p.vals[K_SPACE_WAVEFORM] = 2.0f;
        p.vals[K_REV_ON] = 1.0f; p.vals[K_REV_SIZE] = 9.0f; p.vals[K_REV_DWELL] = 6.0f; p.vals[K_REV_MIX] = 28.0f;
        presets.push_back(p);
    }
}

void EuphoriaAmpAudioProcessor::applyPreset(int idx) {
    if (idx < 0 || idx >= (int)presets.size()) return;
    currentPresetIdx = idx;
    const auto& p = presets[(size_t)idx];
    for (int i = 0; i < K_NUM_PARAMS; ++i)
        setParamValueFromGui(i, p.vals[i]);
}

void EuphoriaAmpAudioProcessor::saveUserPreset(const juce::String& name) {
    if (name.trim().isEmpty()) return;
    PresetItem p;
    p.name = name.trim();
    for (int i = 0; i < K_NUM_PARAMS; ++i)
        p.vals[i] = V(i);

    for (size_t k = 0; k < presets.size(); ++k) {
        if (presets[k].name.equalsIgnoreCase(p.name)) {
            presets[k] = p;
            currentPresetIdx = (int)k;
            return;
        }
    }
    presets.push_back(p);
    currentPresetIdx = (int)presets.size() - 1;
}

int EuphoriaAmpAudioProcessor::getNumPrograms() {
    return std::max(1, (int)presets.size());
}

void EuphoriaAmpAudioProcessor::setCurrentProgram(int index) {
    applyPreset(index);
}

const juce::String EuphoriaAmpAudioProcessor::getProgramName(int index) {
    if (index >= 0 && index < (int)presets.size())
        return presets[(size_t)index].name;
    return "default";
}

void EuphoriaAmpAudioProcessor::changeProgramName(int index, const juce::String& newName) {
    if (index >= 0 && index < (int)presets.size())
        presets[(size_t)index].name = newName;
}

void EuphoriaAmpAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    dspEngine = std::make_unique<EuphoriaDSP::EuphoriaAmpEngine>(sampleRate);
    tuner.reset((float)sampleRate);
    riffSynth.sampleRate = sampleRate;
    currentBlockSize.store(samplesPerBlock);
    monoInScratch.resize((size_t)std::max(samplesPerBlock, 4096), 0.0f);
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

void EuphoriaAmpAudioProcessor::pushParamsToEngine(uint32_t) {
    auto& e = *dspEngine;
    auto on = [this](int i) { return V(i) > 0.5f; };
    auto idx = [this](int i, int hi) { return std::max(0, std::min(hi, (int)std::lround(V(i)))); };

    const bool dlyOn = on(K_DLY_ON);
    e.setDelayParams(V(K_DLY_TIME), V(K_DLY_FB), dlyOn ? V(K_DLY_MIX) : 0.0f,
                     V(K_DLY_DRIVE), V(K_DLY_FLUTTER), on(K_DLY_PINGPONG) ? 1 : 0,
                     V(K_DLY_DUCK), dlyOn);

    static const float shapeFactors[11] = { 0.00f, 0.08f, 0.16f, 0.28f, 0.40f, 0.50f, 0.60f, 0.72f, 0.82f, 0.92f, 1.00f };
    static const float sizeFactors[19]  = { 0.100f, 0.111f, 0.125f, 0.143f, 0.167f, 0.200f, 0.250f, 0.333f, 0.500f,
                                            1.000f, 2.000f, 3.000f, 4.000f, 5.000f, 6.000f, 7.000f, 8.000f, 9.000f, 10.00f };
    e.setFluidSpaceParams(shapeFactors[idx(K_SPACE_SHAPE, 10)], sizeFactors[idx(K_SPACE_SIZE, 18)],
                          on(K_SPACE_MOD_ON), V(K_SPACE_MOD_RATE),
                          V(K_SPACE_SHAPE_DEP) * 0.01f, V(K_SPACE_SIZE_DEP) * 0.01f,
                          idx(K_SPACE_WAVEFORM, 3), V(K_SPACE_SPREAD));

    e.setFluidReverbParams(shapeFactors[idx(K_REV_SHAPE, 10)], sizeFactors[idx(K_REV_SIZE, 18)],
                           V(K_REV_LOWCUT), V(K_REV_HIGHCUT), V(K_REV_DWELL), V(K_REV_MIX));
    e.setEffectBypasses(on(K_REV_ON), on(K_VIB_ON));

    static const float cabSizeFactors[5] = { 0.6f, 0.8f, 1.0f, 1.3f, 1.8f };
    const int cabModel = idx(K_CAB_SIM, 8);
    e.setCabParams(cabModel, cabSizeFactors[idx(K_CAB_SIZE, 4)], on(K_TUNING_REF), V(K_BM_SPKDRV));
    e.setPickupVoicing(idx(K_PICKUP_VOICE, 2));

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

    currentBlockSize.store(numSamples);
    if ((int)monoInScratch.size() < numSamples)
        monoInScratch.resize((size_t)numSamples, 0.0f);

    const float* rawIn = buffer.getReadPointer(0);
    float maxIn = 0.0f;
    const bool injectRiff = testRiffActive.load();

    for (int i = 0; i < numSamples; ++i) {
        float s = rawIn ? rawIn[i] : 0.0f;
        if (injectRiff) s += riffSynth.nextSample();
        monoInScratch[(size_t)i] = s;
        float a = std::fabs(s);
        if (a > maxIn) maxIn = a;
    }

    if (maxIn > 0.0f) inMeterPeak.store(maxIn);
    else inMeterPeak.store(inMeterPeak.load() * 0.95f);

    const auto tStart = std::chrono::high_resolution_clock::now();

    pushParamsToEngine((uint32_t)numSamples);

    float* outL = buffer.getWritePointer(0);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1) : nullptr;

    dspEngine->process(
        monoInScratch.data(), outL, outR, (uint32_t)numSamples,
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
        0.0f,
        0.0f,
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

    tuner.writeSamples(monoInScratch.data(), numSamples);
    tuner.process((V(K_TUNING_REF) > 0.5f) ? 432.0f : 440.0f);

    if (tunerMute.load()) {
        std::fill(outL, outL + numSamples, 0.0f);
        if (outR) std::fill(outR, outR + numSamples, 0.0f);
    }

    float pkL = 0.0f, pkR = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float l = std::fabs(outL[i]);
        float r = outR ? std::fabs(outR[i]) : l;
        if (l > pkL) pkL = l;
        if (r > pkR) pkR = r;
    }
    outMeterPeakL.store(pkL);
    outMeterPeakR.store(pkR);

    const auto tEnd = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(tEnd - tStart).count();
    const double sr = std::max(8000.0, getSampleRate());
    const double blockSec = (double)numSamples / sr;
    const float cpuPct = (float)((elapsedSec / blockSec) * 100.0);
    const float timeUs = (float)(elapsedSec * 1000000.0);

    dspCpuPercent.store(dspCpuPercent.load() * 0.90f + cpuPct * 0.10f);
    dspTimeUs.store(dspTimeUs.load() * 0.90f + timeUs * 0.10f);

    const int qMode = (int)std::lround(V(K_QUALITY));
    const int internalRate = (qMode == 3) ? 384000 : ((qMode == 2) ? 192000 : ((qMode == 1) ? 96000 : 48000));
    dspInternalRate.store(internalRate);
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
