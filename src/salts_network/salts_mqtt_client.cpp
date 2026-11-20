#include <core/error_codes.h>
#include <flowie_mqtt_client.h>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

using namespace godot;

namespace {
constexpr size_t MAX_COMMANDS = 64;
constexpr size_t MAX_MESSAGES = 64;
constexpr int MAX_SYSTEM_ROOTS = 16;
constexpr size_t MAX_MESSAGE_BYTES = 4 * 1024 * 1024;
constexpr uint32_t SHUTDOWN_TIMEOUT_MS = 1000;

using Clock = std::chrono::steady_clock;

enum Operation { CONNECT, PUBLISH, SUBSCRIBE, UNSUBSCRIBE, PING, DISCONNECT, ERROR, MESSAGE };

struct Event {
	Operation operation;
	int status = SALTS_OK;
	int packet_id = 0;
	int reason_code = -1;
	bool session_present = false;
	int qos = 0;
	bool retain = false;
	bool duplicate = false;
	std::vector<uint8_t> topic;
	std::vector<uint8_t> payload;
	std::vector<uint8_t> reason_codes;
};

struct MqttState {
	flowie_mqtt_client_t *client = nullptr;
	std::mutex mutex;
	std::vector<Event> events;
	size_t messages = 0;
	size_t message_bytes = 0;
	size_t outstanding = 0;
	std::chrono::milliseconds ping_interval{0};
	Clock::time_point ping_due{};
	size_t pending_pings = 0;
	bool closing = false;
};

flowie_mqtt_span_t span(const CharString &value) {
	return {reinterpret_cast<const uint8_t *>(value.get_data()), static_cast<size_t>(value.length())};
}

flowie_mqtt_span_t span(const PackedByteArray &value) {
	return {value.ptr(), static_cast<size_t>(value.size())};
}

void copy_span(std::vector<uint8_t> &out, flowie_mqtt_span_t value) {
	if (value.size) {
		out.assign(value.data, value.data + value.size);
	}
}

PackedByteArray to_bytes(const std::vector<uint8_t> &value) {
	PackedByteArray bytes;
	if (!value.empty()) {
		bytes.resize(value.size());
		std::memcpy(bytes.ptrw(), value.data(), value.size());
	}
	return bytes;
}

template <Operation operation>
void completed(flowie_mqtt_client_t *client, int status,
		const flowie_mqtt_control_packet_view_t *response, void *user) {
	auto &state = *static_cast<MqttState *>(user);
	Event event{operation};
	event.status = status;
	if (response) {
		event.packet_id = response->packet_id;
		event.reason_code = response->reason_code;
		event.session_present = response->session_present;
		copy_span(event.reason_codes, response->reason_codes);
	} else if (status != SALTS_OK) {
		uint8_t reason = 0;
		if (flowie_mqtt_client_server_disconnect_reason(client, &reason) == SALTS_OK) {
			event.reason_code = reason;
		}
	}
	std::lock_guard<std::mutex> lock(state.mutex);
	state.events.push_back(std::move(event));
}

void failed(flowie_mqtt_client_t *client, int status, void *user) {
	completed<ERROR>(client, status, nullptr, user);
}

int received(flowie_mqtt_client_t *, const flowie_mqtt_publish_view_t *message, void *user) {
	auto &state = *static_cast<MqttState *>(user);
	std::lock_guard<std::mutex> lock(state.mutex);
	const size_t bytes = message->topic.size + message->payload.size;
	if (state.messages == MAX_MESSAGES || bytes > MAX_MESSAGE_BYTES - state.message_bytes) {
		return SALTS_ENOBUFS;
	}
	Event event{MESSAGE};
	event.packet_id = message->packet_id;
	event.qos = message->qos;
	event.retain = message->retain;
	event.duplicate = message->duplicate;
	copy_span(event.topic, message->topic);
	copy_span(event.payload, message->payload);
	state.events.push_back(std::move(event));
	++state.messages;
	state.message_bytes += bytes;
	return SALTS_OK;
}

bool valid_cstring(const CharString &text) {
	return static_cast<int64_t>(std::strlen(text.get_data())) == text.length();
}

bool valid_options(const Dictionary &options) {
	const Array keys = options.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		if (keys[i].get_type() != Variant::STRING) {
			return false;
		}
		const String key = keys[i];
		const Variant::Type type = options[key].get_type();
		if (key == "path" || key == "ca_file" || key == "cert_file" || key == "key_file" ||
			key == "key_password" || key == "username") {
			if (type != Variant::STRING) {
				return false;
			}
		} else if ((key == "password" && type == Variant::PACKED_BYTE_ARRAY) ||
			(key == "clean_start" && type == Variant::BOOL) ||
			(key == "system_topic_roots" && type == Variant::PACKED_STRING_ARRAY) ||
			((key == "timeout_ms" || key == "keep_alive") && type == Variant::INT)) {
			continue;
		} else {
			return false;
		}
	}
	return true;
}
} // namespace

