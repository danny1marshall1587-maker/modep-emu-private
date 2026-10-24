// ============================================================================
// Euphoria Amp — Native Windows 64-Bit Pure Fluid Dynamics Amp & Studio Suite
// ============================================================================
// Features 4 Separate Pure Fluid Dynamic Hardware Views:
//   Page 0: Euphoria '65 Clean Head (Obsidian Jade + Gold Trim + Mint Jewel Lamp)
//   Page 1: Euphoria '59 / '68 Crunch Head (Burnished Copper + Rose Brass Faceplate + Amber Lamp)
//   Page 2: Euphoria 3-Spring Fluid Tank & Vibrato Rack (Deep Cyan Marine + Liquid Wave Springs)
//   Page 3: Euphoria Studio 15/30 IPS Tape Delay Rack (Regal Royal Indigo + 24k Gold Tape Reels)
//   Audio Settings: Full Audio Hardware & Guitar Interface Configuration Dialog
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <initguid.h>
#include <windows.h>
#include <windowsx.h>
#include <propidl.h>
#include <gdiplus.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmsystem.h>
#include <ksmedia.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <algorithm>
#include <mutex>

#define CYBER_ENABLE_STUDIO_DELAY 1
#include "../../euphoria-amp.lv2/src/EuphoriaAmpEngine.hpp"
#include "iasiodrv.h"

using namespace EuphoriaDSP;

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")

static constexpr int SAMPLE_RATE  = 48000;
static HWND g_hwndMain = nullptr;

struct AudioDeviceConfig {
    int driverMode = 0;      // 0 = ASIO Direct Hardware, 1 = WASAPI Exclusive, 2 = WASAPI Low-Latency Shared
    int asioDriverIdx = 0;   // Index into g_asioDriverList
    int inDevId = -1;        // -1 = Default Windows Device, 0..N = device index
    int outDevId = -1;       // -1 = Default Windows Device, 0..N = device index
    int inChannel = 0;       // 0 = Ch 1 (Left / Inst 1), 1 = Ch 2 (Right / Inst 2), 2 = Stereo Sum
    int bufferFrames = 32;   // 16 (0.33ms), 24 (0.5ms), 32 (0.67ms), 48 (1.0ms), 64 (1.33ms), 96, 128, 256, 512
    char asioDriverName[128] = "Audient USB Audio ASIO Driver";
    char inDevName[128] = "Default Windows Recording Device";
    char outDevName[128] = "Default Windows Playback Device";
};

struct AsioDriverItem {
    char name[128];
    char clsidStr[64];
    CLSID clsid;
};

struct DeviceItem {
    int id; // -1 for default, 0..N for device index
    char name[128];
    wchar_t devIdStr[256];
};

static AudioDeviceConfig g_audioCfg;
static std::atomic<bool> g_audioConfigChanged{false};
static std::atomic<bool> g_asioActive{false};
static std::atomic<bool> g_asioOpenPanelReq{false};
static std::atomic<float> g_inMeterPeak{0.0f};
static std::atomic<float> g_outMeterPeakL{0.0f};
static std::atomic<float> g_outMeterPeakR{0.0f};
static std::atomic<bool>  g_running{true};
static std::atomic<bool>  g_testRiffActive{false};

static std::vector<AsioDriverItem> g_asioDriverList;
static std::vector<DeviceItem> g_inDevList;
static std::vector<DeviceItem> g_outDevList;

static void refreshAudioDeviceLists() {
    CoInitialize(nullptr);
    g_asioDriverList.clear();
    g_inDevList.clear();
    g_outDevList.clear();

    // 0. Enumerate Installed ASIO Drivers from Windows Registry (HKLM\SOFTWARE\ASIO)
    HKEY hAsioRoot = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASIO", 0, KEY_READ, &hAsioRoot) == ERROR_SUCCESS && hAsioRoot) {
        char subKeyName[128];
        DWORD idx = 0;
        while (RegEnumKeyA(hAsioRoot, idx++, subKeyName, sizeof(subKeyName)) == ERROR_SUCCESS) {
            HKEY hDrvKey = nullptr;
            if (RegOpenKeyExA(hAsioRoot, subKeyName, 0, KEY_READ, &hDrvKey) == ERROR_SUCCESS && hDrvKey) {
                char clsidBuf[64] = "";
                char descBuf[128] = "";
                DWORD cbClsid = sizeof(clsidBuf);
                DWORD cbDesc = sizeof(descBuf);
                DWORD type = 0;
                RegQueryValueExA(hDrvKey, "CLSID", nullptr, &type, (LPBYTE)clsidBuf, &cbClsid);
                RegQueryValueExA(hDrvKey, "Description", nullptr, &type, (LPBYTE)descBuf, &cbDesc);
                RegCloseKey(hDrvKey);

                if (clsidBuf[0] != '\0') {
                    wchar_t wClsid[64] = L"";
                    MultiByteToWideChar(CP_UTF8, 0, clsidBuf, -1, wClsid, 64);
                    CLSID clsid = {};
                    if (SUCCEEDED(CLSIDFromString(wClsid, &clsid))) {
                        AsioDriverItem item = {};
                        std::strncpy(item.name, (descBuf[0] != '\0') ? descBuf : subKeyName, sizeof(item.name) - 1);
                        std::strncpy(item.clsidStr, clsidBuf, sizeof(item.clsidStr) - 1);
                        item.clsid = clsid;
                        g_asioDriverList.push_back(item);
                    }
                }
            }
        }
        RegCloseKey(hAsioRoot);
    }

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
    return std::string(exePath) + "euphoria_amp_audio.ini";
}

static void saveAudioConfig() {
    std::string path = getIniPath();
    WritePrivateProfileStringA("Audio", "AsioDriverName", g_audioCfg.asioDriverName, path.c_str());
    WritePrivateProfileStringA("Audio", "InDevName", g_audioCfg.inDevName, path.c_str());
    WritePrivateProfileStringA("Audio", "OutDevName", g_audioCfg.outDevName, path.c_str());
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", g_audioCfg.driverMode);
    WritePrivateProfileStringA("Audio", "DriverMode", buf, path.c_str());
    std::snprintf(buf, sizeof(buf), "%d", g_audioCfg.inChannel);
    WritePrivateProfileStringA("Audio", "InChannel", buf, path.c_str());
    std::snprintf(buf, sizeof(buf), "%d", g_audioCfg.bufferFrames);
    WritePrivateProfileStringA("Audio", "BufferFrames", buf, path.c_str());
}

static void loadAudioConfig() {
    refreshAudioDeviceLists();
    std::string path = getIniPath();
    char asioName[128] = "", inName[128] = "", outName[128] = "";
    GetPrivateProfileStringA("Audio", "AsioDriverName", "", asioName, sizeof(asioName), path.c_str());
    GetPrivateProfileStringA("Audio", "InDevName", "", inName, sizeof(inName), path.c_str());
    GetPrivateProfileStringA("Audio", "OutDevName", "", outName, sizeof(outName), path.c_str());
    g_audioCfg.driverMode = GetPrivateProfileIntA("Audio", "DriverMode", g_asioDriverList.empty() ? 2 : 0, path.c_str());
    if (g_audioCfg.driverMode < 0 || g_audioCfg.driverMode > 2) g_audioCfg.driverMode = 0;
    g_audioCfg.inChannel = GetPrivateProfileIntA("Audio", "InChannel", 0, path.c_str());
    g_audioCfg.bufferFrames = GetPrivateProfileIntA("Audio", "BufferFrames", 32, path.c_str());
    if (g_audioCfg.bufferFrames != 16 && g_audioCfg.bufferFrames != 24 &&
        g_audioCfg.bufferFrames != 32 && g_audioCfg.bufferFrames != 48 &&
        g_audioCfg.bufferFrames != 64 && g_audioCfg.bufferFrames != 96 &&
        g_audioCfg.bufferFrames != 128 && g_audioCfg.bufferFrames != 256 && g_audioCfg.bufferFrames != 512) {
        g_audioCfg.bufferFrames = 32;
    }

    auto isProAudioInterface = [](const char* name) -> bool {
        return std::strstr(name, "EVO") != nullptr ||
               std::strstr(name, "Audient") != nullptr ||
               std::strstr(name, "Line 6") != nullptr ||
               std::strstr(name, "Focusrite") != nullptr ||
               std::strstr(name, "Scarlett") != nullptr ||
               std::strstr(name, "ASIO4ALL") != nullptr ||
               std::strstr(name, "USB Audio") != nullptr ||
               std::strstr(name, "CODEC") != nullptr;
    };

    if (!g_asioDriverList.empty()) {
        bool foundAsio = false;
        if (std::strlen(asioName) > 0) {
            for (size_t i = 0; i < g_asioDriverList.size(); ++i) {
                if (std::strstr(g_asioDriverList[i].name, asioName) != nullptr ||
                    std::strcmp(g_asioDriverList[i].name, asioName) == 0) {
                    g_audioCfg.asioDriverIdx = (int)i;
                    std::strncpy(g_audioCfg.asioDriverName, g_asioDriverList[i].name, sizeof(g_audioCfg.asioDriverName) - 1);
                    foundAsio = true;
                    break;
                }
            }
        }
        if (!foundAsio) {
            // Prefer Audient / Line 6 / Focusrite first, then ASIO4ALL, then index 0
            g_audioCfg.asioDriverIdx = 0;
            std::strncpy(g_audioCfg.asioDriverName, g_asioDriverList[0].name, sizeof(g_audioCfg.asioDriverName) - 1);
            for (size_t i = 0; i < g_asioDriverList.size(); ++i) {
                if (std::strstr(g_asioDriverList[i].name, "Audient") != nullptr ||
                    std::strstr(g_asioDriverList[i].name, "EVO") != nullptr ||
                    std::strstr(g_asioDriverList[i].name, "Line 6") != nullptr ||
                    std::strstr(g_asioDriverList[i].name, "Focusrite") != nullptr) {
                    g_audioCfg.asioDriverIdx = (int)i;
                    std::strncpy(g_audioCfg.asioDriverName, g_asioDriverList[i].name, sizeof(g_audioCfg.asioDriverName) - 1);
                    break;
                }
            }
        }
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
        // Auto-detect Audient EVO / Focusrite / Line 6 / USB Audio CODEC interface if connected
        for (const auto& dev : g_inDevList) {
            if (isProAudioInterface(dev.name)) {
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
        // Auto-detect Audient EVO / Focusrite / Line 6 / USB Audio CODEC interface for playback if connected
        for (const auto& dev : g_outDevList) {
            if (isProAudioInterface(dev.name)) {
                g_audioCfg.outDevId = dev.id;
                std::strncpy(g_audioCfg.outDevName, dev.name, sizeof(g_audioCfg.outDevName) - 1);
                break;
            }
        }
    }
}

struct KnobCtrl {
    const char* label;
    int pageIdx; // -1 = Global Header, 0 = Clean, 1 = Crunch, 2 = Fluid Space, 3 = Reverb/Vib, 4 = Tape Delay
    float minVal;
    float maxVal;
    float defVal;
    float curVal;
    bool isStepped;
    int steps;
    const char* stepLabels[24];
    RECT rect;
};

struct GdiPlus3DAssets {
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::Image* imgWoodFloor = nullptr;
    Gdiplus::Image* imgChassisWide = nullptr;
    Gdiplus::Image* imgChassisCleanBlue = nullptr;
    Gdiplus::Image* imgChassisCrunchBrown = nullptr;
    Gdiplus::Image* imgChassisLeadRed = nullptr;
    Gdiplus::Image* imgRackSpaceTopaz = nullptr;
    Gdiplus::Image* imgRackReverbPink = nullptr;
    Gdiplus::Image* imgRackTremOrange = nullptr;
    Gdiplus::Image* imgRackDelayGreen = nullptr;
    Gdiplus::Image* imgChassisCompact = nullptr;
    Gdiplus::Image* imgGuitar = nullptr;
    Gdiplus::Image* imgFuzz = nullptr;
    Gdiplus::Image* imgKlon = nullptr;
    Gdiplus::Image* imgTs9 = nullptr;
    Gdiplus::Image* imgBlues = nullptr;
    Gdiplus::Image* imgRat = nullptr;
    Gdiplus::Image* imgKnobStrip = nullptr;
    bool initialized = false;

    void init() {
        if (initialized) return;
        Gdiplus::GdiplusStartupInput gsi;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gsi, nullptr);

        auto load = [](const wchar_t* relName) -> Gdiplus::Image* {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            wchar_t* lastSlash = wcsrchr(exePath, L'\\');
            if (lastSlash) *(lastSlash + 1) = L'\0';

            const wchar_t* candidates[] = {
                L"%sJUCE_Assets\\%s",
                L"%sassets\\%s",
                L"%s..\\JUCE_Assets\\%s",
                L"%s..\\assets\\%s",
                L"%splugins\\euphoria-amp-vst\\assets\\%s",
                L"%s..\\plugins\\euphoria-amp-vst\\assets\\%s",
                L"JUCE_Assets\\%s",
                L"assets\\%s",
                L"plugins\\euphoria-amp-vst\\assets\\%s",
                L"..\\JUCE_Assets\\%s",
                L"..\\assets\\%s",
                L"C:\\Users\\danny\\Documents\\modep emu\\JUCE_Assets\\%s",
                L"C:\\Users\\danny\\Documents\\modep emu\\standalone\\assets\\%s",
                L"C:\\Users\\danny\\Documents\\modep emu\\standalone\\JUCE_Assets\\%s",
                L"C:\\Users\\danny\\Documents\\modep emu\\plugins\\euphoria-amp-vst\\assets\\%s"
            };

            for (const auto* fmt : candidates) {
                wchar_t fullPath[MAX_PATH];
                if (wcschr(fmt, L'%') != wcsrchr(fmt, L'%')) {
                    swprintf_s(fullPath, fmt, exePath, relName);
                } else {
                    swprintf_s(fullPath, fmt, relName);
                }
                if (GetFileAttributesW(fullPath) != INVALID_FILE_ATTRIBUTES) {
                    Gdiplus::Image* img = Gdiplus::Image::FromFile(fullPath);
                    if (img && img->GetLastStatus() == Gdiplus::Ok) return img;
                    delete img;
                }
            }

            return nullptr;
        };

        imgWoodFloor          = load(L"wood_floor_tile.png");
        imgChassisWide         = load(L"chassis_wide.png");
        imgChassisCleanBlue    = load(L"chassis_clean_blue.png");
        imgChassisCrunchBrown  = load(L"chassis_crunch_brown.png");
        imgChassisLeadRed      = load(L"chassis_lead_red.png");
        imgRackSpaceTopaz      = load(L"rack_space_topaz.png");
        imgRackReverbPink      = load(L"rack_reverb_pink.png");
        imgRackTremOrange      = load(L"rack_trem_orange.png");
        imgRackDelayGreen      = load(L"rack_delay_green.png");
        imgChassisCompact      = load(L"chassis_compact.png");
        imgGuitar              = load(L"guitar_luxury.png");
        imgFuzz                = load(L"pedal_fuzz_round.png");
        imgKlon                = load(L"pedal_klon_gold.png");
        imgTs9                 = load(L"pedal_ts9_green.png");
        imgBlues               = load(L"pedal_blues_blue.png");
        imgRat                 = load(L"pedal_rat_black.png");
        imgKnobStrip           = load(L"knob_filmstrip.png");
        initialized = true;
    }

    void shutdown() {
        if (initialized) {
            delete imgWoodFloor;
            delete imgChassisWide;
            delete imgChassisCleanBlue;
            delete imgChassisCrunchBrown;
            delete imgChassisLeadRed;
            delete imgRackSpaceTopaz;
            delete imgRackReverbPink;
            delete imgRackTremOrange;
            delete imgRackDelayGreen;
            delete imgChassisCompact;
            delete imgGuitar;
            delete imgFuzz;
            delete imgKlon;
            delete imgTs9;
            delete imgBlues;
            delete imgRat;
            delete imgKnobStrip;
            Gdiplus::GdiplusShutdown(gdiplusToken);
            initialized = false;
        }
    }
};
static GdiPlus3DAssets g_assets3D;

static EuphoriaAmpEngine g_engine{double(SAMPLE_RATE)};
static int   g_activePage = 0;
static float g_animPhase  = 0.0f;

static std::atomic<float> g_dspCpuPercent{1.8f};
static std::atomic<float> g_dspTimeUs{95.0f};
static std::atomic<int>   g_dspInternalRate{192000};

static ULONGLONG g_learnNoiseStartTime = 0;
static ULONGLONG g_learnNoiseDoneTime = 0;
static bool      g_wasLearningNoise = false;

// ============================================================================
// Precision Virtual Strobe Tuner Engine (Peterson / Sonic Research Sub-Cent)
// ============================================================================
struct StrobeTunerEngine {
    static constexpr int BUF_SIZE = 4096;
    float m_filtBuf[BUF_SIZE] = {};
    float m_rawBuf[BUF_SIZE]  = {};
    int   m_writeIdx = 0;
    float m_sampleRate = 48000.0f;

    // Continuous 2-pole pre-filtering state (DC-block 30Hz + Butterworth LPF 700Hz)
    float m_hp_x = 0.0f, m_hp_y = 0.0f;
    float m_lp_y1 = 0.0f, m_lp_y2 = 0.0f;

    // Temporal stabilization state
    float m_displayCents = 0.0f;
    float m_displayFreq = 0.0f;
    int   m_activeMidi = -1;
    int   m_candidateMidi = -1;
    int   m_candidateConfirm = 0;
    int   m_holdFrames = 0;
    int   m_samplesSinceLastRun = 0;

    std::atomic<float> detectedFreq{0.0f};
    std::atomic<float> detectedPitch{0.0f};
    std::atomic<float> centsDeviation{0.0f};
    std::atomic<float> cents{0.0f};
    std::atomic<float> targetFreq{0.0f};
    std::atomic<float> targetPitch{0.0f};
    std::atomic<float> refFreq{440.0f};
    std::atomic<float> confidence{0.0f};
    std::atomic<float> inRms{0.0f};
    std::atomic<int>   noteIdx{-1}; // 0=C, 1=C#, 2=D, ... 11=B
    std::atomic<int>   octave{0};
    std::atomic<int>   midiNote{-1};
    std::atomic<bool>  hasSignal{false};
    std::atomic<bool>  signalActive{false};
    std::atomic<float> strobeSpeed{0.0f};
    std::atomic<int>   closestStringIdx{-1};
    std::atomic<bool>  highPrecisionMode{true};

    void getNoteName(char* out) const {
        static const char* s_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int idx = noteIdx.load();
        if (idx >= 0 && idx < 12) {
            strcpy(out, s_names[idx]);
        } else {
            strcpy(out, "--");
        }
    }

    void reset(float sRate = 48000.0f) {
        m_sampleRate = (sRate > 20000.0f) ? sRate : 48000.0f;
        std::fill(m_filtBuf, m_filtBuf + BUF_SIZE, 0.0f);
        std::fill(m_rawBuf, m_rawBuf + BUF_SIZE, 0.0f);
        m_writeIdx = 0;
        m_hp_x = m_hp_y = 0.0f;
        m_lp_y1 = m_lp_y2 = 0.0f;
        m_displayCents = 0.0f;
        m_displayFreq = 0.0f;
        m_activeMidi = -1;
        m_candidateMidi = -1;
        m_candidateConfirm = 0;
        m_holdFrames = 0;
        m_samplesSinceLastRun = 0;

        detectedFreq.store(0.0f);
        detectedPitch.store(0.0f);
        centsDeviation.store(0.0f);
        cents.store(0.0f);
        targetFreq.store(0.0f);
        targetPitch.store(0.0f);
        confidence.store(0.0f);
        inRms.store(0.0f);
        noteIdx.store(-1);
        octave.store(0);
        midiNote.store(-1);
        hasSignal.store(false);
        signalActive.store(false);
        strobeSpeed.store(0.0f);
        closestStringIdx.store(-1);
    }

    void setSampleRate(float sRate) {
        if (sRate > 20000.0f && std::abs(m_sampleRate - sRate) > 100.0f) {
            reset(sRate);
        }
    }

    float getSampleRate() const { return m_sampleRate; }

    void writeSamples(const float* in, int numSamples) {
        // Continuous per-sample DC-blocking HPF (30 Hz) + 2-pole Bessel/Butterworth LPF (700 Hz)
        // High harmonic treble/pick attack is smoothed continuously without filter settling transients
        float hp_a = 1.0f / (1.0f + 2.0f * 3.14159265f * 30.0f / m_sampleRate);
        float lp_a = (2.0f * 3.14159265f * 700.0f) / (m_sampleRate + 2.0f * 3.14159265f * 700.0f);

        for (int i = 0; i < numSamples; ++i) {
            float s = in[i];
            float hp = hp_a * (m_hp_y + s - m_hp_x);
            m_hp_x = s;
            m_hp_y = hp;

            m_lp_y1 += lp_a * (hp - m_lp_y1);
            m_lp_y2 += lp_a * (m_lp_y1 - m_lp_y2);

            m_rawBuf[m_writeIdx] = s;
            m_filtBuf[m_writeIdx] = m_lp_y2;

            if (++m_writeIdx >= BUF_SIZE) m_writeIdx = 0;
        }
        m_samplesSinceLastRun += numSamples;
    }

    void process(float refA4 = 440.0f) {
        // Decimate YIN pitch detector to once every 1024 samples (~47 Hz at 48kHz)
        // Eliminates >95% of tuner CPU overhead on small low-latency ToneDirect buffers!
        if (m_samplesSinceLastRun < 1024) return;
        m_samplesSinceLastRun = 0;

        float effectiveRef = (refFreq.load() > 400.0f) ? refFreq.load() : refA4;
        constexpr int N = 2048;
        float x[N];
        int rIdx = (m_writeIdx - N + BUF_SIZE) % BUF_SIZE;

        float sumSq = 0.0f;
        for (int i = 0; i < N; ++i) {
            int idx = (rIdx + i) % BUF_SIZE;
            x[i] = m_filtBuf[idx];
            float raw = m_rawBuf[idx];
            sumSq += raw * raw;
        }

        float rms = std::sqrt(sumSq / float(N));
        inRms.store(rms);

        if (rms < 0.0025f) {
            if (m_holdFrames > 0) {
                m_holdFrames--;
                strobeSpeed.store(strobeSpeed.load() * 0.90f);
                return;
            }
            hasSignal.store(false);
            signalActive.store(false);
            strobeSpeed.store(0.0f);
            confidence.store(0.0f);
            return;
        }

        m_holdFrames = 18;
        hasSignal.store(true);
        signalActive.store(true);

        int minLag = int(m_sampleRate / 1050.0f); // High C6 ~ 1046 Hz
        int maxLag = int(m_sampleRate / 30.0f);   // Low B0 ~ 30.87 Hz
        if (maxLag > N / 2) maxLag = N / 2;

        float d[(N / 2) + 2] = {};
        d[0] = 1.0f;
        float runningSum = 0.0f;

        for (int tau = 1; tau <= maxLag; ++tau) {
            float sum = 0.0f;
            for (int j = 0; j < N - tau; j += 2) {
                float delta = x[j] - x[j + tau];
                sum += delta * delta;
            }
            runningSum += sum;
            d[tau] = (runningSum > 1e-9f) ? (sum * float(tau) / runningSum) : 1.0f;
        }

        constexpr float YIN_THRESH = 0.15f;
        int bestTau = -1;
        float minValFound = 1.0f;
        for (int tau = minLag; tau <= maxLag - 1; ++tau) {
            if (d[tau] < YIN_THRESH) {
                while (tau + 1 <= maxLag && d[tau + 1] < d[tau]) tau++;
                bestTau = tau;
                minValFound = d[tau];
                break;
            }
        }

        if (bestTau < 0) {
            float minVal = 1.0f;
            for (int tau = minLag; tau <= maxLag; ++tau) {
                if (d[tau] < minVal) {
                    minVal = d[tau];
                    bestTau = tau;
                }
            }
            minValFound = minVal;
            if (minVal > 0.40f) {
                confidence.store(0.0f);
                return;
            }
        }
        // Subharmonic check: ensure low E2 / A2 fundamental octave is not swallowed by strong 2nd harmonic
        int subTau = bestTau * 2;
        if (subTau <= maxLag) {
            int minK = std::max(minLag, subTau - 4);
            int maxK = std::min(maxLag, subTau + 4);
            int subBest = subTau;
            float subMin = d[subTau];
            for (int k = minK; k <= maxK; ++k) {
                if (d[k] < subMin) {
                    subMin = d[k];
                    subBest = k;
                }
            }
            if (subMin < 0.32f && subMin < d[bestTau] * 1.35f) {
                bestTau = subBest;
                minValFound = subMin;
            }
        }

        confidence.store(std::max(0.0f, std::min(1.0f, 1.0f - minValFound)));

        float s0 = (bestTau > 0) ? d[bestTau - 1] : d[bestTau];
        float s1 = d[bestTau];
        float s2 = (bestTau < maxLag) ? d[bestTau + 1] : d[bestTau];
        float curvature = s0 - 2.0f * s1 + s2;
        float deltaTau = 0.0f;
        if (curvature > 1e-6f) {
            deltaTau = 0.5f * (s0 - s2) / curvature;
            deltaTau = std::clamp(deltaTau, -0.5f, 0.5f);
        }
        float trueTau = float(bestTau) + deltaTau;

        float rawFreq = m_sampleRate / trueTau;
        if (rawFreq < 30.0f || rawFreq > 1200.0f) return;

        // Chromatic Semitone Calculation
        float semitones = 12.0f * std::log2(rawFreq / effectiveRef);
        float exactMidi = semitones + 69.0f;
        int rawMidi = int(std::round(exactMidi));

        // Note decision stabilization (2-frame candidate debounce, no sticky deadband):
        if (m_activeMidi < 0) {
            m_activeMidi = rawMidi;
            m_displayFreq = rawFreq;
        } else if (rawMidi != m_activeMidi) {
            if (rawMidi == m_candidateMidi) {
                if (++m_candidateConfirm >= 2) {
                    m_activeMidi = rawMidi;
                    m_candidateConfirm = 0;
                    m_displayFreq = rawFreq; // Immediate responsive snap to new note
                }
            } else {
                m_candidateMidi = rawMidi;
                m_candidateConfirm = 1;
            }
        } else {
            m_candidateConfirm = 0;
        }

        // Smooth frequency within the active note
        m_displayFreq = m_displayFreq * 0.40f + rawFreq * 0.60f;
        detectedFreq.store(m_displayFreq);
        detectedPitch.store(m_displayFreq);

        float tFreq = effectiveRef * std::pow(2.0f, float(m_activeMidi - 69) / 12.0f);
        targetFreq.store(tFreq);
        targetPitch.store(tFreq);

        float rawCents = 1200.0f * std::log2(m_displayFreq / tFreq);
        // Stabilized, responsive cents smoothing
        m_displayCents = m_displayCents * 0.35f + rawCents * 0.65f;
        centsDeviation.store(m_displayCents);
        cents.store(m_displayCents);

        int nIdx = (m_activeMidi % 12 + 12) % 12;
        noteIdx.store(nIdx);
        octave.store((m_activeMidi / 12) - 1);
        midiNote.store(m_activeMidi);

        float spd = 0.0f;
        if (std::abs(m_displayCents) > 0.15f) {
            float mult = highPrecisionMode.load() ? 0.045f : 0.025f;
            spd = std::clamp(m_displayCents * mult, -3.5f, 3.5f);
        }
        strobeSpeed.store(spd);

        // Chromatic closest string guide
        static const float s_guitarStringHz[6] = { 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f };
        int bestStr = -1;
        float bestDist = 1e9f;
        for (int s = 0; s < 6; ++s) {
            float dist = std::abs(1200.0f * std::log2(m_displayFreq / s_guitarStringHz[s]));
            if (dist < bestDist && dist < 150.0f) {
                bestDist = dist;
                bestStr = s;
            }
        }
        closestStringIdx.store(bestStr);
    }
};

static StrobeTunerEngine g_tuner;
static std::atomic<bool> g_tunerMute{false};
static float             g_strobeWheelPhase = 0.0f;
static RECT              g_btnTunerMuteRect  = { 1022, 140, 1230, 235 };
static RECT              g_btnTunerRefRect   = { 1022, 245, 1230, 325 };
static RECT              g_btnTunerSpeedRect = { 1022, 335, 1230, 415 };
static RECT              g_tunerStatusPill   = { 745, 686, 895, 710 };

static RECT g_tabRects[8] = {
    {  18, 12,  99, 48 }, // 1. CLEAN
    { 103, 12, 184, 48 }, // 2. CRUNCH
    { 188, 12, 269, 48 }, // 3. LEAD
    { 273, 12, 354, 48 }, // 4. DRIVE
    { 358, 12, 439, 48 }, // 5. SPACE
    { 443, 12, 524, 48 }, // 6. REVERB
    { 528, 12, 609, 48 }, // 7. DELAY
    { 613, 12, 698, 48 }  // 8. TUNER
};
static RECT g_presetPrevRect = { 704, 12, 726, 48 };
static RECT g_presetNameRect = { 730, 12, 826, 48 };
static RECT g_presetNextRect = { 830, 12, 852, 48 };
static RECT g_presetSaveRect = { 856, 12, 904, 48 };
static RECT g_learnNoiseBtnRect = { 910, 12, 1004, 48 };
static RECT g_recSampleBtnRect = { 1010, 12, 1088, 48 };
static RECT g_riffBtnRect = { 1094, 12, 1168, 48 };
static RECT g_audioSettingsBtnRect = { 1174, 12, 1264, 48 };

static RECT      g_fuzzMorphMeterRect    = { 1004, 376, 1148, 392 };
static RECT      g_btnFuzzLearnMaxRect   = { 1004, 398, 1072, 424 };
static RECT      g_btnFuzzLearnCleanRect = { 1080, 398, 1148, 424 };
static ULONGLONG g_fuzzLearnStartTime   = 0;

static std::atomic<bool> g_recSampleActive{false};
static ULONGLONG         g_recSampleStartTime = 0;
static ULONGLONG         g_recSampleDoneTime = 0;
static std::vector<float> g_recSampleBuf;
static std::mutex        g_recSampleMutex;

// Parameter Indices (79 knobs total across 7 pages)
enum {
    K_CH_MODE = 0, K_QUALITY, K_CAB_SIM, K_CAB_SIZE, K_TUNING_REF, K_PICKUP_VOICE, // 0..5 (Global Header)
    // Page 0: Euphoria Supreme Glass Clean Head (6..16, 11 knobs)
    K_CLEAN_VOL, K_CLEAN_BRIGHT, K_CLEAN_DEEP, K_CLEAN_BASS, K_CLEAN_MID, K_CLEAN_MIDSHIFT,
    K_CLEAN_TREBLE, K_CLEAN_GLASS, K_CLEAN_PRESENCE, K_CLEAN_MASTER, K_CLEAN_OUT,
    // Page 1: Euphoria '59 Bassman / '68 JMP Crunch (17..26, 10 knobs)
    K_BM_DRIVE, K_BM_VOL, K_BM_BASS, K_BM_MID, K_BM_TREBLE, K_BM_PRESENCE, K_BM_SLOPE, K_BM_TUBES, K_BM_SAG, K_BM_SPKDRV,
    // Page 2: Euphoria '80s Friedman BE-100 / JCM 800 Lead Head (27..36, 10 knobs)
    K_LEAD_GAIN, K_LEAD_VOL, K_LEAD_BASS, K_LEAD_MID, K_LEAD_TREBLE, K_LEAD_PRESENCE, K_LEAD_HBE, K_LEAD_C45, K_LEAD_SAT, K_LEAD_DEHISS,
    // Page 3: Euphoria Dynamic Fluid Stompbox Lab (30 controls across 5 independent series pedals)
    K_KLON_ON, K_KLON_GAIN, K_KLON_TREBLE, K_KLON_OUTPUT, K_KLON_BLEND, K_KLON_HYPER,
    K_TS_ON, K_TS_DRIVE, K_TS_TONE, K_TS_LEVEL, K_TS_MOD, K_TS_HYPER,
    K_BD_ON, K_BD_GAIN, K_BD_TONE, K_BD_LEVEL, K_BD_PHAT, K_BD_HYPER,
    K_RAT_ON, K_RAT_DIST, K_RAT_FILTER, K_RAT_VOL, K_RAT_TURBO, K_RAT_HYPER,
    K_FUZZ_ON, K_FUZZ_GAIN, K_FUZZ_SAG, K_FUZZ_LEVEL, K_FUZZ_TRANSISTOR, K_FUZZ_HYPER,
    // Page 4: Fluid Space Chamber & Space Modulation (57..64, 8 knobs)
    K_SPACE_SHAPE, K_SPACE_SIZE, K_SPACE_MOD_ON, K_SPACE_MOD_RATE,
    K_SPACE_SHAPE_DEP, K_SPACE_SIZE_DEP, K_SPACE_WAVEFORM, K_SPACE_SPREAD,
    // Page 5: Fluid Dynamics Reverb Space & Fender Harmonic Tremolo (65..75, 11 knobs)
    K_REV_ON, K_REV_SHAPE, K_REV_SIZE, K_REV_LOWCUT, K_REV_HIGHCUT, K_REV_DWELL, K_REV_MIX,
    K_VIB_ON, K_VIB_MODE, K_VIB_SPEED, K_VIB_DEPTH,
    // Page 6: Studio Tape Delay Rack (76..83, 8 knobs)
    K_DLY_ON, K_DLY_PINGPONG, K_DLY_TIME, K_DLY_FB, K_DLY_MIX, K_DLY_DRIVE, K_DLY_FLUTTER, K_DLY_DUCK,
    NUM_KNOBS
};

static KnobCtrl g_knobs[NUM_KNOBS] = {
    // Global switches shown in top-right subbar (page -1)
    {"ROUTING",         -1, 0, 3, 0, 0, true, 4, {"CH1 SUPREME GLASS", "CH2 '59/'68 CRUNCH", "CH3 BE-100 LEAD", "JUMPERED BLEND"}, { 18, 56, 140, 92 }},
    {"FLUID OVERSAMPLE",-1, 0, 3, 2, 2, true, 4, {"ECO LIVE 1x", "STAGE 2x", "STUDIO 4x", "ULTRA 8x"},        { 144, 56, 266, 92 }},
    {"PHYSICAL SPEAKER",-1, 0, 8, 0, 0, true, 9, {"2x12 JENSEN (OLD)", "4x10 BASSMAN (OLD)", "2x10 OXFORD (OLD)", "4x12 GREENBACK (OLD)", "432Hz HARMONIC (OLD)", "HYPER GREENBACK", "EUPHORIC ALNICO", "432Hz PROJECTOR", "BYPASS CAB"}, { 270, 56, 425, 92 }},
    {"CAB SIZE / VOL",  -1, 0, 4, 2, 2, true, 5, {"0.6x COMPACT", "0.8x VINTAGE", "1.0x STANDARD", "1.3x OVERSIZED", "1.8x STADIUM"}, { 429, 56, 545, 92 }},
    {"TUNING REF",      -1, 0, 1, 0, 0, true, 2, {"440 Hz STD", "432 Hz NAT", "", ""},                          { 549, 56, 649, 92 }},
    {"PICKUP LOAD",     -1, 0, 2, 0, 0, true, 3, {"1M STUDIO", "STRAT / SC", "GRETSCH / HB", ""},                { 653, 56, 768, 92 }},

    // Page 0: Euphoria Supreme Glass Clean Head (11 knobs)
    {"VOLUME",        0, 0, 10, 5.2f, 5.2f, false, 0, {}, {}},
    {"BRIGHT VOICING",0, 0,  2, 1.0f, 1.0f, true,  3, {"WARM / OFF", "BRIGHT GLASS", "ULTRA CHIME", ""}, {}},
    {"DEEP CONTOUR",  0, 0,  1, 0.0f, 0.0f, true,  2, {"TIGHT (GRETSCH)", "DEEP 3D (STRAT)", "", ""}, {}},
    {"BASS",          0, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"MIDDLE",        0, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"MID SHIFT",     0, 0,  2, 0.0f, 0.0f, true,  3, {"450Hz (STRAT)", "750Hz (BODY)", "1.15kHz (GRETSCH)", ""}, {}},
    {"TREBLE",        0, 0, 10, 5.8f, 5.8f, false, 0, {}, {}},
    {"GLASS AIR",     0, 0, 10, 6.5f, 6.5f, false, 0, {}, {}},
    {"PRESENCE",      0, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"MASTER",        0, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"OUTPUT TRIM",   0, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},

    // Page 1: Euphoria '59 Bassman / '68 JMP Crunch (10 knobs)
    {"PREAMP DRIVE",  1, 0, 10, 5.8f, 5.8f, false, 0, {}, {}},
    {"CHANNEL VOL",   1, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"BASS",          1, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"MIDDLE",        1, 0, 10, 6.0f, 6.0f, false, 0, {}, {}},
    {"TREBLE",        1, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"PRESENCE",      1, 0, 10, 5.2f, 5.2f, false, 0, {}, {}},
    {"SLOPE VOICING", 1, 0,  1, 0.0f, 0.0f, true,  2, {"56k '59 TWEED", "33k '68 JMP", "", ""}, {}},
    {"POWER TUBES",   1, 0,  1, 0.0f, 0.0f, true,  2, {"2x 6L6GC USA", "2x EL34 UK", "", ""}, {}},
    {"FLUID SAG",     1, 0, 10, 4.5f, 4.5f, false, 0, {}, {}},
    {"CONE DRIVE",    1, 0, 10, 4.0f, 4.0f, false, 0, {}, {}},

    // Page 2: Euphoria '80s Friedman BE-100 / JCM 800 Lead Head (10 knobs)
    {"LEAD GAIN",     2, 0, 10, 6.5f, 6.5f, false, 0, {}, {}},
    {"LEAD VOL",      2, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"BASS",          2, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"MIDDLE",        2, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"TREBLE",        2, 0, 10, 6.0f, 6.0f, false, 0, {}, {}},
    {"PRESENCE",      2, 0, 10, 5.8f, 5.8f, false, 0, {}, {}},
    {"HBE CASCADE",   2, 0,  1, 0.0f, 0.0f, true,  2, {"OFF (BE-100)", "ON (HBE LEAD)", "", ""}, {}},
    {"C45 VOICING",   2, 0,  1, 1.0f, 1.0f, true,  2, {"OFF (STOCK)", "C45 MID PUNCH", "", ""}, {}},
    {"SAT CLIPPING",  2, 0,  1, 0.0f, 0.0f, true,  2, {"OFF (OPEN)", "SAT DIODE", "", ""}, {}},
    {"DYNAMIC DE-HISS", 2, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},

    // Page 3: Euphoria Dynamic Fluid Stompbox Lab (5 independent series pedals, bypass via 3D footswitches)
    // --- Pedal 1: Fluid Centaur (Klon) ---
    {"CENTAUR ON",   -2, 0,  1, 0.0f, 0.0f, true,  2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"GAIN",          3, 0, 10, 4.0f, 4.0f, false, 0, {}, {}},
    {"TREBLE",        3, 0, 10, 5.5f, 5.5f, false, 0, {}, {}},
    {"OUTPUT",        3, 0, 10, 6.5f, 6.5f, false, 0, {}, {}},
    {"CLEAN BLEND",   3, 0, 100, 35.0f, 35.0f, false, 0, {}, {}},
    {"CIRCUIT",       3, 0,  1, 0.0f, 0.0f, true,  2, {"(OLD) 18V", "HYPER 27V", "", ""}, {}},

    // --- Pedal 2: TS808 Screamer ---
    {"TS808 ON",     -2, 0,  1, 0.0f, 0.0f, true,  2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"OVERDRIVE",     3, 0, 10, 4.5f, 4.5f, false, 0, {}, {}},
    {"TONE",          3, 0, 10, 4.8f, 4.8f, false, 0, {}, {}},
    {"LEVEL",         3, 0, 10, 7.0f, 7.0f, false, 0, {}, {}},
    {"VOICING",       3, 0,  1, 0.0f, 0.0f, true,  2, {"STOCK 720Hz", "FAT MOD 340Hz", "", ""}, {}},
    {"CIRCUIT",       3, 0,  1, 0.0f, 0.0f, true,  2, {"(OLD) TS808", "HYPER VELVET", "", ""}, {}},

    // --- Pedal 3: BD-2 Blues Driver ---
    {"BLUES DRV ON", -2, 0,  1, 0.0f, 0.0f, true,  2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"GAIN",          3, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"TONE",          3, 0, 10, 5.2f, 5.2f, false, 0, {}, {}},
    {"LEVEL",         3, 0, 10, 6.8f, 6.8f, false, 0, {}, {}},
    {"VOICING",       3, 0,  1, 0.0f, 0.0f, true,  2, {"STOCK BD-2", "PHAT MOD", "", ""}, {}},
    {"CIRCUIT",       3, 0,  1, 0.0f, 0.0f, true,  2, {"(OLD) BD-2", "HYPER JFET", "", ""}, {}},

    // --- Pedal 4: Fluid Rodent (Super-Tight Smooth Synth-Fuzz) ---
    {"RODENT ON",    -2, 0,  1, 0.0f, 0.0f, true,  2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"DISTORT",       3, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"FILTER",        3, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"VOLUME",        3, 0, 10, 6.5f, 6.5f, false, 0, {}, {}},
    {"CLIPPING",      3, 0,  1, 0.0f, 0.0f, true,  2, {"SILICON", "TURBO LED", "", ""}, {}},
    {"CIRCUIT",       3, 0,  1, 0.0f, 0.0f, true,  2, {"(OLD) RODENT", "HYPER LM308", "", ""}, {}},

    // --- Pedal 5: Fluid Fuzz Face (Dallas Arbiter with Dynamic Amp Stage) ---
    {"FUZZ FACE ON", -2, 0,  1, 0.0f, 0.0f, true,  2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"FUZZ",          3, 0, 10, 6.5f, 6.5f, false, 0, {}, {}},
    {"AMP SAG",       3, 0, 10, 5.0f, 5.0f, false, 0, {}, {}},
    {"LEVEL",         3, 0, 10, 7.0f, 7.0f, false, 0, {}, {}},
    {"TRANSISTOR",    3, 0,  1, 0.0f, 0.0f, true,  2, {"NKT275 GE", "BC108 SI", "", ""}, {}},
    {"CIRCUIT",       3, 0,  1, 0.0f, 0.0f, true,  2, {"(OLD) FUZZ", "HYPER 432Hz", "", ""}, {}},

    // Page 4: Fluid Space Chamber & Space Modulation (8 knobs)
    {"CHAMBER SHAPE", 4, 0, 10, 0.0f, 0.0f, true, 11,
        {"CIRCLE TUBE", "24-GON", "16-GON", "12-DODECAGON", "10-DECAGON", "9-NONAGON", "8-OCTAGON", "7-HEPTAGON", "6-HEXAGON", "5-PENTAGON", "4-SQUARE CUBE"}, {}},
    {"432Hz SIZE / VOL", 4, 0, 18, 9.0f, 9.0f, true, 19,
        {"/10 (0.1x)", "/9 (0.11x)", "/8 (0.12x)", "/7 (0.14x)", "/6 (0.17x)", "/5 (0.2x)", "/4 (0.25x)", "/3 (0.33x)", "/2 (0.5x)",
         "1x (432 Hz)", "x2 (2.0x)", "x3 (3.0x)", "x4 (4.0x)", "x5 (5.0x)", "x6 (6.0x)", "x7 (7.0x)", "x8 (8.0x)", "x9 (9.0x)", "x10 (10x)"}, {}},
    {"SPACE MOD",     4, 0, 1, 0.0f, 0.0f, true, 2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"MOD RATE Hz",   4, 0.05f, 8.0f, 0.75f, 0.75f, false, 0, {}, {}},
    {"SHAPE MOD %",   4, 0, 100, 40.0f, 40.0f, false, 0, {}, {}},
    {"SIZE MOD %",    4, 0, 100, 35.0f, 35.0f, false, 0, {}, {}},
    {"MOD WAVEFORM",  4, 0, 3, 0.0f, 0.0f, true, 4, {"SINE WAVE", "TRIANGLE", "432Hz GOLDEN", "SMOOTH DRIFT"}, {}},
    {"STEREO SPREAD", 4, 0, 180, 90.0f, 90.0f, false, 0, {}, {}},

    // Page 5: Fluid Dynamics Reverb Space & Vintage Fender Harmonic Tremolo (11 knobs)
    {"REVERB SWITCH", 5, 0, 1, 1.0f, 1.0f, true, 2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"REV SHAPE",     5, 0, 10, 0.0f, 0.0f, true, 11,
        {"CIRCLE TUBE", "24-GON", "16-GON", "12-DODECAGON", "10-DECAGON", "9-NONAGON", "8-OCTAGON", "7-HEPTAGON", "6-HEXAGON", "5-PENTAGON", "4-SQUARE CUBE"}, {}},
    {"REV 432Hz SIZE",5, 0, 18, 9.0f, 9.0f, true, 19,
        {"/10 (0.1x)", "/9 (0.11x)", "/8 (0.12x)", "/7 (0.14x)", "/6 (0.17x)", "/5 (0.2x)", "/4 (0.25x)", "/3 (0.33x)", "/2 (0.5x)",
         "1x (432 Hz)", "x2 (2.0x)", "x3 (3.0x)", "x4 (4.0x)", "x5 (5.0x)", "x6 (6.0x)", "x7 (7.0x)", "x8 (8.0x)", "x9 (9.0x)", "x10 (10x)"}, {}},
    {"SEND LOW-CUT",  5, 40.0f, 600.0f, 120.0f, 120.0f, false, 0, {}, {}},
    {"SEND HIGH-CUT", 5, 2500.0f, 14000.0f, 6500.0f, 6500.0f, false, 0, {}, {}},
    {"FLUID DWELL",   5, 0.0f, 10.0f, 5.0f, 5.0f, false, 0, {}, {}},
    {"REVERB MIX %",  5, 0.0f, 100.0f, 28.0f, 28.0f, false, 0, {}, {}},
    {"VIB SWITCH",    5, 0, 1, 1.0f, 1.0f, true, 2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"VIB CIRCUIT",   5, 0, 1, 0.0f, 0.0f, true, 2, {"HARMONIC TUBE", "OPTO PHOTOCELL", "", ""}, {}},
    {"SPEED (Hz)",    5, 0.5f, 12.0f, 4.2f, 4.2f, false, 0, {}, {}},
    {"INTENSITY",     5, 0.0f, 10.0f, 5.5f, 5.5f, false, 0, {}, {}},

    // Page 6: Studio 15/30 IPS Tape & Ping-Pong Delay Rack (8 knobs)
    {"TAPE BYPASS",   6, 0,  1, 1.0f, 1.0f, true,  2, {"BYPASSED", "ACTIVE ON", "", ""}, {}},
    {"ROUTING MODE",  6, 0,  1, 1.0f, 1.0f, true,  2, {"15/30 MONO", "PING-PONG", "", ""}, {}},
    {"DELAY TIME ms", 6, 20, 1500, 360.0f, 360.0f, false, 0, {}, {}},
    {"FEEDBACK %",    6, 0, 95, 34.0f, 34.0f, false, 0, {}, {}},
    {"WET MIX %",     6, 0, 100, 22.0f, 22.0f, false, 0, {}, {}},
    {"OXIDE DRIVE %", 6, 0, 100, 28.0f, 28.0f, false, 0, {}, {}},
    {"WOW & FLUTTER", 6, 0, 100, 18.0f, 18.0f, false, 0, {}, {}},
    {"AUTO DUCKING",  6, 0, 100, 25.0f, 25.0f, false, 0, {}, {}}
};

// ============================================================================
// Euphoria User Preset System (Save, Load, Naming & Default Auto-Load)
// ============================================================================
struct EuphoriaPreset {
    char name[64];
    float vals[NUM_KNOBS];
};

static std::vector<EuphoriaPreset> g_presets;
static int g_currentPresetIdx = 0;
static char g_savePresetNameBuf[64] = "default";

static std::string getPresetsDir() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    std::string pDir = std::string(exePath) + "presets\\";
    CreateDirectoryA(pDir.c_str(), nullptr);
    return pDir;
}

static bool savePresetToFile(const char* presetName) {
    if (!presetName || presetName[0] == '\0') return false;
    std::string dir = getPresetsDir();
    std::string fPath = dir + presetName + ".ini";

    FILE* fp = fopen(fPath.c_str(), "w");
    if (!fp) return false;

    fprintf(fp, "[EuphoriaPreset]\nName=%s\n", presetName);
    for (int i = 0; i < NUM_KNOBS; ++i) {
        fprintf(fp, "Knob%d=%.4f\n", i, g_knobs[i].curVal);
    }
    fclose(fp);

    bool found = false;
    for (size_t i = 0; i < g_presets.size(); ++i) {
        if (_stricmp(g_presets[i].name, presetName) == 0) {
            for (int k = 0; k < NUM_KNOBS; ++k) g_presets[i].vals[k] = g_knobs[k].curVal;
            g_currentPresetIdx = (int)i;
            found = true;
            break;
        }
    }
    if (!found) {
        EuphoriaPreset np;
        strncpy(np.name, presetName, sizeof(np.name) - 1);
        np.name[sizeof(np.name) - 1] = '\0';
        for (int k = 0; k < NUM_KNOBS; ++k) np.vals[k] = g_knobs[k].curVal;
        g_presets.push_back(np);
        g_currentPresetIdx = (int)g_presets.size() - 1;
    }
    return true;
}

static bool saveWavFile(const char* filePath, const float* samples, size_t count, int sampleRate) {
#pragma pack(push, 1)
    struct {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize = 0;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4]  = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1;
        uint16_t numChannels = 1;
        uint32_t sampleRate = 48000;
        uint32_t byteRate = 48000 * 2;
        uint16_t blockAlign = 2;
        uint16_t bitsPerSample = 16;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize = 0;
    } hdr;
#pragma pack(pop)

    hdr.sampleRate = (uint32_t)sampleRate;
    hdr.byteRate = hdr.sampleRate * 2;
    hdr.dataSize = (uint32_t)(count * sizeof(int16_t));
    hdr.fileSize = 36 + hdr.dataSize;

    std::vector<int16_t> pcm(count);
    for (size_t i = 0; i < count; ++i) {
        float s = samples[i];
        int32_t v = (int32_t)(s * 32767.0f);
        pcm[i] = (int16_t)std::max(-32768, std::min(32767, v));
    }

    FILE* fp = fopen(filePath, "wb");
    if (!fp) return false;
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fwrite(pcm.data(), sizeof(int16_t), count, fp);
    fclose(fp);
    return true;
}

static bool loadPresetFromFile(const std::string& fPath, EuphoriaPreset& outPreset) {
    FILE* fp = fopen(fPath.c_str(), "r");
    if (!fp) return false;

    char line[256];
    outPreset.name[0] = '\0';
    for (int i = 0; i < NUM_KNOBS; ++i) outPreset.vals[i] = g_knobs[i].defVal;

    while (fgets(line, sizeof(line), fp)) {
        char* cr = strpbrk(line, "\r\n");
        if (cr) *cr = '\0';
        if (strncmp(line, "Name=", 5) == 0) {
            strncpy(outPreset.name, line + 5, sizeof(outPreset.name) - 1);
            outPreset.name[sizeof(outPreset.name) - 1] = '\0';
        } else if (strncmp(line, "Knob", 4) == 0) {
            int idx = -1;
            float val = 0.0f;
            if (sscanf(line, "Knob%d=%f", &idx, &val) == 2) {
                if (idx >= 0 && idx < NUM_KNOBS) {
                    outPreset.vals[idx] = val;
                }
            }
        }
    }
    fclose(fp);
    if (outPreset.name[0] == '\0') {
        size_t b = fPath.find_last_of("\\/");
        std::string fname = (b != std::string::npos) ? fPath.substr(b + 1) : fPath;
        if (fname.size() > 4 && fname.substr(fname.size() - 4) == ".ini") {
            fname = fname.substr(0, fname.size() - 4);
        }
        strncpy(outPreset.name, fname.c_str(), sizeof(outPreset.name) - 1);
    }
    return true;
}

static void applyPreset(const EuphoriaPreset& p) {
    for (int i = 0; i < NUM_KNOBS; ++i) {
        g_knobs[i].curVal = std::max(g_knobs[i].minVal, std::min(g_knobs[i].maxVal, p.vals[i]));
    }
}

static void initPresetSystem() {
    std::string dir = getPresetsDir();
    g_presets.clear();

    std::string defPath = dir + "default.ini";
    bool needsFactorySeed = false;
    FILE* testFp = fopen(defPath.c_str(), "r");
    if (!testFp) {
        needsFactorySeed = true;
    } else {
        char checkBuf[4096];
        size_t n = fread(checkBuf, 1, sizeof(checkBuf) - 1, testFp);
        checkBuf[n] = '\0';
        fclose(testFp);
        char targetKnob[32];
        std::snprintf(targetKnob, sizeof(targetKnob), "Knob%d=", NUM_KNOBS - 1);
        if (strstr(checkBuf, targetKnob) == nullptr) {
            needsFactorySeed = true;
        }
    }

    if (needsFactorySeed) {
        // Factory 1: default (Euphoria Supreme Glass - Stratocaster)
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // CH1 Euphoria Supreme Glass Clean
        g_knobs[K_CAB_SIM].curVal = 0.0f; // 2x12 Jensen C12N
        g_knobs[K_PICKUP_VOICE].curVal = 1.0f; // Strat / SC Glass
        g_knobs[K_CLEAN_VOL].curVal = 5.2f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 1.0f; // Bright Glass
        g_knobs[K_CLEAN_DEEP].curVal = 1.0f;   // Deep 3D (Strat)
        g_knobs[K_CLEAN_BASS].curVal = 5.2f;
        g_knobs[K_CLEAN_MID].curVal = 4.8f;
        g_knobs[K_CLEAN_MIDSHIFT].curVal = 0.0f; // 450Hz Scoop (Strat)
        g_knobs[K_CLEAN_TREBLE].curVal = 6.0f;
        g_knobs[K_CLEAN_GLASS].curVal = 6.6f;   // 3D Glass Air
        g_knobs[K_CLEAN_PRESENCE].curVal = 5.5f;
        g_knobs[K_CLEAN_MASTER].curVal = 6.0f;
        g_knobs[K_CLEAN_OUT].curVal = 5.0f;
        savePresetToFile("default");

        // Factory 2: Euphoria Supreme Glass (Gretsch CB Junior)
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // CH1 Euphoria Supreme Glass Clean
        g_knobs[K_CAB_SIM].curVal = 0.0f; // 2x12 Jensen C12N
        g_knobs[K_PICKUP_VOICE].curVal = 2.0f; // Gretsch / HB Punch
        g_knobs[K_CLEAN_VOL].curVal = 5.5f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 2.0f; // Ultra Chime (brings Filter'Trons to life!)
        g_knobs[K_CLEAN_DEEP].curVal = 0.0f;   // Tight (cuts sub-mud below 65 Hz)
        g_knobs[K_CLEAN_BASS].curVal = 4.8f;   // Fast articulate low punch
        g_knobs[K_CLEAN_MID].curVal = 5.6f;
        g_knobs[K_CLEAN_MIDSHIFT].curVal = 2.0f; // 1.15kHz Gretsch Cut!
        g_knobs[K_CLEAN_TREBLE].curVal = 6.2f;
        g_knobs[K_CLEAN_GLASS].curVal = 7.2f;  // Crystalline chime
        g_knobs[K_CLEAN_PRESENCE].curVal = 6.0f;
        g_knobs[K_CLEAN_MASTER].curVal = 6.0f;
        g_knobs[K_CLEAN_OUT].curVal = 5.0f;
        savePresetToFile("Euphoria Supreme Glass (Gretsch)");

        // Factory 3: Two-Rock Vocal Clean
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // CH1 Clean
        g_knobs[K_CAB_SIM].curVal = 2.0f; // 2x10 Vibrolux
        g_knobs[K_CLEAN_VOL].curVal = 5.6f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 1.0f;
        g_knobs[K_CLEAN_DEEP].curVal = 1.0f;   // Deep 3D
        g_knobs[K_CLEAN_BASS].curVal = 5.4f;
        g_knobs[K_CLEAN_MID].curVal = 6.2f;
        g_knobs[K_CLEAN_MIDSHIFT].curVal = 1.0f; // 750Hz Body (Two-Rock)
        g_knobs[K_CLEAN_TREBLE].curVal = 5.8f;
        g_knobs[K_CLEAN_GLASS].curVal = 6.0f;
        g_knobs[K_CLEAN_PRESENCE].curVal = 5.2f;
        g_knobs[K_CLEAN_MASTER].curVal = 5.8f;
        savePresetToFile("Two-Rock Vocal Clean");

        // Factory 4: Tweed Bassman Crunch
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 1.0f; // CH2 Crunch
        g_knobs[K_CAB_SIM].curVal = 1.0f; // 4x10 Bassman
        g_knobs[K_CAB_SIZE].curVal = 2.0f;
        g_knobs[K_BM_DRIVE].curVal = 6.2f;
        g_knobs[K_BM_VOL].curVal = 5.8f;
        g_knobs[K_BM_BASS].curVal = 5.8f;
        g_knobs[K_BM_MID].curVal = 6.2f;
        g_knobs[K_BM_TREBLE].curVal = 5.6f;
        g_knobs[K_BM_SLOPE].curVal = 0.0f; // 56k Tweed
        g_knobs[K_BM_TUBES].curVal = 0.0f; // 6L6GC
        g_knobs[K_BM_PRESENCE].curVal = 5.8f;
        g_knobs[K_BM_SAG].curVal = 4.8f;
        g_knobs[K_BM_SPKDRV].curVal = 4.5f;
        g_knobs[K_SPACE_SHAPE].curVal = 0.0f;
        g_knobs[K_SPACE_SIZE].curVal = 9.0f; // CENTER: 1x (432Hz)
        savePresetToFile("Tweed Bassman Crunch");

        // Factory 5: JMP British Roar
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 1.0f; // CH2 Crunch
        g_knobs[K_CAB_SIM].curVal = 3.0f; // 4x12 Greenback
        g_knobs[K_CAB_SIZE].curVal = 3.0f; // 1.3x Oversized
        g_knobs[K_BM_DRIVE].curVal = 7.8f;
        g_knobs[K_BM_VOL].curVal = 6.0f;
        g_knobs[K_BM_BASS].curVal = 5.0f;
        g_knobs[K_BM_MID].curVal = 6.8f;
        g_knobs[K_BM_TREBLE].curVal = 6.5f;
        g_knobs[K_BM_SLOPE].curVal = 1.0f; // 33k JMP
        g_knobs[K_BM_TUBES].curVal = 1.0f; // EL34 UK
        g_knobs[K_BM_PRESENCE].curVal = 6.5f;
        g_knobs[K_BM_SAG].curVal = 5.5f;
        g_knobs[K_BM_SPKDRV].curVal = 6.2f;
        g_knobs[K_SPACE_SHAPE].curVal = 4.0f; // 10-Decagon
        g_knobs[K_SPACE_SIZE].curVal = 10.0f; // MULT: x2 (2.0x)
        savePresetToFile("JMP British Roar");

        // Factory 6: Brown Sound BE-100 Lead
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 2.0f; // CH3 BE-100 Lead
        g_knobs[K_CAB_SIM].curVal = 3.0f; // 4x12 Greenback
        g_knobs[K_CAB_SIZE].curVal = 3.0f; // 1.3x Oversized
        g_knobs[K_LEAD_GAIN].curVal = 7.5f;
        g_knobs[K_LEAD_VOL].curVal = 5.8f;
        g_knobs[K_LEAD_BASS].curVal = 5.2f;
        g_knobs[K_LEAD_MID].curVal = 6.2f;
        g_knobs[K_LEAD_TREBLE].curVal = 6.5f;
        g_knobs[K_LEAD_PRESENCE].curVal = 6.0f;
        g_knobs[K_LEAD_HBE].curVal = 0.0f;
        g_knobs[K_LEAD_C45].curVal = 1.0f; // C45 mid punch
        g_knobs[K_LEAD_SAT].curVal = 0.0f;
        g_knobs[K_LEAD_DEHISS].curVal = 5.5f;
        savePresetToFile("Brown Sound BE-100 Lead");

        // Factory 7: Hot-Rodded Screamer Lead
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 2.0f; // CH3 BE-100 Lead
        g_knobs[K_CAB_SIM].curVal = 3.0f; // 4x12 Greenback
        g_knobs[K_TS_ON].curVal = 1.0f; // TS808 active
        g_knobs[K_TS_DRIVE].curVal = 2.5f;
        g_knobs[K_TS_TONE].curVal = 6.2f;
        g_knobs[K_TS_LEVEL].curVal = 8.8f; // High-voltage clean boost driver (+20dB into tube input)
        g_knobs[K_TS_MOD].curVal = 0.0f;
        g_knobs[K_TS_HYPER].curVal = 0.0f; // (OLD) TS808
        g_knobs[K_LEAD_GAIN].curVal = 7.0f;
        g_knobs[K_LEAD_VOL].curVal = 5.5f;
        g_knobs[K_LEAD_BASS].curVal = 4.8f;
        g_knobs[K_LEAD_MID].curVal = 6.0f;
        g_knobs[K_LEAD_TREBLE].curVal = 6.8f;
        g_knobs[K_LEAD_PRESENCE].curVal = 6.2f;
        g_knobs[K_LEAD_HBE].curVal = 1.0f; // HBE High-Gain Cascade
        g_knobs[K_LEAD_C45].curVal = 1.0f;
        g_knobs[K_LEAD_SAT].curVal = 1.0f; // SAT Diodes
        g_knobs[K_LEAD_DEHISS].curVal = 6.0f;
        savePresetToFile("Hot-Rodded Screamer Lead");

        // Factory 8: Centaur Transparent Boost
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // CH1 Clean
        g_knobs[K_CAB_SIM].curVal = 0.0f; // 2x12 Jensen
        g_knobs[K_CLEAN_VOL].curVal = 5.2f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 1.0f;
        g_knobs[K_KLON_ON].curVal = 1.0f; // Centaur active
        g_knobs[K_KLON_GAIN].curVal = 2.8f;
        g_knobs[K_KLON_TREBLE].curVal = 6.0f;
        g_knobs[K_KLON_OUTPUT].curVal = 8.5f; // Massive clean boost headroom (+22dB)
        g_knobs[K_KLON_BLEND].curVal = 45.0f;
        g_knobs[K_KLON_HYPER].curVal = 0.0f; // (OLD) 18V Klon
        savePresetToFile("Centaur Transparent Boost");

        // Factory 9: Blues Driver Touch Dynamic
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // CH1 Clean Head
        g_knobs[K_CAB_SIM].curVal = 0.0f; // 2x12 Jensen
        g_knobs[K_CLEAN_VOL].curVal = 5.0f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 1.0f;
        g_knobs[K_BD_ON].curVal = 1.0f; // BD-2 active
        g_knobs[K_BD_LEVEL].curVal = 7.8f; // High-headroom clean drive (+22dB)
        g_knobs[K_BD_TONE].curVal = 5.4f;
        g_knobs[K_BD_GAIN].curVal = 4.8f; // Touch-sensitive discrete JFET crunch
        g_knobs[K_BD_PHAT].curVal = 1.0f; // Keeley Phat Mod (+3.2dB low-end punch)
        g_knobs[K_BD_HYPER].curVal = 0.0f; // (OLD) BD-2
        savePresetToFile("Blues Driver Touch Dynamic");

        // Factory 10: 432Hz Golden Harmonic
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 3.0f; // Jumpered Blend
        g_knobs[K_CAB_SIM].curVal = 4.0f; // 432Hz Cab
        g_knobs[K_TUNING_REF].curVal = 1.0f; // 432Hz
        g_knobs[K_CLEAN_VOL].curVal = 5.0f;
        g_knobs[K_BM_DRIVE].curVal = 4.5f;
        g_knobs[K_SPACE_SHAPE].curVal = 0.0f;
        g_knobs[K_SPACE_SIZE].curVal = 9.0f; // CENTER: 1x (432Hz)
        g_knobs[K_SPACE_MOD_ON].curVal = 1.0f; // ACTIVE ON
        g_knobs[K_SPACE_MOD_RATE].curVal = 0.432f;
        g_knobs[K_SPACE_SHAPE_DEP].curVal = 30.0f;
        g_knobs[K_SPACE_SIZE_DEP].curVal = 35.0f;
        g_knobs[K_SPACE_WAVEFORM].curVal = 2.0f; // 432Hz Golden
        g_knobs[K_SPACE_SPREAD].curVal = 90.0f;
        g_knobs[K_REV_ON].curVal = 1.0f;
        g_knobs[K_REV_SHAPE].curVal = 0.0f; // Circle tube
        g_knobs[K_REV_SIZE].curVal = 9.0f;  // 1x 432Hz
        g_knobs[K_REV_LOWCUT].curVal = 140.0f;
        g_knobs[K_REV_HIGHCUT].curVal = 7000.0f;
        g_knobs[K_REV_DWELL].curVal = 6.0f;
        g_knobs[K_REV_MIX].curVal = 32.0f;
        g_knobs[K_VIB_ON].curVal = 1.0f;
        g_knobs[K_VIB_MODE].curVal = 0.0f; // Harmonic Tube
        g_knobs[K_VIB_SPEED].curVal = 4.32f;
        g_knobs[K_VIB_DEPTH].curVal = 6.5f;
        g_knobs[K_DLY_ON].curVal = 1.0f;
        g_knobs[K_DLY_MIX].curVal = 24.0f;
        savePresetToFile("432Hz Golden Harmonic");

        // Factory 11: Surf Spring Dream
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // Clean
        g_knobs[K_CAB_SIM].curVal = 0.0f; // 2x12
        g_knobs[K_CLEAN_VOL].curVal = 4.6f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 1.0f;
        g_knobs[K_REV_ON].curVal = 1.0f;
        g_knobs[K_REV_SHAPE].curVal = 0.0f; // Circle tube
        g_knobs[K_REV_SIZE].curVal = 10.0f; // x2 chamber
        g_knobs[K_REV_LOWCUT].curVal = 90.0f;
        g_knobs[K_REV_HIGHCUT].curVal = 8500.0f;
        g_knobs[K_REV_DWELL].curVal = 7.5f;
        g_knobs[K_REV_MIX].curVal = 45.0f;
        g_knobs[K_VIB_ON].curVal = 1.0f;
        g_knobs[K_VIB_MODE].curVal = 0.0f; // Harmonic Tube
        g_knobs[K_VIB_SPEED].curVal = 5.2f;
        g_knobs[K_VIB_DEPTH].curVal = 7.0f;
        savePresetToFile("Surf Spring Dream");

        // Factory 12: Ambient Tape Echo
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 3.0f; // Jumpered Blend
        g_knobs[K_CAB_SIM].curVal = 2.0f; // 2x10 Oxford
        g_knobs[K_REV_ON].curVal = 1.0f;
        g_knobs[K_REV_SHAPE].curVal = 3.0f; // 12-Dodecagon
        g_knobs[K_REV_SIZE].curVal = 12.0f; // x4 Large Hall
        g_knobs[K_REV_LOWCUT].curVal = 120.0f;
        g_knobs[K_REV_HIGHCUT].curVal = 6000.0f;
        g_knobs[K_REV_DWELL].curVal = 6.8f;
        g_knobs[K_REV_MIX].curVal = 35.0f;
        g_knobs[K_DLY_ON].curVal = 1.0f;
        g_knobs[K_DLY_PINGPONG].curVal = 1.0f;
        g_knobs[K_DLY_TIME].curVal = 480.0f;
        g_knobs[K_DLY_FB].curVal = 48.0f;
        g_knobs[K_DLY_MIX].curVal = 32.0f;
        g_knobs[K_DLY_FLUTTER].curVal = 22.0f;
        savePresetToFile("Ambient Tape Echo");

        // Factory 13: Gretsch CB Junior Rock
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 2.0f; // CH3 BE-100 Lead
        g_knobs[K_CAB_SIM].curVal = 3.0f; // 4x12 Greenback
        g_knobs[K_CAB_SIZE].curVal = 2.0f; // 1.0x Standard
        g_knobs[K_PICKUP_VOICE].curVal = 2.0f; // Gretsch / HB Punch
        g_knobs[K_LEAD_GAIN].curVal = 6.8f;
        g_knobs[K_LEAD_VOL].curVal = 5.5f;
        g_knobs[K_LEAD_BASS].curVal = 5.2f;
        g_knobs[K_LEAD_MID].curVal = 6.0f;
        g_knobs[K_LEAD_TREBLE].curVal = 6.2f;
        g_knobs[K_LEAD_PRESENCE].curVal = 5.8f;
        g_knobs[K_LEAD_HBE].curVal = 0.0f;
        g_knobs[K_LEAD_C45].curVal = 1.0f; // C45 mid punch
        g_knobs[K_LEAD_SAT].curVal = 0.0f;
        g_knobs[K_LEAD_DEHISS].curVal = 5.0f;
        savePresetToFile("Gretsch CB Junior Rock");

        // Factory 14: Hyper Greenback Punch (Hyper-Fluid Speaker Model 5)
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 1.0f; // CH2 Crunch
        g_knobs[K_CAB_SIM].curVal = 5.0f; // HYPER GREENBACK
        g_knobs[K_CAB_SIZE].curVal = 3.0f; // 1.3x Oversized
        g_knobs[K_BM_DRIVE].curVal = 7.2f;
        g_knobs[K_BM_VOL].curVal = 5.8f;
        g_knobs[K_BM_BASS].curVal = 5.2f;
        g_knobs[K_BM_MID].curVal = 6.5f;
        g_knobs[K_BM_TREBLE].curVal = 6.2f;
        g_knobs[K_BM_SLOPE].curVal = 1.0f; // 33k JMP
        g_knobs[K_BM_TUBES].curVal = 1.0f; // EL34 UK
        g_knobs[K_BM_PRESENCE].curVal = 6.0f;
        g_knobs[K_BM_SAG].curVal = 5.0f;
        g_knobs[K_BM_SPKDRV].curVal = 6.5f; // Push Chladni 3rd/5th break-up & dynamic hysteresis
        savePresetToFile("Hyper Greenback Punch");

        // Factory 15: Euphoric Alnico Chime (Hyper-Fluid Speaker Model 6)
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // CH1 Clean
        g_knobs[K_CAB_SIM].curVal = 6.0f; // EUPHORIC ALNICO
        g_knobs[K_CAB_SIZE].curVal = 2.0f; // 1.0x Standard
        g_knobs[K_PICKUP_VOICE].curVal = 1.0f; // Strat / SC Glass
        g_knobs[K_CLEAN_VOL].curVal = 5.4f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 1.0f; // Bright Glass
        g_knobs[K_CLEAN_DEEP].curVal = 1.0f;   // Deep 3D
        g_knobs[K_CLEAN_BASS].curVal = 5.0f;
        g_knobs[K_CLEAN_MID].curVal = 5.2f;
        g_knobs[K_CLEAN_MIDSHIFT].curVal = 0.0f;
        g_knobs[K_CLEAN_TREBLE].curVal = 6.2f;
        g_knobs[K_CLEAN_GLASS].curVal = 7.0f;  // 3D Glass Air
        g_knobs[K_CLEAN_PRESENCE].curVal = 5.8f;
        g_knobs[K_CLEAN_MASTER].curVal = 6.2f;
        g_knobs[K_BM_SPKDRV].curVal = 5.5f;   // Alnico magnetic compression bloom
        savePresetToFile("Euphoric Alnico Chime");

        // Factory 16: 432Hz Golden Projector (Hyper-Fluid Speaker Model 7)
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 3.0f; // Jumpered Blend
        g_knobs[K_CAB_SIM].curVal = 7.0f; // 432Hz PROJECTOR
        g_knobs[K_TUNING_REF].curVal = 1.0f; // 432Hz Natural Tuning
        g_knobs[K_CLEAN_VOL].curVal = 5.2f;
        g_knobs[K_BM_DRIVE].curVal = 5.0f;
        g_knobs[K_BM_SPKDRV].curVal = 5.8f; // Phi-scaled overtone drive
        g_knobs[K_SPACE_SHAPE].curVal = 0.0f;
        g_knobs[K_SPACE_SIZE].curVal = 9.0f; // 1x 432Hz
        g_knobs[K_SPACE_MOD_ON].curVal = 1.0f;
        g_knobs[K_SPACE_MOD_RATE].curVal = 0.432f;
        g_knobs[K_SPACE_WAVEFORM].curVal = 2.0f; // 432Hz Golden
        g_knobs[K_REV_ON].curVal = 1.0f;
        g_knobs[K_REV_SIZE].curVal = 9.0f;
        g_knobs[K_REV_DWELL].curVal = 6.0f;
        g_knobs[K_REV_MIX].curVal = 28.0f;
        savePresetToFile("432Hz Golden Projector");

        // Restore defaults for active console
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
    }

    std::string hypCheckPath = dir + "Hyper Greenback Punch.ini";
    FILE* hypFp = fopen(hypCheckPath.c_str(), "r");
    if (!hypFp) {
        // Seed new hyper presets if not already present
        // Factory 14: Hyper Greenback Punch
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 1.0f; // CH2 Crunch
        g_knobs[K_CAB_SIM].curVal = 5.0f; // HYPER GREENBACK
        g_knobs[K_CAB_SIZE].curVal = 3.0f; // 1.3x Oversized
        g_knobs[K_BM_DRIVE].curVal = 7.2f;
        g_knobs[K_BM_VOL].curVal = 5.8f;
        g_knobs[K_BM_BASS].curVal = 5.2f;
        g_knobs[K_BM_MID].curVal = 6.5f;
        g_knobs[K_BM_TREBLE].curVal = 6.2f;
        g_knobs[K_BM_SLOPE].curVal = 1.0f; // 33k JMP
        g_knobs[K_BM_TUBES].curVal = 1.0f; // EL34 UK
        g_knobs[K_BM_PRESENCE].curVal = 6.0f;
        g_knobs[K_BM_SAG].curVal = 5.0f;
        g_knobs[K_BM_SPKDRV].curVal = 6.5f;
        savePresetToFile("Hyper Greenback Punch");

        // Factory 15: Euphoric Alnico Chime
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 0.0f; // CH1 Clean
        g_knobs[K_CAB_SIM].curVal = 6.0f; // EUPHORIC ALNICO
        g_knobs[K_CAB_SIZE].curVal = 2.0f; // 1.0x Standard
        g_knobs[K_PICKUP_VOICE].curVal = 1.0f; // Strat / SC Glass
        g_knobs[K_CLEAN_VOL].curVal = 5.4f;
        g_knobs[K_CLEAN_BRIGHT].curVal = 1.0f; // Bright Glass
        g_knobs[K_CLEAN_DEEP].curVal = 1.0f;   // Deep 3D
        g_knobs[K_CLEAN_BASS].curVal = 5.0f;
        g_knobs[K_CLEAN_MID].curVal = 5.2f;
        g_knobs[K_CLEAN_MIDSHIFT].curVal = 0.0f;
        g_knobs[K_CLEAN_TREBLE].curVal = 6.2f;
        g_knobs[K_CLEAN_GLASS].curVal = 7.0f;  // 3D Glass Air
        g_knobs[K_CLEAN_PRESENCE].curVal = 5.8f;
        g_knobs[K_CLEAN_MASTER].curVal = 6.2f;
        g_knobs[K_BM_SPKDRV].curVal = 5.5f;
        savePresetToFile("Euphoric Alnico Chime");

        // Factory 16: 432Hz Golden Projector
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
        g_knobs[K_CH_MODE].curVal = 3.0f; // Jumpered Blend
        g_knobs[K_CAB_SIM].curVal = 7.0f; // 432Hz PROJECTOR
        g_knobs[K_TUNING_REF].curVal = 1.0f; // 432Hz Natural Tuning
        g_knobs[K_CLEAN_VOL].curVal = 5.2f;
        g_knobs[K_BM_DRIVE].curVal = 5.0f;
        g_knobs[K_BM_SPKDRV].curVal = 5.8f;
        g_knobs[K_SPACE_SHAPE].curVal = 0.0f;
        g_knobs[K_SPACE_SIZE].curVal = 9.0f; // 1x 432Hz
        g_knobs[K_SPACE_MOD_ON].curVal = 1.0f;
        g_knobs[K_SPACE_MOD_RATE].curVal = 0.432f;
        g_knobs[K_SPACE_WAVEFORM].curVal = 2.0f; // 432Hz Golden
        g_knobs[K_REV_ON].curVal = 1.0f;
        g_knobs[K_REV_SIZE].curVal = 9.0f;
        g_knobs[K_REV_DWELL].curVal = 6.0f;
        g_knobs[K_REV_MIX].curVal = 28.0f;
        savePresetToFile("432Hz Golden Projector");

        // Restore defaults for active console
        for (int i = 0; i < NUM_KNOBS; ++i) g_knobs[i].curVal = g_knobs[i].defVal;
    } else {
        fclose(hypFp);
    }

    // Scan directory for all *.ini presets
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA((dir + "*.ini").c_str(), &fd);
    g_presets.clear();
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                EuphoriaPreset ep;
                if (loadPresetFromFile(dir + fd.cFileName, ep)) {
                    g_presets.push_back(ep);
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // Auto-load "default" preset if present
    int defIdx = 0;
    for (size_t i = 0; i < g_presets.size(); ++i) {
        if (_stricmp(g_presets[i].name, "default") == 0) {
            defIdx = (int)i;
            break;
        }
    }
    if (!g_presets.empty()) {
        g_currentPresetIdx = defIdx;
        applyPreset(g_presets[g_currentPresetIdx]);
    }
}

static LRESULT CALLBACK savePresetDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hf = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hfBold = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            HWND hLbl = CreateWindowA("STATIC", "Enter Preset Name (name as 'default' for auto-startup default):", WS_CHILD | WS_VISIBLE, 20, 16, 380, 20, hwnd, nullptr, nullptr, nullptr);
            SendMessageA(hLbl, WM_SETFONT, (WPARAM)hfBold, TRUE);

            HWND hEdit = CreateWindowA("EDIT", g_savePresetNameBuf, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 20, 44, 380, 26, hwnd, (HMENU)201, nullptr, nullptr);
            SendMessageA(hEdit, WM_SETFONT, (WPARAM)hf, TRUE);
            SendMessageA(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);

            HWND hBtnSave = CreateWindowA("BUTTON", "Save Preset", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 180, 84, 105, 30, hwnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageA(hBtnSave, WM_SETFONT, (WPARAM)hfBold, TRUE);

            HWND hBtnCancel = CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE, 295, 84, 105, 30, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageA(hBtnCancel, WM_SETFONT, (WPARAM)hf, TRUE);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDOK) {
                GetDlgItemTextA(hwnd, 201, g_savePresetNameBuf, sizeof(g_savePresetNameBuf));
                char* p = g_savePresetNameBuf;
                while (*p == ' ') p++;
                if (*p != '\0') {
                    savePresetToFile(p);
                }
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

static void openSavePresetDialog(HWND hwndParent) {
    if (g_presets.size() > 0 && g_currentPresetIdx >= 0 && g_currentPresetIdx < (int)g_presets.size()) {
        strncpy(g_savePresetNameBuf, g_presets[g_currentPresetIdx].name, sizeof(g_savePresetNameBuf) - 1);
        g_savePresetNameBuf[sizeof(g_savePresetNameBuf) - 1] = '\0';
    } else {
        strcpy(g_savePresetNameBuf, "default");
    }

    WNDCLASSEXA dc = {};
    dc.cbSize = sizeof(dc);
    dc.lpfnWndProc = savePresetDlgProc;
    dc.hInstance = GetModuleHandle(nullptr);
    dc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    dc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    dc.lpszClassName = "EuphoriaSavePresetDlgWnd";
    RegisterClassExA(&dc);

    RECT pr;
    GetWindowRect(hwndParent, &pr);
    int cx = (pr.left + pr.right) / 2 - 210;
    int cy = (pr.top + pr.bottom) / 2 - 80;

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        dc.lpszClassName,
        "Save Euphoria Amp Preset",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, 435, 160,
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

static void layoutPageKnobs() {
    for (int page = 0; page < 7; ++page) {
        if (page == 3) {
            // Page 3: Euphoria Dynamic Fluid Stompbox Lab (Hardwood Floor Deck)
            // 5 Independent Series Stompboxes across 1244px deck (bx1=18, bx2=1262):
            // Width per pedal: 190px, Gap: 28px
            // Pedal 1: Centaur (x=109..299, cx=204)
            // Pedal 2: TS808 (x=327..517, cx=422)
            // Pedal 3: BD-2 (x=545..735, cx=640)
            // Pedal 4: Fluid Rodent Rat (x=763..953, cx=858)
            // Pedal 5: Fuzz Face (x=981..1171, cx=1076)
            // Standardized Row 1 (3 Knobs, 52px spacing, radius 16px): [GAIN/DRIVE] | [TONE/FILTER/SAG] | [LEVEL/VOLUME]
            // Standardized Row 2: Clean Blend (Centaur) & 3D Machined Metal Rocker Switches
            
            // Pedal 1: Fluid Centaur (Center X = 204)
            g_knobs[K_KLON_GAIN].rect   = { 152 - 22, 206, 152 + 22, 292 };
            g_knobs[K_KLON_TREBLE].rect = { 204 - 22, 206, 204 + 22, 292 };
            g_knobs[K_KLON_OUTPUT].rect = { 256 - 22, 206, 256 + 22, 292 };
            g_knobs[K_KLON_BLEND].rect  = { 160 - 38, 320, 160 + 38, 412 };
            g_knobs[K_KLON_HYPER].rect  = { 248 - 38, 320, 248 + 38, 412 };
            g_knobs[K_KLON_ON].rect     = { 0, 0, 0, 0 };

            // Pedal 2: TS808 Screamer (Center X = 422)
            g_knobs[K_TS_DRIVE].rect    = { 370 - 22, 206, 370 + 22, 292 };
            g_knobs[K_TS_TONE].rect     = { 422 - 22, 206, 422 + 22, 292 };
            g_knobs[K_TS_LEVEL].rect    = { 474 - 22, 206, 474 + 22, 292 };
            g_knobs[K_TS_MOD].rect      = { 422 - 44 - 38, 320, 422 - 44 + 38, 412 };
            g_knobs[K_TS_HYPER].rect    = { 422 + 44 - 38, 320, 422 + 44 + 38, 412 };
            g_knobs[K_TS_ON].rect       = { 0, 0, 0, 0 };

            // Pedal 3: BD-2 Blues Driver (Center X = 640)
            g_knobs[K_BD_GAIN].rect     = { 588 - 22, 206, 588 + 22, 292 };
            g_knobs[K_BD_TONE].rect     = { 640 - 22, 206, 640 + 22, 292 };
            g_knobs[K_BD_LEVEL].rect    = { 692 - 22, 206, 692 + 22, 292 };
            g_knobs[K_BD_PHAT].rect     = { 640 - 44 - 38, 320, 640 - 44 + 38, 412 };
            g_knobs[K_BD_HYPER].rect    = { 640 + 44 - 38, 320, 640 + 44 + 38, 412 };
            g_knobs[K_BD_ON].rect       = { 0, 0, 0, 0 };

            // Pedal 4: Fluid Rodent (1978 ProCo Rat) (Center X = 858)
            g_knobs[K_RAT_DIST].rect    = { 806 - 22, 206, 806 + 22, 292 };
            g_knobs[K_RAT_FILTER].rect  = { 858 - 22, 206, 858 + 22, 292 };
            g_knobs[K_RAT_VOL].rect     = { 910 - 22, 206, 910 + 22, 292 };
            g_knobs[K_RAT_TURBO].rect   = { 858 - 44 - 38, 320, 858 - 44 + 38, 412 };
            g_knobs[K_RAT_HYPER].rect   = { 858 + 44 - 38, 320, 858 + 44 + 38, 412 };
            g_knobs[K_RAT_ON].rect      = { 0, 0, 0, 0 };

            // Pedal 5: Fluid Fuzz Face (Center X = 1076)
            g_knobs[K_FUZZ_GAIN].rect       = { 1024 - 22, 206, 1024 + 22, 292 };
            g_knobs[K_FUZZ_SAG].rect        = { 1076 - 22, 206, 1076 + 22, 292 };
            g_knobs[K_FUZZ_LEVEL].rect      = { 1128 - 22, 206, 1128 + 22, 292 };
            g_knobs[K_FUZZ_TRANSISTOR].rect = { 1076 - 44 - 38, 294, 1076 - 44 + 38, 370 };
            g_knobs[K_FUZZ_HYPER].rect      = { 1076 + 44 - 38, 294, 1076 + 44 + 38, 370 };
            g_knobs[K_FUZZ_ON].rect         = { 0, 0, 0, 0 };
            continue;
        }

        if (page == 5) {
            // Page 5: Dual Companion 19" Racks (Upper = Reverb Pink, Lower = Tremolo Orange)
            // Left control faceplate (x: 148..660, span = 512px) hosts the knobs cleanly.
            // Right bay (x: 680..1128) hosts the built-in acoustic cavity and CRT displays.
            
            // Reverb has 7 knobs (indices K_REV_ON..K_REV_MIX) on left control panel
            int revLeft = 148, revRight = 660;
            int revSlotW = (revRight - revLeft) / 7;
            for (int k = 0; k < 7; ++k) {
                int cx = revLeft + k * revSlotW + revSlotW / 2;
                g_knobs[K_REV_ON + k].rect = { cx - 34, 184, cx + 34, 304 };
            }

            // Tremolo has 4 knobs (indices K_VIB_ON..K_VIB_DEPTH) on left control panel of lower rack
            int tremLeft = 148, tremRight = 660;
            int tremSlotW = (tremRight - tremLeft) / 4;
            for (int k = 0; k < 4; ++k) {
                int cx = tremLeft + k * tremSlotW + tremSlotW / 2;
                g_knobs[K_VIB_ON + k].rect = { cx - 44, 474, cx + 44, 604 };
            }
            continue;
        }

        if (page == 4 || page == 6) {
            // Page 4 (Space) and Page 6 (Delay): 8 knobs across upper faceplate between 19" rack ears
            // Usable rack control faceplate: 148px to 1132px (span = 984px, slotW = 123px)
            std::vector<int> indices;
            for (int i = 0; i < NUM_KNOBS; ++i) {
                if (g_knobs[i].pageIdx == page) indices.push_back(i);
            }
            int n = (int)indices.size();
            int slotW = 984 / n;
            for (int k = 0; k < n; ++k) {
                int cx = 148 + k * slotW + slotW / 2;
                g_knobs[indices[k]].rect = { cx - 42, 204, cx + 42, 324 };
            }
            continue;
        }

        // Pages 0, 1, 2: Guitar Amp Heads (Clean, Bassman/JMP Crunch, Friedman Lead)
        // Authentic 2-Row Boutique Faceplate Layout (Row 1: Preamp/EQ, Row 2: Master/Power/Voicing)
        // Usable brushed metal faceplate: 160px to 1120px (span = 960px)
        std::vector<int> indices;
        for (int i = 0; i < NUM_KNOBS; ++i) {
            if (g_knobs[i].pageIdx == page) indices.push_back(i);
        }
        int n = (int)indices.size();
        if (n == 0) continue;

        int faceLeft = 160;
        int faceSpan = 960;

        if (page == 0) {
            // Page 0 (Clean, 11 knobs): 6 knobs on Row 1, 5 knobs on Row 2
            int nRow1 = 6;
            int nRow2 = 5;
            int slotW1 = faceSpan / nRow1; // 160px
            int slotW2 = faceSpan / nRow2; // 192px
            int halfW = 44;

            // Row 1: Volume, Bright, Deep, Bass, Middle, Mid Shift (6 knobs)
            for (int k = 0; k < nRow1; ++k) {
                int cx = faceLeft + k * slotW1 + slotW1 / 2;
                g_knobs[indices[k]].rect = { cx - halfW, 175, cx + halfW, 295 };
            }
            // Row 2: Treble, Glass Air, Presence, Master, Output Trim (5 knobs)
            for (int k = 0; k < nRow2; ++k) {
                int cx = faceLeft + k * slotW2 + slotW2 / 2;
                g_knobs[indices[nRow1 + k]].rect = { cx - halfW, 305, cx + halfW, 425 };
            }
        } else if (page == 1 || page == 2) {
            // Page 1 (Crunch, 10 knobs) & Page 2 (Lead, 10 knobs): 5 knobs on Row 1, 5 knobs on Row 2
            int nPerRow = 5;
            int slotW = faceSpan / nPerRow; // 192px
            int halfW = 44;

            // Row 1: Gain/Drive, Vol, Bass, Middle, Treble (5 knobs)
            for (int k = 0; k < nPerRow; ++k) {
                int cx = faceLeft + k * slotW + slotW / 2;
                g_knobs[indices[k]].rect = { cx - halfW, 175, cx + halfW, 295 };
            }
            // Row 2: Presence, Voicings, Power Tubes, Sag, Dynamic Controls (5 knobs)
            for (int k = 0; k < nPerRow; ++k) {
                int cx = faceLeft + k * slotW + slotW / 2;
                g_knobs[indices[nPerRow + k]].rect = { cx - halfW, 305, cx + halfW, 425 };
            }
        }
    }
}

static void runEngineBlock(const float* inBuf, float* outL, float* outR, uint32_t frames) {
    const bool dlyOn = (g_knobs[K_DLY_ON].curVal > 0.5f);
    const float dlyMix = dlyOn ? g_knobs[K_DLY_MIX].curVal : 0.0f;
    g_engine.setDelayParams(
        g_knobs[K_DLY_TIME].curVal,
        g_knobs[K_DLY_FB].curVal,
        dlyMix,
        g_knobs[K_DLY_DRIVE].curVal,
        g_knobs[K_DLY_FLUTTER].curVal,
        (g_knobs[K_DLY_PINGPONG].curVal > 0.5f) ? 1 : 0,
        g_knobs[K_DLY_DUCK].curVal,
        dlyOn
    );

    // Fluid Space & Space Modulation Params
    static const float s_chamberShapeFactors[11] = {
        0.00f, 0.08f, 0.16f, 0.28f, 0.40f, 0.50f, 0.60f, 0.72f, 0.82f, 0.92f, 1.00f
    };
    int shapeIdx = std::max(0, std::min(10, int(std::round(g_knobs[K_SPACE_SHAPE].curVal))));
    float shapeVal = s_chamberShapeFactors[shapeIdx];

    static const float s_chamberSizeFactors[19] = {
        0.100f, 0.111f, 0.125f, 0.143f, 0.167f, 0.200f, 0.250f, 0.333f, 0.500f,
        1.000f, 2.000f, 3.000f, 4.000f, 5.000f, 6.000f, 7.000f, 8.000f, 9.000f, 10.00f
    };
    int sizeIdx = std::max(0, std::min(18, int(std::round(g_knobs[K_SPACE_SIZE].curVal))));
    float sizeVal = s_chamberSizeFactors[sizeIdx];

    bool modOn = (g_knobs[K_SPACE_MOD_ON].curVal > 0.5f);
    float modRate = g_knobs[K_SPACE_MOD_RATE].curVal;
    float shapeDep = g_knobs[K_SPACE_SHAPE_DEP].curVal * 0.01f;
    float sizeDep = g_knobs[K_SPACE_SIZE_DEP].curVal * 0.01f;
    int modWave = std::max(0, std::min(3, int(std::round(g_knobs[K_SPACE_WAVEFORM].curVal))));
    float spreadDeg = g_knobs[K_SPACE_SPREAD].curVal;

    g_engine.setFluidSpaceParams(shapeVal, sizeVal, modOn, modRate, shapeDep, sizeDep, modWave, spreadDeg);

    // Pure Fluid Dynamics Reverb Space Params
    int revShapeIdx = std::max(0, std::min(10, int(std::round(g_knobs[K_REV_SHAPE].curVal))));
    float revShapeVal = s_chamberShapeFactors[revShapeIdx];

    int revSizeIdx = std::max(0, std::min(18, int(std::round(g_knobs[K_REV_SIZE].curVal))));
    float revSizeVal = s_chamberSizeFactors[revSizeIdx];

    float revLowCut = g_knobs[K_REV_LOWCUT].curVal;
    float revHighCut = g_knobs[K_REV_HIGHCUT].curVal;
    float revDwell = g_knobs[K_REV_DWELL].curVal;
    float revMix = g_knobs[K_REV_MIX].curVal;

    g_engine.setFluidReverbParams(revShapeVal, revSizeVal, revLowCut, revHighCut, revDwell, revMix);

    bool revOn = (g_knobs[K_REV_ON].curVal > 0.5f);
    bool vibOn = (g_knobs[K_VIB_ON].curVal > 0.5f);
    g_engine.setEffectBypasses(revOn, vibOn);

    static const float s_cabSizeFactors[5] = { 0.6f, 0.8f, 1.0f, 1.3f, 1.8f };
    int cabSizeIdx = std::max(0, std::min(4, int(std::round(g_knobs[K_CAB_SIZE].curVal))));
    float cabSizeVol = s_cabSizeFactors[cabSizeIdx];
    bool use432 = (g_knobs[K_TUNING_REF].curVal > 0.5f);
    int cabModel = std::max(0, std::min(8, int(std::round(g_knobs[K_CAB_SIM].curVal))));
    float spkDrive = g_knobs[K_BM_SPKDRV].curVal;

    g_engine.setCabParams(cabModel, cabSizeVol, use432, spkDrive);

    // Guitar Pickup Voicing & Impedance Loading (Rule 33)
    int pickupVoice = std::max(0, std::min(2, int(std::round(g_knobs[K_PICKUP_VOICE].curVal))));
    g_engine.setPickupVoicing(pickupVoice);

    // 5 Independent Series Stompbox Overdrive Pedals (Rule 33 / Rule 34)
    // 1. Fluid Centaur (Klon)
    bool klonHyper = (g_knobs[K_KLON_HYPER].curVal > 0.5f);
    g_engine.setCentaurParams(
        (g_knobs[K_KLON_ON].curVal > 0.5f),
        g_knobs[K_KLON_GAIN].curVal,
        g_knobs[K_KLON_TREBLE].curVal,
        g_knobs[K_KLON_OUTPUT].curVal,
        g_knobs[K_KLON_BLEND].curVal,
        false,
        klonHyper ? 2 : 0 // 0 = (OLD) 18V Klon, 2 = HYPER 27V ADAA
    );

    // 2. TS808 Screamer
    bool tsFat = (g_knobs[K_TS_MOD].curVal > 0.5f);
    bool tsHyper = (g_knobs[K_TS_HYPER].curVal > 0.5f);
    g_engine.setTS808Params(
        (g_knobs[K_TS_ON].curVal > 0.5f),
        g_knobs[K_TS_DRIVE].curVal,
        g_knobs[K_TS_TONE].curVal,
        g_knobs[K_TS_LEVEL].curVal,
        tsFat,
        (tsHyper ? 2 : 0) + (tsFat ? 1 : 0)
    );

    // 3. BD-2 Blues Driver
    bool bdPhat = (g_knobs[K_BD_PHAT].curVal > 0.5f);
    bool bdHyper = (g_knobs[K_BD_HYPER].curVal > 0.5f);
    g_engine.setBD2Params(
        (g_knobs[K_BD_ON].curVal > 0.5f),
        g_knobs[K_BD_LEVEL].curVal,
        g_knobs[K_BD_TONE].curVal,
        g_knobs[K_BD_GAIN].curVal,
        bdPhat,
        (bdHyper ? 2 : 0) + (bdPhat ? 1 : 0)
    );

    // 4. Fluid Rodent (1978 ProCo Rat)
    bool ratTurbo = (g_knobs[K_RAT_TURBO].curVal > 0.5f);
    bool ratHyper = (g_knobs[K_RAT_HYPER].curVal > 0.5f);
    g_engine.setRatParams(
        (g_knobs[K_RAT_ON].curVal > 0.5f),
        g_knobs[K_RAT_DIST].curVal,
        g_knobs[K_RAT_FILTER].curVal,
        g_knobs[K_RAT_VOL].curVal,
        ratTurbo,
        (ratHyper ? 2 : 0) + (ratTurbo ? 1 : 0)
    );

    // 5. Fluid Fuzz Face with Dynamic Amp Output Stage
    bool fuzzSilicon = (g_knobs[K_FUZZ_TRANSISTOR].curVal > 0.5f);
    bool fuzzHyper = (g_knobs[K_FUZZ_HYPER].curVal > 0.5f);
    g_engine.setFuzzFaceParams(
        (g_knobs[K_FUZZ_ON].curVal > 0.5f),
        g_knobs[K_FUZZ_GAIN].curVal,
        g_knobs[K_FUZZ_LEVEL].curVal,
        g_knobs[K_FUZZ_SAG].curVal,
        fuzzSilicon ? 1 : 0,
        (fuzzHyper ? 2 : 0) + (fuzzSilicon ? 1 : 0)
    );

    // Dedicated Bassman Tone Stack Params (Rule 33)
    g_engine.setBassmanToneParams(
        g_knobs[K_BM_BASS].curVal,
        g_knobs[K_BM_MID].curVal,
        g_knobs[K_BM_TREBLE].curVal
    );

    // Dedicated Friedman BE-100 / JCM 800 Lead Channel Params (Rule 33)
    g_engine.setLeadParams(
        g_knobs[K_LEAD_GAIN].curVal,
        g_knobs[K_LEAD_VOL].curVal,
        g_knobs[K_LEAD_BASS].curVal,
        g_knobs[K_LEAD_MID].curVal,
        g_knobs[K_LEAD_TREBLE].curVal,
        g_knobs[K_LEAD_PRESENCE].curVal,
        (g_knobs[K_LEAD_HBE].curVal > 0.5f),
        (g_knobs[K_LEAD_C45].curVal > 0.5f),
        (g_knobs[K_LEAD_SAT].curVal > 0.5f),
        g_knobs[K_LEAD_DEHISS].curVal
    );

    // Euphoria Supreme Glass Clean Tone Matrix Params
    g_engine.setCleanToneParams(
        g_knobs[K_CLEAN_BRIGHT].curVal,
        g_knobs[K_CLEAN_DEEP].curVal,
        g_knobs[K_CLEAN_MIDSHIFT].curVal,
        g_knobs[K_CLEAN_GLASS].curVal,
        g_knobs[K_CLEAN_PRESENCE].curVal,
        g_knobs[K_CLEAN_VOL].curVal,
        g_knobs[K_CLEAN_BASS].curVal,
        g_knobs[K_CLEAN_MID].curVal,
        g_knobs[K_CLEAN_TREBLE].curVal
    );

    g_engine.process(
        inBuf, outL, outR, frames,
        1.0f,
        g_knobs[K_CH_MODE].curVal,
        g_knobs[K_CLEAN_VOL].curVal,
        g_knobs[K_BM_DRIVE].curVal,
        g_knobs[K_BM_VOL].curVal,
        g_knobs[K_CLEAN_BASS].curVal,
        g_knobs[K_CLEAN_MID].curVal,
        g_knobs[K_CLEAN_TREBLE].curVal,
        g_knobs[K_CLEAN_BRIGHT].curVal,
        g_knobs[K_BM_SLOPE].curVal,
        g_knobs[K_BM_TUBES].curVal,
        g_knobs[K_CLEAN_MASTER].curVal,
        g_knobs[K_BM_PRESENCE].curVal,
        g_knobs[K_BM_SAG].curVal,
        g_knobs[K_REV_DWELL].curVal,
        g_knobs[K_REV_MIX].curVal,
        0.0f, // pRevTone
        0.0f, // pRevDrip
        g_knobs[K_VIB_SPEED].curVal,
        g_knobs[K_VIB_DEPTH].curVal,
        g_knobs[K_VIB_MODE].curVal,
        g_knobs[K_QUALITY].curVal,
        float(cabModel),
        spkDrive,
        g_knobs[K_CLEAN_OUT].curVal,
        1.0f, 1.0f, 1.0f, 5.0f,
        false,
        g_knobs[K_BM_BASS].curVal,
        g_knobs[K_BM_MID].curVal,
        g_knobs[K_BM_TREBLE].curVal
    );

    if (g_tunerMute.load()) {
        std::fill(outL, outL + frames, 0.0f);
        if (outR) std::fill(outR, outR + frames, 0.0f);
    }
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

static const GUID s_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

// ============================================================================
// Native ASIO 2.3 Zero-Copy Hardware DMA Engine (Audient EVO / Line 6 / Focusrite / ASIO4ALL)
// ============================================================================
static IASIO*          g_pAsioDriver = nullptr;
static ASIOBufferInfo  g_asioBufInfos[4] = {};
static ASIOSampleType  g_asioInTypes[2]  = { ASIOSTInt32LSB, ASIOSTInt32LSB };
static ASIOSampleType  g_asioOutTypes[2] = { ASIOSTInt32LSB, ASIOSTInt32LSB };
static long            g_asioInCount     = 0;
static long            g_asioOutCount    = 0;
static long            g_asioBufSize     = 32;
static float           g_asioInFloat[4096] = {};
static float           g_asioOutL[4096]    = {};
static float           g_asioOutR[4096]    = {};

static inline float readAsioSampleToFloat(const void* buf, ASIOSampleType type, long idx) noexcept {
    if (!buf) return 0.0f;
    switch (type) {
        case ASIOSTInt32LSB:
            return float(reinterpret_cast<const int32_t*>(buf)[idx]) * (1.0f / 2147483648.0f);
        case ASIOSTInt24LSB: {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(buf) + idx * 3;
            int32_t v = (int32_t(p[0]) << 8) | (int32_t(p[1]) << 16) | (int32_t(p[2]) << 24);
            return float(v) * (1.0f / 2147483648.0f);
        }
        case ASIOSTInt16LSB:
            return float(reinterpret_cast<const int16_t*>(buf)[idx]) * (1.0f / 32768.0f);
        case ASIOSTFloat32LSB:
            return reinterpret_cast<const float*>(buf)[idx];
        case ASIOSTFloat64LSB:
            return float(reinterpret_cast<const double*>(buf)[idx]);
        default:
            return 0.0f;
    }
}

static inline void writeFloatToAsioSample(void* buf, ASIOSampleType type, long idx, float s) noexcept {
    if (!buf) return;
    s = std::clamp(s, -1.0f, 1.0f);
    switch (type) {
        case ASIOSTInt32LSB:
            reinterpret_cast<int32_t*>(buf)[idx] = int32_t(double(s) * 2147483520.0);
            break;
        case ASIOSTInt24LSB: {
            int32_t v = int32_t(double(s) * 8388600.0);
            uint8_t* p = reinterpret_cast<uint8_t*>(buf) + idx * 3;
            p[0] = uint8_t(v & 0xFF);
            p[1] = uint8_t((v >> 8) & 0xFF);
            p[2] = uint8_t((v >> 16) & 0xFF);
            break;
        }
        case ASIOSTInt16LSB:
            reinterpret_cast<int16_t*>(buf)[idx] = int16_t(s * 32760.0f);
            break;
        case ASIOSTFloat32LSB:
            reinterpret_cast<float*>(buf)[idx] = s;
            break;
        case ASIOSTFloat64LSB:
            reinterpret_cast<double*>(buf)[idx] = double(s);
            break;
        default:
            break;
    }
}

static void asioBufferSwitch(long doubleBufferIndex, ASIOBool /*directProcess*/) {
    EuphoriaHyperDSP::FluidMath::disableDenormals();
    const long nFrames = std::min<long>(g_asioBufSize, 4096);
    if (nFrames <= 0) return;

    const int chSelect = g_audioCfg.inChannel;
    float maxIn = 0.0f;
    const void* inBuf0 = (g_asioInCount >= 1) ? g_asioBufInfos[0].buffers[doubleBufferIndex] : nullptr;
    const void* inBuf1 = (g_asioInCount >= 2) ? g_asioBufInfos[1].buffers[doubleBufferIndex] : nullptr;

    for (long i = 0; i < nFrames; ++i) {
        float s0 = readAsioSampleToFloat(inBuf0, g_asioInTypes[0], i);
        float s1 = (g_asioInCount >= 2) ? readAsioSampleToFloat(inBuf1, g_asioInTypes[1], i) : s0;
        float s = (chSelect == 0) ? s0 : ((chSelect == 1) ? s1 : (s0 + s1) * 0.5f);
        g_asioInFloat[i] = s;
        if (std::fabs(s) > maxIn) maxIn = std::fabs(s);
    }

    if (g_testRiffActive.load()) {
        for (long i = 0; i < nFrames; ++i) {
            float rs = g_riffSynth.nextSample();
            g_asioInFloat[i] += rs;
            if (std::fabs(rs) > maxIn) maxIn = std::fabs(rs);
        }
    }
    if (maxIn > 0.0f) g_inMeterPeak.store(maxIn);
    else g_inMeterPeak.store(g_inMeterPeak.load() * 0.95f);

    LARGE_INTEGER qpcStart, qpcEnd, qpcFreq;
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcStart);

    runEngineBlock(g_asioInFloat, g_asioOutL, g_asioOutR, (uint32_t)nFrames);

    g_tuner.writeSamples(g_asioInFloat, (int)nFrames);
    float refPitch = (g_knobs[K_TUNING_REF].curVal > 0.5f) ? 432.0f : 440.0f;
    g_tuner.process(refPitch);

    QueryPerformanceCounter(&qpcEnd);
    double elapsedSec = double(qpcEnd.QuadPart - qpcStart.QuadPart) / double(qpcFreq.QuadPart);
    float timeUs = float(elapsedSec * 1000000.0);
    double blockSec = double(nFrames) / double(SAMPLE_RATE);
    float cpuPct = float((elapsedSec / blockSec) * 100.0);
    g_dspCpuPercent.store(g_dspCpuPercent.load() * 0.90f + cpuPct * 0.10f);
    g_dspTimeUs.store(g_dspTimeUs.load() * 0.90f + timeUs * 0.10f);

    void* outBuf0 = (g_asioOutCount >= 1) ? g_asioBufInfos[g_asioInCount + 0].buffers[doubleBufferIndex] : nullptr;
    void* outBuf1 = (g_asioOutCount >= 2) ? g_asioBufInfos[g_asioInCount + 1].buffers[doubleBufferIndex] : nullptr;
    float peakL = 0.0f, peakR = 0.0f;
    for (long i = 0; i < nFrames; ++i) {
        float l = EuphoriaHyperDSP::FluidMath::fast_tanh(g_asioOutL[i]);
        float r = EuphoriaHyperDSP::FluidMath::fast_tanh(g_asioOutR[i]);
        if (std::fabs(l) > peakL) peakL = std::fabs(l);
        if (std::fabs(r) > peakR) peakR = std::fabs(r);
        writeFloatToAsioSample(outBuf0, g_asioOutTypes[0], i, l);
        if (outBuf1) writeFloatToAsioSample(outBuf1, g_asioOutTypes[1], i, r);
    }
    g_outMeterPeakL.store(peakL);
    g_outMeterPeakR.store(peakR);

    if (g_pAsioDriver) {
        g_pAsioDriver->outputReady();
    }
}

static ASIOTime* asioBufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess) {
    asioBufferSwitch(doubleBufferIndex, directProcess);
    return params;
}

static void asioSampleRateDidChange(ASIOSampleRate sRate) {
    if (sRate > 8000.0) {
        g_tuner.setSampleRate(float(sRate));
    }
}

static long asioMessageCallback(long selector, long value, void* /*message*/, double* /*opt*/) {
    switch (selector) {
        case kAsioSelectorSupported:
            if (value == kAsioResetRequest || value == kAsioEngineVersion ||
                value == kAsioResyncRequest || value == kAsioLatenciesChanged ||
                value == kAsioSupportsTimeInfo || value == kAsioSupportsTimeCode ||
                value == kAsioOverload) {
                return 1L;
            }
            return 0L;
        case kAsioEngineVersion:
            return 2L;
        case kAsioResetRequest:
            g_audioConfigChanged.store(true);
            return 1L;
        case kAsioResyncRequest:
        case kAsioLatenciesChanged:
            return 1L;
        case kAsioSupportsTimeInfo:
        case kAsioSupportsTimeCode:
            return 1L;
        default:
            return 0L;
    }
}

static ASIOCallbacks g_asioCallbacks = {
    &asioBufferSwitch,
    &asioSampleRateDidChange,
    &asioMessageCallback,
    &asioBufferSwitchTimeInfo
};

static DWORD WINAPI audioThreadProc(LPVOID) {
    CoInitialize(nullptr);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    timeBeginPeriod(1);

    // Register with Windows Multimedia Class Scheduler Service (MMCSS) "Pro Audio"
    // to prevent Windows P/E-core thread demotion and scheduler quantum jitter (e.g. on i5-12400F)
    DWORD mmcssTaskIdx = 0;
    HANDLE hMmcss = AvSetMmThreadCharacteristicsA("Pro Audio", &mmcssTaskIdx);
    if (hMmcss) {
        AvSetMmThreadPriority(hMmcss, AVRT_PRIORITY_CRITICAL);
    }
    EuphoriaHyperDSP::FluidMath::disableDenormals();

    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (FAILED(hr) || !pEnum) {
        if (hMmcss) AvRevertMmThreadCharacteristics(hMmcss);
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

    auto closeAsioHardware = [&]() {
        g_asioActive.store(false);
        if (g_pAsioDriver) {
            g_pAsioDriver->stop();
            g_pAsioDriver->disposeBuffers();
            g_pAsioDriver->Release();
            g_pAsioDriver = nullptr;
        }
    };

    auto openAsioHardware = [&]() -> bool {
        closeAsioHardware();
        if (g_asioDriverList.empty()) return false;
        int idx = std::clamp(g_audioCfg.asioDriverIdx, 0, (int)g_asioDriverList.size() - 1);
        const AsioDriverItem& drv = g_asioDriverList[idx];

        IASIO* pAsio = nullptr;
        HRESULT hrAsio = CoCreateInstance(drv.clsid, nullptr, CLSCTX_INPROC_SERVER, drv.clsid, (void**)&pAsio);
        if (FAILED(hrAsio) || !pAsio) return false;

        if (!pAsio->init((void*)g_hwndMain)) {
            pAsio->Release();
            return false;
        }

        long numIn = 0, numOut = 0;
        if (pAsio->getChannels(&numIn, &numOut) != ASE_OK || numOut <= 0) {
            pAsio->Release();
            return false;
        }

        ASIOSampleRate curRate = 48000.0;
        if (pAsio->canSampleRate(48000.0) == ASE_OK) {
            pAsio->setSampleRate(48000.0);
        }
        if (pAsio->getSampleRate(&curRate) == ASE_OK && curRate > 8000.0) {
            g_tuner.setSampleRate(float(curRate));
        }

        long minSize = 16, maxSize = 2048, prefSize = 64, gran = 0;
        if (pAsio->getBufferSize(&minSize, &maxSize, &prefSize, &gran) != ASE_OK) {
            minSize = 16; maxSize = 2048; prefSize = 64;
        }
        long reqSize = std::clamp<long>(g_audioCfg.bufferFrames, std::max<long>(16, minSize), std::min<long>(4096, maxSize));
        g_asioBufSize = (reqSize > 0) ? reqSize : ((prefSize > 0) ? prefSize : 64);

        g_asioInCount  = std::min<long>(numIn, 2);
        g_asioOutCount = std::min<long>(numOut, 2);
        long totalChans = g_asioInCount + g_asioOutCount;

        for (long c = 0; c < g_asioInCount; ++c) {
            g_asioBufInfos[c].isInput = ASIOTrue;
            g_asioBufInfos[c].channelNum = c;
            g_asioBufInfos[c].buffers[0] = nullptr;
            g_asioBufInfos[c].buffers[1] = nullptr;
            ASIOChannelInfo ci = {};
            ci.channel = c;
            ci.isInput = ASIOTrue;
            if (pAsio->getChannelInfo(&ci) == ASE_OK) g_asioInTypes[c] = ci.type;
        }
        for (long c = 0; c < g_asioOutCount; ++c) {
            long slot = g_asioInCount + c;
            g_asioBufInfos[slot].isInput = ASIOFalse;
            g_asioBufInfos[slot].channelNum = c;
            g_asioBufInfos[slot].buffers[0] = nullptr;
            g_asioBufInfos[slot].buffers[1] = nullptr;
            ASIOChannelInfo ci = {};
            ci.channel = c;
            ci.isInput = ASIOFalse;
            if (pAsio->getChannelInfo(&ci) == ASE_OK) g_asioOutTypes[c] = ci.type;
        }

        if (pAsio->createBuffers(g_asioBufInfos, totalChans, g_asioBufSize, &g_asioCallbacks) != ASE_OK) {
            // Retry with driver's preferred buffer size if custom size was rejected
            g_asioBufSize = (prefSize > 0) ? prefSize : 64;
            if (pAsio->createBuffers(g_asioBufInfos, totalChans, g_asioBufSize, &g_asioCallbacks) != ASE_OK) {
                pAsio->Release();
                return false;
            }
        }

        g_pAsioDriver = pAsio;
        if (pAsio->start() != ASE_OK) {
            closeAsioHardware();
            return false;
        }

        g_asioActive.store(true);
        return true;
    };

    auto closeAudioHardware = [&]() {
        closeAsioHardware();
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

        // 0. Try Native ASIO Direct Hardware first if DriverMode == 0 (ASIO Direct)
        if (g_audioCfg.driverMode == 0 && openAsioHardware()) {
            return true;
        }

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

        // Compute requested buffer duration in 100-nanosecond units (16..512 frames @ 48 kHz)
        REFERENCE_TIME hnsReq = (REFERENCE_TIME)((double(g_audioCfg.bufferFrames) * 10000000.0) / 48000.0 + 0.5);
        if (hnsReq < 3333) hnsReq = 3333; // 16 frames = 0.333 ms

        REFERENCE_TIME hnsDefaultOut = 0, hnsMinOut = 0;
        if (SUCCEEDED(pAudioClientOut->GetDevicePeriod(&hnsDefaultOut, &hnsMinOut)) && hnsMinOut > 0) {
            if (hnsReq < hnsMinOut) hnsReq = hnsMinOut;
        }

        hEventOut = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        HRESULT hrInitOut = E_FAIL;
        if (g_audioCfg.driverMode == 1) {
            // Try WASAPI Exclusive Mode first for direct kernel streaming
            hrInitOut = pAudioClientOut->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsReq, hnsReq, pWfxOut, nullptr);
        }
        if (FAILED(hrInitOut)) {
            hrInitOut = pAudioClientOut->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsReq, 0, pWfxOut, nullptr);
        }
        if (FAILED(hrInitOut)) return false;
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
            REFERENCE_TIME hnsReqIn = hnsReq;
            REFERENCE_TIME hnsDefaultIn = 0, hnsMinIn = 0;
            if (SUCCEEDED(pAudioClientIn->GetDevicePeriod(&hnsDefaultIn, &hnsMinIn)) && hnsMinIn > 0) {
                if (hnsReqIn < hnsMinIn) hnsReqIn = hnsMinIn;
            }
            hEventIn = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            HRESULT hrInitIn = E_FAIL;
            if (g_audioCfg.driverMode == 1) {
                hrInitIn = pAudioClientIn->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsReqIn, hnsReqIn, pWfxIn, nullptr);
            }
            if (FAILED(hrInitIn)) {
                hrInitIn = pAudioClientIn->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsReqIn, 0, pWfxIn, nullptr);
            }
            if (SUCCEEDED(hrInitIn)) {
                pAudioClientIn->SetEventHandle(hEventIn);
                pAudioClientIn->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
                pAudioClientIn->GetBufferSize(&bufIn);
            }
        }

        // ToneDirect Zero-Pre-Fill Direct Monitoring: do NOT pre-fill 1024 samples (21.3ms) of silence.
        // Capture packets drain immediately into the render block on the very first hardware interrupt.

        // Synchronize tuner engine to true negotiated hardware sample rate
        float hwSampleRate = 48000.0f;
        if (pWfxIn && pWfxIn->nSamplesPerSec > 0) {
            hwSampleRate = float(pWfxIn->nSamplesPerSec);
        } else if (pWfxOut && pWfxOut->nSamplesPerSec > 0) {
            hwSampleRate = float(pWfxOut->nSamplesPerSec);
        }
        g_tuner.setSampleRate(hwSampleRate);

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
        if (g_asioOpenPanelReq.exchange(false)) {
            if (g_pAsioDriver) {
                g_pAsioDriver->controlPanel();
            } else if (!g_asioDriverList.empty()) {
                int idx = std::clamp(g_audioCfg.asioDriverIdx, 0, (int)g_asioDriverList.size() - 1);
                IASIO* pTmp = nullptr;
                if (SUCCEEDED(CoCreateInstance(g_asioDriverList[idx].clsid, nullptr, CLSCTX_INPROC_SERVER, g_asioDriverList[idx].clsid, (void**)&pTmp)) && pTmp) {
                    pTmp->init((void*)g_hwndMain);
                    pTmp->controlPanel();
                    pTmp->Release();
                }
            }
        }
        if (g_asioActive.load()) {
            MSG comMsg;
            while (PeekMessageA(&comMsg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&comMsg);
                DispatchMessageA(&comMsg);
            }
            Sleep(5);
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
                                  IsEqualGUID(((WAVEFORMATEXTENSIBLE*)pWfxIn)->SubFormat, s_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)));

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

                        if (g_recSampleActive.load()) {
                            std::lock_guard<std::mutex> lock(g_recSampleMutex);
                            for (UINT32 k = 0; k < framesToProc; ++k) {
                                g_recSampleBuf.push_back(inSamples[k]);
                            }
                            if (g_recSampleBuf.size() >= (size_t)(SAMPLE_RATE * 6)) {
                                g_recSampleActive.store(false);
                                g_recSampleDoneTime = GetTickCount64();
                                saveWavFile("scratch/guitar_hiss_capture.wav", g_recSampleBuf.data(), g_recSampleBuf.size(), SAMPLE_RATE);
                            }
                        }
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

                // ToneDirect Ultra-Tight Clock Drift Lock: never allow more than 2 blocks of ringbuffer queue
                size_t maxCushion = std::max((size_t)128, (size_t)framesNeeded * 2);
                if (avail > maxCushion) {
                    g_ringBuffer.drop(avail - framesNeeded);
                }

                // Inject test riff synth if enabled
                if (g_testRiffActive.load()) {
                    float riffMax = 0.0f;
                    for (UINT32 i = 0; i < framesNeeded; ++i) {
                        float rs = g_riffSynth.nextSample();
                        inFloat[i] += rs;
                        if (std::fabs(rs) > riffMax) riffMax = std::fabs(rs);
                    }
                    if (riffMax > g_inMeterPeak.load()) {
                        g_inMeterPeak.store(riffMax);
                    }
                }

                // Process through full DSP Amp & Rack engine with hardware telemetry
                LARGE_INTEGER qpcStart, qpcEnd, qpcFreq;
                QueryPerformanceFrequency(&qpcFreq);
                QueryPerformanceCounter(&qpcStart);

                runEngineBlock(inFloat.data(), outFloatL.data(), outFloatR.data(), framesNeeded);

                // Update live precision strobe tuner from incoming guitar signal
                g_tuner.writeSamples(inFloat.data(), framesNeeded);
                float refPitch = (g_knobs[K_TUNING_REF].curVal > 0.5f) ? 432.0f : 440.0f;
                g_tuner.process(refPitch);

                QueryPerformanceCounter(&qpcEnd);

                double elapsedSec = double(qpcEnd.QuadPart - qpcStart.QuadPart) / double(qpcFreq.QuadPart);
                float timeUs = float(elapsedSec * 1000000.0);
                double blockSec = double(framesNeeded) / double(SAMPLE_RATE);
                float cpuPct = float((elapsedSec / blockSec) * 100.0);

                float oldCpu = g_dspCpuPercent.load();
                g_dspCpuPercent.store(oldCpu * 0.90f + cpuPct * 0.10f);
                float oldTime = g_dspTimeUs.load();
                g_dspTimeUs.store(oldTime * 0.90f + timeUs * 0.10f);

                int qMode = int(std::round(g_knobs[K_QUALITY].curVal));
                int internalRate = (qMode == 3) ? 384000 : ((qMode == 2) ? 192000 : ((qMode == 1) ? 96000 : 48000));
                g_dspInternalRate.store(internalRate);

                BYTE* pOutData = nullptr;
                if (SUCCEEDED(pRenderClient->GetBuffer(framesNeeded, &pOutData))) {
                    const int outChannels = pWfxOut->nChannels;
                    const bool isFloat = (pWfxOut->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                                         (pWfxOut->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                                          IsEqualGUID(((WAVEFORMATEXTENSIBLE*)pWfxOut)->SubFormat, s_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)));

                    float peakL = 0.0f, peakR = 0.0f;

                    if (isFloat) {
                        float* pFloat = reinterpret_cast<float*>(pOutData);
                        for (UINT32 i = 0; i < framesNeeded; ++i) {
                            float l = EuphoriaHyperDSP::FluidMath::fast_tanh(outFloatL[i]);
                            float r = EuphoriaHyperDSP::FluidMath::fast_tanh(outFloatR[i]);
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
                            float l = EuphoriaHyperDSP::FluidMath::fast_tanh(outFloatL[i]);
                            float r = EuphoriaHyperDSP::FluidMath::fast_tanh(outFloatR[i]);
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
    if (hMmcss) AvRevertMmThreadCharacteristics(hMmcss);
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

static void drawCornerProtector3D(HDC hdc, int x, int y, int w, int h, int cornerType, COLORREF metalCol, COLORREF hiCol) {
    // cornerType: 0 = Top-Left, 1 = Top-Right, 2 = Bottom-Left, 3 = Bottom-Right
    fillRoundRect(hdc, x, y, x + w, y + h, 14, metalCol, hiCol, 2);
    HPEN hiPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HGDIOBJ op = SelectObject(hdc, hiPen);
    if (cornerType == 0) {
        Arc(hdc, x + 2, y + 2, x + w * 2, y + h * 2, x + w, y + 2, x + 2, y + h);
    } else if (cornerType == 1) {
        Arc(hdc, x - w, y + 2, x + w - 2, y + h * 2, x + 2, y + 2, x + w - 2, y + h);
    } else if (cornerType == 2) {
        Arc(hdc, x + 2, y - h, x + w * 2, y + h - 2, x + 2, y + h - 2, x + w, y + h - 2);
    } else {
        Arc(hdc, x - w, y - h, x + w - 2, y + h - 2, x + w - 2, y + 2, x + 2, y + h - 2);
    }
    SelectObject(hdc, op);
    DeleteObject(hiPen);

    int rx = (cornerType == 0 || cornerType == 2) ? (x + 9) : (x + w - 9);
    int ry = (cornerType == 0 || cornerType == 1) ? (y + 9) : (y + h - 9);
    fillRoundRect(hdc, rx - 3, ry - 3, rx + 3, ry + 3, 6, RGB(30, 41, 59), RGB(226, 232, 240), 1);
}

static void drawFacetedJewelLamp(HDC hdc, int cx, int cy, int r, COLORREF jewelBaseCol, COLORREF glowCol, COLORREF rimCol) {
    // 1. Radiant translucent glow halo
    for (int gr = r + 8; gr >= r; gr -= 2) {
        fillRoundRect(hdc, cx - gr, cy - gr, cx + gr, cy + gr, gr * 2, glowCol, glowCol, 1);
    }
    // 2. Heavy chrome knurled mounting bezel
    fillRoundRect(hdc, cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4, (r + 4) * 2, RGB(203, 213, 225), RGB(71, 85, 105), 2);
    for (int k = 0; k < 8; ++k) {
        float a = k * 0.785398f;
        int kx = cx + int(std::cos(a) * (r + 3));
        int ky = cy + int(std::sin(a) * (r + 3));
        fillRoundRect(hdc, kx - 1, ky - 1, kx + 1, ky + 1, 2, RGB(51, 65, 85), RGB(51, 65, 85), 1);
    }
    // 3. Faceted glass jewel lens (8 triangular facets radiating from center)
    fillRoundRect(hdc, cx - r, cy - r, cx + r, cy + r, r * 2, jewelBaseCol, rimCol, 1);
    for (int f = 0; f < 8; ++f) {
        float a1 = f * 0.785398f;
        float a2 = (f + 1) * 0.785398f;
        int x1 = cx + int(std::cos(a1) * (r - 2));
        int y1 = cy + int(std::sin(a1) * (r - 2));
        int x2 = cx + int(std::cos(a2) * (r - 2));
        int y2 = cy + int(std::sin(a2) * (r - 2));
        POINT tri[3] = { { cx, cy }, { x1, y1 }, { x2, y2 } };
        COLORREF facetCol = (f % 2 == 0) ? jewelBaseCol : glowCol;
        HBRUSH fbr = CreateSolidBrush(facetCol);
        HPEN fpen = CreatePen(PS_SOLID, 1, rimCol);
        HGDIOBJ ob = SelectObject(hdc, fbr);
        HGDIOBJ op = SelectObject(hdc, fpen);
        Polygon(hdc, tri, 3);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(fbr);
        DeleteObject(fpen);
    }
    // 4. Brilliant specular sparkle highlight
    fillRoundRect(hdc, cx - r / 2 - 2, cy - r / 2 - 2, cx - r / 2 + 3, cy - r / 2 + 3, 6, RGB(255, 255, 255), RGB(255, 255, 255), 1);
}

static void drawBasketweaveGrille(HDC hdc, int x1, int y1, int x2, int y2, COLORREF darkYarn, COLORREF lightYarn) {
    fillRoundRect(hdc, x1, y1, x2, y2, 8, darkYarn, RGB(15, 15, 15), 2);
    HPEN lightPen = CreatePen(PS_SOLID, 2, lightYarn);
    HPEN darkPen  = CreatePen(PS_SOLID, 2, darkYarn);
    HGDIOBJ oldP  = SelectObject(hdc, lightPen);

    for (int y = y1 + 4; y < y2 - 4; y += 8) {
        for (int x = x1 + 4; x < x2 - 4; x += 16) {
            bool oddRow = (((y - y1) / 8) % 2 == 1);
            int blockX = oddRow ? (x + 8) : x;
            if (blockX + 8 < x2 - 4) {
                SelectObject(hdc, lightPen);
                MoveToEx(hdc, blockX, y + 2, nullptr);
                LineTo(hdc, blockX + 8, y + 2);
                MoveToEx(hdc, blockX, y + 5, nullptr);
                LineTo(hdc, blockX + 8, y + 5);

                SelectObject(hdc, darkPen);
                MoveToEx(hdc, blockX + 8, y, nullptr);
                LineTo(hdc, blockX + 8, y + 8);
            }
        }
    }
    SelectObject(hdc, oldP);
    DeleteObject(lightPen);
    DeleteObject(darkPen);

    // Deep top drop shadow across the top baffle edge
    fillRoundRect(hdc, x1 + 2, y1 + 2, x2 - 2, y1 + 8, 4, RGB(8, 8, 8), RGB(8, 8, 8), 1);
}

// 3D Skeuomorphic Rotary & Chicken-Head Knobs with Conical Highlights and Drop Shadows
static void drawStylizedKnob(HDC hdc, const KnobCtrl& k, int pageStyle) {
    int cx = (k.rect.left + k.rect.right) / 2;

    if (pageStyle == 3 && k.isStepped) {
        // --------------------------------------------------------------------
        // 3D Machined Metal Rocker/Toggle Switch (FAT MOD, PHAT MOD, TRANSISTOR)
        // --------------------------------------------------------------------
        int step = std::max(0, std::min(k.steps - 1, int(std::round(k.curVal))));
        bool active = (step > 0);

        // Switch color theme per pedal (Centaur: <320, TS808: <514, BD-2: <752, Rodent: <992, Fuzz Face: >=992)
        COLORREF labelCol = (cx < 320) ? RGB(254, 240, 138)
                          : ((cx < 514) ? RGB(167, 243, 208)
                          : ((cx < 752) ? RGB(254, 240, 138)
                          : ((cx < 992) ? RGB(254, 240, 138)
                          : RGB(254, 202, 202))));
        COLORREF ledCol   = (cx < 320) ? (active ? RGB(239, 68, 68) : RGB(60, 15, 15))
                          : ((cx < 514) ? (active ? RGB(34, 197, 94) : RGB(20, 60, 35))
                          : ((cx < 752) ? (active ? RGB(59, 130, 246) : RGB(20, 40, 80))
                          : ((cx < 992) ? (active ? RGB(239, 68, 68) : RGB(60, 12, 12))
                          : (active ? RGB(56, 189, 248) : RGB(20, 40, 60)))));
        COLORREF ledGlow  = (cx < 320) ? (active ? RGB(254, 202, 202) : RGB(80, 25, 25))
                          : ((cx < 514) ? (active ? RGB(187, 247, 208) : RGB(40, 90, 55))
                          : ((cx < 752) ? (active ? RGB(191, 219, 254) : RGB(35, 65, 120))
                          : ((cx < 992) ? (active ? RGB(254, 202, 202) : RGB(80, 20, 20))
                          : (active ? RGB(186, 230, 253) : RGB(35, 70, 100)))));

        // 1. Switch Label at top
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, labelCol);
        HFONT fontSw = CreateFontA(9, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        HGDIOBJ oldSwFont = SelectObject(hdc, fontSw);
        RECT tr = { cx - 40, k.rect.top + 2, cx + 40, k.rect.top + 16 };
        DrawTextA(hdc, k.label, -1, &tr, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 2. Beveled Dark Machined Mounting Faceplate
        int swTop = k.rect.top + 18;
        int swBottom = k.rect.top + 50;
        int swHalfW = 34;
        fillRoundRect(hdc, cx - swHalfW + 2, swTop + 2, cx + swHalfW + 2, swBottom + 2, 6, RGB(8, 10, 12), RGB(8, 10, 12), 1); // shadow
        fillRoundRect(hdc, cx - swHalfW, swTop, cx + swHalfW, swBottom, 6, RGB(22, 25, 30), RGB(71, 85, 105), 2); // plate

        // 4 Miniature Corner Hex Screws on plate
        fillRoundRect(hdc, cx - swHalfW + 3, swTop + 3, cx - swHalfW + 6, swTop + 6, 3, RGB(148, 163, 184), RGB(100, 116, 139), 1);
        fillRoundRect(hdc, cx + swHalfW - 6, swTop + 3, cx + swHalfW - 3, swTop + 6, 3, RGB(148, 163, 184), RGB(100, 116, 139), 1);
        fillRoundRect(hdc, cx - swHalfW + 3, swBottom - 6, cx - swHalfW + 6, swBottom - 3, 3, RGB(148, 163, 184), RGB(100, 116, 139), 1);
        fillRoundRect(hdc, cx + swHalfW - 6, swBottom - 6, cx + swHalfW - 3, swBottom - 3, 3, RGB(148, 163, 184), RGB(100, 116, 139), 1);

        // 3. Recessed Toggle Track Slot
        int slotLeft = cx - 18, slotRight = cx + 18;
        int slotTop = swTop + 7, slotBottom = swBottom - 7;
        fillRoundRect(hdc, slotLeft, slotTop, slotRight, slotBottom, 8, RGB(10, 12, 16), RGB(30, 41, 59), 1);

        // 4. 3D Chrome Rocker / Toggle Bat
        int batCenter = active ? (cx + 8) : (cx - 8);
        int batHalfW = 8;
        fillRoundRect(hdc, batCenter - batHalfW + 1, slotTop + 1, batCenter + batHalfW + 1, slotBottom, 6, RGB(10, 10, 12), RGB(10, 10, 12), 1); // bat shadow
        fillRoundRect(hdc, batCenter - batHalfW, slotTop, batCenter + batHalfW, slotBottom - 1, 6, RGB(203, 213, 225), RGB(255, 255, 255), 2); // chrome bat

        // Specular highlight line on chrome bat
        HPEN batSpecPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        HGDIOBJ oldSp = SelectObject(hdc, batSpecPen);
        MoveToEx(hdc, batCenter - 2, slotTop + 3, nullptr);
        LineTo(hdc, batCenter - 2, slotBottom - 4);
        SelectObject(hdc, oldSp);
        DeleteObject(batSpecPen);

        // 5. LED Indicator Lamp (Left side for Step 0, Right side for Step 1)
        int ledX = active ? (cx + 25) : (cx - 25);
        int ledY = (swTop + swBottom) / 2;
        fillRoundRect(hdc, ledX - 3, ledY - 3, ledX + 3, ledY + 3, 6, ledCol, ledGlow, 1);
        if (active) {
            fillRoundRect(hdc, ledX - 1, ledY - 1, ledX + 1, ledY + 1, 2, RGB(255, 255, 255), RGB(255, 255, 255), 1);
        }

        // 6. Inset LED Value Pill
        int pillHalfW = 38;
        RECT vr = { cx - pillHalfW, k.rect.top + 54, cx + pillHalfW, k.rect.top + 70 };
        fillRoundRect(hdc, vr.left, vr.top, vr.right, vr.bottom, 4, RGB(4, 16, 20), active ? ledGlow : RGB(30, 41, 59), 1);

        char valBuf[64];
        std::snprintf(valBuf, sizeof(valBuf), "%s", k.stepLabels[step]);
        SetTextColor(hdc, active ? labelCol : RGB(148, 163, 184));
        DrawTextA(hdc, valBuf, -1, &vr, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        SelectObject(hdc, oldSwFont);
        DeleteObject(fontSw);
        return;
    }

    // Dynamic Knob Radius & Layout per Page and Control Type
    int r = 24;
    int cy = (k.rect.top + k.rect.bottom) / 2 - 16;
    int pillHalfW = 34;

    if (pageStyle == 0 || pageStyle == 1 || pageStyle == 2) {
        // Pages 0, 1, 2: 2-Row Amp Faceplates (160px..192px slots per knob)
        r = 23;
        cy = k.rect.top + 32;
        pillHalfW = 40;
    } else if (pageStyle == 3) {
        // Page 3: Stompboxes
        if (k.rect.top > 300) {
            // Row 2: Blend knob
            r = 16;
            cy = k.rect.top + 22;
            pillHalfW = 26;
        } else {
            // Row 1: 3 knobs across 190px pedal (spaced 52px apart)
            r = 16;
            cy = k.rect.top + 20;
            pillHalfW = 18;
        }
    } else if (pageStyle == 4) {
        // Page 4: Space (8 knobs across 984px, slot 123px)
        r = 21;
        cy = k.rect.top + 32;
        pillHalfW = 38;
    } else if (pageStyle == 5) {
        // Page 5: Dual Companion Racks
        if (k.rect.top < 350) {
            // Upper Rack: Reverb (7 knobs across 512px, slot 73px)
            r = 18;
            cy = k.rect.top + 32;
            pillHalfW = 29;
        } else {
            // Lower Rack: Tremolo (4 knobs across 512px, slot 128px)
            r = 22;
            cy = k.rect.top + 38;
            pillHalfW = 34;
        }
    } else if (pageStyle == 6) {
        // Page 6: Delay (8 knobs across 984px, slot 123px)
        r = 21;
        cy = k.rect.top + 32;
        pillHalfW = 38;
    }

    float norm = (k.curVal - k.minVal) / (k.maxVal - k.minVal);
    norm = std::max(0.0f, std::min(1.0f, norm));

    // 270 degree sweep rotated anti-clockwise by 90 degrees:
    // norm = 0.5 (halfway / 12 o'clock) points vertically straight UP (-PI/2)
    const float startAngle = -3.9269908f; // -225 deg (South-West 7:30)
    const float totalSweep = 4.712389f;  // 270 deg sweep
    float angle = startAngle + norm * totalSweep;

    // 1. Knob Cast Drop Shadow onto faceplate
    HBRUSH shadowBr = CreateSolidBrush(RGB(4, 12, 10));
    HPEN nullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    HGDIOBJ ob = SelectObject(hdc, shadowBr);
    HGDIOBJ op = SelectObject(hdc, nullPen);
    Ellipse(hdc, cx - r + 2, cy - r + 3, cx + r + 3, cy + r + 4);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(shadowBr);
    DeleteObject(nullPen);

    // 2. Calibration Ticks around the perimeter
    int tickLen1 = r + ((r <= 17) ? 2 : 3);
    int tickLen2 = r + ((r <= 17) ? 4 : 7);
    int tickW    = (r <= 17) ? 1 : 2;

    for (int t = 0; t <= 10; ++t) {
        float ta = startAngle + (t / 10.0f) * totalSweep;
        int tx1 = cx + int(std::cos(ta) * tickLen1);
        int ty1 = cy + int(std::sin(ta) * tickLen1);
        int tx2 = cx + int(std::cos(ta) * tickLen2);
        int ty2 = cy + int(std::sin(ta) * tickLen2);

        // Tick drop shadow
        HPEN tickShadowPen = CreatePen(PS_SOLID, tickW, RGB(2, 10, 8));
        HGDIOBJ ops = SelectObject(hdc, tickShadowPen);
        MoveToEx(hdc, tx1 + 1, ty1 + 1, nullptr);
        LineTo(hdc, tx2 + 1, ty2 + 1);
        SelectObject(hdc, ops);
        DeleteObject(tickShadowPen);

        COLORREF tickCol = (pageStyle == 0) ? RGB(245, 158, 11)   // Gold ticks
                         : (pageStyle == 1) ? RGB(254, 215, 170)  // Rose brass ticks
                         : (pageStyle == 2) ? RGB(248, 113, 113)  // Bright Crimson ticks (Lead)
                         : (pageStyle == 3) ? ((cx < 274) ? RGB(251, 191, 36)
                                            : ((cx < 514) ? RGB(134, 239, 172)
                                            : ((cx < 752) ? RGB(147, 197, 253)
                                            : ((cx < 992) ? RGB(254, 240, 138)
                                            : RGB(252, 165, 165)))))
                         : (pageStyle == 4) ? RGB(34, 211, 238)   // Neon cyan ticks (Space)
                         : (pageStyle == 5) ? RGB(244, 114, 182)  // Rose pink ticks (Reverb)
                                            : RGB(251, 191, 36);  // Champagne gold ticks (Delay)
        HPEN tickPen = CreatePen(PS_SOLID, tickW, tickCol);
        HGDIOBJ op2 = SelectObject(hdc, tickPen);
        MoveToEx(hdc, tx1, ty1, nullptr);
        LineTo(hdc, tx2, ty2);
        SelectObject(hdc, op2);
        DeleteObject(tickPen);
    }

    if (g_assets3D.imgKnobStrip) {
        Gdiplus::Graphics g(hdc);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        int frameIdx = std::min(63, std::max(0, int(norm * 63.0f)));
        int frameH = 128;
        int frameW = 128;
        int knobDiam = r * 2;
        g.DrawImage(g_assets3D.imgKnobStrip,
            Gdiplus::Rect(cx - r, cy - r, knobDiam, knobDiam),
            0, frameIdx * frameH, frameW, frameH,
            Gdiplus::UnitPixel);
    } else if (pageStyle == 1) {
        // Page 1: 3D Vintage Chicken-Head Pointer (Euphoria '59/'68 Crunch)
        fillRoundRect(hdc, cx - r, cy - r, cx + r, cy + r, r * 2, RGB(45, 15, 6), RGB(180, 83, 9), 2);
        HPEN hiPen = CreatePen(PS_SOLID, 2, RGB(249, 115, 22));
        HGDIOBJ oph = SelectObject(hdc, hiPen);
        Arc(hdc, cx - r, cy - r, cx + r, cy + r, cx + r, cy, cx, cy - r);
        SelectObject(hdc, oph);
        DeleteObject(hiPen);

        int px = cx + int(std::cos(angle) * (r + 6));
        int py = cy + int(std::sin(angle) * (r + 6));
        int bx1 = cx + int(std::cos(angle + 2.35f) * (r * 0.45f));
        int by1 = cy + int(std::sin(angle + 2.35f) * (r * 0.45f));
        int bx2 = cx + int(std::cos(angle - 2.35f) * (r * 0.45f));
        int by2 = cy + int(std::sin(angle - 2.35f) * (r * 0.45f));

        // Chicken-head drop shadow
        POINT triS[3] = { { px + 2, py + 3 }, { bx1 + 2, by1 + 3 }, { bx2 + 2, by2 + 3 } };
        HBRUSH brS = CreateSolidBrush(RGB(15, 5, 2));
        HGDIOBJ obs = SelectObject(hdc, brS);
        Polygon(hdc, triS, 3);
        SelectObject(hdc, obs);
        DeleteObject(brS);

        // Chicken-head body
        POINT tri[3] = { { px, py }, { bx1, by1 }, { bx2, by2 } };
        HBRUSH br = CreateSolidBrush(RGB(154, 52, 18));
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(254, 215, 170));
        ob = SelectObject(hdc, br);
        op = SelectObject(hdc, pen);
        Polygon(hdc, tri, 3);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        // Raised center ridge
        HPEN spinePen = CreatePen(PS_SOLID, 2, RGB(254, 240, 138));
        HGDIOBJ opspine = SelectObject(hdc, spinePen);
        MoveToEx(hdc, cx, cy, nullptr);
        LineTo(hdc, px - int(std::cos(angle) * 3), py - int(std::sin(angle) * 3));
        SelectObject(hdc, opspine);
        DeleteObject(spinePen);

        // Center brass screw cap
        fillRoundRect(hdc, cx - 7, cy - 7, cx + 7, cy + 7, 14, RGB(251, 191, 36), RGB(124, 45, 18), 1);
        HPEN slotPen = CreatePen(PS_SOLID, 2, RGB(67, 20, 7));
        HGDIOBJ opslot = SelectObject(hdc, slotPen);
        MoveToEx(hdc, cx - 3, cy - 3, nullptr);
        LineTo(hdc, cx + 3, cy + 3);
        SelectObject(hdc, opslot);
        DeleteObject(slotPen);
    } else {
        // 3D Fluted Barrel Knob with Spun-Metal Inset Dial
        COLORREF bodyCol = (pageStyle == 0) ? RGB(4, 47, 46)
                         : (pageStyle == 2) ? RGB(45, 10, 15)   // Deep Crimson/Black for Lead
                         : (pageStyle == 3) ? ((cx < 274) ? RGB(65, 18, 14)
                                            : ((cx < 514) ? RGB(18, 52, 28)
                                            : ((cx < 752) ? RGB(14, 28, 55)
                                            : ((cx < 992) ? RGB(16, 16, 18)
                                            : RGB(35, 18, 20))))) // Custom per pedal!
                         : (pageStyle == 4) ? RGB(8, 51, 68)    // Celestial Cyan for Fluid Space
                         : (pageStyle == 5) ? RGB(68, 10, 40)   // Velvet Wine for Reverb & Vib
                                            : RGB(30, 27, 75);   // Royal Indigo for Tape Delay
        COLORREF rimCol  = (pageStyle == 0) ? RGB(245, 158, 11)
                         : (pageStyle == 2) ? RGB(220, 38, 38)
                         : (pageStyle == 3) ? ((cx < 274) ? RGB(217, 119, 6)
                                            : ((cx < 514) ? RGB(34, 197, 94)
                                            : ((cx < 752) ? RGB(37, 99, 235)
                                            : ((cx < 992) ? RGB(140, 140, 145)
                                            : RGB(220, 38, 38)))))
                         : (pageStyle == 4) ? RGB(6, 182, 212)
                         : (pageStyle == 5) ? RGB(244, 114, 182)
                                            : RGB(245, 158, 11);
        COLORREF hiCol   = (pageStyle == 0) ? RGB(52, 211, 153)
                         : (pageStyle == 2) ? RGB(248, 113, 113)
                         : (pageStyle == 3) ? ((cx < 274) ? RGB(251, 191, 36)
                                            : ((cx < 514) ? RGB(134, 239, 172)
                                            : ((cx < 752) ? RGB(96, 165, 250)
                                            : ((cx < 992) ? RGB(239, 68, 68)
                                            : RGB(248, 113, 113)))))
                         : (pageStyle == 4) ? RGB(103, 232, 249)
                         : (pageStyle == 5) ? RGB(251, 113, 133)
                                            : RGB(251, 191, 36);
        COLORREF capCol  = (pageStyle == 0) ? RGB(6, 78, 59)
                         : (pageStyle == 2) ? RGB(75, 15, 25)
                         : (pageStyle == 3) ? ((cx < 274) ? RGB(115, 30, 22)
                                            : ((cx < 514) ? RGB(28, 75, 40)
                                            : ((cx < 752) ? RGB(20, 40, 80)
                                            : ((cx < 992) ? RGB(8, 8, 10)
                                            : RGB(55, 18, 22)))))
                         : (pageStyle == 4) ? RGB(14, 116, 144)
                         : (pageStyle == 5) ? RGB(136, 19, 55)
                                            : RGB(67, 56, 202);
        COLORREF ptrCol  = (pageStyle == 0) ? RGB(251, 191, 36)
                         : (pageStyle == 2) ? RGB(254, 202, 202)
                         : (pageStyle == 3) ? ((cx < 274) ? RGB(254, 240, 138)
                                            : ((cx < 514) ? RGB(255, 255, 235)
                                            : ((cx < 752) ? RGB(254, 240, 138)
                                            : ((cx < 992) ? RGB(255, 255, 255)
                                            : RGB(255, 255, 255)))))
                         : (pageStyle == 4) ? RGB(34, 211, 238)
                         : (pageStyle == 5) ? RGB(253, 224, 71)
                                            : RGB(251, 191, 36);

        fillRoundRect(hdc, cx - r, cy - r, cx + r, cy + r, r * 2, bodyCol, rimCol, (r <= 17) ? 1 : 2);

        // Conical Top-Left Specular Rim Arc
        HPEN hiRimPen = CreatePen(PS_SOLID, (r <= 17) ? 1 : 2, hiCol);
        HGDIOBJ oph = SelectObject(hdc, hiRimPen);
        Arc(hdc, cx - r, cy - r, cx + r, cy + r, cx + r, cy, cx, cy - r);
        SelectObject(hdc, oph);
        DeleteObject(hiRimPen);

        // Recessed inner spun-aluminum cap
        int capR = r - ((r <= 17) ? 4 : ((pageStyle == 3) ? 6 : 8));
        fillRoundRect(hdc, cx - capR, cy - capR, cx + capR, cy + capR, capR * 2, capCol, hiCol, 1);

        // Conical highlight arc on inner cap
        HPEN capArcPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        HGDIOBJ opc = SelectObject(hdc, capArcPen);
        Arc(hdc, cx - capR + 2, cy - capR + 2, cx + capR - 2, cy + capR - 2, cx, cy - capR + 2, cx - capR + 2, cy);
        SelectObject(hdc, opc);
        DeleteObject(capArcPen);

        // 3D Pointer Notch
        int px1 = cx + int(std::cos(angle) * ((r <= 17) ? 3 : 5));
        int py1 = cy + int(std::sin(angle) * ((r <= 17) ? 3 : 5));
        int px2 = cx + int(std::cos(angle) * (r - 2));
        int py2 = cy + int(std::sin(angle) * (r - 2));

        // Pointer shadow
        HPEN ptSPen = CreatePen(PS_SOLID, (r <= 17) ? 3 : 4, RGB(2, 10, 8));
        HGDIOBJ opps = SelectObject(hdc, ptSPen);
        MoveToEx(hdc, px1 + 1, py1 + 1, nullptr);
        LineTo(hdc, px2 + 1, py2 + 1);
        SelectObject(hdc, opps);
        DeleteObject(ptSPen);

        // Bright pointer line
        HPEN ptPen = CreatePen(PS_SOLID, (r <= 17) ? 2 : 3, ptrCol);
        HGDIOBJ opp = SelectObject(hdc, ptPen);
        MoveToEx(hdc, px1, py1, nullptr);
        LineTo(hdc, px2, py2);
        SelectObject(hdc, opp);
        DeleteObject(ptPen);

        // Center hub dot
        fillRoundRect(hdc, cx - 2, cy - 2, cx + 2, cy + 2, 4, RGB(255, 255, 255), rimCol, 1);
    }

    // Label and Value Readout
    int r_lbl_top = cy + r + ((r <= 17) ? 4 : 7);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(248, 250, 252));

    HFONT oldKnobFont = nullptr;
    HFONT fontCustom = nullptr;
    if (r <= 18 || pillHalfW <= 30) {
        fontCustom = CreateFontA(9, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        oldKnobFont = (HFONT)SelectObject(hdc, fontCustom);
    } else if (pillHalfW <= 34) {
        fontCustom = CreateFontA(10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        oldKnobFont = (HFONT)SelectObject(hdc, fontCustom);
    }

    int lblH = (r <= 18) ? 13 : 15;
    RECT tr = { cx - pillHalfW - 4, r_lbl_top, cx + pillHalfW + 4, r_lbl_top + lblH };
    DrawTextA(hdc, k.label, -1, &tr, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

    char valBuf[64];
    if (k.isStepped) {
        int idx = std::max(0, std::min(k.steps - 1, int(std::round(k.curVal))));
        std::snprintf(valBuf, sizeof(valBuf), "%s", k.stepLabels[idx]);
    } else {
        std::snprintf(valBuf, sizeof(valBuf), "%.1f", k.curVal);
    }

    // 3D Inset LED Value Pill
    int pillTop = r_lbl_top + lblH + 1;
    int pillH   = (r <= 17) ? 15 : 18;
    RECT vr = { cx - pillHalfW, pillTop, cx + pillHalfW, pillTop + pillH };
    fillRoundRect(hdc, vr.left, vr.top, vr.right, vr.bottom, 4, RGB(2, 15, 12), RGB(20, 83, 70), 1);

    COLORREF valCol = (pageStyle == 0) ? RGB(167, 243, 208)
                    : (pageStyle == 1) ? RGB(254, 215, 170)
                    : (pageStyle == 2) ? RGB(254, 202, 202)
                    : (pageStyle == 3) ? ((cx < 320) ? RGB(254, 240, 138) : ((cx < 540) ? RGB(167, 243, 208) : ((cx < 755) ? RGB(191, 219, 254) : ((cx < 970) ? RGB(254, 240, 138) : RGB(254, 202, 202)))))
                    : (pageStyle == 4) ? RGB(103, 232, 249)
                    : (pageStyle == 5) ? RGB(244, 114, 182)
                                       : RGB(253, 230, 138);
    SetTextColor(hdc, valCol);
    DrawTextA(hdc, valBuf, -1, &vr, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    if (fontCustom) {
        SelectObject(hdc, oldKnobFont);
        DeleteObject(fontCustom);
    }
}

static void renderGui(HWND hwnd, HDC hdcWin) {
    RECT cr;
    GetClientRect(hwnd, &cr);
    int w = cr.right - cr.left;
    int h = cr.bottom - cr.top;

    HDC hdc = CreateCompatibleDC(hdcWin);
    HBITMAP hbm = CreateCompatibleBitmap(hdcWin, w, h);
    HGDIOBJ oldBm = SelectObject(hdc, hbm);

    if (g_assets3D.imgWoodFloor) {
        Gdiplus::Graphics g(hdc);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        Gdiplus::TextureBrush woodBrush(g_assets3D.imgWoodFloor);
        g.FillRectangle(&woodBrush, 0, 0, w, h);

        Gdiplus::SolidBrush topDark(Gdiplus::Color(160, 2, 8, 12));
        g.FillRectangle(&topDark, 0, 0, w, 52);

        Gdiplus::Pen vigPen(Gdiplus::Color(90, 0, 0, 0), 8.0f);
        g.DrawRectangle(&vigPen, 0, 0, w, h);
    } else {
        HBRUSH bgBr = CreateSolidBrush(RGB(18, 10, 6));
        FillRect(hdc, &cr, bgBr);
        DeleteObject(bgBr);
    }

    HFONT fontTab   = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT fontTitle = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT fontSub   = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT fontKnob  = CreateFontA(12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT fontHud   = CreateFontA(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, fontTab);

    // 1. Top Navigation Bar: Tabs, Preset Selector & Utility Buttons
    const char* tabTitles[8] = {
        "1. CLEAN",
        "2. CRUNCH",
        "3. LEAD",
        "4. DRIVE",
        "5. SPACE",
        "6. REVERB",
        "7. DELAY",
        "8. TUNER"
    };

    for (int t = 0; t < 8; ++t) {
        bool active = (g_activePage == t);
        COLORREF bg = active ? (t == 0 ? RGB(6, 78, 59) :
                                t == 1 ? RGB(90, 30, 10) :
                                t == 2 ? RGB(90, 10, 20) :
                                t == 3 ? RGB(120, 80, 10) :
                                t == 4 ? RGB(8, 51, 68) :
                                t == 5 ? RGB(68, 10, 40) :
                                t == 6 ? RGB(30, 27, 75) :
                                         RGB(12, 74, 110))
                             : RGB(4, 20, 24);
        COLORREF br = active ? (t == 0 ? RGB(245, 158, 11) :
                                t == 1 ? RGB(249, 115, 22) :
                                t == 2 ? RGB(239, 68, 68) :
                                t == 3 ? RGB(251, 191, 36) :
                                t == 4 ? RGB(6, 182, 212) :
                                t == 5 ? RGB(244, 114, 182) :
                                t == 6 ? RGB(251, 191, 36) :
                                         RGB(56, 189, 248))
                             : RGB(19, 78, 74);
        fillRoundRect(hdc, g_tabRects[t].left, g_tabRects[t].top, g_tabRects[t].right, g_tabRects[t].bottom, 8, bg, br, active ? 2 : 1);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, active ? RGB(254, 240, 138) : RGB(148, 163, 184));
        DrawTextA(hdc, tabTitles[t], -1, &g_tabRects[t], DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    // Preset Controls (Prev, Name Display, Next, Save)
    fillRoundRect(hdc, g_presetPrevRect.left, g_presetPrevRect.top, g_presetPrevRect.right, g_presetPrevRect.bottom, 6, RGB(15, 23, 42), RGB(245, 158, 11), 1);
    SetTextColor(hdc, RGB(254, 240, 138));
    DrawTextA(hdc, "<", -1, &g_presetPrevRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    fillRoundRect(hdc, g_presetNameRect.left, g_presetNameRect.top, g_presetNameRect.right, g_presetNameRect.bottom, 6, RGB(6, 34, 28), RGB(20, 184, 166), 1);
    char pNameBuf[80];
    const char* cName = (!g_presets.empty() && g_currentPresetIdx >= 0 && g_currentPresetIdx < (int)g_presets.size())
                      ? g_presets[g_currentPresetIdx].name : "default";
    std::snprintf(pNameBuf, sizeof(pNameBuf), "PRESET: %s", cName);
    SetTextColor(hdc, RGB(254, 240, 138));
    DrawTextA(hdc, pNameBuf, -1, &g_presetNameRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    fillRoundRect(hdc, g_presetNextRect.left, g_presetNextRect.top, g_presetNextRect.right, g_presetNextRect.bottom, 6, RGB(15, 23, 42), RGB(245, 158, 11), 1);
    SetTextColor(hdc, RGB(254, 240, 138));
    DrawTextA(hdc, ">", -1, &g_presetNextRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    fillRoundRect(hdc, g_presetSaveRect.left, g_presetSaveRect.top, g_presetSaveRect.right, g_presetSaveRect.bottom, 6, RGB(245, 158, 11), RGB(254, 240, 138), 2);
    SetTextColor(hdc, RGB(30, 20, 0));
    DrawTextA(hdc, "SAVE", -1, &g_presetSaveRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Synchronized Learn Noise Button (Hum Killer + 10-Band Cyber-Denoiser)
    bool isLearning = g_engine.isLearningNoise();
    ULONGLONG now = GetTickCount64();

    // Client-side timeout safeguard (Rule 19) to ensure button never perpetually hangs
    if (isLearning && (now - g_learnNoiseStartTime > 1400)) {
        g_engine.humKiller.triggerLearn(false);
        g_engine.cyberDenoiser.cancelLearn();
        isLearning = false;
    }

    if (g_wasLearningNoise && !isLearning) {
        g_learnNoiseDoneTime = now;
    }
    g_wasLearningNoise = isLearning;

    bool justFinished = (!isLearning && (now - g_learnNoiseDoneTime < 2200) && g_learnNoiseDoneTime > 0);

    COLORREF lnBg = isLearning   ? RGB(180, 83, 9)       // Amber
                  : justFinished ? RGB(6, 95, 70)        // Vibrant Emerald
                                 : RGB(13, 48, 44);      // Deep Slate Teal
    COLORREF lnBorder = isLearning   ? RGB(251, 191, 36)
                      : justFinished ? RGB(52, 211, 153)
                                     : RGB(20, 184, 166);
    fillRoundRect(hdc, g_learnNoiseBtnRect.left, g_learnNoiseBtnRect.top,
                  g_learnNoiseBtnRect.right, g_learnNoiseBtnRect.bottom, 6, lnBg, lnBorder, 2);

    char lnText[32];
    if (isLearning) {
        int pct = int(g_engine.getLearnNoiseProgress() * 100.0f);
        std::snprintf(lnText, sizeof(lnText), "LEARN %d%%", std::min(99, std::max(1, pct)));
        SetTextColor(hdc, RGB(254, 240, 138));
    } else if (justFinished) {
        std::snprintf(lnText, sizeof(lnText), "PROFILED OK");
        SetTextColor(hdc, RGB(167, 243, 208));
    } else {
        std::snprintf(lnText, sizeof(lnText), "LEARN NOISE");
        SetTextColor(hdc, RGB(240, 253, 250));
    }
    DrawTextA(hdc, lnText, -1, &g_learnNoiseBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // 6-Second User Audio Sample Recorder Button
    bool isRecording = g_recSampleActive.load();
    bool recJustFinished = (!isRecording && (now - g_recSampleDoneTime < 2800) && g_recSampleDoneTime > 0);
    COLORREF recBg = isRecording ? RGB(185, 28, 28)
                   : recJustFinished ? RGB(6, 95, 70)
                   : RGB(45, 10, 15);
    COLORREF recBorder = isRecording ? RGB(252, 165, 165)
                       : recJustFinished ? RGB(52, 211, 153)
                       : RGB(239, 68, 68);
    fillRoundRect(hdc, g_recSampleBtnRect.left, g_recSampleBtnRect.top, g_recSampleBtnRect.right, g_recSampleBtnRect.bottom, 6, recBg, recBorder, 2);

    if (isRecording) {
        char recText[32];
        int remSec = std::max(1, 6 - int((now - g_recSampleStartTime) / 1000));
        std::snprintf(recText, sizeof(recText), "REC %ds...", remSec);
        SetTextColor(hdc, RGB(254, 240, 138));
        DrawTextA(hdc, recText, -1, &g_recSampleBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    } else if (recJustFinished) {
        SetTextColor(hdc, RGB(167, 243, 208));
        DrawTextA(hdc, "SAVED OK!", -1, &g_recSampleBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    } else {
        SetTextColor(hdc, RGB(254, 202, 202));
        DrawTextW(hdc, L"\x25CF REC 6s", -1, &g_recSampleBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    // Play Test Guitar Riff Button
    bool riffOn = g_testRiffActive.load();
    fillRoundRect(hdc, g_riffBtnRect.left, g_riffBtnRect.top, g_riffBtnRect.right, g_riffBtnRect.bottom, 6,
                  riffOn ? RGB(245, 158, 11) : RGB(6, 78, 59),
                  riffOn ? RGB(254, 240, 138) : RGB(20, 184, 166), 2);
    SetTextColor(hdc, riffOn ? RGB(40, 20, 0) : RGB(240, 253, 250));
    DrawTextA(hdc, riffOn ? "TEST RIFF: ON" : "TEST RIFF", -1, &g_riffBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Audio Settings Button
    fillRoundRect(hdc, g_audioSettingsBtnRect.left, g_audioSettingsBtnRect.top, g_audioSettingsBtnRect.right, g_audioSettingsBtnRect.bottom, 6,
                  RGB(13, 148, 136), RGB(45, 212, 191), 2);
    SelectObject(hdc, fontHud);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextA(hdc, "AUDIO SETTINGS", -1, &g_audioSettingsBtnRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // 2. Global Header Selector Pills (Routing, Oversampling, Physical Speaker, Cab Size, 432Hz Tuning, Pickup Load)
    SelectObject(hdc, fontKnob);
    for (int i = 0; i <= 5; ++i) {
        const KnobCtrl& gk = g_knobs[i];
        bool is432 = (i == 4 && gk.curVal > 0.5f);
        bool isPickupCustom = (i == 5 && gk.curVal > 0.5f);
        COLORREF pillBg = is432 ? RGB(16, 185, 129) : (isPickupCustom ? (gk.curVal > 1.5f ? RGB(180, 83, 9) : RGB(14, 116, 144)) : RGB(6, 34, 28));
        COLORREF pillBorder = is432 ? RGB(167, 243, 208) : (isPickupCustom ? (gk.curVal > 1.5f ? RGB(253, 230, 138) : RGB(103, 232, 249)) : RGB(245, 158, 11));
        fillRoundRect(hdc, gk.rect.left, gk.rect.top, gk.rect.right, gk.rect.bottom, 6, pillBg, pillBorder, 1);
        int idx = std::max(0, std::min(gk.steps - 1, int(std::round(gk.curVal))));

        // Top category label (small)
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, is432 ? RGB(209, 250, 229) : RGB(251, 191, 36));
        RECT rTop = { gk.rect.left + 2, gk.rect.top + 3, gk.rect.right - 2, gk.rect.top + 17 };
        DrawTextA(hdc, gk.label, -1, &rTop, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Value readout (prominent)
        SelectObject(hdc, fontKnob);
        SetTextColor(hdc, (is432 || isPickupCustom) ? RGB(255, 255, 255) : RGB(254, 243, 199));
        RECT rBot = { gk.rect.left + 2, gk.rect.top + 17, gk.rect.right - 2, gk.rect.bottom - 3 };
        DrawTextA(hdc, gk.stepLabels[idx], -1, &rBot, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }

    // Live Real-Time Hardware Performance Monitor HUD
    RECT hudRect = { 774, 56, 1238, 92 };
    fillRoundRect(hdc, hudRect.left, hudRect.top, hudRect.right, hudRect.bottom, 6, RGB(3, 18, 22), RGB(20, 184, 166), 1);

    float cpuVal = g_dspCpuPercent.load();
    float timeVal = g_dspTimeUs.load();
    int intRate = g_dspInternalRate.load();
    const char* osTag = (intRate >= 384000) ? "8x" : ((intRate >= 192000) ? "4x" : ((intRate >= 96000) ? "2x" : "1x"));

    char hudLine1[96];
    std::snprintf(hudLine1, sizeof(hudLine1), "DSP: %.1f%%  |  %u us  |  CORE: %d kHz (%s)  |  BUF: %d",
                  cpuVal, (unsigned int)timeVal, intRate / 1000, osTag, g_audioCfg.bufferFrames);

    SelectObject(hdc, fontHud);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, (cpuVal > 50.0f) ? RGB(248, 113, 113) : ((cpuVal > 25.0f) ? RGB(251, 191, 36) : RGB(52, 211, 153)));
    RECT hudTextR = { hudRect.left + 8, hudRect.top + 4, hudRect.right - 8, hudRect.top + 20 };
    DrawTextA(hdc, hudLine1, -1, &hudTextR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    int barLeft = hudRect.left + 8;
    int barTop = hudRect.top + 21;
    int barRight = hudRect.right - 8;
    int barBottom = hudRect.bottom - 5;
    int totalBarW = barRight - barLeft;
    fillRoundRect(hdc, barLeft, barTop, barRight, barBottom, 3, RGB(2, 10, 12), RGB(13, 148, 136), 1);

    float normLoad = std::max(0.0f, std::min(1.0f, cpuVal / 40.0f));
    int filledW = int(totalBarW * normLoad);
    if (filledW > 2) {
        COLORREF barCol = (cpuVal > 30.0f) ? RGB(239, 68, 68) : ((cpuVal > 15.0f) ? RGB(245, 158, 11) : RGB(16, 185, 129));
        fillRoundRect(hdc, barLeft + 1, barTop + 1, barLeft + filledW, barBottom - 1, 2, barCol, barCol, 1);
    }

    // 3. Main 3D Skeuomorphic Amp Chassis (Tolex, Corner Protectors, Faceplate & Basketweave Grille)
    int bx1 = 18, by1 = 104, bx2 = w - 18, by2 = h - 46;

    if (g_activePage == 0) {
        // ====================================================================
        // PAGE 0: EUPHORIA '65 CLEAN (Boutique Sapphire Blue Tolex & Silver Metal)
        // ====================================================================
        Gdiplus::Image* imgClean = g_assets3D.imgChassisCleanBlue ? g_assets3D.imgChassisCleanBlue : g_assets3D.imgChassisWide;
        if (imgClean) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(imgClean, bx1, by1, bx2 - bx1, by2 - by1);
        } else {
            // Outer Cabinet Drop Shadow onto background (fallback only)
            fillRoundRect(hdc, bx1 + 4, by1 + 6, bx2 + 4, by2 + 6, 20, RGB(1, 4, 6), RGB(1, 4, 6), 1);

            // 3D Cabinet Tolex Exterior Frame
            fillRoundRect(hdc, bx1, by1, bx2, by2, 18, RGB(4, 47, 46), RGB(245, 158, 11), 3);
            // Top edge light sheen
            HPEN sheenPen = CreatePen(PS_SOLID, 2, RGB(20, 83, 70));
            HGDIOBJ opsheen = SelectObject(hdc, sheenPen);
            MoveToEx(hdc, bx1 + 24, by1 + 2, nullptr);
            LineTo(hdc, bx2 - 24, by1 + 2);
            SelectObject(hdc, opsheen);
            DeleteObject(sheenPen);

            // 3D Leather Handle on top
            int hW = 200;
            int hX1 = (bx1 + bx2) / 2 - hW / 2;
            fillRoundRect(hdc, hX1, by1 - 8, hX1 + hW, by1 + 4, 6, RGB(15, 23, 42), RGB(203, 213, 225), 2);
            fillRoundRect(hdc, hX1 - 8, by1 - 6, hX1 + 4, by1 + 6, 4, RGB(203, 213, 225), RGB(71, 85, 105), 1);
            fillRoundRect(hdc, hX1 + hW - 4, by1 - 6, hX1 + hW + 8, by1 + 6, 4, RGB(203, 213, 225), RGB(71, 85, 105), 1);

            // 4 Chrome Corner Protectors with Rivets
            drawCornerProtector3D(hdc, bx1, by1, 28, 28, 0, RGB(148, 163, 184), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx2 - 28, by1, 28, 28, 1, RGB(148, 163, 184), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx1, by2 - 28, 28, 28, 2, RGB(148, 163, 184), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx2 - 28, by2 - 28, 28, 28, 3, RGB(148, 163, 184), RGB(255, 255, 255));

            // Inset Brushed Metal Control Faceplate (with horizontal reflection bands)
            int fpTop = by1 + 16, fpBottom = by1 + 280;
            fillRoundRect(hdc, bx1 + 18, fpTop, bx2 - 18, fpBottom, 10, RGB(6, 34, 28), RGB(251, 191, 36), 2);

            // Brushed metal specular reflection bands
            HPEN bandPen1 = CreatePen(PS_SOLID, 1, RGB(13, 65, 52));
            HPEN bandPen2 = CreatePen(PS_SOLID, 1, RGB(8, 48, 38));
            HGDIOBJ opb = SelectObject(hdc, bandPen1);
            for (int y = fpTop + 6; y < fpBottom - 6; y += 4) {
                SelectObject(hdc, (y % 8 == 0) ? bandPen1 : bandPen2);
                MoveToEx(hdc, bx1 + 22, y, nullptr);
                LineTo(hdc, bx2 - 22, y);
            }
            SelectObject(hdc, opb);
            DeleteObject(bandPen1);
            DeleteObject(bandPen2);

            // Inset gold pinstripe border
            fillRoundRect(hdc, bx1 + 24, fpTop + 8, bx2 - 24, fpBottom - 8, 8, RGB(0, 0, 0), RGB(245, 158, 11), 1);

            // 3D Interlaced Basketweave Grille Cloth
            int grTop = by1 + 292, grBottom = by2 - 16;
            drawBasketweaveGrille(hdc, bx1 + 18, grTop, bx2 - 18, grBottom, RGB(4, 47, 46), RGB(217, 119, 6));
        }

        // Titles with embossed shadow
        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(2, 20, 16));
        RECT subRS = { bx1 + 39, by1 + 17, bx1 + 920, by1 + 37 };
        DrawTextA(hdc, "CHANNEL 1  |  EUPHORIA SUPREME GLASS CLEAN (ACTIVE 3D TONE MATRIX & 16kHz EXTENDED AIR)", -1, &subRS, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(52, 211, 153));
        RECT subR = { bx1 + 38, by1 + 16, bx1 + 920, by1 + 36 };
        DrawTextA(hdc, "CHANNEL 1  |  EUPHORIA SUPREME GLASS CLEAN (ACTIVE 3D TONE MATRIX & 16kHz EXTENDED AIR)", -1, &subR, DT_LEFT | DT_SINGLELINE);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(2, 20, 16));
        RECT titRS = { bx1 + 39, by1 + 35, bx1 + 800, by1 + 65 };
        DrawTextA(hdc, "Euphoria Supreme Glass Clean", -1, &titRS, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(254, 240, 138));
        RECT titR = { bx1 + 38, by1 + 34, bx1 + 800, by1 + 64 };
        DrawTextA(hdc, "Euphoria Supreme Glass Clean", -1, &titR, DT_LEFT | DT_SINGLELINE);

        // 3D Radiant Emerald Jewel Pilot Lamp
        int lampX = bx2 - 68;
        drawFacetedJewelLamp(hdc, lampX, by1 + 40, 16, RGB(5, 150, 105), RGB(52, 211, 153), RGB(254, 240, 138));
    }
    else if (g_activePage == 1) {
        // ====================================================================
        // PAGE 1: EUPHORIA '59 BASSMAN / '68 JMP CRUNCH (Vintage Tweed / Chocolate Brown)
        // ====================================================================
        Gdiplus::Image* imgCrunch = g_assets3D.imgChassisCrunchBrown ? g_assets3D.imgChassisCrunchBrown : g_assets3D.imgChassisWide;
        if (imgCrunch) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(imgCrunch, bx1, by1, bx2 - bx1, by2 - by1);
        } else {
            fillRoundRect(hdc, bx1 + 4, by1 + 6, bx2 + 4, by2 + 6, 20, RGB(1, 4, 6), RGB(1, 4, 6), 1);
            fillRoundRect(hdc, bx1, by1, bx2, by2, 18, RGB(67, 20, 7), RGB(249, 115, 22), 3);
            HPEN sheenPen = CreatePen(PS_SOLID, 2, RGB(154, 52, 18));
            HGDIOBJ opsheen = SelectObject(hdc, sheenPen);
            MoveToEx(hdc, bx1 + 24, by1 + 2, nullptr);
            LineTo(hdc, bx2 - 24, by1 + 2);
            SelectObject(hdc, opsheen);
            DeleteObject(sheenPen);

            int hW = 200;
            int hX1 = (bx1 + bx2) / 2 - hW / 2;
            fillRoundRect(hdc, hX1, by1 - 8, hX1 + hW, by1 + 4, 6, RGB(30, 10, 5), RGB(251, 191, 36), 2);
            fillRoundRect(hdc, hX1 - 8, by1 - 6, hX1 + 4, by1 + 6, 4, RGB(251, 191, 36), RGB(180, 83, 9), 1);
            fillRoundRect(hdc, hX1 + hW - 4, by1 - 6, hX1 + hW + 8, by1 + 6, 4, RGB(251, 191, 36), RGB(180, 83, 9), 1);

            drawCornerProtector3D(hdc, bx1, by1, 28, 28, 0, RGB(217, 119, 6), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 28, by1, 28, 28, 1, RGB(217, 119, 6), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx1, by2 - 28, 28, 28, 2, RGB(217, 119, 6), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 28, by2 - 28, 28, 28, 3, RGB(217, 119, 6), RGB(254, 240, 138));

            int fpTop = by1 + 16, fpBottom = by1 + 280;
            fillRoundRect(hdc, bx1 + 18, fpTop, bx2 - 18, fpBottom, 10, RGB(124, 45, 18), RGB(254, 215, 170), 2);

            HPEN bandPen1 = CreatePen(PS_SOLID, 1, RGB(180, 65, 20));
            HPEN bandPen2 = CreatePen(PS_SOLID, 1, RGB(90, 25, 8));
            HGDIOBJ opb = SelectObject(hdc, bandPen1);
            for (int y = fpTop + 6; y < fpBottom - 6; y += 4) {
                SelectObject(hdc, (y % 8 == 0) ? bandPen1 : bandPen2);
                MoveToEx(hdc, bx1 + 22, y, nullptr);
                LineTo(hdc, bx2 - 22, y);
            }
            SelectObject(hdc, opb);
            DeleteObject(bandPen1);
            DeleteObject(bandPen2);

            fillRoundRect(hdc, bx1 + 24, fpTop + 8, bx2 - 24, fpBottom - 8, 8, RGB(0, 0, 0), RGB(254, 215, 170), 1);

            int grTop = by1 + 292, grBottom = by2 - 16;
            drawBasketweaveGrille(hdc, bx1 + 18, grTop, bx2 - 18, grBottom, RGB(45, 15, 6), RGB(194, 65, 12));
        }

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(20, 5, 2));
        RECT subRS = { bx1 + 39, by1 + 17, bx1 + 820, by1 + 37 };
        DrawTextA(hdc, "CHANNEL 2  |  FLUID MOMENTUM CATHODE FOLLOWER + 56k TWEED / 33k PLEXI VISCOUS SLOPE", -1, &subRS, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(254, 215, 170));
        RECT subR = { bx1 + 38, by1 + 16, bx1 + 820, by1 + 36 };
        DrawTextA(hdc, "CHANNEL 2  |  FLUID MOMENTUM CATHODE FOLLOWER + 56k TWEED / 33k PLEXI VISCOUS SLOPE", -1, &subR, DT_LEFT | DT_SINGLELINE);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(20, 5, 2));
        RECT titRS = { bx1 + 39, by1 + 35, bx1 + 800, by1 + 65 };
        DrawTextA(hdc, "Euphoria '59 Bassman / '68 JMP Crunch", -1, &titRS, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(255, 237, 213));
        RECT titR = { bx1 + 38, by1 + 34, bx1 + 800, by1 + 64 };
        DrawTextA(hdc, "Euphoria '59 Bassman / '68 JMP Crunch", -1, &titR, DT_LEFT | DT_SINGLELINE);

        int lampX = bx2 - 68;
        drawFacetedJewelLamp(hdc, lampX, by1 + 40, 16, RGB(194, 65, 12), RGB(251, 146, 60), RGB(254, 240, 138));
    }
    else if (g_activePage == 2) {
        // ====================================================================
        // PAGE 2: EUPHORIA '80s FRIEDMAN BE-100 / JCM 800 LEAD (Hot-Rod British Red Tolex)
        // ====================================================================
        Gdiplus::Image* imgLead = g_assets3D.imgChassisLeadRed ? g_assets3D.imgChassisLeadRed : g_assets3D.imgChassisWide;
        if (imgLead) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(imgLead, bx1, by1, bx2 - bx1, by2 - by1);
        } else {
            fillRoundRect(hdc, bx1 + 4, by1 + 6, bx2 + 4, by2 + 6, 20, RGB(1, 4, 6), RGB(1, 4, 6), 1);
            fillRoundRect(hdc, bx1, by1, bx2, by2, 18, RGB(45, 10, 15), RGB(239, 68, 68), 3);
            HPEN sheenPen = CreatePen(PS_SOLID, 2, RGB(120, 20, 30));
            HGDIOBJ opsheen = SelectObject(hdc, sheenPen);
            MoveToEx(hdc, bx1 + 24, by1 + 2, nullptr);
            LineTo(hdc, bx2 - 24, by1 + 2);
            SelectObject(hdc, opsheen);
            DeleteObject(sheenPen);

            int hW = 200;
            int hX1 = (bx1 + bx2) / 2 - hW / 2;
            fillRoundRect(hdc, hX1, by1 - 8, hX1 + hW, by1 + 4, 6, RGB(18, 5, 8), RGB(251, 191, 36), 2);
            fillRoundRect(hdc, hX1 - 8, by1 - 6, hX1 + 4, by1 + 6, 4, RGB(251, 191, 36), RGB(180, 83, 9), 1);
            fillRoundRect(hdc, hX1 + hW - 4, by1 - 6, hX1 + hW + 8, by1 + 6, 4, RGB(251, 191, 36), RGB(180, 83, 9), 1);

            drawCornerProtector3D(hdc, bx1, by1, 28, 28, 0, RGB(217, 119, 6), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 28, by1, 28, 28, 1, RGB(217, 119, 6), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx1, by2 - 28, 28, 28, 2, RGB(217, 119, 6), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 28, by2 - 28, 28, 28, 3, RGB(217, 119, 6), RGB(254, 240, 138));

            int fpTop = by1 + 16, fpBottom = by1 + 280;
            fillRoundRect(hdc, bx1 + 18, fpTop, bx2 - 18, fpBottom, 10, RGB(24, 16, 18), RGB(239, 68, 68), 2);

            HPEN bandPen1 = CreatePen(PS_SOLID, 1, RGB(48, 25, 28));
            HPEN bandPen2 = CreatePen(PS_SOLID, 1, RGB(18, 10, 12));
            HGDIOBJ opb = SelectObject(hdc, bandPen1);
            for (int y = fpTop + 6; y < fpBottom - 6; y += 4) {
                SelectObject(hdc, (y % 8 == 0) ? bandPen1 : bandPen2);
                MoveToEx(hdc, bx1 + 22, y, nullptr);
                LineTo(hdc, bx2 - 22, y);
            }
            SelectObject(hdc, opb);
            DeleteObject(bandPen1);
            DeleteObject(bandPen2);

            fillRoundRect(hdc, bx1 + 24, fpTop + 8, bx2 - 24, fpBottom - 8, 8, RGB(0, 0, 0), RGB(245, 158, 11), 1);

            int grTop = by1 + 292, grBottom = by2 - 16;
            drawBasketweaveGrille(hdc, bx1 + 18, grTop, bx2 - 18, grBottom, RGB(18, 16, 16), RGB(60, 50, 50));
        }

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(20, 5, 8));
        RECT subRS = { bx1 + 39, by1 + 17, bx1 + 860, by1 + 37 };
        DrawTextA(hdc, "CHANNEL 3  |  4-STAGE 12AX7 CASCADE + 10k COLD CLIPPER + C45 MID PUNCH + DYNAMIC DE-HISS", -1, &subRS, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(254, 202, 202));
        RECT subR = { bx1 + 38, by1 + 16, bx1 + 860, by1 + 36 };
        DrawTextA(hdc, "CHANNEL 3  |  4-STAGE 12AX7 CASCADE + 10k COLD CLIPPER + C45 MID PUNCH + DYNAMIC DE-HISS", -1, &subR, DT_LEFT | DT_SINGLELINE);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(20, 5, 8));
        RECT titRS = { bx1 + 39, by1 + 35, bx1 + 800, by1 + 65 };
        DrawTextA(hdc, "Euphoria '80s Friedman BE-100 / JCM 800 Lead", -1, &titRS, DT_LEFT | DT_SINGLELINE);
        SetTextColor(hdc, RGB(254, 240, 138));
        RECT titR = { bx1 + 38, by1 + 34, bx1 + 800, by1 + 64 };
        DrawTextA(hdc, "Euphoria '80s Friedman BE-100 / JCM 800 Lead", -1, &titR, DT_LEFT | DT_SINGLELINE);

        int lampX = bx2 - 68;
        drawFacetedJewelLamp(hdc, lampX, by1 + 40, 16, RGB(185, 28, 28), RGB(239, 68, 68), RGB(254, 202, 202));
    }
    else if (g_activePage == 3) {
        // ====================================================================
        // PAGE 3: EUPHORIA DYNAMIC FLUID STOMPBOX LAB (Authentic Hardwood Floorboard Deck)
        // 3 Independent Series Stompboxes: Fluid Centaur, TS808 Screamer, BD-2 Blues Driver
        // ====================================================================
        // 1. Draw Hardwood Plank Floorboard
        if (g_assets3D.imgWoodFloor) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            Gdiplus::TextureBrush woodBrush(g_assets3D.imgWoodFloor);
            g.FillRectangle(&woodBrush, bx1, by1, bx2 - bx1, by2 - by1);

            Gdiplus::Pen edgePen(Gdiplus::Color(180, 24, 14, 8), 2.0f);
            g.DrawRectangle(&edgePen, bx1, by1, bx2 - bx1, by2 - by1);
        } else {
            fillRoundRect(hdc, bx1, by1, bx2, by2, 14, RGB(42, 26, 14), RGB(80, 50, 28), 2);

            // Horizontal hardwood planks (alternating walnut, golden oak, and aged wood tones)
            static const COLORREF s_woodTones[6] = {
                RGB(56, 35, 20), RGB(66, 42, 24), RGB(48, 30, 17),
                RGB(62, 39, 22), RGB(52, 33, 19), RGB(58, 37, 21)
            };
            int plankH = 44;
            int pIdx = 0;
            for (int py = by1 + 4; py < by2 - 4; py += plankH) {
                int pBottom = std::min(by2 - 4, py + plankH);
                COLORREF wCol = s_woodTones[pIdx % 6];
                fillRoundRect(hdc, bx1 + 4, py, bx2 - 4, pBottom, 0, wCol, wCol, 1);

                // Subtle wood grain streaks
                HPEN grainPen1 = CreatePen(PS_SOLID, 1, RGB(std::min(255, GetRValue(wCol) + 12), std::min(255, GetGValue(wCol) + 8), std::min(255, GetBValue(wCol) + 4)));
                HPEN grainPen2 = CreatePen(PS_SOLID, 1, RGB(std::max(0, GetRValue(wCol) - 10), std::max(0, GetGValue(wCol) - 8), std::max(0, GetBValue(wCol) - 5)));
                HGDIOBJ oldGP = SelectObject(hdc, grainPen1);
                MoveToEx(hdc, bx1 + 10, py + 12, nullptr); LineTo(hdc, bx2 - 10, py + 12);
                MoveToEx(hdc, bx1 + 10, py + 28, nullptr); LineTo(hdc, bx2 - 10, py + 28);
                SelectObject(hdc, grainPen2);
                MoveToEx(hdc, bx1 + 10, py + 20, nullptr); LineTo(hdc, bx2 - 10, py + 20);
                SelectObject(hdc, oldGP);
                DeleteObject(grainPen1);
                DeleteObject(grainPen2);

                // Horizontal plank seam (groove shadow + highlight)
                HPEN seamDark = CreatePen(PS_SOLID, 1, RGB(22, 13, 7));
                HPEN seamLite = CreatePen(PS_SOLID, 1, RGB(88, 56, 32));
                HGDIOBJ oldSP = SelectObject(hdc, seamDark);
                MoveToEx(hdc, bx1 + 4, pBottom - 1, nullptr); LineTo(hdc, bx2 - 4, pBottom - 1);
                SelectObject(hdc, seamLite);
                MoveToEx(hdc, bx1 + 4, pBottom, nullptr); LineTo(hdc, bx2 - 4, pBottom);
                SelectObject(hdc, oldSP);
                DeleteObject(seamDark);
                DeleteObject(seamLite);

                // Brass wood screw plugs at staggered intervals
                int screwOffset = (pIdx % 2 == 0) ? 60 : 240;
                for (int sx = bx1 + screwOffset; sx < bx2 - 40; sx += 360) {
                    fillRoundRect(hdc, sx - 3, py + 6, sx + 3, py + 12, 6, RGB(30, 18, 10), RGB(90, 60, 30), 1);
                    fillRoundRect(hdc, sx - 1, py + 8, sx + 1, py + 10, 2, RGB(180, 140, 60), RGB(180, 140, 60), 1);
                }
                pIdx++;
            }
        }

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(254, 240, 138));
        RECT headSubR = { bx1 + 24, by1 + 6, bx2 - 24, by1 + 24 };
        DrawTextA(hdc, "HARDWOOD PEDALBOARD DECK  |  5 INDEPENDENT SERIES STOMPBOXES  |  MASSIVE CLEAN BOOST HEADROOM (+22dB)", -1, &headSubR, DT_CENTER | DT_SINGLELINE);

        // ====================================================================
        // PEDAL 1: FLUID CENTAUR (Mythical Gold Enclosure, Center X = 204)
        // ====================================================================
        int p1X1 = 109, p1X2 = 299, p1Y1 = 135, p1Y2 = 555;
        bool klonActive = (g_knobs[K_KLON_ON].curVal > 0.5f);

        if (g_assets3D.imgKlon) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(g_assets3D.imgKlon, p1X1, p1Y1, p1X2 - p1X1, p1Y2 - p1Y1);
        } else {
            // Cast shadow on hardwood floor
            fillRoundRect(hdc, p1X1 + 7, p1Y1 + 9, p1X2 + 7, p1Y2 + 9, 14, RGB(18, 10, 6), RGB(18, 10, 6), 1);

            // Gold Beveled Enclosure
            fillRoundRect(hdc, p1X1, p1Y1, p1X2, p1Y2, 14,
                          klonActive ? RGB(180, 130, 38) : RGB(140, 100, 30),
                          klonActive ? RGB(254, 240, 138) : RGB(190, 140, 50),
                          klonActive ? 2 : 1);

            // Header Plaque (Oxblood)
            fillRoundRect(hdc, p1X1 + 10, p1Y1 + 8, p1X2 - 10, p1Y1 + 48, 6,
                          RGB(75, 16, 18), RGB(217, 119, 6), 1);

            // Bottom Badge & Chrome 3PDT Footswitch
            fillRoundRect(hdc, p1X1 + 12, p1Y2 - 110, p1X2 - 12, p1Y2 - 90, 4, RGB(55, 12, 15), RGB(217, 119, 6), 1);
            fillRoundRect(hdc, 204 - 28, 470, 204 + 28, 526, 56, RGB(140, 140, 145), RGB(200, 200, 205), 2); // Hex washer
            fillRoundRect(hdc, 204 - 20, 478, 204 + 20, 518, 40, RGB(215, 215, 220), RGB(255, 255, 255), 2); // Chrome button
            fillRoundRect(hdc, 204 - 12, 482, 204 + 2, 496, 14, RGB(255, 255, 255), RGB(255, 255, 255), 1); // Specular highlight arc
        }

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, klonActive ? RGB(254, 240, 138) : RGB(200, 170, 110));
        RECT klonTitR = { p1X1 + 12, p1Y1 + 10, p1X2 - 12, p1Y1 + 34 };
        DrawTextA(hdc, "FLUID CENTAUR", -1, &klonTitR, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(254, 202, 202));
        RECT klonSubR = { p1X1 + 12, p1Y1 + 32, p1X2 - 12, p1Y1 + 46 };
        DrawTextA(hdc, "18V PURE GERMANIUM", -1, &klonSubR, DT_CENTER | DT_SINGLELINE);

        // LED Lamp (Red)
        fillRoundRect(hdc, 204 - 8, p1Y1 + 52, 204 + 8, p1Y1 + 68, 16,
                      klonActive ? RGB(239, 68, 68) : RGB(50, 12, 12),
                      klonActive ? RGB(254, 202, 202) : RGB(80, 20, 20), 1);
        if (klonActive) {
            fillRoundRect(hdc, 204 - 3, p1Y1 + 56, 204 + 3, p1Y1 + 62, 6, RGB(255, 255, 255), RGB(255, 255, 255), 1);
        }

        if (!g_assets3D.imgKlon) {
            SetTextColor(hdc, RGB(254, 240, 138));
            RECT klonBdgR = { p1X1 + 14, p1Y2 - 109, p1X2 - 14, p1Y2 - 91 };
            bool isHyp = (g_knobs[K_KLON_HYPER].curVal > 0.5f);
            DrawTextA(hdc, isHyp ? "HYPER 27V ADAA (+22dB)" : "(OLD) 18V VINTAGE KLON", -1, &klonBdgR, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        SetTextColor(hdc, klonActive ? RGB(254, 240, 138) : RGB(140, 115, 75));
        RECT klonFootLblR = { p1X1, p1Y2 - 24, p1X2, p1Y2 - 6 };
        DrawTextA(hdc, klonActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", -1, &klonFootLblR, DT_CENTER | DT_SINGLELINE);

        // ====================================================================
        // PEDAL 2: TS808 SCREAMER (Vintage Olive Stompbox, Center X = 422)
        // ====================================================================
        int p2X1 = 327, p2X2 = 517, p2Y1 = 135, p2Y2 = 555;
        bool tsActive = (g_knobs[K_TS_ON].curVal > 0.5f);

        if (g_assets3D.imgTs9) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(g_assets3D.imgTs9, p2X1, p2Y1, p2X2 - p2X1, p2Y2 - p2Y1);
        } else {
            // Cast shadow on hardwood floor
            fillRoundRect(hdc, p2X1 + 7, p2Y1 + 9, p2X2 + 7, p2Y2 + 9, 14, RGB(18, 10, 6), RGB(18, 10, 6), 1);

            // Olive Green Enclosure
            fillRoundRect(hdc, p2X1, p2Y1, p2X2, p2Y2, 14,
                          tsActive ? RGB(22, 105, 52) : RGB(16, 75, 38),
                          tsActive ? RGB(110, 231, 183) : RGB(40, 120, 65),
                          tsActive ? 2 : 1);

            // Header Plaque (Dark Olive)
            fillRoundRect(hdc, p2X1 + 10, p2Y1 + 8, p2X2 - 10, p2Y1 + 48, 6,
                          RGB(12, 55, 28), RGB(34, 197, 94), 1);

            // Bottom Badge & TS808 Rubber Footswitch Pad
            fillRoundRect(hdc, p2X1 + 12, p2Y2 - 110, p2X2 - 12, p2Y2 - 90, 4, RGB(8, 42, 22), RGB(52, 211, 153), 1);

            // Square TS808 Ribbed Stomp Pad (Center: 422, 498)
            fillRoundRect(hdc, 422 - 38, 472, 422 + 38, 524, 6, RGB(25, 28, 26), RGB(180, 185, 180), 2);
            HPEN ribPen = CreatePen(PS_SOLID, 2, RGB(45, 50, 48));
            HGDIOBJ oldRibP = SelectObject(hdc, ribPen);
            for (int ry = 480; ry <= 516; ry += 6) {
                MoveToEx(hdc, 422 - 30, ry, nullptr); LineTo(hdc, 422 + 30, ry);
            }
            SelectObject(hdc, oldRibP);
            DeleteObject(ribPen);
        }

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, tsActive ? RGB(167, 243, 208) : RGB(110, 170, 135));
        RECT tsTitR = { p2X1 + 12, p2Y1 + 10, p2X2 - 12, p2Y1 + 34 };
        DrawTextA(hdc, "TS808 SCREAMER", -1, &tsTitR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(209, 250, 229));
        RECT tsSubR = { p2X1 + 12, p2Y1 + 32, p2X2 - 12, p2Y1 + 46 };
        DrawTextA(hdc, "720.5Hz MID-HUMP & JRC4558", -1, &tsSubR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // LED Lamp (Amber)
        fillRoundRect(hdc, 422 - 8, p2Y1 + 52, 422 + 8, p2Y1 + 68, 16,
                      tsActive ? RGB(245, 158, 11) : RGB(50, 30, 8),
                      tsActive ? RGB(254, 240, 138) : RGB(80, 48, 15), 1);
        if (tsActive) {
            fillRoundRect(hdc, 422 - 3, p2Y1 + 56, 422 + 3, p2Y1 + 62, 6, RGB(255, 255, 255), RGB(255, 255, 255), 1);
        }

        if (!g_assets3D.imgTs9) {
            SetTextColor(hdc, RGB(167, 243, 208));
            RECT tsBdgR = { p2X1 + 14, p2Y2 - 109, p2X2 - 14, p2Y2 - 91 };
            bool isHyp = (g_knobs[K_TS_HYPER].curVal > 0.5f);
            bool isFat = (g_knobs[K_TS_MOD].curVal > 0.5f);
            const char* tsBdg = isHyp ? (isFat ? "HYPER FAT (+20dB)" : "HYPER VELVET (+20dB)")
                                      : (isFat ? "(OLD) FAT MOD 340Hz" : "(OLD) STOCK 720Hz");
            DrawTextA(hdc, tsBdg, -1, &tsBdgR, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        SetTextColor(hdc, tsActive ? RGB(167, 243, 208) : RGB(100, 150, 120));
        RECT tsFootLblR = { p2X1, p2Y2 - 24, p2X2, p2Y2 - 6 };
        DrawTextA(hdc, tsActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", -1, &tsFootLblR, DT_CENTER | DT_SINGLELINE);

        // ====================================================================
        // PEDAL 3: BD-2 BLUES DRIVER (Cobalt Blue Boss Enclosure, Center X = 640)
        // ====================================================================
        int p3X1 = 545, p3X2 = 735, p3Y1 = 135, p3Y2 = 555;
        bool bdActive = (g_knobs[K_BD_ON].curVal > 0.5f);

        if (g_assets3D.imgBlues) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(g_assets3D.imgBlues, p3X1, p3Y1, p3X2 - p3X1, p3Y2 - p3Y1);
        } else {
            // Cast shadow on hardwood floor
            fillRoundRect(hdc, p3X1 + 7, p3Y1 + 9, p3X2 + 7, p3Y2 + 9, 14, RGB(18, 10, 6), RGB(18, 10, 6), 1);

            // Metallic Cobalt Blue Enclosure
            fillRoundRect(hdc, p3X1, p3Y1, p3X2, p3Y2, 14,
                          bdActive ? RGB(20, 72, 150) : RGB(14, 48, 105),
                          bdActive ? RGB(96, 165, 250) : RGB(37, 99, 235),
                          bdActive ? 2 : 1);

            // Header Plaque (Deep Navy)
            fillRoundRect(hdc, p3X1 + 10, p3Y1 + 8, p3X2 - 10, p3Y1 + 48, 6,
                          RGB(10, 32, 75), RGB(59, 130, 246), 1);

            // Bottom Badge & Boss Treadle Footswitch
            fillRoundRect(hdc, p3X1 + 12, p3Y2 - 110, p3X2 - 12, p3Y2 - 90, 4, RGB(8, 25, 55), RGB(59, 130, 246), 1);

            // Boss-style Rubber Foot Treadle (Center: 640, 498)
            fillRoundRect(hdc, 640 - 44, 468, 640 + 44, 528, 8, RGB(20, 20, 24), RGB(160, 165, 175), 2);
            HPEN bossRib = CreatePen(PS_SOLID, 2, RGB(40, 42, 48));
            HGDIOBJ oldBossP = SelectObject(hdc, bossRib);
            for (int by = 476; by <= 518; by += 7) {
                MoveToEx(hdc, 640 - 36, by, nullptr); LineTo(hdc, 640 + 36, by);
            }
            SelectObject(hdc, oldBossP);
            DeleteObject(bossRib);

            // Boss Thumb Screw at bottom
            fillRoundRect(hdc, 640 - 6, 523, 640 + 6, 532, 6, RGB(180, 180, 185), RGB(230, 230, 235), 1);
        }

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, bdActive ? RGB(254, 240, 138) : RGB(210, 200, 120)); // Iconic Boss Yellow font
        RECT bdTitR = { p3X1 + 12, p3Y1 + 10, p3X2 - 12, p3Y1 + 34 };
        DrawTextA(hdc, "Blues Driver", -1, &bdTitR, DT_CENTER | DT_SINGLELINE);

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(191, 219, 254));
        RECT bdSubR = { p3X1 + 12, p3Y1 + 32, p3X2 - 12, p3Y1 + 46 };
        DrawTextA(hdc, "BD-2 FLUID LAB | DUAL JFET", -1, &bdSubR, DT_CENTER | DT_SINGLELINE);

        // LED Lamp (Cyan / Blue)
        fillRoundRect(hdc, 640 - 8, p3Y1 + 52, 640 + 8, p3Y1 + 68, 16,
                      bdActive ? RGB(34, 211, 238) : RGB(10, 25, 55),
                      bdActive ? RGB(186, 230, 253) : RGB(25, 50, 95), 1);
        if (bdActive) {
            fillRoundRect(hdc, 640 - 3, p3Y1 + 56, 640 + 3, p3Y1 + 62, 6, RGB(255, 255, 255), RGB(255, 255, 255), 1);
        }

        if (!g_assets3D.imgBlues) {
            SetTextColor(hdc, RGB(254, 240, 138));
            RECT bdBdgR = { p3X1 + 14, p3Y2 - 109, p3X2 - 14, p3Y2 - 91 };
            bool isHyp = (g_knobs[K_BD_HYPER].curVal > 0.5f);
            bool isPhat = (g_knobs[K_BD_PHAT].curVal > 0.5f);
            const char* bdBdg = isHyp ? (isPhat ? "HYPER PHAT (+22dB)" : "HYPER SLIPSTREAM")
                                      : (isPhat ? "(OLD) PHAT MOD 160Hz" : "(OLD) STOCK BD-2");
            DrawTextA(hdc, bdBdg, -1, &bdBdgR, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        SetTextColor(hdc, bdActive ? RGB(254, 240, 138) : RGB(130, 160, 200));
        RECT bdFootLblR = { p3X1, p3Y2 - 24, p3X2, p3Y2 - 6 };
        DrawTextA(hdc, bdActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", -1, &bdFootLblR, DT_CENTER | DT_SINGLELINE);

        // ====================================================================
        // PEDAL 4: FLUID RODENT (1978 ProCo Rat Steel Chassis, Center X = 858)
        // ====================================================================
        int p4X1 = 763, p4X2 = 953, p4Y1 = 135, p4Y2 = 555;
        bool ratActive = (g_knobs[K_RAT_ON].curVal > 0.5f);
        bool ratIsTurbo = (g_knobs[K_RAT_TURBO].curVal > 0.5f);

        if (g_assets3D.imgRat) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(g_assets3D.imgRat, p4X1, p4Y1, p4X2 - p4X1, p4Y2 - p4Y1);
        } else {
            // Cast shadow on hardwood floor
            fillRoundRect(hdc, p4X1 + 7, p4Y1 + 9, p4X2 + 7, p4Y2 + 9, 14, RGB(18, 10, 6), RGB(18, 10, 6), 1);

            // Heavy-duty Matte Black Sloped Steel Chassis
            COLORREF ratBg     = ratActive ? RGB(28, 28, 32) : RGB(18, 18, 22);
            COLORREF ratBorder = ratActive ? (ratIsTurbo ? RGB(239, 68, 68) : RGB(254, 240, 138)) : RGB(80, 80, 90);
            fillRoundRect(hdc, p4X1, p4Y1, p4X2, p4Y2, 14, ratBg, ratBorder, ratActive ? 2 : 1);

            // Header Plaque (Industrial Steel / Red Graphic)
            fillRoundRect(hdc, p4X1 + 10, p4Y1 + 8, p4X2 - 10, p4Y1 + 48, 6,
                          RGB(14, 14, 18), ratIsTurbo ? RGB(220, 38, 38) : RGB(217, 119, 6), 1);

            // Bottom Badge & Heavy Steel Stomp Footswitch
            fillRoundRect(hdc, p4X1 + 12, p4Y2 - 110, p4X2 - 12, p4Y2 - 90, 4, RGB(14, 14, 18), RGB(100, 100, 110), 1);

            // Heavy-duty Steel Footswitch (Center: 858, 498)
            fillRoundRect(hdc, 858 - 28, 470, 858 + 28, 526, 56, RGB(120, 120, 125), RGB(190, 190, 195), 2); // Hex nut
            fillRoundRect(hdc, 858 - 20, 478, 858 + 20, 518, 40, RGB(210, 210, 215), RGB(255, 255, 255), 2); // Chrome button
            fillRoundRect(hdc, 858 - 12, 482, 858 + 2, 496, 14, RGB(255, 255, 255), RGB(255, 255, 255), 1); // Specular highlight arc
        }

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, ratActive ? (ratIsTurbo ? RGB(254, 202, 202) : RGB(254, 240, 138)) : RGB(180, 180, 180));
        RECT ratTitR = { p4X1 + 12, p4Y1 + 10, p4X2 - 12, p4Y1 + 34 };
        DrawTextA(hdc, ratIsTurbo ? "TURBO RODENT" : "FLUID RODENT", -1, &ratTitR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontHud);
        SetTextColor(hdc, ratIsTurbo ? RGB(252, 165, 165) : RGB(254, 240, 138));
        RECT ratSubR = { p4X1 + 2, p4Y1 + 32, p4X2 - 2, p4Y1 + 46 };
        DrawTextA(hdc, ratIsTurbo ? "TURBO RED LED & LM308 SLEW" : "1978 LM308 SLEW & DIODE SHUNT", -1, &ratSubR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // LED Lamp (Red)
        COLORREF ratLedCol  = ratActive ? RGB(239, 68, 68) : RGB(50, 12, 12);
        COLORREF ratLedGlow = ratActive ? RGB(254, 202, 202) : RGB(80, 20, 20);
        fillRoundRect(hdc, 858 - 8, p4Y1 + 52, 858 + 8, p4Y1 + 68, 16, ratLedCol, ratLedGlow, 1);
        if (ratActive) {
            fillRoundRect(hdc, 858 - 3, p4Y1 + 56, 858 + 3, p4Y1 + 62, 6, RGB(255, 255, 255), RGB(255, 255, 255), 1);
        }

        if (!g_assets3D.imgRat) {
            bool isHyp = (g_knobs[K_RAT_HYPER].curVal > 0.5f);
            bool isTurbo = (g_knobs[K_RAT_TURBO].curVal > 0.5f);
            SetTextColor(hdc, isTurbo ? RGB(252, 165, 165) : RGB(254, 240, 138));
            RECT ratBdgR = { p4X1 + 14, p4Y2 - 109, p4X2 - 14, p4Y2 - 91 };
            const char* ratBdg = isHyp ? (isTurbo ? "HYPER TURBO VIOLIN" : "HYPER LM308 SILICON")
                                       : (isTurbo ? "(OLD) TURBO RED LED" : "(OLD) SILICON DIODE");
            DrawTextA(hdc, ratBdg, -1, &ratBdgR, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        SetTextColor(hdc, ratActive ? (ratIsTurbo ? RGB(252, 165, 165) : RGB(254, 240, 138)) : RGB(140, 140, 140));
        RECT ratFootLblR = { p4X1, p4Y2 - 24, p4X2, p4Y2 - 6 };
        DrawTextA(hdc, ratActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", -1, &ratFootLblR, DT_CENTER | DT_SINGLELINE);

        // ====================================================================
        // PEDAL 5: FLUID FUZZ FACE (Dallas Arbiter Enclosure, Center X = 1076)
        // ====================================================================
        int p5X1 = 981, p5X2 = 1171, p5Y1 = 135, p5Y2 = 555;
        bool ffActive = (g_knobs[K_FUZZ_ON].curVal > 0.5f);
        bool ffIsSilicon = (g_knobs[K_FUZZ_TRANSISTOR].curVal > 0.5f);

        if (g_assets3D.imgFuzz) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(g_assets3D.imgFuzz, p5X1, p5Y1, p5X2 - p5X1, p5Y2 - p5Y1);
        } else {
            // Cast shadow on hardwood floor
            fillRoundRect(hdc, p5X1 + 7, p5Y1 + 9, p5X2 + 7, p5Y2 + 9, 14, RGB(18, 10, 6), RGB(18, 10, 6), 1);

            // Vintage Scarlet / London Grey Enclosure
            COLORREF ffChassisBg = ffActive ? (ffIsSilicon ? RGB(65, 75, 95) : RGB(165, 42, 42))
                                            : (ffIsSilicon ? RGB(45, 52, 68) : RGB(105, 30, 30));
            COLORREF ffBorder    = ffActive ? (ffIsSilicon ? RGB(147, 197, 253) : RGB(252, 165, 165))
                                            : (ffIsSilicon ? RGB(96, 115, 145) : RGB(160, 60, 60));

            fillRoundRect(hdc, p5X1, p5Y1, p5X2, p5Y2, 14, ffChassisBg, ffBorder, ffActive ? 2 : 1);

            // Circular Vintage Faceplate Disc Accent
            fillRoundRect(hdc, 1076 - 80, p5Y1 + 42, 1076 + 80, p5Y2 - 96, 80,
                          ffIsSilicon ? RGB(32, 38, 50) : RGB(45, 18, 20),
                          ffIsSilicon ? RGB(75, 90, 115) : RGB(90, 35, 40), 1);

            // Header Plaque (Deep Crimson / Dark Slate)
            fillRoundRect(hdc, p5X1 + 10, p5Y1 + 8, p5X2 - 10, p5Y1 + 48, 6,
                          ffIsSilicon ? RGB(20, 26, 36) : RGB(55, 12, 15),
                          ffIsSilicon ? RGB(96, 165, 250) : RGB(239, 68, 68), 1);

            // Vintage Dallas Arbiter Smiley Arc above footswitch
            HPEN smilePen = CreatePen(PS_SOLID, 2, ffActive ? RGB(254, 240, 138) : RGB(140, 100, 90));
            HGDIOBJ oldSmileP = SelectObject(hdc, smilePen);
            Arc(hdc, 1076 - 34, 466, 1076 + 34, 484, 1076 + 28, 474, 1076 - 28, 474);
            SelectObject(hdc, oldSmileP);
            DeleteObject(smilePen);

            // Chrome 3PDT Footswitch (Center: 1076, 498)
            fillRoundRect(hdc, 1076 - 28, 470, 1076 + 28, 526, 56, RGB(140, 140, 145), RGB(200, 200, 205), 2); // Hex washer
            fillRoundRect(hdc, 1076 - 20, 478, 1076 + 20, 518, 40, RGB(215, 215, 220), RGB(255, 255, 255), 2); // Chrome button
            fillRoundRect(hdc, 1076 - 12, 482, 1076 + 2, 496, 14, RGB(255, 255, 255), RGB(255, 255, 255), 1); // Specular highlight arc
        }

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, ffActive ? RGB(254, 240, 138) : RGB(220, 180, 160));
        RECT ffTitR = { p5X1 + 12, p5Y1 + 10, p5X2 - 12, p5Y1 + 34 };
        DrawTextA(hdc, "FUZZ FACE", -1, &ffTitR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontHud);
        SetTextColor(hdc, ffIsSilicon ? RGB(191, 219, 254) : RGB(254, 202, 202));
        RECT ffSubR = { p5X1 + 2, p5Y1 + 32, p5X2 - 2, p5Y1 + 46 };
        bool ffHyp = (g_knobs[K_FUZZ_HYPER].curVal > 0.5f);
        bool ffSi  = (g_knobs[K_FUZZ_TRANSISTOR].curVal > 0.5f);
        const char* ffSub = ffHyp ? (ffSi ? "HYPER BC108 (432Hz)" : "HYPER NKT275 (432Hz)")
                                  : (ffSi ? "(OLD) BC108 SILICON" : "(OLD) NKT275 GERMANIUM");
        DrawTextA(hdc, ffSub, -1, &ffSubR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // LED Lamp (Red for Germanium, Electric Cyan for Silicon)
        COLORREF ledColor = ffActive ? (ffIsSilicon ? RGB(56, 189, 248) : RGB(239, 68, 68))
                                     : (ffIsSilicon ? RGB(12, 38, 55)   : RGB(50, 12, 12));
        COLORREF ledGlow  = ffActive ? (ffIsSilicon ? RGB(186, 230, 253) : RGB(254, 202, 202))
                                     : (ffIsSilicon ? RGB(25, 60, 85)   : RGB(80, 20, 20));
        fillRoundRect(hdc, 1076 - 8, p5Y1 + 52, 1076 + 8, p5Y1 + 68, 16, ledColor, ledGlow, 1);
        if (ffActive) {
            fillRoundRect(hdc, 1076 - 3, p5Y1 + 56, 1076 + 3, p5Y1 + 62, 6, RGB(255, 255, 255), RGB(255, 255, 255), 1);
        }

        // Two-Button Volume Learn Calibration System: [LEARN MAX (10)] & [LEARN CLEAN (7)]
        bool isFfLearnMax = g_engine.isFuzzFaceLearningMax();
        bool isFfLearnClean = g_engine.isFuzzFaceLearningClean();
        ULONGLONG nowFf = GetTickCount64();
        if ((isFfLearnMax || isFfLearnClean) && (nowFf - g_fuzzLearnStartTime > 10500)) {
            g_engine.finishFuzzFaceLearning();
            isFfLearnMax = false;
            isFfLearnClean = false;
        }

        int secRemaining = 10;
        if (isFfLearnMax || isFfLearnClean) {
            ULONGLONG elapsed = nowFf - g_fuzzLearnStartTime;
            if (elapsed < 10000) {
                secRemaining = int(std::ceil((10000 - elapsed) / 1000.0f));
                if (secRemaining <= 0) secRemaining = 1;
            } else {
                secRemaining = 0;
            }
        }

        // 3D Visual Fuzz Position & Morph Meter Bar: 0% Clean 7 to 100% Full Bore 10
        float morphVal = g_engine.getFuzzFaceMorph();
        morphVal = std::max(0.0f, std::min(1.0f, morphVal));

        COLORREF meterTrackBg = RGB(12, 16, 20);
        COLORREF meterBorder  = ffActive ? (ffIsSilicon ? RGB(75, 90, 115) : RGB(90, 35, 40)) : RGB(40, 25, 25);
        fillRoundRect(hdc, g_fuzzMorphMeterRect.left, g_fuzzMorphMeterRect.top,
                      g_fuzzMorphMeterRect.right, g_fuzzMorphMeterRect.bottom, 4, meterTrackBg, meterBorder, 1);

        int meterInnerW = (g_fuzzMorphMeterRect.right - 2) - (g_fuzzMorphMeterRect.left + 2);
        int fillW = int(meterInnerW * morphVal);
        if (fillW > 2 && ffActive) {
            COLORREF fillCol = (morphVal < 0.35f) ? RGB(34, 211, 238)
                             : (morphVal < 0.70f) ? RGB(245, 158, 11)
                             : RGB(239, 68, 68);
            fillRoundRect(hdc, g_fuzzMorphMeterRect.left + 2, g_fuzzMorphMeterRect.top + 2,
                          g_fuzzMorphMeterRect.left + 2 + fillW, g_fuzzMorphMeterRect.bottom - 2, 2, fillCol, fillCol, 1);
        }

        SelectObject(hdc, fontHud);
        SetBkMode(hdc, TRANSPARENT);
        char morphTxt[48];
        if (isFfLearnMax || isFfLearnClean) {
            std::snprintf(morphTxt, sizeof(morphTxt), "CALIBRATING: %ds", secRemaining);
            SetTextColor(hdc, RGB(254, 240, 138));
        } else {
            std::snprintf(morphTxt, sizeof(morphTxt), "FUZZ MORPH: %.0f%%", morphVal * 100.0f);
            SetTextColor(hdc, ffActive ? RGB(255, 255, 255) : RGB(140, 120, 120));
        }
        DrawTextA(hdc, morphTxt, -1, &g_fuzzMorphMeterRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        RECT leftTagR = { g_fuzzMorphMeterRect.left + 4, g_fuzzMorphMeterRect.top, g_fuzzMorphMeterRect.left + 46, g_fuzzMorphMeterRect.bottom };
        SetTextColor(hdc, RGB(167, 243, 208));
        DrawTextA(hdc, "CLN 7", -1, &leftTagR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        RECT rightTagR = { g_fuzzMorphMeterRect.right - 50, g_fuzzMorphMeterRect.top, g_fuzzMorphMeterRect.right - 4, g_fuzzMorphMeterRect.bottom };
        SetTextColor(hdc, RGB(254, 202, 202));
        DrawTextA(hdc, "BORE 10", -1, &rightTagR, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

        // Button 1: Learn Max (Vol 10)
        COLORREF maxBg = isFfLearnMax ? RGB(180, 83, 9) : (ffIsSilicon ? RGB(24, 30, 42) : RGB(45, 12, 15));
        COLORREF maxBorder = isFfLearnMax ? RGB(251, 191, 36) : (ffIsSilicon ? RGB(96, 165, 250) : RGB(239, 68, 68));
        fillRoundRect(hdc, g_btnFuzzLearnMaxRect.left, g_btnFuzzLearnMaxRect.top,
                      g_btnFuzzLearnMaxRect.right, g_btnFuzzLearnMaxRect.bottom, 4, maxBg, maxBorder, isFfLearnMax ? 2 : 1);
        char maxTxt[32];
        if (isFfLearnMax) {
            float liveRMS = g_engine.getFuzzFaceLearnTracker();
            if (liveRMS >= 0.002f) {
                std::snprintf(maxTxt, sizeof(maxTxt), "%ds: %.3f", secRemaining, liveRMS);
            } else {
                std::snprintf(maxTxt, sizeof(maxTxt), "PLAY 10 (%ds)", secRemaining);
            }
            SetTextColor(hdc, RGB(254, 240, 138));
        } else {
            std::snprintf(maxTxt, sizeof(maxTxt), "MAX 10: %.3f", g_engine.getFuzzFaceCalibratedMax());
            SetTextColor(hdc, ffActive ? RGB(254, 240, 138) : RGB(200, 180, 170));
        }
        DrawTextA(hdc, maxTxt, -1, &g_btnFuzzLearnMaxRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Button 2: Learn Clean (Vol 7)
        COLORREF clnBg = isFfLearnClean ? RGB(6, 95, 70) : (ffIsSilicon ? RGB(24, 30, 42) : RGB(45, 12, 15));
        COLORREF clnBorder = isFfLearnClean ? RGB(52, 211, 153) : (ffIsSilicon ? RGB(96, 165, 250) : RGB(239, 68, 68));
        fillRoundRect(hdc, g_btnFuzzLearnCleanRect.left, g_btnFuzzLearnCleanRect.top,
                      g_btnFuzzLearnCleanRect.right, g_btnFuzzLearnCleanRect.bottom, 4, clnBg, clnBorder, isFfLearnClean ? 2 : 1);
        char clnTxt[32];
        if (isFfLearnClean) {
            float liveRMS = g_engine.getFuzzFaceLearnTracker();
            if (liveRMS >= 0.001f) {
                std::snprintf(clnTxt, sizeof(clnTxt), "%ds: %.3f", secRemaining, liveRMS);
            } else {
                std::snprintf(clnTxt, sizeof(clnTxt), "PLAY 7 (%ds)", secRemaining);
            }
            SetTextColor(hdc, RGB(167, 243, 208));
        } else {
            std::snprintf(clnTxt, sizeof(clnTxt), "CLN 7: %.3f", g_engine.getFuzzFaceCalibratedClean());
            SetTextColor(hdc, ffActive ? RGB(167, 243, 208) : RGB(180, 200, 190));
        }
        DrawTextA(hdc, clnTxt, -1, &g_btnFuzzLearnCleanRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        SetTextColor(hdc, ffActive ? RGB(254, 240, 138) : RGB(160, 120, 120));
        RECT ffFootLblR = { p5X1, p5Y2 - 24, p5X2, p5Y2 - 6 };
        DrawTextA(hdc, ffActive ? "[ACTIVE] BYPASS" : "[BYPASS] ACTIVATE", -1, &ffFootLblR, DT_CENTER | DT_SINGLELINE);
    }
    else if (g_activePage == 4) {
        // ====================================================================
        // PAGE 4: FLUID SPACE CHAMBER & MODULATION LAB (Topaz & Celestial Cyan)
        // ====================================================================
        Gdiplus::Image* imgSpace = g_assets3D.imgRackSpaceTopaz ? g_assets3D.imgRackSpaceTopaz : g_assets3D.imgChassisWide;
        if (imgSpace) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(imgSpace, bx1, by1, bx2 - bx1, by2 - by1);
        } else {
            fillRoundRect(hdc, bx1, by1, bx2, by2, 18, RGB(6, 24, 38), RGB(6, 182, 212), 3);
            drawCornerProtector3D(hdc, bx1, by1, 28, 28, 0, RGB(148, 163, 184), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx2 - 28, by1, 28, 28, 1, RGB(148, 163, 184), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx1, by2 - 28, 28, 28, 2, RGB(148, 163, 184), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx2 - 28, by2 - 28, 28, 28, 3, RGB(148, 163, 184), RGB(255, 255, 255));

            int fpTop = by1 + 16, fpBottom = by1 + 280;
            fillRoundRect(hdc, bx1 + 18, fpTop, bx2 - 18, fpBottom, 10, RGB(4, 28, 36), RGB(103, 232, 249), 2);
            fillRoundRect(hdc, bx1 + 24, fpTop + 8, bx2 - 24, fpBottom - 8, 8, RGB(2, 14, 20), RGB(34, 211, 238), 1);
        }

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(103, 232, 249));
        RECT subR = { bx1 + 38, by1 + 14, bx1 + 920, by1 + 32 };
        DrawTextA(hdc, "ACOUSTIC RESONATOR LAB  |  GEOMETRIC FLUID CHAMBER MORPH (CIRCLE <-> BOX) + 432 Hz SIZE LADDER", -1, &subR, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT titR = { bx1 + 38, by1 + 30, bx1 + 800, by1 + 58 };
        DrawTextA(hdc, "Euphoria Fluid Space Chamber & Modulator", -1, &titR, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        drawFacetedJewelLamp(hdc, bx2 - 68, by1 + 36, 16, RGB(8, 145, 178), RGB(103, 232, 249), RGB(255, 255, 255));

        // Lower Chamber Display Viewport — Seated Strictly Between 19" Rack Ears (152..1128)
        int grLeft = 152, grRight = 1128, grTop = 330, grBottom = by2 - 16;
        fillRoundRect(hdc, grLeft, grTop, grRight, grBottom, 8, RGB(2, 12, 18), RGB(6, 182, 212), 2);
        fillRoundRect(hdc, grLeft + 4, grTop + 4, grRight - 4, grBottom - 4, 6, RGB(1, 8, 12), RGB(8, 40, 50), 1);

        // 4 Corner Hex Mounting Screws on the Display Bezel
        fillRoundRect(hdc, grLeft + 7, grTop + 7, grLeft + 13, grTop + 13, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, grRight - 13, grTop + 7, grRight - 7, grTop + 13, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, grLeft + 7, grBottom - 13, grLeft + 13, grBottom - 7, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, grRight - 13, grBottom - 13, grRight - 7, grBottom - 7, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);

        // Center split divider line in viewport
        int viewMidX = (grLeft + grRight) / 2;
        HPEN splitPen = CreatePen(PS_DOT, 1, RGB(14, 116, 144));
        HGDIOBJ oldSpP = SelectObject(hdc, splitPen);
        MoveToEx(hdc, viewMidX, grTop + 10, nullptr);
        LineTo(hdc, viewMidX, grBottom - 10);
        SelectObject(hdc, oldSpP);
        DeleteObject(splitPen);

        // --- LEFT HALF: 3D Animated Morphing Fluid Chamber (Circle to Square) ---
        int leftCenterX = (grLeft + 18 + viewMidX) / 2;
        int centerY = (grTop + grBottom) / 2;

        int shapeIdx = std::max(0, std::min(10, int(std::round(g_knobs[K_SPACE_SHAPE].curVal))));
        static const float s_chamberShapeFactors[11] = {
            0.00f, 0.08f, 0.16f, 0.28f, 0.40f, 0.50f, 0.60f, 0.72f, 0.82f, 0.92f, 1.00f
        };
        float shapeFact = s_chamberShapeFactors[shapeIdx];

        int sizeIdx = std::max(0, std::min(18, int(std::round(g_knobs[K_SPACE_SIZE].curVal))));
        float dispR = 24.0f + (float(sizeIdx) / 18.0f) * 36.0f;

        bool modActive = (g_knobs[K_SPACE_MOD_ON].curVal > 0.5f);
        float modSpeed = g_knobs[K_SPACE_MOD_RATE].curVal;

        // Draw outer concentric resonance guide rings
        HPEN ringPen = CreatePen(PS_SOLID, 1, RGB(12, 74, 96));
        HGDIOBJ oldRP = SelectObject(hdc, ringPen);
        for (int r = 20; r <= 70; r += 16) {
            Arc(hdc, leftCenterX - r, centerY - r, leftCenterX + r, centerY + r, leftCenterX + r, centerY, leftCenterX + r, centerY);
        }
        SelectObject(hdc, oldRP);
        DeleteObject(ringPen);

        // Draw Morphing Polygon (36 points)
        POINT polyPts[37];
        const int numPts = 36;
        for (int p = 0; p <= numPts; ++p) {
            float theta = (float(p) / float(numPts)) * 6.2831853f;
            float cosT = std::cos(theta);
            float sinT = std::sin(theta);
            float maxCoord = std::max(std::abs(cosT), std::abs(sinT));
            float rSquare = dispR / std::max(0.001f, maxCoord);
            float rCircle = dispR;
            float rFinal = (1.0f - shapeFact) * rCircle + shapeFact * rSquare;

            if (modActive) {
                rFinal += std::sin(theta * 4.0f + g_animPhase * (modSpeed * 0.4f + 0.5f)) * 4.0f;
            }

            polyPts[p].x = leftCenterX + int(cosT * rFinal);
            polyPts[p].y = centerY + int(sinT * rFinal);
        }

        HBRUSH polyBr = CreateSolidBrush(RGB(6, 44, 58));
        HPEN polyPen = CreatePen(PS_SOLID, 2, RGB(34, 211, 238));
        HGDIOBJ oldPB = SelectObject(hdc, polyBr);
        HGDIOBJ oldPP = SelectObject(hdc, polyPen);
        Polygon(hdc, polyPts, numPts);
        SelectObject(hdc, oldPB);
        SelectObject(hdc, oldPP);
        DeleteObject(polyBr);
        DeleteObject(polyPen);

        // If square factor > 0.25, render 4 corner acoustic vortex eddy markers
        if (shapeFact > 0.25f) {
            float cornerDist = dispR * 1.32f;
            static const float s_cornerAngles[4] = { 0.785398f, 2.356194f, 3.92699f, 5.497787f };
            for (int c = 0; c < 4; ++c) {
                int ecx = leftCenterX + int(std::cos(s_cornerAngles[c]) * cornerDist);
                int ecy = centerY + int(std::sin(s_cornerAngles[c]) * cornerDist);
                int er = int(6.0f * shapeFact);
                fillRoundRect(hdc, ecx - er, ecy - er, ecx + er, ecy + er, er * 2, RGB(251, 191, 36), RGB(245, 158, 11), 1);
            }
        }

        // Left Label
        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(103, 232, 249));
        char leftLbl[96];
        std::snprintf(leftLbl, sizeof(leftLbl), "CHAMBER: %s  |  432Hz SIZE: %s",
                      g_knobs[K_SPACE_SHAPE].stepLabels[shapeIdx], g_knobs[K_SPACE_SIZE].stepLabels[sizeIdx]);
        RECT leftLblR = { grLeft + 16, grBottom - 24, viewMidX - 10, grBottom - 6 };
        DrawTextA(hdc, leftLbl, -1, &leftLblR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // --- RIGHT HALF: Harmonic 432 Hz Resonance Oscilloscope & Modulation Phase Waveform ---
        int oscLeft = viewMidX + 24;
        int oscRight = grRight - 20;
        int oscTop = grTop + 14;
        int oscBottom = grBottom - 28;
        int oscW = oscRight - oscLeft;
        int oscMidY = (oscTop + oscBottom) / 2;

        // Oscilloscope grid
        HPEN gridPen = CreatePen(PS_DOT, 1, RGB(10, 50, 65));
        HGDIOBJ oldGP = SelectObject(hdc, gridPen);
        for (int gy = oscTop + 12; gy < oscBottom; gy += 16) {
            MoveToEx(hdc, oscLeft, gy, nullptr);
            LineTo(hdc, oscRight, gy);
        }
        for (int gx = oscLeft + 24; gx < oscRight; gx += 40) {
            MoveToEx(hdc, gx, oscTop, nullptr);
            LineTo(hdc, gx, oscBottom);
        }
        SelectObject(hdc, oldGP);
        DeleteObject(gridPen);

        // Trace 1: 432 Hz Golden Ratio Standing Wave
        HPEN wave432Pen = CreatePen(PS_SOLID, 2, RGB(16, 185, 129));
        HGDIOBJ oldW432 = SelectObject(hdc, wave432Pen);
        MoveToEx(hdc, oscLeft, oscMidY, nullptr);
        for (int x = oscLeft; x <= oscRight; ++x) {
            float fx = float(x - oscLeft) / float(oscW);
            float standingWave = std::sin(fx * 24.0f + g_animPhase * 3.0f) * 12.0f
                               + std::sin(fx * 38.83f + g_animPhase * 4.85f) * 5.0f;
            LineTo(hdc, x, oscMidY + int(standingWave));
        }
        SelectObject(hdc, oldW432);
        DeleteObject(wave432Pen);

        // Trace 2: LFO Modulation Trace (Gold/Amber when active)
        if (modActive) {
            HPEN modWavePen = CreatePen(PS_SOLID, 2, RGB(251, 191, 36));
            HGDIOBJ oldMW = SelectObject(hdc, modWavePen);
            MoveToEx(hdc, oscLeft, oscMidY, nullptr);
            int waveType = std::max(0, std::min(3, int(std::round(g_knobs[K_SPACE_WAVEFORM].curVal))));
            for (int x = oscLeft; x <= oscRight; ++x) {
                float fx = float(x - oscLeft) / float(oscW);
                float lfoVal = 0.0f;
                float phase = fx * 6.2831853f * 2.0f + g_animPhase * modSpeed;
                float normPhase = std::fmod(phase / 6.2831853f, 1.0f);
                if (normPhase < 0.0f) normPhase += 1.0f;

                if (waveType == 0) {
                    lfoVal = std::sin(phase);
                } else if (waveType == 1) {
                    lfoVal = (normPhase < 0.5f) ? (4.0f * normPhase - 1.0f) : (3.0f - 4.0f * normPhase);
                } else if (waveType == 2) {
                    lfoVal = 0.65f * std::sin(phase) + 0.35f * std::sin(phase * 1.618034f);
                } else {
                    lfoVal = 0.50f * std::sin(phase) + 0.30f * std::sin(phase * 0.707f) + 0.20f * std::sin(phase * 2.14f);
                }
                LineTo(hdc, x, oscMidY - int(lfoVal * 20.0f));
            }
            SelectObject(hdc, oldMW);
            DeleteObject(modWavePen);
        }

        // Right Label
        SelectObject(hdc, fontSub);
        SetTextColor(hdc, modActive ? RGB(251, 191, 36) : RGB(148, 163, 184));
        char rightLbl[96];
        if (modActive) {
            std::snprintf(rightLbl, sizeof(rightLbl), "SPACE MOD LFO: ACTIVE (%.1f Hz %s  |  SPREAD: %.0f deg)",
                          modSpeed, g_knobs[K_SPACE_WAVEFORM].stepLabels[std::max(0, std::min(3, int(std::round(g_knobs[K_SPACE_WAVEFORM].curVal))))],
                          g_knobs[K_SPACE_SPREAD].curVal);
        } else {
            std::snprintf(rightLbl, sizeof(rightLbl), "SPACE MODULATION: BYPASSED (432Hz HARMONIC MATRIX STABLE)");
        }
        RECT rightLblR = { viewMidX + 10, grBottom - 24, grRight - 16, grBottom - 6 };
        DrawTextA(hdc, rightLbl, -1, &rightLblR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    else if (g_activePage == 5) {
        // ====================================================================
        // PAGE 5: DUAL RACKS — REVERB SPACE (Pink) & FENDER 6G TREMOLO (Orange)
        // Built-In Hardware Displays on Right Bays (x: 680..1128)
        // ====================================================================
        int r1Y1 = 104, r1Y2 = 380;
        int r2Y1 = 398, r2Y2 = 674;

        // --------------------------------------------------------------------
        // RACK 1 (UPPER): EUPHORIA FLUID REVERB SPACE (Boutique Pink Rack)
        // --------------------------------------------------------------------
        Gdiplus::Image* imgRev = g_assets3D.imgRackReverbPink ? g_assets3D.imgRackReverbPink : g_assets3D.imgChassisWide;
        if (imgRev) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(imgRev, bx1, r1Y1, bx2 - bx1, r1Y2 - r1Y1);
        } else {
            fillRoundRect(hdc, bx1, r1Y1, bx2, r1Y2, 14, RGB(55, 10, 35), RGB(244, 114, 182), 2);
            drawCornerProtector3D(hdc, bx1, r1Y1, 24, 24, 0, RGB(251, 113, 133), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx2 - 24, r1Y1, 24, 24, 1, RGB(251, 113, 133), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx1, r1Y2 - 24, 24, 24, 2, RGB(251, 113, 133), RGB(255, 255, 255));
            drawCornerProtector3D(hdc, bx2 - 24, r1Y2 - 24, 24, 24, 3, RGB(251, 113, 133), RGB(255, 255, 255));
        }

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(251, 113, 133));
        RECT subR1 = { bx1 + 38, r1Y1 + 16, bx1 + 650, r1Y1 + 36 };
        DrawTextA(hdc, "RACK UNIT 1  |  PURE FLUID REVERB SPACE (FDN 432Hz CAVITY & DIFFUSION)", -1, &subR1, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(255, 240, 245));
        RECT titR1 = { bx1 + 38, r1Y1 + 34, bx1 + 600, r1Y1 + 70 };
        DrawTextA(hdc, "Euphoria Fluid Reverb Space", -1, &titR1, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        drawFacetedJewelLamp(hdc, 642, r1Y1 + 42, 16, RGB(190, 24, 93), RGB(244, 114, 182), RGB(255, 255, 255));

        bool revActive = (g_knobs[K_REV_ON].curVal > 0.5f);
        int revShapeIdx = std::max(0, std::min(10, int(std::round(g_knobs[K_REV_SHAPE].curVal))));
        int revSizeIdx = std::max(0, std::min(18, int(std::round(g_knobs[K_REV_SIZE].curVal))));

        // --- UPPER RACK RIGHT HALF: Integrated Fluid Acoustic Cavity & Reflection Tank Display ---
        int revBayX1 = 680, revBayX2 = 1128;
        int revBayY1 = r1Y1 + 48, revBayY2 = r1Y2 - 18;
        fillRoundRect(hdc, revBayX1, revBayY1, revBayX2, revBayY2, 8, RGB(24, 8, 18), RGB(244, 114, 182), 2);
        fillRoundRect(hdc, revBayX1 + 4, revBayY1 + 4, revBayX2 - 4, revBayY2 - 4, 6, RGB(14, 4, 10), RGB(70, 15, 45), 1);

        // 4 Corner Hex Mounting Screws on Display Bezel
        fillRoundRect(hdc, revBayX1 + 6, revBayY1 + 6, revBayX1 + 11, revBayY1 + 11, 4, RGB(251, 113, 133), RGB(136, 19, 55), 1);
        fillRoundRect(hdc, revBayX2 - 11, revBayY1 + 6, revBayX2 - 6, revBayY1 + 11, 4, RGB(251, 113, 133), RGB(136, 19, 55), 1);
        fillRoundRect(hdc, revBayX1 + 6, revBayY2 - 11, revBayX1 + 11, revBayY2 - 6, 4, RGB(251, 113, 133), RGB(136, 19, 55), 1);
        fillRoundRect(hdc, revBayX2 - 11, revBayY2 - 11, revBayX2 - 6, revBayY2 - 6, 4, RGB(251, 113, 133), RGB(136, 19, 55), 1);

        // Display Header Banner
        SelectObject(hdc, fontHud);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, revActive ? RGB(251, 113, 133) : RGB(148, 163, 184));
        RECT revDispHeadR = { revBayX1 + 12, revBayY1 + 6, revBayX2 - 12, revBayY1 + 20 };
        DrawTextA(hdc, "FDN 432Hz CAVITY ACOUSTICS & STEREO DIFFUSION TANK", -1, &revDispHeadR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Animated Concentric Acoustic Ripple Waves in Tank
        int cavMidX = (revBayX1 + revBayX2) / 2;
        int cavMidY = (revBayY1 + revBayY2) / 2 + 2;

        if (revActive) {
            // Draw 4 expanding fluid wavefront rings
            for (int wIdx = 0; wIdx < 4; ++wIdx) {
                float waveFrac = std::fmod((g_animPhase * 0.45f + float(wIdx) * 0.25f), 1.0f);
                int wrX = int(14.0f + waveFrac * 170.0f);
                int wrY = int(8.0f + waveFrac * 70.0f);
                int alphaVal = int((1.0f - waveFrac) * 220.0f);
                COLORREF waveCol = RGB(std::min(255, 120 + alphaVal / 2), std::min(255, 30 + alphaVal / 4), std::min(255, 90 + alphaVal / 2));
                HPEN wPen = CreatePen(PS_SOLID, (wIdx % 2 == 0) ? 2 : 1, waveCol);
                HGDIOBJ oldWP = SelectObject(hdc, wPen);
                Arc(hdc, cavMidX - wrX, cavMidY - wrY, cavMidX + wrX, cavMidY + wrY, cavMidX + wrX, cavMidY, cavMidX + wrX, cavMidY);
                SelectObject(hdc, oldWP);
                DeleteObject(wPen);
            }

            // Draw Acoustic Reflection Rays Bouncing Across Cavity
            HPEN rayPen = CreatePen(PS_SOLID, 1, RGB(253, 224, 71));
            HGDIOBJ oldRayP = SelectObject(hdc, rayPen);
            for (int r = 0; r < 6; ++r) {
                float rayAngle = float(r) * 1.047197f + g_animPhase * 0.2f;
                int rx = cavMidX + int(std::cos(rayAngle) * 130.0f);
                int ry = cavMidY + int(std::sin(rayAngle) * 55.0f);
                MoveToEx(hdc, cavMidX, cavMidY, nullptr);
                LineTo(hdc, rx, ry);
            }
            SelectObject(hdc, oldRayP);
            DeleteObject(rayPen);

            // Center transducer emitter jewel
            fillRoundRect(hdc, cavMidX - 6, cavMidY - 6, cavMidX + 6, cavMidY + 6, 12, RGB(244, 114, 182), RGB(255, 255, 255), 1);
        } else {
            // Static bypassed marker
            HPEN idlePen = CreatePen(PS_DOT, 1, RGB(70, 20, 45));
            HGDIOBJ oldIP = SelectObject(hdc, idlePen);
            MoveToEx(hdc, revBayX1 + 20, cavMidY, nullptr);
            LineTo(hdc, revBayX2 - 20, cavMidY);
            SelectObject(hdc, oldIP);
            DeleteObject(idlePen);
        }

        // Display Bottom Readout
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, revActive ? RGB(253, 224, 71) : RGB(148, 163, 184));
        char revBottomTxt[96];
        if (revActive) {
            std::snprintf(revBottomTxt, sizeof(revBottomTxt), "CAVITY: %s  |  SIZE: %s  |  DWELL: %.1f  |  MIX: %.0f%%",
                          g_knobs[K_REV_SHAPE].stepLabels[revShapeIdx], g_knobs[K_REV_SIZE].stepLabels[revSizeIdx],
                          g_knobs[K_REV_DWELL].curVal, g_knobs[K_REV_MIX].curVal);
        } else {
            std::snprintf(revBottomTxt, sizeof(revBottomTxt), "REVERB ENGINE: BYPASSED (TRUE ZERO-BLEED DRY PASS)");
        }
        RECT revDispBotR = { revBayX1 + 10, revBayY2 - 20, revBayX2 - 10, revBayY2 - 5 };
        DrawTextA(hdc, revBottomTxt, -1, &revDispBotR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // --------------------------------------------------------------------
        // RACK 2 (LOWER): VINTAGE FENDER 6G HARMONIC TREMOLO (Vintage Orange Rack)
        // --------------------------------------------------------------------
        Gdiplus::Image* imgTrem = g_assets3D.imgRackTremOrange ? g_assets3D.imgRackTremOrange : g_assets3D.imgChassisWide;
        if (imgTrem) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(imgTrem, bx1, r2Y1, bx2 - bx1, r2Y2 - r2Y1);
        } else {
            fillRoundRect(hdc, bx1, r2Y1, bx2, r2Y2, 14, RGB(45, 18, 5), RGB(249, 115, 22), 2);
            drawCornerProtector3D(hdc, bx1, r2Y1, 24, 24, 0, RGB(251, 146, 60), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 24, r2Y1, 24, 24, 1, RGB(251, 146, 60), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx1, r2Y2 - 24, 24, 24, 2, RGB(251, 146, 60), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 24, r2Y2 - 24, 24, 24, 3, RGB(251, 146, 60), RGB(254, 240, 138));
        }

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(251, 146, 60));
        RECT subR2 = { bx1 + 38, r2Y1 + 16, bx1 + 560, r2Y1 + 36 };
        DrawTextA(hdc, "RACK UNIT 2  |  VINTAGE FENDER 6G HARMONIC TREMOLO", -1, &subR2, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(255, 237, 213));
        RECT titR2 = { bx1 + 38, r2Y1 + 34, bx1 + 560, r2Y1 + 70 };
        DrawTextA(hdc, "Fender 6G Harmonic Tremolo", -1, &titR2, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        drawFacetedJewelLamp(hdc, 642, r2Y1 + 42, 16, RGB(194, 65, 12), RGB(249, 115, 22), RGB(254, 240, 138));

        // --- LOWER RACK RIGHT HALF: Vintage Fender 6G Harmonic Tremolo Twin Anti-Phase Oscilloscope ---
        int tremBayX1 = 680, tremBayX2 = 1128;
        int tremBayY1 = r2Y1 + 48, tremBayY2 = r2Y2 - 18;
        fillRoundRect(hdc, tremBayX1, tremBayY1, tremBayX2, tremBayY2, 8, RGB(18, 8, 4), RGB(249, 115, 22), 2);
        fillRoundRect(hdc, tremBayX1 + 4, tremBayY1 + 4, tremBayX2 - 4, tremBayY2 - 4, 6, RGB(10, 4, 2), RGB(75, 30, 10), 1);

        // 4 Corner Hex Mounting Screws on Display Bezel
        fillRoundRect(hdc, tremBayX1 + 6, tremBayY1 + 6, tremBayX1 + 11, tremBayY1 + 11, 4, RGB(251, 146, 60), RGB(124, 45, 18), 1);
        fillRoundRect(hdc, tremBayX2 - 11, tremBayY1 + 6, tremBayX2 - 6, tremBayY1 + 11, 4, RGB(251, 146, 60), RGB(124, 45, 18), 1);
        fillRoundRect(hdc, tremBayX1 + 6, tremBayY2 - 11, tremBayX1 + 11, tremBayY2 - 6, 4, RGB(251, 146, 60), RGB(124, 45, 18), 1);
        fillRoundRect(hdc, tremBayX2 - 11, tremBayY2 - 11, tremBayX2 - 6, tremBayY2 - 6, 4, RGB(251, 146, 60), RGB(124, 45, 18), 1);

        int oscLeft = tremBayX1 + 16;
        int oscRight = tremBayX2 - 16;
        int oscTop = tremBayY1 + 22;
        int oscBottom = tremBayY2 - 24;
        int oscW = oscRight - oscLeft;
        int oscMidY = (oscTop + oscBottom) / 2;

        // Amber Phosphor Graticule Grid
        HPEN gridPen = CreatePen(PS_DOT, 1, RGB(55, 24, 10));
        HGDIOBJ oldGP = SelectObject(hdc, gridPen);
        for (int gy = oscTop + 10; gy < oscBottom; gy += 16) {
            MoveToEx(hdc, oscLeft + 4, gy, nullptr);
            LineTo(hdc, oscRight - 4, gy);
        }
        for (int gx = oscLeft + 24; gx < oscRight; gx += 40) {
            MoveToEx(hdc, gx, oscTop + 4, nullptr);
            LineTo(hdc, gx, oscBottom - 4);
        }
        SelectObject(hdc, oldGP);
        DeleteObject(gridPen);

        bool tremActive = (g_knobs[K_VIB_ON].curVal > 0.5f);
        float tremSpeed = g_knobs[K_VIB_SPEED].curVal;
        float tremDepth = g_knobs[K_VIB_DEPTH].curVal * 0.10f; // 0..1.0
        bool isHarmonic = (g_knobs[K_VIB_MODE].curVal < 0.5f);

        if (tremActive) {
            // Trace A: Low-band modulation (Orange Tone)
            HPEN lowPen = CreatePen(PS_SOLID, 2, RGB(249, 115, 22));
            HGDIOBJ oldLP = SelectObject(hdc, lowPen);
            MoveToEx(hdc, oscLeft, oscMidY, nullptr);
            for (int x = oscLeft; x <= oscRight; ++x) {
                float fx = float(x - oscLeft) / float(oscW);
                float phase = fx * 6.2831853f * 2.0f + g_animPhase * tremSpeed;
                float modLow = std::sin(phase) * tremDepth * 18.0f;
                LineTo(hdc, x, oscMidY - int(modLow));
            }
            SelectObject(hdc, oldLP);
            DeleteObject(lowPen);

            // Trace B: High-band anti-phase modulation (Gold/Amber, 180 deg out of phase in Harmonic mode)
            HPEN highPen = CreatePen(PS_SOLID, 2, RGB(251, 191, 36));
            HGDIOBJ oldHP = SelectObject(hdc, highPen);
            MoveToEx(hdc, oscLeft, oscMidY, nullptr);
            for (int x = oscLeft; x <= oscRight; ++x) {
                float fx = float(x - oscLeft) / float(oscW);
                float phase = fx * 6.2831853f * 2.0f + g_animPhase * tremSpeed + (isHarmonic ? 3.14159265f : 0.0f);
                float modHigh = std::sin(phase) * tremDepth * 18.0f;
                LineTo(hdc, x, oscMidY - int(modHigh));
            }
            SelectObject(hdc, oldHP);
            DeleteObject(highPen);
        } else {
            HPEN flatPen = CreatePen(PS_SOLID, 1, RGB(90, 40, 15));
            HGDIOBJ oldFP = SelectObject(hdc, flatPen);
            MoveToEx(hdc, oscLeft, oscMidY, nullptr);
            LineTo(hdc, oscRight, oscMidY);
            SelectObject(hdc, oldFP);
            DeleteObject(flatPen);
        }

        SelectObject(hdc, fontHud);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, tremActive ? RGB(254, 240, 138) : RGB(148, 163, 184));
        char tremRightLbl[128];
        if (tremActive) {
            std::snprintf(tremRightLbl, sizeof(tremRightLbl), "FENDER 6G TREMOLO: %s (%.1f Hz  |  DEPTH: %.1f/10)",
                          isHarmonic ? "HARMONIC 180 deg ANTI-PHASE TUBE" : "OPTO PHOTOCELL CHOP",
                          tremSpeed, g_knobs[K_VIB_DEPTH].curVal);
        } else {
            std::snprintf(tremRightLbl, sizeof(tremRightLbl), "FENDER 6G TREMOLO: BYPASSED");
        }
        RECT tremRightLblR = { tremBayX1 + 10, tremBayY2 - 20, tremBayX2 - 10, tremBayY2 - 5 };
        DrawTextA(hdc, tremRightLbl, -1, &tremRightLblR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    else if (g_activePage == 6) {
        // ====================================================================
        // PAGE 6: EUPHORIA STUDIO TAPE DELAY (Vintage British Racing Green / Sage)
        // Integrated 19" Rack-Mount Reel-to-Reel Tape Transport Bay (152..1128)
        // ====================================================================
        Gdiplus::Image* imgDelay = g_assets3D.imgRackDelayGreen ? g_assets3D.imgRackDelayGreen : g_assets3D.imgChassisWide;
        if (imgDelay) {
            Gdiplus::Graphics g(hdc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(imgDelay, bx1, by1, bx2 - bx1, by2 - by1);
        } else {
            fillRoundRect(hdc, bx1, by1, bx2, by2, 18, RGB(16, 44, 30), RGB(34, 197, 94), 3);
            drawCornerProtector3D(hdc, bx1, by1, 28, 28, 0, RGB(251, 191, 36), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 28, by1, 28, 28, 1, RGB(251, 191, 36), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx1, by2 - 28, 28, 28, 2, RGB(251, 191, 36), RGB(254, 240, 138));
            drawCornerProtector3D(hdc, bx2 - 28, by2 - 28, 28, 28, 3, RGB(251, 191, 36), RGB(254, 240, 138));

            int fpTop = by1 + 16, fpBottom = by1 + 280;
            fillRoundRect(hdc, bx1 + 18, fpTop, bx2 - 18, fpBottom, 10, RGB(8, 28, 18), RGB(74, 222, 128), 2);
            fillRoundRect(hdc, bx1 + 24, fpTop + 8, bx2 - 24, fpBottom - 8, 8, RGB(0, 0, 0), RGB(34, 197, 94), 1);
        }

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(167, 243, 208));
        RECT subR = { bx1 + 38, by1 + 14, bx1 + 880, by1 + 32 };
        DrawTextA(hdc, "LIVE MIXER AUX SEND 3-4 STUDIO 2\" MAGNETIC FLUID TAPE ECHO & PING-PONG", -1, &subR, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(254, 252, 232));
        RECT titR = { bx1 + 38, by1 + 30, bx1 + 880, by1 + 58 };
        DrawTextA(hdc, "Euphoria Studio 15/30 IPS Tape Delay", -1, &titR, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        drawFacetedJewelLamp(hdc, bx2 - 68, by1 + 36, 16, RGB(22, 101, 52), RGB(74, 222, 128), RGB(254, 240, 138));

        // Integrated Studio Tape Transport Bay — Seated Strictly Between 19" Rack Ears (152..1128)
        int grLeft = 152, grRight = 1128, grTop = 330, grBottom = by2 - 16;
        fillRoundRect(hdc, grLeft, grTop, grRight, grBottom, 8, RGB(6, 20, 14), RGB(34, 197, 94), 2);
        fillRoundRect(hdc, grLeft + 4, grTop + 4, grRight - 4, grBottom - 4, 6, RGB(3, 10, 7), RGB(16, 50, 30), 1);

        // 4 Heavy Corner Hex Screws on Transport Faceplate
        fillRoundRect(hdc, grLeft + 7, grTop + 7, grLeft + 13, grTop + 13, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, grRight - 13, grTop + 7, grRight - 7, grTop + 13, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, grLeft + 7, grBottom - 13, grLeft + 13, grBottom - 7, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, grRight - 13, grBottom - 13, grRight - 7, grBottom - 7, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);

        // Inset VFD Digital Tape Counter & Telemetry Readout
        RECT vfdRect = { 510, grTop + 16, 770, grTop + 48 };
        fillRoundRect(hdc, vfdRect.left, vfdRect.top, vfdRect.right, vfdRect.bottom, 4, RGB(2, 16, 12), RGB(20, 184, 166), 1);
        SelectObject(hdc, fontHud);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(45, 212, 191));
        bool dlyActive = (g_knobs[K_DLY_ON].curVal > 0.5f);
        wchar_t vfdBuf[96];
        if (dlyActive) {
            swprintf_s(vfdBuf, L"\x25CF 15 IPS  |  REEL A: 00:24:18.6  |  WOW: %.1f%%", g_knobs[K_DLY_FLUTTER].curVal);
        } else {
            swprintf_s(vfdBuf, L"TAPE TRANSPORT: STOPPED (BYPASS)");
        }
        DrawTextW(hdc, vfdBuf, -1, &vfdRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        // --- Twin Studio Analog VU Meters Flanking the Reels ---
        // Left VU Meter: Input / Send Level
        RECT vuLeftRect = { grLeft + 20, grTop + 65, grLeft + 118, grTop + 165 };
        fillRoundRect(hdc, vuLeftRect.left, vuLeftRect.top, vuLeftRect.right, vuLeftRect.bottom, 6, RGB(20, 24, 22), RGB(71, 85, 105), 2);
        fillRoundRect(hdc, vuLeftRect.left + 4, vuLeftRect.top + 4, vuLeftRect.right - 4, vuLeftRect.bottom - 4, 4, RGB(254, 243, 199), RGB(180, 160, 120), 1); // warm cream dial
        // VU Arc scale
        HPEN arcPen = CreatePen(PS_SOLID, 2, RGB(40, 30, 20));
        HGDIOBJ oldArcP = SelectObject(hdc, arcPen);
        Arc(hdc, vuLeftRect.left + 8, vuLeftRect.top + 10, vuLeftRect.right - 8, vuLeftRect.bottom + 20, vuLeftRect.right - 14, vuLeftRect.top + 26, vuLeftRect.left + 14, vuLeftRect.top + 26);
        SelectObject(hdc, oldArcP);
        DeleteObject(arcPen);
        // Needle Left
        float vuInVal = std::min(1.0f, g_inMeterPeak.load());
        float needleAngleL = -2.35f + vuInVal * 1.57f;
        int nPivotLX = (vuLeftRect.left + vuLeftRect.right) / 2;
        int nPivotLY = vuLeftRect.bottom - 6;
        HPEN needlePenL = CreatePen(PS_SOLID, 2, (vuInVal > 0.7f) ? RGB(239, 68, 68) : RGB(20, 20, 20));
        HGDIOBJ oldNP = SelectObject(hdc, needlePenL);
        MoveToEx(hdc, nPivotLX, nPivotLY, nullptr);
        LineTo(hdc, nPivotLX + int(std::cos(needleAngleL) * 38.0f), nPivotLY + int(std::sin(needleAngleL) * 38.0f));
        SelectObject(hdc, oldNP);
        DeleteObject(needlePenL);
        fillRoundRect(hdc, nPivotLX - 4, nPivotLY - 4, nPivotLX + 4, nPivotLY + 4, 8, RGB(20, 20, 20), RGB(20, 20, 20), 1);
        RECT vuLeftLbl = { vuLeftRect.left, vuLeftRect.bottom + 3, vuLeftRect.right, vuLeftRect.bottom + 17 };
        SetTextColor(hdc, RGB(167, 243, 208));
        DrawTextA(hdc, "INPUT VU", -1, &vuLeftLbl, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Right VU Meter: Output / Repro Level
        RECT vuRightRect = { grRight - 118, grTop + 65, grRight - 20, grTop + 165 };
        fillRoundRect(hdc, vuRightRect.left, vuRightRect.top, vuRightRect.right, vuRightRect.bottom, 6, RGB(20, 24, 22), RGB(71, 85, 105), 2);
        fillRoundRect(hdc, vuRightRect.left + 4, vuRightRect.top + 4, vuRightRect.right - 4, vuRightRect.bottom - 4, 4, RGB(254, 243, 199), RGB(180, 160, 120), 1);
        HPEN arcPenR = CreatePen(PS_SOLID, 2, RGB(40, 30, 20));
        HGDIOBJ oldArcPR = SelectObject(hdc, arcPenR);
        Arc(hdc, vuRightRect.left + 8, vuRightRect.top + 10, vuRightRect.right - 8, vuRightRect.bottom + 20, vuRightRect.right - 14, vuRightRect.top + 26, vuRightRect.left + 14, vuRightRect.top + 26);
        SelectObject(hdc, oldArcPR);
        DeleteObject(arcPenR);
        // Needle Right
        float vuOutVal = std::min(1.0f, g_outMeterPeakL.load());
        float needleAngleR = -2.35f + vuOutVal * 1.57f;
        int nPivotRX = (vuRightRect.left + vuRightRect.right) / 2;
        int nPivotRY = vuRightRect.bottom - 6;
        HPEN needlePenR = CreatePen(PS_SOLID, 2, (vuOutVal > 0.7f) ? RGB(239, 68, 68) : RGB(20, 20, 20));
        HGDIOBJ oldNPR = SelectObject(hdc, needlePenR);
        MoveToEx(hdc, nPivotRX, nPivotRY, nullptr);
        LineTo(hdc, nPivotRX + int(std::cos(needleAngleR) * 38.0f), nPivotRY + int(std::sin(needleAngleR) * 38.0f));
        SelectObject(hdc, oldNPR);
        DeleteObject(needlePenR);
        fillRoundRect(hdc, nPivotRX - 4, nPivotRY - 4, nPivotRX + 4, nPivotRY + 4, 8, RGB(20, 20, 20), RGB(20, 20, 20), 1);
        RECT vuRightLbl = { vuRightRect.left, vuRightRect.bottom + 3, vuRightRect.right, vuRightRect.bottom + 17 };
        SetTextColor(hdc, RGB(251, 191, 36));
        DrawTextA(hdc, "REPRO VU", -1, &vuRightLbl, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // --- Dual Precision 70px Aluminum Tape Reels ---
        int reelY = (grTop + grBottom) / 2 + 10;
        int supplyX = 360;
        int takeupX = 920;
        int rr = 66;

        // Supply Reel (Left)
        // Outer dark magnetic tape pack
        fillRoundRect(hdc, supplyX - rr, reelY - rr, supplyX + rr, reelY + rr, rr * 2, RGB(55, 28, 16), RGB(28, 14, 8), 2);
        // 3-Point Aluminum Flange
        int aluR = rr - 14;
        fillRoundRect(hdc, supplyX - aluR, reelY - aluR, supplyX + aluR, reelY + aluR, aluR * 2, RGB(180, 185, 190), RGB(220, 225, 230), 2);
        HPEN spokePen1 = CreatePen(PS_SOLID, 4, RGB(245, 245, 250));
        HGDIOBJ oldSpoke1 = SelectObject(hdc, spokePen1);
        for (int sp = 0; sp < 3; ++sp) {
            float a = g_animPhase + sp * 2.094395f;
            MoveToEx(hdc, supplyX, reelY, nullptr);
            LineTo(hdc, supplyX + int(std::cos(a) * (aluR - 3)), reelY + int(std::sin(a) * (aluR - 3)));
        }
        SelectObject(hdc, oldSpoke1);
        DeleteObject(spokePen1);
        // Center NAB Locking Hub
        fillRoundRect(hdc, supplyX - 14, reelY - 14, supplyX + 14, reelY + 14, 28, RGB(220, 38, 38), RGB(255, 255, 255), 2);
        fillRoundRect(hdc, supplyX - 6, reelY - 6, supplyX + 6, reelY + 6, 12, RGB(30, 30, 35), RGB(180, 180, 185), 1);

        // Take-Up Reel (Right)
        fillRoundRect(hdc, takeupX - rr, reelY - rr, takeupX + rr, reelY + rr, rr * 2, RGB(55, 28, 16), RGB(28, 14, 8), 2);
        fillRoundRect(hdc, takeupX - aluR, reelY - aluR, takeupX + aluR, reelY + aluR, aluR * 2, RGB(180, 185, 190), RGB(220, 225, 230), 2);
        HPEN spokePen2 = CreatePen(PS_SOLID, 4, RGB(245, 245, 250));
        HGDIOBJ oldSpoke2 = SelectObject(hdc, spokePen2);
        for (int sp = 0; sp < 3; ++sp) {
            float a = g_animPhase + sp * 2.094395f;
            MoveToEx(hdc, takeupX, reelY, nullptr);
            LineTo(hdc, takeupX + int(std::cos(a) * (aluR - 3)), reelY + int(std::sin(a) * (aluR - 3)));
        }
        SelectObject(hdc, oldSpoke2);
        DeleteObject(spokePen2);
        fillRoundRect(hdc, takeupX - 14, reelY - 14, takeupX + 14, reelY + 14, 28, RGB(220, 38, 38), RGB(255, 255, 255), 2);
        fillRoundRect(hdc, takeupX - 6, reelY - 6, takeupX + 6, reelY + 6, 12, RGB(30, 30, 35), RGB(180, 180, 185), 1);

        // --- Center Precision Tape Headblock & Transport Island ---
        int headX1 = 490, headX2 = 790, headY1 = reelY - 45, headY2 = reelY + 55;
        fillRoundRect(hdc, headX1, headY1, headX2, headY2, 6, RGB(24, 28, 26), RGB(100, 116, 139), 2);
        fillRoundRect(hdc, headX1 + 3, headY1 + 3, headX2 - 3, headY2 - 3, 4, RGB(16, 20, 18), RGB(40, 50, 45), 1);

        // Magnetic Tape Path Threaded Across Heads
        HPEN tapePen = CreatePen(PS_SOLID, 5, RGB(78, 38, 20)); // 2" Tape Ribbon
        HGDIOBJ oldTP = SelectObject(hdc, tapePen);
        MoveToEx(hdc, supplyX, reelY + rr - 4, nullptr);
        LineTo(hdc, headX1 + 10, reelY + 6);
        LineTo(hdc, headX2 - 10, reelY + 6);
        LineTo(hdc, takeupX, reelY + rr - 4);
        SelectObject(hdc, oldTP);
        DeleteObject(tapePen);

        // 3 Studio Magnetic Heads
        // Head 1: ERASE Head (Left)
        fillRoundRect(hdc, 532, reelY - 14, 562, reelY + 22, 4, RGB(10, 10, 12), RGB(200, 170, 60), 2);
        HPEN eraseGap = CreatePen(PS_SOLID, 1, RGB(251, 191, 36));
        HGDIOBJ oldEG = SelectObject(hdc, eraseGap);
        MoveToEx(hdc, 547, reelY - 12, nullptr); LineTo(hdc, 547, reelY + 20);
        SelectObject(hdc, oldEG);
        DeleteObject(eraseGap);

        // Head 2: RECORD Head (Center)
        fillRoundRect(hdc, 625, reelY - 14, 655, reelY + 22, 4, RGB(203, 213, 225), RGB(239, 68, 68), 2);
        // Record active red jewel
        fillRoundRect(hdc, 637, reelY - 8, 643, reelY - 2, 4, dlyActive ? RGB(239, 68, 68) : RGB(60, 12, 12), RGB(255, 255, 255), 1);

        // Head 3: REPRO/PLAY Head (Right)
        fillRoundRect(hdc, 718, reelY - 14, 748, reelY + 22, 4, RGB(148, 163, 184), RGB(34, 197, 94), 2);
        // Play active green jewel
        fillRoundRect(hdc, 730, reelY - 8, 736, reelY - 2, 4, dlyActive ? RGB(34, 197, 94) : RGB(10, 50, 20), RGB(255, 255, 255), 1);

        // Guide Rollers with Ruby Pivot Jewels
        fillRoundRect(hdc, headX1 + 10, reelY - 8, headX1 + 24, reelY + 18, 14, RGB(203, 213, 225), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, headX1 + 15, reelY + 3, headX1 + 19, reelY + 7, 4, RGB(239, 68, 68), RGB(255, 255, 255), 1);

        fillRoundRect(hdc, headX2 - 24, reelY - 8, headX2 - 10, reelY + 18, 14, RGB(203, 213, 225), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, headX2 - 19, reelY + 3, headX2 - 15, reelY + 7, 4, RGB(239, 68, 68), RGB(255, 255, 255), 1);

        // Rubber Pinch Roller & Capstan Shaft (Far Right of Headblock)
        fillRoundRect(hdc, 760, reelY - 10, 776, reelY + 16, 14, RGB(18, 18, 20), RGB(60, 60, 65), 2);
        fillRoundRect(hdc, 755, reelY - 4, 759, reelY + 10, 4, RGB(240, 240, 245), RGB(180, 180, 185), 1); // spinning capstan

        // Transport Status Footer
        SelectObject(hdc, fontHud);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, dlyActive ? RGB(167, 243, 208) : RGB(148, 163, 184));
        char tapeStatusTxt[128];
        if (dlyActive) {
            std::snprintf(tapeStatusTxt, sizeof(tapeStatusTxt), "TAPE TRANSPORT: 15/30 IPS | TIME: %.0f ms | FEEDBACK: %.0f%% | DRIVE: %.0f%% | DUCK: %.0f%%",
                          g_knobs[K_DLY_TIME].curVal, g_knobs[K_DLY_FB].curVal, g_knobs[K_DLY_DRIVE].curVal, g_knobs[K_DLY_DUCK].curVal);
        } else {
            std::snprintf(tapeStatusTxt, sizeof(tapeStatusTxt), "TAPE TRANSPORT: BYPASSED (TRUE ANALOG ZERO-BLEED PASS)");
        }
        RECT tapeFootR = { grLeft + 20, grBottom - 22, grRight - 20, grBottom - 6 };
        DrawTextA(hdc, tapeStatusTxt, -1, &tapeFootR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    else if (g_activePage == 7) {
        // ====================================================================
        // PAGE 7: EUPHORIA PRO 0.1-CENT VIRTUAL STROBE TUNER
        // Real-Time YIN CMNDF Analysis with Sub-Cent Multi-Band Optical Strobe
        // ====================================================================
        fillRoundRect(hdc, bx1, by1, bx2, by2, 18, RGB(10, 16, 26), RGB(30, 41, 59), 3);
        drawCornerProtector3D(hdc, bx1, by1, 28, 28, 0, RGB(251, 191, 36), RGB(254, 240, 138));
        drawCornerProtector3D(hdc, bx2 - 28, by1, 28, 28, 1, RGB(251, 191, 36), RGB(254, 240, 138));
        drawCornerProtector3D(hdc, bx1, by2 - 28, 28, 28, 2, RGB(251, 191, 36), RGB(254, 240, 138));
        drawCornerProtector3D(hdc, bx2 - 28, by2 - 28, 28, 28, 3, RGB(251, 191, 36), RGB(254, 240, 138));

        SelectObject(hdc, fontSub);
        SetTextColor(hdc, RGB(56, 189, 248));
        RECT subR = { bx1 + 38, by1 + 14, bx1 + 880, by1 + 32 };
        DrawTextA(hdc, "LIVE MIXER PRECISION INSTRUMENT INFORMATICS  |  SUB-CENT ACCURACY (+/-0.05 ct)", -1, &subR, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, RGB(241, 245, 249));
        RECT titR = { bx1 + 38, by1 + 30, bx1 + 880, by1 + 58 };
        DrawTextA(hdc, "Euphoria Pro 0.1-Cent Virtual Strobe Tuner", -1, &titR, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        bool isMuted = g_tunerMute.load();
        drawFacetedJewelLamp(hdc, bx2 - 68, by1 + 36, 16, isMuted ? RGB(185, 28, 28) : RGB(14, 116, 144),
                             isMuted ? RGB(239, 68, 68) : RGB(56, 189, 248), RGB(255, 255, 255));

        // Main Display Chassis Console (dispLeft..dispRight)
        int dispLeft = 148, dispTop = by1 + 80, dispRight = 1000, dispBottom = by2 - 16;
        fillRoundRect(hdc, dispLeft, dispTop, dispRight, dispBottom, 10, RGB(4, 8, 14), RGB(14, 116, 144), 2);
        fillRoundRect(hdc, dispLeft + 4, dispTop + 4, dispRight - 4, dispBottom - 4, 8, RGB(2, 4, 8), RGB(8, 47, 73), 1);

        float curRef = g_tuner.refFreq.load();
        float detPitch = g_tuner.detectedPitch.load();
        float tgtPitch = g_tuner.targetPitch.load();
        float cents = g_tuner.cents.load();
        float clarity = g_tuner.confidence.load();
        float inRms = g_tuner.inRms.load();
        bool sigActive = g_tuner.signalActive.load();
        int bestStr = g_tuner.closestStringIdx.load();
        int nIdx = g_tuner.noteIdx.load();
        int octave = g_tuner.octave.load();
        char nName[8];
        g_tuner.getNoteName(nName);
        float sRate = g_tuner.getSampleRate();

        // Preview mock values if rendering screenshot and idle
        if (!sigActive && detPitch <= 1.0f) {
            detPitch = 82.38f;
            tgtPitch = 82.41f;
            cents = -0.6f;
            clarity = 0.985f;
            inRms = 0.125f;
            sigActive = true;
            bestStr = 0;
            nIdx = 4; // E
            octave = 2;
            strcpy(nName, "E");
        }

        // 1. Top Screen Status Header
        RECT dispInfoR = { dispLeft + 16, dispTop + 8, dispRight - 16, dispTop + 28 };
        SelectObject(hdc, fontHud);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(56, 189, 248));
        char dispInfoBuf[180];
        std::snprintf(dispInfoBuf, sizeof(dispInfoBuf),
                      "REFERENCE: A4 = %.1f Hz  |  STABILIZED CHROMATIC YIN PITCH ENGINE  |  DEVICE RATE: %.0f Hz (WASAPI LOCKED)",
                      curRef, sRate);
        DrawTextA(hdc, dispInfoBuf, -1, &dispInfoR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        bool inTune = sigActive && (std::abs(cents) <= 1.5f);

        // 2. Full 12-Tone Chromatic Scale Ribbon (dispLeft + 16 .. dispRight - 16, Y: dispTop + 32 .. dispTop + 68)
        static const char* const s_chromaticNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const bool s_isAccidental[12] = { false, true, false, true, false, false, true, false, true, false, true, false };
        int ribLeft = dispLeft + 16;
        int ribRight = dispRight - 16;
        int ribWidth = ribRight - ribLeft;
        int ribTop = dispTop + 32;
        int ribBot = dispTop + 68;

        for (int k = 0; k < 12; ++k) {
            int x1 = ribLeft + (k * ribWidth) / 12 + 2;
            int x2 = ribLeft + ((k + 1) * ribWidth) / 12 - 2;
            bool isCurrent = sigActive && (nIdx == k);

            COLORREF bgCol, brCol, txCol;
            if (isCurrent) {
                if (inTune) {
                    bgCol = RGB(6, 78, 59);
                    brCol = RGB(52, 211, 153);
                    txCol = RGB(255, 255, 255);
                } else if (cents < -1.5f) {
                    bgCol = RGB(12, 74, 110);
                    brCol = RGB(56, 189, 248);
                    txCol = RGB(224, 242, 254);
                } else {
                    bgCol = RGB(120, 53, 15);
                    brCol = RGB(251, 191, 36);
                    txCol = RGB(254, 240, 138);
                }
            } else {
                if (s_isAccidental[k]) {
                    bgCol = RGB(6, 10, 18);
                    brCol = RGB(23, 32, 48);
                    txCol = RGB(100, 116, 139);
                } else {
                    bgCol = RGB(12, 18, 28);
                    brCol = RGB(30, 41, 59);
                    txCol = RGB(148, 163, 184);
                }
            }

            fillRoundRect(hdc, x1, ribTop, x2, ribBot, 4, bgCol, brCol, isCurrent ? 2 : 1);

            RECT noteR = { x1, ribTop + 2, x2, ribBot - 2 };
            SelectObject(hdc, fontKnob);
            SetTextColor(hdc, txCol);
            DrawTextA(hdc, s_chromaticNames[k], -1, &noteR, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

            if (isCurrent) {
                int dotX = (x1 + x2) / 2;
                fillRoundRect(hdc, dotX - 3, ribBot - 6, dotX + 3, ribBot - 2, 2, brCol, brCol, 1);
            }
        }

        // 3. Five-Panel Note & Pitch Deviation Zone (dispTop + 74 .. dispTop + 158, Height: 84px)
        int centerDispX = (dispLeft + dispRight) / 2; // 574

        // Panel 1: Flat Indicator Box (Left: 164 .. 324)
        int flatX1 = dispLeft + 16, flatX2 = flatX1 + 160;
        int pY1 = dispTop + 74, pY2 = dispTop + 158;
        bool isFlat = sigActive && (cents < -1.5f);
        fillRoundRect(hdc, flatX1, pY1, flatX2, pY2, 6,
                      isFlat ? RGB(12, 74, 110) : RGB(8, 14, 22),
                      isFlat ? RGB(56, 189, 248) : RGB(23, 32, 48), isFlat ? 2 : 1);
        RECT flatR1 = { flatX1, pY1 + 10, flatX2, pY1 + 32 };
        RECT flatR2 = { flatX1, pY1 + 34, flatX2, pY1 + 54 };
        RECT flatR3 = { flatX1, pY1 + 56, flatX2, pY2 - 8 };
        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, isFlat ? RGB(56, 189, 248) : RGB(51, 65, 85));
        DrawTextW(hdc, L"◄ FLAT", -1, &flatR1, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, isFlat ? RGB(224, 242, 254) : RGB(71, 85, 105));
        DrawTextW(hdc, isFlat ? L"TUNE UP ▲" : L"--", -1, &flatR2, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (isFlat) {
            char fCtBuf[32];
            std::snprintf(fCtBuf, sizeof(fCtBuf), "%.1f Cents", cents);
            SetTextColor(hdc, RGB(125, 211, 252));
            DrawTextA(hdc, fCtBuf, -1, &flatR3, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Panel 2: Live Detected Frequency Card (330 .. 424)
        int liveX1 = flatX2 + 6, liveX2 = centerDispX - 145;
        fillRoundRect(hdc, liveX1, pY1, liveX2, pY2, 6, RGB(10, 16, 26), RGB(30, 41, 59), 1);
        RECT liveLblR = { liveX1, pY1 + 10, liveX2, pY1 + 28 };
        RECT liveValR = { liveX1, pY1 + 32, liveX2, pY1 + 56 };
        RECT liveSubR = { liveX1, pY1 + 58, liveX2, pY2 - 8 };
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, RGB(148, 163, 184));
        DrawTextA(hdc, "DETECTED", -1, &liveLblR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontKnob);
        SetTextColor(hdc, sigActive ? RGB(241, 245, 249) : RGB(100, 116, 139));
        char detHzBuf[32];
        if (sigActive) std::snprintf(detHzBuf, sizeof(detHzBuf), "%.2f Hz", detPitch);
        else std::strcpy(detHzBuf, "-- Hz");
        DrawTextA(hdc, detHzBuf, -1, &liveValR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, sigActive ? (inTune ? RGB(52, 211, 153) : RGB(56, 189, 248)) : RGB(71, 85, 105));
        DrawTextA(hdc, sigActive ? "STABILIZED" : "STANDBY", -1, &liveSubR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Panel 3: Center Chromatic Note Box (429 .. 719, width 290)
        int noteX1 = centerDispX - 140;
        int noteX2 = centerDispX + 140;
        COLORREF noteBorder = !sigActive ? RGB(51, 65, 85)
                            : inTune     ? RGB(52, 211, 153)
                            : (cents < 0)? RGB(56, 189, 248)
                                         : RGB(251, 191, 36);
        COLORREF noteBg = !sigActive ? RGB(15, 23, 42)
                        : inTune     ? RGB(6, 44, 30)
                        : (cents < 0)? RGB(8, 35, 58)
                                     : RGB(50, 32, 10);
        fillRoundRect(hdc, noteX1, pY1, noteX2, pY2, 8, noteBg, noteBorder, 2);

        HFONT fontBigNote = CreateFontA(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        HFONT fontSubOct  = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        HGDIOBJ oldNoteF = SelectObject(hdc, fontBigNote);
        SetTextColor(hdc, noteBorder);
        RECT noteLetterR = { noteX1, pY1 + 4, noteX2 - 28, pY1 + 52 };
        DrawTextA(hdc, sigActive ? nName : "--", -1, &noteLetterR, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        if (sigActive) {
            SelectObject(hdc, fontSubOct);
            char octBuf[8];
            std::snprintf(octBuf, sizeof(octBuf), "%d", octave);
            RECT octR = { noteX2 - 40, pY1 + 14, noteX2 - 8, pY1 + 44 };
            DrawTextA(hdc, octBuf, -1, &octR, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        }
        SelectObject(hdc, oldNoteF);
        DeleteObject(fontBigNote);
        DeleteObject(fontSubOct);

        // Tuning Status Pill inside note box
        int pillY1 = pY1 + 54, pillY2 = pY2 - 6;
        int pillX1 = noteX1 + 12, pillX2 = noteX2 - 12;
        COLORREF pBg = inTune ? RGB(6, 78, 59) : (sigActive ? (cents < 0 ? RGB(12, 74, 110) : RGB(120, 53, 15)) : RGB(15, 23, 42));
        COLORREF pBr = inTune ? RGB(52, 211, 153) : (sigActive ? (cents < 0 ? RGB(56, 189, 248) : RGB(251, 191, 36)) : RGB(30, 41, 59));
        fillRoundRect(hdc, pillX1, pillY1, pillX2, pillY2, 4, pBg, pBr, 1);
        RECT pillR = { pillX1, pillY1, pillX2, pillY2 };
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, inTune ? RGB(254, 240, 138) : (sigActive ? RGB(255, 255, 255) : RGB(148, 163, 184)));
        wchar_t statPillBuf[64];
        if (!sigActive) {
            std::wcscpy(statPillBuf, L"WAITING FOR SIGNAL");
        } else if (inTune) {
            swprintf_s(statPillBuf, L"★ IN TUNE ★  (%+.1f ct)", cents);
        } else if (cents < 0.0f) {
            swprintf_s(statPillBuf, L"▼ FLAT  (%+.1f ct)", cents);
        } else {
            swprintf_s(statPillBuf, L"▲ SHARP  (%+.1f ct)", cents);
        }
        DrawTextW(hdc, statPillBuf, -1, &pillR, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        // Panel 4: Target Note Frequency Card (724 .. 818)
        int tgtX1 = centerDispX + 145, tgtX2 = ribRight - 166;
        fillRoundRect(hdc, tgtX1, pY1, tgtX2, pY2, 6, RGB(10, 16, 26), RGB(30, 41, 59), 1);
        RECT tgtLblR = { tgtX1, pY1 + 10, tgtX2, pY1 + 28 };
        RECT tgtValR = { tgtX1, pY1 + 32, tgtX2, pY1 + 56 };
        RECT tgtSubR = { tgtX1, pY1 + 58, tgtX2, pY2 - 8 };
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, RGB(148, 163, 184));
        DrawTextA(hdc, "TARGET", -1, &tgtLblR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontKnob);
        SetTextColor(hdc, sigActive ? RGB(241, 245, 249) : RGB(100, 116, 139));
        char tgtHzBuf[32];
        if (sigActive) std::snprintf(tgtHzBuf, sizeof(tgtHzBuf), "%.2f Hz", tgtPitch);
        else std::strcpy(tgtHzBuf, "-- Hz");
        DrawTextA(hdc, tgtHzBuf, -1, &tgtValR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, sigActive ? RGB(56, 189, 248) : RGB(71, 85, 105));
        char tgtNoteOctBuf[32];
        if (sigActive) std::snprintf(tgtNoteOctBuf, sizeof(tgtNoteOctBuf), "%s%d NOTE", nName, octave);
        else std::strcpy(tgtNoteOctBuf, "CHROMATIC");
        DrawTextA(hdc, tgtNoteOctBuf, -1, &tgtSubR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Panel 5: Sharp Indicator Box (Right: 824 .. 984)
        int sharpX1 = ribRight - 160, sharpX2 = ribRight;
        bool isSharp = sigActive && (cents > 1.5f);
        fillRoundRect(hdc, sharpX1, pY1, sharpX2, pY2, 6,
                      isSharp ? RGB(120, 53, 15) : RGB(8, 14, 22),
                      isSharp ? RGB(251, 191, 36) : RGB(23, 32, 48), isSharp ? 2 : 1);
        RECT sharpR1 = { sharpX1, pY1 + 10, sharpX2, pY1 + 32 };
        RECT sharpR2 = { sharpX1, pY1 + 34, sharpX2, pY1 + 54 };
        RECT sharpR3 = { sharpX1, pY1 + 56, sharpX2, pY2 - 8 };
        SelectObject(hdc, fontTitle);
        SetTextColor(hdc, isSharp ? RGB(251, 191, 36) : RGB(51, 65, 85));
        DrawTextW(hdc, L"SHARP ►", -1, &sharpR1, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, isSharp ? RGB(254, 240, 138) : RGB(71, 85, 105));
        DrawTextW(hdc, isSharp ? L"▼ TUNE DOWN" : L"--", -1, &sharpR2, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (isSharp) {
            char sCtBuf[32];
            std::snprintf(sCtBuf, sizeof(sCtBuf), "+%.1f Cents", cents);
            SetTextColor(hdc, RGB(253, 224, 71));
            DrawTextA(hdc, sCtBuf, -1, &sharpR3, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // 4. Multi-Band Optical Virtual Strobe Wheel Aperture (dispLeft + 16 .. dispRight - 16)
        int strobeX1 = dispLeft + 16;
        int strobeX2 = dispRight - 16;
        int strobeY1 = dispTop + 164;
        int strobeY2 = dispTop + 316;
        fillRoundRect(hdc, strobeX1, strobeY1, strobeX2, strobeY2, 6, RGB(3, 7, 12), RGB(20, 83, 45), 2);

        // 4 Concentric / Stacked Bands of rotating strobe pattern teeth
        COLORREF strobeLit = !sigActive ? RGB(20, 30, 45)
                           : inTune     ? RGB(16, 185, 129)
                           : (cents < 0)? RGB(14, 165, 233)
                                        : RGB(249, 115, 22);

        int bandH = (strobeY2 - strobeY1) / 4;
        static const float s_bandPeriods[4] = { 64.0f, 32.0f, 21.33f, 16.0f };

        for (int b = 0; b < 4; ++b) {
            int byTop = strobeY1 + b * bandH;
            int byBot = byTop + bandH - 2;

            float period = s_bandPeriods[b];
            float halfP = period * 0.5f;
            float shift = std::fmod(g_strobeWheelPhase * (b + 1) * 3.5f, period);
            if (shift < 0.0f) shift += period;

            for (float px = strobeX1 - period; px < strobeX2 + period; px += period) {
                int toothL = int(px + shift);
                int toothR = int(toothL + halfP);
                if (toothR < strobeX1 || toothL > strobeX2) continue;

                int clL = std::max(strobeX1 + 1, toothL);
                int clR = std::min(strobeX2 - 1, toothR);
                if (clR > clL) {
                    RECT toothRect = { clL, byTop, clR, byBot };
                    HBRUSH litBr = CreateSolidBrush(strobeLit);
                    FillRect(hdc, &toothRect, litBr);
                    DeleteObject(litBr);
                }
            }

            if (b < 3) {
                HPEN sepP = CreatePen(PS_SOLID, 1, RGB(15, 23, 42));
                HGDIOBJ oldSep = SelectObject(hdc, sepP);
                MoveToEx(hdc, strobeX1 + 2, byBot + 1, nullptr);
                LineTo(hdc, strobeX2 - 2, byBot + 1);
                SelectObject(hdc, oldSep);
                DeleteObject(sepP);
            }
        }

        // Center Target Hairline Fiducial with Alignment Triangles
        HPEN fidPen = CreatePen(PS_SOLID, 2, inTune ? RGB(255, 255, 255) : RGB(239, 68, 68));
        HGDIOBJ oldFid = SelectObject(hdc, fidPen);
        MoveToEx(hdc, centerDispX, strobeY1 + 2, nullptr);
        LineTo(hdc, centerDispX, strobeY2 - 2);
        SelectObject(hdc, oldFid);
        DeleteObject(fidPen);

        POINT topTri[3] = { { centerDispX - 6, strobeY1 - 6 }, { centerDispX + 6, strobeY1 - 6 }, { centerDispX, strobeY1 } };
        HBRUSH triBr = CreateSolidBrush(inTune ? RGB(52, 211, 153) : RGB(239, 68, 68));
        HGDIOBJ oldTriBr = SelectObject(hdc, triBr);
        Polygon(hdc, topTri, 3);
        POINT botTri[3] = { { centerDispX - 6, strobeY2 + 6 }, { centerDispX + 6, strobeY2 + 6 }, { centerDispX, strobeY2 } };
        Polygon(hdc, botTri, 3);
        SelectObject(hdc, oldTriBr);
        DeleteObject(triBr);

        // 5. Fine Cents Deflection Ruler & Needle (-50 ct .. +50 ct)
        int rulerTop = strobeY2 + 10;
        int rulerBot = rulerTop + 44;
        RECT rulerR = { strobeX1, rulerTop, strobeX2, rulerBot };
        fillRoundRect(hdc, rulerR.left, rulerR.top, rulerR.right, rulerR.bottom, 4, RGB(4, 7, 12), RGB(30, 41, 59), 1);

        int inTuneHalfW = 12;
        fillRoundRect(hdc, centerDispX - inTuneHalfW, rulerTop + 1, centerDispX + inTuneHalfW, rulerBot - 1, 2,
                      inTune ? RGB(6, 78, 59) : RGB(8, 25, 20), RGB(16, 185, 129), 1);

        SelectObject(hdc, fontHud);
        SetTextColor(hdc, RGB(148, 163, 184));
        static const int s_tickCents[] = { -50, -40, -30, -20, -10, -5, 0, 5, 10, 20, 30, 40, 50 };
        for (int tc : s_tickCents) {
            int tx = centerDispX + int((float(tc) / 50.0f) * (360.0f));
            int tickH = (tc == 0) ? 14 : ((tc % 10 == 0) ? 10 : 6);
            COLORREF tcColor = (tc == 0) ? (inTune ? RGB(52, 211, 153) : RGB(255, 255, 255)) : RGB(100, 116, 139);
            HPEN tPen = CreatePen(PS_SOLID, (tc == 0) ? 2 : 1, tcColor);
            HGDIOBJ oldTPen = SelectObject(hdc, tPen);
            MoveToEx(hdc, tx, rulerTop + 2, nullptr);
            LineTo(hdc, tx, rulerTop + 2 + tickH);
            SelectObject(hdc, oldTPen);
            DeleteObject(tPen);

            if (tc % 10 == 0 || tc == 0) {
                char tBuf[16];
                std::snprintf(tBuf, sizeof(tBuf), "%d", tc);
                RECT tLblR = { tx - 16, rulerTop + 16, tx + 16, rulerBot - 4 };
                SetTextColor(hdc, (tc == 0 && inTune) ? RGB(52, 211, 153) : RGB(100, 116, 139));
                DrawTextA(hdc, tBuf, -1, &tLblR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }

        if (sigActive) {
            float clampedCents = std::max(-50.0f, std::min(50.0f, cents));
            int needleX = centerDispX + int((clampedCents / 50.0f) * 360.0f);
            COLORREF needleColor = inTune ? RGB(52, 211, 153) : (cents < 0 ? RGB(56, 189, 248) : RGB(249, 115, 22));
            HPEN nPen = CreatePen(PS_SOLID, 3, needleColor);
            HGDIOBJ oldNPen = SelectObject(hdc, nPen);
            MoveToEx(hdc, needleX, rulerTop + 1, nullptr);
            LineTo(hdc, needleX, rulerBot - 1);
            SelectObject(hdc, oldNPen);
            DeleteObject(nPen);

            POINT dPt[4] = { { needleX, rulerTop - 4 }, { needleX + 5, rulerTop + 2 }, { needleX, rulerTop + 8 }, { needleX - 5, rulerTop + 2 } };
            HBRUSH nBr = CreateSolidBrush(needleColor);
            HGDIOBJ oldNBr = SelectObject(hdc, nBr);
            Polygon(hdc, dPt, 4);
            SelectObject(hdc, oldNBr);
            DeleteObject(nBr);
        }

        // 6. Telemetry & Audio Pass-Through Banner
        RECT telemR = { dispLeft + 16, rulerBot + 8, dispRight - 16, dispBottom - 8 };
        fillRoundRect(hdc, telemR.left, telemR.top, telemR.right, telemR.bottom, 4, RGB(2, 6, 10), RGB(14, 116, 144), 1);
        char telemBuf[256];
        if (sigActive) {
            float inDb = (inRms > 1e-5f) ? (20.0f * std::log10(inRms)) : -96.0f;
            std::snprintf(telemBuf, sizeof(telemBuf),
                          "PITCH: %.2f Hz  |  TARGET: %.2f Hz (%s%d)  |  DEVIATION: %+.1f Cents  |  STABILITY: %.1f%%  |  RMS: %.1f dBFS  |  AUDIO: %s",
                          detPitch, tgtPitch, nName, octave, cents, clarity * 100.0f, inDb,
                          isMuted ? "MUTED (SILENT TUNING)" : "UNMUTED (PASS-THRU)");
        } else {
            std::snprintf(telemBuf, sizeof(telemBuf),
                          "CHROMATIC STROBE TUNER: WAITING FOR GUITAR INPUT (PLUCK ANY STRING TO TRACK)  |  AUDIO: %s",
                          isMuted ? "MUTED (SILENT TUNING)" : "UNMUTED (PASS-THRU)");
        }
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, sigActive ? (inTune ? RGB(52, 211, 153) : RGB(125, 211, 252)) : RGB(148, 163, 184));
        DrawTextA(hdc, telemBuf, -1, &telemR, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        // Right Hardware Bay (1018..1235)
        int rBayLeft = 1018, rBayRight = bx2 - 20;
        fillRoundRect(hdc, rBayLeft, dispTop, rBayRight, dispBottom, 8, RGB(8, 12, 18), RGB(30, 41, 59), 2);

        // Button 1: Mute to Tune Footswitch
        fillRoundRect(hdc, g_btnTunerMuteRect.left, g_btnTunerMuteRect.top, g_btnTunerMuteRect.right, g_btnTunerMuteRect.bottom, 8,
                      isMuted ? RGB(185, 28, 28) : RGB(6, 78, 59),
                      isMuted ? RGB(239, 68, 68) : RGB(52, 211, 153), 2);
        SelectObject(hdc, fontKnob);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT muteTopR = { g_btnTunerMuteRect.left, g_btnTunerMuteRect.top + 10, g_btnTunerMuteRect.right, g_btnTunerMuteRect.top + 34 };
        DrawTextA(hdc, isMuted ? "[ MUTE TO TUNE ]" : "[ AUDIO ACTIVE ]", -1, &muteTopR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, isMuted ? RGB(254, 202, 202) : RGB(204, 251, 241));
        RECT muteSubR = { g_btnTunerMuteRect.left + 6, g_btnTunerMuteRect.top + 38, g_btnTunerMuteRect.right - 6, g_btnTunerMuteRect.bottom - 8 };
        DrawTextA(hdc, isMuted ? "OUTPUT MUTED\nSILENT GUITAR TUNING" : "OUTPUT UNMUTED\nAMP AUDIO PASS-THRU", -1, &muteSubR, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

        // Button 2: Reference Pitch
        fillRoundRect(hdc, g_btnTunerRefRect.left, g_btnTunerRefRect.top, g_btnTunerRefRect.right, g_btnTunerRefRect.bottom, 6,
                      RGB(15, 23, 42), RGB(56, 189, 248), 1);
        SelectObject(hdc, fontKnob);
        SetTextColor(hdc, RGB(254, 240, 138));
        char refTopBuf[64];
        std::snprintf(refTopBuf, sizeof(refTopBuf), "REF: %.0f Hz", curRef);
        RECT refTopR = { g_btnTunerRefRect.left, g_btnTunerRefRect.top + 10, g_btnTunerRefRect.right, g_btnTunerRefRect.top + 32 };
        DrawTextA(hdc, refTopBuf, -1, &refTopR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, RGB(148, 163, 184));
        RECT refSubR = { g_btnTunerRefRect.left, g_btnTunerRefRect.top + 36, g_btnTunerRefRect.right, g_btnTunerRefRect.bottom - 6 };
        DrawTextA(hdc, (curRef > 436.0f) ? "STANDARD CONCERT\n[CLICK FOR 432 Hz]" : "VERDI NATURAL 432\n[CLICK FOR 440 Hz]", -1, &refSubR, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

        // Button 3: Tracking Mode
        bool isHiPrec = g_tuner.highPrecisionMode.load();
        fillRoundRect(hdc, g_btnTunerSpeedRect.left, g_btnTunerSpeedRect.top, g_btnTunerSpeedRect.right, g_btnTunerSpeedRect.bottom, 6,
                      RGB(15, 23, 42), isHiPrec ? RGB(56, 189, 248) : RGB(16, 185, 129), 1);
        SelectObject(hdc, fontKnob);
        SetTextColor(hdc, isHiPrec ? RGB(56, 189, 248) : RGB(52, 211, 153));
        RECT spdTopR = { g_btnTunerSpeedRect.left, g_btnTunerSpeedRect.top + 10, g_btnTunerSpeedRect.right, g_btnTunerSpeedRect.top + 32 };
        DrawTextA(hdc, isHiPrec ? "TRACKING: ULTRA HI-RES" : "TRACKING: CHROMATIC STD", -1, &spdTopR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, fontHud);
        SetTextColor(hdc, RGB(148, 163, 184));
        RECT spdSubR = { g_btnTunerSpeedRect.left, g_btnTunerSpeedRect.top + 36, g_btnTunerSpeedRect.right, g_btnTunerSpeedRect.bottom - 6 };
        DrawTextA(hdc, isHiPrec ? "0.05-CENT PARABOLIC\n[CLICK FOR STD]" : "0.1-CENT OPTICAL STROBE\n[CLICK FOR HI-RES]", -1, &spdSubR, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

        // 1/4" Input Jack Graphic & Level Meter
        int jackX = (rBayLeft + rBayRight) / 2;
        int jackY = g_btnTunerSpeedRect.bottom + 42;
        fillRoundRect(hdc, jackX - 16, jackY - 16, jackX + 16, jackY + 16, 6, RGB(148, 163, 184), RGB(71, 85, 105), 1);
        fillRoundRect(hdc, jackX - 12, jackY - 12, jackX + 12, jackY + 12, 12, RGB(226, 232, 240), RGB(100, 116, 139), 1);
        fillRoundRect(hdc, jackX - 6, jackY - 6, jackX + 6, jackY + 6, 6, RGB(0, 0, 0), RGB(15, 23, 42), 1);

        SelectObject(hdc, fontHud);
        SetTextColor(hdc, RGB(203, 213, 225));
        RECT jackLblR = { rBayLeft, jackY + 22, rBayRight, jackY + 38 };
        DrawTextA(hdc, "GUITAR HI-Z IN", -1, &jackLblR, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Instantaneous VU LED Ladder
        int ledTop = jackY + 44;
        int ledW = 120, ledH = 10;
        int ledX1 = jackX - ledW / 2;
        float rmsNorm = std::min(1.0f, inRms * 4.0f);
        int numLit = int(rmsNorm * 8.0f);
        for (int l = 0; l < 8; ++l) {
            int lx = ledX1 + l * (ledW / 8);
            RECT ledR = { lx + 1, ledTop, lx + (ledW / 8) - 2, ledTop + ledH };
            bool lit = (l < numLit);
            COLORREF lCol = (l >= 7) ? RGB(239, 68, 68) : (l >= 5) ? RGB(245, 158, 11) : RGB(16, 185, 129);
            COLORREF dCol = RGB(15, 23, 42);
            fillRoundRect(hdc, ledR.left, ledR.top, ledR.right, ledR.bottom, 2, lit ? lCol : dCol, lit ? lCol : RGB(30, 41, 59), 1);
        }
    }

    // 4. Draw Knobs belonging to the Active Page
    SelectObject(hdc, fontKnob);
    for (int i = 0; i < NUM_KNOBS; ++i) {
        if (g_knobs[i].pageIdx == g_activePage) {
            drawStylizedKnob(hdc, g_knobs[i], g_activePage);
        }
    }

    // 5. Bottom Live Meter Bar and Hardware Status
    float inPk = std::min(1.0f, g_inMeterPeak.load());
    float outL = std::min(1.0f, g_outMeterPeakL.load());
    float outR = std::min(1.0f, g_outMeterPeakR.load());

    char statusBuf[256];
    const char* chNames[3] = { "Ch 1 (Left)", "Ch 2 (Right)", "Stereo Sum" };
    float latMs = float(g_audioCfg.bufferFrames) * 1000.0f / 48000.0f;
    if (g_asioActive.load()) {
        std::snprintf(statusBuf, sizeof(statusBuf),
                      "ASIO DIRECT: %s [%s]  |  BUF: %ld smp (%.2f ms)",
                      g_audioCfg.asioDriverName, chNames[g_audioCfg.inChannel], g_asioBufSize, float(g_asioBufSize) * 1000.0f / 48000.0f);
    } else {
        const char* drvTag = (g_audioCfg.driverMode == 1) ? "WASAPI EXCL" : "TONEDIRECT";
        std::snprintf(statusBuf, sizeof(statusBuf),
                      "%s: %s [%s]  |  BUF: %d smp (%.2f ms)",
                      drvTag, g_audioCfg.inDevName, chNames[g_audioCfg.inChannel], g_audioCfg.bufferFrames, latMs);
    }

    RECT footer = { 22, h - 36, 440, h - 10 };
    SetTextColor(hdc, RGB(204, 251, 241));
    DrawTextA(hdc, statusBuf, -1, &footer, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Calibrated GUITAR IN Meter with Auto Good Gain Target Sweet-Spot
    int meterX = 450;
    RECT inLblR = { meterX, h - 34, meterX + 66, h - 10 };
    SetTextColor(hdc, RGB(45, 212, 191));
    DrawTextA(hdc, "GUITAR IN", -1, &inLblR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    int inBarX = meterX + 68;
    int inW = 85;
    int inBarTop = h - 28;
    int inBarBottom = h - 14;

    // Background track
    fillRoundRect(hdc, inBarX, inBarTop, inBarX + inW, inBarBottom, 4, RGB(4, 24, 20), RGB(19, 78, 74), 1);

    // Target Sweet-Spot notch (-18 dB to -10 dBFS -> linear 0.126 to 0.316 -> sqrt norm 0.355 to 0.562)
    int targetX1 = inBarX + int(inW * 0.355f);
    int targetX2 = inBarX + int(inW * 0.562f);
    fillRoundRect(hdc, targetX1, inBarTop + 1, targetX2, inBarBottom - 1, 2, RGB(6, 44, 34), RGB(16, 185, 129), 1);

    // Active Live Level Fill
    if (inPk > 0.004f) {
        float inNorm = std::min(1.0f, std::sqrt(inPk));
        int fillW = int(inW * inNorm);
        if (fillW > 2) {
            COLORREF c = (inPk > 0.707f) ? RGB(239, 68, 68)     // Red (Clip Risk)
                       : (inPk > 0.36f)  ? RGB(245, 158, 11)     // Warm Amber (Hot Level)
                       : (inPk >= 0.11f) ? RGB(16, 185, 129)     // Vibrant Emerald (Good Gain)
                                         : RGB(20, 184, 166);    // Teal (Low Level)
            fillRoundRect(hdc, inBarX + 1, inBarTop + 1, inBarX + fillW, inBarBottom - 1, 3, c, c, 1);
        }
    }

    // Target Sweet Spot Bracket lines
    HPEN tickPen = CreatePen(PS_SOLID, 1, RGB(52, 211, 153));
    HGDIOBJ oldPen = SelectObject(hdc, tickPen);
    MoveToEx(hdc, targetX1, inBarTop - 2, nullptr); LineTo(hdc, targetX1, inBarBottom + 2);
    MoveToEx(hdc, targetX2, inBarTop - 2, nullptr); LineTo(hdc, targetX2, inBarBottom + 2);
    SelectObject(hdc, oldPen);
    DeleteObject(tickPen);

    // Auto Good Gain Real-Time Status Badge Pill
    RECT badgeRect = { inBarX + inW + 8, h - 34, inBarX + inW + 8 + 120, h - 10 };
    COLORREF badgeBg, badgeBorder, badgeText;
    const char* badgeStr;
    if (inPk < 0.008f) {
        badgeBg = RGB(10, 20, 22); badgeBorder = RGB(30, 50, 55); badgeText = RGB(100, 116, 139);
        badgeStr = "INPUT: IDLE";
    } else if (inPk < 0.11f) {
        badgeBg = RGB(34, 25, 8); badgeBorder = RGB(180, 130, 20); badgeText = RGB(251, 191, 36);
        badgeStr = "^ LOW (BOOST IN)";
    } else if (inPk <= 0.36f) {
        badgeBg = RGB(4, 52, 40); badgeBorder = RGB(52, 211, 153); badgeText = RGB(254, 240, 138);
        badgeStr = "* GOOD GAIN *";
    } else if (inPk <= 0.707f) {
        badgeBg = RGB(50, 35, 10); badgeBorder = RGB(245, 158, 11); badgeText = RGB(254, 215, 170);
        badgeStr = "HOT LEVEL (OK)";
    } else {
        badgeBg = RGB(70, 15, 15); badgeBorder = RGB(239, 68, 68); badgeText = RGB(254, 202, 202);
        badgeStr = "! CLIP WARNING !";
    }
    fillRoundRect(hdc, badgeRect.left, badgeRect.top, badgeRect.right, badgeRect.bottom, 4, badgeBg, badgeBorder, 1);
    SetTextColor(hdc, badgeText);
    DrawTextA(hdc, badgeStr, -1, &badgeRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Interactive Strobe Tuner Telemetry Pill
    char tunerPillBuf[64];
    COLORREF tPillBg, tPillBorder, tPillText;
    if (g_activePage == 7) {
        tPillBg = RGB(12, 74, 110); tPillBorder = RGB(56, 189, 248); tPillText = RGB(224, 242, 254);
        std::snprintf(tunerPillBuf, sizeof(tunerPillBuf), "[ TUNER: ACTIVE ]");
    } else if (g_tuner.signalActive.load()) {
        char pn[8]; g_tuner.getNoteName(pn);
        float ct = g_tuner.cents.load();
        bool it = (std::abs(ct) <= 1.5f);
        tPillBg = it ? RGB(6, 78, 59) : RGB(15, 23, 42);
        tPillBorder = it ? RGB(52, 211, 153) : RGB(56, 189, 248);
        tPillText = it ? RGB(254, 240, 138) : RGB(224, 242, 254);
        std::snprintf(tunerPillBuf, sizeof(tunerPillBuf), "TUNER: %s%d %+.1fct", pn, g_tuner.octave.load(), ct);
    } else {
        tPillBg = RGB(15, 23, 42); tPillBorder = RGB(51, 65, 85); tPillText = RGB(148, 163, 184);
        std::snprintf(tunerPillBuf, sizeof(tunerPillBuf), "TUNER: IDLE");
    }
    fillRoundRect(hdc, g_tunerStatusPill.left, g_tunerStatusPill.top, g_tunerStatusPill.right, g_tunerStatusPill.bottom, 4, tPillBg, tPillBorder, 1);
    SetTextColor(hdc, tPillText);
    DrawTextA(hdc, tunerPillBuf, -1, &g_tunerStatusPill, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Master Output L/R Meters
    int outMeterX = g_tunerStatusPill.right + 14;
    RECT outLblR = { outMeterX, h - 34, outMeterX + 74, h - 10 };
    SetTextColor(hdc, RGB(251, 191, 36));
    DrawTextA(hdc, "MASTER OUT", -1, &outLblR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    int outW = 120;
    int outBarX = outMeterX + 78;
    fillRoundRect(hdc, outBarX, h - 33, outBarX + outW, h - 23, 4, RGB(6, 34, 28), RGB(19, 78, 74), 1);
    fillRoundRect(hdc, outBarX, h - 19, outBarX + outW, h - 9,  4, RGB(6, 34, 28), RGB(19, 78, 74), 1);
    if (outL > 0.005f) fillRoundRect(hdc, outBarX, h - 33, outBarX + int(outW * std::min(1.0f, std::sqrt(outL))), h - 23, 4, RGB(245, 158, 11), RGB(245, 158, 11), 1);
    if (outR > 0.005f) fillRoundRect(hdc, outBarX, h - 19, outBarX + int(outW * std::min(1.0f, std::sqrt(outR))), h - 9,  4, RGB(245, 158, 11), RGB(245, 158, 11), 1);

    BitBlt(hdcWin, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, oldFont);
    DeleteObject(fontTab);
    DeleteObject(fontTitle);
    DeleteObject(fontSub);
    DeleteObject(fontKnob);
    DeleteObject(fontHud);
    SelectObject(hdc, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdc);
}

// ============================================================================
// Euphoria Dedicated Audio Hardware Settings Dialog
// ============================================================================
static HWND g_hDrvModeCombo = nullptr;
static HWND g_hAsioCombo = nullptr;
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

            CreateWindowA("STATIC", "0. Audio Driver Architecture (ASIO / ToneDirect / WASAPI):", WS_CHILD | WS_VISIBLE, 24, 14, 490, 20, hwnd, nullptr, nullptr, nullptr);
            g_hDrvModeCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 24, 36, 490, 150, hwnd, (HMENU)105, nullptr, nullptr);
            SendMessageA(g_hDrvModeCombo, WM_SETFONT, (WPARAM)hfBold, TRUE);
            SendMessageA(g_hDrvModeCombo, CB_ADDSTRING, 0, (LPARAM)"ASIO Direct Hardware (Zero-Copy DMA - Audient EVO / Line 6 / ASIO4ALL)");
            SendMessageA(g_hDrvModeCombo, CB_ADDSTRING, 0, (LPARAM)"WASAPI Exclusive Mode (Direct Kernel Streaming - Ultra-Low Latency)");
            SendMessageA(g_hDrvModeCombo, CB_ADDSTRING, 0, (LPARAM)"WASAPI Low-Latency Shared (Windows Pro Audio + ToneDirect)");
            SendMessageA(g_hDrvModeCombo, CB_SETCURSEL, (WPARAM)std::clamp(g_audioCfg.driverMode, 0, 2), 0);

            CreateWindowA("STATIC", "   ASIO Hardware Driver (Used when ASIO Direct is active):", WS_CHILD | WS_VISIBLE, 24, 68, 490, 20, hwnd, nullptr, nullptr, nullptr);
            g_hAsioCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 24, 90, 355, 180, hwnd, (HMENU)106, nullptr, nullptr);
            SendMessageA(g_hAsioCombo, WM_SETFONT, (WPARAM)hf, TRUE);
            if (g_asioDriverList.empty()) {
                SendMessageA(g_hAsioCombo, CB_ADDSTRING, 0, (LPARAM)"(No ASIO Driver Detected - Using WASAPI ToneDirect)");
                SendMessageA(g_hAsioCombo, CB_SETCURSEL, 0, 0);
            } else {
                for (size_t i = 0; i < g_asioDriverList.size(); ++i) {
                    SendMessageA(g_hAsioCombo, CB_ADDSTRING, 0, (LPARAM)g_asioDriverList[i].name);
                    if ((int)i == g_audioCfg.asioDriverIdx || std::strcmp(g_asioDriverList[i].name, g_audioCfg.asioDriverName) == 0) {
                        SendMessageA(g_hAsioCombo, CB_SETCURSEL, (WPARAM)i, 0);
                    }
                }
                if (SendMessageA(g_hAsioCombo, CB_GETCURSEL, 0, 0) == CB_ERR) {
                    SendMessageA(g_hAsioCombo, CB_SETCURSEL, 0, 0);
                }
            }

            HWND hBtnAsioPanel = CreateWindowA("BUTTON", "ASIO PANEL...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 386, 89, 128, 26, hwnd, (HMENU)107, nullptr, nullptr);
            SendMessageA(hBtnAsioPanel, WM_SETFONT, (WPARAM)hfBold, TRUE);

            CreateWindowA("STATIC", "1. Guitar Input Audio Interface (Windows WASAPI / Fallback):", WS_CHILD | WS_VISIBLE, 24, 126, 490, 20, hwnd, nullptr, nullptr, nullptr);
            g_hInCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 24, 148, 490, 200, hwnd, (HMENU)101, nullptr, nullptr);
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

            CreateWindowA("STATIC", "   Input Channel (Guitar Instrument Port - Applies to ASIO & WASAPI):", WS_CHILD | WS_VISIBLE, 24, 182, 490, 20, hwnd, nullptr, nullptr, nullptr);
            g_hChanCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 24, 204, 490, 150, hwnd, (HMENU)102, nullptr, nullptr);
            SendMessageA(g_hChanCombo, WM_SETFONT, (WPARAM)hf, TRUE);
            SendMessageA(g_hChanCombo, CB_ADDSTRING, 0, (LPARAM)"Channel 1 (Left / Instrument 1 - Audient EVO 4, Line 6, Focusrite)");
            SendMessageA(g_hChanCombo, CB_ADDSTRING, 0, (LPARAM)"Channel 2 (Right / Instrument 2)");
            SendMessageA(g_hChanCombo, CB_ADDSTRING, 0, (LPARAM)"Stereo Sum (Combine Ch 1 + Ch 2)");
            SendMessageA(g_hChanCombo, CB_SETCURSEL, (WPARAM)g_audioCfg.inChannel, 0);

            CreateWindowA("STATIC", "2. Playback Output Device (Monitors / Headphones):", WS_CHILD | WS_VISIBLE, 24, 282, 490, 20, hwnd, nullptr, nullptr, nullptr);
            g_hOutCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 24, 304, 490, 200, hwnd, (HMENU)103, nullptr, nullptr);
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

            CreateWindowA("STATIC", "3. Audio Buffer Size / Hardware Latency (48,000 Hz):", WS_CHILD | WS_VISIBLE, 24, 342, 490, 20, hwnd, nullptr, nullptr, nullptr);
            g_hBufCombo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 24, 364, 490, 240, hwnd, (HMENU)104, nullptr, nullptr);
            SendMessageA(g_hBufCombo, WM_SETFONT, (WPARAM)hfBold, TRUE);
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"16 frames (~0.33 ms) - ASIO / ToneDirect Extreme Ultra-Low Latency");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"24 frames (~0.50 ms) - ASIO / ToneDirect Sub-Millisecond");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"32 frames (~0.67 ms) - ASIO / ToneDirect Ultra (Line 6 / EVO 4 Fast)");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"48 frames (~1.00 ms) - ASIO / ToneDirect Pro 1.0 ms");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"64 frames (~1.33 ms) - ASIO / ToneDirect Fast (Default Direct Monitor)");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"96 frames (~2.00 ms) - Studio Fast Response");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"128 frames (~2.67 ms) - Standard Low Latency");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"256 frames (~5.33 ms) - Balanced Studio");
            SendMessageA(g_hBufCombo, CB_ADDSTRING, 0, (LPARAM)"512 frames (~10.67 ms) - High Stability Safe Mode");

            const int bufSizes[9] = { 16, 24, 32, 48, 64, 96, 128, 256, 512 };
            int selBuf = 2; // default 32 frames
            for (int b = 0; b < 9; ++b) {
                if (g_audioCfg.bufferFrames <= bufSizes[b]) {
                    selBuf = b;
                    break;
                }
            }
            SendMessageA(g_hBufCombo, CB_SETCURSEL, (WPARAM)selBuf, 0);

            HWND hBtnOk = CreateWindowA("BUTTON", "APPLY & SAVE SETTINGS", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 120, 412, 190, 38, hwnd, (HMENU)IDOK, nullptr, nullptr);
            HWND hBtnCancel = CreateWindowA("BUTTON", "CANCEL", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 330, 412, 110, 38, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
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

            RECT meterArea = { 24, 240, 514, 270 };
            fillRoundRect(hdc, meterArea.left, meterArea.top, meterArea.right, meterArea.bottom, 6, RGB(6, 34, 28), RGB(20, 184, 166), 1);

            float pk = std::min(1.0f, g_inMeterPeak.load());
            int barW = meterArea.right - meterArea.left - 120;
            fillRoundRect(hdc, meterArea.left + 8, meterArea.top + 6, meterArea.left + 8 + barW, meterArea.bottom - 6, 4, RGB(4, 20, 16), RGB(13, 148, 136), 1);

            if (pk > 0.005f) {
                COLORREF c = (pk > 0.85f) ? RGB(239, 68, 68) : ((pk > 0.65f) ? RGB(245, 158, 11) : RGB(16, 185, 129));
                fillRoundRect(hdc, meterArea.left + 8, meterArea.top + 6, meterArea.left + 8 + int(barW * pk), meterArea.bottom - 6, 4, c, c, 1);
            }

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, (pk > 0.05f) ? RGB(16, 185, 129) : RGB(148, 163, 184));
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
            if (id == 107) {
                int asioSel = (int)SendMessageA(g_hAsioCombo, CB_GETCURSEL, 0, 0);
                if (asioSel >= 0 && asioSel < (int)g_asioDriverList.size()) {
                    g_audioCfg.asioDriverIdx = asioSel;
                    std::strncpy(g_audioCfg.asioDriverName, g_asioDriverList[asioSel].name, sizeof(g_audioCfg.asioDriverName) - 1);
                }
                g_asioOpenPanelReq.store(true);
                return 0;
            } else if (id == IDOK) {
                int drvModeSel = (int)SendMessageA(g_hDrvModeCombo, CB_GETCURSEL, 0, 0);
                if (drvModeSel >= 0 && drvModeSel <= 2) {
                    g_audioCfg.driverMode = drvModeSel;
                }
                int asioSel = (int)SendMessageA(g_hAsioCombo, CB_GETCURSEL, 0, 0);
                if (asioSel >= 0 && asioSel < (int)g_asioDriverList.size()) {
                    g_audioCfg.asioDriverIdx = asioSel;
                    std::strncpy(g_audioCfg.asioDriverName, g_asioDriverList[asioSel].name, sizeof(g_audioCfg.asioDriverName) - 1);
                }
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
                const int bufSizes[9] = { 16, 24, 32, 48, 64, 96, 128, 256, 512 };
                g_audioCfg.bufferFrames = (bufSel >= 0 && bufSel < 9) ? bufSizes[bufSel] : 32;

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
    dc.lpszClassName = "EuphoriaAudioSettingsDlgWnd";
    RegisterClassExA(&dc);

    RECT pr;
    GetWindowRect(hwndParent, &pr);
    int cx = (pr.left + pr.right) / 2 - 275;
    int cy = (pr.top + pr.bottom) / 2 - 250;

    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        dc.lpszClassName,
        "Euphoria Amp — ASIO / ToneDirect & Guitar Interface Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, 550, 500,
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
static bool  g_dragDidMove = false;

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 33, nullptr);
            return 0;
        case WM_TIMER:
            g_animPhase += 0.10f;
            if (g_animPhase > 6.2831853f) g_animPhase -= 6.2831853f;
            g_strobeWheelPhase += g_tuner.strobeSpeed.load() * 0.40f;
            if (g_strobeWheelPhase > 1000.0f) g_strobeWheelPhase -= 1000.0f;
            else if (g_strobeWheelPhase < -1000.0f) g_strobeWheelPhase += 1000.0f;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            for (int t = 0; t < 8; ++t) {
                if (PtInRect(&g_tabRects[t], pt)) {
                    g_activePage = t;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            if (PtInRect(&g_tunerStatusPill, pt)) {
                g_activePage = (g_activePage == 7) ? 0 : 7;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtInRect(&g_presetPrevRect, pt)) {
                if (!g_presets.empty()) {
                    g_currentPresetIdx = (g_currentPresetIdx - 1 + (int)g_presets.size()) % (int)g_presets.size();
                    applyPreset(g_presets[g_currentPresetIdx]);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (PtInRect(&g_presetNextRect, pt)) {
                if (!g_presets.empty()) {
                    g_currentPresetIdx = (g_currentPresetIdx + 1) % (int)g_presets.size();
                    applyPreset(g_presets[g_currentPresetIdx]);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (PtInRect(&g_presetSaveRect, pt) || PtInRect(&g_presetNameRect, pt)) {
                openSavePresetDialog(hwnd);
                return 0;
            }
            if (PtInRect(&g_learnNoiseBtnRect, pt)) {
                g_learnNoiseStartTime = GetTickCount64();
                g_engine.triggerLearnNoise();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtInRect(&g_recSampleBtnRect, pt)) {
                if (!g_recSampleActive.load()) {
                    std::lock_guard<std::mutex> lock(g_recSampleMutex);
                    g_recSampleBuf.clear();
                    g_recSampleBuf.reserve(SAMPLE_RATE * 6);
                    g_recSampleStartTime = GetTickCount64();
                    g_recSampleActive.store(true);
                } else {
                    g_recSampleActive.store(false);
                    g_recSampleDoneTime = GetTickCount64();
                    std::lock_guard<std::mutex> lock(g_recSampleMutex);
                    if (!g_recSampleBuf.empty()) {
                        saveWavFile("scratch/guitar_hiss_capture.wav", g_recSampleBuf.data(), g_recSampleBuf.size(), SAMPLE_RATE);
                    }
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
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
            if (g_activePage == 3) {
                // Two-Button Volume Learn Calibration for Fluid Fuzz Face
                if (PtInRect(&g_btnFuzzLearnMaxRect, pt)) {
                    g_fuzzLearnStartTime = GetTickCount64();
                    if (g_engine.isFuzzFaceLearningMax()) {
                        g_engine.finishFuzzFaceLearning();
                    } else {
                        g_engine.triggerFuzzFaceLearnMax();
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (PtInRect(&g_btnFuzzLearnCleanRect, pt)) {
                    g_fuzzLearnStartTime = GetTickCount64();
                    if (g_engine.isFuzzFaceLearningClean()) {
                        g_engine.finishFuzzFaceLearning();
                    } else {
                        g_engine.triggerFuzzFaceLearnClean();
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }

                // Interactive 3D Footswitches on Hardwood Pedalboard Deck
                RECT rKlonFoot = { 204 - 35, 465, 204 + 35, 535 };
                if (PtInRect(&rKlonFoot, pt)) {
                    g_knobs[K_KLON_ON].curVal = (g_knobs[K_KLON_ON].curVal > 0.5f) ? 0.0f : 1.0f;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                RECT rTsFoot = { 422 - 45, 465, 422 + 45, 535 };
                if (PtInRect(&rTsFoot, pt)) {
                    g_knobs[K_TS_ON].curVal = (g_knobs[K_TS_ON].curVal > 0.5f) ? 0.0f : 1.0f;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                RECT rBdFoot = { 640 - 45, 465, 640 + 45, 535 };
                if (PtInRect(&rBdFoot, pt)) {
                    g_knobs[K_BD_ON].curVal = (g_knobs[K_BD_ON].curVal > 0.5f) ? 0.0f : 1.0f;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                RECT rRatFoot = { 858 - 35, 465, 858 + 35, 535 };
                if (PtInRect(&rRatFoot, pt)) {
                    g_knobs[K_RAT_ON].curVal = (g_knobs[K_RAT_ON].curVal > 0.5f) ? 0.0f : 1.0f;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                RECT rFuzzFoot = { 1076 - 35, 465, 1076 + 35, 535 };
                if (PtInRect(&rFuzzFoot, pt)) {
                    g_knobs[K_FUZZ_ON].curVal = (g_knobs[K_FUZZ_ON].curVal > 0.5f) ? 0.0f : 1.0f;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            if (g_activePage == 7) {
                if (PtInRect(&g_btnTunerMuteRect, pt)) {
                    g_tunerMute.store(!g_tunerMute.load());
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (PtInRect(&g_btnTunerRefRect, pt)) {
                    float curRef = g_tuner.refFreq.load();
                    g_tuner.refFreq.store((curRef > 436.0f) ? 432.0f : 440.0f);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (PtInRect(&g_btnTunerSpeedRect, pt)) {
                    g_tuner.highPrecisionMode.store(!g_tuner.highPrecisionMode.load());
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            for (int i = 0; i < NUM_KNOBS; ++i) {
                if (g_knobs[i].pageIdx != -1 && g_knobs[i].pageIdx != g_activePage) continue;
                if (PtInRect(&g_knobs[i].rect, pt)) {
                    g_dragKnob = i;
                    g_dragStartY = pt.y;
                    g_dragStartVal = g_knobs[i].curVal;
                    g_dragDidMove = false;
                    SetCapture(hwnd);
                    return 0;
                }
            }
            return 0;
        }
        case WM_RBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            for (int i = 0; i < NUM_KNOBS; ++i) {
                if (g_knobs[i].pageIdx != -1 && g_knobs[i].pageIdx != g_activePage) continue;
                if (PtInRect(&g_knobs[i].rect, pt) && g_knobs[i].isStepped) {
                    int prev = (int(std::round(g_knobs[i].curVal)) - 1 + g_knobs[i].steps) % g_knobs[i].steps;
                    g_knobs[i].curVal = float(prev);
                    InvalidateRect(hwnd, nullptr, FALSE);
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
                if (std::abs(dy) >= 4) {
                    g_dragDidMove = true;
                }
                KnobCtrl& k = g_knobs[g_dragKnob];
                if (k.isStepped) {
                    int stepDelta = dy / 8;
                    int targetStep = int(std::round(g_dragStartVal)) + stepDelta;
                    targetStep = std::max(0, std::min(k.steps - 1, targetStep));
                    if (int(std::round(k.curVal)) != targetStep) {
                        k.curVal = float(targetStep);
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                } else {
                    float range = k.maxVal - k.minVal;
                    k.curVal = std::max(k.minVal, std::min(k.maxVal, g_dragStartVal + (float(dy) / 140.0f) * range));
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (g_dragKnob >= 0) {
                KnobCtrl& k = g_knobs[g_dragKnob];
                if (k.isStepped && !g_dragDidMove) {
                    int next = (int(std::round(k.curVal)) + 1) % k.steps;
                    k.curVal = float(next);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                g_dragKnob = -1;
                g_dragDidMove = false;
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

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (!pImageCodecInfo) return -1;
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    g_assets3D.init();
    g_engine.reset();
    layoutPageKnobs();
    refreshAudioDeviceLists();
    loadAudioConfig();
    initPresetSystem();

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "EuphoriaAmp4PageWnd";
    RegisterClassExA(&wc);

    RECT wr = { 0, 0, 1280, 720 };
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowExA(
        0, wc.lpszClassName,
        "Euphoria Amp — Pure Fluid Dynamics Hybrid Tube & Studio Suite (Windows x64)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInst, nullptr
    );
    g_hwndMain = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    const char* pFullCmd = GetCommandLineA();
    if ((pFullCmd && std::strstr(pFullCmd, "--dump-gui")) || (lpCmdLine && std::strstr(lpCmdLine, "--dump-gui"))) {
        CLSID pngClsid;
        GetEncoderClsid(L"image/png", &pngClsid);
        CreateDirectoryA("scratch", nullptr);
        HDC hdcScreen = GetDC(nullptr);
        for (int p = 0; p < 8; ++p) {
            g_activePage = p;
            HDC hdcMem = CreateCompatibleDC(hdcScreen);
            HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, 1280, 720);
            HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

            renderGui(hwnd, hdcMem);

            Gdiplus::Bitmap bmp(hbm, nullptr);
            wchar_t outPath[MAX_PATH];
            swprintf_s(outPath, L"scratch\\gui_page%d.png", p);
            DeleteFileW(outPath);
            bmp.Save(outPath, &pngClsid, nullptr);

            SelectObject(hdcMem, oldBm);
            DeleteObject(hbm);
            DeleteDC(hdcMem);
        }
        for (int m = 0; m <= 8; ++m) {
            g_activePage = 0;
            g_knobs[K_CAB_SIM].curVal = (float)m;
            HDC hdcMem = CreateCompatibleDC(hdcScreen);
            HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, 1280, 720);
            HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

            renderGui(hwnd, hdcMem);

            Gdiplus::Bitmap bmp(hbm, nullptr);
            wchar_t outPath[MAX_PATH];
            swprintf_s(outPath, L"scratch\\gui_cab%d.png", m);
            DeleteFileW(outPath);
            bmp.Save(outPath, &pngClsid, nullptr);

            SelectObject(hdcMem, oldBm);
            DeleteObject(hbm);
            DeleteDC(hdcMem);
        }
        ReleaseDC(nullptr, hdcScreen);
        DestroyWindow(hwnd);
        g_assets3D.shutdown();
        return 0;
    }

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
    g_assets3D.shutdown();
    return 0;
}

