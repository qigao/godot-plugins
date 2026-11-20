"""Merge staged Godot network add-ons and verify their declared files."""

import configparser
import json
import shutil
import sys
from pathlib import Path


PLATFORMS = {
    "linux.x86_64", "linux.arm64", "macos.arm64",
    "windows.x86_64", "windows.arm64", "android.x86_64", "android.arm64",
}
ADDONS = ("salts_http_client", "salts_mqtt_client")


def package(artifacts, output, expected=PLATFORMS):
    builds = sorted(artifacts.glob("salts-network-build-*"))
    if len(builds) != len(expected):
        raise ValueError(f"Expected {len(expected)} platform artifacts, got {len(builds)}")

    for addon in ADDONS:
        destination = output / "addons" / addon
        shutil.copytree(builds[0] / addon, destination, ignore=shutil.ignore_patterns("bin"))
        combined = configparser.ConfigParser(interpolation=None)
        for build in builds:
            descriptor = configparser.ConfigParser(interpolation=None)
            with (build / addon / f"{addon}.gdextension").open(encoding="utf-8") as file:
                descriptor.read_file(file)
            if "configuration" not in combined:
                combined["configuration"] = dict(descriptor["configuration"])
                combined.add_section("libraries")
                combined.add_section("dependencies")
            elif combined["configuration"] != descriptor["configuration"]:
                raise ValueError(f"Different extension configurations in {build}")
            (platform, library), = descriptor["libraries"].items()
            if platform in combined["libraries"] or set(descriptor["dependencies"]) != {platform}:
                raise ValueError(f"Invalid or duplicate platform in {build}")
            dependencies = descriptor["dependencies"][platform]
            for resource in [json.loads(library), *json.loads(dependencies)]:
                if not resource.startswith("res://"):
                    raise ValueError(f"Invalid resource path {resource}")
                path = Path(resource.removeprefix("res://"))
                source = build / path.relative_to("addons")
                if not source.is_file():
                    raise FileNotFoundError(source)
                staged = output / path
                staged.parent.mkdir(parents=True, exist_ok=True)
                if not staged.exists():
                    shutil.copy2(source, staged)
            combined["libraries"][platform] = library
            combined["dependencies"][platform] = dependencies
        if set(combined["libraries"]) != expected:
            raise ValueError(f"Unexpected platforms for {addon}: {set(combined['libraries'])}")
        with (destination / f"{addon}.gdextension").open("w", encoding="utf-8") as file:
            combined.write(file)


if __name__ == "__main__":
    package(Path(sys.argv[1]), Path(sys.argv[2]))
