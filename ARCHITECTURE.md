# ARCHITECTURE.md — arquitetura escolhida e fluxo técnico

## 1. Resumo da decisão

**Base: ReXGlue SDK v0.10.0** (recompilador + runtime integrados), consumindo o contentor STFS do jogo
(com um passo de extração prévio obrigatório — ver §4.0; o SDK não o expõe no CLI). Nada de runtime caseiro na fase 1.

```
ZIP (XBLA/GOD)
   │  (já feito, .NET, não destrutivo)
   ▼
STFS "LIVE" 58410A07/000D0000/11A114D9…        ← C:\S4EP1Recomp\game\…
   │  T3b extração (extract_stfs.py, stdlib) — o CLI rexglue v0.10.0 NÃO extrai STFS
   ├──────────────► default.xex  (XEX2, base 0x82000000, entry 0x82223690)
   │                 (AES retalho + LZX "basic" do XEX: aplicados pelo SDK em LoadXexImage)
   │                      │  `rexglue codegen`  (PPC → C++23)
   │                      ▼
   │                 generated/*.cpp  (+ hints [entrypoint.functions] p/ UnresolvedCall)
   │                      │  CMake + Ninja + clang++ (preset win-amd64-release)
   ▼                      ▼
assets (AMB/AMA/YSD/CPK/CSB) ──►  pilot.exe  ──►  rexruntime.dll
                                        │
                                        ├─ kernel XboxKrnl/XAM (HLE, derivado do Xenia)
                                        ├─ GPU D3D12 (padrão Windows) ou Vulkan (Xenia-based)
                                        ├─ áudio XMA (FFmpeg) · input SDL3 · VFS
                                        └─ guest memory: XEX image + function table gerada
```

## 2. Porque esta arquitetura (e não as alternativas)

| Critério | ReXGlue SDK | XenonRecomp + runtime próprio (trabalho de `C:\SARTRecomp`) | Fork de UnleashedRecomp |
|---|---|---|---|
| Recompilador | incluído (`rexglue codegen`) | incluído (só isto) | incluído |
| Runtime (kernel/XAM) | **incluído**, derivado do Xenia | **escrever de raiz** (o gargalo real) | incluído, mas amarrado ao Unleashed |
| Extracção STFS/XEX/AES/LZX | **incluída** como biblioteca (extração STFS do contentor *não* exposta no CLI v0.10 → passo externo, §4.0; XEX/AES/LZX no load) | não tem | tem (mas orientada a um ficheiro concreto) |
| GPU / shaders | D3D12 **e** Vulkan, herdados do Xenia (tradução de shaders em runtime) | escrever de raiz | específico do Unleashed |
| Deteção de jump-tables/ABI-helper/RTTI vtable | **automática** | manual (causa raiz dos `// ERROR`) | automática |
| Provas de execução real | South Park (XBLA) jogável; ~99% de codegen em 747 títulos XBLA (playbook 360tools) | harness de 1 função; jogo nunca arrancou | jogo completo (Unleashed) jogável |
| Licença | permissiva estilo BSD-3 (com atribuição Xenia) | MIT | **GPL-3.0 (copyleft)** |
| Esforço até "guest code a correr" | menor | **muito alto** | alto (separar do jogo-origem) |

**Conclusão:** a arquitetura recomendada é ReXGlue SDK. É a única opção cujas peças
difíceis (kernel HLE, GPU, áudio, extracção XEX encriptado) já existem e têm evidência de
execução em títulos **XBLA** — a mesma classe do nosso piloto.

**Vantagem específica para este piloto (verificado):** a base de imagem do nosso XEX é
`0x82000000`, que é exatamente a base "usual XBLA" suportada pelo runtime. A classe de títulos
problemática (base `0x92000000`, ex.: Aegis Wing/Hexic/UNO) **não** nos afeta.

## 3. Modelo de execução (o que se espera, a validar no piloto)

- O XEX é carregado e mapeado em **memória guest** num espaço de 32 bits do processo x64.
- O código PPC é traduzido **antecipadamente** (AOT) para C++23; cada função recebe
  `PPCContext&` (registos) e um ponteiro `base` (memória guest 32-bit) — a mesma convenção que o
  projeto anterior confirmou ser a do XenonRecomp (o ReXGlue partilha a mesma linhagem).
- Chamadas/imports para o kernel e XAM são resolvidos pelo runtime (HLE), não pelo XEX.
- Saltos indiretos usam uma **tabela de funções** gerada (`function table initialized for module`),
  o que evita a duplicação manual de tabelas que o projeto anterior teve de fazer.
