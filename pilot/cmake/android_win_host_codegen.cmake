# Windows host must run rexglue codegen when cross-compiling Android (device cannot execute).
if(ANDROID AND CMAKE_HOST_WIN32)
  set(_SONIC4_HOST_REXGLUE "${REXSDK_DIR}/out/win-amd64/Release/rexglue.exe")
  if(NOT EXISTS "${_SONIC4_HOST_REXGLUE}")
    message(FATAL_ERROR
      "Android cross-build on Windows requires host rexglue at ${_SONIC4_HOST_REXGLUE}")
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/sources.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/generated/default/sources.cmake")
    set(_SONIC4_CODEGEN_OUTPUTS
      "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.build.stamp"
      ${GENERATED_SOURCES})
    add_custom_command(
      OUTPUT ${_SONIC4_CODEGEN_OUTPUTS}
      COMMAND "${_SONIC4_HOST_REXGLUE}" codegen ${CMAKE_CURRENT_SOURCE_DIR}/sonic4_manifest.toml
      DEPFILE "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/codegen.d"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Generating recompiled code for sonic4 (host rexglue)"
      VERBATIM
    )
  endif()
endif()