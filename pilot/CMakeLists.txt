# sonic4 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(sonic4 LANGUAGES CXX)

# The NDK toolchain can leave WIN32 set on a Windows host. The parent project
# must clear it too: add_subdirectory does not propagate the SDK's override.
if(ANDROID)
    set(WIN32 FALSE)
    set(UNIX TRUE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cross-compiling Android on Windows must run host rexglue.exe (generated/rexglue.cmake
# defaults to $<TARGET_FILE:rex::rexglue>, which is the Android ELF).
if(ANDROID AND CMAKE_HOST_WIN32)
    if(NOT REXSDK_DIR)
        message(FATAL_ERROR "REXSDK_DIR must be set for Android cross-builds on Windows")
    endif()
    set(_SONIC4_HOST_REXGLUE "${REXSDK_DIR}/out/win-amd64/Release/rexglue.exe")
    file(TO_CMAKE_PATH "${_SONIC4_HOST_REXGLUE}" _SONIC4_HOST_REXGLUE)
    if(NOT EXISTS "${_SONIC4_HOST_REXGLUE}")
        message(FATAL_ERROR
            "Android cross-build on Windows requires host rexglue at ${_SONIC4_HOST_REXGLUE}")
    endif()
    set(_sonic4_rexglue_cmake "${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake")
    file(READ "${_sonic4_rexglue_cmake}" _sonic4_rexglue_content)
    if(_sonic4_rexglue_content MATCHES "COMMAND \\$<TARGET_FILE:rex::rexglue> codegen")
        string(REPLACE
            "add_custom_command("
            "# Patched by pilot/CMakeLists.txt for Windows host Android codegen\nif(ANDROID AND CMAKE_HOST_WIN32)\n    set(_SONIC4_CODEGEN_TOOL \"${_SONIC4_HOST_REXGLUE}\")\nelse()\n    set(_SONIC4_CODEGEN_TOOL $<TARGET_FILE:rex::rexglue>)\nendif()\nadd_custom_command("
            _sonic4_rexglue_patched "${_sonic4_rexglue_content}")
        string(REPLACE
            "COMMAND $<TARGET_FILE:rex::rexglue> codegen"
            "COMMAND \"\${_SONIC4_CODEGEN_TOOL}\" codegen"
            _sonic4_rexglue_patched "${_sonic4_rexglue_patched}")
        file(WRITE "${_sonic4_rexglue_cmake}" "${_sonic4_rexglue_patched}")
    endif()
endif()

include(generated/rexglue.cmake)

# Hot-path PROBE hooks (atomics on every scene dispatch, etc.) — diag / Windows only.
set(_rex_timing_probes_default OFF)
if(WIN32 AND NOT ANDROID)
  set(_rex_timing_probes_default ON)
endif()
option(REX_TIMING_PROBES "Sonic4 full timing hooks (high overhead on Android)"
       ${_rex_timing_probes_default})
option(REX_RENDER_TELEMETRY "Vulkan frame_stats counters + PROBE RENDER_VK in retail hooks" OFF)
if(REX_DIAG_PROFILE)
  set(REX_TIMING_PROBES ON CACHE BOOL "Enabled with diagnostic profile" FORCE)
endif()
if(REX_RENDER_TELEMETRY AND REX_DIAG_PROFILE)
  message(FATAL_ERROR "Use only one of REX_DIAG_PROFILE or REX_RENDER_TELEMETRY")
endif()

# Sources
set(SONIC4_SOURCES src/main.cpp)
if(REX_TIMING_PROBES)
  list(APPEND SONIC4_SOURCES src/sonic4_timing_hooks.cpp)
else()
  list(APPEND SONIC4_SOURCES src/sonic4_timing_hooks_retail.cpp)
endif()
if(ANDROID)
    list(APPEND SONIC4_SOURCES src/sonic4_android_pad.cpp)
endif()

if(ANDROID)
    add_library(sonic4 SHARED ${SONIC4_SOURCES})
    if(REX_DIAG_PROFILE)
        target_compile_definitions(sonic4 PRIVATE REX_DIAG_PROFILE=1)
    endif()
    if(REX_RENDER_TELEMETRY)
        target_compile_definitions(sonic4 PRIVATE REX_RENDER_TELEMETRY=1 REX_FRAME_STATS=1)
    endif()
    set_target_properties(sonic4 PROPERTIES OUTPUT_NAME "main")
    # SDL's JNI entry points live in the static library and nothing in C++
    # references them. Keep JNI_OnLoad so the activity can call into SDL.
    target_link_options(sonic4 PRIVATE "-Wl,-u,JNI_OnLoad")
elseif(WIN32)
    add_executable(sonic4 WIN32 ${SONIC4_SOURCES})
else()
    add_executable(sonic4 ${SONIC4_SOURCES})
endif()

rexglue_setup_target(sonic4 GPU_PLUGINS xenos)
