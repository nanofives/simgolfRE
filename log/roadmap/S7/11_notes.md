# S7.11 audio out: edge cases and the app gates (2026-10-30, claude2 opus; row S7.11 of re/roadmap/S7_steps.tsv, U-1693..U-1699)

Continues S7.10 (accepted): its files are this step's (log/roadmap/S7/10_files.txt). Mirror: `log/roadmap/S7/11_mirror.py`
(port/build/S7_11_root = `git archive 37e58e96 port re/model/include` + S7.10's and S7.11's owned files; x64 in
port/build/x64, Win32 in port/build/w32 inside it; /W4 /WX clean on both).

## 1. No output device
- The original's device-less path, as measured (re/analysis/data/scenario_baseline.md section 1, log/c3/b2_soundprobe.js):
  timeGetDevCaps 0; DirectSoundEnumerateA (#2) 0, its callback once with GUID NULL; CoInitialize 0; CoCreateInstance(
  CLSID_DirectSound) S_OK; IDirectSound::Initialize (slot 10, call at sound.dll rva 0x11d1d) DSERR_NODRIVER 0x88780078;
  Release (slot 2, rva 0x11a2a) 0; midiOutOpen MMSYSERR_ERROR, waveOutOpen MMSYSERR_BADDEVICEID, waveOutGetNumDevs 0; initSound
  0x00483e90 gets 3 from the music-device open, freeSoundLib 0x00484060 runs, g_soundReady 0x0083afc4 stays 0 (CLAUDE.md,
  no-audio-output paragraph). The port's initSound (audio_exe.cpp init_sound) has the same arms: resourceLoad (music device)
  non-zero -> freeSoundLib -> return; g_soundReady is set only on success.
- Before S7.11 the sink answered those probes from S4's model (Initialize ok), so the shadow's engine believed it had a device
  and mixed into nothing (S7.10 notes U-1691).
- Now (ds_sink.cpp): the first device probe (DSOUND #1, CoCreateInstance, midiOutOpen, waveOutOpen, wave/midiOutGetNumDevs, or
  an IDirectSound call) runs the real DirectSoundCreate (DSound mode); if it fails, or `force_no_device` is set (any mode but
  Model: the test switch), the sink is in its NO-DEVICE state and answers: Initialize DSERR_NODRIVER, that object's Release 0,
  DSOUND #1 DSERR_NODRIVER with *out 0, midiOutOpen 1, waveOutOpen 2, the GetNumDevs 0; CoCreateInstance keeps S_OK.
- Measured in the shadow (log/roadmap/S7/11_nd_capture_1.lines, the title script): initSound's commands are HeapAlloc,
  timeGetDevCaps, critical sections, SetWindowLongA (the window subclass, U-1693), DSOUND #2, CoInitialize, CoCreateInstance,
  com 1.10 (Initialize) -> DSERR_NODRIVER, com 1.2 (Release) -> 0, HeapFrees, CoUninitialize: the measured order, no
  CreateSoundBuffer, no timeSetEvent. initSound returns 3, g_soundReady 0.
- AudioOut (audio_out.cpp): after its auto initSound it reads the shadow's g_soundReady. 0 = the original's device-less state,
  in which every later sound call of the exe returns at its g_soundReady / g_soundDll test: the request that triggered it and
  every later one are dropped (`dropped_requests`), the thread leaves its loop at once and releases the shadow (no timer, no
  work after init: `thread_ended`), stop() skips shutdownSoundSystem (the engine is gone, as after freeSoundLib), and
  write_capture of a device-less Capture run writes an empty WAV. Control 3 (`--control 3`, `--negctl audio-keep-thread`)
  keeps the thread and replays every request: RED.
- This PC's real state: DirectSoundCreate returned DSERR_NODRIVER (-2005401480) in every run (TV off), so `--audio on` (now
  the default) took exactly the forced path: init_eax 3, sound_ready 0, thread_ended, ticks 0
  (log/roadmap/S7/11_livetrace_nodevice.log, 11_window_close.log).
- Sim unchanged: sandbox_basic's corpus trace with `--audio off`, `on`, `on --audio-no-device`, `capture --audio-no-device`,
  `capture` and no `--audio` at all: trace.trace sha256 af96ba979f0... and framehash.bin b6545fc6c40... in all six
  (11_livetrace_nodevice.log).

