# S3.27 brief (wave 8 follow-up, group 2 / U-1952: the GL stream of the map-corner render) — lead cmvu18cc81fcpp715vy6fd2dv

You are a worker of the S3 section (in-game frame) of the SimGolf port roadmap. Read the repo's CLAUDE.md (SimGolf) first:
NO-GUESSING, the banned words (probably, likely, seems to, appears to, I think, presumably, might be, maybe), cite addresses,
process hygiene (track and kill only the PIDs you spawn, never by name; Frida temp; a hung Frida spawn: kill your own PID and retry,
U-0683). Then: log/roadmap/S3/26_notes.md section "Group 2" and 26_uncertain.tsv U-1952, log/roadmap/S3/25_notes.md (S3.25's
recordings on the same instance, its terrain recording log/p5/terrain/s325_cov_r_s2_screens), log/roadmap/S3/1_notes.md (S3.1's GL
recorder: re/tools/gl_record.py --scenario, --inject; port/test/glcorpus.py export), log/roadmap/S3/2_notes.md (terrain_calls.py
--pass-frames), log/roadmap/S3/W8_HEAVY_RULE.md (the lock).

## Task (a recording and a reading, NO port code)

cov_r_s2_screens course / radial_hover and the six report rows differ in the map-corner box 781..799 x 451..466: the original's terrain
draws a near-black dither over the map's right edge the port's draw list does not hold, although the port's Terrain calls (20,204) and
all 32 Terrain states equal the original's (U-1952). Find which GL pass and which primitives draw it:
1. Record the GL stream of the original's draws on cov_r_s2_screens, at least the full render at Terrain call seq 19931 and every
   localRender / strip pass between it and the `course` checkpoint, with the root before / after each pass (S3.1 / S3.2 tools; extend
   them only in new files of your own if they lack a selector for a call seq).
2. Compare those passes with the port's draw list and stream for the same calls (port.test.terrainlive / glcorpus tools, read-only use)
   and name the primitive(s) whose pixels land in the box: which pass, which tile / face / draw call, its vertices and colour, and why
   the port's list lacks it (a Terrain.dll path the port does not model, an order difference, ...), citing Terrain.dll addresses.
3. Write the finding: 27_notes.md, 27_uncertain.tsv (U-1952 settled or narrowed; new ids U-1955..U-1959), 27_files.txt. If the cause is
   found, describe the fix for wave 9 (files, rule, expected rows) but do not edit port/.

## Rules

- Game slot GRANTED (orchestrator, 2026-10-31) on instance `instances\s3_w8` only (it exists; S3.25 used it). Every recording must
  reproduce its pinned reference: `re/tools/scenario_refs.py check --scenario cov_r_s2_screens --run <run>` SAME as det_b.
- Every boot and every heavy run (build, pytest, framegate) only through
  `~/bin/py -3.12 log/roadmap/S3/w8_lock.py run S3.27 --what "<run>" -- <command>`, one boot per hold. The lead's census also queues there.
- No port code, no shared file edits, never write original\, no game bytes (frames, images, streams) in tracked paths: recordings go to
  port/corpus/ or log/p5/ (gitignored), images to log/roadmap/S3/27_img/ (listed as local).
- claude2: bare `cmake` is not allowed; use `~/bin/py`. pytest one file per process.
- Time box 6 h. Report to the lead (send_to_session cmvu18cc81fcpp715vy6fd2dv) only DONE / PARTIAL / BLOCKED, in this shape:
  `S3.27 DONE|PARTIAL|BLOCKED: <one line>` then gate (runs SAME), finding, notes / files / open, short.
