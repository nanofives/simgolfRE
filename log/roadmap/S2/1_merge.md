# S2.1 merge notes (shared files the integrator applies)

The owned files are already in the working tree (port/src/sim/gfx.cpp, gfx_body.h, gfx_families.inc, gfx_selftest.cpp,
port/test/gfx.py, test_gfx.py; gfx.h unchanged). Two shared files need the lines below. Item 1 must land together with
gfx_selftest.cpp: without it port/test/test_static.py::test_every_module_file_is_listed fails ("Extra items: gfx_selftest").

## 1. port/test/test_static.py, test_every_module_file_is_listed (REQUIRED)

Patch file: log/roadmap/S2/1_test_static.patch (made against HEAD c6d7d78b; `git apply` from the repo root after replacing
the `b/port/build/S2_1_head/` prefix, or apply by hand). The function becomes:

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

Checked in the mirror: port/test/test_static.py 6 passed.

## 2. port/test/README.md, new section (gfx_body.h's header points at it)

Append at the end of the file:

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

## 3. Nothing for modules.inc, CMakeLists.txt, tick.cpp, commands.cpp

gfx_selftest.cpp is picked up by CMake's `src/sim/*.cpp` glob and holds no module. No registry.cpp touch is needed.
