#!/usr/bin/env python3
"""Build the in-tree GDMP GDExtension with pinned native dependencies."""

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from zipfile import ZipFile

SOURCE = Path(__file__).resolve().parent
ADDON = SOURCE.parents[1] / "addons/GDMP"
CODELOAD = "https://codeload.github.com"
ARCHIVES = {
    "mediapipe": (
        f"{CODELOAD}/google-ai-edge/mediapipe/tar.gz/80ae8afbd03465b0d6d9f9e874f8cacf093d23e9",
        "09e6bae5999c89de7bf457ee7b36aaa1fd469bcd9d37ed994c6f90212516266e",
        "mediapipe-0.10.26.tar.gz",
    ),
    "godot-cpp": (
        f"{CODELOAD}/godotengine/godot-cpp/tar.gz/e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77",
        "d4c03daf46c8bef4544614182b0762c3e32789151c7f8f44e8eb2053d5cddeba",
        "godot-cpp.tar.gz",
    ),
    "ocv": (
        f"{CODELOAD}/qigao/ocv/tar.gz/35d834d6bf049424becfbe536d367f9279fa78bb",
        "570f5f791b093f68bd3e7305767b6d36e19ea161ff2c8041b034fecdfd7f722f",
        "ocv-gdmp-35d834d6bf04.tar.gz",
    ),
}
ZLIB = (
    "https://zlib.net/fossils/zlib-1.2.13.tar.gz",
    "b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30",
    "zlib-1.2.13.tar.gz",
)

DESKTOP = {
    "Linux": ("libGDMP.so", "libGDMP.linux.so"),
    "Darwin": ("libGDMP.dylib", "libGDMP.macos.dylib"),
    "Windows": ("GDMP.dll", "GDMP.windows.dll"),
}


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def fetch(downloads, asset):
    url, sha, name = asset
    archive = downloads / name
    if not archive.is_file():
        partial = archive.with_suffix(".part")
        subprocess.run(["curl", "-fsSL", "--retry", "3", url, "-o", partial], check=True)
        if digest(partial) != sha:
            raise ValueError(f"SHA-256 mismatch for {url}")
        partial.replace(archive)
    if digest(archive) != sha:
        raise ValueError(f"SHA-256 mismatch for {archive}")
    return archive


def prepare(out, android):
    dependencies = out / "source-gdmp"
    mediapipe = dependencies / "mediapipe"
    godot_cpp = dependencies / "godot-cpp"
    downloads = out / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    patches = sorted((SOURCE / "patch/mediapipe").glob("*.diff"))
    profile = SOURCE / "godot-cpp-profile.json"
    stamp = dependencies / ".prepared"
    key = json.dumps([str(SOURCE), android, *(digest(path) for path in [Path(__file__), profile, *patches])])
    if stamp.is_file() and stamp.read_text(encoding="utf-8") == key:
        return mediapipe

    if dependencies.exists():
        shutil.rmtree(dependencies)
    for name, asset in ARCHIVES.items():
        archive = fetch(downloads, asset)
        destination = dependencies / name
        destination.mkdir(parents=True)
        subprocess.run(["tar", "-xzf", archive, "--strip-components=1", "-C", destination], check=True)
    fetch(downloads, ZLIB)

    # Apply these patches to the pinned MediaPipe release outside its Git tree.
    patch_env = os.environ | {"GIT_CEILING_DIRECTORIES": str(dependencies)}
    for patch in patches:
        subprocess.run(
            ["git", "apply", patch], cwd=mediapipe, env=patch_env, check=True,
        )

    sys.path.insert(0, str(godot_cpp))
    from binding_generator import generate_bindings
    from build_profile import generate_trimmed_api

    api = generate_trimmed_api(godot_cpp / "gdextension/extension_api.json", profile)
    api_path = godot_cpp / "gdmp-extension-api.json"
    api_path.write_text(json.dumps(api), encoding="utf-8")
    generate_bindings(str(api_path), True, output_dir=str(godot_cpp))

    workspace = mediapipe / "WORKSPACE"
    contents = workspace.read_text(encoding="utf-8").replace(
        'path = "../",', f"path = {json.dumps(SOURCE.as_posix())},", 1
    )
    if android:
        contents += (
            '\nandroid_ndk_repository(name = "androidndk", api_level = 26)\n'
            'bind(name = "android/crosstool", actual = "@androidndk//:toolchain")\n'
        )
    workspace.write_text(contents, encoding="utf-8")
    stamp.write_text(key, encoding="utf-8")
    return mediapipe


