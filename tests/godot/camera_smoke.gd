extends SceneTree


func _initialize() -> void:
	call_deferred("run")


func require(condition: bool, message: String) -> bool:
	if not condition:
		printerr(message)
		quit(1)
	return condition


func run() -> void:
	for type_name in ["SaltsCameraManager", "SaltsCameraDevice", "SaltsCameraFeed", "SaltsCameraFrame"]:
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
	print("Salts Camera loaded and unavailable-device paths passed. Enumerated cameras: ", devices.size())
	quit(0)
