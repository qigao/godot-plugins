# Salts MQTT Client

A Godot 4.7.2 GDExtension backed by Flowie's native MQTT client. It exposes MQTT 3.1, 3.1.1 and 5 over TCP, TLS, WebSocket and secure WebSocket.

Copy both `addons/salts_mqtt_client` and `addons/salts_http_client` from the Salts Network CI artifact. The MQTT extension shares the CHttp and Salts runtimes staged under the HTTP add-on. Add a `SaltsMqttClient` node to the scene tree and call it from the main thread.

```gdscript
var mqtt := SaltsMqttClient.new()
add_child(mqtt)
mqtt.operation_completed.connect(func(operation, status, ack):
    if operation == "connect" and status == 0 and ack.reason_code == 0:
        mqtt.subscribe("game/lobby", 1))
mqtt.message_received.connect(func(topic, payload, metadata):
    print(topic, " ", payload.get_string_from_utf8(), " QoS ", metadata.qos))
var status := mqtt.open("broker.example.org", 8883, "godot-lobby", SaltsMqttClient.TLS)
```

| Call | Behavior |
| --- | --- |
| `open(host, port, client_id, transport = TCP, version = MQTT_5, options = {})` | Creates the client and queues CONNECT. Use the `connect` completion and its broker reason code to determine the result. |
| `subscribe(filter, qos = 0)` / `unsubscribe(filter)` | Queues a subscription change. SUBACK/UNSUBACK codes appear in the completion's `reason_codes`. |
| `publish(topic, payload, qos = 0, retain = false)` | Queues a binary `PackedByteArray` payload at QoS 0, 1 or 2. |
| `is_connected()` / `ping()` | Reads connection state or queues a PINGREQ. |
| `disconnect_from_broker()` | Queues a graceful disconnect. Wait for the `disconnect` completion before closing. |
| `close()` | Shuts down the worker and clears queued signals. After it returns zero, the node can open a new connection. A nonzero status allows another call to `close()`. |

Operations return zero when queued or a negative Salts status on immediate failure. `operation_completed(operation, status, ack)` reports command results and broker acknowledgements. Its dictionary contains `packet_id`, `reason_code`, `reason_codes` and `session_present`. A reason code of `-1` means that the operation carried no reason code. Background transport errors emit `connection_failed(status, reason_code)`. The add-on copies inbound payloads before returning to the Flowie worker and emits `message_received(topic, payload, metadata)` on Godot's main thread.

| Open option | Value |
| --- | --- |
| `path` | WS/WSS path. Flowie defaults to `/mqtt`. |
| `ca_file`, `cert_file`, `key_file`, `key_password` | Optional verified TLS/WSS trust and client identity. Native file paths must be readable on the device. Set certificate and key together. |
| `username`, `password` | Optional String username and binary `PackedByteArray` password. |
| `system_topic_roots` | Optional PackedStringArray of up to 16 distinct `$…` topic roots. Defaults to `["$SYS"]`. Include the roots for the system topic filters you subscribe to. |
| `clean_start` | Boolean. Defaults to `true`. |
| `keep_alive`, `timeout_ms` | Keep-alive seconds and Flowie I/O timeout in milliseconds. Both default to 30 seconds. The node sends PINGREQ before either interval expires while frames are processing. A zero keep-alive leaves ping scheduling to the caller. |

The receive queue holds up to 64 messages or 4 MiB between frames. Overflow is reported to Flowie as a receive error. The node forwards ordinary topics and the configured system topic hierarchies.

See the [Salts HTTP Client build instructions](../salts_http_client/README.md#build-from-source) for the common CMake build.
