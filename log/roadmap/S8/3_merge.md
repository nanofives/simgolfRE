# S8.3 merge notes (for the integrator)

All four items were applied in the private tree port/build/S8_3_root (log/roadmap/S8/3_mirror.py --variant merged writes items 1-3
exactly as below) and the gates of 3_notes.md were measured there. Item 4 is optional (diagnostics only).

1. port/src/sim/modules.inc: append one line

```
SG_MODULE(video)
```

(then touch port/src/registry.cpp: MSBuild does not rebuild it when only modules.inc changes)

2. port/src/sim/tick.cpp, kDrops: delete this line (the drop wins over the registry in `resolve`, so the module is unreachable while it
stays):

```
    {"playAudioFile", "the closing Bink video (binkw32); its pumps dispatch no recorded input in the corpus"},
```

The closing call in tick_menu.inc (`cc("playAudioFile", (GS(f_strpool_4c6ba4) + 16), 1);`, ML+0x4e4) then reaches the module unchanged.
`showTitleScreen` stays a drop (its gfx body draws the title buttons).

3. port/src/sim/tick_boot.inc, BootGameMain: insert these three lines directly before `    cc("showTitleScreen");`

```
    // gameMain+0x41f showTitleScreen plays the intro first (its call at showTitleScreen+0x27, immediate 0): the video
    // module (S8.3), whose default host takes the original's own skip path (BinkOpen NULL)
    cc("playAudioFile", (GS(f_strpool_4c89ac) + 96), 0);           // ./flics/SMSG_introfinal.bik
```

(showTitleScreen calls playAudioFile before anything else that writes state: its prologue, `xor ebx, ebx`, `push ebx`, `push 0x004c8a0c`,
`call 0x0043cce0` at 0x0043cd97; 0x004c8a0c = f_strpool_4c89ac + 0x60.)

4. Optional, port/src/sim/tick.cpp, the frameless boot branch of tick_run_frames (the block that sets
`"trace has no FRAME record after the boot; the boot ran to the end of the trace"`): keep the boot step's FrameExit and report the quit,
so a title-menu-only trace (S8.2's cov_r_s8_full) says "mainLoop returned" like cov_r_quit does (U-1721). Replace

```
      try {
        tk::tick_frame_io(ctx, in, io);
      } catch (const NeedInput &) {
```
by
```
      FrameExit fex = FrameExit::Menu;
      try {
        fex = tk::tick_frame_io(ctx, in, io);
      } catch (const NeedInput &) {
```
and the detail line by
```
      if (detail) *detail = fex == FrameExit::Quit ? "trace has no FRAME record after the boot; mainLoop returned (the title menu's quit)"
                                                   : "trace has no FRAME record after the boot; the boot ran to the end of the trace";
```
(not applied in the measured tree; test_video.py accepts both texts.)

5. Data (local, gitignored, already in place in the shared tree): port/corpus/traces/cov_r_s8_play.trace (+ .json), log/p2/p24/cov_r_s8_play/c_a/
(framehash.bin / .json, summary.json, s8_3_install.json), port/corpus/saves/cov_r_s8_play/{run_a,end}. With them `port.test.boot --all` has one
more row: cov_r_s8_play PASS 216 / 216.

6. UNCERTAINTIES.md: append the rows of log/roadmap/S8/3_uncertain.tsv (U-1720..U-1723).

7. CLAUDE.md port section, one paragraph (suggested text):

```
S8.3 video module (2026-10-24, re/analysis/video/skip_path.md): module `video` ports playAudioFile and BinkPlayer::play / setTarget / close behind a
MovieHost (portsim/sim/video.h; D2: no decoder, the default host answers every open "unavailable", so both movies take the original's own BinkOpen-NULL
path: deadline word set and cleared, g_bink target set, one pumpInput483bd0, handle / buffer 0, g_ui83ab9c untouched). The intro is called from the boot
(tick_boot.inc before showTitleScreen), the closing movie from tick_menu.inc's QUIT block (playAudioFile is no longer a drop). An intro-ON trace needs
nothing else: binkw32's 54,945 QPC records never enter a clock list (parse_trace keeps kind-1 values; reads are site-keyed), and g_ui83ab9c (engine) has
no reader outside BinkPlayer::play. Evidence: emulator oracle 23 / 23 cases with 12 / 12 branches, pinned digests x64 = Win32, new intro-ON boot scenario
cov_r_s8_play (re/tools/s8_boot_capture.py, instance s8_3) PASS 216 / 216 and to the exit with the quit files equal; segments 205 / 205 unchanged.
```
