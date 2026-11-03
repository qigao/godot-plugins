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

The plugin will use **Salts::Media** as the sole native camera provider. For Android, Java is permitted only for runtime permission/export lifecycle; no Java Camera2 implementation will be added. For Apple targets, Objective-C++ is permitted only for permission/export integration; no second AVFoundation capture pipeline will be added.

## MVP constraints

Current Capture callback format is inferred from the exact selected mode; no per-frame V2 structure or GPU-native handle exists yet. The first plugin is CPU-based and uses a bounded latest-frame mailbox. Its convert/upload route can involve copies and must not be advertised as GPU zero-copy. Desktop comes first; mobile export and real-device testing require their own acceptance gates.

No release artifact or working GDScript API exists at this design stage.
