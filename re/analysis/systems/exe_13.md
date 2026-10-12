# exe_13: round 30 renames (2026-10-12)

Names corrected by round 30's batches (re/C3_UI_PLAN.md), applied to their existing rows in re/names.

## Golfer sprite (batch c3ei)
- `composeGolferSprite` 0x00462020 (was formatGolferTitle) formats no text: it composes the layered golfer sprite from
  part-object and compositor slot calls and writes six bytes of the golfer type record back (log/c3/c3ei_purpose.md has the
  instruction addresses of each slot call and store).
