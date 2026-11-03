MERGED

# P8B: widescreen stages 2 + 3 with native-resolution rendering (2026-11-03, brief log/roadmap/P8/BRIEF.md, decisions G32, notes P8B_notes.md)

P8B's tree (base b786c29a) merged over HEAD bfd4ce98 (which holds P8A) in port/build/P8M by 3-way `git merge-file` per file of
P8B_files.txt (29 files) against b786c29a. Two conflicts, both additions: host.cpp's includes (both kept) and present.h's members (P8B's kept,
P6B's loading_done_ / loading_expected_ dropped because P8A removed their users). re/port/play_profile.tsv gains P8B_profile.tsv's two rows.

What it does (details P8B_notes.md sections 7-9):
- `--wide` ON by default (SG_APP_P8B=off and `--wide off` restore the old app; conftest sets SG_APP_P8B=off). The view width W follows the
  window during a session; the host writes it into a view block outside the GameState at a frame head as a recorded write, so replays apply it.
- The world (terrain, scene sprites with side-column scenery, overlay lines) is rendered at the window's native pixel size (`--render-scale auto`).
- HUD compositor: title panel to the left edge, meters to the right edge, the bottom band (radial menu, System button, palette and the panels drawn
  there, golfer and employee panels included) to the left edge; ticker, dialogs and reports centred; panels scaled with window height; clicks over
  a moved panel remapped to 800-space, so no click handler changed.
- `play` switches worldToScreen.wideViewVisibility and tileToScreen.wideViewVisibility (G32 item 1): the sim's visibility test and the scenery window
  cover the whole wide view; default 1 keeps `original` / `fixed` identical.

Merge gates (log/roadmap/P8/P8M2_*): build x64 / Win32 rc 0; segments x64 205 / 205, 0 rows differ from I25 (P8M_segs_x64.tsv); app --selftest 37,
--selftest-p5b 6, --selftest-p7 9, --selftest-p8b 11; port test_static 6, test_profiles 21, test_app_shell 18, test_app_input 16, test_app_replay 14,
test_app_present 3, test_app_ingame 14. Combination no worker ran: P8A's loading banner under P8B's wide default, captured live at 1600 x 900
(P8M_runs/banner_wide2/load_04.png, driver P8M_banner_wide.py): the location screen fills the window with its edge bands and the banner is centred.

Open (P8B notes): golfer / employee panels sit in the bottom band so they went left, not right; a click in a panel's vacated place still reaches the
game as a click on that panel; overlay lines inside a panel rectangle move with the panel and a tooltip across a rectangle edge is split; a walker name
label that crosses the title panel's rectangle is cut (seen on Gary Golf in G32_sandbox_play_hud_1920x1080.png). Not run: test_app_live, test_app_audio.
