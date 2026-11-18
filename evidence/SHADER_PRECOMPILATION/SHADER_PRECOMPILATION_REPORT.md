# Precompilação de shaders e pipelines

## Causa medida

O SDK já gravava o microcode Xenos (`.xsh`) e a descrição de cada pipeline (`.xpso`) e, no arranque seguinte, recriava esses pipelines antes de soltar a thread do jogo. O que faltava no Vulkan era o custo da primeira vez.

Na sessão anterior, com o cache de descrições ainda vazio, o log registrou 102 criações de pipeline: 78 no primeiro minuto, 22 no seguinte e 2 no outro. Cada combinação nova chamava `vkCreateGraphicsPipelines` na thread do quadro para um pipeline provisório e, em seguida, o desenho era descartado. O tempo de compilação caía no quadro e o provisório não era exibido.

Direct3D 12 já enfileirava a criação fora da thread do quadro e pulava o desenho enquanto o PSO não existia. A compilação nativa dos dois backends continua separada. O que é compartilhado é só o microcode do Xenos.

## O que mudou

No Vulkan, uma combinação nova não compila mais pipeline na thread do quadro. A criação real vai para as threads já existentes e o desenho daquele objeto espera os quadros seguintes. As 51 descrições já gravadas foram recriadas antes do jogo começar, em 109,5 ms, todas com `worker=1`. Nenhuma criação com `worker=0` apareceu nesse arranque.

O `VkPipelineCache` do driver passa a ser gravado em `shaders/shareable/<title>.vkcache`, com vendor, device, driver e UUID. Cache incompatível é ignorado. Nesta primeira gravação saíram 1.564.265 bytes. Direct3D 12 ganhou o mesmo registro `PSO_CREATE` quando a criação passa de 2 ms. O cache de PSO do D3D12 continua o do próprio driver mais o `.xpso` que já existia.

## Limite

Não existe, nos assets, uma lista fechada de todas as combinações de blend, depth e render target. Um efeito nunca visto ainda gera uma entrada nova. Ela não trava o quadro; fica pronta para o próximo lançamento. Cobrir o jogo inteiro exige jogar cada fase uma vez para alimentar o `.xpso`. A tradução Xenos acima de 2 ms também é registrada como `SHADER_TRANSLATE`.
