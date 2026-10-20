// ============================================================================
// Cyber Ultimate Hybrid — Native Windows 64-Bit 4-Page Hardware Amp & Rack App
// (Tubular Amp — Pure Analog Vacuum Tube Circuit Emulation Engine)
// ============================================================================
// Features 4 Separate Hardware Views:
//   Page 0: '65 Fender Blackface AB763 Clean Amp Head (Black Tolex + Silver Grille + Skirted Dials)
//   Page 1: '59 Tweed Bassman 5F6-A / '68 JMP Plexi Amp Head (Lacquered Tweed + Brass Faceplate + Chicken-Head Knobs)
//   Page 2: Accutronics 3-Spring Reverb & Brownface / Greg Harmonic Tube Vibrato Rack (Animated Springs + Bakelite Dials)
//   Page 3: Live Mixer Studio 15/30 IPS Tape & Ping-Pong Delay Rack (Spinning 2" Tape Reels + Oxide/Flutter/Ducker)
//   Audio Settings: Full Audio Hardware & Guitar Interface Configuration Dialog
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <initguid.h>
#include <windows.h>
#include <windowsx.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <algorithm>

#define CYBER_ENABLE_STUDIO_DELAY 1
#include "../../cyber-ultimate-hybrid.lv2/src/CyberUltimateHybridEngine.hpp"

using namespace CyberDSP;

#pragma comment(lib, "ksuser.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

static constexpr int SAMPLE_RATE  = 48000;

struct AudioDeviceConfig {
    int inDevId = -1;        // -1 = Default Windows Device, 0..N = device index
    int outDevId = -1;       // -1 = Default Windows Device, 0..N = device index
    int inChannel = 0;       // 0 = Ch 1 (Left / Inst 1), 1 = Ch 2 (Right / Inst 2), 2 = Stereo Sum
    int bufferFrames = 256;  // 128 (2.7ms), 256 (5.3ms), 512 (10.7ms)
    char inDevName[128] = "Default Windows Recording Device";
    char outDevName[128] = "Default Windows Playback Device";
};

struct DeviceItem {
    int id; // -1 for default, 0..N for device index
    char name[128];
    wchar_t devIdStr[256];
};

static AudioDeviceConfig g_audioCfg;
static std::atomic<bool> g_audioConfigChanged{false};
static std::atomic<float> g_inMeterPeak{0.0f};
static std::atomic<float> g_outMeterPeakL{0.0f};
static std::atomic<float> g_outMeterPeakR{0.0f};
static std::atomic<bool>  g_running{true};
static std::atomic<bool>  g_testRiffActive{false};

static std::vector<DeviceItem> g_inDevList;
static std::vector<DeviceItem> g_outDevList;

static void refreshAudioDeviceLists() {
    CoInitialize(nullptr);
    g_inDevList.clear();
    g_outDevList.clear();

    DeviceItem defIn = { -1, "Default Windows Recording Device", L"" };
    g_inDevList.push_back(defIn);
    DeviceItem defOut = { -1, "Default Windows Playback Device", L"" };
    g_outDevList.push_back(defOut);

    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (SUCCEEDED(hr) && pEnum) {
        // Capture devices (Microphones, Guitar Interfaces)
        IMMDeviceCollection* pColIn = nullptr;
        if (SUCCEEDED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pColIn))) {
            UINT count = 0;
            pColIn->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice* pDev = nullptr;
                if (SUCCEEDED(pColIn->Item(i, &pDev))) {
                    LPWSTR pStrId = nullptr;
                    pDev->GetId(&pStrId);
                    IPropertyStore* props = nullptr;
                    pDev->OpenPropertyStore(STGM_READ, &props);
                    PROPVARIANT var;
                    PropVariantInit(&var);
                    props->GetValue(PKEY_Device_FriendlyName, &var);

                    DeviceItem item;
                    item.id = (int)i;
                    item.devIdStr[0] = L'\0';
                    if (pStrId) {
                        wcsncpy(item.devIdStr, pStrId, 255);
                        CoTaskMemFree(pStrId);
                    }
                    char nameBuf[128] = "Audio Input";
                    if (var.pwszVal) {
                        WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
                    }
                    std::snprintf(item.name, sizeof(item.name), "[%u] %s", i + 1, nameBuf);
                    PropVariantClear(&var);
                    props->Release();
                    pDev->Release();
                    g_inDevList.push_back(item);
                }
            }
            pColIn->Release();
        }

        // Render devices (Speakers, Headphones)
        IMMDeviceCollection* pColOut = nullptr;
        if (SUCCEEDED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pColOut))) {
            UINT count = 0;
            pColOut->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice* pDev = nullptr;
                if (SUCCEEDED(pColOut->Item(i, &pDev))) {
                    LPWSTR pStrId = nullptr;
                    pDev->GetId(&pStrId);
                    IPropertyStore* props = nullptr;
                    pDev->OpenPropertyStore(STGM_READ, &props);
                    PROPVARIANT var;
                    PropVariantInit(&var);
                    props->GetValue(PKEY_Device_FriendlyName, &var);

                    DeviceItem item;
                    item.id = (int)i;
                    item.devIdStr[0] = L'\0';
                    if (pStrId) {
                        wcsncpy(item.devIdStr, pStrId, 255);
                        CoTaskMemFree(pStrId);
                    }
                    char nameBuf[128] = "Audio Output";
                    if (var.pwszVal) {
                        WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
                    }
                    std::snprintf(item.name, sizeof(item.name), "[%u] %s", i + 1, nameBuf);
                    PropVariantClear(&var);
                    props->Release();
                    pDev->Release();
                    g_outDevList.push_back(item);
                }
            }
            pColOut->Release();
        }

        pEnum->Release();
    }
    CoUninitialize();
}

static std::string getIniPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* p = strrchr(exePath, '\\');
    if (p) *(p + 1) = '\0';
    return std::string(exePath) + "cyber_ultimate_hybrid_audio.ini";
}

static void saveAudioConfig() {
    std::string path = getIniPath();
    WritePrivateProfileStringA("Audio", "InDevName", g_audioCfg.inDevName, path.c_str());
    WritePrivateProfileStringA("Audio", "OutDevName", g_audioCfg.outDevName, path.c_str());
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", g_audioCfg.inChannel);
    WritePrivateProfileStringA("Audio", "InChannel", buf, path.c_str());
    std::snprintf(buf, sizeof(buf), "%d", g_audioCfg.bufferFrames);
    WritePrivateProfileStringA("Audio", "BufferFrames", buf, path.c_str());
}

static void loadAudioConfig() {
    refreshAudioDeviceLists();
    std::string path = getIniPath();
    char inName[128] = "", outName[128] = "";
    GetPrivateProfileStringA("Audio", "InDevName", "", inName, sizeof(inName), path.c_str());
    GetPrivateProfileStringA("Audio", "OutDevName", "", outName, sizeof(outName), path.c_str());
    g_audioCfg.inChannel = GetPrivateProfileIntA("Audio", "InChannel", 0, path.c_str());
    g_audioCfg.bufferFrames = GetPrivateProfileIntA("Audio", "BufferFrames", 256, path.c_str());
    if (g_audioCfg.bufferFrames != 128 && g_audioCfg.bufferFrames != 256 && g_audioCfg.bufferFrames != 512) {
        g_audioCfg.bufferFrames = 256;
    }

    if (std::strlen(inName) > 0) {
        for (const auto& dev : g_inDevList) {
            if (std::strstr(dev.name, inName) != nullptr || std::strcmp(dev.name, inName) == 0) {
                g_audioCfg.inDevId = dev.id;
                std::strncpy(g_audioCfg.inDevName, dev.name, sizeof(g_audioCfg.inDevName) - 1);
                break;
            }
        }
    } else {
        // Auto-detect USB Audio CODEC / interface if connected
        for (const auto& dev : g_inDevList) {
            if (std::strstr(dev.name, "USB Audio") != nullptr || std::strstr(dev.name, "CODEC") != nullptr) {
                g_audioCfg.inDevId = dev.id;
                std::strncpy(g_audioCfg.inDevName, dev.name, sizeof(g_audioCfg.inDevName) - 1);
                break;
            }
        }
    }

    if (std::strlen(outName) > 0) {
        for (const auto& dev : g_outDevList) {
            if (std::strstr(dev.name, outName) != nullptr || std::strcmp(dev.name, outName) == 0) {
                g_audioCfg.outDevId = dev.id;
                std::strncpy(g_audioCfg.outDevName, dev.name, sizeof(g_audioCfg.outDevName) - 1);
                break;
            }
        }
    } else {
        // Auto-detect USB Audio CODEC / interface for playback if connected
        for (const auto& dev : g_outDevList) {
            if (std::strstr(dev.name, "USB Audio") != nullptr || std::strstr(dev.name, "CODEC") != nullptr) {
                g_audioCfg.outDevId = dev.id;
                std::strncpy(g_audioCfg.outDevName, dev.name, sizeof(g_audioCfg.outDevName) - 1);
                break;
            }
        }
    }
}

