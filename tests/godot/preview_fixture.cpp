#include "preview_fixture.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace salts_camera {

void SaltsCameraPreviewFixture::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("present", "bytes", "timestamp_us"),
                               &SaltsCameraPreviewFixture::present);
}

bool SaltsCameraPreviewFixture::_activate_feed() {
    layout_ = *FrameLayout::from_mode({2, 2, 30, 1, SALTS_VIDEO_CAPTURE_FORMAT_BGRA, 1});
    return mailbox_.prepare(layout_);
}

void SaltsCameraPreviewFixture::present(const godot::PackedByteArray &bytes, int64_t timestamp_us) {
    if (is_active()) {
        mailbox_.publish(bytes.ptr(), static_cast<size_t>(bytes.size()), layout_.width, layout_.height,
                         static_cast<uint64_t>(timestamp_us));
        drain_frame();
    }
}

} // namespace salts_camera
