# Validação Android e performance (POCO F7)

Atualizado em **17 de novembro de 2026**. Dispositivo: **POCO F7** (`25053PC47G`, serial `a63a397c`), **Android 17**, **arm64-v8a**, GPU **Adreno 825** (kgsl `644 MHz` típico sob carga). Pacote: `com.s4ep1recomp.sonic4`. APK: `C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk`.

**Veredicto:** gameplay **acessível** pós-fix de init, mas **performance e áudio ainda não atingem o alvo**. Ver secção **13**. **Atualização 17/11 — validação dispositivo:** APK **sem** `REX_DIAG_PROFILE` medido no POCO F7 (`perf_nodiag_150s.log`, `sonic4_024`): **~44,3 Hz** médio vs **~39,6 Hz** no baseline com diag (`perf_post_initfix_150s.log`) — melhora **~12%**, ainda longe de 60 Hz. **Atualização 17/11 (noite):** APK com **retail hooks** (`REX_TIMING_PROBES=OFF`, `perf_retail_hooks_150s.log`, `sonic4_025`): **~45,9 Hz** médio, `frame_us` **~18,5 ms** — ver `ROOT_CAUSE_PERFORMANCE_INVESTIGATION.md`. **Vulkan CPU (17/11):** decomposição em `VULKAN_CPU_RECORDING_INVESTIGATION.md`; retail pós-opt `perf_vk_opt_retail2_150s.log` **39,1 Hz** médio (revalidar frio).

Build: **NDK r28.2.13676358**, Clang 19, CMake **Release**, código recompilado com **`-O3 -fno-strict-aliasing -flto=thin`** (após correção do crash na seleção de mundo). Runtime compartilhado inclui `ExecuteUnregisteredTailThunk` em `third_party/rexglue-sdk/src/system/function_dispatcher.cpp`.

**Build diagnóstico (Vulkan + áudio):** `REX_DIAG_PROFILE=1` só Android — ver relatório dedicado [`VULKAN_AUDIO_PROFILING.md`](VULKAN_AUDIO_PROFILING.md). APK instalado **2026-11-16 ~22:40**; log `sonic4_017` → `diag_sceneA_ground.log` / `diag_sceneB_sky.log`.

Não há commit git neste repositório; referência de árvore: árvore local `C:\S4EP1Recomp` no momento do build **2026-11-16 22:28** (APK LTO + diag).

---

## 1. Validação do cenário que crashava (seleção de mundo)

### Windows (referência, já validado nesta sessão)

- Log: `evidence/ANDROID_PORT/win_worldselect_after_fix.log`
- Linha: `Dispatching unregistered guest tail thunk at 0x8218FB50 -> 0x82190270` (**22:13:18**), **sem FATAL**
- `PROBE PACE` sustentado ~**59–60/s**, `frame_us` ~16 000–17 000
- Navegação na seleção de zonas, entrada em ato e gameplay Splash Hill (capturas `win_after_fix_screen3.png`, `win_after_fix_print.png`)

### Android

- Instalação: `adb install -r` (dados e save preservados)
- Logs: `evidence/ANDROID_PORT/android_worldselect_after_fix_app.log` (`sonic4_014`), `android_lto_perf.log` (`sonic4_015`/`016`)
- Mesmo thunk: **22:17:31** e **22:29:47** — **sem FATAL**
- Capturas: seleção de zona/mundo (`android_after_fix_screen1.png`), atos (`android_after_fix_screen2.png`), gameplay (`android_after_fix_gameplay.png`, `android_lto_screen1.png`)

**Conclusão:** o fix de tail thunk vale para **Windows e Android** no fluxo com progressão salva.

---

## 2. `-O3` / ThinLTO **não** resolveram o lag

| Configuração (recomp) | Guest `PROBE PACE` (últimas 20 janelas de 5 s) | Observação |
| --- | ---: | --- |
| `-O2 -fno-inline` (baseline pós-crash) | **39,0** Hz média (min 35,4) | `android_worldselect_after_fix_app.log` |
| `-O3` | **41,5** Hz média (min 36,4) | `android_o3_perf.log` — melhora marginal |
| `-O3 -flto=thin` | **44,8** Hz média (min 36,4); pico **59,3** Hz em transições | `android_lto_perf.log` — ainda **~35–42 Hz** sustentado em fase |

