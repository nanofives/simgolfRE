# P9E: the open layer-compositor fixes, one at a time (2026-11-04)

Mariano: "loop all fixes one by one". Do the queue below IN ORDER. After each fix, STOP and reply `FIX <n> READY` with a patch
`log/roadmap/P9/P9E_<n>.patch` against the previous fix's tree (fix 1 against cde8b72d, fix 2 against cde8b72d + P9E_1, ...), so the
orchestrator can merge and publish it while you continue. Keep your tree cumulative. Then go on with the next item without waiting
for an answer, unless the orchestrator sends you something.

Queue:
1. POPUP POSITION. showPopup popups and the System menu stay at their 800-space place, so with the moved HUD the System menu now
   overlaps the palette instead of the radial menu it belongs to (P9D_notes.md "Still open"). A popup opened from a HUD panel's control
   should keep its place relative to that panel (anchor with the panel's offset, clamped inside the window); a popup not opened from a
   panel stays centred. Clicks inside it must still reach it (remap).
2. HIRE CLOSE FLASH. The single frame that closes the hire dialog can show panels at their 800-space places (P9D_notes.md). Make that
   frame compose like its neighbours.
3. VACATED PLACE CLICKS. A click where a moved panel used to be (in 800-space, now showing world) still reaches that panel, because the
   game's own hit tests see it there (P9B_notes.md section 6, hitTestSlot432f90, nearestMenuSpot ...). Make a click in a vacated place
   act on the world, not on the panel: prefer an app-side solution (the remap sends such a click to a point outside every panel's 800
   rectangle only if that keeps the world point right; else a `play` switch in the hit tests fed by the panel offsets from a view-block
   word as G32's width). Pick the smallest one that is correct and say why.
4. TOOL PREVIEW AT OLD PLACE. What the game draws at the 800-space cursor point while the cursor is over a moved panel (P7's tool
   previews and labels when a tool is held over the palette) appears at the panel's old place (P9B_notes.md section 6). Anchor such
   centred drawing to the layer the remap used for the last cursor answer.

Read: log/roadmap/P9/P9D_notes.md, P9B_notes.md (compositor, section 6), P9C_notes.md (text runs ride on layers), MERGE.md.

TESTING IS LIGHT (Mariano's rule for play-test feedback, overrides older briefs): build x64 (Win32 only if you touch port/src/sim or
port/include); one smoke run of the app per fix (boots, reaches the situation, no crash) with one 1920 x 1080 screenshot of it after
the fix; segments x64 once per fix only if sim-side code changed (equal to log/roadmap/merges/I25/segs_x64.tsv). No 1280 x 720 sweeps,
no controls, no framegate, no full app test files. A sim-side `play` switch defaults to 1 (= original) and is set 0 in kPlayProfile
and re/port/play_profile.tsv (tests run under `fixed`, so they are unaffected). Short notes per fix in log/roadmap/P9/P9E_notes.md
(cause, fix, files, screenshot). App options for new behaviour default on and go off with SG_APP_P9E=off (add it to port/test/conftest.py
if you add one).

Tree: port/build/P9E = git archive cde8b72d + junctions (log/ is the shared tree's: deliver only under log/roadmap/P9/P9E_*). Do not
commit, do not publish. `~/bin/py -3.12`, no bare cmake (port/test/build.py from a Python script, SG_FREETYPE_ROOT
C:/Users/maria/vcpkg/installed/x64-windows), never `pytest port/test` in one process, heavy runs through log/roadmap/S3/w8_lock.py run
P9E -- ... and test-slot. If an item turns out to need a decision from Mariano, write the options in the notes, skip it and go on.
After item 4 reply DONE.
