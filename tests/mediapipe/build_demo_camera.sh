#!/usr/bin/env bash
set -euo pipefail

inputs=$1
godot=$2
case "$3" in
    x86_64)
        arch=x64
        sdk_sha=c9585bbb7ce002f2a8783edca809b56fac2768f7dc7eb1be98dd42b1ad171323
        deps_sha=49686968e2da4e41b44ab9fac26bc77fa2e51953ce6bf32666eaa48d2860e5f0
        ;;
    arm64)
        arch=arm64
        sdk_sha=09418af3bf9247e66e2237b940287f709f61c8327d7391e6d8d9fd3949feb0f7
        deps_sha=dccd0f5c853b6167ae75f008a94cad9a75b05c90d85d425fa4d7a9926d476862
        ;;
    *) echo "Unsupported Linux architecture: $3" >&2; exit 2 ;;
esac
mkdir -p "$inputs"

extract_verified() {
    local url=$1 sha=$2 directory=$3
    local archive="$inputs/${url##*/}"
    curl -fsSL --retry 3 "$url" -o "$archive"
    printf '%s  %s\n' "$sha" "$archive" | sha256sum -c -
    mkdir -p "$directory"
    tar -xf "$archive" -C "$directory"
}

revision=507ed9d840c01a3c5b2a39af8bb4000bfac30bf5
extract_verified https://codeload.github.com/godotengine/godot-cpp/tar.gz/$revision \
    30da4ac295997061a6af81ae531510efd4e6a86e506eb4466edc0898ef3fe048 "$inputs/bindings"
extract_verified https://github.com/qigao/salts-base/releases/download/v0.1.3/salts-base-0.1.3-linux-clang-$arch.tar \
    "$sdk_sha" "$inputs/salts-base"
extract_verified https://github.com/qigao/vcpkg-cache/releases/download/v0.1.3/installed-$arch-linux.tar.xz \
    "$deps_sha" "$inputs/dependencies"

cmake -S . -B build-demo-camera -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    "-DGODOT_CPP_DIR=$inputs/bindings/godot-cpp-$revision" "-DGODOT_EXECUTABLE=$godot" \
    "-DCMAKE_PREFIX_PATH=$inputs/salts-base;$inputs/dependencies/vcpkg_installed/$arch-linux"
cmake --build build-demo-camera --target salts_camera_preview_fixture --parallel 4
