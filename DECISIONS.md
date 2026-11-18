# DECISIONS.md — decisões, justificações, alternativas e dúvidas abertas

Formato ADR simplificado. Estado: **Decidido** / **Pendente** / **Descartado**.
Cada decisão indica a evidência que a suporta.

---

## D1 — Base de port: ReXGlue SDK (Decidido, sujeito a validação prática)

**Decisão:** usar `rexglue/rexglue-sdk` v0.10.0 como recompilador **e** runtime; consumir o contentor STFS
diretamente e gerar um projeto com `rexglue init` + `rexglue codegen`.

**Porquê (evidência):**
- É a única base que traz as peças difíceis prontas: kernel/XAM HLE, GPU D3D12/Vulkan (do Xenia),
  áudio XMA (FFmpeg), input SDL3, VFS — verificado em `third_party/rexglue-sdk/{src,include}`.
- Extração STFS/XEX/AES/LZX **nativa** (`stfs_container_device.h`, `lzx.cpp`, `xex_module.cpp`,
  `xboxkrnl_crypt.cpp`) — dispensa ferramentas externas. *(Correção T1: ver Nota abaixo — a extração
  STFS do contentor não está exposta no CLI v0.10.0; AES/LZX do XEX sim, na cadeia de load.)*
- Deteção automática de jump-tables, vtables/RTTI e ABI-helpers — precisamente as lacunas que forçaram
  o trabalho manual no projeto anterior.
- Evidência de execução em títulos **XBLA** (mesma classe do piloto) e funil quantificado no playbook
  360tools (extracção ~97%, codegen ~99% com hints, em 747 títulos).
- Licença permissiva (BSD-3-style) → integração sem obrigações copyleft.
- **Nota (2026-10-19, T1):** a extração **STFS** existe como *biblioteca* (`StfsContainerDevice`)
  mas **não está exposta** no CLI v0.10.0 nem no monte `game:` (`HostPathDevice`) — na prática a
  extração do contentor é externa (T3b). A extração/descodificação **do XEX** (AES retalho + LZX)
  está na cadeia de load e é feita pelo SDK (`xboxkrnl_crypt.cpp`, `lzx.cpp`, `xex_module.cpp`).

**Alternativas descartadas:**
- *XenonRecomp + runtime próprio* (o caminho de `C:\SARTRecomp`): o gargalo nunca foi a tradução PPC,
  foi o runtime. Repetir isso é repetir o custo mais alto. **Descartado.**
- *Fork de UnleashedRecomp*: GPL-3.0, e todo o valor está amarrado ao Unleashed (paths, hooks, assets).
  **Descartado** como base; mantido como referência.
- *Xenia como "port"*: é emulação (JIT), não port nativo. **Descartado** (fora do objetivo).

**Risco aceite:** o SDK está em desenvolvimento ativo e a API muda. Mitigação: fixar v0.10.0, manter o
clone local como fonte de verdade, e não misturar código de terceiros sem licença clara.

---

## D2 — `C:\SARTRecomp` é referência, não fornecedor de código (Decidido)

**Decisão:** não importar código, configs ou artefactos do workspace anterior. Usá-lo apenas para
lições e para comparação de abordagens.

**Porquê:** o seu runtime mínimo foi construído contra a recomendação do XenonRecomp (MSVC-only),
com correções manuais de instruções (16 065 shims) que o ReXGlue faz por deteção automática; reutilizá-lo
arrastaria dívida arquitetural. O workspace também está fora do escopo de escrita desta fase.

---

## D3 — Não instalar nada nesta fase (Decidido — instrução explícita do utilizador)

**Decisão:** concluir inventário + pesquisa + documentação e **parar**. A instalação de dependências
é a próxima tarefa, com justificação já apresentada (`TOOLS.md` §2 / §4).

**Instalações bloqueantes identificadas:** **Clang 18+ (LLVM)** (SDK) e **Python 3** (extração T3b — correção T1).
- O `CMakeLists.txt` do SDK falha explicitamente sem Clang ≥ 18 e usa C++23.
- O sistema só tem `clang-format`/`clang-tidy` (sem `clang.exe`/`clang++`).
- Caminho preferido (Windows): componente VS 2022 `Microsoft.VisualStudio.Component.VC.Llvm.Clang`
  (+ `…VC.Llvm.ClangToolset`); alternativa: LLVM standalone ≥ 18 (o playbook 360tools usou 21.1.8).
