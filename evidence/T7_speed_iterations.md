# T7 - Iterações de correção de velocidade

## T7_SPEED_01
- **Data/Hora**: 2026-10-20
- **Hipótese**: Frequência do guest clock incorreta (usando host QPC em vez de 50 MHz do Xbox 360)
- **Alteração**: `third_party/rexglue-sdk/src/core/clock.cpp` linha 30 - set `guest_tick_frequency_ = 50000000ULL`
- **Build**: Sucesso
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe`
- **Teste**: Executado com argumentos corretos; velocidade continua acelerada. Forçar FPS limit externo (Afterburner/RivaTuner) para 30/60 não alterou aceleração.
- **Status**: FALHOU - Hipótese não explica aceleração
- **Notas**: Alteração mínima; próximo passo precisa investigar clock scaling/ratio diferente.

## T7_SPEED_02
- **Data/Hora**: 2026-10-20
- **Hipótese**: Clock scaling com valor 2x. Testar scalar 0.5x (reduzir pela metade) para compensar aceleração 2x.
- **Alteração**: `third_party/rexglue-sdk/src/core/clock.cpp` linha 28 - `guest_time_scalar_ = 0.5`
- **Build**: Sucesso
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe`
- **Teste**: Velocidade permaneceu igual (nem mais, nem menos acelerada).
- **Status**: FALHOU - guest_time_scalar_ não está sendo aplicado no caminho que causa esta aceleração
- **Notas**: Possível que o jogo use diretamente contadores de alta resolução ou caminho diferente.

## T7_SPEED_03
- **Data/Hora**: 2026-10-20
- **Hipótese**: Os métodos `ScaleGuestDuration*` podem não estar sendo aplicados pelo caminho usado pelo jogo. Adicionei fator 0.5 como tentativa compensatória para reduzir velocidade global.
- **Alteração**: `third_party/rexglue-sdk/src/core/clock.cpp` - `ScaleGuestDurationMillis`, `ScaleGuestDurationFileTime` multiplicados/divididos por 0.5
- **Build**: Sucesso
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe`
- **Teste**: Não resolveu. Medição: fase completa em ~1:09 no jogo vs ~0:51-0:57 real (com 240fps ou 60fps forçado) - aceleração ~1.2-1.35x persistente.
- **Status**: FALHOU
- **Notas**: Ao pressionar contra parede velocidade tende a normalizar. Forçar FPS com limitador externo não afeta - sugere que simulação física/loop não está sincronizada por FPS/render, mas por algum contador de tempo direto.

## T7_SPEED_04
- **Data/Hora**: 2026-10-20
- **Hipótese**: Usando medição precisa do usuário (~1.35x mais rápido: 1:09 jogo vs 0:51 real), aplicar `guest_time_scalar_ = ~0.74` (1/1.35). Testando 0.77 inicialmente.
- **Alteração**: `third_party/rexglue-sdk/src/core/clock.cpp` linha 28 - `guest_time_scalar_ = 0.77`
- **Build**: Sucesso
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe`
- **Teste**: Não resolveu conforme feedback.
- **Status**: FALHOU
- **Notas**: Baseado em medição real com timer. A variação ao colidir com parede sugere que em momentos de colisão/idle alguma coisa altera temporização.

## T7_SPEED_05
- **Data/Hora**: 2026-10-20
- **Hipótese**: Usando observação crítica (velocidade varia com colisão contra parede - tende a normalizar), talvez clock scaling ativo cause inconsistências. Forçar `clock_no_scaling = true` por padrão e restaurar scalar=1.0.
- **Alteração**: `third_party/rexglue-sdk/src/core/clock.cpp` - `clock_no_scaling` default true, `guest_time_scalar_ = 1.0`
- **Build**: Sucesso
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe`
- **Teste**: Velocidade continua variável (varia por área - desacelera um pouco em algumas áreas, acelera mais em outras). Não resolvido.
- **Status**: FALHOU
- **Notas**: Comportamento variável por área confirmado pelo usuário. Clock changes revertidos para defaults.

## T7_SPEED_06
- **Data/Hora**: 2026-10-20
- **Hipótese**: Jogo roda sem limitação adequada, alcançando ~240 FPS e velocidade depende da taxa de frames (simulação avança a cada frame). Sonic 4 no PC teve esse comportamento; recompilações estáticas precisam limitar a 60 FPS se simulação é frame-tied.
- **Investigação**: Buscado mecanismos de FPS limiting/VSync/frame pacing no SDK. Não encontrados limitadores explícitos em código; UI loop não tem throttling óbvio. Clock revertido aos padrões. Build gerado com estado funcional.
- **Alteração**: Nenhuma alteração de código nesta tentativa - investigação em andamento. Aguardando análise mais profunda do loop e possíveis mecanismos existentes.
- **Build**: Sucesso
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe`
- **Status**: INVESTIGANDO - Sem alteração aplicada ainda, aguardando entendimento melhor do pipeline
- **Notas**: Seguindo instrução - investigar antes de aplicar correção. Precisa entender se há throttle em algum lugar.

