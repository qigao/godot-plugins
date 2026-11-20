extends SceneTree

var http: SaltsHttpClient
var mqtt: SaltsMqttClient
var responses := {}
var operations := {}
var messages := []
var failures := 0


func _initialize() -> void:
	call_deferred("_run")


func check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error(message)


func wait_for(predicate: Callable) -> bool:
	var deadline := Time.get_ticks_msec() + 4000
	while not predicate.call():
		if Time.get_ticks_msec() >= deadline:
			return false
		await create_timer(0.01).timeout
	return true


func _run() -> void:
	var ports := OS.get_cmdline_user_args()
	http = SaltsHttpClient.new()
	mqtt = SaltsMqttClient.new()
	root.add_child(http)
	root.add_child(mqtt)
	http.request_completed.connect(func(id, result): responses[id] = result)
	http.rpc_completed.connect(func(id, result): responses[id] = result)
	http.poll_failed.connect(func(status): check(false, "HTTP poll failed: %s" % status))
	mqtt.operation_completed.connect(func(operation, status, ack): operations[operation] = [status, ack])
	mqtt.message_received.connect(func(topic, payload, metadata): messages.append([topic, payload, metadata]))
	mqtt.connection_failed.connect(func(status, reason):
		check(false, "MQTT connection failed: %s, %s" % [status, reason]))

	await test_http(ports)
	await test_mqtt(int(ports[3]), SaltsMqttClient.MQTT_5)
	await test_mqtt(int(ports[3]), SaltsMqttClient.MQTT_3_1_1)
	check(http.close() == 0, "HTTP shutdown")
	http.queue_free()
	mqtt.queue_free()
	await process_frame
	if failures == 0:
		print("Salts networking test passed")
	quit(0 if failures == 0 else 1)


func test_http(ports: PackedStringArray) -> void:
	var authority := "127.0.0.1"
	var h1 := "tcp://127.0.0.1:" + ports[0]
	var h2 := "tcp://127.0.0.1:" + ports[1]
	var hold := "tcp://127.0.0.1:" + ports[2]
	var bytes := PackedByteArray([0, 50, 255])
	var ordinary := http.request(h1, authority, "/echo", SaltsHttpClient.POST,
		SaltsHttpClient.HTTP_1_1, {"X-Test": "godot"}, bytes)
	var multiplexed := http.request(h2, authority, "/", SaltsHttpClient.GET, SaltsHttpClient.HTTP_2)
	var params := {"values": [9007199254740993, "长安", null, true, -8, 1.5]}
	var rpc := http.request_rpc(h2, authority, "/rpc", "echo", params)
	var remote_error := http.request_rpc(h1, authority, "/rpc", "fail", null, SaltsHttpClient.HTTP_1_1)
	var cancel := http.request(hold, authority, "/", SaltsHttpClient.GET, SaltsHttpClient.HTTP_2)
	if ordinary <= 0 or multiplexed <= 0 or rpc <= 0 or remote_error <= 0 or cancel <= 0:
		check(false, "HTTP admission failed: %s" % [[ordinary, multiplexed, rpc, remote_error, cancel]])
		return
	await create_timer(0.1).timeout
	check(http.cancel(cancel) == 0, "Cancel an HTTP/2 request")
	if not await wait_for(func(): return responses.size() == 5):
		check(false, "Missing HTTP completions: %s" % responses)
		return
	var h1_result: Dictionary = responses[ordinary]
	var h2_result: Dictionary = responses[multiplexed]
	check(h1_result.get("error") == 0 and h1_result.get("http_major") == 1
		and h1_result.get("status_code") == 200 and h1_result.get("body") == bytes,
		"HTTP/1.1 byte response: %s" % h1_result)
	var repeated := []
	for header in h1_result.get("headers", []):
		if header[0].to_lower() == "x-repeat":
			repeated.append(header[1])
	check(repeated == ["first", "second"], "HTTP/1.1 duplicate response headers")
	check(h2_result.get("error") == 0 and h2_result.get("http_major") == 2
		and h2_result.get("body") == "h2".to_utf8_buffer(), "HTTP/2 response: %s" % h2_result)
	check(responses[rpc].get("result") == params, "HTTP/2 JSON-RPC result: %s" % responses[rpc])
	check(responses[remote_error].get("remote_error") ==
		{"code": -32001, "message": "fixture error", "data": [true, null]},
		"HTTP/1.1 JSON-RPC remote error: %s" % responses[remote_error])
	check(responses[cancel].get("error", 0) != 0 and http.cancel(cancel) < 0,
		"Canceled request ends once and its handle becomes stale")
	check(http.request_rpc(h2, authority, "/rpc", "echo", [Vector2.ONE]) < 0,
		"Reject non-JSON RPC parameters")


func test_mqtt(port: int, version: int) -> void:
	operations.clear()
	messages.clear()
	check(mqtt.open("127.0.0.1", port, "godot-test", SaltsMqttClient.TCP, version) == 0,
		"MQTT CONNECT admission")
	if not await wait_for(func(): return operations.has("connect")):
		check(false, "MQTT CONNACK missing")
		mqtt.close()
		return
	check(mqtt.is_connected() and operations["connect"][0] == 0
		and operations["connect"][1]["reason_code"] == 0, "MQTT CONNACK")
	var topic := "godot/test"
	var payload := PackedByteArray([1, 0, 255])
	check(mqtt.subscribe(topic, 1) == 0, "MQTT SUBSCRIBE admission")
	if not await wait_for(func(): return operations.has("subscribe")):
		check(false, "MQTT SUBACK missing")
		mqtt.close()
		return
	check(operations["subscribe"][0] == 0 and
		operations["subscribe"][1]["reason_codes"] == PackedByteArray([1]), "MQTT SUBACK")
	check(mqtt.publish(topic, payload, 1) == 0, "MQTT PUBLISH admission")
	if await wait_for(func(): return operations.has("publish") and messages.size() == 1):
		check(operations["publish"][0] == 0 and messages[0][0] == topic and messages[0][1] == payload
			and messages[0][2]["qos"] == 1, "MQTT QoS 1 publish and binary delivery")
	else:
		check(false, "MQTT publish or message missing: %s, %s" % [operations, messages])
	check(mqtt.ping() == 0 and mqtt.unsubscribe(topic) == 0, "MQTT PING and UNSUBSCRIBE admission")
	check(await wait_for(func(): return operations.has("ping") and operations.has("unsubscribe")),
		"MQTT ping and unsubscribe callbacks")
	check(mqtt.disconnect_from_broker() == 0, "MQTT DISCONNECT admission")
	check(await wait_for(func(): return operations.has("disconnect")), "MQTT disconnect callback")
	check(not mqtt.is_connected() and mqtt.close() == 0, "MQTT shutdown")