Clang 19 compilou **`-O3` + LTO** sem crash de compilador (o crash histórico estava ligado a **`-fasync-exceptions`**, só Windows).

**Diagnóstico atualizado:** o teto de ~**37–42 atualizações guest/s** em gameplay sustentado **não é** primariamente falta de otimização de compilador. O guest no PC, com as mesmas sondas, fica em **~60/s** (`win_worldselect_after_fix.log`).

---

## 3. Cenário A (muita cenário) vs B (pouca geometria / “no céu”)

### Observação do usuário (mantida como hipótese direcionadora)

Performance **melhora no ar**, com pouco cenário visível; **piora** com geometria e objetos ao redor.

### Medições com sondas já existentes (`sonic4_timing_hooks.cpp`)

As funções instrumentadas incluem:

| Símbolo | Papel (Ghidra) |
| --- | --- |
| `sub_821A8488` | Dispatcher de cena → chama `sub_82197F28` |
| `sub_82197F28` | Atualização de cena |
| `sub_8249CF80` | Atualização por objeto |
| `sub_821B35C8` | Viewport (chamadas por frame do main loop) |

**Atualização diag 16/11:** draws Vulkan, barriers e tempo de record por present passam a sair em `PROBE DIAG_VK` (detalhe em `VULKAN_AUDIO_PROFILING.md`). GPU execution continua `gpu_exec_ns=na` (sem timestamp queries neste build).

### Janelas de 5 s — `android_scene_ab_test.log` (gameplay automatizado + sessão LTO)

| Condição | `SCENEDISP_821A8488/s` | `PROBE PACE` (Hz) | `frame_us` (µs) |
| --- | ---: | ---: | ---: |
| **Leve** (ex.: transição / poucos objetos) | **197–336** | **54–59** | **16 100–17 700** |
| **Pico “cenário pesado”** | **1469** | **33,3** | **28 210** |
| **Alto** | **1248** | **45,6** | **20 925** |
| **Médio** | **540–863** | **36–50** | **18 400–25 000** |

**Correlação direta nos piores quadros:**

- Pior PACE: **33,3 Hz** @ `SCENEDISP` **1469/s**, `SCENEUPD` **1528/s**, `OBJUPD` **1469/s** (`22:32:08`)
- Melhor PACE: **59,3 Hz** @ `SCENEDISP` **198/s** (`22:29:37`)

O teste ADB solo (correr no chão vs saltos) **não reproduziu** tão bem o contraste manual; na sessão diag (`sonic4_017`) o trecho “céu” automatizado manteve `SCENEDISP` **387–663/s** e PACE **~29–31 Hz** (dispositivo já aquecido após cenário A). Os logs mostram que **quando a taxa de dispatch dispara (>1000/s), o ritmo guest cai para ~28–33 Hz** e **`draws_per_present_avg` sobe para ~250–310** — compatível com “muito cenário visível = pior”.

| Momento (diag) | `SCENEDISP/s` | PACE (Hz) | `draws/present` | `record_us_avg` |
| --- | ---: | ---: | ---: | ---: |
| Leve `22:41:11` | 212 | 59,5 | 34 | 1665 |
| Pico `22:41:26` | 1195 | 28,1 | 310 | 10950 |
| Pesado `22:41:36` | 442 | 24,3 | 214 | 7444 |

### GPU (indicadores ADB, não substituem Perfetto completo)

- `gpu_busy_percentage`: **66–77%** durante gameplay
- `devfreq/cur_freq`: **644 MHz**
- **Não** concluímos gargalo **só** de GPU: CPUs guest estão em **100%** de um núcleo enquanto a GPU não está em 100%

### simpleperf (`evidence/ANDROID_PORT/perf_lto.data`, ~6 s gameplay)

| Overhead | Símbolo / DSO |
| ---: | --- |
| 28% | `libmain.so` (código recompilado) |
| 21% | `libc.so` (`__kernel_clock_gettime`, `__schedule`) |
| 16% | kernel |
| 10% | `libllvm-qgl.so` (compilador shader Adreno) |
| ~10% | guest hot: `sub_821C62C8`, `sub_821C4870` (loop de pacing/tempo) |

Threads (`top -H`): **XThread** e **Main XThread** ~**90–100%** CPU; **Audio Worker** ~**72–93%**; **GPU Commands** ~**53–72%**; **SDLThread** ~**65–89%**.

