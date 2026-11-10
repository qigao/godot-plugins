# Salts Camera Godot add-on (planned)

**Not yet an installable add-on.** This directory currently contains only the proposed public API contract. The native `.gdextension`, binaries and export bundles will be added only after the first tested build.

- Full architecture: [docs/SALTS-CAMERA-DESIGN.md](../../docs/SALTS-CAMERA-DESIGN.md).
- Acceptance: [docs/SALTS-CAMERA-ACCEPTANCE.md](../../docs/SALTS-CAMERA-ACCEPTANCE.md).
- Upstream Frame V2/GPU backlog: [salts-core #197](https://github.com/qigao/salts-core/issues/197).

## Intended user experience

1. Host enables CameraServer feed monitoring where required.
2. `SaltsCameraManager` handles permission state and device discovery.
3. Open a `SaltsCameraDevice`, select **one of its exact native modes**, then create a `SaltsCameraFeed`.
4. Explicitly register the feed with Godot CameraServer and activate it; `CameraTexture` consumes preview frames.
5. A script or native consumer can request an **owned** raw-frame snapshot without keeping a Capture callback pointer.
6. Deactivate, unregister, stop/join and release device resources on shutdown.

The sole native camera provider is the static `Salts::Media` target from the installed SaltsBase v0.1.2 SDK. Android Java and Apple Objective-C++ bridges are limited to permission and export integration. [SDK release](https://github.com/qigao/salts-base/releases/tag/v0.1.2)

## MVP constraints

The plugin reads the pixel format from the selected native mode and receives borrowed frame bytes through `video_capture_cb` in `capture.h`. The first plugin is CPU-based and uses a bounded latest-frame mailbox. Its convert/upload route can involve copies and must not be advertised as GPU zero-copy. Desktop comes first; mobile export and real-device testing require their own acceptance gates.

No release artifact or working GDScript API exists at this design stage.
