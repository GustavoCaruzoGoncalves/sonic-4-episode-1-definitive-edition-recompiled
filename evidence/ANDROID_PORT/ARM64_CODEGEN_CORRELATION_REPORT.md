# ARM64 codegen correlation report

**Date:** 2026-11-17  
**Scope:** static correlation of the ARM64 `libmain.so` hot-symbol disassembly with XenonRecomp-generated C++ memory macros.  
**Constraint:** this is measurement/analysis only. No engine or recomp optimization was introduced, and the reverted `PipelineDescription` cache was not restored.

---

## 1. Question

Determine whether the ARM64 recompiled guest code has a plausible structural source of extra CPU cost on POCO F7, without assuming that a compiler flag or a Vulkan optimization is responsible.

---

## 2. Evidence sources

| Artifact | Content |
| --- | --- |
| `raw/arm64_static/disasm_hot.txt` | `objdump`-style ARM64 disassembly of 8 hot recompiled functions from the shipping APK's `libmain.so` |
| `tools/summarize_arm64_disasm.py` | Instruction-mix summarizer |
| `raw/arm64_static/codegen_hot_functions.csv` | Generated-source correlation for the same 8 symbols |
| `tools/correlate_codegen.py` | Correlator over `pilot/generated/default/sonic4_recomp.*.cpp` |
| `pilot/generated/default/sonic4_pch.h` | Endian/physical-memory macro definitions |

Function names are host import thunks such as `__imp__sub_821C62C8`; the generated functions are `DEFINE_REX_FUNC(sub_821C62C8)`.

---

## 3. ARM64 instruction mix

Sample size: **2084 instructions across 8 functions**.

| Category | Count | Share |
| --- | ---: | ---: |
| Store | 562 | 26.97% |
| Integer/compare | 468 | 22.46% |
| Load | 358 | 17.18% |
| Move/barrier/other | 318 | 15.26% |
| Control-branch | 158 | 7.58% |
| Byteswap | 153 | 7.34% |
| FP/SIMD | 44 | 2.11% |
| Other | 19 | 0.91% |
| Control-indirect | 4 | 0.19% |

Top mnemonics:

| Mnemonic | Count | Share |
| --- | ---: | ---: |
| `str` | 326 | 15.64% |
| `ldr` | 303 | 14.54% |
| `mov` | 258 | 12.38% |
| `add` | 213 | 10.22% |
| `strb` | 164 | 7.87% |
| `rev` | 152 | 7.29% |
| `cset` | 103 | 4.94% |
| `cmp` | 73 | 3.50% |
| `stp` | 66 | 3.17% |
| `bl` | 60 | 2.88% |

Per-function highlights:

| Function | Instructions | Byteswap | Load | Store |
| --- | ---: | ---: | ---: | ---: |
| `sub_82197F28` | 497 | 35 (7.0%) | 87 (17.5%) | 150 (30.2%) |
| `sub_8249E2F8` | 428 | 33 (7.7%) | 70 (16.4%) | 97 (22.7%) |
| `sub_82223E18` | 10 | 2 (20.0%) | 3 (30.0%) | 2 (20.0%) |
| `sub_8249CF80` | 363 | 31 (8.5%) | 58 (16.0%) | 98 (27.0%) |
| `sub_821C4870` | 152 | 12 (7.9%) | 33 (21.7%) | 41 (27.0%) |
| `sub_821A8488` | 150 | 6 (4.0%) | 21 (14.0%) | 47 (31.3%) |
| `sub_821C62C8` | 219 | 20 (9.1%) | 44 (20.1%) | 61 (27.9%) |
| `sub_821D5478` | 265 | 14 (5.3%) | 42 (15.8%) | 66 (24.9%) |

Reproduce:

```powershell
python evidence\ANDROID_PORT\tools\summarize_arm64_disasm.py evidence\ANDROID_PORT\raw\arm64_static\disasm_hot.txt
```

---

## 4. Generated-source correlation

Counts are lexical occurrences inside each generated function body. They are lower bounds for actual memory traffic because helper macros may expand to additional instructions.