def target_options(target, arch, host):
    if target == "android":
        cpu, target_platform = {
            "arm64": ("arm64-v8a", "android_arm64_platform"),
            "x86_64": ("x86_64", "android_x86_64_platform"),
        }[arch]
        return [
            "--config=android",
            f"--cpu={cpu}",
            "--incompatible_enable_cc_toolchain_resolution",
            f"--platforms=//mediapipe:{target_platform}",
            "--extra_toolchains=@androidndk//:all",
        ]
    if target == "ios":
        return ["--config=ios", "--apple_generate_dsym=false"]
    if target == "web":
        return [
            "--define=ocv_wasm=1",
            "--incompatible_enable_cc_toolchain_resolution",
            "--crosstool_top=@emsdk//emscripten_toolchain:everything",
            "--host_crosstool_top=@bazel_tools//tools/cpp:toolchain",
            "--copt=-D_LARGEFILE64_SOURCE",
            "--copt=-fexceptions",
            "--copt=-pthread",
            "--copt=-sDISABLE_EXCEPTION_CATCHING=1",
            "--copt=-sSIDE_MODULE=1",
            "--copt=-sSUPPORT_LONGJMP=wasm",
            "--linkopt=-pthread",
            "--linkopt=-sSIDE_MODULE=1",
            "--linkopt=-sSUPPORT_LONGJMP=wasm",
            "--linkopt=-sWASM_BIGINT",
            "--linkopt=--oformat=wasm",
        ]
    if host == "Darwin":
        flags = [f"--cpu=darwin_{arch}"]
        if arch == "x86_64":
            flags.append("--define=xnn_enable_avxvnniint8=false")
        return flags
    if host == "Windows":
        return ["--conlyopt=/std:c11", "--conlyopt=/experimental:c11atomics"]
    if host == "Linux" and arch == "x86_64":
        # Ubuntu's Clang 18 crashes compiling XNNPACK's AVX-512 FP16 kernel.
        return ["--define=xnn_enable_avx512fp16=false"]
    return []


def stage_addon(out, mediapipe, target, arch, host):
    output = mediapipe / "bazel-bin/external/GDMP/GDMP" / target
    addon = out / "addons/GDMP"
    shutil.copytree(ADDON, addon, dirs_exist_ok=True)
    for license_path, name in [
        (SOURCE / "LICENSE", "LICENSE"),
        (mediapipe / "LICENSE", "LICENSE.MediaPipe"),
        (mediapipe.parent / "godot-cpp/LICENSE.md", "LICENSE.godot-cpp"),
        (mediapipe / "bazel-mediapipe/external/qigao_opencv_source/LICENSE", "LICENSE.OpenCV"),
    ]:
        shutil.copyfile(license_path, addon / name)
    libraries = addon / "libs"
    libraries.mkdir(exist_ok=True)
    if target == "ios":
        with ZipFile(output / "GDMP.xcframework.zip") as bundle:
            bundle.extractall(libraries)
        return
    if target == "web":
        shutil.copyfile(output / "GDMP.wasm", libraries / "GDMP.web.wasm")
        return
    folder = libraries / arch
    folder.mkdir(exist_ok=True)
    if target == "android":
        src, dst = "libGDMP.so", "libGDMP.android.so"
    else:
        src, dst = DESKTOP[host]
    shutil.copyfile(output / src, folder / dst)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=["desktop", "android", "ios", "web"])
    parser.add_argument("--arch", choices=["arm64", "x86_64"])
    parser.add_argument("--out", type=Path, default=Path("build-mediapipe"))
    parser.add_argument("--bazel", default="bazel", help="path to Bazel 6.5.0")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    host = platform.system()
    host_arch = {"aarch64": "arm64", "amd64": "x86_64"}.get(
        platform.machine().lower(), platform.machine().lower()
    )
    arch = args.arch or host_arch
    if args.target == "desktop" and host != "Darwin" and arch != host_arch:
        parser.error("Desktop builds on Linux and Windows use the host architecture.")

    out = args.out.resolve()
    mediapipe = prepare(out, args.target == "android")
    bazel = shutil.which(args.bazel)
    if not bazel:
        parser.error(f"Bazel executable {args.bazel} is unavailable.")
    flags = [
        "--define=MEDIAPIPE_DISABLE_GPU=1",
        "--repo_env=HERMETIC_PYTHON_VERSION=3.12",
        "--action_env=PYTHONHASHSEED=0",
    ]
    if host != "Windows":
        flags += ["--copt=-fPIC", "--cxxopt=-std=c++17", "--host_cxxopt=-std=c++17"]
    build_env = os.environ.copy()
    if args.target == "android":
        build_env["ANDROID_NDK_HOME"] = str(Path(build_env["ANDROID_NDK_HOME"]).resolve())
    subprocess.run(
        [
            Path(bazel).resolve(), f"--output_user_root={out / 'bazel'}", "build", "-c", "opt",
            f"--jobs={args.jobs}", f"--disk_cache={out / 'disk-cache'}",
            f"--repository_cache={out / 'repository-cache'}", f"--distdir={out / 'downloads'}",
            *flags, *target_options(args.target, arch, host), f"@GDMP//GDMP/{args.target}:GDMP",
        ],
        cwd=mediapipe,
        env=build_env,
        check=True,
    )
    stage_addon(out, mediapipe, args.target, arch, host)


if __name__ == "__main__":
    main()
