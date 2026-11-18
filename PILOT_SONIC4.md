# PILOT_SONIC4.md — análise do piloto *Sonic the Hedgehog 4: Episode I*

Tudo abaixo foi **observado nos ficheiros locais** (evidência em `evidence/`) ou **citado de fonte
identificada**. As inferências estão marcadas como *hipótese*.

## 1. O artefacto de entrada (o "ISO")

> **Correção de premissa:** não existe uma ISO em `C:\S4EP1Recomp`. Existe um **ZIP** que contém um
> **contentor XBLA/GOD**. O utilizador autorizou tratar o artefacto presente; nada foi modificado.

| Campo | Valor |
|---|---|
| Ficheiro | `C:\S4EP1Recomp\Sonic The Hedgehog 4 - Episode 1 (World) (XBLA).zip` |
| Tamanho | 183 432 129 bytes (174,93 MiB) |
| SHA-256 | `E2637A06980510AAEBFD58E48B4A1310CF80EB47BD01D15DA81BDB1F3EDD8FCA` |
| Magic | `50 4B 03 04` (`PK\x03\x04` — ZIP) |
| Conteúdo | **1 entrada**: `58410A07/000D0000/11A114D9F2871CF5D8DE04D9919B916C9D9B6AEC58` (199 815 168 B) |

O caminho interno corresponde ao layout de conteúdo do HDD Xbox 360: `<TitleID>/<ContentType>/<ContentID>`.
`000D0000` = **Content Type Games on Demand**. Extraído para
`game/58410A07/000D0000/11A114D9…` (SHA-256 `7DF56C7CAFB12C91BBDC0D92F0834A1589A5009B9CAAE0E4AAD98C33B25FA64B`).

## 2. O contentor: STFS

| Campo | Valor | Fonte |
|---|---|---|
| Magic | `LIVE` (`4C 49 56 45`) → **STFS assinado LIVE** | offset 0x0 |
| Content Type | `0x000D0000` (Games on Demand) | offset 0x344 |
| Metadata Version | `0x00000002` | 0x348 |
| Media ID | `0x2AA8DED1` | 0x354 |
| Version | `0x00000001` | 0x358 |
| **Title ID** | **`0x58410A07`** | 0x360 |
| Platform | `0x00` | 0x364 |
| Display Name | UTF-16BE em 0x411 → "Sonic 4 Episode I" | 0x411 / 0x1691 |
| Thumbnail | PNG embutido (`IHDR`/`iCCP`/`IDAT`/`cHRM`) | offset 0x1726 |

O **Title ID** `0x58410A07` e o **Media ID** `0x2AA8DED1` coincidem com listagens públicas
independentes (XboxDB, gist "Xbox 360 TitleIDs list", neperos, thread Digiex) que identificam
**Sonic The Hedgehog 4 (Episode I)** — evidência externa, consultada via web search nesta sessão.

## 3. O executável: `default.xex`

Nome encontrado na tabela de ficheiros (ASCII em `0xC3C0`, UTF-16LE em `0x330FF`).
Cabeçalho XEX2 localizado em **`0x0BC56000`** do contentor; o cabeçalho é claro (não encriptado),
o que permitiu descodificar a tabela de optional headers:

| Campo | Valor |
|---|---|
| Magic | `XEX2` |
| Module Flags | `0x00000001` |
| Size of Headers | `0x2000` |
| Security Info | offset `0x90` (dentro do XEX) |
| Optional headers | **15** |
| **Entry Point** | **`0x82223690`** |
| **Image Base Address** | **`0x82000000`** ← base "usual XBLA" suportada pelo runtime |
| System Flags | `0x4` |
| Import Libraries | tabela em `0x19B0` |
| Base File Format | em `0x0AE0`: `entrySize=36`, **encryption = 1 (retail)**, **compression = 2 (basic)** |
| XexSecurityInfo | `headerSize = 0x00000A3C`, `imageSize = 0x005D0000` (**6 094 848 B ≈ 5,81 MiB**) |

Consequências (fatos):
- O XEX está **encriptado (retail)** e **comprimido ("basic")** → **não** pode ser entregue cru a um
  recompilador que não faça AES+LZX. O ReXGlue faz (`xboxkrnl_crypt.cpp`, `lzx.cpp`).
  O `360tools/extract_pe.py` documenta limitações nesta variante de compressão.
- A imagem recompilável tem **~5,8 MiB** — pequena para os padrões 360 (>1 GB noutros títulos) → bom piloto.
- Base `0x82000000` evita a classe de títulos problemática (base `0x92000000`) do playbook 360tools.
- Existe um `DeltaPatchDescriptor` no XEX → **fechado na T2:** é **opcional** (`RexModuleConfig::delta_patches`),
  sem campo no manifest `.rex` e nenhum delta no contentor; **não** exige title update para o boot
  (cópia RTM; ver `evidence/T2_AUDIT.md` §3).

Existe **apenas 1** ocorrência de `XEX2` no contentor → há uma única imagem executável, e o `TitleInfo`
do contentor identifica-a explicitamente: `Path="default.xex"` (confirmado na T2).

## 4. Inventário de assets (tabela de ficheiros do STFS)

Nomes extraídos em UTF-16LE da região do *file table* (`evidence/stfs_filetable.txt`), organizados por pastas:

