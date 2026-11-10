#include "task.h"

#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"

#if MEDIAPIPE_DISABLE_GPU
#error "GDMP requires a GPU-enabled MediaPipe build"
#endif

void MediaPipeTaskBaseOptions::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_model_asset_buffer"), &MediaPipeTaskBaseOptions::get_model_asset_buffer);
	ClassDB::bind_method(D_METHOD("set_model_asset_buffer"), &MediaPipeTaskBaseOptions::set_model_asset_buffer);
	ClassDB::bind_method(D_METHOD("get_model_asset_path"), &MediaPipeTaskBaseOptions::get_model_asset_path);
	ClassDB::bind_method(D_METHOD("set_model_asset_path"), &MediaPipeTaskBaseOptions::set_model_asset_path);
	ClassDB::bind_method(D_METHOD("get_delegate"), &MediaPipeTaskBaseOptions::get_delegate);
	ClassDB::bind_method(D_METHOD("set_delegate"), &MediaPipeTaskBaseOptions::set_delegate);
	ADD_PROPERTY(PropertyInfo(Variant::Type::PACKED_BYTE_ARRAY, "model_asset_buffer"), "set_model_asset_buffer", "get_model_asset_buffer");
	ADD_PROPERTY(PropertyInfo(Variant::Type::STRING, "model_asset_path"), "set_model_asset_path", "get_model_asset_path");
	ADD_PROPERTY(PropertyInfo(Variant::Type::INT, "delegate"), "set_delegate", "get_delegate");
	BIND_ENUM_CONSTANT(DELEGATE_CPU);
	BIND_ENUM_CONSTANT(DELEGATE_GPU);
	BIND_ENUM_CONSTANT(DELEGATE_EDGETPU_NNAPI);
}

PackedByteArray MediaPipeTaskBaseOptions::get_model_asset_buffer() {
	return model_asset_buffer;
}

void MediaPipeTaskBaseOptions::set_model_asset_buffer(PackedByteArray buffer) {
	model_asset_buffer = buffer;
}

String MediaPipeTaskBaseOptions::get_model_asset_path() {
	return model_asset_path;
}

void MediaPipeTaskBaseOptions::set_model_asset_path(const String &path) {
	model_asset_path = path;
}

MediaPipeTaskBaseOptions::Delegate MediaPipeTaskBaseOptions::get_delegate() {
	return DELEGATE_GPU;
}

void MediaPipeTaskBaseOptions::set_delegate(Delegate delegate) {
	ERR_FAIL_COND_MSG(delegate != DELEGATE_GPU, "GDMP requires the GPU delegate.");
}

std::unique_ptr<BaseOptions> MediaPipeTaskBaseOptions::get_base_options() {
	std::unique_ptr<BaseOptions> base_options = std::make_unique<BaseOptions>();
	if (model_asset_buffer.size())
		base_options->model_asset_buffer = std::make_unique<std::string>((char *)model_asset_buffer.ptr(), model_asset_buffer.size());
	base_options->model_asset_path = model_asset_path.utf8().get_data();
	base_options->delegate = BaseOptions::GPU;
	return std::move(base_options);
}

void MediaPipeTask::_bind_methods() {}

void MediaPipeTask::_register_task() {
	ClassDB::register_class<MediaPipeTaskBaseOptions>();
	ClassDB::register_abstract_class<MediaPipeTask>();
}

void MediaPipeTask::register_tasks() {
	_register_task();
	_register_tasks();
}
