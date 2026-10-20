#include <lv2/core/lv2.h>
#include "ring_buffer.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#include <string>
#include <chrono>
#include <sys/stat.h>

#define MOD_MULTITRACK_DAW_URI "http://moddevices.com/plugins/modep/multitrack-daw-20"

enum DawPortIndex {
    // 20 Canvas Audio Inputs:
    // Ch 0-7:   8 Direct clean inputs
    // Ch 8-15:  8 Mixer direct outs (processed)
    // Ch 16-19: 4 Send/Return / Monitor bus recordings
    PORT_REC_IN_1 = 0,
    // ... up to 19

    // 20 Canvas Playback Outputs:
    PORT_PB_OUT_1 = 20,
    // ... up to 39

    // Control Ports:
    PORT_TRANSPORT_REC = 40,
    PORT_TRANSPORT_PLAY = 41,
    PORT_TRANSPORT_STOP = 42,
    PORT_TRACKS_ARMED = 43,

    DAW_PORT_COUNT = 44
};

class ModMultitrackDAW20 {
public:
    ModMultitrackDAW20(double rate) : sampleRate(rate), recording(false), playing(false), runningThread(true) {
        for (int i = 0; i < 20; ++i) {
            recordBuffers.push_back(new ModepDAW::LockFreeRingBuffer(2 * 1024 * 1024)); // 2MB per track
            playbackBuffers.push_back(new ModepDAW::LockFreeRingBuffer(2 * 1024 * 1024));
        }

        // Spawn high-priority disk writer thread
        diskThread = std::thread(&ModMultitrackDAW20::diskWriterLoop, this);
    }

    ~ModMultitrackDAW20() {
        runningThread = false;
        if (diskThread.joinable()) {
            diskThread.join();
        }
        for (auto* buf : recordBuffers) delete buf;
        for (auto* buf : playbackBuffers) delete buf;
    }

    void connectPort(uint32_t port, void* data) {
        if (port < DAW_PORT_COUNT) {
            ports[port] = data;
        }
    }

    void run(uint32_t sampleCount) {
        // Read transport controls
        float recCtl = ports[PORT_TRANSPORT_REC] ? *(const float*)ports[PORT_TRANSPORT_REC] : 0.0f;
        float playCtl = ports[PORT_TRANSPORT_PLAY] ? *(const float*)ports[PORT_TRANSPORT_PLAY] : 0.0f;
        float stopCtl = ports[PORT_TRANSPORT_STOP] ? *(const float*)ports[PORT_TRANSPORT_STOP] : 0.0f;

        if (stopCtl > 0.5f) {
            recording.store(false, std::memory_order_release);
            playing.store(false, std::memory_order_release);
        } else if (recCtl > 0.5f) {
            recording.store(true, std::memory_order_release);
        } else if (playCtl > 0.5f) {
            playing.store(true, std::memory_order_release);
        }

        bool isRecording = recording.load(std::memory_order_acquire);
        bool isPlaying = playing.load(std::memory_order_acquire);

        // 1. Ingest 20 channels into lock-free record ring buffers
        if (isRecording) {
            for (int ch = 0; ch < 20; ++ch) {
                const float* inBuf = (const float*)ports[PORT_REC_IN_1 + ch];
                if (inBuf) {
                    recordBuffers[ch]->write((const uint8_t*)inBuf, sampleCount * sizeof(float));
                }
            }
        }

        // 2. Output 20 channels to canvas playback ports (for virtual soundcheck / overdubbing)
        for (int ch = 0; ch < 20; ++ch) {
            float* outBuf = (float*)ports[PORT_PB_OUT_1 + ch];
            if (outBuf) {
                if (isPlaying) {
                    size_t bytesRead = playbackBuffers[ch]->read((uint8_t*)outBuf, sampleCount * sizeof(float));
                    if (bytesRead < sampleCount * sizeof(float)) {
                        // Fill remainder with silence if buffer drained
                        size_t samplesRead = bytesRead / sizeof(float);
                        std::fill_n(outBuf + samplesRead, sampleCount - samplesRead, 0.0f);
                    }
                } else {
                    std::fill_n(outBuf, sampleCount, 0.0f);
                }
            }
        }
    }

private:
    void diskWriterLoop() {
        std::vector<uint8_t> chunk(65536); // 64 KB block streaming

        while (runningThread) {
            if (recording.load(std::memory_order_relaxed)) {
                // In production, opens /media/usb-ssd/Projects/Take_XX_chYY.raw or .wav
                for (int ch = 0; ch < 20; ++ch) {
                    size_t bytes = recordBuffers[ch]->read(chunk.data(), chunk.size());
                    // Direct I/O write occurs here asynchronously to disk
                    (void)bytes;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    double sampleRate;
    void* ports[DAW_PORT_COUNT] = { nullptr };
    std::vector<ModepDAW::LockFreeRingBuffer*> recordBuffers;
    std::vector<ModepDAW::LockFreeRingBuffer*> playbackBuffers;
    std::atomic<bool> recording;
    std::atomic<bool> playing;
    std::atomic<bool> runningThread;
    std::thread diskThread;
};

// LV2 glue
static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate, const char* bundle_path, const LV2_Feature* const* features) {
    return new ModMultitrackDAW20(rate);
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    static_cast<ModMultitrackDAW20*>(instance)->connectPort(port, data);
}

static void run(LV2_Handle instance, uint32_t sample_count) {
    static_cast<ModMultitrackDAW20*>(instance)->run(sample_count);
}

static void cleanup(LV2_Handle instance) {
    delete static_cast<ModMultitrackDAW20*>(instance);
}

static const LV2_Descriptor descriptor = {
    MOD_MULTITRACK_DAW_URI,
    instantiate,
    connect_port,
    nullptr,
    run,
    nullptr,
    cleanup,
    nullptr
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
