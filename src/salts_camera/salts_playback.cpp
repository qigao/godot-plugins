#include "salts_playback.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace salts_camera {

using namespace godot;

namespace {

Error playback_error(int result) {
    switch (result) {
        case SALTS_PLAYBACK_OK: return OK;
        case SALTS_PLAYBACK_ERR_NOMEM: return ERR_OUT_OF_MEMORY;
        case SALTS_PLAYBACK_ERR_FORMAT: return ERR_INVALID_PARAMETER;
        case SALTS_PLAYBACK_ERR_BUSY: return ERR_BUSY;
        case SALTS_PLAYBACK_ERR_TIMEOUT: return ERR_TIMEOUT;
        case SALTS_PLAYBACK_ERR_UNSUPPORTED: return ERR_UNAVAILABLE;
        default: return ERR_CANT_ACQUIRE_RESOURCE;
    }
}

} // namespace

void SaltsPlayback::_bind_methods() {
    ClassDB::bind_method(D_METHOD("list_devices"), &SaltsPlayback::list_devices);
    ClassDB::bind_method(D_METHOD("open_device", "id", "sample_rate", "channels", "format", "buffer_ms"),
                         &SaltsPlayback::open_device, DEFVAL(48000), DEFVAL(2), DEFVAL(FORMAT_F32), DEFVAL(100));
    ClassDB::bind_method(D_METHOD("start"), &SaltsPlayback::start);
    ClassDB::bind_method(D_METHOD("pause"), &SaltsPlayback::pause);
    ClassDB::bind_method(D_METHOD("resume"), &SaltsPlayback::resume);
    ClassDB::bind_method(D_METHOD("stop"), &SaltsPlayback::stop);
    ClassDB::bind_method(D_METHOD("close"), &SaltsPlayback::close);
    ClassDB::bind_method(D_METHOD("get_state"), &SaltsPlayback::get_state);
    ClassDB::bind_method(D_METHOD("write_pcm", "bytes", "byte_offset"), &SaltsPlayback::write_pcm, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("get_available_bytes"), &SaltsPlayback::get_available_bytes);
    ClassDB::bind_method(D_METHOD("get_buffered_bytes"), &SaltsPlayback::get_buffered_bytes);
    ClassDB::bind_method(D_METHOD("get_last_error"), &SaltsPlayback::get_last_error);
    BIND_ENUM_CONSTANT(FORMAT_S16);
    BIND_ENUM_CONSTANT(FORMAT_S32);
    BIND_ENUM_CONSTANT(FORMAT_F32);
    BIND_ENUM_CONSTANT(STATE_CLOSED);
    BIND_ENUM_CONSTANT(STATE_STOPPED);
    BIND_ENUM_CONSTANT(STATE_STARTING);
    BIND_ENUM_CONSTANT(STATE_PLAYING);
    BIND_ENUM_CONSTANT(STATE_PAUSED);
    BIND_ENUM_CONSTANT(STATE_STOPPING);
    BIND_ENUM_CONSTANT(STATE_ERROR);
}

SaltsPlayback::~SaltsPlayback() {
    close();
}

String SaltsPlayback::device_id(size_t index) const {
    return String::num_uint64(get_instance_id()) + ":" + String::num_uint64(enumeration_) +
           ":" + String::num_uint64(index);
}

TypedArray<Dictionary> SaltsPlayback::list_devices() {
    ++enumeration_;
    last_error_ = playback_error(playback_list_devices(devices_.data(), devices_.size(), &device_count_));
    TypedArray<Dictionary> result;
    for (size_t index = 0; index < device_count_; ++index) {
        Dictionary device;
        device["id"] = device_id(index);
        device["name"] = String::utf8(devices_[index].name);
        device["is_default"] = devices_[index].is_default != 0;
        result.append(device);
    }
    return result;
}

Error SaltsPlayback::open_device(const String &id, int64_t sample_rate, int64_t channels,
                                 int64_t format, int64_t buffer_ms) {
    if (playback_) {
        return ERR_BUSY;
    }
    if (id.is_empty() || (sample_rate != 8000 && sample_rate != 16000 &&
                          sample_rate != 24000 && sample_rate != 48000) ||
        (channels != 1 && channels != 2) || format < FORMAT_S16 || format > FORMAT_F32 ||
        buffer_ms < SALTS_PLAYBACK_MIN_BUFFER_MS || buffer_ms > SALTS_PLAYBACK_MAX_BUFFER_MS) {
        return ERR_INVALID_PARAMETER;
    }
    for (size_t index = 0; index < device_count_; ++index) {
        if (device_id(index) != id) {
            continue;
        }
        const playback_config_t config{static_cast<uint32_t>(sample_rate), static_cast<uint32_t>(channels),
                                       static_cast<playback_format_t>(format), static_cast<uint32_t>(buffer_ms)};
        const Error error = playback_error(playback_create(&devices_[index], &config, &playback_));
        if (error == OK) {
            frame_bytes_ = channels * (format == FORMAT_S16 ? 2 : 4);
        }
        return error;
    }
    return ERR_DOES_NOT_EXIST;
}

Error SaltsPlayback::start() {
    return playback_ ? playback_error(playback_start(playback_)) : ERR_UNCONFIGURED;
}

Error SaltsPlayback::pause() {
    return playback_ ? playback_error(playback_pause(playback_)) : ERR_UNCONFIGURED;
}

Error SaltsPlayback::resume() {
    return playback_ ? playback_error(playback_resume(playback_)) : ERR_UNCONFIGURED;
}

Error SaltsPlayback::stop() {
    if (!playback_) {
        return ERR_UNCONFIGURED;
    }
    const Error error = playback_error(playback_stop(playback_));
    return error == OK ? playback_error(playback_clear(playback_)) : error;
}

void SaltsPlayback::close() {
    playback_destroy(playback_);
    playback_ = nullptr;
    frame_bytes_ = 0;
}

SaltsPlayback::State SaltsPlayback::get_state() const {
    return playback_ ? static_cast<State>(playback_get_state(playback_)) : STATE_CLOSED;
}

int64_t SaltsPlayback::write_pcm(const PackedByteArray &bytes, int64_t byte_offset) {
    if (!playback_) {
        last_error_ = ERR_UNCONFIGURED;
        return 0;
    }
    if (byte_offset < 0 || byte_offset > bytes.size() || byte_offset % frame_bytes_ != 0) {
        last_error_ = ERR_INVALID_PARAMETER;
        return 0;
    }
    if (byte_offset == bytes.size()) {
        last_error_ = OK;
        return 0;
    }
    size_t written;
    last_error_ = playback_error(playback_write(playback_, bytes.ptr() + byte_offset,
                                                static_cast<size_t>(bytes.size() - byte_offset), &written));
    return static_cast<int64_t>(written);
}

int64_t SaltsPlayback::get_available_bytes() const {
    return static_cast<int64_t>(playback_get_available(playback_));
}

int64_t SaltsPlayback::get_buffered_bytes() const {
    return static_cast<int64_t>(playback_get_buffered(playback_));
}

} // namespace salts_camera
