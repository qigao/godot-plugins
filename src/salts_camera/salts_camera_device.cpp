#include "salts_camera_device.hpp"

#include "native_capture.hpp"
#include "salts_camera_feed.hpp"

#include <array>
#include <limits>
#include <set>
#include <utility>
#include <vector>
#include <godot_cpp/core/class_db.hpp>

namespace salts_camera {

using namespace godot;

namespace {

// The Godot main thread serializes all device operations.
std::set<std::string> open_devices;

Dictionary encode_mode(const video_native_mode_t &mode, int64_t owner) {
    Dictionary entry;
    entry["device_token"] = owner;
    entry["mode_id"] = static_cast<int64_t>(mode.mode_id);
    entry["width"] = mode.width;
    entry["height"] = mode.height;
    entry["framerate_numerator"] = mode.framerate_numerator;
    entry["framerate_denominator"] = mode.framerate_denominator;
    entry["format"] = mode.format;
    return entry;
}

bool decode_mode(const Dictionary &entry, int64_t owner, video_native_mode_t &mode) {
    constexpr std::array keys = {"device_token", "mode_id", "width", "height",
                                 "framerate_numerator", "framerate_denominator", "format"};
    std::array<int64_t, keys.size()> fields{};
    for (size_t index = 0; index < keys.size(); ++index) {
        const Variant value = entry.get(keys[index], Variant());
        if (value.get_type() != Variant::INT) {
            return false;
        }
        fields[index] = value;
    }
    const auto [token, id, width, height, numerator, denominator, format] = fields;
    if (token != owner || width <= 0 || width > std::numeric_limits<int>::max() ||
        height <= 0 || height > std::numeric_limits<int>::max() ||
        numerator <= 0 || numerator > UINT32_MAX || denominator <= 0 || denominator > UINT32_MAX ||
        format < SALTS_VIDEO_CAPTURE_FORMAT_I420 || format > SALTS_VIDEO_CAPTURE_FORMAT_MJPEG) {
        return false;
    }
    mode = {static_cast<int>(width), static_cast<int>(height), static_cast<uint32_t>(numerator),
            static_cast<uint32_t>(denominator), static_cast<int>(format), static_cast<uint64_t>(id)};
    return true;
}

} // namespace

void SaltsCameraDevice::_bind_methods() {
    ClassDB::bind_method(D_METHOD("list_modes", "standard_only"), &SaltsCameraDevice::list_modes,
                         DEFVAL(true));
    ClassDB::bind_method(D_METHOD("create_feed", "mode"), &SaltsCameraDevice::create_feed);
    ClassDB::bind_method(D_METHOD("close"), &SaltsCameraDevice::close);
    ClassDB::bind_method(D_METHOD("get_last_error"), &SaltsCameraDevice::get_last_error);
}

SaltsCameraDevice::~SaltsCameraDevice() {
    close();
}

Error SaltsCameraDevice::open(const String &id, const String &name) {
    const auto utf8 = id.utf8();
    id_.assign(utf8.get_data(), static_cast<size_t>(utf8.length()));
    if (open_devices.count(id_) != 0) {
        return ERR_BUSY;
    }
    const Error error = capture_error(video_device_open(id_.c_str(), &device_));
    if (error == OK) {
        open_devices.insert(id_);
        name_ = name;
    }
    return error;
}

TypedArray<Dictionary> SaltsCameraDevice::list_modes(bool standard_only) {
    TypedArray<Dictionary> result;
    if (!device_) {
        last_error_ = ERR_UNCONFIGURED;
        return result;
    }
    constexpr size_t max_modes = 65536;
    std::vector<video_native_mode_t> modes(SALTS_CAPTURE_MAX_VIDEO_MODES);
    size_t count;
    for (;;) {
        last_error_ = capture_error(video_device_list_modes_all(device_, modes.data(), modes.size(), &count));
        if (last_error_ != OK) {
            return result;
        }
        if (count < modes.size()) {
            break;
        }
        if (modes.size() == max_modes) {
            last_error_ = ERR_PARAMETER_RANGE_ERROR;
            return result;
        }
        modes.resize(modes.size() * 2);
    }
    for (size_t index = 0; index < count; ++index) {
        if (!standard_only || video_mode_is_standard_fps(&modes[index])) {
            result.append(encode_mode(modes[index], static_cast<int64_t>(get_instance_id())));
        }
    }
    return result;
}

Ref<SaltsCameraFeed> SaltsCameraDevice::create_feed(const Dictionary &mode) {
    if (!device_ || feed_) {
        last_error_ = device_ ? ERR_BUSY : ERR_UNCONFIGURED;
        return {};
    }
    video_native_mode_t native_mode{};
    if (!decode_mode(mode, static_cast<int64_t>(get_instance_id()), native_mode)) {
        last_error_ = ERR_INVALID_PARAMETER;
        return {};
    }
    Ref<SaltsCameraFeed> feed;
    feed.instantiate();
    last_error_ = feed->configure(Ref<SaltsCameraDevice>(this), native_mode);
    if (last_error_ != OK) {
        return {};
    }
    feed_ = feed.ptr();
    return feed;
}

void SaltsCameraDevice::close() {
    if (!device_) {
        return;
    }
    auto *device = std::exchange(device_, nullptr);
    if (feed_) {
        Ref<SaltsCameraFeed> feed(feed_);
        feed->close();
    }
    video_device_close(device);
    open_devices.erase(id_);
    last_error_ = OK;
}

} // namespace salts_camera