class SaltsMqttClient : public Node {
	GDCLASS(SaltsMqttClient, Node)

	std::unique_ptr<MqttState> state;
	uint64_t generation = 0;

	int can_submit() const {
		if (!state) {
			return SALTS_ENOTCONN;
		}
		if (state->closing) {
			return SALTS_ESHUTDOWN;
		}
		return state->outstanding < MAX_COMMANDS ? SALTS_OK : SALTS_ENOBUFS;
	}

	int submitted(int status) {
		if (status == SALTS_OK) {
			++state->outstanding;
		}
		return status;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("open", "host", "port", "client_id", "transport", "version", "options"),
			&SaltsMqttClient::open, DEFVAL(TCP), DEFVAL(MQTT_5), DEFVAL(Dictionary()));
		ClassDB::bind_method(D_METHOD("is_connected"), &SaltsMqttClient::is_connected);
		ClassDB::bind_method(D_METHOD("publish", "topic", "payload", "qos", "retain"),
			&SaltsMqttClient::publish, DEFVAL(0), DEFVAL(false));
		ClassDB::bind_method(D_METHOD("subscribe", "filter", "qos"), &SaltsMqttClient::subscribe, DEFVAL(0));
		ClassDB::bind_method(D_METHOD("unsubscribe", "filter"), &SaltsMqttClient::unsubscribe);
		ClassDB::bind_method(D_METHOD("ping"), &SaltsMqttClient::ping);
		ClassDB::bind_method(D_METHOD("disconnect_from_broker"), &SaltsMqttClient::disconnect_from_broker);
		ClassDB::bind_method(D_METHOD("close"), &SaltsMqttClient::close);
		ADD_SIGNAL(MethodInfo("operation_completed", PropertyInfo(Variant::STRING, "operation"),
			PropertyInfo(Variant::INT, "status"), PropertyInfo(Variant::DICTIONARY, "ack")));
		ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::STRING, "topic"),
			PropertyInfo(Variant::PACKED_BYTE_ARRAY, "payload"), PropertyInfo(Variant::DICTIONARY, "metadata")));
		ADD_SIGNAL(MethodInfo("connection_failed", PropertyInfo(Variant::INT, "status"),
			PropertyInfo(Variant::INT, "reason_code")));
		BIND_ENUM_CONSTANT(TCP);
		BIND_ENUM_CONSTANT(TLS);
		BIND_ENUM_CONSTANT(WS);
		BIND_ENUM_CONSTANT(WSS);
		BIND_ENUM_CONSTANT(MQTT_3_1);
		BIND_ENUM_CONSTANT(MQTT_3_1_1);
		BIND_ENUM_CONSTANT(MQTT_5);
	}

