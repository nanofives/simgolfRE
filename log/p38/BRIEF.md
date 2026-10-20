# Phase 3.8 shared brief (all streams 38a..38f)

Plan: re/PHASE38_PLAN.md (approved by the user 2026-10-20: GDI text widths validated by recorded widths, a multi-year
course_season recording, six parallel streams). Read it, then SimGolf/CLAUDE.md (port sections: Phase 2.4 .. 3.7b),
port/CORE.md, port/test/README.md, and the notes your stream builds on (log/p3/p37a_notes.txt, log/p3/p37b_notes.txt).

## Rules
- Python is `~/bin/py -3.12` (bare `py` is refused on this account).
- BUILD DIR: your own only. `cmake -S port -B port/build/<stream> -A x64` (and `-A Win32` in port/build/<stream>_w32 if you
  need x87), then point tests at it with `SG_PORTSIM_DLL=<path to portsim_c.dll>`. NEVER build into port/build/x64 (the
  parent's; a test run holding it broke the last full suite). If a sibling's half-written file breaks your build, use a
  private mirror as 3.7b did (log/p3/p37b/mirror.py) instead of editing their file.
- SHARED FILES you must NOT edit: port/src/sim/modules.inc, port/src/sim/tick.cpp kDrops / fallback tables,
  port/src/sim/commands.cpp registrations, port/CMakeLists.txt, CLAUDE.md, UNCERTAINTIES.md. Put the lines you need in
  `log/p38/<stream>_merge.md` (exact text and where) and the parent merges them. Your own new module file
  port/src/sim/<module>.cpp (+ header) and your own tests port/test/test_<module>.py are yours.
- NO-GUESSING (CLAUDE.md): cite addresses in notes; banned hedge words; unclear -> a new row in log/p38/<stream>_uncertain.tsv
  (columns of UNCERTAINTIES.md, ids from your range: 38a U-0960.., 38b U-0970.., 38c U-0980.., 38d U-0990.., 38e U-1000..,
  38f U-1010..); the parent merges them.
- Port code has no image-range literals, not even in comments (test_static): cite functions by name, flag bits as shifts.
- EVIDENCE per ported function: vectors if any exist (re/port/vectors_manifest.tsv), else the emulator oracle
  (port/test/cmdemu.py, SG_EMU_ORACLE=1) and golden digests, plus the segments it unblocks
  (log/p3/p37a_cmdsegs.py with your DLL). Every function gets a negative control that reads RED (check its arithmetic: a control
  that changes nothing is an equivalent mutant).
- GAME RUNS (38a, 38f only): your own instance (`re/tools/instances.py create p38a|p38f`), SIMGOLF_ALLOW_MULTI=1, kill only the
  PID you spawned, C3FT_AUDIO=none (default). Never touch original\ or another instance. Call purge_frida_temp after runs.
- Long test runs: foreground with full output, tee to a log in log/p38/.
- Git: do not commit. When done, write `log/p38/<stream>_notes.md` (what was done, numbers, files, open items, U-rows) and
  `log/p38/<stream>_files.txt` (every file you created or changed, one path per line), then report to the parent in one short
  message. If you hit a session limit, your notes file must let a fresh session continue.
