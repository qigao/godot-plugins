extends SceneTree


func _initialize() -> void:
	call_deferred("run")


func require(condition: bool, message: String) -> bool:
	if not condition:
		printerr(message)
		quit(1)
	return condition


func run() -> void:
	CameraServer.monitoring_feeds = true
	var feed: SaltsCameraPreviewFixture = SaltsCameraPreviewFixture.new()
	var first := PackedByteArray([1, 2, 3, 255, 4, 5, 6, 255, 7, 8, 9, 255, 10, 11, 12, 255])
	feed.set_active(true)
	feed.present(first, 1)
	if not require(feed.get_statistics().converted == 0, "Raw-only feed converted a frame"):
		return
	CameraServer.add_feed(feed)
	var texture := CameraTexture.new()
	texture.camera_feed_id = feed.get_id()
	texture.camera_is_active = true
	feed.present(first, 7)
	if not require(texture.get_width() == 2 and texture.get_height() == 2, "CameraTexture did not receive the converted frame"):
		return
	var snapshot := feed.get_latest_raw_frame()
	if not require(snapshot != null and snapshot.get_format() == SaltsCameraFrame.FORMAT_BGRA and snapshot.get_timestamp_us() == 7, "Incorrect raw snapshot metadata"):
		return
	var second := first.duplicate()
	second[0] = 42
	feed.present(second, 8)
	if not require(feed.get_latest_raw_frame().get_bytes() == second and feed.get_statistics().displayed == 2, "Latest frame or preview counter did not advance"):
		return
	feed.close()
	if not require(not feed.is_active() and feed.get_latest_raw_frame() == null, "Close left a frame in the mailbox"):
		return
	if not require(snapshot.get_bytes() == first and texture.get_width() == 0, "Close altered the owned snapshot or left the feed registered"):
		return
	var image := snapshot.to_image()
	var expected := PackedByteArray([3, 2, 1, 255, 6, 5, 4, 255, 9, 8, 7, 255, 12, 11, 10, 255])
	if not require(image != null and image.get_format() == Image.FORMAT_RGBA8 and
			image.get_data() == expected, "Snapshot could not supply a CPU image after capture closed"):
		return
	print("Salts Camera test-only frame reached CameraTexture and the raw snapshot survived close")
	quit(0)
