# T7 - Tentativa de correção de velocidade - T7_SPEED_01

## Hipótese testada
O clock do Xbox 360 (guest) estava sendo inicializado com a frequência do clock da plataforma host (Windows QueryPerformanceFrequency) em vez da frequência padrão do Xbox 360 (50 MHz). Isso pode causar escala incorreta de tempo, resultando em execução acelerada.

## Alteração realizada
- Arquivo: `third_party/rexglue-sdk/src/core/clock.cpp`
- Linha 30: Alterado de `guest_tick_frequency_ = Clock::host_tick_frequency_platform();` para `guest_tick_frequency_ = 50000000ULL; // 50 MHz`

## Justificativa
O Xbox 360 usa 50 MHz como frequência base do contador de performance (498750000 Hz é mais preciso em algumas implementações, mas a abordagem padrão xenia usa 50MHz). Esta alteração corrige a relação de conversão entre ticks de host e ticks de guest.

## Build
- Comando: `cmake --build pilot/out/build/win-amd64-release --config Release`
- Resultado: Build bem-sucedido (codegen up-to-date, linking concluído)
- Executável: `pilot/out/build/win-amd64-release/sonic4.exe`
- DLL necessária: `pilot/out/build/win-amd64-release/rexgpu-xenos.dll`

## Comando para execução (aguardando teste)
```powershell
cd "C:\S4EP1Recomp\pilot\out\build\win-amd64-release"
.\sonic4.exe --game_data_root="C:\S4EP1Recomp\game\extracted" --gpu_plugin=xenos --log_file="C:\S4EP1Recomp\evidence\T7_SPEED_01\t7_speed_01.log"
```

## Status
Build gerado. Aguardando feedback do usuário sobre velocidade do jogo.
