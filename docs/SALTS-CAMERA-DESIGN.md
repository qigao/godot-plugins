# Salts Camera: Godot GDExtension design

**Status:** Proposed / not implemented. **Reference capture head:** `qigao/salts-core@c888705263dffbcb93a480e074d8e8d450635645`.
**Media static migration:** [salts-core #198](https://github.com/qigao/salts-core/pull/198). **Upstream tracking:** [salts-core #197](https://github.com/qigao/salts-core/issues/197). **Reference implementations:** [godot-native-camera](https://github.com/godot-mobile-plugins/godot-native-camera) and [godot-cameraserver-extension](https://github.com/j20001970/godot-cameraserver-extension).

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
             installed Salts::Media
                     |
 Windows MF | Linux V4L2 | Android NDK Camera2 | Apple AVFoundation
```

1. **Only** `qigao/salts-core/media` owns physical camera devices and native capture. Godot must not implement camera acquisition a second time in Java, Swift or C++.
2. Plugin is a C++ **GDExtension**, scoped to the Godot consumer; Salts Core stays pure C11 and has **no** Godot dependency. TurboMedia and FlexUI can use Salts Capture independently.
3. The current `salts_video_capture_cb` is sufficient for M0: the *selected exact mode* is retained with the live device handle, so the callback's `width/height/bytes/timestamp_us` can be interpreted using the selected format. Do **not** mislabel arbitrary buffers as RGBA.
4. Frame V2 (`struct_size`, planes/stride, colorspace/timebase/rotation) is additive work in upstream issue #197. It is **not** a prerequisite to build the first CPU-based plugin. Native GPU sharing is a separate milestone.
5. No background/thread callback may call a Godot object or mutate a Godot image/texture. All Godot API use occurs at a documented engine-safe dispatch point.
6. Never imply current CameraServer extensions are GPU zero-copy. Camera preview may involve capture-side conversion, one buffered CPU copy, format conversion and Godot texture upload.

## Proposed Godot-facing API (not yet available)

| Class | Responsibility | Proposed methods |
| --- | --- | --- |
| `SaltsCameraManager : RefCounted` | Access/permission state, list devices | `list_devices()`, `open_device(id)`, `permission_status()`, `request_permission()` |
| `SaltsCameraDevice : RefCounted` | Own one `salts_video_device_t` and native mode identity | `list_modes(standard_only=true)`, `create_feed(mode)`, `close()` |
| `SaltsCameraFeed : CameraFeed` | Own one `salts_capture_t`, register optionally with Godot CameraServer | `_activate_feed()`, `_deactivate_feed()`, `get_latest_raw_frame()`, `get_statistics()` |
| `SaltsCameraFrame : RefCounted` | Owned **snapshot** for script use (never a borrowed callback pointer) | `get_bytes()`, `get_format()`, `get_width()`, `get_height()`, `get_timestamp_us()` |

**Device/mode identity:** `mode_id` in the Salts API is valid only for the originating open device handle / connection. `list_modes()` returns dictionaries owned by that `SaltsCameraDevice`; `create_feed(mode)` checks the full mode fields and current handle. A serialized mode or device reopen must not silently reuse a stale identity. Opening a second feed from the same device requires explicit backend permission and independent ownership; do not assume concurrent opens work.

**Registration policy:** `CameraServer.monitoring_feeds = true` is set **by the application before** manager/feed creation where required (notably Godot 4.5+). Native `CameraFeed` registration is **opt-in**. A manager never starts capturing or installs feeds automatically. Maintain a local provider/device registry so the plugin does not add a second copy of its own feed; when Godot already provides a native feed, users must explicitly choose the Salts provider because stable cross-provider identity cannot be assumed.

**Version target:** Pin one stable Godot 4.5.x engine + matching `godot-cpp` revision for the first acceptance matrix; declare other Godot 4.x versions supported only after compiling and executing their own extension ABI tests. Do not assume static type casts to `CameraFeed` preserve custom format methods; test typed and untyped access, reflecting the upstream extension's observed issue.

### Proposed GDScript usage (illustrative; API is not implemented)

```gdscript
# Enable before constructing an extension/feed on Godot versions that require it.
CameraServer.monitoring_feeds = true
var manager := SaltsCameraManager.new()
var devices = manager.list_devices()
# After the platform has granted camera permission:
var device := manager.open_device(devices[0]["id"])
var modes = device.list_modes()
var feed := device.create_feed(modes[0])
CameraServer.add_feed(feed)     # Explicit opt-in, not auto-discovery.
feed.set_active(true)
# Use the feed with Godot's CameraTexture, or request an owned raw snapshot.
var latest = feed.get_latest_raw_frame()
# On shutdown: deactivate feed, remove our registered feed, close device.
```

The actual implementation must define how enumeration errors, unsupported modes, already-open cameras, permission denial and native resource failures surface as explicit errors. Empty device lists are valid on headless/no-camera hosts; no fake feed is exposed in production.

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

- The mailbox is **owned by the Godot adapter**, NOT by Salts Capture. Initial design: max three preallocated frame slots, bounded by chosen mode dimensions and maximum frame bytes. States: FREE / WRITING / READY / READING, with monotonic generation. Drop the arriving frame or retire an unread READY frame when no slot can safely be reused; never block an external capture callback to wait for Godot UI.
- Frame delivery statistics track captured, dropped, converted, displayed, and last-latency counters. No unbounded queue, timer-driven frame accumulation, per-frame `emit_signal(bytes)`, or CPU/GPU resource that outlives its owner.
- The display converter is a **consumer-specific adapter** and may use a separately pinned `libyuv` dependency; it must not refer to Salts Core's private internal symbols. I420/NV12 should remain raw for AI/codec clients unless a display is requested. MJPEG is decoded only on display path. Copy/stride assumptions require exact mode validation; odd dimensions, padded rows and overflow must be handled or explicitly rejected.
- The V1 timestamp is the existing `timestamp_us` value. Cross-platform timestamps are **not** assumed to share an epoch or clock until upstream Frame V2 defines it. Report unknown color range/rotation as UNKNOWN rather than guessing.
- Shutdown: stop producer via `salts_capture_stop()` on the serialized control plane, wait for callbacks to drain, clear adapter mailbox generations, detach the feed from `CameraServer`, then destroy capture and device. No `stop/destroy` from the capture callback. Cancel/restart invalidates old generation snapshots for display, without invalidating script-owned copies.

## Permission and mobile packaging boundary

- Android: native device access uses existing **NDK Camera2** provider; add minimal Android permission/request/lifecycle bridge and manifest packaging only, **not** Java `CameraDevice`/`ImageReader` camera logic. Current Salts Android minimum is API 26. Request `android.permission.CAMERA` from the host and support explicit denial/cancel/resume.
- iOS: platform bridge only for `AVCaptureDevice` authorization and host `NSCameraUsageDescription`; **not** a second Swift capture pipeline. Validate actual Godot export and iOS binary/linking rules before claiming support.
- Desktop: surface accurate system permission/availability result; missing physical camera is not a successful camera test.
- All platforms: follow app lifecycle; avoid multiple owners competing for the same device. Packaging must include the actual shipped `Salts::Media` runtime and required third-party license/runtime closure.

## Plugin source and packaging layout (planned; no generated binaries yet)

```text
addons/salts_camera/
    salts_camera.gdextension          # generated only when native builds exist
    bin/<platform>/<arch>/...         # actual published targets only
    README.md
src/salts_camera/
    register_types.cpp
    salts_camera_manager.{h,cpp}
    salts_camera_device.{h,cpp}
    salts_camera_feed.{h,cpp}
    salts_camera_frame.{h,cpp}
    frame_mailbox.{h,cpp}
    frame_converter.{h,cpp}
platform/android/
    permission_bridge.*              # permission/packaging only
platform/apple/
    permission_bridge.mm             # permission only
cmake/
tests/native/                         # native tests, TinyTest
tests/godot/                          # headless Godot tests
docs/
```

Runtime `.gdextension` descriptors, Gradle AARs and iOS export files must not list binaries which the build has not actually produced. Packaging and `[dependencies]` entries in a future descriptor must match verified per-platform installed SDK artifacts.

## Build/dependency constraints

- CMake must consume a **released/installed** SaltsCore package via `find_package(SaltsCore CONFIG REQUIRED)` and `target_link_libraries(salts_camera PRIVATE Salts::Media)`. Never copy/fork Capture sources or use its private ABI; any proof-of-concept against a source tree is not release evidence.
- Pin `godot-cpp` to the chosen engine ABI; avoid implicit latest dependencies. Keep implementation libraries and their license notices in the deployed artifact.
- Target Linux/macOS native Clang and Windows **llvm-mingw** (x64/ARM64 as supported by the real toolchain); ccache/cache for repeated builds, download external tool archives with curl where needed. **No MSVC, Mono installation, installer/consumer test projects, or probe-only CI/CMake targets.**
- Godot camera code is a plugin target; there is no reason for salts-core to link against Godot or for TurboMedia to be mandatory.

## Milestones

| Gate | Deliverable | Required proof |
| --- | --- | --- |
| M0 | Desktop CameraFeed + raw snapshot via old callback | Linux/Windows/macOS exact-mode integration + bounded mailbox + headless Godot test, real device tests separately |
| M1 | Android/iOS packaging and permission bridge | export/link, denial/resume/stop, real-device manual gate distinct from CI |
| M2 | Upstream Frame V2 consumer | stride/plane/timebase contract, exact format & metadata tests; legacy ABI preserved |
| M3 | Native GPU frame (optional) | per-backend lifetime, synchronize/retain/release and measured copy budget; no generic zero-copy claim |

See [acceptance requirements](SALTS-CAMERA-ACCEPTANCE.md) for conditions and explicit blockers.
