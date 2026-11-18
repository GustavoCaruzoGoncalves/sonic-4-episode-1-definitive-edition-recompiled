# Arquitetura Android

O guest continua sendo o PowerPC do Xbox 360, recompilado para C++ pelo ReXGlue. O host Android executa esse C++ em AArch64. O XEX original não é executado.

## O que é reutilizado sem mudança de lógica

- Código gerado em `pilot/generated/default/sonic4_recomp.*.cpp`, inclusive o conserto de ritmo (rearm em `sub_82488958`, deadline antes do sinal em `sub_8249E2F8`) e o gancho de licença.
- Plugin `rexgpu-xenos`: tradução de shaders Xenos, cache `.xsh` / `.xpso` e o `VkPipelineCache` com UUID de vendor, device e driver.
- Presenter Vulkan já existente, ramo `VK_KHR_android_surface` / `ANativeWindow`.
- SDL3 estático para janela, áudio (AAudio com OpenSL ES presente no NDK link) e joystick.
- Áudio, VFS e o caminho de save do runtime. No aparelho o save saiu em `files/user/B13EBABEBABEBABE/58410A07/00000001/...` (Title ID retail `58410A07`).

O codegen não roda no build Android (`pilot/generated/rexglue.cmake` deixa o alvo vazio quando `ANDROID` está definido). Editar os `.cpp` gerados no Windows continua valendo para o APK.

## O que é específico de Android

- `pilot` produz `libmain.so` (`OUTPUT_NAME main`) em vez de `sonic4.exe`.
- `Sonic4Activity` estende `org.libsdl.app.SDLActivity` e carrega, nesta ordem, `c++_shared`, `rexruntime`, `main`. O `JNI_OnLoad` do SDL está em `librexruntime.so`. O `SDL_main` está em `libmain.so`.
- `windowed_app_main_sdl.cpp` monta o argv: `--game_data_root`, `--user_data_root` e `--cache_root` sob `SDL_GetAndroidInternalStoragePath()`, mais `--gpu_plugin=xenos --gpu_backend=vulkan --license_mask=1`.
- Log em `files/user/logs`, porque o diretório da biblioteca nativa é somente leitura.
- `fiber_android.cpp`: troca de contexto AArch64 (x19–x30, sp, d8–d15). Não mexe em x18. O Bionic não tem `getcontext`/`makecontext`.
- Mutex robusto e `std::from_chars` de float seguem o caminho já usado no Apple. `timer_queue.cpp` compila com `-fexperimental-library`.
- `OpenAndroidContentFileDescriptor` é um stub. Os dados do jogo são um caminho real, não um `content://`.
- Overlay Java com D-pad, A/B/X/Y e pausa. O JNI `nativeSetButton` / `nativeSetAxis` alimenta um gamepad virtual do SDL.

## Ajustes de build que não vazam para o Windows

`WIN32` fica verdadeiro num host Windows mesmo com o NDK. Os dois `CMakeLists.txt` (SDK e `pilot`) fazem `set(WIN32 FALSE)` e `set(UNIX TRUE)` só quando `ANDROID`. D3D12 só é opção em `WIN32 AND NOT ANDROID`. O tipo de linker MSVC do SDK também fica atrás de `AND NOT ANDROID`.

O Clang 19 do NDK r28 cai com `0xC0000005` no otimizador em `-O3` nos TUs enormes da recompilação. Só esses TUs recebem `-O1` no Android. O build Windows permanece em `-O3`.

## Plugin da GPU

`librexgpu-xenos.so` não é `DT_NEEDED` de `libmain.so`. O runtime faz `dlopen` ao lado de `libmain.so` (`dladdr` de `GetExecutableFolder`). O APK coloca as quatro bibliotecas em `lib/arm64-v8a/`: `libmain.so`, `librexruntime.so`, `librexgpu-xenos.so`, `libc++_shared.so`.
