# S6.6 notes: verification of the unverified defect rows by disassembly and Unicorn

Output: `log/roadmap/S6/6_decisions.tsv` (31 rows for the 22 requested ids, 17 columns per SCHEMA.md; D40, D42 and D52 are split as S6.2
split them, D42 and D52 in one more arm each), `6_uncertain.tsv` (U-1580..U-1589), probes and logs in `log/roadmap/S6/6_probes/`.
No port code, no game boot, no Frida. The original ran in Unicorn over the file image (`original/golf_clean.exe`, no hooks) with a
GameState image from the segment corpus (`port/corpus/segments/{sandbox_basic,course_season}/00/start.data.bin`) and the writes diffed.

## Method
- `6_probes/plib.py`: capstone listing of any range (`dis`), exe-image reference scan (`refs`, `pr_refs`), address-map lookup (`syms`),
  `emu(entry, args, data, ...)` returning `completed | hang | fault | import` with registers, eip, every changed data byte and the
  unmapped accesses; hooks to script `Random::range` (`script_random`), record visited addresses (`visit`), pre-fill the stack
  (`stack_fill`). `hang` = the instruction budget (20 M default, up to 1.2 G where stated) was used up; a control that completes
  runs next to every hang so the budget is known to be sufficient (the full generateRandomCourse needs 60..200 M instructions).
- Probe scripts: d07_stamp, d11_spawnwalker, d40_habitat, d41_spawnobject, d42_golfpro, d43_placeanimal, d46_replace, d50_heightfield,
  d51_bio, d52a_basetype, d52b_search, d52b_seeds, d52b_clubhouse, scans.py (static scans); each writes `<name>.log` next to it
  (d52b_run1.log, d52b_search.log, d52b_clubhouse.log from the runs recorded here). Reproduce one: `cd log/roadmap/S6/6_probes && ~/bin/py -3.12 d43_placeanimal.py`.
  `make_decisions.py` regenerates 6_decisions.tsv.
- D5 reading used: `fix` for a hang or fault proven on a constructed state where the fix is a precheck that cannot change a run the
  original completes, even where reach of that state in play is open (the U-row records the open reach). Rows whose class itself
  depends on reach (D12, D49a, D52a) and every silent overrun stay `ask` or `keep`. The lead may reread this rule: the rows it
  affects are D8, D40a, D40b, D41, D42b, D52b1, D52b2 (hang states built by hand).

## Changes against S6.1 / S6.2
| id | S6.1 / S6.2 | S6.6 | why |
|---|---|---|---|
| D7 | overrun-silent, ask, changes_sim yes | harmless, keep, all no | the scan only reads; index >= 0x100 returns 0 at 0x0040e30f and writes no record, so the original already refuses a full table; the scan ends at 0x0058dd48 (state) or 0x008431f8 (.rsrc) in every probe |
| D8 | ask | fix (D40a's switch `placeHabitat.clusterLoopBound`) | the read decides only whether the D40a loop ends; byte != 0xff completes, 0xff hangs |
| D11 | hang, fix | overrun-silent, ask | Unicorn: 64 live walkers complete, return 64 and write the record over g_editQueueX; no hang, no fault in any state tried |
| D13 | overrun-silent, ask | harmless, keep | the copy is of the type NAME to its NUL, not of a 0x230-byte slot; destination holds the 19-byte sentinel; the longest of 76 shipped names is 10 characters |
| D22 | overrun-silent, keep | harmless, keep | the loop keeps ebp == edi * 50 (0x00449781 / 0x00449782) and exits before either goes negative; the register test is equivalent here |
| D3 (movie half) | unknown | dead-state inside an overrun-silent row | g_movieCount 0x008406e4 and g_movieIds have no writer; the list is always empty |
| D40b | keep | fix (with D40a) | the -1 link write happens only inside the D40a hang |
| D42 | one fix row | D42a fault fix, D42b hang fix, D42c hang ask | d = 4 / -4 fault and an empty pro roster hang as stated; new: d = 5 / 6 never ends even with every record filled (4 - d < 0 shrinks the window) |
| D48a | 0x40-byte slots | slots of 0x38 and 0x80 bytes | slot spacing 0x5c - 0x24 at 0x0046c4ba / 0x0046c6ac |
| D49b | keep | keep, now read | `and eax, 0xe0` then `cmp eax, 0x100` (0x00427114..0x0042711e) |
| D50 | hang, fix | harmless, keep | the flags table is an exe constant (0x004c1a40) with no type carrying bits 0, 1 and 2; Unicorn completes every grid on shipped flags and hangs only with type 21 forced to 7 |
| D51 | hang, fix | harmless, keep | every pick has 5..8 candidate types and consumes one; Unicorn with a cycling draw completes |
| D52b | one fix row | D52b1 fix, D52b2 fix, D52b3 ask | two searches proven to hang on a constructed grid (type-0x11 window, clubhouse window); 0x0047172e is structural only |
| D52a | wrong-value, ask | same, with the effect measured | the base type equals the pre-filled stack byte; with stack 0x00 / 0x55 the type 4 runs did not finish in 400 M instructions |
Rows unchanged in class and recommendation: D1 (values and readers now known), D2, D4, D12, D36, D40a, D41, D42a / b, D43, D46, D47a, D47b, D48b, D49a.

## Numbers
- recommendation over the 31 rows: fix 10 (D8, D40a, D40b, D41, D42a, D42b, D43, D46, D52b1, D52b2), ask 10, keep 11.
- Hang or fault proven in Unicorn with a completing control: D40a, D41, D42a, D42b, D42c, D43, D46, D50 (forced table only), D52b1, D52b2.
- Hang or fault asserted by S6.1 / S6.2 and NOT reproduced: D11 (completes), D50 and D51 on shipped data.
- No row's recommendation depends on an unrun probe except D47a, D48a, D52b3, D49a, D12 (all `keep` or `ask`).

## Controls
Each hang probe has a control that completes at the same budget (d40 arms (a) and (c), d41 cell free, d42 one record filled, d52b_search
type-0x11 present, d52b_clubhouse window as generated, d46 tail 0x100 / 0x1ff, d43 count 6). Hand-built states changed only the fields
named in the row; a probe is not a port test and adds no negative control to the port.

## Open
U-1580..U-1589 in 6_uncertain.tsv. Largest: reach of the hand-built hang states in a played game (U-1581, U-1582, U-1586), the
`g_difficulty` 5 / 6 reach of loadGolfPro (U-1583), course types 4 and above from theme data (U-1588), g_textBuffer length at the
story cache fill (U-1584), the draw-list append site of D3 (U-1589).
Not run: loadStoryStrings (needs the CRT fopen / fgets stand-ins; the D47 rows rest on the disassembly and the story-file scan),
netQueueMessage, the D52b3 loop.
