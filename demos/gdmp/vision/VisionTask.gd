class_name VisionTask
extends Control

const CAMERA_PERMISSION := "android.permission.CAMERA"
const CAMERA_INTERVAL_MS := 66
const CAMERA_FORMATS := {
	SaltsCameraFrame.FORMAT_I420: "I420",
	SaltsCameraFrame.FORMAT_NV12: "NV12",
	SaltsCameraFrame.FORMAT_RGB24: "RGB24",
	SaltsCameraFrame.FORMAT_BGRA: "BGRA",
	SaltsCameraFrame.FORMAT_MJPEG: "MJPEG",
}

var request: HTTPRequest
var running_mode := MediaPipeVisionTask.RUNNING_MODE_IMAGE
var camera_manager := SaltsCameraManager.new()
var camera_device: SaltsCameraDevice
var camera_feed: SaltsCameraFeed
var waiting_for_permission := false
var last_camera_timestamp_us := -1
var next_camera_frame_ms := 0
var last_video_timestamp_ms := -1

@onready var external_files_disabled: Label = $VBoxContainer/ExternalFileDisabled
@onready var progress_bar: ProgressBar = $VBoxContainer/ProgressBar
@onready var image_view: TextureRect = $VBoxContainer/Image
@onready var video_player: VideoStreamPlayer = $Video
@onready var btn_back: Button = $VBoxContainer/Title/Back
@onready var btn_load_image: Button = $VBoxContainer/Buttons/LoadImage
@onready var btn_load_video: Button = $VBoxContainer/Buttons/LoadVideo
@onready var btn_open_camera: Button = $VBoxContainer/Buttons/OpenCamera
@onready var image_file_dialog: FileDialog = $ImageFileDialog
@onready var video_file_dialog: FileDialog = $VideoFileDialog
@onready var select_camera_dialog: ConfirmationDialog = $SelectCamera
@onready var opt_camera_feed: OptionButton = $SelectCamera/VBoxContainer/SelectFeed
@onready var opt_camera_format: OptionButton = $SelectCamera/VBoxContainer/SelectFormat
@onready var permission_dialog: AcceptDialog = $PermissionDialog

func _exit_tree() -> void:
	if request:
		request.cancel_request()
		request.queue_free()
		request = null
	_close_camera()

func _ready():
	btn_back.pressed.connect(self._back)
	btn_load_image.pressed.connect(self._open_image)
	btn_load_video.pressed.connect(self._open_video)
	btn_open_camera.pressed.connect(self._open_camera)
	image_file_dialog.file_selected.connect(self._load_image)
	video_file_dialog.file_selected.connect(self._load_video)
	get_tree().on_request_permissions_result.connect(_camera_permission_result)
	select_camera_dialog.get_ok_button().disabled = true
	opt_camera_feed.item_selected.connect(_camera_selected)
	opt_camera_format.item_selected.connect(_format_selected)
	select_camera_dialog.confirmed.connect(_start_camera)
	select_camera_dialog.canceled.connect(_close_camera)
	_init_task()

func _process(_delta: float) -> void:
	if request:
		var max_size := request.get_body_size()
		if max_size > 0:
			progress_bar.value = 100.0 * request.get_downloaded_bytes() / max_size
	if camera_feed != null:
		_read_camera_frame()
	elif video_player.is_playing():
		var timestamp_ms := roundi(video_player.stream_position * 1000.0)
		if timestamp_ms <= last_video_timestamp_ms:
			return
		var image := video_player.get_video_texture().get_image()
		if image != null:
			last_video_timestamp_ms = timestamp_ms
			_process_video(_prepare_image(image), timestamp_ms)

func _prepare_image(image: Image) -> Image:
	image.convert(Image.FORMAT_RGBA8)
	return image

func _reset() -> void:
	video_player.stop()
	_close_camera()

func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_PAUSED and camera_device != null:
		_close_camera()

func _back() -> void:
	_reset()
	Global.go_to_main_scene()

func _get_external_file() -> void:
	progress_bar.hide()
	image_view.show()
	request = null
	_init_task()

func _init_task() -> void:
	btn_load_image.disabled = false
	btn_load_video.disabled = false
	btn_open_camera.disabled = false

func _task_initialization_failed() -> void:
	btn_load_image.disabled = true
	btn_load_video.disabled = true
	btn_open_camera.disabled = true
	permission_dialog.title = "GDMP"
	permission_dialog.dialog_text = "Could not initialize inference for this model."
	permission_dialog.popup_centered()

func _open_image() -> void:
	_reset()
	image_file_dialog.popup_centered_ratio()

func _load_image(path: String) -> void:
	var image := Image.load_from_file(path)
	if image == null:
		return
	if running_mode != MediaPipeVisionTask.RUNNING_MODE_IMAGE:
		running_mode = MediaPipeVisionTask.RUNNING_MODE_IMAGE
		_init_task()
	if not btn_load_image.disabled:
		_process_image(_prepare_image(image))

func _open_video() -> void:
	_reset()
	video_file_dialog.popup_centered_ratio()

func _load_video(path: String) -> void:
	var stream := VideoStreamTheora.new()
	stream.file = path
	running_mode = MediaPipeVisionTask.RUNNING_MODE_VIDEO
	last_video_timestamp_ms = -1
	_init_task()
	if not btn_load_video.disabled:
		video_player.stream = stream
		video_player.play()

