#include <lv2/core/lv2.h>
#include "dsp_blocks.hpp"
#include "automix_ducker.hpp"
#include "CyberDenoiserEngine.hpp"
#include <vector>
#include <cmath>

#define MOD_DIGITAL_MIXER_URI "http://moddevices.com/plugins/modep/digital-mixer-8"

// Port indices matching the TTL definition
enum PortIndex {
    // Audio Inputs (8 channels)
    PORT_IN_1 = 0,
    PORT_IN_2,
    PORT_IN_3,
    PORT_IN_4,
    PORT_IN_5,
    PORT_IN_6,
    PORT_IN_7,
    PORT_IN_8,

    // Aux FX Returns (Stereo pairs: Aux 1/2 and Aux 3/4)
    PORT_AUX_RET_1,
    PORT_AUX_RET_2,
    PORT_AUX_RET_3,
    PORT_AUX_RET_4,

    // Audio Outputs (Master Main PA L/R)
    PORT_OUT_MAIN_L,
    PORT_OUT_MAIN_R,

    // Aux Sends (Sends 1-4 for In-Ear Monitors & External Reverb/Delay)
    PORT_AUX_SEND_1,
    PORT_AUX_SEND_2,
    PORT_AUX_SEND_3,
    PORT_AUX_SEND_4,

    // Mixer Direct Outs (Channels 1-8 direct outputs for DAW capture/monitoring)
    PORT_DIRECT_OUT_1,
    PORT_DIRECT_OUT_2,
    PORT_DIRECT_OUT_3,
    PORT_DIRECT_OUT_4,
    PORT_DIRECT_OUT_5,
    PORT_DIRECT_OUT_6,
    PORT_DIRECT_OUT_7,
    PORT_DIRECT_OUT_8,

    // Control Ports (Global)
    PORT_MASTER_FADER,
    PORT_AUTOMIX_ENABLE,
    PORT_FX_DUCK_AMOUNT,

    PORT_COUNT
};

struct ChannelStrip {
    AudioDSP::CyberDenoiserEngine denoiser;
    ModepDSP::FETPreamp1176 preamp;
    ModepDSP::DeEsser deesser;
    ModepDSP::ParametricEQ7Band eq7;
    ModepDSP::ChannelCompressor compressor;
    ModepDSP::FeedbackDestroyer fbDestroyer;

    float faderLevel;
    float pan;
    float auxSends[4];
    bool mute;
    bool solo;
    bool automixAssign;
};

class ModDigitalMixer8 {
public:
    ModDigitalMixer8(double rate) : sampleRate(rate) {
        automixer.init((float)sampleRate);
        ducker.init((float)sampleRate);
        masterEQ7_L.init((float)sampleRate);
        masterEQ7_R.init((float)sampleRate);
        masterLA2A_L.init((float)sampleRate);
        masterLA2A_R.init((float)sampleRate);
        masterLimiter_L.init((float)sampleRate);
        masterLimiter_R.init((float)sampleRate);

        for (int i = 0; i < 4; ++i) {
            monitorEQ7[i].init((float)sampleRate);
        }

        for (int i = 0; i < 8; ++i) {
            channels[i].denoiser.prepare(sampleRate);
            channels[i].denoiser.setReductionAmount(80.0f); // 80% default clean subtractive denoise
            channels[i].preamp.setDrive(2.0f); // Subtle analog warmth
            channels[i].deesser.init((float)sampleRate);
            channels[i].eq7.init((float)sampleRate);
            channels[i].compressor.init((float)sampleRate);
            channels[i].fbDestroyer.init((float)sampleRate);

            channels[i].faderLevel = 1.0f; // Unity
            channels[i].pan = 0.5f;        // Center
            channels[i].auxSends[0] = 0.0f;
            channels[i].auxSends[1] = 0.0f;
            channels[i].auxSends[2] = 0.0f;
            channels[i].auxSends[3] = 0.0f;
            channels[i].mute = false;
            channels[i].solo = false;
            channels[i].automixAssign = true;
        }

        masterFader = 1.0f;
        masterLA2A_L.setPeakReduction(25.0f);
        masterLA2A_R.setPeakReduction(25.0f);
        masterLA2A_L.setGainDb(1.5f);
        masterLA2A_R.setGainDb(1.5f);
    }

