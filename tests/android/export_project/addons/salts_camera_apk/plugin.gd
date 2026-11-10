@tool
extends EditorPlugin

var manifest_export: EditorExportPlugin


func _enter_tree() -> void:
	manifest_export = CameraManifestExport.new()
	add_export_plugin(manifest_export)


func _exit_tree() -> void:
	remove_export_plugin(manifest_export)


class CameraManifestExport extends EditorExportPlugin:
	const XML_RESOURCE_MAP := 0x0180
	const XML_START_ELEMENT := 0x0102
	const MIN_SDK_VERSION_ID := 0x0101020c
	const TYPE_INT_DEC := 0x10

	func _get_name() -> String:
		return "SaltsCameraManifest"

	func _supports_platform(platform: EditorExportPlatform) -> bool:
		return platform.get_os_name() == "Android"

	func _update_android_prebuilt_manifest(_platform: EditorExportPlatform,
			manifest: PackedByteArray) -> PackedByteArray:
		# Android binary XML maps attribute name indices to framework resource IDs.
		var chunk := manifest.decode_u16(2)
		var resource_map := 0
		var resource_count := 0
		while chunk < manifest.size():
			var type := manifest.decode_u16(chunk)
			var header_size := manifest.decode_u16(chunk + 2)
			var chunk_size := manifest.decode_u32(chunk + 4)
			if header_size < 8 or chunk_size < header_size or chunk + chunk_size > manifest.size():
				break
			var payload := chunk + header_size
			if type == XML_RESOURCE_MAP:
				resource_map = payload
				resource_count = (chunk_size - header_size) >> 2
			elif type == XML_START_ELEMENT:
				var attribute := payload + manifest.decode_u16(payload + 8)
				var stride := manifest.decode_u16(payload + 10)
				for index in range(manifest.decode_u16(payload + 12)):
					var offset := attribute + index * stride
					var name_index := manifest.decode_u32(offset + 4)
					if name_index < resource_count and manifest.decode_u32(resource_map + name_index * 4) == MIN_SDK_VERSION_ID:
						if manifest.decode_u8(offset + 15) == TYPE_INT_DEC and manifest.decode_u32(offset + 16) == 24:
							var updated := manifest.duplicate()
							updated.encode_u32(offset + 16, 26)
							return updated
						break
			chunk += chunk_size
		push_error("Expected API 24 in the Godot 4.5.2 debug template manifest")
		return PackedByteArray()