| Function | Translation unit / line | Generated lines | Load macros | Store macros | Guest calls | CR compares | Timebase |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `sub_82197F28` | `sonic4_recomp.11.cpp:7767` | 383 | 37 | 17 | 10 | 16 | 0 |
| `sub_8249E2F8` | `sonic4_recomp.12.cpp:36111` | 267 | 29 | 8 | 14 | 3 | 0 |
| `sub_82223E18` | `sonic4_recomp.23.cpp:13258` | 8 | 2 | 0 | 0 | 0 | 0 |
| `sub_8249CF80` | `sonic4_recomp.26.cpp:35852` | 230 | 25 | 18 | 8 | 7 | 0 |
| `sub_821C4870` | `sonic4_recomp.47.cpp:9300` | 116 | 11 | 5 | 2 | 6 | 0 |
| `sub_821A8488` | `sonic4_recomp.49.cpp:8258` | 115 | 6 | 4 | 2 | 6 | 0 |
| `sub_821C62C8` | `sonic4_recomp.50.cpp:9144` | 137 | 16 | 7 | 3 | 8 | 1 |
| `sub_821D5478` | `sonic4_recomp.58.cpp:9782` | 142 | 11 | 7 | 3 | 3 | 0 |

Reproduce:

```powershell
python evidence\ANDROID_PORT\tools\correlate_codegen.py
```

---

## 5. Worked example: `sub_821C62C8`

`pilot/generated/default/sonic4_recomp.50.cpp:9144`:

- 137 generated lines,
- 16 load macros,
- 7 store macros,
- 3 guest calls,
- 8 condition-register compares,
- 1 timebase query.

Representative generated operations:

```cpp
REX_STORE_U32(ea, ctx.r1.u32);
ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 10896);
ctx.cr6.compare<uint32_t>(ctx.r9.u32, ctx.r11.u32, ctx.xer);
ctx.r11.u64 = REX_QUERY_TIMEBASE();
```

The corresponding ARM64 sample has 219 instructions, including 61 stores, 44 loads, 20 byteswaps, 14 control branches, repeated `strb`, and `rev` instructions. This is a direct structural link between generated guest-memory operations and emitted ARM64 instructions.

---

## 6. Endianness conversion

`pilot/generated/default/sonic4_pch.h` defines 16/32/64-bit guest loads and stores with byte swaps:

```cpp
#define REX_LOAD_U32(x)  __builtin_bswap32(*(volatile u32*)(base + (u32)(x) + REX_PHYS_HOST_OFFSET(x)))
#define REX_STORE_U32(x,y) (*(volatile u32*)(base + (u32)(x) + REX_PHYS_HOST_OFFSET(x)) = __builtin_bswap32(y))
```

On Android, `REX_PHYS_HOST_OFFSET(addr)` is `0u`; on the tested Windows host it is nonzero for the `>=0xE0000000` region. The byte swap remains in both paths.

The generated-source CSV reports `bswap_calls_in_body=0` because the swap is hidden inside macros defined in the PCH. The disassembly shows the emitted consequence: **153 byteswap instructions (`rev`/`rev16`/`rev32`/`rev64`) in the 8-function sample**, 152 of them plain `rev`.

This is a plausible source of ARM64-side integer work in addition to the load/store traffic. It is not, by itself, a measurement of how many cycles it costs on the POCO F7.

---

## 7. Interpretation

Supported by this evidence:

- The hot recompiled functions are **memory-operation intensive**: load + store categories account for **44.15%** of the sampled instructions.
- Endian conversion is materially present: **7.34%** of sampled instructions are byteswaps.
- The generated source and disassembly agree: `REX_LOAD_*`/`REX_STORE_*` heavy functions map to `ldr`/`str`/`strb`/`rev` heavy ARM64 code.
- `sub_821C62C8`, previously identified by dynamic profiling as a high-overhead pacing/wait function, has both a timebase query and dense guest-memory traffic in generated code.

Not established by this evidence:

- The fraction of total frame time attributable to these instructions versus Vulkan CPU recording or driver/scheduling.
- Whether Clang is generating optimal addressing for the guest-memory arena.
- Whether eliminating or reshaping byte swaps would improve sustained PACE.

The prior dynamic decomposition remains the basis for cost separation: GPU execution was measured at roughly **0.8–2.6 ms** in diagnostic builds, while Vulkan CPU record and guest-side work were larger in heavy scenes. This static report explains **why ARM64 guest code is a credible contributor**, not how much it contributes.

---

## 8. Changes made

None to retail/engine code. Added only evidence tooling and reports:

- `tools/correlate_codegen.py`
- `tools/summarize_arm64_disasm.py`
- `raw/arm64_static/codegen_hot_functions.csv`
- `raw/arm64_static/disasm_hot.txt`

No pipeline cache was reintroduced. No D3D12, Vulkan, Android, audio, touch, or save behavior was changed.