## 2. The default
- `--audio` defaults to `on` (main.cpp Options::audio), so the LAN build plays through DirectSound when the player PC has a
  device and falls back to item 1 otherwise. `--audio off` keeps the old behaviour.
- No app test needed a fixed setting: the shadow never writes the sim's GameState (S7.10 design, unchanged), item 1 makes the
  device-less default end its thread at init, and with a device the sim is the same by construction. The heavy part (b) runs
  every test_app_* file on this default (NEED_HEAVY list in the final report).

## 3. Shutdown, pause, focus
- ORDER. The original: gameMain runs shutdownSoundSystem after mainLoop returns, before its window goes. The app's window
  session could close the window first (the user, `--exit-after`, or WM_CLOSE after the game's quit while the sim thread runs
  AudioGuard::finish). main.cpp now subclasses the session window (audio_close_proc): WM_CLOSE calls AudioOut::close_early
  (later requests dropped as `closed_drops`, then stop(): queued requests, shutdownSoundSystem, quit, join; the thread's end
  releases audio_release, the DsSink: real buffers then IDirectSound, the model, the context) before DestroyWindow;
  g_close_mu serialises it with AudioGuard::finish. audio.json `window_at_stop` 1 = the window existed when the shadow
  stopped. Control `--negctl audio-close-late` (no hook): window_at_stop 0, closed_early false: RED.
