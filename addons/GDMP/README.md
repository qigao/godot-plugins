# GDMP for Godot

GDMP is a native Godot 4.5 add-on for MediaPipe graphs and tasks. It accepts Godot `Image` objects, including images decoded from a video and snapshots from SaltsCamera.

## Build

### Build against a MediaPipe SDK

The CMake build consumes the native MediaPipe Tasks 1.2.2 SDK from [qigao/mediapipe](https://github.com/qigao/mediapipe) on Linux, Android, macOS, and Windows, each with an x64 and ARM64 archive. Extract the matching GPU-enabled SDK and point CMake at its `lib/cmake/MediaPipe` directory. The SDK includes its OpenCV libraries. Install the EGL and OpenGL ES development libraries on Linux.

```sh
cmake -S src/gdmp -B build-gdmp-sdk -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DMediaPipe_DIR=/path/to/mediapipe-sdk/lib/cmake/MediaPipe -DGODOT_CPP_DIR=/path/to/godot-cpp
cmake --build build-gdmp-sdk
```

On macOS, add `-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0` and set `CMAKE_OSX_ARCHITECTURES` to `arm64` or `x86_64` for the selected archive. On Android, add `-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" -DANDROID_PLATFORM=android-26` and set `ANDROID_ABI` to `arm64-v8a` or `x86_64`. Use NDK 27.3.13750724.

On Windows, use the LLVM-MinGW toolchain for the selected architecture. The build stages the SDK's private ANGLE DLLs beside `GDMP.windows.dll`. The extension uses Vulkan for inference and reports an error when no compatible hardware device is available. Face Stylizer is unavailable in the 1.2.2 SDK because MediaPipe removed that task API.

The build stages the add-on under `build-gdmp-sdk/addons/GDMP`. Run the [GDMP with MediaPipe SDK workflow](../../.github/workflows/gdmp-sdk.yml) with a published SDK tag and the SHA-256 of its `SHA256SUMS` file. It builds GDMP for all eight targets and exercises the inference demo on Linux x64 and ARM64.

### Build from MediaPipe source

Install Bazel 6.5.0, Python 3.11 or newer, curl, Git, tar, and the platform's C++ toolchain. Build Android with NDK 27.3.13750724 by setting `ANDROID_NDK_HOME` to the NDK directory.

```sh
python3 src/gdmp/build.py desktop
python3 src/gdmp/build.py android --arch arm64
```

Pass `--opencv sdk` to statically link the checksum-pinned [OCV v0.1.0 SDK](https://github.com/qigao/ocv/releases/tag/v0.1.0) on Linux or Android x64 and ARM64. Linux SDK packages target the Ubuntu 24.04 toolchain. Android SDK packages use NDK 27.3.13750724 and API 26. CI uses SDK mode on Linux x64 and Android ARM64.

The source profile is the default. macOS uses it to preserve deployment compatibility because the v0.1.0 macOS archives target macOS 15.

Copy `build-mediapipe/addons/GDMP` to the Godot project's `addons/` directory, open the project in Godot, and enable **GDMP** under **Project Settings → Plugins**. Build several targets into the same output directory to assemble a multi-platform add-on.

| Target | Architectures | Inference backend |
| --- | --- | --- |
| Linux | x86_64, arm64 | EGL and OpenGL ES 3.1+ |
| macOS | arm64, x86_64 | Metal inference |
| Android | arm64, x86_64 | EGL and OpenGL ES 3.1+ |
| Windows | x86_64, arm64 | ANGLE / Vulkan (CMake SDK build) |
| iOS | arm64 and simulators | Metal inference |
| Web | wasm32 | GPU inference backend required |

[Native source and dependency pins](../../src/gdmp/UPSTREAM.md)

GDMP requires the GPU delegate for MediaPipe Tasks. An explicit GPU request fails if MediaPipe cannot select a GPU inference calculator. Some upstream pipelines still use separate CPU stages, including gesture classification and optional face blendshape inference. The Godot demo uses the GPU delegate for its tasks and displays results through the Compatibility renderer.

Linux CI uses surfaceless EGL to exercise the OpenGL ES code path. A software Mesa renderer can run that check, so physical GPU compatibility requires device testing.

## Process a camera frame

Choose the device and start capture through SaltsCamera. Pass a CPU snapshot into GDMP and render the original preview through your existing Godot texture.

```gdscript
var options := MediaPipeTaskBaseOptions.new()
options.model_asset_buffer = FileAccess.get_file_as_bytes("res://models/face_landmarker.task")

var tracker := MediaPipeFaceLandmarker.new()
if tracker.initialize(options, MediaPipeVisionTask.RUNNING_MODE_VIDEO):
    var image := MediaPipeImage.new()
    image.set_image(snapshot.to_image())
    var result := tracker.detect_video(image, timestamp_ms)
    if result != null:
        var faces := result.get_face_landmarks()
```

Use the same `MediaPipeImage.set_image()` and `detect_video()` calls with frames from a video decoder. Supply increasing timestamps in milliseconds. Godot exports need to include any `.task` models read through `FileAccess`.
