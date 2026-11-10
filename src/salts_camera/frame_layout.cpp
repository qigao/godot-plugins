#include "frame_layout.hpp"

#include <algorithm>

#ifndef __APPLE__
#include <libyuv/convert_argb.h>

extern "C" bool salts_camera_decode_jpeg(const uint8_t *source, size_t size, int width,
                                         int height, uint8_t *destination);
#endif

namespace salts_camera {

std::optional<FrameLayout> FrameLayout::from_mode(const video_native_mode_t &mode) noexcept {
    constexpr size_t max_rgba_bytes = 64 * 1024 * 1024;
    if (mode.width <= 0 || mode.height <= 0) {
        return std::nullopt;
    }
    const auto pixels = static_cast<uint64_t>(mode.width) * mode.height;
    if (pixels > max_rgba_bytes / 4) {
        return std::nullopt;
    }
    const size_t rgba_bytes = static_cast<size_t>(pixels) * 4;
    size_t capacity;
    switch (mode.format) {
        case SALTS_VIDEO_CAPTURE_FORMAT_BGRA:
            capacity = rgba_bytes;
            break;
        case SALTS_VIDEO_CAPTURE_FORMAT_RGB24:
            capacity = static_cast<size_t>(pixels) * 3;
            break;
#ifndef __APPLE__
        case SALTS_VIDEO_CAPTURE_FORMAT_I420:
        case SALTS_VIDEO_CAPTURE_FORMAT_NV12:
            if (mode.width % 2 || mode.height % 2) {
                return std::nullopt;
            }
            capacity = static_cast<size_t>(pixels + pixels / 2);
            break;
        case SALTS_VIDEO_CAPTURE_FORMAT_MJPEG:
            capacity = std::max<size_t>(64 * 1024, rgba_bytes);
            break;
#endif
        default:
            return std::nullopt;
    }
    return FrameLayout{mode.width, mode.height, mode.format, capacity, rgba_bytes};
}

bool FrameLayout::accepts(int frame_width, int frame_height, size_t size) const noexcept {
    if (frame_width != width || frame_height != height) {
        return false;
    }
    if (format == SALTS_VIDEO_CAPTURE_FORMAT_MJPEG) {
        return size > 0 && size <= capacity;
    }
    return size == capacity;
}

bool convert_rgba8(const FrameLayout &layout, const uint8_t *source, size_t size,
                   uint8_t *destination) noexcept {
    if (!layout.accepts(layout.width, layout.height, size)) {
        return false;
    }
    const size_t pixels = layout.rgba_bytes / 4;
#ifndef __APPLE__
    const int stride = layout.width * 4;
#endif
    switch (layout.format) {
        case SALTS_VIDEO_CAPTURE_FORMAT_BGRA:
            for (size_t pixel = 0; pixel < pixels; ++pixel, source += 4, destination += 4) {
                destination[0] = source[2];
                destination[1] = source[1];
                destination[2] = source[0];
                destination[3] = source[3];
            }
            return true;
        case SALTS_VIDEO_CAPTURE_FORMAT_RGB24:
            // Media Foundation's packed RGB24 is BGR in memory.
            for (size_t pixel = 0; pixel < pixels; ++pixel, source += 3, destination += 4) {
#ifdef _WIN32
                destination[0] = source[2];
                destination[2] = source[0];
#else
                destination[0] = source[0];
                destination[2] = source[2];
#endif
                destination[1] = source[1];
                destination[3] = 255;
            }
            return true;
#ifndef __APPLE__
        case SALTS_VIDEO_CAPTURE_FORMAT_I420:
            return libyuv::I420ToABGR(source, layout.width, source + pixels, layout.width / 2,
                                     source + pixels + pixels / 4, layout.width / 2,
                                     destination, stride, layout.width, layout.height) == 0;
        case SALTS_VIDEO_CAPTURE_FORMAT_NV12:
            return libyuv::NV12ToABGR(source, layout.width, source + pixels, layout.width,
                                     destination, stride, layout.width, layout.height) == 0;
        case SALTS_VIDEO_CAPTURE_FORMAT_MJPEG:
            return salts_camera_decode_jpeg(source, size, layout.width, layout.height, destination);
#endif
        default:
            return false;
    }
}

} // namespace salts_camera
