"""Wave 8 fix layer 2 for I21's suite (2026-10-31): the host-GL tests S3.21's GlRaster rules moved. Applied after w8_assemble.py and
w8_fix_i21.py (wave 9's stack too). Every old text must occur exactly once; an edit already present is refused.

    ~/bin/py -3.12 log/roadmap/S3/w8_fix_i21b.py --root <tree> [--dry]

3. port/test/test_gl_wgl.py test_raster_backend_is_unchanged pinned GlRaster's sandbox_basic sandbox_00 at I14's 125 px. With S3.21
   part 1 (float32 vertex rule + clipping, default on) the census row is FAIL 9 px, all one level (log/roadmap/S3/w8_out/census_split.tsv,
   equal to 21_out/census.tsv). Renamed test_raster_backend_equals_its_census_row, pin 9.
4. port/test/test_gl_wgl_selfcheck.py test_self_check_sees_the_rasteriser_residual was the check's negative control: GlRaster
   differed from the original in 45 of the reference rectangle's 1,024 px (S3.20). Under S3.21's rules GlRaster equals the original
   there, so the control lost its residual. glWgl.rasterCheck gets one argument, old_rules: 1 sets GlRaster tune knobs 28 and 29
   (S3.21's off switches, the same as SG_RASTER_TUNE=28=1,29=1) on that rasteriser only, which brings the 45 px back. The test now
   pins both: the default GlRaster PASSES the check (0 px differ, S3.21's result), and the old rules read RED on the 45 px, all one
   level. Code: gl_wgl.h / gl_wgl.cpp (raster_self_check(ctx, old_rules = 0), the harness entry's arity 0 -> 1); no other caller of
   raster_self_check exists, and the app's `--gl auto` runs wgl_self_check (host GL), never raster_self_check.
"""
import argparse
import pathlib
import sys

