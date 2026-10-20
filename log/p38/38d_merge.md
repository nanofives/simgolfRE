# Stream 38d: lines for the parent to merge

1. `port/src/sim/modules.inc`: append one line after `SG_MODULE(commands)` (any position works; the module registers by
   hooks.csv name and calls its peers through the registry):

   ```
   SG_MODULE(dialogs)
   ```

   Registered names: skillPointDialog, showCharacterIntro, drawGolferInfoPanel, drawScrollText, runScrollText,
   scrollTextWindow, showBulletinBoard, plus test entries dialogs.setScript / scriptStats / clearScript / scriptPeer /
   clearPeers. None of these names is registered by another module at HEAD.

2. `port/src/sim/tick.cpp` FallbackReg: `fallbacks()["showBulletinBoard"] = fb_show_bulletin_board;` is never used once
   module `dialogs` registers showBulletinBoard (a registered function wins). Optional cleanup: delete that line and
   `fb_show_bulletin_board` (its non-modal prefix is the first part of `show_bulletin_board` in dialogs.cpp, same writes).

3. `port/src/sim/tick.cpp` HeadlessTerrain: cov_b_holes18/00 stops with `missing: headless Terrain arena` at record
   1140 of 2130 (the arena is a 1 MB bump allocator for operator new and is never reclaimed). Measured in the 38d mirror
   only: with

   ```
   static constexpr uint32_t kArenaSize = 0x01000000u;   // was 0x00100000u
   ```

   the segment PASSes 2130/2130 (log/p38/38d/segs_exp.tsv). Either raise the size or make the stand-in's operator delete
   reuse freed blocks; the parent's choice (tick is not this stream's file).

4. `UNCERTAINTIES.md`: the rows of `log/p38/38d_uncertain.tsv` (U-0990..U-0996), and status updates:
   - U-0865 (skillPointDialog not ported): resolved by module dialogs (log/p38/38d_notes.md section 2).
   - U-0931 (showCharacterIntro unregistered): resolved; ui_tour/02 and ui_tour_v/02 now stop later, on Text_put inside
     the dialog's showPopup (records 154 / 160 reached with 20 / 22 dispatches delivered), until 38a's provider is merged.
   - U-0950 (drawScrollText's layout): resolved (layout ported over TextMetrics, oracle GREEN with scripted widths).
   - U-0905 (runBulletinBoardWithSound reaches showBulletinBoard through backend_call): showBulletinBoard is now
     registered; with the photo index 99 the board has no award dialog, only its modal wait.

5. `CLAUDE.md` (one paragraph, the parent's wording): module `dialogs` (phase 3.8 stream 38d), its oracle
   (test_dialogs.py: scripted cursor sequences through `dialogs.setScript`, scripted story / sprite peers through
   `dialogs.scriptPeer`), the segment results.

6. `port/test/test_events.py::test_run_bulletin_board_with_sound` (3.5's test) fails once `dialogs` is linked:
   runBulletinBoardWithSound reaches the registered showBulletinBoard, which (as the original) waits for input and
   increments g_bulletinAnimTick first. Replace the loop body's lines from `st.write(tick, ...)` to `got_word = ...`
   with (checked against the 38d DLL by log/p38/38d/check_events_patch.py, both modes pass):

   ```python
            st.write(tick, struct.pack("<i", 7))
            st.write(words + 4, struct.pack("<i", -1))   # no photo position: the board takes no camera / photo step
            board = lib.has("showBulletinBoard")          # module dialogs (38d): the board waits for a click or a key
            if board:
                import ctypes as C
                esc = (C.c_uint32 * 18)(8, 0x1B, 0, 0, 0, 0, 7, 0x1B, 0x1B, 0, 0, 0, 9, 0, 0, 0, 0, 0)   # onChar, onKey, onChar0
                st.map(0x30000000, esc, readonly=True)
                st.call("dialogs.setScript", [0x30000000, 3, 0x30000000, 0])
            st.call("runBulletinBoardWithSound", [])
            if board:
                st.call("dialogs.clearScript", [])
            got_tick = struct.unpack("<i", st.read(tick, 4))[0]
            got_word = struct.unpack("<i", st.read(words, 4))[0]
        assert got_word == -1
        assert got_tick == (0 if mode == 0 else (0x15 if board else 0x14))
   ```

   (A click would divide by scaleX(2) = 0 in the zero state; the photo position 0 would call worldToScreen on a zero
   terrain, U-0867.)
