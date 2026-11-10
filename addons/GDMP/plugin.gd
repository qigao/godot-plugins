@tool
extends EditorPlugin

func _enable_plugin() -> void:
	add_autoload_singleton("MediaPipeExternalFiles", "MediaPipeExternalFiles.gd")
	add_autoload_singleton("GDMPAndroid", "GDMPAndroid.gd")

func _disable_plugin() -> void:
	remove_autoload_singleton("MediaPipeExternalFiles")
	remove_autoload_singleton("GDMPAndroid")
