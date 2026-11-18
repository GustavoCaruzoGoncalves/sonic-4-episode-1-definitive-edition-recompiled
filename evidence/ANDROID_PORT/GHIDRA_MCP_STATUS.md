# Estado do Ghidra MCP

Verificado em 16 de novembro de 2026, depois que o backend em `127.0.0.1:8080` voltou a responder.

## Instalação e versão

- Diretório: `C:\Tools\Ghidra`
- Versão: Ghidra 12.1.4 PUBLIC
- Revisão: `8b6bbb857accdfa20dc5b2f5dea471178c2e9fbc`
- Data do build: 21 de setembro de 2026
- JDK: Eclipse Temurin 21.0.13+9 em `C:\Program Files\Eclipse Adoptium\jdk-21.0.13.9-hotspot`
- `JAVA_HOME` gravado pelo Ghidra: o mesmo JDK

## O que estava quebrado

O processo `ghidra-mcp.exe` (PID 17504) já estava em execução. Ele é só a ponte stdio do Cursor. O Ghidra em si não estava aberto, e nada escutava em `127.0.0.1:8080`. Por isso a chamada anterior falhou com `BACKEND_UNAVAILABLE`.

O plugin já estava instalado e habilitado no CodeBrowser. Não foi preciso instalar outro servidor MCP.

## Projeto aberto

- Projeto: `Sonic4EpisodeI`
- Arquivo: `C:\Tools\ghidra-projects\Sonic4EpisodeI.gpr`
- Programa aberto no CodeBrowser: `/default.xex`
- O projeto de teste anterior, `C:\Tools\ghidra-mcp-test\GhidraMCPTest`, contém apenas `sample_x86_64.exe` e não foi usado nesta sessão.

## Executável importado e arquitetura

- Arquivo: `C:\S4EP1Recomp\game\extracted\default.xex`
- Formato detectado: `XEX Loader by Warranty Voider`
- Linguagem: `PowerPC:BE:64:A2ALT-32addr`
- Compilador: `default`
- Endianness: big-endian
- Tamanho de ponteiro: 32 bits
- Entrada: `0x82223690`
- Faixa de memória: `0x82000400`–`0x825d179b` (5.876.628 bytes)
- Funções após a análise automática: 10720
- MD5 do arquivo importado: `e47af90755d65483ab675efcbd091890`

A análise automática terminou com sucesso em cerca de 130 segundos. O log está em `C:\Tools\ghidra-projects\sonic4-import.log`.

O carregador registrou a base da imagem como `0x82000000`. O campo `image_base` devolvido pelo MCP aparece como `00000000`; os endereços reais do programa começam em `0x82000400`, e a desmontagem em `0x82223690` está correta.

## Transporte e endpoint

A configuração do Cursor em `C:\S4EP1Recomp\.cursor\mcp.json` e em `C:\Users\Administrator\.cursor\mcp.json` é um processo local, não uma URL MCP direta:

- Comando: `C:\Tools\ghidra-mcp-venv\Scripts\ghidra-mcp.exe`
- Argumento: `--ghidra-server http://127.0.0.1:8080/`
- Pacote: `ghidra-mcp` 2.0.0, transporte MCP stdio
- Backend Java: HTTP somente em loopback, `http://127.0.0.1:8080/`

Quem escuta a porta 8080 é o processo do Ghidra, PID 12304 (`python.exe` com a JVM embutida pelo PyGhidra 3.2.0). O `ghidra-mcp.exe` continua sendo a ponte que o Cursor já tinha aberta.

## Plugin

- Extensão instalada: `C:\Users\Administrator\AppData\Roaming\ghidra\ghidra_12.1.4_PUBLIC\Extensions\GhidraMCP`
- Versão declarada: 12.1.4
- Jar: `lib\GhidraMCP.jar`
- Classe: `com.lauriewired.GhidraMCPPlugin`
- Estado: habilitada no CodeBrowser (`_code_browser.tcd` lista a extensão `GhidraMCP`)
- Log: `BackendHttpServer` passou a escutar em `127.0.0.1:8080` às 21:55:28

O servidor sobe no `init()` do plugin, quando o CodeBrowser abre. Só o gerenciador de projetos não publica o backend.

## Carregador de XEX

O Ghidra 12.1.4 não traz carregador de Xbox 360. Foi instalada a extensão já usada para esse formato, `XEXLoaderWV` de zeroKilo:

- Release publicada: 12.1.3 (`ghidra_12.1.3_PUBLIC_20260920_XEXLoaderWV.zip`)
- Não existe release 12.1.4
- Cópia local: `C:\Tools\downloads\ghidra_12.1.3_PUBLIC_20260920_XEXLoaderWV.zip`
- Instalada em: `C:\Users\Administrator\AppData\Roaming\ghidra\ghidra_12.1.4_PUBLIC\Extensions\XEXLoaderWV`
- O campo `version` de `extension.properties` foi ajustado de `12.1.3` para `12.1.4` para o Ghidra 12.1.4 carregar a extensão. O jar não foi recompilado.

Durante a descompressão LZX o carregador registrou `out of input bytes`. Mesmo assim mapeou as seções, inclusive `.text` em `0x820C0000` com tamanho `0x003E05DC`, criou 9090 funções a partir dos dados do executável e concluiu a análise.

## Operações MCP confirmadas

Todas estas chamadas responderam pelo namespace `user-ghidra` com `default.xex` aberto:

| Operação | Resultado |
| --- | --- |
| `get_program_info` | Programa `default.xex`, loader XEX, PowerPC big-endian, 10720 funções |
| `list_functions` | Primeira página a partir de `FUN_820c0000`; total 10720 |
| `get_function_by_address` `0x82223690` | `Function_82223690`, corpo `0x82223690`–`0x82223847` |
| `disassemble_function` `0x82223690` | Primeira instrução `mfspr r12,LR`; 110 instruções no corpo |
| `get_xrefs_to` `0x82223690` | Uma referência, do ponto de entrada |
| `decompile_function_by_address` `0x82223690` | Pseudocódigo C completo, sem truncamento |

## Limitações

- A ponte do Cursor já estava no ar. Se o Ghidra for fechado, a porta 8080 cai e as ferramentas voltam a falhar até o CodeBrowser abrir de novo com um programa.
- O aviso de LZX não foi investigado além do fato de a importação e a análise terem terminado.
- `image_base` no MCP não repete o `0x82000000` registrado pelo carregador.
- A extensão XEX não foi compilada contra o Ghidra 12.1.4; só a versão declarada foi alinhada. O carregamento e a desmontagem do ponto de entrada funcionaram.
- Depois da verificação, a primeira leitura do endereço do crash foi feita no programa aberto. `0x8218FB50` não é uma função no Ghidra. Os bytes são `80 63 00 38` (`lwz r3,56(r3)`) e `48 00 07 1c` (`b 0x82190270`). A causa e a correção continuam na investigação do crash, não neste relatório de conectividade.
