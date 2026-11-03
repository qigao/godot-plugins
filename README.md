# Godot Plugins

Godot integrations for the qigao native libraries. This repository is being initialized with a **design-first** camera plugin; no compiled plugin is published yet.

## Planned plugin: Salts Camera

A Godot 4 GDExtension adapter for the existing [Salts Core Capture](https://github.com/qigao/salts-core/tree/master/capture) C11 library. The plugin will expose optional `CameraServer` / `CameraFeed` integration and bounded raw-frame access **without** recreating the platform camera backends.

- Upstream API evolution: [salts-core #197](https://github.com/qigao/salts-core/issues/197).
- Initial MVP consumes the current `Salts::Capture` callback; Frame V2 and GPU-native handles are optional later milestones.
- Target platforms: Windows, Linux, macOS, Android, iOS (per-platform compilation and runtime validation pending).
- No release, device qualification, CI success, or zero-copy guarantee is implied by this README.

The implementation and packaging layout are proposed for review before adding binaries, mobile packaging or native code.
