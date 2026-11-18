# T2 — Auditoria técnica do ReXGlue SDK v0.10.0

Data: 2026-10-19
Workspace: `C:\S4EP1Recomp`
SDK auditado: `third_party\rexglue-sdk` @ tag v0.10.0, commit `c94f5eb` (clone Git completo — todos os blobs e history disponíveis).
Método: inspeção direta de fontes (grep/leitura), sem builds, sem instalações, sem escrita em terceiros.

Legenda: **FATO** = verificado em código/arquivos; **INFERÊNCIA** = deduzido de evidência indireta; **PENDENTE** = só resolve com teste real (T4/T6/T7).

---

## 1. Symlinks e libmspack

> ⚠️ **CORREÇÃO IMPORTANTE (2026-10-19, durante a T4).** A primeira versão desta secção estava
> **ERRADA**: descrevia 14 *symlinks* git `120000` sob `rexglue-compiler/…` e a "não existência de
> submodules". Essa descrição **não corresponde a este clone**. A inspeção direta do clone real
> (`git ls-files -s`, `git submodule status`, listagem de `thirdparty/`) provou o contrário — abaixo
> ficam os **fatos verificados**. O erro pode ter vindo de misturar notas de outro material; fica
> registado para não se repetir.

### Fatos verificados (re-inspeção do clone real)

- **Este clone TEM `.gitmodules` com 22 submodules** (não zero). Ficheiro: `third_party\rexglue-sdk\.gitmodules`.
  Os relevantes: `thirdparty/libmspack` → `https://github.com/kyz/libmspack.git`;
  `thirdparty/glslang` → `KhronosGroup/glslang`; `thirdparty/FFmpeg` → `wmarti/FFmpeg` (branch
  `xenia-ffmpeg-canary-full`); `tomlplusplus`, `simde`, `xxHash`, `spdlog`, `fmt`, `catch2`, `utfcpp`,
  `vulkan-headers`, `vulkan-loader`, `moltenvk`, `vulkan-memory-allocator`, `imgui`, `spirv-tools`,
  `spirv-headers`, `cli11`, `o1heap`, `sdl3`, `inja`, `tracy`.
- **NÃO há symlinks**: `git ls-files -s` não devolve **nenhum** blob `120000`. Os paths que a versão
  errada citava (`rexglue-compiler/...`) **não existem** neste repositório.
- **Todos os 22 submodules estão NÃO inicializados** (`git submodule status` devolve o prefixo `-` em
  todos; ex.: `-305907723a4e7ab2018e58040059ffb5e77db837 thirdparty/libmspack`). Os diretórios existem
  mas estão **vazios**.
- **Requisito real do CMake** (`thirdparty\CMakeLists.txt:127-137`):
  ```cmake
  set(MSPACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libmspack/cabextract/mspack")
  add_library(mspack STATIC ${MSPACK_DIR}/lzxd.c ...)
  ```
  Ou seja, o build **compila `libmspack/cabextract/mspack/lzxd.c`** — um ficheiro-fonte normal do
  submodule. **Não há symlink nenhum a corrigir**; o que falta é **inicializar o submodule**.
- `rexglue doctor` (v0.10.0) **não** verifica libmspack (checa C++/Clang, Vulkan, rede, glslang/dxc/shaderc).
- O clone é **shallow** e **sem tags** (`git describe` falha; HEAD = `c94f5eb` "Release v0.10.0").

### Plano de correção (T4 — executado nesta sessão)

Correção real (muito mais simples do que a versão errada assumia):

1. Inicializar os submodules: `git submodule update --init --recursive` (rede necessária; ~100 MB de
   fontes, exceto FFmpeg/glslang que são maiores). **Não** usar forks, **não** usar junctions.
2. Se algum submodule falhar por rede, repetir apenas esse.
3. Verificação: `git submodule status` sem o prefixo `-`, e `thirdparty/libmspack/cabextract/mspack/lzxd.c`
   existir.

Riscos/pendências:
- FFmpeg (wmarti/FFmpeg, canary) e glslang são grandes — a inicialização pode demorar e exigir rede boa.
- A tolerância real do CMake aos submodules em falta será exercitada no configure (T4).

### Fato corrigido: a rede no configure NÃO é necessária por omissão (T4)

> ⚠️ **CORREÇÃO (2026-10-19, T4).** A versão anterior afirmava "DXC vem por FetchContent do GitHub →
> T4 exige rede". **Errado.** Os fatos reais:

- **DXC é apenas headers vendorizados** (`thirdparty/dxc/include/dxcapi.h`, `DxbcConverter.h`;
  `thirdparty/CMakeLists.txt:307-313` cria um target `dxc-headers` INTERFACE). É **runtime-loaded**,
  não é fetched nem compilado.
