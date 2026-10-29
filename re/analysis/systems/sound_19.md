# sound_19: callee names for the zero-fallback step S4.11 (2026-10-24)

sound.dll functions the S4 modules reach through the development fallback and that had no names row (re/names/sound_19.tsv,
evidence per row; step notes log/roadmap/S4/11_notes.md).

- `GlblHeader::init` 0x1002e3c0, `Sound::getField1b8` 0x10005f40, `MixBuffer::deletingDtor` 0x1002cf50 and
  `Sound::queueMidiCommand3` 0x10020480 are the four unnamed callees of loadSoundFile (0x100041d0) and
  Seq_applyTrackFlag: the first initialises the 12-byte header the chunk handler of tag dword 0x6c626c67 (compare at 0x1000441b) reads, the second and fourth are
  Sound vtable slots 0xb4 and 0x14, the third is slot 0 of vtbl_MixBuffer.
- The static CRT entries the modules call by address: the allocation chain `operator_new` 0x100424d7 / `_malloc`
  0x10042acd over `__nh_malloc` 0x10042adf and `__callnewh` 0x10045b80 (heap_alloc 0x10042b0b and free 0x10042c07 already
  have rows), the lock `__lock` 0x10047710 with the exit-table pair `_lockexit` 0x10045226 / `_unlockexit` 0x1004522f, the
  exit registration `_atexit` 0x10042647 over `__onexit` 0x100425c9, `_strrchr` 0x100424b0 and the eh vector destructor
  iterator `ehVectorDestructorIterator` 0x1004280e (its constructor twin 0x10042902 already has a row).
