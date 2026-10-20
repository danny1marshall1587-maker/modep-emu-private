#pragma once
#include <vector>
#include <atomic>
#include <cstring>
#include <cstdint>

namespace ModepDAW {

// Single-Producer Single-Consumer (SPSC) lock-free ring buffer
// Guarantees zero allocations and zero locks inside the real-time audio thread
class LockFreeRingBuffer {
public:
    LockFreeRingBuffer(size_t capacityBytes = 4 * 1024 * 1024) // 4 MB default = ~45s of 48k/24b mono audio
        : capacity(capacityBytes), writePos(0), readPos(0) {
        buffer = new uint8_t[capacity];
        std::memset(buffer, 0, capacity);
    }

    ~LockFreeRingBuffer() {
        delete[] buffer;
    }

    size_t getAvailableWrite() const {
        size_t w = writePos.load(std::memory_order_relaxed);
        size_t r = readPos.load(std::memory_order_acquire);
        if (w >= r) {
            return capacity - (w - r) - 1;
        } else {
            return r - w - 1;
        }
    }

    size_t getAvailableRead() const {
        size_t w = writePos.load(std::memory_order_acquire);
        size_t r = readPos.load(std::memory_order_relaxed);
        if (w >= r) {
            return w - r;
        } else {
            return capacity - (r - w);
        }
    }

    // Called strictly from the Real-Time audio thread
    bool write(const uint8_t* data, size_t bytes) {
        if (getAvailableWrite() < bytes) return false; // Buffer overflow protection

        size_t w = writePos.load(std::memory_order_relaxed);
        size_t firstChunk = std::min(bytes, capacity - w);
        std::memcpy(buffer + w, data, firstChunk);

        if (bytes > firstChunk) {
            std::memcpy(buffer, data + firstChunk, bytes - firstChunk);
        }

        writePos.store((w + bytes) % capacity, std::memory_order_release);
        return true;
    }

    // Called strictly from the Background disk-writer thread
    size_t read(uint8_t* dest, size_t maxBytes) {
        size_t avail = getAvailableRead();
        size_t bytesToRead = std::min(avail, maxBytes);
        if (bytesToRead == 0) return 0;

        size_t r = readPos.load(std::memory_order_relaxed);
        size_t firstChunk = std::min(bytesToRead, capacity - r);
        std::memcpy(dest, buffer + r, firstChunk);

        if (bytesToRead > firstChunk) {
            std::memcpy(dest + firstChunk, buffer, bytesToRead - firstChunk);
        }

        readPos.store((r + bytesToRead) % capacity, std::memory_order_release);
        return bytesToRead;
    }

private:
    uint8_t* buffer;
    size_t capacity;
    std::atomic<size_t> writePos;
    std::atomic<size_t> readPos;
};

} // namespace ModepDAW