## T7_SPEED_07
- **Data/Hora**: 2026-10-20
- **Hipótese**: Jogo é frame-tied e roda a ~240 FPS causando aceleração variável por área. Implementar limitação controlada de 60 FPS no ponto apropriado. Pesquisado - não encontrado limitador FPS/VSync explícito no SDK. Adicionada tentativa mínima de pacing em KeDelayExecutionThread e Sleep sem alterar clock scaling/physics.
- **Alteração**: `third_party/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_threading.cpp` (KeDelayExecutionThread_entry - adicionado comentário/estrutura), `third_party/rexglue-sdk/src/core/threading_win.cpp` (ajuste menor)
- **Build**: Sucesso
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe`
- **Status**: AGUARDANDO TESTE - Build entregue
- **Notas**: Alterações mínimas e conservadoras. Não alterando guest_time_scalar/clock_no_scaling. Foco em tentar pacing sem refatoração ampla.

## T7_SPEED_08
- **Data/Hora**: 2026-10-20
- **Hipótese (confirmada no código)**: Loop de frame do guest (`sub_821C0E58` → único `VdSwap` no binário recompilado) avança sem esperar refresh; host faz `Present(0, …)` sem vsync; contador GPU `counter_` incrementava em **cada** `PM4_XE_SWAP` (~240 Hz) **e** no worker de vblank (~60 Hz), desincronizando waits do guest. `guest_time_scalar`/`clock_no_scaling` não afetam esse caminho (mftb via `REX_QUERY_TIMEBASE` já mapeia QPC→50 MHz corretamente).
- **Alteração**:
  - `xboxkrnl_video.cpp`: `PaceGuestFrameAfterVdSwap()` após `VdSwap_entry` (cvar `Clock > guest_frame_pacing`, default true; usa `video_mode_refresh_rate` / 60 Hz); cvar `Clock > timing_log_interval_sec` para log periódico.
  - `command_processor.cpp`: removido `++counter_` em `ExecutePacketType3_XE_SWAP` (contador só via `MarkVblank`).
  - Removidos stubs ineficazes em `KeDelayExecutionThread` / comentários em `Sleep`.
- **Build**: SDK Release + `pilot` preset `win-amd64-release` OK.
- **Executável**: `pilot/out/build/win-amd64-release/sonic4.exe` (+ `rexruntime.dll`, `rexgpu-xenos.dll` atualizados 2026-10-20 22:20).
- **Teste automático**: jogo não executado aqui (requer `--game_data_root` local).
- **Validação sugerida**: F4 → `Clock > guest_frame_pacing=true`; opcional `timing_log_interval_sec=5` ou `Perf > perf_log_csv=pace.csv` (Debug); medir cronômetro in-game vs relógio real e consistência entre áreas.
- **Status**: AGUARDANDO TESTE MANUAL

## T7_SPEED_09
- **Data/Hora**: 2026-10-20
- **Evidência do teste T7_SPEED_08**: Log confirmou ~59.8 VdSwap/s (pacing OK), mas usuário ainda vê ~240 FPS no overlay (180 Hz monitor) e velocidade variável por área.
- **Diagnóstico refinado**:
  - 240 FPS ≠ loop guest: host `Present(0)` pode apresentar múltiplas vezes por frame guest (overlay mede presents/GPU, não simulação).
  - Pacing só em `VdSwap` não cobre caminhos que pulam swap; loop principal é `sub_8249E2F8` (chamado em loop em `sub_82164790`).
  - Update físico principal: `sub_821B35C8` recebe delta em `f1` (segundos); deltas acima de 1/60 s causam passos grandes / catch-up irregular.
- **Alteração**:
  - `PaceGuestMainLoopFrame()` exportado; removido pacing de `VdSwap_entry`.
  - `pilot/src/sonic4_timing_hooks.cpp`: hook em `sub_8249E2F8` (pacing pós-frame) e `sub_821B35C8` (clamp dt via `GuestClampPhysicsDeltaSeconds`).
  - Cvars: `Clock > guest_max_physics_dt` (default 1/60), `Clock > guest_physics_dt_scale` (default 1.0).
- **Build**: SDK + pilot OK.
- **Teste sugerido**: mesmo comando T7_SPEED_08; log deve mostrar `Guest frame pacing:`; se ainda ~1.35x rápido, tentar `guest_physics_dt_scale=0.74` via CLI/F4.
- **Status**: AGUARDANDO TESTE MANUAL

## T7_SPEED_10
- **Data/Hora**: 2026-10-20
- **Feedback usuário (T7_SPEED_09)**: Velocidade inalterada; variação por área igual; não testou `guest_physics_dt_scale=0.74`.
- **Causa provável do T7_SPEED_09 ineficaz**: hook em `sub_821B35C8` só ajustava `f1`, mas a função usa **`f30←f2`** na entrada; render chama física com `f1`–`f6` preenchidos.
- **Alteração**: clamp **f1–f6**; cap em frames (default 1.0) se `|dt|>0.25`; log `Physics steps: …/s` vs main loop; cvar `guest_use_fixed_physics_dt`.
- **Status**: AGUARDANDO TESTE MANUAL

## T7_SPEED_17
- **Feedback (T16)**: Splash rápido + tela branca de novo.
- **Ação**: pilot volta a **só** `PaceGuestMainLoopFrame` em `8249E2F8` (estado jogável T14). Budget/clamp **default off** (F4 ainda pode ter eles ligados; sem hook não aplicam).
- **Aberto**: velocidade variável por área **não** corrigida — reescrever f1 quebra o boot.

## T7_SPEED_19 (análise + instrumentação — 2026-10-21)
- **Meta**: encontrar a rotina que realmente avança gameplay, sua fonte temporal e por que varia por área.
- **Mapa da sub-sistema de tempo (análise estática do recompilado)**:
  - `REX_QUERY_TIMEBASE()` = `Clock::QueryGuestTickCount()` = `UpdateGuestClock()` (clock.cpp): acumulador **virtual 50 MHz** que segue o QPC real.
  - `sub_821D30E0` (recomp.29) — **temporizador da cena**; chamado 1×/frame por `sub_821C0E58`; calcula delta real em ticks (mftb atual − anterior) no **campo 21792**.
  - `sub_821D2478` (recomp.19) — **getter** com switch por query-id: caso0 `f1 = delta(21792)*escala(21808)` (→ segundos), caso1 `escala(21804)/delta`, etc.
  - `sub_82446628` (recomp.61) / `sub_82447348` (recomp.55) — **segundo temporizador de módulo** (0x82446/7); calcula delta e chama `sub_82446148`.
  - `sub_821D2B80` (recomp.24) lê 21792 e chama o getter, mas é **handler de eventos** (switch msg 226/227/255), não stepper por frame.
- **Probe T7_SPEED_PROBE (probe2.log)**: loop principal `8249E2F8` = **58.8/s** (paced OK); present host = 238/s; TODAS as funções sondadas (821B35C8/821C1C80/8249CC00/8249D308) são de **renderização** (f1=0 ou const 1.0) a 173–886/s. **Nenhuma capturou o passo de gameplay.**
- **Bug provado (código)**: `PaceGuestMainLoopFrame()` chama `ResetGuestClockHostBaseline()` (xboxkrnl_video.cpp:418) após o sleep; clock.cpp:164-170 move `last_host_tick_count_=now` **sem creditar** `last_guest_tick_count_` → **descarta o tempo de sleep** do relógio guest. Efeito: relógio guest avança ~`(work/frame)×50MHz` (atrás do real); valor perdido **varia por área** (work por frame varia). Prediz SLOWDOWN, não o SPEEDUP observado → indica que o gameplay rápido NÃO consome este relógio (é frame-tied) OU o estado runtime difere. **Precisa medição.**
- **Alteração (observation-only, sem tocar registos do guest)**: `sonic4_timing_hooks.cpp` —
  - `ProbeClockRate()`: mede taxa efetiva do relógio guest (QueryGuestTickCount) vs host a cada 5s → linha `CLOCKPROBE ... guest_effective_hz (target 50000000)`.
  - Hooks de contagem em `sub_821D30E0` e `sub_82446628` → linha `TIMERS scene_821D30E0 X/s module_82446628 Y/s (vs main Z/s)`.
- **Build**: ninja incremental pilot OK (EXIT=0). `sonic4.exe` novo.
- **Teste**: rodar e comparar `CLOCKPROBE` (relógio correto ~50e6 vs descartando tempo <<50e6) e `TIMERS` (temporizadores ~58.8/s = 1×/frame vs mais rápido). Interpretação na próxima iteração.
- **Status**: BUILD — AGUARDANDO TESTE MANUAL (comando em NEXT_STEPS/abaixo).

Comando de teste (T7_SPEED_19):
```powershell
cd "C:\S4EP1Recomp\pilot\out\build\win-amd64-release"
.\sonic4.exe --game_data_root="C:\S4EP1Recomp\game\extracted" --gpu_plugin=xenos --log_file="C:\S4EP1Recomp\evidence\T7_SPEED_19\t7_speed_19.log"
```

## T7_SPEED_19 — resultado do teste (log `T7_SPEED_19\t7_speed_19.log`, 2026-10-21)
- **Sessão**: 23:56:39→23:58:36, 23 janelas CLOCKPROBE/TIMERS de ~5 s (115.19 s host medido), sem falhas de amostragem.
- **CLOCKPROBE** (Δjanela = guest_now−guest_start entre dois probes; cada janela inclui os sleeps e resets dos ~29 frames intermédios, logo a medição é válida apesar de o probe correr antes de `PaceGuestMainLoopFrame`):
  - por janela: 49,431,840–50,001,352 Hz → **ratio 0.9886–1.0000** (nunca zerado, nunca acima do alvo);
  - agregado: 5,743,311,295 ticks / 115.187 s = **49,860,758 Hz (0.9972×)** → perda total **0.32 s (0.28%)**;
  - pior janela (0.9886, 23:57:40→45) coincide com a maior carga de render (viewport/main 21.10); janelas leves (menu/início e fim, vp/main 9.8–16) dão 0.999–1.0000.
  - **Conclusão**: `ResetGuestClockHostBaseline()` descarta tempo (existe no código), mas o efeito líquido é só ~0.3% (pior 1.14%) porque queries concorrentes de outros threads creditam o acumulador durante o sleep. **Refuta** a previsão de colapso do clock durante o sleep; **descarta B como causa da aceleração** (direção = desaceleração, magnitude ≤1.1%).
- **TIMERS**:
  - `scene_821D30E0`: 58.4–60.0/s (média 59.43) = `8249E2F8` (média 59.44) → **exatamente 1× por iteração do loop** (ratio 1.000 em 22/23 janelas; 0.997 só na 1ª, durante carga);
  - `module_82446628`: **0.0/s em todas as 23 janelas** (0 chamadas em 115 s); único caller estático = `sub_82435B98` (recomp.45:31069), alcançado só por dispatch indireto → inativo nesta sessão;
  - campo `(vs main 0.0/s)` = artefacto: `MaybeLog()` faz `dump("8249E2F8", g_main)` (troca `g_main.calls` por 0) **antes** da linha TIMERS; usar a linha PROBE 8249E2F8 como main.
  - **Conclusão**: descarta C-frequência (nenhum timer atualiza mais rápido que o frame); consumo do delta continua não medido.
- **Taxas de contexto**: Host Present 225.8–239.6/s (média ≈238, sem vsync); `821B35C8` 579.2–1252.4/s = **9.84–21.05 chamadas/loop principal, variando por área**; `821C1C80` 232.4–715.6/s; `8249CC00`/`8249D308` 173.5–298.2/s.
- **Coerência**: todos os caminhos medidos ≤1× (clock 0.997, loop 59.44 Hz ≈ −0.9% vs 60, timer 1×/frame) mas o utilizador mede ~1.2–1.35× (T7_SPEED_03) → **a rotina que avança gameplay não está entre as medidas** (A aberta); a perda de clock não pode causar aceleração.
- **Suspeito principal novo/antigo**: `sub_821B35C8` (corpo `sonic4_recomp.44.cpp:8952–9301`): consome **f1,f2,f5,f6** (`fmr f31,f1; f30,f2; f29,f5; f28,f6`), faz aritmética tipo lerp (`f26*(f28−f29)+f31`), escreve ~14 campos float de estado (10504–10524, 12824–12848) e **não faz nenhuma chamada de desenho** (só `sub_821B5B28`) → candidato a passo de atualização, não viewport. Às 9.84–21.05 chamadas/loop, varia por área (T7_SPEED_15 já registou ~16×/frame com soma de dt >> 1/60 s).
- **Próximo passo (observation-only)**:
  1. Sonda de **valores** em `sub_821B35C8`: contagem/loop + f2/f5/f6 à entrada + soma dos dt por frame vs 1/60 s (não reescrever regs).
  2. Ver consumidores do delta: campo 21792 pós-`sub_821D30E0` (recomp.29:9591) ≈ 841k ticks/frame; f1 calculado em `sub_821D2B80` (recomp.24:9978) = delta×escala(21808)×const **0x820940E0** (confirmar ticks→segundos); getter `sub_821D2478` (recomp.19).
  3. Se `821B35C8` for mesmo render, instrumentar as outras 14 funções `REX_QUERY_TIMEBASE` (`.17:9888 sub_821D34A0`, `.28:10257 sub_821C0E58`, `.34:9061 sub_821C5690`, `.36:31574 sub_82446DA8`, `.5:9892 sub_821D3F50`, `.50:9228 sub_821C62C8`, `.53:9379 sub_821C5060`, `.53:13339/13346 sub_82223E78`, `.55:31249/31355 sub_82447348`, `.6:31720 sub_82446428`, `.60:8959 sub_821C47A8`, `.62:8722/8783 sub_821C0650`, `.8:8849/9002 sub_821C0770`).
- **Status**: ANÁLISE CONCLUÍDA — causa-raiz da aceleração ainda não demonstrada; nenhuma alteração de código feita após o teste.


## T7_SPEED_20 — `sub_821B35C8` = configuração de rect/clip/UV de renderização (DESCARTADA como causa) (2026-10-21)
- **Meta (T7)**: decidir, com evidência, se `sub_821B35C8` altera estado de gameplay, se as chamadas extra por frame são legítimas e se algum argumento/valor calculado explica a velocidade variável por área.
- **Semântica da função (corpo completo, `sonic4_recomp.44.cpp:8952-9301`)**:
  - Entradas: `f1→f31` (origem x), `f2→f30` (origem y), `f5→f29`, `f6→f28`; `fctiwz` de f1..f4 → inteiras; clip: `ex=int(f3)+int(f1) ≤ largura`, `ey=int(f4)+int(f2) ≤ altura` (limites = campos 14/15-bit do descritor `[ptr+36]` ou cache 13356/13360 conforme flags 10940).
  - Escritas: rect A `12824=x, 12828=y, 12832=largura clipped, 12836=altura clipped, 12840=f5, 12844=f6, 12848=0`; chama `sub_821B5B28(this, this+12852)` que interseca rect A com rect B e empacota o clip em `10436/10440` (bitfields 15-bit = coords em pixels); escreve campos UV-like `10504=w·K, 10508=w·K+x, 10512=−h·K, 10516=h·K+y, 10520=f6−f5, 10524=f5` (K = float const em VA `0x8207C954`); seta 6 bits dirty no u64 em +16.
  - **Nenhuma integração temporal**: todas as escritas são sobrescrita (flags = OR idempotente); função puramente determinística de entrada + limites.
- **Escritor único (prova por grep completo, indexado e via `addi`)**: `12824-12848` e `10504-10524` são escritos **apenas** por `sub_821B35C8` em todo o código recompilado.
- **6 call sites — todos com f1..f6 = constantes do pool OU round-trip do próprio rect A** (fechado por indução: únicos estados = constantes transformadas):
  1. main loop `sub_8249E2F8` (`.12:36265`): f1..f6 = cópia de rect A feita no início do frame (round-trip);
  2-4. `sub_821C2D30` (`.17:9302` constantes, `.17:9351` round-trip da cópia salva antes do site 1 = restauro, `.17:9585` constantes) — chamado de `sub_8249CC00` (orquestração de render);
  5. `sub_821C30D0` (`.55:9877`, constantes) — chamado de `sub_8249D308` (loop de passes: N× `sub_821C1C80` de desenho);
  6. `sub_821B3220` tail call `b 0x821b35c8` (`.46:8847`, constantes) — via `sub_821B5D40` (main loop `.12:36190` + `sub_82493DB0`, `sub_821C4BD8`, `sub_821D01C0` ×2, `sub_821CF5C8`, `sub_821CF318`), `sub_821B6090` (mesmos) e `sub_821B6368` (`sub_8248A408`/`sub_8248A510`).
  - Constantes idênticas nos sites 2/3/4/5/6: `f1=f2=A` (VA 0x8207E274), `f3=f4=B` (VA 0x8209431C), `f5=A`, `f6=D` (VA 0x8207D6C0) = rect default (tela inteira). Valores literais não lidos estaticamente (basefile XEX comprimido; leitura em runtime seria opcional e não bloqueante).
- **LRs do probe reconciliados**: `821C2ECC/821C2F18/821C30B0` = 3 sites de `sub_821C2D30`; `821C3550` = site de `sub_821C30D0`; `821B5EDC/821B6888/821B60C8` = callers de `sub_821B3220` (o tail call `b` preserva o LR); main loop = retorno direto. Nenhum call site oculto.
- **Consumidores dos campos escritos (todos de renderização)**:
  - rect A: `sub_821B5B28` (interseção de clip), `sub_821B2920` (via `sub_821B2FD0`, chamado por `sub_821C2D30`), round-trips;
  - clip `10436/10440`: `sub_821C1C80` (de desenho), `sub_821B30F8`, `sub_821B8EB0`, `sub_821C40C8`, `sub_821B2920`;
  - `10504-10524`: apenas `sub_8218CBD8` (mesma classe — `r28=r3` com offsets 10440/10504; só helpers imediatos). Os "leitores" em `sub_820CB028` (`.15:727`) e `sub_821525D8` (`.3:5293`) são **falsos positivos**: `lis+addi` constroem endereços globais (0x8209290C / 0x82152800-0x82152908), não campos do objeto.
- **Chamadas por loop (log T7_SPEED_19)**: 9.84-21.05/loop variando por área, em conjunto com `821C1C80` (3.9-12×/loop) e `8249CC00/8249D308` (~3-5×/loop) → cada chamada = um passo de configuração de rect de superfície/passe de render; a variação = nº de passes/superfícies da área (complexidade de render), não passos de simulação.
- **Predição validada pelo log existente**: f1 ≡ 0 em 100% das 23 janelas (const A=0 ou round-trip de 0) — coerente com entradas constantes; f2/f5/f6 derivam das mesmas constantes (conjunto fechado, sem canal externo).
- **Instrumentação nova**: **não necessária** — a prova estática fecha as entradas (escritor único + constantes/round-trip) e o log T7_SPEED_19 já responde contagens por área e f1; sem novo teste (regra: o log existente responde).
- **Decisão (§4)**: **descartar `sub_821B35C8` como causa da aceleração**. É configuração de rect/clip/UV de renderização; as chamadas extra por frame são legítimas (passes/superfícies) e não avançam gameplay (sem dt, sem acumulação, entradas constantes). Refuta a hipótese "física" de T7_SPEED_09/10/15 (f1 nunca foi dt: era origem x = 0).
- **Próximo caminho de atualização efetiva do gameplay**: consumidores de tempo restantes — árvore de chamadas de `sub_821C0E58` (update de frame), getter `sub_821D2478` → `sub_821D2B80` (medido 0.25-0.61/s), e a lista de 14 funções `REX_QUERY_TIMEBASE` do T7_SPEED_19 (item 3).
- **Status**: ANÁLISE CONCLUÍDA — nenhuma alteração de código; causa-raiz da aceleração continua não localizada.

## T7_SPEED_16
- **Feedback (T15)**: Tela branca de novo (F4 232 FPS); log mostrava budget com avg f1 baixo.
- **Causa**: budget **igualava f1–f6** (f30≠f31) e **zerava** extras; cap em `8249CC00`/`8249D308` no dispatch.
- **Alteração**: budget **escala proporcional**; extras só clamp; removidos hooks `8249CC00`/`8249D308`; mantém cap só em `821C1C80`.

## T7_SPEED_15
- **Feedback (T14)**: Jogável de novo, mas **rápido variável** (normal → acelera por área).
- **Causa**: ~**16× `821B35C8`/frame** com dt pequeno → soma >> 1/60 s de sim por frame de parede; picos de f1 no dispatch/render.
- **Alteração**: `guest_physics_frame_dt_budget=true` (teto ~1 paced frame de dt/física por loop); `GuestCapRunawayFrameDelta` em `8249CC00`/`8249D308`/`821C1C80`; hook física com budget (sem replace render).
- **F4**: manter `guest_use_paced_render_dt=false`.
- **Status**: BUILD — teste + log `timing_log_interval_sec=5`.

## T7_SPEED_14
- **Feedback (T13+)**: Splash rápido, menu injogável, render quebrado.
- **Causa**: hooks que **substituíam f1** em `8249CC00` / `8249D308` / `821C1C80` / `821B35C8` + `guest_use_paced_render_dt=true` (F4) desync sim vs GPU.
- **Alteração**: pilot só `PaceGuestMainLoopFrame` **antes** de `8249E2F8`; `ResetGuestClockHostBaseline()` após sleep; default `guest_use_paced_render_dt=false`.
- **F4**: desligar `guest_use_paced_render_dt` e `guest_use_paced_frame_dt` se ainda ligados; salvar `sonic4.toml`.

## T7_SPEED_13
- **Feedback (T7_SPEED_12)**: Mesmo sintoma T11; print Clock OK; log `avg f1=0.00000` (passos extras zerados) + splash usa **globals guest** `f31` (+24 / -7564), não só hooks.
- **Alteração**: hook `82489F48` publica dt paced nos VAs `0x822D56F0`, `0x822D6274`; física ignora sanitize quando f1–f6 ≈ 0.
- **Status**: BUILD — teste manual

## T7_SPEED_12
- **Feedback usuário (T7_SPEED_11)**: Splash SEGA azul **muito rápido**, tela branca, não chega ao title.
- **Evidência log T7_SPEED_11**: main loop ~59 Hz OK, mas **~941 physics steps/s** (+4425 extras) com `avg f1≈0.001` → `guest_use_paced_frame_dt` zerava dt nos extras; **821C1C80** (render/splash) **sem hook**.
- **Alteração**: pacing **após** `8249E2F8`; default `guest_use_paced_frame_dt=false`; extras com clamp (não zero); `guest_use_paced_render_dt=true` + hook **821C1C80** (`GuestSanitizePrimaryFrameDelta`).
- **Status**: BUILD OK 22:49 — AGUARDANDO TESTE MANUAL

## T7_SPEED_11
- **Log T7_SPEED_10**: só `Guest frame pacing ~60/s`; **sem** `Physics steps` → SDK (`rexruntime.dll`) provavelmente não reinstalado após T7_SPEED_10.
- **Refs**: LARecomp (real frame delta); ReXGlue 0.7.4 (vblank pacing); Sonic 4 retail/Xenia (60 Hz); SVR07-Recomp (desync sim vs render).
- **Hipótese**: pacing no **fim** do loop + múltiplos `sub_821B35C8`/timers intra-frame com deltas QPC curtos em áreas leves.
- **Alteração**: pacing no **início** de `8249E2F8`; `guest_use_paced_frame_dt=true`; `guest_physics_extra_step_scale=0`; log de passos extras.
- **Status**: AGUARDANDO TESTE MANUAL

## T7_SPEED_21 (analise estatica + sondas observation-only)
- **Objetivo**: localizar como o estado de gameplay avanca e a sua fonte temporal.
- **Metodo**: leitura estatica completa do frame tree + 3 build/run observation-only (menu/attract).

### Fatos confirmados (runtime)
- **Frame update `sub_821C0E58` corre 1x/frame pelo caminho B** (`mainB ~59.5/s`), caminho A 0/s, **worker thread 0/s**, gate `world+10942 & 2` nunca setado -> **dupla-atualizacao refutada**.
- **Callback `world+13644` = 0 em runtime** (todos os janelas), `cb_nonzero=0` -> caminho de delta-segundos (`sub_821C47A8`) **morto**.
- **`POLLINIT`**: `world=40095380 cb=00000000 rate21804=5e+07 scale21808=2e-08 ticks2s_820940E0=1000` -> a formula produz milissegundos, mas ninguem consome.
- **Poll (`sub_821C47A8`)**: `mode3` 100%, avg 60k-230k ticks/frame (1.2-4.6 ms), max ate 2.7M (54 ms) -> e backpressure/sync, nao stepper.
- **Clock 0.9997-1.0002x 50 MHz** (estavel) -> fonte temporal global correta.
- **`STATE[82585E40] = 3` mesmo no menu/attract** -> o gate state==3 do master loop (`sub_821647A0`) esta ATIVO sempre; logo nao distingue gameplay.
- **`sub_82197F28` ~510/s** = 2x/frame do master loop (LR `821648A4`) + ~6.5x/frame via handler dispatch (LR `821A8544`).

### Cadeia do handler de "scene" (nova)
- `sub_8249A950(key,arg)` (23 call sites) = **dispatch por chave** numa tabela de 20 bytes/entrada; `bctrl` no handler.
- Handler: `sub_8249CF80(entry)` (`r31=[entry+8]`), ~392/s, LR unico `8249A9DC` (loop dentro de `sub_8249A950`).
- `sub_8249CF80` -> `sub_821A8488(obj->[8], obj->[28], 0,0)` -> `sub_82197F28`.
- `sub_821A8488` = dispatcher magro: `[[0x825F9A10]+0?]` ... seta modo global `0x82582688`, `obj+16 |= 64`, chama `sub_82197F28`.
- `sub_82197F28` (0x8219F/0x821AA subtree):
  - `sub_8219F228`/`sub_8219F0F8` = gestor de slots/bitmask de modulos (indices <16, record 96B) - camada de modulos/visibilidade;
  - `sub_821AA1C0`/`sub_821A9CC8` = **programacao de registradores GPU Xenos** (`obj+16 |= 512|262144|134217728`, `stfs 10500 = f(const)`), estados 770/771;
  - `sub_821AA9E0`/`sub_821AAC98` = emit/present.
- **Conclusao**: a "scene" ~510/s e **camada de apresentacao/render-state**, nao o passo de simulacao.

### Conclusao T7_SPEED_21
- Todo o frame tree (master loop + `sub_8249E2F8` + `sub_821C0E58` + dispatch 0x8249A9xx) termina em GPU/render/sync/telemetria; **nenhum stepper de gameplay** encontrado nesses caminhos.
- Fonte temporal global (clock 50 MHz) e pacing (59.5 Hz) estao corretos -> a aceleracao 1.2-1.35x variavel **nao vem de dt/clock nem de dupla-atualizacao**.
- **Proximo**: procurar o consumidor real de tempo do gameplay FORA do frame tree (provavelmente por modulo de jogo com contador proprio, ex. `sub_821C0770` pacing alvo fps `world+21772`), ou instrumentar dados de gameplay diretamente.

### Artefatos
- Probes: `pilot/src/sonic4_timing_hooks.cpp` (FRAMEUPD/Poll/SCENEUPD/SCENEDISP/OBJUPD, LR-classified).
- Logs: `logs/t7s21_probe.log`, `logs/t7s21_probe2.log`, `logs/t7s21_probe3.log`.

### T7_SPEED_21b - sessao de gameplay real (log t7s21_gameplay.log)
- **Master loop `sub_8249E2F8`: 59.6-60.0/s em TODAS as janelas** - pacing correto e constante.
- **`sub_8249CF80` (per-object scene update, LR unico `8249A9DC` = loop de dispatch de chave em `sub_8249A950`) escala com o conteudo:**
  | janela | OBJUPD/s | ~objs/frame |
  |---|---|---|
  | boot/menu | 9-393 | 0-6 |
  | menu carregado | ~707-790 | ~12 |
  | gameplay area 1 | 1383-2025 | 23-34 |
  | area 2 estavel | ~706 | ~12 |
  | area 3 | 1052-1553 | 18-26 |
- **`sub_82197F28` (scene view/GPU-state, LR `821648A4` master-loop 2x/frame + `821A8544` dispatch): 826-2145/s, tambem escala com a area.**
- **`821B35C8` (view/render): 896-1253/s (~15-21/frame), varia por area; `f1` SEMPRE 0.0.**
- **`821C1C80`: 478-716/s; `f1` SEMPRE exatamente 1.0** (nao e dt).
- **`sub_821C0E58` FRAMEUPD dur_us: 20 us (leve) ate 11-15.6 ms (pesado)** - o frame update serializa no busy-poll `sub_821C4870` e o custo cresce com o conteudo:
  - leve: POLL/s ~7-13, avg_ticks ~20k-58k (~0.4-1.2 ms), dur avg 20-514 us
  - pesado: POLL/s ~55-60 (toda frame), avg_ticks ~250k-553k (5-11 ms), dur avg 5.5-11 ms, max 19.8 ms
- **Poll delta (ticks) e o tempo de espera/backpressure, nao dt de simulacao**; `mode3` sempre, `cb_nonzero=0` (callback `world+13644` nulo).
- **Clock 0.9967-0.9997x 50 MHz** (variacao <0.4%, sem relacao com area).

#### Conclusao CORRIGIDA (reanalise com alinhamento por timestamp)
- **A versao anterior desta entrada (hipotese "conteudo de area -> handler por-invocacao") foi REFUTADA pelos proprios dados.** Fica registado o erro para nao repetir.
- Series pareadas reais (mesma janela de 5s) mostram que **OBJUPD/s e dur_us sao INDEPENDENTES**:
  - 21:57:28 OBJUPD=706 -> dur=**7063 us**; 21:58:23 OBJUPD=734 -> dur=**19.9 us**. Mesmo nº de objetos, ~350x diferenca de duracao.
  - OBJUPD=706 abrange dur de 19.9 us ate 10283 us -> contagem de objetos NAO determina a carga.
- **Master loop `sub_8249E2F8` e PACE `sub_821C0770`: fixos em 59.4-60.2/s em TODAS as janelas** (nunca oscila com area nem com OBJUPD).
- **`POLL/s` e bimodal e independente da carga**: alterna ~0-13/s vs ~50-60/s com OBJUPD constante (ex. 21:58:23 poll=0.6 -> 21:58:43 poll=60.0, obj 734->1052).
- **OBJUPD/s fica em PLATEAUS discretos por varios segundos** (9, 191, 393, 706, 1436, 2026), mudando em degraus -> parece transicao de estado/cena, nao escalonamento continuo de conteudo.
- **Evidencia comportamental do utilizador (decisiva)**: parado na MESMA area, a velocidade estava normal e "do nada acelerou". => o gatilho **nao e a area nem o conteudo**; e **temporal/estado**, sem deslocamento.
- **O que este log NAO prova**: nao localiza a origem da aceleracao; nao suporta "dupla atualizacao", "dt errado" nem "handler por-invocacao".

#### Hipotese atual (nao confirmada, a testar)
- Como o gatilho ocorre **parado na mesma area**, a causa mais provavel e um **estado/caminho que muda apos um tempo** (ex. timeout de poll, transicao de fase, flag de sincronizacao que passa a deixar o passo correr mais vezes). O "poll bimodal" (`sub_821C4870`: ou dorme ou faz spin) e um candidato forte a correlacionar com o instante da aceleracao.
- **Proximo passo**: capturar o INSTANTE da aceleracao (nao estado estacionario) e procurar o flip discreto no log nesse instante - comparar segundos antes/depois do momento em que o utilizador viu acelerar. Requer sessao com marcador temporal ("acelerou agora").

### Artefatos T7_SPEED_21
- Probes: `pilot/src/sonic4_timing_hooks.cpp` (FRAMEUPD/Poll/SCENEUPD/SCENEDISP/OBJUPD/PACE, classificacao por LR).
- Logs: `logs/t7s21_probe.log` (menu), `probe2/3/4.log`, **`logs/t7s21_gameplay.log`** (sessao real, melhor evidencia).

### T7_SPEED_23 - eliminacao definitiva da hipotese temporal (logs t7s22/t7s23)
- **Sessao com o utilizador a confirmar "ACELERADO AGORA"** (t7s22_accel_reference.log): em estado acelerado confirmado, TODAS as camadas temporais estao nominais:
  - `GUESTCLK`: near=300/300, jump=0, max_delta~1.0M (~1.2x de 833k, mas sem acumular)
  - `CLOCKPROBE`: 0.998-1.000x de 50 MHz
  - `8249E2F8` (master loop): 59.9-60.0/s
  - `PACE_821C0770`: 59.9/s
  - `FRAMEUPD`: mainB=59.9/s, dur_us avg ~1 ms (estado leve)
- **t7s23 (frame scheduler instrumentado, world=0x40095380):**
  - `sub_821C0770` resolve world por **dupla indireccao**: `w = mem32[mem32[0x820004F8]]` (corrigido do probe anterior que usava indireccao simples -> lia zeros).
  - `f16760` (contagem-alvo) avanca **exactamente +300/5s = 60.0/s em TODAS as janelas**, incluindo o instante em que o utilizador disse "agora" (acelerado). `queue_jumps=0` sempre.
  - `f16768=f16760`; `f16776/f16784` = consumidos, backlog `f16760-f16784` ~13-15 estavel.
  - `max_delta_ticks` ~270k-825k (0.3-1.0 frame).
  - `target_fps` passou de 0 (menu) para 60 (gameplay).
- **Conclusao (definitiva): a aceleracao NAO e temporal.** Com clock, master loop, frame scheduler, pacing e poll todos provadamente correctos e iguais nos dois estados (normal vs acelerado), nao existe fonte de dt/contagem de passos errada.
- **Direccao restante**: a causa tem de estar nos VALORES que o gameplay usa (constante de velocidade/fisica/refresh lida do estado), ou num modo/estado proprio do jogo (ex. estado tipo "boost", replay/demo, timer interno) que muda a *velocidade percebida* sem alterar o numero de passos.
- **Proximo medidor decisivo**: posicao do player por frame (delta) - se a posicao avanca mais por frame no estado acelerado com o mesmo input, a constante de velocidade esta errada.

### Artefatos T7_SPEED_23
- `logs/t7s22_clockjump.log`, `logs/t7s22_accel_reference.log` (estado acelerado confirmado, clock perfeito).
- `logs/t7s23_sched.log` (probe com indireccao errada -> zeros), `logs/t7s23_sched2.log` (corrigido, scheduler 60/s).
- Probes: `pilot/src/sonic4_timing_hooks.cpp` (GUESTCLK jump detector + SCHED_821C0770).

### T7_SPEED_24 - primitivas de tempo do engine (dt query) tambem nominais
- **`sub_821D0020`** = `((mftb>>10) - world[21892])` = query de "ms desde a ultima frame" (servico do engine, chamado por ponteiro). Instrumentado: **NUNCA e chamado em gameplay normal** (`DT_821D0020/s=0`, Count=0 na corrida `t7s24_dt.log`). Ou seja, o gameplay NAO consome dt por esta via.
- **`sub_821D1DE0`** = compara `world[16776]` vs `world[21876]` (query de sync de frames), tambem servico do engine.
- **`sub_82223E78`** = primitiva "ler timebase para u64 out-param" (tipo QueryPerformanceCounter), nao um formatador.
- **Estrutura do master loop `sub_8249E2F8`** (recomp.12:36111-36360), confirmada por leitura:
  - gate `[_wrld.voice.822738]*` ... chama `sub_821B5D40`, `sub_821C0920`, **`sub_821C1C80(f1 = [r3+24], constante da tabela)`** (submit de cena com alpha/interp constante, `f1` fixo), `sub_821C61C8`, `sub_821B35C8(f1..f6 de r31[12852..12864])`, `sub_821B5B28`, **`sub_821C0E58` (scene/game update)**, `sub_82493890`.
  - **`sub_821C0E58` e chamado exactamente UMA vez por iteracao do master loop** (linhas 36279 e 36308 = dois branches do mesmo ponto). Logo 1 frame logico = 1 update de cena = 60/s. Confirmado por leitura estatica + runtime.
- **Corrida `t7s24_dt.log`** (PID 13112, gameplay): tudo igual a `t7s22/t7s23` � FRAMEUPD/B mainB=60.0/s, POLL=60.0/s, PACE=60.0/s, `f16760` +300/5s, `queue_jumps=0`, `max_delta_ticks` 270k-724k.
- **Conclusao acumulada**: TODAS as primitivas temporais do engine (clock guest, mftb, frame counters, scheduler, pacing, dt-query) estao corretas e identicas normal/acelerado. O dt-query do gameplay nem e chamado. A aceleracao nao vem de nenhuma delas.
- **Direccao restante (unica)**: o bloco de floats `f1..f6` passado a ramo de cena (`sub_821B35C8`) e/ou as constantes de escala de velocidade. Proximo passo: registar os 6 floats vivos por janela e comparar normal vs acelerado; e medir delta de posicao real do player.

### Artefatos T7_SPEED_24
- `logs/t7s24_dt.log` (probe DT_821D0020; confirmacao de que nao e chamado em gameplay).

### T7_SPEED_25 - pipeline de dt do engine mapeado e tambem inactivo em gameplay
- **Encontrado o servico de tempo do engine `sub_821D2478`** (recomp.19:9590), um switch 0..19 que converte o delta de ticks por frame em varias unidades:
  - case 19: `f1 = (float)world[21792] * world[21808]` = **segundos** (`21808=2e-8=1/50e6`).
  - case ~6: `f1 = world[21804] / (float)world[21792]` = rate (5e7/delta) = fps derivado.
  - case loc_821D2538: ratio `world[21832]/world[21792] * const` **clampado** entre dois consts -> escala limitada.
- **Fonte do delta `world[21792]`**: `sub_821D30E0` (recomp.29:9525) faz `world[21792] = mftb_now - world[21800/21796]` e roda a historia de timestamps (`21832<-21816`, `21800/21796<-now`). Chamado de `sub_821C0E58` (frame update) recomp.28:10301.
- **Medido**: `TIMERS scene_821D30E0 = 59.8-59.9/s` -> o delta e recalculado **exactamente uma vez por frame**. Logo `world[21792]` ~ 833k ticks = 1/60 s. dt correcto.
- **Dispatcher de subscritores** (recomp.24:9960): loop de 17 iteracoes sobre bitmask `world[25892]`, chamando `sub_821D2478(world, case)` por bit activo. **Medido em runtime: `sub_821D2478` NUNCA e chamado em gameplay** (bitmask 0) -> este servico de tempo esta **desligado** durante o jogo. Nao e a fonte do dt do gameplay.
- **Conclusao**: o pipeline de dt do engine (`mftb -> 21792 -> x2e-8 = s`) e correcto e re-calculado 1x/frame; e o servico que o expoe aos subsistemas nem e usado em gameplay. Mais uma eliminacao.
- **Proximo**: medir a POSICAO real do player por frame (ground truth de "move-se mais depressa"). objectos vem de `sub_8249CF80` via `obj=[r3+8]`, `state=[obj+8]==4`.

### Artefatos T7_SPEED_25
- `logs/t7s25_timesvc.log` (probe TIMESVC_821D2478 + world[21792]; servico nao chamado em gameplay).

### T7_SPEED_26 - mapa completo do loop; nada de gameplay fora do frame de 60/s
- **`sub_8249CF80` NAO e update de objectos de gameplay** - e um **alocador bump** (pool em `world[17416]/[17512]`, copia blocos de 16 bytes). O "state==4" e estado de pool de memoria. Logo `OBJUPD_8249CF80/s` (ate 2690/s) mede alocacoes, nao logica.
- **`sub_82197F28` (SCENEUPD) e `sub_821A8488` (SCENEDISP)** escrevem **registos GPU Xenos** (`world_gpu[10568]`, `sub_821A9CC8`, `sub_821A9D68`) - sao **setup de estado de render por draw-call** (420-630/s). Nao sao simulacao.
- **`sub_821B30F8`** (fim do frame update) constroi **pacotes de comando GPU** (headers `0xFFFF8000|...` no ring buffer) a partir do rect clipado `world[10436/10440]` - **submissao de render/present**.
- **`sub_821C0E58` (frame update)** = poll queue (`sub_821C4870`) ate backlog `(world[16776]-world[16784]) < 15`, processa work items (`sub_821C47A8`, callback `world[13644]=NULL`), recalcula `world[21792]=mftb-21800` (`sub_821D30E0`), depois `sub_821B30F8` (packets GPU).
- **`PaceGuestMainLoopFrame`** (host): faz sleep para `host_freq/refresh_hz` e **reseta o baseline do clock guest** por frame. So CAPA o loop a 60/s; nunca acelera.
- **Conclusao consolidada**: o unico caminho de simulacao e o master loop `sub_8249E2F8` (60.0/s), que chama `sub_821C0E58` 1x/frame; o dt `world[21792]` e recalculado 1x/frame (60/s) do mftb real e escalado por 2e-8 (1/60 s). Tudo o resto medido e render/GPU/alocador. **Nao existe bug temporal demonstravel.**

### Lead restante (T7_SPEED_26)
- **Backlog do frame update**: `world[16776]-world[16784]` cresce 5->15 ao longo de uma sessao (observado em t7s23). O frame update **drena a queue ate backlog<15** (recomp.28:10276). Se em certos estados o backlog cresce, o jogo processa MAIS work items por frame; se algum work item for um sub-passo de simulacao, isso e "passo extra". Proximo passo: correlacionar backlog e itens-drenados por frame com o instante da aceleracao; e identificar o que o `sub_821C4870`/`sub_821C47A8` drenam (que tipo de trabalho).

### Artefatos T7_SPEED_26
- `logs/t7s26_objdiff.log`, `logs/t7s26b_objtrace.log` (dif/trace de objecto -> revelaram alocador/heap reusado).

### T7_SPEED_27 - WORLDLATCH (latch do estado por janela)
- Prova `WORLDLATCH` faz snapshot de 8192 words de `world=0x40095380`, reporta offsets que estavam *estaveis* na janela anterior e mudaram (latch) + churn.
- Smoke test OK (`logs/t7s27_latch_smoke.log`): ~3-5 latches/janela, churn ~350/8192, sem crash.
- Corrida A/B em `logs/t7s28_latch_AB.log` (marcadores "normal agora"/"acelerado agora" nao ficam no ficheiro; segmentacao por timestamps da conversa).
- **Estruturas identificadas com confirmacao de codigo (nao por aparencia do float)**:
  - `0x8B0..0x96C` + `0xB80..0xC90`: **array de 4 registos, stride 0x40** (campos repetidos em 0x8BC/0x8FC/0x93C, 0x8C0/0x900/0x940, ...). Valores tipo `3.19x`, `3.07x` ~ **descem com o tempo** (contadores/distancia), NAO sao multiplicadores.
  - `0x2A1C`, `0x2AE8`, `0x2AF4/0x2AF8`, `0x2B04`, `0x2B68..0x2BBC`, `0x2C14..0x2C28`: **regiao do ring buffer de comandos GPU** (confirmado: `world[11012]=0x2B04` e escrito com `KfAcquireSpinLock` + headers `0xFFFF8000|` + cursor `+= size`, recomp.43:9581-9665). NAO e estado de gameplay.
  - `+0x1C` muda para `0xC0000000` (=-2.0) por area -> parametro continuo, nao flag.
- **Conclusao**: o latch do WORLDLATCH nao isolou um flag booleano de modo. A maior parte do churn e ring buffer/allocador/estado de render.

### T7_SPEED_28 - vblank, scheduler do guest e cadeia dt re-verificados a fundo (todos correctos)
- **Handler de interrupcao GPU do guest**: `SetInterruptCallback(821C65E0, 40095380)` no log -> confirma `world = 0x40095380` e o handler `sub_821C65E0` (recomp.4:9291).
- **`sub_821C0650`** (recomp.62:8692) = handler de vblank: `world[16760]++` sob spinlock, `world[16764]=mftb`, drena fila de callbacks agendados (`world[16916]/[16920]`, entradas de 8 bytes em `16788`), chama callback `world[16756]`.
- **`sub_821C0770`** (recomp.8:8780) = **scheduler frame-rate-independent do guest**: le a taxa-alvo `world[21772]` (default 60 se 0), calcula `r7 = (mftb - world[16764])*100/(KeQueryPerformanceFrequency()/target)` (frames decorridos em %), clamp 100; agenda/executa. **Bug de sonda (nota)**: o probe usava `c16760` (contador de frames) como "target" -> `frames_gt1` sempre verdadeiro (artefacto). Target real = `world[21772]` = 60 em gameplay.
- **Vblank do SDK** (`graphics_system.cpp:149-176`): thread `GPU VSync` dispara `MarkVblank()` a `VdQueryVideoMode().refresh_rate` Hz. `video_mode_refresh_rate` cvar default **60.0** (`ui/window.cpp:63`). Logo vblank = 60/s. `MarkVblank` -> `DispatchInterruptCallback(0,2)` -> handler guest -> `sub_821C0650`.
- **`KeQueryPerformanceFrequency` devolve 50 MHz; `mftb` = `QueryGuestTickCount()`** (REX_QUERY_TIMEBASE). `KeQuerySystemTime` = guest ticks x 100ns/50MHz. Tudo coerente.
- **Medicoes da corrida t7s28 (PID 16348, ~23:00-23:19)**:
  - `mainB/FU = 59.8-59.9/s`; `PACE/s=59.5-59.8 target_fps=60`; `world[16760]` +299/5s = 59.8/s; `world[16764]` (mftb do ultimo vblank) avanca 249.1M/250M ticks por 5 s = **0.997x** (guest clock correcto).
  - `max_delta_ticks` <= ~823k < 833333 (1 vblank a 60 Hz) -> **nunca passa de 1 intervalo de vblank entre chamadas**.
  - `queue_jumps=0` -> `world[16784]` nunca salta > 1 -> **nao ha passos extra de scheduler**.
  - Backlog `f16776 - f16784 = 1`.
  - `GUESTCLK ... below=0 near=300 high=0 jump=0` -> guest clock nominal.
- **`PaceGuestMainLoopFrame`** (SDK, unica intervencao activa; `guest_frame_pacing=true` default): faz sleep ate `host_freq/refresh_hz` (target 60), guarda delta paced, `ResetGuestClockHostBaseline()`. So **capa** a 60/s, nunca acelera; o reset do baseline introduz no maximo um ligeiro *atraso* (guest clock medido 0.993-1.0x).
- **Outras intervencoes do SDK estao INACTIVAS**: `GuestSanitizePhysicsStepInputs`, `GuestSanitizePrimaryFrameDelta`, `GuestCapRunawayFrameDelta`, `GuestPublishPacedFrameDeltaTo`, `GuestWriteFrameDeltaToGuestVAs` **nao sao chamadas em lado nenhum**; cvars `guest_use_fixed_physics_dt/guest_use_paced_frame_dt/guest_physics_frame_dt_budget/guest_clamp_runaway_frame_dt = false`, `guest_physics_dt_scale=1.0`.
- **`Host Present ~239/s (no vsync)`** = swapchain do host (monitor 180-240 Hz), desacoplado do loop guest (60/s). Nao e o jogo.
- **Conclusao (T7_SPEED_28)**: a cadeia completa vblank -> contador de frames -> scheduler -> dt -> clock esta **correcta a 60/s** e identica ao nominal. Nao existe caminho temporal que produza aceleracao uniforme. Resta: (a) valor/constante/estado de gameplay; (b) segundo relogio lido pelo gameplay. **Proxima medicao decisiva: valor proprio do guest `r7`/frames-avancados por chamada (world[21772] como target) + posicao real do player por frame.**

### Artefatos T7_SPEED_28
- `logs/t7s28_latch_AB.log` (47+ janelas WORLDLATCH ate 23:19; timing nominal confirmado; vblank 60/s; scheduler sem passos extra).
- Factos de codigo: `graphics_system.cpp:149-176`, `xboxkrnl_video.cpp:99-135/380-451`, `xboxkrnl_threading.cpp:371-402`, `core/clock.cpp:46-170`, recomp.4:9291, recomp.8:8780, recomp.62:8692, recomp.28:10301.

### T7_SPEED_29 - sonda MOVERATE (posicao por frame) + correccoes de sondas
- **Objectivo**: obter ground truth da velocidade real do jogo medindo |delta(posicao)| de um objecto por frame, comparando segmentos "normal" vs "acelerado".
- **Correccao critica de rotulagem das sondas (integridade de evidencia)**:
  - `g_viewport`/`PROBE ratio viewport/main` e incrementado no hook de **`sub_821B35C8`**, NAO de `sub_821C1C80` (ver `sonic4_timing_hooks.cpp:584-588`). As notas anteriores que atribuiam "15x" a `sub_821C1C80` estao erradas. `sub_821C1C80` tem contador separado (`g_scene`).
  - `sub_821B35C8` e chamado ~9.8-21x por iteracao do loop principal, **variando por area**; o hook directo no loop mestre chama-o **1x** (`recomp.12:36263`), o resto vem de dentro das sub-chamadas de render (`sub_821B5D40`/`sub_821C0920`/`sub_821C1C80`/`sub_821C61C8`/`sub_821C0E58`). Confirmado que `sub_821B35C8` le/escreve estado de render ("window[12616]/[12632]/[12904]/+10940"), nao fisica de gameplay.
- **Bug da propria sonda MOVERATE (negativo honesto)**: a sonda assumia `+16`/`+24` do objecto latcheado como vec3 de posicao. O dump completo do objecto (`OBJDIFF obj=40100660`, 128 slots) mostra que `+16=0x401006B0` e `+20=0x401005E0` sao **ponteiros para objectos irmaos** (reinterpretados como floats 2.2504/2.25036), ie **`next`/`prev` de lista ligada**, NAO posicoes. Logo `units_per_sec=477` e `max_step=1152` medidos sao **ruido de ponteiros**, nao velocidade. Licao reiterada: **nao assumir significado de offsets a partir da aparencia do float**.
- **O que a MOVERATE provou, apesar do bug**: a sonda emite 1 amostra por frame (`frames=297-301` em janelas de ~5.0 s = 60.0/s) e o objecto latcheado persiste durante a corrida -> o **mecanismo de amostragem por frame funciona**; so o campo escolhido e que estava errado.
- **Timing reconfirmado nesta corrida** (`logs/t7s29_moverate.log`, PID 13032/6800): `SCHED f16760` avanca **exactamente +300 por 5 s = 60.0/s** em todas as 21 janelas; `f16768` acompanha; `queue_jumps=0` (sem passos extra); `PACE/s=59.0-60.0 target_fps=60`; `GUESTCLK below=0 high=0 jump=0` na maioria. `max_delta_ticks` bimodal (~230k-480k vs ~800k) reflecte apenas variacao do instante de amostragem do loop mestre dentro do intervalo de vblank, nao aceleracao.
- **Objecto `0x40100660`**: layout com `+36=62.37`, `+52=0x302`, `+56=0x303`, `+60=0x8006`, `+224/228/232 = 0.7/0.95/1.0`, `+244=500.0` -> descriptor/flags de render/GPU (coerente com a classificacao de ring buffer do T7_SPEED_27), **nao** o player. Reforca: o objecto latcheado pelo hook `sub_8249CF80` nao e o player.
- **Conclusao T7_SPEED_29**: motor continua provadamente a 60.0/s uniforme. A sonda de posicao precisa de (a) identificar um campo de posicao **verificado** (nao por aparencia) e (b) um objecto **estavel** (o hook de objectos latcheia descritores, nao entidades de gameplay). Nenhuma conclusao de velocidade do gameplay pode ser tirada desta corrida.

### Artefatos T7_SPEED_29
- `logs/t7s29_moverate.log` (MOVERATE por frame + correccao de campos; timing 60.0/s; objecto latcheado = descriptor GPU).
- `pilot/src/sonic4_timing_hooks.cpp`: nova `MeterTrackedObject(base)` chamada 1x/frame em `sub_8249E2F8`; `g_move` (MoveRate); rotulos `g_viewport`=821B35C8 / `g_scene`=821C1C80.

### T7_SPEED_30 - AUDIO: causa-raiz encontrada (volume por-aplicacao do Windows)
- **Sintoma**: audio completamente mudo no recomp (pedido explicito do utilizador).
- **Diagnostico por camadas (evidence-first)**, todas medidas em runtime:
  1. **Cliente audio registado** 1x (`reg_total=1`, caller LR `82465788`, via `sub_82449D50`). OK.
  2. **Submissao activa**: `sub_82449958` (loop mixer) = ~188 submissões/s; `sub_82449710` (passagem de mix) = 188/s. 188 ≈ 48000/256 = 187.5/s -> **geometria e taxa exactas** (`frame_channels_=6`, `channel_samples_=256`, `frame_frequency_=48000`).
  3. **Conteudo do buffer do guest** (ptr `7006E730`, 6144 B): lido logicamente via `REX_LOAD_U32` (que ja faz o byte-swap, ie o que o SDK ve apos `byte_swap`) -> planar, exactamente **canal 0 e 1 (FL/FR) nao nulos**, pico ~0.72. Byte order **BE na memoria crua**, portanto o `byte_swap` do SDK esta **correcto**.
  4. **Replicacao exacta da conversao do SDK** (`sequential_6_BE_to_interleaved_2_LE`, com pesos `center=0.707/surround=0.707/scale=0.586`) feita no piloto sobre esse buffer: `OUT_nz=256/256`, `OUT_peakL`/`OUT_peakR` = **0.18-0.42**. -> o SDK produz **audio estereo limpo, nao-silencioso**.
  5. **Endpoint**: `SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK` resolve para `Speakers (Realtek(R) Audio)` = **exactamente o device predefinido do Windows** (confirmado via `IMMDeviceEnumerator::GetDefaultAudioEndpoint`). Endpoint **activo (state=1), nao muted, volume 0.64**. Sem erros `SDL_*`.
  6. **Sessao de audio por-aplicacao** (Volume Mixer do Windows, `IAudioSessionManager2`/`ISimpleAudioVolume`): `pid(sonic4) vol=0.000 mute=False` -> **o volume por-app estava a ZERO**.
- **Causa-raiz**: **volume por-aplicacao do `sonic4.exe` a 0.000 no Misturador de Volume do Windows** (nao muted, apenas a zero). O Windows multiplicava toda a saida da app por 0. **NAO era bug do recomp.**
- **Verificacao**: ao pôr a sessao a `1.0` (`SetMasterVolume(1.0)`), o utilizador confirmou **"Sim, ouço som agora"**.
- **Contra-prova necessaria (feita)**: a hipotese inicial (fold `= {}` a zerar os pesos em `downmix.cpp`) foi testada e **refutada** — `GetStereoFold()` ANTES de qualquer set devolve ja `center=0.707107 surround=0.707107 lfe=0 scale=0.585786 gain=1` (o `= {}` num struct com NSDMIs **nao** zera; aplica os default member initializers). Logo o workaround `SetStereoFold({})` no `OnPostSetup` e **um no-op** e nao e a correccao.
- **NOTA**: o bloco `REXCVAR_DEFINE_BOOL(audio_mute, false, ...)` e o caminho `if (REXCVAR_GET(audio_mute)) memset(...)` nao estavam activos; `sonic4.toml` nao tem override de audio.
- **Licao**: silencio total com pipeline interno provado correcto -> verificar **camada do SO (volume por-app / endpoint / mute)** antes de suspeitar do codigo. O REA/inspeccao de memoria nao detecta estado do SO.

### T7_SPEED_31 - AUDIO nao acelera (dado decisivo para o bug de velocidade)
- Observacao do utilizador: **"O audio nao esta acelerado, somente o jogo mesmo"**.
- **Implicacao decisiva**: a aceleracao **nao e um time-scale global**. O audio e gerado no mesmo timeline de ticks do guest, e o thread de audio submete a taxa **exacta** correcta (~188/s = 48000/256). Se o problema fosse o clock global / dt / pacing do guest, o audio estaria **tambem** acelerado (pitch/tempo acima). Nao esta.
- **Reforca** os resultados de T7_SPEED_19-28: clock, vblank (60/s), contador de frames, scheduler e dt do guest estao **correctos**. A aceleracao esta **localizada ao subsistema que conduz gameplay/render**, nao ao timeline do guest nem ao thread de audio.

### T7_SPEED_32 - AUDIO: fecho e limpeza
- **Causa confirmada**: volume por-aplicacao do `sonic4.exe` a 0.000 no Windows (corrigido para 1.0; utilizador confirma som). **Nao era bug do recomp.**
- **Contra-prova do workaround**: `GetStereoFold()` antes de qualquer set = `center=0.707107 surround=0.707107 lfe=0 scale=0.585786 gain=1` -> o `= {}` em `downmix.cpp` **nao** zera (aplica os default member initializers). O `SetStereoFold({})` no `OnPostSetup` era **no-op**.
- **Limpeza aplicada**:
  - `pilot/src/sonic4_app.h`: removido o bloco `OnPostSetup` e os includes de audio (voltou ao estado original).
  - `pilot/src/sonic4_timing_hooks.cpp`: removidos os hooks `sub_82449D50`/`sub_82449958`/`sub_82449710`, os contadores `g_audio_*`, os relatorios `PROBE AUDIO`/`PROBE AUDIORAW` e as declaracoes `__imp__` correspondentes.
- **Build limpo** (ninja OK) e corrida `logs/t7s41_clean.log`: endpoint inicializa normal, **0** linhas `PROBE AUDIO`, **0** `AUDIOFIX`, sondas de timing intactas.
- **Regra aprendida**: silencio total com pipeline interno provado correcto -> verificar **camada do SO** (volume por-app no Misturador, endpoint predefinido, mute) antes de suspeitar do codigo; o REA/inspeccao de memoria nao ve estado do SO.