---

## 4. Áudio — stutter e congelamentos

### Sintomas

Áudio continua irregular **após `-O3`**, alinhado ao relatório anterior (`PERFORMANCE_OPTIMIZATION_REPORT.md`).

### Mecanismo provável (evidência, não palpite)

1. **Guest CPU saturado** → menos de **60** ticks guest/s → fila de frames de áudio atrasa.
2. **Audio Worker** executa callbacks guest via `function_dispatcher` (`audio_system.cpp`); sob carga compete com **Main/XThread** pelos mesmos núcleos (top mostra **~80%+** no worker).
3. **`PROBE WAITSHAPE`** no log: centenas de esperas **`long>12ms`** por janela de 5 s; `FRAMEUPD dur_us avg` **~24 000–30 000** µs no Android vs **~4 500** µs no Windows — o loop principal passa muito tempo bloqueado/esperando, o que **correlaciona** com picos de `SCENEDISP`.

4. **Diag `PROBE DIAG_AUDIO`:** `worker_disp_us_avg` **~1,3–1,5 ms** × **~187/s** ≈ **26% de um núcleo** em código guest; callback SDL **~13 µs**; **`underrun/s` ≈ 0** na gameplay medida (stutter não explicado por fila vazia crônica). Snapshot `top`: Audio Worker **~73%**, XThread **~97%**.

**Não** confirmamos que a thread de render **bloqueia diretamente** o áudio; o modelo atual é **fome de CPU compartilhada** + simulação/pacing guest lento (`821C62C8` ≈ duração do frame quando PACE baixo).

---

## 5. Vulkan / pipelines

- Cache driver: `Vulkan driver pipeline cache loaded (32 bytes)` — praticamente vazio a cada boot (`58410A07.vkcache` no dispositivo **64 bytes** após sessão).
- PSO: **47** criações em trecho de gameplay; **máx ~515 ms**, **média ~156 ms** (`android_worldselect_after_fix_app.log`); pico inicial ~**444 ms** com `worker=0` no thread de apresentação.
- Contribui a **stalls audíveis no início** e em mudanças de material, mas **não explica** 35 Hz **contínuos** após warmup (PSO count baixa, PACE permanece baixo com `SCENEDISP` alto).

### Diag workload (mesma sessão POCO F7)

| Carga | `draws/s` | `draws/present` | `desc_binds/s` | `record_us_avg` | `submit_us_avg` |
| --- | ---: | ---: | ---: | ---: | ---: |
| Leve | ~2032 | ~34 | ~4971 | ~1665 | ~528 |
| Pico `SCENEDISP` | ~8703 | ~310 | ~17951 | ~10950 | ~533 |
| Pesado sustentado | ~5200–7400 | ~213–266 | ~11k–16k | ~7400–8500 | ~670–850 |

`gpu_exec_ns` permanece **não medido** neste build — não concluir gargalo GPU só por `gpu_busy` ADB.

---

## 6. Gargalo identificado (antes de nova otimização)

### Primário (revisado 17/11): **orçamento de frame guest + Vulkan record**

- **`SCENEDISP` alto não implica PACE baixo:** no mesmo APK, **1519 disp/s @ 59,7 Hz** com **181 draws/present**; Windows sustenta **~1895 disp/s @ 60 Hz**.
- Queda de PACE alinha com **`FRAMEUPD dur_us` ~34 ms**, **`draws/present` ~300+** e **`record_us_avg` 8–11 ms**, não com µs/chamada isolados em scene dispatch.
- Até 17/11, medições “Release” usavam **`REX_DIAG_PROFILE=ON`** (overhead estrutural) — baseline de produto deve usar build perf sem diag.

### Secundário

- **Pacing/time** (`sub_821C62C8` / `sub_821C4870`): loop de poll até o relógio guest avançar — **`821C62C8_us_avg` ≈ 28–37 ms** quando PACE ~24–30 Hz; **`821C4870` ~4,5M/s** com ~0,1 µs/chamada (Ghidra: `do { 821C4870(); } while` tempo não atingido). simpleperf anterior: `clock_gettime`, `schedule`.
- **Vulkan record CPU:** até **~11 ms/present** correlacionado a **~250–310 draws/present**.
- **Viewport** `sub_821B35C8`: **~450–800 chamadas/s** com ~15 viewports por iteração do main loop.
- **GPU + compilação shader** (libllvm-qgl, PSO): relevante em spikes, não no teto sustentado.

