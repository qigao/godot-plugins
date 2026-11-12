cmake_minimum_required(VERSION 3.27)
project(SaltsBase VERSION 0.1.3 LANGUAGES C CXX)

set(SALTS_BASE_SOURCE_DIR "" CACHE PATH "SaltsBase v0.1.3 source directory")
set(SALTS_DEPENDENCY_PREFIX "" CACHE PATH "Architecture-matched vcpkg-cache prefix")
list(PREPEND CMAKE_PREFIX_PATH "${SALTS_DEPENDENCY_PREFIX}")
set(SALTSCORE_NATIVE_SOVERSION 0.1)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include("${SALTS_BASE_SOURCE_DIR}/cmake/CmakeUtils.cmake")
find_package(Threads REQUIRED)

add_subdirectory("${SALTS_BASE_SOURCE_DIR}/tinytest" tinytest)
add_subdirectory("${SALTS_BASE_SOURCE_DIR}/core" core)
add_subdirectory("${SALTS_BASE_SOURCE_DIR}/media" media)

set(package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/SaltsBase")
configure_package_config_file("${SALTS_BASE_SOURCE_DIR}/cmake/SaltsBaseConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsBaseConfig.cmake" INSTALL_DESTINATION "${package_dir}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/SaltsBaseConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SaltsBaseConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/SaltsBaseConfigVersion.cmake" DESTINATION "${package_dir}")
install(EXPORT SaltsBaseTargets NAMESPACE Salts:: DESTINATION "${package_dir}")
