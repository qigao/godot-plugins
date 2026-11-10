extends VBoxContainer

const PLAYBACK_CLASS := &"SaltsPlayback"
const SAMPLE_RATE := 48000
const TONE_FRAMES := 2 * SAMPLE_RATE
const TAIL_FRAMES := 12000
const FADE_FRAMES := 480

var playback: Object
var state_playing: int
var state_paused: int
var state_error: int
var tone := PackedByteArray()
var byte_offset := 0

@onready var status: Label = $Status
@onready var outputs: OptionButton = $OutputSelection/Outputs
@onready var scan: Button = $OutputSelection/Scan
@onready var play: Button = $PlaybackActions/Play
@onready var pause: Button = $PlaybackActions/Pause
@onready var stop: Button = $PlaybackActions/Stop


func _ready() -> void:
	if not ClassDB.class_exists(PLAYBACK_CLASS):
		status.text = "Salts Playback extension did not load"
		return
	playback = ClassDB.instantiate(PLAYBACK_CLASS)
	state_playing = ClassDB.class_get_integer_constant(PLAYBACK_CLASS, &"STATE_PLAYING")
	state_paused = ClassDB.class_get_integer_constant(PLAYBACK_CLASS, &"STATE_PAUSED")
	state_error = ClassDB.class_get_integer_constant(PLAYBACK_CLASS, &"STATE_ERROR")
	scan.pressed.connect(_scan_outputs)
	outputs.item_selected.connect(_select_output)
	play.pressed.connect(_play_tone)
	pause.pressed.connect(_pause_or_resume)
	stop.pressed.connect(_finish.bind("Audio playback stopped"))
	scan.disabled = false


func _scan_outputs() -> void:
	_finish("Choose an audio output")
	outputs.clear()
	play.disabled = true
	var devices: Array[Dictionary] = playback.list_devices()
	for entry in devices:
		var label := String(entry["name"])
		if entry["is_default"]:
			label += " · system default"
		outputs.add_item(label)
		outputs.set_item_metadata(outputs.item_count - 1, entry["id"])
	outputs.select(-1)
	outputs.text = "Audio output"
	outputs.disabled = devices.is_empty()
	if devices.is_empty():
		var error: Error = playback.get_last_error()
		status.text = "No audio outputs" if error == OK else error_string(error)


func _select_output(_index: int) -> void:
	play.disabled = false
	status.text = "Ready to play a test tone"


func _play_tone() -> void:
	var result: Error = playback.open_device(outputs.get_selected_metadata(), SAMPLE_RATE, 1)
	if result != OK:
		_finish("Could not open the selected output (%s)" % error_string(result))
		return
	if tone.is_empty():
		tone.resize((TONE_FRAMES + TAIL_FRAMES) * 4)
		tone.fill(0)
		for frame in TONE_FRAMES:
			var gain := 0.15 * minf(1.0, minf(frame, TONE_FRAMES - 1 - frame) / FADE_FRAMES)
			tone.encode_float(frame * 4, gain * sin(TAU * 440.0 * frame / SAMPLE_RATE))
	byte_offset = 0
	if not _queue_tone():
		return
	result = playback.start()
	if result != OK:
		_finish("Could not start playback (%s)" % error_string(result))
		return
	outputs.disabled = true
	play.disabled = true
	pause.disabled = false
	stop.disabled = false
	status.text = "Playing tone · " + outputs.get_item_text(outputs.selected)


func _queue_tone() -> bool:
	byte_offset += playback.write_pcm(tone, byte_offset)
	var result: Error = playback.get_last_error()
	if result != OK:
		_finish("Could not queue audio (%s)" % error_string(result))
	return result == OK


func _pause_or_resume() -> void:
	var resuming: bool = playback.get_state() == state_paused
	var result: Error = playback.resume() if resuming else playback.pause()
	if result != OK:
		_finish("Playback control failed (%s)" % error_string(result))
		return
	pause.text = "Pause" if resuming else "Resume"
	status.text = "Playing tone" if resuming else "Playback paused"


func _process(_delta: float) -> void:
	if playback == null:
		return
	var state: int = playback.get_state()
	if state == state_error:
		_finish("Audio output interrupted · scan to refresh devices")
	elif state == state_playing:
		if byte_offset < tone.size():
			_queue_tone()
		elif playback.get_buffered_bytes() == 0:
			_finish("Test tone finished")


func _finish(message: String) -> void:
	playback.close()
	outputs.disabled = outputs.item_count == 0
	play.disabled = outputs.selected < 0
	pause.disabled = true
	pause.text = "Pause"
	stop.disabled = true
	status.text = message


func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_PAUSED and playback != null and not stop.disabled:
		_finish("Audio playback stopped")


func _exit_tree() -> void:
	if playback != null:
		playback.close()
