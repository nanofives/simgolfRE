# Phase 3.8 stream 38a: text widths (U-0944, U-0947), 2026-10-20

Task (parent's message): (1) a host GDI TextMetrics provider, (2) an opt-in recorder of every Font::textWidth call, default
behaviour and trace format byte-identical, (3) the provider equal to every recorded width, (4) the 17 Text_put segments run
with the provider. All four done; numbers below. Merge lines for the parent: log/p38/38a_merge.md. Uncertainties:
log/p38/38a_uncertain.tsv (U-0960, U-0961). Files: log/p38/38a_files.txt.

## 1. The chain, from the disassembly (every row checked in capstone output or in a 100% matched source)

Text_put 0x00477250 (thiscall (text), `ret 4`): NULL -> 0 (0x00477259), else drawMarkupRun(this, text, strlen) (0x00477272).
drawMarkupRun 0x00477280 (thiscall (text, len), C4 hook c3o.cpp): a zero +0x5c is filled from g_primarySurface 0x0083ad44
(0x0047728c); +0x40 == 0 measures the whole run in +0x5c; otherwise the token pass (scanToken 0x00476dd0 while +0x4c == 1,
skipToken 0x00476d80 while the style +0x38 is 3, else scanMarkupText 0x00476d40; the switch at 0x00477315; the `$` tokens
compared with _strncmp 0x004a6ad0 against f_strpool_4e4218 + 0x24 / + 0x30 / + 0x3c; `=` spans skipped with _strchr
0x004a6170). measureTextWidth 0x00483930 (thiscall (str, maxlen) on a FontRef): [this+4] == 0 -> 0 (0x00483936), str NULL ->
0 (0x0048393e), count = maxlen when maxlen < strlen (signed, `jl` 0x00483953) else strlen, then `call [edi + 0x10]` on the jgld
Font (0x00483969). jgld Font vtable 0x1011da6c: slot 1 -> thunk 0x10001a19 -> Font::loadFile 0x10068ae0, slot 2 -> 0x10001230 ->
Font::create 0x10068850, slot 4 -> 0x10001078 -> Font::textWidth 0x10068e60 (re/match/jgld_font.cpp, 100% match).
Font::textWidth: s NULL -> 0 (0x10068e7d..0x10068e85); SelectObject(g_fontDC 0x10128724, [this+0x18]) (0x10068e97);
GetTextExtentPoint32A(dc, s, n, &sz) (0x10068eb8); SelectObject(dc, GetStockObject(SYSTEM_FONT = 0xd)) (0x10068ec9 /
0x10068ee0); returns sz.cx (0x10068eed); sz starts as the /GZ fill 0xcccccccc (0x10068e72). initFonts 0x10068f30:
g_fontDC = CreateCompatibleDC(0).
Font::create: lfHeight = -size (0x10068903..0x10068908), lfWidth / lfEscapement / lfOrientation 0 (0x1006890b..0x10068919),
lfWeight 0x2bc when style bit 0 else 0 (0x10068920..0x10068930), lfItalic bit 1 (0x10068933..0x1006893f), lfUnderline bit 2
(0x10068942..0x1006894e), lfStrikeOut 0, lfCharSet 0, lfOutPrecision 7, clip / quality / pitch 0 (0x10068951..0x10068965),
strcpy face (0x10068971), CreateFontIndirectA (0x1006897f; 0 -> 13), then GetTextMetricsA (0x100689ce) and the FontRef words
+0xc tmHeight + tmExternalLeading, +0x10 tmAscent - tmInternalLeading, +0x14 tmInternalLeading, +0x18 tmAscent, +0x1c
tmDescent (0x100689db..0x10068a1a; the Font's owner word +4 is the FontRef's handle field, FontRef + 4: FontRef::createFromId
0x00483821 / createSized 0x00483873 pass that address to Factory::createFont 0x10067850, which stores it).
Font::loadFile: the .fot name is the file name with its last 4 characters replaced by ".fot" (0x10068b88..0x10068bb2, string
0x1011da88), the path GetCurrentDirectoryA (0x10068bc8) + "\" (0x1011da84) + file; CreateScalableFontResourceA(0, fot, path,
0) (0x10068c10), an error other than 0x50 (ERROR_FILE_EXISTS) or 0 -> 1 (0x10068c36..0x10068c46); AddFontResourceA(fot)
(0x10068c74; 0 -> 1); PostMessageA(0xffff, WM_FONTCHANGE 0x1d, 0, 0) (0x10068cc1); then create(face, size, style) (0x10068cf3).

