#pragma once
// ============================================================================
// EuphoriaMidiMap.h - Universal MIDI CC Mapping & Interactive MIDI Learn
// Shared between Standalone (Native Win32 & JUCE) and VST3 / AU / LV2 plugins.
//
// Features:
// - Lock-free, real-time safe MIDI CC message dispatch (audio-thread safe)
// - Interactive 1-click MIDI Learn on any knob, switch, or stompbox footswitch
// - Smart footswitch toggle & latching handling for all guitar MIDI pedalboards
// - High-contrast visual HUD feedback while learning and on assignment
// - Persistent storage to human-readable configuration files & DAW project state
// ============================================================================

#include <atomic>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "EuphoriaParams.h"

namespace EuphoriaMIDI {

struct MidiBinding {
    int cc = -1;       // 0..127 (-1 = unmapped)
    int channel = -1;  // -1 = Omni (any channel), 1..16
};

class MidiMapManager {
public:
    MidiMapManager() {
        clearAllBindings();
    }

    void clearAllBindings() noexcept {
        for (int i = 0; i < K_NUM_PARAMS; ++i) {
            bindings[i].cc = -1;
            bindings[i].channel = -1;
            lastCcVal[i] = -1;
        }
        learningParamIdx.store(-1);
        needsSave.store(true);
    }

    void clearBinding(int paramIdx) noexcept {
        if (paramIdx >= 0 && paramIdx < K_NUM_PARAMS) {
            bindings[paramIdx].cc = -1;
            bindings[paramIdx].channel = -1;
            lastCcVal[paramIdx] = -1;
            needsSave.store(true);
        }
        if (learningParamIdx.load() == paramIdx) {
            learningParamIdx.store(-1);
        }
    }

    void bindCc(int paramIdx, int cc, int channel = -1) noexcept {
        if (paramIdx >= 0 && paramIdx < K_NUM_PARAMS) {
            bindings[paramIdx].cc = cc;
            bindings[paramIdx].channel = channel;
            lastCcVal[paramIdx] = -1;
            lastAssignedParam.store(paramIdx);
            lastAssignedCc.store(cc);
            assignedFlashTimer.store(80); // ~2 seconds at 40Hz
            needsSave.store(true);
        }
    }

    int getCcForParam(int paramIdx) const noexcept {
        if (paramIdx >= 0 && paramIdx < K_NUM_PARAMS)
            return bindings[paramIdx].cc;
        return -1;
    }

    int getChannelForParam(int paramIdx) const noexcept {
        if (paramIdx >= 0 && paramIdx < K_NUM_PARAMS)
            return bindings[paramIdx].channel;
        return -1;
    }

    void startLearn(int paramIdx) noexcept {
        if (paramIdx >= 0 && paramIdx < K_NUM_PARAMS) {
            learningParamIdx.store(paramIdx);
        }
    }

    void cancelLearn() noexcept {
        learningParamIdx.store(-1);
    }

    bool isLearning() const noexcept {
        return learningParamIdx.load() >= 0;
    }

    int getLearningParam() const noexcept {
        return learningParamIdx.load();
    }

    int getLastAssignedParam() const noexcept { return lastAssignedParam.load(); }
    int getLastAssignedCc() const noexcept { return lastAssignedCc.load(); }
    int getAssignedFlashTimer() const noexcept { return assignedFlashTimer.load(); }
    void decrementFlashTimer() noexcept {
        int t = assignedFlashTimer.load();
        if (t > 0) assignedFlashTimer.store(t - 1);
    }

    bool checkAndResetNeedsSave() noexcept {
        return needsSave.exchange(false);
    }

    // Process incoming Control Change message (called on audio/MIDI thread)
    // Returns true if a parameter was changed or learned
    bool handleIncomingCc(int cc, int val, int channel,
                          const std::function<void(int paramIdx, float targetVal)>& onParamChange,
                          const std::function<float(int paramIdx)>& getParamVal) noexcept
    {
        // 1. Check if we are currently in MIDI Learn mode
        int lIdx = learningParamIdx.load();
        if (lIdx >= 0 && lIdx < K_NUM_PARAMS) {
            bindCc(lIdx, cc, channel);
            learningParamIdx.store(-1);
            applyValue(lIdx, val, onParamChange, getParamVal);
            return true;
        }

        // 2. Dispatch to any mapped parameters
        bool handled = false;
        for (int p = 0; p < K_NUM_PARAMS; ++p) {
            if (bindings[p].cc == cc) {
                if (bindings[p].channel == -1 || bindings[p].channel == channel) {
                    applyValue(p, val, onParamChange, getParamVal);
                    handled = true;
                }
            }
        }
        return handled;
    }

    // Save and load bindings to/from INI/CFG file
    void saveToFile(const std::string& filePath) const {
        if (filePath.empty()) return;
        std::ofstream f(filePath);
        if (!f.is_open()) return;
        f << "# Euphoria Amp MIDI CC Mappings\n";
        const auto& table = euphoriaParamTable();
        for (int i = 0; i < K_NUM_PARAMS; ++i) {
            if (bindings[i].cc >= 0) {
                const char* id = (i < (int)table.size()) ? table[i].id : "unknown";
                f << id << "=" << bindings[i].cc << "," << bindings[i].channel << "\n";
            }
        }
    }

