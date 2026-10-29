# third_party/smsg: snapshot of github.com/SMSG-decomp (2026-10-29)

Copied on Mariano's instruction ("copy everything, I don't mind licensing issues", 2026-10-29). SMSG-decomp is an independent
matching decompilation of the same English v1.03 that this project targets. Its three DLLs and its wrapped golf.exe are identical to our
anchors. Its unwrapped golf.exe differs from ours in its first 954,368 bytes; where they differ was not determined. Everything here is a
plain export of the named commit, without .git. The upstream licence files stay in place: SMSG-decomp and MSVC6-tooling are LGPL-3.0,
and the docs/*.ksy specs are CC0-1.0.

| Directory | Source | Commit |
|---|---|---|
| SMSG-decomp/ | github.com/SMSG-decomp/SMSG-decomp | 0f0a4cd524e98e9588af819113d756643a8bb25b (2026-10-20) |
| SMSG-decomp/sound/lpc10.lib/lpc10 | github.com/jafingerhut/lpc10 (submodule) | a051165fced7802ce8508eb62779fc9d07012285 |
| SMSG-decomp/golf/jpeg.lib/jpeg-6a | github.com/libjpeg-turbo/ijg (submodule) | 489583f5165e05d37302e8eeec58104ea0109127 |
| SMSG-decomp/jgl/libpng.lib/libpng | github.com/pnggroup/libpng (submodule) | 860ab2b1c02759c61efb5d63d2b07bd0d484a4c4 |
| SMSG-decomp/jgl/zlib.lib/zlib | github.com/madler/zlib (submodule) | e26a448e9673d67dc2866e11a48d24fc352e5f80 |
| MSVC6-tooling/ | github.com/SMSG-decomp/MSVC6-tooling | 6235a8308d09b2152f0e26af8d652df59eca4384 (2026-10-24) |
| Firaxis-decomp-notes/ | github.com/SMSG-decomp/Firaxis-decomp-notes | d340cb19afcb98b91886320e3ffe3aad04f686ee |
| release/*.pdb | the SMSG-decomp release of 2026-10-19 (golf, jgld, sound, Terrain) | release assets |

## Fetched locally, not committed (Microsoft toolchains and SDK; gitignored like tools/vc6)

These are the SMSG-decomp submodules for its toolchains:

| Directory | Source | Commit |
|---|---|---|
| tools/vc6sp5/ | github.com/archaic-msvc/msvc600_sp5 (VC6 SP5; sound.dll's C++ back end is C2 12.00.8966) | b0b07e29108e2695eb0274c2a377a7b7d7326150 |
| tools/vc6sp2/ | github.com/archaic-toolchains/msvc600_sp2 (the SP2 debug CRT that jgld.dll and Terrain.dll link) | 79157a87dec5e5ff014178f817f27a70098fd862 |
| tools/dxsdk8/ | github.com/edgeforce/directx8 (DirectX 8 SDK) | e6fa10f5f92bf523c15fdb068206ce25c8d2121d |

## Not copied

- SMSG-decomp's `toolchain/vc6` submodule (isledecomp/MSVC600-8168) is the same cl 12.00.8168 we already have in tools/vc6.
- github.com/SMSG-decomp/MSVC6-decomp: Ghidra output of Microsoft's CL / C1 / C1XX / C2. Nothing in our pipeline uses compiler
  sources, so it would add nothing.

## How it may be used here

Their claims are hypotheses until our own tools confirm them, following NO-GUESSING and re/CONFIDENCE.md:

- A matched source counts once our re/tools/match.py compiles it to 100% against our anchored binary. That is C4 match evidence in
  its own right.
- A name or field meaning counts once our disassembly supports it, with the evidence cited in re/names or the model.
- Their PDBs go into a scratch Ghidra pool clone, never into the master.
