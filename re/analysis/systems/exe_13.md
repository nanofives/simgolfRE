# exe_13: round 30 renames (2026-10-12)

Names corrected by round 30's batches (re/C3_UI_PLAN.md), applied to their existing rows in re/names.

## Golfer sprite (batch c3ei)
- `composeGolferSprite` 0x00462020 (was formatGolferTitle) formats no text: it composes the layered golfer sprite from
  part-object and compositor slot calls and writes six bytes of the golfer type record back (log/c3/c3ei_purpose.md has the
  instruction addresses of each slot call and store).

## Root-window input handlers (batch c3el)
Eleven vtable slots of the root window class 0x004ba2fc (slots 29..39, +0x74..+0x9c; installer staticInit_404360
0x00404360) lay between 0x00465577 and 0x004658b0 outside every function of the master. They were created with
CreateFunctions.java (2026-10-12) and named from what each slot writes (rows in re/names/exe_13.tsv, evidence per row):
`rootWin_onPress1` / `onPress1b` / `onPress2` / `onPress2b`, `rootWin_onPoint` / `onPoint2`, `rootWin_onUnpoint` /
`onUnpoint2`, `rootWin_onKey`, `rootWin_onChar` / `onChar0`. modal.js (round 30) feeds scripted input through them.

## listPanel slot 28 (batch c3ej)
`listPanelSlot28` 0x0049e990 is slot 28 (+0x70) of the listPanel part's table 0x004bb744, reached through
listPanelSlot28_vtordisp 0x00491b70 and called by viewDispatchSlot28 0x004a0a10 for tag 0x10: it writes -1 to the two
selected-row words listPanelDraw reads.
