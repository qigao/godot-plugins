# GDMP source import

The native sources and Godot editor add-on were imported from [GDMP v0.6](https://github.com/j20001970/GDMP/tree/c8795dab5b3a0e8b25b71a8c32bad6365ef475e5) under the MIT license. The build compiles the checked-in `GDMP/` sources and stages `addons/GDMP/`.

`build.py` pins MediaPipe 0.10.26 and Godot 4.5 bindings. The MediaPipe OpenCV target statically links OpenCV 4.11 from [the qigao/ocv source profile](https://github.com/qigao/ocv/tree/dcd5d534fd3cf16556fce562e42c9c5b8efb5dfe) or the [OCV v0.1.0 SDK](https://github.com/qigao/ocv/releases/tag/v0.1.0). CPU is the default inference delegate. Android and iOS retain the upstream mobile GPU build configuration.

The imported `README.md` records the upstream project and authors. Use the [add-on instructions](../../addons/GDMP/README.md) to build and install this copy.
