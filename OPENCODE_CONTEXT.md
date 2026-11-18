# OPENCODE_CONTEXT.md

> **Documento autossuficiente.** Escrito para iniciar um **novo chat** do OpenCode que **não** tem acesso
> ao histórico da conversa anterior. Fonte de continuidade = os ficheiros deste workspace.
> Última atualização: **2026-10-19**. Estado: **Fase 0 + T1 + T2 concluídas** (interface do CLI e
> auditoria técnica do SDK v0.10.0 verificadas no código; evidência em `evidence/T2_AUDIT.md`);
> nenhuma recompilação iniciada, nada instalado; ZIP original intacta (SHA-256 re-verificado a 2026-10-19).

---

## 0. Instruções para o próximo chat (ler primeiro)

> Leia `OPENCODE_CONTEXT.md` integralmente antes de executar qualquer ação. Em seguida, leia `README.md`,
> `DECISIONS.md`, `NEXT_STEPS.md` e os documentos relevantes para a tarefa atual.
>
> Não presuma que o histórico de conversas anteriores está disponível. Utilize os arquivos do workspace
> como fonte de continuidade e verifique o estado real dos artefactos antes de confiar em informações registradas.
>
> Não repita pesquisas, inventários ou builds já concluídos sem uma razão concreta. Não instale ferramentas
> sem verificar os requisitos e justificar a necessidade. Não inicie compilações longas antes de validar a
> arquitetura e as dependências. Preserve a ISO original e todos os artefatos existentes.
>
> Trabalhe em etapas curtas, com critérios de conclusão verificáveis. Atualize a documentação quando
> decisões, ferramentas ou resultados mudarem. Se encontrar contradições entre os documentos e os arquivos
> reais, investigue e registre a correção.
>
> Ao começar uma nova sessão, apresente um resumo curto do estado atual e execute a próxima tarefa pendente
> descrita em `NEXT_STEPS.md`.

---

## 1. Objetivo do projeto

- Construir uma **infraestrutura reutilizável** para criar ports nativos de jogos de **Xbox 360** para
  **Windows x64** e, mais tarde, avaliar **Android ARM64**.
- Começar por um piloto deliberadamente simples: **Sonic the Hedgehog 4: Episode I** (XBLA/GOD, Title ID `0x58410A07`).
- Ordem de objetivos: (a) execução funcional no Windows x64; (b) só depois, avaliar ARM64/Android.
- Método: estudar projetos públicos funcionais, escolher uma base integrada, validar dependências,
  recompilar e iterar com evidência de execução. **Sem** "marketing": cada marco exige prova.

## 2. Novo workspace e arquivos

- **Workspace principal:** `C:\S4EP1Recomp` (novo; é aqui que tudo acontece).
- **Workspace antigo:** `C:\SARTRecomp` → **exclusivamente referencial** (lições). Não modificar, não reutilizar código.
- **Ficheiros criados neste workspace:** `README.md`, `TOOLS.md`, `ARCHITECTURE.md`, `REFERENCE_PROJECTS.md`,
  `DECISIONS.md`, `PILOT_SONIC4.md`, `NEXT_STEPS.md`, `OPENCODE_CONTEXT.md`, `.gitignore`,
  `tools/{scan_stfs_xex.js,probe_xex_meta.js,scan_stfs_filetable.js}`, `evidence/*.txt`,
  `game/<dados do jogo, git-ignored>`, `third_party/{rexglue-sdk,XenonRecomp,XenosRecomp,360tools}`.

### 2.1 O artefacto de entrada — **correção de premissa importante**

**Não existe uma ISO** em `C:\S4EP1Recomp`. Existe:

| Campo | Valor verificado |
|---|---|
| Ficheiro | `C:\S4EP1Recomp\Sonic The Hedgehog 4 - Episode 1 (World) (XBLA).zip` |
| Tamanho | 183 432 129 bytes (174,93 MiB) |
| SHA-256 | `E2637A06980510AAEBFD58E48B4A1310CF80EB47BD01D15DA81BDB1F3EDD8FCA` |
| Formato | ZIP (`PK\x03\x04`) com **1 entrada** |
| Entrada | `58410A07/000D0000/11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58` (199 815 168 B) |
| Formato interno | **STFS** magic `LIVE`; Content Type `0x000D0000` (Games on Demand) |
| Título | **Sonic The Hedgehog 4 (Episode I)** — Title ID `0x58410A07`, Media ID `0x2AA8DED1` |
| Cópia local | `game\58410A07\000D0000\11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58` (SHA-256 `7DF5…A64B`) |

O ZIP foi extraído (só o contentor) com .NET `ZipFile`; o ZIP original **não** foi alterado
(`LastWriteTime` inalterado — verificado). Os dados do jogo **não** podem ser redistribuídos.