The 15 FontRefs the game creates (FontRef::createFromId 0x00483800 = Font::create; FontRef::createSized 0x00483850 =
Font::loadFile; scaleX 0x00404970 = (g_screenWidth * v + 0xa0) / 320, signed):

| FontRef | site | file | face | size | style |
|---|---|---|---|---|---|
| g_font83ad20 | setPrimarySurface 0x00483aff (engineInit 0x0048569f passes it) | - | f_strpool_4e43ac + 0 (Times New Roman) | 0xc | 0 |
| g_font51b320 | gameMain 0x0045bd8f | - | f_strpool_4d38b0 + 0x48 (Arial Bold) | 9 | 0 |
| g_fontRef_519fd8 | 0x0045bda1 | - | same | 0xa | 0 |
| g_font51b360 | 0x0045bdb9 | f_strpool_4d38b0 + 0x3c (manu3_.ttf) | [f_ptrtab_4d3888] = 0x004d38a0 (Manual SSi Bold) | 0xf | 0 |
| g_font519928 | 0x0045bdd2 | manu3_.ttf | same | 0x14 | 0 |
| g_font51a028 | 0x0045bde9 | f_strpool_4d38b0 + 0x2c (klepto__.ttf) | f_strpool_4d21a4 + 0x5a4 (Klepto ITC TT) | 0x12 | 0 |
| g_fontRef_519a40 | 0x0045be00 | klepto__.ttf | same | 0x18 | 0 |
| g_font519948 | 0x0045be19 | manu3_.ttf | Manual SSi Bold | 0x18 | 0 |
| g_font51b340 | 0x0045be35 | - | f_strpool_4d38b0 + 0x18 (Comic Sans MS Bold) | scaleX(0x28) | 2 |
| g_font519968 | 0x0045be4f | - | f_strpool_4d38b0 + 0 (Times New Roman Bold) | scaleX(ebp = 7, 0x0045bc63) | 0 |
| g_fontRef_821020 | loadInfoScreenArt 0x0044be1f | - | Klepto ITC TT | 0x18 | 0 (push 0 at 0x0044be06) |
| g_fontRef_821f08 / 821ec8 / 821f28 / 821ee8 | 0x0044be32 / 45 / 58 / 6b | - | f_strpool_4d21a4 + 0x598 (Manual SSi) | 0x14 / 0x12 / 0x10 / 0xe | 0 |

g_font519988, g_font5199a8, g_object519a20, g_surfaceRef_51a008 and g_font51a048 have a zero handle word in every segment
start image (log/p38/38a: segfonts output): never created; measureTextWidth returns 0 for them and so does the provider.
The draw context the widgets measure on is f_stateptr_4c1570[0] = 0x00519cd4 = g_rootWindow + 0x274 (its Image), in .data:
in every start image of the 17 segments +0x38 = 0, +0x3c = 0, +0x40 = 1, +0x4c = 0, +0x54 = 0, +0x60..+0x68 = 0.

## 2. The provider (module gditext: port/src/sim/gditext.cpp, port/include/portsim/sim/gditext.h)

