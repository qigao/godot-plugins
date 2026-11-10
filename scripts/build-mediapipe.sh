#!/usr/bin/env bash
set -euo pipefail

repo=$(cd -- "$(dirname -- "$0")/.." && pwd)
out=$(realpath -m -- "$1")
platform=$2
case "$platform" in
    linux-x64)
        library_dir=linux/x86_64
        target_flags=()
        ;;
    android-arm64)
        library_dir=android/arm64
        export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.3.13750724"
        target_flags=(
            --config=android
            --cpu=arm64-v8a
            --incompatible_enable_cc_toolchain_resolution
            --platforms=//mediapipe:android_arm64_platform
            --extra_toolchains=@androidndk//:all
            --linkopt=-Wl,-z,max-page-size=16384
            --linkopt=-Wl,-z,common-page-size=16384
        )
        ;;
    *)
        printf 'Expected linux-x64 or android-arm64, got %s\n' "$platform" >&2
        exit 2
        ;;
esac

downloads="$out/downloads"
deps="$out/source-v0.6"
mediapipe="$deps/mediapipe"
godot_cpp="$deps/godot-cpp"
sources="$repo/src/salts_mediapipe"
addon="$out/addons/salts_mediapipe"
mkdir -p "$downloads"

fetch() {
    local url=$1 sha=$2 file="$downloads/$3"
    if [[ ! -f "$file" ]]; then
        curl -fsSL --retry 3 "$url" -o "$file.part"
        printf '%s  %s\n' "$sha" "$file.part" | sha256sum -c -
        mv -- "$file.part" "$file"
    fi
    printf '%s  %s\n' "$sha" "$file" | sha256sum -c -
}

bazel="$downloads/bazel-6.5.0-linux-x86_64"
fetch https://releases.bazel.build/6.5.0/release/bazel-6.5.0-linux-x86_64 \
    a40ac69263440761199fcb8da47ad4e3f328cbe79ffbf4ecc14e5ba252857307 "$(basename -- "$bazel")"
chmod +x "$bazel"

inputs=$(sha256sum "$repo/scripts/build-mediapipe.sh" "$sources/"{godot-cpp-profile.json,opencv-native.patch} | sha256sum)
if [[ ! -f "$deps/.prepared" ]] || [[ "$(cat "$deps/.prepared")" != "$inputs" ]]; then
    fetch https://codeload.github.com/j20001970/GDMP/tar.gz/refs/tags/v0.6 \
        761fbfaf2505de9046eb61cc7a02516cecba9bf2116812c26ce83da4839056d8 GDMP-src-v0.6.tar.gz
    fetch https://codeload.github.com/google-ai-edge/mediapipe/tar.gz/80ae8afbd03465b0d6d9f9e874f8cacf093d23e9 \
        09e6bae5999c89de7bf457ee7b36aaa1fd469bcd9d37ed994c6f90212516266e mediapipe-0.10.26.tar.gz
    fetch https://codeload.github.com/godotengine/godot-cpp/tar.gz/e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77 \
        d4c03daf46c8bef4544614182b0762c3e32789151c7f8f44e8eb2053d5cddeba godot-cpp.tar.gz

    mkdir -p "$deps/gdmp" "$mediapipe" "$godot_cpp"
    tar -xzf "$downloads/GDMP-src-v0.6.tar.gz" --strip-components=1 -C "$deps/gdmp"
    tar -xzf "$downloads/mediapipe-0.10.26.tar.gz" --strip-components=1 -C "$mediapipe"
    tar -xzf "$downloads/godot-cpp.tar.gz" --strip-components=1 -C "$godot_cpp"

    patch --batch --directory="$mediapipe" -p1 < "$deps/gdmp/patch/mediapipe/01-opencv.diff"
    patch --batch --directory="$mediapipe" -p1 < "$sources/opencv-native.patch"
    python3 -B - "$godot_cpp" "$mediapipe" "$sources" <<'PY'
import json
from pathlib import Path
import sys

godot_cpp, mediapipe, sources = map(Path, sys.argv[1:])
sys.path.insert(0, str(godot_cpp))
from binding_generator import generate_bindings
from build_profile import generate_trimmed_api

profile = sources / "godot-cpp-profile.json"
trimmed = generate_trimmed_api(godot_cpp / "gdextension/extension_api.json", profile)
api = godot_cpp / "salts-extension-api.json"
api.write_text(json.dumps(trimmed), encoding="utf-8")
generate_bindings(str(api), True, output_dir=str(godot_cpp))

with (mediapipe / "WORKSPACE").open("a", encoding="utf-8") as f:
    f.write(f'''
local_repository(
    name = "salts_mediapipe",
    path = {json.dumps(str(sources))},
)
new_local_repository(
    name = "godot-cpp",
    build_file = "@salts_mediapipe//:godot-cpp.BUILD",
    path = "../godot-cpp",
)
android_ndk_repository(name = "androidndk", api_level = 26)
bind(name = "android/crosstool", actual = "@androidndk//:toolchain")
''')
PY
    printf '%s\n' "$inputs" > "$deps/.prepared"
fi

(
    cd "$mediapipe"
    "$bazel" --output_user_root="$out/bazel" build -c opt \
        --jobs="${SALTS_MEDIAPIPE_JOBS:-4}" \
        --disk_cache="$out/disk-cache" --repository_cache="$out/repository-cache" --distdir="$downloads" \
        --copt=-fPIC --cxxopt=-std=c++17 --host_cxxopt=-std=c++17 \
        --define=MEDIAPIPE_DISABLE_GPU=1 --define=OPENCV=source \
        --linkopt=-Wl,--exclude-libs,ALL --linkopt=-Wl,-z,defs \
        "${target_flags[@]}" @salts_mediapipe//:libsalts_mediapipe.so
)
install -D "$mediapipe/bazel-bin/external/salts_mediapipe/libsalts_mediapipe.so" \
    "$addon/bin/$library_dir/libsalts_mediapipe.so"
install -m 644 "$repo/addons/salts_mediapipe/"{salts_mediapipe.gdextension,LICENSE} "$addon/"
install -m 644 "$mediapipe/LICENSE" "$addon/LICENSE.MediaPipe"