struct KnobCtrl {
    const char* label;
    int pageIdx; // -1 = Global Header, 0 = Blackface, 1 = Bassman/JMP, 2 = Reverb/Vibrato, 3 = Tape Delay
    float minVal;
    float maxVal;
    float defVal;
    float curVal;
    bool isStepped;
    int steps;
    const char* stepLabels[4];
    RECT rect;
};

static CyberUltimateHybridEngine g_engine{double(SAMPLE_RATE)};
static int   g_activePage = 0;
static float g_animPhase  = 0.0f;

static RECT g_tabRects[4] = {
    {  18, 12, 218, 48 },
    { 224, 12, 424, 48 },
    { 430, 12, 630, 48 },
    { 636, 12, 836, 48 }
};
static RECT g_riffBtnRect = { 846, 12, 1000, 48 };
static RECT g_audioSettingsBtnRect = { 1010, 12, 1182, 48 };

// Parameter Indices
enum {
    K_CH_MODE = 0, K_QUALITY, K_CAB_SIM,
    // Page 0: '65 Blackface AB763 (3..9)
    K_BF_VOL, K_BF_BRIGHT, K_BF_TREBLE, K_BF_MID, K_BF_BASS, K_BF_MASTER, K_BF_OUT,
    // Page 1: '59 Bassman / '68 JMP (10..17)
    K_BM_DRIVE, K_BM_VOL, K_BM_SLOPE, K_BM_TUBES, K_BM_PRESENCE, K_BM_SAG, K_BM_SPKDRV,
    // Page 2: 3-Spring Reverb & Harmonic Vibrato (18..24)
    K_REV_DWELL, K_REV_MIX, K_REV_TONE, K_REV_DRIP, K_VIB_MODE, K_VIB_SPEED, K_VIB_DEPTH,
    // Page 3: Studio Tape Delay Rack (25..32)
    K_DLY_ON, K_DLY_PINGPONG, K_DLY_TIME, K_DLY_FB, K_DLY_MIX, K_DLY_DRIVE, K_DLY_FLUTTER, K_DLY_DUCK,
    NUM_KNOBS
};

static KnobCtrl g_knobs[NUM_KNOBS] = {
    // Global switches shown in top-right subbar (page -1)
    {"ROUTING",      -1, 0, 2, 2, 2, true, 3, {"CH1 BLACKFACE", "CH2 BASSMAN/JMP", "JUMPERED BLEND", ""}, { 24, 58, 240, 90 }},
    {"PHYSICS CPU",  -1, 0, 3, 2, 2, true, 4, {"ECO LIVE 1x", "STAGE 2x", "STUDIO 4x", "ULTRA 8x"},      { 252, 58, 450, 90 }},
    {"CABINET IR",   -1, 0, 3, 1, 1, true, 4, {"BYPASS CAB", "2x12 JENSEN C12N", "4x10 BASSMAN P10R", "4x12 GREENBACK"}, { 462, 58, 690, 90 }},

    // Page 0: '65 Fender Blackface AB763 Clean
    {"VOLUME",        0, 0, 10, 4.8f, 4.8f, false, 0, {}, {}},
    {"BRIGHT CAP",    0, 0,  1, 1.0f, 1.0f, true,  2, {"OFF", "120pF ON", "", ""}, {}},
    {"TREBLE",        0, 0, 10, 6.2f, 6.2f, false, 0, {}, {}},
    {"MIDDLE",        0, 0, 10, 4.8f, 4.8f, false, 0, {}, {}},
    {"BASS",          0, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"MASTER",        0, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"OUTPUT TRIM",   0, 0, 10, 7.5f, 7.5f, false, 0, {}, {}},

    // Page 1: '59 Tweed Bassman 5F6-A / '68 Marshall JMP
    {"PREAMP DRIVE",  1, 0, 10, 5.8f, 5.8f, false, 0, {}, {}},
    {"CHANNEL VOL",   1, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"SLOPE VOICING", 1, 0,  1, 0.0f, 0.0f, true,  2, {"56k '59 TWEED", "33k '68 JMP", "", ""}, {}},
    {"POWER TUBES",   1, 0,  1, 0.0f, 0.0f, true,  2, {"2x 6L6GC USA", "2x EL34 UK", "", ""}, {}},
    {"PRESENCE",      1, 0, 10, 5.2f, 5.2f, false, 0, {}, {}},
    {"B+ TUBE SAG",   1, 0, 10, 4.5f, 4.5f, false, 0, {}, {}},
    {"CONE DRIVE",    1, 0, 10, 4.0f, 4.0f, false, 0, {}, {}},

    // Page 2: Accutronics 3-Spring Reverb & Brownface / Greg Harmonic Vibrato
    {"TUBE DWELL",    2, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"REVERB MIX",    2, 0, 10, 3.2f, 3.2f, false, 0, {}, {}},
    {"TANK TONE",     2, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"SPRING DRIP",   2, 0, 10, 6.0f, 6.0f, false, 0, {}, {}},
    {"VIB CIRCUIT",   2, 0,  1, 0.0f, 0.0f, true,  2, {"HARMONIC TUBE", "BF OPTO CELL", "", ""}, {}},
    {"SPEED (Hz)",    2, 0.5f, 12.0f, 4.2f, 4.2f, false, 0, {}, {}},
    {"INTENSITY",     2, 0, 10, 4.0f, 4.0f, false, 0, {}, {}},

    // Page 3: Studio 15/30 IPS Tape & Ping-Pong Delay Rack
    {"TAPE BYPASS",   3, 0,  1, 1.0f, 1.0f, true,  2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"ROUTING MODE",  3, 0,  1, 1.0f, 1.0f, true,  2, {"15/30 MONO", "PING-PONG", "", ""}, {}},
    {"DELAY TIME ms", 3, 20, 1500, 360.0f, 360.0f, false, 0, {}, {}},
    {"FEEDBACK %",    3, 0, 95, 34.0f, 34.0f, false, 0, {}, {}},
    {"WET MIX %",     3, 0, 100, 22.0f, 22.0f, false, 0, {}, {}},
    {"OXIDE DRIVE %", 3, 0, 100, 28.0f, 28.0f, false, 0, {}, {}},
    {"WOW & FLUTTER", 3, 0, 100, 18.0f, 18.0f, false, 0, {}, {}},
    {"AUTO DUCKING",  3, 0, 100, 25.0f, 25.0f, false, 0, {}, {}}
};

static void layoutPageKnobs() {
    for (int page = 0; page < 4; ++page) {
        std::vector<int> indices;
        for (int i = 0; i < NUM_KNOBS; ++i) {
            if (g_knobs[i].pageIdx == page) indices.push_back(i);
        }
        int n = (int)indices.size();
        if (n == 0) continue;
        int startX = 48;
        int totalW = 1104;
        int slotW = totalW / n;
        for (int k = 0; k < n; ++k) {
            int cx = startX + k * slotW + slotW / 2;
            g_knobs[indices[k]].rect = { cx - 58, 196, cx + 58, 356 };
        }
    }
}

