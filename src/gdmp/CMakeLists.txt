cmake_minimum_required(VERSION 3.27)
project(GDMP LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "The MediaPipe SDK build currently targets Linux")
endif()

find_package(MediaPipe 0.10.26 EXACT CONFIG REQUIRED)
if(NOT MediaPipe_GPU_ENABLED)
  message(FATAL_ERROR "GDMP requires a GPU-enabled MediaPipe SDK")
endif()
set(GODOT_CPP_DIR "" CACHE PATH "Godot C++ bindings at e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77")
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp-profile.json")
set(GODOTCPP_TARGET template_release)
set(GODOTCPP_SYSTEM_HEADERS ON)
add_subdirectory("${GODOT_CPP_DIR}" godot-cpp EXCLUDE_FROM_ALL)
set_property(SOURCE "${GODOT_CPP_DIR}/src/godot.cpp" TARGET_DIRECTORY godot-cpp
  APPEND PROPERTY COMPILE_OPTIONS -include cstdlib)

file(GLOB_RECURSE gdmp_sources CONFIGURE_DEPENDS
  GDMP/framework/*.cc
  GDMP/gpu/*.cc
  GDMP/proto/*.cc
  GDMP/tasks/*.cc
  GDMP/util/*.cc)
add_library(gdmp SHARED
  GDMP/gdlibrary.cc
  GDMP/io/camera_helper.cc
  GDMP/io/camera_helper_stub.cc
  ${gdmp_sources})
target_compile_features(gdmp PRIVATE cxx_std_17)
target_include_directories(gdmp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gdmp PRIVATE MediaPipe::Tasks godot::cpp)
set(export_map "${CMAKE_CURRENT_SOURCE_DIR}/GDMP/exports.lds")
target_link_options(gdmp PRIVATE "LINKER:--version-script=${export_map}" "LINKER:-z,defs")
set_property(TARGET gdmp APPEND PROPERTY LINK_DEPENDS "${export_map}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(gdmp_arch arm64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set(gdmp_arch x86_64)
else()
  message(FATAL_ERROR "Unsupported Linux GDMP architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(addon "${CMAKE_CURRENT_BINARY_DIR}/addons/GDMP")
set_target_properties(gdmp PROPERTIES
  OUTPUT_NAME GDMP.linux
  LIBRARY_OUTPUT_DIRECTORY "${addon}/libs/${gdmp_arch}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../../addons/GDMP/" DESTINATION "${addon}")
get_filename_component(mediapipe_sdk "${MediaPipe_DIR}/../../.." ABSOLUTE)
file(COPY "${mediapipe_sdk}/licenses" DESTINATION "${addon}")
configure_file(LICENSE "${addon}/LICENSE" COPYONLY)
configure_file("${GODOT_CPP_DIR}/LICENSE.md" "${addon}/LICENSE.godot-cpp" COPYONLY)
