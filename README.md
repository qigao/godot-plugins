# Godot Plugins

Godot integrations built on top of qigao's native C11 libraries.

## Salts Camera

[Build and use the add-on](addons/salts_camera/README.md) · [Design](docs/SALTS-CAMERA-DESIGN.md) · [Acceptance gates](docs/SALTS-CAMERA-ACCEPTANCE.md)

The Godot 4.5.2 GDExtension uses `Salts::Media` from the installed SaltsBase v0.1.3 SDK. [SDK release](https://github.com/qigao/salts-base/releases/tag/v0.1.3)

- `SaltsCameraManager` and `SaltsCameraDevice` enumerate and configure real cameras.
- Opt-in `SaltsCameraFeed` for Godot `CameraServer` / `CameraTexture` preview.
- An opt-in bounded CPU/raw-frame path for processing, without duplicating camera backends.
- `SaltsPlayback` enumerates audio outputs and streams PCM to the selected device.

CI builds and tests the desktop extension and exports a signed Android ARM64 debug APK with camera preview and an audio output test. Physical-device checks, release packaging, and the Frame V2 and GPU frame work have separate gates. [Frame V2 and GPU roadmap](https://github.com/qigao/salts-core/issues/197)

## GDMP

An in-tree import of the GDMP Godot add-on for MediaPipe graphs and tasks. It uses static OpenCV from qigao/ocv and accepts Godot images, including SaltsCamera snapshots. [Build and API](addons/GDMP/README.md)