### O que **não** é o gargalo principal

- Compilador **`-O3` / LTO** sozinho (ganho < ~15% na média, ainda longe de 60 Hz).
- O crash de seleção de mundo (corrigido; não confundir com performance).

---

## 7. Otimizações implementadas nesta fase

| Mudança | Objetivo | Resultado medido |
| --- | --- | --- |
| Tail thunk runtime (`function_dispatcher.cpp`) | Crash `0x8218FB50` | Validação OK Win/Android |
| `-O3` no recomp Android (`rexglue.cmake`) | Performance | **~+2 Hz** média — insuficiente |
| `-flto=thin` no recomp + link | Inline entre TUs | **~+5 Hz** média; picos melhores; sustentado ainda ~37–42 Hz |
| `REX_DIAG_PROFILE=1` | Contadores Vulkan/áudio/guest | Build diag apenas Android; ver `VULKAN_AUDIO_PROFILING.md` |

**Nenhuma** alteração de qualidade gráfica, culling, batching Vulkan ou áudio de produção nesta passagem — de propósito, até fixar a causa.

---

## 8. Antes / depois (mesmo dispositivo, mesma fase)

| Métrica | Baseline `-O2 -fno-inline` | `-O3` + LTO | Windows Vulkan (ref.) |
| --- | ---: | ---: | ---: |
| Guest PACE médio (gameplay) | ~39 Hz | ~41–45 Hz | ~60 Hz |
| `frame_us` típico | ~24 000–26 000 | ~22 000–26 000 | ~16 000 |
| `SCENEDISP` pico | >1200/s | >1400/s | (não extraído aqui) |
| FATAL seleção mundo | Corrigido | Corrigido | Corrigido |
| Áudio | Stutter | Stutter | OK |

---

## 9. Metodologia e artefatos

| Ferramenta | Uso |
| --- | --- |
| Sondas `PROBE *` em `pilot/src/sonic4_timing_hooks.cpp` | PACE, cena, viewport, waits; **diag:** `DIAG_VK`, `DIAG_AUDIO`, `DIAG_GUEST` |
| `adb shell top -H` | CPU por thread |
| `dumpsys thermalservice` | Térmica (status 0; CPU ~70–79 °C sob carga) |
| `simpleperf record` | `perf_baseline_o3.data`, `perf_lto.data` |
| Ghidra MCP | `821A8488`, `821C62C8`, `821C4870` |
| Screenshots | `evidence/ANDROID_PORT/android_*` |

Baseline bruta: `evidence/ANDROID_PORT/perf_baseline_221955/`.

---

## 10. Fase de otimização ARM64 (novo objetivo)

Meta: **~60 Hz guest / 60 FPS** no POCO F7, paridade com Windows, sem sacrificar qualidade. A comparação “céu vs chão” **não** é benchmark oficial.

Primeira otimização implementada: relógio guest **analítico lock-free** em `clock.cpp` (Win+Android). Medição Android: PACE **~43–47 Hz** com `SCENEDISP` ~430–710/s vs baseline **~24–29 Hz** (`perf_clock_lockfree_60s.log`). Ainda longe de 60 Hz em cenas pesadas.

Detalhes: **[`ARM64_PERFORMANCE_OPTIMIZATION.md`](ARM64_PERFORMANCE_OPTIMIZATION.md)**.

---

## 11. Limitações e próximos passos (com hipótese)

1. ~~Draw calls Vulkan por cenário A vs B~~ — **feito** (diag); falta **GPU timestamp** e **repetir céu manual** pós cool-down.
2. **Perfilhar quem chama `sub_821A8488` >60× por frame guest** — callers `8249CF50` / `8249D0EC` dominam em carga; investigar redundância Xenos→host.
3. **Áudio:** underruns medidos (≈0); próximo passo é **correlacionar série temporal** com PACE/WAITSHAPE, não mudar buffer ainda.
4. **Vulkan cache:** blob **32 bytes** no driver; PSO spikes no boot continuam relevantes.
5. **Não** repetir flags de compilador sem nova hipótese.

Relatório completo: **[`VULKAN_AUDIO_PROFILING.md`](VULKAN_AUDIO_PROFILING.md)**.

---

