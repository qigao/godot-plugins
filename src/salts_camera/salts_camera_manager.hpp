#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace salts_camera {

class SaltsCameraDevice;

class SaltsCameraManager : public godot::RefCounted {
    GDCLASS(SaltsCameraManager, godot::RefCounted)

public:
    enum PermissionStatus {
        PERMISSION_UNKNOWN,
        PERMISSION_REQUESTABLE,
        PERMISSION_GRANTED,
        PERMISSION_DENIED,
        PERMISSION_RESTRICTED,
    };

    godot::TypedArray<godot::Dictionary> list_devices();
    godot::Ref<SaltsCameraDevice> open_device(const godot::String &id);
    PermissionStatus permission_status() const;
    godot::Error request_permission() const;
    godot::Error get_last_error() const { return last_error_; }

protected:
    static void _bind_methods();

private:
    godot::Error last_error_ = godot::OK;
};

} // namespace salts_camera

VARIANT_ENUM_CAST(salts_camera::SaltsCameraManager::PermissionStatus)
