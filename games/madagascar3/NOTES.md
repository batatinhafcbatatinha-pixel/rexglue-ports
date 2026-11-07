# DreamWorks Madagascar 3: The Video Game — Projeto de port PC (Xbox 360 Recomp)

## Informações do Jogo e Mídia Original

- **Origem:** `D:\ISO\DreamWorks Madagascar 3 - The Video Game (USA)\DreamWorks Madagascar 3 - The Video Game (USA).iso`
- **Tamanho da ISO:** 7.838.695.424 bytes (7,30 GiB)
- **Formato da Mídia:** Xbox 360 XGD3 (Offset da Partição de Jogo: 265.879.552 / `0x0FDA0000`)
- **Plataforma:** Xbox 360 (PowerPC Tri-Core Big-Endian, GPU ATI Xenos)
- **Desenvolvedora:** Monkey Bar Games
- **Publicadora:** D3Publisher
- **Total de Recursos:** 9.402 arquivos (4.169.718.232 bytes descompactados na mídia)

---

## Estrutura do Executável Principal (`default.xex`)

- **Tamanho do Arquivo:** 5.545.984 bytes
- **SHA-256:** `38d0d7e6665c89567b0566dff387d2925a0b7f001367b99fe08bfd48e32fd1a0`
- **Formato:** XEX2
- **Title ID:** `44500801`
- **Media ID:** `1457EFAA`
- **Versão:** `0x00000001`
- **Base Version:** `0x00000001`
- **Ponto de Entrada (Entry Point):** `0x822C1F50`
- **Image Base:** `0x82000000`
- **Disco:** 1 de 1

---

## Estrutura de Arquivos Base do Port

- `default.xex` — Cópia direta do executável principal do jogo extraído da partição XGD3.
- `game_files/` — Arquivos de configuração, renderização e recursos base extraídos (`default.xex`, `gameconfig.bin`, `nxeart`, `Configurations/`, `RenderConfig/`).
- `analysis/` — Registros completos de inventário da mídia (`iso-inventory.json`) e executáveis extraídos.
- `config/` — Manifesto de recompilação ReXGlue (`madagascar3_manifest.toml`) e metadados (`xex-config.json`).
- `launcher/` — Código do lançador e integração.
- `project/` — Base do projeto para geração de código e compilação do executável portado.
- `scripts/` — Utilitários de inspeção e extração de mídia Xbox 360 (`inspect_iso.py`, `extract_iso.py`).
- `build-madagascar.cmd` — Script de compilação com Ninja / MSVC / Clang.
- `Play-Madagascar3.cmd` — Inicializador amigável para testes de execução.

## Continuação de 25/10/2026

O manifesto inicial era apenas metadado e não era aceito pelo SDK. O projeto agora usa o manifesto gerado pelo ReXGlue local `0.10.0.24-dev.gbd833a2`. A tradução AOT concluiu após declarar uma função leaf omitida pela descoberta automática; a justificativa e os bytes estão em `analysis/leaf-function-fix.json`.

A imagem decodificada tem 9.371.648 bytes, 12.627 registros válidos em `.pdata` e 168 imports nomeados. Há 654 branches diretos para 163 imports em `analysis/import-call-xrefs.json`. Essas contagens são estáticas e não validam boot.

Comandos (na raiz deste jogo):

```
build-madagascar.cmd release codegen
build-madagascar.cmd release configure
build-madagascar.cmd release build
```

O último comando executa as três etapas com dois compiladores simultâneos. O monitor interrompe a árvore do processo se o projeto atingir 2 GiB ou o disco ficar com menos de 20 GiB livres; a verificação ocorre a cada dois segundos, com possível avanço entre verificações. Não baixa outro SDK nem extrai os recursos da ISO. O backend selecionado é o plugin Xenos do SDK, não um renderer D3D11 nativo próprio.

`extract_iso.py` limita extração adicional a 512 MiB e mantém 20 GiB livres por padrão. Use `--include` para selecionar caminhos. A extração completa dos ~4 GiB de recursos é recusada com esses limites. `inspect_iso.py --inventory-only` lê a ISO sem duplicar o XEX. Os recursos restantes ainda estão em `D:\ISO`; jogo, áudio, controles e saves continuam sem validação.

Build Release concluída e smoke executado com GPU ativa. O runtime ainda aborta no alvo indireto não registrado `0x822F7ED8`; não há menu/gameplay validado.

## Integração nativa — continuação sem execução

- `recover_indirect_entries.py` encontrou e aplicou 51 entradas apontadas por dados, fora de `.pdata`, com limites obtidos por CFG. Quatro candidatos ambíguos foram excluídos. O alvo anterior `0x822F7ED8` agora está traduzido até `0x822F7FCC`; isso remove a lacuna estática, sem presumir um resultado de boot. Manifesto e relatório preservam bytes/hashes. Total: **16.050 mappings de funções**.
- Áudio substituído pela fábrica `m3audio::NativeAudioSystem`: decoder FFmpeg XMA + saída SDL3, com sete exports realmente importados pelo jogo. Não instancia a APU do SDK. A biblioteca XMAPlayback ainda usa um adaptador de comandos/contextos; essa compatibilidade e o comportamento ainda precisam de validação. Código em `project/src/native_audio/`, build concluída.
- Compilador offline `madagascar3_shaderc.exe` e biblioteca `madagascar3_native_shaders.lib` compilados. Convertem microcódigo Xenos big-endian em HLSL e DXBC Shader Model 5 usando D3DCompile. Recusam shaders com memexport ainda não integrado. Uso: `madagascar3_shaderc vs|ps arquivo.bin diretorio-novo`. Não foi executado nesta rodada. É uma etapa do renderer nativo, não um renderer completo.
- O desenho de frames ainda usa o plugin Xenos. Falta recuperar a ABI/layouts próprios do D3D deste jogo e ligar draw/clear/resolve/present/recursos ao backend do host. Endereços e offsets do Battlefield não foram copiados como se fossem deste jogo.

Executável e ferramentas recompilados; `analysis/native-integration.json` registra identidades e limites. Nenhum jogo foi executado nesta continuação. Build/codegen mantêm reserva de 20 GiB e teto observado de 2 GiB no projeto.
