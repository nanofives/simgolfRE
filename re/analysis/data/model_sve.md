# Phase 1 stream G2: .sve round trip, ctypes mirror, uncertainty merge (2026-10-18)

Stream G2 of `re/PHASE1_PLAN.md` (stream G split: G1 merges the header, address map, overlaps, model_check and names;
G2 does the save-file side). Inputs: `re/tools/sve.py`, `re/analysis/save/0040afa0_save_serializer.md`, the six stream
headers `re/model/include/sections/*.h` and address maps `re/model/*_addresses.tsv`, the pinned scenario references
(`re/analysis/data/scenario_refs.json`, `log/c3/alt/refs`).

## 1. Files

| file | content |
|---|---|
| `re/model/sve_fields.tsv` | the .sve field table: 91 rows (2 headers, 86 fields, tail A, 2 tail-B blocks) |
| `re/analysis/data/sve_field_status.md` | named / placeholder / unnamed per field, the 63 newly named, 6 proposed rows for G1, overlaps |
| `re/tools/model_ctypes_gen.py` | C reader for the section headers + generator of the mirror (`--check` for staleness) |
| `re/model/model_ctypes.py` | GENERATED ctypes mirror: 104 structs, 6 array typedefs, 1,055 symbols, .sve and dump helpers |
| `tests/test_sve_roundtrip.py` | static: table vs sve.py, offsets, symbols resolve, mirror current, no addresses, fixture and top10 round trips |
| `UNCERTAINTIES.md` | 178 stream rows merged (U-0110..U-0514), IDs kept |
| scratch `log/p1/g2/` (not committed) | `build_sve_fields.py`, `g2_run.py` (scenario runs with saves), `roundtrip_report.py`, `merge_uncertain.py`, `g2_refs.py`, runs/, saves/, `roundtrip.json`, `saves_sha256.txt` |

## 2. Field table (`re/model/sve_fields.tsv`)

Columns: `index` (H0 / H1 / 0..85 / tailA / tailB0 / tailB1), `part`, `va`, `size`, `file_offset` (in a one-header file),
`offset_rule`, `present_if`, `sve_py_name` and `old_status` (sve.py NAMES), `mapping` (exact 84, slice 5, compose 0,
none 2), `model_symbols` (`sym`, `sym[k:n]`, or `sym@+0xoff` for a composed field), `element_types`, `streams`,
`typed_bytes`, `status`, `stream_sve_claims` (what the streams wrote in their sve_field column), `other_rows`
(overlapping rows not chosen) and `names_globals` (re/names rows inside the field). The test checks that its order,
VAs, sizes, tail masks and offsets equal sve.py's FIELDS / TAIL_A / TAIL_B and that every symbol resolves in the mirror
to exactly the field's size.

Status of the 89 serialized globals: 83 named (20 sve.py already named + **63 of the 69 unnamed**), 4 placeholders
(fields 25, 29, 46, 78), 2 with no model row (76, 77). Each of the 6 has a proposed row with evidence in
`sve_field_status.md` section 3: 25 `g_courseReportScore`, 29 `g_buildingUnlockLevel`, 46 `g_proChallenger`, 76 / 77 /
78 the name-string pool `g_namePool` / `g_namePoolOffset` / `g_namePoolLength` (slot 0 course name, 1..18 hole names,
20..29 best-round holders). Bytes: the 89 globals hold 448,913 bytes; 444,291 are in named rows, 4,622 in the 6 fields.

## 3. ctypes mirror (`re/model/model_ctypes.py`)

`re/tools/model_ctypes_gen.py` reads the six headers itself (defines, enums, `typedef struct`, array typedefs, static
asserts; checked first that the headers have no unions, bit-fields or pointer members) and lays structs out by MSVC's
default rules, which ctypes also follows. The build fails unless all **273 static asserts** (sizeof and offsetof) and all
**1,087 `+0xNN` offset comments** of the headers agree with the computed layout, and the generated module re-checks
every class's ctypes sizeof and member offsets at import. Symbols come from the address maps' `element_type` / `size`
(aliases: int32 / flags32 / ptr / vtable -> uint32-sized types; a pointer member of the original is a uint32 here):
1,055 typed; 4 not: `g_maskSpriteTable` (`{int16 flic; int16 variant}`), `unk_0055e5b0` (`Surface`),
`surface_820ed8` (`Surface object`), and the duplicate `g_surfaces5678b8` (D and E1). The module embeds no original
address (the test scans its hex literals); the .sve helpers read `sve_fields.tsv` for order, sizes and tail masks only.

