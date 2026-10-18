# Phase 2.2: the generated GameState and the checkpoint tracer (2026-10-18)

Step 2.2 of re/PHASE23_PLAN.md. Decisions 1-5 of its section 7 apply (C++17 `portsim` in `port/`, a generated GameState
that reproduces the model's data layout, bit-exact, corpora local only). Running notes: `log/p2/p22_notes.txt`. Every
number below is from a run or a generator output listed there.

## 1. Model fix: the `pointer` column (U-0604)

`re/model/addresses.tsv` has an 18th column `pointer`, written by `re/tools/model_merge.py merge`:

| value | meaning | rows |
|---|---|---|
| `type` | the type holds a pointer by phase 2.1's rule (`re/frida/restart_ab.py` is_pointer_object: a `*` type, `ptr` / `vtable` / `vbtable`, or a struct with a `+0x.. ptr` / `vptr` member, propagated) | 304 (195 top-level .data objects, 2,863,036 bytes: phase 2.1's set) |
| `words:+0x..` | plain-typed object whose listed dwords hold heap pointers or handles (model_merge.py POINTER_WORDS, evidence per word) | 5 objects, 12 dwords |
| `-` | neither | 814 |

The 12 dwords are U-0604's: g_rootWindow +0x11c, +0x120, +0x124, +0x128, +0x14c (View fields View_ctor zeroes at
0x00479e49..0x00479e5b / 0x00479dcd; the heap value's writer is not determined, U-0610), +0x278 (the window Image's
`surface`), +0x378 and +0x518 (the `entries` arrays of the Image's two HotLists); g_listPalette (an object with a vtable,
loadImage 0x00475987), g_msgBoxLabelA / B (_malloc'd strings, MsgBox::setLabelA 0x0048de42), g_graphsyObject
(LoadLibraryA's handle, 0x004a00b2). The types were not changed: marking SgView's members `ptr` would flag every window
class through the propagating rule and change phase 2.1's measured set. The first 17 columns of the map, simgolf_model.h
and model_ctypes.py are unchanged (re/model/DECISIONS.md, 2026-10-18 phase 2.2).

## 2. The generator (`re/tools/model_state_gen.py`)

Inputs: the merged map (top-level rows inside golf_clean.exe's .data, 0x004c1000 + 0x381174), the merged header
`re/model/include/simgolf_model.h` (types, through model_ctypes_gen.py's C reader for sizes), and
`re/analysis/data/premise_check_hidden_state.tsv`. Outputs (generated, deterministic, committed):

- `port/include/gamestate.h`: `namespace portsim { struct GameState { ... }; }` under `#pragma pack(1)`, one member per
  model object at its offset with its model type (c_type, else element type; the original's pointers are `uint32_t`, so
  the layout is the same on x86 and x64), a `uint8_t gap_<offset>[n]` member for every run of bytes no object covers,
  `static_assert(sizeof(GameState) == 0x381174)` and an offsetof + member-size assert per member. No original address
  appears in it (checked with model_merge.ADDR_RE); member comments carry the offset, the class, the hidden-state code,
  `saved` and the pointer words. Some symbol NAMES contain an address without `0x` (f_w_4c2848, unk_005a6d40): they are
  the model's symbols, renamed only in the model.
- `port/include/gamestate_meta.inc`: X-macro table `SG_OBJECT(symbol, offset, size, class, pointer, hidden, saved)`,
  `SG_POINTER_WORD(symbol, offset_in_object)`, `SG_GAP(offset, size)`; every macro is #undef'd at its end.
- `port/include/gamestate_addresses.inc`: `SG_DATA_SECTION(va, size)`, `SG_ADDRESS(symbol, va)`; TOOLING ONLY (the image
  parsers, the tests); port code never includes it.

Classes (first rule that applies): (1) the hidden-state table, matched by va and size: H0 / H1 / H2 rewritten-before-use /
H7 -> `sim`, H2 stale / H5 / H8 -> `hidden`, H3 -> `ui`, H4 -> `render`, H6 -> `audio`; (2) a .sve field -> `sim`;
(3) audio types (Snd, NodeList, NodeListBucket, VoiceRx) or names (sound, music, snd, voice, midi, wave, audio, resample)
-> `audio`; (4) pointer column `type` -> `pointer`; (5) the rows stream G1 typed itself, by a table in the generator;
(6) by stream: B / C -> `sim`, D -> `render`, E1 -> `ui`, E2 -> `engine`, F -> `const`. `engine` and `const` were added
to the five classes the brief named: stream E2's engine classes and stream F's constant tables are neither UI nor
simulation.

| class | objects | bytes | compared by default |
|---|---|---|---|
| sim | 203 | 518,308 | yes (checkpoint hashes and frame-head hashes) |
| hidden | 18 | 14,594 | checkpoint hashes only |
| ui | 122 | 13,093 | checkpoint hashes only (12 pointer words masked) |
| render | 67 | 120,424 | checkpoint hashes only |
| audio | 13 | 33,028 | no (per-process: g_sounds, g_resampleBuf) |
| pointer | 186 | 2,821,872 | no (per-process) |
| engine | 51 | 11,065 | checkpoint hashes only |
| const | 329 | 90,789 | checkpoint hashes only |
| gaps | 425 | 51,311 | no (phase 2.1's unmodeled bytes) |
| total | 989 objects + 425 gaps | 3,674,484 | |

The pointer class has 186 objects, not 195: nine type-pointer objects take an earlier rule, g_sounds (H6) and
g_resampleBuf, g_musicDevice, g_soundHead / Tail / Fns, g_voiceDevice (audio), g_spriteObj and g_spriteSnapObj (H4 render;
phase 2.1 found exe .bss addresses in them, the same in every process, and both were equal between OFF runs at every
checkpoint). Their `pointer` field in gamestate_meta.inc stays `type`, so a consumer can still exclude them.

Excluded from every default comparison: 186 pointer objects, 13 audio objects, 12 pointer dwords and 51,311 gap bytes,
2,906,259 bytes in all (79 % of the section, almost all of it the sprite / surface / Flic pools). Every one of the 123
hidden-state rows with an address matched an object. One row has no header type (g_maskSpriteTable,
`struct {int16 flic; int16 variant}`) and is a byte array (U-0612).

`tests/test_gamestate_gen.py` (static): the pointer column against phase 2.1 (195 objects, 2,863,036 bytes, the 12
flag-gap dwords), the files current and two generations identical, no address literal, the layout tiling the section with
every offset equal to va - 0x004c1000, and MSVC C++17 /W4 /WX compiles for x86 and x64 of gamestate.h plus a probe that
asserts `offsetof(GameState, s) == va - 0x004c1000` for every SG_ADDRESS / SG_OBJECT pair; a wrong offset assert fails.

## 3. State images (`re/tools/state_image.py`)

A state image is the GameState as bytes plus a KNOWN mask. Parsers: `full` (a whole .data dump: scenario_ab's
`<name>.full.bin`, restart_ab's `.full.bin` / `.pre.bin` / `.fh.bin`, whose first 0x381174 bytes are .data), `dump` (a
scenario_ab checkpoint dump: the .sve FIELDS, both tail blocks, the two extra words), `sve` (a save through
model_ctypes.split_sve / join_sve over sve_fields.tsv, which must round-trip the file; the last 100-byte header lands in
g_textBuffer) and `state` (its own `.state.bin` / `.state.json`). The object table comes from the generated
gamestate_meta.inc / gamestate_addresses.inc. Object hash `sgh64`: the bytes as little-endian dwords (last zero-padded),
s = sum (w_i ^ 0x5bd1e995) * ((2i + 1) * 0x9e3779b97f4a7c15) mod 2^64, then s ^ length and the splitmix64 finaliser; one
changed dword always changes it; pointer words hash as 0. CLI: `hash`, `diff` (objects that differ where both images
know the bytes: symbol, class, first differing byte in the object, byte count; gaps with `--all`), `convert`.

Checks on phase 2.1's local runs: a checkpoint dump equals the full .data dump of the same checkpoint (0 objects);
two continuous runs' full dumps differ only in 121 pointer and 2 audio objects and 17 gap runs; the save written at a
frame head (`capture --save-at`) equals the frame-head capture there except g_textBuffer (29 bytes: saveGame rewrites the
text buffer it saves). `tests/test_state_image.py`: 8 static tests on synthetic images (hash properties, diff naming,
masking, unknown bytes, the dump and .sve parsers, the round trip) and one runtime-marked test on the pinned det_b
checkpoint and the 2.1 runs (skipped when the folders are absent).

## 4. The checkpoint tracer (opt-in; the default is unchanged)

`re/frida/scenario_ab.py` (env or flags; nothing changes without them):

- `C3FT_OBJHASH=1|image` (`--obj-hash`): at every checkpoint the whole .data is read and `<name>.objhash.json` gets
  `{symbol: [class, sgh64]}` for all 989 objects; `image` keeps `<name>.full.bin` too.
- `C3FT_FRAMEHASH=<classes>` (`--frame-hash sim`): at every mainLoop FRAME HEAD (the main-thread timeGetTime call at
  0x0040fc29, return address 0x0040fc2f, phase 2.1's restart instant) the agent appends [T handed to the game, frame-head
  index, sgh64 of each object of those classes] natively: the tgt stub gets `cmp dword [esp], 0x0040fc2f; jne skip;
  push eax; push eax; call [D+0x50]; add esp, 4; pop eax` before its due test, and D+0x50 points at a Frida CModule
  (`re/frida/js/framehash_cm.c`, the same hash; checked equal to state_image.sgh64 for 8 buffers in a 32-bit process). No
  JavaScript runs per frame. Records are fetched at every checkpoint and at the end into `framehash.bin` /
  `framehash.json`; `C3FT_FRAMEHASH_CAP` bounds the records held between fetches (default 20,000; overflow is counted
  and a run with dropped records never compares SAME).
- `C3FT_PERTURB=<va>:<xor>@<virtual ms>` (`--perturb`): xors one dword at the first poll at or after that time (agent
  action `x32`), the negative control below.

`re/tools/frame_trace.py cmp <a> <b>` reports the FIRST DIVERGENT FRAME (record, T, frame index, the objects whose hash
differs there, how many later records differ) and the checkpoint object hashes per class. `re/frida/state_trace.py`
runs a pinned scenario's schedule on instances\p22 (winmm.dll 381ed2b2 of the pinned references, C3FT_AUDIO=none) into
`log/p2/p22/<scenario>/<label>/` and has `run`, `cmp`, `check` (against the pinned det_b).

| run | result |
|---|---|
| sandbox_basic, no tracer, vs pinned det_b | SAME at 10 / 10 checkpoints (state and frame): the edited agent and harness keep the default |
| sandbox_basic off_a (obj + frame hashes) vs det_b | SAME at 10 / 10: the tracer does not move the timeline (9-10 s wall) |
| sandbox_basic off_a vs off_b | frame hashes SAME, 439 / 439 records x 203 sim objects; checkpoint hashes differ only in the pointer (120-121 objects) and audio (2) classes |
| course_season off_a vs off_b | frame hashes SAME, 6,296 / 6,296 records; checkpoint hashes SAME at 12 / 12 for sim, hidden, ui, render, engine, const; off_a vs det_b SAME 12 / 12 (49 / 69 s wall) |
| sandbox_basic pert_rng: g_rng ^= 1 at the poll T = 20,099 virtual ms | first divergent frame = record 154 at T = 20,105 (record 153 at T = 20,006 equal), objects: g_rng only; all 285 later records differ; first checkpoint with a difference sandbox_15 (g_pathGrid, g_rng, g_skyActors, g_walkers) |
| course_season pert_date: g_date ^= 1 at the poll T = 300,071 virtual ms | first divergent frame = record 2,985 at T = 300,119 (record 2,984 at T = 300,020 equal), objects: g_date only; all 3,311 later records differ; first checkpoint with a difference season_300 (16 sim objects), 46 at season_600 |

Within one OFF run no `const` object changes after the menu checkpoint in either scenario; `engine` g_codeBuf changes in
course_season; `hidden` g_pathQueueX / Y (sandbox_basic) and 14 objects (course_season) change.

### 4.1 Reading the controls

In both controls the first divergent frame is the first frame head after the poll that applied the xor, and the only
object that differs there is the perturbed one; the next checkpoint already shows the spread (4 sim objects 4.9 virtual s
later in sandbox_basic, 16 in course_season). Without the frame records the checkpoints alone would have placed the
cause somewhere in a 5 s or 60 s window among 4 or 16 objects. Comparisons: `log/p2/p22/<scenario>/cmp_*.json`.

## 5. How to regenerate

```
~/bin/py -3.12 re/tools/model_merge.py merge            # addresses.tsv (pointer column)
~/bin/py -3.12 re/tools/model_state_gen.py              # port/include/gamestate.h, gamestate_meta.inc, gamestate_addresses.inc
~/bin/py -3.12 re/tools/model_state_gen.py --check      # exit 1 when stale (tests/test_gamestate_gen.py does the same)
cmake -S port -B port/build/x64 -A x64 && cmake --build port/build/x64 --config Release && ctest --test-dir port/build/x64 -C Release
```

## 6. What 2.4 / 2.6 need to know, known gaps

- Compare `sim` by default; `hidden` (stale buffers, text), `ui`, `render`, `engine`, `const` are comparable between
  runs of the original (SAME in 12 / 12 course_season checkpoints) but a port is not expected to produce them; `pointer`
  and `audio` are per-process and never comparable. Mask SG_POINTER_WORD dwords.
- A segment start image is phase 2.1's `data` variant; GameState is the same bytes, so a `.full.bin` / `.fh.bin` loads
  directly (state_image `full`).
- The frame-head tracer sees only mainLoop's frame head: nothing is recorded inside modal screens, the title menu or
  the session dispatch (U-0613); a divergence there shows at the next frame head or checkpoint.
- Terrain.dll, sound.dll, jgld.dll and the heaps are outside GameState (U-0607): the image and the hashes cover
  golf_clean.exe's .data only.
- The frame-head records are aligned by index; a run whose number of frame heads differs (another schedule, a hang) is
  reported at the first record whose T differs.
- 2.3's time and input trace is a separate file set (the sibling step); the frame index here is the count of frame-head
  calls since boot, which a trace of the same run can use to join the two.