func _open_camera() -> void:
	if camera_feed != null:
		_close_camera()
		return
	_reset()
	if camera_manager.permission_status() == SaltsCameraManager.PERMISSION_REQUESTABLE:
		waiting_for_permission = OS.get_name() == "Android"
		var error := camera_manager.request_permission()
		if error != OK:
			waiting_for_permission = false
			_camera_failed("Could not request camera permission", error)
			return
		if waiting_for_permission:
			if camera_manager.permission_status() == SaltsCameraManager.PERMISSION_GRANTED:
				_camera_permission_result(CAMERA_PERMISSION, true)
			return
	_select_camera()

func _camera_permission_result(permission: String, granted: bool) -> void:
	if permission != CAMERA_PERMISSION or not waiting_for_permission:
		return
	waiting_for_permission = false
	if granted:
		_select_camera()
	else:
		_camera_failed("Camera permission was denied", ERR_UNAUTHORIZED)

func _select_camera() -> void:
	opt_camera_feed.clear()
	opt_camera_format.clear()
	select_camera_dialog.get_ok_button().disabled = true
	var devices := camera_manager.list_devices()
	if devices.is_empty():
		_camera_failed("No camera is available", camera_manager.get_last_error())
		return
	for device in devices:
		opt_camera_feed.add_item(device["name"])
		opt_camera_feed.set_item_metadata(opt_camera_feed.item_count - 1, device["id"])
	opt_camera_feed.select(-1)
	select_camera_dialog.popup_centered()

func _camera_selected(index: int) -> void:
	_close_camera()
	opt_camera_format.clear()
	select_camera_dialog.get_ok_button().disabled = true
	camera_device = camera_manager.open_device(opt_camera_feed.get_item_metadata(index))
	if camera_device == null:
		_camera_failed("Could not open the selected camera", camera_manager.get_last_error())
		return
	var modes := camera_device.list_modes(false)
	if modes.is_empty():
		_camera_failed("The selected camera has no available modes", camera_device.get_last_error())
		return
	for mode in modes:
		var fps := float(mode["framerate_numerator"]) / float(mode["framerate_denominator"])
		opt_camera_format.add_item("%d × %d · %.2f fps · %s" % [
			mode["width"], mode["height"], fps, CAMERA_FORMATS[mode["format"]],
		])
		opt_camera_format.set_item_metadata(opt_camera_format.item_count - 1, mode)
	opt_camera_format.select(-1)

func _format_selected(_index: int) -> void:
	select_camera_dialog.get_ok_button().disabled = false

func _start_camera() -> void:
	if camera_device == null or opt_camera_format.selected < 0:
		return
	running_mode = MediaPipeVisionTask.RUNNING_MODE_VIDEO
	_init_task()
	if btn_open_camera.disabled:
		_close_camera()
		return
	camera_feed = camera_device.create_feed(opt_camera_format.get_selected_metadata())
	if camera_feed == null:
		_camera_failed("Could not create the camera feed", camera_device.get_last_error())
		return
	camera_feed.set_active(true)
	if not camera_feed.is_active():
		_camera_failed("Could not start the camera", camera_feed.get_last_error())
		return
	btn_open_camera.text = "Stop Camera"

func _read_camera_frame() -> void:
	if not camera_feed.is_active():
		_camera_failed("Camera capture ended", camera_feed.get_last_error())
		return
	var now := Time.get_ticks_msec()
	if now < next_camera_frame_ms:
		return
	var frame := camera_feed.get_latest_raw_frame()
	if frame == null or frame.get_timestamp_us() == last_camera_timestamp_us:
		return
	last_camera_timestamp_us = frame.get_timestamp_us()
	next_camera_frame_ms = now + CAMERA_INTERVAL_MS
	var image: Image = frame.to_image()
	if image != null:
		_process_video(_prepare_image(image), now)

func _close_camera() -> void:
	if camera_feed != null:
		camera_feed.close()
		camera_feed = null
	if camera_device != null:
		camera_device.close()
		camera_device = null
	last_camera_timestamp_us = -1
	next_camera_frame_ms = 0
	btn_open_camera.text = "Open Camera"

func _camera_failed(message: String, error: Error) -> void:
	_close_camera()
	select_camera_dialog.hide()
	permission_dialog.title = "Camera"
	permission_dialog.dialog_text = message if error == OK else "%s (%s)" % [message, error_string(error)]
	permission_dialog.popup_centered()

func _process_image(_image: Image) -> void:
	pass

func _process_video(_image: Image, _timestamp_ms: int) -> void:
	pass

func get_external_model(path: String) -> FileAccess:
	if request != null:
		return null
	var file := Global.get_model(path)
	if file != null:
		return file
	image_view.hide()
	request = Global.get_external_model(path, _get_external_file)
	if request != null:
		progress_bar.show()
	else:
		external_files_disabled.show()
	return null

func update_image(image: Image) -> void:
	image.convert(Image.FORMAT_RGB8)
	var texture := image_view.texture as ImageTexture
	if Vector2i(texture.get_size()) == image.get_size():
		texture.update(image)
	else:
		texture.set_image(image)