## 12. Restrições respeitadas

- Crash de seleção de mundo e timing guest **não** foram contornados com cheats.
- Windows D3D12/Vulkan e Android Vulkan **não** foram desabilitados.
- Save original preservado; testes usaram dados no sandbox do app.

---

## 13. Avaliação pós-fix de init (2026-11-17, medição obrigatória)

### 1. O que o último fix realmente corrigiu

- **Problema:** deadlock na init estática de `clock.cpp` ao carregar `librexruntime.so` (seqlock ímpar + `ComputeGuestTickFromHost` em loop).
- **Sintoma:** logo infinita, sem `libmain`, 100% CPU.
- **Não corrigiu:** ritmo guest (~60 Hz), stutter de áudio, custo Vulkan, `SCENEDISP`, poll `821C4870`.

### 2. O que permanece quebrado (sintomas do utilizador confirmados por sonda)

| Sintoma | Evidência (`perf_post_initfix_150s.log`) |
| --- | --- |
| Gameplay lento | PACE **28,4–41,5 Hz** em fase pesada; média **40,6 Hz** em janelas mistas (inclui trechos a ~59 Hz no fim — transição/leve) |
| Frame longo | `frame_us` **22–29 ms**; `821C62C8_us_avg` **19–26 ms** |
| Vulkan caro no CPU | `record_us_avg` **6,5–11 ms**; **203–248** draws/present |
| Poll agressivo | `821C4870/s` **~4,0–4,6M/s** enquanto PACE baixo |
| Áudio instável | `underrun/s=0`, mas `worker_disp_us_avg` **~1,5 ms** × **~187/s** com PACE &lt;60 → atraso perceptível |
| GPU desconhecida | `gpu_exec_ns=na` |

### 3. Repositórios efetivamente inspecionados (código, não só README)

Clones em `evidence/ANDROID_PORT/research_clones/`: **xeo**, **xenia-android**, **asuras-wrath-recomp**, **pinyon-shift**, **xenia**. Tabela detalhada: [`XBOX360_ANDROID_RESEARCH.md`](XBOX360_ANDROID_RESEARCH.md).

**Limite:** implementação de `android_thread_cores` no Pinyon vive no **ShiftGlue** (submódulo privado); só documentação pública + `threading_posix` genérico foram inspecionados no clone.

### 4. Técnicas úteis identificadas (ainda não validadas no Sonic 4)

- Pinning `GPU Commands` (nome já usado em `command_processor.cpp` L144).
- Vulkan timestamp queries no diag.
- Turnip opcional (Pinyon MIT bundle).
- Symbol-order link (Pinyon NN-1.9).
- **Rejeitado:** `-ffast-math` (Asura), cheats de PACE, reduzir MSAA/sombras.

### 5. Baseline de performance atual

- **Artefato:** `evidence/ANDROID_PORT/perf_post_initfix_150s.log` (pull `sonic4_022.log`, build após fix init + opts #1–#3).
- **Procedimento:** cold start → ~150 s gameplay com save existente → extrair log via `adb exec-out run-as com.s4ep1recomp.sonic4 cat files/user/logs/sonic4_022.log`.
- **Comparar sempre** com: `diag_sceneA_ground.log` (pré-opt), `perf_clock_lockfree_60s.log` (efeito relógio isolado histórico), `win_worldselect_after_fix.log` (Windows).

### 6. Gargalo prioritário

**Capacidade de completar um frame guest a tempo (~20–26 ms em `821C62C8` sob carga)** — composto por dispatch de cena/objeto + record Vulkan CPU + espera/poll (`821C4870`). Até isso cair perto de **~16 ms** sustentado, PACE não atinge 60 Hz e o áudio continuará irregular apesar de `underrun/s` baixo.

### 7. Próximo passo de implementação (com A/B)

1. Instrumentação **GPU timestamp** (só medir — sem afirmar ganho).
2. **`android_thread_cores` auto** espelhando Pinyon `docs/ANDROID.md` — medição em cenário pesado **reproduzível** (mesmo save, mesma fase, 120 s).

### Definition of done (ainda **não** satisfeita)

- [ ] PACE sustentado ~55–60 Hz em gameplay típico ≥5 min
- [ ] Áudio sem stutter audível
- [ ] Windows build não regressado (incl. relógio novo)
- [ ] Logs before/after documentados