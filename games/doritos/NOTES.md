# Doritos Crash Course — análise estática

## Identidade da mídia

- Pacote: `D:\ISO\Doritos Crash Course (World) (XBLA)\Doritos Crash Course (World) (XBLA)\58410A71\000D0000\AC156E0F6A8E78FA306375D57C1210050E46DE4E58`
- Formato observado: pacote Xbox 360 STFS/LIVE (`LIVE` nos quatro primeiros bytes).
- Tamanho declarado em `TASK.md` e confirmado no disco: `61,476,864` bytes.
- SHA-256 do pacote: `81a27027dc9029ddd9a4df872450cb56ade98a0d385e019506cdd2ff51a5f2be`.
- O pacote não é um ZIP/7z reconhecível e o 7-Zip recusou a abertura.
- O `stfschk` offline confirmou 24 arquivos, `ContentSize=0x3A96000`, `TitleId=0x58410A71`, `MediaId=0x261A2859` e integridade de tabelas/blocos/diretórios; a assinatura LIVE não pôde ser validada pela ferramenta.

## REA

Foi tentada a rota obrigatória do REA com `open_binary` sobre o pacote STFS. O servidor recusou o alvo como `unsupported binary format` / `file_format_rejected`. Isso impede usar o inventário de artefato diretamente no contêiner STFS.

Após extração, `open_binary` também recusou o XEX (`unsupported binary format`). O PE base convertido pelo XexTool-RE foi recusado por arquitetura PowerPC `COFF machine 0x01f2`. Assim, o REA foi usado e documentado, mas não consegue fornecer a análise nativa deste alvo no Windows.

## Extração e XEX

- Conteúdo extraído de forma somente leitura para `D:\CodexBuilds\doritos\game_files`.
- `default.xex`: `4,501,504` bytes, SHA-256 `6c4fa5a12ef2ad51db4809d5d654b487300a1ace06c2cdf6cb14258fd273dfc1`.
- XEX2 retail, comprimido e encriptado; módulo title; PE original `default.exe`.
- Image base `0x82000000`, entry point `0x823E1790`, image size `0x1210000`.
- Bibliotecas estáticas declaradas: `XPARTY`, `D3D9`, `XAPILIB`, `XMP`, `XGRAPHC`, `XBOXKRNL`, `LIBCPMT`, `XAVATAR`, `XCAM`, `XAUDIO2`, `XAPOBA`, `XMIC`, `XAUD`, `D3DX9`, `XONLINE`, `QNETXA2`, `XRNSS`, `XRNMS`, `XMCORE`, `XHV2`, `LINK`, `C2`, `C1`.
- Bibliotecas de importação: `xam.xex` e `xboxkrnl.exe`, ambas v2.0.11626.0.
- O XexTool-RE foi usado apenas como ferramenta offline para obter o PE base; nenhum código do jogo foi executado.

## Análise estática fallback

Como previsto nas regras para um alvo PowerPC recusado pelo REA, o PE base foi analisado offline pelo Ghidra 12.1.4 com o processador `PowerPC:BE:32:default`. O projeto e os relatórios ficam fora do repositório em `D:\CodexBuilds\doritos\ghidra` e `D:\CodexBuilds\doritos\analysis`.

- 10.101 funções identificadas e 1.714 símbolos de strings exportados.
- Segmentos observados: `.text` `0x821A0000–0x829F77FF`, `.data` `0x82A00000–0x831E9B5B`, `.idata` `0x831F0000–0x831F05FF`, além de `.rdata`, `.pdata`, `.embsec_`, `.XBMOVIE`, `.XEXID`, `.XBLD` e relocations.
- Strings indicam renderer/D3D9 e compilação de shaders Xenos (`vertexshader`, `pixelshader`, `Texture2D`, `D3D10_SHADER_*`).
- Strings indicam áudio FMOD com threads de stream, mixer, geometria, arquivo e `FMOD_XMic_update_thread`, além de codec Ogg/Vorbis e FLAC.
- A presença dessas strings é evidência estática; não prova quais caminhos são executados nem valida boot, vídeo ou áudio.

