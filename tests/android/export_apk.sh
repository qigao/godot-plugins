#!/usr/bin/env bash
set -euo pipefail

build=$(realpath "$1")
godot=$(realpath "$2")
template=$(realpath "$3")
android_sdk=$(realpath "$4")
export JAVA_HOME
JAVA_HOME=$(realpath "$5")
export PATH="$JAVA_HOME/bin:$PATH"

tests=$(cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d "$build/android-export.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
project="$work/project"
apk="$build/salts-camera-android-arm64.apk"

mkdir -p "$project"
cp -a "$tests/export_project/." "$project/"
cp -a "$build/addons" "$project/"

export XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data" XDG_CACHE_HOME="$work/cache"
install -D "$template" "$XDG_DATA_HOME/godot/export_templates/4.5.2.stable/android_debug.apk"
mkdir -p "$XDG_CONFIG_HOME/godot"
keytool -genkeypair -keystore "$work/debug.keystore" -storetype PKCS12 \
  -alias androiddebugkey -storepass android -keypass android -keyalg RSA -keysize 2048 \
  -validity 3650 -dname 'CN=Salts Camera Debug'

python3 - "$android_sdk" "$work/debug.keystore" <<'PY'
import json
import os
from pathlib import Path
import sys

settings = {
    "export/android/java_sdk_path": os.environ["JAVA_HOME"],
    "export/android/android_sdk_path": sys.argv[1],
    "export/android/debug_keystore": sys.argv[2],
    "export/android/debug_keystore_user": "androiddebugkey",
    "export/android/debug_keystore_pass": "android",
}
path = Path(os.environ["XDG_CONFIG_HOME"]) / "godot/editor_settings-4.5.tres"
path.write_text('[gd_resource type="EditorSettings" format=3]\n\n[resource]\n' +
                ''.join(f'{key} = {json.dumps(value)}\n' for key, value in settings.items()),
                encoding="utf-8")
PY

"$godot" --headless --path "$project" --export-debug "Android Camera" "$apk"
python3 "$tests/check_apk.py" "$apk" "$build" "$android_sdk/build-tools/35.0.0"
