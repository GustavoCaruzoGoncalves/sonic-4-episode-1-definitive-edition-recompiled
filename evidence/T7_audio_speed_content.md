# T7 — Testes detalhados: Áudio, Velocidade, Conteúdo

## Ambiente de teste
- Executável: `pilot/out/build/win-amd64-release/sonic4.exe`
- Game data: `game/extracted/`
- GPU plugin: `rexgpu-xenos.dll` (copiado)
- Comandos usados: `.\sonic4.exe --game_data_root="C:\S4EP1Recomp\game\extracted" --log_file="..." --gpu_plugin=xenos`

## Resultados de execução
- `t6_run8.log`: Inicialização OK, áudio endpoint detectado, controladores detectados. Sem crash fatal observado. Duração observada ~>1min antes do último log verificado.

## Observações do usuário
- Jogo roda quase perfeito, mas sem áudio audível
- Velocidade acelerada (sugerido travar em ~60fps para verificar)
- Versão demo - addon `Sonic The Hedgehog 4 Episode I (World) (58410A07) (Addon).zip` contém arquivos em `00009000/` (8 arquivos). Não integrado.

## Investigações necessárias (documentadas)
- **Áudio:** Backend SDL, XMA decoding presente no SDK. Precisa verificar se código jogo chama APIs e se decodificação/streaming funciona.
- **Velocidade:** Verificar sincronização/clock e possivelmente limites de FPS. Não alterar animações arbitrariamente.
- **Addon:** Investigar VFS mounting de pacotes STFS adicionais no SDK; não copiar aleatoriamente.

## Arquivos alterados nesta fase
- Manifesto: `pilot/sonic4_manifest.toml` (boundary hints adicionados iterativamente)
- Build artifacts regenerados conforme necessário
- DLL copiada: `pilot/out/build/win-amd64-release/rexgpu-xenos.dll`
