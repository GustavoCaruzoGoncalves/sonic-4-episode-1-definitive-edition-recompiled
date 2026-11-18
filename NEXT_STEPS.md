# NEXT_STEPS.md — próximas tarefas concretas (ordenadas)

Regra: cada tarefa é pequena, verificável e termina com uma prova. Nada de compilações longas antes de
as dependências e a arquitetura estarem validadas. Nada de instalar sem autorização (T3 é o único pedido).

---

## T1 — Confirmar a interface do `rexglue` v0.10.0 (0 instalações, 10 min) — ✅ CONCLUÍDA (2026-10-19)
Ler no clone local `third_party/rexglue-sdk/src/rexglue/commands/init_command.cpp` e `codegen_command.cpp`:
- nomes reais das flags (`--app_name/--app_root` vs `--project-name/--xex-path/--game-root/--project-root`);
- se `--xex-path` aceita **o contentor STFS diretamente** ou um `default.xex` extraído;
- comandos/opções de extracção (usar `vfs_dump` se necessário).

**Resultado:** comandos exatos com citações em `ARCHITECTURE.md` §4; dúvidas 1-2 de `DECISIONS.md`
fechadas. Resumo: (a) flags com **hífen** (`init_command.cpp:566-581`); (b) o contentor STFS **não**
é aceito — loader só aceita XEX1/XEX2/ELF (`user_module.cpp:134-145`); (c) **não existe extração
STFS no CLI** v0.10.0 (só `init`/`codegen`/`recompile-tests`, `main.cpp:82-84`; `vfs_dump` morto em
`vfs_dump.cpp:118`) → criada a tarefa T3b. Python confirmado ausente (só stub da Store) — passa a
ser requisito da extração (corrigido em `TOOLS.md` §2 e `DECISIONS.md` D3).

## T2 — Fechar as dúvidas 3 e 5–7 de `DECISIONS.md` por leitura (0 instalações) — ✅ CONCLUÍDA (2026-10-19)
- (Q3) `thirdparty/libmspack` na v0.10.0: ainda são symlinks `120000`?
- (Q5) confirmar suporte a tradução de shaders em runtime (procurar o compilador de shaders herdado do Xenia).
- (Q7) ler `DeltaPatchDescriptor`/`ApplyDeltaPatch` no `xex_module.cpp` e decidir se o title update é
  necessário para boot — ou reetiquetar explicitamente como experimento no primeiro arranque.
- (Q6) `.AMB/.YSD` via VFS: por leitura não há como confirmar o que o código do jogo faz com eles;
  reetiquetar como experimento do primeiro arranque (critério: primeiro log/frame sem erro de ficheiro).
- ~~"confirmar que o XEX é carregado a partir do contentor"~~ → **respondido na T1 (NÃO)**; ver `DECISIONS.md` Q2.

**Resultado (evidência completa em `evidence/T2_AUDIT.md`):**
- **Q3 ✅/corrigido** O clone tem **22 submodules git** (incl. `thirdparty/libmspack` → `kyz/libmspack`),
  todos **não inicializados**; **não há symlinks** na v0.10.0 (o gotcha dos `120000` era da v0.8.0).
  Correção T4 = `git submodule update --init --depth 1` dos 12 exigidos no Windows/D3D12.
- **Q5 ⚠️ corrigida** a premissa: a tradução de shaders é **em build** (→ SPIR-V, sidecar `shaders.xex`),
  **não** em runtime; DXC/glslang são ferramentas de build. Cobertura do título só se confirma na T7.
- **Q6 ✅ reetiquetada** como experimento de primeiro arranque (T7); `game:\` é VFS read-only genérica.
- **Q7 ✅** nenhum title update é necessário/exigido: `DeltaPatchDescriptor` é 100% opcional (sem campo
  no manifest `.rex`, sem exemplos); a cópia é RTM (`default.xex` é o único XEX no contentor).
- **Extras achados**: driver **Vulkan 1.0** é requisito de runtime *se* Vulkan estiver ligado; no Windows
  o default é D3D12=ON/Vulkan=OFF. **Correção (T4):** o configure **não** vai à rede (FidelityFX default
  OFF; DXC = só headers); a rede é precisa para os **submodules**.

## T3 — AUTORIZAÇÃO: instalar Clang 18+ e Python 3 (bloqueantes) — *pedido ao utilizador*
**Justificação Clang:** o SDK exige C++20/23 com toolchain **Clang** para o código gerado (D6; MSVC apenas
para o próprio SDK); não há `clang.exe`/`clang++` na máquina (só `clang-format`/`clang-tidy`).
**Justificação Python (correção T1):** o CLI rexglue v0.10.0 **não extrai STFS**; a extração (T3b) usa
`360tools/tools/extract_stfs.py` (só stdlib — **não** exige pycryptodome). Sem Python não há `default.xex`
e o `init`/`codegen`/execução ficam bloqueados.
**Pedido:**
  - (preferida) componente VS 2022 `Microsoft.VisualStudio.Component.VC.Llvm.Clang`;
    alternativa: LLVM standalone ≥ 18 (referência: 21.x usado pelo playbook 360tools);
  - Python 3 (python.org, opção "Add to PATH").
  **Espaço em disco:** 71,1 GB livres (medido, folgado).
**Opcional, não bloqueante:** `pycryptodome` (apenas scripts de triagem do 360tools).
Ver `evidence/T2_AUDIT.md` §5 para origens, versões e comandos de verificação exatos.

**Concluída quando:** existe `clang++ --version` ≥ 18 e `clang++` está no PATH num shell de dev do VS,
e `py -3 --version` ≥ 3.10 responde com um interpretador real (não o stub da Store).

## T3b — Extrair o contentor STFS (nova — surgiu da T1; pós-T3, ~5 min)
```powershell
python third_party\360tools\tools\extract_stfs.py `
  "C:\S4EP1Recomp\game\58410A07\000D0000\11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58" `
  C:\S4EP1Recomp\game\extracted
