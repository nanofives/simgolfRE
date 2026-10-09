# sound_10: a sequencer track helper (hand-named, 2026-10-09, batch c3cl)

`Track_setField8RaisingMax` 0x1002e430 (20 bytes, no callee, matched 100% in re/match/sound_raw_04.cpp) is the only
callee of `Seq_findTrackById` 0x10023080, reached through the incremental-link thunk 0x1000136b. Called on a track
element of the sequencer's list at Seq+0x218, it raises the dword at +0xc to its argument when that dword is strictly
below it (unsigned compare at 0x1002e437-0x1002e43b, so +0xc keeps the largest value ever passed) and stores the
argument at +8. The name is structural: what the two fields count is not determined by the body. Named so that
Seq_findTrackById's callee check has an identified callee (log/c3/c3cf_notes.txt section G).
