# S10.1 notes: SMSG sources -> re/match wrappers (2026-10-29)

Status: DONE. Converter + PDB reader written, Terrain.dll calibrated (169 / 169), golf.exe pilot (Geometry.cpp, plus Plot.cpp
and InputPoll.cpp to exercise overloads and ICF folding), inventories for all four modules, match tier and static tier run.

## Tools

- `re/tools/smsg_pdb.py` (promoted from log/roadmap/S10/pdb/pdbread.py): `load(<golf|jgld|sound|Terrain>)` gives
  `procs` (rva, size, kind G/L, undecorated name, DBI module = the .obj, lib = the archive or the .obj itself), `publics`,
  `public_names()` (rva -> decorated names) and `procs_of("<x>.obj")`. CLI: `smsg_pdb.py procs|publics|modules <pdb>
  [--obj X.obj]`. The old tables in log/roadmap/S10/pdb/ lacked the module name of library members (they printed only the
  archive), which the per-file mapping of jackal.lib / libpng / zlib / jpeg / lpc10 sources needs.
- `re/tools/match.py`: two new directives (default behaviour unchanged, every existing file keeps its result):
  `// INCLUDE <dir>` (repo-relative or absolute, search order; `// INCLUDE $VC` places VC6's own include directory there,
  else it comes last) and `// COMPILE <path>` (cl compiles that repo-relative file in place of the annotated one).
  COMPILE exists because of the first calibration run: with a wrapper that `#include`d Terrain.cpp, three Terrain functions
  missed (localRender 59.7 %, RenderTileEdgeSkirt 90.3 %, renderTile 96.3 %). Under /ZI, `__LINE__` in the MAIN source is the
  per-function line variable (`movsx eax, word ptr [0x10063e48]; add eax, 3` in the original localRender at 0x100089e0..)
  but a constant in an #included file (`push 0x648`). Compiling their file directly gives 100 % on all three.
- `re/tools/smsg_import.py`, one command per SMSG source file (paths relative to third_party/smsg/SMSG-decomp):
  ```
  ~/bin/py -3.12 re/tools/smsg_import.py file golf/jackal.lib/src/fifo.cpp          # one source
  ~/bin/py -3.12 re/tools/smsg_import.py project golf/jackal.lib/jackal.dsp          # every source of a .dsp
  ~/bin/py -3.12 re/tools/smsg_import.py inventory golf                             # log/roadmap/S10/inventory_golf.tsv
  ~/bin/py -3.12 re/tools/smsg_import.py calibrate Terrain                          # 1_calibration_Terrain.tsv
  ~/bin/py -3.12 re/tools/smsg_import.py list                                       # projects and sources
  ```
  Projects: terrain/Terrain.dll/Terrain.dsp, jgl/{zlib.lib/zlib,libpng.lib/libpng,jgld.dll/jgld}.dsp,
  sound/{lpc10.lib/lpc10,sound.dll/sound}.dsp, golf/{golf.exe/golf,jackal.lib/jackal,jpeg.lib/jpeg}.dsp.

## What `file` does

1. FLAGS: the project's `# ADD CPP` line plus the source's own `# ADD CPP` / `# SUBTRACT CPP` lines (golf.dsp gives /GX to
   11 sources, one of them with `/Oa /Og /Oi /Ot /Oy /Ob1 /Gs /Gf /Gy`, and `/D TERRAIN_EXPORTS` to one; sound.dsp /GX, /Ob2
   per file on 34; jgld.dsp `/GF /GX` on gsobject.cpp and jglfont.cpp). Dropped: /nologo /c /Gm /YX /FD /FR /W*
   /Fp /Yu /Yc /Fo /Fd (no code effect, or set by match.py); /I becomes INCLUDE lines.
2. INCLUDE: the .dsp's /I directories, then the INCLUDE SMSG-decomp/build.bat sets for that project (table PROJECTS in the
   tool, cited by build.bat line: :terrain 79, jgld 85, lpc10 93, sound 96, golf 103); DX8 headers from tools/dxsdk8.