static void runEngineBlock(const float* inBuf, float* outL, float* outR, uint32_t frames) {
    const float dlyMix = (g_knobs[K_DLY_ON].curVal > 0.5f) ? g_knobs[K_DLY_MIX].curVal : 0.0f;
    g_engine.setDelayParams(
        g_knobs[K_DLY_TIME].curVal,
        g_knobs[K_DLY_FB].curVal,
        dlyMix,
        g_knobs[K_DLY_DRIVE].curVal,
        g_knobs[K_DLY_FLUTTER].curVal,
        (g_knobs[K_DLY_PINGPONG].curVal > 0.5f) ? 1 : 0,
        g_knobs[K_DLY_DUCK].curVal
    );

    g_engine.process(
        inBuf, outL, outR, frames,
        1.0f,
        g_knobs[K_CH_MODE].curVal,
        g_knobs[K_BF_VOL].curVal,
        g_knobs[K_BM_DRIVE].curVal,
        g_knobs[K_BM_VOL].curVal,
        g_knobs[K_BF_BASS].curVal,
        g_knobs[K_BF_MID].curVal,
        g_knobs[K_BF_TREBLE].curVal,
        g_knobs[K_BF_BRIGHT].curVal,
        g_knobs[K_BM_SLOPE].curVal,
        g_knobs[K_BM_TUBES].curVal,
        g_knobs[K_BF_MASTER].curVal,
        g_knobs[K_BM_PRESENCE].curVal,
        g_knobs[K_BM_SAG].curVal,
        g_knobs[K_REV_DWELL].curVal,
        g_knobs[K_REV_MIX].curVal,
        g_knobs[K_REV_TONE].curVal,
        g_knobs[K_REV_DRIP].curVal,
        g_knobs[K_VIB_SPEED].curVal,
        g_knobs[K_VIB_DEPTH].curVal,
        g_knobs[K_VIB_MODE].curVal,
        g_knobs[K_QUALITY].curVal,
        g_knobs[K_CAB_SIM].curVal,
        g_knobs[K_BM_SPKDRV].curVal,
        g_knobs[K_BF_OUT].curVal,
        1.0f, 1.0f, 1.0f, 5.0f,
        false
    );
}

struct TestGuitarSynth {
    double phase = 0.0;
    double noteEnv = 0.0;
    int sampleCounter = 0;
    int stepIdx = 0;
    float nextSample() {
        static const double chordNotes[] = { 82.41, 110.0, 146.83, 196.00, 246.94, 329.63, 220.0, 164.81 };
        const int stepSamples = SAMPLE_RATE / 3;
        if (sampleCounter % stepSamples == 0) {
            noteEnv = 1.0;
            stepIdx = (stepIdx + 1) % 8;
        }
        sampleCounter++;
        const double freq = chordNotes[stepIdx];
        phase += freq / double(SAMPLE_RATE);
        if (phase >= 1.0) phase -= 1.0;
        noteEnv *= 0.99988;
        const double p2 = std::fmod(phase * 2.0, 1.0);
        const double p3 = std::fmod(phase * 3.0, 1.0);
        const double raw = (std::sin(phase * 6.2831853) * 0.65
                          + std::sin(p2 * 6.2831853) * 0.25
                          + std::sin(p3 * 6.2831853) * 0.10) * noteEnv;
        return float(raw * 0.45);
    }
} g_riffSynth;

template<typename T, size_t CAPACITY>
class LockFreeRingBuffer {
public:
    std::atomic<size_t> writeHead{0};
    std::atomic<size_t> readHead{0};
    T buffer[CAPACITY] = {};

    size_t availableRead() const {
        size_t w = writeHead.load(std::memory_order_acquire);
        size_t r = readHead.load(std::memory_order_relaxed);
        return (w >= r) ? (w - r) : (CAPACITY - (r - w));
    }

    size_t availableWrite() const {
        return (CAPACITY - 1) - availableRead();
    }

    size_t write(const T* src, size_t count) {
        size_t avail = availableWrite();
        size_t toWrite = std::min(count, avail);
        size_t w = writeHead.load(std::memory_order_relaxed);
        for (size_t i = 0; i < toWrite; ++i) {
            buffer[w] = src[i];
            w = (w + 1) % CAPACITY;
        }
        writeHead.store(w, std::memory_order_release);
        return toWrite;
    }

    size_t read(T* dst, size_t count) {
        size_t avail = availableRead();
        size_t toRead = std::min(count, avail);
        size_t r = readHead.load(std::memory_order_relaxed);
        for (size_t i = 0; i < toRead; ++i) {
            dst[i] = buffer[r];
            r = (r + 1) % CAPACITY;
        }
        readHead.store(r, std::memory_order_release);
        return toRead;
    }

    void drop(size_t count) {
        size_t avail = availableRead();
        size_t toDrop = std::min(count, avail);
        size_t r = (readHead.load(std::memory_order_relaxed) + toDrop) % CAPACITY;
        readHead.store(r, std::memory_order_release);
    }

    void reset() {
        writeHead.store(0, std::memory_order_release);
        readHead.store(0, std::memory_order_release);
    }
};

static LockFreeRingBuffer<float, 65536> g_ringBuffer;