public:
	enum Transport {
		TCP = FLOWIE_MQTT_CLIENT_TRANSPORT_TCP, TLS = FLOWIE_MQTT_CLIENT_TRANSPORT_TLS,
		WS = FLOWIE_MQTT_CLIENT_TRANSPORT_WS, WSS = FLOWIE_MQTT_CLIENT_TRANSPORT_WSS
	};
	enum Version {
		MQTT_3_1 = FLOWIE_MQTT_VERSION_3_1, MQTT_3_1_1 = FLOWIE_MQTT_VERSION_3_1_1,
		MQTT_5 = FLOWIE_MQTT_VERSION_5
	};

	int open(const String &host, int64_t port, const String &client_id, int64_t transport,
			int64_t version, const Dictionary &options) {
		if (state) {
			return SALTS_EALREADY;
		}
		if (port < 1 || port > UINT16_MAX || transport < TCP || transport > WSS ||
			version < MQTT_3_1 || version > MQTT_5 || !valid_options(options)) {
			return SALTS_EINVAL;
		}
		const CharString hostname = host.utf8();
		const CharString id = client_id.utf8();
		const CharString path = String(options.get("path", String())).utf8();
		const CharString ca = String(options.get("ca_file", String())).utf8();
		const CharString cert = String(options.get("cert_file", String())).utf8();
		const CharString key = String(options.get("key_file", String())).utf8();
		const CharString key_password = String(options.get("key_password", String())).utf8();
		const CharString username = String(options.get("username", String())).utf8();
		const PackedByteArray password = options.get("password", PackedByteArray());
		const int64_t keep_alive = options.get("keep_alive", 30);
		const int64_t timeout_ms = options.get("timeout_ms", 30000);
		if (!valid_cstring(hostname) || !valid_cstring(path) || !valid_cstring(ca) ||
			!valid_cstring(cert) || !valid_cstring(key) || !valid_cstring(key_password) ||
			keep_alive < 0 || keep_alive > UINT16_MAX || timeout_ms < 2 || timeout_ms > UINT32_MAX) {
			return SALTS_EINVAL;
		}

		PackedStringArray default_roots;
		default_roots.push_back("$SYS");
		const PackedStringArray system_roots = options.get("system_topic_roots", default_roots);
		if (system_roots.size() > MAX_SYSTEM_ROOTS) {
			return SALTS_ENOBUFS;
		}
		std::vector<CharString> filters;
		filters.reserve(system_roots.size() + 1);
		filters.push_back(String("#").utf8());
		for (int64_t i = 0; i < system_roots.size(); ++i) {
			const String root = system_roots[i];
			if (!root.begins_with("$") || root.contains("/") || root.contains("+") || root.contains("#")) {
				return SALTS_EINVAL;
			}
			filters.push_back((root + "/#").utf8());
		}
		std::vector<flowie_mqtt_client_topic_handler_t> handlers;
		handlers.reserve(filters.size());
		for (const CharString &filter : filters) {
			handlers.push_back({span(filter), received});
		}
		state = std::make_unique<MqttState>();
		if (keep_alive) {
			state->ping_interval = std::chrono::milliseconds(std::min(keep_alive * 500, timeout_ms / 2));
		}
		flowie_mqtt_client_config_t config = FLOWIE_MQTT_CLIENT_CONFIG_INIT;
		config.transport = static_cast<flowie_mqtt_client_transport_t>(transport);
		config.host = hostname.get_data();
		config.port = static_cast<int>(port);
		config.path = path.is_empty() ? nullptr : path.get_data();
		config.timeout_ms = timeout_ms;
		config.tls = {
			ca.is_empty() ? nullptr : ca.get_data(), cert.is_empty() ? nullptr : cert.get_data(),
			key.is_empty() ? nullptr : key.get_data(), key_password.is_empty() ? nullptr : key_password.get_data()
		};
		config.topic_handlers = {handlers.data(), handlers.size()};
		config.on_connect = completed<CONNECT>;
		config.on_publish = completed<PUBLISH>;
		config.on_subscribe = completed<SUBSCRIBE>;
		config.on_unsubscribe = completed<UNSUBSCRIBE>;
		config.on_ping = completed<PING>;
		config.on_disconnect = completed<DISCONNECT>;
		config.on_error = failed;
		config.user_data = state.get();
		int status = flowie_mqtt_client_create(&config, &state->client);
		if (status != SALTS_OK) {
			state.reset();
			return status;
		}
		status = flowie_mqtt_client_set_version(state->client, static_cast<flowie_mqtt_version_t>(version));
		if (status == SALTS_OK) {
			flowie_mqtt_connect_packet_t packet = FLOWIE_MQTT_CONNECT_PACKET_INIT;
			packet.clean_start = static_cast<bool>(options.get("clean_start", true));
			packet.keep_alive = static_cast<uint16_t>(keep_alive);
			packet.client_id = span(id);
			packet.has_username = options.has("username");
			packet.has_password = options.has("password");
			packet.username = span(username);
			packet.password = span(password);
			status = submitted(flowie_mqtt_client_connect(state->client, &packet));
		}
		if (status != SALTS_OK) {
			close();
			return status;
		}
		++generation;
		set_process(true);
		return SALTS_OK;
	}

	bool is_connected() const {
		return state && !state->closing && flowie_mqtt_client_is_connected(state->client);
	}

	int publish(const String &topic, const PackedByteArray &payload, int64_t qos, bool retain) {
		const int status = can_submit();
		if (status != SALTS_OK || qos < 0 || qos > 2) {
			return status != SALTS_OK ? status : SALTS_EINVAL;
		}
		const CharString name = topic.utf8();
		flowie_mqtt_client_publish_topic_t entry{};
		entry.qos = static_cast<uint8_t>(qos);
		entry.retain = retain;
		entry.topic = span(name);
		entry.payload = span(payload);
		flowie_mqtt_client_publish_topic_vec_t topics = FLOWIE_MQTT_CLIENT_PUBLISH_TOPIC_VEC_INIT;
		topics.data = &entry;
		topics.count = 1;
		return submitted(flowie_mqtt_client_publish(state->client, &topics));
	}

	int subscribe(const String &filter, int64_t qos) {
		const int status = can_submit();
		if (status != SALTS_OK || qos < 0 || qos > 2) {
			return status != SALTS_OK ? status : SALTS_EINVAL;
		}
		const CharString name = filter.utf8();
		flowie_mqtt_subscription_t subscription{};
		subscription.filter = span(name);
		subscription.qos = static_cast<uint8_t>(qos);
		flowie_mqtt_subscribe_packet_t packet = FLOWIE_MQTT_SUBSCRIBE_PACKET_INIT;
		packet.subscriptions = &subscription;
		packet.subscription_count = 1;
		return submitted(flowie_mqtt_client_subscribe(state->client, &packet));
	}

	int unsubscribe(const String &filter) {
		const int status = can_submit();
		if (status != SALTS_OK) {
			return status;
		}
		const CharString name = filter.utf8();
		const flowie_mqtt_span_t topic_filter = span(name);
		flowie_mqtt_unsubscribe_packet_t packet = FLOWIE_MQTT_UNSUBSCRIBE_PACKET_INIT;
		packet.filters = &topic_filter;
		packet.filter_count = 1;
		return submitted(flowie_mqtt_client_unsubscribe(state->client, &packet));
	}

	int ping() {
		const int status = can_submit();
		if (status != SALTS_OK) {
			return status;
		}
		const int result = submitted(flowie_mqtt_client_ping(state->client));
		if (result == SALTS_OK) {
			++state->pending_pings;
			state->ping_due = Clock::now() + state->ping_interval;
		}
		return result;
	}

	int disconnect_from_broker() {
		const int status = can_submit();
		return status == SALTS_OK
			? submitted(flowie_mqtt_client_disconnect(state->client, 0, {})) : status;
	}

	int close() {
		if (!state) {
			return SALTS_OK;
		}
		state->closing = true;
		const int status = flowie_mqtt_client_try_destroy(state->client, SHUTDOWN_TIMEOUT_MS);
		if (status == SALTS_OK) {
			++generation;
			state.reset();
		}
		return status;
	}

	void _process(double) override {
		if (!state || state->closing) {
			return;
		}
		const uint64_t current = generation;
		std::vector<Event> events;
		{
			std::lock_guard<std::mutex> lock(state->mutex);
			events.swap(state->events);
			state->messages = 0;
			state->message_bytes = 0;
		}
		static const char *operation_names[] = {"connect", "publish", "subscribe", "unsubscribe", "ping", "disconnect"};
		for (const auto &event : events) {
			if (current != generation || state->closing) {
				return;
			}
			if (event.operation == MESSAGE) {
				Dictionary metadata;
				metadata["packet_id"] = event.packet_id;
				metadata["qos"] = event.qos;
				metadata["retain"] = event.retain;
				metadata["duplicate"] = event.duplicate;
				emit_signal("message_received",
					String::utf8(reinterpret_cast<const char *>(event.topic.data()), event.topic.size()),
					to_bytes(event.payload), metadata);
			} else if (event.operation == ERROR) {
				emit_signal("connection_failed", event.status, event.reason_code);
			} else {
				--state->outstanding;
				if (event.operation == PING) {
					--state->pending_pings;
				} else if (event.operation == CONNECT) {
					state->ping_due = Clock::now() + state->ping_interval;
				}
				Dictionary ack;
				ack["packet_id"] = event.packet_id;
				ack["reason_code"] = event.reason_code;
				ack["reason_codes"] = to_bytes(event.reason_codes);
				ack["session_present"] = event.session_present;
				emit_signal("operation_completed", String(operation_names[event.operation]), event.status, ack);
			}
		}
		if (current == generation && is_connected() && state->ping_interval.count() &&
			!state->pending_pings && Clock::now() >= state->ping_due) {
			ping();
		}
	}

	~SaltsMqttClient() override {
		int status = close();
		if (status == SALTS_ETIMEDOUT || status == SALTS_EBUSY || status == SALTS_ENOBUFS) {
			status = close();
		}
		if (status != SALTS_OK) {
			UtilityFunctions::push_error("SaltsMqttClient shutdown failed: ", status);
			state.release();
		}
	}
};

VARIANT_ENUM_CAST(SaltsMqttClient::Transport);
VARIANT_ENUM_CAST(SaltsMqttClient::Version);

extern "C" GDExtensionBool GDE_EXPORT salts_mqtt_client_library_init(
		GDExtensionInterfaceGetProcAddress get_proc_address, GDExtensionClassLibraryPtr library,
		GDExtensionInitialization *initialization) {
	GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
	init.register_initializer([](ModuleInitializationLevel level) {
		if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
			GDREGISTER_CLASS(SaltsMqttClient);
		}
	});
	init.register_terminator([](ModuleInitializationLevel) {});
	init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init.init();
}
