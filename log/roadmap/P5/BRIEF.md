# P5: Mariano's first play-test feedback (2026-11-03)

Mariano played the LAN build 70c2e89c (simgolf_port.exe, profile `play`) and sent nine items. Read first: CLAUDE.md's P4
paragraph, log/roadmap/P4/BRIEF.md (its Rules section applies here unchanged: own tree, default switches unchanged, light testing,
NO-GUESSING, w8_lock / test-slot for heavy runs, `~/bin/py -3.12`, no bare `cmake`, never `pytest port/test` in one process, do not
commit).

For every item, FIRST decide which of these it is, with evidence (addresses, a run of the original, the port's code):
- (a) a PORT BUG: the port differs from what the original does. Fix it so the port equals the original (no switch needed), and show
  how you checked (emulator oracle, a run of the original on an instance, a segment / boot / framegate row).
- (b) the ORIGINAL'S OWN BEHAVIOUR that Mariano wants changed: implement the change behind a new defect switch (default 1 = the
  original) added to re/port/play_profile.tsv's rows in your deliverable, or for app-side behaviour (window, input mapping, scaling)
  behind an app option whose default is the new behaviour.
- (c) not doable in this pass: say why and what it would take.
To see the original: re/tools/game.py / scenario tooling boots original\golf_clean.exe (use an instance: `re/tools/instances.py
create <name>`, SIMGOLF_ALLOW_MULTI=1, SIMGOLF_GAME_DIR=instances/<name>; never kill a process you did not start). Screenshots of the
port: the app's own record dir / framegate snapshots. Screenshots Mariano sent: log/roadmap/P5/fb1_*.png.

Deliver in log/roadmap/P5/<ID>_notes.md (per item: class a/b/c, cause with citations, what changed, how checked),
<ID>_files.txt, <ID>_profile.tsv (if any new play switch), build logs and the segments x64 TSV (must equal
log/roadmap/merges/I25/segs_x64.tsv on status and digest). Reply DONE / BLOCKED. Base tree: a `git archive` of HEAD (made for you).