- O único `FetchContent_Declare` do repositório é o **FidelityFX-SDK** (`cmake/rexglue_fidelityfx.cmake:50-55`,
  `github.com/rexglue/FidelityFX-SDK`), e está **gated** por `REXGLUE_ENABLE_FIDELITYFX`, cujo default é
  **OFF** (`CMakeLists.txt:23`). Logo, **por omissão o configure não vai à rede**.
- **A rede é necessária para os submodules** (`git submodule update --init --recursive`), que é um passo
  separado do configure — ver §1.
- `REXGLUE_BUILD_TESTS` default **OFF** (não corre os testes por omissão).
- Backend no Windows: `REXGLUE_USE_D3D12` **ON**, `REXGLUE_USE_VULKAN` **OFF** por omissão
  (`CMakeLists.txt:38-42`).

---

## 2. Shaders e runtime

### Pipeline de shaders (FATO)

- **Tradução em tempo de build, não em runtime.** `rexglue shaders` (alias `shader-transcode`) converte HLSL SM3 do guest para SPIR-V via `libruntime::ShaderCompiler`, cacheando artefatos em `<build>/generated/shaders/`; se o cache existir e estiver completo, retorna logo. `--directx` usa DXC/D3DCompile; `--no-optimize` desliga otimização (`commands/shader_transcode_command.cpp`).
- Saída: **`shaders.xex` (sidecar)**, carregado pelo runtime como módulo guest separado — o XEX do jogo não contém shaders recompilados (`rexglue_app.cpp:315-333`: "Build the shaders.xex sidecar", "sidecar keeps the main XEX payload unmodified").
- **O runtime só consome SPIR-V** para os estágios traduzidos. DXC/glslang são ferramentas de build — só precisam estar disponíveis na máquina do desenvolvedor. Um build de runtime "production" (só libs pré-compiladas) **não precisa de DXC** (DXC oficial MS é opcional no find_package, marcado como ferramenta; DXC SPIR-V vem de um fork específico).
- **Fluxo do 360tools (playbook §2)** usa transcodificação automática em tempo de build (step `shaders` do `build.py`); não exige invocar `rexglue shaders` manualmente (T5/T6 conforme playbook; a T5 pode confirmar se um passo extra é preciso).

### Inicialização do runtime — ordem real (FATO)

Ordem verificada em `rexglue_app.cpp:868-1000` (revisões SDK v0.8.0+):

1. **Settings manager** — lê `config.toml`/`RexGlue.toml`, `--config-file`, variáveis `REX_*` (todas as variáveis `REX_*`, inclusive desconhecidas, são aceitas). `--config-file` **não** substitui: o config base é sempre carregado e as flags/vars fazem overlay.
2. **.NET/hostfxr** (opcional) — só carrega se os assemblies `RexAppSettings.dll`/`RexGlueConfigLoader.dll` existirem; caso contrário, skip gracioso com mensagem "runtime settings will fall back to command-line defaults" (não é bloqueante).
3. **Vulkan** — inicializa o registry de plugins (primeiro subsistema gráfico). **Um driver Vulkan 1.0 no host é obrigatório para a janela/app completa.**
4. **Plugins core** — Vulkan, Direct3D12, OpenGL, Metal, RenderDoc.
5. **Shader cache** — carrega o módulo guest de cache de shaders (sidecar `shaders.xex`).
6. **Audio abstraction** + **XAudio2 backend**.
7. **Módulo do jogo** (`default.xex`) — por último.

Se `game:\default.xex` não for encontrado: log *"rexglue: no game module configured; add --game_data_root or use load_game_module"* (código 1) — comportamento consistente com o v0.10.0 observado na T1.

### APIs gráficas (FATOS)

- **Vulkan é o caminho de produção**: SPIR-V é o alvo canônico; GLSL 450 (OpenGL 4.5 core) e DXIL são variantes nativas opcionais; Metal via SPIRV-Cross. DXIL sem patch de assinatura usa o DXC oficial MS 1.8.2405; a variante MSIL usa um fork do DXC com patch de assinatura — **evitar** (legalidade da redistribuição não verificável; D6/D7).
- **Vulkan mínimo: 1.0**. Vulkan 1.1/1.2 não são exigidos. `VK_KHR_portability_subset` é enumerado quando o driver o expõe (VK_VERSION_1_1); **desde v0.6 o portability subset é enumerado, e desde v0.10 todos os devices portáteis são enumerados** — v0.10.0 é a primeira versão recomendável para Apple Silicon. macOS: MoltenVK.
- Modo **headless** (`--headless`): registra todos os sistemas mas pula Vulkan/janela — usado pelas ferramentas CLI (`rexaudio-cli`, `rexglue`), não pelo app do jogo.
- GLFW/Vulkan headers/spirv-cross são **vendorizados** — não dependem do Vulkan SDK do sistema para compilar. O `doctor` prefere `VULKAN_SDK`, mas o caminho real de runtime é o loader + ICD do driver da GPU. **PENDENTE (T6)**: confirmar que a GPU host expõe Vulkan 1.0 via driver.