## 3. Histórico técnico relevante (projeto anterior `C:\SARTRecomp`)

Contexto de Sonic & All-Stars Racing Transformed (Xbox 360) → C++ com **XenonRecomp**. Marcado por grau de confiança.

### 3.1 Factos confirmados
- **XenonRecomp é apenas um gerador de código** ("does not provide a runtime implementation"). Todo o
  runtime é responsabilidade de quem porta. No projeto anterior foi escrito um runtime mínimo próprio.
- **Convenção do código gerado:** `void sub_<VA>(PPCContext& __restrict ctx, uint8_t* base)`. O estado da CPU
  (todos os registos) vai no `ctx`; o segundo argumento é a base da memória guest 32-bit. Imports aparecem
  como `__imp__*`. Saltos indiretos exigem uma **tabela de funções** (dispatch) construída no runtime.
- **MSVC vs código gerado:** o projeto anterior foi **MSVC-only** e gastou enorme esforço a contornar
  incompatibilidades e a "corrigir" saídas do gerador. O XenonRecomp recomenda **Clang** e diz que outros
  compiladores "have not been tested and are not recommended, **including for recompilation output**".
  → Lição: **não** usar MSVC no código gerado.
- **`// ERROR` por saltos para o interior de funções:** foram encontrados **16 065** sites que o gerador
  marcava como erro, todos correspondentes a saltos que caem no *meio* de funções vizinhas (epílogos
  `restgprlr`/`restvmx`, etc.). A solução aplicada foi **manual** (shims gerados a partir de fatias do
  corpo, com labels sintéticos para saltos para trás). Interpretação (hipótese forte): a causa raiz é a
  **ausência de configuração dos endereços de ABI-helpers** no TOML do XenonRecomp
  (`savegprlr_*`/`restgprlr_*`, `restfpr_*`, `restvmx_*`/`savevmx_*`, `setjmp`/`longjmp`). No ReXGlue
  esta deteção é **automática**.
- **Depurou-se até** (com evidência de execução real): o binário compilou e ligou (link EXIT=0) e o
  **runtime iniciou** e chegou a executar **código do jogo** — a última verificação registada mostrava
  **33 chamadas de import** de **11 tipos** (`NtAllocateVirtualMemory`, `KeTlsAlloc/SetValue/Free`,
  `RtlEnter/LeaveCriticalSection`, `RtlInitializeCriticalSection(AndSpinCount)`, `KeQuerySystemTime`,
  `XexCheckExecutablePrivilege`, …) antes de falhar por acessos a TLS via `r13`/`[r13+0x100]` (o registo
  de *small data / thread block* do PowerPC, que o loader tem de inicializar).
- **Harness de testes:** foram construídos verificadores que executam funções recompiladas isoladamente
  e validam registos/memória (passaram). **Isto não prova que o jogo corre** — é uma unidade isolada.

### 3.2 Resultados relatados (não re-verificados nesta sessão)
- Contagens de ficheiros/unidades (`3126` unidades, `319,3 MB` de C++ gerado), tamanho do executável
  intermédio (~82 MB) e do mapa (/MAP ~91 MB), e o número de sites corrigidos.
- Não há neste workspace cópia desses artefactos; estão em `C:\SARTRecomp` (não os copiar).

### 3.3 Armadilhas aprendidas (válidas para qualquer título)
- `PPC_LOAD_U32/PPC_STORE_U32(x,y)` (nosheaders gerados do XenonRecomp) expandem `base + (x)` — não passar `base + x`.
- Ficheiros `.bat` no Windows: CRLF; e `IF` do cmd interpreta índices com zeros à esquerda como **octal**.
- Comandos muito longos inline falham por limite de tokens → usar ficheiros `.js`/`.ps1`.
- Builds longos escondem erros → **registar sempre exit code + log**, e nunca apagar funções/ficheiros para "fazer compilar".
- Não declarar sucesso sem execução observada; separar claramente "compilou" de "executou".

### 3.4 Conclusão deste histórico
O projeto anterior provou que **o gargalo não é a recompilação PPC** (o XenonRecomp funcionou), mas o
**runtime** (kernel/GPU/áudio/VFS) e a **configuração correta do gerador**. Por isso a nova iniciativa
escolhe uma base que **já inclui** essas peças (ver §5).

## 4. Ferramentas e referências

### 4.1 URLs dos projetos investigados
- ReXGlue SDK (base escolhida): https://github.com/rexglue/rexglue-sdk
- XenonRecomp: https://github.com/hedge-dev/XenonRecomp
- XenosRecomp: https://github.com/hedge-dev/XenosRecomp
- UnleashedRecomp: https://github.com/hedge-dev/UnleashedRecomp
- Xenia: https://github.com/xenia-project/xenia
- rexglue-recomps (piloto XBLA South Park + knowledge base): https://github.com/superheher/rexglue-recomps
- 360tools (playbook prático sobre ReXGlue): https://github.com/sp00nznet/360tools
- simpsonsarcade (exemplo XBLA): https://github.com/sp00nznet/simpsonsarcade
- N64Recomp (linhagem conceptual): https://github.com/N64Recomp/N64Recomp
- Wiki do SDK (Getting Started, ReXApp, TOML config, Kernel State): https://github.com/rexglue/rexglue-sdk/wiki