```python
import sys; sys.path.insert(0, "re/model"); import model_ctypes as M
table = M.load_field_table("re/model/sve_fields.tsv")
headers, parts = M.split_sve(open(path, "rb").read(), table)   # parts: {symbol: bytes}, e.g. parts["g_golfers"]
golfers = M.view("g_golfers", parts["g_golfers"])                # Golfer * 156
golfers[3].mood, M.as_dict(golfers[3])["hole_strokes"]
blob = M.join_sve(headers, M.serialize(M.parse(parts)), table)   # byte-identical
parts, changed = M.value_roundtrip(parts)                        # rebuilt from Python values; changed == []
dump = M.split_dump(open("log/c3/alt/refs/ui_tour/det_b/end.bin", "rb").read(), table)
```

`value_roundtrip` rebuilds every part from its Python values into a zero-filled object, so a byte that lies outside
every member (padding, or a member the struct lacks) would come back as 0 and be reported. It found one mirror
defect during the work: ctypes returns a `char[n]` struct member cut at its first NUL; golfer-type records 76..79
(tail A) hold bytes after the NUL, so `as_dict` / `from_values` now move char arrays raw.

## 4. Round trip

Corpus: the 3 fixtures, 65 saves recorded by this stream (the game's own save routine 0x0040b4a0 called at every
checkpoint of 7 pinned scenarios), 15 saves the game wrote by itself during those runs (autosaves, ui_tour's
`While_Browsing.sve`), and 33 distinct saves earlier batches' scenario runs left in other instances. Each save is split
by the table, every part parsed and rebuilt from values, re-joined, and compared byte for byte with the file and field
by field with `sve.py parse`. Totals: **116 saves, 116 byte-identical, 0 field mismatches**; 0 parts changed by the
value rebuild.


Layouts covered: tail A only (87 fields, 296,973 bytes: every save except the championship ones; flag words
0x40000000 with bits 0x01000000 / 0x00040000 / 0x20000000 / 0x2 / 0x400 in 8 combinations) and tail B only (88 fields,
446,773 bytes: every championship save, flag 0x04200000). Not covered by any file on disk: no tail (86 fields,
294,733 bytes), both tails (89 fields, 449,013 bytes) and a two-header file (the
championship course is stored as `.cse`, 95,777 bytes, another format: `Themes\Championship\Ocean's Edge MC.cse`;
the table's H1 row and `split_sve`'s two-header path are untested). `original\top10.sve` (1,560 bytes) is C's
`top_scores` (TopScore[10] = 0x618 bytes; mainLoop writes 0x618 bytes from 0x00541ce0 at 0x0040fa72..0x0040fa82): it
round-trips through the mirror too.

Checkpoint dumps: `re/frida/scenario_ab.py` dumps every body field, tail A, both tail-B blocks and 8 bytes (RNG seed
0x00822d9c, seed time 0x0051b380) at each checkpoint (448,921 bytes); `split_dump` parses them by the same table.
**577 det_b dumps of 61 pinned scenarios** parse and survive the value rebuild unchanged.
Dump against save: each recorded save was taken on the blocked main thread right after its checkpoint dump, so they hold
the same instant: **65 / 65 equal part for part** (87 or 88 parts). Against the PINNED det_b dumps of the same
checkpoints: 61 / 65 equal; the 4 others are championship tour_0060, tour_0122, tour_0180 and tournament_end, which
differ in `g_financeYears` only, because the save routine itself changes the game: its header calls appendDate
0x0040d7b0, which refreshes the unsaved `g_year` 0x005a6d3c that finance code uses as a row index (sve_field_status.md
section 4). A control run of championship with the same agent and no save call equals the pinned reference at all 6
checkpoints, so the other 6 scenarios' saves do not move their timelines (their dumps equal det_b at every checkpoint).

