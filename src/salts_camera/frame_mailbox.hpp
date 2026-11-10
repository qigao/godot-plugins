#pragma once

#include "frame_layout.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace salts_camera {

class FrameMailbox {
public:
    using Clock = std::chrono::steady_clock;

    struct Frame {
        const uint8_t *bytes;
        size_t size;
        uint64_t timestamp_us;
        Clock::time_point arrived;
    };

    bool prepare(const FrameLayout &layout) noexcept;
    void publish(const uint8_t *bytes, size_t size, int width, int height,
                 uint64_t timestamp_us) noexcept;

    // The consumer pins the returned slot until its next take or clear.
    const Frame *take_latest() noexcept;
    const Frame *latest() const noexcept;

    // The capture owner resets between runs and discards frames after stop.
    void reset() noexcept;
    void discard() noexcept;
    void release() noexcept;
    void report_capture_error() noexcept { capture_error_.store(true, std::memory_order_relaxed); }
    bool capture_failed() const noexcept { return capture_error_.load(std::memory_order_relaxed); }
    uint64_t captured() const noexcept { return captured_.load(std::memory_order_relaxed); }
    uint64_t dropped() const noexcept { return dropped_.load(std::memory_order_relaxed); }

private:
    struct Slot {
        std::unique_ptr<uint8_t[]> buffer;
        Frame frame{};
    };

    std::array<Slot, 3> slots_;
    FrameLayout layout_{};
    std::mutex mutex_;
    int ready_ = -1;
    int pinned_ = -1;
    int next_ = 0;
    std::atomic<uint64_t> captured_{0};
    std::atomic<uint64_t> dropped_{0};
    std::atomic<bool> capture_error_{false};
};

} // namespace salts_camera
