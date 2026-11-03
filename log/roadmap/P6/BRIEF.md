# P6: Mariano's second play-test feedback (2026-11-03)

Same rules as log/roadmap/P5/BRIEF.md (read it and log/roadmap/P4/BRIEF.md's Rules): own tree port/build/<ID> (a `git archive` of
HEAD f04c7591 plus junctions), default switches unchanged (segments x64 must equal log/roadmap/merges/I25/segs_x64.tsv on status and
digest), light testing, NO-GUESSING with addresses, w8_lock / test-slot for heavy runs, `~/bin/py -3.12`, no bare `cmake`, never
`pytest port/test` in one process, do not commit. For each item decide first: (a) port bug (fix to equal the original), (b) the
original's own behaviour Mariano wants changed (new switch in re/port/play_profile.tsv + port/app/main.cpp kPlayProfile, default 1 =
original; or an app option, default the new behaviour), (c) not doable now (say why and what it takes).

Deliver log/roadmap/P6/<ID>_notes.md, <ID>_files.txt, <ID>_profile.tsv (if any), build logs, segments TSV; reply DONE / BLOCKED.
Screenshot: log/roadmap/P6/fb2_gary_offmap.png (Gary Golf standing on black ground beyond the course edge, sandbox, build palette open).
