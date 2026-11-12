extends SceneTree


func _initialize() -> void:
	call_deferred("run")


func require(condition: bool, message: String) -> bool:
	if not condition:
		printerr(message)
		quit(1)
	return condition


func cpu_options(model: String) -> MediaPipeTaskBaseOptions:
	var options := MediaPipeTaskBaseOptions.new()
	options.delegate = MediaPipeTaskBaseOptions.DELEGATE_CPU
	options.model_asset_buffer = FileAccess.get_file_as_bytes("res://test-data/" + model)
	return options


func check_face_detector() -> bool:
	var options := cpu_options("blaze_face_short_range.tflite")
	if not require(options.delegate == MediaPipeTaskBaseOptions.DELEGATE_CPU,
			"Could not select the CPU delegate"):
		return false
	var task := MediaPipeFaceDetector.new()
	if not require(task.initialize(options), "Could not initialize CPU Face Detector"):
		return false
	var portrait := Image.load_from_file("res://test-data/portrait.jpg")
	portrait.convert(Image.FORMAT_RGBA8)
	var image := MediaPipeImage.new()
	image.set_image(portrait)
	var result := task.detect(image)
	return require(result != null and not result.detections.is_empty(), "CPU Face Detector returned no detections")


func check_text_tasks() -> bool:
	var options := MediaPipeClassifierOptions.new()
	var text_classifier := MediaPipeTextClassifier.new()
	if not require(text_classifier.initialize(cpu_options("bert_classifier.tflite"), options),
			"Could not initialize CPU Text Classifier"):
		return false
	var classification := text_classifier.classify("This movie was wonderful and I enjoyed every minute of it.")
	if not require(classification != null and not classification.classifications.is_empty() and
			not classification.classifications[0].categories.is_empty(), "CPU Text Classifier returned no category"):
		return false
	var language_detector := MediaPipeLanguageDetector.new()
	if not require(language_detector.initialize(cpu_options("language_detector.tflite"), options),
			"Could not initialize CPU Language Detector"):
		return false
	var languages := language_detector.detect("This is a clear English sentence used to verify the language detector.")
	return require(not languages.is_empty() and languages[0].language_code == "en",
			"CPU Language Detector did not detect English")


func check_audio_classifier() -> bool:
	var options := MediaPipeClassifierOptions.new()
	var task := MediaPipeAudioClassifier.new()
	if not require(task.initialize(cpu_options("yamnet.tflite"), MediaPipeAudioTask.RUNNING_MODE_AUDIO_CLIPS, options),
			"Could not initialize CPU Audio Classifier"):
		return false
	var samples := PackedVector2Array()
	samples.resize(16000)
	for i in range(samples.size()):
		samples[i] = Vector2(0.1 * sin(TAU * 440.0 * float(i) / 16000.0), 0)
	var result := task.classify(samples, false, 16000.0)
	return require(not result.is_empty() and not result[0].classifications.is_empty(),
			"CPU Audio Classifier returned no classification")


func run() -> void:
	if check_face_detector() and check_text_tasks() and check_audio_classifier():
		print("CPU inference passed for Face Detector, Text Classifier, Language Detector, and Audio Classifier")
		quit(0)
