# Auditoria do POCO F7

Coletado com `C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe` em 16 de novembro de 2026. O aparelho estava autorizado (`device`, não `unauthorized`).

## Identidade

| Campo | Valor |
| --- | --- |
| Serial | `a63a397c` |
| Modelo | `25053PC47G` (POCO F7) |
| Product / device | `onyx_global` / `onyx` |
| Placa | `sun` |
| SoC | Qualcomm SM8735 |
| `ro.build.version.release` | 17 |
| `ro.build.version.sdk` | 37 |
| Fingerprint | `POCO/onyx_global/onyx:15/AQ3A.250226.002/OS3.0.7.0.WOLMIXM:user/release-keys` |

O `release` e o SDK reportam Android 17 / API 37. O fingerprint ainda carrega o segmento `15`. Os dois valores foram preservados como o aparelho os devolve.

## CPU e memória

- ABI: somente `arm64-v8a`. Não há `armeabi-v7a` na lista.
- Núcleos: 8 (`nproc`).
- RAM: `MemTotal` 11502928 kB (~11,0 GiB).

## GPU e Vulkan

Medido no processo do jogo (`sonic4_003.log`), não por um utilitário separado:

- GPU: Qualcomm Adreno (TM) 825
- Instância Vulkan: 1.4.0
- Dispositivo: API 1.3.284, vendor `0x5143`, device `0x44030000`, driver `0x80320047`
- Driver: Qualcomm Technologies Inc. Adreno Vulkan Driver, conformance 1.3.8.0
- Extensões ligadas pelo jogo: `VK_EXT_shader_stencil_export`, `VK_EXT_custom_border_color`, `VK_EXT_robustness2`, `VK_KHR_swapchain`
- Superfície: `VK_KHR_android_surface`

## Tela

- Painel físico: 1280×2772, 504 dpi
- Modo SurfaceFlinger observado: 1280×2772 a 60 Hz, faixa reportada até 120 Hz
- Rotação natural: `ROTATION_0` (retrato)
- Com o override de orientação do jogo: `ROTATION_90`, swapchain 2772×1280

O SDL, por padrão, pedia `SCREEN_ORIENTATION_FULL_USER` porque a janela é redimensionável e o hint de orientação vinha vazio. A activity passou a forçar `SCREEN_ORIENTATION_SENSOR_LANDSCAPE` em `setOrientationBis`.

## Armazenamento

Arquivos empurrados para `/storage/emulated/0/Android/data/...` ficam com dono `root`/`shell`. O app recebe `Permission denied` no `stat`, mesmo com modo 666. O caminho que funciona é o armazenamento interno do app:

`/data/user/0/com.s4ep1recomp.sonic4/files/`

Cópia usada:

```
adb push game\extracted /data/local/tmp/sonic4game
adb shell "cd /data/local/tmp/sonic4game && tar -cf - . | run-as com.s4ep1recomp.sonic4 tar -xf - -C files/game"
```

`adb install -r` preservou esses arquivos.
