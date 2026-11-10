import argparse
from pathlib import Path
import re
import subprocess
from tempfile import TemporaryDirectory
from zipfile import ZIP_STORED, ZipFile

from check_bundle import require_16k_load_alignment


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("apk", type=Path)
    parser.add_argument("build", type=Path)
    parser.add_argument("build_tools", type=Path)
    args = parser.parse_args()

    subprocess.run([args.build_tools / "apksigner", "verify", "--verbose", args.apk], check=True)
    subprocess.run([args.build_tools / "zipalign", "-c", "-P", "16", "4", args.apk], check=True)
    badging = subprocess.check_output([args.build_tools / "aapt2", "dump", "badging", args.apk], text=True)
    for record in (
        r"^package: name='org\.qigao\.saltscamera' ",
        r"^minSdkVersion:'26'$",
        r"^uses-permission: name='android\.permission\.CAMERA'$",
        r"^launchable-activity: ",
        r"^native-code: 'arm64-v8a'\s*$",
    ):
        if not re.search(record, badging, re.MULTILINE):
            raise SystemExit(f"APK manifest check failed for {record}\n{badging}")

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
            staged = args.build / "addons/salts_camera/bin/android/arm64" / name
            library = apk.getinfo(f"lib/arm64-v8a/{name}")
            if library.compress_type != ZIP_STORED or apk.read(library) != staged.read_bytes():
                raise SystemExit(f"APK library {name} differs from the staged add-on")

        apk.getinfo("lib/arm64-v8a/libgodot_android.so")
        binary = Path(scratch) / "library.so"
        for member in apk.infolist():
            if member.filename.startswith("lib/") and member.filename.endswith(".so"):
                binary.write_bytes(apk.read(member))
                headers = subprocess.check_output(["readelf", "--program-headers", "--wide", binary], text=True)
                alignment = require_16k_load_alignment(member.filename, headers)
                print(f"{member.filename}: {alignment // 1024} KiB ELF load alignment")
    print("Signed Android ARM64 APK verified at API 26 with camera permission and 16 KiB native alignment")


if __name__ == "__main__":
    main()