- COUNTS (app::proc_counts: GetProcessHandleCount and the Toolhelp32 thread ids; `threads_added` = ids after that were not
  there before; the bare count moves with the system's thread-pool workers, 4 -> 2 in one dsound cycle):
  - audio_check `--cycles 4` (11_nodevice_cycles.log): capture 105 -> 105 handles every cycle; forced no device 105 -> 105;
    real dsound (DirectSoundCreate failing) 105 -> 123 in the first cycle (the system's audio DLLs load) and 123 -> 123 after;
    no thread id added in any cycle. x64 and Win32 (test_shutdown_leaves_nothing).
  - the app over sandbox_basic (11_livetrace_counts.log): the audio-free reference (`--audio off --negctl audio-count-only`)
    104 -> 109 handles over the same span; capture and forced no device 104 -> 109, threads_added 0; the real failing
    DirectSoundCreate 104 -> 124 (its one-time 20).
  - control 4 (`--control 4`, `--negctl audio-leak-thread`: stop() leaves the thread running, the object leaked so it stays
    valid): threads_added 1, handles +1: RED.
- PAUSE (g_flags bit 2): key 0x50 runs `xor [0x0059e7b8], 4` at 0x0041d020 and jumps to the dispatch end (0x0041d027): no
  sound call (c3fy.cpp H1); P5 0x004175f2 skips the tick while paused, so tick-made sound requests stop, UI ones continue; the
  other three tests of bit 2 in mainLoop (0x00411f6c P1 flag sprite, 0x00413348 tile drawing, 0x0041c1d2 championship popup)
  draw only. The engine's timer does not read g_flags. The shadow copies the sim's requests, so the port needs no code (U-1696).
- FOCUS: the engine's buffers carry no GLOBALFOCUS / STICKYFOCUS (primary 0x1, stream 0x10088, static 0x100e0) and it asks for
  DSSCL_PRIORITY (2) on the window initSound got (11_focus_describe.txt, 11_focus.lines); DirectSound mutes such buffers while
  that window is not in the foreground. The sink passes the flags (3D / hardware / defer bits dropped) and the app window, so
  the same rule applies (U-1694; not observable here without a device).

## 4. Long session (U-1692 -> U-1697)
course_season_6y's corpus trace (51,092 frame records, 6.22 game years) through `simgolf_port --live-trace ... --terrain
headless --audio model` (x64, Release; log/roadmap/S7/11_long_session.log, 11_long_session2.log): framehash.bin equals the
original's capture log/p2/p24/course_season_6y/c_a/framehash.bin (sha256 d6a9d6fcb7d7137e...) in both runs.

| run | wall | requests | replayed | ticks | tick mean / max | arena / live / peak live | live blocks | threads_added |
|---|---|---|---|---|---|---|---|---|
| 1 | 612 s | 2,359 | 2,360 | 20,337 | 1.445 / 2.32 ms | 14,453,616 / 14,265,124 / 14,452,724 B | 573 | 1 (a pool worker, see below) |
| 2 | 578 s | 2,359 | 2,360 | 19,249 | 1.461 / 2.34 ms | 14,453,616 / 14,265,124 / 14,452,724 B | 573 | 0 |

- The heap is the same in both runs (it follows the requests, not the wall time): one 16 MB chunk of the 8 (128 MB) the model
  allows, live bytes within 0.2 MB of the bump arena, no MissingResource, failed 0. late_ticks 0, skipped 0.
- Run 1 counted a system thread-pool worker started during the run as a thread left; proc_counts now leaves threads whose
  start address is in ntdll.dll (TppWorkerThread) out of the ids (run 2: 0; control 4 still RED, its thread starts in the CRT).
- Model mode answers SetWindowLongA 0 (S4's DeviceSink), so its 41 completion messages go to hwnd 0 (`dropped`, S7.10 U-1687);
  on/capture deliver them.

## 5. LAN (U-1499)
The exact change for scripts/lan_release/stage_lan_release.py is in 11_merge.md (the script is not edited, original\ is
not touched): `sounds\Golf_Sfx` staged as `sounds\Golf sfx` (the exe's spelling, 17 strings + one lower-case). Checked on a
scratch install of junctions / hard links into original\ with only that directory renamed (11_sfx_install.py,
11_sfx_install.log): the device run makes 229 mmioOpen calls instead of 211, 14,309 lines instead of 13,020 (U-1698).

## 6. Real device
DirectSoundCreate failed (DSERR_NODRIVER) in every run of the session, so `simgolf_audio_check script --mode dsound` and
`--audio on` took the device-less path; the real-device run and the listening check stay Mariano's (U-1699 carries U-1691).

## 7. Gate (light part, done) and the heavy part (b) (needs the lead's slot)
Light, on the mirror (log/roadmap/S7/11_test_*.log):
- test_app_audio.py 28 passed (x64 + Win32): S7.10's 16 + S7.11's 12 (no-device path x64 / Win32, its control, dsound on this
  PC, shutdown cycles capture / dsound x64 / Win32, the leak control, the app no-device / default / capture sim equality and
  counts, the app controls, the WM_CLOSE order and its control).
- test_app_shell.py 18 passed (with `--audio` defaulting to on).
- test_audio_exe 302 passed / 5 skipped, test_audio_ds 131 / 71 skipped, test_audio_core 145 / 128 skipped / 1 failed
  (test_soundstate_header_current: soundstate_gen.py --check against S10's re/names renames, reads no file of this step; the
  same failure S7.10 recorded).
Controls: 5 new, all RED (no-device keep-thread x2 [tool, app], leak-thread x2 [tool, app], close-late); S7.10's unlock-skip and
volume-ignored still RED), 0 blind; S7.10's pan / frequency equivalents unchanged.

Heavy part (b), not run yet: test_app_ingame / input / live / present / replay one file per process x64 + Win32 (the app now
runs with audio on by default), segments 205 / 205, boot 66 / 2 / 2, live equivalence 68 + 2, app replay 3 / 3, on the mirror.

## 8. Reproduce
    ~/bin/py -3.12 log/roadmap/S7/11_mirror.py --build both
    cd port/build/S7_11_root && SG_PORTSIM_DLL=$PWD/port/build/x64/Release/portsim_c.dll \
      ~/bin/py -3.12 -m pytest port/test/test_app_audio.py -v -p no:cacheprovider
    port/build/S7_11_root/port/build/x64/app/Release/simgolf_audio_check.exe script --exe original/golf_clean.exe \
      --install original --script log/roadmap/S7/10_title.script --mode capture --no-device 1 --cycles 4 --stats out.json
    simgolf_port.exe --install original --live-trace port/corpus/traces/course_season_6y.trace --terrain headless --audio model
    ~/bin/py -3.12 log/roadmap/S7/11_sfx_install.py <scratch dir>

## 9. Files
log/roadmap/S7/11_files.txt. Merge notes 11_merge.md, uncertain rows 11_uncertain.tsv (U-1693..U-1699).
