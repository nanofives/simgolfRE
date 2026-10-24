# Step worker brief (roadmap orchestrator, re/ROADMAP_ORCHESTRATOR.md)

You do one step S<n>.<k> of a section of the SimGolf port roadmap. Your row in re/roadmap/S<n>_steps.tsv gives the step, its gate,
and the files you own. Your lead judges your result from the files on disk, so the files are the deliverable, not your report.

## Read first
SimGolf/CLAUDE.md (port sections from "PORT PIVOT" on, and the phase your step continues), port/CORE.md, port/test/README.md, the
plan file your section names, and the notes your step builds on. If log/roadmap/S<n>/<k>_notes.md exists you are continuing a stopped
worker: start from it.

## Rules
- Python is `~/bin/py -3.12` (bare `py` is refused on claude2).
- BUILD DIR: your own only, `cmake -S port -B port/build/S<n>_<k> -A x64` (and `-A Win32` in port/build/S<n>_<k>_w32 for x87 code),
  tests pointed at it with `SG_PORTSIM_DLL=<path to portsim_c.dll>`. Never build into port/build/x64 (the integrator's). If a sibling's
  half-written file breaks your build, build a private mirror (log/p3/p37b/mirror.py) instead of touching their file.
- FILES: create or edit only the files in your row's `files_owned` plus your own notes / logs under log/roadmap/S<n>/. SHARED FILES
  nobody edits directly: port/src/sim/modules.inc, port/src/sim/tick.cpp kDrops / fallback tables, port/src/sim/commands.cpp
  registrations, port/CMakeLists.txt, CLAUDE.md, UNCERTAINTIES.md, re/PORT_PLAN.md, re/roadmap/sections.tsv. Write the lines you need
  into log/roadmap/S<n>/<k>_merge.md (exact text and where); the integrator applies them. After modules.inc changes in a mirror, touch
  registry.cpp (MSBuild does not rebuild it).
- PORT CODE: written from the C3 / C4 notes, the vectors and the recordings, never pasted from re/match. No image-range literals in
  port code, comments included (test_static): cite functions by name, flag bits as shifts. Assets come from the user's install through
  GameFiles; no asset bytes or strings in the repo.
- NO-GUESSING: cite the address of every constant and offset in notes; banned words: probably, likely, seems to, appears to, I think,
  presumably, might be, maybe. Unclear -> a row in log/roadmap/S<n>/<k>_uncertain.tsv (UNCERTAINTIES.md columns, your id range).
- EVIDENCE: every ported body is judged by an oracle (vectors, the emulator oracle port/test/cmdemu.py with SG_EMU_ORACLE=1, the
  pinned recordings, the frame or stream references) and gets a negative control that reads RED. Check a control's arithmetic: a
  control that changes nothing on the judged data is an equivalent mutant, record it as blind with what would judge it.
  The segment gate (all accepted segments, digests unchanged) and the boot gate must stay as re/roadmap/BASELINE.md states.
- TESTS: never `pytest port/test` as one process (it reached 10-16 GB): one file per process (log/p38/suite_by_file.sh) or one file.
  Long runs in the foreground with full output, tee'd to log/roadmap/S<n>/. At most one heavy run of yours at a time.
- GAME RUNS (only if your row says needs_game and your lead granted a slot): your own instance (`re/tools/instances.py create
  s<n>_<k>`), `SIMGOLF_ALLOW_MULTI=1`, C3FT_AUDIO=none unless your step records audio. Kill only the PID you spawned; a stopped
  background shell leaves its Python runner and game alive (kill those by command line). Never boot in original\ (do not import game.py
  before scenario_refs sets the instance; wrap walks in game.preserve_saves()). Call purge_frida_temp after runs. If Defender
  quarantines a DLL in instances\, rebuild and re-run; do not change code to avoid it.
- Write multi-line code and text with the Write / Edit tools, not shell heredocs that carry backslashes.
- Git: do not commit, do not stage, never `git checkout -- <dir>` (siblings share the tree).
- Children: you may spawn helpers (claude2, alias model) for independent sub-parts only with your lead's agreement in your row's notes;
  close every one before you report.

## When you finish (or approach your session limit)
- log/roadmap/S<n>/<k>_notes.md: what was done, the numbers, the commands that reproduce the gate, files, controls, open items and
  U-rows. Complete enough that a fresh worker continues from it.
- log/roadmap/S<n>/<k>_files.txt: every file you created or changed, one path per line.
- Final message to your lead, at most 10 lines:
```
S<n>.<k> <DONE | PARTIAL <reason> | BLOCKED <reason>>
gate: <numbers>   controls: <red>/<total> RED, <blind> blind
notes: log/roadmap/S<n>/<k>_notes.md   files: log/roadmap/S<n>/<k>_files.txt   merge: <path or none>
open: <one line each>
```
