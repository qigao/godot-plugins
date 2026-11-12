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
        f"{CODELOAD}/godotengine/godot-cpp/tar.gz/507ed9d840c01a3c5b2a39af8bb4000bfac30bf5",
        "30da4ac295997061a6af81ae531510efd4e6a86e506eb4466edc0898ef3fe048",
        "godot-cpp-10.0.0-stable.tar.gz",
    ),
}
OCV_SOURCE = (
    f"{CODELOAD}/qigao/ocv/tar.gz/dcd5d534fd3cf16556fce562e42c9c5b8efb5dfe",
    "2e37345245c72f6a6e3dced9f2c442833a140b7079c97dc565ba0d9de49063c4",
    "ocv-gdmp-dcd5d534fd3c.tar.gz",
)
OCV_SDK_URL = "https://github.com/qigao/ocv/releases/download/v0.1.0"
OCV_SDK_BUILD = SOURCE / "ocv-sdk.BUILD"
OCV_SDK_SHA256 = {
    "android-arm64": "58932c6c12100990e31be556c9351bc0cf3cf6b6b9e3100a39d6307c2589db8f",
    "android-x64": "f3354d22f315fa61c25d4a2691581d6dbba6fe5b71c2dcb338a9a3c7c3e7ce6b",
    "linux-arm64": "16ced3247662a09c0f1fa10832d6dc43fb68639d37fd09b13512cb1fc41b077d",
    "linux-x64": "01fe644d9869c2ed2f79e6078f8fbaece6725d2175bbf2806a62d689d34f8237",
}
OCV_SDK_NDK = "27.3.13750724"
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


