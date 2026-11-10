extends VBoxContainer

const CAMERA_MANAGER := &"SaltsCameraManager"
const CAMERA_PERMISSION := "android.permission.CAMERA"
const FORMATS := ["I420", "NV12", "RGB24", "BGRA", "MJPEG"]

var manager: Object
var device: Object
var feed: Object
var waiting_for_permission := false

@onready var status: Label = $Status
@onready var preview: TextureRect = $Preview
@onready var cameras: OptionButton = $CameraSelection/Cameras
@onready var modes: OptionButton = $CameraSelection/Modes
@onready var scan: Button = $CameraSelection/Scan
@onready var start: Button = $CameraActions/Start
@onready var stop: Button = $CameraActions/Stop


func _ready() -> void:
	if not ClassDB.class_exists(CAMERA_MANAGER):
		status.text = "Salts Camera extension did not load"
		return
	manager = ClassDB.instantiate(CAMERA_MANAGER)
	CameraServer.monitoring_feeds = true
	get_tree().on_request_permissions_result.connect(_permission_result)
	scan.pressed.connect(_scan_cameras)
	cameras.item_selected.connect(_select_camera)
	modes.item_selected.connect(_select_mode)
	start.pressed.connect(_start_preview)
	stop.pressed.connect(_stop_preview)
	scan.disabled = false


func _scan_cameras() -> void:
	_release_camera()
	cameras.clear()
	cameras.disabled = true
	var requestable := ClassDB.class_get_integer_constant(CAMERA_MANAGER, &"PERMISSION_REQUESTABLE")
	if manager.permission_status() != requestable:
		_list_cameras()
		return
	waiting_for_permission = OS.get_name() == "Android"
	scan.disabled = waiting_for_permission
	status.text = "Grant camera access to list devices"
	var result: Error = manager.request_permission()
	if result != OK:
		waiting_for_permission = false
		scan.disabled = false
		_camera_failed("Camera permission request failed", result)
	elif waiting_for_permission and OS.get_granted_permissions().has(CAMERA_PERMISSION):
		_permission_result(CAMERA_PERMISSION, true)


func _permission_result(permission: String, granted: bool) -> void:
	if permission != CAMERA_PERMISSION or not waiting_for_permission:
		return
	waiting_for_permission = false
	scan.disabled = false
	if granted:
		_list_cameras()
	else:
		_camera_failed("Camera permission denied", ERR_UNAUTHORIZED)


func _list_cameras() -> void:
	var devices: Array[Dictionary] = manager.list_devices()
	if devices.is_empty():
		_camera_failed("No camera available", manager.get_last_error())
		return
	for entry in devices:
		cameras.add_item("%s · %s" % [entry["name"], entry["id"]])
		cameras.set_item_metadata(cameras.item_count - 1, entry["id"])
	cameras.select(-1)
	cameras.text = "Camera"
	cameras.disabled = false
	status.text = "Choose a camera"


func _select_camera(index: int) -> void:
	_release_camera()
	device = manager.open_device(cameras.get_item_metadata(index))
	if device == null:
		_camera_failed("Could not open the selected camera", manager.get_last_error())
		return
	var available_modes: Array[Dictionary] = device.list_modes(false)
	if available_modes.is_empty():
		_camera_failed("Camera has no available mode", device.get_last_error())
		return
	for mode in available_modes:
		var fps := float(mode["framerate_numerator"]) / float(mode["framerate_denominator"])
		modes.add_item("%d × %d · %.2f fps · %s" % [
			mode["width"], mode["height"], fps, FORMATS[mode["format"]],
		])
		modes.set_item_metadata(modes.item_count - 1, mode)
	modes.select(-1)
	modes.text = "Mode"
	modes.disabled = false
	status.text = "Choose a camera mode"


func _select_mode(_index: int) -> void:
	_stop_preview()
	status.text = "Ready to start the selected mode"


func _start_preview() -> void:
	feed = device.create_feed(modes.get_selected_metadata())
	if feed == null:
		_camera_failed("Could not create a feed", device.get_last_error())
		return
	var result: Error = feed.register_preview()
	if result != OK:
		_camera_failed("Could not register the preview", result)
		return
	var texture := CameraTexture.new()
	texture.camera_feed_id = feed.get_id()
	texture.camera_is_active = true
	if not texture.camera_is_active:
		_camera_failed("Could not start capture", feed.get_last_error())
		return
	preview.texture = texture
	start.disabled = true
	stop.disabled = false
	status.text = "Live preview · " + modes.get_item_text(modes.selected)


func _camera_failed(message: String, error: Error) -> void:
	_release_camera()
	cameras.select(-1)
	cameras.text = "Camera"
	status.text = message if error == OK else "%s (%s)" % [message, error_string(error)]


func _stop_preview() -> void:
	preview.texture = null
	if feed != null:
		feed.close()
		feed = null
	start.disabled = modes.selected < 0
	stop.disabled = true
	status.text = "Camera preview stopped"


func _release_camera() -> void:
	modes.clear()
	modes.disabled = true
	_stop_preview()
	if device != null:
		device.close()
		device = null


func _process(_delta: float) -> void:
	if feed != null and not feed.is_active():
		_camera_failed("Camera preview ended", feed.get_last_error())


func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_PAUSED and device != null:
		_release_camera()
		cameras.select(-1)
		cameras.text = "Camera"
		status.text = "Camera released · choose a device to reopen"


func _exit_tree() -> void:
	if device != null:
		device.close()
