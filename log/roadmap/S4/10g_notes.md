# S4.10g: U-1421, x87 control word at the codec entries in the live game

## Verdict

**Not settled by a measurement, because no scenario reaches the codec.** In every live run with the audio device present,
`codecDecodeFrame` 0x1003a980 and `codecEncodeFrame` 0x1003b150 were entered 0 times, and so were their callers
`openMmioSound` 0x10030840 / `Sound5c450::openMmio` 0x10030710 and `loadWaveResource` 0x100316c0 /
`WaveInDevice_requeueBuffer` 0x100393d0 (counts below). The control word at the codec entry is therefore not observed.
What the runs do establish:

- The x87 word of the main thread after load is 0x027f (639) (control, 3 of 3 runs).
- The sound thread runs the mixers at **0x0f7f** (3967): `Voice_mixAddMono16` 0x10019770 is entered with 0x0f7f 5,125 to
  8,185 times per 60-120 s run and leaves it at 0x0f7f (control for the probe: it reads the word the `fldcw` at 0x10019911
  sets, and keeps it across calls).
- Sound is live (waveOutGetNumDevs = 1) and mixing, so a codec call would have been recorded had a scenario made one.
- Which sound.dll caller reaches the codec is data of a **memory** sound: `openMmioSound` opens an `MEM ` I/O proc
  (fccIOProc 0x204d454d, cchBuffer 0x168, decompile of 0x10030840), unpacks 54 bits per frame from `param_1+0x18` and writes
  180 samples; `loadWaveResource` is the encoder side, reached from `WaveInDevice_requeueBuffer` (wave-IN). All 319 sound
  files of the install (`original\Sounds`, RIFF format tag 1 PCM, plus one FORM) are PCM or IFF and never take this path.
  `waveInGetNumDevs()` is 0 on this PC. The path matches the game's network voice class (`VoiceRx`, device vtable slot
  +0x8c = `Sound5c450::openMmio`, 10_fpcw_paths.txt) and needs a multiplayer session with a microphone. No single-player
  scenario (title, sandbox, build a hole, 120 s season) reaches it.

For the port: until a voice session is recorded, U-1421 stays "caller word unknown". Both thread candidates are consistent
with the measured facts: the decode caller `Sound5c450::openMmio` is a device vtable method called from the exe thread
(main thread word 0x027f), the encoder is on the wave-IN pump (soundThreadPump 0x1002a970, the thread that runs the mixers
at 0x0f7f). Neither is measured. The port keeps 0x027f for the codec (`audio_codec` vectors), which is exact for the
decoder if it runs on the main thread, and a port that needs the encoder at 0x0f7f has to take the word from
`sg_set_fpcw` on the vectors.

## Table

| run | scenario | devs | codecDecodeFrame | codecEncodeFrame | openMmioSound / openMmio | loadWaveResource / requeueBuffer | exe VoiceRx::fwdDev35 / netPoll / VoiceRx::start | mixer calls (word) |
|---|---|---|---|---|---|---|---|---|
| menu3 | menu, sandbox, 60 s | 1 | 0 | 0 | 0 | 0 | not hooked | 212 (0x0f7f) |
| season | sandbox, tee + green, hole open, 120 s at x8 | 1 | 0 | 0 | 0 | 0 | not hooked | 8,185 (0x0f7f) |
| season2 | same | 1 | 0 | 0 | 0 | 0 | 0 / 0 / 0 | 5,125 (0x0f7f) |

menu.json (devs 0: the HDMI endpoint was asleep at the start of that run, no mixer ran) and menu2.json (probe stub freed by
Frida's GC: all callbacks threw) are kept as the failed first attempts.

## Method

- `10g_probe.js`: a 7-byte stub `mov eax,[esp+4]; fnstcw [eax]; ret` called through a NativeFunction from each Interceptor
  callback, so it stores the control word of the thread being measured. It must stay referenced from JS
  (`globalThis.__stub`), or Frida frees it and every callback throws (menu2). Sites: codecDecodeFrame, codecEncodeFrame,
  openMmioSound, loadWaveResource, Sound5c450::openMmio, WaveInDevice_requeueBuffer, soundThreadPump, streamRenderBlock,
  Voice_mixAddMono16 / SetMono16 (enter and leave word as controls), exe VoiceRx::fwdDev35 0x485550, netPoll 0x49b7b0,
  VoiceRx::start 0x485450. sound.dll is hooked from the LoadLibraryA return (relocated base, 0x4540000 in season2).
- `10g_run.py <menu|season|intro> <out.json>` runs the harness on instances\s4_u1421 (assert against original\),
  SIMGOLF_KEEP_SOUND=0 so sound.dll is not pinned and the game runs its real sound path, SIMGOLF_SKIP_INTRO=1, TIMEWARP 8 for
  the season. It kills only its own PID and calls purge_frida_temp.
- The audio device flaps with the TV: a run with waveOutGetNumDevs 0 is discarded (menu.json).

## Open

- U-1421: reproduce with a voice session. Candidate drivers, none tried: two game instances in a network game with voice
  enabled, or a Frida-called `Sound5c450::openMmio` on the exe thread (that measures the main thread's word, not the real
  caller). waveInGetNumDevs() = 0 here, so the encoder path needs a virtual wave-IN device.
- The exe-side hooks (netPoll etc.) read 0 in season2, which also says no network session was started; they were not
  controlled against a known call.

## Files

log/roadmap/S4/10g_probe.js, 10g_run.py, 10g_notes.md, 10g_merge.md, 10g_files.txt, log/roadmap/S4/10g/*.json|log.
