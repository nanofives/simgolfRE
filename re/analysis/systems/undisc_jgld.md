# undisc_jgld: jgld.dll code Ghidra never made into functions (2026-10-09)

Only one of the 236 entries that `re/tools/undiscovered.py` found and `ghidra/scripts/CreateFunctions.java`
created (log/undisc_created.tsv) is in jgld.dll, which matches the module's survey numbers: jgld.dll is a debug
build, so its incremental-link thunk table already gives Ghidra an entry for almost everything.

## Font::deletingDtor_adj8 0x10069260

```
10069260  sub ecx, 8
10069263  jmp 0x10001753          ; the thunk of Font::deletingDtor
```

A this-adjustor thunk, reached only from slot 0 (+0x0) of the table at 0x1011da68, which `Font::Font`
0x10068660 and `Font::~Font` 0x10068760 store. The fixed 8-byte adjustment is the offset of the sub-object whose
vtable 0x1011da68 is: the thunk steps `this` back to the Font and tail-jumps to the deleting destructor, so the
secondary base's slot 0 behaves like the primary one.

This is the same shape as the eight `_adj274` thunks of golf_clean.exe's Window classes
(`re/analysis/systems/undisc_exe.md`, section 3) and the one `add ecx, 0x70` adjustor in sound.dll
(`undisc_sound.md`, section 2); the subtracted constant is the only thing that changes.
