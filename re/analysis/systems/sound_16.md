# sound_16: round 34 callee names (2026-10-14)

sound.dll functions named by batches c3fh and c3fk (log/c3/<batch>_names.tsv, evidence per row).

- `__ftol` 0x10042550 and `ehVectorConstructorIterator` 0x10042902 (FID `??_L@YGXPAXIHP6EX0@Z1@Z`): CRT callees of
  the x87 helpers c3fh hooks (Voice_resetDefaults, Sound5b6a8_ctor, the channel rate and gain functions).
- `NodeB000_initBase` 0x1002e3f0: the base initialiser NodeB000_ctor 0x1000b000 calls; it zeroes the first 0x20 bytes
  of the 0x38-byte track node and stores -1 at +0x20. c3fk hooks it.
- `ChannelTable_loadRange` 0x1001aef0 and `ChannelTable_releaseObject` 0x1001b0d0: Sound_lookupOrCreate's only
  unnamed callee and its callee. loadRange builds a MultiSound (plus member MultiSounds at +0x1440) for a channel
  range, loads it through vtable slot +0x10 and installs it in every slot of the range; releaseObject clears an
  object from the 0x80-slot table and deletes up to three members and the object through vtable slot 0.
