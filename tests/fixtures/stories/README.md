# stories5.sve (not committed: game-derived)

A sandbox save on Ocean's Edge MC with 5 short par-3 holes (55-103 yards): tee, fairway and green each,
built so the holes do not cross, all within one screen. Purpose: give golfer-pair stories room to advance
(FUN_00466370 needs hole >= 2; the landmark notice needs both partners at chapter 4, UNCERTAINTIES U-0003).

Rebuild: Sandbox Mode, then with the Build Course palette: fairway clicks first (avoid the ends, a fairway
click on a green replaces it), then green, then tee, right click to drop the tool, `H` to open the hole.
Save with wrench -> "Save the Current Game" (the folder `original\saved games` must exist), name it
`stories5`. Load with `scenario.load_saved_game(g, scenario.STORIES_FIXTURE)` then
`scenario.view_stories_course(g)` (the camera does not come back to where it was).

# stories9.sve (not committed: game-derived)

Scotland (Harold's Keep): 9 short holes, 35-103 yards, 5 on the left field and 4 on the right, built at
normal zoom in one screen (the Build Course palette resets the zoom). Load with
`scenario.load_saved_game(g, scenario.STORIES9_FIXTURE)` then `scenario.view_stories9_course(g)` (no zoom: a
zoomed-out 25 min run never passed stage 1; at normal zoom the tees of holes 1-5 sit just left of the rect). Why 9: a story needs 4 chapter advances in one visit and each hole
from the 2nd gives at most one chance; the 5-hole course topped out at chapter 3 in 25 min at x32.
Greens placed next to another green are refused silently: check the "Hole #N ... is now open" message.

# mood5.sve (not committed: game-derived)

Ocean's Edge MC: 5 par-3 holes of ~90 yards, each with a fairway painted between tee and green (fairway clicks
from 20% to 80% of the tee-green line every ~20 px, then green, then tee, right click, `H`), built to keep golfer
moods up (re/analysis/golfers/00406670_membership.md, "What drives mood down"). Hole 5's green was refused on
the first click and placed again. Saved with wrench -> "Save the Current Game" (Backspace over the default
name, type `mood5`, Enter). Load with `scenario.load_saved_game(g, scenario.MOOD5_FIXTURE)` then
`scenario.view_mood5_course(g)` (camera tile (28, 15), read from 0x4c2ba0/a4 when the save was made).