    void connectPort(uint32_t port, void* data) {
        if (port < PORT_COUNT) {
            ports[port] = data;
        }
    }

    void run(uint32_t sampleCount) {
        // Pointers for audio buffers
        const float* inBuffers[8];
        for (int i = 0; i < 8; ++i) {
            inBuffers[i] = (const float*)ports[PORT_IN_1 + i];
        }

        const float* auxRet[4];
        for (int i = 0; i < 4; ++i) {
            auxRet[i] = (const float*)ports[PORT_AUX_RET_1 + i];
        }

        float* outMainL = (float*)ports[PORT_OUT_MAIN_L];
        float* outMainR = (float*)ports[PORT_OUT_MAIN_R];

        float* auxSend[4];
        for (int i = 0; i < 4; ++i) {
            auxSend[i] = (float*)ports[PORT_AUX_SEND_1 + i];
            if (auxSend[i]) std::fill_n(auxSend[i], sampleCount, 0.0f);
        }

        float* directOuts[8];
        for (int i = 0; i < 8; ++i) {
            directOuts[i] = (float*)ports[PORT_DIRECT_OUT_1 + i];
        }

        if (outMainL) std::fill_n(outMainL, sampleCount, 0.0f);
        if (outMainR) std::fill_n(outMainR, sampleCount, 0.0f);

        // Control parameters
        if (ports[PORT_MASTER_FADER]) masterFader = *(const float*)ports[PORT_MASTER_FADER];
        if (ports[PORT_AUTOMIX_ENABLE]) automixer.setEnabled(*(const float*)ports[PORT_AUTOMIX_ENABLE] > 0.5f);
        if (ports[PORT_FX_DUCK_AMOUNT]) ducker.setParams(-24.0f, *(const float*)ports[PORT_FX_DUCK_AMOUNT]);

        // 1. Process Cyber-Denoiser PRO per channel (block-based 10-band spectral subtraction)
        std::vector<float> denoisedBuffers[8];
        for (int ch = 0; ch < 8; ++ch) {
            denoisedBuffers[ch].resize(sampleCount, 0.0f);
            if (inBuffers[ch]) {
                channels[ch].denoiser.processBlock(inBuffers[ch], nullptr, denoisedBuffers[ch].data(), nullptr, sampleCount);
            }
        }

        // Process audio block sample by sample for sub-sample Dugan & Ducking reactivity
        for (uint32_t s = 0; s < sampleCount; ++s) {
            float processedChannels[8] = {0.0f};

            // 2. Process Channel Strips: Denoiser -> 1176 Preamp -> DeEsser -> 7-Band Parametric EQ -> Compressor -> FB Destroyer
            for (int ch = 0; ch < 8; ++ch) {
                if (!inBuffers[ch]) continue;
                float sample = denoisedBuffers[ch][s];

                sample = channels[ch].preamp.process(sample);
                sample = channels[ch].deesser.process(sample);
                sample = channels[ch].eq7.process(sample);
                sample = channels[ch].compressor.process(sample);
                sample = channels[ch].fbDestroyer.process(sample);

                processedChannels[ch] = sample;

                // Direct out for DAW recording
                if (directOuts[ch]) {
                    directOuts[ch][s] = sample;
                }
            }

            // 3. Dugan Gain Sharing update
            automixer.updateGainSharing(processedChannels);

            // 4. Dynamic FX Ducking Sidechain Trigger (lead channel 0/1)
            ducker.updateSidechain(processedChannels[0]);

            // 5. Summing Matrix: Faders, Pan, Aux Sends, and Main Bus
            float sumL = 0.0f;
            float sumR = 0.0f;

            for (int ch = 0; ch < 8; ++ch) {
                if (channels[ch].mute) continue;

                float automixGain = channels[ch].automixAssign ? automixer.getChannelGain(ch) : 1.0f;
                float finalChSample = processedChannels[ch] * channels[ch].faderLevel * automixGain;

                // Equal-power stereo panning
                float panAngle = channels[ch].pan * (3.14159265f * 0.5f);
                sumL += finalChSample * std::cos(panAngle);
                sumR += finalChSample * std::sin(panAngle);

                // Aux / Monitor Sends (Pre/Post fader)
                for (int aux = 0; aux < 4; ++aux) {
                    if (auxSend[aux]) {
                        auxSend[aux][s] += finalChSample * channels[ch].auxSends[aux];
                    }
                }
            }

            // Apply 7-band EQ on Monitor Aux Sends
            for (int aux = 0; aux < 4; ++aux) {
                if (auxSend[aux]) {
                    auxSend[aux][s] = monitorEQ7[aux].process(auxSend[aux][s]);
                }
            }

            // 6. Add Dynamic Ducked FX Returns to Master Bus
            if (auxRet[0] && auxRet[1]) {
                float duckedFX_L = ducker.processFX(auxRet[0][s]);
                float duckedFX_R = ducker.processFX(auxRet[1][s]);
                sumL += duckedFX_L;
                sumR += duckedFX_R;
            }

            // 7. Master Processing Section: Master Fader -> Master 7-Band EQ -> LA-2A Optical Leveler -> Zero-Latency Brickwall Limiter
            float masterSigL = sumL * masterFader;
            float masterSigR = sumR * masterFader;

            masterSigL = masterEQ7_L.process(masterSigL);
            masterSigR = masterEQ7_R.process(masterSigR);

            masterSigL = masterLA2A_L.process(masterSigL);
            masterSigR = masterLA2A_R.process(masterSigR);

            // Zero-latency instant brickwall limit safeguard (no delay buffer!)
            if (outMainL) outMainL[s] = masterLimiter_L.process(masterSigL);
            if (outMainR) outMainR[s] = masterLimiter_R.process(masterSigR);
        }
    }

