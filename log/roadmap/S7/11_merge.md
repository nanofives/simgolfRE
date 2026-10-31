# S7.11 merge notes

Base: 37e58e96 (HEAD 67890563 changes nothing under port/ or re/model/include) + S7.10's files (log/roadmap/S7/10_files.txt) +
S7.11's (log/roadmap/S7/11_files.txt). Every edit is in a file of the two rows' files_owned; no shared file was edited.

- port/CMakeLists.txt, port/src/sim/modules.inc, tick.cpp, commands.cpp: no change. port/app/CMakeLists.txt: no change beyond
  S7.10's (tlhelp32 / GetProcessHandleCount are in kernel32, already linked).
- port/app/host.cpp / host.h / window.cpp: no change. The WM_CLOSE hook is a subclass installed by main.cpp's run_live_window.
- port/src/sim/audio_exe.cpp / audio_exe.h (additive): `DeviceHeapUsage` and the virtual `DeviceModel::heap_usage()`,
  overridden by DeviceSink (reads its arena; changes no answer). Every S4 golden unchanged (test_audio_exe 302 / 5 skipped).
- port/test/test_app_*.py: no change needed. `--audio` now defaults to `on`; without an output device that is the
  device-less path (the shadow ends at initSound), with one the sim is still unchanged (the shadow never writes the sim's
  GameState), so no app test needs a fixed setting. test_app_audio.py also finds binaries in S7_11_root and in a mirror's
  own port/build/w32.
- Integrator's builds: simgolf_audio_check is S7.10's target; nothing new.

## LAN staging (U-1499): dropped

Mariano decided 2026-10-30 (through the orchestrator): the staging script and the install stay as they are; the port
finds both names through a defect switch instead (step S7.12, log/roadmap/S7/12_notes.md).

## Suggested CLAUDE.md paragraph (after the S7.10 one)

Roadmap S7.11, audio out edge cases (2026-10-30, notes log/roadmap/S7/11_notes.md, U-1693..U-1699): `--audio` defaults to
`on`. NO DEVICE: when DirectSoundCreate fails (or `--audio-no-device` / audio_check `--no-device 1` forces it) the sink answers
the engine's device probes as a PC without an output device does (scenario_baseline.md section 1: IDirectSound::Initialize
DSERR_NODRIVER, its Release 0, midiOutOpen / waveOutOpen errors), so the shadow's initSound takes the original's own failure
path (eax 3, g_soundReady 0, after exactly the measured probe order), its thread ends and the observer drops every later
request; the sim's trace.trace and framehash.bin equal `--audio off`'s. SHUTDOWN: the window session's WM_CLOSE stops the
shadow before DestroyWindow (gameMain's order); no thread id is left (Toolhelp32) and the handle count stops growing after a
process's first DirectSoundCreate. Pause (g_flags bit 2, key 0x50 at 0x0041d020) makes no sound call and focus muting is
DirectSound's own (the engine's buffers carry no GLOBALFOCUS / STICKYFOCUS, level DSSCL_PRIORITY), so neither needs code.
