# 2010 FIFA World Cup South Africa (Xbox 360) — port nativo

Origem: `E:\ISO\2010 World Cup.iso` (XGD2, partição no offset 0). Arquivos extraídos em `game_files/` (5,6 GB).
Mesma arquitetura do FIFA Street 2012 (`../street4`): launcher `default.xex` (entrada 0x820FB900, imagem
0x82000000-0x821F0000) carrega `fifadllzf.xex` (sem `.dll`; base 0x82300000, entrada 0x82832840).

## Estrutura

- `project/` — cópia da camada nativa do Street 4 (renderer D3D11 próprio, tradutor de shaders, XMA nativo, menu F1),
  renomeada (`wc2010`, variáveis `WC2010_*`). Manifesto `project/wc2010_manifest.toml`.
- `analysis/decoded{,-fifadll}/` — imagens decodificadas (ferramenta `copa2014/analysis-tools`, compilada em
  `D:\CodexBuilds\wc2010-tools\copa_image_inspect.exe`) + `pdata-functions.json` (`scripts/summarize_image.py`).
- `build.cmd` (configura + ninja incremental), `build-detached.cmd` (build completo via `ps2-perf-tools/buildsup.py`,
  status em `D:\CodexBuilds\wc2010\build-status.txt`). Árvore de build: `D:\CodexBuilds\wc2010\win-amd64-relwithdebinfo`.
- Teste: `python scripts/run_test.py --seconds N --input inputs/x.txt --dump-every 600 [--cdb] [--env K=V]`.

## Biblioteca D3D (XDK mais antigo que o do Street 4)

Funções achadas por forma de instrução contra o Street 4 (`scripts/match_functions.py`, `align_libs.py`,
`ppcview.py --cmp`):

| Função | Street 4 | WC2010 |
|---|---|---|
| Clear | 8270ED88 | 8272DBB8 |
| Present/Swap | 8270FDC0 | 8272E488 |
| BeginTiling / EndTiling | 82710C68 / 82710FD8 | 8272F1C0 / 8272F530 |
| Resolve (já com a ABI do thunk do S4) | 82713018 | 827301B8 |
| BlockOnFence | 82713710 | 82731880 |
| Kick (CP_RB_WPTR inline, índice em dev+0x2ACC) | 82713B80 (+82713368) | 82731CE0 |
| InsertTimestamp | 82714060 | 827320F0 |
| InsertCallback / callback de replay de tiling | 827141A0 / 82718450 | 827322C0 / 827362C0 |
| Begin/EndVertices, DrawVerticesUP | 8271B9C8/B9B8, 8271BE88 | 82732E48/82732E38, 827332E0 |
| DrawVertices / DrawIndexedVertices | 8271C4B0 / 8271C8C8 | 82733328 / 82733718 |
| VS imediato / patch in-place / flush | 8271ACF0 / 8271AEF8 / 8271B038 | 82738960 / 82738BF8 / 82738D38 |
| handler de interrupção | 82713448 | 82731458 |
| printf do CRT | 82737EA8 | 828341E8 |
| setjmp / longjmp | 83576BC0 / 83576E90 | 8283C680 / 8283C950 |

DrawIndexedVerticesUP não existe nesta biblioteca. Fim de occlusion query ainda não localizado (sem sentinela
0xFFFFFEED na biblioteca).

Offsets do device: `scripts/offset_map.py` → `analysis/device-offset-map.txt`. Sombras de registradores (< +0x2A80)
iguais; depois: status 0x2A90, callback 0x2A94, timestamp 0x2A9C, flags 0x2ABC/0x2ABD (bit GPU-less 0x02, como no
Street 3), pendentes dos workers 0x2B04, declaração 0x2ED8, index buffer 0x3144, PS/VS 0x3244/0x3248, dados inline
0x3554. Sem sombra PA_CL_UCP. Objetos de shader com o mesmo layout (vs+0x368/0x380, ps+0x18/0x28/0x40).

## Estado (03/11/2026)

- Codegen: launcher precisou de 2 thunks; DLL de 17 funções fora do `.pdata` (`scripts/closure_extent.py`).
- Removidos do Street 4: nativo de animação, ganchos de menu (assinatura LIVE), sondas de efeitos; tema do F1 ainda
  é o do Street 4 (fontes não existem neste disco → visual padrão).

## 04/11/2026

- O SDK atual (battlefield2/third_party/rexglue-sdk) nomeia o alvo do módulo `wc2010_fifadllzf` (sem `_xex`, ao
  contrário do Street 4): corrigidos `project/CMakeLists.txt` (senão os ganchos/renderer não entravam no build, sem
  erro) e `RegisterRecompiledModule` em `wc2010_app.h`.
- Fontes nativos compilam limpos; build completo lançado via `build-detached.cmd` (status em
  `D:\CodexBuilds\wc2010\build-status.txt`). Próximo: primeiro boot com `scripts/run_test.py`.

## 04/11/2026 noite (Claude)

- Primeiro boot (`runs/20261104-185240`): módulo `fifadllzf.xex` registra 86.834 funções; memória, EA Messaging,
  big file de pré-carga e locale sobem; D3D em modo GPU-less; morre em
  `[FATAL] Call to invalid or unregistered function at guest address 0x835F7700` (alvo só alcançável por vtable).
