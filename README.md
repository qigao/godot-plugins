# Godot Plugins

Godot integrations built on top of qigao's native C11 libraries.

## Salts Camera — design proposal

[Salts Camera plugin design](docs/SALTS-CAMERA-DESIGN.md) | [Acceptance gates](docs/SALTS-CAMERA-ACCEPTANCE.md) | [Add-on contract](addons/salts_camera/README.md)

The proposed Godot 4 GDExtension uses `Salts::Media` from the installed SaltsBase v0.1.2 SDK. [SDK release](https://github.com/qigao/salts-base/releases/tag/v0.1.2)

- `SaltsCameraManager` and `SaltsCameraDevice` enumerate and configure real cameras.
- Opt-in `SaltsCameraFeed` for Godot `CameraServer` / `CameraTexture` preview.
- An opt-in bounded CPU/raw-frame path for processing, without duplicating camera backends.

This is a design proposal. The MVP uses the SDK's borrowed video frame callback. The Frame V2 and GPU frame roadmap is tracked in [salts-core #197](https://github.com/qigao/salts-core/issues/197).

Not in scope: another Camera2/AVFoundation/V4L2/Media Foundation implementation, a new media engine, a default fake camera, Web export, or automatic zero-copy claims.
