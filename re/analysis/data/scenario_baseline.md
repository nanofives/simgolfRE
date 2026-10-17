# Scenario baseline: what makes two runs comparable

Batch b2 (2026-10-17). The C4 scenario route (`re/frida/scenario_ab.py`, agent `re/frida/js/scenario_det.js`) compares a
hook set ON against an OFF arm at fixed checkpoints. A stored OFF arm (`det_b`) can stand in for a fresh one only when
both boots ran the same timeline. This page records why stored references stopped matching on LIVING-PC, the fix,
the checklist, and the procedure that regenerates and pins the references (`re/tools/scenario_refs.py`, manifest
`re/analysis/data/scenario_refs.json`).

## 1. Root cause of the boot desync: the audio output device

Symptom (c3fyk1, c3fyk2, c3fyk3, 2026-10-17): every stored `det_b` differed from a fresh run at the first checkpoint in
boot words only: seed_time `0x0051b380` = `0x00100001` stored against `0x00100000` fresh, the title ticker word
`0x00822c78` = `0xbb71` against `0xb81f`, and the RNG seed `0x00822d9c`; the virtual T, the main-thread timeGetTime count
and the QPC count at the checkpoint were equal.

Evidence:

- seed_time is T at seedFromTime `0x00467110` (`call [0x004ba21c]` at `0x00467110`, stored at `0x00467119`; RNG seed =
  t * 37 stored at `0x00467121`). T starts at T0 = `0x00100000` and goes up by 1 per main-thread timeGetTime call, so
  `0x00100001` means exactly one main-thread timeGetTime call before seedFromTime, `0x00100000` none.
- The clock log (`--clock-log`, section 5) of a fresh boot here: the first main-thread timeGetTime is seedFromTime's
  (`golf_clean.exe+0x67116`), after 7 QPC calls from `jgld.dll+0x6a414`.
- gameMain `0x0045baf0` calls initSound at `0x0045bd75` and seedFromTime at the next call, `0x0045bd7d`.
- Survey of the first checkpoint of every stored run under `log/c3/alt` (909 runs, `log/c3/b2_seedsurvey.py`,
  `log/c3/b2_soundtimeline.py`): seed_time takes only the two values, and it is `0x00100001` in every run whose sound
  timer thread was calling timeGetTime at the first checkpoint (`forwarded` > 0 in the run's status) and `0x00100000` in
  every run where it was not. No run has another value.
- This PC today: `waveOutGetNumDevs()` = 0; the only audio endpoint, `Philips FTV (NVIDIA High Definition Audio)`
  (the TV on HDMI), has status Unknown (`Get-PnpDevice -Class AudioEndpoint`). midiOutOpen of the Microsoft GS
  Wavetable Synth returns 1 (MMSYSERR_ERROR) and waveOutOpen returns 2 (MMSYSERR_BADDEVICEID).
- The device-less path, measured on the main thread (`log/c3/b2_soundprobe.js`): timeGetDevCaps (`sound.dll+0x8a14`)
  0; DirectSoundEnumerateA (`sound.dll+0x1153f`) 0, its callback (`sound.dll+0x17a3`) called once with guid NULL and
  the primary driver's description, returning 1; CoInitialize (`sound.dll+0x11ccb`) 0; CoCreateInstance of
  CLSID_DirectSound (`sound.dll+0x11d06`) S_OK; IDirectSound::Initialize (slot 10, `sound.dll+0x11d1d`)
  `0x88780078` DSERR_NODRIVER; Release (slot 2, `sound.dll+0x11a2a`) 0. g_soundReady `0x0083afc4` stays 0.

So whether the TV is on decides which of two timelines a boot runs. The stored references of c3fy2, c3fya, c3fyb and
c3fyc were made with the device present; every run since 2026-10-17 03:46 had none until the TV came back at about
14:00 the same day. With it on, a `C3FT_AUDIO=live` boot's clock log (sandbox_basic `tv_on_live`) has one extra
main-thread timeGetTime before seedFromTime: index 7, through sound.dll's slot, returning to `sound.dll+0x10dc9`, i.e.
the call at `0x10010dc3` in DsBuffer::play `0x10010db0`, reached from initSound `0x00483e90` on the main thread. That
boot's first checkpoint has exactly the old references' boot words (seed_time `0x00100001`, ticker `0xbb71`, RNG seed
`0x1d467741`) and 219 other-thread clock calls; golf_clean.exe made 3,433 calls to the `menu` checkpoint instead of
3,434 (its time-based waits absorb the extra one).

The device state also flipped between two consecutive boots: c3fyc's stories_fixture `ab_base2_on` (2026-10-16 14:31)
ran without the sound thread and the next boot (14:32) with it, while two of c3fyc's games were booting at the same
time (c3fyc notes: "two of my games booting together moved seed_time"). Why the second boot's device open failed is
not determined.

## 2. The fix: the audio stand-in (default since b2)

`C3FT_AUDIO=none` (the default; `C3FT_AUDIO=live` restores the old behaviour) makes scenario_det.js answer sound.dll's
and binkw32's device calls with the device-less results above, on every PC and with any device:

- sound.dll's DSOUND #2 (DirectSoundEnumerateA): the real enumeration runs, only its NULL-guid entry reaches
  sound.dll's callback;
- sound.dll's CoCreateInstance for CLSID_DirectSound: S_OK and a fake IDirectSound whose Initialize returns
  DSERR_NODRIVER and whose Release returns 0 (other CLSIDs: the real API);
- binkw32's waveOutOpen: 2 (MMSYSERR_BADDEVICEID);
- sound.dll is pinned (game.py keepsound.js) on every PC, as it already was on a PC without a device.

Each stand-in call is counted in the run's status (`audio`: dsenum 1, dsenumPassed 1, cocreateDs 1, dsCalls [10, 2]
in every boot measured). Verified here: boots with the stand-in equal boots without it in state, frames and the whole
clock-call sequence (sandbox_basic, 2 checkpoints, 21,265 main-thread clock calls), and the 19 cov_k1 / cov_k2 / cov_k3
references the c3fyk batches recorded on 2026-10-17 without a device and without the stand-in equal the new stand-in
references at every checkpoint (section 10). With the device present (TV on, `waveOutGetNumDevs` 1, the stand-in's
`dsenumDropped` 1: the real enumeration listed the TV and it was filtered out): sandbox_basic (10 checkpoints, OFF),
cov_b_inject2 (5, OFF) and cov_b_career (6, mainLoop ON) each equal their device-less reference at every checkpoint,
with `forwarded` 0; the same sandbox_basic boot with `C3FT_AUDIO=live` differs from the first checkpoint
(`scenario_refs.py check` reports `audio path device-less / sound engine live`).