- `scripts/s4img.py` (cópia do Street 4 com o layout do wc2010: `.text` 0x82650000+19434932, registro
  `wc2010_register.cpp`) e `find_missing_functions.py` ajustado (`.rdata` 0x82300400, `.data` 0x838E0000).
  320 candidatos -> `closure_extent.py` -> 348 entradas novas sem sobreposição com `.pdata` nem entre si,
  acrescentadas ao fim de `[modules.functions]` (backup em `D:\CodexBuilds\wc2010\manifest.before-ptrscan.toml`).
- Codegen OK (271 arquivos reescritos). Build completo via `build-detached.cmd` (1 job, ~4,4 GB livres).
  Próximo: `python scripts/run_test.py --seconds 60` e ver o próximo bloqueio.
- Entrada 0x838D74AC removida: codegen "outranked by import" (thunk de import), registrava sem corpo -> link falhava (sub_838D74AC indefinido).
- Build 19:47 OK. Boot (`runs/20261104-194852`, `-194907` com cdb): passa do 0x835F7700, 64 presents, monta
  `data/sceneassets/genheadtex.big`, eventos DLC CONTENT_UPDATE/INITIALIZATION_COMPLETE; depois
  `Unhandled guest access violation: read of guest 0x0006C6B8` na thread 0xF8000344.
  Pilha: sub_82CBD7E0+0x107 <- 82CA1B50 <- 8265E158 <- 8265E810 <- 8265F5F0 <- 8267CAD8 <- 82836038 <- 828360B8 <- 8282FD10.
  Em 82CBD7E0: `r3 = sub_832BF128(sub_82C501D0())`, depois `lwzx r28,r3,0x6C6B8`. 82C501D0 devolve o objeto global
  em 0x83A4648C; 832BF128 devolve o campo +100 dele, que ainda é 0. match_functions contra o Street 4 não acha
  par (código próprio da Copa). Próximo: descobrir quem preenche obj+100 (provável inicialização feita por outra
  thread/etapa que não rodou ou falhou antes); 26 funções candidatas leem 0x83A4648C e gravam em +100.

## 05/11/2026 (Claude)

- O guard de `sub_82CBD7E0` (project/src/native/game_natives.cpp, 02:01) não estava no build: a DLL é de 01:57; corrida
  12:56 morreu de novo em 0x6C6B8. Análise do Codex em `analysis/ASK-obj100.md` (quem preenche G+100 = `82C9A4B0`,
  adiado até o estado 8 do bootflow; o r28 só é usado no estado 3). Rebuild na fila (D:/CodexBuilds/q-wc2010.sh).
- Com o guard (run 20261105-015902) a tela de idioma aparece (frame-000600.png).
- 05/11: src/input_hooks.cpp (experimento inacabado de 02:01, pcs::ObservePad inexistente) tirado do CMake; o arquivo ficou.

## 06/11/2026 noite (Claude) — partida jogável a 60 fps
- Quedas por corrida no input: o InputSystem do SDK (DLL pré-compilada) não tem trava; várias threads em
  XamInputGetState/RefreshDevices liberavam o mesmo vetor (AV em _Destroy_range<DeviceInfo>; XObject liberado e
  reaproveitado pela string "Xbox 360"). `src/input_hooks.cpp` voltou ao build (sem o ObservePad inexistente):
  serializa XamInputGetState/GetCapabilities/SetState/GetKeystrokeEx com Wc2010InputLock; ReadInputSystem e
  ApplyVibration (pc_settings.cpp) usam a mesma trava.
- Funções faltantes ao carregar a partida (0x82B158C8, 0x82A2DBE8, ...): `find_missing_functions.py
  project/generated/fifadllzf_xex` (rápido em segundo plano) -> 57 candidatos -> `function_extent.py`; fim = salto de
  cauda + 4 (os alvos 0x8283292C..3C são auxiliares restgprlr) -> 42 entradas novas no manifesto (18 puladas por
  sobrepor entradas existentes; candidatos em analysis/missing-*-20261106.txt). `codegen-build.cmd` = codegen + build
  (fila D:/CodexBuilds/bq.sh).
- Texturas (renderer.cpp): comparação completa até 1 MB (era 256 KB) e revalidação no mesmo quadro quando o vigia de
  escrita acusa as páginas — glyphs trocados/ausentes no menu ("hecline", "S EECH", "Passinʀ"). Ainda não conferido.
- Teste ao vivo: `scripts/press.py` (botão agora ou `--seq "A:3 START:2"`) acrescenta ao input.txt da corrida;
  tela via Windows MCP Screenshot (região da janela).
- Resultado (run 20261106-211...): idioma -> perfil -> menu -> Kick-off ENG x GER -> cena de abertura -> escalações
  -> partida em campo (HUD, radar), 60 fps estáveis (600 presents/10 s), sem queda.
- Aberto: lado do controle (D-pad abre o diálogo; escolhi "Yes" = CPU x CPU); conferir glyphs; áudio.
