extends SceneTree

func _initialize() -> void:
	call_deferred("run")

func require(condition: bool, message: String) -> bool:
	if not condition:
		printerr(message)
		quit(1)
	return condition

func run() -> void:
	var menu := (load("res://Main.tscn") as PackedScene).instantiate()
	root.add_child(menu)
	var paths := [menu.tasks_vision, menu.tasks_text, menu.tasks_audio]
	menu.free()
	for group in paths:
		for scene_path in group.values():
			var scene := load(scene_path) as PackedScene
			if not require(scene != null, "Could not load " + scene_path):
				return
			var node := scene.instantiate()
			root.add_child(node)
			var valid: bool = node.get_script() != null and node.get_script().can_instantiate()
			node.free()
			if not require(valid, "Invalid scene script: " + scene_path):
				return

	var scene := load("res://vision/hand_landmarker/HandLandmarker.tscn") as PackedScene
	var demo = scene.instantiate()
	root.add_child(demo)
	demo._load_image(ProjectSettings.globalize_path("res://right_hands.jpg"))
	var label := demo.get_node("VBoxContainer/Image/Handedness") as Label
	await process_frame
	if not require(not label.text.strip_edges().is_empty(), "The hand demo did not label the sample image"):
		return

	demo.running_mode = MediaPipeVisionTask.RUNNING_MODE_VIDEO
	demo._init_task()
	var feed := SaltsCameraPreviewFixture.new()
	feed.set_active(true)
	demo.camera_feed = feed
	feed.present(PackedByteArray([0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255]), 1)
	demo._read_camera_frame()
	await process_frame
	if not require(demo.image_view.texture.get_size() == Vector2(2, 2) and label.text.is_empty(),
			"The Salts Camera snapshot did not reach the GDMP scene"):
		return
	demo._close_camera()
	if not require(not feed.is_active() and demo.camera_feed == null, "Stopping the demo left the camera active"):
		return
	demo.free()
	print("All GDMP demo scenes opened. Hand inference processed both a photo and a Salts Camera frame.")
	quit(0)
