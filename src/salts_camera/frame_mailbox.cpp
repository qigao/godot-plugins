#include "frame_mailbox.hpp"

#include <cstring>
#include <new>
#include <utility>

namespace salts_camera {

bool FrameMailbox::prepare(const FrameLayout &layout) noexcept {
    std::array<Slot, 3> slots;
    for (auto &slot : slots) {
        slot.buffer.reset(new (std::nothrow) uint8_t[layout.capacity]);
        if (!slot.buffer) {
            return false;
        }
    }
    slots_ = std::move(slots);
    layout_ = layout;
    reset();
    return true;
}

void FrameMailbox::publish(const uint8_t *bytes, size_t size, int width, int height,
                           uint64_t timestamp_us) noexcept {
    captured_.fetch_add(1, std::memory_order_relaxed);
    if (!bytes || !layout_.accepts(width, height, size)) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const auto arrived = Clock::now();
    const std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (ready_ >= 0) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
    }
    if (next_ == pinned_) {
        next_ = (next_ + 1) % static_cast<int>(slots_.size());
    }
    auto &slot = slots_[next_];
    std::memcpy(slot.buffer.get(), bytes, size);
    slot.frame = {slot.buffer.get(), size, timestamp_us, arrived};
    ready_ = next_;
    next_ = (next_ + 1) % static_cast<int>(slots_.size());
}

const FrameMailbox::Frame *FrameMailbox::take_latest() noexcept {
    const std::lock_guard lock(mutex_);
    if (ready_ < 0) {
        return nullptr;
    }
    pinned_ = ready_;
    ready_ = -1;
    return latest();
}

const FrameMailbox::Frame *FrameMailbox::latest() const noexcept {
    return pinned_ < 0 ? nullptr : &slots_[pinned_].frame;
}

void FrameMailbox::discard() noexcept {
    ready_ = -1;
    pinned_ = -1;
    next_ = 0;
}

void FrameMailbox::reset() noexcept {
    discard();
    captured_.store(0, std::memory_order_relaxed);
    dropped_.store(0, std::memory_order_relaxed);
    capture_error_.store(false, std::memory_order_relaxed);
}

void FrameMailbox::release() noexcept {
    discard();
    slots_ = {};
    layout_ = {};
}

} // namespace salts_camera
