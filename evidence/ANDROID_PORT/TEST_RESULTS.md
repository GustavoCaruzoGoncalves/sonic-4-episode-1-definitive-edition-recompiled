# Resultados no POCO F7

APK instalado: `C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk`. Log: `evidence\ANDROID_PORT\sonic4_003.log`. Capturas: `boot.png` (fase em retrato, antes do override), `landscape.png` (menu), `after_a.png` (fase após o botão A), `after_right.png` (Sonic no ar após o direcional direito).

## O que foi observado

| Verificação | Resultado |
| --- | --- |
| Processo sobe e permanece | Sim. Última activity `Sonic4Activity`, rotação 90. |
| Vulkan | Instância 1.4.0, Adreno 825, API 1.3.284, swapchain 2772×1280, formato 37, present mode 1. |
| Menu | New Game, Help & Options, Leaderboards, Achievements, Exit Game. |
| Retail | `LICENSE flag160=0 (0 = full, 1 = demo)`. Title ID do save `58410A07`. |
| Fase | Splash Hill visível: Sonic, loop, anéis ×002, vidas, placar, inimigo na captura em retrato. |
| Toque | `input tap` no A saiu de New Game e entrou na fase. `input swipe` no direcional direito deixou o Sonic no ar e o placar foi de 0 para 1000. |
| Áudio | Endpoint `System audio playback device`, 6 canais, 48000 Hz. A bandeja do sistema mostrou `His World` como sessão de mídia do aparelho. |
| Ritmo do guest | Sonda intacta, alvo 60. Amostras de `PACE_821C0770`: 49,1, 59,5, 54,5, 51,2, depois 30,2, 27,8, 28,1, 45,2 e em seguida uma faixa de 28–34/s até 21:01:57. |
| Cache | Primeira execução salvou 32 bytes. A seguinte carregou 32. No disco, depois da fase: `58410A07.vkcache` 64 bytes, `.xsh` 17492, `.fbo.vk.xpso` 3114. |
| PSO | Quase todos `worker=1`. Um `worker=0` de 439,8 ms no thread de apresentação. Um present foi pulado por placeholder assíncrono. |
| Erros no log do jogo | Nenhum `[error]`. Avisos: lista de devices Vulkan, `gamecontrollerdb.txt` ausente, um VFS `ShaderDumpxe` sem device. |

Não há medição de 1% low, 0,1% low nem temperatura. `dumpsys thermalservice` não foi relido nesta passagem.

## O que um toque automático não cobre

Ninguém segurou o telefone para percorrer a fase, testar pulo contínuo, pausa, rotação física, segundo controle Bluetooth ou ouvir o mix. O botão A e o direcional direito responderam via `adb input`. A sessão de mídia confirma que o áudio chegou ao sistema. O resto precisa de uma pessoa no aparelho.

## Windows

D3D12 e o Vulkan de Windows não foram recompilados nem executados nesta sessão. Os arquivos do conserto de ritmo e do cache de shaders não foram editados.
