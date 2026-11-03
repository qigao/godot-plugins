# Salts Camera — acceptance and CI gates

**Status:** Design only; no CI has been run and no device has been exercised. [Upstream Capture issue #197](https://github.com/qigao/salts-core/issues/197).

## Source of truth

- Pin an exact SaltsCore SDK release and `godot-cpp` engine-compatible revision. Never treat compilation against `master` as proof of published SDK availability.
- `capture/release-blockers.json` in salts-core currently has `release_ready=false`. The miniaudio PulseAudio initialization leak, iOS missing-six-symbol compatibility qualification, physical device/permission testing and redistributable dependency closure are **separate** release gates. Plugin build/CI does not override any of them.
- Disallow production duplicate camera drivers, fake cameras or fallback to a different explicit `mode_id`.

## CI matrix (after implementation)

| Target | Toolchain | Automated evidence |
| --- | --- | --- |
| Linux x64 native | runner clang + ccache | build/link, TinyTest converter/mailbox, Godot headless GDExtension load/test |
| Windows x64 native/cross | llvm-mingw + cache | build/link, native TinyTest; real Godot headless runtime when a compatible runner is available |
| macOS ARM64 | runner clang + ccache | build/link, native TinyTest and headless GDExtension smoke |
| Android ARM64 | Android NDK clang | native build, export package manifest/dependency closure, permission bridge compilation; not hardware runtime |
| iOS ARM64 | Apple clang | native build, correct Godot export/link form, privacy metadata checks; not device runtime |
| Additional x64/ARM64 RIDs | supported LLVM | only add after exact produced artifacts and load tests |

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

Only release when: compiled and loadable artifacts exist for each advertised RID; exact-head CI and ABI/source tests pass; required native-device checks have evidence; SaltsCore release blockers are resolved for shipped RIDs; packaged runtime dependencies and license closure are verified. No forced godot-cpp or libyuv runtime installed by the game from a source checkout.