scenario_ab.py's `write_results` reports an A/B whose arms ran different audio paths as `INCOMPARABLE ARMS` and gives
every function the verdict INCOMPARABLE (never GREEN or RED). An arm ran the device-less path when it had the stand-in
(`"audio": "none"` in summary.json) or when its sound timer thread never called timeGetTime (`forwarded` 0 at the first
checkpoint), so a reference made before b2 on a PC without a device stays comparable with stand-in runs; one made
with the sound engine live does not.

## 3. Checklist: two runs are comparable when

1. Same `golf_clean.exe` and install files: the instance's mutable files (saved games, Themes, logfile.txt, top10.sve)
   restored before every boot from one snapshot (`log/c3/alt/s36a/pristine`, or `instances\s36b_snap` for ui_tour), plus
   the scenario's fixture files.
2. Same schedule.json (the record run's screen-wait times and its record-only decisions, "obs").
3. Same audio condition: both runs with the audio stand-in (`summary.json` "audio": "none"). A reference made before b2
   is `live` and matches only boots made with the same device state.
4. The OFF arm with every hook off (SIMGOLF_HOOKS_OFF=all); the shim build only matters for the ON arm.
5. No control DLL left in the instance: `winmm.dll` equal to `winmm.dll.c3ft_main` (section 6).
6. The boot not overlapping another game's boot (section 7).
7. Both runs finished: summary.json without `failure`, every checkpoint present. A run that died before writing its
   comparison is an ERROR, never SAME.
8. The same frame source and stack seed (b5, section 11): both default (`"frame": "backbuffer"`, `"stack_seed":
   "zero"` in summary.json). A run made before b5 has neither key. Its window-captured frames equal back-buffer frames
   wherever its window was current, but frames on the location screen can differ (`scenario_refs.py check` prints a
   note).

## 4. The stale-reference symptom

A stale reference differs from a fresh run AT THE FIRST CHECKPOINT, in the boot words seed_time `0x0051b380`, ticker
`0x00822c78` and RNG seed `0x00822d9c` (and whatever the RNG drives later), with T, tgt and qpc equal. A hook defect
almost never shows there: the first checkpoint is taken before the scenario's first input. Seen this pattern: compare
`scenario_refs.py check --scenario <s> --run <run>`, which prints the conditions that differ (`STALE CONDITION: ...`)
and both runs' boot words, and regenerate the reference rather than scoring the run.

## 5. Opt-in diagnostics added by b2

- `--clock-log N` (scenario_ab.py `determinism` / `ab`, scenario_refs.py `run` / `variance`): the first N main-thread
  timeGetTime / QPC calls with their caller, recorded natively by the clock stubs (`clock.json` in the run folder).
  Without it the stubs are byte-identical to the default ones.
- `--msg-log N`: the first N main-thread entries of jgld's windowProc `0x100692b0` as [T, msg, wParam, lParam]
  (`wndmsg.json`); `log/c3/b2_wmdiff.py` compares runs with handle-valued parameters masked.
- `C3FT_SHOT_SETTLE=<ms>`: capture the checkpoint frame again every <ms> until two captures agree, and record whether
  the first one differed (`<name>.first.png`, `shot` in the checkpoint's summary entry).
- `C3FT_FULLDUMP=1`: also write golf_clean.exe's whole .data section (`0x004c1000`, `0x381174` bytes, globals and .bss)
  at each checkpoint as `<name>.full.bin`; `log/c3/b2_groupdiff.py` lists the bytes that are constant within each of two
  groups of runs and differ between them (heap pointers differ in every run, so a plain diff is noise).
- scenario_refs.py: `SCENARIO_REFS_EXTRA=<file.py>` (a `register(S)` adding scenarios, for studies of an older script
  version) and a per-instance lock `log/c3/alt/refs/instance_<name>.lock` (one run per install copy).

## 6. Control-DLL restore recipe

A killed `ab --negctl` (or a mutant wrapper) leaves the control `winmm.dll` in the instance and a stale boot lock. Before
the next boot: compare `instances\<x>\winmm.dll` with `winmm.dll.c3ft_main` (sha256) and copy the backup back; delete
the runner's `boot.lock` directory; when the main DLL is rebuilt, copy it into BOTH `winmm.dll` and
`winmm.dll.c3ft_main` (scenario_ab creates the backup once and restores it after every control run, so a stale backup
puts the old build back). scenario_refs.py refuses to boot an instance whose two DLLs differ. After stopping any
background runner, list processes by command line and kill leftover python / golf_clean.exe of that runner only.

## 7. The boot lock

Every boot holds a lock directory from before the spawn until its first checkpoint's dump (scenario_refs.py:
`log/c3/alt/refs/boot.lock`; it also waits while `log/c3/alt/{c3fyk1,c3fyk2,c3fyk3,c3fyc}/boot.lock` exist; a lock
older than 600 s is taken as left by a killed run). Measured with the stand-in (sandbox_basic, 2 checkpoints, clock
and message logs): 10 boots in pairs WITHOUT the lock were identical to the locked ones in state, frames and clock
sequence; the window-message log shows what overlapping games do to each other: each receives the other's
WM_FONTCHANGE broadcasts (the game installs its fonts at boot) at a virtual time that depends on when the other game
started (T = 0, 439, 923 in four runs), and extra WM_GETICON. jgld's windowProc has no handler for either and the state
did not change, but the arrival time is outside the virtual timeline, so the lock stays: a scenario run must not
overlap another game's boot. Messages from outside the harness seen in the logs of the variance runs (section 9), all
at run-dependent virtual times and with no effect on the compared state or the clock in any of those runs:
WM_FONTCHANGE (`0x1d`) from the other instance's boot; the registered message `0xc1db`
(`DIRECTINPUT_NOTIFICATION_MSGSTRING`, a DirectInput broadcast of other programs); WM_ACTIVATE / WM_ACTIVATEAPP /
WM_NCACTIVATE / WM_KILLFOCUS when the next game on the other instance takes the foreground (jgld's onActivate
`0x10069b90` invalidates the window and realizes the palette). golf_clean.exe's only GetForegroundWindow call
(`0x00479b03` in Surface_clip `0x00479a80`) gates ClipCursor, which the windowed shim suppresses.

## 8. Procedure (re/tools/scenario_refs.py)

```
~/bin/py -3.12 re/tools/scenario_refs.py regen <scenario...|all> [--rerecord] [--jobs 2]
```

per scenario: record `det_a` (screen waits, new schedule.json; only with `--rerecord` or when none exists), replay
`det_b` and `det_c`; the reference is pinned when det_b == det_c at every checkpoint (and det_a == det_b when recorded),
and the manifest entry records the schedule's sha256, the reference digest (checkpoint dumps + frame pixel hashes),
the first checkpoint's boot words, the build (winmm.dll, golf_clean.exe, c3fy.cpp, scenario_det.js sha256) and the
conditions (host, waveOutGetNumDevs, keep_sound, instance). `variance --scenario <s> --n N --arm off|on` repeats a
scenario and reports how many runs equal the reference and the first run; `check` compares one run. Instances
`instances\b2` / `b2b` by default, at most two games at a time.

## 9. Measured run-to-run variance (2026-10-17, LIVING-PC, waveOutGetNumDevs 0, audio stand-in on)

Build: shim `winmm.dll` 381ed2b2... (shim/re_batches.txt + c3fy.cpp 7ef71859..., `log/c3/b2_batches.txt`), instances
`b2` / `b2b`, at most two games at a time. "ON" = mainLoop `0x0040f5c0` ON and every other hook off, except the
sandbox_basic ON rows (every registered hook ON). Identical = state blob and frame pixels equal at every checkpoint.

| scenario | arm | N | finished | identical to | count | other measure |
|---|---|---|---|---|---|---|
| sandbox_basic (2 checkpoints) | OFF | 20 | 20 | reference | 20 | one clock sequence: 21,265 main-thread calls, state sha 162ce36b... |
| sandbox_basic (2 checkpoints) | ON (all hooks) | 20 | 20 | OFF reference | 20 | one clock sequence (same count; callers inside reimplementations differ) |
| sandbox_basic, no boot lock, two games overlapping | OFF | 10 | 10 | reference | 10 | one clock sequence |
| cov_b_inject2 (5) | OFF | 10 | 10 | det_b | 10 | |
| cov_b_inject2 (5) | ON | 10 | 10 | det_b | 10 | |
| cov_a_build3 (8) | OFF | 10 | 10 | det_b | 10 | |
| cov_a_build3 (8) | ON | 10 | 10 | det_b | 10 | |
| cov_a_career (15, Monterey since b2) | OFF | 10 | 10 | det_b | 10 | |
| cov_a_career (15) | ON | 10 | 10 | det_b | 10 | |
| cov_b_career (6, San Diego since b2) | OFF | 10 | 10 | det_b | 10 | |
| cov_b_career (6) | ON | 10 | 10 | det_b | 10 | |
| cov_b_career_phx (the pre-b2 script, c3fyk3's schedule) | OFF | 10 | 10 | first run | 7 | the others: frame only |
| same, again | OFF | 10 | 10 | first run | 4 | |
| same, one game at a time | OFF | 10 | 10 | first run | 5 | |
| same | ON | 10 | 10 | first OFF run | 0 | both pixel variants occur ON as well |
| every one of the 55 references | OFF | 3 (det_a record, det_b, det_c) | 3 | each other | 3 | `re/analysis/data/scenario_refs.json` |

The four flakes reported before b2:

- cov_b_inject2 (Start New Game instead of Sandbox in one ON run, 2026-10-16): made with the sound engine live. That
  run's main thread made 8 more QPC calls (jgld Palette::update `0x1006a3d0` calls QPC at `jgld.dll+0x6a414` on every
  call) than det_b between T 3,434 and 3,939, right after the first click (`log/c3/alt/c3fyb/cov_b_inject2/*/
  poll_log.json`). With the stand-in: 20 of 20 runs identical. The cause of the old divergence is not determined beyond
  this: it happened with the sound engine running, whose threads run on real time.
- cov_a_build3 (date, RNG, walker word at the last two checkpoints, one run) and cov_a_career (frames, one run): both in
  the sound-live period; with the stand-in 20 of 20 identical each (cov_a_career now buys Monterey, section 10).
- cov_b_career (frame-only, `career` / `offer`, in c3fyk1's OFF arm and c3fyk3's ON arm): on the device-less timeline
  the pre-b2 script's Phoenix click is refused and the game stays on the location screen. That screen alone shows the
  variance: two pixel variants at `career` (384 px: the Phoenix pins on the globe and in the list drawn white or grey)
  and two at `offer` (5,830 px: the round button expanded with save / load, or collapsed), in OFF runs (7, 4 and 5 of
  10 identical to the first) and ON runs alike, so it is not the mainLoop hook. Measured identical between the
  variants: the compared state, the main-thread timeGetTime and QPC counts at every poll, the timer activity, every
  window message before `career` (handle parameters masked), the capture (`C3FT_SHOT_SETTLE=50`: the first capture
  always equals the second while the main thread is blocked), and golf_clean.exe's whole .data section
  (`C3FT_FULLDUMP=1`, 3 + 3 runs: no byte constant within each variant and different between them). Running one game
  at a time does not remove it. So the state that decides those pixels lives outside golf_clean.exe's globals (heap
  objects of the UI, or jgld.dll / Terrain.dll data); NOT determined further. The corrected cov_b_career enters the
  career and shows no variance in 20 runs. Lead: dump jgld.dll's data section and the location screen's widget objects
  the same way. Resolved by b5 (section 11): the `career` variant is an uninitialised frame word of courseActionMenu
  (stack, not globals); the `offer` variant is the window lagging the game's unchanged back buffer.

