# Phase 1 stream G1: static integration of the data model (2026-10-18)

Stream G1 of re/PHASE1_PLAN.md: merge the six stream models (B, C, D, E1, E2, F) into one header and one address map,
resolve every overlap, account for every cluster, check the whole map against every access and against the C3 / C4
hook sources, and settle the names. Stream G2 (the .sve round trip, the ctypes mirror, the UNCERTAINTIES merge) runs
in parallel and is not covered here. NO-GUESSING applies; decisions are in re/model/DECISIONS.md (2026-10-18, G1).

## 1. Files

| file | content |
|---|---|
| `re/tools/model_merge.py` | the merge, re-runnable: `merge` (address map + overlap scan + struct-size check), `overlaps`, `coverage [--json]`, `header [--check]`, `checkinput` (model_check input), `hooks` (hook literal cross-check). RESOLUTIONS / G_ROWS / MARKS / CANONICAL_TYPE hold every decision with its instructions |
| `re/model/addresses.tsv` | 1,119 rows: symbol, module, va, size, element_type, c_type (canonical header type), count, stride, signed, stream, sve_field, clusters, relation (top 1,102 / member_of 11 / element_of 3 / alias_of 3), parent, evidence, confidence, stream_symbol |
| `re/model/include/simgolf_model.h` | the merged C99 header, generated from the six sections plus an integration block (cross-stream asserts) |
| `tests/test_model_header.py` | 14 static tests: header current, no address in it, map without overlap / duplicate / size disagreement, MSVC x86 / x64 C and C++ at /W4 /WX, clang front end (C99 / C11 / C++17, i686 and x86_64 Linux), a wrong size assert fails with both compilers |
| `re/analysis/data/model_defects.md` | 33 original defects (incl. G2's save-refreshes-g_year port requirement) indexed with instructions, and the 10 checker errors that are correct code |
| `re/analysis/data/model_name_changes.md` | every names change with file, address, old, new, evidence |
| `re/names/model_g1_globals.tsv` | 12 new global names; `re/analysis/systems/model_g1.md` is the names_validate stub |
| `log/p1/g1/` (scratch) | check_all.tsv (model_check input, 1,102 ranges), viol_all.tsv, hooks.tsv, coverage.json, cc/ (compile scripts), names backup, apply_names.py, names_scan.py, sgdis.py, ctxm.py |

Edits to the stream files (each a verified error): e2.h `NodeListBucket` (vtable at +8, not +0: FUN_004877d0 stores byte
[+0] = 1, dword [+4] = 0x7f, dword [+8] = 0x4bafac, 0x004877d4..0x004877ee) and e2_classes.tsv's rows for it; e2.h two
static-assert tags (`MappedFile_size`, `StdList_size` used twice: a typedef redefinition, an error in strict C99, found by
clang); e2_vtables.tsv vbtable 0x4ba28c cut to its 2 entries (the next dwords are 12 zero bytes and the double 1.0 at
0x4ba2a0); c.h `Animal` +0x38 / +0x39 named heading / walk_steps from stream B's evidence (0x00401ab0..0x00401ac8,
0x00401b13, 0x00401bd0..0x00401be2, re-read for this merge).

## 2. Method

1. **Merge** (`model_merge.py merge`): load the six `<s>_addresses.tsv` maps, E2's 45 vtables that only e2_vtables.tsv
   lists, G1's 17 rows; apply RESOLUTIONS; take a names-file label as the symbol where one sits at the object's start.
2. **Overlap scan**: any two rows whose byte ranges intersect must be nested with a declared relation (member_of /
   element_of / alias_of and `parent`); anything else is an overlap. Before resolution: 20 intersecting pairs, 3 of them
   partial (D's 0x519a20 Surface against E1's FontRef 0x519a40, D's credits pointers against E2's Reader, D / E1's twin
   0x5678b8 array). After: 0 overlaps, 17 nested, 0 duplicate symbols, 0 struct size disagreements (stride = sizeof and
   size = count x stride for every row typed with a header struct).
3. **Coverage**: each kept inventory address must lie in a row, be an end marker of a row, or be a MARK (biased base or
   loop end, with the instruction).
4. **Header**: concatenate the sections, rewrite address tokens in comments (code addresses dropped, data addresses
   replaced by an address-free map symbol), append the integration asserts; compile with two compilers.
5. **Check**: each stream's own model_check rows (they carry the struct field lists the stream verified) plus rows built
   from the map (nested structs flattened) for G1's rows, E2's vtables and edited rows; run over all game-logic functions.