| Grupo | Conteúdo | Formato |
|---|---|---|
| (raiz) | `SHADER.AMB`, `STAFF_LIST.YSD`, `NNSTDSHADER` | `.AMB`, `.YSD` |
| `DEMO/*` | `BUY_SCREEN`, `CMN`, `LOADING`, `LOGO`, `MANUAL`, `OPTION`, `RANKING`, `STFRL`, `STGSLCT`, `TITLE`, `WORLDMAP` | `.AMB` (por idioma FR/GE/IT/JP/SP/US) |
| `G_COM/*` | `CPIT`, `EFF`, `ENE` (Hari), `GMK` (Animal, Capsule, Dash, Goal, Item, Needle, Marker, Spring, SS_Ring), `MENU` (`G_PAUSE*`), `PLY` (`SON_MDL/MTN/TEX`, `SSON_*` = Sonic/Super Sonic), `RING` | `.AM?` |
| `G_SS/*` | `MAP` (SS1..SS7 + `SS_ATTR/SS_M/SS_T`), `MAPFAR`, `GMK` (1UP, Arrow, Circle, Emerald, Endurance, Goal, Oblong, RingGate, Square, Time), `EFF` | `.AMB` |
| `G_ZONE1/2/3/4`, `G_ZONEF` | `MAP` (ZONE11..13, BOSS, E), `MAPFAR`, `ATTR`, `T`, `M_V`, `M_M`, `BOSS0x`, `DECO`, `EFF`, `ENE_*` (inimigos), `GMK_*` (cenário/objetos) | `.AMB` |
| `MSG/AO` | `AO_SYS_MSG_*`, `AO_SYS_SAVE_*` (6 idiomas) | `.MSG` |
| `SOUND/` | `SND_FX.CSB`, `SONICDL_SNG01.CPK`, `SONICDL_SNG01.CSB` | **CRIWARE** (`CPK`/`CSB`) |

Observações:
- O motor usa **arquivos proprietários `.AMB`** (modelos/motions/texturas/mapas) e CRIWARE para áudio.
- Há tabelas CRIWARE (`TBLCSB`, `TBLCUE`, `TBLSDL`, `TBLSYN`, `CpkTocInfo`, `DirName/FileName/FileSize/
  ExtractSize/FileOffset`) → o áudio é **bancos CRIWARE dentro de um CPK**.
- Os nomes são por *workspace* de produção (ex.: `Synth/S4FX/S4FX016a_S2_645F_44`), o que confirma que
  o título foi construído sobre o toolchain da SEGA/CRI.
- *Hipótese:* como o próprio código do jogo sabe ler `.AMB`/`.MSG`/CRIWARE, **não** é necessário escrever
  parsers de host na primeira fase — apenas garantir o VFS/paths corretos.

## 5. Riscos e mitigação

| Risco | Probabilidade | Impacto | Mitigação |
|---|---|---|---|
| Áudio CRIWARE (`CPK/CSB`) não suportado pelo runtime (que fala XMA/FFmpeg) | média-alta | médio (jogo pode correr mudo) | adiar áudio; investigar bancos CRIWARE-XMA depois de haver imagem |
| Codegen falha com `UnresolvedCall` | alta (esperado) | baixo | injetar `[entrypoint.functions]` (auto-resolve; média ~7 hints/título) |
| Link falha por export de kernel ausente (`XUsbcam*`) | média (~26% dos XBLA) | baixo | copiar `templates/overlay/src/stubs.cpp` do 360tools |
| `.AMB`/`.YSD` não interpretados → tela preta/menu vazio | média | alto | *hipótese* de que o código do jogo lê os ficheiros via VFS; validar nos primeiros logs/frames |
| Shaders não traduzidos corretamente | baixa | alto | o SDK transpila SM3→SPIR-V **em build** (sidecar `shaders.xex`); se o resultado falhar, considerar XenosRecomp |
| SDK jovem, API em mudança (v0.10.0) | média | médio | pin da versão; ler o código local antes de assumir flags |
| `DeltaPatchDescriptor` a exigir *title update* | **fechado** | — | T2: é opcional e não é exigido para este título (RTM); nenhum TU a obter |
| Submodules do SDK não inicializados (T4) | alta | baixo | `git submodule update --init` dos 12 exigidos (Windows/D3D12) |
| Driver Vulkan 1.0 ausente no host | baixa | alto | confirmar/atualizar driver da GPU antes da T6 |
| Builds longos escondendo erros | alta | médio | usar harness/tiers; registar sempre exit codes e logs |

## 6. Critérios de sucesso (progressivos, sem marketing)

1. **Extração** — `default.xex` obtido e magic `XEX2` conferido. *(cabeçalho já analisado no contentor;
   falta a extração do contentor — tarefa T3b via `extract_stfs.py`, ver `ARCHITECTURE.md` §4.0)*
2. **Codegen completo** — `rexglue codegen` termina; nº de `UnresolvedCall` resolvidos registado.
3. **Build do projeto** — link sem símbolos por resolver (ou com stubs explícitos e rastreados).
4. **Runtime inicia** — log com `Runtime initialized successfully` / `Loading XEX image`.
5. **Código do jogo executa** — visita a PCs RVA dentro da imagem (ex.: perto de `0x82223690`), sem falha imediata.
6. **Inicialização avançada** — subsistemas a direito (VFS/paths, gráficos, input) e o jogo não morre no arranque.
7. **Imagem exibida** — primeiro frame/menu apresentado.
8. **Jogável** — controlo + progressão de nível.

Cada marco só é declarado com **evidência de execução** registada (log/exit code/screenshot), nunca por inferência.