## 10. Scenario changes made with the references

Location prices come from the boot RNG, so two scenarios written on the device timeline clicked a location they could
not afford on the device-less one (the record run then timed out waiting for the HUD; replays stayed on the location
screen): cov_b_career buys San Diego ($70,000, so $30,000 left as with Phoenix before; Phoenix costs $150,000 now) and
cov_a_career buys Monterey ($50,000, the price Northeast had; Northeast costs $500,000 now). cov_k3_career had already
moved to Monterey for the same reason.

Old against new references (`log/c3/b2_oldnew.py`): the 19 cov_k1 / cov_k2 / cov_k3 references the c3fyk batches made
without a device are identical to the new ones at every checkpoint; the other 35 old ones (made with the sound engine
live) differ from their first checkpoint on. Checkpoint lists that changed: championship (`tour_0120` is now
`tour_0122`; it still ends on the results screen after 10 shots), stories9_fixture (the story advances after 40 s, so
`season_0120` is gone), cov_b_shots and cov_b_shots2 (`results` and `shot_08` gone: Gary takes 7 and 5 shots, then the
results screen is not recognised by tournament_over and the scenario's Esc presses return to the title menu, where it
waits to the 900 s cap; a lead for those two scripts). b5 found the cause and rewrote both scripts (section 12).