6. **Hook cross-check**: every literal address in shim/src/re/*.cpp code (negctl sources excluded) inside a typed range,
   width from the idiom around it, `+ k` terms added, compared with the field the address lands in.

## 3. Results

### 3.1 Header

`re/model/include/simgolf_model.h`, 2,466 lines, sections in the order f, c, b, d, e2, e1 (no section uses another's
types). Compiles clean at /W4 /WX with MSVC 19.44 (cl): x86 C (default, /std:c11, /Za), x86 C++17 and C++20
(/permissive-), x64 C (/std:c11, /Za) and C++17: 8 configurations. Second compiler: no clang, gcc, zig or tcc is installed
on this PC, but the Build Tools ship `clang-tidy` 19.1.5, whose clang front end parses and type-checks the header: C99,
C11 and C++17 with -pedantic -Wall -Wextra -Werror for i686, x86_64 and aarch64 Linux (System V layouts, freestanding
stdint / stddef) and C11 for i686-pc-windows-msvc with the MSVC headers: 10 configurations, all clean; every size and
offset assert holds under every ABI (the sections store pointers as uint32_t and no struct holds an 8-byte member at an
offset the ABIs align differently). Negative control: an assert `sizeof(Snd) == 0x6d` fails with both compilers.
The clang pass found one conformance error MSVC accepts (the duplicate typedef tags above).

### 3.2 Address map and coverage

| measure | done | total |
|---|---:|---:|
| clusters accounted for (stream A's 1,767) | 1,767 | 1,767 |
| kept addresses: inside a row / end marker / MARK | 5,931 / 11 / 7 | 5,949 |
| addresses inside a row, by the row's confidence: high / medium / low | 5,323 / 376 / 232 | 5,931 |
| cluster weight by referencing functions (stream A's cluster function counts) | 5,914 | 5,914 |
| address weight by referencing functions (sum over addresses) | 11,108 | 11,108 |

Per owner (clusters / addresses / function weight, all 100%): B 178 / 549 / 1,068, C 70 / 227 / 492, D 446 / 529 / 790,
E1 410 / 710 / 1,347, E2 277 / 420 / 1,164, F 366 / 3,487 / 1,011, X 20 / 27 / 42. Rows per stream: F 374, E1 209,
E2 197 (152 + 45 vtables), B 164, D 108, C 50, G1 17.

**Exit criterion 1** (every cluster classed, no unknown cluster touched by a C3 / C4 function): met. Stream A's 6 unknown
clusters are elements of SpriteRef arrays: 0x53c5c8 = g_sprites53c570[2], 0x583fa8 = g_sprites583a80[30], 0x587fdc =
g_surfaces587ea8[7], 0x58baa0 = g_sprites58b680[24], 0x59cab0 = g_surfaces59ca58[2], 0x5a57e8 = g_sprites5a53c8[24].
**Exit criterion 2** (every bss cluster referenced by a C3 / C4 function typed): met, 1,242 of 1,242 (1,716 of 1,716 in
all sections).

The 20 X clusters: typed by G1 (g_animSprites SpriteRef[591] 0x509688, g_sprites540d98 SpriteRef[32], g_saveIoRead
0x53e638, g_jpegSrcPitch 0x56125c, g_namePool char[0x1002] / g_namePoolOffset int16[0x80] (save fields 76 / 77),
g_pendingCourseInfo 0x587da0, g_resampleBuf int16* 0x83ac10, g_keyTables KeyTable[4] 0x83b170, g_sinTable int32[256]
0x83b9f4, g_dataDirA / B / C and g_pathScratch 0x83be04..0x83c104) or marked (0x56e950 animal loop end, 0x59d91c
g_namePoolOffset loop end). Also typed by G1: the money-popup ring x / y 0x542fd8 / 0x542ff8 (C cluster 647) and the double
1.0 at 0x4ba2a0. Other MARKs: 0x5722e7 (tile_type biased base), 0x587e7c (g_surfaces587ea8 biased), 0x5a8c34 (1-based
g_objectTypeLevel), 0x838a8c (g_storyFiles loop end), 0x838c1b (genNoise biased base).

### 3.3 model_check (exit criterion 3)

1,102 ranges, every access of the 1,812 game-logic functions (mainLoop's whole body included): **13 errors, 770
warnings, 0 unexplained**. Errors: 3 original defects (golfer-156 stores x2, the dword read of 0x838220;
model_defects.md 1 / 14), 8 compiler-inlined string copies read by dwords / words (type `char` is right), 2 loop-end
compares of a pointer that is not the accessed one (corner_cache, g_sprites53df58). The first run had 6 more errors on
E2's vbtable 0x4ba28c (x87 reads of 0x4ba2a0): a wrong vbtable extent, fixed. Warnings: stride 445, width_narrow 193,
base_outside 64, sign 36, width_block 32, all on stream ranges whose write-ups explain them (the six reports listed
767); none falls on a row G1 typed or edited.

### 3.4 Hook cross-check (exit criterion 4, first half)

15,797 literal uses in typed ranges: ok 7,625, narrow 242 (byte / word reads of wider fields as the original does),
untyped 7,876 (address-only: lea, pointer bases, tables of addresses), **wide 51 and stride 3, all explained, 0
unexplained**: 39 transcribe a block copy or clear the original makes with `rep movsd / stosd` or dword moves (attribute
table copy from 0x4c1a40, golfer / pro skill bytes, phrase and type backups, g_playerSkills, record banks, shot-track
snapshots, tile screen cache, comment counts, shade grids, the rough fill, top scores, the 23-byte 0x58dd34 fill
0x0040f939..0x0040f960); 4 the inline string copies of 3.3; 5 read E1's opaque Image bytes of g_rootWindow at E2
Image fields with their own width (+0x04 surface, +0x5c text_font, +0x6c text_colour); 3 transcribe the defects of 3.3;
3 `stride` are the tool reading the nested tile index (x * 0x32) or the 0x1c bucket stride inside g_musicDevice.

### 3.5 Names

80 changes (model_name_changes.md): 24 function renames (class prefixes now follow the model: TextView, CompositeView,
Window, PaletteCache, FontRef, S3ListNode, ListNode10, SndGroup_*), 2 purpose corrections, 5 data labels withdrawn
(gRngState, gTextBuffer2Index, g_animals, g_typeAttr, g_recordOccupancy), 11 renamed (g_pendingCourseInfo, g_polyFill,
g_netInterface, g_windowSlotA / B, f_tab_tileTypeAttrInit_4c1a40, and the save fields named from G2's proposals:
g_courseReportScore, g_buildingUnlockLevel, g_proChallenger, g_namePool, g_namePoolOffset), 26 retyped, 12 added
(g_namePoolLength among them); 51 map symbols replaced by
the names-file label. names_sanitize, names_validate (all touched ids) and names_check_evidence pass; hooks.csv followed
through `re_classify.py retag` (24 rows, levels unchanged). ApplyNames was not run.

## 4. Decisions

re/model/DECISIONS.md, section 2026-10-18 stream G1. In short: E2 / E1 class names are canonical (D's placeholders and
E1's opaque bytes are asserted equal in size and offsets); names-file labels are the map symbols; nine overlaps resolved
by instruction evidence; F's 0x4c1a40 is the TileTypeAttr[23] file image; the vbtable 0x4ba28c has 2 entries.

## 5. Defects index

re/analysis/data/model_defects.md: 13 out-of-object accesses, 16 wrong-width / never-written / data-loss defects, 3 dead
state groups, and the port requirement that saving refreshes g_year (G2's measurement, entry 33); two found here: sinScaled reads entry 256 of a 256-entry table (0x00491ca7) and resolveDataPath's three
path prefixes are never written (0x00491f53, 0x00491e38, 0x00491ecf).

## 6. What remains untyped

Every referenced address lies in a typed object; what is open is meaning, not extent:
- 178 U rows in the stream files (b 64, c 13, d 31, e1 18, e2 37, f 15), merged into UNCERTAINTIES.md by stream G2;
- 485 header lines declare neutral members (`f_XX`, `unk_XX`): E1's 354 fields without a meaning, E2's
  constructor-only fields, D's Snd +0x0c..+0x2f;
- extents fixed by the next referenced address rather than an instruction (confidence low / medium): g_pendingCourseInfo
  (0xdc), g_dataDirA / B and g_pathScratch (0x100 each), g_dataDirC (1 byte read);
- 232 addresses sit in low-confidence rows (mostly E1 `g_ui_*` panel words and D's opaque Surface singles).

## 7. How to use the model (phase 3)

1. **Symbol -> address**: `re/model/addresses.tsv` (grep the symbol, or the stream symbol in the last column). `c_type`
   is the struct in `simgolf_model.h`; `relation` / `parent` say whether the symbol is a field or element of a bigger
   object (use the parent's type then).
2. **Address -> object**: `py -3.12 re/tools/model_merge.py overlaps` lists the nesting; the row containing a VA is the
   one whose [va, va + size) holds it (`model_merge.coverage` does the lookup).
3. **Struct -> evidence**: the member comment in `re/model/include/sections/<s>.h` (with instruction addresses), the
   stream tables (`e1_fields.tsv` lists every access of every member, `e2_classes.tsv`, `b_types.tsv`), and the stream
   write-up `re/analysis/data/model_<s>.md`.
4. **Constants** (decision 1): rows of stream F are descriptors; read the bytes from the user's own golf_clean.exe at
   `va - 0x400000` (file offset = va - section va + raw pointer: .rdata 0x4ba000 -> 0xba000, .data 0x4c1000 -> 0xc1000).
5. **Defects**: before porting a function, look up its accesses in model_defects.md and decide keep or fix.
6. **Changing the model**: edit the section file (and the stream map), add a RESOLUTIONS row when two streams disagree,
   then `model_merge.py merge`, `header`, `checkinput` + `model_check.py`, `hooks`, and `pytest tests/test_model_header.py`.
