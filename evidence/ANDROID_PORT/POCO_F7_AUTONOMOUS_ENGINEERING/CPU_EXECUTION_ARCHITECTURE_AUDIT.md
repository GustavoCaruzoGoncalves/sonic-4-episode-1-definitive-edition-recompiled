# Auditoria da arquitetura de execução CPU — POCO F7 / ARM64

**Data:** 2026-11-17  
**Dispositivo:** POCO F7, `arm64-v8a`, `com.s4ep1recomp.sonic4`  
**Binários:** `libmain.so` (recompilado), `librexruntime.so`, `librexgpu-xenos.so`

---

## Resumo executivo

| Pergunta | Conclusão |
| --- | --- |
| O código CPU do jogo é recompilado estáticamente para ARM64? | **Sim.** ~12 864 funções guest mapeadas para símbolos `sub_*` nativos em `libmain.so` (ELF64 AArch64). |
| Há interpretação PowerPC durante gameplay? | **Não no caminho quente.** Não existe interpretador PPC no runtime Android; amostras CPU dominam funções `sub_*` geradas. |
| Há JIT ou tradução dinâmica de PPC? | **Não.** `rexglue` / codegen roda só no build. `FunctionDispatcher::Execute` &lt; 0,1 % no perfil. |
| O que mais consome CPU? | Funções guest nativas (`sub_821C62C8` ~7,5 %, `sub_821C4870` ~5,9 %), `QueryGuestTickCount`, workers GPU/Vulkan e kernel Android. |
| “Interpretação” existe em algum lugar? | **GPU apenas:** `ShaderInterpreter` = microcódigo de **shader Xenos**, não CPU PPC. |

A hipótese de que o port “ainda emula” instruções PPC no CPU **é refutada** para execução normal. O custo vem de **código ARM64 gerado** + **runtime host** (relógio, memória guest, MMIO, Vulkan), não de um loop decode-dispatch PPC.

---

## 1. Pipeline de execução (CPU)

```
default.xex (PowerPC no Xbox 360)
    → rexglue codegen (build host: sonic4_manifest.toml)
    → pilot/generated/default/sonic4_recomp.*.cpp (C++ com macros REX_LOAD_*/REX_CALL_*)
    → clang-NDK -O3 → libmain.so (AArch64)
    → runtime: XThread inicia em endereço guest → GetFunction(addr) → ponteiro PPCFunc
    → chamada direta: fn(PPCContext&, uint8_t* base)  // sem decode de opcode PPC
```

**Entrada do processo:** `pilot/src/main.cpp` → `REX_DEFINE_APP` → runtime carrega XEX, registra tabela (`sonic4_register.cpp` / `sonic4_init.cpp`).

**Tabela de funções:** `PPCFuncMappings[]` em `pilot/generated/default/sonic4_init.cpp` — **12 864** entradas `{ guest_va, sub_* }`.

**Comentário explícito no runtime:**

```600:616:third_party/rexglue-sdk/src/system/xthread.cpp
  // NOTE(tomc): JIT execution replaced with direct function calls
  // In rexglue, guest code is compiled ahead of time and called directly.
  PPCFunc* func = dispatcher->GetFunction(address);
```

**Dispatch de função (não é interpretador):**

```204:226:third_party/rexglue-sdk/src/system/function_dispatcher.cpp
  PPCFunc* fn = GetFunction(address);
  ...
  fn(*ctx, memory_->virtual_membase());
```

Chamadas **indiretas** (`bctr`) usam tabela por módulo ou `ResolveIndirectFunction` → outro `PPCFunc*` nativo (`sonic4_pch.h`, `REX_CALL_INDIRECT_FUNC`). Não há passo por opcode.

**Fallback:** `InvalidFunctionTrap` apenas se o endereço **não está registrado** — não aparece no top de 192 288 amostras `simpleperf` em gameplay.

**Casos especiais (não são interpretador):**

- *Tail thunks* PDATA-less (`function_dispatcher.cpp`, `GuestWord`) — poucos saltos registrados em log, leitura de palavras guest para resolver destino.
- Thunks `__imp__sub_*` em `sonic4_timing_hooks_retail.cpp` — delegam ao corpo recompilado ou hooks de pacing.

---

## 2. Auditoria de código-fonte — mecanismos suspeitos

| Mecanismo | Arquivo | CPU PPC? | No APK Android? | Gameplay? |
| --- | --- | --- | --- | --- |
| Interpretador PPC | *Ausente em `rexglue-sdk/src`* | — | Não | Não |
| `FunctionDispatcher` | `function_dispatcher.cpp` | Dispatch para nativo | Sim (`librexruntime`) | Sim, baixo overhead |
| `rexcodegen` / `rexglue` | build-time | N/A | Ferramenta de build | Não no dispositivo |
| `ShaderInterpreter` | `graphics/pipeline/shader/interpreter.cpp` | **GPU shader ucode** | `librexgpu-xenos` | Sim (tradução shader Xenos) |
| Berberis / QEMU / Houdini | projeto | N/A | **Não linkado** | Não |
| ART `art::interpreter::*` | framework Android | Dalvik/ART Java | Sistema | &lt; 0,02 % (JNI bridge) |

Comentários `TODO: Translate JIT methods` em `kernel/CMakeLists.txt` referem-se a **exports do kernel Xbox** ainda não recompilados, não a JIT de gameplay.

O clone **Pinyon-Shift** em `evidence/.../research_clones/` contém `rex/ppc/interpreter.h` para **verificação offline** — **não faz parte** do APK Sonic4.

---

## 3. Prova estática — `libmain.so`

