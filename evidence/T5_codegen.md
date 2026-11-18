# T5 - init + codegen report (2026-10-20)

## Versions
- Clang: 21.1.4
- Python: 3.13.15
- rexglue: v0.10.0.0-dev.unknown (installed from SDK build, c94f5eb)

## Init
Command: rexglue init --project-name sonic4 --xex-path C:\S4EP1Recomp\game\extracted\default.xex --game-root C:\S4EP1Recomp\game\extracted --project-root C:\S4EP1Recomp\pilot
Exit: 0 (evidence\t5_init_log.txt). Created CMakeLists.txt, CMakePresets.json, sonic4_manifest.toml, generated\rexglue.cmake, src\main.cpp, src\sonic4_app.h

XEX: TitleID 58410A07, MediaID 2AA8DED1, Version 0.0.0.1, 2010-09-06 03:40:40 UTC

## Codegen
Codegen requires 2 function boundary hints for this title (UnresolvedCall). Added to manifest [entrypoint.functions] as 0x821F42D8 = {} and 0x8218653C = {} using correct TOML syntax (table, not array).

Final manifest saved; codegen ran successfully (exit 0). Log: evidence\t5_codegen_retry4.txt

## Generated files (pilot\generated\default)
- sonic4_funcs.*.h (65 header files: funcs.0.h..funcs.64.h) + sonic4_funcs.h
- sonic4_recomp.*.cpp (65 cpp files: recomp.0.cpp..recomp.64.cpp)
- sonic4_init.cpp, sonic4_init.h, sonic4_pch.h
- sonic4_register.cpp
- sources.cmake, codegen.build.stamp, codegen.stamp, codegen.d, codegen.partition.json
Total: 65 .cpp + 68 .h + supporting files. No single monolithic default.xex.cpp (partitioned).

## Result
Codegen passes validation after adding the two boundary hints. Generated C++ code is present and partitioned. Project is ready for CMake build (T6).

## Next step
T6: Build the pilot project (cmake --preset win-amd64-release using installed rexglue SDK, then build). Exit criteria: sonic4.exe built, run1.log obtained via --log_file.