def prepare(out, android, ocv_sdk):
    dependencies = out / "source-gdmp"
    mediapipe = dependencies / "mediapipe"
    godot_cpp = dependencies / "godot-cpp"
    downloads = out / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    patches = sorted((SOURCE / "patch/mediapipe").glob("*.diff"))
    profile = SOURCE / "godot-cpp-profile.json"
    stamp = dependencies / ".prepared"
    inputs = [Path(__file__), profile, *patches]
    if ocv_sdk:
        inputs.append(OCV_SDK_BUILD)
    key = json.dumps([str(SOURCE), android, ocv_sdk, *(digest(path) for path in inputs)])
    if stamp.is_file() and stamp.read_text(encoding="utf-8") == key:
        return mediapipe

    if dependencies.exists():
        shutil.rmtree(dependencies)
    archives = ARCHIVES if ocv_sdk else ARCHIVES | {"ocv": OCV_SOURCE}
    for name, asset in archives.items():
        archive = fetch(downloads, asset)
        destination = dependencies / name
        destination.mkdir(parents=True)
        subprocess.run(["tar", "-xzf", archive, "--strip-components=1", "-C", destination], check=True)
    if ocv_sdk:
        name = f"opencv-4.11.0-static-{ocv_sdk}.tar.gz"
        archive = fetch(downloads, (f"{OCV_SDK_URL}/{name}", OCV_SDK_SHA256[ocv_sdk], name))
        sdk = dependencies / "ocv/bazel"
        sdk.mkdir(parents=True)
        subprocess.run(["tar", "-xzf", archive, "-C", sdk], check=True)
        (sdk.parent / "WORKSPACE").write_text('workspace(name = "qigao_ocv")\n', encoding="utf-8")
        shutil.copyfile(OCV_SDK_BUILD, sdk / "BUILD.bazel")
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

    interface_path = godot_cpp / "gdextension/gdextension_interface.json"
    api = generate_trimmed_api(godot_cpp / "gdextension/extension_api-4-7.json", profile)
    api_path = godot_cpp / "gdmp-extension-api.json"
    api_path.write_text(json.dumps(api), encoding="utf-8")
    generate_bindings(str(api_path), str(interface_path), True, output_dir=str(godot_cpp))

    workspace = mediapipe / "WORKSPACE"
    contents = workspace.read_text(encoding="utf-8").replace(
        'path = "../",', f"path = {json.dumps(SOURCE.as_posix())},", 1
    )
    if not ocv_sdk:
        contents += (
            '\nload("@qigao_ocv//bazel:repositories.bzl", "ocv_repositories")\n'
            'ocv_repositories()\n'
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


def stage_addon(out, mediapipe, target, arch, host, ocv_sdk):
    output = mediapipe / "bazel-bin/external/GDMP/GDMP" / target
    addon = out / "addons/GDMP"
    shutil.copytree(ADDON, addon, dirs_exist_ok=True)
    ocv_license = (
        mediapipe.parent / "ocv/bazel/LICENSE"
        if ocv_sdk
        else mediapipe / "bazel-mediapipe/external/qigao_opencv_source/LICENSE"
    )
    for license_path, name in [
        (SOURCE / "LICENSE", "LICENSE"),
        (mediapipe / "LICENSE", "LICENSE.MediaPipe"),
        (mediapipe.parent / "godot-cpp/LICENSE.md", "LICENSE.godot-cpp"),
        (ocv_license, "LICENSE.OpenCV"),
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
    parser.add_argument("--opencv", choices=["source", "sdk"], default="source",
                        help="use the pinned OpenCV source profile or the OCV v0.1.0 SDK")
    args = parser.parse_args()
    host = platform.system()
    host_arch = {"aarch64": "arm64", "amd64": "x86_64"}.get(
        platform.machine().lower(), platform.machine().lower()
    )
    arch = args.arch or host_arch
    if args.target == "desktop" and host not in {"Linux", "Darwin"}:
        parser.error("GPU builds on desktop currently require Linux EGL or macOS Metal.")
    if args.target == "web":
        parser.error("The WebAssembly build has no MediaPipe GPU inference backend.")
    if args.target == "desktop" and host == "Linux" and arch != host_arch:
        parser.error("Desktop builds on Linux use the host architecture.")

    ocv_sdk = None
    if args.opencv == "sdk":
        sdk_os = {"android": "android", "desktop": {"Linux": "linux"}.get(host)}.get(args.target)
        sdk_arch = {"arm64": "arm64", "x86_64": "x64"}.get(arch)
        ocv_sdk = f"{sdk_os}-{sdk_arch}"
        if ocv_sdk not in OCV_SDK_SHA256:
            parser.error("The OCV SDK build supports Linux and Android on x64 and ARM64.")

    build_env = os.environ.copy()
    if args.target == "android":
        ndk = Path(build_env["ANDROID_NDK_HOME"]).resolve()
        build_env["ANDROID_NDK_HOME"] = str(ndk)
        if ocv_sdk:
            properties = (ndk / "source.properties").read_text(encoding="utf-8")
            if f"Pkg.Revision = {OCV_SDK_NDK}" not in properties.splitlines():
                parser.error(f"The OCV SDK requires Android NDK {OCV_SDK_NDK}.")

    out = args.out.resolve()
    mediapipe = prepare(out, args.target == "android", ocv_sdk)
    bazel = shutil.which(args.bazel)
    if not bazel:
        parser.error(f"Bazel executable {args.bazel} is unavailable.")
    flags = [
        "--repo_env=HERMETIC_PYTHON_VERSION=3.12",
        "--action_env=PYTHONHASHSEED=0",
    ]
    if args.target == "desktop" and host == "Linux":
        flags += [
            "--copt=-DEGL_NO_X11",
            "--copt=-DMESA_EGL_NO_X11_HEADERS",
            "--per_file_copt=gl_context_egl[.]cc$@-DMEDIAPIPE_OMIT_EGL_WINDOW_BIT",
        ]
    if host != "Windows":
        flags += ["--copt=-fPIC", "--cxxopt=-std=c++17", "--host_cxxopt=-std=c++17"]
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
    stage_addon(out, mediapipe, args.target, arch, host, ocv_sdk)


if __name__ == "__main__":
    main()
