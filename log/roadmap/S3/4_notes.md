# S3.4 notes: FLIC decoding of loadFlic's sprite-sheet cells

Worker: claude2 opus (session cmvmrnwb33stzo115k44eqook), 2026-10-24. Lead: S3 (cmvmns8sn3fsbo115g4dgm1nf). Game slot: instance
`instances\s3_4` (pinned winmm.dll 381ed2b2 copied from instances\p24), hooks off, C3FT_AUDIO=none. Status: DONE.

## 1. Result

- New module `flic` (port/src/sim/flic.cpp, port/include/portsim/sim/flic.h): a plain decoder over the user's .flc bytes
  (GameFiles), no GameState read or write. `flic::load(files, names, {path, frames, size, flag})` gives loadFlic's cells (cropSprite
  result, SpriteRef geometry +0x10 .. +0x28, pixel bytes), the column count, the sequence count and the palettes of the load's slot
  and the shadow slot. `flic::sheet_to_gfx` is the hook-up menu.cpp calls (log/roadmap/S3/4_merge.md section 2): it puts the cells
  into the gfx backend's sprites under their g_cellPool addresses. Test entries `flic.load`, `flic.toGfx`.
- Oracle: the original's sheet memory recorded after every loadFlic (re/tools/flic_cells_rec.py + re/frida/js/flic_cells.js).
- Gate: every loadFlic of loadWorldAssets for every theme. 443 / 443 entries of menu_flics.inc's tables recorded and decoded equal:
  Parkland (sandbox_basic, cov_b_career_types), Links and Desert (cov_b_career_types), Tropical (cov_b_s34_hawaii with the Tree
  alias, U-1330). Over all recordings: 549 distinct loadWorldAssets loads (17,955 cells) and 1,698 distinct loads including
  loadInterfaceArt's boot loads (148,926 cells) equal, x64 and Win32. Equal = same column count (the count word loadFlic reads),
  rows, cell count, per cell the same cropSprite result, SpriteRef +0x08 .. +0x28, jgld Sprite words (bpp 8, bytes, pitch, width,
  height, palette 0, no row spans) and pixel bytes; the slot palette (and the shadow slot's when the load drew a shadow) equal.
- U-1701 settled (U-1330): the original dies at loadWorldAssets' first `Tree Cerc` load on every tropical course of the shipped
  install. Decision row U-1331 for the lead (the port runs on today).
- Controls (port/test/test_flic.py, `flic.load` negctl): 4 / 4 RED, none blind on the recordings; one is thin (U-1334).

| mode | wrong body | loadWorldAssets loads that differ (of 549) |
|---|---|---|
| 1 | the first word-delta chunk of each stream skipped | 298 |
| 2 | the colour chunks ignored | 549 (the slot palette) |
| 3 | a sheet row's frames cut in reverse order | 300 |
| 4 | byte 0xfe drawn (`> 0xfe` instead of `>= 0xfe`) | 1 (bldgs\links\PG_pflag; 1 of 1,698 with the boot loads) |

The loads equal under modes 1 / 3 are one-frame sheets (size -1, or one frame per sequence and one row).

## 2. What the original does (cited, written into flic.cpp)

loadFlic 0x0043d740, args (id, path, frames, size, flag), plain `ret`:
- Working image g_image5aa778 0x005aa778: 480 x 480, pitch 480, palette slot +0xe8 = 0 (recorded in every load). Win_fillRect
  (0, 0, 0x1e0, 0x1e0, 0xff) at 0x0043d7c2 before the opens.
- flag != 0: fileExists(prefix + path + "Shadow.flc") (0x0043d896) -> flicOpenStream slot 0x24e (0x0043d8b8) and Image_draw into the
  working image (0x0043d8d2); a missing shadow clears the flag (0x0043d8d9). Then the main file, slot = id + g_sheetBase (0x0043d971)
  and Image_draw (0x0043d98b). A failed open returns NULL and Image_draw reads [NULL + 8] (0x00473e67): the process dies (U-1330).
- The cut loop: rows = frames (1 for size -1, 0x0043d7a2), columns = the count word (header +0x62). Every cell after the first:
  fill (size 0: (0xd2, 0xc8, 0x3c, 0x3c) at 0x0043da56; else the whole image), flicNextFrame of the shadow (when drawn) and of the
  main slot, Image_draw of each (0x0043da7b .. 0x0043dab5). The crop (Surface_blit -> cropSprite, kind 1, trim 1): size 0 ->
  (0xd2, 0xc8, 0x3c, 0x3c) (0x0043dac3), 1 or -1 -> (0x64, 0x64, 0x118, 0xf0) (0x0043db56), 0xf0 -> (0, 0, 0x1e0, 0x1e0)
  (0x0043db3c), any other s -> (0xf0 - s, 0xf0 - s, 2s, 3s / 2) (0x0043db0c .. 0x0043db2b). size -1 stops after one cell (0x0043db87).
- flicNextFrame / Image::step / Image::seek / decodeImageChunks and the chunk decoders follow their C3 / C4 notes (cited at the top
  of flic.cpp). Points that mattered: a converted header keeps the sequence count at +0x60 and frames per sequence at +0x62;
  Flic::indexFrames walks M + 1 frame chunks per sequence (the ring frame) and the last sequence M; flicNextFrame steps inside a
  sequence and seeks to slot + 1 at its last frame (a mask bit that is clear leaves the frame as it is); the frame Surface sits at
  the header's words +0x64 / +0x66 in the working image; the 8-bit to 8-bit draw (jgld draw_1005ed80 via Sprite::drawB) copies
  bytes below 0xfe; colour chunks (4 and 11) go to the palette object of the frame-table record 0x004e9aa0 + 0x58 * slot, not to
  the cell (every cell's jgld Sprite has palette 0).
- File facts (1,893 .flc): all converted (0xf1f1f2f2), 8-bit; sequences x mask: (1, 0) 2, (1, 1) 60, (2, 3) 58, (4, 0xf) 1,173,
  (5, 0xf) 1, (8, 0xf) 1, (8, 0xff) 598; 9,667 sequence starts, every one a frame whose first sub-chunk is a byte run; sub-chunk
  types 7, 4, 15 and 9 (4 chunks in two dirt-shadow files, decoded as nothing); every colour chunk sets all 256 entries.

## 3. Recordings (port/corpus/flic/<scenario>/<label>/, local, 348 MB)

| recording | run | loads | reference |
|---|---|---|---|
| sandbox_basic/flic | `--stop-after 2` (boot + Monterey) | 506 | SAME (2 checkpoints) |
| cov_b_career_types/flic | whole script (6 careers) | 1,097 | SAME (12 checkpoints) |
| cov_b_s34_hawaii/flic | study scenario, `--record --tree-alias` | 500 | none (study) |
| cov_b_s34_hawaii/stock | study scenario, stock install | 491 + the failed open, then the game died | none |

The study scenario is log/roadmap/S3/4_scenarios.py (SCENARIO_REFS_EXTRA): menu -> Sandbox Mode -> Hawaii. Its schedule is
log/c3/alt/refs/cov_b_s34_hawaii/schedule.json (made by the alias run, replayed by the stock run). The alias copies live in the
instance only for the run (removed afterwards; s36a's manifest check logged them as changed files).

## 4. Gates (mirror port/build/S3_head = c6d7d78b, flic module added in a source copy, no menu.cpp hook-up)

- test_flic.py: 9 passed + 1 skipped (S3_4; the skip is the gfx hook-up test: HEAD has no gfx_body.h) and 10 passed (S3_4_gfx:
  the source copy plus the working tree's uncommitted S2 wave 1 gfx files gfx.cpp, gfx_body.h, gfx_families.inc,
  gfx_selftest.cpp, gfx.h).
- Hook-up, S3_4_gfx with section 2 of the merge applied to its menu.cpp copy: compiles; framegate sandbox_basic gives the same
  10 rows as without it and the boot verdict PASS (every frame's sim hashes equal), 11.2 s against 5.3 s (the decodes run).
- Segments x64 205 / 205 PASS, Win32 205 / 205 PASS, 0 differ from log/roadmap/merges/S6-1/segs_*.tsv.
- Boot gate 58 PASS / 2 KNOWN / 1 NO_FRAMES of HEAD's 61, 0 rows differ from S0 (the extra rows cov_r_bio / cov_r_flic_input FAIL
  as in BASELINE.md, U-1102).
- Port test_static 6 passed (source copy and mirror). Repo static in the mirror: 595 passed, 5 skipped, 2 failed: the same two
  mirror-only shim tests as S3.1's mirror run (they read shim/build/winmm.dll, which a git archive does not hold).

## 5. Commands

```
# recordings (repo root; game slot; one at a time)
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/flic_cells_rec.py record --scenario sandbox_basic --instance s3_4 --stop-after 2
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/flic_cells_rec.py record --scenario cov_b_career_types --instance s3_4
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/flic_cells_rec.py record --scenario cov_b_s34_hawaii --instance s3_4 \
    --record --extra log/roadmap/S3/4_scenarios.py --tree-alias
SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/flic_cells_rec.py record --scenario cov_b_s34_hawaii --instance s3_4 \
    --label stock --extra log/roadmap/S3/4_scenarios.py
# builds
~/bin/py -3.12 log/roadmap/S3/4_mirror.py --w32        # S3_4 (x64) + S3_4_w32
~/bin/py -3.12 log/roadmap/S3/4_mirror.py --gfx        # S3_4_gfx (with S2 wave 1's gfx files)
# decoder gate
cd port/build/S3_head && SG_PORTSIM_DLL=$PWD/port/build/S3_4/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_flic.py -q
SG_PORTSIM_DLL=port/build/S3_head/port/build/S3_4/Release/portsim_c.dll ~/bin/py -3.12 port/test/test_flic.py --all [--negctl N]
~/bin/py -3.12 log/roadmap/S3/4_w32_run.py --all       # the same on the Win32 runner
~/bin/py -3.12 log/roadmap/S3/4_coverage.py            # 443 / 443 table entries covered
# regression gates (mirror)
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll $SG_PORTSIM_DLL --jobs 8 --out log/roadmap/S3/4_segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend win32 --dll32 port/build/S3_4_w32/Release/portsim_c.dll \
    --runner32 port/build/S3_4_w32/Release/portsim_runner32.exe --jobs 8 --out log/roadmap/S3/4_segs_win32.tsv
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S3/4_boot_gate.tsv ; ~/bin/py -3.12 log/roadmap/S3/4_boot_compare.py
```

Logs: 4_rec_*.log, 4_build*.log, 4_pytest_*.log, 4_controls.log, 4_coverage.log, 4_w32.log, 4_segs_*.{log,tsv}, 4_boot_gate.*,
4_framegate_{hooked,unhooked}.tsv, 4_pytest_repo_static.log.

## 6. Open items

- Merge: modules.inc line + menu.cpp hook-up + UNCERTAINTIES rows (4_merge.md). The hook-up only acts once S2 wave 1's
  gfx_body.h is in the tree.
- U-1331: the tropical decision (fault vs alias fix vs run-on).
- U-1332: the palette a sheet cell is drawn with (sprite-queue step).
- U-1333 / U-1334 / U-1335: evidence limits (initial frame content unobservable, thin 0xfe evidence, unmodelled formats).
