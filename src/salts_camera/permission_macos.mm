#include "salts_camera_manager.hpp"

#import <AVFoundation/AVFoundation.h>

namespace salts_camera {

SaltsCameraManager::PermissionStatus SaltsCameraManager::permission_status() const {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
        case AVAuthorizationStatusNotDetermined: return PERMISSION_REQUESTABLE;
        case AVAuthorizationStatusAuthorized: return PERMISSION_GRANTED;
        case AVAuthorizationStatusDenied: return PERMISSION_DENIED;
        case AVAuthorizationStatusRestricted: return PERMISSION_RESTRICTED;
    }
    return PERMISSION_UNKNOWN;
}

godot::Error SaltsCameraManager::request_permission() const {
    const auto status = permission_status();
    if (status == PERMISSION_GRANTED) {
        return godot::OK;
    }
    if (status != PERMISSION_REQUESTABLE) {
        return godot::ERR_UNAUTHORIZED;
    }
    if (![[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSCameraUsageDescription"]) {
        return godot::ERR_UNCONFIGURED;
    }
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL) {}];
    return godot::OK;
}

} // namespace salts_camera
