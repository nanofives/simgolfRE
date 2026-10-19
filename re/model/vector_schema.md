# Function-vector corpus: the `.vec` format (schema `simgolf-vec/1`, phase 2.5, 2026-10-18)

Writer: `re/tools/vector_capture.py build`. Reader / validator: `re/tools/vector_lib.py` (`read_vec`, `validate_doc`).
Files: `port/corpus/vectors/<function>.vec`, local only (decision 5 of re/PHASE23_PLAN.md: the corpus holds game data).
Index: `re/port/vectors_manifest.tsv` (committed). How the vectors were made: re/analysis/data/vector_corpus.md.

A `.vec` file is ONE JSON document, gzip-compressed (gzip mtime 0, so identical content gives identical bytes). It holds
every captured vector of one golf_clean.exe function, grouped by registry key (re/frida/hooks_registry.py: one key = one
fixture and one vector list).

## 1. Locations

Memory is never addressed by an original address, except in members named `tooling_*`. A location is an object with
exactly one of these members:

| member | meaning |
|---|---|
| `{"sym": S, "off": n}` | byte `n` of GameState member `S` (port/include/gamestate_meta.inc; a gap is `gap_<offset6>`, its generated member name) |
| `{"block": k, "off": n}` | byte `n` of fixture block `k` of this key (see 3) |
| `{"exe": ".rdata", "off": n}` | byte `n` of a section of the user's own golf_clean.exe (constants, string literals, code) |
| `{"null": true}` | the pointer value 0 |
| `{"raw": va}` | an address none of the above covers (none in the corpus as built; reserved for tooling) |

A REGION is a location plus `"hex"` (the bytes). A region lies inside one GameState member or gap: the writer splits a
byte run at member boundaries.

## 2. Document

```
{ "schema": "simgolf-vec/1",
  "function": "Random::next",               # hooks.csv name
  "module": "golf_clean.exe",
  "tooling_va": "0x0045c1a0",               # tooling only: the function's address in the original
  "subsystem": "util", "tier": "util_callee", "level": "C4",   # from re/port/vector_selection.tsv
  "exe_sha256": "E74E...6789",              # the anchored golf_clean.exe the emulation ran
  "gamestate_meta_sha256": "<16 hex>",      # the address map the symbols come from
  "keys": [ <key>, ... ] }
```

## 3. Key

```
{ "key": "Random_next", "batch": "core", "fixture": "rng_seeds",
  "abi": "thiscall" | "default" (cdecl) | "mscdecl" (cdecl) | "stdcall" | "fastcall",
  "ret": Frida return type ("int", "uint32", "uint8", "int8", "uint16", "bool", "pointer", "double", "float", "int64", "void"),
  "args": [Frida argument types],           # thiscall: args[0] travels in ecx; fastcall: args[0..1] in ecx, edx
  "fpcw": 639,                              # x87 control word of the calling thread at capture (0x027f)
  "x87": true,                              # x87 instructions in the function or its static callee closure (direct
                                            #   call / jmp targets; virtual callees are not followed)
  "uses": ["freeze", ...],                  # stand-ins / helpers the fixture used (information; see vector_corpus.md 3)
  "imports": [],                            # imports the emulated original reached (a reached import fails the vector)
  "callees": ["FUN_...", ...],              # the static callee closure (same rule)
  "stood_in": [names], "stood_in_reached": [names],   # functions the fixture had replaced at capture / that the key's
                                            #   vectors' callee closure contains
  "unstubbed": via the key name suffix `+unstub`: the same registry key captured with the fixture's stand-ins on
                                            #   selected (sim-scope) functions reverted, so the original ran its real callees
  "pointer_objects_read": [S],              # seed objects of model class `pointer` / with pointer words (original addresses)
  "blocks": [ {"id": 0, "role": "fixture" | "page", "size": n, "hex": "...", "tooling_live_base": va}, ... ],
  "relocs": [ {"at": <location>, "to": {"block": k, "off": n}}, ... ],
  "seed":  [ <region>, ... ],               # data-section bytes ANY vector of the key read, at the key's start
  "const": [ {"exe": ".rdata", "off": n, "hex": "..."} ],   # exe bytes read outside the data section (copied for checks)
  "registry_state": [[base, off, size]],    # the registry's `state` regions (tooling; what the C3 A/B compared)
  "capture": {"captured": iso time, "winmm": "<16 hex of the shim>", "seconds": s},
  "relocs_proven": true,                    # every ok vector reproduced again with every block moved (see 5)
  "coverage": {"jcc": n, "both": n, "one_side": n, "none": n, "open": ["+0x1c:taken", ...]},
  "distinct_results": n,                    # distinct (ret, out) pairs over the ok vectors
  "vectors": [ <vector>, ... ] }
```

Blocks are the memory a fixture allocated (`role: fixture`) and pages captured behind pointers that were neither data,
module nor block (`role: page`, 4 KiB, kept only when a vector read or wrote them). A harness allocates every block
anywhere, copies `hex` in, then writes each relocation: the dword at `at` becomes the address of `to`. `at` is a block
location or a data-section location (`sym`).

## 4. Vector

```
{ "i": 0,                                   # index in the registry key's vector list
  "status": "ok" | "live_error" | "emu_error" | "ret_mismatch" | "write_mismatch" | "concurrent_input" | "not_emulated",
  "detail": "...",                          # when status is not ok
  "args": [ {"int": n} | {"float": x} | {"int64": n} | {"ptr": <location>} ],
  "ret": n | x | null (void) | {"ptr": <location>} | {"float_repr": "nan" | "inf" | "-inf"},
  "in":  [ <region> ],                      # bytes this vector read whose value differs from the seed (rare: data the
                                            #   running menu changed between vectors)
  "reads": [ [S | "#k", off, len] ],        # every data / block byte range this vector read (no bytes: they are in seed / in / blocks)
  "out": [ <region> ],                      # every byte the call wrote outside the stack, final value (data and blocks)
  "out_ptrs": [ {"at": <location>, "to": {"block": k, "off": n}} ],   # written dwords (any alignment) that point into a
                                            #   block: compare them as locations; their bytes in `out` are the capture's
  "stack_pop": n,                           # bytes removed beyond the return address (`ret n`)
  "blocks_executed": n,                     # basic blocks of the function's own body executed (a cost measure)
  "reloc_check": true }                     # this vector reproduced with the blocks moved (absent when the key has no block)
```

Only `status: ok` vectors are test vectors. Their `ret` is the LIVE original's return value (Frida's normalisation of the
type: `int` signed 32-bit, `uint32` / `pointer` unsigned, `uint8` the low byte, `double` the x87 st(0) rounded to double);
`out` is the emulated original's write set, which the build checked byte for byte against the live call's changes.

## 5. How a harness runs one vector

1. GameState = all zero; write the key's `seed`, then the vector's `in`.
2. Allocate the blocks, copy them, apply `relocs`.
3. Set the x87 control word to `fpcw` (matters only when `x87`).
4. Call the function with `args` (a `ptr` location resolved to the harness's own address).
5. Compare the return value with `ret` (a pointer as a location) and, for every `out` region, the bytes now at that
   location, where each `out_ptrs` dword must hold the harness's address of its `to` location instead. A stricter check also compares the set of bytes written (`re/tools/vector_validate.py emu` does).

Constants read from the exe (`const`) come from the user's own golf_clean.exe in the port; the corpus copies them only
so a check can tell a constant that moved.
