# S4.10: the exe's sound layer on the port's sound.dll engine (module audio_exe)

Status: DONE. Worked on claude2, moved to claude3 mid-step (claude2 at 100% usage); no children.

## What was built

- **port/src/sim/audio_exe.cpp, port/include/portsim/sim/audio_exe.h**: module `audio_exe`, 199 registered functions:
  - all 52 C3 / C4 backend-audio rows of re/port/scope.tsv except the three other modules own (playHabitatSound: tick,
    playSoundAt: bridge, playAudioFile: video);
  - all 53 C2 rows (forwarders, getters / setters, the `returnB*` stubs, the exit destructors,
    NodeList::playChildSuspendingSounds);
  - the helpers they reach (SndGroup_isDisabled / isMuted, HashTable::remove, resourceQuery / Load / LoadB,
    releaseWithHookA/B/C, closeHookA/B, showMessageBox, arrayStaticInit_448160);
  - 74 sound.dll functions with no hooks.csv row, registered as `snd.FUN_<va>` (U-1498);
  - the test / host entries `audio_exe.setup`, `.useSink`, `.streamLog`, `.report`, `.mapImage`, `.deviceFixture`,
    `.deviceStep`.

  Written from the hook sources and the disassembly (log/roadmap/S4/10_dis.py, 10_snd_dis.py), against the consolidated
  core API of 2b_notes.md.
- **Routing.** `exe_call_va` sends a claimed VA to the host, an exe VA to the registered port function by hooks.csv name,
  and anything else to the replay host (counted as a fallback) or MissingResource. sound.dll VAs go to snd_call_va.
  `exe_import` maps LoadLibraryA ".\sound.dll" to audio_attach (preferred base, image from GameFiles) and GetProcAddress
  to the export table by ordinal. timeGetTime comes from the InputSource clock at the frame's call sites. Every other
  import goes to the AudioSink, named from the exe's import directory.
- **The exe image.** `exe_map_image` maps the user's golf_clean.exe read-only (headers, .text, .rdata; SizeOfImage
  0x44414e, .data checked against the GameState). loadSoundTable reads its 219 groups from the mapped .text
  (`push mode; push path; mov ecx, slot; call Snd::applyMode484e30`), so the table stays in the user's exe.
- **The heap.** The exe's static CRT heap goes through the replay host's heap in replay; without a host it comes from a
  1 MB arena at 0xFE000000 (U-1495).
- **The DeviceSink (gate c), a production AudioSink.** The heap comes from 16 MB chunks at 0xF4000000..0xFC000000, with
  freed blocks reused by size. It also answers sync, TLS, version, the virtual clock and timeSetEvent; mmio over GameFiles,
  with the MEM-file semantics of mmiostandin.js; DirectSound and ole32; midiOut. The device-less fallback for waveIn
  reports no device.
- **port/test/audio_stream.py**, the runner. The `oracle` command runs the G1 oracle: the port is a second arm of
  sound_emu's KeyRun on the emulator's memory, in layouts A / B / C, tied to the path-1 CSVs by sound_census. It writes
  bundles to port/corpus/audio_exe and goldens to port/test/golden_audio_exe.json. The other commands are `replay`, `stream`
  (gate b over every accepted segment) and `device` (gate c, with `--update` pinning `$device`). It also holds the 57
  custom `s410:` keys over log/roadmap/S4/10_fixtures.js for the rows with no registry key.
- **port/test/test_audio_exe.py**, the tests. Static checks; replay of all 182 pinned keys; 108 per-function negative
  controls; 2 segment streams; the device run and 6 device controls. With `SG_EMU_ORACLE=1` it adds 5 live-oracle keys.
- **re/analysis/data/audio_stream.md**: the stream format and both gates' numbers.

## Gates (final build port/build/S4_10_mirror/x64_all = 10_mirror.py --modules all --tag _all)

- **(a) Oracle, G1** (log/roadmap/S4/10_oracle_final.tsv, 10_oracle_new.tsv, 10_oracle_new2.tsv): 182 keys, 1,861
  vectors, all GREEN against the emulated original in three layouts.
  - Tiers: 50 EXACT (the literal path-1 hash, regions without run addresses), 75 EQUAL_PI (equal on the
    address-independent fields), 57 EMU_ONLY (custom keys with no CSV).
  - 125 of 125 registry keys tie to their CSVs: the 122 backend-audio keys plus arrayStaticInit_448160's 3.
  - 4 keys are GREEN_PENDING on callees other modules own: return0_479820 and the engine-glue NodeList / B / C
    destructors (10_merge.md section 5).
  - The exe CRT allocator is host-served by design and is not counted as pending.
- **(b) Segment streams** (10_streams_x64.tsv, 10_streams_w32.tsv): 205 / 205 PASS on x64 and on Win32. Every digest
  equals log/roadmap/S0/1_segs_x64.tsv, and the x64 and Win32 streams are byte-identical in all 205.
  - 4,964 requests in 169 segments (playSound 4,937, startSoundSlot 16, Snd::status 11) over 4,599 frames, all matched
    to a frame record.
  - 0 other commands (g_soundReady = 0 in every start image).
