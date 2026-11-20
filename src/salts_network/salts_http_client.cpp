#include "rpc_variant.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <vector>

using namespace godot;

namespace {
constexpr int MAX_REQUESTS = 16;
constexpr int MAX_HEADERS = 64;
constexpr size_t MAX_BODY = 1024 * 1024;
constexpr uint32_t SHUTDOWN_TIMEOUT_MS = 1000;

struct Completion {
	int64_t id;
	bool rpc;
	Dictionary result;
};

struct HttpState;

struct Pending {
	HttpState *owner;
	int64_t id;
	chttp_request handle{};
};

struct HttpState {
	chttp_async_client client{};
	chttp_tls_profile tls[2]{};
	std::map<int64_t, std::unique_ptr<Pending>> pending;
	std::vector<Completion> ready;
	int64_t next_id = 1;
	bool closing = false;
};

Dictionary error_result(int status, int native_status, const char *stage) {
	Dictionary result;
	result["error"] = status;
	result["native_error"] = native_status;
	result["stage"] = stage ? String::utf8(stage) : String();
	return result;
}

void complete(Pending *pending, bool rpc, const Dictionary &result) {
	HttpState &owner = *pending->owner;
	owner.ready.push_back({pending->id, rpc, result});
	owner.pending.erase(pending->id);
}

void http_complete(void *user, chttp_request, const chttp_response_view *response, const chttp_error *error) {
	Dictionary result;
	if (error) {
		result = error_result(error->status, error->native_status, error->stage);
	} else {
		result["error"] = SALTS_OK;
		result["status_code"] = response->status_code;
		result["http_major"] = response->http_major;
		result["http_minor"] = response->http_minor;
		Array headers;
		for (size_t i = 0; i < response->header_count; ++i) {
			PackedStringArray entry;
			entry.push_back(String::utf8(response->headers[i].name));
			entry.push_back(String::utf8(response->headers[i].value));
			headers.push_back(entry);
		}
		result["headers"] = headers;
		PackedByteArray body;
		if (response->body_size) {
			body.resize(response->body_size);
			std::memcpy(body.ptrw(), response->body, response->body_size);
		}
		result["body"] = body;
	}
	complete(static_cast<Pending *>(user), false, result);
}

void rpc_complete(void *user, chttp_request, const crpc_response_view *response, const crpc_error *error) {
	Dictionary result;
	if (error) {
		result = error_result(error->status, error->native_status, error->stage);
		result["status_code"] = error->http_status;
	} else {
		result["error"] = SALTS_OK;
		result["status_code"] = response->http_status;
		Variant value;
		cserde_status decoded = CSERDE_OK;
		if (response->kind == CRPC_RESPONSE_RESULT) {
			decoded = decode_rpc_value(response->value.result, value);
			result["result"] = value;
		} else {
			const crpc_remote_error &remote = response->value.remote_error;
			Dictionary remote_error;
			remote_error["code"] = remote.code;
			remote_error["message"] = remote.message.size
				? String::utf8(reinterpret_cast<const char *>(remote.message.data), remote.message.size) : String();
			if (remote.data) {
				decoded = decode_rpc_value(remote.data, value);
				remote_error["data"] = value;
			}
			result["remote_error"] = remote_error;
		}
		if (decoded != CSERDE_OK) {
			result = error_result(SALTS_EPROTO, decoded, "godot-rpc-decode");
			result["status_code"] = response->http_status;
		}
	}
	complete(static_cast<Pending *>(user), true, result);
}

struct Headers {
	std::vector<CharString> strings;
	std::vector<chttp_header> entries;

