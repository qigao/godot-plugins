# GDMP for Godot

GDMP is a native Godot 4.5 add-on for MediaPipe graphs and tasks. It accepts Godot `Image` objects, including images decoded from a video and snapshots from SaltsCamera.

## Build

Install Bazel 6.5.0, Python 3.11 or newer, curl, Git, tar, and the platform's C++ toolchain. Build Android with NDK 27.3.13750724 by setting `ANDROID_NDK_HOME` to the NDK directory.

```sh
python3 src/gdmp/build.py desktop
python3 src/gdmp/build.py android --arch arm64
```

Copy `build-mediapipe/addons/GDMP` to the Godot project's `addons/` directory, open the project in Godot, and enable **GDMP** under **Project Settings → Plugins**. Build several targets into the same output directory to assemble a multi-platform add-on.

| Target | Architectures | CI coverage |
| --- | --- | --- |
| Linux | x86_64, arm64 | x86_64 Godot inference |
| macOS | arm64, x86_64 | arm64 Godot inference |
| Android | arm64, x86_64 | arm64 link, dependencies, and 16 KiB ELF alignment |
| Windows | x86_64 | Upstream desktop configuration |
| iOS | arm64 and simulators | Upstream XCFramework configuration |
| Web | wasm32 | Upstream Emscripten configuration |

[Native source and dependency pins](../../src/gdmp/UPSTREAM.md)

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
