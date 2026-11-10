#pragma once

#include "salts_camera_feed.hpp"

namespace salts_camera {

class SaltsCameraPreviewFixture : public SaltsCameraFeed {
    GDCLASS(SaltsCameraPreviewFixture, SaltsCameraFeed)

public:
    bool _activate_feed() override;
    void present(const godot::PackedByteArray &bytes, int64_t timestamp_us);

protected:
    static void _bind_methods();
};

} // namespace salts_camera
