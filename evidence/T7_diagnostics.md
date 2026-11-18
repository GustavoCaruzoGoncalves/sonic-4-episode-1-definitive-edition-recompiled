# T7 — Diagnóstico: Áudio, Velocidade e Demo vs Full Game

## Resumo do estado (T6)
- Execução estabilizada após adição de boundary hints para chamadas indiretas.
- Runtime inicializa com D3D12 + GPU plugin xenos; shaders traduzidos, pipelines criados.
- Áudio: endpoint detectado e inicializado (SDL/Audio backend). Sem crash.
- Input: controladores detectados.
- Problemas relatados: (1) sem áudio audível, (2) velocidade acelerada, (3) demo vs full game.

## 1) Áudio — diagnóstico

### Evidências
- `evidence/t6_run8.log:20`: `[info] [apu] [t17468] audio endpoint 'Speakers (Realtek(R) Audio)': 2 ch, 48000 Hz, format 0x8120; submitting 2 ch`
- Backend SDL: `src/ui/rex_app.cpp:310` usa `SDLAudioSystem`

### Observações
- Detecção do dispositivo não implica reprodução de áudio do jogo. 
- Xbox 360 usa XMA; SDK possui `xma_decoder.cpp` e `audio_system.cpp`. 
- Pode ser que áudio seja carregado de assets específicos (CPK/CSB) ou que jogo aguarde estado específico (tela de título/nível). Também pode haver silenciamento inicial.

### Próximas investigações necessárias
- Procurar por logs de XMA/decodificação, buffers, reprodução durante execução mais longa.
- Verificar se há chamadas a funções de áudio Xbox que falham silenciosamente.
- Testar com execução prolongada (tela título/jogável) para capturar logs de áudio.

**Status:** PENDENTE (causa não confirmada)

## 2) Velocidade acelerada — diagnóstico

### Observações
- Jogos Xbox 360 rodam a 30/60 fps; se relógio do guest não está sincronizado corretamente com host, pode acelerar.
- SDK usa `chrono::Clock` (kernel timing). Funções KeQuerySystemTime/KeQueryPerformanceFrequency implementadas.
- GPU command processor mede FPS (`graphics/command_processor.cpp`).

### Próximas investigações necessárias
- Examinar implementação de `chrono::Clock` no SDK (guest tick frequency vs host).
- Verificar se há cvars para limitar framerate/vsync.
- Instrumentar timing durante execução.

**Status:** PENDENTE (causa não confirmada)

## 3) Demo vs Full Game — diagnóstico

### Arquivo addon
- ZIP: `Sonic The Hedgehog 4 Episode I (World) (58410A07) (Addon).zip`
- Conteúdo extraído em `game\addon_inspect\58410A07\00009000\` (8 arquivos, tamanhos 81KB-266KB)
- Formato STFS/pacote Xbox 360 (nomes com Title ID). Tipo 00009000 é tipicamente conteúdo adicional (DLC/pacote de dados).

### Observações
- Estes arquivos não foram montados no VFS. O runtime lê de `game_data_root` (`game\extracted\`). 
- Precisamos entender como SDK monta conteúdo extra; não basta copiar arquivos aleatoriamente.
- Não há evidência de DRM/bypass - apenas investigação de montagem.

### Próximas investigações necessárias
- Verificar como VFS do SDK lê múltiplos containers/pacotes.
- Testar se montar addon no path correto altera metadados/carregamento.
- Procurar logs que indiquem trial/demo vs full.

**Status:** PENDENTE (classificação inconclusiva sem evidência)

## Tabela de Status
| Problema | Causa confirmada | Alteração feita | Teste executado | Status |
|---|---|---|---|---|
| Áudio ausente | Não | Nenhuma | Observação de logs | PENDENTE |
| Velocidade acelerada | Não | Nenhuma | Observação visual | PENDENTE |
| Demo vs Full Game | Não | Nenhuma | Inspeção de arquivos | PENDENTE |

## Próximos passos
1. Executar jogo com log mais verboso e tempo de execução maior para capturar eventos de áudio.
2. Investigar cvars/timing do SDK (chrono::Clock, frame limiting).
3. Investigar integração de addon no VFS/runtime (documentado antes de qualquer teste).
