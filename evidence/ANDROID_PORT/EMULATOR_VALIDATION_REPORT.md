# Android emulator validation report

**Date:** 2026-11-17  
**Host:** Windows 10.0.19045, AMD Ryzen 7 5700X, 32 GB RAM, NVIDIA GeForce RTX 5060  
**Goal:** determine whether a local Android emulator can provide a comparable benchmark for `com.s4ep1recomp.sonic4` and whether it can isolate ARM64 recompilation from device/driver effects.

---

## 1. Verdict

**No valid emulator gameplay benchmark exists in this environment.**

- The native **ARM64** AVD was created from the official `arm64-v8a` system image, but the x86_64 Windows emulator refuses to boot it: `QEMU2` requires the system image architecture to match the host.
- The **x86_64** AVD boots, but the game is ARM64-only. It is launched through **Berberis ABI translation**, and Vulkan calls traverse NDK translation and the emulator's `vulkan.ranchu.so`/gfxstream layers. The process crashes during startup in `vk_common_SetDebugUtilsObjectNameEXT`.
- Even if the x86_64 launch had survived, its ABI translation and emulated Vulkan path would make it **invalid as a proxy for native ARM64 recomp cost or Adreno driver behavior**.

Therefore all quantitative comparisons in this investigation use:

1. native Windows x64 retail build on RTX 5060 / Vulkan, and
2. native POCO F7 ARM64 device on Adreno 825 / Vulkan.

---

## 2. Official installation sources

Android Studio and SDK components were obtained from official Android developer endpoints.

| Component | Source | Local result |
| --- | --- | --- |
| Android Studio Rabbit 2 `2026.2.2 Patch 1` ZIP | `https://edgedl.me.gvt1.com/android/studio/ide-zips/2026.2.2.9/android-studio-rabbit2-patch1-windows.zip` | Extracted to `C:\S4EP1Recomp\tools\android-studio-rabbit2-patch1\android-studio` |
| Command-line tools | `https://dl.google.com/android/repository/commandlinetools-win-15859902_latest.zip` | Installed under isolated SDK root |
| Emulator acceleration guidance | `https://developer.android.com/studio/run/emulator-acceleration` | AEHD selected because WHPX/Hyper-V is unavailable in this environment |

Verified hashes:

| Artifact | SHA-256 |
| --- | --- |
| Command-line tools ZIP | `90AE805D20434428BFFCB699C290860F19BB5F66A67E6B330067E3DE801FB04A` |
| Android Studio ZIP | `6813B0CE669844B87C5D56B9C5F661362BD3BEBFF55327415496BD7457A7B917` |

The GUI setup wizard was **not** run, to avoid modifying the existing Android development environment. SDK installation was isolated under:

`C:\S4EP1Recomp\android\perf-sdk`

---

## 3. Installed SDK components

Installed with the official `sdkmanager`:

| Component | Version / result |
| --- | --- |
| Platform Tools | 37.0.1 |
| Build Tools | 36.0.0 |
| Android platform | `platforms;android-36` |
| Emulator | 37.2.13 |
| AEHD | 2.2.0 |
| ARM64 image | `system-images;android-36;google_apis;arm64-v8a` |
| x86_64 image | `system-images;android-36;google_apis;x86_64` |

Host acceleration check:

```text
accel:
0
AEHD (version 2.2) is installed and usable.
accel
```

Evidence: `raw/emulator/accel_check.txt`, `raw/emulator/emulator_version.txt`.

Hyper-V/WHPX was disabled and HVCI was off; AEHD 2.2 was installed and running. This is a host-execution accelerator only; it does **not** permit an ARM64 guest image on an x86_64 host.

---

## 4. AVD configuration

AVD home: `C:\S4EP1Recomp\android\perf-sdk\avd`

| AVD | ABI | Profile | API | Resolution | vCPU | RAM | GPU |
| --- | --- | --- | --- | --- | ---: | ---: | --- |
| `PerfARM64_API36` | `arm64-v8a` | Pixel 7 | 36, Google APIs | 2772x1280 landscape | 4 | 4096 MB | host |
| `PerfX86_API36` | `x86_64` | Pixel 7 | 36, Google APIs | 2772x1280 landscape | 4 | 4096 MB | host |

Evidence: `raw/emulator/avd_create_arm64.txt`, `raw/emulator/avd_list.txt`.

---

## 5. Native ARM64 launch attempt

Command path used:

`C:\S4EP1Recomp\android\perf-sdk\emulator\emulator.exe -avd PerfARM64_API36 ...`

Result:

```text
INFO         | Found AVD name 'PerfARM64_API36'
INFO         | Found AVD target architecture: arm64
INFO         | Found systemPath ...\system-images\android-36\google_apis\arm64-v8a\
FATAL        | Avd's CPU Architecture 'arm64' is not supported by the QEMU2 emulator on x86_64 host. System image must match the host architecture.
```

