# Salts MediaPipe

A Godot 4.5 GDExtension for CPU face, hand, and pose landmarking. The native image and task bindings are adapted from GDMP v0.6 and built against MediaPipe 0.10.26. [GDMP v0.6](https://github.com/j20001970/GDMP/tree/v0.6)

## Build and install

Build on Linux x86-64 with a C++17 compiler, Python 3, curl, patch, and standard GNU tools. The build script pins Bazel 6.5.0, the MediaPipe source revision used by GDMP v0.6, and the Godot 4.5 bindings used by SaltsCamera.

```sh
bash scripts/build-mediapipe.sh build-mediapipe linux-x64
```

For Android ARM64, set `ANDROID_HOME` to an SDK containing NDK r27d (`27.3.13750724`). The build targets API 26 and uses 16 KiB ELF alignment.

```sh
bash scripts/build-mediapipe.sh build-mediapipe android-arm64
```

Copy `build-mediapipe/addons/salts_mediapipe` into the Godot project's `addons` directory. Build both profiles into the same directory to stage Linux and Android libraries for exporting from a Linux editor.

## Run a model

Supply the `.task` model as bytes. Include `*.task` in the Godot export preset's filters to load bundled models with `FileAccess`. [Face model](https://ai.google.dev/edge/mediapipe/solutions/vision/face_landmarker) · [Hand model](https://ai.google.dev/edge/mediapipe/solutions/vision/hand_landmarker) · [Pose model](https://ai.google.dev/edge/mediapipe/solutions/vision/pose_landmarker)

```gdscript
var landmarker := SaltsMediaPipeLandmarker.new()

func detect_face(image: Image) -> Array[PackedVector3Array]:
    var model := FileAccess.get_file_as_bytes("res://models/face_landmarker.task")
    var error := landmarker.open(model, SaltsMediaPipeLandmarker.FACE)
    if error != OK:
        push_error(error_string(error))
        return []
    var subjects := landmarker.detect(image)
    error = landmarker.get_last_error()
    landmarker.close()
    if error != OK:
        push_error(error_string(error))
    return subjects
```

`open(model, task, mode, max_subjects)` defaults to `IMAGE` mode and one subject. Use `FACE`, `HAND`, or `POSE` with the corresponding model. `detect()` accepts an RGB8 or RGBA8 Godot `Image` and returns one `PackedVector3Array` per detected subject in MediaPipe's normalized landmark coordinates. An empty result with `get_last_error() == OK` means the model detected no subjects. Keep a session open to process multiple images. Serialize calls on the thread that owns the session.

Use `VIDEO` mode for timestamped frames. Pass strictly increasing millisecond timestamps to `detect(image, timestamp_ms)` and open a new session when switching streams. `SaltsCameraFrame.to_image()` supplies an RGBA8 image from a retained CPU snapshot. The snapshot's timestamp is in microseconds. Convert it to milliseconds and submit each new frame once.

## Verify

```sh
bash tests/mediapipe/run.sh build-mediapipe /path/to/Godot_v4.5.2-stable_linux.x86_64
```

The smoke test runs the three models on fixed images and exercises the RGBA8 video path. Test assets are fetched by SHA-256. The Android CI job checks the staged ARM64 library's entry point, system dependencies, and ELF alignment.
