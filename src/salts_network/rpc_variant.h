#pragma once

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <http_client/rpc.h>

cserde_status encode_rpc_params(void *user, cserde_writer *writer);
cserde_status decode_rpc_value(cserde_reader *reader, godot::Variant &value);
