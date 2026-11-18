# Versão completa vs demo

O pacote instalado é o título XBLA (content type `0x000D0000`, Title ID `58410A07`), não um pacote demo (`0x00080000`). Os assets das zonas estão em `data.cpk`. A pasta `DEMO` dentro do pacote é a interface de compra, não um executável separado.

A distinção demo/completo é o bit 0 da máscara de licença. `sub_82163E00` grava 1 em objeto+160 e só zera esse campo se `XamContentGetLicenseMask` devolver o bit 0. O cabeçalho LIVE do pacote já traz `license_bits = 1`. O runtime ignora esse campo e usa a cvar `license_mask`, cujo padrão é 0.

O addon em `game/addon_inspect/58410A07/00009000` é content type `0x00009000` (avatar item). Os oito pacotes são: Sonic 4 Ep. I Tee (2), Sonic the Hedgehog 4 Episode I Hat, Chaos Emeralds, Sonic Plushie, Chopper Toy, Moto Bug Toy, Sonic 4 Cartridge. Não entram na máscara de licença.

Aplicado: `license_mask = 1` em `pilot/out/build/win-amd64-release/sonic4.toml`. O XEX não foi alterado. Lançamento com `--license_mask=1` registrou `LICENSE flag160=0` (versão completa). O processo foi encerrado depois da medição.