- **(c) Device present** (`python -m port.test.audio_stream device`, sandbox_basic/00 start image):
  - initSound 0, loadSoundTable 0 (219 allocations), 178 playSound (every slot whose file the install holds, U-1499),
    40 timer ticks, shutdownSoundSystem 0;
  - 181 requests, 0 host fallbacks;
  - the IDirectSoundBuffer Unlocks carry non-silent rendered samples; the stream digest is pinned (`$device`, 13,020
    lines).
- **Boot gate** (10_boot_x64.tsv, with the BOOT_EDITS of 10_merge.md 2b): the 67 rows of
  log/roadmap/merges/S1S8-4/boot_x64.tsv are unchanged (64 PASS, 2 KNOWN, 1 NO_FRAMES). The three extra rows are other
  steps' uncommitted recordings. cov_r_s2_screens fails at record 771 on the cursor tile with port/build/x64 too.
- **Tests:**
  - test_audio_exe.py: 302 passed, 5 skipped (the opt-in oracle). With SG_EMU_ORACLE=1, `-k oracle` gives 5 passed.
  - Static suite (`pytest -m static`): 630 passed, 2 skipped (10_pytest_static.log).
- **Controls:**
  - 108 per-function replay controls, all RED. The C2 forwarders share one switch (g_neg_c2) and the snd.FUN_ functions
    another (g_neg_sndExtra).
  - 6 device-stream controls RED (log/roadmap/S4/10_device_negctl.log). Of 16 functions tried there, 10 leave the
    device stream unchanged: their wrong bodies sit on branches that run does not take. Each of the 10 reads RED on its
    own oracle keys.
  - NEGCTL_BLIND is empty.

Reproduce:
```
~/bin/py -3.12 log/roadmap/S4/10_mirror.py --modules all --tag _all            # [--w32] for the Win32 runner
~/bin/py -3.12 -m pytest port/test/test_audio_exe.py                             # own process
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_exe.py -k oracle
~/bin/py -3.12 -m port.test.audio_stream oracle [--keys k1,k2] [--update] --out <tsv>
~/bin/py -3.12 -m port.test.audio_stream stream --dll <portsim_c.dll> --jobs 3 --out <tsv>
~/bin/py -3.12 -m port.test.audio_stream device [--update] [--out <stream.txt>]
SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>
```

## Findings

- **tick.cpp's kDrops.** Removing playSound / startSoundSlot / Snd::status from kDrops is not enough. The boot starts from
  the exe image, where g_sounds has no class tables, so the boot must run arrayStaticInit_448160 and loadSoundTable
  (10_merge.md 2b). Otherwise every boot faults at record 0.
- **S4.9's audio_wave arity defects.**
  - Slot 0x40 of Wave_Device (FUN_1000e830, `mov eax, [ecx + 0x1ac]; ret`) takes no argument, so the caller's push becomes
    DsBuffer::setRegionMs's second argument. This affects seven sites with 0xe and 0x32fc0 with 8.
  - flushSoundList is cdecl with no arguments at five sites.
  - The fixes are in 10_merge.md section 4.
- **DeviceSink fixes the device run forced.**
  - Memory files: a streamed sound (desert3.wav) opens a MEM file; with an unbuffered model the renderer read pchBuffer
    NULL at tick 2.
  - Arena: the 16 MB arena ran out with about 85 streamed sounds playing; it is now chunked and reuses freed blocks.
- **The fixture VM** needed three bridges: binding `this`, TEB DeallocationStack, and callback addresses per layout
  (U-1497).
- **Oracle speed.** A byte loop in diff_writes made shutdownSoundSystem's keys run for hours (thousands of host calls over
  the 3.6 MB GameState). It now uses numpy with a page prefilter.
- **U-1499 resolved.** The CD's InstallShield header names the directory `Golf Sfx`; the extracted install renamed it
  `Golf_Sfx`.

## Decisions on carried rows (text for UNCERTAINTIES.md in 10_merge.md section 6)

- **U-1411 resolved.** sound.dll's heap reaches its KERNEL32 imports and the production sink serves them from an arena.
- **U-1415 resolved by survey** (10_thunk_survey.tsv). 1,207 thunks: 660 targets ported, 65 dead rows, 482 with no
  row. Of those 482, 156 are unreferenced, 316 have data references only and 10 have code references. None is reached by
  a key or by the device run.
- **U-1416 resolved.** The addresses are kept; the new arenas sit below them.
- **U-1494 resolved.** mmioOpenA names are served from GameFiles by the sink.
- **U-1421: NEED_GAME.** No fldcw on either codec path (10_fpcw_paths.txt). Which thread runs codecDecodeFrame
  (class-table slot 0x5c4dc) and codecEncodeFrame (sound thread / wave pump) is not settled statically.

## Open

- U-1495: the exe CRT heap has no port owner.
- U-1496: survey the call sites of the four null-path forwarders that return the caller's eax.
- U-1497: move the VM bridges into re/tools/fixvm.
- U-1498: name the 74 snd.FUN_ functions and give them oracle keys.
- Pending callees for other modules: return0_479820 and NodeList / B / C::dtor.
- initSound is not run by the boot (device-less effect already in the image).
- Win32 was not run for test_audio_exe.py (x64 only); the Win32 gate is the segment-stream run.
