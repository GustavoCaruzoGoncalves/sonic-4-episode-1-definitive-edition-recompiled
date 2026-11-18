# Pesquisa Xbox 360 / Android — recompilação e emuladores

**Data:** 17 de novembro de 2026  
**Projeto:** Sonic 4 EP1 (`C:\S4EP1Recomp`)  
**Clones locais:** `evidence/ANDROID_PORT/research_clones/` (shallow clone)

---

## 1. Projetos inspecionados

| Projeto | URL | Tipo | Código inspecionado |
| --- | --- | --- | --- |
| **Xeo** | https://github.com/ChuckStation/xeo | Emulador Android (JIT Xenia) | `patches/base/threading_posix.cc`, `memory.h`, `main_android.cc`, `app/` Gradle, JNI |
| **xenia-android** | https://github.com/xenia-android/xenia-android | Fork Android (EOL → Xeo) | `app/src/main/cpp/CMakeLists.txt` (ARM64, Vulkan, sem SDL) |
| **Asura's Wrath Recomp** | https://github.com/PauloUbirajara/asuras-wrath-recomp | Recomp estática + Android exp. | `CMakeLists.txt`, `src/asurawrath_app.h`, `jni_input.cpp` |
| **Pinyon Shift** | https://github.com/arcanite24/pinyon-shift | Recomp FH1 + **Android alpha** | `cmake/PinyonShiftRexGlue.cmake`, `docs/ANDROID.md`, backlogs 60fps |
| **Xenia upstream** | https://github.com/xenia-project/xenia | Emulador referência | Arquitetura CPU JIT / GPU (via patches Xeo) |

**Pinyon Shift / Forza Horizon — plataformas:** Windows, Linux, Steam Deck, macOS, **Android alpha** (não é emulador). Build APK no PC a partir do disco do usuário (`docs/ANDROID.md`).

---

## 2. Forza Horizon no Android — evidência

| Método | Evidência | Conclusão |
| --- | --- | --- |
| **Recomp nativa (Pinyon Shift)** | README + `docs/ANDROID.md`: PowerPC → C++ (ShiftGlue), APK arm64, Vulkan 1.3, Turnip em Adreno 7xx | **Principal** caminho “nativo” documentado |
| **Emulação Xenia** | Reddit/GameHub: Xenia + Proton/DX12 em camadas; texturas quebram fora da pista | **Possível** mas instável; não recomp |
| **aX360e / outros** | Relatos de crash no boot | **Não** qualificado para FH1 |

**Transferível ao Sonic 4:** mesma família ReXGlue/ShiftGlue, mesmas limitações Android (fibers, 16 KB page, drivers Vulkan), otimizações de **-march**, **linker**, **afinidade de thread**, **driver Turnip** — não o motor JIT do Xeo.

---

## 3. Matriz comparativa (arquitetura)

| Área | **Nós (S4EP1)** | **Xeo / xenia-android** | **Asura's Wrath** | **Pinyon Shift** |
| --- | --- | --- | --- | --- |
| **CPU** | PPC → C++ estático, sem JIT | JIT + tradução dinâmica ARM64 | Idem recomp | Idem + ShiftGlue fork |
| **ARM64 opts** | `-O3 -flto=thin` recomp; **agora** `-march=armv8.2-a` | JIT otimizado por bloco | `-O3 -ffast-math -funroll-loops` global recomp | `-march=armv8.2-a` SDK+host; LSE atomics |
| **Memória guest** | `base` + `REX_LOAD_*` volatile + bswap; offset `0xE0000000` | Tabela virtual Xenia | Mesmo padrão ReXGlue | Mesmo + tunings FH1 |
| **Threads** | XThread + fibers Android asm | pthread + sinais RT (Android) | SDL Android | `android_thread_cores` pinning FH1 |
| **GPU** | ReXGlue Xenos → Vulkan | Xenos → Vulkan Android | Xenos → Vulkan | Renderer FH1 + presets MSAA/shadows |
| **Vulkan** | Sem Turnip; cache ~32 B driver | Vulkan enabled | Vulkan | Turnip Gen8; timestamp perf em backlog |
| **Áudio** | SDL + worker guest | XMA decode nativo | XAudio paths | *(FH1-specific, não extraído)* |
| **Timing** | Guest clock + `PaceGuestMainLoopFrame`; **lock-free MFTB** (novo) | `clock_posix` + emulador | ReXGlue padrão | Host present limits 60/120 |
| **Android shell** | SDL Activity + touch pilot | Activities + SurfaceView | `SHARED` lib + JNI input | Launcher + Wi-Fi deploy |

