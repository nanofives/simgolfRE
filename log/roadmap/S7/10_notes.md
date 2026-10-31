# S7.10 audio out, the host (2026-10-29, claude2 opus; row S7.10 of re/roadmap/S7_steps.tsv, U-1686..U-1692)

## 1. What was built
- `port/app/audio_out.{h,cpp}`: `app::AudioOut`, a SHADOW context on its own audio thread (created, used, released there:
  one Ctx per thread). Start: the exe's initialised data at the sim's .data address (ExeConstants InitialImage) plus the
  device-holder constructors (`audio_exe_construct_holders`), its own read-only `DirGameFiles(install)` (write-through off), a
  `DeviceModel` + `DsSink` attached by `audio_exe_device_fixture(prepare=false)`; or, for gate (a), a given GameState image
  prepared by deviceFixture (`start_image`). `attach(sim)` installs audio_exe's request observer on the sim context: every
  request (Req depth 0) is copied into a queue and replayed in the shadow by its hooks.csv name with the same slots (the
  shadow uses the same data address, so a GameState pointer such as Snd::status's Snd names the same record). `auto_init`
  runs gameMain's `initSound(window token 1, 0x8a)` before the first request after the CRT's `arrayStaticInit_448160`, and
  `stop()` runs `shutdownSoundSystem`. The timer client timeSetEvent registered (model's `timer_cb / timer_user / timer_id /
  timer_period`, period 30 ms) is called `(id, 0, user, 0, 0)` stdcall on the audio thread: at its period in real time
  (`realtime`, late ticks beyond four periods skipped and counted), or once per `tick()`. After every call the messages the
  engine posted to its window are delivered to its window procedure (section 4). Any exception of a replayed call or tick
  stops the shadow (silent, `first_error`); the sim is never affected.
- `port/app/ds_sink.{h,cpp}`: `app::DsSink`, the AudioSink. Every command goes to S4's DeviceSink model first (its answer and
  log line), then to a backend: `Model` (nothing else: gate a), `DSound` (real dsound.dll: DirectSoundCreate, SetCooperativeLevel
  with the app window, CreateSoundBuffer from the engine's DSBUFFERDESC / WAVEFORMATEX with the 3D / hardware / defer flags
  dropped, SetFormat of the primary, each Unlock's bytes copied from the context into the real buffer at the same offset,
  Play / Stop / SetVolume / SetPan / SetFrequency / SetCurrentPosition, GetCurrentPosition / GetStatus and timeGetTime /
  GetTickCount from the real device / clock), `Capture` (a software device: Unlocked bytes kept per buffer, the playing buffers
  mixed into a 16-bit capture at the primary's format, GetCurrentPosition from that play position, WAV out). Test-only wrong
  bodies `SinkDefect` (unlock copy skipped, volume / pan / frequency ignored).
- `port/app/tools/audio_check.cpp` (`simgolf_audio_check`): `device` (gate a) and `script` (gate c, cost, the dsound check).
- `port/app/main.cpp`: `--audio off|on|model|capture` (default off; S7.11 decides the default) for the live window session
  (`on` passes the window handle) and `--live-trace`; `AudioGuard` attaches after the terrain and finishes (detach, stop,
  `<rec>/audio.json`, `<rec>/audio.wav` for capture) before the context goes. `port/app/CMakeLists.txt`: library
  `portapp_audio` (links portsim, dsound, winmm), `simgolf_audio_check`.
- `port/src/sim/audio_exe.cpp` / `audio_exe.h`, additive only: `audio_exe_set_observer` (ExeCtx::observer, called in Req at
  depth 0 before the stream line), `DeviceModel` (DeviceSink's base: clock, timer_cb / user / id and the new `timer_period`
  from timeSetEvent's first argument, unanswered), `make_device_model()`, `audio_exe_device_fixture(ctx, sink, model,
  prepare)` (deviceFixture's preparation moved into `device_prepare`, unchanged), deviceStep 3 / 4 and streamLog find an
  external model too, one mutex `g_dev_mu` over device_sinks() and the new external_models(), `audio_exe_construct_holders`
  and the configuration entry `audio_exe.constructHolders` (negctl `g_neg_constructHolders` 1: the buckets' default 0x7e).

## 2. Gates (all on the mirror build of HEAD 37e58e96 + these files)
- (a) `audio_stream.device_run`'s sequence through AudioOut / DsSink in Model mode on the audio thread (simgolf_audio_check
  device on sandbox_basic/00): 13,020 lines, sha256 4b6ed61a9306793c298044d20fb3c91e32c2fa9a4e75b11220220c43b8f0eddb =
  golden_audio_exe.json $device, x64 AND Win32. `~/bin/py -3.12 -m port.test.audio_stream device` with the mirror DLL also
  still gives 4b6ed61a (the additive edits change nothing).
- (b) the shadow's start equals a captured image: InitialImage + arrayStaticInit_448160 + constructHolders against
  sandbox_basic/00 + deviceFixture(0), both after loadSoundTable: g_sfxDevice / g_musicDevice / g_voiceDevice / g_soundHead /
  g_soundTail / g_soundFns / g_soundReady / g_soundDll equal, g_sounds differs in 3 bytes only (the +4 word of records 0x2d,
  0x33, 0x80, written by the recorded run: Snd::applyField484750's field).
- (c) title-menu capture (log/roadmap/S7/10_title.script: loadSoundTable, playSound 0x18 = boing.wav at volume 100 / 60 /
  pan -100 / pan 100, playSound 0x2c = buy2short.wav (pan 100) then startSoundSlot 0x2c, playSound 0x80 = Music Idea.wav then
  startSoundSlot 0x80; 340 ticks of 30 ms) judged in Python against the install's WAVs (wave module, numpy; FFT cross-
  correlation): clicks found at 0.460 / 1.330 / 2.230 / 3.130 s, correlation 1.0, gains 0.7891 / 0.4766 (= volume / 127, the
  exe's Snd::setVolume scale, within 0.01), pan -100 left only (L 1.0, R 0.0), pan 100 right only; the static buffers at
  unity gain (correlation 0.9996-1.0) then faded over ten 100 ms windows (0.795 0.634 ... 0.029) to 0. x64 capture = Win32
  capture byte for byte. Facts: log/roadmap/S7/10_judge_facts.txt.
- Controls: unlock copy skipped RED (6 failures, nothing found), volume ignored RED (both fades read as steps); pan ignored and
  frequency ignored are equivalent (captures byte-identical: the engine sends no SetPan to a DS buffer, pans by mixing into
  its stereo stream buffer, and SetFrequency only with 22050 Hz, U-1689). Gate (a) plumbing controls (AudioOutOptions::control):
  1 timer without its user word RED, 2 a replayed request missing its last slot RED; an 11 ms model clock step per tick is
  equivalent (no line carries a time answer). constructHolders mode 1 RED (g_musicDevice differs).
- The sim is unchanged: `simgolf_port --live-trace sandbox_basic --terrain headless --audio capture` gives trace.trace and
  framehash.bin byte-identical to `--audio off`; 3 requests mirrored, all replayed, failed 0.
- The live window session (`--audio capture --exit-after 8000`): requests arrayStaticInit_448160, loadSoundTable,
  playSound 0x80; 284 real-time ticks in 8.5 s; the capture holds Music Idea.wav at t 0.03 s, gain 1.00003, correlation
  0.99999998.
- Real-time cost per tick (log/roadmap/S7/10_cost.tsv, U-1690): title x64 0.10 ms mean / 0.68 max, Win32 0.25 / 1.12; the
  178-sound device run x64 16.5-17.2 / 24.3, Win32 32.3-36.9 / 122 (over the 30 ms period).
- Real device: not run. DirectSoundCreate returned DSERR_NODRIVER (0x88780078) on LIVING-PC (the TV's HDMI endpoint was off):
  the shadow continued on the model's answers, silent (log/roadmap/S7/10_dsound_x64.json, U-1691). No listening check.
- Light regressions: test_app_audio 16 passed; test_app_shell 18 passed (run inside the mirror); test_audio_exe 302 passed /
  5 skipped, test_audio_ds 131 / 71 skipped, test_audio_core 145 passed / 128 skipped / 1 failed (from the shared tree with
  SG_PORTSIM_DLL = the mirror DLL); the failure is test_soundstate_header_current, `re/tools/soundstate_gen.py --check`
  against re/names (S10's renames), which reads no file of this step. Win32 runs of those three were not made (their runner
  path is the integrator's fixed port/build/Win32).

## 3. Findings
- Only the first click sounded until the window fix: S4's DeviceSink answers SetWindowLongA with 0, so SoundWindowObj_subclass
  (sound.dll rva 0x286f0) takes its failure exit (0x28712, `return 0x18`) before storing the window at +8 (0x28721), so
  g_soundWindow (= SoundWindowObj 0xb49b8 + 8; no instruction writes it directly) stays 0, MultiSound_ctorWithType copies 0
  into each sound's +0x1420, and every 0x7ee completion (postNotification) is posted to hwnd 0. A sound whose completion
  never arrives keeps its voice busy: a second playSound of the slot is silent. The original subclasses the real game window,
  whose SetWindowLongA returns the previous procedure. The sink now answers a token (1 << 12) outside Model mode (U-1687).
- The engine plays music-type slots (0x2c, 0x32..0x35, 0x6e, 0x73, 0x78, 0x7d..0x80 in the device run) as static DS buffers
  (whole file Unlocked once, SetFrequency 22050 + Play) and everything else (effects, voices, the ambience 0x2d) through its
  own mixer into one 64 KB looping stream buffer (22050 Hz stereo 16-bit, flags GETCURRENTPOSITION2 | CTRLVOLUME |
  LOCSOFTWARE). The primary buffer is set to 44100 Hz stereo 16-bit. Fades of startSoundSlot are SetVolume steps (33 steps over 1 s for slot 0x80,
  -279 .. -10000 hundredths of dB) then Stop.
- Clicks start 0.43-0.46 s after their request with the capture's cursors (U-1688).
- Build hygiene: a mirror sync with shutil.copy2 keeps the source mtime; a file edited while a build ran is then older than
  the objects that build made from its previous text, MSBuild links stale objects, and the Win32 tool died with 0xC0000409
  (an ODR mismatch on AudioOutOptions). 10_mirror.py now copies with a fresh mtime.
- U-1499 (the install's `Golf_Sfx` against the exe's `Golf sfx`): 178 of 300 slots open, as in S4; no slot of the title
  script is in that directory. Nothing in original\ was renamed.

## 4. Reproduce
    ~/bin/py -3.12 log/roadmap/S7/10_mirror.py --build both        # port/build/S7_10_root (x64 and w32 build dirs inside)
    SG_PORTSIM_DLL=$PWD/port/build/S7_10_root/port/build/x64/Release/portsim_c.dll \
      ~/bin/py -3.12 -m pytest port/test/test_app_audio.py -v -p no:cacheprovider
    ~/bin/py -3.12 log/roadmap/S7/10_cost.py                         # the tick cost table
    port/build/S7_10_root/port/build/x64/app/Release/simgolf_audio_check.exe script --exe original/golf_clean.exe \
      --install original --script log/roadmap/S7/10_title.script --mode dsound --stats out.json   # the real device (audible)
    simgolf_port.exe --install original --audio on                   # the app with sound (Mariano listens, gate d)

## 5. Open (for S7.11 and the lead)
- The real-device run and the listening check (U-1691); the no-device path is already silent with the sim unchanged.
- `--audio` defaults to off: flipping it to on is S7.11's (its app gates); the existing app tests run without audio.
- Win32 cannot keep up with 13 simultaneous voices (U-1690); typical title / course sessions cost under 1.2 ms per tick.
- Long-session heap of the model (U-1692); the delivery time of the window messages (U-1687); the capture's cursor and
  interpolation model (U-1688, U-1689).

## 6. Lead re-run fixes (2026-10-29)
- Lead's re-run (log/roadmap/S7/10_lead_rerun.log, about 50 % load): 15 passed, test_live_window_session_plays_the_title_music
  failed with 166 ticks against `200 <= ticks <= 300`. That assertion counted ticks from process start, so a slow boot under
  load failed it.
- Audio thread priority: in realtime mode AudioOut::thread_main sets THREAD_PRIORITY_TIME_CRITICAL, the level of winmm's
  timer thread where the original's timer client runs. It sits above the sim thread (ABOVE_NORMAL, host.cpp) and the
  normal-priority threads of a busy machine. Virtual-time runs (tests, gate a / c) keep the default.
- The test launches the app with app_drive.priority_flags() (Normal priority class, S3.14 / U-1395;
  SG_APP_PRIORITY=inherit keeps the parent's class).
- The test now judges the timer by its own rate. The shadow records `skipped_periods` (the periods a late tick skips beyond
  four periods; `due` advances by whole periods) and `tick_span_ns` (from the first tick's actual start to the last's), both
  in audio.json. Pass: (ticks - 1 + skipped_periods) / span within 0.6 ms (2 %) of 30 ms, and ticks >= 100 (a loaded boot
  still meets it: 166 before the fix).
- Three runs after the fix (log/roadmap/S7/10_live_window_3runs.log), machine load 65 / 48 / 68 %:

  | run | ticks | late | skipped | span | period | tick mean / max |
  |---|---|---|---|---|---|---|
  | 1 | 279 | 0 | 0 | 8.314 s | 29.907 ms | 0.217 / 1.446 ms |
  | 2 | 279 | 0 | 0 | 8.307 s | 29.882 ms | 0.208 / 1.472 ms |
  | 3 | 280 | 0 | 0 | 8.344 s | 29.907 ms | 0.205 / 0.934 ms |

  All 3 pass. The full file passes 16 / 16 after the change. x64 and Win32 rebuilt clean (/W4 /WX).
