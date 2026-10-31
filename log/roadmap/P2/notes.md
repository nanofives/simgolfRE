# P2: the GDI text provider survives a killed process (2026-10-31)

## Cause
port/src/sim/gditext.cpp wrote the game's font files to `%TEMP%\sg_gditext_<pid>_<n>` (an existing directory was accepted) and
registered the .fot with AddFontResourceA (session-wide). A killed process never ran ~Impl, so the session kept the .ttf locked;
a later process with the same pid picked the same name, fopen(.ttf, "wb") failed and the run stopped at record 0 with
MissingResource{"text font file"} (-11).

## Edits (log/roadmap/P2/edits.py, anchored, one file + the new test; mirror port/build/P2 = git archive 2291a342)
1. `dir()`: CreateDirectoryA must create a NEW directory; on ERROR_ALREADY_EXISTS the name gets a GetTickCount64 suffix, 64 tries,
   then the existing MissingResource. 2. AddFontResourceExA(fot, FR_PRIVATE) / RemoveFontResourceExA(.., FR_PRIVATE) (port choice,
   commented next to the Font::loadFile description). 3. negctl mode 8 on gditext = fix off (old behaviour). 4. new
   port/test/test_gditext_tempdir.py (copied from log/roadmap/P2/test_gditext_tempdir.py by edits.py).
FR_PRIVATE changed no measured value: gditext suite incl. recorded widths and the 15 font-metric words unchanged on x64 and Win32.

## Gates (logs g_*.log, g_gates.log; builds build_x64.log / build_Win32.log, both with FreeType, rc 0)
- test_gditext + test_gditext_tempdir: x64 26 passed / 1 skipped (SG_EMU_ORACLE opt-in); Win32 22 passed / 5 skipped (the 4 child tests skip on win32 by design, plus the oracle).
- test_fttext x64 21 passed / 1 skipped; port test_static 6 passed.
- segments 205/205 x64 and Win32: every row and digest equal to I21 (only the wall-time column differs).
- boot x64 66 PASS / 2 KNOWN / 2 NO_FRAMES: equal to I21 on 69 rows; stories_fixture, FAIL (-11, missing text font file) in I21, is PASS 269/269.
- test_app_shell 18 passed, test_app_live 13 passed (x64).
- New test, 4 tests x64: (a) child process; the parent creates sg_gditext_<childpid>_1..3 holding exclusively-locked manu3_.ttf/klepto__.ttf
  before the child builds its provider: provider still works; (b) child killed with TerminateProcess: no locked font file left.
  Controls: negctl 8 makes (a) fail (provider does not reach DONE) and (b) leave locked files: both RED as required.
Note: the trap files are locked with an exclusive CreateFile handle (what a session registration does to them), not AddFontResourceA.
