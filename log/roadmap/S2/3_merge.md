# S2.3 merge notes (credits screen and the surface / image primitives)

Base: HEAD c6d7d78b + the S2.1 wave-1 files (log/roadmap/S2/1_files.txt) + log/roadmap/S2/1_test_static.patch. Everything was
built and judged in the mirror `port/build/S2_3_head` (git archive of HEAD with junctions to log/, original/, port/corpus/ and the
other gitignored data). Nothing of mine is in the working tree yet: the three new family files need the gfx_body.h change below
(they use Gfx::state_of), and a gfx_<f>.cpp that is not listed in gfx_families.inc fails test_static's module listing, so copying
them alone would break the shared build.

## 1. New files (copy from the mirror, `port/build/S2_3_head/<path>` -> `<path>`)

- port/src/sim/gfx_surface.cpp     family `surface` (9 bodies)
- port/src/sim/gfx_prim_text.cpp   family `prim_text` (2 bodies; also defines gfxp::draw_text_run, used by gfx_credits.cpp)
- port/src/sim/gfx_credits.cpp     family `credits` (4 bodies)
- port/test/test_gfx_credits.py    47 tests

## 2. Shared-file edits: log/roadmap/S2/3_shared.patch

One patch, `git apply log/roadmap/S2/3_shared.patch` from the repo root. Checked with `git apply --check` against the working tree of
2026-10-24 (siblings' uncommitted commands.cpp / tick.cpp included): every hunk applies, tick.cpp's at offset 52.

| file | base | change |
|---|---|---|
| port/src/sim/gfx_body.h | S2.1 | `#include <memory>`; `Gfx::family_state` (map name -> shared_ptr<void>) and `template <class T> T &state_of(const char *family)`: per-family state that lives and dies with the Gfx |
| port/src/sim/gfx.cpp | S2.1 | load_image: flag bit 0 (the Image's own palette cache) decodes like flag bit 1 with the file's palette (a palette argument with it returns 3, LoadImage_re); decode_pcx8's palette-argument test is `(flags & 3) != 0 \|\| pal != 0` (was `& 2`). The unsupported reason `loadImage: flag bit 0 (the image's palette cache)` is gone |
| port/src/sim/gfx_families.inc | S2.1 | three lines `SG_GFX_FAMILY(surface)`, `SG_GFX_FAMILY(prim_text)`, `SG_GFX_FAMILY(credits)` after selftest |
| port/src/sim/commands.cpp | HEAD | `#include "portsim/sim/gfx.h"` after course.h |
| port/src/sim/commands_screens.inc | HEAD | showCreditsScreen's drawing calls (credits_gx / credits_teardown helpers, local keys gfx_local_key(0x3000 / 0x3100 / 0x3200)): objects, createFull, the two loads, blitTakeDirty, showCreditsScreen.logo, Image_releaseAll, showCreditsScreen.lines, setDrawTarget, Widget_setQuad, per pass Image_draw + showCreditsScreen.line per line + Window::refreshFull, then showCreditsScreen.free and the teardown on every return path; no GameState write |
| port/src/sim/tick.cpp | HEAD | TraceInput::clock: a served clock read calls `gfx_progress_fast(ctx_, clock_pos[i] + 1)` unless tick.frame negctl 11 is set (mode 11 is new; the working tree already uses 8 elsewhere, 9 and 10 are free) |

After applying: touch nothing else (modules.inc, CMakeLists.txt, gfx.h, test_static.py unchanged; the glob picks the new .cpp files).

## 3. UNCERTAINTIES.md

Append the 7 rows of log/roadmap/S2/3_uncertain.tsv (U-1220..U-1226) after the S2.1 / S2.2 rows.

## 4. CLAUDE.md paragraph (after the S2 wave-1 paragraph)

```
Roadmap S2.3, the credits screen (2026-10-24, notes log/roadmap/S2/3_notes.md, U-1220..U-1226): cov_r_quit's five credits
checkpoints (quit_00..03, credits: the scroll at five positions) equal the original's back buffer in every pixel, x64 and Win32.
Three body families: `surface` (gfx_surface.cpp: Image_ctor / dtor / releaseAll, Surface_ctor / free / createFull erase the key's
backend entries; Surface_blitTakeDirty = Surface_blit; Surface_drawRegion = jgld Sprite::draw -> draw16_1001cde0, which draws
0xfe and skips only 0xff, at the trims; Surface_blitThunk = Surface_blit3 with one shared size), `prim_text` (gfx_prim_text.cpp:
Widget_setQuad's colour words per target in the new per-family Gfx::state_of, drawText's min(strlen, limit) characters in the
main colour, no shadow) and `credits` (gfx_credits.cpp: the logo at its own size, the kept lines read by the backend, one line per
call, the logo centred by its kept width). loadImage's flag bit 0 decodes with the file's palette. The port's show_credits_screen
makes the original's drawing calls (credits_gx, local keys). TraceInput::clock advances the gfx snapshots past a served clock read
(ordinal + 1): the credits scroll reads the clock once per pass and dispatches nothing, so before it every snapshot came from the
end of the input. Gates: census 609 rows, 604 unchanged, the 5 credits rows UNSUPPORTED -> PASS (PASS 45 -> 50); segments
205 / 205 x64 + Win32, 0 digests changed; boot equal to S2.1 in all 67 rows (HEAD's 61: 58 / 2 / 1); test_gfx_credits 47 x64 and
Win32, test_gfx 25, port test_static 6, test_commands 83 / 44 skipped, test_tick 250. Controls: 4 RED on the frames (prim_text 1,
2, credits 2, tick.frame 11), every other mode RED in a unit test (equivalent on the five frames, U-1223); gfx.attach 3 stays blind
(the background has no byte 0xfe, U-1200).
```

## 5. Gate commands after the merge (repo root, port/build/x64)

```
~/bin/py -3.12 -m pytest port/test/test_gfx_credits.py -q -p no:cacheprovider                    # 47 passed
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx_credits.py -q -p no:cacheprovider   # 47 passed
~/bin/py -3.12 -m pytest port/test/test_gfx.py port/test/test_static.py -q -p no:cacheprovider     # 31 passed
~/bin/py -3.12 -m port.test.framegate cov_r_quit --checkpoints quit_00,quit_01,quit_02,quit_03,credits --out <tsv>   # PASS 5
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out <tsv>                # 205 / 205, 0 differ (same win32)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>                                       # as S2.1
```
