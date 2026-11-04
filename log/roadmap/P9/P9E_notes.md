# P9E: the open layer-compositor fixes, one at a time (2026-11-04)

Tree port/build/P9E (git archive cde8b72d + junctions, P9E_mirror.py). Brief: P9E_BRIEF.md. Patches: P9E_<n>.patch against the tree after
fix n - 1 (P9E_1 against cde8b72d), made by P9E_patch.sh (files list and an apply check in P9E_<n>_files.txt; snapshots in P9E_snap/).
Light testing (the brief): builds, one smoke run with a 1920 x 1080 screenshot per fix, segments x64 when sim code changed
(P9E_gates.sh -> P9E_<n>_gates.log). Nothing committed or published.

## Fix 1: popups at their panel's place
Cause: showPopup / textInputBox (the System menu is showSystemMenu's showPopup(0xfa, 0x154), commands_menus.inc system_popup) draw
into the root at 800-space places. With P9D's carry their drawing is the centred gap layer of the carried frame, composed at the
frame's place, while the radial menu they belong to is composed at the left edge: at 1920 x 1080 the System popup sat 240 client px
right of the radial menu, over the palette's old place (P9E_shots/r1920x1080_cov_r_quit_248_before.png).
Fix (`--hud-popups on`, the default with --hud-modal; off = P9D; SG_APP_P9E=off, set in port/test/conftest.py):
- sim, records only (gfx.h gfx_set_hud_popups / gfx_hud_popup_begin / _end; gfx.cpp, gfx_body.h, gfx_terrain.cpp,
  commands_popup.inc DialogMark): every showPopup / textInputBox body is a PIECE. Its opener is (1) the panel layer open when it
  starts (a popup a panel's own code opens), else for a click (g_rootInput's button word set) (2) the piece that owns the clicked
  pixel (a popup opened from a popup goes with it: Rename Your Course's textInputBox is drawn over the System popup by design), (3)
  nothing when the clicked pixel was drawn since the last boundary outside every popup (a modal screen, a label), (4) the topmost
  panel layer with a pixel at the click (the System button -> the bottom band). A key-opened popup has no click (the quit popup
  after the System menu's Exit: mainLoop clears the button word at ML+0xee78 before it), so it stays centred. A per-pixel owner
  plane (the piece open when the root pixel last changed, observed at each piece boundary and publish) is published with the
  frame's pieces and their boxes. The pieces end at the next frame's picture (terrain pass, its restore, window paint). The frame,
  the root, the back buffer and the GameState are not touched, and no drawing call is added, so with capture off nothing changes.
- app (present.h / present.cpp, main.cpp): in a centred gap layer that carries popups, the pixels a piece with an opener owns are
  composed at the opener's anchor offset (left -origin_x, right + the right band), clamped so the piece's box stays in the window,
  and are their own remap layer at that offset (clicks and hovers inside the moved popup reach it at its 800-space place). P9C runs
  of such a layer move with the piece that owns their box's centre.
Checks (P9E_1_gates.log): build x64 and Win32 0 errors 0 warnings; segments x64 205 / 205 PASS, 0 rows differ from
log/roadmap/merges/I25/segs_x64.tsv (P9E_1_segs_compare.log). Smoke: live-trace of cov_r_quit (the app, 1920 x 1080,
`fixed`, P9E_shots.py; before = --hud-popups off), 735 presents, rc 0, stop quit, both modes.
Screenshot: P9E_shots/r1920x1080_cov_r_quit_248_after.png (the System popup next to the radial menu at the left edge, as in the
original's frame, its text redrawn with it; the quit popup above it, key-opened, centred), _pair.png = before / after.
_389: the save-name box opened from the quit popup stays centred, the System popup's leftover stays with the radial menu.