static DWORD WINAPI audioThreadProc(LPVOID) {
    CoInitialize(nullptr);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    timeBeginPeriod(1);

    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (FAILED(hr) || !pEnum) {
        timeEndPeriod(1);
        CoUninitialize();
        return 1;
    }

    IMMDevice* pRenderDev = nullptr;
    IMMDevice* pCaptureDev = nullptr;
    IAudioClient* pAudioClientOut = nullptr;
    IAudioClient* pAudioClientIn = nullptr;
    IAudioRenderClient* pRenderClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pWfxOut = nullptr;
    WAVEFORMATEX* pWfxIn = nullptr;
    HANDLE hEventOut = nullptr;
    HANDLE hEventIn = nullptr;
    UINT32 bufOut = 0, bufIn = 0;

    auto closeAudioHardware = [&]() {
        if (pAudioClientIn)  { pAudioClientIn->Stop(); }
        if (pAudioClientOut) { pAudioClientOut->Stop(); }
        if (pCaptureClient)  { pCaptureClient->Release(); pCaptureClient = nullptr; }
        if (pRenderClient)   { pRenderClient->Release(); pRenderClient = nullptr; }
        if (pAudioClientIn)  { pAudioClientIn->Release(); pAudioClientIn = nullptr; }
        if (pAudioClientOut) { pAudioClientOut->Release(); pAudioClientOut = nullptr; }
        if (pCaptureDev)     { pCaptureDev->Release(); pCaptureDev = nullptr; }
        if (pRenderDev)      { pRenderDev->Release(); pRenderDev = nullptr; }
        if (pWfxIn)          { CoTaskMemFree(pWfxIn); pWfxIn = nullptr; }
        if (pWfxOut)         { CoTaskMemFree(pWfxOut); pWfxOut = nullptr; }
        if (hEventIn)        { CloseHandle(hEventIn); hEventIn = nullptr; }
        if (hEventOut)       { CloseHandle(hEventOut); hEventOut = nullptr; }
    };

    auto openAudioHardware = [&]() -> bool {
        closeAudioHardware();
        g_ringBuffer.reset();

        // 1. Get Render device
        if (g_audioCfg.outDevId == -1) {
            pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderDev);
        } else {
            for (const auto& dev : g_outDevList) {
                if (dev.id == g_audioCfg.outDevId && dev.devIdStr[0] != L'\0') {
                    pEnum->GetDevice(dev.devIdStr, &pRenderDev);
                    break;
                }
            }
            if (!pRenderDev) pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderDev);
        }

        // 2. Get Capture device
        if (g_audioCfg.inDevId == -1) {
            pEnum->GetDefaultAudioEndpoint(eCapture, eConsole, &pCaptureDev);
        } else {
            for (const auto& dev : g_inDevList) {
                if (dev.id == g_audioCfg.inDevId && dev.devIdStr[0] != L'\0') {
                    pEnum->GetDevice(dev.devIdStr, &pCaptureDev);
                    break;
                }
            }
            if (!pCaptureDev) pEnum->GetDefaultAudioEndpoint(eCapture, eConsole, &pCaptureDev);
        }

        if (!pRenderDev) return false;

        // 3. Activate Audio Clients
        pRenderDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientOut);
        if (!pAudioClientOut) return false;
        pAudioClientOut->GetMixFormat(&pWfxOut);

        if (pCaptureDev) {
            pCaptureDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientIn);
            if (pAudioClientIn) pAudioClientIn->GetMixFormat(&pWfxIn);
        }

        REFERENCE_TIME hnsReq = (g_audioCfg.bufferFrames == 128) ? 30000 : ((g_audioCfg.bufferFrames == 512) ? 120000 : 60000);

        hEventOut = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        hr = pAudioClientOut->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsReq, 0, pWfxOut, nullptr);
        if (FAILED(hr)) return false;
        pAudioClientOut->SetEventHandle(hEventOut);
        pAudioClientOut->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
        pAudioClientOut->GetBufferSize(&bufOut);

        // Pre-fill initial render buffer with silence
        BYTE* pData = nullptr;
        if (SUCCEEDED(pRenderClient->GetBuffer(bufOut, &pData))) {
            std::memset(pData, 0, bufOut * pWfxOut->nBlockAlign);
            pRenderClient->ReleaseBuffer(bufOut, 0);
        }

        // Initialize capture if available
        if (pAudioClientIn && pWfxIn) {
            hEventIn = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            hr = pAudioClientIn->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsReq, 0, pWfxIn, nullptr);
            if (SUCCEEDED(hr)) {
                pAudioClientIn->SetEventHandle(hEventIn);
                pAudioClientIn->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
                pAudioClientIn->GetBufferSize(&bufIn);
            }
        }

        // Pre-fill ring buffer with 1024 samples of silence cushion to guarantee zero initial underrun
        std::vector<float> silence(1024, 0.0f);
        g_ringBuffer.write(silence.data(), silence.size());

        if (pAudioClientIn) pAudioClientIn->Start();
        pAudioClientOut->Start();
        return true;
    };

    openAudioHardware();

    std::vector<float> inFloat(4096, 0.0f);
    std::vector<float> outFloatL(4096, 0.0f);
    std::vector<float> outFloatR(4096, 0.0f);

    while (g_running.load()) {
        if (g_audioConfigChanged.exchange(false)) {
            openAudioHardware();
            continue;
        }

        HANDLE waitEvents[2] = { hEventOut, hEventIn };
        DWORD numWait = (hEventIn != nullptr) ? 2 : 1;
        WaitForMultipleObjects(numWait, waitEvents, FALSE, 20);

        // 1. Drain Capture Client
        if (pCaptureClient && pWfxIn) {
            UINT32 packetLen = 0;
            float maxIn = 0.0f;
            const int inChannels = pWfxIn->nChannels;
            const int chSelect = g_audioCfg.inChannel;
            const bool isFloat = (pWfxIn->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                                 (pWfxIn->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                                  ((WAVEFORMATEXTENSIBLE*)pWfxIn)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));

            while (SUCCEEDED(pCaptureClient->GetNextPacketSize(&packetLen)) && packetLen > 0) {
                BYTE* pInBuf = nullptr;
                UINT32 numFramesRead = 0;
                DWORD flags = 0;
                if (SUCCEEDED(pCaptureClient->GetBuffer(&pInBuf, &numFramesRead, &flags, nullptr, nullptr))) {
                    if (numFramesRead > 0) {
                        float inSamples[2048];
                        UINT32 framesToProc = std::min(numFramesRead, (UINT32)2048);

                        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                            for (UINT32 i = 0; i < framesToProc; ++i) inSamples[i] = 0.0f;
                        } else if (isFloat) {
                            const float* pF = reinterpret_cast<const float*>(pInBuf);
                            for (UINT32 i = 0; i < framesToProc; ++i) {
                                float s = 0.0f;
                                if (inChannels == 1) {
                                    s = pF[i];
                                } else {
                                    if (chSelect == 0) s = pF[i * inChannels + 0];
                                    else if (chSelect == 1) s = pF[i * inChannels + 1];
                                    else s = (pF[i * inChannels + 0] + pF[i * inChannels + 1]) * 0.5f;
                                }
                                inSamples[i] = s;
                                if (std::fabs(s) > maxIn) maxIn = std::fabs(s);
                            }
                        } else if (pWfxIn->wBitsPerSample == 16) {
                            const int16_t* p16 = reinterpret_cast<const int16_t*>(pInBuf);
                            for (UINT32 i = 0; i < framesToProc; ++i) {
                                float s = 0.0f;
                                if (inChannels == 1) {
                                    s = float(p16[i]) / 32768.0f;
                                } else {
                                    float l = float(p16[i * inChannels + 0]) / 32768.0f;
                                    float r = float(p16[i * inChannels + 1]) / 32768.0f;
                                    s = (chSelect == 0) ? l : ((chSelect == 1) ? r : (l + r) * 0.5f);
                                }
                                inSamples[i] = s;
                                if (std::fabs(s) > maxIn) maxIn = std::fabs(s);
                            }
                        }
                        g_ringBuffer.write(inSamples, framesToProc);
                    }
                    pCaptureClient->ReleaseBuffer(numFramesRead);
                }
            }
            if (maxIn > 0.0f) g_inMeterPeak.store(maxIn);
            else g_inMeterPeak.store(g_inMeterPeak.load() * 0.95f);
        }

        // 2. Fill Render Client
        if (pAudioClientOut && pRenderClient && pWfxOut) {
            UINT32 padding = 0;
            pAudioClientOut->GetCurrentPadding(&padding);
            UINT32 framesNeeded = bufOut - padding;

            if (framesNeeded > 0) {
                if (framesNeeded > inFloat.size()) {
                    inFloat.resize(framesNeeded);
                    outFloatL.resize(framesNeeded);
                    outFloatR.resize(framesNeeded);
                }

                size_t avail = g_ringBuffer.availableRead();
                size_t readCount = g_ringBuffer.read(inFloat.data(), framesNeeded);

                // Zero out any remaining frames if buffer was momentarily starved
                if (readCount < framesNeeded) {
                    for (size_t i = readCount; i < framesNeeded; ++i) inFloat[i] = 0.0f;
                }

                // Clock drift compensation: keep cushion locked around ~1024 samples
                if (avail > 2560) {
                    g_ringBuffer.drop(1); // Gently slip 1 sample
                }

                // Inject test riff synth if enabled
                if (g_testRiffActive.load()) {
                    for (UINT32 i = 0; i < framesNeeded; ++i) {
                        inFloat[i] += g_riffSynth.nextSample();
                    }
                }

                // Process through full DSP Amp & Rack engine
                runEngineBlock(inFloat.data(), outFloatL.data(), outFloatR.data(), framesNeeded);

                BYTE* pOutData = nullptr;
                if (SUCCEEDED(pRenderClient->GetBuffer(framesNeeded, &pOutData))) {
                    const int outChannels = pWfxOut->nChannels;
                    const bool isFloat = (pWfxOut->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                                         (pWfxOut->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                                          ((WAVEFORMATEXTENSIBLE*)pWfxOut)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));

                    float peakL = 0.0f, peakR = 0.0f;

                    if (isFloat) {
                        float* pFloat = reinterpret_cast<float*>(pOutData);
                        for (UINT32 i = 0; i < framesNeeded; ++i) {
                            float l = std::tanh(outFloatL[i]);
                            float r = std::tanh(outFloatR[i]);
                            if (std::fabs(l) > peakL) peakL = std::fabs(l);
                            if (std::fabs(r) > peakR) peakR = std::fabs(r);
                            pFloat[i * outChannels + 0] = l;
                            if (outChannels > 1) {
                                pFloat[i * outChannels + 1] = r;
                            }
                        }
                    } else if (pWfxOut->wBitsPerSample == 16) {
                        int16_t* p16 = reinterpret_cast<int16_t*>(pOutData);
                        for (UINT32 i = 0; i < framesNeeded; ++i) {
                            float l = std::tanh(outFloatL[i]);
                            float r = std::tanh(outFloatR[i]);
                            if (std::fabs(l) > peakL) peakL = std::fabs(l);
                            if (std::fabs(r) > peakR) peakR = std::fabs(r);
                            p16[i * outChannels + 0] = int16_t(std::clamp(l, -1.0f, 1.0f) * 32760.0f);
                            if (outChannels > 1) {
                                p16[i * outChannels + 1] = int16_t(std::clamp(r, -1.0f, 1.0f) * 32760.0f);
                            }
                        }
                    }

                    g_outMeterPeakL.store(peakL);
                    g_outMeterPeakR.store(peakR);
                    pRenderClient->ReleaseBuffer(framesNeeded, 0);
                }
            }
        }
    }

    closeAudioHardware();
    if (pEnum) pEnum->Release();
    timeEndPeriod(1);
    CoUninitialize();
    return 0;
}

static void fillRoundRect(HDC hdc, int x1, int y1, int x2, int y2, int r, COLORREF fillCol, COLORREF borderCol, int borderW = 2) {
    HBRUSH br = CreateSolidBrush(fillCol);
    HPEN pen = CreatePen(PS_SOLID, borderW, borderCol);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, pen);
    RoundRect(hdc, x1, y1, x2, y2, r, r);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(br);
    DeleteObject(pen);
}

