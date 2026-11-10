#include "salts_mediapipe_landmarker.hpp"

#include <godot_cpp/godot.hpp>

namespace {

void initialize(godot::ModuleInitializationLevel level) {
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(salts_mediapipe::SaltsMediaPipeLandmarker);
    }
}

void uninitialize(godot::ModuleInitializationLevel level) {
    (void)level;
}

} // namespace

extern "C" GDExtensionBool GDE_EXPORT salts_mediapipe_init(
    GDExtensionInterfaceGetProcAddress get_proc_address, GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(uninitialize);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
