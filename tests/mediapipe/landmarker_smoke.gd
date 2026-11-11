extends SceneTree


func _initialize() -> void:
	call_deferred("run")


func require(condition: bool, message: String) -> bool:
	if not condition:
		printerr(message)
		quit(1)
	return condition


func model_options(name: String) -> MediaPipeTaskBaseOptions:
	var options := MediaPipeTaskBaseOptions.new()
	options.delegate = MediaPipeTaskBaseOptions.DELEGATE_CPU if OS.get_name() == "Windows" else MediaPipeTaskBaseOptions.DELEGATE_GPU
	options.model_asset_buffer = FileAccess.get_file_as_bytes("res://test-data/%s.task" % name)
	return options


func check_landmarks(result: Object, getter: String, count: int) -> bool:
	if not require(result != null, "%s returned no result" % getter):
		return false
	var subjects: Array = result.call(getter)
	if not require(subjects.size() == 1, "%s returned %d subjects" % [getter, subjects.size()]):
		return false
	var landmarks: Array = subjects[0].get_landmarks()
	if not require(landmarks.size() == count, "%s returned %d landmarks" % [getter, landmarks.size()]):
		return false
	for landmark in landmarks:
		var point := Vector3(landmark.get_x(), landmark.get_y(), landmark.get_z())
		if not require(point.is_finite(), "%s returned a non-finite landmark" % getter):
			return false
	return true


func run() -> void:
	var cases := [
		["MediaPipeFaceLandmarker", "face_landmarker", "portrait", "get_face_landmarks", 478],
		["MediaPipeHandLandmarker", "hand_landmarker", "right_hands", "get_hand_landmarks", 21],
		["MediaPipePoseLandmarker", "pose_landmarker_lite", "pose", "get_pose_landmarks", 33],
	]
	for test in cases:
		var task := ClassDB.instantiate(test[0]) as Object
		if not require(task != null and task.call("initialize", model_options(test[1])),
				"Could not initialize " + test[0]):
			return
		var sample := Image.load_from_file("res://test-data/%s.jpg" % test[2])
		sample.convert(Image.FORMAT_RGBA8)
		var image := MediaPipeImage.new()
		image.set_image(sample)
		if not check_landmarks(task.call("detect", image), test[3], test[4]):
			return
		print("%s returned %d landmarks" % [test[0], test[4]])

	var task := MediaPipeFaceLandmarker.new()
	if not require(task.initialize(model_options("face_landmarker"), MediaPipeVisionTask.RUNNING_MODE_VIDEO),
			"Could not initialize the face landmarker in video mode"):
		return
	var portrait := Image.load_from_file("res://test-data/portrait.jpg")
	portrait.convert(Image.FORMAT_RGBA8)
	var frame := MediaPipeImage.new()
	frame.set_image(portrait)
	for timestamp in [0, 33]:
		if not check_landmarks(task.detect_video(frame, timestamp), "get_face_landmarks", 478):
			return
	print("RGBA video inference passed at two increasing timestamps")
	quit(0)
