#pragma once

#include <array>
#include <playback.h>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace salts_camera {

class SaltsPlayback : public godot::RefCounted {
    GDCLASS(SaltsPlayback, godot::RefCounted)

public:
    enum Format {
        FORMAT_S16 = SALTS_PLAYBACK_FORMAT_S16,
        FORMAT_S32 = SALTS_PLAYBACK_FORMAT_S32,
        FORMAT_F32 = SALTS_PLAYBACK_FORMAT_F32,
    };

    enum State {
        STATE_CLOSED = -1,
        STATE_STOPPED = SALTS_PLAYBACK_STATE_STOPPED,
        STATE_STARTING = SALTS_PLAYBACK_STATE_STARTING,
        STATE_PLAYING = SALTS_PLAYBACK_STATE_PLAYING,
        STATE_PAUSED = SALTS_PLAYBACK_STATE_PAUSED,
        STATE_STOPPING = SALTS_PLAYBACK_STATE_STOPPING,
        STATE_ERROR = SALTS_PLAYBACK_STATE_ERROR,
    };

    ~SaltsPlayback() override;

    godot::TypedArray<godot::Dictionary> list_devices();
    godot::Error open_device(const godot::String &id, int64_t sample_rate = 48000,
                             int64_t channels = 2, int64_t format = FORMAT_F32,
                             int64_t buffer_ms = 100);
    godot::Error start();
    godot::Error pause();
    godot::Error resume();
    godot::Error stop();
    void close();
    State get_state() const;

    int64_t write_pcm(const godot::PackedByteArray &bytes, int64_t byte_offset = 0);
    int64_t get_available_bytes() const;
    int64_t get_buffered_bytes() const;
    godot::Error get_last_error() const { return last_error_; }

protected:
    static void _bind_methods();

private:
    godot::String device_id(size_t index) const;

    std::array<playback_device_t, SALTS_PLAYBACK_MAX_DEVICES> devices_{};
    size_t device_count_ = 0;
    uint64_t enumeration_ = 0;
    playback_t *playback_ = nullptr;
    int64_t frame_bytes_ = 0;
    godot::Error last_error_ = godot::OK;
};

} // namespace salts_camera

VARIANT_ENUM_CAST(salts_camera::SaltsPlayback::Format)
VARIANT_ENUM_CAST(salts_camera::SaltsPlayback::State)
