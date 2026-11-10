# Salts Camera

Salts Camera exposes native video capture to Godot 4.5.2 as a `CameraFeed` with optional `CameraTexture` preview and owned raw snapshots. It links the static `Salts::Media` target from the installed SaltsBase v0.1.2 SDK. [Design](../../docs/SALTS-CAMERA-DESIGN.md) · [Acceptance](../../docs/SALTS-CAMERA-ACCEPTANCE.md)

## Build

Use CMake 3.27+, Ninja, Python 3, Godot 4.5.2, and `godot-cpp` at `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`. Extract the SaltsBase v0.1.2 SDK and vcpkg-cache v0.1.2 dependency archive for your target triplet into separate directories. Verify the archives using the published checksums. [Godot](https://github.com/godotengine/godot-builds/releases/tag/4.5.2-stable) · [Bindings](https://github.com/godotengine/godot-cpp/tree/e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77) · [SDK](https://github.com/qigao/salts-base/releases/tag/v0.1.2) · [Dependencies](https://github.com/qigao/vcpkg-cache/releases/tag/v0.1.2)

| Target | SDK profile | Dependency prefix inside the archive | Toolchain |
| --- | --- | --- | --- |
| Linux x64 | `linux-clang-x64` | `vcpkg_installed/x64-linux` | Clang, PipeWire and X11 development libraries |
| Windows x64 | `windows-llvm-mingw-x64` | `vcpkg_installed/qigao-x64-windows-llvm-mingw` | LLVM-MinGW `20261020` UCRT |
| macOS ARM64 | `macos-clang-arm64` | `vcpkg_installed/arm64-osx` | Clang, Xcode 16.4 at `/Applications/Xcode_16.4.app` |
| Android ARM64 | `android-arm64` | `vcpkg_installed/arm64-android` | NDK r27d `27.3.13750724`, `arm64-v8a`, API 26 |

From the repository root on Linux, configure the released SDK and dependency prefixes together.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DGODOT_CPP_DIR=/path/to/godot-cpp \
  '-DCMAKE_PREFIX_PATH=/path/to/salts-base;/path/to/vcpkg_installed/x64-linux' \
  -DBUILD_TESTING=ON -DGODOT_EXECUTABLE=/path/to/Godot_v4.5.2-stable_linux.x86_64
cmake --build build
ctest --test-dir build --output-on-failure
```

Use the matching compiler paths and dependency prefix on Windows and macOS. On macOS, set `DEVELOPER_DIR` to `/Applications/Xcode_16.4.app/Contents/Developer`. For a build without the test projects, omit `BUILD_TESTING` and `GODOT_EXECUTABLE`. The extension and its runtime libraries are staged under `build/addons/salts_camera/`. Copy that directory to `addons/salts_camera/` in your Godot project and open it with Godot 4.5.2. The generated descriptor lists the platform built by that configure.

On Linux, cross-build the Android add-on with the NDK at the path recorded in the published SaltsBase SDK.

```sh
ndk=/usr/local/lib/android/sdk/ndk/27.3.13750724
prefix='/path/to/salts-base-android;/path/to/vcpkg_installed/arm64-android'
cmake -S . -B build-android -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static \
  -DGODOT_CPP_DIR=/path/to/godot-cpp \
  "-DCMAKE_PREFIX_PATH=$prefix" "-DCMAKE_FIND_ROOT_PATH=$prefix"
cmake --build build-android
python3 tests/android/check_bundle.py build-android "$ndk"
```

The Android check inspects the staged `.gdextension`, AArch64 shared objects and their runtime dependencies. For your Android export preset, enable ARM64, use a minimum SDK of 26 or higher, and enable **Camera** under **Permissions**.

The Android CI job publishes `salts-camera-debug-arm64`, a signed test APK for checking permission, preview, and stop on a device. The sample uses the Godot 4.5.2 debug template and sets API 26 through Godot's prebuilt manifest export hook. To export it locally with Android build-tools 35.0.0 and JDK 17, run the following command after extracting `templates/android_debug.apk` from the official 4.5.2 export-template archive.

```sh
bash tests/android/export_apk.sh build-android /path/to/godot /path/to/android_debug.apk "$ANDROID_HOME" "$JAVA_HOME"
```

The output is `build-android/salts-camera-android-arm64.apk`. The APK passes 16 KiB ZIP alignment and the extension passes 16 KiB ELF load alignment. The v0.1.2 SaltsBase runtime still uses 4 KiB ELF load alignment.

## Open and preview a camera

Call the add-on API from the Godot main thread. On macOS, include `NSCameraUsageDescription` in the host application's `Info.plist`. If `permission_status()` returns `PERMISSION_REQUESTABLE`, call `request_permission()` and wait for `PERMISSION_GRANTED` before opening a camera. On Linux and Windows the permission method reports `PERMISSION_UNKNOWN`. The camera open result reflects availability from the native backend.

On Android, connect to `MainLoop.on_request_permissions_result` before calling `request_permission()`. Proceed after the result grants `android.permission.CAMERA`, then recheck `permission_status()`. Stop the feed when the app pauses and recheck authorization before restarting it.

The following node expects a `TextureRect` child named `Preview` and a host with camera access.

```gdscript
extends Node

var device: SaltsCameraDevice
var feed: SaltsCameraFeed

func _ready() -> void:
    var manager := SaltsCameraManager.new()
    var devices := manager.list_devices()
    if devices.is_empty():
        print("Camera enumeration status ", manager.get_last_error())
        return
    device = manager.open_device(devices[0]["id"])
    if device == null:
        push_error(error_string(manager.get_last_error()))
        return
    var modes := device.list_modes()
    if modes.is_empty():
        print("Camera mode enumeration status ", device.get_last_error())
        return
    feed = device.create_feed(modes[0])
    if feed == null:
        push_error(error_string(device.get_last_error()))
        return
    CameraServer.monitoring_feeds = true
    var result := feed.register_preview()
    if result != OK:
        push_error(error_string(result))
        return
    var texture := CameraTexture.new()
    texture.camera_feed_id = feed.get_id()
    texture.camera_is_active = true
    if not texture.camera_is_active:
        push_error(error_string(feed.get_last_error()))
        return
    $Preview.texture = texture

func _exit_tree() -> void:
    if device != null:
        device.close()
```

`list_modes(false)` includes nonstandard frame rates. Select an exact returned dictionary from the current device handle. Each device ID may be open once through this add-on and each open handle can own one feed. When Godot has assigned the same ID to two unregistered feeds, `register_preview()` reports `ERR_ALREADY_EXISTS`. Create and register feeds sequentially for multiple previews.

For raw-only capture, activate the feed with `set_active(true)` before requesting snapshots. `get_latest_raw_frame()` returns a snapshot of the latest frame consumed during `SceneTree.process_frame` or `null` until one arrives. The snapshot owns its bytes, dimensions, format, and native microsecond timestamp. It remains valid after a restart or close. `get_statistics()` reports callback arrivals and drops, conversions, preview submissions, conversion errors, and time from callback arrival to preview submission. The latency counter uses a monotonic clock local to the adapter.

The V1 adapter accepts packed BGRA and RGB24, plus even-dimension I420/NV12 and MJPEG on Linux, Windows and Android. The macOS SDK's camera backend emits BGRA and the Android camera backend emits I420. Packed buffers with unexpected lengths are dropped. RGBA8 frames are limited to 64 MiB, with up to three preallocated capture slots and one preview conversion buffer.