| kind | source | saves | bytes | fields | flag words | identical | mismatches | = own dump | = pinned det_b |
|---|---|---|---|---|---|---|---|---|---|
| fixture | tests/fixtures/stories | 3 | 296973 | 87 | 1 distinct | 3/3 | 0 | - | - |
| recorded | championship | 6 | 296973, 446773 | 87, 88 | 2 distinct | 6/6 | 0 | 6/6 | 2/6 |
| recorded | course_season | 12 | 296973 | 87 | 2 distinct | 12/12 | 0 | 12/12 | 12/12 |
| recorded | cov_a_career | 15 | 296973 | 87 | 2 distinct | 15/15 | 0 | 15/15 | 15/15 |
| recorded | cov_b_career | 6 | 296973 | 87 | 2 distinct | 6/6 | 0 | 6/6 | 6/6 |
| recorded | cov_b_holes18 | 6 | 296973 | 87 | 2 distinct | 6/6 | 0 | 6/6 | 6/6 |
| recorded | sandbox_basic | 10 | 296973 | 87 | 2 distinct | 10/10 | 0 | 10/10 | 10/10 |
| recorded | ui_tour | 10 | 296973 | 87 | 1 distinct | 10/10 | 0 | 10/10 | 10/10 |
| organic | championship | 2 | 446773 | 88 | 1 distinct | 2/2 | 0 | - | - |
| organic | championship_nosave | 2 | 446773 | 88 | 1 distinct | 2/2 | 0 | - | - |
| organic | course_season | 2 | 296973 | 87 | 1 distinct | 2/2 | 0 | - | - |
| organic | cov_a_career | 2 | 296973 | 87 | 1 distinct | 2/2 | 0 | - | - |
| organic | cov_b_career | 2 | 296973 | 87 | 1 distinct | 2/2 | 0 | - | - |
| organic | cov_b_holes18 | 2 | 296973 | 87 | 2 distinct | 2/2 | 0 | - | - |
| organic | ui_tour | 3 | 296973 | 87 | 1 distinct | 3/3 | 0 | - | - |
| existing | 18 instances of earlier batches | 33 | 296973, 446773 | 87, 88 | 9 distinct | 33/33 | 0 | - | - |

## 5. Game runs (instance p1g2)

`re/tools/instances.py create p1g2`, shim built with `shim/build_instance.ps1 -Id p1g2` from the pinned build's batch
list (`log/c3/p1g2_batches.txt` = `log/c3/b2_batches.txt`); the result (winmm.dll `fb411952...`) is not byte-equal to
the pinned `381ed2b2...`, so every comparison with a reference was made here and not through the manifest's stale
check. `log/p1/g2/g2_run.py <scenario>` runs `re/tools/scenario_refs.py run` (hooks OFF, replay of the pinned
schedule, the shared boot lock `log/c3/alt/refs/boot.lock`, the s36a / s36b restores) with its output root moved to
`log/p1/g2/runs` and one addition to the agent: a `g2save` reply that calls 0x0040b4a0("g2_<checkpoint>") on the blocked
main thread after each dump. One game at a time; no control DLL was used, so nothing had to be restored. Runs:
sandbox_basic (10 checkpoints), course_season (12), championship (6), ui_tour (10), cov_b_career (6), cov_b_holes18 (6,
18-hole sandbox), cov_a_career (15), and the championship control without saves; all finished with every checkpoint.

## 6. Unnamed fields left

None without a proposal: fields 25, 29, 46, 76, 77, 78 have evidence and a proposed row for G1
(`sve_field_status.md` section 3). Until G1 takes them, `sve_fields.tsv` lists them as placeholder / unnamed. Not
determined inside named fields: the stream uncertainty rows (e.g. golfer record U-0009..U-0017, hole record U-0201).

## 7. Uncertainties merge

