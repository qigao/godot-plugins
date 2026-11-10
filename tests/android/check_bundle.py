import argparse
from configparser import ConfigParser
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    parser.add_argument("ndk", type=Path)
    args = parser.parse_args()

    descriptor = args.build / "addons/salts_camera/salts_camera.gdextension"
    config = ConfigParser()
    with descriptor.open() as source:
        config.read_file(source)
    entry = json.loads(config["configuration"]["entry_symbol"])
    feature = "android.arm64"
    extension = json.loads(config["libraries"][feature])
    dependencies = json.loads(config["dependencies"][feature])
    prefix = "res://addons/salts_camera/bin/android/arm64/"
    if extension != prefix + "libsalts_camera.so" or dependencies != {prefix + "libsalts.so": ""}:
        raise SystemExit("Unexpected Android paths in the GDExtension descriptor")

    llvm = args.ndk / "toolchains/llvm/prebuilt/linux-x86_64"
    system_libs = {path.name for path in (llvm / "sysroot/usr/lib/aarch64-linux-android/26").glob("lib*.so")}
    packaged_libs = {Path(path).name for path in (extension, *dependencies)}
    for resource in (extension, *dependencies):
        library = args.build / resource.removeprefix("res://")
        elf = subprocess.check_output([
            llvm / "bin/llvm-readelf", "--file-header", "--dynamic", "--dyn-syms", "--wide", library,
        ], text=True)
        if not re.search(r"Machine:\s+AArch64", elf) or not re.search(r"Type:\s+DYN\b", elf):
            raise SystemExit(f"{library.name} must be an AArch64 shared object")
        needed = set(re.findall(r"\(NEEDED\).*?\[([^\]]+)\]", elf))
        unresolved = needed - system_libs - packaged_libs
        if unresolved:
            raise SystemExit(f"{library.name} has unstaged dependencies {sorted(unresolved)}")
        if resource == extension:
            symbol = rf"\bGLOBAL\s+DEFAULT\s+\d+\s+{re.escape(entry)}$"
            if not re.search(symbol, elf, re.MULTILINE) or "libcamera2ndk.so" not in needed:
                raise SystemExit("Android camera extension must export its entry and link the NDK camera backend")
        print(f"{library.name} verified with dependencies {sorted(needed)}")


if __name__ == "__main__":
    main()
