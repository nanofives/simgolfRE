# smsg_sound_lib: sound.dll static CRT routines by library-object identity (S10.7, 2026-10-29)

Archive: VC6 SP5 libcmt.lib (tools/vc6sp5/VC98/lib), also libcimt / libcpmt. Why this archive: sound.pdb has no archive module for the CRT, so the candidates are PDB publics outside every procedure; SP5 is identical for 393 of the 394 libcmt candidates, SP2 for 391, RTM for 368 (7_agree_sound.tsv).

A row is accepted when the library member's function object is identical to our body: match.py's normalized
instruction comparison at 100% (relocated operands masked), or, where jump tables sit inside the body
(memcpy, memmove), equal bytes with the relocation sites masked and each section-local relocation holding
address + label + stored addend. No compilation is involved. Driver: log/roadmap/S10/7_tools/libscore.py.

Rows here: 260 (4 of them found by comparing leftover functions with every library body, no PDB name). Skipped: 68 addresses already have a row in another names file,
101 are entry labels or thunks that no function of ours starts at.

Extent used for the 100% result (how many addresses):
- byte-identity (relocation sites masked): 72
- extent as listed: 270
- lib-code-size: 12
- lib-to-section-end: 5
- pdb-size: 57
- pdb-size trim-pad: 9

Names: C routines keep the PDB public name without the @N suffix (S10.8 convention) except jgld.dll and
Terrain.dll, which take the PDB's undecorated name (S10.9 convention). C++ names read Class::member, and a
name already used in the module gets the address as a suffix.

CSVs: log/roadmap/S10/7_lib/sound/<archive tag>/ (never log/diff; the copy step is in 7_merge.md).
