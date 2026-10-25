# S2.1 notes: gfx plumbing, wave 1 (body families, S0 recommendations 1, 3, 4)

Status: DONE. Built and judged in a private mirror, `port/build/S2_1_head` (git archive of HEAD c6d7d78b, HEAD_COMMIT inside, with
junctions original/, log/, port/corpus/); the owned files were then copied into the working tree (they were at HEAD there, so
nothing of a sibling's was overwritten). Builds: x64 `port/build/S2_1` (log/roadmap/S2/build.py, cmake from the VS Build Tools
path on PATH), Win32 inside the mirror (`port/build/S2_1_head/port/build/Win32`, port/test/build.py --arch Win32, runner32 too).

## What changed

- port/src/sim/gfx_body.h (new): Image8 / Sprite8 / Surface16 / Gfx moved out of gfx.cpp, the shared helpers under `gfxb::`
  (vstring, put_file, surface_bytes, load_image, crop_sprite, draw16, copy8, draw_g16, shade_lut, draw_text_string, draw_text,
  draw_frame_r3, draw_message_buffer, find_body), and the family table types GfxBodyDef / GfxFamily.
- port/src/sim/gfx_families.inc (new): one `SG_GFX_FAMILY(<f>)` line per family file gfx_<f>.cpp. gfx.cpp expands it three
  times: extern declarations, the kFamilies[] table, and one registry entry `gfx.family.<f>` per family carrying the family's
  negctl switch. Explicit references, not self-registration: portsim is a static library, an unreferenced object is dropped.
- port/src/sim/gfx.cpp: the core table kBodies unchanged in content; dispatch goes through a BodyIndex (name -> body, owner; built
  once) that also records names registered twice; new test entries `gfx.call(name_va, a0..a10)` and `gfx.check()`
  (`$gfx/bodies.txt`, returns the duplicate count). S0 rec 3: `Gfx::unsup` appends every distinct reason to `notes` (the 64 cap
  is gone) and gfx.log writes `first\t<k>\t<reason>` in first-occurrence order (framegate.py's parse_gfx_log already reads it).
  S0 rec 1: kNotDrawing gains playSound, Snd::status, capsLockOff, emptyFunc_4a0890, saveCourseJpg, Widget_readPixel,
  enterModal483d30 (each one's kDrops reason in tick.cpp says it draws nothing; framegate.py NOT_DRAWING holds the same 9 names).
- port/src/sim/gfx_selftest.cpp (new, TEST-ONLY family `selftest`): gfx_selftest.fillRoot (fill a root rectangle),
  gfx_selftest.loadCopy (loadImage through the shared helper + copy8), not-drawing gfx_selftest.quiet; negctl 1 / 2.
- port/test/gfx.py: S0 rec 4 as `record_budget(scenario, last_ordinal, max_records)`: FRAME records at or before the last
  checkpoint's ordinal + 2 (was + 1), and to_end when the budget exceeds the trace's FRAME records (or the capture has none).
  framegate.py computes the same at its line 417.
- port/test/test_gfx.py: 8 new tests (family registered and listed with owners, fill dispatched, loadCopy over a synthetic PCX,
  negctl 1 / 2 RED, not-drawing names never unsupported, 80 reasons in first-occurrence order with no cap, record_budget).

## Gate (all numbers from this step's runs; logs in log/roadmap/S2/1_*)

| gate | result | log |
|---|---|---|
| test_gfx x64 | 25 passed (17 at HEAD + 8 new) | 1_test_gfx_x64.log |
| test_gfx Win32 (SG_BACKEND=win32) | 25 passed | 1_test_gfx_w32.log |
| family removal control | 6 of the 8 new tests RED with `SG_GFX_FAMILY(selftest)` removed (the other 2, log order and record_budget, do not depend on it) | 1_removal_control.log, build 1_build_nofam.log |
| framegate sandbox_basic + cov_b_location + cov_r_quit | 30 rows: status and total_px equal to frame_census.tsv in every row (11 PASS, 19 UNSUPPORTED); first_unsupported is now the first drawing call (Widget_writePixel, was Snd::status), unsup_before drops by the 4 not-drawing names, unsup_order `gfx` (was `final` / `partial`) | 1_framegate_x64.tsv |
| segments x64 | 205 / 205 PASS, compare with log/p5/segs_x64_loc.tsv: 0 differ | 1_segs_x64*.log |
| segments Win32 | 205 / 205 PASS, compare with log/p5/segs_win32_loc.tsv: 0 differ | 1_segs_win32*.log |
| boot | HEAD's 61 scenarios 58 PASS / 2 KNOWN / 1 NO_FRAMES; all 65 baseline rows equal status to S0's 1_boot_gate.tsv; cov_r_tutorial records and expected both 1026 -> 1195 (its corpus recording changed, not in HEAD's refs, U-1102); 2 newer corpus scenarios extra (cov_r_flic_input FAIL, cov_r_s8_play PASS, not HEAD) | 1_boot_gate.tsv, 1_boot_compare.log |
| existing gfx negctl | 1, 2, 4, 5, 6 and tick.frame 7 RED (test_gfx); 3 blind, as at HEAD, and blind on all 11 framegate PASS rows (U-1200) | 1_framegate_negctl3.tsv |
| port test_static | 5 / 6 with the tree's test_static.py; 6 / 6 with the patch in 1_merge.md item 1 | - |

Controls: 8 RED / 9 (gfx 1, 2, 4, 5, 6, tick.frame 7, selftest 1, 2), 1 blind (gfx 3, U-1200); plus the family-removal build RED.

## Reproduce

```
cd port/build/S2_1_head      # or the repo root once merged (drop SG_PORTSIM_DLL to use port/build/x64)
env PATH="<VS BuildTools>/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin:$PATH" ~/bin/py -3.12 log/roadmap/S2/build.py port/build/S2_1_head <abs>/port/build/S2_1
env SG_PORTSIM_DLL=<abs>/port/build/S2_1/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_gfx.py -v
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx.py -v
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.framegate sandbox_basic cov_b_location cov_r_quit --jobs 2 --out log/roadmap/S2/1_framegate_x64.tsv
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --dll <dll> --jobs 4 --out log/roadmap/S2/1_segs_x64.tsv
~/bin/py -3.12 -m port.test.integrator compare log/p5/segs_x64_loc.tsv log/roadmap/S2/1_segs_x64.tsv      (same for win32)
env SG_PORTSIM_DLL=... ~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out log/roadmap/S2/1_boot_gate.tsv
~/bin/py -3.12 log/roadmap/S2/1_boot_compare.py
```

log/roadmap/S2/build.py calls a bare `cmake`, which is not on ~/bin/py's PATH here: prefix PATH as above (or the helper could use
port/test/build.py's cmake_exe()). Its build-dir argument is resolved against the source root when relative, so pass an absolute
path to keep the build out of the mirror.

## Open

- Merge: 1_merge.md item 1 (test_static.py) must land with gfx_selftest.cpp; item 2 (README section) is documentation.
- U-1200: gfx negctl 3 stays blind until a gated checkpoint draws a sprite holding index 0xfe.
- The mirror port/build/S2_1_head, the removal-control copy port/build/S2_1_nofam_src and the builds port/build/S2_1, S2_1_nofam
  can be deleted after acceptance.