## Limites e segurança

- A mídia original não foi alterada.
- Nenhum jogo, runner, launcher ou executável foi iniciado.
- A leitura feita até agora é somente de cabeçalho/bytes do pacote e ferramentas offline.
- Os arquivos extraídos e o PE base são artefatos locais em `D:\CodexBuilds\doritos` e não devem ser versionados.

## Próxima análise

1. O manifesto ReXGlue foi configurado para `D:\CodexBuilds\doritos\game_files\default.xex`; o nome do projeto usa `doritoscrashcourse` para manter consistentes os nomes de targets e arquivos gerados pelo SDK.
2. O codegen offline foi concluído com sucesso: 291 saídas escritas/inalteradas, incluindo 142 partições de funções, `init`, `register`, PCH e fontes recompiladas. O relatório `codegen.stamp` e os fontes ficam em `project/generated/`, que é ignorado e não deve ser versionado.
3. Nenhuma função faltante foi emitida pelo codegen. Isso não substitui a revisão de imports/APIs e não prova que o port inicializa.
4. A compilação não pôde ser iniciada porque `cmake`, `ninja` e `clang++` não estão disponíveis no PATH deste ambiente; não instalar ferramentas globalmente conforme as regras.
5. Implementar hooks nativos de vídeo/áudio somente após confirmar as APIs do SDK e compilar a base.

## Build offline — 2026-11-02

O CMake 3.31.6, Ninja 1.12.1, LLVM/Clang 22.1.2 e o Windows SDK estavam instalados, porém fora do `PATH`. `build-doritos.cmd` passou a localizar essas ferramentas sem instalação global nem alteração persistente do sistema. A configuração CMake e o codegen concluíram; o codegen confirmou 291 saídas já atualizadas.

A compilação não concluiu porque o SIMDe incluído no ReXGlue SDK usa `__builtin_nontemporal_store` e `__builtin_nontemporal_load` com ponteiros para unions (`simde__m64`/`simde__m128i`), combinação rejeitada pelo Clang 22.1.2 em `simde/x86/sse.h` e `simde/x86/sse4.1.h`. A tentativa com MSVC foi abandonada porque os headers do SDK exigem builtins/extensões específicas do Clang. Nenhum binário do jogo foi iniciado.

## Build offline — tentativa seguinte

Foi criada uma cópia local dos headers SIMDe x86 em `project/compat/include`, com os caminhos opcionais nontemporal desativados; o SDK compartilhado permaneceu intocado. Com essa compatibilidade, o build compilou o PCH, o app e 145 de 150 etapas. A falha restante está no código gerado `generated/default/doritoscrashcourse_recomp.137.cpp`: há sete `goto`s para labels `loc_8285E3C8`, `loc_8285E2F8`, `loc_8285E300`, `loc_8285E308`, `loc_8285E1D0`, `loc_8285E1FC` e `loc_8285E0F8` que não foram emitidos. Como `generated/` é saída do codegen, não foi editado manualmente. Isso aponta para incompatibilidade/bug do codegen ReXGlue, não para hooks do port.

## Retomada (Claude) — 03/11/2026

