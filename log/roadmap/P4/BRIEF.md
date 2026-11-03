# P4: gameplay fixes behind defect switches (decision G30, Mariano, 2026-11-03)

The roadmap is done; the port now diverges from the original ON PURPOSE, one switch per fix. Read first:
- re/roadmap/DECISIONS_PENDING.md row G30 (scope and rules).
- re/port/defect_decisions.tsv: your rows (columns id, function, summary, class, switch_state, evidence).
- re/analysis/data/model_defects.md: the defect write-ups the rows cite.
- How a switch looks: port/include/portsim/registry.h `struct DefectDef`, and the `kDefects[]` table of port/src/sim/shots.cpp.
  A switch is an `int` flag, default 1 = the original's behaviour; 0 = the fix. Its name is `<function>.<camelCaseWhat>`.
  Existing examples of fixes at 0: log/roadmap/S6/ (placeHabitat.clusterLoopBound, loadGolfPro.negativeWindowBound).

## Rules
- Default 1 everywhere: with every switch at 1 the port must stay byte-identical to the original (profile `original` is the
  test oracle). Never change behaviour outside `if (!flag)` / `if (flag)` arms.
- Save files MAY change format (G30); prefer fixes that do not need it.
- Testing is LIGHT (G30): build x64 + Win32 must pass, and the segments gate under the default switches must stay 205 / 205 with
  0 digests changed. No per-fix proof tests are required; a short unit test per switch is welcome when it is cheap.
- One switch per defect row (two rows may share one when they are the same bug). Note every switch in your notes with the row
  id, the instructions it changes (addresses), and what the fix does, in plain words.
- NO-GUESSING (CLAUDE.md): cite addresses; no banned words.
- Work ONLY in your own tree port/build/<ID> (made for you; a `git archive` of HEAD plus junctions). Never edit the shared tree.
  Build with `~/bin/py -3.12 port/test/build.py --arch x64` (then `--arch Win32`) from your tree's root, with
  SG_FREETYPE_ROOT='C:\Users\maria\vcpkg\installed'. Point tests at your DLL with
  SG_PORTSIM_DLL=<tree>/port/build/x64/Release/portsim_c.dll.
- Heavy runs (build, a pytest file, the segments gate) take the shared lock:
  `~/bin/py -3.12 log/roadmap/S3/w8_lock.py run <ID> -- bash -c '<command>'`.
  Test runners need the slot: `test-slot bash -c '...'`.
- Segments gate: from your tree, `test-slot bash -c "~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out <file>"`;
  compare with log/roadmap/merges/I25/segs_x64.tsv (status and digest columns equal on every row).
- Never run `pytest port/test` as one process (memory); one file per process.

## Deliver (in log/roadmap/P4/<ID>_*)
- `<ID>_notes.md`: per switch: row id, name, sites, the fix, how checked.
- `<ID>_files.txt`: every file of your tree you changed or created (paths relative to the tree root).
- `<ID>_profile.tsv`: rows for re/port/play_profile.tsv, columns `switch value decision_id reason` (value 0).
- the build and segments logs.
Then reply DONE with the counts, or BLOCKED with the reason. Do not commit; the orchestrator merges.
