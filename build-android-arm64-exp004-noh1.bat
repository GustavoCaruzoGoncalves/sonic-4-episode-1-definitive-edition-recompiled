@echo off
setlocal
if defined CMAKE_LINKER_TYPE set CMAKE_LINKER_TYPE=
set "NDK=%LOCALAPPDATA%\Android\Sdk\ndk\28.2.13676358"
cmake -S "%~dp0pilot" -B "%~dp0android\build-arm64" -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE="%NDK%\build\cmake\android.toolchain.cmake" ^
  -DANDROID_ABI=arm64-v8a ^
  -DANDROID_PLATFORM=android-28 ^
  -DANDROID_STL=c++_shared ^
  -DREXSDK_DIR="%~dp0third_party\rexglue-sdk" ^
  -DREX_DIAG_PROFILE=OFF ^
  -DREX_RENDER_TELEMETRY=ON ^
  -DREX_TIMING_PROBES=OFF ^
  -DREX_BATCH_DRAW_RESIDENCY=OFF ^
  -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build "%~dp0android\build-arm64" --target sonic4 --parallel
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0android\package-arm64.ps1"
exit /b %ERRORLEVEL%