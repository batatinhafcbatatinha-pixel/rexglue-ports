# Status — Doritos Crash Course

## Atualização 03/11 (Claude)

Compila e roda até o gameplay (detalhes em NOTES.md, "Retomada"). Pendências: jogador invisível (assets de avatar do sistema), áudio quase mudo, renderer nativo.

## Regras ativas (fase das equipes; encerrada)

- Nenhum jogo, runner ou launcher será executado.
- `D:\ISO` é somente leitura.
- Dados extraídos e builds ficam em `D:\CodexBuilds\doritos`.

## Feito

- [x] Revisados `EQUIPES-REGRAS.md`, `AGENTS.md` e `TASK.md`.
- [x] Confirmado que não havia trabalho anterior em `doritos-crash-course`.
- [x] Espaço verificado: C: aproximadamente 12,7 GiB livres; D: aproximadamente 43,9 GiB livres.
- [x] Criados `.gitignore`, `build-location.txt`, `README.md`, `NOTES.md` e a sonda offline `scripts/inspect_package.py`.
- [x] Inicializado repositório Git local com identidade `batatinhafcbatatinha-pixel`.
- [x] Calculado SHA-256 do pacote: `81a27027dc9029ddd9a4df872450cb56ade98a0d385e019506cdd2ff51a5f2be`.
- [x] Confirmado cabeçalho `LIVE` do pacote STFS.
- [x] Validado o contêiner com `stfschk` offline: 24 arquivos, hashes/tabelas/diretórios válidos; assinatura LIVE não validada.
- [x] Extraído o conteúdo somente leitura para `D:\CodexBuilds\doritos\game_files`.
- [x] Gerado relatório SHA-256 dos 24 arquivos extraídos em `D:\CodexBuilds\doritos\manifest.json`.
- [x] Convertido o XEX comprimido/encriptado para PE base com XexTool-RE offline.
- [x] Tentado REA no pacote, XEX e PE base; recusou o contêiner e a arquitetura PowerPC (`COFF machine 0x01f2`).
- [x] Usado fallback Ghidra PowerPC offline: 10.101 funções e 1.714 strings exportadas.
- [x] Preparados `Install-Files.cmd`, `installer.json`, `Play-Doritos.cmd` e `README.md`; launcher não foi executado.
- [x] Criado esqueleto ReXGlue em `project/`: manifest, config, CMake, app mínima, hooks e script de build.
- [x] Executado codegen ReXGlue offline com sucesso usando o SDK `0.10.0.24-dev.gbd833a2`; foram geradas 142 partições de funções e os arquivos auxiliares de inicialização/registro.
- [x] Corrigidos os nomes do projeto/manifesto para manter consistentes os targets e arquivos gerados pelo SDK.
- [x] Redirecionado o build CMake para `D:\CodexBuilds\doritos\build`, fora do repositório.
- [x] Ajustado `build-doritos.cmd` para descobrir CMake, Ninja, LLVM, `rc.exe`, `mt.exe` e bibliotecas do Windows instalados localmente, sem alterar PATH global.
- [x] Configuração CMake concluída com CMake 3.31.6, Ninja 1.12.1 e ReXGlue SDK `0.10.0.24-dev.gbd833a2`.
- [x] Codegen foi reexecutado pelo build e confirmou o módulo `default.xex` sem alterações pendentes.

## Em andamento

- Revisão da configuração gerada concluída; nenhum jogo, runner ou launcher foi iniciado.
- Overlay local de compatibilidade SIMDe foi adicionado em `project/compat/include` e integrado ao CMake, sem alterar o SDK compartilhado.
- Compilação offline avançou para 145/150 objetos; os headers e a maioria das partições geradas compilam.

## Bloqueios

- REA não suporta este pacote STFS nem PE PowerPC neste ambiente Windows; fallback Ghidra foi aplicado conforme as regras.
- O REA recusou o contêiner STFS e o PE PowerPC; o fallback Ghidra e o codegen ReXGlue foram concluídos, mas hooks e compilação ainda não foram validados.
- O PATH não contém as ferramentas, mas elas existem localmente e agora são descobertas pelo script de build.
- O compilador Clang 22.1.2 falha nos overloads nontemporal do SIMDe empacotado pelo SDK. O port agora usa uma cópia local dos headers x86 com esses caminhos opcionais desativados; o SDK compartilhado não foi alterado.
- O codegen/ReXGlue gerou `project/generated/default/doritoscrashcourse_recomp.137.cpp` com referências a labels inexistentes (`loc_8285E3C8`, `loc_8285E2F8`, `loc_8285E300`, `loc_8285E308`, `loc_8285E1D0`, `loc_8285E1FC`, `loc_8285E0F8`). O build falha nesse objeto, depois de compilar 145/150 etapas. O arquivo é gerado e não deve ser corrigido manualmente; é necessário corrigir/atualizar o codegen ou usar uma versão de SDK compatível.
- Uma tentativa com MSVC `cl.exe` também foi descartada: o SDK depende de extensões/builtins Clang (`__builtin_bswap*`, `__builtin_isnan` e intrinsics PPC), portanto MSVC não é substituto compatível.

## Atualização 05/11 (nemotron)

### Feito

- [x] Build concluído com sucesso: `doritoscrashcourse_recomp.137.cpp` compila sem erros (labels ausentes resolvidas pelo `[[entrypoint.switch_tables]]` no manifesto).
- [x] `doritoscrashcourse.exe` vinculado em `D:\CodexBuilds\doritos\build\win-amd64-release\`.
- [x] Codegen reexecutado pelo CMake: `0 written, 0 unchanged, 0 deleted, 1 module(s) up to date` — saída gerada está consistente com o manifesto.
- [x] Build incremental forçado (touch + rebuild) confirmou compilação limpa de `recomp.137.cpp` e link do executável.

### Resultado

O bloqueio de compilação em `recomp.137.cpp` (7 `goto`s para labels `loc_8285E*` ausentes) está resolvido. O codegen agora emite a tabela de switch de `sub_8285DCB0` com 16 entradas válidas (casos 0–15) e descarta a entrada 4 (aponta para `sub_8285DE00`, vira trap via chamada de função). Todos os 150 objetos compilam e o executável é gerado.

### Bloqueios

- Nenhum bloqueio de compilação restante.
- Áudio nativo implementado (Madagascar 3, FFmpeg XMA) mas não verificado audível ("não foi ouvido").
- Renderer ainda usa plugin Xenos (`rexgpu-xenos.dll`); D3D11 nativo não implementado.
- Avatar tem "faixa amarelada no rosto" (UV do corpo na cabeça / texturas de rosto improvisadas) — problema visual menor.

### Próximo passo

- Revisar hooks de vídeo/áudio conforme o plano anterior.
- Considerar renderer nativo D3D11 (substituir Xenos).
- O dono deve testar manualmente o jogo; esta equipe não pode executar jogo, runner ou launcher.

## Validação

- Nenhum jogo, runner ou launcher foi executado.
- Nenhum comportamento de runtime foi validado.
- Build validado offline: todos os objetos compilam, executável linkado com sucesso.
- `CMakePresets.json` válido; todas as fontes em `generated/default/sources.cmake` existem.