static void drawStylizedKnob(HDC hdc, const KnobCtrl& k, int pageStyle) {
    int cx = (k.rect.left + k.rect.right) / 2;
    int cy = k.rect.top + 56;
    int r = 38;

    float norm = (k.curVal - k.minVal) / (k.maxVal - k.minVal);
    norm = std::max(0.0f, std::min(1.0f, norm));
    float angle = -2.356194f + norm * 4.712389f;

    for (int t = 0; t <= 10; ++t) {
        float ta = -2.356194f + (t / 10.0f) * 4.712389f;
        int tx1 = cx + int(std::cos(ta) * (r + 4));
        int ty1 = cy + int(std::sin(ta) * (r + 4));
        int tx2 = cx + int(std::cos(ta) * (r + 8));
        int ty2 = cy + int(std::sin(ta) * (r + 8));
        HPEN tickPen = CreatePen(PS_SOLID, 2, (pageStyle == 1) ? RGB(69, 26, 3) : RGB(203, 213, 225));
        HGDIOBJ op = SelectObject(hdc, tickPen);
        MoveToEx(hdc, tx1, ty1, nullptr);
        LineTo(hdc, tx2, ty2);
        SelectObject(hdc, op);
        DeleteObject(tickPen);
    }

    if (pageStyle == 0) {
        fillRoundRect(hdc, cx - r, cy - r, cx + r, cy + r, r * 2, RGB(15, 23, 42), RGB(148, 163, 184), 2);
        int capR = r - 12;
        fillRoundRect(hdc, cx - capR, cy - capR, cx + capR, cy + capR, capR * 2, RGB(226, 232, 240), RGB(100, 116, 139), 1);
        int px = cx + int(std::cos(angle) * (r - 2));
        int py = cy + int(std::sin(angle) * (r - 2));
        HPEN ptPen = CreatePen(PS_SOLID, 4, RGB(255, 255, 255));
        HGDIOBJ op = SelectObject(hdc, ptPen);
        MoveToEx(hdc, cx, cy, nullptr);
        LineTo(hdc, px, py);
        SelectObject(hdc, op);
        DeleteObject(ptPen);
    } else if (pageStyle == 1) {
        fillRoundRect(hdc, cx - r, cy - r, cx + r, cy + r, r * 2, RGB(69, 10, 10), RGB(245, 158, 11), 2);
        int px = cx + int(std::cos(angle) * (r + 6));
        int py = cy + int(std::sin(angle) * (r + 6));
        int bx1 = cx + int(std::cos(angle + 2.4f) * 12);
        int by1 = cy + int(std::sin(angle + 2.4f) * 12);
        int bx2 = cx + int(std::cos(angle - 2.4f) * 12);
        int by2 = cy + int(std::sin(angle - 2.4f) * 12);
        POINT tri[3] = { { px, py }, { bx1, by1 }, { bx2, by2 } };
        HBRUSH br = CreateSolidBrush(RGB(120, 20, 20));
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(254, 240, 138));
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        Polygon(hdc, tri, 3);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);
        fillRoundRect(hdc, cx - 10, cy - 10, cx + 10, cy + 10, 20, RGB(245, 158, 11), RGB(69, 10, 10), 1);
    } else if (pageStyle == 2) {
        fillRoundRect(hdc, cx - r, cy - r, cx + r, cy + r, r * 2, RGB(15, 23, 42), RGB(16, 185, 129), 2);
        int capR = r - 10;
        fillRoundRect(hdc, cx - capR, cy - capR, cx + capR, cy + capR, capR * 2, RGB(30, 41, 59), RGB(52, 211, 153), 1);
        int px = cx + int(std::cos(angle) * (r - 2));
        int py = cy + int(std::sin(angle) * (r - 2));
        HPEN ptPen = CreatePen(PS_SOLID, 4, RGB(52, 211, 153));
        HGDIOBJ op = SelectObject(hdc, ptPen);
        MoveToEx(hdc, cx, cy, nullptr);
        LineTo(hdc, px, py);
        SelectObject(hdc, op);
        DeleteObject(ptPen);
    } else {
        fillRoundRect(hdc, cx - r, cy - r, cx + r, cy + r, r * 2, RGB(24, 24, 27), RGB(168, 85, 247), 2);
        int capR = r - 12;
        fillRoundRect(hdc, cx - capR, cy - capR, cx + capR, cy + capR, capR * 2, RGB(216, 180, 254), RGB(147, 51, 234), 1);
        int px = cx + int(std::cos(angle) * (r - 2));
        int py = cy + int(std::sin(angle) * (r - 2));
        HPEN ptPen = CreatePen(PS_SOLID, 4, RGB(124, 58, 237));
        HGDIOBJ op = SelectObject(hdc, ptPen);
        MoveToEx(hdc, cx, cy, nullptr);
        LineTo(hdc, px, py);
        SelectObject(hdc, op);
        DeleteObject(ptPen);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, (pageStyle == 1) ? RGB(40, 15, 0) : RGB(241, 245, 249));
    RECT tr = { cx - 58, cy + r + 10, cx + 58, cy + r + 28 };
    DrawTextA(hdc, k.label, -1, &tr, DT_CENTER | DT_SINGLELINE);

    char valBuf[32];
    if (k.isStepped) {
        int idx = std::max(0, std::min(k.steps - 1, int(std::round(k.curVal))));
        std::snprintf(valBuf, sizeof(valBuf), "%s", k.stepLabels[idx]);
    } else {
        std::snprintf(valBuf, sizeof(valBuf), "%.1f", k.curVal);
    }
    SetTextColor(hdc, (pageStyle == 1) ? RGB(120, 30, 0) : RGB(148, 163, 184));
    RECT vr = { cx - 58, cy + r + 28, cx + 58, cy + r + 46 };
    DrawTextA(hdc, valBuf, -1, &vr, DT_CENTER | DT_SINGLELINE);
}

