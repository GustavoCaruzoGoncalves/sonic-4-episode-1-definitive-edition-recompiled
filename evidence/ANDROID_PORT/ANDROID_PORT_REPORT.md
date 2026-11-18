# Relatório do porte Android

Sonic the Hedgehog 4: Episode I, recompilação nativa, Vulkan, POCO F7.

## 1. O que foi implementado

- Biblioteca nativa `libmain.so` (AArch64) ligada ao runtime e ao plugin `librexgpu-xenos.so`.
- Activity `com.s4ep1recomp.sonic4.Sonic4Activity` em cima do SDL, com superfície Android, ciclo de vida do SDL e overlay de toque (D-pad, A/B/X/Y, pausa) ligado a um gamepad virtual.
- Argv fixo para dados, save e cache no armazenamento interno, Vulkan e `license_mask=1`.
- Fibras AArch64, mutex sem `PTHREAD_MUTEX_ROBUST`, `from_chars` portátil e `-fexperimental-library` onde o libc++ do NDK exige.
- Empacotamento `aapt2` + `d8` + `zipalign` + `apksigner`. Script `build-android-arm64.bat`.
- Orientação landscape forçada, porque o SDL pedia `FULL_USER` e o painel ficava em retrato.

D3D12, o Vulkan de Windows, o conserto de ritmo e o cache de shaders compartilhado não foram revertidos.

## 2. ARM64

Funciona. O APK instala e o processo sobe no POCO F7.

## 3. ARM32

Não funciona e não foi empacotado. O runtime e o codegen são 64 bits (`#error` em 32 bits, FPSCR e `simde` só em x64/AArch64). O F7 também não lista `armeabi-v7a`.

## 4. Android suportado

`minSdk` 28, `targetSdk` 35. Validado em API 37 (o `ro.build.version.release` deste aparelho diz 17; o fingerprint ainda diz 15).

## 5. Vulkan exigido

Vulkan 1.0 declarado no manifesto, com `VK_KHR_surface`, `VK_KHR_android_surface` e `VK_KHR_swapchain`. Extensões de stencil export, border color e robustness2 só são ligadas se existirem. Neste aparelho: Adreno 825, device 1.3.284, instância 1.4.0.

## 6. O POCO F7 executa o jogo

Sim. Swapchain 2772×1280, menu retail na tela, fase Splash Hill renderizada, `LICENSE flag160=0`.

## 7. Gameplay testado

Por automação, não por uma pessoa com o telefone na mão. O toque no A confirmou New Game e entrou na fase. O direcional direito colocou o Sonic no ar e o placar mudou para 1000. O cronômetro correu (0'14"43 e depois 0'27"69 nas capturas). A música `His World` apareceu na sessão de mídia. Pausa, controle Bluetooth, colisão detalhada e um ato completo não foram percorridos manualmente.

## 8. Desempenho

Sonda do guest, alvo 60, log `sonic4_003.log`:

- 21:00:32 a 21:00:47: 49,1 / 59,5 / 54,5 / 51,2 por segundo
- 21:00:52 em diante, com pipelines ainda sendo criados: cerca de 28–34 por segundo, com um pico de 45,2

Frames do host na mesma janela: ~53–59/s no começo, depois ~33–42/s (`frame_us` 22–28 ms). PSO quase todo em `worker=1`. Um PSO de 439,8 ms caiu no thread de apresentação. Cache de driver: 32 bytes carregados, 64 bytes no disco após a fase.

Não há 1% low nem leitura térmica.

## 9. Limitações

Ver `KNOWN_LIMITATIONS.md`. As que mudam a experiência agora: `-O1`, ritmo ~30 Hz durante a compilação de pipelines, botões sobre a imagem, áudio em 6 canais, sem ARM32.

## 10. APK

`C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk`

Não há `Sonic4-armeabi-v7a.apk` nem `Sonic4-universal.apk`.

## 11. Recompilar e instalar

```
build-android-arm64.bat
C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe install -r C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk
C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe shell am force-stop com.s4ep1recomp.sonic4
C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe shell am start -n com.s4ep1recomp.sonic4/com.s4ep1recomp.sonic4.Sonic4Activity
```

Os dados já estão em `files/game` deste aparelho. Se o app for desinstalado, repetir a cópia descrita em `ADB_DEPLOYMENT.md`.
