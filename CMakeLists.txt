cmake_minimum_required(VERSION 3.27)
project(SaltsCamera LANGUAGES C CXX)

find_package(SaltsBase 0.1.2 EXACT CONFIG REQUIRED)
set(GODOT_CPP_DIR "" CACHE PATH "godot-cpp checkout at e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77")
if(NOT EXISTS "${GODOT_CPP_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Set GODOT_CPP_DIR to the pinned godot-cpp source directory")
endif()
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/godot-cpp-profile.json")
set(GODOTCPP_TARGET template_release)
set(GODOTCPP_SYSTEM_HEADERS ON)
add_subdirectory("${GODOT_CPP_DIR}" godot-cpp EXCLUDE_FROM_ALL)
# The pinned bindings use free/realloc without their header in this source.
set_property(SOURCE "${GODOT_CPP_DIR}/src/godot.cpp" TARGET_DIRECTORY godot-cpp
  APPEND PROPERTY COMPILE_OPTIONS -include cstdlib)

add_library(salts_camera_frames STATIC
  src/salts_camera/frame_layout.cpp
  src/salts_camera/frame_mailbox.cpp)
set_target_properties(salts_camera_frames PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(salts_camera_frames PUBLIC cxx_std_17)
target_include_directories(salts_camera_frames PUBLIC src/salts_camera)
target_link_libraries(salts_camera_frames PRIVATE Salts::Media)
if(NOT APPLE)
  target_sources(salts_camera_frames PRIVATE src/salts_camera/frame_jpeg.c)
  target_link_libraries(salts_camera_frames PRIVATE yuv JPEG::JPEG)
endif()

set(camera_sources
  src/salts_camera/register_types.cpp
  src/salts_camera/salts_camera_device.cpp
  src/salts_camera/salts_camera_feed.cpp
  src/salts_camera/salts_camera_frame.cpp
  src/salts_camera/salts_camera_manager.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  set(camera_platform linux)
  set(camera_arch x86_64)
  set(camera_library libsalts_camera.so)
  set(core_library libsalts.so.0.1)
  set(camera_rpath "$ORIGIN")
  set(camera_link_options "LINKER:--exclude-libs,ALL" "LINKER:-z,defs")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  set(camera_platform windows)
  set(camera_arch x86_64)
  set(camera_library salts_camera.dll)
  set(core_library libsalts.dll)
  find_package(libjpeg-turbo CONFIG REQUIRED)
  get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(windows_runtime "$<TARGET_FILE:yuv>" "$<TARGET_FILE:libjpeg-turbo::jpeg>")
  foreach(dll IN ITEMS libc++.dll libunwind.dll)
    find_file(runtime_dll NAMES "${dll}" NO_CACHE REQUIRED NO_DEFAULT_PATH
      PATHS "${compiler_bin}/../x86_64-w64-mingw32/bin" "${compiler_bin}")
    list(APPEND windows_runtime "${runtime_dll}")
    unset(runtime_dll)
  endforeach()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
  enable_language(OBJCXX)
  list(APPEND camera_sources src/salts_camera/permission_macos.mm)
  set(camera_platform macos)
  set(camera_arch arm64)
  set(camera_library libsalts_camera.dylib)
  set(core_library libsalts.0.1.dylib)
  set(camera_rpath "@loader_path")
  set(camera_link_options "LINKER:-exported_symbol,_salts_camera_library_init")
elseif(ANDROID AND CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
  set(camera_platform android)
  set(camera_arch arm64)
  set(camera_library libsalts_camera.so)
  set(core_library libsalts.so)
  set(camera_link_options "LINKER:--exclude-libs,ALL" "LINKER:-z,defs")
else()
  message(FATAL_ERROR "Salts Camera targets Linux x64, Windows x64 llvm-mingw, macOS ARM64 and Android ARM64")
endif()

set(bin_relative "bin/${camera_platform}/${camera_arch}")
set(camera_dependencies "\"res://addons/salts_camera/${bin_relative}/${core_library}\": \"\"")
if(WIN32)
  foreach(dll IN ITEMS libyuv.dll libjpeg-62.dll libc++.dll libunwind.dll)
    string(APPEND camera_dependencies ", \"res://addons/salts_camera/${bin_relative}/${dll}\": \"\"")
  endforeach()
endif()

function(add_camera_library target project_directory)
  add_library(${target} SHARED ${camera_sources})
  target_link_libraries(${target} PRIVATE Salts::Media salts_camera_frames godot::cpp)
  target_link_options(${target} PRIVATE ${camera_link_options})
  set(addon "${project_directory}/addons/salts_camera")
  set(bin "${addon}/${bin_relative}")
  configure_file(addons/salts_camera/salts_camera.gdextension.in "${addon}/salts_camera.gdextension" @ONLY)
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME salts_camera
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    OBJCXX_STANDARD 17
    OBJCXX_STANDARD_REQUIRED ON
    LIBRARY_OUTPUT_DIRECTORY "${bin}/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${bin}/$<0:>"
    ARCHIVE_OUTPUT_DIRECTORY "${project_directory}/import-libraries/$<0:>"
    INSTALL_RPATH "${camera_rpath}"
    BUILD_WITH_INSTALL_RPATH ON)
  if(WIN32)
    set_target_properties(${target} PROPERTIES PREFIX "")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${windows_runtime} "${bin}"
      VERBATIM)
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Salts::Core>" "${bin}/${core_library}"
    VERBATIM)
