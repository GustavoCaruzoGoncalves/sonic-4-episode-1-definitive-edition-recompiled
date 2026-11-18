# Auditoria da arquitetura de execução Android

**Projeto:** Sonic 4 EP I — recompilação estática ReXGlue + runtime Xenos/Vulkan.  
**Dispositivo alvo:** POCO F7 (Snapdragon 8-series, arm64, Adreno 825).  
**Data:** 2026-11-17.

---

## 1. Caminho de execução (mapa)

```text
Xbox 360 PPC (retail)
  → código gerado (sonic4_recomp.*.cpp, libmain.so)   [REX_LOAD/STORE + bswap + volatile]
  → ReXGlue runtime (librexruntime.so)                [threads, relógio, dispatcher, global lock]
  → plugin rexgpu-xenos (librexgpu-xenos.so)           [command processor Vulkan, caches]
  → driver Vulkan Adreno (Turnip/Qualcomm)
  → SDL3 áudio (AAudio/OpenSL) + apresentação
```

**Referência Windows:** mesma pilha guest + runtime; backend Vulkan ou D3D12; **~60 Hz** com as mesmas sondas (`win_worldselect_after_fix.log`).

---

## 2. Falhas arquiteturais identificadas (com evidência)

### F1 — APK “Release” com instrumentação diagnóstica sempre ativa (**corrigido 17/11**)

Até 16/11, `REX_DIAG_PROFILE=1` era definido para **todo** build Android Release (`sonic4`, `rexruntime`, `rexgpu-xenos`, `rexaudio`). Isso injeta:

- `OnVkDraw`, `OnVkBindDescriptorSets`, `OnVkPipelineBarrier`, … em **cada** operação Vulkan (`deferred_command_buffer.cpp`).
- Contadores atómicos em áudio/SDL.

Com **8–10k draws/s** e **15–20k descriptor binds/s** (`perf_post_initfix_150s.log`), isso é contenção de cache e instruções extras em um caminho já limitado por CPU.

**Correção:** opção CMake `REX_DIAG_PROFILE` (default **OFF**); `build-android-arm64-diag.bat` para medições.

**Impacto esperado:** redução mensurável de `record_us_avg` e tempo guest — **validação pendente no POCO F7**.

### F2 — Confusão sintoma/causa no loop de pacing (`821C62C8` / `821C4870`)

O jogo original faz busy-wait em `sub_821C4870` (poll de relógio guest) dentro de `sub_821C62C8` até avançar 1/60 s.

- `821C4870/s` **sobe** quando o frame **não termina** a tempo — milhões de chamadas a ~0,1 µs são **baratas**; o tempo real está em **`FRAMEUPD` (8249E44C)** e render.
- Otimizar o relógio lock-free **ajudou** (43–47 Hz), mas não remove o déficit de **~10–15 ms** por frame em carga pesada.

**Causalidade:** poll alto **segue** frame lento; raramente **causa** frame lento após lock-free.

### F3 — Custo por frame dominado por dupla: guest ARM64 + Vulkan record CPU

Correlação no mesmo log (`perf_post_initfix_150s.log`):

| Regime | PACE | SCENEDISP/s | draws/present | record_us_avg | FRAMEUPD avg µs |
| --- | ---: | ---: | ---: | ---: | ---: |
| Saudável | 59,7 | 1519 | 181 | 2665 | 1615 |
| Degradado | 25,9 | 727 | **306** | **8370** | **34157** |
| Windows @ alta cena | 60 | **1895** | (não medido) | — | **~6500** max |

Windows sustenta **SCENEDISP >1800/s** a 60 Hz; Android com **SCENEDISP similar** pode estar a 60 Hz **se** draws/present e trabalho guest couberem em ~16 ms.

**Inferência:** a recompilação ARM64 + gravação Vulkan no host **não escala** como x64 Windows para o mesmo título — não é “emulador genérico”, mas o **código gerado + backend Vulkan** ainda pagam fator ~2–4× no loop de frame.

### F4 — Memória guest: padrão codegen correto para Android, porém caro por natureza

Em Android, `REX_PHYS_HOST_OFFSET` é **0** (`sonic4_pch.h`) — alinhado a Pinyon Shift pós-correção de mapeamento 4K.

Cada load/store permanece: `volatile` + `__builtin_bswap*` — necessário para semântica Xbox. **Não** há fast-path ausente óbvio sem mudança de codegen global (projeto Pinyon investe anos nisto).

### F5 — Threading: competição por CPUs, pinning ineficaz no F7

`top` histórico: XThread/Main **~97%**, Audio Worker **~73%**, GPU Commands **~53–72%**.  
`android_thread_cores=auto` não ativou regra Pinyon (só **cpu7** @ 3,2 GHz no topo) — `perf_thread_cores_auto_120s.log`.

**Hipótese secundária:** sem isolamento audio/GPU/main, stutter de áudio segue PACE guest.

### F6 — Global critical region (runtime genérico)

`function_dispatcher::ExecuteInterrupt` e vários subsistemas usam `global_critical_region_`. Pinyon documenta centenas de contenções/frame em títulos pesados no Android. **Sem contagem neste título ainda** — candidato a `REXGLUE_ENABLE_PERF_COUNTERS` + próximo `simpleperf`.

### F7 — Métricas GPU ausentes até 17/11

`gpu_exec_ns=na` impedia separar “CPU record lento” vs “GPU lenta”. **Implementado:** query pool `VK_QUERY_TYPE_TIMESTAMP` em build diag (`gpu_exec_us_avg` em `PROBE DIAG_VK`).

---

## 3. O que **não** explica o teto sustentado

| Hipótese | Evidência contra |
| --- | --- |
| Falta `-O3`/LTO | Já aplicado; ganho marginal |
| Relógio guest mutex-bound pós lock-free | Poll ainda alto mas não dominante em μs/chamada |
| SCENEDISP “anormalmente alto” vs Windows | Windows **mais alto** a 60 Hz |
| underrun SDL | ~0 com stutter audível |
| Device “fraco” | POCO F7 + jogo simples; **implementação** não atinge orçamento de 16 ms |

---

## 4. Oportunidades alinhadas a projetos externos (transferíveis)

| Técnica (Pinyon / Xenia) | Aplicável aqui? | Prioridade |
| --- | --- | --- |
| Diag só em build dedicado | Sim — **feito** | P0 |
| `simpleperf --call-graph fp` em libmain.so | Sim | P0 |
| Redução binds/barriers Vulkan | Sim — investigar vs Windows | P1 |
| Relaxar global lock / LR paths | Parcial — requer análise por título | P1 |
| Melhorias codegen (CR, indiretas, hot regions) | Sim — longo prazo | P1 |
| Turnip / driver | Opcional | P2 |

---

## 5. Plano de medição A/B (metodologia fixa)

1. **Perf APK:** `build-android-arm64.bat` (`REX_DIAG_PROFILE=OFF`).  
2. **Diag APK:** `build-android-arm64-diag.bat` (sondas + GPU timestamps).  
3. Mesmo save, Splash Hill, 150 s, extrair `files/user/logs/sonic4_*.log`.  
4. `python evidence/ANDROID_PORT/tools/summarize_probe_log.py <log>`.  
5. Comparar com `perf_post_initfix_150s.log` **somente** após anotar se build tinha diag ON/OFF.