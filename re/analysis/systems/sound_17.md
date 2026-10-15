# sound_17: round 35 callee names (2026-10-14)

sound.dll functions named by batches c3fo and c3fu (log/c3/<batch>_names.tsv, evidence per row).

- `ehVectorCtor_finally` 0x1004296c and `__ArrayUnwind` 0x1004288e: sound.dll's copies of the eh vector constructor
  iterator's `__finally` funclet and of the routine that unwinds a partly built array.
- `dynInit_soundWindowObj100b49b8` 0x100091c0 and `SoundWindowObj_ctor` 0x10028600: created in the master this round.
  The first is the C++ dynamic initializer in slot 66 of the CRT initializer table 0x10063000. It constructs the static
  object 0x100b49b8 with the second, which stores the vtable 0x1005b8ac, then registers atexitDtor_100b49b8 0x100091f0
  with _atexit. That makes the initializer the only reference to atexitDtor_100b49b8.
