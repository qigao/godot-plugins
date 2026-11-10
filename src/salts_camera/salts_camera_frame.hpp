#pragma once

#include <capture.h>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace salts_camera {

class SaltsCameraFrame : public godot::RefCounted {
    GDCLASS(SaltsCameraFrame, godot::RefCounted)

public:
    enum Format {
        FORMAT_I420 = SALTS_VIDEO_CAPTURE_FORMAT_I420,
        FORMAT_NV12 = SALTS_VIDEO_CAPTURE_FORMAT_NV12,
        FORMAT_RGB24 = SALTS_VIDEO_CAPTURE_FORMAT_RGB24,
        FORMAT_BGRA = SALTS_VIDEO_CAPTURE_FORMAT_BGRA,
        FORMAT_MJPEG = SALTS_VIDEO_CAPTURE_FORMAT_MJPEG,
    };

    godot::PackedByteArray get_bytes() const { return bytes_; }
    int get_format() const { return format_; }
    int get_width() const { return width_; }
    int get_height() const { return height_; }
    int64_t get_timestamp_us() const { return timestamp_us_; }
    godot::Ref<godot::Image> to_image() const;

protected:
    static void _bind_methods();

private:
    friend class SaltsCameraFeed;
    godot::PackedByteArray bytes_;
    int format_ = -1;
    int width_ = 0;
    int height_ = 0;
    int64_t timestamp_us_ = 0;
};

} // namespace salts_camera

VARIANT_ENUM_CAST(salts_camera::SaltsCameraFrame::Format)
