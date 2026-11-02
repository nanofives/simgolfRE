S4 DONE and CLOSED (2026-10-29): merged by I14 as ee6efc1d (log/roadmap/merges/I14.md); build dirs S4r2 / S4r / S4rx deleted
S4 status (lead: cmvsixi81044xp727qsgerjz6, fourth lead from 2026-10-28 on claude2, continuing after I11's refusal; earlier leads claude2, claude3, claude2 cmvphl14)
steps: 15 planned, 14 accepted, 1 held (S4.10g: U-1421, codec only on the network voice path), 0 running
live workers: none (S4.12 closed 2026-10-29)
game slots held: none
function map: re/roadmap/S4_functions.tsv (744 sound.dll rows: 573 C3/C4 + 31 C2 emu-only to S4.2..S4.9; 71 CRT and 69 dead not ported)
decision G1 answered 2026-10-24 by Mariano: proposed reading accepted (DECISIONS_PENDING.md Answered); written into S4_steps.tsv gates
last event: 2026-10-29 I14 MERGED (ee6efc1d); lead deleted port/build/S4r2, S4r, S4rx (junctions unlinked first, no process used them); S4 CLOSED
next: none (orchestrator runs 11_merge.md section 6 tracker steps; S4.10g / U-1421 stays held for D6 network voice)
uncertainty ids: S4.k gets U-14(k-1)0..U-14(k-1)9

## G1, the gate question (sent to the orchestrator)

The path-1 CSVs hold only an FNV-1a hash of each compared region's raw bytes (re/frida/js/diff_hook.js, rows at line 76).
Every one of the 19,785 state regions of the 980 sound.dll keys is a fixture pointer ("$name"), and the regions hold the
run's own addresses: Memory.alloc / heap stand-in blocks, list links, vtable pointers into a sound.dll that was relocated
(ImageBase 0x10000000 is taken by jgld.dll; DllCharacteristics 0, 5,816 relocations). Neither the blocks' addresses nor the
load base were recorded, so the literal replay of test_bridge.py (rebuild the fixture, hash the regions) cannot reproduce a
hash whose region holds such an address. test_bridge.py worked because its exe regions sit at fixed addresses and hold no
pointers.

Proposed reading of the gate: per key, the fixture is replayed into a virtual address space (Node runs the fixture JS
over a mock Frida API), the ORIGINAL sound.dll function runs in Unicorn on it (the user's file, relocated to the chosen
base; the 3.1a / 3.7b emulator-oracle pattern), and the port runs on the same space; port and emulated original must agree
on the return and every compared region byte for byte. The emulated original is tied to the live evidence on every field
of the CSV that does not depend on run addresses (scalar return, state_changed, distinct_results, and the hash itself
where the region holds no run address). Default test runs check golden digests pinned from the emulator (3.1a pattern).
S4.1's census gives the exact count of rows each tier covers.

## Event log
- 2026-10-24 survey: 980 sound.dll keys in 33 registry.d batches, 779 fixtures; 20 of 33 fixture files use live-process
  machinery (heap / import / mmio stand-ins, freeze, Interceptor.replace); Node v24.20.0 and unicorn 2.1.4 present, no
  Python JS engine.
- 2026-10-24 g_soundReady (0x83afc4) = 0 in all 205 segment start images: every segment runs the exe's device-less path.
- 2026-10-24 S4.1 accepted: census EXACT 181, EQUAL_PI 785, DISTINCT_DIFF 2, DIFF 5 (__ftol_snd, Unicorn x87 flags), PARTIAL_ERR 5, FIXTURE_ERR 2; controls 10 RED, 2 blind; U-1400..U-1409 in 1_uncertain.tsv
- 2026-10-24 S4.2 accepted; files added to files.txt; golden_audio_core.json is S4.2-owned (row updated)
- 2026-10-24 wave 1 (S4.1+S4.2) MERGE.md + files.txt written at the orchestrator's request; lead re-check on HEAD 4bfefc2a green; sound_replay.py got SG_S4_MIRROR (lead fix)
- 2026-10-24 S4.6 accepted; wave 2 so far: S4.6 (6_merge.md: SG_MODULE(audio_chan), U-1450..U-1455; snd_frame request for S4.10). Rule taken from S4.6: a vector where the emulated original and the port differ only in the quiet bit of a NaN copied through fld/fst is forgiven (Unicorn keeps sNaN signaling; the CPU quiets it), U-1451, 17 vectors in 7 keys
- 2026-10-24 S4.8 accepted (8_merge.md: SG_MODULE(audio_ds), SG_MODULE(audio_dev), audio_dev_release in sg_state_free, U-1470..U-1479; 222 dev fallbacks to S4.7 + CRT _atexit, U-1477). S4.6 and S4.8 solved stack locals two ways: step S4.2b added to consolidate (held until wave 1 is committed, because it edits audio.h / audio_core.cpp / sound_replay.py)
- 2026-10-24 S4.4 accepted (4_merge.md; U-1430..U-1437; Mixer_ctor GREEN_PENDING on 36 fallbacks to S4.7). A third stack mechanism (x87::frame_push, block 0x6E000000) folded into S4.2b scope
- 2026-10-24 S4.5 accepted (5_merge.md; U-1440..U-1449; 472 layout-A fallbacks, all to S4.7/S4.8/S4.9 rows: its mirror predates S4.8)
- 2026-10-24 S4.7 accepted (7_merge.md: SG_MODULE(audio_sound); U-1460..U-1465). S6 merged as 32c11a36; S4 wave 1 still queued
- 2026-10-24 S4.3 accepted (3_merge.md: SG_MODULE(audio_codec), after 2_ and 8_merge.md; U-1420..U-1425; U-1421 caller FPCW carried to S4.10). Wave 1 being merged by integrator I3 in a mirror of c6d7d78b
- 2026-10-24 wave 1 refused (I3: test_vectors / test_win32 ran exe NodeList::clear vectors on sound.dll NodeList_clear); all other gates held. S4.2b: snd.<name> registration rule, static collision test, may now write wave-1 files in the tree; MERGE.md wave 1 superseded
- 2026-10-24 S4.9 accepted for 100 of 101 rows (9_merge.md: SG_MODULE(audio_wave), U-1480..U-1489); S4.9b loadSoundFile spawned (ids U-1490..U-1494; S4.10 keeps U-1495..U-1499). One transient x64 segment failure course_season/04 (missing: text font file) passed alone; watched in S4.2b runs
- 2026-10-24 S4.9b accepted (9b_merge.md, U-1490..U-1494; its mirror and sound_replay snapshot in 9b_snap/ predate S4.2b). S4.11 added: close every development fallback (CRT helpers, 4 unnamed callees); needs ids beyond U-1499
- 2026-10-24 S4.11 ids U-1540..U-1549 granted by the orchestrator
- 2026-10-24 lead and all children moved from claude2 to claude3 (claude2 at 100%); no new children on claude2. Cap 2 workers: S4.11 asked to stop and write notes
- 2026-10-24 S4.11 closed for the cap (no 11_notes.md written; restart from its transcript); G7: new workers on claude3
- 2026-10-25 S4.10 accepted (10_merge.md: must merge with 2_ + 2b_ (kDrops removal needs tick_boot arrayStaticInit_448160 + loadSoundTable); audio_wave arity fixes section 4; U-1495..U-1499; U-1421 NEED_GAME)
- 2026-10-25 S4.10g held: codec never entered in 3 live device runs (all 319 install sounds PCM, waveInGetNumDevs 0; openMmioSound / loadWaveResource are the network voice path); slot released, worker closed
- 2026-10-25 S4.11 accepted (11_merge.md, U-1540..U-1544; U-1493 resolved by re/names/sound_19.tsv). Open for the wave: audio_exe.cpp 3 address literals + 74 snd.FUN_ names (S4.2b)
- 2026-10-25 S4.2b accepted (1,087 keys 0 fallbacks, 12 goldens equal, suite 70 files, static 630\/2, segments 205\/205, boot 0 baseline rows changed); MERGE.md rewritten for the resubmitted wave; S4 DONE reported
- 2026-10-26 I8 REFUSED the wave at the Win32 suite (test_buyland golden digests, 0..81 of 232 differ). Lead fixes: test_buyland sets audio_exe.stubRequests; audio_release calls audio_exe_release (ExeCtx + DeviceSink erased at sg_state_free); 2b_apply_shared.py reads wave-1 sections from log/roadmap/S4/wave1/, keeps modules.inc CRLF, inserts MERGE.md's wave-2 paragraph, --prove = edit_equiv vs S4_2b_full (all SAME on 78ffb06d). Mirror port/build/S4r: test_buyland Win32 6/6 pass, x64 pass; probe without stub 81/232 in 4/4 Win32 runs and on x64 (deterministic), with stub 0

- 2026-10-27 resubmission proof complete (MERGE.md "Resubmission proof"): static 630/2, x64 suite 81/81 = I8, Win32 suite = baseline's 20 known, audio files whole on Win32 PASS, segments 205/205 x3, boot 66/2/2 x3 (Win32 on re-run), chains 342, census 698 = I8, terrain 527/0/75, G1 oracle 1,087/16,859 GREEN, exe 182, goldens EQUAL, replay 0 fail, controls as accepted, sink logs EQUAL, streams 205 with counters = requests. U-1545 (cov_r_flic_input Win32 flake, 2/42 under low free commit) open. Instrumented throwaway build port/build/S4rx kept for U-1545.- 2026-10-27 ORCH: resubmission queued as I11 after I10 (S7 merging); U-1545 noted (I11 re-runs a boot row that fails under low memory)

- 2026-10-28 I11 REFUSED at the apply step: S7 split portsim_c.cpp into two extern "C" blocks. Lead: 2b_apply_shared.py's i3_script() now relaxes I3's check to the tail `#endif / } / blank / }  // extern "C"` (sg_build_info's end, the file's last block) occurring once, so S4's C ABI block lands after sg_build_info as on 78ffb06d and b3d61e41; --prove on 01d84188: all 8 files SAME. port/build/I11 deleted (junctions unlinked first). S4.12 spawned.
- 2026-10-28 ORCH: HEAD is e179bd24 (gfx_terrain.cpp = I13's; 01d84188 did not build from a clean archive, C2039). S4.12 told to discard its 01d84188 mirror and rebuild S4r2 on e179bd24, --prove and edit_equiv re-run there.
- 2026-10-28 S4.12 had already built S4r2 on 01d84188 with port/build/I13's gfx_terrain.cpp. That file has the same content as e179bd24's (md5 without CR e88dfcab...), and e179bd24 changes nothing else under port/. So S4r2 = e179bd24 + S4 and is kept; the earlier instruction to discard it is withdrawn.

- 2026-10-29 S4.12 accepted (12_notes.md, resub2/*_compare.log checked by the lead): static 630/2; segments 205/205 x64 + Win32 + freetype, boot 66/2/2 on all three, live equivalence 68+2 and app replay 340/205/266 both backends, census 698 PASS 100 none lost, terrain 527, chains 342: 0 rows differ from I13 / I12; x64 suite 95 files 94 PASS (test_fttext U-1393); Win32 20 failed ids all known, 0 new; G1 1,087 / 16,859 GREEN 0 fallbacks, goldens EQUAL, controls 667/669 RED (2 blind as accepted). U-1546 resolved by e179bd24 (kept in 12_uncertain.tsv). MERGE.md: I11 block, --prove sentence, proof and command block replaced (old text in resub2/MERGE.old_proof_78ffb06d.md); --prove re-run by the lead on 6e6df6e7 SAME 8/8.
- 2026-10-29 ORCH: S4 DONE received; I14 (cmvtcrf3) merging on 6e6df6e7. Lead idle until its verdict.
- 2026-10-29 I14 MERGED as ee6efc1d (suite numbers reused under G11: I14's mirror byte-identical to S4r2). Lead deleted port/build/S4r2, S4r, S4rx after checking no process used them; junctions unlinked first, original/ log/ port/corpus/ tools/vc6 intact. S4 CLOSED.