#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 3 && $# != 4 ]]; then
    echo "Usage: bash $0 CAMERA_BUILD_DIR GDMP_BUILD_DIR NEW_PROJECT_DIR [ANDROID_CAMERA_BUILD_DIR]" >&2
    exit 2
fi

demo=$(cd -- "$(dirname -- "$0")" && pwd)
camera_addon="$1/addons/salts_camera"
gdmp_addon="$2/addons/GDMP"
project=$3

test -f "$camera_addon/salts_camera.gdextension"
test -f "$gdmp_addon/GDMP.gdextension"
mkdir -- "$project"
mkdir -- "$project/addons"
cp -R "$demo"/{audio,text,vision} "$project/"
cp "$demo"/{Global.gd,Global.gd.uid,Main.gd,Main.gd.uid,Main.tscn,default_bus_layout.tres,export_presets.cfg,icon.svg,icon.svg.import,project.godot} "$project/"
cp -R "$demo/../../tests/android/export_project/addons/salts_camera_apk" "$camera_addon" "$gdmp_addon" "$project/addons/"

if [[ $# == 4 ]]; then
    android_addon="$4/addons/salts_camera"
    staged_addon="$project/addons/salts_camera"
    android_descriptor="$android_addon/salts_camera.gdextension"
    library=$(sed -n '/^\[libraries\]/,/^\[dependencies\]/{ /^android\.arm64 =/p; }' "$android_descriptor")
    dependencies=$(sed -n '/^\[dependencies\]/,$ { /^android\.arm64 =/p; }' "$android_descriptor")
    test -n "$library"
    test -n "$dependencies"
    test -f "$gdmp_addon/libs/arm64/libGDMP.android.so"
    cp -R "$android_addon/bin/android" "$staged_addon/bin/"
    awk -v library="$library" -v dependencies="$dependencies" '
        { print }
        /^\[libraries\]$/ { print library }
        /^\[dependencies\]$/ { print dependencies }
    ' "$staged_addon/salts_camera.gdextension" > "$staged_addon/merged.gdextension"
    mv -- "$staged_addon/merged.gdextension" "$staged_addon/salts_camera.gdextension"
fi
