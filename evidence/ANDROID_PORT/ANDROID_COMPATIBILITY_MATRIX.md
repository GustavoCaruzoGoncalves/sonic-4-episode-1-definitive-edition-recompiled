# Matriz de compatibilidade

Cinco eixos distintos. Um aparelho pode passar num e falhar noutro.

| Eixo | ARM64 | ARM32 (`armeabi-v7a`) |
| --- | --- | --- |
| ABI do CPU | Alvo principal. POCO F7 é só `arm64-v8a`. | Não gerado. |
| Android | `minSdk` 28, `targetSdk` 35. Validado no API 37. | Não se aplica. |
| Vulkan | Feature obrigatória `android:required="true"`, versão `0x400000` (Vulkan 1.0). O aparelho expõe 1.3 no device e 1.4 na instância. | Não se aplica. |
| GPU | Adreno 825 validado. Sem hardcode de modelo. Extensões opcionais só entram se o device as anuncia. | Não se aplica. |
| Desempenho | Primeiro minuto no F7: ritmo do guest ~50–60/s, depois ~28–34/s enquanto pipelines ainda nascem. Recompilação em `-O1`. | Não medido. |

## Por que ARM32 não existe

Não é uma limitação do NDK. O runtime recusa 32 bits:

- `platform.h` emite `#error` em plataforma de 32 bits.
- O FPSCR do guest só tem caminho x64 e AArch64.
- Vários intrinsics `simde` usados pelo código gerado só existem em x86_64 e ARM64.
- O POCO F7 não tem `armeabi-v7a` no `abilist`, então um APK 32 bits nem instalaria neste aparelho.

Um porte ARM32 exigiria um backend de codegen de 32 bits, revisão de todos os ponteiros e atômicos do runtime, e bibliotecas de terceiros em `armeabi-v7a`. Isso não foi iniciado. Não há `Sonic4-armeabi-v7a.apk` nem APK universal.

## Vulkan mínimo de fato

O presenter pede Vulkan 1.0 mais `VK_KHR_surface` e `VK_KHR_android_surface`. O jogo liga, quando existem:

- `VK_KHR_swapchain`
- `VK_EXT_shader_stencil_export`
- `VK_EXT_custom_border_color`
- `VK_EXT_robustness2`

Formatos de textura `k_Cr_Y1_Cb_Y0_REP` e `k_Y1_Cr_Y0_Cb_REP` não acharam os formatos preferidos (`1000156000` / `1000156001`) e caíram no formato 37. A imagem do jogo saiu correta mesmo assim.

Não há backend OpenGL ES. Aparelho sem Vulkan não é alvo.

## Orientação e aspecto

O painel do F7 é 20:9 em pé. O jogo pede landscape. A imagem 16:9 fica com tarjas laterais. Isso é o encaixe de aspecto, não um quadro vazio.
