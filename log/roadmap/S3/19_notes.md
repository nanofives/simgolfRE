# S3.19 notes: the bio EditBox in the graphics backend

Step row S3.19 of re/roadmap/S3_steps.tsv (wave 7). Base: wave 6 + S3.17 + S3.18 (`18_assemble.py --dst S3_19`). Mirror:
port/build/S3_19, x64 build in it (port/test/build.py, SG_FREETYPE_ROOT); pristine copy port/build/S3_19_base (19_base/ holds the
four files edited). Env for every command: `cd port/build/S3_19; TREE=S3_19 source ../../../log/roadmap/S3/18_env.sh`. Heavy runs
went through `bash log/roadmap/S3/19_heavy.sh <what> <log> <command...>` (takes and releases the wave 7 token). Delivery:
19_edits.py (19_make_edits.py), 19_new/, 19_assemble.py, 19_merge.md, 19_uncertain.tsv (U-1846..U-1849).

## 1. The GameState word [+0x57c] (reported to the lead before building)

EditBox::drawContents 0x004864f0 stores the first visible line [EditBox + 0x57c] at 0x0048662e (lowered to the caret's line) and
0x0048663d (raised to caret line - n + 1), both tested against the value read at 0x00486620; n = (rect height + Widget_value -
Widget_get) / Widget_value (idiv 0x0048661e). Its only reader is EditBox::pixelToCharPos 0x004863e0 (0x0048648d), reached from
EditBox::onKey's Up / Down (0x0048688b) and the caret-drag clicks; the port has none of them (U-1190). For the bio it is
g_characterEditorEditBox + 0x57c, class `ui` in gamestate_meta.inc: no segment holds cov_r_bio and the boot gate hashes the 203 sim
objects only. In cov_r_bio the original never changes it (bio 84..99 characters, summary.json bio0..bio2; about 3 lines against 8
visible with g_font519928's metrics +8 = -1, +0xc = 0x19, +0x10 = 0xe and the inner height 192), so both stores are skipped and the
word stays EditBox::resetFields' 0. Decision (proposed to the lead before any code was written, approved by the S3 lead): no
GameState write changes; chareditor.cpp gets no [+0x57c] store (no reader in the port, no run that changes it); the backend keeps
the same rule in its own window state, read once from the word at EditBox.window; the port-side store belongs with Up / Down and
the EditBox clicks (U-1190, U-1846).

## 2. What the original does (measured from the instructions and the C3 / C4 hook sources)

- The window: EditBox::create (c3ep.cpp) builds it with Window::create(x, y, w, h, 0, flags 0x2020 or 0x102020, parent root).
  layoutViewTree (c3ef.cpp): no bit of 0x1d, so +0xa0 bit 1 is cleared (no edges) and the rect is (x, y, x + w, y + h);
  Window::innerSize (c3k.cpp) changes nothing for +0x9c = 0x2020 and no owner. So the surface is 0x12c x 0xc8 at (0x1e0, 0x70).
- drawContents is the EditBox vtable's slot +0x58 (pointer_callers.py: slot 22 of 0x004ba4a8). Callers: Window::show (0x0047b70b,
  bit 0 of its argument clear: the bio's show(0)) and Window::doLayout (0x00480d9b), which the EditBox's slot +0x120 is; insertChar,
  onKey and setCaretActive end in slot +0x120 (c3de.cpp, c3fz.cpp, c3ct.cpp). So it draws at the show and after every edit.
- Its callees: the handle's getBounds (+0xd0) and callBounds (+0xc4 = setClip(bounds), c3ce.cpp); Window::paint(+0x584) =
  Widget_clearArea (no flag 0x80000) = fillRectDispatch(NULL, colour); [+0x598] Widget_drawBevelBoxR passes; inset; Widget_fillRectR
  = the handle's setClip(rect) (slot +0x34); wrapToLines; lineIndexAt; Widget_value / Widget_get; drawTextLines(lines[first], rect,
  0, 0); with +0x5a8 Widget_drawVLine(inset + border + drawMarkupRun(line, text + caret - line), Widget_value (ci - first) +
  inset + border - (value - get) / 2, that + Widget_value, [+0x2e0]).
- Composition: redrawDrawList walks the draw list and Window::blitToTarget (c3fm.cpp) blits each window's whole surface onto the
  display's back image at its position (Surface_blitThunk; the DC path needs +0x9c bit 0x1000000, not set).
- Colours (U-1847): [+0x584] = Palette::nearest(g_engine83ad0c's palette, 255, 255, 255) (0x00439e29..0x00439e3f); style 0's colour
  = g_editBoxDefaults' third word (EditBox::create's Widget_setQuad, 0x00486195) = 0 in the cov_r_bio captures; jgld
  Palette::setDefault's table has 00 00 00 at entry 0 and ff ff ff last. drawTextRun's run palette ([g_engine83ad18 + 4]) is 0 in the
  capture.
- The port's state (U-1848): tick_boot.inc never runs the EditBox's static constructor, so the port's Image at +0x274 is all zero
  (the first cov_r_bio boot reported `an Image with markup off`); the original holds Image_ctor's words (c3dl.cpp ImageCtor_re).

## 3. The port

- gfx_editbox.cpp (family `editbox`): EditBox.window(eb, x, y, w, h), EditBox::drawContents(eb, paint), EditBox.close(eb); one body
  of drawContents over a Callees interface: Real draws on the window surface (jgld fill / hline16 / vline16 rules, setClip, the
  text through gfx_text.cpp), Rec logs the calls with the oracle's scripted answers (test-only editbox.record / editbox.script).
  `image_word` reads an unconstructed Image (vtable word 0) as Image_ctor / EditBox::create leave it. `gfxe::compose` copies every
  shown window over the back buffer at Window::refreshFull.
- gfx_text.cpp: a window target (`set_text_target`: surface, clip, the Image's fonts, its text object) for the run drawer,
  drawRichText and drawMarkupRun; glyphs drawn through a scratch copy so only the clip rectangle changes; `load_text_obj` from the
  Image's words; `measure_markup_run` = measureMarkupRun (c3df.cpp) with wrapTextToWidth (c3z.cpp) and the break tail (TidyA/B/C,
  reverseFindChar c3r.cpp); index 0 black on a target only (the root path is unchanged).
- gfx.cpp: `gfxe::compose(g, ctx)` after `g.back.px = g.root.px` in Window::refreshFull.
- chareditor.cpp: bio_entry issues EditBox.window, drawContents(eb, white 0x80007fff) and a refreshFull before the first input, then
  per pass drawContents when a command reached the EditBox (handlers 12 / 13) and refreshFull, and EditBox.close after
  releaseState; the old `showCharacterEditor.bio` drop is gone. Nothing writes the GameState (gx only, with a backend attached).
- Finding in the original (the call oracle found it): drawContents' line table is 32 entries at the top of its frame and
  wrapToLines stores the text's end after the last line, so with 32 lines that store overwrites drawContents' return address and
  it returns into the text. The backend reports 32 lines or more; record mode logs `fault return`. The bio cannot reach it.

## 4. Results

- Emulator CALL oracle (`SG_EMU_ORACLE=1 pytest port/test/test_gfx_s3_19.py -k original`): the ORIGINAL drawContents under Unicorn
  (cmdemu.py) with recorders for getBounds / callBounds (a fake handle), Window::paint, Widget_drawBevelBoxR, Widget_fillRectR,
  wrapToLines (scripted table, 1..32 lines), drawTextLines, drawMarkupRun (width = sum of base + (c & mask)), Widget_drawVLine, with
  lineIndexAt / Widget_value / Widget_get real: 300 / 300 cases equal (calls, arguments, final [+0x57c]); the cases reach bevel
  passes and none, caret on and off, and the first line on both store arms: lowered to a caret line above it (0x0048662e,
127 cases), raised for a caret line below first + n - 1 (0x0048663d, 45 cases), kept (128); the test asserts all three. Before the 32-line rule, 8 of 300 differed only in that return.
- Default suite, x64 (19_test_x64.log; the oracle re-run with the arm assertion: 19_test_oracle_arms.log, 6 passed): test_gfx_s3_19.py 15 passed + 6 oracle skipped; with SG_EMU_ORACLE=1 21 passed; port
  test_static.py 6 passed; gfx.check 0 duplicates (test_bodies_registered_without_duplicates).
- Composed frame (constructed bio state on a mood5_fixture segment start image, the host GDI provider, original/): outside the
  box = the root, the 4-px margin white, text in the inset rectangle, a caret column of 21+ black rows, the GameState unchanged,
  EditBox.close removes the box; the caret moves with the caret position.
- Boot: cov_r_bio with the backend attached PASS (687 records); EditBox.window 2, EditBox.close 2, drawContents >= 2; no unsupported
  reason mentions the bio or the EditBox.
- framegate cov_r_bio (`--no-probe`, 19_fg/new_cov_r_bio.tsv) against S3.18's 18_fg: exit UNSUPPORTED -> FAIL 107,138 px (2,595 /
  104,543), end UNSUPPORTED -> FAIL 130 px (129 / 1), exactly S3.18's px (HUD and view residue: S3.15 / S3.16); loaded / panel /
  editor / bio1_done / bio2_done unchanged (bio1_done / bio2_done still 0 px with the extra refreshes).
