# Godot Plugins

Godot integrations built on top of qigao's native C11 libraries.

## Salts Camera — design proposal

[Salts Camera plugin design](docs/SALTS-CAMERA-DESIGN.md) | [Acceptance gates](docs/SALTS-CAMERA-ACCEPTANCE.md) | [Add-on contract](addons/salts_camera/README.md)

The proposed Godot 4 GDExtension provides:
- `SaltsCameraManager` and `SaltsCameraDevice` to enumerate and configure real cameras through the **installed** `Salts::Capture` SDK.
- Opt-in `SaltsCameraFeed` for Godot `CameraServer` / `CameraTexture` preview.
- An opt-in bounded CPU/raw-frame path for processing, without duplicating camera backends.

**Status:** Design-only. No compiled add-on, binaries, hardware qualification, or release is available yet. Source-side Frame V2/GPU work is tracked in [salts-core #197](https://github.com/qigao/salts-core/issues/197). The MVP uses the existing legacy frame callback without waiting for that work. Also preserve upstream release blockers.

Not in scope: another Camera2/AVFoundation/V4L2/Media Foundation implementation, a new media engine, a default fake camera, Web export, or automatic zero-copy claims.