- **Não bloqueante / opcional:** `pycryptodome` (apenas alguns scripts de triagem do 360tools).
- **Correção (2026-10-19, T1):** **Python 3 passa a necessário** — não para o SDK, mas para a
  **extração STFS** (`360tools/tools/extract_stfs.py`, stdlib-only), porque o CLI rexglue v0.10.0
  não expõe extração (ver dúvida 2 e `NEXT_STEPS.md` T3b).

---

## D4 — Extração já feita com ferramentas do sistema (Decidido e executado)

**Decisão:** extrair apenas o contentor STFS do ZIP com `System.IO.Compression` (.NET) — sem instalar
7-Zip/outras ferramentas; preservar a árvore de ficheiros original.

**Porquê:** a extração era necessária para identificar o formato real, e é não destrutiva. Verificado que
o ZIP não foi alterado (`LastWriteTime` inalterado antes/depois).

**Consequência:** `game/58410A07/000D0000/11A114D9…` existe localmente, git-ignored, **não distribuível**.

---

## D5 — Não construir um analisador STFS próprio (Decidido)

**Decisão:** a leitura estrutural do STFS fica a cargo do ReXGlue (v0.10.0 traz
`stfs_container_device` + `vfs_dump`). Os nossos scripts Node são **apenas triagem heurística**
(magics, optional headers do XEX, nomes de ficheiros) e estão identificados como tal.
**Nota (T1):** `vfs_dump` está morto no CLI v0.10.0 e o device não está ligado ao `game:` — se a
extração via Python falhar (T3b), escrever uma ferramenta mínima que **reutilize** `StfsContainerDevice`
do SDK (continua a cumprir esta decisão: não escrever parser STFS próprio).

**Porquê:** implementar LZX/AES/hash-tables do STFS de raiz seria trabalho não reutilizável e sujeito a
erros; o SDK já o faz e é a mesma biblioteca que vai carregar o jogo.

---

## D6 — Não usar MSVC para o código gerado (Decidido)

**Porquê:** o ReXGlue exige Clang (>= 18) por design; o XenonRecomp declara que compiladores que não Clang
"have not been tested and are not recommended, **including for recompilation output**". No projeto
anterior, a escolha MSVC-only coincidiu com milhares de sites `// ERROR` e com correções manuais.
Mantém-se MSVC apenas para o que for nativo do Windows (headers/D3D12/VS dev shell).

---

## D7 — Não distribuir nada do jogo (Decidido)

`game/` (e qualquer artefacto com dados do jogo) fica em `.gitignore` e nunca sai desta máquina.
`evidence/` contém apenas metadados e **nomes** de ficheiros produzidos pela nossa análise.

---

## Dúvidas abertas (por responder antes/durante a recompilação — T1 2026-10-19, T2 2026-10-19; ver `evidence/T2_AUDIT.md`)

1. **Flags exatas do `rexglue init` na v0.10.0** (`--app_name/--app_root` na wiki vs
   `--project-name/--xex-path/--game-root/--project-root` no playbook v0.8.0). Resolver lendo
   `src/rexglue/commands/init_command.cpp` (já clonado; a v0.10 moveu os comandos para `commands/`) — **sem instalar nada**.
   → **✅ RESPONDIDO (2026-10-19, T1):** grafia real = **hífen**:
   `--project-name/--xex-path/--game-root/--project-root/--scan-dll/--template-dir`
   (`src/rexglue/commands/init_command.cpp:566-581`; nota: os ficheiros mudaram para
   `src/rexglue/commands/` na v0.10). `--app_name/--app_root` da wiki **não existem** neste CLI
   (as mensagens de erro citam underscores, mas o registo é com hífen). Comandos exatos
   documentados em `ARCHITECTURE.md` §4.