## 11. The location-screen frame variants (batch b5, 2026-10-17)

Section 9's two pixel variants of the pre-b2 cov_b_career script (registered for studies as `cov_b_career_phx` by
`log/c3/b2_oldcareer.py`) have two separate causes. Both are now removed by default; the committed regression scenario
is `cov_b_location` (re/tools/scenario.py).

### 11.1 `career` (384 px): an uninitialised frame word of courseActionMenu

The location screen is courseActionMenu `0x0046f550` (re/analysis/ui/0046f550_courseActionMenu.md). It reads three
frame words before writing them: the blink counter [F+0x24] (`test byte [esp+0x24], 8` at `0x00470027`; the nearest
site's label plate is drawn in state 3 while bit 3 is set; `0x004703c3` increments it once per frame), the dwell
counter [F+0x28] (`0x00470299`) and the previous hot button [F+0x30] (`0x0047028f`). F = entry esp - 0x4f0 (prologue
`0x0046f550..0x0046f56e`: three dwords of SEH frame, `sub esp, 0x4d4`, four register pushes), so the blink counter is
the dword at entry esp - 0x4cc. In cov_b_career_phx the pointer rests on Phoenix's list entry, so Phoenix is the
nearest site and its plate and globe pin blink.

Evidence:
- The back buffer itself differs between the variants (jgld's DIB section dumped at the checkpoint,
  `log/c3/b5_moddump.js`): 384 px, the same pixels as the window frames.
- Recorded natively at the thunk 0x00470a00 before the jump (scenario_det.js STACK SEED in `probe` mode, 8 boots,
  `log/c3/alt/refs/cov_b_career_phx/b5probe_off01..08`): the blink word as found on the stack was 0xc874f8, 0xc9a200,
  0xc5fc20, 0xcc82c8, 0xc86750, 0xbaffb8, 0xcb6ca0 and 0xaffbd0. Every boot had a different value, and the dwell word
  varied too (0xc20000, 0xc70000, ...). The previous-hot word was 0x1ae538 in every boot, and so was the entry esp
  (0x1ae9c4). Bit 3 of the blink word predicts the frame in 8 of 8 boots: set (3 boots) gives frame bb00778f, clear
  (5 boots) gives b656be05. Who writes those values at that depth is not determined. Note that a Frida Interceptor on
  the entry is not a valid probe: its callback runs on the main thread's stack below esp and overwrites the slot
  (the first measurement read Frida's own words, 0x3516db4 ...).

Fix (default): scenario_det.js puts a native stub at the thunk `0x00470a00` (`jmp 0x0046f550`). That thunk is the only
way in: the calls at 0x00407d46 and 0x0040fb9f, and c3fy's mainLoop reimplementation (`CC(0x00470A00, 1)`), all call
it. The stub zero-fills the 16 KB below esp (descending `rep stosd`) and jumps on, into the shim's hook when one is
installed. So the original and c3fw's reimplementation (its CamFrame lies elsewhere in its frame) both read 0 in all
three words. The function's result and the compared state do not depend on these words; they only choose drawing.
`status().stackSeed` = [entries, the three words as found]. `C3FT_STACK_SEED=probe` records without filling;
`C3FT_STACK_SEED=0` leaves the thunk alone (the agent key is then absent).

### 11.2 `offer` (5,830 px): the window behind the game's back buffer

In the variant boots the back buffer is the SAME as in the others: the two side buttons are drawn, because the scenario
writes the date and the screen draws save and load while the date is not 0. The window still showed the collapsed
picture at `offer`, `year_end` and `year2` (90 virtual s). During that time the game called Display::present
`0x10066470` with the full 800x600 rect on every frame: 794 calls before `play` and 428 before `offer`, logged with
`log/c3/b5_evlog.js` (main-thread windowProc entries, present calls with their rects, onPaint, InvalidateRect,
GdiFlush). So the stale picture comes from after the game's BitBlt: the window contents pass through the
DWM8And16BitMitigation layer that game.py starts the game with. The mechanism there is not determined. Frequency:
b2's sequential boots 0 of 10; boots next to another game 1 to 5 of 10; b5 3 of 40.

Fix (default): scenario_ab.py takes the checkpoint frame from jgld's back buffer. That is the DIB section the game draws
into: g_display `0x10128420` -> +0x148 mode -> +4 Surface -> +0x4b4 HBITMAP. It is read on the blocked main thread with
GetObjectA and converted from RGB555 by (v << 3) | (v >> 2) per channel. Its rows are top-down, although GetObjectA
reports biHeight +600. When the window is current this equals the window capture in every pixel: 0 px differ at all 6
in-game checkpoints of cov_b_career and at the location screen, and the stored frame_sha of cov_b_career's checkpoints
was reproduced from the dumped bits. So references made from window captures stay valid. Each checkpoint still
captures the window, records `shot: {source, window_px_differ}` and keeps a differing window as `<name>.window.png`.
`C3FT_FRAME=window` restores the old capture. Bink and other screens drawn without jgld's back buffer would fall back to
the window, with the reason recorded (none seen in b5's runs).

### 11.3 Result

cov_b_career_phx with both fixes, instances b5 / b5b, at most two games at a time, beside another batch's games:

| arm | N | identical (state + frames, 6 checkpoints) | blink word with bit 3 set (old career variant) | window stale at offer..year2 |
|---|---|---|---|---|
| OFF | 20 | 20 / 20 | 7 | 3 |
| ON (mainLoop 0x40f5c0 on, other hooks off) | 20 | 20 / 20, and equal to the OFF run | 7 | 0 |

The new references' conditions record `frame` and `stack_seed`. `scenario_refs.py check` prints a note when a run and
its reference differ in the stack seed, since frames taken on the location screen can then differ.

## 12. cov_b_shots / cov_b_shots2 (batch b5, 2026-10-17)

Section 10's lead. Diagnostic record runs saved every frame the scripts' decisions looked at, plus one frame every
2 virtual s (`log/c3/b5_diag_extra.py`, frames in `log/c3/b5/diag/`). The fixture championship course has one hole.
After Gary holes out, his info panel shows the skill he gained ("Accurate Putter +20%"). Its SHOT_PANEL crop is 19.9
from tests/golden/shot_panel.png, inside the old threshold of 25, so the script took it for Gary's turn and ran a whole
shot sequence: five key groups, the SHOT_STRAIGHT click, hovers and the click at the target, 16 virtual s in all. The
TOURNAMENT RESULTS panel came up in the middle of that sequence (between T 262,600 and 264,620 in cov_b_shots; between
181,800 and 183,820 in cov_b_shots2). The sequence's own input then closed it, and the game went to the title menu
(frame at 266,640 / 185,840). The loop never checked for the results inside a turn, so it never saw them. On the title
menu it found neither a turn nor the results and pressed Esc until the 900 s cap. The old scripts' "7 and 5 shots"
count that false turn.

A distance threshold alone cannot separate the two panels: real turns measured 0.0 to 18.4, because the ovals change
with lie and club (17.8 in the rough with a lob wedge, where a first fix at 16 left cov_b_shots2 stuck for 510
decisions). Over 1,620 frames of the b5 runs, every turn panel is 19.9 or more from a crop of the post-hole panel
(tests/golden/gary_skill_panel.png) and the panel itself is 0.0, so the scripts test both templates.

Changes (re/tools/scenario.py `_cov_b_champ_rounds`, tests/test_scenario_b5.py):
- `_cov_b_tstate`: `gary` = within 25 of shot_panel.png and more than 8 from gary_skill_panel.png; `holed` = within 8
  of the latter; then `over`, `menu`, `other`.
- A turn is a generator of input groups. The loop checks for the results before each group and stops the turn when
  they are up. The `other` path checks for them again before its Esc (play_tournament's race fix).
- On TOURNAMENT RESULTS: a checkpoint (`results`, then `results_r<n>`), the check mark, and a new championship round
  from the title menu, until 24 shots (`shot_08`, `shot_16`, `shot_24`, `end`).
- Once per round, on the `holed` panel, the turn's input runs on purpose, not counted as a shot. Outside a turn the
  shot-type keys reach other H1 cases (0x0041c5cd and 0x0041c651 set tool modes 2 / 7 through 0x0040d890; 0x0041c6e9
  calls announceBuilding 0x0040e720) and the hovers reach P7's tool preview (0x0041a99a). A first version without this
  probe lost those 32 blocks against the b2 references, which had reached them only through their false turn.

New references, pinned with det_a == det_b == det_c on instances b5 / b5b:

| scenario | rounds | shots | checkpoints |
|---|---|---|---|
| cov_b_shots (Moderate) | 3 (two results screens) | 24 | tournament_start, results, shot_08, shot_16, results_r2, shot_24, end |
| cov_b_shots2 (Difficult) | 4 (three results screens) | 24 | tournament_start, results, shot_08, results_r2, shot_16, results_r3, shot_24, end |
| cov_b_location | (location screen) | | refused, site_hover, buttons, tip, back |

Block coverage of the original mainLoop: one OFF replay of each reference with a one-shot INT3 on all 2,554 block
leaders, each run identical to the reference it replays (`log/c3/b5_coverage.py run` / `diff`, c3fyb_coverage.py's
method; the b2 references replayed with the scripts as committed before b5, `log/c3/b5_oldshots.py`):

- b2 references (7 + 5 shots, no results seen): 831 blocks. New: 917. New minus old: 86 blocks (P1 12, P2 4, P3 13,
  P4 4, P7 2, H2 51: the results panel and the round end). Old minus new: 0.
- Against the device-era c3fyb coverage runs of the same two scripts (5 + 11 shots, results seen): new minus those 28,
  those minus new 6 (P3 0x00414f38, 0x00414f83, 0x00414f8d, 0x00414f96, 0x004150d0; P7 0x0041b705). Those blocks follow
  that timeline's shot positions.
- The union of the 35 coverage runs (c3fy2, c3fya, c3fyb) stays at 2,103 / 2,554. Every block the new runs hit was
  already in it.
- Shot-type key dispatch H1 0x0041c5b9..0x0041c6f3: 12 of its 17 blocks hit (0x0041c5b9, 5be, 5cd, 641, 646, 651, 66d,
  676, 6d9, 6de, 6e9, 6f3). Not hit: 0x0041c5e9, 0x0041c613, 0x0041c685, 0x0041c6a1, 0x0041c6bd.
