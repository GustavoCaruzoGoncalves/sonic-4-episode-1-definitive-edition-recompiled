# REFERENCE_PROJECTS.md — análise comparativa dos projetos públicos

Metadados recolhidos via API GitHub em 2026-10-18; detalhes técnicos verificados **no código clonado**
em `third_party/` (fonte primária) ou nos documentos dos próprios projetos.

## 1. Tabela comparativa

| Projeto | Licença | Estrelas (2026-10-18) | Último push | Papel | Evidência real de execução |
|---|---|---|---|---|---|
| [rexglue/rexglue-sdk](https://github.com/rexglue/rexglue-sdk) | "Other" = **texto BSD-3-Clause**, com atribuição ao Xenia (ver §2.1) | 1 114 | 2026-10-02 | **Base escolhida**: recompilador + runtime + toolkit | Sim: portes XBLA/DLC; aviso explícito de "early development" |
| [hedge-dev/XenonRecomp](https://github.com/hedge-dev/XenonRecomp) | MIT | 6 572 | 2025-08-04 | Codegen PPC→C++ "puro" (sem runtime) | Sim, via UnleashedRecomp |
| [hedge-dev/XenosRecomp](https://github.com/hedge-dev/XenosRecomp) | MIT | 502 | 2026-03-12 | Shaders Xenos → HLSL/SPIR-V | Sim, via UnleashedRecomp |
| [hedge-dev/UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) | **GPL-3.0** | 5 222 | 2026-06-29 | Port completo (referência de meta) | Sim: jogo completo jogável em Windows/Linux |
| [xenia-project/xenia](https://github.com/xenia-project/xenia) | custom (API: NOASSERTION; na prática BSD-3-style) | 9 739 | 2026-02-18 | Emulador: fonte das camadas GPU/kernel que o ReXGlue reutiliza | Sim: emulação de milhares de títulos |
| [superheher/rexglue-recomps](https://github.com/superheher/rexglue-recomps) | — (sem licença declarada; 0 estrelas) | 0 | 2026-06-14 | Super-repo + *knowledge base* de recompilação XBLA (piloto South Park) | Relatado pelo próprio README (build jogável em Linux); **não** verificado por nós |
| [sp00nznet/360tools](https://github.com/sp00nznet/360tools) | MIT | 48 | 2026-06-18 | **Playbook** + ferramentas de triagem sobre ReXGlue | Relata triagem de **747 títulos XBLA** com funil quantificado (ver §2.6) |
| [sp00nznet/simpsonsarcade](https://github.com/sp00nznet/simpsonsarcade) | MIT | 9 | 2026-10-14 | Exemplo de porte XBLA pequeno (Simpsons Arcade) | Sim (recomp com ReXGlue) |
| [N64Recomp/N64Recomp](https://github.com/N64Recomp/N64Recomp) | MIT | 8 266 | 2026-05-27 | Linhagem conceptual (recompilação estática) | Sim (Zelda 64 Recompiled) |

Nenhum **porte público de Sonic the Hedgehog 4: Episode I** foi encontrado nas pesquisas desta sessão
(pesquisado: recomp/port/rexglue/XenonRecomp + título + Title ID). Tratar como oportunidade, não como prova de facilidade.

## 2. Análise por projeto

### 2.1 ReXGlue SDK — base escolhida
- Auto-descrição: "converts Xbox 360 PowerPC code into portable C++ that runs natively on modern
  platforms", "heavily rooted on the foundations of Xenia", com codegen AOT (inspirado no XenonRecomp).
- Requisitos verificados no código: **CMake ≥ 3.25, Ninja, C++23, Clang ≥ 18** (VS 2022 com
  "C++ Clang Compiler for Windows" + "MSBuild support for LLVM (clang-cl)"), Windows SDK/D3D12.
- Comandos verificados na árvore local: `src/rexglue/{main,init_command,codegen_command,project_scan,test_recompiler}.cpp`.
- Extração/descodificação **nativa**: `stfs_container_device`, `stfs_xbox`, `lzx.cpp`, `xex_module.cpp`,
  `xboxkrnl_crypt.cpp`, `vfs_dump.cpp`.
- Runtime: `src/kernel/xboxkrnl/*` (inclui `xboxkrnl_usbcam.cpp`), `src/kernel/xam/*`
  (conteúdo/achievements/UI), `src/ui/vulkan/*` (e camada D3D12), áudio via FFmpeg, input SDL3.
- Carência declarada pelo próprio projeto: **não** é "push-button"; é preciso iterar
  (`--force`, hints de funções, stubs) — ver §2.6.
- Licença: o `LICENSE` começa por "Copyright (c) 2026, Tom Clay … Portions of this software are derived
  from the Xenia project … All rights reserved. Redistribution and use in source and binary forms …"
  → texto permissivo de 3 cláusulas (a API classifica como "NOASSERTION" por não ser um SPDX puro).
- **Reutilizável:** quase tudo. **Risco:** projeto jovem (v0.10.0) e em mudança de API.

### 2.2 XenonRecomp (MIT)
- "Does not provide a runtime implementation. It only converts the game code to C++ … Making the game
  work is your responsibility." — confirma a causa do fracasso estrutural do workspace anterior.
- Requer Clang 18+ (avisa que outros compiladores "have not been tested and are not recommended, including
  for recompilation output") → lição direta: o `MSVC-only` do `C:\SARTRecomp` era contra a recomendação.
- Config TOML (as chaves que explicam os problemas vistos no projeto anterior):
  `savegprlr_*`/`restgprlr_*`, `restfpr_*`, `restvmx_*`/`savevmx_*` (endereços ABI),
  `longjmp_address`/`setjmp_address`, `invalid_instructions`, fronteiras explícitas de funções, `patches`.
  Quando estes endereços não estão configurados, um `b` para o **interior** de uma função-vizinho sai
  como `// ERROR` — exatamente o sintoma (16 065 sites) do projeto anterior.
- Os **mismos conceitos** existem no ReXGlue, mas com **deteção automática** de jump-tables,
  vtables/RTTI e ABI-helpers — a razão pela qual não se deve voltar a escrever shims à mão.
- **Reutilizável:** a lista de chaves do TOML como *checklist* de configuração; a convenção
  `PPCContext& + base`; o conceito de tabela de funções no fim da imagem.

### 2.3 XenosRecomp (MIT)
- Converte microcódigo de shaders Xenos em HLSL/SPIR-V.
- No plano atual **não** deve ser necessário: o ReXGlue herda do Xenia a tradução de shaders em runtime.
  (Hipótese com bom suporte documental; rever se aparecerem problemas gráficos.)

### 2.4 UnleashedRecomp (GPL-3.0) — referência de meta, não de base
- Prova que a abordagem (estática, sem emulação) chega a jogo completo e com melhorias (4K, ultrawide, mods).
- Exige `default.xex`, `default.xexp` (title update) e `shader.ar` do jogo; o instalador aceita
  **"containers or images dumped from an Xbox 360"** — o mesmo tipo de artefacto que temos.
- **Copyleft GPL-3.0**: não copiar código para dentro do nosso projeto sem aceitar as obrigações da GPL.
  Pode ser consultado como *referência conceptual*.
- **Reutilizável (conceito):** fluxo de instalação/extracção a partir de contentores;
  organização "recomp lib privada + app".

### 2.5 Xenia (custom/BSD-style)
- Emulador; **não** é uma alternativa de port (interpretação/JIT), mas é a fonte das camadas que o
  ReXGlue reutiliza e a referência semântica para kernel/XAM/GPU.
- **Reutilizável (conceito):** documentação e código de semântica de APIs Xbox 360.

### 2.6 360tools (MIT) — o playbook mais útil para o piloto
- Fluxo oficial: `extract` → `rexglue init` → `rexglue codegen` → `cmake` → exe.
- Factos quantificados (v0.8.0, 747 títulos XBLA):
  - extracção ~**97%** de sucesso;
  - codegen ~**99%** com *auto-resolve* de hints, mas apenas ~**16%** sem hints
    (`UnresolvedCall: 0xTARGET from 0xSITE` → adicionar `[entrypoint.functions]` e repetir);
  - média de ~7 hints por título;
  - o único export de kernel sistematicamente ausente é **`XUsbcam*`** (~26% dos títulos, via XDK partilhado);
    o SDK implementa-o mas deixa-o **fora** do build do runtime → usar `templates/overlay/src/stubs.cpp`.
- Contexto de base de imagem: base não-standard (`0x92000000`: Aegis Wing, Hexic HD, Hexic 2, UNO)
  quebra a tabela de funções do runtime. **A nossa base é `0x82000000` → fora desse grupo de risco.**
- Templates fornecidos: `stubs.cpp`, `dispatch_tolerance.cpp`, `overrides.example.cpp`,
  `crash_diag.cpp`, `manifest-overrides.example.toml`, `game_app.reference.h`.
- Ferramentas de triagem em Python (`extract_stfs.py`, `extract_pe.py`, `xex_info.py`, `lzx_decompress.py`,
  `extract_switch_tables.py`, `parse_xex_imports.py`) marcadas como **superseded** pelo que o próprio
  ReXGlue já faz. Requerem Python 3.8+ e `pycryptodome`.
- **Reutilizável:** o fluxo, os *stubs* por defeito, o harness de auto-resolve e as quotas de expectativa.
  Repositório de autor individual com poucas estrelas → tratar as afirmações como **relatadas**, testáveis,
  não como garantia.

### 2.7 rexglue-recomps (superheher) — referência, com cautela
- Super-repo do porte do **South Park: Let's Go Tower Defense Play!** (XBLA, Title ID `58410931`), com
  `knowledge-base` (submódulo), `third_party/{XenonRecomp,XenosRecomp,rexglue-sdk}` e `south-park-recomp`.
- README afirma: "The SDK-track build is fully playable on the Linux host (boot → menu → match → win →
  save → continue, audio + input, Vulkan)" e que "XenonRecomp is no longer required" no SDK atual.
- 0 estrelas, sem licença declarada, sem evidência independente → **usar como material de estudo**,
  nunca copiar código (sem licença clara).

### 2.8 N64Recomp (MIT)
- Não é de Xbox 360, mas é a linhagem conceptual (recompilação estática AOT + runtime) que inspirou
  XenonRecomp/ReXGlue. Serve para entender a doutrina do modelo.

## 3. O que se aproveita de cada um (decisão prática)

| Vem de | O que usamos |
|---|---|
| ReXGlue SDK | codegen, runtime kernel/XAM, GPU, áudio, input, VFS, extracção STFS/XEX/AES/LZX, CLI |
| XenonRecomp | checklist de configuração (endereços ABI, setjmp/longjmp, fronteiras), convenções de codegen |
| 360tools | fluxo de trabalho, estratégia de auto-resolve de hints, `stubs.cpp` (XUsbcam), harness de triagem |
| UnleashedRecomp | meta/expectativas, ideias de instalação a partir de contentores |
| Xenia | semântica de kernel/XAM/GPU (via ReXGlue) |
| rexglue-recomps | estudo de caso XBLA (sem copiar código nem dados) |
| `C:\SARTRecomp` | **apenas** lições negativas: não reimplementar runtime, não usar MSVC no código gerado, não ignorar a configuração de ABI-helpers |

## 4. Licenças — regra de utilização neste projeto

- **Permitido** integrar como dependência: ReXGlue SDK (BSD-3-style), XenonRecomp/XenosRecomp (MIT),
  360tools (MIT) — mantendo avisos de copyright e atribuição (incl. a atribuição obrigatória ao Xenia
  exigida pelo ReXGlue).
- **Não copiar código**: UnleashedRecomp (GPL-3.0) e rexglue-recomps (sem licença).
- **Nunca** distribuir assets do jogo (ver `README.md` §6 e `.gitignore`).
- Verificar a licença final do SDK e do Xenia no próprio repositório antes de publicar qualquer artefacto.
