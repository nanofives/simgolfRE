# S10.4 notes: jgld.dll, libpng.lib, zlib.lib from SMSG sources (2026-10-29)

Status: DONE. `smsg_import.py project` over jgl/jgld.dll/jgld.dsp (23 sources), jgl/libpng.lib/libpng.dsp (10) and
jgl/zlib.lib/zlib.dsp (9) in one chain (4_run.log), then `file ... --evidence` for the 4 sources with a canonical file,
`inventory jgld`, smsg_dedupe.py smsg_jgld (0 duplicates). Counts per source: 4_counts.md.

## Numbers (the calibration)

42 files, 773 procedures: 767 already ours at 100 %, 6 new at 100 %, 0 folded, 0 miss, 0 nosym, 0 compile errors, 773 mapped.
Calibration result: every address we match at 100 % that has a source here (767 procedures;
the inventory counts 769 distinct addresses of ours) also scores 100 % from their source with the project's flags
(/Od /ZI /GZ, /GX /GF on gsobject.cpp and jglfont.cpp). Exceptions: none.
jgld.pdb has 1,092 procedures: 773 in these sources, 319 in libcmtd.lib (SP2 debug CRT, no source).
libpng 1.0.5 and zlib 1.0.2 compile from their sources to the same 100 % as our vendored copies: 17+9+34+6+12+3+31+29+16+17 = 174
libpng and 1+2+5+3+1+7+6+1+4 = 30 zlib procedures, all already ours.

## The 6 new lines (re/match/smsg_jgld_*.cpp, evidence CSVs in log/diff as `<rva>_jgld_<Class>_<method>.match.csv`)

| rva | their name | our hooks row | level | note |
|---|---|---|---|---|
| 0x7e40 | JGLBuffer::init | allocDrawBuffer | C4 | already C4 by scenario; a 100 % match now backs it |
| 0xb140 | JGLBuffer::line_16 | Surface::line16 | C4 | same |
| 0xd4f0 | JGLBuffer::DrawDashedVLine | Surface::dashedVLine16 | C2 held | the original faults (0x1000d912, round 13); a match is not a C2 -> C3 step (U-1853) |
| 0x19c80 | JGLSprite::draw_with_alpha | none | none | no hooks.csv row |
| 0x1a6b0 | JGLSprite::draw_16_16 | none | none | no hooks.csv row |
| 0x692b0 | WndProc | windowProc | C4 | already C4 |

C4 candidates (C3 rows without a MATCH): none (inventory c4_candidate = 0 for jgld), so 4_merge.md has no promote commands.
Two of the six addresses have no tracker row; names from their sources are leads only (S10.9 handles names).

## Gates

- `~/bin/py -3.12 -m pytest tests/test_match.py tests/test_match_tools.py -q`: 4_match_tier.log: 473 passed, 0 failed (final run after dedupe, covers S10.2-4).
- Controls: S10.1's (release flags on a debug source read miss). The calibration itself is the control: 767 of 767 equal.
- Dedupe: `smsg_dedupe.py smsg_jgld` reports 0 duplicate lines.

## Uncertainties (4_uncertain.tsv, U-1865 .. U-1869)

U-1865 no row for 0x19c80 / 0x1a6b0, U-1866 Surface::dashedVLine16 now has a 100 % match while held at C2, U-1867 the 319
libcmtd.lib procedures have no source (SP2 CRT; FidDb is S10.7), U-1868 the 767 calibration lines cover 2 libraries whose
sources are our vendored copies and theirs: identical output does not tell which tree is closer to the original source text,
U-1869 reserved, unused.
