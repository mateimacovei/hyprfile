cmake_minimum_required(VERSION 3.10)

project(hyprfile VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

## Allow overriding the build profile from the command line or cache.
## Usage: cmake -S . -B build -DBUILD_PROFILE=Release
if(NOT DEFINED BUILD_PROFILE)
  set(BUILD_PROFILE "Debug" CACHE STRING "Build profile (Debug/Release)")
endif()

## If the generator didn't already set CMAKE_BUILD_TYPE, pick up BUILD_PROFILE.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "${BUILD_PROFILE}" CACHE STRING "Build type" FORCE)
endif()

## Normalize common lowercase inputs to canonical CMake values.
string(TOUPPER "${CMAKE_BUILD_TYPE}" _BUILD_PROFILE_UPPER)
if(_BUILD_PROFILE_UPPER STREQUAL "DEBUG")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
elseif(_BUILD_PROFILE_UPPER STREQUAL "RELEASE")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build profile: ${CMAKE_BUILD_TYPE}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPR REQUIRED IMPORTED_TARGET hyprtoolkit hyprutils pixman-1 aquamarine libdrm)
pkg_check_modules(XKB REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libswscale libavutil)
pkg_check_modules(GDK_PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)

file(GLOB_RECURSE HYPR_EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
# Ensure Parser.cpp is included even if glob patterns behave differently on some CMake versions
list(APPEND HYPR_EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/cli/Parser.cpp")
set(HYPRFILE_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version/Version.hpp.in"
    "${HYPRFILE_GENERATED_INCLUDE_DIR}/version/Version.hpp"
    @ONLY
)
add_executable(hyprfile ${HYPR_EXAMPLE_SOURCES})
target_include_directories(hyprfile PRIVATE "${HYPRFILE_GENERATED_INCLUDE_DIR}")

target_link_libraries(hyprfile PkgConfig::HYPR PkgConfig::XKB PkgConfig::FFMPEG PkgConfig::GDK_PIXBUF)

enable_testing()
add_subdirectory(tests)
