#include "salts_camera_frame.hpp"

#include "frame_layout.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace salts_camera {

using godot::ClassDB;
using godot::D_METHOD;

void SaltsCameraFrame::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_bytes"), &SaltsCameraFrame::get_bytes);
    ClassDB::bind_method(D_METHOD("get_format"), &SaltsCameraFrame::get_format);
    ClassDB::bind_method(D_METHOD("get_width"), &SaltsCameraFrame::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &SaltsCameraFrame::get_height);
    ClassDB::bind_method(D_METHOD("get_timestamp_us"), &SaltsCameraFrame::get_timestamp_us);
    ClassDB::bind_method(D_METHOD("to_image"), &SaltsCameraFrame::to_image);
    BIND_ENUM_CONSTANT(FORMAT_I420);
    BIND_ENUM_CONSTANT(FORMAT_NV12);
    BIND_ENUM_CONSTANT(FORMAT_RGB24);
    BIND_ENUM_CONSTANT(FORMAT_BGRA);
    BIND_ENUM_CONSTANT(FORMAT_MJPEG);
}

godot::Ref<godot::Image> SaltsCameraFrame::to_image() const {
    video_native_mode_t mode{};
    mode.width = width_;
    mode.height = height_;
    mode.format = format_;
    const auto layout = FrameLayout::from_mode(mode);
    if (!layout || !layout->accepts(width_, height_, bytes_.size())) {
        return {};
    }
    godot::PackedByteArray rgba;
    if (rgba.resize(layout->rgba_bytes) != godot::OK ||
        !convert_rgba8(*layout, bytes_.ptr(), bytes_.size(), rgba.ptrw())) {
        return {};
    }
    return godot::Image::create_from_data(width_, height_, false, godot::Image::FORMAT_RGBA8, rgba);
}

} // namespace salts_camera