static void renderGui(HWND hwnd, HDC hdcWin) {
    RECT cr;
    GetClientRect(hwnd, &cr);
    int w = cr.right - cr.left;
    int h = cr.bottom - cr.top;

    HDC hdc = CreateCompatibleDC(hdcWin);
    HBITMAP hbm = CreateCompatibleBitmap(hdcWin, w, h);
    HGDIOBJ oldBm = SelectObject(hdc, hbm);

    HBRUSH bgBr = CreateSolidBrush(RGB(9, 13, 22));
    FillRect(hdc, &cr, bgBr);
    DeleteObject(bgBr);

    HFONT fontTab   = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT fontTitle = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT fontSub   = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT fontKnob  = CreateFontA(12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, fontTab);
    const char* tabTitles[4] = {
        "1. '65 BLACKFACE AB763",
        "2. '59 BASSMAN / '68 JMP",
        "3. REVERB & HARMONIC VIB",
        "4. STUDIO 2\" TAPE DELAY"
    };

    for (int t = 0; t < 4; ++t) {
        bool active = (g_activePage == t);
        COLORREF bg = active ? RGB(30, 41, 59) : RGB(15, 23, 42);
        COLORREF br = active ? RGB(56, 189, 248) : RGB(51, 65, 85);
        fillRoundRect(hdc, g_tabRects[t].left, g_tabRects[t].top, g_tabRects[t].right, g_tabRects[t].bottom, 8, bg, br, active ? 2 : 1);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, active ? RGB(255, 255, 255) : RGB(148, 163, 184));
        DrawTextA(hdc, tabTitles[t], -1, &g_tabRects[t], DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    // Play Test Guitar Riff Button
    bool riffOn = g_testRiffActive.load();
    fillRoundRect(hdc, g_riffBtnRect.left, g_riffBtnRect.top, g_riffBtnRect.right, g_riffBtnRect.bottom, 8,
                  riffOn ? RGB(16, 185, 129) : RGB(30, 41, 59),
                  riffOn ? RGB(167, 243, 208) : RGB(100, 116, 139), 2);
    SetTextColor(hdc, riffOn ? RGB(6, 20, 14) : RGB(226, 232, 240));
    DrawTextA(hdc, riffOn ? "TEST RIFF: ON" : "TEST RIFF", -1, &g_riffBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Audio Settings Button
    fillRoundRect(hdc, g_audioSettingsBtnRect.left, g_audioSettingsBtnRect.top, g_audioSettingsBtnRect.right, g_audioSettingsBtnRect.bottom, 8,
                  RGB(14, 116, 144), RGB(56, 189, 248), 2);
    SetTextColor(hdc, RGB(240, 253, 250));
    DrawTextA(hdc, "AUDIO SETTINGS", -1, &g_audioSettingsBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // 2. Global Header Selector Pills (Routing, Physics CPU, Cabinet IR)
    SelectObject(hdc, fontKnob);
    for (int i = 0; i <= 2; ++i) {
        const KnobCtrl& gk = g_knobs[i];
        fillRoundRect(hdc, gk.rect.left, gk.rect.top, gk.rect.right, gk.rect.bottom, 6, RGB(17, 24, 39), RGB(56, 189, 248), 1);
        int idx = std::max(0, std::min(gk.steps - 1, int(std::round(gk.curVal))));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s: %s", gk.label, gk.stepLabels[idx]);
        SetTextColor(hdc, RGB(224, 242, 254));
        RECT tr = gk.rect;
        DrawTextA(hdc, buf, -1, &tr, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    // 3. Main Stylized Hardware View Area
    int bx1 = 18, by1 = 102, bx2 = w - 18, by2 = h - 46;

    if (g_activePage == 0) {
        // PAGE 0: '65 FENDER BLACKFACE AB763 AMP HEAD
        fillRoundRect(hdc, bx1, by1, bx2, by2, 16, RGB(18, 21, 28), RGB(100, 116, 139), 3);
        fillRoundRect(hdc, bx1 + 18, by1 + 16, bx2 - 18, by1 + 275, 10, RGB(7, 10, 15), RGB(203, 213, 225), 2);

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(56, 189, 248));
        RECT subR = { bx1 + 36, by1 + 24, bx1 + 650, by1 + 46 };
        DrawTextA(hdc, "CHANNEL 1  |  AB763 PLATE-DRIVEN SCOOPED TONE STACK & 120pF BRIGHT CAP", -1, &subR, DT_LEFT | DT_SINGLELINE);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(248, 250, 252));
        RECT titR = { bx1 + 36, by1 + 44, bx1 + 650, by1 + 82 };
        DrawTextA(hdc, "Cyber '65 Twin / Deluxe Blackface", -1, &titR, DT_LEFT | DT_SINGLELINE);

        fillRoundRect(hdc, bx2 - 76, by1 + 34, bx2 - 44, by1 + 66, 32, RGB(37, 99, 235), RGB(147, 197, 253), 2);

        fillRoundRect(hdc, bx1 + 18, by1 + 288, bx2 - 18, by2 - 16, 10, RGB(148, 163, 184), RGB(51, 65, 85), 2);
        HPEN grillePen = CreatePen(PS_SOLID, 1, RGB(71, 85, 105));
        HGDIOBJ oldP = SelectObject(hdc, grillePen);
        for (int gy = by1 + 294; gy < by2 - 20; gy += 6) {
            MoveToEx(hdc, bx1 + 22, gy, nullptr);
            LineTo(hdc, bx2 - 22, gy);
        }
        for (int gx = bx1 + 22; gx < bx2 - 22; gx += 6) {
            MoveToEx(hdc, gx, by1 + 294, nullptr);
            LineTo(hdc, gx, by2 - 20);
        }
        SelectObject(hdc, oldP);
        DeleteObject(grillePen);

        RECT badge = { bx1 + 42, by1 + 332, bx1 + 560, by1 + 388 };
        fillRoundRect(hdc, badge.left, badge.top, badge.right, badge.bottom, 8, RGB(9, 13, 22), RGB(226, 232, 240), 2);
        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(248, 250, 252));
        DrawTextA(hdc, "FENDER AB763 GLASSY CLEAN VOICING  |  2x12 JENSEN C12N", -1, &badge, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
    else if (g_activePage == 1) {
        // PAGE 1: '59 TWEED BASSMAN 5F6-A / '68 MARSHALL JMP PLEXI
        fillRoundRect(hdc, bx1, by1, bx2, by2, 16, RGB(180, 123, 42), RGB(92, 45, 12), 3);
        fillRoundRect(hdc, bx1 + 18, by1 + 16, bx2 - 18, by1 + 275, 10, RGB(234, 179, 8), RGB(69, 26, 3), 2);

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(69, 26, 3));
        RECT subR = { bx1 + 36, by1 + 24, bx1 + 780, by1 + 46 };
        DrawTextA(hdc, "CHANNEL 2  |  DC-COUPLED 12AX7 CATHODE FOLLOWER + SWITCHABLE 56k TWEED / 33k PLEXI SLOPE", -1, &subR, DT_LEFT | DT_SINGLELINE);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(28, 10, 0));
        RECT titR = { bx1 + 36, by1 + 44, bx1 + 750, by1 + 82 };
        DrawTextA(hdc, "'59 Tweed Bassman 5F6-A  /  '68 Marshall JMP Plexi", -1, &titR, DT_LEFT | DT_SINGLELINE);

        fillRoundRect(hdc, bx2 - 76, by1 + 34, bx2 - 44, by1 + 66, 32, RGB(220, 38, 38), RGB(254, 202, 202), 2);

        fillRoundRect(hdc, bx1 + 18, by1 + 288, bx2 - 18, by2 - 16, 10, RGB(69, 10, 10), RGB(120, 53, 15), 2);
        HPEN oxPen = CreatePen(PS_SOLID, 1, RGB(146, 64, 14));
        HGDIOBJ oldOxP = SelectObject(hdc, oxPen);
        for (int gy = by1 + 294; gy < by2 - 20; gy += 6) {
            MoveToEx(hdc, bx1 + 22, gy, nullptr);
            LineTo(hdc, bx2 - 22, gy);
        }
        SelectObject(hdc, oldOxP);
        DeleteObject(oxPen);

        RECT badge = { bx1 + 42, by1 + 332, bx1 + 620, by1 + 388 };
        fillRoundRect(hdc, badge.left, badge.top, badge.right, badge.bottom, 8, RGB(28, 10, 0), RGB(245, 158, 11), 2);
        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(253, 230, 138));
        DrawTextA(hdc, "CATHODE-DRIVEN TONE STACK + PUSH-PULL 6L6GC / EL34 SAG", -1, &badge, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
    else if (g_activePage == 2) {
        // PAGE 2: 3-SPRING ACCUTRONICS REVERB & HARMONIC TUBE VIBRATO RACK
        fillRoundRect(hdc, bx1, by1, bx2, by2, 16, RGB(15, 23, 42), RGB(16, 185, 129), 3);

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(52, 211, 153));
        RECT subR = { bx1 + 36, by1 + 24, bx1 + 780, by1 + 46 };
        DrawTextA(hdc, "RACK UNIT 1  |  12AT7 TUBE-DRIVEN 3-SPRING TANK + BROWNFACE 6G HARMONIC TUBE VIBRATO", -1, &subR, DT_LEFT | DT_SINGLELINE);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(248, 250, 252));
        RECT titR = { bx1 + 36, by1 + 44, bx1 + 750, by1 + 82 };
        DrawTextA(hdc, "Accutronics 3-Spring Tank & Harmonic Vibrato", -1, &titR, DT_LEFT | DT_SINGLELINE);

        RECT tank = { bx1 + 32, by1 + 292, bx2 - 32, by2 - 20 };
        fillRoundRect(hdc, tank.left, tank.top, tank.right, tank.bottom, 10, RGB(7, 10, 18), RGB(16, 185, 129), 2);

        int springYs[3] = { tank.top + 28, (tank.top + tank.bottom) / 2, tank.bottom - 28 };
        for (int sIdx = 0; sIdx < 3; ++sIdx) {
            int sy = springYs[sIdx];
            HPEN spPen = CreatePen(PS_SOLID, 2, RGB(52, 211, 153));
            HGDIOBJ oldSpP = SelectObject(hdc, spPen);
            MoveToEx(hdc, tank.left + 24, sy, nullptr);
            for (int sx = tank.left + 24; sx < tank.right - 24; sx += 12) {
                float wave = std::sin(g_animPhase * 2.0f + (sx * 0.05f) + sIdx) * 8.0f;
                LineTo(hdc, sx + 6, sy + int(wave));
                LineTo(hdc, sx + 12, sy - int(wave));
            }
            SelectObject(hdc, oldSpP);
            DeleteObject(spPen);
        }
    }
    else {
        // PAGE 3: STUDIO 15/30 IPS TAPE & PING-PONG DELAY RACK UNIT
        fillRoundRect(hdc, bx1, by1, bx2, by2, 16, RGB(23, 18, 48), RGB(168, 85, 247), 3);

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(192, 132, 252));
        RECT subR = { bx1 + 36, by1 + 24, bx1 + 820, by1 + 46 };
        DrawTextA(hdc, "LIVE MIXER AUX SEND 3-4 STUDIO 2\" MAGNETIC TAPE ECHO & PING-PONG DELAY RACK", -1, &subR, DT_LEFT | DT_SINGLELINE);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(248, 250, 252));
        RECT titR = { bx1 + 36, by1 + 44, bx1 + 880, by1 + 82 };
        DrawTextA(hdc, "Studio 15/30 IPS Tape Delay  |  Dual Read-Head & Auto-Ducker", -1, &titR, DT_LEFT | DT_SINGLELINE);

        RECT deck = { bx1 + 32, by1 + 292, bx2 - 32, by2 - 20 };
        fillRoundRect(hdc, deck.left, deck.top, deck.right, deck.bottom, 10, RGB(7, 10, 18), RGB(124, 58, 237), 2);

        int reelY = (deck.top + deck.bottom) / 2;
        int reelXs[2] = { (deck.left + deck.right) / 2 - 150, (deck.left + deck.right) / 2 + 150 };
        int rr = 42;
        for (int rIdx = 0; rIdx < 2; ++rIdx) {
            int rx = reelXs[rIdx];
            fillRoundRect(hdc, rx - rr, reelY - rr, rx + rr, reelY + rr, rr * 2, RGB(30, 41, 59), RGB(192, 132, 252), 2);
            HPEN spokePen = CreatePen(PS_SOLID, 3, RGB(216, 180, 254));
            HGDIOBJ oldSpokeP = SelectObject(hdc, spokePen);
            for (int sp = 0; sp < 3; ++sp) {
                float a = g_animPhase + sp * 2.094395f;
                MoveToEx(hdc, rx, reelY, nullptr);
                LineTo(hdc, rx + int(std::cos(a) * (rr - 4)), reelY + int(std::sin(a) * (rr - 4)));
            }
            SelectObject(hdc, oldSpokeP);
            DeleteObject(spokePen);
            fillRoundRect(hdc, rx - 8, reelY - 8, rx + 8, reelY + 8, 16, RGB(248, 250, 252), RGB(124, 58, 237), 1);
        }
    }

    // Draw Knobs belonging to the Active Page
    SelectObject(hdc, fontKnob);
    for (int i = 0; i < NUM_KNOBS; ++i) {
        if (g_knobs[i].pageIdx == g_activePage) {
            drawStylizedKnob(hdc, g_knobs[i], g_activePage);
        }
    }

    // Bottom Live Meter Bar and Hardware Status
    float inPk = std::min(1.0f, g_inMeterPeak.load());
    float outL = std::min(1.0f, g_outMeterPeakL.load());
    float outR = std::min(1.0f, g_outMeterPeakR.load());

    char statusBuf[256];
    const char* chNames[3] = { "Ch 1 (Left)", "Ch 2 (Right)", "Stereo Sum" };
    std::snprintf(statusBuf, sizeof(statusBuf),
                  "IN: %s [%s]  |  OUT: %s  |  BUF: %d samples  (Click [AUDIO SETTINGS] to change)",
                  g_audioCfg.inDevName, chNames[g_audioCfg.inChannel], g_audioCfg.outDevName, g_audioCfg.bufferFrames);

    RECT footer = { 22, h - 36, 680, h - 10 };
    SetTextColor(hdc, RGB(148, 163, 184));
    DrawTextA(hdc, statusBuf, -1, &footer, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Meters: Guitar In (Green/Yellow) and Master Out L/R
    int meterX = 720;
    // IN meter label
    RECT inLblR = { meterX, h - 34, meterX + 50, h - 10 };
    SetTextColor(hdc, RGB(56, 189, 248));
    DrawTextA(hdc, "GUITAR IN", -1, &inLblR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // IN meter bar
    int inW = 100;
    fillRoundRect(hdc, meterX + 56, h - 28, meterX + 56 + inW, h - 16, 4, RGB(17, 24, 39), RGB(51, 65, 85), 1);
    if (inPk > 0.005f) {
        COLORREF c = (inPk > 0.85f) ? RGB(239, 68, 68) : ((inPk > 0.65f) ? RGB(245, 158, 11) : RGB(34, 197, 94));
        fillRoundRect(hdc, meterX + 56, h - 28, meterX + 56 + int(inW * inPk), h - 16, 4, c, c, 1);
    }

    // OUT L/R meters
    int outMeterX = meterX + 175;
    RECT outLblR = { outMeterX, h - 34, outMeterX + 60, h - 10 };
    SetTextColor(hdc, RGB(168, 85, 247));
    DrawTextA(hdc, "MASTER OUT", -1, &outLblR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    int outW = 160;
    fillRoundRect(hdc, outMeterX + 70, h - 33, outMeterX + 70 + outW, h - 23, 4, RGB(17, 24, 39), RGB(51, 65, 85), 1);
    fillRoundRect(hdc, outMeterX + 70, h - 19, outMeterX + 70 + outW, h - 9,  4, RGB(17, 24, 39), RGB(51, 65, 85), 1);
    if (outL > 0.005f) fillRoundRect(hdc, outMeterX + 70, h - 33, outMeterX + 70 + int(outW * outL), h - 23, 4, RGB(52, 211, 153), RGB(52, 211, 153), 1);
    if (outR > 0.005f) fillRoundRect(hdc, outMeterX + 70, h - 19, outMeterX + 70 + int(outW * outR), h - 9,  4, RGB(52, 211, 153), RGB(52, 211, 153), 1);

    BitBlt(hdcWin, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, oldFont);
    DeleteObject(fontTab);
    DeleteObject(fontTitle);
    DeleteObject(fontSub);
    DeleteObject(fontKnob);
    SelectObject(hdc, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdc);
}

// ============================================================================
// Dedicated Audio Hardware & Guitar Input Settings Modal Dialog
// ============================================================================
static HWND g_hInCombo = nullptr;
static HWND g_hChanCombo = nullptr;
static HWND g_hOutCombo = nullptr;
static HWND g_hBufCombo = nullptr;

static LRESULT CALLBACK audioSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            SetTimer(hwnd, 1, 33, nullptr);
            HFONT hf = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hfBold = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            // 1. Guitar Input Device
            CreateWindowA("STATIC", "1. Guitar Input Audio Interface (Recording):", WS_CHILD | WS_VISIBLE, 24, 18, 460, 20, hwnd, nullptr, nullptr, nullptr);
            g_hInCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 24, 42, 470, 200, hwnd, (HMENU)101, nullptr, nullptr);
            SendMessageA(g_hInCombo, WM_SETFONT, (WPARAM)hf, TRUE);

            for (size_t i = 0; i < g_inDevList.size(); ++i) {
                SendMessageA(g_hInCombo, CB_ADDSTRING, 0, (LPARAM)g_inDevList[i].name);
                if (g_inDevList[i].id == g_audioCfg.inDevId) {
                    SendMessageA(g_hInCombo, CB_SETCURSEL, (WPARAM)i, 0);
                }
            }
            if (SendMessageA(g_hInCombo, CB_GETCURSEL, 0, 0) == CB_ERR) {
                SendMessageA(g_hInCombo, CB_SETCURSEL, 0, 0);
            }

            // 2. Input Channel
            CreateWindowA("STATIC", "   Input Channel (Instrument Port):", WS_CHILD | WS_VISIBLE, 24, 82, 460, 20, hwnd, nullptr, nullptr, nullptr);
            g_hChanCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 24, 104, 470, 150, hwnd, (HMENU)102, nullptr, nullptr);
            SendMessageA(g_hChanCombo, WM_SETFONT, (WPARAM)hf, TRUE);
            SendMessageA(g_hChanCombo, CB_ADDSTRING, 0, (LPARAM)"Channel 1 (Left / Instrument 1 - Audient, Focusrite)");
            SendMessageA(g_hChanCombo, CB_ADDSTRING, 0, (LPARAM)"Channel 2 (Right / Instrument 2)");
            SendMessageA(g_hChanCombo, CB_ADDSTRING, 0, (LPARAM)"Stereo Sum (Combine Ch 1 + Ch 2)");
            SendMessageA(g_hChanCombo, CB_SETCURSEL, (WPARAM)g_audioCfg.inChannel, 0);

            // 3. Playback Output Device
            CreateWindowA("STATIC", "2. Playback Output Device (Monitors / Headphones):", WS_CHILD | WS_VISIBLE, 24, 184, 460, 20, hwnd, nullptr, nullptr, nullptr);
            g_hOutCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 24, 208, 470, 200, hwnd, (HMENU)103, nullptr, nullptr);
            SendMessageA(g_hOutCombo, WM_SETFONT, (WPARAM)hf, TRUE);

            for (size_t i = 0; i < g_outDevList.size(); ++i) {
                SendMessageA(g_hOutCombo, CB_ADDSTRING, 0, (LPARAM)g_outDevList[i].name);
                if (g_outDevList[i].id == g_audioCfg.outDevId) {
                    SendMessageA(g_hOutCombo, CB_SETCURSEL, (WPARAM)i, 0);
                }
            }
            if (SendMessageA(g_hOutCombo, CB_GETCURSEL, 0, 0) == CB_ERR) {
                SendMessageA(g_hOutCombo, CB_SETCURSEL, 0, 0);
            }

            // 4. Buffer Size
            CreateWindowA("STATIC", "3. Audio Buffer Latency (Sample Rate: 48,000 Hz):", WS_CHILD | WS_VISIBLE, 24, 248, 460, 20, hwnd, nullptr, nullptr, nullptr);
            g_hBufCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 24, 272, 470, 150, hwnd, (HMENU)104, nullptr, nullptr);
            SendMessageA(g_hBufCombo, WM_SETFONT, (WPARAM)hf, TRUE);
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"128 frames (~2.7 ms) - Ultra Low Latency (Fast Guitar Response)");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"256 frames (~5.3 ms) - Balanced Studio (Recommended)");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"512 frames (~10.7 ms) - High Stability Safe Mode");
            int selBuf = (g_audioCfg.bufferFrames == 128) ? 0 : ((g_audioCfg.bufferFrames == 512) ? 2 : 1);
            SendMessageA(g_hBufCombo, CB_SETCURSEL, (WPARAM)selBuf, 0);

            // Buttons
            HWND hBtnOk = CreateWindowA("BUTTON", "APPLY & SAVE SETTINGS", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 120, 336, 180, 36, hwnd, (HMENU)IDOK, nullptr, nullptr);
            HWND hBtnCancel = CreateWindowA("BUTTON", "CANCEL", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 320, 336, 110, 36, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageA(hBtnOk, WM_SETFONT, (WPARAM)hfBold, TRUE);
            SendMessageA(hBtnCancel, WM_SETFONT, (WPARAM)hf, TRUE);

            return 0;
        }
        case WM_TIMER:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Paint Live Input Signal Meter Area
            RECT meterArea = { 24, 142, 494, 172 };
            fillRoundRect(hdc, meterArea.left, meterArea.top, meterArea.right, meterArea.bottom, 6, RGB(15, 23, 42), RGB(71, 85, 105), 1);

            float pk = std::min(1.0f, g_inMeterPeak.load());
            int barW = meterArea.right - meterArea.left - 120;
            fillRoundRect(hdc, meterArea.left + 8, meterArea.top + 6, meterArea.left + 8 + barW, meterArea.bottom - 6, 4, RGB(2, 6, 23), RGB(30, 41, 59), 1);

            if (pk > 0.005f) {
                COLORREF c = (pk > 0.85f) ? RGB(239, 68, 68) : ((pk > 0.65f) ? RGB(245, 158, 11) : RGB(34, 197, 94));
                fillRoundRect(hdc, meterArea.left + 8, meterArea.top + 6, meterArea.left + 8 + int(barW * pk), meterArea.bottom - 6, 4, c, c, 1);
            }

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, (pk > 0.05f) ? RGB(34, 197, 94) : RGB(148, 163, 184));
            char txt[64];
            if (pk > 0.005f) {
                float db = 20.0f * std::log10(pk);
                std::snprintf(txt, sizeof(txt), "SIGNAL: %.1f dB", db);
            } else {
                std::snprintf(txt, sizeof(txt), "NO GUITAR SIGNAL");
            }
            RECT txtR = { meterArea.right - 105, meterArea.top, meterArea.right - 8, meterArea.bottom };
            DrawTextA(hdc, txt, -1, &txtR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDOK) {
                int inSel = (int)SendMessageA(g_hInCombo, CB_GETCURSEL, 0, 0);
                if (inSel >= 0 && inSel < (int)g_inDevList.size()) {
                    g_audioCfg.inDevId = g_inDevList[inSel].id;
                    std::strncpy(g_audioCfg.inDevName, g_inDevList[inSel].name, sizeof(g_audioCfg.inDevName) - 1);
                }
                int chSel = (int)SendMessageA(g_hChanCombo, CB_GETCURSEL, 0, 0);
                if (chSel >= 0 && chSel <= 2) {
                    g_audioCfg.inChannel = chSel;
                }
                int outSel = (int)SendMessageA(g_hOutCombo, CB_GETCURSEL, 0, 0);
                if (outSel >= 0 && outSel < (int)g_outDevList.size()) {
                    g_audioCfg.outDevId = g_outDevList[outSel].id;
                    std::strncpy(g_audioCfg.outDevName, g_outDevList[outSel].name, sizeof(g_audioCfg.outDevName) - 1);
                }
                int bufSel = (int)SendMessageA(g_hBufCombo, CB_GETCURSEL, 0, 0);
                g_audioCfg.bufferFrames = (bufSel == 0) ? 128 : ((bufSel == 2) ? 512 : 256);

                saveAudioConfig();
                g_audioConfigChanged.store(true);
                DestroyWindow(hwnd);
                return 0;
            } else if (id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void openAudioSettingsDialog(HWND hwndParent) {
    refreshAudioDeviceLists();

    WNDCLASSEXA dc = {};
    dc.cbSize = sizeof(dc);
    dc.style = CS_HREDRAW | CS_VREDRAW;
    dc.lpfnWndProc = audioSettingsDlgProc;
    dc.hInstance = GetModuleHandle(nullptr);
    dc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    dc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    dc.lpszClassName = "CyberAudioSettingsDlgWnd";
    RegisterClassExA(&dc);

    RECT pr;
    GetWindowRect(hwndParent, &pr);
    int cx = (pr.left + pr.right) / 2 - 270;
    int cy = (pr.top + pr.bottom) / 2 - 215;

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        dc.lpszClassName,
        "Audio Hardware & Guitar Interface Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, 540, 430,
        hwndParent, nullptr, dc.hInstance, nullptr
    );

    EnableWindow(hwndParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    EnableWindow(hwndParent, TRUE);
    SetForegroundWindow(hwndParent);
    InvalidateRect(hwndParent, nullptr, TRUE);
}

static int   g_dragKnob = -1;
static int   g_dragStartY = 0;
static float g_dragStartVal = 0.0f;

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 33, nullptr);
            return 0;
        case WM_TIMER:
            g_animPhase += 0.10f;
            if (g_animPhase > 6.2831853f) g_animPhase -= 6.2831853f;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            for (int t = 0; t < 4; ++t) {
                if (PtInRect(&g_tabRects[t], pt)) {
                    g_activePage = t;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            if (PtInRect(&g_riffBtnRect, pt)) {
                g_testRiffActive.store(!g_testRiffActive.load());
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtInRect(&g_audioSettingsBtnRect, pt)) {
                openAudioSettingsDialog(hwnd);
                return 0;
            }
            for (int i = 0; i < NUM_KNOBS; ++i) {
                if (g_knobs[i].pageIdx != -1 && g_knobs[i].pageIdx != g_activePage) continue;
                if (PtInRect(&g_knobs[i].rect, pt)) {
                    if (g_knobs[i].isStepped) {
                        int next = (int(std::round(g_knobs[i].curVal)) + 1) % g_knobs[i].steps;
                        g_knobs[i].curVal = float(next);
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        g_dragKnob = i;
                        g_dragStartY = pt.y;
                        g_dragStartVal = g_knobs[i].curVal;
                        SetCapture(hwnd);
                    }
                    return 0;
                }
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            for (int i = 0; i < NUM_KNOBS; ++i) {
                if (g_knobs[i].pageIdx != -1 && g_knobs[i].pageIdx != g_activePage) continue;
                if (PtInRect(&g_knobs[i].rect, pt)) {
                    g_knobs[i].curVal = g_knobs[i].defVal;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_dragKnob >= 0) {
                int dy = g_dragStartY - GET_Y_LPARAM(lp);
                KnobCtrl& k = g_knobs[g_dragKnob];
                float range = k.maxVal - k.minVal;
                k.curVal = std::max(k.minVal, std::min(k.maxVal, g_dragStartVal + (float(dy) / 140.0f) * range));
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (g_dragKnob >= 0) {
                g_dragKnob = -1;
                ReleaseCapture();
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            renderGui(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            g_running.store(false);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    g_engine.reset();
    layoutPageKnobs();
    refreshAudioDeviceLists();
    loadAudioConfig();

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "CyberUltimateHybrid4PageWnd";
    RegisterClassExA(&wc);

    RECT wr = { 0, 0, 1200, 560 };
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowExA(
        0, wc.lpszClassName,
        "Tubular Amp — 4-Page Analog Vacuum Tube & Studio Rack Suite (Windows x64)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInst, nullptr
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    HANDLE hAudioThread = CreateThread(nullptr, 0, audioThreadProc, nullptr, 0, nullptr);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    g_running.store(false);
    if (hAudioThread) {
        WaitForSingleObject(hAudioThread, 1000);
        CloseHandle(hAudioThread);
    }
    return 0;
}