EDITS = [
    # ---- 3: test_gl_wgl.py
    ("port/test/test_gl_wgl.py",
     'def test_raster_backend_is_unchanged(lib):\n'
     '    # GlRaster (--gl raster) keeps its residual: sandbox_00\'s I14 census numbers (125 px, all one level)\n'
     '    r = {x["checkpoint"]: x for x in _run(lib, "sandbox_basic", host_gl=False, gl="raster")}["sandbox_00"]\n'
     '    assert r["status"] == "FAIL" and int(r["total_px"]) == 125 and int(r["beyond_px"]) == 0 and r["renderer"] == "raster", r\n',
     'def test_raster_backend_equals_its_census_row(lib):\n'
     '    # GlRaster (--gl raster) keeps a residual on sandbox_00: its census row after roadmap S3.21 (float32 vertex rule and clipping,\n'
     '    # default on) is 9 px, all one level (I14 had 125 px before S3.21; log/roadmap/S3/w8_out/census_split.tsv)\n'
     '    r = {x["checkpoint"]: x for x in _run(lib, "sandbox_basic", host_gl=False, gl="raster")}["sandbox_00"]\n'
     '    assert r["status"] == "FAIL" and int(r["total_px"]) == 9 and int(r["beyond_px"]) == 0 and r["renderer"] == "raster", r\n'),
    # ---- 4a: gl_wgl.h
    ("port/include/portsim/sim/gl_wgl.h",
     '// The same reference pass through the portable rasteriser (tests: the check\'s sensitivity to the U-1392 residual)\n'
     'SelfCheck raster_self_check(Ctx &ctx);\n',
     '// The same reference pass through the portable rasteriser (tests). Since roadmap S3.21 GlRaster equals the original on the reference\n'
     '// rectangle; old_rules = 1 sets GlRaster tune 28 and 29 (S3.21\'s pre-rule vertex model and no clipping) on this rasteriser only,\n'
     '// the check\'s negative control (45 of the 1,024 px differ, the U-1392 residual S3.20 measured)\n'
     'SelfCheck raster_self_check(Ctx &ctx, int old_rules = 0);\n'),
    # ---- 4b: gl_wgl.cpp, the function
    ("port/src/sim/gl_wgl.cpp",
     'SelfCheck raster_self_check(Ctx &ctx) {\n'
     '  SelfCheck s;\n'
     '  const auto t0 = std::chrono::steady_clock::now();\n'
     '  GlRaster r(800, 600);\n'
     '  r.no_context();\n',
     'SelfCheck raster_self_check(Ctx &ctx, int old_rules) {\n'
     '  SelfCheck s;\n'
     '  const auto t0 = std::chrono::steady_clock::now();\n'
     '  GlRaster r(800, 600);\n'
     '  r.no_context();\n'
     '  if (old_rules) r.tune[28] = r.tune[29] = 1;   // roadmap S3.21\'s off switches (= SG_RASTER_TUNE=28=1,29=1), this rasteriser only\n'),
    # ---- 4c: gl_wgl.cpp, the harness entry
    ("port/src/sim/gl_wgl.cpp",
     'void a_raster_check(Ctx &c, const uint32_t *, Ret &r) {\n'
     '  const gl::SelfCheck s = gl::raster_self_check(c);\n',
     'void a_raster_check(Ctx &c, const uint32_t *a, Ret &r) {\n'
     '  const gl::SelfCheck s = gl::raster_self_check(c, static_cast<int>(a[0]));\n'),
    ("port/src/sim/gl_wgl.cpp",
     '    {"glWgl.rasterCheck", "default", "int", 0, 0, a_raster_check, nullptr},\n',
     '    {"glWgl.rasterCheck", "default", "int", 1, 0, a_raster_check, nullptr},\n'),
    ("port/src/sim/gl_wgl.cpp",
     '// `$wgl/selfcheck.txt` (pass, reason, got, want, ms) and `$wgl/selfcheck_rect.555` (the rectangle drawn); glWgl.rasterCheck() the\n'
     '// same reference through GlRaster (`$wgl/raster_rect.555`).\n',
     '// `$wgl/selfcheck.txt` (pass, reason, got, want, ms) and `$wgl/selfcheck_rect.555` (the rectangle drawn); glWgl.rasterCheck(old_rules)\n'
     '// the same reference through GlRaster (`$wgl/raster_rect.555`; old_rules 1 = S3.21\'s rules off, the negative control).\n'),
    # ---- 4d: test_gl_wgl_selfcheck.py
    ("port/test/test_gl_wgl_selfcheck.py",
     'texture coordinate one texel off (perturb 2, negctl 14), a DIB with RGB565 masks (variant 1, negctl 8), the same reference through\n'
     'GlRaster (its U-1392 residual: 45 of the 1,024 pixels).',
     'texture coordinate one texel off (perturb 2, negctl 14), a DIB with RGB565 masks (variant 1, negctl 8), the same reference through\n'
     'GlRaster with roadmap S3.21\'s rules off (glWgl.rasterCheck(1): the U-1392 residual S3.20 measured, 45 of the 1,024 pixels). Since\n'
     'S3.21 the default GlRaster passes the check too (0 pixels differ); the app\'s `--gl auto` runs the check on the host GL only.'),
    ("port/test/test_gl_wgl_selfcheck.py",
     'RASTER_PX = 45          # pixels of the rectangle GlRaster draws differently from the original (U-1392)\n',
     'RASTER_PX = 45          # pixels of the rectangle GlRaster with S3.21\'s rules off draws differently from the original (U-1392)\n'),
    ("port/test/test_gl_wgl_selfcheck.py",
     'def test_self_check_sees_the_rasteriser_residual(lib):\n'
     '    """GlRaster on the same reference: RED, on exactly the U-1392 one-level pixels"""\n'
     '    ret, kv, px, _ = _check(lib, "glWgl.rasterCheck", ())\n',
     'def test_default_rasteriser_passes_the_self_check(lib):\n'
     '    """roadmap S3.21 (float32 vertex rule and clipping, default on): GlRaster equals the original on the reference rectangle"""\n'
     '    ret, kv, px, _ = _check(lib, "glWgl.rasterCheck", (0,))\n'
     '    assert ret == 1 and kv["pass"] == "1" and kv["got"] == kv["want"] == _ref()["sha"], kv\n'
     '    if SNAP.exists():\n'
     '        assert int((px != _original_rect()).sum()) == 0\n'
     '\n'
     '\n'
     'def test_self_check_sees_the_rasteriser_residual(lib):\n'
     '    """GlRaster with S3.21\'s rules off on the same reference: RED, on exactly the U-1392 one-level pixels"""\n'
     '    ret, kv, px, _ = _check(lib, "glWgl.rasterCheck", (1,))\n'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    root = pathlib.Path(a.root)
    out = {}
    for rel, old, new in EDITS:
        p = root / rel
        s = out.get(rel) or p.read_bytes().decode("utf-8")
        crlf = "\r\n" in s
        o, n = (old.replace("\n", "\r\n"), new.replace("\n", "\r\n")) if crlf else (old, new)
        if s.count(o) == 0 and s.count(n) >= 1:
            sys.exit(f"refused: {rel}: edit already applied")
        k = s.count(o)
        if k != 1:
            sys.exit(f"refused: {rel}: old text found {k} times")
        out[rel] = s.replace(o, n)
        print(f"edit {rel}")
    if a.dry:
        print("dry run: nothing written")
        return
    for rel, s in out.items():
        (root / rel).write_bytes(s.encode("utf-8"))
        print(f"wrote {rel}")


if __name__ == "__main__":
    main()