- **Formato:** `llvm-readelf -h libmain.so` → `ELF64`, `Machine: AArch64`.
- **Símbolos:** `llvm-nm` lista **24 893** símbolos contendo `sub_` (funções recompiladas + PLT).
- **Exemplo quente:** `sub_821C62C8` em `0x00d116d4` — prologue AArch64 (`stp`, `ldr`, `rev` para endian), não bytecode PPC.

Correlação macro ↔ ARM64: `ARM64_CODEGEN_CORRELATION_REPORT.md` — ~7–27 % instruções `rev` (byteswap) em funções quentes; `REX_PHYS_HOST_OFFSET` = **0** no Android (`sonic4_pch.h`).

---

## 4. Prova dinâmica — `simpleperf` (POCO F7)

**Captura:** `raw/arch_audit/perf_arch.data` — 15 s, `cpu-clock`, call-graph FP, PID gameplay, **192 288** amostras.

### Top símbolos (guest vs runtime)

| Overhead | Símbolo | Categoria |
| ---: | --- | --- |
| 7,51 % | `sub_821C62C8` | **Guest ARM64 recomp** (pacing/wait) |
| 5,87 % | `sub_821C4870` | **Guest ARM64 recomp** (spin/poll) |
| 3,27 % | `rex::chrono::Clock::QueryGuestTickCount()` | Runtime relógio |
| 0,63 % | `__savegprlr_29` / `__restgprlr_29` | Prologue/epilogue recomp |
| 0,06 % | `FunctionDispatcher::Execute` | Dispatch tabela |
| 0,13 % | `FunctionDispatcher::ExecuteInterrupt` | Callbacks GPU/kernel |
| 0,00 % | `InvalidFunctionTrap` | *(ausente do ranking)* |

Demais top: `__schedule`, `clock_gettime`, mutex futex, `CommandProcessor::WriteRegister`, `MMIOHandler::CheckLoad` — **host + GPU recording**, não interpretação PPC.

Soma aproximada dos `sub_*` visíveis no relatório: **&gt; 15 %** de amostras em threads guest (`XThread*`); restante é runtime Vulkan/scheduling/Android.

**Interpretador Java (ART):** símbolos `art::interpreter::*` &lt; 0,02 % — ruído do framework ao redor da Activity, irrelevante para Xbox guest.

---

## 5. Comparação com Xendroid+ / Xenia

| Projeto | CPU do jogo |
| --- | --- |
| Xendroid+, Xeo, Xenia Canary | **JIT PPC** (ex.: xbyak_aarch64 no Xeo) — tradução **em tempo de execução** |
| Sonic4 Recomp (este projeto) | **AOT:** PPC→C++→ARM64 antes do APK; execução = chamadas de função |

Técnicas Xendroid (sleep em workers, FIFO present, cópias MSAA) aplicam-se ao **runtime Vulkan/kernel**, não substituem o motor CPU.

Referência POCO F7 publicada: Sonic Unleashed ~50–52 fps (emulador JIT). Nosso título: ~60 Hz em cena leve, ~38–48 Hz em carga pesada medida hoje — gap é **eficiência do caminho recomp + GPU CPU**, não presença de interpretador.

---

## 6. Overhead arquitetural mensurável (não “emulação”)

| Componente | Evidência | Otimizável? |
| --- | --- | --- |
| Loads/stores + `rev` no guest | Static + perf `sub_*` | Sim (codegen/macros, funções hot) |
| Poll loop `821C4870` / `821C62C8` | 13 %+ perf | Sim (relógio, yields — **Exp-001 cntvct** em andamento) |
| `QueryGuestTickCount` | 3 %+ perf | Sim (contador ARM64 vs `clock_gettime`) |
| Indirect `bctr` + `ResolveIndirectFunction` | Poucas amostras no top | Monitorar; provavelmente secundário |
| GPU command translation | threads `GPU Commands` | Vulkan/Xenos — separado do CPU PPC |
| `ShaderInterpreter` | librexgpu | Só se shader não tiver tradução SPIR-V em cache |

---

## 7. Conclusões numeradas (deliverable)

1. **CPU estático ARM64?** Sim — prova: 12 864 mappings, disassembly AArch64, perf dominado por `sub_*`.
2. **Interpretação PPC em gameplay?** Não detectada; sem fonte de interpretador no SDK Android.
3. **JIT PPC?** Não; JIT no perfil é apenas ART (Java).
4. **Mecanismos que mais custam CPU:** funções recompiladas de pacing; relógio guest; GPU MMIO/registros; scheduler.
5. **Overhead da arquitetura recomp:** prologue helpers, byteswap por acesso, polls apertados — **não** custo de decode PPC.
6. **Otimizar sem quebrar compat:** relógio ARM64, pacing host, hotspots guest isolados, Vulkan CPU (sem reintroduzir cache `PipelineDescription` incompleto).
7. **Novidade desta auditoria:** confirmação **dinâmica** de que `FunctionDispatcher::Execute` é marginal e que a suspeita de “emulador disfarçado” no CPU **não se sustenta**; o trabalho restante é **qualidade/eficiência do ARM64 gerado** + runtime host.

---

## 8. Próximos passos (loop de engenharia ativo)

- Validar **Exp-001** (`cntvct_el0`) com baseline A/B em gameplay pesado.
- `simpleperf` focado em cena estável (&gt;60 s) para `%` guest vs `librexgpu-xenos.so`.
- Continuar referências Xendroid+ para Vulkan/thermal, não para JIT PPC.

**Navegação:** após `adb install -r` + `am start`, aguardar entrada em gameplay para medições.