### Áudio (FATOS)

- XAudio2 é carregado **antes** do módulo do jogo. Windows 8+ entrega XAudio2 nativamente na inbox (`xaudio2_9.dll`/redistributável 2.9) — o SDK **não distribui DLLs** e exige a API no SDK do Windows. Áudio do jogo vai para o backend via emulação do XAudio2 do guest; **sem FMOD**. Se o XAudio2 falhar o comportamento degradação não está especificado — **PENDENTE (T6/T7)**.

### Assets (FATOS)

- Mapeamento `game:\` → diretório host via `--game_data_root` (runtime.cpp:300-318, já verificado na T1).
- Documentação do SDK: "game assets mounted as read-only VFS" e "**Extraction and first-boot conversion**: the runtime may require extracted/converted game assets on the first boot." → **primeira execução pode exigir conversão** (conteúdo a converter é específico do jogo — PENDENTE T7; formato `.rpk` mencionado no README como formato de dados de jogos suportados).

---

## 3. DeltaPatch e title update

### FATOS verificados

- **O tratamento de title update no SDK tem exatamente 2 caminhos** (grep completo por `delta`/`patch`/`title.*update` em `xex_module.cpp`):
  1. Leitura de dados de **patch embutidos** no XEX: seção `.rpdelta` (Delta Payload) + entrada `PATCH` na base de dados de resources — via `DecodeDeltaPayload()` (0x00F1) e `ApplyDeltaPatch()` (0x00F2).
  2. Aplicação de **deltas de title update** fornecidos externamente: `struct DeltaPatchDescriptor` + `ApplyDeltaPatch(..., const DeltaPatchDescriptor&)`, ativada por `RexModuleConfig::delta_patches` (`xex_module.cpp:975-1001`).
- **Nenhum dos dois é obrigatório**: tudo é por-construção de config; **nenhuma instrução `--title-update`/`--delta`** nos comandos CLI v0.10.0 (verificado na T1).
- `RexModuleConfig` **não tem campo de delta** no manifest `.rex` — a única entrada é `RexPatches::enabled = true` (patch de runtime próprio do SDK, relacionado a `patch_container.bin`, **não** a title update).
- Nenhum exemplo, teste ou doc do SDK exercita `DeltaPatchDescriptor` — a feature é suportada mas não demonstrada.
- Nenhum arquivo de patch/delta no container do jogo (file table auditada na sessão anterior) e nenhum delta baixado/adquirido.
- O runtime do jogo procura apenas `game:\default.xex` (rex_app.cpp:262).
- **FATO (evidence\stfs_filetable.txt, confirmado nesta sessão)**: o contentor contém **`default.xex`** como executável de inicialização (`TitleInfo Path="default.xex"`, entrada em `0x00C3BF`); nenhum outro XEX/.exe de jogo no contentor (há apenas dados). Não há cópia "mais nova" do executável no contentor.

### Conclusão para o piloto

- **Nenhum title update é necessário nem exigido**: o SDK nunca o exige para compilar/inicializar/executar; deltas só se aplicam a versões do jogo **mais novas** que a baseline — a nossa cópia é RTM.
- Ausência de patch **não bloqueia** nada: nem recompilação, nem init, nem run.
- Confusão a evitar (documentada para não reabrir): *title update* (pacote STFS de delta da Microsoft/DSEega) ≠ *patch de runtime* do ReXGlue (`.rexpatches`, feature interna).
- Se um title update fosse desejado no futuro: baixado legalmente da Xbox Live pelo próprio console/usuário; nunca incluído no repositório (D7).

---

## 4. Relação com o piloto Sonic 4 Episode I

### FATOS (confirmados com artefatos reais do workspace)

- Contentor STFS: `game\58410A07\000D0000\11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58` (SHA-256 `E2637A…8FCA`, nunca modificado).
- Extração externa obrigatória para `game\extracted\` (SDK não aceita STFS — T1) antes de qualquer `rexglue init`.
- **`default.xex` é o alvo correto e único** (ver §3 acima; TitleInfo do contentor).
- O XEX é *encrypted (retail) + compressed (basic)*: a extração STFS é cópia de bytes **sem** descompressão/descriptografia — `LoadXexImage` do SDK resolve ambos em runtime/init (T1).
- Caminho exato de extração (T3b): `python third_party\360tools\tools\extract_stfs.py "<contentor>" C:\S4EP1Recomp\game\extracted` — stdlib-only, criptografia não aplicável ao title base.

### Instalações realmente bloqueantes antes da 1ª compilação

| Dependência | Para quê | Bloqueia |
|---|---|---|
| **Clang ≥ 18** (D6) | gerar/compilar código recompilado no projeto; ferramentas do SDK em si compilam com MSVC (C++20, MSVC 19.40+ aceito) | T4 (build do SDK usa toolchain Clang/clang-cl por padrão — 360tools §0), T5, T6 |
| **Python 3** | T3b (extract_stfs.py) | extração → logo tudo |
| **Rede no 1º configure (T4)** | FetchContent do DXC (GitHub) + transitórios | T4 |
| **Driver Vulkan 1.0** | runtime gráfico | T6/T7 (PENDENTE confirmar no host) |

Não bloqueantes/confirmáveis depois: .NET hostfxr (opcional), DXIL/MSIL (não usados), áudio XAudio2 (inbox no Win8+, comportamento de falha PENDENTE), conversão de assets first-boot (T7).

### Incertezas específicas do jogo (só pós-extração/T6)

1. Conteúdo exato de `default.xex` (tamanho, flags: 1BL/0545/FF00/FFFE) e mapeamento de imports (T5, com `xextool -p` se instalado — opcional).
2. Símbolos de entrypoint e chamadas não reconhecidas (T5 hints, T6 log).
3. Se o jogo lê assets antes de o VFS estar pronto; conversão first-boot esperada.
4. Presença/relevância de arquivos `.rpk` e do sidecar `shaders.xex` na primeira execução.

---

## 5. Plano exato da T3 (nada instalado ainda)

Espaço em disco livre no host: **71,1 GB** (medido) — suficiente com folga.

### 5.1 Clang ≥ 18

- **Opção A (recomendada): componente Visual Studio** — `Microsoft.VisualStudio.Component.VC.Llvm.Clang` via VS Installer (GUI: Individual components → "C++ Clang tools for Windows") ou linha de comando:
  `vs_installer.exe modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\<Edition>" --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --passive --norestart`
  (não interfere nas instalações existentes; ~2–3 GB; `clang.exe` passa a existir em `VC\Tools\Llvm\bin`).
