# golf_clean.exe 0x0040afa0 save-game serializer (.sve format)

Module: `golf_clean.exe` (anchor in `re/anchors.json`). Parser: `re/tools/sve.py`.
CRT names below come from Function ID against `re/fid/simgolf_vc6.fidb` (VC6 12.00.8168 libs, unique best
match, `re/fid/golf_clean.exe.fid.tsv`): `FUN_004a5d48` = `_open`, `FUN_004a583a` = `_read`,
`FUN_004a5b58` = `_write`, `FUN_004a5a78` = `_close`, `FUN_004ad425` = `_itoa`.

## Purpose
One routine serializes the whole game state in both directions. `FUN_0040afa0(int mode)` stores `mode`
in `DAT_0053e638` and calls `FUN_0040af70(addr, size)` 86 times unconditionally, plus 1 or 2 more per tail block, on a
fixed list of globals. `FUN_0040af70` calls `FUN_004a583a` (read) when `DAT_0053e638 != 0` and
`FUN_004a5b58(DAT_00568d08, addr, size)` (write) otherwise. The file is the concatenation of those
memory ranges: no compression, no checksum, no version field, no per-field tags.

## Save: FUN_0040b4a0(char *name)
- Path: `"saved games\\"` (`s_saved_games__004c3f4c`) + `name` + `".sve"` (`DAT_004c3f44`, bytes `2e 73 76 65 00`).
- `FUN_004a5d48(path, 0x8301, 0x80)`. `0x8301` = `_O_BINARY|_O_TRUNC|_O_CREAT|_O_WRONLY` and `0x80` =
  `_S_IWRITE` per `tools/vc6/vc98/include/fcntl.h` lines 28-41 and `sys/stat.h` line 146.
- Header text built in the 100-byte buffer `DAT_0051a068`: `"-"` (`DAT_004c52bc`), course name
  (`FUN_0040daa0(0)`), `", "` (`DAT_004c52b8`), day, `" "` (`DAT_004c3f70`), month and year (`FUN_0040d7b0`).
- `FUN_004a5b58(fd, DAT_0051a068, 100)`: the **whole** 100 bytes are written, so bytes after the
  string's NUL are whatever the buffer held before (other UI strings: `stories5.sve` carries
  "evious Game", "Save Course for Championship", "Save Gary Golf for Champ").
- `FUN_0040afa0(0)`, then `FUN_004a5a78(fd)`.

## Load: FUN_0040b9b0(char *name, int a2, int a3, int champ)
- Folder `"saved games\\"` when `champ == 0`, `"Themes\\Championship\\"` (`s_Themes_Championship__004c5328`) otherwise.
- `FUN_004a5d48(path, 0x8000)` (`_O_BINARY`), `FUN_004a583a(fd, DAT_0051a068, 100)`; a **second**
  100-byte read when `champ != 0`. `a3 != 0` closes the file after the header (header-only read).
- `FUN_0040afa0(1)` then post-load resets (`DAT_0059e7b8` keeps bit 2 from before the load and gains
  `0x40000000`; `FUN_0042f7a0`, `FUN_0042f340`, `FUN_0042f2c0`).

## Date field (DAT_00834170)
Header formatting reads it, which ties the body field to the visible date:
- day = `((v & 0x3ff) * 0x1e >> 10) + 1` (`FUN_0040b4a0`, `FUN_004ad425` = int to decimal string, radix 10).
- month = `PTR_s_March_004c2908[(v >> 10) & 7]`: March, April, May, June, July, August, September,
  October (pointers `0x004c3e10` .. `0x004c3dd8`).
- year = `(v >> 13) + 0x7d1` (2001) (`FUN_0040d7b0`; it also stores `v >> 13` in `DAT_005a6d3c`).
`sve.py check` recomputes the date from the body and compares it to the header text: 3/3 files agree.

## Layout
File = 100-byte header (200 for championship files) + the `FIELDS` list in `re/tools/sve.py`, in the call
order of `0x0040afa0`..`0x0040b492`. Mixed sizes are packed with no padding: `DAT_005a34e0` is 1 byte,
so the dword after it starts at file offset `0x75`.
Conditional tail, tested on `DAT_0059e7b8` after it has been restored (it is field 71 of the list):
- bit `0x40000000`: `DAT_004e06c8`, 0x8c0 bytes.
- bit `0x00200000`: `DAT_0058f338` 0xb7c0 bytes, then `DAT_00520a28` 0x19a28 bytes.
Body size: 296,873 bytes with only bit `0x40000000` set, the state of all three saves on disk
(flags `0x41040000`); file size 296,973 = 100 + 296,873. `sve.py check`: byte-exact round trip on
`&AutoSave1.sve`, `&AutoSave2.sve`, `stories5.sve`.

