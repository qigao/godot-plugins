# Salts HTTP Client

A Godot 4.7.2 GDExtension backed by CHttp 0.1.4. It supports HTTP/1.1, HTTP/2 and unary JSON-RPC 2.0 over either protocol.

Copy `addons/salts_http_client` from the Salts Network CI artifact into your project's `addons` directory. The extension registers the `SaltsHttpClient` node. Add the node to the scene tree and call it from the main thread. The node advances requests each frame and emits results on that thread.

```gdscript
var http := SaltsHttpClient.new()
add_child(http)
http.request_completed.connect(func(id, response):
    print(id, " ", response.get("status_code"), " ", response.get("body")))
http.rpc_completed.connect(func(id, response):
    print(id, " ", response))

var request_id := http.request("tls://api.example.org:443", "api.example.org", "/status",
    SaltsHttpClient.GET, SaltsHttpClient.HTTP_2)
var rpc_id := http.request_rpc("tls://api.example.org:443", "api.example.org", "/rpc",
    "game.get_state", {"room": "lobby"})
```

| Call | Behavior |
| --- | --- |
| `request(connection_uri, authority, target, method = GET, protocol = HTTP_1_1, headers = {}, body = PackedByteArray(), timeout_ms = 15000)` | Returns a positive request ID on admission or a negative Salts status. |
| `request_rpc(connection_uri, authority, target, method, params = null, protocol = HTTP_2, headers = {}, timeout_ms = 15000)` | Calls one JSON-RPC method. Pass an Array or a Dictionary with String keys for params. |
| `set_ca_file(path)` | Selects a PEM CA file for future TLS requests. Pass an empty path to restore platform trust. Returns a Salts status. |
| `cancel(request_id)` | Requests cancellation of one admitted request. Completion still arrives through its signal. |
| `close()` | Stops and drains the native owner. Observe any terminal signals on the following frame. A nonzero status leaves cleanup pending and allows another call to `close()`. |

Use `tcp://host:port` for cleartext or `tls://host:port` for verified TLS. `authority` supplies HTTP Host or `:authority` and `target` is the origin-form path and query. Cleartext HTTP/2 uses prior knowledge. TLS uses ALPN for the selected protocol. CA paths are filesystem paths readable by the native library.

The node emits `request_completed(id, response)` and `rpc_completed(id, response)`. HTTP success at the transport layer yields `error = 0`, `status_code`, `http_major`, `http_minor`, `headers` and a binary `body`. Response headers are ordered name/value pairs and preserve repeats. An RPC reply yields `status_code` and either `result` or `remote_error` with the RPC code, message and optional data. Transport and decoding failures include `error`, `native_error` and `stage`. Native polling failures emit `poll_failed(status)`.

A node admits up to 16 concurrent operations. HTTP bodies are bounded at 1 MiB and the encoded RPC request at 8 KiB. JSON integers in the signed 64-bit range retain their precision.

## Build from source

Use the same compiler profile across the installed [CHttp 0.1.4](https://github.com/qigao/chttp/releases/tag/v0.1.4), [SaltsBase 0.1.10](https://github.com/qigao/salts-base/releases/tag/v0.1.10), [CMeta 0.1.10](https://github.com/qigao/cmeta/releases/tag/v0.1.10), [SaltsCNet 0.1.18](https://github.com/qigao/salts-cnet/releases/tag/v0.1.18) and [SaltsUtils 0.1.8](https://github.com/qigao/salts-utils/releases/tag/v0.1.8) SDKs. The project builds the two add-ons together using [pinned godot-cpp](https://github.com/godotengine/godot-cpp/tree/507ed9d840c01a3c5b2a39af8bb4000bfac30bf5) and [Flowie 1.0.5 source](https://github.com/qigao/flowie/tree/3cdfbd9ff42668c873a26e5fb1f307870c74ff3a). Use host re2c and a matching Lemon executable and parser template from [vcpkg-cache 0.1.8](https://github.com/qigao/vcpkg-cache/releases/tag/v0.1.8).

```sh
cmake -S src/salts_network -B build-network -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DGODOT_CPP_DIR=/path/to/godot-cpp -DFLOWIE_SOURCE_DIR=/path/to/flowie \
  -DSALTS_BASE_ROOT=/sdk/salts-base \
  '-DCMAKE_PREFIX_PATH=/sdk/cmeta;/sdk/salts-cnet;/sdk/salts-utils;/sdk/chttp' \
  -DRE2C_EXECUTABLE=/tools/re2c/bin/re2c \
  -DLEMON_EXECUTABLE=/tools/lemon/bin/lemon -DLEMPAR=/tools/lemon/share/lemon/lempar.c
cmake --build build-network
```

The build stages `build-network/addons`. For desktop loopback tests, configure with `-DBUILD_TESTING=ON -DGODOT_EXECUTABLE=/path/to/godot` and run `ctest --test-dir build-network --output-on-failure`.

Use LLVM-MinGW on Windows and Android NDK r27d with API 26 for Android. Enable the Internet permission in the Godot Android export preset. The current Android ARM64 CHttp SDK has 4 KiB ELF load alignment. A 16 KiB-page Android export requires a compatible upstream SDK.
