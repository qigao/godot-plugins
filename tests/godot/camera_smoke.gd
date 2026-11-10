extends SceneTree


func _initialize() -> void:
	call_deferred("run")


func require(condition: bool, message: String) -> bool:
	if not condition:
		printerr(message)
		quit(1)
	return condition


func check_playback() -> bool:
	var playback := SaltsPlayback.new()
	if not require(playback.get_state() == SaltsPlayback.STATE_CLOSED and
			playback.get_available_bytes() == 0 and playback.get_buffered_bytes() == 0,
			"Closed playback exposed an active PCM buffer"):
		return false
	if not require(playback.open_device("") == ERR_INVALID_PARAMETER and
			playback.open_device("missing-output") == ERR_DOES_NOT_EXIST and
			playback.open_device("missing-output", 44100) == ERR_INVALID_PARAMETER,
			"Playback accepted an invalid device selection or format"):
		return false
	if not require(playback.start() == ERR_UNCONFIGURED and playback.stop() == ERR_UNCONFIGURED and
			playback.pause() == ERR_UNCONFIGURED and playback.resume() == ERR_UNCONFIGURED,
			"Unopened playback accepted stream control"):
		return false
	if not require(playback.write_pcm(PackedByteArray([0, 0])) == 0 and
			playback.get_last_error() == ERR_UNCONFIGURED, "Unopened playback accepted PCM"):
		return false

	var devices := playback.list_devices()
	if not require(playback.get_last_error() in [OK, ERR_CANT_ACQUIRE_RESOURCE],
			"Unexpected audio output enumeration result"):
		return false
	if devices.is_empty():
		return true
	var previous_id: String = devices[0]["id"]
	var other := SaltsPlayback.new()
	other.list_devices()
	if not require(other.open_device(previous_id) == ERR_DOES_NOT_EXIST,
			"Playback accepted a selection from another owner"):
		return false
	playback.list_devices()
	return require(playback.open_device(previous_id) == ERR_DOES_NOT_EXIST,
			"Playback accepted a selection from an old enumeration")


func check_sample_scene() -> bool:
	var scene := load("res://camera.tscn") as PackedScene
	if not require(scene != null, "The media sample scene could not be loaded"):
		return false
	var sample := scene.instantiate()
	root.add_child(sample)
	var camera_scan: Button = sample.get_node("CameraSelection/Scan")
	var output_scan: Button = sample.get_node("Playback/OutputSelection/Scan")
	var result := require(not camera_scan.disabled and not output_scan.disabled,
			"The media sample could not initialize both device selectors")
	sample.free()
	return result


func run() -> void:
	for type_name in ["SaltsCameraManager", "SaltsCameraDevice", "SaltsCameraFeed", "SaltsCameraFrame", "SaltsPlayback"]:
		if not require(ClassDB.class_exists(type_name), "Missing extension class " + type_name):
			return
	if not require(not ClassDB.class_exists("SaltsCameraPreviewFixture"), "Test input shipped in the production extension"):
		return
	CameraServer.monitoring_feeds = true
	var manager: SaltsCameraManager = SaltsCameraManager.new()
	var devices: Array[Dictionary] = manager.list_devices()
	if not require(manager.get_last_error() in [OK, ERR_UNAUTHORIZED, ERR_CANT_ACQUIRE_RESOURCE], "Unexpected enumeration result"):
		return
	if not require(manager.open_device("") == null and manager.get_last_error() == ERR_INVALID_PARAMETER, "Empty ID opened a default device"):
		return
	if not require(manager.open_device("salts-camera-missing-device") == null and manager.get_last_error() in [ERR_DOES_NOT_EXIST, ERR_UNAUTHORIZED, ERR_CANT_ACQUIRE_RESOURCE], "Unknown camera ID resolved"):
		return
	var device: SaltsCameraDevice = SaltsCameraDevice.new()
	if not require(device.list_modes().is_empty() and device.get_last_error() == ERR_UNCONFIGURED, "Unopened device returned modes"):
		return
	if not require(device.create_feed({}) == null and device.get_last_error() == ERR_UNCONFIGURED, "Unopened device created a feed"):
		return
	var feed: SaltsCameraFeed = SaltsCameraFeed.new()
	var camera_feed: CameraFeed = feed
	camera_feed.set_active(true)
	if not require(not camera_feed.is_active() and feed.get_last_error() == ERR_UNCONFIGURED, "CameraFeed activated without an exact native mode"):
		return
	if not require(feed.register_preview() == ERR_UNCONFIGURED and feed.get_latest_raw_frame() == null, "Unconfigured feed exposed a preview or snapshot"):
		return
	if not require(feed.get_statistics().captured == 0, "Inactive feed received frames"):
		return
	feed.close()
	device.close()
	if not check_playback() or not check_sample_scene():
		return
	print("Salts Camera loaded and unavailable-device paths passed. Enumerated cameras: ", devices.size())
	print("SaltsPlayback device identity and closed-state checks passed")
	quit(0)
