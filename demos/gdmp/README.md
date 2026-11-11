# GDMP Demo

The vision, text, and audio scenes are adapted from [GDMP-demo at `b70fdf9`](https://github.com/j20001970/GDMP-demo/tree/b70fdf9b9bdcac4447fc09dcf139ef2ed720e98e/project) under the [MIT license](LICENSE).

## Run on desktop

Build [Salts Camera](../../addons/salts_camera/README.md) and [GDMP](../../addons/GDMP/README.md) for your host. Use Godot 4.5.2. Windows x64 and ARM64 use XNNPACK for CPU inference. Linux and Android require OpenGL ES 3.1+ for inference. macOS uses Metal for inference. Godot uses the Compatibility renderer for display. From the repository root, stage both add-ons and open the generated project.

```sh
bash demos/gdmp/stage.sh build build-mediapipe build-gdmp-demo
godot --editor --path build-gdmp-demo
```

Run the project and allow model downloads in the main menu. Open **Vision → Hand Landmarker**, wait for the model to load, then select **Open Camera**, a device, and a mode. The scene shows the GDMP result with landmarks drawn on the captured image. **Load Image** and **Load Video** run the same task on a local image or Ogg Theora video.

Models are cached in the project's `user://GDMP` directory. You can also prepopulate that directory using the relative model paths in the task scripts.

## Export Android ARM64

Build Salts Camera for the host and Android ARM64. Build GDMP for the host and Android ARM64 into the same GDMP output directory. Stage both camera targets so the desktop editor can import the project before exporting.

```sh
bash demos/gdmp/stage.sh build build-mediapipe build-gdmp-demo-android build-android
godot --editor --path build-gdmp-demo-android
```

In Godot 4.5.2 with the matching Android templates installed, export the **Android ARM64** debug preset. The camera export hook sets API 26 on the prebuilt template. Camera access is requested when opening a device.

The in-tree GDMP version supplies the tasks listed in the demo menu. The upstream Holistic Landmarker depends on a binding absent from this version.
