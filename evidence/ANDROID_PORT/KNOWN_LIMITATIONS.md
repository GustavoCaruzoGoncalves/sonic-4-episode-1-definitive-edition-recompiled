# Limitações conhecidas

- ARM32 não é suportado. Ver `ANDROID_COMPATIBILITY_MATRIX.md`. Não existe APK `armeabi-v7a` nem universal.
- A recompilação Android está em `-O1`. O Clang 19 do NDK r28 aborta no otimizador em `-O3` nesses arquivos. O ritmo do guest no F7 cai para cerca de 30 atualizações por segundo enquanto pipelines novos ainda são criados. A lógica de pacing não foi alterada para compensar.
- Um PSO de ~440 ms ainda nasceu no thread de apresentação (`worker=0`). O restante foi para o worker.
- O cache de driver Vulkan neste aparelho continua pequeno (64 bytes depois de uma fase). A chave inclui vendor, device, driver e `pipelineCacheUUID`. Não deve ser copiada para outra GPU.
- Áudio é aberto em 6 canais a 48 kHz. O telefone mostrou a música na sessão de mídia. Não houve escuta confirmada do mix estéreo.
- `gamecontrollerdb.txt` não está no pacote. Controle físico depende do mapeamento padrão do SDL Android e não foi testado.
- Os botões na tela cobrem as laterais da imagem 16:9. Não há layout configurável.
- Tarjas laterais são o aspecto 16:9 dentro do painel 20:9 em landscape.
- Sem o override de `setOrientationBis`, o SDL pedia `FULL_USER` e o jogo ficava em retrato com a imagem numa faixa no meio.
- Dados do jogo ficam no armazenamento interno. Um `adb uninstall` apaga save, cache e o CPK. O APK não leva os 189 MB.
- `OpenAndroidContentFileDescriptor` retorna -1. Um `content://` não abre.
- Formatos YUV preferidos do Xenos caem no formato Vulkan 37 neste Adreno.
- Não há OpenGL ES. Sem Vulkan o app não é instalável (`uses-feature` obrigatório).
- APK de desenvolvimento, `debuggable=true`, keystore debug.
- Desempenho térmico e FPS de apresentação (1% low) não foram medidos.
