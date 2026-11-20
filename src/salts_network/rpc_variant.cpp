#include "rpc_variant.h"
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <cmath>
#include <limits>

using namespace godot;

namespace {
constexpr unsigned MAX_DEPTH = 16;

cserde_status write_string(cserde_writer *writer, const String &value) {
	const CharString utf8 = value.utf8();
	cserde_token token{};
	token.kind = CSERDE_STRING;
	token.value.slice = {reinterpret_cast<const unsigned char *>(utf8.get_data()),
		static_cast<size_t>(utf8.length()), CSERDE_VIEW_TRANSIENT};
	return cserde_writer_write(writer, &token);
}

cserde_status write_value(cserde_writer *writer, const Variant &value, unsigned depth) {
	if (depth > MAX_DEPTH) {
		return CSERDE_LIMIT_EXCEEDED;
	}
	cserde_token token{};
	switch (value.get_type()) {
		case Variant::NIL:
			token.kind = CSERDE_NULL;
			break;
		case Variant::BOOL:
			token.kind = CSERDE_BOOL;
			token.value.boolean = value;
			break;
		case Variant::INT:
			token.kind = CSERDE_SINT;
			token.value.sint = value;
			break;
		case Variant::FLOAT:
			token.kind = CSERDE_FLOAT;
			token.value.floating = value;
			if (!std::isfinite(token.value.floating)) {
				return CSERDE_VALUE_OUT_OF_RANGE;
			}
			break;
		case Variant::STRING:
			return write_string(writer, value);
		case Variant::ARRAY: {
			const Array array = value;
			token.kind = CSERDE_ARRAY_BEGIN;
			cserde_status status = cserde_writer_write(writer, &token);
			for (int64_t i = 0; i < array.size() && status == CSERDE_OK; ++i) {
				status = write_value(writer, array[i], depth + 1);
			}
			token.kind = CSERDE_ARRAY_END;
			return status == CSERDE_OK ? cserde_writer_write(writer, &token) : status;
		}
		case Variant::DICTIONARY: {
			const Dictionary map = value;
			const Array keys = map.keys();
			token.kind = CSERDE_MAP_BEGIN;
			cserde_status status = cserde_writer_write(writer, &token);
			for (int64_t i = 0; i < keys.size() && status == CSERDE_OK; ++i) {
				const Variant key = keys[i];
				if (key.get_type() != Variant::STRING) {
					return CSERDE_INVALID_ARGUMENT;
				}
				status = write_string(writer, key);
				if (status == CSERDE_OK) {
					status = write_value(writer, map[key], depth + 1);
				}
			}
			token.kind = CSERDE_MAP_END;
			return status == CSERDE_OK ? cserde_writer_write(writer, &token) : status;
		}
		default:
			return CSERDE_UNSUPPORTED;
	}
	return cserde_writer_write(writer, &token);
}

String read_string(const cserde_slice &slice) {
	return slice.size ? String::utf8(reinterpret_cast<const char *>(slice.data), slice.size) : String();
}

cserde_status read_value(cserde_reader *reader, const cserde_token &token, Variant &value, unsigned depth) {
	if (depth > MAX_DEPTH) {
		return CSERDE_LIMIT_EXCEEDED;
	}
	switch (token.kind) {
		case CSERDE_NULL:
			value = Variant();
			return CSERDE_OK;
		case CSERDE_BOOL:
			value = token.value.boolean;
			return CSERDE_OK;
		case CSERDE_SINT:
			value = token.value.sint;
			return CSERDE_OK;
		case CSERDE_UINT:
			if (token.value.uint > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
				return CSERDE_VALUE_OUT_OF_RANGE;
			}
			value = static_cast<int64_t>(token.value.uint);
			return CSERDE_OK;
		case CSERDE_FLOAT:
			value = token.value.floating;
			return CSERDE_OK;
		case CSERDE_STRING:
			value = read_string(token.value.slice);
			return CSERDE_OK;
		case CSERDE_ARRAY_BEGIN: {
			Array array;
			cserde_token item{};
			cserde_status status;
			while ((status = cserde_reader_next(reader, &item)) == CSERDE_OK) {
				if (item.kind == CSERDE_ARRAY_END) {
					value = array;
					return CSERDE_OK;
				}
				Variant element;
				status = read_value(reader, item, element, depth + 1);
				if (status != CSERDE_OK) {
					return status;
				}
				array.push_back(element);
			}
			return status;
		}
		case CSERDE_MAP_BEGIN: {
			Dictionary map;
			cserde_token item{};
			cserde_status status;
			while ((status = cserde_reader_next(reader, &item)) == CSERDE_OK) {
				if (item.kind == CSERDE_MAP_END) {
					value = map;
					return CSERDE_OK;
				}
				if (item.kind != CSERDE_STRING) {
					return CSERDE_INVALID_TOKEN;
				}
				const String key = read_string(item.value.slice);
				status = cserde_reader_next(reader, &item);
				if (status != CSERDE_OK) {
					return status;
				}
				Variant member;
				status = read_value(reader, item, member, depth + 1);
				if (status != CSERDE_OK) {
					return status;
				}
				map[key] = member;
			}
			return status;
		}
		default:
			return CSERDE_INVALID_TOKEN;
	}
}
} // namespace

cserde_status encode_rpc_params(void *user, cserde_writer *writer) {
	return write_value(writer, *static_cast<const Variant *>(user), 0);
}

cserde_status decode_rpc_value(cserde_reader *reader, Variant &value) {
	cserde_token token{};
	const cserde_status status = cserde_reader_next(reader, &token);
	return status == CSERDE_OK ? read_value(reader, token, value, 0) : status;
}
