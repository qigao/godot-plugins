#!/usr/bin/env bash
set -euo pipefail

out=$(cd -- "$1" && pwd)
godot=$(cd -- "$(dirname -- "$2")" && pwd)/$(basename -- "$2")
tests=$(cd -- "$(dirname -- "$0")" && pwd)
project="$out/test-mediapipe"
mkdir -p "$project/addons" "$project/test-data" "$project/.godot"
cp -a "$out/addons/GDMP" "$project/addons/"
cp "$tests/project.godot" "$tests/landmarker_smoke.gd" "$tests/cpu_tasks_smoke.gd" "$project/"

checksum=(sha256sum -c -)
if [[ $OSTYPE == darwin* ]]; then
    checksum=(shasum -a 256 -c -)
fi

fetch() {
    local url=$1 sha=$2 name=${1##*/}
    if [[ ! -f "$project/test-data/$name" ]]; then
        curl -fsSL --retry 3 "$url" -o "$project/test-data/$name.part"
        mv -- "$project/test-data/$name.part" "$project/test-data/$name"
    fi
    printf '%s  %s\n' "$sha" "$project/test-data/$name" | "${checksum[@]}"
}

fetch https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task \
    64184e229b263107bc2b804c6625db1341ff2bb731874b0bcc2fe6544e0bc9ff
fetch https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task \
    fbc2a30080c3c557093b5ddfc334698132eb341044ccee322ccf8bcf3607cde1
fetch https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task \
    59929e1d1ee95287735ddd833b19cf4ac46d29bc7afddbbf6753c459690d574a
fetch https://storage.googleapis.com/mediapipe-assets/portrait.jpg \
    a6f11efaa834706db23f275b6115058fa87fc7f14362681e6abe14e82749de3e
fetch https://storage.googleapis.com/mediapipe-assets/right_hands.jpg \
    4b5134daa4cb60465535239535f9f74c2842aba3aa5fd30bf04ef5678f93d87f
fetch https://storage.googleapis.com/mediapipe-assets/pose.jpg \
    c8a830ed683c0276d713dd5aeda28f415f10cd6291972084a40d0d8b934ed62b

fetch https://storage.googleapis.com/mediapipe-models/face_detector/blaze_face_short_range/float16/1/blaze_face_short_range.tflite \
    b4578f35940bf5a1a655214a1cce5cab13eba73c1297cd78e1a04c2380b0152f
fetch https://storage.googleapis.com/mediapipe-models/text_classifier/bert_classifier/float32/1/bert_classifier.tflite \
    9b45012ab143d88d61e10ea501d6c8763f7202b86fa987711519d89bfa2a88b1
fetch https://storage.googleapis.com/mediapipe-models/language_detector/language_detector/float32/1/language_detector.tflite \
    7db4f23dfe1ad8966b050b419a865da451143fd43eb6b606a256aadeeb1e5417
fetch https://storage.googleapis.com/mediapipe-models/audio_classifier/yamnet/float32/1/yamnet.tflite \
    4d8b4a53282dc83ef04e3e7dbc4fbc98082e34e44ed798e16c3a0cdd4c584faf

# The smoke test reads raw images and model files directly at runtime.
printf '%s\n' 'res://addons/GDMP/GDMP.gdextension' > "$project/.godot/extension_list.cfg"
"$godot" --headless --path "$project" --script res://landmarker_smoke.gd
"$godot" --headless --path "$project" --script res://cpu_tasks_smoke.gd
