MERGED

# P4: gameplay fixes behind defect switches, profile `play` (decision G30, 2026-11-03)

Base 7f7860c5 (I25). Three trees: P4A (worker, money / reports / golfers / stories), P4B (worker, memory overruns), P4C (orchestrator:
profile `play`). P4A and P4B were merged into P4C with `git merge-file` against HEAD; the two conflicts (events.cpp, golfers.cpp) were
both-sides additions to the switch tables and were kept as the union. Testing light by G30.

## Profile `play`
- re/port/play_profile.tsv: the members `play` adds to every `fixed` member (17 rows); port/app/main.cpp kPlayProfile is its
  compiled copy. `play` is the app's default; `--profile original|fixed|play` and the env override SG_APP_PROFILE choose another
  (an explicit flag wins). port/test/conftest.py sets SG_APP_PROFILE=fixed for the port suite, so app tests that compare an app
  run with the original's recordings keep their pre-G30 behaviour. port/test/profiles.py knows `play` (members, validate, apply,
  current); test_profiles.py and test_defects_golfsfx.py's --profile-list check cover it.
- Members: the five existing switches (sinScaled.readPastTable, shotPower.cacheIgnoresPutt, chooseShotTarget.strictFlagSticky,
  prepareShot.hazardCellTransposed, narrateShot.maskNotRestored) plus twelve new ones:
  - P4A (P4A_notes.md): loadGolferRoster.namedStoryStride (D17), loadGolferRoster.skillsNextRecord (D19), mainLoop.cashLowWordOnly
    (D23; saturates at the int16 range: the history and ledger fields are int16 in the GameState and the save), showFinancialReport.
    sumWriteBack (D24), emitGolferThought.tileMoodWrap (D26), resetGolferState.singleHoleRecord (D44).
  - P4B (P4B_notes.md): spawnWalker.fullTable (D11), ballScreenList.overflow (D12), cornerLevel.unclampedAndShared (D21; clamps to the
    byte range, U-0209 open), loadStoryStrings.cacheEntryLimit (D47b), buildGolferComment.slotLimit (D48a), postEvent.idBound (D49a).
- Not in `play`: computeStoryReply.clampLowerBound (D48b). The switch exists, but the intended clamp is not determined from the code
  (the brief's `clamp(mood, 0, 0x63)` moves the story chapter from 19 to 2 on P4A's test state). D5 / D6 (grid-edge reads) have no
  switch yet: each of about 30 sites needs its own meaning for an outside cell (P4B_notes.md).

## Gates (log/roadmap/P4/P4_*)
| gate | result |
|---|---|
| build x64 / Win32 | rc 0 / rc 0, no warnings |
| segments x64 / Win32 (default switches) | 205 / 205 each, status and digest equal to I24's tables on every row |
| port test_static | 6 passed (after rewriting two raw addresses in a P4B comment as buildGolferComment+0x29cf / +0x2bbf) |
| test_profiles / test_defects_p4a / golfsfx --profile-list | 21 / 3 / 3 passed |
| app under `play` | course_season's corpus trace with 27 switches at 0 runs 1,426 frames, then waits for input in pumpAndRedraw (the original's recorded input no longer fits the diverged game, as intended); under `original` all 6,296 frames, framehash differs between the two |

Not run (G30): the full suites, boot, census, Win32 port tests.