2. **O `init` aceita o caminho do contentor STFS diretamente, ou exige um `default.xex` já extraído?**
   O 360tools afirma "rexglue reads XEX/STFS directly"; confirmar no código/CLI.
   → **✅ RESPONDIDO (2026-10-19, T1): NÃO aceita o contentor.** Evidência: `init` exige ficheiro
   regular (`init_command.cpp:112-115`) dentro de `--game-root` (`:133-138`); codegen/app carregam
   `game:\…` com `game:\default.xex` hardcoded (`project_recompiler.cpp:251-254`, `rex_app.cpp:262`);
   o loader só aceita XEX1/XEX2/ELF (`user_module.cpp:134-145`, `xex_module.cpp:870-876`) e `game:`
   é um diretório host (`runtime.cpp:300-318`). Extração STFS **obrigatória**; o CLI v0.10.0 não a
   expõe (só `init`/`codegen`/`recompile-tests`, `main.cpp:82-84`; `vfs_dump` morto,
   `vfs_dump.cpp:118`) → nova tarefa **T3b** em `NEXT_STEPS.md`.
3. **O fix dos symlinks do `libmspack` ainda se aplica na v0.10.0?** (era gotcha na v0.8.0).
   → **✅ RESPONDIDO e CORRIGIDO (2026-10-19, T2+T4): NÃO na v0.10.0.** O gotcha dos symlinks `120000`
   era da **v0.8.0** (documentado no 360tools v0.8.0). Na **v0.10.0** o `libmspack` é um **submodule git**
   (`thirdparty/libmspack` → `github.com/kyz/libmspack`), tal como outros 21 submodules, e o CMake
   **compila `libmspack/cabextract/mspack/lzxd.c`** diretamente (`thirdparty/CMakeLists.txt:127-137`).
   **Não há symlink a corrigir.** Correção real (T4) = `git submodule update --init --depth 1` dos 12
   submodules exigidos no Windows/D3D12. Ver `evidence/T2_AUDIT.md` §1 (a versão anterior desta resposta
   estava errada — descrevia os symlinks v0.8.0 como se fossem v0.10.0).
4. **Áudio CRIWARE**: o jogo usa `CPK/CSB`; o runtime traz áudio **XMA** via FFmpeg. É preciso trabalho
   dedicado para CRIWARE, ou os bancos contêm XMA/ADX que o FFmpeg já lê? (impacto: áudio pode ficar em falta no MVP).
5. **Shaders**: confirmar que a tradução em runtime (herança Xenia) cobre os shaders deste título
   (não correr XenosRecomp sem necessidade).
   → **✅ CORRIGIDO (2026-10-19, T2): a tradução NÃO é em runtime.** O pipeline do SDK transpila
   HLSL SM3 do guest para **SPIR-V em tempo de build** (`rexglue shaders`/auto via build.py;
   `commands/shader_transcode_command.cpp`), gerando o sidecar **`shaders.xex`**
   (`rexglue_app.cpp:315-333`); o runtime só consome SPIR-V (DXC/glslang são ferramentas de build).
   A cobertura dos shaders do título é a PENDENTE restante — só se confirma na primeira execução
   (T6/T7), via `shaders.xex` + log. Ver `evidence/T2_AUDIT.md` §2.
6. **`.AMB/.AMA/.YSD`**: o código do jogo lê-os sozinho (esperado) — confirmar no primeiro arranque que
   não é preciso um "VFS shim" para estes formatos.
   → **(2026-10-19, T2) Re-classificada como EXPERIÊNCIA DE PRIMEIRO ARRANQUE (T7), não bloqueio de T3–T6.**
   O SDK monta `game:\` como VFS read-only genérica (`runtime.cpp:300-318`) e a doc admite
   "extraction and first-boot conversion" — a conversão é específica do título e só se observa no run.
7. **Base de imagem**: `0x82000000` é a base suportada; confirmar que não há segunda imagem/overlay
   (o XEX tem `DeltaPatchDescriptor`; verificar se é preciso o *title update* para o boot).
   → **✅ RESPONDIDO (2026-10-19, T2): NENHUM title update é necessário nem exigido.**
   `DeltaPatchDescriptor`/`ApplyDeltaPatch` (`xex_module.cpp:975-1001`) são 100% opcionais — só
   ativam por `RexModuleConfig::delta_patches` (campo inexistente no manifest `.rex`; nenhum exemplo
   no SDK; nenhum delta no contentor). Deltas aplicam-se apenas a versões do jogo **mais novas** que a
   baseline; a nossa cópia é RTM (`default.xex` é o único XEX no contentor — `TitleInfo Path="default.xex"`).
   A ausência não bloqueia compilação, init nem run. Ver `evidence/T2_AUDIT.md` §3.