GdiTextMetrics implements commands.h's TextMetrics: `text_put(ctx, draw_ctx, font, text)` = Text_put after the caller's
setDrawTarget(font, 0, 0, 0) (setDrawTarget 0x004762d0 stores +0x5c only when the FontRef's +4 is set, 0x004762dd, and
+0x60 / +0x64 / +0x68 always). Also the original functions over the state as it is, registered under their hooks.csv names:
`Text_put`, `drawMarkupRun`, `measureTextWidth` (thiscall), and `gditext.attach` / `attachScripted` (test rule) / `detach` /
`fontWidth` (Font::textWidth: exactly n bytes) / `fontMetrics` / `textPut` (the interface call). drawMarkupRun's writes
(+0x38, +0x3c, +0x4c, the +0x5c fill) are applied to the draw context. Fonts: a table of the 15 FontRefs (by GameState
member, no address in port code), faces and file names read from the state's pools, LOGFONTA as Font::create, one HFONT per
distinct LOGFONTA, one CreateCompatibleDC(0). Font files: bytes through the context's GameFiles (the logical name gameMain
passes), written into a private %TEMP%\sg_gditext_<pid>_<n> directory, CreateScalableFontResourceA(0, <dir>\<name>.fot,
<dir>\<file>, NULL) + AddFontResourceA, removed (RemoveFontResourceA, files deleted) when the provider is destroyed.
Deviations, none of which changes a measurement: the temp directory instead of the install (the game's .fot sits in the cwd
and names the TTF relatively), no PostMessageA broadcast of WM_FONTCHANGE, one registration per distinct file (the game
registers manu3_ three times and klepto__ twice). SAFETY CHECK: before a font is used, its GetTextMetricsA words must equal
the FontRef's +0xc..+0x1c in the state when those are set (segment images carry them), else MissingResource{"text font
(host GDI metrics differ from the state's)"} (a host that substitutes another face would otherwise measure silently wrong).
Negative controls (g_negctl_gditext): 1 width + 1, 2 weight 700 always, 3 markup switch ignored, 4 max instead of min in
measureTextWidth, 5 `}` / `]` keep the style, 6 `{` selects style 2, 7 the interface call ignores its font.

## 3. Recorder (re/tools/widths_rec.py, re/frida/js/widths_rec.js)

A trace_tool.py record with widths_rec.js appended after trace_rec.js (trace_tool.install wrapped at run time; neither
trace_tool.py nor trace_rec.js nor scenario_det.js changed). CModule Interceptor listeners on jgld Font::textWidth (RVA
0x68e60), exe Text_put 0x00477250 and drawMarkupRun 0x00477280, main thread only, three rings published at the function's
exit, drained by the JS thread every 20 ms; END (trEnd wrapped) stops and drains. Output port/corpus/widths/<scenario>.widths.tsv
/ .textput.tsv / .markup.tsv / .widths.json / raw .tw.bin .tp.bin .dm.bin / .end.json / .trace (local corpus, gitignored).
Instance instances\p38a (copy of original\ with the pinned build winmm.dll 381ed2b2 from instances\p24).

Proof: a plain `trace_tool.py record --scenario cov_a_play` on p38a (log/p38/38a/plain_record.log; the trace itself was
deleted after the comparison: game data stays in the gitignored corpus) is byte-identical to
port/corpus/traces/cov_a_play.trace and SAME (8 checkpoints) as the pinned reference; every widths run's own trace is
byte-identical to the corpus trace and SAME as the reference, with 0 drops and 0 calls on other threads:

| scenario | textWidth calls (distinct) | Text_put | drawMarkupRun | checkpoints SAME |
|---|---|---|---|---|
| cov_a_build2 | 81,538 (584) | 43,093 (425) | 55,437 (521) | 10 |
| cov_a_champ | 103,157 (268) | 39,276 (157) | 58,368 (216) | 11 |
| cov_a_keys2 | 64,363 (427) | 10,019 (235) | 27,914 (382) | 9 |
| cov_a_play | 38,955 (532) | 6,821 (399) | 15,533 (479) | 8 |
| cov_b_career | 144,044 (1,429) | 33,025 (1,155) | 69,341 (1,374) | 6 |
| cov_b_career_types | 403,576 (1,911) | 128,248 (1,743) | 215,868 (1,877) | 12 |
| cov_b_people | 75,668 (500) | 11,375 (383) | 31,486 (466) | 5 |
| cov_k1_animals | 91,761 (1,069) | 37,411 (914) | 54,944 (1,027) | 5 |
| ui_tour | 441,553 (2,903) | 198,182 (2,349) | 268,991 (2,746) | 10 |
| ui_tour_v | 259,091 (1,858) | 78,387 (1,341) | 129,288 (1,710) | 10 |
| total | 1,703,706 (11,481) | 585,837 (9,101) | 927,170 (10,798) | |

## 4. The provider against the recordings (port/test/widths_check.py; logs log/p38/38a/widths_check_{x64,w32}.log/.json)

Per scenario one context with a segment start image of that scenario, the GameState at 0x004c1000, original\ as GameFiles.
x64 (in-process DLL) and Win32 (the 32-bit runner of my build): every row with printable text equal, both backends:
textWidth 11,459 / 11,459 distinct rows, Text_put 9,079 / 9,079 (result and the four context words after), drawMarkupRun
10,776 / 10,776 (28 / 28 rows with a markup token), the TextMetrics interface call (setDrawTarget semantics, +0x5c cleared
first) 9,079 / 9,079. The other 22 rows per table are a lone 0x0a, measured by drawMessageBuffer (Text_put at 0x0045b140):
the original itself gave two results for the same call across runs (log/p38/38a/consistency.log), and the host gives 0 or
the default advance per process (log/p38/38a/newline_probe.log): equal 14 / 22 on the x64 run, 9 / 22 on the Win32 run
(U-0960). Font metrics: the provider's 15 fonts give the five words the original stored, 15 / 15 (only 9 fonts appear in the
recordings: U-0961).
drawMarkupRun token logic, EMULATOR ORACLE (test_gditext.py, SG_EMU_ORACLE=1): the original drawMarkupRun under Unicorn
(port/test/cmdemu.py) with its real measureTextWidth and the jgld font slot stubbed by the scripted rule the port's
`gditext.attachScripted` uses; 600 random cases (styles, link / dropdown states, style fonts, fonts with no handle, `$`
tokens, doubled tokens, `=...>` spans, run lengths past the NUL and NULL texts): eax and the whole data image equal in all
411 that complete, the other 189 fault in both (negative run lengths after a `$` token: the scanners read to the block end);
48 of 49 jcc sides of drawMarkupRun, measureTextWidth and the three scanners; the missing one (0x00477307 taken, the switch's
out-of-table arm) cannot run because every scanner stops only on a table character. Golden digests (300 cases) pinned in
port/test/golden_gditext.json after the oracle was GREEN on them.
Negative controls: 1, 2, 7 RED on the recordings (test_negctl_red_on_recordings); 3, 4 RED on the recordings and the golden
digests; 5, 6 RED on the golden digests, blind on the recordings (every recorded style font is none, so a wrong style choice
measures in the same font). log/p38/38a/negctl.log, negctl_markup.log, test_gditext.log (22 passed, 1 opt-in skipped; the
opt-in oracle passed in its own run).

