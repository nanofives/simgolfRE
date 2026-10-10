# exe_9: two optionDialog slot bodies (2026-10-09)

Named for round 23 (batch c3ct), whose vtordisp thunks optionDialogSlot28_vtordisp 0x0049ffc0 and
optionDialogSlot38_vtordisp 0x0049fff0 needed their jump targets named for the gate's anti-island callee check. Both
bodies sit in optionDialog's vtable 0x004bbeb0 (installed by viewFieldCtor49ebb0 0x0049ebb0) and are also reached from
the two-level switch dispatchers of re/analysis/systems/undisc2_exe.md (viewDispatchSlot28 0x004a0a10 and
viewDispatchSelectChanged 0x004a0c90, tag 1, this-0x168):

- `optionDialogSlot28` 0x0049fe20: clears the hover index optionDialogOnMouse 0x0049fda0 sets (-1 at vbase[2]+0xd8),
  then calls slot +0x120 of the Window base.
- `optionDialogSlot38` 0x0049fc30: copies vbase[1]+0x114 to 0x0083ab2c and, when the callback at this-0x10 is set,
  passes it the mask optionDialogOnMouse tests (vbase[2]+0xd4).

What slot +0x120 and the callback do is not determined by these bodies.
