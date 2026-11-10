#pragma once

#include <capture.h>
#include <godot_cpp/classes/global_constants.hpp>

namespace salts_camera {

inline godot::Error capture_error(int result) {
    switch (result) {
        case SALTS_CAPTURE_OK: return godot::OK;
        case SALTS_CAPTURE_ERR_NOMEM: return godot::ERR_OUT_OF_MEMORY;
        case SALTS_CAPTURE_ERR_FORMAT: return godot::ERR_INVALID_PARAMETER;
        case SALTS_CAPTURE_ERR_BUSY: return godot::ERR_BUSY;
        case SALTS_CAPTURE_ERR_UNSUPPORTED: return godot::ERR_UNAVAILABLE;
        default: return godot::ERR_CANT_ACQUIRE_RESOURCE;
    }
}

} // namespace salts_camera
