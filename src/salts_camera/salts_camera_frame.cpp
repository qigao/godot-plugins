#include "salts_camera_frame.hpp"

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
    BIND_ENUM_CONSTANT(FORMAT_I420);
    BIND_ENUM_CONSTANT(FORMAT_NV12);
    BIND_ENUM_CONSTANT(FORMAT_RGB24);
    BIND_ENUM_CONSTANT(FORMAT_BGRA);
    BIND_ENUM_CONSTANT(FORMAT_MJPEG);
}

} // namespace salts_camera
