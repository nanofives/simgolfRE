# smsg_terrain_lib: Terrain.dll static CRT routines by library-object identity (S10.7, 2026-10-29)

Archive: VC6 SP2 libcmtd.lib, libcimtd.lib. Why this archive: Terrain.pdb names libcmtd.lib (340 procedures) and libcimtd.lib (133).

A row is accepted when the library member's function object is identical to our body: match.py's normalized
instruction comparison at 100% (relocated operands masked), or, where jump tables sit inside the body
(memcpy, memmove), equal bytes with the relocation sites masked and each section-local relocation holding
address + label + stored addend. No compilation is involved. Driver: log/roadmap/S10/7_tools/libscore.py.

Rows here: 423 (14 of them found by comparing leftover functions with every library body, no PDB name). Skipped: 61 addresses already have a row in another names file,
1 are entry labels or thunks that no function of ours starts at.

Extent used for the 100% result (how many addresses):
- byte-identity (relocation sites masked): 5
- extent as listed: 433
- lib-to-section-end: 7
- pdb-size: 26
- trim-pad lib-to-section-end: 1

Names: C routines keep the PDB public name without the @N suffix (S10.8 convention) except jgld.dll and
Terrain.dll, which take the PDB's undecorated name (S10.9 convention). C++ names read Class::member, and a
name already used in the module gets the address as a suffix.

CSVs: log/roadmap/S10/7_lib/Terrain/<archive tag>/ (never log/diff; the copy step is in 7_merge.md).
