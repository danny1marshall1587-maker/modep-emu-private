/*
 * Euphoria Amp (Pure Fluid Dynamics '65 Blackface + '59 Bassman/JMP)
 * MODEP / Live Mixer LV2 Plugin Wrapper
 */

#define CYBER_ENABLE_STUDIO_DELAY 0
#include "EuphoriaAmpEngine.hpp"
#include "lv2/lv2.h"

#define PLUGIN_URI "http://cyber-audio.co.uk/plugins/euphoria-amp"

enum PortIndex {
    PORT_AUDIO_IN       = 0,
    PORT_AUDIO_OUT      = 1,
    PORT_BYPASS         = 2,
    PORT_CHANNEL_MODE   = 3,
    PORT_CLEAN_VOL      = 4,
    PORT_DRIVE_GAIN     = 5,
    PORT_DRIVE_VOL      = 6,
    PORT_BASS           = 7,
    PORT_MID            = 8,
    PORT_TREBLE         = 9,
    PORT_BRIGHT_SW      = 10,
    PORT_SLOPE_MODE     = 11,
    PORT_POWER_TUBES    = 12,
    PORT_MASTER         = 13,
    PORT_PRESENCE       = 14,
    PORT_SAG_AMOUNT     = 15,
    PORT_REV_DWELL      = 16,
    PORT_REV_MIX        = 17,
    PORT_REV_TONE       = 18,
    PORT_REV_DRIP       = 19,
    PORT_TREM_SPEED     = 20,
    PORT_TREM_DEPTH     = 21,
    PORT_TREM_MODE      = 22,
    PORT_QUALITY_MODE   = 23,
    PORT_SPEAKER_CAB    = 24,
    PORT_SPEAKER_DRIVE  = 25,
    PORT_OUTPUT_LEVEL   = 26,
    PORT_NOISE_GATE     = 27,
    PORT_NOISE_SPECTRAL = 28,
    PORT_NOISE_DEFIZZ   = 29,
    PORT_NOISE_LEVEL    = 30,
    PORT_COUNT          = 31
};

struct EuphoriaAmpLV2 {
    EuphoriaDSP::EuphoriaAmpEngine* dsp;
    const float* audioIn;
    float* audioOut;
    const float* bypass;
    const float* channelMode;
    const float* cleanVol;
    const float* driveGain;
    const float* driveVol;
    const float* bass;
    const float* mid;
    const float* treble;
    const float* brightSw;
    const float* slopeMode;
    const float* powerTubes;
    const float* master;
    const float* presence;
    const float* sagAmount;
    const float* revDwell;
    const float* revMix;
    const float* revTone;
    const float* revDrip;
    const float* tremSpeed;
    const float* tremDepth;
    const float* tremMode;
    const float* qualityMode;
    const float* speakerCab;
    const float* speakerDrive;
    const float* outputLevel;
    const float* noiseGate;
    const float* noiseSpectral;
    const float* noiseDefizz;
    const float* noiseLevel;

    EuphoriaAmpLV2()
        : dsp(nullptr), audioIn(nullptr), audioOut(nullptr), bypass(nullptr),
          channelMode(nullptr), cleanVol(nullptr), driveGain(nullptr), driveVol(nullptr),
          bass(nullptr), mid(nullptr), treble(nullptr), brightSw(nullptr),
          slopeMode(nullptr), powerTubes(nullptr), master(nullptr), presence(nullptr),
          sagAmount(nullptr), revDwell(nullptr), revMix(nullptr), revTone(nullptr),
          revDrip(nullptr), tremSpeed(nullptr), tremDepth(nullptr), tremMode(nullptr),
          qualityMode(nullptr), speakerCab(nullptr), speakerDrive(nullptr),
          outputLevel(nullptr), noiseGate(nullptr), noiseSpectral(nullptr),
          noiseDefizz(nullptr), noiseLevel(nullptr) {}

