# Salts Camera: Godot GDExtension design

M0 targets Godot 4.5.2 and the installed SaltsBase v0.1.3 SDK from source commit `039d033c6f4a742dc5bac3b4e07908a3c4068a4f`. [SDK release](https://github.com/qigao/salts-base/releases/tag/v0.1.3)

The Frame V2 and GPU frame roadmap is tracked in [salts-core #197](https://github.com/qigao/salts-core/issues/197). The Godot references are [godot-native-camera](https://github.com/godot-mobile-plugins/godot-native-camera) and [godot-cameraserver-extension](https://github.com/j20001970/godot-cameraserver-extension).

## Decision and boundaries

```text
Godot scene / CameraTexture / script / raw frame consumer
             |                            |
       SaltsCameraFeed              SaltsCameraFrame
             \                          /
               SaltsCameraDevice / Manager
                     |
            frame mailbox + CPU converter
                     |
        SaltsBase v0.1.3 / Salts::Media
                     |
 Windows MF | Linux V4L2 | Android NDK Camera2 | Apple AVFoundation
```

1. The static `Salts::Media` target owns native Capture and Playback and publishes `capture.h` and `playback.h`. The camera adapter takes frames from Capture and displays them through Godot's `CameraFeed` and `CameraTexture`. [Media source in SaltsBase v0.1.3](https://github.com/qigao/salts-base/tree/v0.1.3/media)
2. Godot-specific code lives in the C++ **GDExtension**. TurboMedia and FlexUI can use the SaltsBase Media API directly.
3. M0 uses `video_capture_cb` from `capture.h`. The adapter retains the exact `video_native_mode_t` selected through the live device handle and uses its format to interpret the callback's borrowed bytes, dimensions and microsecond timestamp.
4. Frame V2 (`struct_size`, planes/stride, colorspace/timebase/rotation) is additive work in upstream issue #197. It is **not** a prerequisite to build the first CPU-based plugin. Native GPU sharing is a separate milestone.
5. No background/thread callback may call a Godot object or mutate a Godot image/texture. All Godot API use occurs at a documented engine-safe dispatch point.
6. Never imply current CameraServer extensions are GPU zero-copy. Camera preview may involve capture-side conversion, one buffered CPU copy, format conversion and Godot texture upload.

## Godot-facing API

| Class | Responsibility | Methods |
| --- | --- | --- |
| `SaltsCameraManager : RefCounted` | Access/permission state, list devices | `list_devices()`, `open_device(id)`, `permission_status()`, `request_permission()` |
| `SaltsCameraDevice : RefCounted` | Own one `video_device_t` and native mode identity | `list_modes(standard_only=true)`, `create_feed(mode)`, `close()` |
| `SaltsCameraFeed : CameraFeed` | Own one `capture_t` and optionally register with Godot CameraServer | `register_preview()`, `unregister_preview()`, `set_active()`, `get_latest_raw_frame()`, `get_statistics()`, `close()` |
| `SaltsCameraFrame : RefCounted` | Owned **snapshot** for script use (never a borrowed callback pointer) | `get_bytes()`, `get_format()`, `get_width()`, `get_height()`, `get_timestamp_us()` |
| `SaltsPlayback : RefCounted` | Enumerate audio outputs and own one bounded PCM sink | `list_devices()`, `open_device()`, `write_pcm()`, `start()`, `pause()`, `resume()`, `stop()`, `get_state()`, `close()` |

**Device/mode identity:** `mode_id` in the Salts API is valid only for the originating open device handle / connection. `list_modes()` returns dictionaries owned by that `SaltsCameraDevice`; `create_feed(mode)` checks the full mode fields and current handle. A serialized mode or device reopen must not silently reuse a stale identity. Opening a second feed from the same device requires explicit backend permission and independent ownership; do not assume concurrent opens work.

The adapter enumerates with `capture_list_video_devices()` and opens the chosen device with `video_device_open()`. It calls `video_device_list_modes()` for standard frame rates or `video_device_list_modes_all()` for every native mode. For the selected mode it calls `video_device_create_capture()`, `video_capture_set_callback()` and `capture_start()` in that order.

The application opts into preview with `CameraServer.monitoring_feeds = true` and `feed.register_preview()`. The add-on owns one device handle per enumerated ID and checks the engine's assigned feed ID for collisions at registration. An activated feed without registration supports raw snapshots. Stable identity across the built-in and Salts providers requires separate device qualification.

The build pins Godot 4.5.2 and the `godot-cpp` 4.5 bindings at `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`. Other engine versions require their own ABI and load tests.

Manager, device, and feed operations expose Godot error codes. Read `get_last_error()` after an empty enumeration, a failed open or create, or an activation that leaves `is_active()` false. A successful empty device list is valid on hosts without cameras. [GDScript usage](../addons/salts_camera/README.md#open-and-preview-a-camera)

The playback adapter owns the native device descriptions from its latest enumeration. Script sees an opaque selection ID scoped to that enumeration and owner. Open passes the original description to SaltsBase. The Godot main thread serializes both control and the sole PCM producer. SaltsBase's native callback consumes the bounded buffer. The application retains the input bytes and advances by the actual accepted byte count. [Playback usage](../addons/salts_camera/README.md#select-an-output-and-play-pcm)

## Capture-to-Godot pipeline

```text
native video_cb (capture worker)
  -> validate selected format, dimensions, length, generation
  -> copy into preallocated, fixed-size latest-frame mailbox (drop on contention)
  -> return immediately; borrowed pointer is now invalid
Godot scene/render-safe drain:
  -> acquire newest mailbox slot
  -> convert only for display path (I420/NV12/RGB24/BGRA/MJPEG to RGBA8)
  -> Image::set_data / CameraFeed::set_rgb_image using documented Godot thread context
  -> release slot
Script raw access (opt-in):
  -> create an owned SaltsCameraFrame snapshot of original selected format bytes
```

- The Godot adapter owns three preallocated slots sized for the selected mode. The producer uses a try-lock to publish and replaces an unread ready frame with the latest arrival. The consumer pins its current slot while it converts or copies a snapshot. Stop discards the ready and pinned indices after the provider returns.
- Frame delivery statistics track captured, dropped, converted, displayed, and last-latency counters. No unbounded queue, timer-driven frame accumulation, per-frame `emit_signal(bytes)`, or CPU/GPU resource that outlives its owner.
- The display converter uses public APIs. Where it needs `libyuv`, use the package from the selected dependency prefix. I420/NV12 should remain raw for AI/codec clients unless a display is requested. MJPEG is decoded only on display path. Copy/stride assumptions require exact mode validation; odd dimensions, padded rows and overflow must be handled or explicitly rejected.
- The V1 timestamp is the existing `timestamp_us` value. Cross-platform timestamps are **not** assumed to share an epoch or clock until upstream Frame V2 defines it. Report unknown color range/rotation as UNKNOWN rather than guessing.
- On the serialized control plane, call `capture_stop()`, wait for callbacks to drain, clear adapter mailbox generations and detach the feed from `CameraServer`. Release the handles with `capture_destroy()` and `video_device_close()`. Cancel/restart invalidates old generation snapshots for display, without invalidating script-owned copies.

## Permission and mobile packaging boundary

- Android uses the SaltsBase **NDK Camera2** provider and Godot's permission API. The add-on builds for `arm64-v8a` at API 26. The host export declares `android.permission.CAMERA` and handles permission results, pause and resume.
- iOS: platform bridge only for `AVCaptureDevice` authorization and host `NSCameraUsageDescription`; **not** a second Swift capture pipeline. Validate actual Godot export and iOS binary/linking rules before claiming support.
- Desktop: surface accurate system permission/availability result; missing physical camera is not a successful camera test.
- Follow the app lifecycle and keep one owner per device. The extension links `Salts::Media` statically. Package the transitive `Salts::Core` shared runtime, any required redistributable shared libraries and their license notices.

## Plugin source and packaging

`src/salts_camera/` contains the extension and the native frame adapter. CMake stages the platform-specific `.gdextension`, shared extension and runtime libraries under `build/addons/salts_camera/`. Native TinyTest and headless tests live under `tests/`. The synthetic preview input is built into a separate test extension. Android CI exports and verifies a signed debug APK using the staged ARM64 add-on.

## Build/dependency constraints

Set `CMAKE_PREFIX_PATH` to the extracted SaltsBase SDK prefix and the matching vcpkg-cache v0.1.3 dependency prefix for the target triplet. The installed package supplies the public headers and imported target. The dependency prefix provides JPEG and `libyuv` for Linux, Windows and Android. [Dependency release](https://github.com/qigao/vcpkg-cache/releases/tag/v0.1.3)

```cmake
find_package(SaltsBase 0.1.3 EXACT CONFIG REQUIRED)
target_link_libraries(salts_camera PRIVATE Salts::Media)
```

- Pin `godot-cpp` to the chosen engine ABI; avoid implicit latest dependencies. Keep implementation libraries and their license notices in the deployed artifact.
- Target Linux/macOS native Clang and Windows **llvm-mingw**. Use ccache/cache for repeated builds and curl for pinned external archives. **No MSVC, Mono installation, installer/consumer test projects, or probe-only CI/CMake targets.** [SDK profiles and acceptance matrix](SALTS-CAMERA-ACCEPTANCE.md#ci-matrix)

## Milestones

| Gate | Deliverable | Required proof |
| --- | --- | --- |
| M0 | Desktop CameraFeed + raw snapshot via old callback | Linux/Windows/macOS exact-mode integration + bounded mailbox + headless Godot test, real device tests separately |
| M1 | Android/iOS packaging and permission bridge | export/link, denial/resume/stop, real-device manual gate distinct from CI |
| M2 | Upstream Frame V2 consumer | stride/plane/timebase contract, exact format & metadata tests; legacy ABI preserved |
| M3 | Native GPU frame (optional) | per-backend lifetime, synchronize/retain/release and measured copy budget; no generic zero-copy claim |

See [acceptance requirements](SALTS-CAMERA-ACCEPTANCE.md) for conditions and explicit blockers.
