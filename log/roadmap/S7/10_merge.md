# S7.10 merge notes

Base: HEAD 37e58e96. Every edit is in a file of the row's files_owned (log/roadmap/S7/10_files.txt); no shared file was edited.

- port/CMakeLists.txt: no change (port/app is already added; portapp_audio links the Windows system libraries dsound and winmm).
- port/src/sim/modules.inc, tick.cpp, commands.cpp: no change. audio_exe.cpp registers one more configuration entry inside its
  own table (`audio_exe.constructHolders`); touching registry.cpp is not needed.
- port/app/host.cpp / host.h: no change and no patch (AudioGuard lives in main.cpp and wraps the session's run).
- Build: the integrator's builds need the new target `simgolf_audio_check` (x64 at port/build/x64/app/Release, Win32 at
  port/build/Win32/app/Release): port/test/test_app_audio.py looks there after $SG_AUDIO_CHECK_X64 / _W32 and the S7.10
  mirror, and skips an architecture with no checker. It reads log/roadmap/S7/10_title.script and needs SG_PORTSIM_DLL for
  its two start-state tests.
- `--audio` defaults to off, so every existing app test and recording is unchanged; S7.11 decides the default.
- Goldens: none changed (golden_audio_exe.json $device reproduced through the app sink, x64 and Win32).
- On the shared tree port/test/test_static.py::test_every_module_file_is_listed fails because of another step's untracked
  port/src/sim/net.cpp (not in modules.inc); in the S7.10 mirror the literal scans pass.

Suggested CLAUDE.md paragraph (after "Roadmap S7.9"):

Roadmap S7.10, audio out (2026-10-29, notes log/roadmap/S7/10_notes.md, U-1686..U-1692): `simgolf_port --audio on|model|capture`
(default off) keeps the sim's device-less sound path (GameState unchanged: --live-trace sandbox_basic trace.trace and
framehash.bin equal with and without audio) and copies every request of audio_exe (Req depth 0, the new
`audio_exe_set_observer`) to a SHADOW context on an audio thread (port/app/audio_out.*): the exe's initialised data plus the
device-holder constructors (`audio_exe_construct_holders`), gameMain's initSound(1, 0x8a) before the first non-CRT request,
shutdownSoundSystem at the end, the timer client called at its 30 ms period on that thread. Its sink (port/app/ds_sink.*)
answers through S4's DeviceSink model (`DeviceModel`, `make_device_model`, `audio_exe_device_fixture`) and drives real
dsound.dll buffers (DSound) or a software capture (Capture). Gate (a): audio_stream.device_run's sequence through the app
sink gives $device 4b6ed61a x64 and Win32 (`simgolf_audio_check device`). Gate (c): a scripted title session's capture
against the install's WAVs (port/test/test_app_audio.py): clicks at volume / 127 and hard pan, static buffers at unity gain
then faded; controls unlock-skip and volume-ignored RED, pan / frequency equivalent (the engine sends no SetPan, SetFrequency
only at 22050 Hz). Finding: S4's model answers SetWindowLongA 0, so SoundWindowObj_subclass never kept the window and every
completion message went to hwnd 0 (a slot played once); the app sink answers a previous-procedure token and delivers the
messages to soundWindowSubclassProc. The real device was not available (DirectSoundCreate DSERR_NODRIVER, TV off, U-1691).
