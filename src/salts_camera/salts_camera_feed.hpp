#pragma once

#include "frame_mailbox.hpp"

#include <godot_cpp/classes/camera_feed.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace salts_camera {

class SaltsCameraDevice;
class SaltsCameraFrame;

class SaltsCameraFeed : public godot::CameraFeed {
    GDCLASS(SaltsCameraFeed, godot::CameraFeed)

public:
    ~SaltsCameraFeed() override;
    bool _activate_feed() override;
    void _deactivate_feed() override;
    godot::Error register_preview();
    void unregister_preview();
    godot::Ref<SaltsCameraFrame> get_latest_raw_frame();
    godot::Dictionary get_statistics() const;
    godot::Error get_last_error() const { return last_error_; }
    void close();

protected:
    static void _bind_methods();

private:
    friend class SaltsCameraDevice;
    friend class SaltsCameraPreviewFixture;

    godot::Error configure(const godot::Ref<SaltsCameraDevice> &device, const video_native_mode_t &mode);
    godot::Error open_capture();
    void stop_capture();
    void drain_frame();
    bool preview_registered() const;

    godot::Ref<SaltsCameraDevice> device_;
    video_native_mode_t mode_{};
    FrameLayout layout_{};
    FrameMailbox mailbox_;
    capture_t *capture_ = nullptr;
    godot::ObjectID scene_tree_;
    godot::Error last_error_ = godot::OK;
    godot::PackedByteArray rgba_;
    godot::Ref<godot::Image> image_;
    uint64_t converted_ = 0;
    uint64_t displayed_ = 0;
    uint64_t conversion_errors_ = 0;
    int64_t last_latency_us_ = 0;
};

} // namespace salts_camera
