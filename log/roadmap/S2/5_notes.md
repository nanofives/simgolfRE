# S2.5 notes: traces for cov_a_quit and cov_a_quit_title (U-1115)

## Result
- The c_a captures (log/p2/p24/<scen>/c_a, framehash.bin) already existed; only the traces were missing. The p24 era trace runs
  ended with the game exiting and wrote only a 147-byte `.trace.json` (rc 0, stats null). trace_rec.js has since gained the
  ExitProcess hook (END record, `trec_ack`), so a plain re-run works with no tool change.
- Old stale `.trace.json` files moved to log/roadmap/S2/5_old_*.trace.json, then:
  `SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/segment_corpus.py --instance s2_5 trace --scenario <s>`
  (instance s2_5 = instances\s2_5 with instances\p4q\winmm.dll, sha 381ed2b2). No re-pin.
- cov_a_quit: trace 757,481 B, 15 checkpoints, validate OK vs reference SAME. cov_a_quit_title: 200,369 B, 9 checkpoints, SAME.
- Boot gate (port.test.boot, S2_6 HEAD-mirror DLL): cov_a_quit PASS 301/301; cov_a_quit_title NO_FRAMES (the scenario never
  enters the frame loop: title menu then credits, so the capture has no frame record: correct by construction, not KNOWN).
- Framegate: cov_a_quit PASS 5 (menu, exit_leave, title1, exit_saved, title2), UNSUPPORTED 10 (Widget_writePixel in-game / credits);
  cov_a_quit_title PASS 2 (menu, hover_exit), UNSUPPORTED 7 (credits, playAudioFile has no body). No row NOT_RUN any more
  (24 checkpoints all run).
- Test: port/test/test_s2_5_traces.py (3 passed).

## Logs
log/roadmap/S2/5_trace_quit.log, 5_trace_quit_title.log, 5_boot.log, 5_framegate.log.

## Open
- cov_a_quit_title framegate/boot cannot be PASS beyond the title rows: credits needs showCreditsScreen drawing (Widget_writePixel,
  playAudioFile) in the gfx backend.
- New traces are local corpus (port/corpus gitignored); no merge text needed.