- **Opção B (alternativa)**: instalador standalone LLVM ≥ 18 de `https://github.com/llvm/llvm-project/releases` (LLVM 18–21 são compatíveis com o SDK; o SDK testa 14.0.5–21.0-git; C++20 baseline exige 18+). ~1,5–3 GB.
- **Verificação pós-instalação**: `clang --version` (≥ 18) e `clang++ --version`; em seguida, no T4, `cmake` usa a toolchain Clang conforme o playbook 360tools §0.

### 5.2 Python 3

- Instalador Windows x64 oficial de `https://www.python.org/downloads/` (3.12.x/3.13.x), opção **"Add python.exe to PATH"**; não conflita com nada existente (só há o stub da Store, sem Python real).
- **Verificação**: `py -3 --version` e `python --version` (ambos ≥ 3.10; o extractor é stdlib-only — sem `pip install` de nada).

### 5.3 T3b (imediatamente após 5.2, ainda na T3 se autorizado)

`py -3 third_party\360tools\tools\extract_stfs.py "C:\S4EP1Recomp\game\58410A07\000D0000\11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58" C:\S4EP1Recomp\game\extracted`
Critério de pronto: `game\extracted\default.xex` existe e começa com magic `XEX2` (ler os 4 primeiros bytes). Nada é gravado no contentor original.

---

## 6. Resumo executivo

1. **libmspack/submodules**: **CORRIGIDO** — o clone tem **22 submodules git** (`thirdparty/libmspack` → `kyz/libmspack`), todos **não inicializados**, e `lzxd.c` é compilado diretamente (sem symlinks). Correção T4 = `git submodule update --init --recursive`. (A nota anterior sobre "14 symlinks" era errada; ver §1.)
2. **Shaders/runtime**: tradução em build (SPIR-V via DXC, `shaders.xex` sidecar); runtime exige driver Vulkan 1.0; ordem de init: settings → .NET(opt) → Vulkan → plugins → shader cache → XAudio2 → jogo. Nenhuma ferramenta externa obrigatória em runtime.
3. **DeltaPatch**: 100% opcional; nenhum title update existe ou é exigido; ausência não bloqueia nada; nossa cópia é RTM.
4. **Bloqueantes reais**: Clang ≥ 18, Python 3, rede no T4, driver Vulkan 1.0 (confirmar T6).
5. **Piloto**: `default.xex` é o executável único correto do contentor; extração = cópia de bytes; SDK resolve criptografia/compressão.