    ~EuphoriaAmpLV2() {
        if (dsp) delete dsp;
    }
};

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor, double rate,
    const char* bundle_path, const LV2_Feature* const* features
) {
    EuphoriaAmpLV2* h = new EuphoriaAmpLV2();
    h->dsp = new EuphoriaDSP::EuphoriaAmpEngine(rate);
    return (LV2_Handle)h;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    EuphoriaAmpLV2* h = (EuphoriaAmpLV2*)instance;
    switch ((PortIndex)port) {
        case PORT_AUDIO_IN:       h->audioIn       = (const float*)data; break;
        case PORT_AUDIO_OUT:      h->audioOut      = (float*)data; break;
        case PORT_BYPASS:         h->bypass        = (const float*)data; break;
        case PORT_CHANNEL_MODE:   h->channelMode   = (const float*)data; break;
        case PORT_CLEAN_VOL:      h->cleanVol      = (const float*)data; break;
        case PORT_DRIVE_GAIN:     h->driveGain     = (const float*)data; break;
        case PORT_DRIVE_VOL:      h->driveVol      = (const float*)data; break;
        case PORT_BASS:           h->bass          = (const float*)data; break;
        case PORT_MID:            h->mid           = (const float*)data; break;
        case PORT_TREBLE:         h->treble        = (const float*)data; break;
        case PORT_BRIGHT_SW:      h->brightSw      = (const float*)data; break;
        case PORT_SLOPE_MODE:     h->slopeMode     = (const float*)data; break;
        case PORT_POWER_TUBES:    h->powerTubes    = (const float*)data; break;
        case PORT_MASTER:         h->master        = (const float*)data; break;
        case PORT_PRESENCE:       h->presence      = (const float*)data; break;
        case PORT_SAG_AMOUNT:     h->sagAmount     = (const float*)data; break;
        case PORT_REV_DWELL:      h->revDwell      = (const float*)data; break;
        case PORT_REV_MIX:        h->revMix        = (const float*)data; break;
        case PORT_REV_TONE:       h->revTone       = (const float*)data; break;
        case PORT_REV_DRIP:       h->revDrip       = (const float*)data; break;
        case PORT_TREM_SPEED:     h->tremSpeed     = (const float*)data; break;
        case PORT_TREM_DEPTH:     h->tremDepth     = (const float*)data; break;
        case PORT_TREM_MODE:      h->tremMode      = (const float*)data; break;
        case PORT_QUALITY_MODE:   h->qualityMode   = (const float*)data; break;
        case PORT_SPEAKER_CAB:    h->speakerCab    = (const float*)data; break;
        case PORT_SPEAKER_DRIVE:  h->speakerDrive  = (const float*)data; break;
        case PORT_OUTPUT_LEVEL:   h->outputLevel   = (const float*)data; break;
        case PORT_NOISE_GATE:     h->noiseGate     = (const float*)data; break;
        case PORT_NOISE_SPECTRAL: h->noiseSpectral = (const float*)data; break;
        case PORT_NOISE_DEFIZZ:   h->noiseDefizz   = (const float*)data; break;
        case PORT_NOISE_LEVEL:    h->noiseLevel    = (const float*)data; break;
        default: break;
    }
}

static void activate(LV2_Handle instance) {
    EuphoriaAmpLV2* h = (EuphoriaAmpLV2*)instance;
    if (h && h->dsp) h->dsp->reset();
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    EuphoriaAmpLV2* h = (EuphoriaAmpLV2*)instance;
    if (!h || !h->dsp || !h->audioIn || !h->audioOut) return;

    h->dsp->process(
        h->audioIn, h->audioOut, nullptr, n_samples,
        h->bypass        ? *h->bypass        : 1.0f,
        h->channelMode   ? *h->channelMode   : 2.0f,
        h->cleanVol      ? *h->cleanVol      : 6.2f,
        h->driveGain     ? *h->driveGain     : 5.8f,
        h->driveVol      ? *h->driveVol      : 5.5f,
        h->bass          ? *h->bass          : 5.5f,
        h->mid           ? *h->mid           : 6.0f,
        h->treble        ? *h->treble        : 6.5f,
        h->brightSw      ? *h->brightSw      : 1.0f,
        h->slopeMode     ? *h->slopeMode     : 0.0f,
        h->powerTubes    ? *h->powerTubes    : 0.0f,
        h->master        ? *h->master        : 5.5f,
        h->presence      ? *h->presence      : 5.8f,
        h->sagAmount     ? *h->sagAmount     : 6.0f,
        h->revDwell      ? *h->revDwell      : 5.0f,
        h->revMix        ? *h->revMix        : 22.0f,
        h->revTone       ? *h->revTone       : 6.2f,
        h->revDrip       ? *h->revDrip       : 6.8f,
        h->tremSpeed     ? *h->tremSpeed     : 3.8f,
        h->tremDepth     ? *h->tremDepth     : 28.0f,
        h->tremMode      ? *h->tremMode      : 1.0f,
        h->qualityMode   ? *h->qualityMode   : 1.0f,
        h->speakerCab    ? *h->speakerCab    : 0.0f,
        h->speakerDrive  ? *h->speakerDrive  : 5.5f,
        h->outputLevel   ? *h->outputLevel   : 7.0f,
        h->noiseGate     ? *h->noiseGate     : 1.0f,
        h->noiseSpectral ? *h->noiseSpectral : 1.0f,
        h->noiseDefizz   ? *h->noiseDefizz   : 1.0f,
        h->noiseLevel    ? *h->noiseLevel    : 5.0f,
        false
    );
}

static void deactivate(LV2_Handle instance) {}

static void cleanup(LV2_Handle instance) {
    EuphoriaAmpLV2* h = (EuphoriaAmpLV2*)instance;
    if (h) delete h;
}

static const void* extension_data(const char* uri) { return nullptr; }

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