	int parse(const Dictionary &headers) {
		if (headers.size() > MAX_HEADERS) {
			return SALTS_ENOBUFS;
		}
		strings.reserve(headers.size() * 2);
		entries.reserve(headers.size());
		const Array keys = headers.keys();
		for (int64_t i = 0; i < keys.size(); ++i) {
			const Variant key = keys[i];
			const Variant value = headers[key];
			if (key.get_type() != Variant::STRING || value.get_type() != Variant::STRING) {
				return SALTS_EINVAL;
			}
			const CharString name = String(key).utf8();
			const CharString text = String(value).utf8();
			if (static_cast<int64_t>(std::strlen(name.get_data())) != name.length() ||
				static_cast<int64_t>(std::strlen(text.get_data())) != text.length()) {
				return SALTS_EINVAL;
			}
			strings.push_back(name);
			strings.push_back(text);
			entries.push_back({strings[strings.size() - 2].get_data(), strings.back().get_data()});
		}
		return SALTS_OK;
	}
};

bool valid_cstring(const CharString &text) {
	return static_cast<int64_t>(std::strlen(text.get_data())) == text.length();
}

int init_client(chttp_async_client &client) {
	chttp_client_config config{};
#if defined(_WIN32)
	config.network.backend = NATIVE_IO_BACKEND_IOCP;
#elif defined(__APPLE__)
	config.network.backend = NATIVE_IO_BACKEND_KQUEUE;
#else
	config.network.backend = NATIVE_IO_BACKEND_EPOLL;
#endif
	config.network.connection_capacity = 8;
	config.network.command_capacity = 64;
	config.network.request_capacity = 64;
	config.network.completion_batch_capacity = 32;
	config.network.event_capacity = 64;
	config.network.max_send_bytes = MAX_BODY + 32768;
	config.network.receive_buffer_bytes = 65536;
	config.network.connect_timeout_ms = 10000;
	config.network.read_timeout_ms = 60000;
	config.network.write_timeout_ms = 15000;
	config.network.tls_io_buffer_bytes = 65536;
	config.network.tls_handshake_timeout_ms = 10000;
	config.request_capacity = MAX_REQUESTS;
	config.max_start_line_bytes = 8192;
	config.max_header_count = MAX_HEADERS;
	config.max_header_bytes = 16384;
	config.max_request_body_bytes = MAX_BODY;
	config.max_response_body_bytes = MAX_BODY;
	config.max_informational_responses = 4;
	return chttp_async_client_init(&client, &config);
}
} // namespace

class SaltsHttpClient : public Node {
	GDCLASS(SaltsHttpClient, Node)

	std::unique_ptr<HttpState> state = std::make_unique<HttpState>();

	int prepare(int64_t protocol, int64_t timeout_ms, const CharString &uri,
			const CharString &authority, const CharString &target) {
		if (state->closing) {
			return SALTS_ESHUTDOWN;
		}
		if (protocol < CHTTP_HTTP_1_1 || protocol > CHTTP_HTTP_2 || timeout_ms <= 0 ||
			timeout_ms > UINT32_MAX || !valid_cstring(uri) || !valid_cstring(authority) ||
			!valid_cstring(target)) {
			return SALTS_EINVAL;
		}
		if (state->pending.size() + state->ready.size() >= MAX_REQUESTS ||
			state->next_id == std::numeric_limits<int64_t>::max()) {
			return SALTS_ENOBUFS;
		}
		return state->client.impl ? SALTS_OK : init_client(state->client);
	}

	const chttp_tls_profile *tls_for(const String &uri, int64_t protocol) const {
		return uri.begins_with("tls://") && state->tls[protocol].impl ? &state->tls[protocol] : nullptr;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("request", "connection_uri", "authority", "target", "method",
			"protocol", "headers", "body", "timeout_ms"), &SaltsHttpClient::request,
			DEFVAL(GET), DEFVAL(HTTP_1_1), DEFVAL(Dictionary()), DEFVAL(PackedByteArray()), DEFVAL(15000));
		ClassDB::bind_method(D_METHOD("request_rpc", "connection_uri", "authority", "target", "method",
			"params", "protocol", "headers", "timeout_ms"), &SaltsHttpClient::request_rpc,
			DEFVAL(Variant()), DEFVAL(HTTP_2), DEFVAL(Dictionary()), DEFVAL(15000));
		ClassDB::bind_method(D_METHOD("set_ca_file", "path"), &SaltsHttpClient::set_ca_file);
		ClassDB::bind_method(D_METHOD("cancel", "request_id"), &SaltsHttpClient::cancel);
		ClassDB::bind_method(D_METHOD("close"), &SaltsHttpClient::close);
		ADD_SIGNAL(MethodInfo("request_completed", PropertyInfo(Variant::INT, "request_id"),
			PropertyInfo(Variant::DICTIONARY, "response")));
		ADD_SIGNAL(MethodInfo("rpc_completed", PropertyInfo(Variant::INT, "request_id"),
			PropertyInfo(Variant::DICTIONARY, "response")));
		ADD_SIGNAL(MethodInfo("poll_failed", PropertyInfo(Variant::INT, "status")));
		BIND_ENUM_CONSTANT(HTTP_1_1);
		BIND_ENUM_CONSTANT(HTTP_2);
		BIND_ENUM_CONSTANT(GET);
		BIND_ENUM_CONSTANT(HEAD);
		BIND_ENUM_CONSTANT(POST);
		BIND_ENUM_CONSTANT(PUT);
		BIND_ENUM_CONSTANT(DELETE);
		BIND_ENUM_CONSTANT(PATCH);
		BIND_ENUM_CONSTANT(OPTIONS);
	}