- **Labels ausentes no recomp.137:** `sub_8285DCB0` despacha por tabela de bytes em `0x820ECEF8` sem checagem de limite; o codegen leu 256 entradas e engoliu o corpo de `sub_8285DE00`. Só 16 entradas são válidas (as seguintes apontam para o meio de loops `lwarx`). Corrigido com `[[entrypoint.switch_tables]]` no manifesto (bctr `0x8285DCF4`, r11), igual ao caso do Street 3. A entrada 4 aponta para `sub_8285DE00` e o codegen a descarta (vira trap).
- **Link falhava com `_mm_*` não resolvidos:** o projeto usava `clang-cl` e um overlay SIMDe local. Os outros ports usam o driver GNU `clang++` (D:\llvm22) dentro do ambiente `vcvars64`; com isso o SIMDe do SDK compila sem alteração. Overlay `project/compat` removido; presets e `build-doritos.cmd` alinhados ao Madagascar 3.
- Teste: `python scripts/run_test.py --seconds 60 --shot-every 10 [--cdb]` (janela sem foco, capturas em `D:\CodexBuilds\doritos\runs\<id>`).
- **Funções faltantes (FATAL invalid function):** `scripts/decode_image.py` gera seções/pdata a partir do PE base; `recover_indirect_entries.py` (59 entradas por dados), `find_code_func_ptrs.py` + `function_extent.py` (367 ponteiros lis/addi; bases de jump table logo após o próprio bctr excluídas) e `find_vcall_thunks.py` (37 thunks `lwz/lwz/mtctr/bctr`). Manifesto: 463 entradas.
- **XUsbcam\*** (câmera Live Vision) não existe no SDK: stubs em `src/hooks.cpp` retornando "device not connected".
- **GPU:** por ora o plugin Xenos do SDK (`rexgpu-xenos.dll`, copiado pelo CMake; `gpu_plugin=xenos` padrão no app).
- **Teste automático:** `python scripts/run_test.py --seconds 130 --shot-every 5 --nav` vai do boot ao gameplay sozinho: A a cada 6 s a partir de 35 s; B quando reconhece o aviso azul "not signed in to Xbox LIVE" (o "Saving..." tem a mesma faixa azul, por isso conta pixels de texto); depois anda para frente e pula. O pad de script fica sozinho no usuário 0 (`ScriptOnlyAssignment`), senão o controle real disputa o slot; `DORITOS_INPUT_WITH_PADS=1` desliga isso. A tela de título ignora A nos primeiros segundos.

## Estado em 03/11 12h
Boot → logos → título → menu → Single Player → USA 1 "Safety First" → corrida com HUD e cronômetro. Câmera segue o jogador.
Pendências:
1. **Jogador invisível:** o personagem é o Avatar do Xbox; malha/animações vêm do pacote de assets de avatar do sistema do console (fora do pacote do jogo). O SDK não tem esses dados.
2. **Áudio quase mudo:** só ~10 `SubmitFrame` em 110 s e ~80 mil "XMA: write to unknown register" (FMOD + XMA). Investigar como no Copa (anel XMA cheio) / áudio nativo do Madagascar.
3. Renderer nativo D3D11 (hoje Xenos), menu F1, Vulkan, launcher/kit.

