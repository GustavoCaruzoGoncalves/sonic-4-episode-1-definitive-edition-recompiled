# Análise de hotspots CPU (ARM64)

**Dispositivo:** POCO F7. **Build de referência diagnóstica:** `perf_post_initfix_150s.log` (Release **com** `REX_DIAG_PROFILE=ON` — ver auditoria F1).  
**Data:** 2026-11-17.

---

## 1. Ranking por tempo de frame (causalidade)

Ordem derivada de janelas de 5 s (`FRAMEUPD`, `DIAG_VK`, `DIAG_GUEST`), não apenas contadores/s.

| Prioridade | Componente | Evidência | Inclusivo em frame degradado | Notas |
| --- | --- | --- | ---: | --- |
| **P0** | Loop de atualização guest `8249E44C` / cena+render Xenos | `FRAMEUPD dur_us avg` **34 156** @ 25,9 Hz vs **1 615** @ 59,7 Hz | ~30+ ms | Inclui simulação + gravação comandos guest |
| **P0** | Gravação Vulkan CPU | `record_us_avg` **8370** @ 306 draws/present vs **2665** @ 181 | 8–11 ms | Escala ~linear com draws/present |
| **P1** | Código recompilado `libmain.so` | `simpleperf` histórico ~**28%** (`ANDROID_VALIDATION`) | — | Precisa novo perfil **sem** diag |
| **P1** | Audio Worker (guest dispatch) | **1,5 ms × 187/s ≈ 26%** de 1 núcleo | Competitivo | Não underrun; atrasa com PACE |
| **P2** | `821C4870` poll | 4–5M/s × 0,1 µs | ~0,5 ms/núcleo | Sintoma |
| **P2** | `libc` `clock_gettime` / scheduler | ~21% simpleperf antigo | Reduzido pós lock-free | Revalidar |
| **P2** | `libllvm-qgl` (shader) | ~10% em spikes PSO | Início de sessão | Não explica 30 Hz sustentado |

---

## 2. simpleperf histórico (`perf_lto.data`, ~6 s)

| Overhead estimado | DSO / área |
| ---: | --- |
| 28% | `libmain.so` |
| 21% | `libc.so` (`clock_gettime`, `schedule`) |
| 16% | kernel |
| 10% | `libllvm-qgl.so` |
| ~10% combinado | `sub_821C62C8`, `sub_821C4870` (inclusive chain) |

**Limitação:** captura curta, build com diag, cena não documentada. **Requer:**  
`simpleperf record -p <pid> --call-graph fp -e cpu-clock -o evidence/ANDROID_PORT/perf_nodiag.data` com APK perf.

---

## 3. Qualidade do código gerado (amostra)

### 3.1 Acesso à memória (`sonic4_pch.h`)

```c
#define REX_LOAD_U32(x) __builtin_bswap32(*(volatile u32*)(base + (u32)(x) + REX_PHYS_HOST_OFFSET(x)))
```

Android: `REX_PHYS_HOST_OFFSET` → 0 (sem penalidade E0 fix do Windows). Cada load paga bswap + volatile — esperado; otimização requer evolução do codegen (Pinyon: -22% instruções host após trabalho de CR/select).

### 3.2 Poll de tempo `sub_821C4870` (`sonic4_recomp.47.cpp`)

- Loops `db16cyc` no PPC **não geram atraso real** no ARM (comentários only / ctr loop curto).
- Corpo faz vários `REX_LOAD_*` + chamada `sub_82223E18` (cadeia de tempo guest).

### 3.3 Compilação guest

- Android: `-O3 -fno-strict-aliasing -flto=thin` em todos os TUs (`rexglue.cmake`).
- `-march=armv8.2-a` via `rexglue_apply_target_settings` no alvo `_recomp`.

**Gap vs Windows:** mesmo IR guest, host x64 Clang MSVC/Clang com LTO geralmente produz IPC melhor; ARM64 Adreno path não foi perfilado com **Spill/regs** ainda.

---

## 4. Hot functions guest (por frequência × custo)

| Símbolo | Papel | Chamadas/s (degradado) | µs/chamada (hook) | Ação sugerida |
| --- | --- | ---: | ---: | --- |
| `821A8488` | Scene dispatch | 727–1452 | scenedisp ~13–22 | Perfil inclusive ARM64 |
| `82197F28` | Scene update | ~mesma ordem | ~6–8 | Idem |
| `8249CF80` | Object update | idem | 12–115 (picos) | Idem |
| `821B35C8` | Viewport | ~880 | baixo | Secundário |
| `821C62C8` | Pacing frame | = PACE Hz | enganoso se poll | Medir sem poll interno |
| `821C4870` | Poll tick | milhões | ~0,1 | Não otimizar isolado |

---

## 5. Próximos passos CPU (ordem)

1. **simpleperf** no APK `REX_DIAG_PROFILE=OFF` — ranking inclusivo real.  
2. Comparar **mesma janela** Win vs Android para `8249E44C` / `SCENEDISP` (GHIDRA + hooks).  
3. Avaliar **RELATIVE** custo de hooks `sonic4_timing_hooks` em builds perf (manter probes leves, não atomics Vulkan).  
4. Codegen: identificar top 20 funções guest por `% cycles` e inspecionar ASM (`llvm-objdump -d libmain.so`).