public:
	enum Protocol { HTTP_1_1 = CHTTP_HTTP_1_1, HTTP_2 = CHTTP_HTTP_2 };
	enum Method {
		GET = CHTTP_METHOD_GET, HEAD = CHTTP_METHOD_HEAD, POST = CHTTP_METHOD_POST,
		PUT = CHTTP_METHOD_PUT, DELETE = CHTTP_METHOD_DELETE, PATCH = CHTTP_METHOD_PATCH,
		OPTIONS = CHTTP_METHOD_OPTIONS
	};

	int64_t request(const String &uri, const String &authority, const String &target,
			int64_t method, int64_t protocol, const Dictionary &headers, const PackedByteArray &body,
			int64_t timeout_ms) {
		const CharString u = uri.utf8(), a = authority.utf8(), t = target.utf8();
		if (method < GET || method > OPTIONS || body.size() > MAX_BODY) {
			return SALTS_EINVAL;
		}
		Headers converted;
		int status = converted.parse(headers);
		if (status == SALTS_OK) {
			status = prepare(protocol, timeout_ms, u, a, t);
		}
		if (status != SALTS_OK) {
			return status;
		}
		const int64_t id = state->next_id++;
		auto pending = std::make_unique<Pending>();
		pending->owner = state.get();
		pending->id = id;
		chttp_request_options options{};
		options.connection_uri = u.get_data();
		options.authority = a.get_data();
		options.target = t.get_data();
		options.method = static_cast<chttp_method>(method);
		options.headers = converted.entries.data();
		options.header_count = converted.entries.size();
		options.body = body.ptr();
		options.body_size = body.size();
		options.on_complete = http_complete;
		options.user = pending.get();
		options.tls = tls_for(uri, protocol);
		options.protocol = static_cast<chttp_protocol>(protocol);
		const chttp_request_timeouts timeouts{static_cast<uint32_t>(timeout_ms), 0};
		status = chttp_async_client_submit_with_timeouts(&state->client, &options, &timeouts, &pending->handle);
		if (status == SALTS_OK) {
			state->pending.emplace(id, std::move(pending));
			set_process(true);
		}
		return status == SALTS_OK ? id : status;
	}

	int64_t request_rpc(const String &uri, const String &authority, const String &target,
			const String &method, Variant params, int64_t protocol, const Dictionary &headers,
			int64_t timeout_ms) {
		const CharString u = uri.utf8(), a = authority.utf8(), t = target.utf8(), m = method.utf8();
		if (!valid_cstring(m) || (params.get_type() != Variant::NIL &&
			params.get_type() != Variant::ARRAY && params.get_type() != Variant::DICTIONARY)) {
			return SALTS_EINVAL;
		}
		Headers converted;
		int status = converted.parse(headers);
		if (status == SALTS_OK) {
			status = prepare(protocol, timeout_ms, u, a, t);
		}
		if (status != SALTS_OK) {
			return status;
		}
		std::vector<crpc_metadata> metadata;
		metadata.reserve(converted.entries.size());
		for (const auto &header : converted.entries) {
			metadata.push_back({header.name, header.value});
		}
		const int64_t id = state->next_id++;
		auto pending = std::make_unique<Pending>();
		pending->owner = state.get();
		pending->id = id;
		crpc_options options{};
		options.connection_uri = u.get_data();
		options.authority = a.get_data();
		options.target = t.get_data();
		options.method.name = m.get_data();
		options.request_id = id;
		options.metadata = metadata.data();
		options.metadata_count = metadata.size();
		options.deadline_ms = static_cast<uint32_t>(timeout_ms);
		if (params.get_type() != Variant::NIL) {
			options.encode_params = encode_rpc_params;
			options.params_user = &params;
		}
		options.tls = tls_for(uri, protocol);
		options.protocol = static_cast<chttp_protocol>(protocol);
		chttp_client_rpc_limits limits = CHTTP_CLIENT_RPC_LIMITS_INIT;
		limits.max_json_depth = 16;
		status = chttp_async_client_rpc_submit(&state->client, &limits, &options, rpc_complete,
			pending.get(), &pending->handle);
		if (status == SALTS_OK) {
			state->pending.emplace(id, std::move(pending));
			set_process(true);
		}
		return status == SALTS_OK ? id : status;
	}

	int set_ca_file(const String &path) {
		const CharString ca = path.utf8();
		if (!valid_cstring(ca) || state->closing) {
			return SALTS_EINVAL;
		}
		chttp_tls_profile profiles[2]{};
		if (!path.is_empty()) {
			const char *alpn[] = {"http/1.1", "h2"};
			for (int protocol = HTTP_1_1; protocol <= HTTP_2; ++protocol) {
				cnet_tls_client_config config{};
				config.size = sizeof(config);
				config.ca_file = ca.get_data();
				config.alpn_protocols = &alpn[protocol];
				config.alpn_protocol_count = 1;
				const int status = chttp_tls_profile_init(&profiles[protocol], &config);
				if (status != SALTS_OK) {
					chttp_tls_profile_destroy(&profiles[HTTP_1_1]);
					return status;
				}
			}
		}
		for (int protocol = HTTP_1_1; protocol <= HTTP_2; ++protocol) {
			chttp_tls_profile_destroy(&state->tls[protocol]);
			state->tls[protocol] = profiles[protocol];
		}
		return SALTS_OK;
	}

	int cancel(int64_t id) {
		const auto it = state->pending.find(id);
		return it == state->pending.end() ? SALTS_ENOENT :
			chttp_async_request_cancel(&state->client, it->second->handle);
	}

	int close() {
		if (!state->client.impl) {
			return SALTS_OK;
		}
		state->closing = true;
		int status = chttp_async_client_stop(&state->client, SHUTDOWN_TIMEOUT_MS);
		if (status == SALTS_OK) {
			status = chttp_async_client_destroy(&state->client);
		}
		if (status == SALTS_OK) {
			state->closing = false;
		}
		return status;
	}

	void _process(double) override {
		int status = SALTS_OK;
		if (state->client.impl && !state->closing) {
			size_t completions = 0;
			status = chttp_async_client_poll(&state->client, 0, &completions);
		}
		std::vector<Completion> ready;
		ready.swap(state->ready);
		for (const auto &event : ready) {
			emit_signal(event.rpc ? "rpc_completed" : "request_completed", event.id, event.result);
		}
		if (status != SALTS_OK) {
			emit_signal("poll_failed", status);
		}
	}

	~SaltsHttpClient() override {
		int status = close();
		if (status == SALTS_ETIMEDOUT) {
			status = close();
		}
		if (status != SALTS_OK) {
			UtilityFunctions::push_error("SaltsHttpClient shutdown failed: ", status);
			state.release();
			return;
		}
		for (auto &tls : state->tls) {
			chttp_tls_profile_destroy(&tls);
		}
	}
};

VARIANT_ENUM_CAST(SaltsHttpClient::Protocol);
VARIANT_ENUM_CAST(SaltsHttpClient::Method);

extern "C" GDExtensionBool GDE_EXPORT salts_http_client_library_init(
		GDExtensionInterfaceGetProcAddress get_proc_address, GDExtensionClassLibraryPtr library,
		GDExtensionInitialization *initialization) {
	GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
	init.register_initializer([](ModuleInitializationLevel level) {
		if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
			GDREGISTER_CLASS(SaltsHttpClient);
		}
	});
	init.register_terminator([](ModuleInitializationLevel) {});
	init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	return init.init();
}
