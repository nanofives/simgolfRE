# S4.10g merge text (U-1421)

No port code. Replace the U-1421 row's status/resolution in UNCERTAINTIES.md with:

> U-1421 (S4.10g, 2026-10-25): PARTIAL. Live game, audio device present (waveOutGetNumDevs 1), probe fnstcw on every entry of
> codecDecodeFrame 0x1003a980, codecEncodeFrame 0x1003b150, openMmioSound 0x10030840, Sound5c450::openMmio 0x10030710,
> loadWaveResource 0x100316c0, WaveInDevice_requeueBuffer 0x100393d0: 0 entries in menu, sandbox, hole build and a 120 s season
> (3 runs). The codec is reached only with a memory sound of 54-bit frames (openMmioSound opens an `MEM ` I/O proc) or the
> wave-IN path; all 319 install sounds are PCM and waveInGetNumDevs is 0, so it is the network voice class, not single-player.
> Measured facts: main thread word 0x027f; the sound thread runs Voice_mixAddMono16 at 0x0f7f and leaves it set (5,125 to 8,185
> calls). The word at the codec entries stays unmeasured. The port keeps codec vectors at 0x027f; remains open until a voice
> session is recorded. Evidence: log/roadmap/S4/10g_notes.md.

Also record in 3_merge.md / 10_merge.md section on U-1421: "S4.10g: needs a network voice session, not a single-player scenario".
