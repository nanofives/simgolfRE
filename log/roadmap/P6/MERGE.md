MERGED

# P6 + P7: Mariano's second play-test feedback (2026-11-03, brief log/roadmap/P6/BRIEF.md)

Base f04c7591. Worker trees P6A (simulation items, P6A_notes.md), P6B (app items, P6B_notes.md) and P7 (widescreen stage 1,
log/roadmap/P7/P7_notes.md, PLAN.md) merged in port/build/P6M with `git merge-file` against HEAD. Conflicts in host.cpp, host.h,
main.cpp, present.h and tick.cpp were all additions on both sides; both were kept. One fix after the merge: P7's side-band hint
(host.cpp LiveSession::track_screen) treated a pump as the title menu only when g_difficulty == -1, which only the boot path
stores (0x0040f8ab, P6B item 1), so after a game the title menu kept black bands. It now also clears on P6B's title_seen_ (the
title menu's own notification). The profile row hoverTip.tenFrameDwell (P6B_profile.tsv) was added to re/port/play_profile.tsv.

| item | class | result |
|---|---|---|
| 0 X closes straight to Windows | P5B bug (its title test used g_difficulty == -1) + app | `--close-prompt` now leaves through the game's own title-menu exit: after the in-game prompt (save / quit) the title menu's first pass clicks its exit icon, so top10.sve and Default.pro are written as on a normal quit and the window closes; on the title menu X exits at once (window hidden) |
| 1 Gary Golf walks off the map | original (updateWalkers kind 0xfa follows the unbounded cursor tile; pathStep keeps the unclamped direction) | play switches updateWalkers.cursorTargetOffGrid, updateGolfers.walkOffGrid |
| 2 camera off the map | original (only H1's arrow keys / edge pan clamp; every follower writes unbounded) | play switch mainLoop.cameraFollowOffGrid: every writer clamps the centre to 0..0x31 as H1 does; at the edge black around the land still shows, as in the original |
| 3 map loading indicator | app | `--loading-overlay` (default on): a `Loading...` box with a bar during the world load (window DC only). The load is about 1.5 s; flic sheet decoding is 60-65 % of it (lead: decode on first draw) |
| 4 quicker build hover tip | original (count > 10 frames, about 1.0 s, 0x00433257 and five more panels) | play switch hoverTip.tenFrameDwell: tip after 3 frames (about 0.37 s) |
| 5 widescreen | app, stage 1 | `--wide` (default OFF until Mariano decides): 16:9 window, the 4:3 frame centred, bands filled from the frame's edge colours (`--wide-fill soft|clamp|black`) on the 2D screens before a game; black bands in game. Stage 2 (UI moved to the sides, a wider course view) needs the plan in log/roadmap/P7/PLAN.md |

Env switches for tests: SG_APP_P5B=off, SG_APP_P6B=off (port/test/conftest.py sets both), SG_APP_WIDE.

Gates (log/roadmap/P6/P6M_*): build x64 / Win32 rc 0 (0 warnings x64); segments x64 205 / 205, 0 rows differ from
log/roadmap/merges/I25/segs_x64.tsv; app --selftest 37, --selftest-p5b 6, --selftest-p7 9; port test_static 6, test_profiles 21,
test_defects_p6a 3, test_app_shell 18, test_app_input 16, test_app_replay 14. Worker gates in P6A_* / P6B_* / P7/P7_*.
