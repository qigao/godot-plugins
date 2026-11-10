#!/usr/bin/env bash
set -euo pipefail

tests=$(cd -- "$(dirname -- "$0")" && pwd)
out=$(cd -- "$1" && pwd)
godot=$(cd -- "$(dirname -- "$2")" && pwd)/$(basename -- "$2")
project=$(mktemp -d "$out/demo.XXXXXX")
trap 'rm -rf -- "$project"' EXIT
export XDG_DATA_HOME="$project/userdata"
model_dir="$XDG_DATA_HOME/godot/app_userdata/GDMP Demo/GDMP/hand_landmarker/hand_landmarker/float16/latest"
mkdir -p "$model_dir"
cp "$out/test-mediapipe/test-data/hand_landmarker.task" "$model_dir/"
bash "$tests/../../demos/gdmp/stage.sh" "$3/test-preview" "$out" "$project/project"
cp "$out/test-mediapipe/test-data/right_hands.jpg" "$tests/demo_smoke.gd" "$project/project/"

run_godot() {
    "$godot" --headless --path "$project/project" "$@" 2>&1 | tee "$project/godot.log"
    if rg -q '^(SCRIPT )?ERROR:' "$project/godot.log"; then
        return 1
    fi
}

run_godot --editor --import --frame-delay 1000
run_godot --script res://demo_smoke.gd
