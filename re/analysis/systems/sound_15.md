# sound_15: round 33 callee names (2026-10-13)

sound.dll callees named by batch c3fd (log/c3/c3fd_names.tsv, evidence per row), so their callers can pass the C3 gate.

- `Channel_ctorB` 0x10031be0 and `return0xb_32660` 0x10032660: the callees that blocked MultiSound_ctor /
  MultiSound_ctorWithType and MultiSound_setSlotSource.
- `VoiceSlot_dtorEmpty` 0x10037df0, `ChanRecord_init` 0x1001ccf0, `ChanRecord_freeBuffers` 0x1001cd30,
  `ChannelTable_callSlot4` 0x1001ae50, `StrPtr_assign` 0x1001b160, `NodeB000_ctor` 0x1000b000: callees of the
  Sound5c450 / Sound5b6a8 destructors, freeBufferList, Seq_setupChannel / Seq_updateChannel and Seq_buildTrack9/14
  (leads for the next round).
- `getField204` 0x1002d430: its whole body is `mov eax, [ecx+0x204]; ret`. Sound_lookupOrCreate's row in sound_3.tsv
  had read it as a string comparison; that row was corrected.