### 4.2 Ferramentas verificadas na máquina (2026-10-18)
Git 2.55.0 · CMake **4.5.0-rc1** · Ninja **1.12.1** (dentro do VS, fora do PATH) · VS 2022 Community
**17.14.37806.7** (MSVC, Windows SDK) · Node.js **24.21.0** · Java **21.0.12.1 LTS** · Rust/Cargo 1.99.1 ·
7-Zip 24.08 (fora do PATH) · GitHub CLI 2.102.0 (fora do PATH) · curl/tar do Windows.
Detalhes e reprodução em `TOOLS.md` e `evidence/tools_inventory.txt`.

### 4.3 Dependências **ausentes** (bloqueios — refinado na T2)
- **Clang 18+ / LLVM** → **BLOQUEIA** o build do projeto gerado (D6; o preset usa Clang). O VS 2022 tem
  apenas `clang-format.exe`/`clang-tidy.exe`. Opções: componente VS `Microsoft.VisualStudio.Component.VC.Llvm.Clang`
  ou LLVM standalone ≥ 18 (playbook 360tools: 21.x). *Nota T2:* o próprio SDK também compila com MSVC C++20
  (≥19.40) e testa Clang 14–21; a exigência Clang ≥ 18 prende-se ao **código gerado**.
- **Python 3.10+** → **BLOQUEIA a extração STFS (T3b)** — o CLI rexglue v0.10.0 não expõe extração;
  usa-se `360tools/tools/extract_stfs.py` (só stdlib, **sem** pycryptodome). `python` neste sistema é o
  *stub* da Microsoft Store: tratar como ausente até `python -V` responder de verdade.
- **.NET SDK** → desnecessário (hostfxr do runtime é **opcional**; falta só degrada graciosamente).

### 4.3bis Descobertas da T2 que afetam o build/execução (detalhe em `evidence/T2_AUDIT.md`)
- **Symlinks `libmspack`:** os **14** links git `120000` continuam presentes na v0.10.0 → materializar como
  **junctions NTFS** na T4 (fork `github.com/SummerFrog/libmspack`). Sem submodules.
- **Rede no 1º configure do SDK:** DXC vem por `FetchContent` do GitHub → T4 precisa de rede (ou `_deps` pré-semeados).
- **Vulkan 1.0:** requisito do runtime gráfico (inicializado antes do módulo do jogo; SPIR-V é o alvo
  canônico). Confirmar o driver do host na T6.
- **Shaders:** tradução **em build** (→ SPIR-V, sidecar `shaders.xex`), **não** em runtime; DXC/glslang são
  ferramentas de build. (Corrige a premissa anterior.)
- **Title update/DeltaPatch:** **nenhum necessário** — é 100% opcional; a cópia é RTM.
- **Ordem de init do runtime:** settings → .NET(opt) → Vulkan → plugins → shader cache → XAudio2 → módulo do jogo.

### 4.4 Licenças (para reutilização)
- ReXGlue SDK: texto permissivo **BSD-3-Clause-style** ("Other" na API), com obrigação de atribuição ao **Xenia**.
- XenonRecomp / XenosRecomp / N64Recomp / 360tools / simpsonsarcade: **MIT**.
- **UnleashedRecomp: GPL-3.0** (copyleft → não copiar código).
- Xenia: "NOASSERTION" na API (estilo BSD-3; confirmar o LICENSE no repo antes de publicar artefactos).
- rexglue-recomps: **sem licença declarada** → estudar apenas, nunca copiar.

### 4.5 Caminhos e comandos confirmados localmente
```powershell
# triagem do contentor (só leitura; sem instalações)
node C:\S4EP1Recomp\tools\scan_stfs_xex.js       "C:\S4EP1Recomp\game\58410A07\000D0000\11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58"
node C:\S4EP1Recomp\tools\probe_xex_meta.js      "…mesmo ficheiro…"
node C:\S4EP1Recomp\tools\scan_stfs_filetable.js "…mesmo ficheiro…" 0xC0000
# ambiente de dev do VS + LLVM (exemplo do playbook)
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
```
Saídas guardadas em `evidence/{stfs_xex_scan,xex_meta,stfs_filetable,tools_inventory}.txt`.
Clones de estudo (só leitura): `third_party/{rexglue-sdk,360tools,XenonRecomp,XenosRecomp}`.

## 5. Decisões e pendências