## 5. Segments (log/p38/38a/segs.py: p37a_cmdsegs.py's runner with SegmentRunner.load_check wrapped to attach the provider)

The 17 that stopped on Text_put (log/p38/38a/segs.tsv; Win32 runner: identical status, frames and detail, segs_w32.tsv):

| segment | now | next stop |
|---|---|---|
| cov_a_build2/02 | PASS 43/43 | (end checkpoint inside showPopup's modal wait) |
| cov_a_build2/04 | PASS 123/123 | |
| cov_a_champ/03 | PASS 36/36 | |
| cov_a_play/02 | PASS 78/78 | |
| cov_b_career/03 | PASS 612/612 | |
| cov_b_people/00 | PASS 28/28 | |
| ui_tour/04, ui_tour_v/04 | PASS 157/157 | |
| cov_a_keys2/03 | FAIL 1/43 | missing: saveCourse (showPopup's choice calls it) |
| ui_tour/01, ui_tour_v/01 | FAIL 182/233, 182/234 | missing: saveGameDialog |
| ui_tour/05, ui_tour_v/05 | FAIL 6/135 | missing: promptAndSaveFile (showSystemMenu, System popup option 3) |
| cov_b_career_types/00, /01, /02 | FAIL 18/67, 19/68, 18/67 | the frame left to the title menu |
| cov_k1_animals/00 | FAIL 18/57 | the frame left to the title menu |

The four title-menu stops are the original's path, measured with a debug build (log/p38/38a/dbg_patch.py instruments a COPY
of the mirror, port/build/m38a_dbg_root; port/src untouched): frame 18's body dispatch is the menu-bar click (132, 580)
-> showSystemMenu -> the System popup (x 250, y 340, 8 options, widest 279 in port and original alike: the original's
recorded Text_put rows at 0x0046d766 are the same 8 lines); the cursor reaches (200, 535) = option 7 "Exit the Game" and
the click there returns 7; the next popup (x 400, y 100, 2 captions + 3 options) gets the click (330, 212) = option 2
"So long for now"; the frame step then returns FrameExit::Menu (tick_frame.inc ML+0xf106 -> ML+0x11875). The original
does the same: the strings it measured next are the title menu's ("Continue Saved Game", "Start New Game", "(Standard)",
...) and then a new career's location screen. sg_run_frames does not run mainLoop's title-menu section, so these
segments end there; the first 18 frame records match.
All 155 COMMAND segments, same DLL: 109 PASS without the provider (= 3.7a's count), 117 PASS with it, none worse
(log/p38/38a/segs_all_base.tsv, segs_all_gdi.tsv). The proposed SegmentRunner hook (log/p38/38a/segments_py.patch) gives
the same results by default; `text_metrics="none"` restores the Text_put stops.

## 6. Builds and how to continue

`~/bin/py -3.12 log/p38/38a/mirror.py` (HEAD + gditext.{cpp,h}, modules.inc + SG_MODULE(gditext) into port/build/m38a_root),
then the VS cmake (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\
CMake\CMake\bin\cmake.exe`): `-S port/build/m38a_root/port -B port/build/38a -A x64` and `-B port/build/38a_w32 -A Win32`
(targets portsim_c, portsim_runner32). Tests: `SG_PORTSIM_DLL=port/build/38a/Release/portsim_c.dll ~/bin/py -3.12 -m pytest
port/test/test_gditext.py` (SG_EMU_ORACLE=1 for the oracle, =update to re-pin the digests); widths check: `~/bin/py -3.12
-m port.test.widths_check [--win32 port/build/38a_w32] [--negctl N]`; segments: `log/p38/38a/segs.py [--all] [--win32 dir]
[--no-attach]`; record more scenarios: `SIMGOLF_ALLOW_MULTI=1 ~/bin/py -3.12 re/tools/widths_rec.py record --scenario <s>`.
Open: U-0960 (lone control characters), U-0961 (six fonts with no recorded width), portable widths without GDI (phase 4, plan
section 2.3, not attempted). The regression run of test_commands / test_tick with my DLL hit MemoryError while a sibling's
pytest held 2.2 GB (machine commit nearly exhausted; result in section 7). re/tools/game.py's purge_frida_temp needs psutil,
which ~/bin/py's Python lacks, so it could not be called by hand after the runs.

## 7. Regression of existing tests with my DLL

- Segments with my DLL and the provider attached: TIME-FORWARD 35 / 35 PASS (log/p38/38a/segs_tf_gdi.tsv), COMMAND 117 / 155
  (109 / 155 with the same DLL and no provider, equal to 3.7a's count: registering Text_put / drawMarkupRun /
  measureTextWidth changes nothing while no provider is attached, and tick keeps its own `Text_put` drop for the frame's
  drawing calls because drops resolve before the registry).
- test_gditext.py: 22 passed, 1 skipped (the opt-in oracle, which passed on its own with SG_EMU_ORACLE=1).
- test_static.py: 5 passed; test_every_module_file_is_listed fails in the working tree until modules.inc lists gditext
  (and the siblings' modules), as expected.
- test_commands.py / test_tick.py / test_segments.py could NOT be judged: from 01:51 on another session's full pytest
  (PID 34832, `pytest port/test/test_bridge.py ... test_dialogs.py`, not mine) held 10.9 GB of commit and the machine had
  2.6-4 GB free, so every golden-digest and segment-load test raised MemoryError within seconds, with my DLL and with the
  parent's port/build/x64 DLL alike (log/p38/38a/regress_cmd_tick.log, regress_tick_segments.log). To re-run once memory is
  free: `SG_PORTSIM_DLL=port/build/38a/Release/portsim_c.dll ~/bin/py -3.12 -m pytest port/test/test_commands.py
  port/test/test_tick.py port/test/test_segments.py -m "not slow"`.
