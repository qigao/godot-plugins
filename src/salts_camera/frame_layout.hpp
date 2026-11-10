#pragma once

#include <capture.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace salts_camera {

struct FrameLayout {
    int width;
    int height;
    int format;
    size_t capacity;
    size_t rgba_bytes;

    static std::optional<FrameLayout> from_mode(const video_native_mode_t &mode) noexcept;
    bool accepts(int frame_width, int frame_height, size_t size) const noexcept;
};

bool convert_rgba8(const FrameLayout &layout, const uint8_t *source, size_t size,
                   uint8_t *destination) noexcept;

} // namespace salts_camera