3. Wrapper: annotation-only (`// FLAGS`, `// INCLUDE`, `// COMPILE third_party/smsg/SMSG-decomp/<src>`, `// LANG c` for .c,
   `// SMSG-TOOLCHAIN sp5` for sound.dll's own sources, informative only until S10.5).
4. Compile once, map every PDB procedure of that object file (`<stem>.obj`; members of a .lib project also by archive
   name) to an obj symbol: the PDB public at that RVA if the obj defines it (`public`), else the obj functions whose
   undecorated name (dbghelp UnDecorateSymbolName NAME_ONLY, `class `/`struct ` and spaces dropped) equals the procedure name
   (`name`; several = `overload`: PDB names carry no parameter types, every candidate is scored and the best kept), else
   `nosym` (the obj does not emit it, e.g. an inline template member not used in this file).
5. Score each with match.score_function (no evidence). Rows at an address where another procedure scored 100 % become
   `folded` (ICF, U-1850). A source with `__declspec(naked)` sends its 100 % rows to wip as `naked_file` (re/C4_PLAN.md:
   naked transcriptions are not evidence; the snapshot has none, grep 2026-10-29).
6. Outputs: `re/match/smsg_<pdb>_[<lib>_]<stem>.cpp` = 100 % lines at addresses no other re/match/*.cpp file has, one per
   address, then `// ALSO 100%, already in <file>` comments (not MATCH lines, so no duplicate evidence); it is re-verified
   with match.compare right after writing (`verified=n/n`). `re/match/wip/smsg_...cpp` = misses / errors as MATCH lines with
   `// miss 87.3% 120/137 ins` and `// SMSG-NOSYM:` comments. `log/roadmap/S10/runs/<stem>.tsv` per procedure and one line per
   run in `runs/summary.tsv` (procs, mapped, new_100, already_100, miss, nosym, folded, error, verified, files, seconds).
   `--evidence` runs match.py's writer on the canonical file. Never pass it while exploring.

## Results

Terrain.dll calibration (`smsg_import.py project terrain/Terrain.dll/Terrain.dsp`, then `calibrate Terrain`;
log/roadmap/S10/1_calibration_Terrain.tsv, 1_terrain_run.log): their six objects hold 169 procedures (VertexArrays.obj
none); all 169 map through the PDB public at their RVA and all 169 compile to 100 % at addresses our re/match already
matches. Our 170th Terrain MATCH, abs 0x100158b0, is a libcmtd.lib member in their PDB (U-1854). New Terrain addresses: 0,
so no smsg_Terrain_*.cpp file exists. Every calibration mismatch of the first run was the #include artefact (above).

golf.exe pilot (runs/summary.tsv, 1_golf_pilot.log):

| source | procs | new 100 % | already ours (100 %) | folded | miss | nosym |
|---|---|---|---|---|---|---|
| Geometry.cpp (the pilot) | 14 | 5 | 9 | 0 | 0 | 0 |
| Plot.cpp | 32 | 11 | 18 | 3 | 0 | 0 |
| InputPoll.cpp | 8 | 6 | 2 | 0 | 0 | 0 |

Every address of ours in these files also scores 100 % from their source. Plot.cpp found the two mapping rules above:
overloads (0x00402180 is the virtual `fill_entry(T *) { return 0; }`, `xor eax, eax; ret 4`; the first version took the
`fill_entry(LinkListEntry *)` overload and scored 16.7 %) and ICF folding (8 procedures at 0x2180, 15 at 0x2190 over
Plot.obj and jackal.lib's SoundDevice.obj / window.obj). Evidence written for the 22 new addresses (1_golf_evidence.log),
all GREEN; C4 promotions in 1_merge.md (2 C3 rows: drawRecords 0x004012d0, appendWalkerName 0x00467600).

Inventories (`inventory <pdb>`, columns rva va size kind pdb_name module lib smsg_source our_match hooks_name hooks_level
c4_candidate smsg_status smsg_score smsg_symbol; distinct addresses):

| PDB | procedures | in their sources | with our MATCH | C4 candidates (C3, no MATCH) |
|---|---|---|---|---|
| golf | 2,107 | 1,958 addresses (all procedures) | 1,346 | 103 |
| jgld | 1,092 | 773 (319 libcmtd.lib SP2) | 769 | 0 |
| sound | 1,238 | 1,238 | 606 | 56 |
| Terrain | 642 | 169 (473 libcmtd / libcimtd) | 170 | 0 |

golf.pdb and sound.pdb carry no procedures for the release CRT (libcmt.lib has no debug records), so every one of their
procedures is in an SMSG source. Re-run `inventory` after each bulk run: smsg_status / smsg_score come from runs/*.tsv.

## Gates run

- `~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py -q`: 354 passed (1_match_tier.log; includes the
  three new smsg_golf_*.cpp files and every existing re/match file).
- `~/bin/py -3.12 -m pytest -m static -q`: 640 passed, 2 skipped (the baseline 630 / 2 plus the 7 new tests and the three new smsg files of test_match; 1_static_tier.log).
- `~/bin/py -3.12 -m pytest tests/test_smsg_import.py -q`: 7 passed. Controls: Course.cpp with release flags `/O2 /GX` reads
  `miss` (RED); the overload test fails with the first mapping rule (take the n-th candidate), which scored 16.7 %.

## For the bulk workers (golf / jackal / jgld / sound)

- One command per source (`file`) or per project (`project`); runs are serial, about 1-3 s per source file. At most 2 in
  parallel while I15 runs. Read runs/summary.tsv, then the wip file of each source with misses.
- Do not hand-edit generated smsg_*.cpp files: re-run the tool. A hand fix of a miss belongs in a new re/match file of your
  own, with the source copied and the reason noted.
- `--evidence` only on a final run; it writes CSVs for canonical lines only, all 100 % by construction.
- sound.dll's own sources need S10.5's toolchain selector (U-1851); lpc10.lib is RTM and can run now.
- A `compile_error` row carries the first cl error line in `note`.
