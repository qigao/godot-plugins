cmake_minimum_required(VERSION 3.27)
project(SaltsNetwork LANGUAGES C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
include(GNUInstallDirs)

set(GODOT_CPP_DIR "" CACHE PATH "Pinned godot-cpp source checkout")
set(FLOWIE_SOURCE_DIR "" CACHE PATH "Pinned Flowie source checkout")
set(SALTS_BASE_ROOT "" CACHE PATH "Installed SaltsBase SDK")
set(RE2C_EXECUTABLE "" CACHE FILEPATH "Host re2c executable")
set(LEMON_EXECUTABLE "" CACHE FILEPATH "Host Lemon executable")
set(LEMPAR "" CACHE FILEPATH "Matching Lemon parser template")

# The CMeta SDK config reads this dependency root from the environment.
set(ENV{SALTS_BASE_ROOT} "${SALTS_BASE_ROOT}")
find_package(SaltsBase 0.1.10 EXACT CONFIG REQUIRED PATHS "${SALTS_BASE_ROOT}"
  NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_package(CMeta 0.1.10 EXACT CONFIG REQUIRED NO_CMAKE_FIND_ROOT_PATH)
find_package(SaltsCNet 0.1.18 EXACT CONFIG REQUIRED NO_CMAKE_FIND_ROOT_PATH)
find_package(SaltsUtils 0.1.8 EXACT CONFIG REQUIRED NO_CMAKE_FIND_ROOT_PATH)
find_package(Chttp 0.1.4 EXACT CONFIG REQUIRED COMPONENTS Client NO_CMAKE_FIND_ROOT_PATH)

set(network_cpu "${CMAKE_SYSTEM_PROCESSOR}")
if(ANDROID)
  set(network_platform android)
  set(network_cpu "${CMAKE_ANDROID_ARCH_ABI}")
elseif(APPLE)
  set(network_platform macos)
  if(CMAKE_OSX_ARCHITECTURES)
    set(network_cpu "${CMAKE_OSX_ARCHITECTURES}")
  endif()
elseif(WIN32 AND MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(network_platform windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(network_platform linux)
else()
  message(FATAL_ERROR "Salts Network targets Linux, macOS, Android and Windows with LLVM-MinGW")
endif()

string(TOLOWER "${network_cpu}" network_cpu)
if(network_cpu MATCHES "^(arm64|aarch64|arm64-v8a)$")
  set(network_arch arm64)
elseif(network_cpu MATCHES "^(x86_64|amd64)$")
  set(network_arch x86_64)
else()
  message(FATAL_ERROR "Salts Network requires one x64 or ARM64 target")
endif()

set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp-profile.json")
set(GODOTCPP_API_VERSION 4.7)
set(GODOTCPP_TARGET template_release)
set(GODOTCPP_SYSTEM_HEADERS ON)
add_subdirectory("${GODOT_CPP_DIR}" godot-cpp EXCLUDE_FROM_ALL)

# Use Flowie's host grammar generation, protocol and vendored Monocypher targets.
# Build its client source without configuring the broker or Windows Workbench.
option(BUILD_TESTING "Run the headless networking tests" OFF)
function(add_flowie_client)
  set(BUILD_TESTING OFF)
  set(LEMON_DEPENDS "${LEMON_EXECUTABLE}" "${LEMPAR}")
  if(NOT RE2C_EXECUTABLE OR NOT LEMON_EXECUTABLE OR NOT LEMPAR)
    message(FATAL_ERROR "Set RE2C_EXECUTABLE, LEMON_EXECUTABLE and LEMPAR for the build host")
  endif()
  include("${FLOWIE_SOURCE_DIR}/cmake/CmakeUtils.cmake")
  add_subdirectory("${FLOWIE_SOURCE_DIR}/flowie/protocol" flowie-protocol EXCLUDE_FROM_ALL)
  add_subdirectory("${FLOWIE_SOURCE_DIR}/vendor/monocypher" monocypher EXCLUDE_FROM_ALL)
  add_library(flowie_client SHARED "${FLOWIE_SOURCE_DIR}/flowie/client/src/flowie_mqtt_client.c")
  target_compile_definitions(flowie_client PRIVATE FLOWIE_MQTT_CLIENT_BUILD)
  target_include_directories(flowie_client
    PUBLIC "${FLOWIE_SOURCE_DIR}/flowie/client/include"
          "${FLOWIE_SOURCE_DIR}/flowie/protocol/include"
    PRIVATE "${FLOWIE_SOURCE_DIR}/flowie/detail")
  target_link_libraries(flowie_client PRIVATE Flowie::Protocol Salts::Core CMeta::CSTL
    Salts::CNet CHttp::Client monocypher)
  set_target_properties(flowie_client PROPERTIES C_VISIBILITY_PRESET hidden)
endfunction()
add_flowie_client()

set(addons_source "${CMAKE_CURRENT_SOURCE_DIR}/../../addons")
set(addons "${CMAKE_CURRENT_BINARY_DIR}/addons")
set(platform_dir "bin/${network_platform}/${network_arch}")
# Both extensions and their common runtimes use the same loader directory.
set(runtime_relative "addons/salts_http_client/${platform_dir}")
set(runtime_dir "${CMAKE_CURRENT_BINARY_DIR}/${runtime_relative}")
set(common_runtime CHttp::Client Salts::CNet Salts::Crypto Salts::DataBind Salts::Core)
set(runtime_files "")
set(common_dependencies "")
foreach(target IN LISTS common_runtime)
  list(APPEND runtime_files "$<TARGET_FILE:${target}>")
  list(APPEND common_dependencies "\"res://${runtime_relative}/$<TARGET_FILE_NAME:${target}>\": \"\"")
endforeach()

foreach(package IN ITEMS Chttp SaltsCNet)
  file(COPY "${${package}_DIR}/../../../share/licenses/" DESTINATION "${addons}/salts_http_client/licenses")
endforeach()
file(COPY "${SALTS_BASE_ROOT}/share/licenses/" DESTINATION "${addons}/salts_http_client/licenses")

if(WIN32)
  get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  if(network_arch STREQUAL "arm64")
    set(windows_triplet aarch64-w64-mingw32)
  else()
    set(windows_triplet x86_64-w64-mingw32)
  endif()
  foreach(dll IN ITEMS libc++.dll libunwind.dll)
    find_file(runtime_dll NAMES "${dll}" NO_CACHE REQUIRED NO_DEFAULT_PATH
      PATHS "${compiler_bin}/../${windows_triplet}/bin" "${compiler_bin}")
    list(APPEND runtime_files "${runtime_dll}")
    list(APPEND common_dependencies "\"res://${runtime_relative}/${dll}\": \"\"")
    unset(runtime_dll)
  endforeach()
elseif(ANDROID AND ANDROID_STL STREQUAL "c++_shared")
  set(android_cxx "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/libc++_shared.so")
  list(APPEND runtime_files "${android_cxx}")
  list(APPEND common_dependencies "\"res://${runtime_relative}/libc++_shared.so\": \"\"")
endif()
list(JOIN common_dependencies ", " http_dependencies)

function(set_network_output target)
  set_target_properties(${target} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${runtime_dir}/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${runtime_dir}/$<0:>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/import-libraries/$<0:>"
    BUILD_WITH_INSTALL_RPATH ON
    INSTALL_RPATH "$<IF:$<PLATFORM_ID:Darwin>,@loader_path,$ORIGIN>"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
  if(WIN32)
    set_target_properties(${target} PROPERTIES PREFIX "")
  elseif(NOT APPLE)
    # Linux SDKs omit RUNPATH. DT_RPATH lets the loader resolve transitive
    # dependencies in the add-on directory as well as the extension's imports.
    target_link_options(${target} PRIVATE "LINKER:--exclude-libs,ALL" "LINKER:-z,defs"
      "LINKER:--disable-new-dtags")
  endif()
  if(ANDROID)
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "")
    target_link_options(${target} PRIVATE "LINKER:-z,max-page-size=16384" "LINKER:-z,common-page-size=16384")
  endif()
endfunction()

set_network_output(flowie_client)
add_library(salts_http_client SHARED salts_http_client.cpp rpc_variant.cpp)
target_link_libraries(salts_http_client PRIVATE godot::cpp CHttp::Client)
add_library(salts_mqtt_client SHARED salts_mqtt_client.cpp)
target_link_libraries(salts_mqtt_client PRIVATE godot::cpp flowie_client Salts::Core)

foreach(addon IN ITEMS salts_http_client salts_mqtt_client)
  target_compile_features(${addon} PRIVATE cxx_std_17)
  set_network_output(${addon})
  if(APPLE)
    target_link_options(${addon} PRIVATE "LINKER:-exported_symbol,_${addon}_library_init")
  endif()
  if(addon STREQUAL "salts_http_client")
    set(dependencies "${http_dependencies}")
  else()
    set(dependencies "${http_dependencies}, \"res://${runtime_relative}/$<TARGET_FILE_NAME:flowie_client>\": \"\"")
  endif()
  configure_file("${addons_source}/${addon}/README.md" "${addons}/${addon}/README.md" COPYONLY)
  file(GENERATE OUTPUT "${addons}/${addon}/${addon}.gdextension" CONTENT
"[configuration]
entry_symbol = \"${addon}_library_init\"
compatibility_minimum = \"4.7.2\"
compatibility_maximum = \"4.7.2\"
reloadable = false

[libraries]
${network_platform}.${network_arch} = \"res://${runtime_relative}/$<TARGET_FILE_NAME:${addon}>\"

[dependencies]
${network_platform}.${network_arch} = {${dependencies}}
")
endforeach()

add_custom_target(stage_network_runtime ALL
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${runtime_dir}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_files} "${runtime_dir}"
  VERBATIM)
configure_file("${GODOT_CPP_DIR}/LICENSE.md" "${addons}/salts_http_client/LICENSE.godot-cpp" COPYONLY)
configure_file("${FLOWIE_SOURCE_DIR}/LICENSE" "${addons}/salts_mqtt_client/LICENSE.Flowie" COPYONLY)

if(BUILD_TESTING)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(GODOT_EXECUTABLE "" CACHE FILEPATH "Godot 4.7.2 executable for headless tests")
  set(tests "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/salts_network")
  configure_file("${tests}/project.godot" project.godot COPYONLY)
  configure_file("${tests}/network_tests.gd" network_tests.gd COPYONLY)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/.godot/extension_list.cfg"
    "res://addons/salts_http_client/salts_http_client.gdextension\nres://addons/salts_mqtt_client/salts_mqtt_client.gdextension\n")
  add_test(NAME salts_network.godot COMMAND "${Python3_EXECUTABLE}" "${tests}/run_network_tests.py"
    "${GODOT_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(salts_network.godot PROPERTIES TIMEOUT 45)
endif()