Legenda: células vazias em concorrentes = não verificado no clone shallow.

---

## 4. Lições aplicáveis (com implementação)

### 4.1 Já aplicado (com validação de performance)

| Mudança | Arquivo | Medição | Veredito |
| --- | --- | --- | --- |
| Relógio guest lock-free | `third_party/rexglue-sdk/src/core/clock.cpp` | `perf_clock_lockfree_60s.log`: PACE ~43–47 Hz (carga média) vs ~24–29 Hz baseline | **Mantido** — ganho parcial, insuficiente para 60 Hz |
| Fix seqlock init (deadlock) | `clock.cpp` `ReanchorGuestClockAtCurrentSample` | Arranque + `perf_post_initfix_150s.log` | **Mantido** — funcional, sem efeito em PACE |
| `-march=armv8.2-a` | `cmake/rexglue_helpers.cmake` | `perf_armv82_research_75s.log` ~37–39 Hz aquecido | **Mantido** — ganho marginal |
| `-Bsymbolic-functions` | `src/system/CMakeLists.txt`, `src/graphics/CMakeLists.txt` | Não isolado; baseline misturado | **Provisório** até A/B |

Nenhuma dessas mudanças cumpre “definition of done” de performance.

### 4.2 Alta prioridade (próximos PRs)

| Técnica | Fonte | Risco | Notas |
| --- | --- | --- | --- |
| **Afinidade de thread** (`android_thread_cores`) | Pinyon `docs/ANDROID.md` | Médio | GPU Commands nos big cores; requer perf A/B no F7 |
| **Turnip / seleção de driver** | Pinyon Android | Médio | Adreno 825: medir stock vs Mesa; licença MIT Turnip |
| **Symbol order / ICF** | Pinyon `symbol-order-v4.txt` | Baixo-médio | Reduz PSS e misses; precisa simpleperf local |
| **Vulkan GPU timestamps** | Nosso diagnóstico + FH backlogs | Baixo | Separar record CPU vs GPU real |
| **Pinning / menos contention** no `function_dispatcher` | Observação nossa + áudio worker | Médio | Cache de `GetFunction` para alvos quentes |

### 4.3 Não aplicável

| Técnica | Motivo |
| --- | --- |
| JIT / blocos Xenia | Arquitetura estática diferente |
| D3D12 / VKD3D no Android | Nós usamos Vulkan nativo |
| Presets que desligam sombras/MSAA (Pinyon) | Viola restrição de qualidade visual neste projeto |

---

## 5. Xeo / xenia-android (destaques de código)

