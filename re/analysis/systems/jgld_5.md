# jgld_5: the tracked-object list's teardown registration (2026-10-10)

Named for round 24 (batch c3dc). jgld keeps every Tracked object (Surfaces, Sprites, Palettes, Displays) in one
module-wide list at 0x10128428 (re/analysis/systems/jgld_4.md, constructTrackedList 0x100069b0). The compiler-generated
static initializer `registerTrackedListTeardown` 0x100069f0 (`_$E3`, matched in re/match/jgld_list.cpp) registers
atexitDestroyGlobalList 0x10006a40 with _atexit 0x1007f080, so the list's remaining objects are deleted at module exit.
Its own caller is 0x10006970.
