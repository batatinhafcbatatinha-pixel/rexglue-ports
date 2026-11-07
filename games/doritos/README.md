# Doritos Crash Course — port PC nativo

> [!NOTE]
> This port now lives in the [rexglue-ports](../../) repository as `games/doritos`. The native
> layers it shares with the other Xbox 360 ports (renderer, XMA audio, PC settings menu, platform code) are in
> [`shared/src`](../../shared/src/); this game keeps its own code and a `project/src/port_game.h`. Clone
> rexglue-ports (with `git submodule update --init` for the game-file installer) and run the scripts from
> this folder.

Port experimental para PC baseado em recompilação estática ReXGlue do pacote Xbox 360 original. Este repositório contém apenas código, configuração e documentação; não contém o pacote STFS, XEX, dados originais, código gerado ou executável compilado.

## Estado atual

A extração STFS somente leitura e a análise estática do XEX já foram concluídas. O pacote `LIVE` não é aceito pelo 7-Zip nem pela rota `open_binary` do REA; o contêiner foi validado por `stfschk`, extraído offline e o XEX foi convertido para PE base para Ghidra.

Os arquivos originais devem ser fornecidos pelo usuário e extraídos para `D:\CodexBuilds\doritos\game_files`. O build, quando existir, deverá ficar em `D:\CodexBuilds\doritos\build`; esse caminho também está em `build-location.txt`.

## Codegen e build

O projeto ReXGlue está em `project/`. A configuração usa o SDK compartilhado em `..\battlefield2\third_party\rexglue-sdk\win-amd64` (ou `REXGLUE_SDK`) e mantém o XEX fora do repositório. Para gerar código e compilar, use `build-doritos.cmd codegen`; para apenas compilar uma geração já existente, use `build-doritos.cmd`. A saída gerada fica em `project/generated/` e o build fica em `D:\CodexBuilds\doritos\build`; ambos são locais e ignorados pelo Git.

O codegen foi executado offline com ReXGlue v0.10.0.24-dev.gbd833a2, produzindo 291 arquivos gerados para o XEX fornecido. O build ainda depende de CMake, Ninja e um compilador C++ disponíveis no ambiente. Nenhum binário do jogo foi iniciado.

## Aviso

Doritos Crash Course, seus códigos, marcas e dados pertencem aos respectivos detentores. Não distribua a mídia original, arquivos extraídos, código gerado ou binários compilados.
