# P9 merge (2026-11-04): third play-test feedback

Tree port/build/P9M = git archive 2fd3553c + P9A + P9B by 3-way merge (log/roadmap/P9/P9M_merge.py, base = the archive). One
conflict, port/app/main.cpp's Options flags line (both workers added a selftest flag): both kept. present.h / present.cpp / tick.cpp
merged cleanly (P9A only touched the band fill and the title-menu switches, P9B the compositor and the layer scope). P9B's one-line
window.cpp change (--cursor message answers read the remapped lp) taken. Files: P9M_files.txt.

Gates (log/roadmap/P9/P9M_gates.sh, P9M_gates.log): build x64 / Win32 rc 0; selftests x64 and Win32 37, p5b 6, p7 9, p8b 11,
p9a 5, p9b 10, all pass; segments x64 205 / 205, 0 rows differ from I25; pytest one file per process: test_static 6, test_profiles
21, test_app_shell 18, test_app_input 16, test_app_replay 14, test_app_ingame 14, test_app_present 3, test_app_live 13 passed. Live
check (P9M_live.py, 1920 x 1080, play profile): title -> Sandbox -> Monterey -> in game, screenshots in P9M_runs/live1920 (the
sandbox's money meter is blank, as before P9).

What changed for the player:
1. simgolf_port.exe is a WIN32-subsystem program (no console window); output goes to <data>/simgolf_port.log, and to the console when
   launched from one. simgolf_port_console.exe is the console build (tests and tools pipe stdout, so they work with either).
2. Esc goes back (play switches, P9A_esc_survey.md): world map Back, buy land Done, character editor Exit, its look picker Cancel,
   key-less popups Close, skill point dialog OK. Not changed: title menu (Esc would quit), hire dialog (Esc = hire nobody already).
3. The false hover is the original's title menu: Start New Game lit from the first frame, and a hover radius of 160 px around each
   label. Play switches mainLoop.titleDefaultItem1 and mainLoop.titleHotspotRange (hover only over the button's art).
5. Band fill `--wide-fill edge` (default): the frame's edge colour for 24 logical columns, then a fade to the screen's background
   colour, so a cut shape is not streaked. `--wide-fill soft` or SG_APP_P9=off = the P7 fill.
6-8. The layered HUD compositor (P9B_notes.md): HUD panels are captured as their own RGBA layers where the game draws them (title,
   meters, bottom band, a right-edge panel layer for the golfer action / employee panels and the shot panel's texts), composed at their
   anchors with their blends redone over what is under them; messages, labels and tips stay whole and centred; clicks remapped by layer
   coverage. `--hud-layers off` = P8B's rectangles. The tropical scenery icons that overflow the palette are the original's (its own
   reference frame shows them); only the cutting was ours.
4. Fonts: not in this merge. P9B's notes section 5 gives the text-run interface for P9C (text redrawn at the window's resolution).

Left: a click where a panel used to be still reaches that panel (needs play switches in the game's hit tests); a tool preview drawn
at the cursor over a moved panel shows at the panel's old place; the hire dialog / title-menu Esc decisions.

## P9C merge (2026-11-04): text at the window's resolution
P9C.patch (made on c5e66b59) applied to 0bc71594, whose port/ equals c5e66b59's (the two commits between touch only CLAUDE.md,
re/history/ and scripts/gates.py). Every patched file equals port/build/P9C's except for line endings, so the worker's gates
(P9C_gates.log, P9C_notes.md section 4) are the merged tree's. Write-up: re/history/ROADMAP.md, P9C paragraph.
