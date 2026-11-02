# S11 RE upkeep, status (lead session on claude3)

steps: 5 total | accepted 5 (wave 1 = S11.1-S11.3 queued as I23; wave 2 = S11.4 + S11.5 ready: MERGE_w2.md, S11.4 re-run after S3 wave 10)
live workers: none (all closed)
game slots held: 0
tracker at open (2026-11-01): C4 2,928, C3 5 (exe 4, sound 1), C1 11 (5-byte thunks), C2 2,277 (open 1,881, held 113, end states 283)
after the S11.1 merge notes: C4 2,930 (gameMain, sound strcpy), C3 3 held (fillSpanDown/Up pending U-2000, drawAmenitiesPanel U-2001)
after the marks: C2 open 1,881 -> 1,661 (end states 283 -> 503)
master: carries every re/names row except 4 IMPORTED `entry` rows (kept: loader symbol); 5 withdrawn labels removed (S11.2)
decision open: U-2000 -> G26 (Mariano); fillSpans held, CSVs parked. U-2001 held, no slot (orchestrator). U-1351 resolved in 1a.
last event: 2026-11-01 orchestrator accepted wave 2; merge order I22, I23 (wave 1), I24 (S3 wave 10), I25 (wave 2 re-run); lead idle until told wave 10 merged
uncertainty ids: S11.1 U-2000..09, S11.2 U-2010..19, S11.3 U-2020..29, S11.4 U-2030..39, lead U-2040..49

## Log
- 2026-11-01T17:48Z opened by G23; read LEAD_BRIEF, sections row, status --summary.
- C1 rows: 11 golf_clean.exe 5-byte jmp thunks; c2_note skips thunks, so they stay C1 by design (CLAUDE.md, cleanup 2026-10-09).
- S11.1 accepted (checked on disk: CSVs GREEN 41 / 40, check log PASS lines, no tracked file touched, 0 banned words). The lead
  moved the two fillSpan CSVs from log/diff to log/roadmap/S11/1_ev/ so no blanket pass promotes them before U-2000 is ruled
  (1_merge.md section 6). U-1351 resolution (S3's row) is a merge-note edit: the 18 call sites recorded from the binary.
- S11.2 accepted (2_diff_after.tsv 4 entry rows, control RED, backup present). Lead ruling on 2_merge.md item 4: the four
  `entry` rows stay (IMPORTED loader symbol; the CRT startup names are in smsg_*_lib.tsv); listed exception of the gate.
- S11.3 accepted (1,882 lines = 1,881 rows + header, 220 mark addresses in 3_merge.md, hand check 12 cases, 0 banned words,
  no tracked file touched). Its U-2020 (endstate.py misses PE entry points) is a shared-tool fix for later.
- Orchestrator rulings 2026-11-01: wave 1 queued as I23 after I22; U-2000 -> G26; U-2001 hold, no slot; U-1351 resolve as 1a;
  U-2020 -> S11.5; S11.4 now as anchored edit script on a copy of port/build/I22, heavy runs through log/roadmap/S3/w8_lock.py.
- S11.5 accepted after one revision (test independent of log/ and of the unpatched tool; end states from hooks.csv; stubbed TLS /
  export tests). Lead re-ran tests/test_endstate_entry.py: 10 passed on the patched copy and on a simulated merged tool.
- S11.4 accepted: 936 edits / 190 files + 2 new, shim comments only, gates on the I22 copy equal to I22 (lead compared segs / boot
  tables); the lead stopped its detached suite (30-40 min lock queue per file); golden re-pins (8 files) and full suites are B3 of MERGE_w2.
- Idle: on the orchestrator's message, run log/roadmap/S11/4_assemble.py --root <post-wave-10 tree> --check-only; regenerate with
  4_tools/gen.py --base <tree> if an anchor moved; then MERGE_w2 B1-B5 gates (golden re-pins B3).
