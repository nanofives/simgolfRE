# smsg_exe_1: exe functions the master listed as FUN_, named with our evidence (S10.8, 2026-10-29)

Roadmap step S10.8 compared SMSG-decomp's golf.pdb (third_party/smsg/release, tables in log/roadmap/S10/pdb/) with our names.
Its names are hypotheses. A row here exists only where our own tools show the name: the PDB name was the lead, never the
evidence. Every golf.pdb procedure already has a names row except one (0x0047e770). The other 41 rows are functions that batch
b1 created in the master with neutral FUN_ names and golf.pdb lists only as publics (CRT and libjpeg statics).

Three kinds of evidence:

- **Static CRT, 17 rows (subsystem crt).** log/c3/m36e_lib_match.py compares the master's extent with the object member of
  tools/vc6/vc98/lib/libcmt.lib that defines the symbol, using match.py's own comparison. 13 are equal in every instruction.
  The other 4 (__NLG_Notify, ___onexitinit, __purecall, ___initstdio) equal the library in every instruction of the extent
  re/functions_ghidra.tsv records. The library body goes on for 1 to 3 instructions after a call that does not return. The
  evidence CSVs are in log/roadmap/S10/8_crtlib/, not in log/diff. These rows are names only, not C4 evidence.
- **IJG libjpeg 6a statics, 24 rows (subsystem util).** re/tools/match_autoname.py compiles the existing wrappers
  re/match/golf_jpeg_*.cpp, plus a scratch wrapper for jfdct*.c (log/roadmap/S10/8_tools/jpeg_fdct.cpp), and pairs functions
  at 100%. 22 rows pair at 100%. Each row also cites the store that installs it (re/tools/pointer_callers.py). The other two
  rows:
  - rgb_ycc_start 0x004b4ca0 scores 98.2%. Its only unequal instruction has the same text on both sides. The immediate
    0x807fff falls in the image range and match.py masks it as an address (U-1886).
  - jpeg_fdct_islow 0x004b5d50 scores 22.6%. Its register allocation and frame differ from the compiled jfdctint.c. It is
    named through its installer: jinit_forward_dct 0x004b34b0 (100% matched) stores it as do_dct at 0x004b351c, next to
    jpeg_fdct_ifast (100%) and jpeg_fdct_float (100%) (U-1885).
  These MATCH lines are not added to re/match. That is S10 goal 1, and the merge list hands it over.
- **View::drawSlot78 0x0047e770.** This is slot 78 of the View vtable 0x004ba8b4, installed by View_ctor and View_dtor, and it
  has no direct caller. All of its callees are drawing functions (rectangle fills and blits). golf.pdb's name is
  WidgetBase::on_paint. Stream H deferred it because its visual role was not established, and that is still true. The name
  says only that it draws.

Counts, settlements and renames of existing rows: log/roadmap/S10/8_notes.md and 8_merge.md.
