# Salts Camera

Salts Camera exposes native camera preview, owned raw frames, and PCM audio playback to Godot 4.7.2. It links the static `Salts::Media` target from the installed SaltsBase v0.1.3 SDK. [Design](../../docs/SALTS-CAMERA-DESIGN.md) · [Acceptance](../../docs/SALTS-CAMERA-ACCEPTANCE.md)

## Build

Use CMake 3.27+, Ninja, Python 3, Godot 4.7.2, and `godot-cpp` at `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5`. Extract the SaltsBase v0.1.3 SDK and vcpkg-cache v0.1.3 dependency archive for your target triplet into separate directories. Verify the archives using the published checksums. [Godot](https://github.com/godotengine/godot-builds/releases/tag/4.7.2-stable) · [Bindings](https://github.com/godotengine/godot-cpp/tree/507ed9d840c01a3c5b2a39af8bb4000bfac30bf5) · [SDK](https://github.com/qigao/salts-base/releases/tag/v0.1.3) · [Dependencies](https://github.com/qigao/vcpkg-cache/releases/tag/v0.1.3)

| Target | SDK profile | Dependency prefix inside the archive | Toolchain |
| --- | --- | --- | --- |
| Linux x64 | `linux-clang-x64` | `vcpkg_installed/x64-linux` | Clang, PipeWire and X11 development libraries |
| Linux ARM64 | `linux-clang-arm64` | `vcpkg_installed/arm64-linux` | Clang, PipeWire and X11 development libraries |
| Windows x64 | `windows-llvm-mingw-x64` | `vcpkg_installed/qigao-x64-windows-llvm-mingw` | LLVM-MinGW `20261020` UCRT |
| Windows ARM64 | SaltsBase v0.1.3 Core, Media, and TinyTest from source | `vcpkg_installed/qigao-arm64-windows-llvm-mingw` | LLVM-MinGW `20261020` UCRT |
| macOS ARM64 | `macos-clang-arm64` | `vcpkg_installed/arm64-osx` | Clang, Xcode 16.4 at `/Applications/Xcode_16.4.app` |
| Android ARM64 | `android-arm64` | `vcpkg_installed/arm64-android` | NDK r27d `27.3.13750724`, `arm64-v8a`, API 26 |
| Android x64 | `android-x64` | `vcpkg_installed/x64-android` | NDK r27d `27.3.13750724`, `x86_64`, API 26 |

The Windows ARM64 job builds the three SaltsBase components required by the plugin from the checksum-pinned v0.1.3 source archive, using [the SDK build](../../cmake/salts-base-media/CMakeLists.txt). It installs their CMake package before building and testing the add-on.

From the repository root on Linux, configure the released SDK and dependency prefixes together.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DGODOT_CPP_DIR=/path/to/godot-cpp \
  '-DCMAKE_PREFIX_PATH=/path/to/salts-base;/path/to/vcpkg_installed/x64-linux' \
  -DBUILD_TESTING=ON -DGODOT_EXECUTABLE=/path/to/Godot_v4.7.2-stable_linux.x86_64
cmake --build build
ctest --test-dir build --output-on-failure
```

Use the matching compiler paths and dependency prefix on Windows and macOS. On macOS, set `DEVELOPER_DIR` to `/Applications/Xcode_16.4.app/Contents/Developer`. For a build without the test projects, omit `BUILD_TESTING` and `GODOT_EXECUTABLE`. The extension and its runtime libraries are staged under `build/addons/salts_camera/`. Copy that directory to `addons/salts_camera/` in your Godot project and open it with Godot 4.7.2. The generated descriptor lists the platform built by that configure.

On Linux, cross-build the Android add-on with the NDK at the path recorded in the published SaltsBase SDK. The example targets ARM64. For x64, use the matching SDK and dependency prefixes, set `ANDROID_ABI=x86_64`, and pass `x86_64` after the NDK path to the bundle checker.

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

The Android check inspects the staged `.gdextension`, target shared objects and their runtime dependencies. For your Android export preset, enable the matching architecture, use a minimum SDK of 26 or higher, and enable **Camera** under **Permissions**.

Android CI publishes signed `salts-camera-debug-arm64` and `salts-camera-debug-x86_64` test APKs for camera and mode selection, live preview, and an audio output test. The sample uses the Godot 4.7.2 debug template and sets API 26 through Godot's prebuilt manifest export hook. To export it locally with Android build-tools 35.0.0, JDK 17, and `readelf`, run the following command after extracting `templates/android_debug.apk` from the official 4.7.2 export-template archive.

```sh
bash tests/android/export_apk.sh build-android /path/to/godot /path/to/android_debug.apk "$ANDROID_HOME" "$JAVA_HOME"
```

The output is `build-android/salts-camera-android-arm64.apk`. Append `x86_64` to the command to export the x64 build. The export check verifies 16 KiB ZIP alignment and at least 16 KiB ELF load alignment for every packaged native library.

## Run the sample

On desktop, build with `BUILD_TESTING=ON` and open the staged scene with `godot --path build res://camera.tscn`. On Android, install the signed APK. Select **Scan cameras**, choose a camera and its mode, then select **Start preview**. Scan and select an audio output separately, then select **Play test tone**. Both controls can run in the same scene. [Sample scene](../../tests/android/export_project/camera.tscn)

## Open and preview a camera

Call the add-on API from the Godot main thread. On macOS, include `NSCameraUsageDescription` in the host application's `Info.plist`. If `permission_status()` returns `PERMISSION_REQUESTABLE`, call `request_permission()` and wait for `PERMISSION_GRANTED` before opening a camera. On Linux and Windows the permission method reports `PERMISSION_UNKNOWN`. The camera open result reflects availability from the native backend.

On Android, connect to `MainLoop.on_request_permissions_result` before calling `request_permission()`. Proceed after the result grants `android.permission.CAMERA`, then recheck `permission_status()`. Stop the feed when the app pauses and recheck authorization before restarting it.

Populate the camera selector with `manager.list_devices()`. Pass the chosen ID to `open_camera()`, populate the mode selector from its result, then pass the chosen mode dictionary to `start_preview()`. The node below expects a `TextureRect` child named `Preview`.

```gdscript
extends Node

var manager := SaltsCameraManager.new()
var device: SaltsCameraDevice
var feed: SaltsCameraFeed

func open_camera(id: String) -> Array[Dictionary]:
    if device != null:
        $Preview.texture = null
        device.close()
        feed = null
    device = manager.open_device(id)
    if device == null:
        push_error(error_string(manager.get_last_error()))
        return []
    return device.list_modes(false)

func start_preview(mode: Dictionary) -> void:
    if feed != null:
        $Preview.texture = null
        feed.close()
    feed = device.create_feed(mode)
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

## Select an output and play PCM

Create a `SaltsPlayback` on the Godot main thread. `list_devices()` returns `id`, `name`, and `is_default` for each output exposed by SaltsBase. Pass the selected ID to `open_device(id, sample_rate, channels, format, buffer_ms)` on the same object. Refreshing the list invalidates previous IDs. The v0.1.3 Android backend exposes the system-routed output. The desktop selector lists the devices reported by the native backend.

`open_device()` defaults to 48 kHz stereo F32 and a 100 ms PCM buffer. It accepts 8, 16, 24, and 48 kHz, one or two channels, and interleaved S16, S32, or F32 PCM. The buffer can hold 10 to 2000 ms of audio.

Queue frame-aligned bytes with `write_pcm(bytes, byte_offset)` before or after `start()`. Advance the byte offset by the returned count and retry the unwritten suffix on a later frame. A zero-byte result with `get_last_error() == OK` means a full buffer or completed input. `get_buffered_bytes()` and `get_available_bytes()` report queue occupancy and free space. Use `pause()` and `resume()` to keep queued samples, `stop()` to discard them, or `close()` to release the device before switching outputs. Poll `get_state()` for device errors. [Streaming test-tone example](../../tests/android/export_project/playback.gd)