`log/p1/g2/merge_uncertain.py` appended the 178 rows of `re/model/{b,c,d,e1,e2,f}_uncertain.tsv` to `UNCERTAINTIES.md`
in ID order (b 64, c 13, d 31, e1 18, e2 37, f 15; IDs U-0110..U-0514, no collision with U-0001..U-0019, no
duplicates; idempotent). Format of the file kept (CRLF, one table row each, status `open`); the stream's `kind` is kept
in the description as `[stream s, kind k]` and mapped onto the file's four types (semantic 91, structural 76, tooling 7,
behavioural 4). The addr cell holds the stream's address only when it is one hex address (121 rows); other location
text (`SgEditBox +0x5a0`, ranges, lists) moved into the description, because re_classify.py's `norm_addr` parses that
cell with `int(a, 16)` for every row. Checked after the merge: re_classify reads 197 IDs, and no hooks.csv function
address has an open semantic / structural row (no C3 gate is blocked by the merge). The stream files are unchanged.
U-0311 and U-0473 are answered by section 3's proposals and stay open until G1 takes the rows.

## 8. Tests

`py -3.12 -m pytest tests/test_sve_roundtrip.py tests/test_sve.py`: 20 passed (the 3 fixture cases skip with a reason
when the untracked fixture files are absent).

## Appendix: sha256 of the saves this stream recorded or the runs wrote (copies in log/p1/g2/saves/, not committed)

The 33 saves found in other instances are listed in `log/p1/g2/saves_sha256.txt` with the rest.

