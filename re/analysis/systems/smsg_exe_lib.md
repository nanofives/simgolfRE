# smsg_exe_lib: golf_clean.exe static CRT routines by library-object identity (S10.7, 2026-10-29)

Archive: VC6 RTM libcmt.lib (tools/vc6/vc98/lib), also libcimt / libcpmt. Why this archive: the exe links the RTM static CRT: of the library procedures with a member in libcmt, the RTM archive is identical for every one except __amsg_exit, SP5 and SP2 for fewer (7_agree_golf.tsv).

A row is accepted when the library member's function object is identical to our body: match.py's normalized
instruction comparison at 100% (relocated operands masked), or, where jump tables sit inside the body
(memcpy, memmove), equal bytes with the relocation sites masked and each section-local relocation holding
address + label + stored addend. No compilation is involved. Driver: log/roadmap/S10/7_tools/libscore.py.

Rows here: 188 (3 of them found by comparing leftover functions with every library body, no PDB name). Skipped: 45 addresses already have a row in another names file,
4 are entry labels or thunks that no function of ours starts at.

Extent used for the 100% result (how many addresses):
- byte-identity (relocation sites masked): 9
- extent as listed: 202
- lib-code-size: 4
- lib-to-section-end: 4
- pdb-size: 14
- pdb-size trim-pad: 1

Names: C routines keep the PDB public name without the @N suffix (S10.8 convention) except jgld.dll and
Terrain.dll, which take the PDB's undecorated name (S10.9 convention). C++ names read Class::member, and a
name already used in the module gets the address as a suffix.

CSVs: log/roadmap/S10/7_lib/golf/<archive tag>/ (never log/diff; the copy step is in 7_merge.md).
