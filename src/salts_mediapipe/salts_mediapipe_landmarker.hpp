#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <memory>

namespace salts_mediapipe {

class SaltsMediaPipeLandmarker : public godot::RefCounted {
    GDCLASS(SaltsMediaPipeLandmarker, godot::RefCounted)

public:
    enum Task { FACE, HAND, POSE };
    enum Mode { IMAGE, VIDEO };

    SaltsMediaPipeLandmarker();
    ~SaltsMediaPipeLandmarker() override;

    godot::Error open(const godot::PackedByteArray &model, Task task, Mode mode = IMAGE,
                      int64_t max_subjects = 1);
    godot::TypedArray<godot::PackedVector3Array> detect(const godot::Ref<godot::Image> &image,
                                                      int64_t timestamp_ms = -1);
    godot::Error close();
    godot::Error get_last_error() const { return last_error_; }

protected:
    static void _bind_methods();

private:
    struct Session;
    std::unique_ptr<Session> session_;
    godot::Error last_error_ = godot::OK;
};

} // namespace salts_mediapipe

VARIANT_ENUM_CAST(salts_mediapipe::SaltsMediaPipeLandmarker::Task)
VARIANT_ENUM_CAST(salts_mediapipe::SaltsMediaPipeLandmarker::Mode)
