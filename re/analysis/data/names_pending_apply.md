# Names pending ApplyNames (batch b3, 2026-10-17)

The rows below were changed in `re/names` and in `hooks.csv` (through `scripts/re_classify.py retag`, levels unchanged,
one CHANGELOG line each). The Ghidra master still carries the old names: batch b1 owned the master while these were
written, so ApplyNames was not run. To apply (parent, after b1):

```
<ghidra>\support\analyzeHeadless.bat C:\Users\maria\Desktop\Proyectos\SimGolf SimGolf -process golf_clean.exe -noanalysis ^
  -scriptPath ghidra\scripts -postScript ApplyNames.java re\names overwrite
```

and the same for `sound.dll` (one row). `ApplyNames` with `overwrite` renames exactly the changed rows (it keeps an equal
name whatever its source). The new function in the last section is not in `hooks.csv`.

## Renamed rows (module golf_clean.exe unless noted)

| address | old name | new name | file of the row | what the old name got wrong |
|---|---|---|---|---|
| 0x00467600 | pickGolferName | appendWalkerName | exe_2.tsv | indexes `g_walkers` 0x00585850 (stride 0x4c) and appends the record's name to the text buffer 0x0051a068; it does not pick a name and reads no golfer record |
| 0x00453260 | golferScore | golferAge | exe_1.tsv | drawGolferInfoPanel 0x0045c560 prints its result after the text `, age ` (0x0045c7e0..0x0045c81e) |
| 0x00409950 | updatePairSnapshot | appendPairTrackPoint | exe_6.tsv | appends one (x, y, height) point to a pair slot's three 32-entry arrays; the table 0x00579594 its row cited is field +0xdc of record 0 |
| 0x004767a0 | allocTextBuffer | drawTextRun | exe_2.tsv | the body draws one counted text run through the surface slots +0xa8 / +0xb8 / +0xac; the malloc is a working copy |
| 0x0048aa00 | ListBox::onNavKey | ListBox::onPress | exe_4.tsv | takes a point (x, y) and handles Ctrl / Shift multi-selection on a row; it is not a keyboard handler |
| 0x0048a9c0 | ListBox::onNavKeyMarked | ListBox::onPressMarked | undisc2_exe.tsv | wrapper around slot 30 (onPress) |
| 0x0048ccb0 | ListBox::onNavKey_vtordisp | ListBox::onPress_vtordisp | undisc_exe.tsv | this-adjustor thunk of onPress |
| 0x0048ccc0 | ListBox::onNavKeyMarked_vtordisp | ListBox::onPressMarked_vtordisp | undisc2_exe.tsv | this-adjustor thunk of onPressMarked |
| sound.dll 0x10040f60 | codecCombFilter | codecRing5Add | sound_8.tsv | adds one word of a 5-word ring into another and returns it; no signal is filtered |

`hooks.csv` also carried four legacy hook-registry names that differed from the names TSVs; they were retagged to the
TSV names (the TSV rows are backed by the string and callee evidence listed in `re/names`):

| address | hooks.csv name before | now |
|---|---|---|
| 0x00432720 | OptionsMenuBuild | showSystemMenu |
| 0x0044cff0 | CareerRetirementFlow | showEndOfYearScreen |
| 0x0045c560 | GolferPanelEject | drawGolferInfoPanel |
| 0x004722c0 | LandmarkAvailableNotice | announceLandmark |

The master already holds the TSV names for these four, so ApplyNames changes nothing for them. The SG_HOOK names in
`shim/src/re/*.cpp`, the registry keys (`re/frida/registry.d`) and the note file names under `re/analysis/` keep the old
spellings (the shim sources were not edited by this batch).

## Row text corrected without a rename

- `resetSpriteQueue` 0x004627d0 (exe_2.tsv): the loop at 0x004627ea..0x004627f6 stores -1 into the single dword
  0x008371f4 1,024 times; the next-array is not cleared by this function.

## New function, not yet tracked

- `fillSlots54` 0x004a29a0 (`re/names/exe_17.tsv`, writeup `re/analysis/systems/exe_17.md`): a 19-byte thiscall that
  fills the ten dwords at this+0x54 with its argument; the only caller is TextView::render 0x0048fe60 (call 0x0049038d).
  The function exists in the master as `FUN_004a29a0` and has no `hooks.csv` row. After ApplyNames: `re_classify.py batch
  <tsv> --to C1`, `c2_note.py --all --out <tsv>` and `batch --to C2` (the round 14 recipe in CLAUDE.md).

## Decisions on the other leads

| lead | decision | reason |
|---|---|---|
| log/c3/c3eq_names.tsv fillSlots54 | APPLY (exe_17.tsv) | `ret 4` thiscall, `rep stosd` of 10 dwords at this+0x54 (0x004a29a0..0x004a29b0) |
| log/c3/c3et_names.tsv drawTextRun | APPLY | calls at 0x00476944, 0x00476ade, 0x00476c26, 0x00476ca0, 0x00476ce5 |
| log/c3/c3fi_names.tsv `??_L@YGXPAXIHP6EX0@Z1@Z` for 0x004a55d2 | REJECT | that is the decorated symbol; the readable name ehVectorConstructorIterator stands |
| round 31 / 32 notes: codecCorrelationLags 0x10040330 (ivfilt_), codecGainSmoother 0x1003f310 (dyptrk_) | DEFER | the f2c names in log/c3/m36c_D2_notes.txt are a spelling guide (the note says no source copy was compared); a behavioural rename needs the 0x1003f310 and 0x10040330 bodies read field by field |
| g_zoomLevel 0x00822c8c | already applied | g_screenWidth / g_screenHeight in exe_2_globals.tsv |
| ListBox::onNavKey (round 23 lead, "name to revisit") | APPLY | see the table above |
