#include "salts_camera_feed.hpp"

#include "native_capture.hpp"
#include "salts_camera_device.hpp"
#include "salts_camera_frame.hpp"

#include <cstring>
#include <godot_cpp/classes/camera_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

namespace salts_camera {

using namespace godot;

void SaltsCameraFeed::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_preview"), &SaltsCameraFeed::register_preview);
    ClassDB::bind_method(D_METHOD("unregister_preview"), &SaltsCameraFeed::unregister_preview);
    ClassDB::bind_method(D_METHOD("get_latest_raw_frame"), &SaltsCameraFeed::get_latest_raw_frame);
    ClassDB::bind_method(D_METHOD("get_statistics"), &SaltsCameraFeed::get_statistics);
    ClassDB::bind_method(D_METHOD("get_last_error"), &SaltsCameraFeed::get_last_error);
    ClassDB::bind_method(D_METHOD("close"), &SaltsCameraFeed::close);
}

SaltsCameraFeed::~SaltsCameraFeed() {
    stop_capture();
    if (device_.is_valid()) {
        device_->feed_ = nullptr;
    }
}

Error SaltsCameraFeed::configure(const Ref<SaltsCameraDevice> &device, const video_native_mode_t &mode) {
    const auto layout = FrameLayout::from_mode(mode);
    if (!layout) {
        return ERR_UNAVAILABLE;
    }
    if (!mailbox_.prepare(*layout)) {
        return ERR_OUT_OF_MEMORY;
    }
    layout_ = *layout;
    mode_ = mode;
    device_ = device;
    const Error error = open_capture();
    if (error == OK) {
        set_name(device_->name_);
        set_transform(Transform2D());
    }
    return error;
}

Error SaltsCameraFeed::open_capture() {
    return capture_error(video_device_create_capture(device_->device_, &mode_, &capture_));
}

bool SaltsCameraFeed::_activate_feed() {
    auto *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (device_.is_null() || !device_->device_ || !tree) {
        last_error_ = ERR_UNCONFIGURED;
        return false;
    }
    if (!capture_) {
        last_error_ = open_capture();
        if (last_error_ != OK) {
            return false;
        }
    }
    last_error_ = tree->connect("process_frame", callable_mp(this, &SaltsCameraFeed::drain_frame));
    if (last_error_ != OK) {
        stop_capture();
        return false;
    }
    scene_tree_ = tree->get_instance_id();
    mailbox_.reset();
    converted_ = 0;
    displayed_ = 0;
    conversion_errors_ = 0;
    last_latency_us_ = 0;
    video_capture_set_callback(capture_, [](capture_t *, const uint8_t *bytes, size_t size,
                                           int width, int height, uint64_t timestamp, void *data) {
        static_cast<FrameMailbox *>(data)->publish(bytes, size, width, height, timestamp);
    }, &mailbox_);
    capture_on_state(capture_, [](capture_t *, capture_state_t state, void *data) {
        if (state == SALTS_CAPTURE_STATE_ERROR) {
            static_cast<FrameMailbox *>(data)->report_capture_error();
        }
    });
    if (capture_start(capture_) != 0) {
        stop_capture();
        last_error_ = ERR_CANT_ACQUIRE_RESOURCE;
        return false;
    }
    return true;
}

void SaltsCameraFeed::_deactivate_feed() {
    stop_capture();
}

void SaltsCameraFeed::stop_capture() {
    if (capture_) {
        capture_stop(capture_);
        capture_destroy(capture_);
        capture_ = nullptr;
    }
    if (auto *tree = ObjectDB::get_instance(scene_tree_)) {
        tree->disconnect("process_frame", callable_mp(this, &SaltsCameraFeed::drain_frame));
    }
    scene_tree_ = ObjectID();
    mailbox_.discard();
}

Error SaltsCameraFeed::register_preview() {
    auto *server = CameraServer::get_singleton();
    if (device_.is_null() || !device_->device_ || !server->is_monitoring_feeds()) {
        return ERR_UNCONFIGURED;
    }
    for (int index = 0; index < server->get_feed_count(); ++index) {
        const Ref<CameraFeed> feed = server->get_feed(index);
        if (feed.ptr() == this) {
            return OK;
        }
        if (feed->get_id() == get_id()) {
            return ERR_ALREADY_EXISTS;
        }
    }
    server->add_feed(Ref<CameraFeed>(this));
    return OK;
}

void SaltsCameraFeed::unregister_preview() {
    if (auto *server = CameraServer::get_singleton()) {
        server->remove_feed(Ref<CameraFeed>(this));
    }
}

bool SaltsCameraFeed::preview_registered() const {
    auto *server = CameraServer::get_singleton();
    if (!server->is_monitoring_feeds()) {
        return false;
    }
    for (int index = 0; index < server->get_feed_count(); ++index) {
        if (server->get_feed(index).ptr() == this) {
            return true;
        }
    }
    return false;
}

void SaltsCameraFeed::drain_frame() {
    if (mailbox_.capture_failed()) {
        set_active(false);
        last_error_ = ERR_CANT_ACQUIRE_RESOURCE;
        return;
    }
    const auto *frame = mailbox_.take_latest();
    if (!frame || !preview_registered()) {
        return;
    }
    const auto resize_error = static_cast<Error>(rgba_.resize(static_cast<int64_t>(layout_.rgba_bytes)));
    if (resize_error != OK) {
        last_error_ = resize_error;
        ++conversion_errors_;
        return;
    }
    if (!convert_rgba8(layout_, frame->bytes, frame->size, rgba_.ptrw())) {
        last_error_ = ERR_INVALID_DATA;
        ++conversion_errors_;
        return;
    }
    if (image_.is_null()) {
        image_ = Image::create_from_data(layout_.width, layout_.height, false, Image::FORMAT_RGBA8, rgba_);
    } else {
        image_->set_data(layout_.width, layout_.height, false, Image::FORMAT_RGBA8, rgba_);
    }
    ++converted_;
    set_rgb_image(image_);
    ++displayed_;
    last_latency_us_ = std::chrono::duration_cast<std::chrono::microseconds>(
        FrameMailbox::Clock::now() - frame->arrived).count();
    last_error_ = OK;
}

Ref<SaltsCameraFrame> SaltsCameraFeed::get_latest_raw_frame() {
    const auto *frame = mailbox_.latest();
    if (!frame) {
        return {};
    }
    Ref<SaltsCameraFrame> snapshot;
    snapshot.instantiate();
    last_error_ = static_cast<Error>(snapshot->bytes_.resize(static_cast<int64_t>(frame->size)));
    if (last_error_ != OK) {
        return {};
    }
    std::memcpy(snapshot->bytes_.ptrw(), frame->bytes, frame->size);
    snapshot->format_ = layout_.format;
    snapshot->width_ = layout_.width;
    snapshot->height_ = layout_.height;
    snapshot->timestamp_us_ = static_cast<int64_t>(frame->timestamp_us);
    return snapshot;
}

Dictionary SaltsCameraFeed::get_statistics() const {
    Dictionary stats;
    stats["captured"] = mailbox_.captured();
    stats["dropped"] = mailbox_.dropped();
    stats["converted"] = converted_;
    stats["displayed"] = displayed_;
    stats["conversion_errors"] = conversion_errors_;
    stats["last_latency_us"] = last_latency_us_;
    return stats;
}

void SaltsCameraFeed::close() {
    set_active(false);
    stop_capture();
    if (device_.is_valid()) {
        device_->feed_ = nullptr;
        device_.unref();
    }
    unregister_preview();
    mailbox_.release();
    rgba_.clear();
    image_.unref();
}

} // namespace salts_camera
