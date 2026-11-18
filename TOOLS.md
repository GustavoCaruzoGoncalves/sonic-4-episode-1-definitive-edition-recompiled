# TOOLS.md — ferramentas, versões e dependências

Inventário levantado nesta máquina em **2026-10-18** (Windows, host `C:\`).
Evidência bruta reproduzível: `evidence/tools_inventory.txt` (comandos no fim deste ficheiro).

## 1. Instalado e verificado

| Ferramenta | Versão | Caminho / nota | Usada para |
|---|---|---|---|
| Git | 2.55.0.windows.5 | `C:\Program Files\Git\cmd\git.exe` (no PATH) | clonar SDK/referências |
| CMake | 4.5.0-rc1 | `C:\Program Files\CMake\bin\cmake.exe` (no PATH) | build do SDK e do projeto-piloto |
| Ninja | 1.12.1 | `…\VS2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe` | gerador do SDK (não está no PATH) |
| MSVC / VS 2022 | Community **17.14.37806.7** | `vswhere -property installationVersion` | Windows SDK, headers D3D12, ambiente dev |
| Node.js | 24.21.0 | `C:\Program Files\nodejs\node.exe` (no PATH) | scripts de triagem próprios (`tools/*.js`) |
| Java (JDK) | 21.0.12.1 LTS | Eclipse Adoptium | **reserva** (Ghidra, se for preciso análise extra) |
| Rust / Cargo | 1.99.1 / — | `C:\Users\Administrator\.cargo\bin` | não usado no plano atual |
| 7-Zip | 24.08 (x64) | `C:\Program Files\7-Zip\7z.exe` (**fora do PATH**) | inspeção de ZIP; não foi necessário (usei .NET) |
| GitHub CLI | 2.102.0 | `C:\Program Files\GitHub CLI\gh.exe` (**fora do PATH**) | consulta de metadados/licenças dos repos |
| curl | Windows | `C:\Windows\System32\curl.exe` | metadados GitHub |
| tar | Windows | `C:\Windows\System32\tar.exe` | utilitário |

Sem custo de instalação nesta fase: extração do ZIP foi feita com `System.IO.Compression`
(.NET, incluído no Windows) — **nenhuma ferramenta nova foi instalada**.

## 2. Ausente e relevante

| Em falta | Necessário para | Bloqueia? | Como obter (a decidir com o utilizador) |
|---|---|---|---|
| **Clang 18+ (LLVM)** — `clang.exe`/`clang++` | **ReXGlue SDK** (`CMakeLists.txt` exige `^(Apple)?Clang$` e `>= 18`; C++23) | **SIM** — **RESOLVIDO (T3)** | LLVM 21.1.4 instalado em `C:\Program Files\LLVM\bin` (`clang++ --version 21.1.4`). SDK v0.10.0 compilou com Clang 21.1.4 (preset usa clang/clang++). |
| Python 3.10+ | **extração STFS (T3b)** via `360tools/tools/extract_stfs.py` (só stdlib) | **SIM (T3b)** — **RESOLVIDO (T3)** | Python 3.13.15 instalado (`C:\Program Files\Python313`, real interpreter, não stub). `pycryptodome` continua opcional. |
| **Vulkan 1.0** (driver ICD do host) | **runtime gráfico do ReXGlue (T6/T7)** — Vulkan é inicializado antes do módulo do jogo; SPIR-V é o alvo canônico | Provavelmente **SIM para T6** — o SDK não o instala; a confirmar empiricamente (T2 não testou a GPU) | Já vem com driver GPU atual; confirmar com `vulkaninfo`/driver do fabricante. O `doctor` procura `VULKAN_SDK`, mas o caminho real é o loader + ICD |
| Rede (submodules) | `git submodule update --init --recursive` (22 submodules; T2-corrigido) | **SIM para T4** | Já disponível. O configure em si **não** vai à rede por omissão (FidelityFX OFF; DXC só headers) || .NET SDK | nenhum passo do plano (o runtime usa hostfxr **opcional**) | NÃO | — |
| `make`, `pkg-config`, `ripgrep` | nenhum passo do plano | NÃO | — |

> **Armadilha verificada:** o comando `python` resolve para o *stub* da Microsoft Store
> (`WindowsApps\python.exe` → "Python was not found"). Considerar Python **ausente** até haver
> um interpretador real confirmado com `python -V`.
>
> **Armadilha verificada:** o VS 2022 tem `VC\Tools\Llvm\{bin,x64\bin,ARM64\bin}` mas apenas
> `clang-format.exe` e `clang-tidy.exe` — **não há compilador Clang instalado**.

## 3. Verificações feitas para o ReXGlue (fonte primária: código clonado)

- `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.25)`, `set(CMAKE_CXX_STANDARD 23)`,
  e falha explícita se o compilador não for Clang ou for Clang < 18. Versão do SDK: **0.10.0**.
  (T2 refinou: o **próprio SDK** também compila com MSVC C++20 ≥ 19.40 e testa Clang 14–21; a exigência
  Clang ≥ 18 prende-se sobretudo ao **código gerado** — D6 — e ao preset/toolchain predefinidos.)
- `CMakePresets.json`: presets `win-amd64{,-debug,-release,-relwithdebinfo}` com
  `CMAKE_C_COMPILER=clang`, `CMAKE_CXX_COMPILER=clang++`.
- Extração nativa já presente no SDK: `include/rex/filesystem/devices/stfs_container_device.h`,
  `stfs_container_entry/file.h`, `stfs_xbox.h`, `include/rex/system/lzx.h`, `src/system/lzx.cpp`,
  `src/system/xex_module.cpp`, `src/kernel/xboxkrnl/xboxkrnl_crypt.cpp`, `src/filesystem/vfs_dump.cpp`.
  → **mas o CLI v0.10.0 não expõe essa extração** (`vfs_dump` é código morto) → continua a ser preciso
  o `extract_stfs.py` (T3b) para obter `default.xex`.
- `docs/` do SDK: `RELEASING.md`, `ppc/core_instructions.pdf`, `ppc/altivec_instructions.pdf`, `ppc/vmx128.txt`.
- `tools/binutils/` do SDK inclui **binutils powercell-none-elf** (nm/objdump) para inspeção PPC.
- Preparação de terceiros (T4): o clone tem **22 submodules git** (`.gitmodules`), todos trazidos vazios.
  O CMake verifica e falha se faltar algum exigido (`thirdparty/CMakeLists.txt:33-74`). No **Windows/D3D12**
  (Vulkan OFF, Tracy ON, tests OFF) bastam **12**: `cli11 libmspack FFmpeg tomlplusplus simde xxHash spdlog
  fmt utfcpp imgui sdl3 tracy` → `git submodule update --init --depth 1 thirdparty/<m>`.
  **Não há symlinks a corrigir na v0.10.0** (o gotcha dos symlinks `120000` era da **v0.8.0** — ver
  `evidence/T2_AUDIT.md` §1, que corrige a primeira leitura da T2). O `mspack` é construído a partir de
  `thirdparty/libmspack/cabextract/mspack/lzxd.c`.
- **Rede:** necessária para inicializar os submodules; o **configure em si não vai à rede** por omissão
  (FidelityFX default `OFF`; DXC são apenas headers vendorizados em `thirdparty/dxc/include`).

## 4. Ferramentas específicas do piloto Sonic 4

| Necessidade | Estado | Nota |
|---|---|---|
| Ler o contentor STFS | ✅ sem instalar (`tools/scan_stfs_xex.js` + `probe_xex_meta.js`) | já feito; XEX2 localizado e descrito |
| Extrair `default.xex` do contentor | ❌ ainda não (tarefa **T3b**) | `360tools/tools/extract_stfs.py` (Python, stdlib) — *correção 2026-10-19 (T1)*: o CLI rexglue v0.10.0 não extrai STFS. A desencriptação AES+LZX **do XEX** continua a cargo do ReXGlue em `LoadXexImage`; alternativa pós-T4: ferramenta mínima sobre `StfsContainerDevice` |
| Decompilar PPC (opcional, para entender lógica) | ❌ | Java 21 + Ghidra, ou `powerpc-none-elf-objdump` do próprio SDK |
| Ler assets `.AMB/.AMA` | ❌ | formato proprietário; provavelmente não é preciso fora do jogo (o próprio código recompilado lê os ficheiros) |
| Áudio CRIWARE (`CPK/CSB`) | ❌ | risco conhecido; o SDK traz áudio XMA via FFmpeg — ver `PILOT_SONIC4.md` §5 |

## 5. Comandos de reprodução do inventário

```powershell
git --version; cmake --version; node --version; java -version; cargo --version
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' --version
& 'C:\Program Files\GitHub CLI\gh.exe' --version
& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -products * -format value -property installationVersion
Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm' -Recurse -File -Filter '*.exe'   # só clang-format/clang-tidy
```
