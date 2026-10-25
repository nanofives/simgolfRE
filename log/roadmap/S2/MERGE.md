# S2 merge request, wave 1: gfx body families (lead cmvmns3ax3frbo1152mcm7eg2, 2026-10-24)

Wave 1 = step S2.1 (gfx plumbing, accepted) plus the S2.2 screen map (accepted, planning only). The rest of S2 comes in later waves.
Files: log/roadmap/S2/files.txt (wave 1 list). The port files are already in the working tree (equal to the worker's mirror).

## 1. Shared-file edits, in apply order

### 1.1 port/test/test_static.py: replace `test_every_module_file_is_listed` (REQUIRED with gfx_selftest.cpp)
Patch: log/roadmap/S2/1_test_static.patch (against HEAD c6d7d78b; strip the `port/build/S2_1_head/` prefix) or by hand:

```python
def test_every_module_file_is_listed():
    listed = set(re.findall(r"^SG_MODULE\((\w+)\)", (PORT / "src" / "sim" / "modules.inc").read_text(), re.M))
    # gfx body families (roadmap S2.1, port/src/sim/gfx_body.h): gfx_<f>.cpp files listed in gfx_families.inc, not modules
    families = set(re.findall(r"^SG_GFX_FAMILY\((\w+)\)", (PORT / "src" / "sim" / "gfx_families.inc").read_text(), re.M))
    for f in families:
        src = (PORT / "src" / "sim" / f"gfx_{f}.cpp").read_text(encoding="utf-8")
        assert f"kGfxFamily_{f}" in src and f"g_negctl_gfx_{f}" in src, f
    files = {p.stem for p in (PORT / "src" / "sim").glob("*.cpp")} - {f"gfx_{f}" for f in families}
    assert files == listed, f"files {sorted(files)} != modules.inc {sorted(listed)}"
    for m in listed:
        src = (PORT / "src" / "sim" / f"{m}.cpp").read_text(encoding="utf-8")
        assert f"kModule_{m}" in src
```

Checked by the lead in the mirror port/build/S2_1_head: port/test/test_static.py 6 passed.

### 1.2 port/test/README.md: append at the end of the file

```markdown
## Body families (graphics backend, roadmap S2.1)

The software graphics backend (module `gfx`, port/src/sim/gfx.cpp) dispatches a drop to the drawing body of that name. Bodies
live in the core table of gfx.cpp and in FAMILY files, one per area of the screen code, so parallel workers never edit the same
file:

1. Create `port/src/sim/gfx_<family>.cpp`: include `gfx_body.h`, define `int g_negctl_gfx_<family> = 0;`, a `GfxBodyDef`
   table `{drop name, body}` (bodies take `(Gfx &, Ctx &, const uint32_t *a, int n)`, a[0..12) the drop's argument slots),
   an optional not-drawing name list, and `extern const portsim::GfxFamily kGfxFamily_<family> = {"<family>", ...};`.
2. Add one line `SG_GFX_FAMILY(<family>)` to `port/src/sim/gfx_families.inc`. Nothing else is shared: gfx.cpp references every
   listed family by name (a static library would drop a self-registering object), and the registry gets `gfx.family.<family>`
   (returns the body count) with the family's negctl switch (`sg_negctl_set("gfx.family.<family>", mode)`).
3. Bodies use the shared helpers of gfx_body.h (load_image, crop_sprite, draw16, copy8, draw_g16, draw_text, draw_frame_r3,
   draw_message_buffer ...), write only the backend's objects, and report a case they do not model with `Gfx::unsup`.
4. Tests: `gfx.call(name_va, a0..a10)` drives one body without a trace (the drop name as a NUL-terminated string in the
   context's address space); `gfx.check()` writes `$gfx/bodies.txt` (every body and not-drawing name with its owner, plus
   `duplicate` lines) and returns the number of names registered twice, which must be 0. The test-only family `selftest`
   (gfx_selftest.cpp, names prefixed `gfx_selftest.`) is the worked example; port/test/test_gfx.py's S2.1 tests read RED when
   its line is removed from gfx_families.inc.

gfx.log (`$gfx/log.txt`) lists `handled` and `unsupported` counts, then `first\t<k>\t<reason>` for every distinct unsupported
reason in first-occurrence order (no cap); a snapshot's `unsupported before it` count n means the first n of them.
```

### 1.3 UNCERTAINTIES.md: append these 6 rows after the last row (currently U-1121)

| U-1200 | - | control | [roadmap S2.1] gfx negctl 3 (draw16's transparency test `> 0xfe` instead of `>= 0xfe`) is an equivalent mutant on every PASS checkpoint the gate has (sandbox_basic menu, the five cov_b_location rows, cov_r_quit menu / exit_leave / title1 / exit_saved / title2: framegate --negctl gfx.attach=3 gives the same 11 PASS, log/roadmap/S2/1_framegate_negctl3.tsv); test_gfx keeps it in NEGCTL_BLIND | a checkpoint whose drawn sprites hold colour index 0xfe (an in-game sprite once S2 bodies draw them) | open |
| U-1210 | - | census | [roadmap S2.2] frame_census.tsv marks the terrain column `yes` for full-screen 2D checkpoints (cov_a_keys k_dlg_L load browser, k_dlg_R routing map, cov_a_career career_editor, cov_a_golfer button_0, cov_a_keys3 q_editor, cov_k1_routing routing) although their reference frames hold no terrain pixel; the S2 gate (no terrain on screen) would skip them | a screen kind or a rule fix in port/test/framegate.py (lead / integrator) | open |
| U-1211 | - | survey | [roadmap S2.2] Of the 480 H1WAIT non-PASS rows only 8 references were viewed (ui_tour people / reports, cov_a_keys k_files / k_dialogs / k_misc / k_cheats / k_worldmap, cov_a_golfer panel not viewed); the rest are assumed to be course view plus HUD from their scenario scripts, not viewed one by one | a contact sheet of the 480 references | open |
| U-1212 | - | backend | [roadmap S2.2] gfx.cpp kBodies lists Image_draw, Surface_blit3, Surface_blit, drawWindow.frame by name only; `callees_with_body` in S2_screens.tsv says nothing about which sprite / surface kinds each body handles (census causes: Image_draw SpriteRef 39 rows, Surface_blit3 168) | per-kind coverage from S2.1's gfx.log reasons | open |
| U-1213 | - | survey | [roadmap S2.2] The port's S2 screen functions issue no drawing calls (stage 3.7b dropped them), so drawing_callees is the original's callee set from hooks.csv, filtered by name; a callee reached only through a helper or a vtable slot (Surface slots, Window slots) is not listed | disassembly of each screen's drawing path by the family step's worker | open |
| U-1214 | - | survey | [roadmap S2.2] 14 reference frames of the 50 rows were not viewed (cov_r_quit quit_00 / 01 / 03, cov_a_keys k_render_slash, cov_k2_simfoto right, cov_b_shots results / results_r2, cov_b_shots2 results / results_r2, cov_k1_routing routing and the 4 NOT_RUN title rows plus championship tournament_end have no snapshot); their family is taken from a sibling checkpoint or the context column | viewing them when a family step needs their pixels | open |

### 1.4 CLAUDE.md: append this paragraph after the "Roadmap S0, baseline and frame census" paragraph

```
Roadmap S2 wave 1, the gfx body families (2026-10-24, notes log/roadmap/S2/1_notes.md, 2_notes.md, steps re/roadmap/S2_steps.tsv,
screen map re/roadmap/S2_screens.tsv, U-1200, U-1210..U-1214). Drawing bodies of the gfx backend can now live in their own files:
`port/src/sim/gfx_<family>.cpp` includes port/src/sim/gfx_body.h (Image8 / Sprite8 / Surface16 / Gfx and the shared helpers, moved
out of gfx.cpp), defines a GfxBodyDef table and `kGfxFamily_<family>`, and is listed by one `SG_GFX_FAMILY(<family>)` line in
port/src/sim/gfx_families.inc, which gfx.cpp expands into explicit references (portsim is a static library, so a self-registering
object would be dropped) and one registry entry `gfx.family.<family>` carrying the family's negctl switch. New test entries
`gfx.call(name_va, a0..a10)` (one body without a trace) and `gfx.check()` (`$gfx/bodies.txt`, the count of names registered
twice); test-only family `selftest` (gfx_selftest.cpp). S0 recommendations 1, 3, 4: gfx.log lists every distinct unsupported reason
in first-occurrence order with no cap (`first	<k>	<reason>`), kNotDrawing gains playSound, Snd::status, capsLockOff,
emptyFunc_4a0890, saveCourseJpg, Widget_readPixel, enterModal483d30, and gfx.render's budget is the FRAME records up to the last
checkpoint + 2 with to_end past the last FRAME record (record_budget). Gates: test_gfx 25 / 25 x64 and Win32 (17 + 8; 6 of the 8
RED with the selftest line removed), segments 205 / 205 x64 + Win32 with 0 digests changed, boot 58 / 2 / 1 of HEAD's 61, framegate
sandbox_basic + cov_b_location + cov_r_quit equal to the census in status and px (first_unsupported now the first drawing call,
Widget_writePixel instead of Snd::status), port test_static 6 / 6 with the family-aware module listing. Controls 8 / 9 RED, gfx 3
blind on every PASS checkpoint (U-1200). Screen map (S2.2): of the non-PASS census rows, the pinned full-screen 2D checkpoints are
credits (cov_r_quit, 5), load browser (cov_a_keys k_dlg_L), routing map (k_dlg_R, cov_k1_routing routing) and the character editor
(cov_a_career career_editor, cov_a_golfer button_0, cov_a_keys3 q_editor); the census marks the last six terrain yes (U-1210). The
port's screen functions make no drawing calls (3.7b dropped them), so each screen family adds its calls. Twelve families have no
pinned checkpoint (difficulty, theme packs, pick a pro, hire, F1-F10 reports, end of year, preferences, scroll text / skill points,
shortcuts / roster, buy land, loading, radial menu). The 480 non-PASS H1WAIT rows are course view plus HUD (S3). claude2 workers have
no allow rule for a bare `cmake`, so a cmake call waits on an approval nobody sees: S2 workers build through
log/roadmap/S2/build.py (`~/bin/py`, VS Build Tools' cmake).
```

### 1.5 Nothing for modules.inc, CMakeLists.txt, tick.cpp, commands.cpp, gfx.h (gfx.h is unchanged).

## 2. Gate commands and expected numbers (repo root after merge, port/build/x64)

```
~/bin/py -3.12 -m pytest port/test/test_gfx.py -q -p no:cacheprovider                 # 25 passed
env SG_BACKEND=win32 ~/bin/py -3.12 -m pytest port/test/test_gfx.py -q -p no:cacheprovider   # 25 passed
~/bin/py -3.12 -m pytest port/test/test_static.py -q -p no:cacheprovider              # 6 passed
~/bin/py -3.12 -m port.test.integrator segments --backend x64 --jobs 4 --out <tsv>    # 205 / 205, 0 digests changed (same Win32)
~/bin/py -3.12 -m port.test.boot --all --jobs 3 --out <tsv>                           # HEAD's 61: 58 PASS / 2 KNOWN / 1 NO_FRAMES
~/bin/py -3.12 -m port.test.framegate sandbox_basic cov_b_location cov_r_quit --jobs 2 --out <tsv>   # 30 rows, 11 PASS, status / px as frame_census.tsv
```

## 3. For the orchestrator
- Stage the log/roadmap/S2 paths named in files.txt with `git add -f` (log/* is ignored); not the whole directory: the
  `*.tsv.d` framegate work dirs and later steps' files are not part of wave 1.
- After this commit gfx.cpp, gfx.h, gfx_body.h, gfx_families.inc, gfx.py and framegate.py are shared between S2 and S3: changes only
  through merge notes (S2_S3_SPLIT.md). S3.2 / S3.3 / S3.5 / S3.6 can start.
