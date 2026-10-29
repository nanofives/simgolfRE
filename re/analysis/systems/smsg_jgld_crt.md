# smsg_jgld_crt: jgld.dll static debug CRT routines by library-object identity (S10.9, 2026-10-29)

SMSG-decomp's jgld.pdb puts these functions in `C:\VS6SP2\VC98\LIB\libcmtd.lib`. A row is accepted when
log/c3/m36e_lib_match.py finds our function instruction-identical (100%, relocated operands masked) to the member of
the VC6 SP2 debug CRT that defines the symbol. That is the library-object identity rule from round 36.
- Driver: log/roadmap/S10/9_tools/libid.py.
- Libraries tried: tools/vc6sp2/VC98/lib/lib/libcmtd.lib (SP2), then LIBCMTD.LIB (RTM) and LIBCIMTD.LIB.
- CSVs: log/roadmap/S10/9_lib/jgld/lib<n>/, not log/diff.

Ten functions match only with SMSG's extent: _nh_malloc_dbg, _msize_dbg, _CrtSetDbgBlockType,
_CrtDoForAllClientObjects, _CrtMemCheckpoint, _CrtMemDumpAllObjectsSince, _lockerr_exit, _expand_base, _realloc_base and
_free_base.
- Our function list ends each of them early. The run that matches uses SMSG's size, through a process-local override
  of match.boundary_overrides in 9_tools/libid_extent.py. Its CSVs are in 9_lib/jgld/ext_sp2/.
- The boundaries rows they need are listed in log/roadmap/S10/9_merge.md.

Rejected: _CrtCheckMemory (98.1% with SMSG's extent, 6 of 315 instructions differ) and zlibVersion (zlib, no CRT
member). The `$$$00001` at 0x10091200 is not a function in our master (deferred).
