# Data-model inventory, classifier and model checker (phase 1 stream A, 2026-10-17)

Stream A of `re/PHASE1_PLAN.md`: every data address golf_clean.exe's game logic references, grouped into clusters,
classified, assigned to exactly one data-model stream (B..F, or X with the reason), and a static checker that tests a
proposed type against every access. NO-GUESSING applies: every class is the output of a stated rule over the anchored
image; where the rule has no evidence the class is `unknown` or the confidence is `low`.

## 1. Files

| file | content |
|---|---|
| `re/tools/model_scan.py` | shared scanner: capstone over every range of every function, one `Ref` per data reference, straight-line register tracking |
| `re/tools/model_inventory.py` | keep rule, address rows, clusters, classifier, ownership, validation; writes the six TSVs below (~30 s) |
| `re/tools/model_check.py` | static check of a typed-range TSV against every access (errors and warnings, section 7) |
| `tests/test_model_tools.py` | 13 static tests (golfer known good, three wrong types, walker loop end, scanner multiplier chain, keep rule, validation rates, streams, vtables) |
| `re/model/inventory_addresses.tsv` | 5,950 rows, one per literal data address (kept or dropped) |
| `re/model/inventory_clusters.tsv` | 1,767 rows, one per cluster with features, class, confidence, stride, count |
| `re/model/inventory_records.tsv` | 182 record / row-array clusters: lowest indexed displacement, field offsets, named addresses that are not the base |
| `re/model/ownership.tsv` | 1,767 rows: stream, rule, the vote it overrides, votes, margin |
| `re/model/vtables.tsv` | 86 vtables and 17 vbtables: slots, installers, class named by the installers, slot targets |
| `re/model/check_example.tsv` | example input for model_check.py (golfer struct, a sprite array, the month table) |
| `re/model/inventory_validation.tsv` | 40 tuned items + 106 hold-out items + 2 skipped |

Re-run: `py -3.12 re/tools/model_inventory.py --summary` (deterministic: two runs give byte-identical TSVs) and
`py -3.12 re/tools/model_check.py <types.tsv> --out <violations.tsv>`.

## 2. Method

### 2.1 Scan (`model_scan.py`)

The 1,812 functions of `re/functions_ghidra.tsv` (after b1) whose entry lies in 0x401000..0x4a4ecf, every address
range of each (mainLoop is one range 0x0040f5c0..0x00421615; the 275 game-code functions b1 created and the vtordisp
thunks included), decoded with capstone. A reference is recorded for:

- a memory operand whose displacement is in the data range 0x4ba000..0x842178, with any base / index register, for
  every mnemonic (lea, call and jmp included). Kinds: `abs`, `base`, `idx`, `lea_*`, `call_*`, `jmp_*`;
- an immediate in the data range (kind `imm`), with its use: `push`, `movreg`, `store` (`mov [m], imm`; `field` when
  the destination is `[reg + k]`, `stackarg` for `[esp + k]`), `cmp`, `add`, `arith`;