endfunction()

add_camera_library(salts_camera "${CMAKE_CURRENT_BINARY_DIR}")

option(BUILD_TESTING "Build the Salts Camera native and Godot tests" OFF)
include(CTest)
if(BUILD_TESTING)
  set(GODOT_EXECUTABLE "" CACHE FILEPATH "Godot 4.5.2 executable for headless tests")
  if(NOT EXISTS "${GODOT_EXECUTABLE}")
    message(FATAL_ERROR "Set GODOT_EXECUTABLE to the Godot 4.5.2 executable")
  endif()
  add_executable(salts_camera_native_tests tests/native/frame_tests.cpp)
  target_link_libraries(salts_camera_native_tests PRIVATE salts_camera_frames Salts::Media Salts::TinyTest)
  if(NOT APPLE)
    target_link_libraries(salts_camera_native_tests PRIVATE JPEG::JPEG)
  endif()
  add_test(NAME salts_camera.native COMMAND salts_camera_native_tests)
  configure_file(tests/godot/project.godot project.godot COPYONLY)
  configure_file(tests/godot/camera_smoke.gd camera_smoke.gd COPYONLY)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/.godot/extension_list.cfg" "res://addons/salts_camera/salts_camera.gdextension\n")
  add_test(NAME salts_camera.godot
    COMMAND "${GODOT_EXECUTABLE}" --headless --path "${CMAKE_CURRENT_BINARY_DIR}" --script res://camera_smoke.gd)

  set(preview_project "${CMAKE_CURRENT_BINARY_DIR}/test-preview")
  add_camera_library(salts_camera_preview_fixture "${preview_project}")
  target_sources(salts_camera_preview_fixture PRIVATE tests/godot/preview_fixture.cpp)
  target_include_directories(salts_camera_preview_fixture PRIVATE tests/godot)
  target_compile_definitions(salts_camera_preview_fixture PRIVATE SALTS_CAMERA_TEST_FIXTURE)
  configure_file(tests/godot/project.godot "${preview_project}/project.godot" COPYONLY)
  configure_file(tests/godot/camera_preview.gd "${preview_project}/camera_preview.gd" COPYONLY)
  file(WRITE "${preview_project}/.godot/extension_list.cfg" "res://addons/salts_camera/salts_camera.gdextension\n")
  add_test(NAME salts_camera.preview
    COMMAND "${GODOT_EXECUTABLE}" --headless --path "${preview_project}" --script res://camera_preview.gd)
  set_tests_properties(salts_camera.native salts_camera.godot salts_camera.preview PROPERTIES TIMEOUT 30)
  if(WIN32)
    set_tests_properties(salts_camera.native salts_camera.godot PROPERTIES
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_CURRENT_BINARY_DIR}/addons/salts_camera/${bin_relative}")
    set_tests_properties(salts_camera.preview PROPERTIES
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${preview_project}/addons/salts_camera/${bin_relative}")
  endif()
endif()
