MERGED

# P5: Mariano's first play-test feedback (2026-11-03, brief log/roadmap/P5/BRIEF.md)

Base b89154c4. Worker trees P5A (in-game items, notes P5A_notes.md) and P5B (app items, notes P5B_notes.md) merged in port/build/P5A
with `git merge-file` against HEAD (host.h, host.cpp, main.cpp changed by both: clean). One fix after the merge: two raw exe addresses
in a reports.cpp comment rewritten as showCourseReport+0x40..+0x35f8 (port test_static).

| item | class | result |
|---|---|---|
| exit hover area too big | original | play switch mainLoop.titleExitHotspotWide: the exit is chosen only inside its own icon |
| theme-pack highlight cut on the left | original (U-1260 stray cell) | play switch showThemePackSelect.column5StackWord: the stray cell is not drawn |
| park highlights only after a mouse move | port bug | the live core runs the world map's 50 ms passes without input (`--no-timed-passes` restores the old wait) |
| course report / top 10 designers blank | port bug | both screens (and the trophy page after the year end) now draw; not pixel-compared with the original (no capture inside them) |
| edge scroll while unfocused | app | `--edge-guard` (default on) |
| + / - zoom | app | `--zoom-keys all` (default): + / - (both keyboards) -> the game's z / x in the frame loop; the original's + (money) / - (lower terrain) keys go away under `all`, `numpad` keeps them |
| close with X | app | `--close-prompt` (default on): the game's own exit prompt in a session, direct close on the title menu |
| resizable, pixelated | app | `--resizable`, `--scale auto|nearest|smooth` (default auto: nearest at integer factors, bilinear otherwise), 4:3 bars |
| widescreen | scoped only | P5B_notes.md item 5 |

`SG_APP_P5B=off` turns P5B's options off (port/test/conftest.py sets it for the app tests).

Gates (log/roadmap/P5/P5_*): build x64 / Win32 rc 0; segments x64 205 / 205 equal to I25's table; app --selftest 37 and --selftest-p5b 6;
port test_static 6, test_profiles 21, test_app_shell 18, test_app_input 16, test_app_replay 14. Worker gates in P5A_* / P5B_* (Win32 builds,
test_gfx_boot_screens 32, test_reports 14, test_commands 83, test_live 38, test_app_ingame 14, test_app_live 12 + the CPU-load test).
