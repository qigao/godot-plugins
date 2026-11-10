# Salts Camera — acceptance and CI gates

Desktop CI builds the M0 extension and runs native TinyTest, a production-library Godot load test, and a separate headless `CameraTexture` fixture. The Android job builds the ARM64 add-on, exports the camera sample, and verifies its signed debug APK. Physical-device checks and release signing require separate evidence. [Frame V2 and GPU roadmap](https://github.com/qigao/salts-core/issues/197)

## Source of truth

- Pin the SaltsBase v0.1.2 SDK, the matching vcpkg-cache v0.1.2 dependency archive for its triplet, and an engine-compatible `godot-cpp` revision. Verify the SDK checksum against the release manifest. [SaltsBase release](https://github.com/qigao/salts-base/releases/tag/v0.1.2) · [vcpkg-cache release](https://github.com/qigao/vcpkg-cache/releases/tag/v0.1.2)
- Carry forward the PulseAudio initialization failure under leak checking, iOS compatibility, physical-device and permission testing, and redistributable dependency closure as independent gates for the affected platforms. These gates predate the repository move. [Media release gate record](https://github.com/qigao/salts-core/blob/0a4eb95c03427fa5dd4ca5486162e24e52f0e0d2/media/release-blockers.json)
- Disallow production duplicate camera drivers, fake cameras or fallback to a different explicit `mode_id`.

## CI matrix

| Target | SaltsBase SDK profile | vcpkg triplet | Planned automated evidence |
| --- | --- | --- | --- |
| Linux x64 | `linux-clang-x64` | `x64-linux` | Build/link with runner Clang and ccache, TinyTest converter/mailbox, Godot headless load/test |
| Windows x64 | `windows-llvm-mingw-x64` | `qigao-x64-windows-llvm-mingw` | Build/link with llvm-mingw and cache, native TinyTest, Godot headless test on a compatible runner |
| macOS ARM64 | `macos-clang-arm64` | `arm64-osx` | Build/link with runner Clang and ccache, native TinyTest, Godot headless load |
| Android ARM64 | `android-arm64` | `arm64-android` | NDK r27d/API 26 build, AArch64 dependency checks, Godot export, signed APK verification |

Add iOS ARM64 and Windows ARM64 once matching SDKs ship. The iOS gate also requires a native link, Godot export and privacy metadata checks. For Linux x64 ThreadSanitizer runs, select the matching `linux-clang-x64-tsan` SDK and an instrumented plugin build.

The Android APK gate checks API 26, the camera permission, ARM64-only native libraries, Godot's extension list, and byte equality with the built add-on. It verifies the debug signature and 16 KiB ZIP alignment. `libsalts_camera.so` also passes the 16 KiB ELF alignment check. The published v0.1.2 `libsalts.so` still has 4 KiB ELF load alignment, so the full package requires a matching aligned SaltsBase SDK before 16 KiB device qualification.

Build and link the real extension from an extracted SDK on each target toolchain. In v0.1.2 the Linux export records absolute PipeWire and X11 library paths in `INTERFACE_LINK_LIBRARIES`. The macOS and Android exports record paths under the producer's `Xcode_16.4.app` and NDK `27.3.13750724` installations. Confirm those references resolve on the consumer runner before counting that platform as qualified. If paths differ, fix the export in SaltsBase and qualify the resulting published SDK.

No Mono, MSVC, installer/consumer test projects or probe-only CI; external archives may be fetched with curl from pinned, authenticated locations. CI must not claim real camera validation on runner hosts without cameras.

## Non-device and CPU tests

- [ ] Initial extension loads into the declared Godot version from an actual built artifact, with correct `CameraFeed` subclass registration and a working `CameraTexture` route.
- [ ] List devices: zero devices valid, error != empty, explicit id missing -> error, stale `mode_id` -> error, unavailable format -> error; no fallback.
- [ ] Frame format table: I420/NV12/RGB24/BGRA/MJPEG paths; verify colors, channel order, offsets, stride, odd-size bounds, overflows and malformed JPEG rejection.
- [ ] A synthetic **adapter test input** exercises mailbox READY/READING/WRITING transitions, dropped frames, no unbounded backlog, stop and restart epochs; synthetic frames are not substituted for real production Capture backends.
- [ ] 1000+ capture-style callback/consume/stop interleavings under ASan/UBSan and TSan where supported; no retained borrowed pointer after callback, no Godot object on producer thread, no post-destroy callback.
- [ ] Native TinyTest is mandatory. Godot headless test checks class registration, missing-camera behavior and feed enable/disable where possible. Tests must run real plugin sources, not a detached mock copy.
- [ ] Failure propagation: denied permissions, concurrent open/busy, device disconnect, startup error, repeated start/stop, canceled activation and application background/resume.
- [ ] The same device cannot silently register twice under this plugin. Built-in Godot CameraServer feed coexistence is explicit and documented; do not promise cross-provider dedup without stable identity mapping.

## Human-device acceptance (separate from CI)

- [ ] Windows/Linux/macOS physical camera: 720p/1080p, expected fps, reconnect, suspend/resume, manually denied permission.
- [ ] Android physical device: user permission grant/deny, back/front switch, home/background/resume, orientation, USB camera when supported, app shutdown without dangling callbacks.
- [ ] iOS physical device: authorization prompt, rejection, foreground/background, back/front switch and actual export signing/launch.
- [ ] Measure capture timestamp to preview/display latency; show resolution, fps, color format, copies/frame, allocations/frame and drops. Baseline against original Godot CameraServer route under the same device/mode, no unsupported performance numbers.

## Release gate

For each advertised RID, release requires a compiled and loadable artifact, passing CI and ABI/source tests on the release commit, native-device evidence, completion of the applicable Media gates, and verified runtime dependencies and license notices in the package.
