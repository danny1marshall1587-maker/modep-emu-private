#ifndef SIMPLE_MIDI_FILE_HPP
#define SIMPLE_MIDI_FILE_HPP

#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>

struct MidiNoteEvent {
    double timestampSeconds;
    uint8_t status;
    uint8_t note;
    uint8_t velocity;
};

class SimpleMidiFile {
public:
    uint16_t format = 0;
    uint16_t numTracks = 0;
    uint16_t division = 480;
    std::vector<MidiNoteEvent> events;
    double durationSeconds = 0.0;
    bool isLoaded = false;
    std::string loadedPath;

    static uint32_t readVarLen(const uint8_t*& ptr, const uint8_t* end) {
        uint32_t val = 0;
        while (ptr < end) {
            uint8_t b = *ptr++;
            val = (val << 7) | (b & 0x7F);
            if (!(b & 0x80)) break;
        }
        return val;
    }

    static uint16_t readU16(const uint8_t* ptr) {
        return (uint16_t)((ptr[0] << 8) | ptr[1]);
    }

    static uint32_t readU32(const uint8_t* ptr) {
        return (uint32_t)((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]);
    }

    void clear() {
        events.clear();
        durationSeconds = 0.0;
        isLoaded = false;
        format = 0;
        numTracks = 0;
        division = 480;
        loadedPath.clear();
    }

    bool load(const std::string& filepath) {
        clear();
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        if (buffer.size() < 14) return false;
        const uint8_t* p = buffer.data();
        const uint8_t* end = p + buffer.size();

        if (memcmp(p, "MThd", 4) != 0) return false;
        uint32_t headerLen = readU32(p + 4);
        format = readU16(p + 8);
        numTracks = readU16(p + 10);
        division = readU16(p + 12);
        p += 8 + headerLen;

        struct RawEvent {
            uint32_t tick;
            uint8_t status;
            uint8_t data1;
            uint8_t data2;
            uint32_t tempoUsPerQuarter;
        };
        std::vector<RawEvent> allEvents;

        for (uint16_t t = 0; t < numTracks && p < end; ++t) {
            if (end - p < 8) break;
            if (memcmp(p, "MTrk", 4) != 0) {
                p += 4;
                if (p + 4 > end) break;
                uint32_t skipLen = readU32(p);
                p += 4 + skipLen;
                continue;
            }
            uint32_t trackLen = readU32(p + 4);
            p += 8;
            const uint8_t* trackEnd = std::min(end, p + trackLen);
            const uint8_t* tp = p;

            uint32_t currentTick = 0;
            uint8_t runningStatus = 0;

            while (tp < trackEnd) {
                uint32_t delta = readVarLen(tp, trackEnd);
                currentTick += delta;
                if (tp >= trackEnd) break;

                uint8_t status = *tp;
                if (status & 0x80) {
                    runningStatus = status;
                    tp++;
                } else {
                    status = runningStatus;
                }

                if (status == 0xFF) { // Meta event
                    if (tp >= trackEnd) break;
                    uint8_t metaType = *tp++;
                    uint32_t metaLen = readVarLen(tp, trackEnd);
                    if (metaType == 0x51 && metaLen == 3 && tp + 3 <= trackEnd) { // Set Tempo
                        uint32_t usPerQ = (tp[0] << 16) | (tp[1] << 8) | tp[2];
                        allEvents.push_back({currentTick, 0xFF, 0x51, 0, usPerQ});
                    }
                    tp += metaLen;
                } else if (status == 0xF0 || status == 0xF7) { // SysEx
                    uint32_t sysexLen = readVarLen(tp, trackEnd);
                    tp += sysexLen;
                } else {
                    uint8_t type = status & 0xF0;
                    uint8_t d1 = (tp < trackEnd) ? *tp++ : 0;
                    uint8_t d2 = 0;
                    if (type != 0xC0 && type != 0xD0) {
                        d2 = (tp < trackEnd) ? *tp++ : 0;
                    }
                    if (type == 0x90 || type == 0x80) {
                        allEvents.push_back({currentTick, status, d1, d2, 0});
                    }
                }
            }
            p = trackEnd;
        }

        std::sort(allEvents.begin(), allEvents.end(), [](const RawEvent& a, const RawEvent& b) {
            return a.tick < b.tick;
        });

        uint32_t currentTempoUs = 500000; // default 120 BPM
        uint32_t lastTick = 0;
        double currentSec = 0.0;
        uint16_t ticksPerQuarter = (division & 0x8000) ? 480 : division;
        if (ticksPerQuarter == 0) ticksPerQuarter = 480;

        for (const auto& ev : allEvents) {
            uint32_t tickDiff = ev.tick - lastTick;
            double secPerTick = ((double)currentTempoUs / 1000000.0) / (double)ticksPerQuarter;
            currentSec += (double)tickDiff * secPerTick;
            lastTick = ev.tick;

            if (ev.status == 0xFF && ev.data1 == 0x51) {
                currentTempoUs = ev.tempoUsPerQuarter;
            } else if ((ev.status & 0xF0) == 0x90 || (ev.status & 0xF0) == 0x80) {
                events.push_back({currentSec, ev.status, ev.data1, ev.data2});
            }
        }
        durationSeconds = currentSec;
        isLoaded = !events.empty();
        if (isLoaded) {
            loadedPath = filepath;
        }
        return isLoaded;
    }
};

#endif // SIMPLE_MIDI_FILE_HPP
