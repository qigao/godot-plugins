extends VBoxContainer

const CAMERA_PERMISSION := "android.permission.CAMERA"

var manager: Object
var device: Object
var feed: Object
var waiting_for_permission := false

@onready var status: Label = $Status
@onready var preview: TextureRect = $Preview
@onready var start: Button = $Start
@onready var stop: Button = $Stop


func _ready() -> void:
	if not ClassDB.class_exists("SaltsCameraManager"):
		status.text = "Salts Camera extension did not load"
		return
	manager = ClassDB.instantiate("SaltsCameraManager")
	CameraServer.monitoring_feeds = true
	get_tree().on_request_permissions_result.connect(_permission_result)
	start.pressed.connect(_request_camera)
	stop.pressed.connect(_stop_camera)
	start.disabled = false


func _request_camera() -> void:
	if OS.get_granted_permissions().has(CAMERA_PERMISSION):
		_open_camera()
		return
	start.disabled = true
	waiting_for_permission = true
	status.text = "Waiting for camera permission"
	var result: Error = manager.request_permission()
	if result != OK:
		waiting_for_permission = false
		_fail("Camera permission request failed", result)
	elif OS.get_granted_permissions().has(CAMERA_PERMISSION):
		waiting_for_permission = false
		_open_camera()


func _permission_result(permission: String, granted: bool) -> void:
	if permission != CAMERA_PERMISSION or not waiting_for_permission:
		return
	waiting_for_permission = false
	if granted:
		_open_camera()
	else:
		_fail("Camera permission denied", ERR_UNAUTHORIZED)


func _open_camera() -> void:
	var devices: Array = manager.list_devices()
	if devices.is_empty():
		_fail("No camera available", manager.get_last_error())
		return
	device = manager.open_device(devices[0]["id"])
	if device == null:
		_fail("Could not open the camera", manager.get_last_error())
		return
	var modes: Array = device.list_modes(false)
	if modes.is_empty():
		_fail("Camera has no available mode", device.get_last_error())
		return
	feed = device.create_feed(modes[0])
	if feed == null:
		_fail("Could not create a feed", device.get_last_error())
		return
	var result: Error = feed.register_preview()
	if result != OK:
		_fail("Could not register the preview", result)
		return
	var texture := CameraTexture.new()
	texture.camera_feed_id = feed.get_id()
	texture.camera_is_active = true
	if not texture.camera_is_active:
		_fail("Could not start capture", feed.get_last_error())
		return
	preview.texture = texture
	start.disabled = true
	stop.disabled = false
	status.text = "%s · %d × %d" % [devices[0]["name"], modes[0]["width"], modes[0]["height"]]


func _fail(message: String, error: Error) -> void:
	_stop_camera()
	status.text = message if error == OK else "%s (%s)" % [message, error_string(error)]


func _stop_camera() -> void:
	preview.texture = null
	if device != null:
		device.close()
		device = null
	feed = null
	start.disabled = manager == null
	stop.disabled = true
	status.text = "Camera stopped"


func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_PAUSED and device != null:
		_stop_camera()


func _exit_tree() -> void:
	if device != null:
		device.close()
