# Phase 1 plan: the data model (written 2026-10-17, not started)

Phase 1 of re/PORT_PLAN.md: a typed, evidence-backed model of every piece of game state and constant data the game
logic touches, so that phases 3-6 (simulation, formats, backends, UI) are written against structs and not against
addresses. Nothing here launches yet: the user approves the decisions in section 2 first.

## 1. Inputs (measured 2026-10-17, repaired index)

- 5,589 data addresses referenced by the 1,812 game-logic functions (0x401000-0x4a4ecf), 960 clusters (gap <= 0x40):
  - **bss 765 clusters** (0x4e7000-0x842000, the mutable game state; 63 have a named member),
  - **initialised .data 167 clusters** (0x4c1000-0x4e7000; 3 named): constant tables, vtables, string-pointer tables,
    UI layout data. The largest is 0x4c80d0 (span 0x9fa4, 1,602 addresses), then 0x4d21a4, 0x4e28e0, 0x4c73cc.
  - **.rdata 28 clusters** (strings, jump tables).
  - immediates in the data range count as addresses, so these are upper bounds.
- Already typed (do not redo): the golfer record (re/analysis/data/golfer_record.md, 156 x 0x100 at 0x005794b8) and the
  tables of record_tables.md (walkers 64 x 0x4c, golferStats 32 x 0xb8, per-type state 84 x 0x2c, story defs 100 x 50).
- 136 exe globals named; about 69 `.sve` fields unnamed (U-0004); open ranges U-0009..U-0017.
- 1,018 neutral-named functions (exe 437 [275 game code, 433 reachable], jgld 62, Terrain 56, sound 463):
  log/b1/new_functions_<module>.tsv.
- Free cross-checks: the 1,577 exe hooks use literal addresses with widths (9,660 occurrences), the `.sve` serializer
  lists 86 globals with sizes, the pinned scenario checkpoints dump the `.sve` globals, and 3 `.sve` fixtures exist
  (tests/fixtures/stories: mood5, stories5, stories9; not tracked in git). The round trip in stream G needs more saves:
  stream G records more by saving from the pinned scenarios (course_season, championship, ui_tour save/load steps).
- NOT covered by the survey: heap OBJECT layouts (C++ classes of the UI toolkit: Window/Widget/Button/EditBox/ListBox/
  TextView/panels with their 0x274 Surface sub-objects, Image, Snd, Timer, Flic, Cache482f60 ...). Those are reached
  through `this`, not through static addresses, and the UI toolkit is about 590 functions. Stream E below.

## 2. Decisions for the user (before launch)