### 5.1 Decisão de arquitetura (escolhida)
**ReXGlue SDK v0.10.0 como recompilador + runtime integrados**, consumindo o contentor STFS diretamente.
Justificação: traz prontas as peças difíceis (kernel/XAM HLE, GPU D3D12/Vulkan herdados do Xenia, áudio
XMA via FFmpeg, input SDL3, VFS, extracção STFS/XEX/AES/LZX, deteção automática de jump-tables/ABI-helpers/RTTI),
com licença permissiva e evidência de execução em títulos **XBLA** — a mesma classe do piloto.
Detalhes e comparação em `REFERENCE_PROJECTS.md`; pipelines em `ARCHITECTURE.md`; ADRs em `DECISIONS.md`.

### 5.2 Alternativas e porquê descartadas
- **XenonRecomp + runtime próprio** (o caminho do `C:\SARTRecomp`): o custo real está no runtime. Descartado.
- **Fork de UnleashedRecomp**: GPL-3.0 e amarrado ao Unleashed. Descartado como base.
- **Xenia**: emulação, não port nativo. Descartado.

### 5.3 Questões técnicas a responder antes/durante a recompilação (ver `DECISIONS.md`)
1. Flags exatas do `rexglue init` na v0.10.0 (mudaram entre v0.8 e v0.10) → ler `src/rexglue/commands/init_command.cpp` (v0.10: mudou de sítio).
   → **✅ respondido (2026-10-19, T1):** grafia com hífen, ficheiro em `src/rexglue/commands/init_command.cpp:566-581`.
2. `--xex-path` aceita o **contentor STFS** ou exige `default.xex` extraído?
   → **✅ respondido (2026-10-19, T1): NÃO aceita o contentor** (loader só XEX/ELF) e o CLI não tem
   extração → obrigatória a nova tarefa **T3b** (Python + `extract_stfs.py`). Detalhes: `DECISIONS.md`.
3. O fix dos *symlinks* do `libmspack` ainda se aplica na v0.10.0?
   → **✅ respondido e corrigido (2026-10-19, T2+T4): NÃO.** Era gotcha da **v0.8.0**. Na v0.10.0
   `thirdparty/libmspack` é **submodule git** (`kyz/libmspack`), sem symlinks; correção real = inicializar submodules.
4. Áudio **CRIWARE** (`CPK/CSB`) vs áudio XMA do runtime — que trabalho é preciso?
   → **ainda aberta** (não é bloqueante da 1ª execução; ver `PILOT_SONIC4.md` §5).
5. Shaders: a tradução em runtime (herança Xenia) cobre este título? (evitar XenosRecomp sem necessidade)
   → **✅ premissa corrigida (2026-10-19, T2): a tradução é em BUILD** (→ SPIR-V, `shaders.xex`); a
   cobertura dos shaders do título só se confirma no 1º frame (T7).
6. `.AMB/.AMA/.YSD` são lidos pelo código do jogo via VFS? (esperado sim)
   → **✅ reetiquetada (2026-10-19, T2)** como experimento do primeiro arranque (T7); `game:\` é VFS genérica.
7. `DeltaPatchDescriptor`/title update: é necessário para o boot?
   → **✅ respondido (2026-10-19, T2): NÃO** — 100% opcional; cópia RTM; não bloqueia nada.

### 5.4 Próxima tarefa recomendada e critério de conclusão
Seguir `NEXT_STEPS.md` pela ordem: **T1 ✅** e **T2 ✅** concluídas (2026-10-19). A próxima é **T3**
(**autorização do utilizador para instalar Clang ≥ 18 + Python 3** — quanto mais não seja porque sem
Python não há `default.xex`), depois **T3b** (extrair STFS → `game\extracted\`), **T4** (construir/instalar
o SDK; submodules inicializados), **T5** (init + codegen), **T6** (build + 1ª execução com
`run1.log`), **T7** (iterar sobre o primeiro obstáculo real).
Critério da próxima entrega: um `run1.log` com exit code e progresso observável, e o primeiro obstáculo
real identificado por **evidência**, não por suposição.
**Nada deve ser instalado sem autorização explícita do utilizador (T3 está pendente).**

## 6. O que **não** repetir (anti-padrões herdados)

- Não reimplementar um runtime quando existe uma base integrada mantida.
- Não usar MSVC para o código gerado.
- Não "corrigir instruções uma a uma" antes de a cadeia completa arrancar; não escrever shims manuais para
  saltos intra-função (o ReXGlue deteta ABI-helpers/jump-tables automaticamente).
- Não remover funções/unidades para fazer compilar; não mascarar falhas com stubs de sucesso artificial.
- Não declarar marcos sem log/exit code/screenshot; separar sempre "código compilado" de "código executado".
- Não distribuir dados do jogo; não alterar o ZIP original nem `C:\SARTRecomp`.
