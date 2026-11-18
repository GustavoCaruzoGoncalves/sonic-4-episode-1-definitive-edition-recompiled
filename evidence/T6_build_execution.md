# T6 — Build do piloto Sonic 4 e primeira execução (investigação da tela preta)

## Status
- Codegen confirmado (T5): OK
- Build integrado confirmado: OK (sonic4.exe gerado)
- Runtime iniciado: SIM (com GPU plugin carregado)
- Módulo do jogo carregado: SIM (tabela de funções inicializada)
- Código do jogo efetivamente executado: SIM (avança além da inicialização; shaders carregados, pipelines criados, áudio inicializado, input detectado)
- Inicialização avançada: PARCIAL
- Imagem exibida: PROVÁVEL (jogo em execução, sem crash fatal observado nas últimas execuções estáveis)

## 1) Codegen / Build
- Configuração: `cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH=C:\S4EP1Recomp\third_party\rexglue-sdk\out\install\win-amd64` (ver `evidence\t6_configure.txt`)
- Build: `cmake --build out/build/win-amd64-release` (exit 0, ver `evidence\t6_build.txt`)
- Executável: `pilot\out\build\win-amd64-release\sonic4.exe` (23,558,144 bytes)

## 2) Primeira execução (tela preta) — `evidence\t6_run1.log`
- **Causa:** GPU plugin não carregado (dll ausente ao lado do executável). Chamadas `Vd*` ignoradas → sem rendering da GPU Xenos.

## 3) Correções aplicadas
- Copiado `rexgpu-xenos.dll` do SDK instalado para `pilot\out\build\win-amd64-release\`
- Adicionados boundary hints no manifest `[entrypoint.functions]` para endereços chamados indiretamente: `0x821F42D8`, `0x8218653C`, `0x8216AEA8`, `0x82186514`, `0x821865B0`, `0x82186490`, `0x82149FF8`, `0x8220FD10`, `0x8220FD18` (adicionados iterativamente conforme crashes apareciam nos logs).
- Cada adição forçou regeneração do codegen e rebuild incremental; o build permanece funcional.

## 4) Execuções com GPU plugin
- `t6_run3.log`: Avança até crash por função não registrada `0x8220FD10` (corrigido com boundary)
- `t6_run5.log`: Avança até crash em `0x8216AEA8` (corrigido)
- `t6_run6.log`: Avança até crash em `0x821865B0` (corrigido)
- `t6_run7.log`: Avança até crash em `0x82149FF8` (corrigido)
- `t6_run8.log`: Inicialização completa (GPU D3D12, shaders traduzidos/pipelines criados, interrupt callback registrado, áudio inicializado, controladores detectados). Sem crash fatal até fim do log observado. Jogo roda (com observações abaixo).

## 5) Observações do runtime (run8)
- Shaders traduzidos: 21, pipelines criados: 14
- GPU interrupt callback registrado: `821C65E0`
- Áudio: endpoint detectado e inicializado
- Input: SDL detecta controlador XInput (quando presente)
- Sem FATAL nas últimas linhas observadas - execução estável na inicialização e pós-inicialização.

## 6) Issues reportados pelo usuário (para investigação futura - não bloqueiam T6)
1. **Sem áudio** - áudio foi inicializado no log (endpoint detectado). Pode ser configuração/roteamento ou estado do jogo.
2. **Acelerado (velocidade)** - jogo parece rodar acima da velocidade normal; possivelmente relacionado a timing/limitação de FPS (v-sync/framerate cap). Requer investigação de cvars/config do runtime.
3. **Versão demo vs full** - addon fornecido (`Sonic The Hedgehog 4 Episode I (World) (58410A07) (Addon).zip`) contém arquivos STFS adicionais (pasta `00009000` com vários arquivos). Estes são dados adicionais (content package/title content) - precisam ser colocados no local correto para o runtime ler (VFS/game data). Não foi integrado nesta fase.

## Conclusão T6
- **Compilação integrada confirmada:** SIM
- **Runtime iniciado:** SIM  
- **Módulo carregado:** SIM
- **Código executado:** SIM
- **Inicialização avançada:** PARCIAL (jogo parece rodar)

O objetivo principal da T6 (build + primeira execução com progresso observável) foi atingido. Próximo passo: investigar os itens 1-2-3 acima ou prosseguir para T7 conforme necessário, documentando com evidências.

## Evidências
- `evidence/t6_configure.txt`, `evidence/t6_build.txt`
- `evidence/t6_run1.log`..`evidence/t6_run8.log`
- `pilot/sonic4_manifest.toml` (boundary hints adicionados)
- DLL GPU: `pilot\out\build\win-amd64-release\rexgpu-xenos.dll`