- an access through a register that holds a data address (kind `reg`, the address is the first iteration's);
- a dereference of a register loaded from a data address (kind `ptruse`: `ptrval` the global holds a pointer that is
  dereferenced, `vtblval` the global points to an object called through its vtable, `thisptrval` the loaded value is
  passed as `this`).

Register tracking is straight-line in decode order inside one range (reset at `ret` / `jmp`; `call` clobbers eax,
ecx, edx). Per register: `anchor` (holds a data address, optionally plus an index times a known multiplier), `index`
(a value times a multiplier: `shl`, `imul`, `lea r, [a + a*k]`, `add` / `sub` of the same value; any other arithmetic
starts a new value with multiplier 1, so the multiplier applied after combining two loaded values is kept: golferStats
is indexed by `lea eax, [edx + ecx*4]; lea ecx, [eax + eax*2]; shl ecx, 3; sub ecx, eax; mov ebx, [ecx*8 + 0x56ae7c]`
at 0x0042bdae..0x0042bdb9 = (cls + 4 * prefs) * 23 * 8 = stride 0xb8), `loaded`, `const`. Stride candidates of a
reference: the tracked multiplier times the scale (`scale`), the index multiplier of an anchor (`anchor`), or the
single loop step (`add r, K` / `inc r` / `lea r, [r + K]`) inside a backward branch of at most 0x800 bytes that also
contains the access (`step`; mainLoop's frame loop spans 73 KB and is not an inner loop). Also recorded: signedness
(movsx / movzx, or the jcc after a compare of the operand or of the register it was just loaded into), bit operations
(and / or / xor / test / bt with an immediate on the operand or the loaded register), `rep stos / movs` block sizes
when ecx is a known constant, `this` (an immediate moved into ecx and not read before a `call`), the jcc after a
compare, and a constant upper bound on an index register (`cmp r, N` + ja / jae / jg / jge on the fall-through).

### 2.2 Keep rule (counts in section 3.1)

An address becomes a kept row when at least one reference is: `mem` (a memory operand displacement), `deref` (an
immediate in a register that a later instruction uses as a memory base or index), `addbase` (an immediate added to an
index register that is then dereferenced), `thiscall`, `cmpend` (a compare of a register that holds a tracked pointer
or was a memory base / index in the previous 64 instructions), or `value` (pushed, moved to a register or stored, and
not a single bit: a power of two is a flag constant). An address whose only uses are arithmetic / bit operations or a
compare of a register with no pointer evidence is dropped (row with `kept` = 0). An address reached only by `cmpend`
is an `end_marker`: it bounds a loop over the cluster below it and does not form clusters of its own.

### 2.3 Clusters

Kept non-marker addresses are grouped with the survey's gap rule (next address at most 0x40 above the previous one)
and split at evidence that a new object starts: section edges (IAT 0x4ba000..0x4ba264, .rdata, file-initialised .data
up to 0x4e7000, .bss), the five typed tables of golfer_record.md and record_tables.md (`KNOWN`: g_golfers 156 x 0x100,
g_walkers 64 x 0x4c, g_golferStats 32 x 0xb8, g_golferTypeState 84 x 0x2c, g_storyDefs 100 x 0x32), every `.sve` field start and end
(`re/tools/sve.py` FIELDS and both tails), every vtable / vbtable start and end, every named global outside KNOWN
(`re/names/*_globals.tsv`), every address passed as `this`, the edges of the plan's named objects (section 2.5), a
change of file content kind (code / string pointer / data pointer / text / other) between two neighbours of
file-initialised data that share no stride above 4, and a fixed-address member followed by the base of an indexed
access with no stride above 4 (`scalar before array`). End markers attach to the cluster whose span holds them, else
the nearest one below.

Neighbouring array clusters with the same stride S above 4 and a gap of at most 2 * S are parts of one table
(`part_of`): fields reached only through separate displacements, columns (eight objects 0x2c apart, each the `this` of Surface_blit / Surface_stretchBlit, in rows of
0x160 at 0x00562368), or biased bases (`[hole * 0x208 + 0x5758a8]` reaches the `.sve` block 0x575ab0 for hole 1).
Each part takes the owner of the group's anchor.

### 2.4 Classes

Applied in this order; `confidence` is high when two independent signals agree, medium for one, low for a fallback.

| class | rule |
|---|---|
| import_table | inside the IAT (an extra class: the import address table, named from the import directory) |
| jump table | indexed `jmp [r*4 + tbl]` into code-pointer content (none in game logic: VC6 puts switch tables in .text) |
| vtable | the cluster starts at a vtable (a .rdata dword that is a function entry, installed by `mov [reg + k], imm` or loaded / pushed; slots = the run of function entries up to the next start) or a vbtable (small integers stored into a field) |
| string pool / string table / pointer table | read-only file data whose members are text (>= 2 characters, or 0-1 for addresses only passed by value), string pointers, or code / data pointers that are indexed or called through |
| record array | inside a KNOWN table (`covered_by`), or a stride vote above the widest access: the largest stride whose vote (distinct instructions) is at least a quarter of the top vote and at least 2 (records hold sub-arrays: g_holes' 8-byte pairs outvote its 0x208 stride) |
| const lookup table | read-only file data that is indexed or has a stride vote |
| object | an address passed as `this` (a global C++ object; an extra class) |
| buffer | at least 2/3 of the accesses are bytes, the byte stride is not outvoted, and the address is passed as a value at least once per two accesses (char buffers); or passed by address with a block size, or byte-indexed (medium); or only passed by address (low: the layout is the callees') |
| scalar array | a stride vote equal to the access width, or indexed with no stride evidence (low) |
| flag word | bit tests / masks are at least half of the reads |
| scalar | fixed-address accesses only (read-only file data: a constant) |
| unknown | none of the above |

Counts come from the `.sve` field size, a `rep stos / movs` block, an end marker, or the span (a lower bound), in that
order; `count_source` says which.

### 2.5 Ownership (`ownership.tsv`)

In this order: a KNOWN table -> B; an object `re/PHASE1_PLAN.md` section 3 names for a stream (`PLAN_CLAIMS` in the
tool, 27 entries with the extent from the `.sve` field size or the named dword: tile grids, placed objects, height grid,
hole records, camera, zoom, rotation -> C; course records, level table, RNG seed, g_date, story pairs -> B; the sprite
queue arrays 0x82415c..0x838204, the sound table 0x80d840 x 0x6c, the display object, the colour table -> D; the net
session -> E; scalars / flag words in 0x4e4000..0x4e6000 -> F); vtable or object -> E;
import table or read-only file data -> F; then the VOTE: each referencing function votes by its subsystem tag
(golfer / sim / economy / ai -> B, terrain / course -> C, render / video / audio -> D, ui / input / frontend / net -> E;
boot, util, save and crt do not vote: static initializers, generic helpers, the serializer and the CRT touch every kind
of data); a tie on functions is broken by the number of references; written file-initialised scalars / flag words that
the vote gives to E go to F (configuration words with file defaults). No voter: written .data -> F, .bss -> X with the
non-voting tags as the reason. Last, every `part_of` member that is not in a KNOWN table takes its anchor's stream
(rule `part of the table at cluster N`).

Tags: hooks.csv and `re/names/*.tsv` give 1,526 of the 1,812 functions a tag; untagged functions take the majority tag
of their direct callers (13), else of their callees (104), else the tag of the nearest tagged functions on both sides by
address when they agree (109; MSVC keeps one source file's functions together); 60 stay untagged.

## 3. Counts

### 3.1 Addresses

30,102 references: 24,706 literal, 4,277 derived through registers, 1,119 pointer uses. **5,950 literal addresses:
5,949 kept, 1 dropped** (0x00800000, 5 references, all `or` / `test` with the immediate: 0x00422fdf, 0x004250a2
`test dword [0x59e7b8], 0x800000`, 0x00466416, 0x00466438, 0x0048ef64). 94 kept addresses are end markers.

| kept by | addresses | | section | addresses |
|---|---:|---|---|---:|
| value only (string / buffer / vtable pointers passed or stored) | 2,396 | | .data file-initialised | 3,471 |
| mem only | 1,354 | | .bss | 2,275 |
| deref + value | 1,112 | | .rdata | 136 |
| thiscall + value | 498 | | IAT | 67 |
| other combinations | 589 | | | |

The survey's caveat (`immediates in the data range inflate the count`) does not hold in this binary. The 2,396
value-only addresses land in: string pools 2,067 (.data 2,065, .rdata 2), vtables / vbtables 103, a pointer table 1
and const scalars 2 in file data; buffers 177, objects 25, scalars 9, record arrays 6, scalar arrays 2 and unknown 4
in .bss. No immediate was dropped by the single-bit rule; the one dropped address is 0x00800000, whose uses are all bit
operations. The survey's 5,589 were a lower bound: its rule counted a memory displacement only with no base register and skipped
call / jmp operands. This inventory adds 285 base-register displacements, 51 call / jmp memory operands (IAT slots,
function-pointer slots) and 25 mixed ones, and contains all 5,589.

Cross-check: mainLoop references exactly the 675 addresses of `log/c3/c3fy_deps.tsv`'s `global` rows (675 common,
none missing on either side).

### 3.2 Clusters

**1,767 clusters** (986 by the gap rule alone over the kept set; the survey's 960 were over 5,589 addresses). Splits
besides 968 gaps: 563 object starts, 167 content changes, 68 scalar-before-array. By section: .bss 1,275, .data 377,
.rdata 112, IAT 3.

| class | high | medium | low | none | total |
|---|---:|---:|---:|---:|---:|
| object | 565 | 19 | | | 584 |
| string pool | 230 | 9 | | | 239 |
| scalar | 106 | 82 | 36 | | 224 |
| record array | 73 | 33 | 85 | | 191 |
| buffer | | 10 | 172 | | 182 |
| scalar array | 82 | 28 | 23 | | 133 |
| vtable (vtable + vbtable starts) | 71 | 32 | | | 103 |
| const lookup table | 31 | 39 | | | 70 |
| string table | 12 | 3 | | | 15 |
| pointer table | | 13 | | | 13 |
| unknown | | | | 6 | 6 |
| flag word | 4 | | | | 4 |
| import_table | 3 | | | | 3 |
| total | 1,177 | 268 | 316 | 6 | 1,767 |

`covered_by`: 15 clusters (g_golfers 10, g_golferStats 2, g_walkers 1, g_golferTypeState 1, g_storyDefs 1).
`part_of`: 33 clusters belong to another cluster's table. Of the 584 objects, 175 lie in the sound table
0x80d840..0x8134a4; the other 409 include rows of instances 0x2c apart (0x00540110, 0x0054013c, 0x00540168,
0x00540194; the same spacing as the rows at 0x00562368; both sets are the `this` of Surface_blit and Surface_stretchBlit). Which class each `this` belongs to is the
callee's: stream E reads it from the called method.

### 3.3 Ownership shares

| stream | clusters | addresses (incl. markers) | referencing functions | sections (clusters) |
|---|---:|---:|---:|---|
| B simulation state | 185 | 572 | 203 | bss 177, data 8 |
| C course / terrain / world | 59 | 185 | 138 | bss 55, data 4 |
| D rendering and audio | 450 | 548 | 243 | bss 444, data 6 |
| E object layouts | 687 | 1,130 | 714 | bss 579, rdata 103, data 5 |
| F constants, strings, configuration | 366 | 3,487 | 333 | data 354, rdata 9, IAT 3 |
| X unassigned | 20 | 27 | 30 | bss 20 |

Rules: vote 515, object 408, read-only file data 346, plan 240, vtable 103, tie 77, part_of 33, no voter 19,
covered_by 15, configuration word 8, import table 3. 579 clusters have a stream other than their vote: read-only file
data (vote E 174, B 126, D 20, C 18 -> F), objects (vote D 156, B 16 -> E), vtables (vote D 21 -> E), the plan (25:
option words E / D -> F 8, hole records B / E -> C 8, camera E -> C 2, zoom D -> C 1, rotation D -> C 1, placed
objects E -> C 1, course records E -> B 1, level table C -> B 1, colour table B -> D 1, display object E -> D 1),
configuration words E -> F 8, `part_of` B -> C 6.

## 4. Validation

### 4.1 Tuned set (40 items, `VALIDATION` in the tool): 40 correct

These items guided the rules (they are not independent): the five KNOWN tables classified blind (the `covered_by`
shortcut off), seven more record tables from the names TSVs, eight scalar arrays (the sprite queue's five int[]
arrays, g_pathQueueX / Y short[], g_shotStartDate), five vtables, three string tables, six scalars, the flag word
g_flags 0x59e7b8, three objects, three buffers. Blind results: g_golfers stride 0x100 / 156 (59 field offsets from
0x005794b8 in `inventory_records.tsv`), g_walkers 0x4c / 64, g_golferStats 0xb8 / 32, g_golferTypeState 0x2c / 84,
g_storyDefs 0x32 / 100, g_holes 0x208 / 20, golfer types 0x230 / 76, placed objects 0x10 / 256, animals 0x3c / 100,
story pairs 0x388, sprite arrays stride 4 / 1,024 (count from the `rep stosd` blocks); vtables rootWin 0x4ba2fc 91 slots
(slots 29..39 are the root window handlers), NodeListC 0x4bb014 27, Snd 0x4bacf8 61 (slot 28 is Snd::flags); string
tables of 3 (golfer preferences 0x4c2858) and 4 (course types 0x4c3078).

**Counts are the weakest output**: of 16 items with an independently known count, 10 match; the misses are the
month table 0x4c2908 (22: the string-pointer run continues into the next unreferenced table; `sve.py` MONTHS has 8),
g_recordBanks (1 vs 16 slots), g_typeAttr (2; the type table is 23 x 0x30 from 0x578350), g_pathQueueX / Y (1 vs
0x400), g_shotStartDate (1 vs 152). A count with `count_source` = `span` is a lower bound.

### 4.2 Hold-out (106 named globals): 94 compatible, 12 not

Every `re/names/*_globals.tsv` row of golf_clean.exe outside the tuned set and outside KNOWN whose declared type maps
to a class (`expected_from_type`: `int` / pointers -> scalar or flag word, `T[]` -> array, `record` / `stride` /
`table` -> record array, class names -> object); 2 rows skipped (type text not mappable). 94 of 106 compatible
(88.7%). Of the 12:

- **4 declared types are stale; the code supports the classifier**: g_blitSaveBuf 0x0083a7b8 (`void*`) is the Image
  gImageCtor_83a7b8 0x00479a40 constructs; gTileBuffer 0x00820f18 (`void*`) is the std::list initList820f18
  constructs (CLAUDE.md round 24 rename); g_pairArray 0x008406e8 is the PairArray object (PairArray::ctor); g_logFile
  0x00840930 (`FILE*`) is passed as `this` by initLogFile at 0x004a0135 and 0x004a0339 (to 0x004a0180).
- **8 classifier misses** (7.5% of the hold-out): gCourseInfoDirty 0x00587da0 and gTextBuffer2Index 0x0059d91c (an int
  that shares a cluster with the buffer / array after it), g_textBuf 0x0058a528 (record array 0x2c), g_codeBuf
  0x0083aba4 / g_soundFns 0x0083af6c / g_widgetTable 0x0083fe78 (arrays reached only at fixed slots, classed scalar),
  g_screenMgrB 0x0083ac30 (an object never passed as `this`, classed buffer), g_musicDevice 0x0083ad80 (classed as
  part of a 0x1c-stride array of the SoundDevice objects around it).

Error rate: tuned 0 / 40 (not independent); hold-out 12 / 106 disagreements, 8 / 106 classifier errors.

### 4.3 model_check against the golfer record

| typed range | errors | warnings |
|---|---:|---|
| `Golfer` struct of golfer_record.md section 4 at 0x005794b8 x 156 | **0** | width_narrow 79, width_block 11, sign 9, base_outside 7 |
| the same struct at 0x005794c0 (the old hook sources' base) | 34 width_cross | 1,008 |
| `short[0x4e00]` at 0x005794b8 | 1,388 width_cross | 1,035 |

The warnings on the known-good type are observations for stream B, each with its instruction in the violations TSV:
byte tests of the flag dword +0x10 (27) and of the 16-bit +0xb8 / +0x88 / +0x16 / +0xb6 / +0xb0 / +0xa4 fields;
dword copies of skill[12] +0xf0 (3 x 3) and fileTail +0xfc (2); **+0x16 (declared uint16_t) is read with `movsx` at 8
sites** (narrateShot 0x0040890a, 0x00408c07, customizeCharacter 0x0040f550, mainLoop 0x0041b914, updateGolfers
0x0042bf3b, 0x0042bf62, drawGolferInfoPanel 0x0045c591, pairGolfersWithStory 0x0045ed63); mainLoop indexes the table
one record low (`[eax + 0x5793b8]` 0x00415a60, `[eax + 0x5793bc]`, `[eax + 0x5793d9]` 0x00415a50 / 0x0041f065,
`[eax + 0x57946e]` 0x0041d93f / 0x0041d957, `[eax + 0x579470]` 0x0041d975: displacement = field - 0x100).

## 5. Records addressed at several offsets (`inventory_records.tsv`)

The golfer-record lesson, applied to every record array: the lowest displacement used with the record stride, the
field offsets from it, and named addresses that are not that base.

- **Hole records**: `[hole * 0x208 + 0x5758a8]`, `+ 0x575aa8`, ... reach the `.sve` block 0x575ab0 (20 x 0x208) for
  hole 1: the index is 1-based, the lowest displacement 0x005758a8 lies 0x208 below the block. Clusters 0x5758a8,
  0x575aa8, 0x575b88 and 0x575c9c are `part_of` cluster 0x575ab0. re/PHASE1_PLAN.md writes 0x575cb8 x 18 (= the
  block's record 1).
- **Animals**: the `.sve` block is 0x0056d1b8 (6,000 bytes = 100 x 0x3c = 300 x 0x14); exe_*_globals.tsv names
  g_animals at 0x0056d1d8 (struct[100], 0x3c). Indexed displacements: from 0x0056d1d8 with stride 0x3c (fields +0x00..
  +0x1a) and at 0x0056d1b8 with stride 0x14. Whether the 0x20 bytes from 0x56d1b8 are part of record 0 is for stream C.
- **Tile-type attributes**: g_typeAttr is named at 0x00578372; the table mainLoop SETUP fills is 0x114 dwords copied
  from 0x004c1a40 to 0x00578350 (re/analysis/sim/mainLoop_structure.md), and cluster 0x578350 (stride 0x30, fields
  +0x00, +0x10, +0x20, +0x21) is `part_of` the 0x578372 cluster: 0x578372 is +0x22 of the record at 0x578350.
- **Golfers**: displacements 0x005793b8.. and 0x0057946e.. (section 4.3) are `part_of` g_golfers.
- **Story pairs**: the `.sve` block 0x0059fc60 is 0x3880 = 16 x 0x388; exe_*_globals.tsv says 10 entries of 0x388;
  0x59fde8 and 0x59fee8 (narrateShot's / addGolferPair's copies, golfer_record.md) are inside the same block.

## 6. The 20 hardest ties (vote margin 0, broken by references)

| cluster | range | class | stream | votes |
|---|---|---|---|---|
| 1691 | 0x0083ad0c-0x0083ad20 | scalar | E | D 7, E 7, B 1 |
| 960 | 0x0059bf90 | scalar | E | C 5, E 5, B 4 |
| 1088 | 0x005a8c34-0x005a8c88 | scalar array | B | B 5, C 5, E 2, D 1 |
| 680 | 0x00561970-0x005619a0 | record array | B | B 4, C 4, D 1, E 1 |
| 698 | 0x00567328 | scalar array | E | B 4, E 4, C 2 (`.sve` theme name, char[100]) |
| 1599 | 0x00822b98 | scalar | D | D 4, E 4 (gDirtyFlagA) |
| 1070 | 0x005a7140-0x005a7144 | flag word | B | B 3, E 3 |
| 1122 | 0x0080b130 | record array | E | B 2, E 2, C 1, D 1 (story text source of g_storyDefs) |
| 1340 | 0x0081ca10-0x0081ca14 | record array | B | B 3, D 3 |
| 629 | 0x00541cd8 | scalar | B | B 2, C 2, E 1 (`.sve` skill rating) |
| 882 | 0x0058b198 | scalar | B | B 2, E 2, D 1 |
| 931 | 0x0059ae78 | scalar | C | B 2, C 2, E 1 (`.sve` fun rating) |
| 547 | 0x0053bbac | scalar array | C | B 2, C 2 (`.sve` 50 x 50 grid) |
| 648 | 0x005439e0 | record array | D | B 2, D 2 |
| 771 | 0x0056c770-0x0056c790 | scalar array | E | B 2, E 2 |
| 774 | 0x0056c7e4 | record array | B | B 2, C 2 (`.sve` 50 x 50 grid) |
| 838 | 0x005830b8 | scalar array | C | B 1, C 1, D 1, E 1 (50 x 50 byte grid after the golfers) |
| 1068 | 0x005a6378 | record array | B | B 2, C 2 (`.sve` 50 x 50 grid) |
| 1655 | 0x00839a58-0x00839a68 | scalar | D | D 2, E 2 |
| 1668 | 0x0083aaa4 | scalar | D | D 2, E 2 (g_blitX) |

The four `.sve` 50 x 50 grids (0x53bbac, 0x56c7e4, 0x5830b8, 0x5a6378: 0x9c4 bytes, `rep stosd` 2,500-byte clears) are
split B / C by one vote; re/PHASE1_PLAN.md gives `tile grids` to C. Stream G should settle them with B and C.

## 7. `model_check.py`

Input: one typed range per row (`module va size type count stride sign const fields name`; struct fields inline
`off:type[n],...` or `cstruct:<file>#<Name>`, whose `/* +0xNN */` comments are checked against the computed layout).
Every reference within a margin (default 0x100) of each range is checked. Errors (the type and the instruction cannot
both be right): `width_cross`, `unaligned`, `no_field`, `float_int`, `const_write`, `index_bound` (a constant bound
`cmp r, N` + ja / jae / jg / jge lets the index pass the end), `loop_end` (`cmp reg, imm` + jcc of a pointer loaded
with an address inside the range, whose last element is past the end: g_walkers declared with 63 records instead of 64
gives four, at 0x00417967, 0x0041f041, 0x0042774c, 0x00456c37). Warnings (read the instruction): `width_narrow`
(except flags8/16/32 fields), `width_block` (several array elements at once), `sign`, `stride` (only from tracked
multipliers: loop-step strides are not used), `base_outside` (an indexed displacement outside the range with the
range's stride, when that stride is above 8: a biased index, or a wrong base). Address-of uses
and string instructions are checked for range only. One scan serves all ranges (~25 s); `--all-functions` checks the
CRT too.

## 8. Not classified, unassigned

**6 unknown clusters**, each one .bss address with no width, no index and no content evidence (loaded into a register
whose use the straight-line tracker loses): 0x0053c5c8 (courseActionMenu, C3), 0x00583fa8 (drawGolferActionPanel,
C4), 0x00587fdc, 0x0058baa0, 0x005a57e8 (loadInterfaceArt, C3), 0x0059cab0 (mainLoop, C2). Five are touched by a C3/C4
function, so exit criterion 1 of re/PHASE1_PLAN.md section 5 (zero unknown clusters touched by a C3/C4 hook) is
**not met** by stream A alone; each needs the disassembly read at its reference.

**20 X clusters** (no voting caller): referenced only by save-tagged functions (0x0053e638 saveFieldIo /
serializeCourse / deserializeCourse; 0x0056125c saveCourseJpg; 0x00563338..0x00563390 promptAndSaveFile; 0x0056fcb0
and 0x0059d81c serializeCourse / clearBuffers; 0x00587da0 flushCourseInfo), util-tagged ones (0x00820d00
allocBlock / freeBlock; 0x0083ac10 allocResampleTable; 0x0083af80 / 0x0083af88 the sound-library open / close hooks;
0x0083b9f4 initSinTable / sinScaled; 0x0083be04, 0x0083bf04, 0x0083c004, 0x0083c104 resolveDataPath), or CRT array
initializers (0x00509688, 0x00540d98, 0x0083b170 g_keyTables). All 20 are touched by C3/C4 functions.

316 clusters have low confidence (buffer 172, record array 85, scalar 36, scalar array 23); 172 of them are .bss
addresses only passed to callees (`buffer`, low), whose layout comes from the callee: streams D and E take them from
the callees' parameter types.

## 9. Class vtables for stream E (`vtables.tsv`)

86 vtables (1,824 slots) and 17 vbtables; 66 have a class named by their installers (`X::ctor`, `X::dtor`, `X_ctor`,
...). The Window family has 91-slot vtables with 5-slot and 3-slot sub-object vtables and vbtables beside them:

| class (installers) | vtables (address:slots, b = vbtable) |
|---|---|
| root window (staticInit_404360) | 4ba2e8:5 4ba2fc:91 |
| EditBox | 4ba494:5 4ba4a8:91 |
| Button | 4bb094:5 4bb0a8:92 |
| ListBox | 4bb240:3 4bb24c:5 4bb260:91 4bb3cc:1 4bb3d0:3b |
| Dialog / TextView / LabelButton / view fields | 4bb3e4:5 4bb3f8:106; 4bb5a0:5 4bb5b4:92; 4bb724:3 4bb730:5 4bb744:91; 4bb8b0:3 4bb8bc:5 4bb8d0:91; 4bbab4:5 4bbac8:91; 4bbce0:5 4bbcf4:91; 4bbe90:3 4bbe9c:5 4bbeb0:91; 4bc088:3 4bc094:5 4bc0a8:91; 4bc224:3 4bc230:5 4bc244:91 |
| View / Base4805a0 | 4ba8b4:87 4baa14:1; 4baa1c:5 4baa30:91 |
| ListModel | 4bb21c:3 4bb228:1 4bb22c:2 4bb234:2b |
| Image | 4ba858:5 4ba86c:3 |
| Snd / SndMusic | 4bac68:33 4bacf8:61 4badec:3 |
| Screen / VoiceRx | 4badf8:38 |
| NodeList / NodeListB / NodeListC / StrList | 4baf00:43 4bafac:1; 4bafb0:25; 4bb014:27; 4baeac:21 |
| Flic / decoders | 4ba2dc:1 4ba2e0:1; 4ba288:1 4ba28c:6b; 4ba2b4:1 4ba2b8:2b |
| Cache482f60 / 482fd0 / 483010 | 4ba2a8:1 4ba2ac:2 4baba8:1 4babac:2 4babb4:2b 4babbc:1 |
| Timer, BinkPlayer, Reader, KeyTable, HotList, MappedFile, PairArray, Cursor, Buf | 1 slot each; the slot holds the function named `<class>::scalarDtor` or `<class>_deleteDtor` |

A slot count is the run of consecutive function-entry dwords up to the next installed start, so two vtables laid out
back to back with only the first installed read as one.

## 10. Findings for other streams

- `re/names/*_globals.tsv` types to correct (owners: the names files): g_blitSaveBuf, gTileBuffer, g_pairArray,
  g_logFile are objects (section 4.2); g_storyPairs' entry count (section 5).
- Golfer record: +0x16 is read signed at 8 sites; 16-bit fields +0xb8, +0x88.., +0xb6, +0xb0, +0xa4 are byte-tested;
  mainLoop's biased indexes (section 4.3).
- Hole records are 1-based from 0x005758a8; tile-type attributes start at 0x00578350; animals' base (section 5).
- Tie list (section 6), 6 unknown and 20 X clusters (section 8): the remaining ownership work for stream G.