1. **Where do exe-embedded constants live in the port?** The initialised tables (strings, sprite rectangles, colour
   tables, UI layout data, name lists) are game data inside golf_clean.exe. Options: (a) the port reads them from the
   user's own install at start (an extractor keyed by table name and the model's offsets, never shipping the bytes),
   (b) the repo embeds extracted copies. The repo is public and the user's stated rule is that assets stay the user's:
   recommendation (a). Under (a) phase 1 produces table DESCRIPTORS (name, kind, element type, count, source offset in
   the user's exe) and no copy of the bytes.
2. **Header shape.** `re/model/include/simgolf_model.h`: clean C (C99, fixed-width types, `static_assert` sizes), no
   original addresses inside it. The address map lives apart (`re/model/addresses.tsv`: symbol, module, VA, size,
   stride, count, evidence, .sve flag), so the header can ship in the port and the map stays a tooling file.
3. **DLL FUN_ functions.** Name only what the port needs: the 275 exe game-code functions and the reachable exe CRT
   helpers. jgld/Terrain/sound new functions are covered by their API boundary (Display/Surface vtables, 26 Terrain
   methods, 12 sound exports); name them only when a model stream needs one.
4. **Order.** Tooling first (stream A), then the subsystem streams in parallel (B-F) on disjoint address ranges, then
   integration (G) and function naming (H).

## 3. Streams

**A. Inventory and classifier (1 Opus child, first, blocks B-F).** From the repaired index build
`re/model/inventory.tsv`: one row per data address and per cluster with: region (rdata/init/bss), read/write counts by
width, signedness of the compares that follow, index scale (`[reg*k + base]` gives the element stride), the referencing
functions and their subsystem tags, whether the address is in the `.sve` map, whether it holds a code or data pointer
in the file (vtable, string table, jump table), and an automatic class with confidence: scalar, flag word, array of
scalars, record array (stride, count), pointer table, vtable, string table, jump table, buffer, unknown. Also
`re/tools/model_check.py` (static): given a proposed type for an address range, verify every access in every function
(including code that b1 now lists) against width, stride and bounds, and list violations. Deliverables: tool, inventory,
a clustering by owner subsystem (which functions touch the cluster) that assigns each cluster to exactly one stream.

**B. Simulation state (Opus).** Walkers/employees (`g_walkers`), golfer-type state, golferStats, stories, economy and
finance tables, course records (0x571ff4 x 0x2e), level tables, tournament, membership, date/time and event state, RNG.
Output: structs + addresses + .sve flag for each, field maps with evidence, names for the unnamed `.sve` fields that
belong here.

**C. Course, terrain and world (Opus).** Tile grids (type 0x5722e8, flags 0x53caf0, placed objects 0x58bcb8, height grid
0x838c1c, walls), tile-type attribute tables, hole records (0x575cb8 x 0x208 x 18), animals, camera and projection state
(0x4c2ba0.., zoom, rotation), the Terrain.dll boundary (26 methods and their object), course/file records (.cse .srf).

**D. Rendering and audio state (Sonnet).** Sprite queue (parallel arrays at 0x82xxxx-0x83xxxx), surfaces and display
object (0x83ad50, Display vtable 86 slots, Surface vtable 60), palettes, colour tables (0x822c74 32K conversion table),
fonts, cursor state; the sound table (219 path/slot pairs, 0x80d840 x 0x6c), Snd objects, device slots (soundVersionCheck).

**E. Object layouts of the C++ classes (Opus).** The UI toolkit and engine classes, from constructors, vtables and
accessor patterns: Window family (0x274 sub-object, vbtable at this-0x58), Button/EditBox/ListBox/ListModel/TextView/
LabelButton/panels, Image, Surface wrappers, Timer, Flic/Reader, Cache482f60/483010, VoiceRx/S3List, net session
(0x8400b8). Output: struct + vtable slot maps with sizes taken from the allocators and ctor stores, one file per class.

**F. Initialised data tables and configuration (Sonnet).** The 167 init clusters and 28 rdata clusters: classify each
(string table, sprite/rect table, colour table, UI layout, key map, name lists, jump table, vtable), record element type,
count, stride and the exact file offset in the user's exe (descriptor only, per decision 1), and the global option/flag
words in 0x4e4xxx-0x4e5xxx (UI state defaults gameMain sets at 0x0045bca4..0x0045bd64). Also the unnamed `.sve` fields
that are plain options.

**G. Integration and verification (1 Opus child after B-F).**
- Merge the stream outputs into `simgolf_model.h` and `addresses.tsv`; names and addresses unique (names_validate).
- Compile the header with MSVC (static_assert sizes, no overlap, alignment) and with a second compiler.
- `model_check.py` over ALL accesses: 0 violations, or each violation explained and filed (a violation is either a wrong
  type or an access the original does out of bounds, already catalogued as a defect).
- Hook cross-check: every literal address+width in the 1,577 hook sources agrees with the model.
- `.sve` round trip: parse each fixture .sve through the model (a Python ctypes mirror generated from the same
  descriptors) and re-serialize byte-identical; also parse the memory dump of every pinned scenario checkpoint.
- Coverage report: addresses and cluster classes typed, weighted by number of referencing functions; unnamed remainder
  listed with the reason.
- Apply names to the Ghidra master (ApplyNames, backup first) and re-export the lists.

**H. Function naming (3-4 Sonnet children, after G).** The 275 exe game-code FUN_ functions and the reachable CRT
helpers, from the model (a function that reads `g_tile.flags` bit X is named by that) plus callers' context; evidence
per name, names_sanitize/validate/check_evidence, hooks.csv rows through the batch recipe (C1 -> C2), neutral names where
the evidence stops. DLL FUN_ functions only on demand.

## 4. Sequence, children, effort

| step | children | model | depends on |
|---|---|---|---|
| A inventory + classifier + model_check | 1 | Opus high | nothing |
| B, C, E | 3 | Opus high | A |
| D, F | 2 | Sonnet high | A |
| G integration | 1 | Opus xhigh | B-F |
| H naming | 3-4 | Sonnet high | G |

About 10-11 children over five stages. B-F write only their own header section files
(`re/model/include/sections/<stream>.h`, `re/model/<stream>.tsv`, `re/analysis/data/<stream>.md`) so merging is
mechanical; only G edits the merged header, the master and the shared trackers.

## 5. Exit criteria (all measurable)

1. Every one of the 960 clusters has a class; zero unknown clusters touched by a C3/C4 hook function.
2. 100% of bss clusters referenced by a C3/C4 function typed; weighted coverage by function reach reported.
3. `model_check.py`: 0 unexplained violations over all 1,812 game-logic functions plus mainLoop's body.
4. Hook cross-check: 0 disagreements; `.sve` round trip: byte-identical on all fixtures; scenario checkpoint dumps parse.
5. The ~69 unnamed `.sve` fields named or listed as U-NNNN with the evidence that stopped the naming.
6. Class layouts for every class with a constructor in the exe's UI toolkit and engine (sizes match the allocation
   sizes).
7. Tests: a static test that the header compiles and its size asserts hold; CLAUDE.md paragraph; project-info pane.

## 6. Risks

- Immediates in the data range inflate the address count: stream A's classifier must drop non-address immediates (an
  immediate that no instruction uses as a pointer or index).
- A table can be both read as ints and as pointers (UI layout data embeds addresses): the model records both views.
- The golfer record base differs between old hook sources (0x5794c0) and the model (0x5794b8): the address map is the
  single source, hooks are not edited.
- Per-stream drift: two streams may type the same range differently; stream A's ownership assignment prevents this and
  G rejects overlaps.
- Scale: the 0x4c80d0 table (1,602 addresses) and similar blocks may be UI layout code-as-data; the classifier flags them
  for stream F rather than forcing a record type.
