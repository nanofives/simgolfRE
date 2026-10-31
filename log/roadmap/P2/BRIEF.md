# P2: the GDI text provider must survive a killed process (orchestrator brief, 2026-10-31)

## The defect (measured 2026-10-31 by the orchestrator)
port/src/sim/gditext.cpp registers the game's two font files (manu3_.ttf, klepto__.ttf, read through GameFiles) the way jgld's
Font::loadFile does: it writes them into `%TEMP%\sg_gditext_<pid>_<n>` (n = a per-process counter starting at 1; an existing
directory is accepted, ERROR_ALREADY_EXISTS), makes a .fot with CreateScalableFontResourceA and calls AddFontResourceA(fot).
AddFontResourceA registers the font for the whole SESSION. A process that is killed (timeouts, TaskStop, taskkill) never runs
the provider's destructor (RemoveFontResourceA + DeleteFileA), so the session keeps the .ttf open and LOCKED. When Windows later
gives that pid to a new port process, its first provider picks the same directory name `sg_gditext_<pid>_1`, `fopen(path, "wb")`
on the locked .ttf fails (gditext.cpp register_file) and the run stops with MissingResource{"text font file"} at record 0
(sg_run_boot / sg_run_frames -11).

Evidence: on 2026-10-31 %TEMP% held 35 sg_gditext_* dirs; 7 of them belonged to dead pids and still had both .ttf locked
(scratchpad gditext_locks.py: open r+b fails). The I21 merge hit the failure three times in one day (boot stories_fixture
"missing text font file" at record 0; census cov_a_build3 and census_auto cov_r_s2_screens -11 after 0 records), each run
passing when repeated. The orchestrator released the 7 stale registrations by hand (RemoveFontResourceA on the dead dirs'
.fot, nothing deleted). The app (simgolf_port.exe) uses the same provider, so an app session that crashes leaves the same
trap for a later launch.

## The fix (two parts, both in gditext.cpp)
1. A fresh directory every time: dir() must not accept an existing directory. Loop the name (pid, counter, and if needed a
   further suffix such as GetTickCount64) until CreateDirectoryA creates a NEW one; give up with the existing
   MissingResource only after a bounded number of tries.
2. Process-private registration: AddFontResourceExA(fot, FR_PRIVATE, 0) and the matching RemoveFontResourceExA(fot,
   FR_PRIVATE, 0), so the registration ends with the process even when it is killed. Check that CreateFontIndirectA still
   resolves the faces by name (it does for FR_PRIVATE fonts of the same process) and that every width is unchanged.
   If FR_PRIVATE changes any measured value, keep AddFontResourceA, report the difference, and ship part 1 alone.
Comment the change in the code next to the original's Font::loadFile description (the original registers session-wide;
the port's provider is a host service, so private registration is a port choice; no game behaviour depends on it).

## Gates (mirror port/build/P2 = git archive of HEAD + junctions to port/corpus and the other gitignored data, as P1)
- build x64 + Win32 with FreeType (log/roadmap/P1/build.sh is the model; SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed').
- the gditext / widths tests on x64 and Win32 (port/test/test_gditext*.py and any test reading port/corpus/widths: 11,459 /
  11,459 printable widths, 15 / 15 metric words), test_fttext, port test_static.
- segments 205 / 205 x64 and Win32 and boot x64 66 / 2 / 2, every row and digest equal to log/roadmap/merges/I21/segs_x64.tsv /
  segs_win32.tsv / boot_x64.tsv.
- a NEW test (port/test/test_gditext_tempdir.py): (a) a directory with the name the provider would pick next already exists
  and holds a locked .ttf (registered by the test with AddFontResourceA, released in teardown) -> the provider still works;
  (b) a child process that builds a provider and is then killed with TerminateProcess leaves no locked file behind (with
  FR_PRIVATE). Negative controls: switch the fix off (a test-only negctl mode on the gditext module, default off) -> (a) and (b)
  read RED.
- app: test_app_shell.py and test_app_live.py on x64 (the app attaches the GDI provider at start).

## Rules
- HEAVY RUNS go through the S3 lock: `~/bin/py -3.12 log/roadmap/S3/w8_lock.py run P2 --what "<run>" -- <command>`, ONE build or
  ONE pytest file or ONE gate per hold (never a chain under one hold). The integrator I21 and S3 wave 9 share this lock.
- Never `cmake` directly: build through port/test/build.py with ~/bin/py. Never write into original\ or the install.
- Do not edit the shared tree: work in the mirror; write your notes and an edits.py (anchored edits, like log/roadmap/P1/edits.py)
  under log/roadmap/P2/; the orchestrator commits from your mirror.
- Kill only processes you started. Never delete other sessions' files (the stale sg_gditext_* dirs of other processes stay).
- Report DONE (or BLOCKED with the reason) to the orchestrator with the numbers, then stop.
