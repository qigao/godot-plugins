# Salts QR Code

Salts QR Code is a Godot 4.5.2 GDExtension backed by ZXing-C++ 3.1.1. The add-on supports Linux, Windows and Android on x64 and ARM64, plus macOS ARM64.

## Use the add-on

Copy `addons/salts_qrcode` from the combined CI artifact into your Godot project, then open the project in Godot. The extension registers the `SaltsQRCode` class.

```gdscript
var qr_image := SaltsQRCode.render_text("https://example.org/二维码")
$TextureRect.texture = ImageTexture.create_from_image(qr_image)

var decoded := SaltsQRCode.decode(qr_image)
if not decoded.is_empty():
    print(decoded["text"])
```

| Method | Result |
| --- | --- |
| `render_text(text: String, pixels_per_module: int = 8)` | A grayscale `Image` from UTF-8 text. |
| `render_bytes(bytes: PackedByteArray, pixels_per_module: int = 8)` | A grayscale `Image` from a binary payload. |
| `decode(image: Image)` | A dictionary with `text` and raw `bytes` from the first QR Code found. An empty dictionary means no code was found. |

The encoder uses medium error correction and a four-module white quiet zone. Choose a positive integer module size and use nearest-neighbor filtering when displaying the image. The render methods return `null` for empty payloads, encoder errors, or images that exceed 16384 pixels per side. Use `bytes` when the payload is binary.

## Build from source

Extract the ZXing-C++ 3.1.1 release archive and the pinned godot-cpp revision `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`. The ZXing-C++ archive SHA-256 is `c3c02c29c0b519de7bd4e25b376e606e87f0761befd1282815642a2246613d14`. [ZXing-C++ source archive](https://github.com/zxing-cpp/zxing-cpp/releases/tag/v3.1.1) · [godot-cpp source](https://github.com/godotengine/godot-cpp/tree/e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77)

```sh
cmake -S src/salts_qrcode -B build-qrcode -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DGODOT_CPP_DIR=/path/to/godot-cpp \
  -DZXING_CPP_DIR=/path/to/zxing-cpp-3.1.1
cmake --build build-qrcode
```

The build stages the add-on at `build-qrcode/addons/salts_qrcode`. Use LLVM-MinGW on Windows and Android NDK r27d with API 26 for the Android CI targets.

For desktop tests, add `-DBUILD_TESTING=ON -DGODOT_EXECUTABLE=/path/to/godot` at configure time and run `ctest --test-dir build-qrcode --output-on-failure` after building.
