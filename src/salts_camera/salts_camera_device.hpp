#pragma once

#include <capture.h>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <string>

namespace salts_camera {

class SaltsCameraFeed;
class SaltsCameraManager;

class SaltsCameraDevice : public godot::RefCounted {
    GDCLASS(SaltsCameraDevice, godot::RefCounted)

public:
    ~SaltsCameraDevice() override;
    godot::TypedArray<godot::Dictionary> list_modes(bool standard_only = true);
    godot::Ref<SaltsCameraFeed> create_feed(const godot::Dictionary &mode);
    void close();
    godot::Error get_last_error() const { return last_error_; }

protected:
    static void _bind_methods();

private:
    friend class SaltsCameraFeed;
    friend class SaltsCameraManager;

    godot::Error open(const godot::String &id, const godot::String &name);

    video_device_t *device_ = nullptr;
    SaltsCameraFeed *feed_ = nullptr;
    std::string id_;
    godot::String name_;
    godot::Error last_error_ = godot::OK;
};

} // namespace salts_camera
