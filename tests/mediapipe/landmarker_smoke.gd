extends SceneTree


func _initialize() -> void:
	call_deferred("run")


func require(condition: bool, message: String) -> bool:
	if not condition:
		printerr(message)
		quit(1)
	return condition


func run() -> void:
	var landmarker := SaltsMediaPipeLandmarker.new()
	var cases := [
		[SaltsMediaPipeLandmarker.FACE, "face_landmarker", "portrait", 478],
		[SaltsMediaPipeLandmarker.HAND, "hand_landmarker", "right_hands", 21],
		[SaltsMediaPipeLandmarker.POSE, "pose_landmarker_lite", "pose", 33],
	]
	for test in cases:
		var model := FileAccess.get_file_as_bytes("res://test-data/%s.task" % test[1])
		var image := Image.load_from_file("res://test-data/%s.jpg" % test[2])
		if not require(landmarker.open(model, test[0]) == OK, "Could not open " + test[1]):
			return
		var subjects := landmarker.detect(image)
		if not require(landmarker.get_last_error() == OK and subjects.size() == 1 and
				subjects[0].size() == test[3], "Wrong landmark count for " + test[1]):
			return
		for point in subjects[0]:
			if not require(point.is_finite(), "Non-finite landmark for " + test[1]):
				return
		if not require(landmarker.close() == OK, "Could not close " + test[1]):
			return
		print("%s returned %d landmarks" % [test[1], test[3]])

	var face := FileAccess.get_file_as_bytes("res://test-data/face_landmarker.task")
	var portrait := Image.load_from_file("res://test-data/portrait.jpg")
	portrait.convert(Image.FORMAT_RGBA8)
	if not require(landmarker.open(face, SaltsMediaPipeLandmarker.FACE, SaltsMediaPipeLandmarker.VIDEO) == OK,
			"Could not open video landmarker"):
		return
	var first := landmarker.detect(portrait, 0)
	if not require(landmarker.get_last_error() == OK and first.size() == 1 and
			first[0].size() == 478, "Video mode did not process the RGBA frame"):
		return
	if not require(landmarker.detect(portrait, 0).is_empty() and
			landmarker.get_last_error() == ERR_INVALID_PARAMETER, "Video accepted a repeated timestamp"):
		return
	var second := landmarker.detect(portrait, 33)
	if not require(landmarker.get_last_error() == OK and second.size() == 1 and
			second[0].size() == 478, "Video did not advance after the rejected timestamp"):
		return
	if not require(landmarker.close() == OK, "Could not close video landmarker"):
		return
	print("RGBA video inference and timestamp ordering passed")
	quit(0)
