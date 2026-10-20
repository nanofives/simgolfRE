# 38a merge notes (lines for the parent; no shared file was edited)

Stream 38a: text widths (U-0944, U-0947). New module `gditext` (host GDI TextMetrics provider), an opt-in widths recorder,
the recorded widths of 10 scenarios and the check, the 17 Text_put segments. Notes: log/p38/38a_notes.md.

## 1. port/src/sim/modules.inc

Append one line (anywhere: no other module registers `Text_put`, `drawMarkupRun`, `measureTextWidth` or `gditext.*`):

```
SG_MODULE(gditext)
```

`gditext.cpp` links gdi32 through `#pragma comment(lib, "gdi32.lib")`, so port/CMakeLists.txt needs no change (MSVC's
default libraries include gdi32 as well). Off Windows the module compiles; every measurement throws
MissingResource{"host GDI"}.

## 2. port/test/segments.py: attach the provider in every segment run (patch)

`git apply log/p38/38a/segments_py.patch` (made from the working-tree file AFTER 38b's segment_files overlay;
`git apply --check` passes on it; regenerate with `~/bin/py -3.12 log/p38/38a/make_segments_patch.py <scratch dir>` if
segments.py changes again). What it does:

- `SegmentRunner(lib, text_metrics=None)`: `"gdi"` (default, or `SG_TEXT_METRICS` from the environment) attaches the host
  GDI provider with `gditext.attach` after the game files are set up, when the library registers it; `"none"` runs
  without one (the old behaviour, the Text_put stops come back).
- The provider is detached in a `finally` before the context is freed: the provider maps (commands.cpp `metrics()`,
  gditext.cpp `owned()`) are keyed by the context's address, which a later context can reuse. To make the `finally`
  possible, the run's body after the files setup moves unchanged into `_run_loaded(seg, st, res)`.

Checked with the patched file loaded in place of segments.py (my DLL): cov_a_play/02 PASS with the default, the same
segment with `text_metrics="none"` stops on `missing: Text_put` as before, cov_k1_animals/00 stops at the title menu,
course_season/03 PASS.

## 3. port/test/test_tick.py (OPEN_MISSING and the title-menu stops)

With the provider attached no COMMAND segment stops on `Text_put` any more (all 155 run with my DLL: log/p38/38a/segs_all_gdi.tsv
against log/p38/38a/segs_all_base.tsv, same DLL without the provider: 109 -> 117 PASS, no segment worse). Proposed:

- `"Text_put"`: keep the entry but change its reason to
  `"only without a TextMetrics provider (SegmentRunner(text_metrics='none') / SG_TEXT_METRICS=none): the host GDI provider (module gditext, 38a) gives the widths"`.
- add (names the 17 now stop on, measured on HEAD + gditext; 38b's saveload module may register some of them):
  ```
  "saveCourse": "not registered (cov_a_keys2/03: showPopup's choice calls it; 38a)",
  "saveGameDialog": "not registered (ui_tour/01, ui_tour_v/01 at 182/233, 182/234; 38a)",
  "promptAndSaveFile": "not registered (showSystemMenu's System popup option 3, ui_tour/05, ui_tour_v/05 at 6/135; 38a)",
  ```
- the four segments that now stop with run_detail `the frame left to the title menu` (cov_b_career_types/00, /01, /02 at
  18/67, 19/68, 18/67 and cov_k1_animals/00 at 18/57) are FAIL with `record count N != expected M`, so
  `_command_verdict` rejects them. Their cause, measured: in frame 18 the System popup's click (200, 535) selects option 7
  "Exit the Game" and the next popup's click (330, 212) selects option 2 "So long for now" in both the original and the
  port (debug build, log/p38/38a_notes.md section 5), and the next strings the original measured are the title menu's
  ("Continue Saved Game", "Start New Game", ...). The port's frame step returns FrameExit::Menu there (tick_frame.inc
  ML+0xf106 -> ML+0x11875), which sg_run_frames does not run. Proposed entry:
  ```
  OPEN_DIVERGENT = {s: "the original leaves the frame step for mainLoop's title-menu section (Exit the Game, So long for now; "
                       "log/p38/38a_notes.md section 5); the rest of the segment is the title menu and a new game, outside sg_run_frames"
                    for s in ("cov_b_career_types/00", "cov_b_career_types/01", "cov_b_career_types/02", "cov_k1_animals/00")}
  ```
  (or a run_detail test `r.run_detail.startswith("the frame left to the title menu")`, if the parent prefers a class
  rule; the ledger of 38f wants the cause either way).

## 4. UNCERTAINTIES.md

Rows in log/p38/38a_uncertain.tsv (U-0960, U-0961). Status changes of existing rows:

- U-0944 `decided (interface); provider open` -> `resolved`: the host GDI provider exists and equals the original on every
  recorded printable width (counts in the notes).
- U-0947 (segment runs need a host provider) -> `resolved` by the same, with the segments.py hook of section 2.

## 5. CLAUDE.md (one paragraph, after "Phase 3.7a input", for the parent to place)

```
Phase 3.8 stream 38a, text widths (2026-10-20, notes log/p38/38a_notes.md, U-0960..U-0961): module gditext
(port/src/sim/gditext.cpp, portsim/sim/gditext.h) is the host GDI TextMetrics provider: the 15 FontRefs the game creates at
startup (gameMain, loadInfoScreenArt, setPrimarySurface) rebuilt with Font::create's LOGFONTA, the game's own font files
read through GameFiles and registered as Font::loadFile does (CreateScalableFontResourceA + AddFontResourceA, in a private
temp dir), measured with GetTextExtentPoint32A on its own memory DC; drawMarkupRun's token pass, measureTextWidth and
Text_put ported over it (registered under their hooks.csv names, plus gditext.attach / detach / fontWidth / textPut).
A provider whose font metrics differ from the FontRef words Font::create stored in the state refuses (MissingResource).
re/tools/widths_rec.py records every main-thread Font::textWidth / Text_put / drawMarkupRun call of a pinned scenario
(re/frida/js/widths_rec.js, CModule listeners, appended after trace_rec.js; a plain trace_tool record is unchanged and the
widths run's own trace is byte-identical to the corpus trace) into port/corpus/widths/; port/test/widths_check.py judges
the provider on them. 10 scenarios, 1,703,706 textWidth calls (11,481 distinct): every printable row equal on x64 and
Win32; only a lone 0x0a varies, in the original between runs (0 or the default advance) and in the host as well (U-0960).
drawMarkupRun's token logic is judged by an emulator oracle (original under Unicorn, scripted font slot; 600 cases, 48/49
jcc sides) and golden digests. COMMAND segments with the provider (SegmentRunner hook): 109 -> 117 PASS; the 17 that
stopped on Text_put: 8 PASS, 5 stop on saveCourse / saveGameDialog / promptAndSaveFile, 4 leave the frame step for
mainLoop's title menu exactly where the original does (Exit the Game).
```