Evidence: `raw/emulator/arm64_boot_attempt.txt`.

**Conclusion:** installing an ARM64 image is not sufficient. The official emulator on this x86_64 host cannot boot it. There is no “ARM64 emulation without translation” fallback here.

---

## 6. x86_64 AVD: boot succeeded, game did not

The x86_64 AVD boots with AEHD. Emulator logs show `Target arch = 'x86_64'`, `qemu-system-x86_64.exe`, host Vulkan/GLES selected, and the RTX 5060 detected through gfxstream:

```text
emuglConfig_init: gpu_mode_requested: host
emuglConfig_init: vulkan_mode_selected:host gles_mode_selected:host
Found physical GPU 'NVIDIA GeForce RTX 5060', apiVersion: 1.4.357
Initializing gfxstream backend
```

Evidence: `raw/emulator/x86_boot.txt`.

The APK installed successfully:

```text
Performing Incremental Install
Performing Streamed Install
Success
```

Evidence: `raw/emulator/apk_install.txt`.

However, the APK contains `lib/arm64-v8a` only. Logcat confirms the game process runs with `Guest architecture: 'arm64'` on an x86_64 system image and uses Berberis:

```text
V SDL : Model: sdk_gphone64_x86_64
D vulkan : searching for layers in .../base.apk!/lib/arm64-v8a
F DEBUG : Build fingerprint: 'google/sdk_gphone64_x86_64/emu64xa:16/...'
F DEBUG : ABI: 'x86_64'
F DEBUG : Guest architecture: 'arm64'
```

The process crashes during startup in the GPU Commands thread:

```text
F libc : Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x40 in tid ... (GPU Commands (F), pid ... (SDLActivity)
F DEBUG : pid: ..., tid: ..., name: GPU Commands (F  >>> com.s4ep1recomp.sonic4 <<<
F DEBUG : signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x0000000000000040
#00 pc ... /vendor/lib64/hw/vulkan.ranchu.so (vk_common_SetDebugUtilsObjectNameEXT+283)
#01 pc ... /system/lib64/libndk_translation_proxy_libvulkan.so (berberis::...::DoCustomTrampolineWithThunk_vkSetDebugUtilsObjectNameEXT...)
I Zygote : Process ... exited due to signal 11 (Segmentation fault)
```

Evidence: `raw/emulator/game_launch2_logcat.txt`, `raw/emulator/game_launch5_logcat.txt`.

Software/SwiftShader variants were also attempted (`raw/emulator/x86_boot_software.txt*`, `x86_boot_swiftshader*.txt*`); none produced a valid gameplay benchmark.

---

## 7. Why this cannot isolate the Windows/Android gap

The x86_64 emulator path adds at least three independent variables that do not exist on the POCO F7:

1. **Instruction-set translation:** ARM64 guest code is translated by Berberis to x86_64.
2. **Vulkan proxying:** ARM64 Vulkan calls pass through `libndk_translation_proxy_libvulkan.so` and the emulator's gfxstream/ranchu implementation.
3. **Host GPU mediation:** the RTX 5060 is reached through emulator command streams rather than a native Adreno driver.

A result from this path would conflate:

- XenonRecomp ARM64 codegen,
- Berberis translation,
- gfxstream/ranchu overhead,
- host Vulkan driver behavior,
- and the game's own frame pacing.

It would therefore **not** answer whether the POCO F7 deficit comes from ARM64 recompilation, guest execution, Vulkan CPU work, or Adreno driver behavior. No FPS estimate is inferred from the failed launch.

---

## 8. Artifacts

| Path | Meaning |
| --- | --- |
| `raw/emulator/accel_check.txt` | AEHD 2.2 usable |
| `raw/emulator/emulator_version.txt` | Emulator 37.2.13.0 |
| `raw/emulator/avd_create_arm64.txt` | ARM64 AVD creation |
| `raw/emulator/avd_list.txt` | Both AVDs |
| `raw/emulator/arm64_boot_attempt.txt` | ARM64 boot refusal |
| `raw/emulator/arm64_boot_attempt.txt.err` | Emulator stderr |
| `raw/emulator/x86_boot.txt` | x86_64 boot / host Vulkan / gfxstream |
| `raw/emulator/apk_install.txt` | APK install success |
| `raw/emulator/game_launch2_logcat.txt` | Startup SIGSEGV, translation stack |
| `raw/emulator/game_launch5_logcat.txt` | Repeated SIGSEGV |
| `raw/emulator/game_launch*.png` | Emulator screenshots |
| `raw/emulator/x86_boot_software.txt*`, `x86_boot_swiftshader*.txt*` | Alternative GPU-mode attempts |