```
**Concluída quando:** `game\extracted\default.xex` existe, primeiros 4 bytes = `XEX2`, e a
lista de ficheiros extraídos bate certo (nomes/contagem) com `evidence/stfs_filetable.txt`;
registrar o log e o tamanho do `default.xex`.
**Fallback** (se a extração falhar por compressão LZX no contentor): adiar para pós-T4 e escrever
uma ferramenta mínima que ligue `StfsContainerDevice` do SDK (respeita D5 — não escrever parser STFS).

## T4 — Construir e instalar o ReXGlue SDK (uma vez) — ✅ INFRAESTRUTURA VALIDADA (2026-10-19)
Seguir `third_party/360tools/docs/rexglue-workflow.md` §0 (adaptado à v0.10.0):
1. **Submodules inicializados** (14 exigidos): `git submodule update --init --depth 1` para
   `cli11 libmspack FFmpeg tomlplusplus simde xxHash spdlog fmt utfcpp imgui sdl3 tracy o1heap inja`
   (lista imposta por `thirdparty/CMakeLists.txt:33-74`). Na v0.10.0 o `libmspack` tem **symlink stubs**
   internos do tipo v0.8.0 que **precisaram de ser materializados** (15 symlinks copiados). **Sem forks/junctions.**
2. `cmake --preset win-amd64` (Clang 21.1.4 LLVM, Ninja) → `cmake --build out/build/win-amd64 --config Release`
   → `cmake --install out/build/win-amd64 --config Release`. Configure sem rede; submodules com rede.
**Evidência:** `evidence/t4_sdk_build.txt`, `evidence/t4_cmake_configure.txt`, `evidence/t4_build2.txt`, `evidence/t4_install.txt`.
**Verificado:** `out/install/win-amd64/bin/rexglue.exe` existe; `rexglueConfig.cmake` presente; `rexglue --help` funciona.

## T5 — Gerar o projeto-piloto e correr o codegen
`rexglue init …` (flags de T1) → `rexglue codegen`. Registar o log completo.
Para cada `UnresolvedCall: 0xTARGET from 0xSITE`: adicionar `[entrypoint.functions]` (fixture em
`third_party/360tools/templates/overlay/manifest-overrides.example.toml`) e repetir. Contar iterações.

**Concluída quando:** codegen termina (sem `--force` ou com lista explícita de unresolved justificados).

## T6 — Build do projeto e primeira execução — ✅ PARCIAL/CONCLUÍDO (2026-10-19)
```
cmake --preset win-amd64-release "-DCMAKE_PREFIX_PATH=…\rexglue-sdk\out\install\win-amd64"
cmake --build out/build/win-amd64-release
.\out\build\win-amd64-release\sonic4.exe --game_data_root=<…>\game\extracted --log_file=run1.log --gpu_plugin=xenos
```
- GPU plugin `rexgpu-xenos.dll` copiado ao lado do executável (necessário). 
- Adicionados boundary hints iterativamente no manifest (`[entrypoint.functions]`) para endereços chamados indiretamente até execução estabilizar.
- Estado atual: runtime inicializa, shaders traduzidos, pipelines criados, áudio inicializado, input detectado. Jogo em execução (com observações: áudio pode estar sem som, velocidade acelerada, versão demo).

**Evidências:** `evidence\T6_build_execution.md`, `evidence\t6_run*.log`. 
## T7 — Iterar sobre o primeiro obstáculo real
Repetir: medir (log/exit code) → diagnosticar → corrigir (config/stub/override) → repetir. Nunca
"corrigir instruções uma a uma" nem declarar sucesso sem execução observada.

**T7 — Estado:** Tentativa T7_SPEED_01 realizada (alteração de frequência de clock). Build gerado com sucesso em `pilot/out/build/win-amd64-release/sonic4.exe`. Aguardando feedback do usuário sobre resultado da velocidade.

Comandos para teste (T7_SPEED_01):
```powershell
cd "C:\S4EP1Recomp\pilot\out\build\win-amd64-release"
.\sonic4.exe --game_data_root="C:\S4EP1Recomp\game\extracted" --gpu_plugin=xenos --log_file="C:\S4EP1Recomp\evidence\T7_SPEED_01\t7_speed_01.log"
```

**Concluída quando:** o marco 4 (runtime inicializa) ou superior de `PILOT_SONIC4.md` §6 é atingido com prova.

---

## Backlog (depois do MVP a arrancar)
- Áudio CRIWARE (`CPK/CSB`) — descoberta de formato e integração.
- Verificação de assets `.AMB/.YSD` no VFS.
- Android ARM64: **só** depois de haver execução funcional estável no Windows x64.
- Empacotar o conhecimento reutilizável (harness de triagem, convenções) para o próximo título.