- Shaders Xenos: o SDK traduz HLSL SM3 do guest para **SPIR-V em tempo de build**
  (`rexglue shaders` + cache em `<build>/generated/shaders/`), emitindo o sidecar **`shaders.xex`**
  (`rexglue_app.cpp:315-333`); o runtime só consome SPIR-V — DXC/glslang são ferramentas de build,
  presentes apenas na máquina do dev (v0.10.0; `commands/shader_transcode_command.cpp`) →
  **não** deve ser necessário correr o XenosRecomp offline (cobertura dos shaders do título: confirmar
  no primeiro frame — T7; ver `evidence/T2_AUDIT.md` §2).

## 4. Estratégia de build (reprodutível — comandos verificados no código v0.10.0)

> Verificado no clone local `third_party/rexglue-sdk` (commit `c94f5eb` "Release v0.10.0") a **2026-10-19**.
> O CLI real usa grafia com **hífen** (`--project-name`); as mensagens de erro do próprio código citam
> underscores (`--project_name`, `init_command.cpp:92-95`) — o registo CLI é o das linhas 566-581.
> A partir da v0.10 os comandos estão em `src/rexglue/commands/` (não `src/rexglue/`).

### 4.0 Pré-requisito: extrair o contentor STFS (tarefa T3b)

**Facto (T1): o `rexglue` v0.10.0 não expõe extração STFS e `--xex-path` não aceita o contentor:**
- Subcomandos registados: apenas `init`, `codegen`, `recompile-tests` (`src/rexglue/main.cpp:82-84`);
- `vfs_dump_main` existe mas está **morto** (`src/filesystem/vfs_dump.cpp:118` — `XE_DEFINE_CONSOLE_APP` comentado);
- `init` exige `--xex-path` como ficheiro regular (`commands/init_command.cpp:112-115`) e **dentro**
  de `--game-root` (`:133-138`);
- codegen e app carregam via VFS `game:\…` (`src/codegen/project_recompiler.cpp:251-254`;
  `rex_app.cpp:262` tem `game:\default.xex` hardcoded) e o loader só aceita magics XEX1/XEX2/ELF
  (`user_module.cpp:134-145`; `xex_module.cpp:870-876`) — o magic `LIVE` daria `Unknown module magic`;
- `game:` é montado como **diretório host** (`HostPathDevice`, `runtime.cpp:300-318`), sem auto-detecção STFS.

Caminho preferido (playbook 360tools §1, MIT, só stdlib — `hashlib/os/struct/sys/time`,
`extract_stfs.py:9-13`; **não** precisa de pycryptodome; precisa de **Python 3** → T3):

```powershell
python third_party\360tools\tools\extract_stfs.py `
  "C:\S4EP1Recomp\game\58410A07\000D0000\11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58" `
  C:\S4EP1Recomp\game\extracted
```

**Verificação (critério T3b):** `game\extracted\default.xex` existe, primeiros 4 bytes = `XEX2`,
contagem/nomes contra `evidence/stfs_filetable.txt`. Fallback se a extração falhar (LZX no contentor):
ferramenta mínima pós-T4 ligando `StfsContainerDevice` do próprio SDK (respeita D5 — não escrever
parser STFS próprio).

### 4.1 SDK (uma vez) — T4

```powershell
# T4 CORRIGIDO (2026-10-19): libmspack NAO tem symlinks — e um submodule git.
# Inicializar os submodules exigidos pela v0.10.0 (Windows/D3D12; Vulkan OFF, Tracy ON):
#   git submodule update --init --depth 1 thirdparty/{cli11,libmspack,FFmpeg,tomlplusplus,simde,xxHash,spdlog,fmt,utfcpp,imgui,sdl3,tracy}
# (a lista e verificada pelo proprio CMake: thirdparty/CMakeLists.txt:33-74). O configure NAO vai a rede por omissao.
cmake --preset win-amd64
cmake --build out/build/win-amd64 --config Release
cmake --install out/build/win-amd64 --config Release   # regista find_package(rexglue)
```

### 4.2 Projeto-piloto: init + codegen (T5)

```powershell
rexglue init --project-name sonic4 `
  --xex-path     C:\S4EP1Recomp\game\extracted\default.xex `
  --game-root    C:\S4EP1Recomp\game\extracted `
  --project-root C:\S4EP1Recomp\pilot

