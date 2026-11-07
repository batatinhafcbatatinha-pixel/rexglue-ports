# Redistribution and provenance

This is a source and tooling distribution. It contains no original game assets, no game executable (XEX), no generated translation units, no disassembly, no SDK binaries and no compiled game executable. The manifest lists function addresses and sizes that the translator needs; it describes where functions are, not what they contain.

The independently written native integration (renderer, shader translation, audio, hooks, PC settings) and tooling are offered under the root MIT license. This license grants no rights to game content owned by its publisher or by DreamWorks Animation.

Fonts in `project/fonts/` are PT Sans by ParaType, under the SIL Open Font License 1.1 (`licenses/PT-Sans-OFL.txt`); they are free fonts, not files taken from the game.

External dependencies are downloaded separately and are not stored here: the ReXGlue SDK (BSD-3-Clause, derived from Xenia, `licenses/ReXGlue-Xenia-LICENSE.txt`), FFmpeg public headers (LGPL-2.1, `licenses/FFmpeg-COPYING.LGPLv2.1.txt`) and DXVK 3.1.1 (zlib license, `licenses/DXVK-LICENSE.txt`).

The publication audit (`tools/audit_publication.py`) uses an allowlist and rejects executables, images, audio, archives, game resources, generated code and oversized files. It is an engineering safeguard, not a legal guarantee for every jurisdiction or use. Do not submit game content or private build artifacts in pull requests or issues.