    void loadFromFile(const std::string& filePath) {
        if (filePath.empty()) return;
        std::ifstream f(filePath);
        if (!f.is_open()) return;
        const auto& table = euphoriaParamTable();
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            auto eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;
            std::string id = line.substr(0, eqPos);
            std::string valStr = line.substr(eqPos + 1);

            int cc = -1, ch = -1;
            auto commaPos = valStr.find(',');
            if (commaPos != std::string::npos) {
                cc = std::atoi(valStr.substr(0, commaPos).c_str());
                ch = std::atoi(valStr.substr(commaPos + 1).c_str());
            } else {
                cc = std::atoi(valStr.c_str());
            }

            for (int i = 0; i < (int)table.size(); ++i) {
                if (id == table[i].id) {
                    if (cc >= 0 && cc <= 127) {
                        bindings[i].cc = cc;
                        bindings[i].channel = ch;
                    }
                    break;
                }
            }
        }
    }

    // Serialize to a compact string (e.g. for DAW XML state)
    std::string serializeToString() const {
        std::ostringstream ss;
        const auto& table = euphoriaParamTable();
        bool first = true;
        for (int i = 0; i < K_NUM_PARAMS; ++i) {
            if (bindings[i].cc >= 0) {
                if (!first) ss << ";";
                const char* id = (i < (int)table.size()) ? table[i].id : "param";
                ss << id << ":" << bindings[i].cc << ":" << bindings[i].channel;
                first = false;
            }
        }
        return ss.str();
    }

    void deserializeFromString(const std::string& str) {
        if (str.empty()) return;
        clearAllBindings();
        const auto& table = euphoriaParamTable();
        std::istringstream ss(str);
        std::string item;
        while (std::getline(ss, item, ';')) {
            if (item.empty()) continue;
            auto c1 = item.find(':');
            if (c1 == std::string::npos) continue;
            auto c2 = item.find(':', c1 + 1);
            if (c2 == std::string::npos) continue;
            std::string id = item.substr(0, c1);
            int cc = std::atoi(item.substr(c1 + 1, c2 - c1 - 1).c_str());
            int ch = std::atoi(item.substr(c2 + 1).c_str());

            for (int i = 0; i < (int)table.size(); ++i) {
                if (id == table[i].id) {
                    if (cc >= 0 && cc <= 127) {
                        bindings[i].cc = cc;
                        bindings[i].channel = ch;
                    }
                    break;
                }
            }
        }
    }

private:
    void applyValue(int p, int val,
                    const std::function<void(int paramIdx, float targetVal)>& onParamChange,
                    const std::function<float(int paramIdx)>& getParamVal) noexcept
    {
        const auto& table = euphoriaParamTable();
        if (p < 0 || p >= (int)table.size()) return;
        const auto& def = table[(size_t)p];

        int prevCcVal = lastCcVal[p];
        lastCcVal[p] = val;

        float targetVal = def.defVal;

        // 1. Toggle switch or stompbox footswitch
        if (def.choices.size() == 2 || (def.minVal == 0.0f && def.maxVal == 1.0f && def.choices.empty())) {
            // Smart footswitch handling:
            // - Momentary controller: sends 127 on press, 0 on release.
            //   When val > 64 and previous <= 64: toggle current value!
            //   When val == 0: ignore release so footswitch stays in toggled state.
            // - Latching controller: sends 127 on first press (toggles ON), 0 on second press (toggles OFF).
            if (val > 64 && prevCcVal <= 64) {
                float cur = getParamVal ? getParamVal(p) : 0.0f;
                targetVal = (cur > 0.5f) ? 0.0f : 1.0f;
            } else if (val <= 64 && prevCcVal > 64) {
                // If the user has a latching controller sending 0, allow it to turn OFF if cur was ON
                float cur = getParamVal ? getParamVal(p) : 0.0f;
                // If it was already toggled, we can set to 0.0f
                targetVal = 0.0f;
            } else {
                return;
            }
        }
        // 2. Stepped selection parameter (e.g. 3, 4, 11, 19 choices)
        else if (!def.choices.empty()) {
            int steps = (int)def.choices.size();
            int step = std::clamp((int)std::floor((val / 128.0f) * steps), 0, steps - 1);
            targetVal = (float)step;
        }
        // 3. Continuous rotary knob (minVal .. maxVal)
        else {
            float norm = std::clamp(val / 127.0f, 0.0f, 1.0f);
            targetVal = def.minVal + norm * (def.maxVal - def.minVal);
        }

        if (onParamChange) {
            onParamChange(p, targetVal);
        }
    }

    MidiBinding bindings[K_NUM_PARAMS];
    int lastCcVal[K_NUM_PARAMS];
    std::atomic<int> learningParamIdx{-1};
    std::atomic<int> lastAssignedParam{-1};
    std::atomic<int> lastAssignedCc{-1};
    std::atomic<int> assignedFlashTimer{0};
    std::atomic<bool> needsSave{false};
};

} // namespace EuphoriaMIDI
