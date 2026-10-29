# S4.2 notes: the audio core (module audio_core)

Status: DONE (2026-10-24). Merge request: log/roadmap/S4/2_merge.md. Uncertainties U-1410..U-1417: log/roadmap/S4/2_uncertain.tsv.
Files: log/roadmap/S4/2_files.txt.

## What was built

| file | what |
|---|---|
| `re/tools/soundstate_gen.py` -> `port/include/portsim/sim/soundstate.h` | GENERATED: `portsim::SoundState` = sound.dll .data (rva 0x63000, 0x54098 B; offsets static_asserted), members from re/names/sound_*_globals.tsv (duplicate names suffixed with their offset, unsized types one element: U-1410), `snd_rva::` constants of the named .rdata globals (vtables, g_float1, GUIDs), `kSoundFunctions[]` (744 hooks.csv sound rows: rva, name, `collides` for the 5 CRT names other modules also use), the .rdata hash. `--check` = stale test. |
| `port/include/portsim/sim/audio.h` | the API S4.3..S4.9 write against (header comment = how to write a function): `audio_attach / detach / release`, `snd_base / snd_va / snd_data_va / snd_obj / snd_arr`, `ld / st`, the dispatcher `snd_call / snd_vcall / snd_call_va / snd_import`, `snd_new / snd_delete`, `snd_crt::` rvas, `SndCallC` (the C ABI struct), `SoundHost`, claims, counters, detail. |
| `port/include/portsim/sim/audio_sink.h` | `AudioSink` (import / com commands), `AudioCommand`, `audio_sink_object`, `RecordingSink`. |
| `port/src/sim/audio_core.cpp` | part 1 the core: per-Ctx side table (commands.cpp pattern), PE mapping from GameFiles with relocation and identity checks, dispatcher, CallbackHost, ScriptHost (transcript format in the file), sink route, config entries `audio_core.*`; part 2 the 60 S4.2 functions + the CRT strncpy served by rva. |
| `port/test/sound_replay.py` | ORACLE / DEFAULT replay, negctl runner, bundles, goldens, the Win32 path (`load_w32`: Lib32 over the mirror's runner). |
| `port/test/test_audio_core.py` | 271 tests (128 need SG_EMU_ORACLE=1). |
| `port/test/golden_audio_core.json` | emulator digests (68 keys, 1,219 vectors), tiers, regions. Outside files_owned: listed in the merge request. |
| `re/tools/snd_hook2port.py` | the converter (below). |
| `log/roadmap/S4/2_mirror.py`, `2_cabi.h.txt`, `2_cabi.cpp.txt` | the mirror build (HEAD + own files + C ABI patch); `--w32`, `--base` (HEAD only, reference), `--extra <module.cpp>` (a test-only module, e.g. the converter's output). |

## Design (as built)

- The address space is the emulator's: every page the VM / Unicorn holds (except the emulator's 1 MB stack, U-1413) is mapped
  into the port context as contiguous runs (sg_map; the image's headers/.text/.rdata read-only), the GameState at a base no
  page overlaps (0x20000000 in all three layouts). Port and original see the same bytes at the same addresses.
- Calls (audio.h): claimed VA -> host (Claimed); in image -> E9 thunks followed (claims checked per hop) -> registered port
  function by rva (kSoundFunctions name, or the CRT table: `audio_core.crt.strncpy` for rva 0x42710) -> else host (Fallback);
  outside -> host (External), or the AudioSink when a sink and no host are attached (sink trap range, IAT by name).
- Live host (oracle): Python syncs port pages -> emulator, `Machine.native_call(va, abi, args)` (traps, redirects and fallback
  code all run there), syncs back (new pages mapped), returns eax / edx / st0. In layout A it records the transcript entry
  (request, reply, byte-level host writes, new pages) for the bundle.
- Script host (default / Win32): the transcript of the vector from "$audio/script"; each port request must equal the next
  entry (kind, va, abi, nargs, want_st0, every slot) or the call stops with the entry index in the detail; the report
  (`audio_core.report` -> "$audio/out") gives counters, script position and detail. U-1412 is the limit of this mode.
- One config call per vector (`audio_core.setup base flags`): the Win32 runner copies every writable block per sg_call
  (Win32 replay of all 68 keys: 2 min).

## Gate numbers (2026-10-24)

- ORACLE (`sound_replay oracle`, log/roadmap/S4/2_oracle_update.tsv / .log): 68 keys (64 registry keys of the 60 rows + 4
  derived keys of the 3 emu-only rows), 1,219 vectors; port == emulated original on the return text and every region byte in
  layout A (1,219 / 1,219) and in B and C (2,438 / 2,438); 0 errors; 0 fallbacks. Emulated original vs CSV (S4.1 compare(),
  rerun per key): EXACT 8, EQUAL_PI 55, EQUAL_PI_DISTINCT_DIFF 1 (cmdNode::scalarDeletingDtor, U-1414), EMU_ONLY 4.
- Negative controls (`sound_replay negctl`, log/roadmap/S4/2_negctl_r1.tsv; again live in the SG_EMU_ORACLE pytest run): mode 1
  of all 60 functions RED (778 of 1,219 vectors differ), 0 blind; plus audio_core.crt.strncpy mode 1 RED (27 / 108).
- DEFAULT replay from bundles against the goldens: x64 68 / 68 keys PASS (6 s), Win32 68 / 68 (2 min) (2_replay_x64.log,
  2_replay_w32.log).
- pytest: default 143 passed / 128 skipped (2_pytest_default.log); SG_EMU_ORACLE=1 271 passed (2_pytest_oracle.log, 5 min).
- Segment gate: 205 / 205 PASS x64 on the mirror and on a HEAD build (`2_mirror.py --base`), `integrator compare` 0 digests
  differ; Win32 mirror 205 / 205, 0 differ from the x64 HEAD digests (2_segments_*.tsv / .log).
- Boot gate on the mirror: 61 PASS / 2 KNOWN / 1 NO_FRAMES / 1 FAIL (cov_r_bio, sg_run_boot -11), row for row equal to
  log/roadmap/S1/5_boot_gate_base.log (2_boot_gate_mirror.log).
- test_static: only test_every_module_file_is_listed fails (audio_core's modules.inc line is the merge request); the address
  test passes on every new file.

## The converter (re/tools/snd_hook2port.py)

`--rva 0x... | --step S4.k | --file f --detour d`, `--out`, `--tu` (compilable TU), `--compile` (cl /c /W4 against the port
headers), `--module <name> --prefix conv:` (a test-only port module). Accessor ROLES are found by body shape (sound / soundva /
base / fn / imp / g / gref / vslot / slot / fnptr), so c3cn's SoundVA / SoundNew() style and the c3xx_sound / c3xx_fn style both
convert; memory accessors -> ld / st (compound assignments too), `&acc<T>(b, o)` -> address (binary `&` / `&&` kept), typed
pointer indexing -> element loads / stores, function pointers read from memory -> snd_call_va, bits of a local -> `pun` /
`pun_store` (memcpy), file constants (kIat..., kG...) declared once, helpers converted and emitted once per output, VAs in
comments -> RVAs. Evidence: `--step S4.2 --tu --compile` 0 errors 0 warnings, 0 TODO; its unedited output as module audio_conv
(`2_mirror.py --extra log/roadmap/S4/2_conv/audio_conv.cpp`) passes all 64 keys of the 57 hook-sourced functions from the
bundles (`sound_replay replay --prefix conv:` with that DLL, log/roadmap/S4/2_conv_replay.log; the 4 failures there are the
derived emu-only keys, which have no hook source). Not covered (marked TODO(convert) when met): x87 helper calls, inline asm,
`static` locals, reference bindings to memory, arrays of host pointers. Signed `ld<int32_t>(...) - 1` is emitted as written;
the port rule (arith.h for signed overflow) is the worker's edit.

## Reproduce

```
~/bin/py -3.12 re/tools/soundstate_gen.py --check
~/bin/py -3.12 log/roadmap/S4/2_mirror.py --w32                      # x64 + Win32 mirror DLLs and runner (~4 min first time)
~/bin/py -3.12 -m pytest port/test/test_audio_core.py                # default (bundles + goldens), x64 + Win32
SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_core.py
~/bin/py -3.12 -m port.test.sound_replay oracle [--update] [--keys k ...] --out log/roadmap/S4/2_oracle.tsv
~/bin/py -3.12 -m port.test.sound_replay negctl --out log/roadmap/S4/2_negctl.tsv
~/bin/py -3.12 -m port.test.sound_replay replay [--w32] [--prefix conv:]
~/bin/py -3.12 re/tools/snd_hook2port.py --step S4.2 --module audio_conv --compile --out log/roadmap/S4/2_conv/audio_conv.cpp
~/bin/py -3.12 log/roadmap/S4/2_mirror.py --extra log/roadmap/S4/2_conv/audio_conv.cpp
~/bin/py -3.12 -m port.test.integrator segments --dll port/build/S4_mirror/x64/Release/portsim_c.dll --jobs 3 --out <tsv>
```

## For the next S4 workers

- Write against audio.h; convert first (`snd_hook2port.py --step S4.k --module <m> --compile`), then edit: replace the
  converter's hook names by port names, give every function a negctl mode, route every call through the dispatcher.
- Add your step's rows to sound_replay with `--step S4.k` (keys_for reads re/roadmap/S4_functions.tsv); emu-only rows need a
  DERIVED entry (a twin's or a same-signature key, by measured instruction identity).
- Register under hooks.csv names; a CRT function without a row goes in audio_crt_name() + a FuncDef `audio_core.crt.*` (S4.2
  owns audio_core.cpp: add yours in your own module and extend the lookup through a merge request, or ask the lead).
- x87: the oracle's host gives st0 through `fbits`; fpcw keys are S4.4's (Win32 authoritative).
- The fixture VM's per-key state starts fresh; the port context is fresh per key (LivePort) and per vector remapped.

## Open

- U-1411 heap without a stand-in (S4.10), U-1415 thunk survey, U-1416 sink addresses: decisions for S4.10.
- The live callback crosses no process boundary: Win32 runs only from bundles (script host). A Win32 live oracle would need a
  runner op; not needed by the gate.
