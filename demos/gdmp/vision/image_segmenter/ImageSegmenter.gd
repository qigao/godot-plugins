extends VisionTask

var task: MediaPipeImageSegmenter
var task_file := "image_segmenter/selfie_segmenter/float16/latest/selfie_segmenter.tflite"
var mask: ImageTexture

func _ready():
	super._ready()
	mask = image_view.material.get_shader_parameter("mask")

func _init_task() -> void:
	var file := get_external_model(task_file)
	if file == null:
		return
	var base_options := MediaPipeTaskBaseOptions.new()
	base_options.model_asset_buffer = file.get_buffer(file.get_length())
	task = MediaPipeImageSegmenter.new()
	if not task.initialize(base_options, running_mode):
		_task_initialization_failed()
		return
	super()

func _process_image(image: Image) -> void:
	var input_image := MediaPipeImage.new()
	input_image.set_image(image)
	var result := task.segment(input_image)
	show_result(input_image, result)

func _process_video(image: Image, timestamp_ms: int) -> void:
	var input_image := MediaPipeImage.new()
	input_image.set_image(image)
	var result := task.segment_video(input_image, timestamp_ms)
	show_result(input_image, result)

func show_result(image: MediaPipeImage, result: MediaPipeImageSegmenterResult) -> void:
	if result.has_confidence_masks():
		# TODO: apply multiple confidence mask
		if result.confidence_masks.size():
			var mask_image := result.confidence_masks[0].get_image()
			mask.set_image(mask_image)
	update_image(image.image)