| sha256 | file |
|---|---|
| `0d6436be906dd10971516ccc8795e0b3fa0908bb1ef2ef7f34c8fda7e24556b8` | tests/fixtures/stories/mood5.sve |
| `d37a263a01e705de16f8b16bae2e86ad1f617954af58b8293d3f2a2b618652b9` | tests/fixtures/stories/stories5.sve |
| `8f26a9f4fdf40ad7cc984e29165a59f2c55d3239f854fa68d43f8aef742e0c64` | tests/fixtures/stories/stories9.sve |
| `6c164bf7e2c1193315e5f97972c59c48609aa723f7058cccc82a78d2ba696797` | log/p1/g2/saves/championship/&AutoSave1.sve |
| `926b9600f07b2114c9188792797000a5da5f5e23cb07e731e8a40d47ec2bf957` | log/p1/g2/saves/championship/&AutoSave2.sve |
| `fb37847757c14971c5651ed691b6e6d73593bf2bcf0a3cd8b8fbf4e14dfc3b96` | log/p1/g2/saves/championship/g2_menu.sve |
| `63b0bf7debd37d83696b47db9d654c859ee0b8549726232a09ee0768e3cc3bdf` | log/p1/g2/saves/championship/g2_tour_0060.sve |
| `3bd583f56f3c857c01880ebf7b9e337e239456289b4e9fbfc920eba9b09c18b4` | log/p1/g2/saves/championship/g2_tour_0122.sve |
| `2349966a4d6ad43dec2771cef5509cb7c94221979d3b2284dea93b53dbe30d3f` | log/p1/g2/saves/championship/g2_tour_0180.sve |
| `d4eba1412befff6e6e66b0c055ef3abf0f6d03e91abdbc13fd9d407399c0d6d5` | log/p1/g2/saves/championship/g2_tournament_end.sve |
| `4a7c58c112ea4b35398305869220f128b6e9a94264e7a843417d2c008a4687bf` | log/p1/g2/saves/championship/g2_tournament_start.sve |
| `f06258aa8fc63ec6bf56033dc7e9b4daed58a2f11ef214ae544d5e4c07bbb7d1` | log/p1/g2/saves/championship_nosave/&AutoSave1.sve |
| `cc8ece66f01aeba28b09082b559d4db4913b529e4afe3c1456e78e52ecf370d7` | log/p1/g2/saves/championship_nosave/&AutoSave2.sve |
| `6615c976027230f1e73e685aece4131eabe0f6dea2e5939dcb17df9f71889280` | log/p1/g2/saves/course_season/&AutoSave1.sve |
| `ce0ddcfe7bc1b9e1038e53e968b5cc1c672d757fd2c026899191144f351fe471` | log/p1/g2/saves/course_season/&AutoSave2.sve |
| `38530d1357a9a196d42161debed0b3a0de19c98540dad862fe9283e71c9fc2bf` | log/p1/g2/saves/course_season/g2_hole_open.sve |
| `e7dcb1df0847f44f2838ca0c1ac7a05dd45e3612705ca8438129c925873999e6` | log/p1/g2/saves/course_season/g2_sandbox.sve |
| `5cc65d0f76491ea555c400e0f9278a7fce146297ab94e6f111761826180aa4a0` | log/p1/g2/saves/course_season/g2_season_060.sve |
| `2713d76060496c94d7450ab02f4ce969bd4d3c6691e7707a9e9c27324b11d768` | log/p1/g2/saves/course_season/g2_season_120.sve |
| `86dd3f20d6db650951f0c0e1fdf518ebbb76eecae3dda3979a6f1caadac70652` | log/p1/g2/saves/course_season/g2_season_180.sve |
| `6f6833883baa9455044b1d65114d76e2703b5e9b8a943311254a18d35f97a344` | log/p1/g2/saves/course_season/g2_season_240.sve |
| `8d58849b7a39d6bef1699b48da1c434ee30c7b77268666dd30bf18f1f35295b4` | log/p1/g2/saves/course_season/g2_season_300.sve |
| `eee4eaeff6cfd71ef15b11446852a67a9ae4d89af3a099bc54378ee039409b6a` | log/p1/g2/saves/course_season/g2_season_360.sve |
| `4bd131c8234d1a0e6bc853d8b900092bc9b93b60d422cc280f350474a59b3055` | log/p1/g2/saves/course_season/g2_season_420.sve |
| `fca8da1ec47c5cdbd5286fc1dce64f95ab582e5adcc7b77f3e5d939ed2c2ea97` | log/p1/g2/saves/course_season/g2_season_480.sve |
| `4b2df1d2b0f21855443b2c51c36a4196f9ec2f1fed638a0ba1c197e96b7c4646` | log/p1/g2/saves/course_season/g2_season_540.sve |
| `d252f0ee9954cf83f1afcad690e281b1290abbb110cd179b067b6b8a2a38b193` | log/p1/g2/saves/course_season/g2_season_600.sve |
| `d5ffc05e1e6c7d0c803f75e45a7bc743ad1bf9056711d7857f7ded8c567eed6a` | log/p1/g2/saves/cov_a_career/&AutoSave1.sve |
| `5a878ae777ba349d910385e9ce424c1f7143fc1f8d7ddee24bb1663242a218aa` | log/p1/g2/saves/cov_a_career/&AutoSave2.sve |
| `273c9869156aaf9234e4abcb9fd974b5bd1da02f52b56a3c54f8020d52dfe020` | log/p1/g2/saves/cov_a_career/g2_career_editor.sve |
| `c2e66a87095c926844279423807520bad577bd055c0f083ae1f7124fc91ec494` | log/p1/g2/saves/cov_a_career/g2_career_popup.sve |
| `d92510e2ae6cc4f3c025640d7944d7980c1e7a665d6288dc7b68543a0aa0cc9f` | log/p1/g2/saves/cov_a_career/g2_end.sve |
| `0e45d9a0028a0d3fb7c067711a9633372c80530b5357534a4d93ab4793a3c777` | log/p1/g2/saves/cov_a_career/g2_game.sve |
| `efaf94d4fef4db34b5822658c3d4f47dfe8cb079d45a1a6536586c5a31eaa047` | log/p1/g2/saves/cov_a_career/g2_hole.sve |
| `fb37847757c14971c5651ed691b6e6d73593bf2bcf0a3cd8b8fbf4e14dfc3b96` | log/p1/g2/saves/cov_a_career/g2_menu.sve |
| `91804743bb25152558e3ebb83990681a5bd1ea69de121845691bec92f2a863d5` | log/p1/g2/saves/cov_a_career/g2_render.sve |
| `1ad27d8a6e5744c2d80c3aeba38b490c688ace666647c123fac39f95f16d7f87` | log/p1/g2/saves/cov_a_career/g2_year_100.sve |
| `29edfb8fa6d5012e9c9202efe1f11ea08f358796c101d26e0543aef45ddbe52d` | log/p1/g2/saves/cov_a_career/g2_year_200.sve |
| `253bb2085e83362f7e3e094cf95c5cdc46f91f634667e4ad4254c47d0fda0ead` | log/p1/g2/saves/cov_a_career/g2_year_300.sve |
| `42f22b36ca3dc3ab173af888107bc383b271b11106def37fcc61c858d71dc9cb` | log/p1/g2/saves/cov_a_career/g2_year_400.sve |
| `6d793060cb52ededdb5b0a1f34025ebe88ff3f0da9e9ecfb1fcd1d473022e9f0` | log/p1/g2/saves/cov_a_career/g2_year_500.sve |
| `bead9553d2a2b1d4d6b4329755cc263ada6180637c60b3403a4fabc37c1eb19b` | log/p1/g2/saves/cov_a_career/g2_year_600.sve |
| `f8971ce894c8178e9223753458012cc092b1d78a96c1c96aba85301ddaed9d32` | log/p1/g2/saves/cov_a_career/g2_year_700.sve |
| `df373d1c361ba64484c83c941b13cee16f46a9e1b7393be8c04deaa6ebdc1815` | log/p1/g2/saves/cov_a_career/g2_year_800.sve |
| `4312261df494a5a2e355fec790c37b8c5c24d873e4dd2342f595af6194cb1c0d` | log/p1/g2/saves/cov_b_career/&AutoSave1.sve |
| `aa82511e6780ab78d96044cde9b0487f5800cf8ee421fccb8145dbab2e43d9cd` | log/p1/g2/saves/cov_b_career/&AutoSave2.sve |
| `bf2928d812af8786389f454d9a3acc25c039cbba011bfdd9721046a2abbc856b` | log/p1/g2/saves/cov_b_career/g2_career.sve |
| `6e3530fe566d1cb4f3a58d994eea122010c4a5015fe0acf7f865dd353931458a` | log/p1/g2/saves/cov_b_career/g2_hole.sve |
| `e0b59473a53104ba0ee489e6230dfeee4bcb820bd36cd8b1bc2d4c6d12404a68` | log/p1/g2/saves/cov_b_career/g2_offer.sve |
| `2983623bba0676ed6e19634259f2688c312986217b21c8564d12d6bc0143c25e` | log/p1/g2/saves/cov_b_career/g2_play.sve |
| `b613222fb7879d5199aa5d368d4f1cb81150961dbd9dc0fa2eaa1bfe5ef4c24f` | log/p1/g2/saves/cov_b_career/g2_year2.sve |
| `1ba135984e0e479687a6a9aba1f3c1a0c2e72f53e24799d8f92bad50a9b13465` | log/p1/g2/saves/cov_b_career/g2_year_end.sve |
| `a5274c5b7f3d5412d2362b0762c8ec56445b25082bd270bf3cea0857cedcb050` | log/p1/g2/saves/cov_b_holes18/&AutoSave1.sve |
| `0f7b8ec1d1f82dc71afe3f311d2eeb16743c7f02b367f617f1749b066a06c318` | log/p1/g2/saves/cov_b_holes18/&AutoSave2.sve |
| `f4295d527082b1705cfa5b36196442874aa7ab2288780be30269b997cb18c8e4` | log/p1/g2/saves/cov_b_holes18/g2_end.sve |
| `53a498e45f91470e75500057f24440cfa5df3247aeb983b4d26a48c26e18152d` | log/p1/g2/saves/cov_b_holes18/g2_hired.sve |
| `4cac67be6ee5435ea02db9afc39997b5e537ffa19361178fd1cd2943ed3a1ee6` | log/p1/g2/saves/cov_b_holes18/g2_holes.sve |
| `8326ae59395964ec775b9b14c6bad5e003e3ce71758c7c6257e3c9d42c0c1ff1` | log/p1/g2/saves/cov_b_holes18/g2_limit.sve |
| `d9784aebf3f47a7de47ed92a2763623610e14c194be44b07031fe5ac7ed0178b` | log/p1/g2/saves/cov_b_holes18/g2_long_holes.sve |
| `c372dbab5510998b14607b24b244481948482c22d43f92e7e21f5398b2676b0b` | log/p1/g2/saves/cov_b_holes18/g2_zoom1.sve |
| `fb37847757c14971c5651ed691b6e6d73593bf2bcf0a3cd8b8fbf4e14dfc3b96` | log/p1/g2/saves/sandbox_basic/g2_menu.sve |
| `e7dcb1df0847f44f2838ca0c1ac7a05dd45e3612705ca8438129c925873999e6` | log/p1/g2/saves/sandbox_basic/g2_sandbox_00.sve |
| `dbd1eeb260578f2ea044ab3c6d9d3ea309a3b81e70bc1abc518da439e83d6344` | log/p1/g2/saves/sandbox_basic/g2_sandbox_05.sve |
| `3dc416c61e78459abfec6bbb63d6333d73940b4f26c74656ebc2cbf297d156b6` | log/p1/g2/saves/sandbox_basic/g2_sandbox_10.sve |
| `9a19310d39bf5ab39bbca73071de5eeab7eeb0195d7136e22b30ea484c868855` | log/p1/g2/saves/sandbox_basic/g2_sandbox_15.sve |
| `57e882724155def76979828794956bd3f5623aa57f2426c483afefef6dee3ce9` | log/p1/g2/saves/sandbox_basic/g2_sandbox_20.sve |
| `a5d98159d4997c0fd3a461368fbf3a2204c819b78da35fdd7d7d4cfe1fe94ff3` | log/p1/g2/saves/sandbox_basic/g2_sandbox_25.sve |
| `67e9a0aad6a5560530aa588eb578466a9e8fa0a0acb6dd234f3a179adec39195` | log/p1/g2/saves/sandbox_basic/g2_sandbox_30.sve |
| `d2f335095c2d4647827ac55252fe67f426cec4eb70b93fd3b680ba9e2a79ea90` | log/p1/g2/saves/sandbox_basic/g2_sandbox_35.sve |
| `aad299a680e9e526e2c12e94d535a2e40ef2b7dc06cd3604232177ec81f74d2d` | log/p1/g2/saves/sandbox_basic/g2_sandbox_40.sve |
| `ff8e386423fcf255e0558b3d7f3e7ea72d2ebb1c724c85bafd0361f24063c307` | log/p1/g2/saves/ui_tour/&AutoSave1.sve |
| `6c7aa347cbbac74c5330b469e3ea8dfe7e0cec1433e5151a509a164a0835cc2c` | log/p1/g2/saves/ui_tour/&AutoSave2.sve |
| `508d2050301e4fabb580a84041c220c8c1f132be1d8413cb9c548b0ed242b1cd` | log/p1/g2/saves/ui_tour/g2_cancelled.sve |
| `48ac9128bc9a15ec0be1b2db740766f661f65c3f69ea013efd2f01916820ca3b` | log/p1/g2/saves/ui_tour/g2_end.sve |
| `81d01fd41924837473f3233cd7ac94f5ebf1bbe3cf0419eb4ddb9b4df2473953` | log/p1/g2/saves/ui_tour/g2_game.sve |
| `fb37847757c14971c5651ed691b6e6d73593bf2bcf0a3cd8b8fbf4e14dfc3b96` | log/p1/g2/saves/ui_tour/g2_menu.sve |
| `3b560b17f1439bf51fe734363d1daf10c120c37c60396e7fb37b32de63fbb0f7` | log/p1/g2/saves/ui_tour/g2_menu_done.sve |
| `08891a01884dcec962a1fc63e57c8d20d04745aaf48e0f0bdd8ce07331347765` | log/p1/g2/saves/ui_tour/g2_menus.sve |
| `b21cb64d88fc9f0f06344992336ad58c685d4d553133ae8f8977dac80f4a5f44` | log/p1/g2/saves/ui_tour/g2_people.sve |
| `e965e54eccebdfac35aa9f8ec47fcc5ef11802a664a80adbf1b53026b3ec4c20` | log/p1/g2/saves/ui_tour/g2_practice.sve |
| `6990804985a5d6f2f5da9cd9301c58d67e1806b155e45f7064f13c85735f94b4` | log/p1/g2/saves/ui_tour/g2_reports.sve |
| `a43a5ae915dea6b4c906ab848e464891c121491dee599195a3a01daa134403a9` | log/p1/g2/saves/ui_tour/g2_shot.sve |
| `de35cd4ed72d79c14c553e916a40da3dd135c0318c7e0cc0a3ff1a3e7cae23c3` | log/p1/g2/saves/ui_tour/While_Browsing.sve |
