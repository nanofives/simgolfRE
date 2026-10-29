# smsg_jgld_lib: jgld.dll static CRT routines by library-object identity (S10.7, 2026-10-29)

Archive: VC6 SP2 libcmtd.lib (tools/vc6sp2/VC98/lib/lib), also LIBCIMTD / LIBCPMTD. Why this archive: jgld.pdb names C:\VS6SP2\VC98\LIB\libcmtd.lib as the archive of 319 procedures.

A row is accepted when the library member's function object is identical to our body: match.py's normalized
instruction comparison at 100% (relocated operands masked), or, where jump tables sit inside the body
(memcpy, memmove), equal bytes with the relocation sites masked and each section-local relocation holding
address + label + stored addend. No compilation is involved. Driver: log/roadmap/S10/7_tools/libscore.py.

Rows here: 261 (13 of them found by comparing leftover functions with every library body, no PDB name). Skipped: 68 addresses already have a row in another names file,
1 are entry labels or thunks that no function of ours starts at.

Extent used for the 100% result (how many addresses):
- byte-identity (relocation sites masked): 5
- extent as listed: 279
- lib-to-section-end: 8
- pdb-size: 25
- trim-pad lib-to-section-end: 1

Names: C routines keep the PDB public name without the @N suffix (S10.8 convention) except jgld.dll and
Terrain.dll, which take the PDB's undecorated name (S10.9 convention). C++ names read Class::member, and a
name already used in the module gets the address as a suffix.

CSVs: log/roadmap/S10/7_lib/jgld/<archive tag>/ (never log/diff; the copy step is in 7_merge.md).
