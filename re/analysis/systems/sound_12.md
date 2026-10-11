# sound_12: Seq_rewindTracks' list walker (2026-10-11)

Named for round 27 (batch c3do). `ListValues_callSlot18` 0x1001bc00 is the only callee of Seq_rewindTracks besides its
own frame checks (call at 0x1001e912 through the incremental-link thunk 0x100022ed): it resets the list cursor +0x14 to
the head +0xc and calls vtable slot +0x18 on every non-NULL value until it reaches a NULL node, NULL next node or NULL
value. The other 16 names c3do proposed rename existing rows (the mixer bodies and dsEnumCallbackAppend) and are applied
with the round's other renames.