    void setChannelFader(int ch, float lvl) { if (ch >= 0 && ch < 8) channels[ch].faderLevel = lvl; }
    void setChannelPan(int ch, float p) { if (ch >= 0 && ch < 8) channels[ch].pan = p; }
    void setChannelMute(int ch, bool m) { if (ch >= 0 && ch < 8) channels[ch].mute = m; }
    void setChannelSolo(int ch, bool s) { if (ch >= 0 && ch < 8) channels[ch].solo = s; }
    void setAuxSend(int ch, int aux, float lvl) {
        if (ch >= 0 && ch < 8 && aux >= 0 && aux < 4) channels[ch].auxSends[aux] = lvl;
    }

private:
    double sampleRate;
    void* ports[PORT_COUNT] = { nullptr };
    ChannelStrip channels[8];
    ModepDSP::DuganAutoMixer automixer;
    ModepDSP::DynamicFXDucker ducker;
    float masterFader;

    // Master bus processing
    ModepDSP::ParametricEQ7Band masterEQ7_L;
    ModepDSP::ParametricEQ7Band masterEQ7_R;
    ModepDSP::LA2AOpticalLeveler masterLA2A_L;
    ModepDSP::LA2AOpticalLeveler masterLA2A_R;
    ModepDSP::LiveZeroLatencyLimiter masterLimiter_L;
    ModepDSP::LiveZeroLatencyLimiter masterLimiter_R;

    // Monitor bus 7-band EQs
    ModepDSP::ParametricEQ7Band monitorEQ7[4];
};

// LV2 C Glue API
static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate, const char* bundle_path, const LV2_Feature* const* features) {
    return new ModDigitalMixer8(rate);
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    static_cast<ModDigitalMixer8*>(instance)->connectPort(port, data);
}

static void run(LV2_Handle instance, uint32_t sample_count) {
    static_cast<ModDigitalMixer8*>(instance)->run(sample_count);
}

static void cleanup(LV2_Handle instance) {
    delete static_cast<ModDigitalMixer8*>(instance);
}

static const LV2_Descriptor descriptor = {
    MOD_DIGITAL_MIXER_URI,
    instantiate,
    connect_port,
    nullptr, // activate
    run,
    nullptr, // deactivate
    cleanup,
    nullptr  // extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
