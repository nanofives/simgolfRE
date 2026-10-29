# S4.1 notes: replay tooling for sound.dll (fixture VM, emulated original, census)

Status: DONE (2026-10-24). Full write-up: re/analysis/data/sound_replay.md. Uncertainties U-1400..U-1409:
log/roadmap/S4/1_uncertain.tsv. No port code.

## What was built
- re/tools/fixvm/fixvm.js, frida.js, mem.js, syslibs.js: the fixture VM. One Node process, a fresh vm context per key with
  the mock Frida API; diff_hook.py's script parts load unchanged (the real heap / import / mmio / file stand-ins, recdisplay,
  uitree, modal, framemask, all enabled fragments); `__freeze` is the VM's own (one thread). Fake system DLLs with
  deterministic Win32 bodies for the CRT; locale / code-page calls answered by this PC's Windows (host calls); anything else
  raises `unmodelled import X` (the missing API the census names). The shim's test exports C3foX87Probe (hand-assembled from
  c3fo.cpp), C3foSehCall5 and C3fzaSehCall4 are provided.
- port/test/sound_emu.py: relocated image, Machine (Unicorn; traps at 0x7A000000-0x7C000000, redirect hooks, fstp return
  stubs, instruction-count budget instead of Unicorn's timeout, which cost 15 ms per start), FixVM (the link: u32 length +
  JSON frames, nested requests both ways, dirty pages on every message), KeyRun (diff_hook.js for one key, DllMain first),
  Frida-exact formatting (two double eras), CSV reader, `second_arm` hook for S4.2.
- re/tools/sound_census.py: all 980 keys, three layouts, worker processes with a per-key watchdog; re/roadmap/S4_census.tsv
  and log/roadmap/S4/1_census/<key>.json.
- re/tools/fixvm/controls.py, x87_cw_measure.py, x87_probe_unicorn.py; tests/test_fixvm.py.

## Gate numbers
(a) 980 / 980 keys have a census row; 978 fixtures rebuilt, 2 wait for the live engine (named). Tiers: EXACT 181,
EQUAL_PI 785, EQUAL_PI_DISTINCT_DIFF 2, DIFF 5, PARTIAL_ERR 5, FIXTURE_ERR 2. Vectors 15,498: returns compared 13,257 / equal
13,247 / run address 2,196; state compared 4,194 / equal 3,974 (+210 equal modulo the x87 flag byte) / run address 11,007;
state_changed 15,201 / 15,201; errors 21 (RaiseException, timeGetTime). The 10 + 10 differences are __ftol's empty-st(0) rows.
(b) one key per style, every vector equal: pure memory Channel_setPanGains, globals codecProcessBlock, NativeCallback vtables
Midi_Device::setChannelVolume, heap Channel_setFormat_oom, import Midi_Device::flushMessages_h, replace Channel_start_null,
frozen thread Channel_setRateGlide (all EXACT); mmio parseRiffWave (EQUAL_PI: no mmio key is EXACT, their state holds the
stand-in's absolute pointers).
(c) controls 10 / 10 RED (flip 3, wrong relocation base 3, stub +1 4); 2 stub controls BLIND recorded (28 of the first 40
EXACT import / replace keys are blind to the stub control: recorder callbacks whose return is discarded).
(d) Unicorn honours precision control; sets no x87 exception flag; fistp of an empty register stores 0. c3fh caller-word keys:
DsBuffer::setVolume and Seq_setTempo match at their word and go RED with the word swapped (4 keys SENSITIVE); 43 BLIND through
the CSV, 28 of them change in the emulator with the word.

## Reproduce
```
~/bin/py -3.12 re/tools/sound_census.py --jobs 8          # 447 s, log/roadmap/S4/1_census_final.log
~/bin/py -3.12 re/tools/fixvm/controls.py                  # log/roadmap/S4/1_controls.tsv
~/bin/py -3.12 re/tools/fixvm/x87_cw_measure.py            # log/roadmap/S4/1_x87_cw.tsv
~/bin/py -3.12 re/tools/fixvm/x87_probe_unicorn.py
~/bin/py -3.12 -m pytest tests/test_fixvm.py               # 16 passed, ~35 s (log/roadmap/S4/1_pytest.log)
```

## Findings worth keeping
- Live TLS index of sound.dll's CRT: 27 (hash search over 0..1099 on _errno_a; three more keys confirm).
- Frida's double printing changed between 2026-10-07 (shortest) and 2026-10-14 (17 truncated digits).
- c3fu's fixtures resolve `$name` through getters that write memory: vectors must be resolved in the VM, per vector, before
  the snapshot (diff_hook.js order). c3fo replaces NativeFunction: arms must be built by the VM after the fixture.
- c3fq's converter NativeCallback threw in the live run too (number.toUInt32); the key matches with the call returning 0.
- Only 5 keys depend on DllMain (`--no-init` census).

## Open
- SEH dispatch (RaiseException / RtlUnwind) not modelled: 4 keys partly (CRT EH helpers).
- U-1403 (two destructors whose live free log is empty).
- Live-address recovery from pointer returns could make more run-address rows comparable (sound_replay.md section 7).
- For S4.2: `KeyRun(second_arm=...)` is tested with the original as second arm (match) and an off-by-one body (RED);
  a port executor still needs the address-space copy into its context and the trap forwarding described in section 6.
