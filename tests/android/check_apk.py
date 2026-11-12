import argparse
from pathlib import Path
import re
import subprocess
from tempfile import TemporaryDirectory
from zipfile import ZIP_STORED, ZipFile

from check_bundle import ANDROID_TARGETS, require_16k_load_alignment


def check_launcher(aapt, apk):
    # Godot 4.7 exposes an activity alias that aapt2 badging omits.
    manifest = subprocess.check_output([aapt, "dump", "xmltree", apk, "--file", "AndroidManifest.xml"], text=True)
    components = re.findall(r"(?m)^( +)E: (activity(?:-alias)?) \([^\n]*\)\n((?:\1 +[^\n]*\n)*)", manifest)
    target = "com.godot.game.GodotApp"
    name = ":name(0x01010003)="
    launcher_fields = (
        f'{name}"{target}Launcher"',
        ":exported(0x01010010)=true",
        f':targetActivity(0x01010202)="{target}"',
        f'{name}"android.intent.action.MAIN"',
        f'{name}"android.intent.category.LAUNCHER"',
    )
    if not any(kind == "activity" and f'{name}"{target}"' in body for _, kind, body in components):
        raise SystemExit("APK does not declare the Godot activity")
    if not any(kind == "activity-alias" and all(field in body for field in launcher_fields)
               for _, kind, body in components):
        raise SystemExit("APK does not declare the exported Godot launcher alias")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("apk", type=Path)
    parser.add_argument("build", type=Path)
    parser.add_argument("build_tools", type=Path)
    parser.add_argument("arch", choices=ANDROID_TARGETS, nargs="?", default="arm64")
    args = parser.parse_args()
    abi, _, _ = ANDROID_TARGETS[args.arch]

    subprocess.run([args.build_tools / "apksigner", "verify", "--verbose", args.apk], check=True)
    subprocess.run([args.build_tools / "zipalign", "-c", "-P", "16", "4", args.apk], check=True)
    aapt = args.build_tools / "aapt2"
    badging = subprocess.check_output([aapt, "dump", "badging", args.apk], text=True)
    for record in (
        r"^package: name='org\.qigao\.saltscamera' ",
        r"^minSdkVersion:'26'$",
        r"^uses-permission: name='android\.permission\.CAMERA'$",
        rf"^native-code: '{abi}'\s*$",
    ):
        if not re.search(record, badging, re.MULTILINE):
            raise SystemExit(f"APK manifest check failed for {record}\n{badging}")
    check_launcher(aapt, args.apk)

    descriptor = "addons/salts_camera/salts_camera.gdextension"
    with ZipFile(args.apk) as apk, TemporaryDirectory() as scratch:
        startup_assets = {"assets/project.binary", "assets/camera.tscn.remap", "assets/camera.gdc", "assets/playback.gdc"}
        if not startup_assets.issubset(apk.namelist()):
            raise SystemExit("APK does not contain the camera sample's startup assets")
        if apk.read("assets/" + descriptor) != (args.build / descriptor).read_bytes():
            raise SystemExit("APK does not contain the staged GDExtension descriptor")
        extensions = apk.read("assets/.godot/extension_list.cfg").decode().splitlines()
        if "res://" + descriptor not in extensions:
            raise SystemExit("Godot did not register the packaged GDExtension for loading")
        for name in ("libsalts_camera.so", "libsalts.so"):
            staged = args.build / "addons/salts_camera/bin/android" / args.arch / name
            library = apk.getinfo(f"lib/{abi}/{name}")
            if library.compress_type != ZIP_STORED or apk.read(library) != staged.read_bytes():
                raise SystemExit(f"APK library {name} differs from the staged add-on")

        apk.getinfo(f"lib/{abi}/libgodot_android.so")
        binary = Path(scratch) / "library.so"
        for member in apk.infolist():
            if member.filename.startswith("lib/") and member.filename.endswith(".so"):
                binary.write_bytes(apk.read(member))
                headers = subprocess.check_output(["readelf", "--program-headers", "--wide", binary], text=True)
                alignment = require_16k_load_alignment(member.filename, headers)
                print(f"{member.filename}: {alignment // 1024} KiB ELF load alignment")
    print(f"Signed Android {abi} APK verified at API 26 with camera permission and 16 KiB native alignment")


if __name__ == "__main__":
    main()
