#include "salts_camera_device.hpp"
#include "salts_camera_feed.hpp"
#include "salts_camera_frame.hpp"
#include "salts_camera_manager.hpp"
#include "salts_playback.hpp"

#ifdef SALTS_CAMERA_TEST_FIXTURE
#include "preview_fixture.hpp"
#endif

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

namespace {

void initialize(godot::ModuleInitializationLevel level) {
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(salts_camera::SaltsCameraFrame);
        GDREGISTER_CLASS(salts_camera::SaltsCameraFeed);
        GDREGISTER_CLASS(salts_camera::SaltsCameraDevice);
        GDREGISTER_CLASS(salts_camera::SaltsCameraManager);
        GDREGISTER_CLASS(salts_camera::SaltsPlayback);
#ifdef SALTS_CAMERA_TEST_FIXTURE
        GDREGISTER_CLASS(salts_camera::SaltsCameraPreviewFixture);
#endif
    }
}

void uninitialize(godot::ModuleInitializationLevel) {}

} // namespace

extern "C" GDExtensionBool GDE_EXPORT salts_camera_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address, GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize);
    init.register_terminator(uninitialize);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
