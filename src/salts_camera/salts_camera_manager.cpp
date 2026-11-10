#include "salts_camera_manager.hpp"

#include "salts_camera_device.hpp"

#include <array>
#include <godot_cpp/core/class_db.hpp>

namespace salts_camera {

using namespace godot;

void SaltsCameraManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("list_devices"), &SaltsCameraManager::list_devices);
    ClassDB::bind_method(D_METHOD("open_device", "id"), &SaltsCameraManager::open_device);
    ClassDB::bind_method(D_METHOD("permission_status"), &SaltsCameraManager::permission_status);
    ClassDB::bind_method(D_METHOD("request_permission"), &SaltsCameraManager::request_permission);
    ClassDB::bind_method(D_METHOD("get_last_error"), &SaltsCameraManager::get_last_error);
    BIND_ENUM_CONSTANT(PERMISSION_UNKNOWN);
    BIND_ENUM_CONSTANT(PERMISSION_REQUESTABLE);
    BIND_ENUM_CONSTANT(PERMISSION_GRANTED);
    BIND_ENUM_CONSTANT(PERMISSION_DENIED);
    BIND_ENUM_CONSTANT(PERMISSION_RESTRICTED);
}

TypedArray<Dictionary> SaltsCameraManager::list_devices() {
    TypedArray<Dictionary> result;
    const auto permission = permission_status();
    if (permission == PERMISSION_DENIED || permission == PERMISSION_RESTRICTED) {
        last_error_ = ERR_UNAUTHORIZED;
        return result;
    }
    std::array<capture_device_t, SALTS_CAPTURE_MAX_DEVICES> devices{};
    const int count = capture_list_video_devices(devices.data(), static_cast<int>(devices.size()));
    if (count < 0) {
        last_error_ = ERR_CANT_ACQUIRE_RESOURCE;
        return result;
    }
    for (int index = 0; index < count; ++index) {
        Dictionary entry;
        entry["id"] = String::utf8(devices[index].id);
        entry["name"] = String::utf8(devices[index].name);
        entry["is_default"] = devices[index].is_default != 0;
        result.append(entry);
    }
    last_error_ = OK;
    return result;
}

Ref<SaltsCameraDevice> SaltsCameraManager::open_device(const String &id) {
    if (id.is_empty()) {
        last_error_ = ERR_INVALID_PARAMETER;
        return {};
    }
    if (permission_status() == PERMISSION_REQUESTABLE) {
        last_error_ = ERR_UNAUTHORIZED;
        return {};
    }
    const auto devices = list_devices();
    if (last_error_ != OK) {
        return {};
    }
    for (int64_t index = 0; index < devices.size(); ++index) {
        const Dictionary entry = devices[index];
        if (String(entry["id"]) == id) {
            Ref<SaltsCameraDevice> device;
            device.instantiate();
            last_error_ = device->open(id, entry["name"]);
            return last_error_ == OK ? device : Ref<SaltsCameraDevice>();
        }
    }
    last_error_ = ERR_DOES_NOT_EXIST;
    return {};
}

#ifndef __APPLE__
SaltsCameraManager::PermissionStatus SaltsCameraManager::permission_status() const {
    return PERMISSION_UNKNOWN;
}

Error SaltsCameraManager::request_permission() const {
    return ERR_UNAVAILABLE;
}
#endif

} // namespace salts_camera
