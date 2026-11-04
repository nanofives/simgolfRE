# P9D: two layer-placement defects of P9B's HUD compositor (2026-11-04)

Mariano chose these as the next step after P9C. Both are P9B's layer placement and look the same with `--text-res frame`
(P9C_notes.md, last section). Both are the port's presenter, not the original.

1. HUD DRAWN TWICE IN MODAL REPORT FRAMES. Reference picture: log/roadmap/P9/P9C_shots/r1920x1080_cov_r_s2_screens_396_after.png
   (and _before.png, the same with frame text): the F-key report (Financial Report) is up; the title panel, the money / rating
   meters and the bottom band (radial menu, build palette) appear twice: once at their moved anchors (left / right edges) and once
   at their 800-space places inside the centred frame, dimmed under the report. Find, with evidence from the layer capture (which
   draw calls open which layer in these frames: the reports dim the whole in-game frame through black.pcx / drawAlpha, S2.8, and the
   HUD under it is the root's old content or is drawn again by the report's own refresh), why the centred layer still holds the HUD
   pixels there, and fix it so each panel shows once, at its anchor, dimmed the way the original dims it. Check every modal screen
   over the course that S2.8 / S2.9 / S2.11 cover (reports F1..F10, hole stats, best scores, end of year, hire, showPopup / system
   menu, textInputBox, course report page, tournament results, bulletin board, SimFoto) at 1920 x 1080 and 1280 x 720.
2. PALETTE TOOLTIP NOT MOVED WITH ITS PANEL. Reference: log/roadmap/P9/P9C_live/tip_top_7_1920x1080_textnative.png: the Tropical
   bush tip (drawn by showInfoCursorPanel's tool tip for the build palette, gfx_hud.cpp / S3.7) is at its centred 800-space place
   while the palette it belongs to has moved left with the bottom band. A tip belongs to the panel whose hover produced it: put it in
   that panel's layer (or anchor it with that panel's offset), clamped inside the window. Check every HUD tip (palette slots and
   tools, radial menu, toolbar, amenities, golfer action panel, employee panels, meters) at both sizes, and that a tip the original
   draws over another panel stays whole (P9B's fb_4 case).

Read first: log/roadmap/P9/BRIEF.md (rules), MERGE.md, P9B_notes.md (the compositor: GfxLayerScope, kHudPanels, compose, remap,
section 6 limits), P9C_notes.md (text runs ride on layers: keep them consistent), log/roadmap/P8/P8B_notes.md sections 7-8.
Rules: log/roadmap/P4/BRIEF.md "Rules", unchanged. NO-GUESSING with addresses / function names. Port code cites functions by name.

Tree: port/build/P9D = git archive 0ab75c1c (HEAD, P9C merged) + junctions (log/roadmap/P9/P9D_mirror.py; log/ is the shared tree's,
so write deliverables only under log/roadmap/P9/P9D_*). Do not commit. `~/bin/py -3.12`, no bare cmake (port/test/build.py from a
Python script, SG_FREETYPE_ROOT C:/Users/maria/vcpkg/installed/x64-windows or x86-windows), never `pytest port/test` in one process,
heavy runs through log/roadmap/S3/w8_lock.py run P9D -- ... and test-slot. If a change alters a default the app tests compare with
the original's recordings, gate it behind conftest's existing SG_APP_P9=off or add SG_APP_P9D=off.

Evidence: before / after screenshots at 1920 x 1080 and 1280 x 720 for every screen listed in 1 and 2; the 800 x 600 back buffer and
GameState unchanged (segments x64 TSV equal to log/roadmap/merges/I25/segs_x64.tsv on status and digest, framegate rows of
sandbox_basic / cov_b_location equal to P9C_framegate.tsv, trace / framehash equal with and without the fix), app selftests x64 +
Win32 (incl. p8b / p9a / p9b / p9c), a selftest or control for each fix that reads RED with the fix undone, the 8 app test files one
per process. Deliver log/roadmap/P9/P9D_notes.md, files list, logs, screenshots and P9D.patch against 0ab75c1c. Reply DONE or BLOCKED.
