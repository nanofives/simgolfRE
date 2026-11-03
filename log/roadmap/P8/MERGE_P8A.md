MERGED

# P8A: the loading banner in the game's own style (2026-11-03, brief log/roadmap/P8/BRIEF.md, notes P8A_notes.md)

Base b786c29a = HEAD at the merge, so P8A's six changed files were copied as they are (port/app/host.cpp, host.h, main.cpp,
present.cpp, present.h, port/src/sim/gfx_text.cpp). P6B's GDI box is gone. The banner is the gfx body `loading.banner` (text
family): the calls drawWindow makes for the ticker window (drawFrameR3 panel, the banner font g_font51b360, drawTextSurface's
colour), plus a progress bar in the panel's colours (no bar art exists outside showLoadingScreen, which draws two Random::range
values). It is drawn on a copy of the back buffer, so neither the back buffer nor the GameState is written. Standing rule from
Mariano (memory port-ui-uses-game-style): every UI the port adds uses the game's fonts and art.

Gates: build x64 / Win32 rc 0; segments x64 205 / 205 equal to I25 (P8A_segs_x64.tsv); app --selftest 37, --selftest-p5b 6,
--selftest-p7 9; test_gfx_text 32 (+20 oracle skips), test_app_shell 18, test_app_input 16, test_app_replay 14, test_app_ingame
(P8M_t_test_app_ingame.log); port test_static 6. Screenshots P8A_runs/dpi96/load_04.png, P8A_runs/scaled/load_04.png,
P8A_side_by_side.png. Not measured: sim hashes of a live session with the banner on against off (no write by construction).
P8B (widescreen) was briefed on b786c29a and merges over this with a 3-way merge (it owns gfx_text.cpp and present.* otherwise).