## Avatar do jogador — investigação 03/11 tarde
- SDK e Xenia: `XamAvatarInitialize` retorna `0xFFFFFFFF`; demais XamAvatar* não existem. O jogo (`sub_82395970`, init do AVATARMODELMANAGER) chama `XAvatarInitialize` (`sub_825EEE50`, lib XAvatar do XDK estática) e, se falha, põe o gerenciador no estado 3 (desligado).
- Fluxo normal: manifesto (`RetrieveMetadata` `sub_82395B30`: local `XamAvatarGetManifestLocalUser`, aleatório `GetMetadataRandom`, deslogado `GetMetadataSignedOutProfile`; ou `.amd` predefinido via `_ReadMetadataFromFile` `sub_82395770`) → `BuildAvatarBuffers` `sub_82394600` (`XamAvatarGetAssetsResultSize` + `XamAvatarGetAssets` máscara 0x1FFF, assíncrono) → `UpdateAvatarDataLoading` `sub_82394070` → render `sub_82392028` (shaders `avatar/avatar_vs|ps`, submalhas de 0x200 bytes, buffers em master+0x214/+0x228/+0x22C). As malhas vêm do pacote de assets de avatar do console: não existem no jogo.
- O jogo traz `data/art/_avatar/dummymodel.cmf/.csf/.cfg` (Cal3D v1010: corpo 3073 vértices/6756 triângulos + olhos/sobrancelhas/boca, esqueleto 71 ossos, todas as animações) e `precache/avatar_pc.txt` (versão PC de desenvolvimento usava `BNMD:_avatar/DummyModel.cfg`). Leitor do pacote: `scripts/pak_list.py`, `scripts/pak_extract.py` (LZMA alone) → `D:\CodexBuilds\doritos\pak`.
- O jogo lê pastas soltas `game:\data\<dir>\` no lugar das do pacote (a pasta inteira é substituída). Experimento: `fighter.wbr` com `XAVT:*` → `BNMD:_avatar/dummymodel.cfg` + registro novo em `bonedmodel.wbr` (`scripts/wbr.py`): carrega sem travar, mas o jogador continua invisível. Arquivos guardados em `D:\CodexBuilds\doritos\experiments\loose-templates-dummy` (fora do jogo).
- Ghidra headless: `scripts/ghidra/decompile.cmd 0xADDR...` (cópia com savegprlr→nop / restgprlr→blr para o descompilador) → `D:\CodexBuilds\doritos\ghidra2\out`.
- Caminho proposto: fingir o sistema de avatar (Initialize OK, manifesto falso, GetAssets assíncrono) e montar os buffers que `sub_82392028` desenha a partir do `dummymodel.cmf`, no layout da XAvatar.
- Também visto: `DEVICE_HUNG` (TDR) no plugin Xenos durante uma corrida.

## Avatar falso — 03/11 noite
- `src/avatar_xam.cpp` (XamAvatar* falsos + `avatar_dummy.bin` de `scripts/make_avatar_blob.py`) funciona: GetAssets roda, o caminho de desenho `sub_823919D0`/`sub_82392028` é chamado todo frame.
- Jogador ainda invisível porque a paleta de ossos (`0x831E7E00`, 72×0x30, copiada em `sub_82391A68`) é toda NaN.
- A paleta vem do objeto esqueleto (lookup `sub_8223B8F8` com a chave em res+0x140; vtable `0x82161C24`, +0x1B8 contagem `sub_8239C768`, +0x1E8 matriz `sub_8239D1E8`). Nós: `*(*(obj+0x268)+4)` → vetor de nós; nó+0x48 posição, +0x54 rotação, +0x64 matriz 3x3. **Todos os 72 nós já estão NaN** (posição, rotação, matriz), então o NaN nasce antes do render: na montagem/animação do esqueleto, não nas matrizes do desenho.
- Os registros de osso do blob batem com o osso extra que o jogo acrescenta (`sub_82394968`): +0x10 vec, +0x20 quat, +0x30 translação, +0x40 quat, +0x50 escala; os quats estão normalizados. Suspeitas restantes: w da translação (+0x3C, o jogo põe 1.0, o blob põe 0), +0x1C, ou dados de animação/física aplicados ao esqueleto.
- Próximo passo: pôr +0x3C=1.0 no blob; depois rastrear quem escreve nó+0x48 (vtable +0xC0/+0xC4 do modelo e `sub_82394968`).
- Trace: `DORITOS_AVATAR_TRACE=1` (leituras seguras; uma versão anterior que chamava função do guest dentro do hook travava na tela de título).
- Python com PIL: `py -3.14 scripts/run_test.py ...` (o `python` padrão não tem PIL).

## Avatar visível — 04/11
- O NaN do esqueleto já tinha sido resolvido na madrugada (blob com +0x3C=1.0 etc.); o jogador passou a aparecer, mas verde chapado.
- **Normal:** a declaração de vértices do corpo é montada em `sub_82395EC8` (não há cópia estática): +0 FLOAT3 posição, +0xC `0x2A2190` (k_10_11_11 com sinal: x 11 bits no bit 0, y 11 no bit 11, z 10 no bit 22), +0x10 UBYTE4N pesos, +0x14 UBYTE4 índices, +0x18 D3DCOLOR, +0x1C/+0x20/+0x24 FLOAT16_2 uv0..2, stride 40. O blob gravava DEC3N 10/10/10. Corrigido (`hend3n`).
- **UV:** FLOAT16_2 é lido com x na metade alta do dword big-endian (teste com textura de quadrantes): `half(u) << 16 | half(v)`.
- **Verde:** comandos tipo 3 → registrador de PS pela tabela de `sub_82395EC8`: 0x0D..0x15 → c215..c223 (tons da cabeça), 0x16..0x18 → c210..c212 (CustomColor1-3), 0x19 → c214 (Reflectivity), 0x1A → c213 (Transparency), **0x1B → c5 rimLightColour** (w = expoente). Os batches reais trazem c5; o blob não trazia, e c5 ficava com lixo do desenho anterior (verde com expoente negativo → aro verde no corpo inteiro). Agora o blob grava `--rim 0.15 0.15 0.15 3.0`.
- Padrões do `make_avatar_blob.py`: `--intensity clear`, CustomColor 0 (a textura do corpo aparece direto). `--body-texture` troca a textura (diagnóstico).
- O shader do avatar está em HLSL dentro do exe (`dxshaders/avatar.shader`, ~0x82126EA0; tipo de batch = bits _AVATAR_HEAD/SHINY/TRANSPARENT) e em `data/art/dxshaders/avatarpose_xenon.sha`. Os shaders compilados do `precache_xenon.bsh` não têm formato de vfetch (o D3D do 360 corrige na hora do bind).
- Resultado: avatar com cabelo, pele, camisa amarela e calça azul correndo/pulando com as animações do jogo. Resta: faixa amarelada no rosto (UV do corpo na cabeça / texturas de rosto improvisadas).

## Áudio nativo — 04/11
- Causa do quase-mudo: o jogo registra dois clientes de render driver (mesmo callback `sub_8261FE68`, args 0x40011640/0x400116A0) com 200 ms de diferença; o APU do SDK atendeu só os 10 primeiros quadros do cliente 0 e nunca mais chamou nenhum dos dois.
- Trocado pelo áudio nativo do Madagascar 3 (`project/src/native_audio/`, sem endereços do jogo: hooks dos 7 imports XAudio/XMA + MMIO do XMA; FFmpeg do SDK). `DORITOS_SDK_AUDIO=1` volta ao APU do SDK; `DORITOS_AUDIO_LOG=1` imprime contadores por segundo.
- Medido (run 20261104-125124): ~375 callbacks/s (2 clientes × 187,5), pico 5.000–20.000 desde a tela de título até a corrida; XMA 0 kicks (o FMOD decodifica Vorbis em software). Alguns segundos caem para 30–70 callbacks (provável travada de CPU no carregamento). Não foi ouvido; gravar loopback precisa de `pip install soundcard`.

## Renderer nativo — 05/11 (Claude)
- `project/src/native_gpu/` = renderer D3D11 do Madagascar 3; `pc_settings/pc_theme/pc_font` = menu F1 (tema ainda o do
  Madagascar; trocar pelo visual do Doritos). Plugin Xenos removido (`config.gpu_plugin.clear()`, graphics nulo).
- Mapeamento das funções e offsets: `analysis/native-gpu-map.md` (scripts `callers.py`, `callees.py`,
  `offset_pairs.py`; imagem decodificada em `D:\CodexBuilds\doritos\decoded`). EndVertices é inline no
  DrawPrimitiveUP `823C7460` (hook nele). Offsets a confirmar no teste: decl 0x2ED0, scratch 0x4230 (escrito por 823D9260).
- Build na fila `D:/CodexBuilds/bq.sh` (log `D:\CodexBuilds\logs\doritos-build.out`). Nada testado ainda.