- Not run (heavy, held while the wave 7 rule is in force): segments / boot gates, Win32, the census. Segment and boot runs attach no
  backend, and the only new code they execute is `drawing = false` plus the `edited` flag in bio_entry, so their digests cannot
  change; the one boot that reaches the bio (cov_r_bio) was run with the backend and PASSes.

## 5. Controls (gfx.family.editbox)

| mode | what | where | result |
|---|---|---|---|
| 1 | the caret one pixel right | oracle, golden digest | RED |
| 2 | the first line not raised below the window | oracle, golden digest | RED |
| 3 | the inset left out of the text rectangle | oracle, golden digest | RED |
| 4 | the visible-line count without value - get | oracle (120 cases; first differs at case 80), golden digest | RED |
| 5 | the window composed one pixel lower | composed frame | RED |
| 6 | Window::paint does not clear the surface | composed frame | RED |
| 7 | EditBox.close leaves the window composed | frame after close | RED |
| 8 | the bevel's light / dark swapped | oracle, golden digest | RED |
| 9 | wrapToLines' width 8 pixels wider | composed frame of a long text | RED |

9 / 9 RED, 0 blind. All are blind on the pinned frames by construction (no checkpoint shows the box, U-1849).

## 6. Reproduce

```
~/bin/py -3.12 log/roadmap/S3/19_assemble.py --dst S3_19v          # the tree (or --root <tree> --only on a tree holding S3.18)
cd port/build/S3_19v && TREE=S3_19v source ../../../log/roadmap/S3/18_env.sh && ~/bin/py -3.12 port/test/build.py --arch x64
~/bin/py -3.12 -m pytest port/test/test_gfx_s3_19.py              # + SG_EMU_ORACLE=1 for the call oracle
~/bin/py -3.12 -m port.test.framegate cov_r_bio --no-probe --out <tsv>
```

## 7. Open

U-1846 (the port never stores [+0x57c]; needed with Up / Down / clicks), U-1847 (palette indices: white paint and black text are
not measured at the bio), U-1848 (the unconstructed Image in the port's state; the steady caret), U-1849 (no reference frame of the
box). Lead in 19_merge.md: gfx_chareditor.cpp's unused body `showCharacterEditor.bio` (not this step's file); U-1844 can be resolved.