| URL | Arquivo / trecho | O que faz | Nós | Aplicável? | Testado aqui? |
| --- | --- | --- | --- | --- | --- |
| [xeo](https://github.com/ChuckStation/xeo) | `patches/base/threading_posix.cc` L50–79 `AndroidInitialize()` | `dlopen` libc + `pthread_getname_np` em API &lt;26 | `threading_posix.cpp` L73–108 igual | Sim (já portado) | Sim |
| [xeo](https://github.com/ChuckStation/xeo) | `app/src/main/cpp/CMakeLists.txt` L435–440 | `xbyak_aarch64` só em ARM64 — JIT do emulador | Recomp estática, sem JIT guest | **Não** | N/A |
| [xenia-android](https://github.com/xenia-android/xenia-android) | `app/src/main/cpp/CMakeLists.txt` L77–88 | `XE_ARCH_ARM64=1`, `XE_VULKAN_ENABLED=1`, **`XE_ENABLE_SDL=0`** | SDL + AAudio em nós | Diferente por design (emulador vs recomp) | N/A |
| [xenia-android](https://github.com/xenia-android/xenia-android) | Idem L248–253 | APU: XMA + nop, **sem SDL audio** | `audio_system.cpp` + worker guest | Inspiração parcial | Não copiado |
| [xeo](https://github.com/ChuckStation/xeo) | `CMakeLists.txt` L186–198 | `main_android.cc`, `filesystem_android.cc`, `memory_posix` | `rex_app` + `REX_LOAD` guest único | Parcial | Já alinhado ReXGlue |

---

## 6. Asura's Wrath Recomp (destaques)

| URL | Arquivo | O que faz | Nós | Aplicável? | Testado? |
| --- | --- | --- | --- | --- | --- |
| [asuras-wrath-recomp](https://github.com/PauloUbirajara/asuras-wrath-recomp) | `CMakeLists.txt` L8–18 | `-O3 -ffast-math -funroll-loops` global | `-O3 -flto=thin`, **sem** `-ffast-math` | **Rejeitado** (FP PPC) | N/A |
| [asuras-wrath-recomp](https://github.com/PauloUbirajara/asuras-wrath-recomp) | L53–66 | Android: `SHARED` + `jni_input.cpp` + SDL3 | Mesmo padrão pilot SDL | Sim (estrutura) | Sim (nosso port) |
| [asuras-wrath-recomp](https://github.com/PauloUbirajara/asuras-wrath-recomp) | `src/jni_input.cpp` | `__android_log_print` para input | Touch via pilot | Referência | Não necessário copiar |

Conclusão: arquitetura **idêntica**; gap de performance é **título + volume de dispatch de cena + tunings Android** (pinning, driver, PSO), não o boilerplate do APK.

---

## 7. Pinyon Shift — Android performance (citações do fonte)

| URL | Arquivo | Técnica | Diferença vs nós | Aplicável? | Implementado / testado? |
| --- | --- | --- | --- | --- | --- |
| [pinyon-shift](https://github.com/arcanite24/pinyon-shift) | `docs/ANDROID.md` L62–72 | `android_thread_cores` auto: decoder GPU nos cores mais rápidos | Sem pinning — scheduler mistura threads | **Sim** (POCO F7 = 8 Elite class) | **Não** — implementação está no ShiftGlue **não** publicado no clone shallow |
| [pinyon-shift](https://github.com/arcanite24/pinyon-shift) | `docs/ANDROID.md` L18–44 | Turnip Gen8 V37 embutido + seletor de driver | Só driver Qualcomm stock | **Sim** (Adreno 825) | **Não** |
| [pinyon-shift](https://github.com/arcanite24/pinyon-shift) | `docs/ANDROID.md` L98–111 | `-march=armv8.2-a`, `symbol-order-v4.txt` link | Temos armv8.2; **sem** symbol order | Parcial | armv8.2 sim; symbol order **não** |
| [pinyon-shift](https://github.com/arcanite24/pinyon-shift) | `docs/NEAR_NATIVE_RESEARCH.md` NN-3.14 | `linux_thread_cores` = mesma API no Deck | N/A Android ainda | Sim (Linux deveria espelhar) | **Não** |
| [pinyon-shift](https://github.com/arcanite24/pinyon-shift) | `docs/MALI_BACKLOG.md` MT-3.1 | Pin decoder em 4–7 **sem ganho** quando GPU-bound | Útil só se CPU decoder for gargalo | Condicional | N/A até medição GPU |

**Requisitos públicos:** ARMv8.2-A, Vulkan 1.3, Snapdragon 8 Gen 2+, 8–12 GB RAM (`docs/ANDROID.md` L8–9).  
**Não aplicar ao Sonic 4:** presets que desligam sombras/MSAA (qualidade retail fixa).

---

## 8. Gaps do Sonic 4 vs referências

1. **Sem pinning de thread** Android (Pinyon tem).
2. **Sem alternância de driver Vulkan** (Turnip).
3. **Sem ordenação de símbolos no link** (Pinyon v4).
4. **`SCENEDISP` alto** — problema de **carga de jogo**, não visto nos docs públicos do Asura; precisa simpleperf + ShiftGlue interno (não público).
5. **Áudio:** underrun baixo mas **stutter perceptível** → correlacionar com PACE guest (não buffer SDL).

---

## 9. Licenças

- Xenia / Xeo: BSD
- Pinyon Shift / ShiftGlue: BSD (fork ReXGlue)
- Asura's Wrath: ver repositório
- **Não** foi copiado código proprietário; apenas técnicas documentadas reimplementadas no nosso tree.

---

## 10. Artefatos

| Caminho | Conteúdo |
| --- | --- |
| `research_clones/xeo/` | Patches Android Xenia |
| `research_clones/xenia-android/` | Build CMake ARM64 |
| `research_clones/asuras-wrath-recomp/` | Recomp paralelo |
| `research_clones/pinyon-shift/docs/ANDROID.md` | Guia Android FH1 |
| `research_clones/xenia/` | Referência upstream (shallow) |