cd C:\S4EP1Recomp\pilot
rexglue codegen        # manifesto *_manifest.toml descoberto no cwd (codegen_command.cpp:141-182)
```

- Flags registadas em `commands/init_command.cpp:566-581` (também `--scan-dll`, `--template-dir`;
  subcomandos `init module` `:586-594` e `init achievements` `:624-627`);
- `--project-root` por omissão = cwd (`:103-104`); `sonic4` é nome válido (`template_utils.h:30-47`);
- gerados em `:215-223`: `CMakeLists.txt`, `CMakePresets.json`, `sonic4_manifest.toml`,
  `src\main.cpp`, `src\sonic4_app.h`, `generated\rexglue.cmake`; para reger os `RequiresForce`:
  flag global `-f/--force` (`main.cpp:76`; política `:225-238`);
- `codegen` aceita `[config]` posicional, `--target NAME` (repetível), `--ignore-stamp`
  (`codegen_command.cpp:222-231`);
- `UnresolvedCall` → acrescentar ao manifesto (o `[entrypoint]` é lido em `manifest.cpp:110-115`,
  a tabela `[functions]` em `config.cpp:177-180`):
  ```toml
  [entrypoint.functions]
  0x8207D730 = {}
  ```

### 4.3 Build do piloto (T6)

```powershell
cmake --preset win-amd64-release "-DCMAKE_PREFIX_PATH=C:\S4EP1Recomp\third_party\rexglue-sdk\out\install\win-amd64"
cmake --build --preset win-amd64-release
```

- Presets do projeto: `win-amd64{debug,release,relwithdebinfo}` com `CMAKE_CXX_COMPILER=clang++`
  (`resources/templates/init/cmake_presets.inja:11-12,86-90,190`);
- `generated/rexglue.cmake` chama `find_package(rexglue)` (`rexglue_cmake.inja:15-25`) →
  `CMAKE_PREFIX_PATH` ao install do SDK é obrigatório;
- Alvo: executável `sonic4` (`cmakelists.inja:19,24`; no Windows é WIN32/GUI).

### 4.4 Execução com log (T6)

```powershell
.\out\build\win-amd64-release\sonic4.exe `
  --game_data_root=C:\S4EP1Recomp\game\extracted --log_file=run1.log
```

- `--game_data_root` é obrigatório (erros em `rex_app.cpp:205,211`) e `game:\default.xex` é
  procurado dentro dele (`rex_app.cpp:262,277-286`);
- a app aceita `--<cvar>=valor` para qualquer cvar registado (`cvar.cpp:582-636`; cvars
  `game_data_root` em `runtime.cpp:31`, `log_file`/`log_level` em `logging.cpp:36-40`;
  sem `--log_file`, o log tem nome sequencial automático);
- o alvo é WIN32 (GUI) → **não confiar no stdout**; `--log_file` + `logs/` do runtime são a
  fonte de verdade do `run1.log` (playbook 360tools §4).

## 5. Separação genérico / específico do Sonic 4

| Genérico (reutilizável) | Específico do Sonic 4 |
|---|---|
| ReXGlue SDK (codegen, runtime, GPU, áudio, input, VFS) | hints de funções no manifesto (`[entrypoint.functions]`) |
| `tools/scan_stfs_xex.js`, `probe_xex_meta.js`, `scan_stfs_filetable.js` | overrides de paths/dados (`ReXApp` hooks) |
| `.gitignore`, convenções de evidência/registos | stubs de APIs Xbox efetivamente usadas pelo jogo |
| Playbook 360tools (harness de triagem, stubs por defeito) | shaders/assets `.AMB` próprios do título |

## 6. Riscos arquiteturais

1. **Dependência de um projeto em desenvolvimento ativo** (o SDK avisa "expect things not to work").
   Mitigação: pin da versão (`v0.10.0`) e manter `third_party/rexglue-sdk` como fonte de verdade local.
2. **Clang 18+ e Python 3 ausentes** → bloqueios de instalação (ver `TOOLS.md` §2 e `NEXT_STEPS.md` T3/T3b).
3. **Submodules do SDK não inicializados** — o clone tem 22 submodules git (v0.10.0), todos vazios; o
   CMake falha com FATAL_ERROR se faltar algum dos exigidos (`thirdparty/CMakeLists.txt:33-74`).
   Correção: `git submodule update --init --depth 1` (12 no Windows/D3D12; ver `evidence/T2_AUDIT.md` §1,
   que **corrige** a nota anterior sobre "symlinks" — eram um gotcha da v0.8.0).
4. **Rede necessária para os submodules** (T4), não para o configure (FidelityFX default OFF; DXC só headers).
5. **Driver Vulkan 1.0 no host** — requisito do runtime (o SDK não o instala; `doctor` procura
   `VULKAN_SDK`, mas o caminho real é o loader + ICD da GPU). Confirmar empiricamente na T6.
6. **Áudio CRIWARE** (`CPK/CSB` em vez de XMA puro) — pode exigir trabalho específico.
7. **Código gerado é grande** → tempos de build longos; usar o build tiered do harness e nunca
   "corrigir instruções uma a uma" antes de a cadeia completa arrancar.
8. **Title update/DeltaPatch** — risco **fechado** pela T2: é opcional e nenhum update existe/é
   exigido para este título (RTM; ver `evidence/T2_AUDIT.md` §3).