## Observed differences between two autosaves (16 April 2007 -> 1 May 2007, same session)
26 of the 87 serialized fields (86 + tail A) change; `date` goes `0x0000C600` -> `0x0000C800`. Largest: `g_0059FC60` (3244 of 0x3880
bytes), `g_005794B8` (1286 of 0x9c00). Field meaning beyond `date` and `flags` is not established.

## Named fields (sve.py NAMES)
Course summary, from `FUN_00431fa0` (called only by `FUN_00432170`): opens `<name>.srf` (`".srf"` at
`0x004c791c`) with mode `"wt"` (`0x004c7924`) and `fprintf`s `[Course]` and one `Label=` line per global.
The label strings sit at `0x004c7844`..`0x004c7918`:
| global | label | conversion in FUN_00431fa0 |
|---|---|---|
| `DAT_005685f0` | `Holes=%d` | v - 1 |
| `DAT_0059aafc` | `Par=%d` | v |
| `DAT_0058d36c` | `Yards=%d` | v |
| `DAT_005a34e0` (1 byte) | `Type=%s` | `PTR_DAT_004c3078[v & 3]`: 0 Park, 1 Desert, 2 Tropical, 3 Links (strings `0x004c30a4`, `0x004c309c`, `0x004c3090`, `0x004c3088`) |
| `DAT_00567328` (char[100]) | `Theme=%s` | string |
| `DAT_0056a524` (10 dwords) | `Record=%d by %s` | dword [0]; the name comes from `FUN_0045b9f0(0x14)` |
| `DAT_00571fd4` | `Cash=%d` | v * 100 |
| `DAT_0059ae78` | `FunRating=%d` | v |
| `DAT_00541cd8` | `SkillRating=%.2f` | v * `DAT_004ba488` (double `0x3f847ae147ae147b` = 0.01) |
| `DAT_005a882c` | `LengthSkill=%.2f` | v * 0.01 |
| `DAT_0056949c` | `AccuracySkill=%.2f` | v * 0.01 |
| `DAT_005a636c` | `Imagination=%.2f` | v * 0.01 |
Cross-checks: `tests/fixtures/championship/Ocean's Edge MC.srf` (written by the game on "Save Course for
Championship", fixture README) has these keys in this order, plus `Name=` and `Designer=` as in the code, with `Type=Park`. For `stories5.sve`, `sve.summary` gives Holes 5, Par 15, Yards 430, Park,
Standard, Record 10, Cash 1,000,000, Fun 99, Length -0.15, Accuracy 0.99, Imagination 0.20: the values the
Load Previous Game panel shows for that file (`log/ttd/stories_load/last_frame.png`; `tests/test_sve.py`).
Also named from existing matched sources: `tile_type` `0x005722e8` and `tile_byte` `0x0056988c`
(char[50][50], `re/match/golf_small.cpp`), `golfers` `0x005794b8` (0x9c x 0x100, `re/match/golf_story.cpp`),
`golfer_types` `0x004d6088` (holds the 0x230-stride records read at `0x004d60a8`, `re/match/golf_util.cpp`;
0xa640 = 76 x 0x230), `placed_objects` `0x0058bcb8` (256 x 0x10, `re/match/wip/golf_objects.cpp`, not matched).
Per-global reference counts for all 89 serializable globals: `re/analysis/save/sve_globals_refs.tsv`
(`ghidra/scripts/RefsByRange.java`).

## Runtime (TTD trace log/ttd/stories_load/golf_clean01.run, 2026-10-03)
Main menu -> Continue Saved Game -> `stories5.sve` row -> Load: `0x0040b9b0` executes 6 times,
`0x0040afa0` 2 times, `0x0040af70` 174 times (= 2 x 87). Both `0x0040afa0` calls have `mode = 1`
(`[esp+4]`) and return to `0x0040badb` (inside `FUN_0040b9b0`). Which UI step triggers each of the two
full loads is not established.

## Open questions
- [UNCERTAIN U-0004] Semantics of the other serialized globals. Next step: name them from their readers in
  Ghidra, starting with the large arrays (`g_00543D10` 0x19a28, the same size as the bit-0x200000 block
  `DAT_00520a28`).
- `original\top10.sve` (1,560 bytes, string `op10.sve` at `0x004c52ad`) does not fit this layout; its
  reader and writer are not located yet.
