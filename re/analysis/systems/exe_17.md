# exe_17: round 37 callee name and renames (2026-10-17)

Name work of batch b3 (documentation and names cleanup). Each row is checked against the disassembly of the anchored
binary; the evidence column of the row cites the instructions.

New name:

- `fillSlots54` 0x004a29a0 (this file, `exe_17.tsv`): a 19-byte thiscall that stores its one argument into the ten
  dwords at this+0x54 with `rep stosd` (0x004a29a8 / 0x004a29ad). Its only caller is TextView::render 0x0048fe60 (call
  0x0049038d). What the ten slots hold is not determined.

Renames applied in the existing rows (the old name described the function wrongly):

- `pickGolferName` 0x00467600 -> `appendWalkerName`: appends a walker or employee record's display name (record
  `index * 0x4c` of g_walkers 0x00585850) to the text buffer 0x0051a068.
- `golferScore` 0x00453260 -> `golferAge`: drawGolferInfoPanel 0x0045c560 prints the result after the string 0x004d3a0c.
- `updatePairSnapshot` 0x00409950 -> `appendPairTrackPoint`: appends one (x, y, height) point to the track of a pair slot
  of g_storyPairs; it does not copy a snapshot.
- `allocTextBuffer` 0x004767a0 -> `drawTextRun`: the body draws one counted text run; the allocation is a working copy.
- `ListBox::onNavKey` 0x0048aa00 -> `ListBox::onPress` (and `onNavKeyMarked` 0x0048a9c0 -> `onPressMarked`, with their two
  vtordisp thunks): the handler takes a point and handles Ctrl / Shift multi-selection.
- `codecCombFilter` 0x10040f60 (sound.dll) -> `codecRing5Add`: adds one word of a 5-word ring into another, no signal
  filtering.

Row text corrected without a rename: `resetSpriteQueue` 0x004627d0 (the next-array is not cleared by this function).
