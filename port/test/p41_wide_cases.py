"""P41W: the wide front-end backdrop checks (see port/test/test_p41_uivec.py's docstring), imported by test_p41_uivec.py so they run
in its process with its fixtures (portlib, the install, _title_state)."""
from __future__ import annotations

import ctypes as C
import subprocess
import sys

import numpy as np
import pytest

WIDE_VA = 0x30000000
WIDE_CAP = 1 << 23   # words
MAD_MAX, PX_MAX = 1.0, 12.0
# (name, aspect, scale, frame origin x, y, w, h): the whole 4:3 and 16:9 screens at 1x; crops at 2.7x (the marks) and 3.6x (the ground
# upsampled, across the old frame edge x = 0)
WIDE_CASES = [("43", 4 / 3, 1.0, 0.0, 0.0, 800, 600), ("169", 16 / 9, 1.0, -133.33, 0.0, 1067, 600),
              ("marks_x2.7", 16 / 9, 2.7, 0.0, 480.0, 700, 300), ("edge_x3.6", 16 / 9, 3.6, -60.0, 200.0, 500, 400)]


def make(port_export, sdf, uivec_dir, install, title_state, root_of):
    need_install = pytest.mark.skipif(not (install / "golf_clean.exe").exists(), reason="no install (original/)")
    screens = list(port_export.WIDE)

    @pytest.fixture(scope="module")
    def wlib(portlib):
        if not portlib.has("uvwide.render"):
            pytest.skip("the library has no gfx_uvwide module")
        return portlib

    @pytest.fixture(scope="module")
    def wide_static():
        """{screen: (static scene, refs, full scene)}: the wide scene without the overlays the game draws itself"""
        out = {}
        for screen in screens:
            sc = port_export.wide_scene(screen)
            ov = port_export.WIDE[screen]["overlays"]
            out[screen] = (dict(sc, draw=[e for e in sc["draw"] if e.get("name") not in ov]), port_export.wide_refs(screen), sc)
        return out

    def wide_state(lib):
        st = lib.state()
        st.files_root(install)
        buf = (C.c_uint32 * WIDE_CAP)()
        st.map(WIDE_VA, buf)
        st._wide_buf = buf
        return st

    def wide_dll(st, k, s, fx0, fy0, w, h, gcap=1.0):   # gcap: gfx_uvwide.h kUwGroundScaleMax
        n = st.call("uvwide.render", [k, int(round(s * 1000)), int(round(fx0 * 100)) & 0xFFFFFFFF, int(round(fy0 * 100)) & 0xFFFFFFFF,
                                      w, h, WIDE_VA, WIDE_CAP, int(round(gcap * 1000))]).eax
        assert n == 3 * w * h, (k, s, n)
        return np.frombuffer(bytes(st._wide_buf)[:12 * w * h], "<f4").reshape(h, w, 3).astype(np.float64)

    def wide_ref(static, refs, a, s, fx0, fy0, w, h):
        """wide.render's picture before its rounding (place, sdf.render, composite_picture) over a crop"""
        import wide
        img = np.zeros((h, w, 3))
        under = wide.unders(static, refs)
        pics = [i for i, e in enumerate(static["draw"]) if "picture" in e]
        k = 0
        for entry in wide.place(static, a, refs):
            if entry[0] == "items":
                sdf.render(entry[1], img, s, origin=(fx0, fy0))
            else:
                wide.composite_picture(img, entry[1], s, (fx0, fy0), entry[2], under.get(pics[k]))
                k += 1
        return img

    def judge(got, want):
        d = np.abs(got - want)
        return float(d.mean()), float(d.max())

    @need_install
    def test_wide_includes_are_the_exporters_output():
        r = subprocess.run([sys.executable, str(uivec_dir / "port_export.py"), "--wide", "all", "--check"], capture_output=True, text=True)
        assert r.returncode == 0, r.stdout + r.stderr

    @need_install
    def test_wide_backdrop_equals_wide_render(wlib, wide_static):
        bad = []
        cases = [(scr, c) for scr in screens for c in WIDE_CASES]
        with wide_state(wlib) as st:
            for n, (screen, (name, a, s, fx0, fy0, w, h)) in enumerate(cases):
                k = screens.index(screen)
                assert st.call("uvwide.prepare", [k]).eax == 1, screen
                static, refs, _ = wide_static[screen]
                mad, worst = judge(wide_dll(st, k, s, fx0, fy0, w, h), wide_ref(static, refs, a, s, fx0, fy0, w, h))
                ok = mad <= MAD_MAX and worst <= PX_MAX
                print(f"[{n + 1}/{len(cases)}] {screen}.{name} {'PASS' if ok else 'FAIL'} mad {mad:.2e} max {worst:.2e}", flush=True)
                if not ok:
                    bad.append((screen, name, mad, worst))
        assert not bad, bad

    @need_install
    def test_wide_picture_runs_equal_composite_picture(wlib, wide_static):
        import wide
        bad, n = [], 0
        with wide_state(wlib) as st:
            for screen in screens:
                k = screens.index(screen)
                assert st.call("uvwide.prepare", [k]).eax == 1
                _, refs, full = wide_static[screen]
                pics = [e for e in full["draw"] if "picture" in e]
                for p, e in enumerate(pics):
                    under = wide.under_1x(full, refs, full["draw"].index(e)) if e["picture"]["matte"]["kind"] == "ground_diff" else None
                    for s in (1.0, 2.7):
                        m = st.call("uvwide.picRender", [k, p, int(round(s * 1000)), WIDE_VA, WIDE_CAP]).eax
                        assert m > 4, (screen, p, s)
                        w, h, x0, y0 = (C.c_int32 * 4).from_buffer(st._wide_buf)[:]
                        prem = np.frombuffer(bytes(st._wide_buf)[16:16 + 16 * w * h], "<f4").reshape(h, w, 4).astype(np.float64)
                        mads, worst = [], 0.0
                        for bg in (0.0, 255.0):
                            ref = np.full((h, w, 3), bg)
                            wide.composite_picture(ref, e, s, (x0, y0), 0.0, under)
                            got = prem[..., :3] + (1 - prem[..., 3:4]) * bg
                            mad, wst = judge(got, ref)
                            mads.append(mad)
                            worst = max(worst, wst)
                        n += 1
                        ok = max(mads) <= MAD_MAX and worst <= PX_MAX
                        print(f"[{n}] {screen}.{e['name']} x{s} {'PASS' if ok else 'FAIL'} mad {max(mads):.2e} max {worst:.2e}", flush=True)
                        if not ok:
                            bad.append((screen, e["name"], s, max(mads), worst))
        assert n > 0 and not bad, bad

    @need_install
    def test_wide_overlay_scenes_equal_sdf(wlib):
        """every overlay scene (port_export.py "draws") rendered as its run equals sdf.render of the approved scene at 1x and 2.7x,
        over black and white"""
        bad, n = [], 0
        with wide_state(wlib) as st:
            for screen in screens:
                k = screens.index(screen)
                draws = [d for d in port_export.WIDE[screen].get("draws", []) if d["scene"] != "restore"]
                nov = st.call("uvwide.overlays", [k]).eax
                scene_ovl = [i for i in range(nov) if st.call("uvwide.overlay", [k, i]).eax & 0xFF == 2]
                assert len(scene_ovl) == len(draws), (screen, len(scene_ovl), len(draws))
                for i, d in zip(scene_ovl, draws):
                    items = port_export.overlay_items(d["scene"])
                    for s in (1.0, 2.7):
                        m = st.call("uvwide.ovlRender", [k, i, int(round(s * 1000)), WIDE_VA, WIDE_CAP]).eax
                        assert m > 4, (screen, d["name"], s)
                        w, h, x0, y0 = (C.c_int32 * 4).from_buffer(st._wide_buf)[:]
                        prem = np.frombuffer(bytes(st._wide_buf)[16:16 + 16 * w * h], "<f4").reshape(h, w, 4).astype(np.float64)
                        mads, worst = [], 0.0
                        for bg in (0.0, 255.0):
                            ref = np.full((h, w, 3), bg)
                            sdf.render(items, ref, s, origin=(x0, y0))
                            got = prem[..., :3] + (1 - prem[..., 3:4]) * bg
                            mad, wst = judge(got, ref)
                            mads.append(mad)
                            worst = max(worst, wst)
                        n += 1
                        ok = max(mads) <= MAD_MAX and worst <= PX_MAX
                        print(f"[{n}] {screen}.{d['name']} x{s} {'PASS' if ok else 'FAIL'} mad {max(mads):.2e} max {worst:.2e}", flush=True)
                        if not ok:
                            bad.append((screen, d["name"], s, max(mads), worst))
        assert not bad, bad

    @need_install
    @pytest.mark.parametrize("mode", [1, 2, 3])
    def test_wide_negctl_reads_red(wlib, wide_static, mode):
        screen = "title"
        k = screens.index(screen)
        static, refs, _ = wide_static[screen]
        # mode 1 (soft x2) on the whole 4:3 screen, 2 (the upsample misregistered) on the 4K crop, 3 (no feather) on the marks' crop
        name, a, s, fx0, fy0, w, h = WIDE_CASES[{1: 0, 2: 3, 3: 2}[mode]]
        want = wide_ref(static, refs, a, s, fx0, fy0, w, h)
        wlib.negctl("uvwide.render", mode)
        try:
            with wide_state(wlib) as st:
                assert st.call("uvwide.prepare", [k]).eax == 1
                mad, worst = judge(wide_dll(st, k, s, fx0, fy0, w, h), want)
        finally:
            wlib.negctl_reset()
        red = mad > MAD_MAX or worst > PX_MAX
        print(f"uvwide mode {mode}: {screen}.{name} mad {mad:.2e} max {worst:.2e} {'RED' if red else 'BLIND'}")
        assert red, f"uvwide mode {mode} is blind"

    @need_install
    def test_wide_option_off_makes_the_original_calls(wlib):
        if wlib.exe_load():
            pytest.skip("no exe image")
        roots, shown, frame = {}, {}, None
        for pilot in (None, 0, 1, "neg4"):
            if pilot == "neg4":
                wlib.negctl("uvwide.render", 4)
            try:
                with title_state(wlib, 1 if pilot == "neg4" else pilot) as st:
                    buf = (C.c_uint32 * (800 * 600))()
                    st.map(WIDE_VA, buf)
                    assert st.call("uvwide.drawTitleBg", []).eax == 1, pilot
                    roots[pilot] = root_of(st)
                    shown[pilot] = C.c_int32(st.call("uvwide.shown", []).eax).value
                    if pilot == 1:
                        assert st.call("uvwide.frame", [0, WIDE_VA]).eax == 1
                        frame = np.frombuffer(bytes(buf)[:800 * 600 * 2], "<u2").reshape(600, 800).copy()
                    # the logo cut 13 at its own place (168, 198): a picture run only while the backdrop is shown
                    before = root_of(st)
                    assert st.call("uivec.drawTitle", [13, 168, 198]).eax == 1, pilot
                    roots[(pilot, "logo")] = (before, root_of(st))
            finally:
                if pilot == "neg4":
                    wlib.negctl_reset()
        art = roots[None]
        assert (roots[0] == art).all(), "theme off: the title background is not the original body's art"
        assert (roots["neg4"] == art).all(), "uvwide 4: the hook off does not draw the art"
        assert shown[None] == -1 and shown[0] == -1 and shown["neg4"] == -1, shown
        assert shown[1] == 0, shown
        assert (roots[1] == frame).all(), "theme on: the root is not the backdrop's 1x frame"
        assert (roots[1] != art).any()
        print("[1/2] title background: off = art, on = backdrop frame, uvwide 4 = art PASS", flush=True)
        b0, a0 = roots[(None, "logo")]
        b1, a1 = roots[(0, "logo")]
        assert (a1 == a0).all() and (a0 != b0).any(), "theme off: the logo cut is not the art"
        bn, an = roots[("neg4", "logo")]
        assert ((an != bn) == (a0 != b0)).all(), "no backdrop shown: the logo cut is not drawn as art"
        bo, ao = roots[(1, "logo")]
        changed = ao != bo
        assert changed.any() and not changed[:, :168].any() and not changed[:198].any(), "theme on: the logo run drew outside its place"
        print("[2/2] title logo: off = art, no backdrop = art, on = picture run PASS", flush=True)

    @need_install
    def test_wide_backgrounds_take_the_hook(wlib):
        """every wide screen's background, loaded from its own sheet and copied whole onto the root the way that screen does it
        (Surface_blitThunk; the world map's Surface_fillRegion): with the theme off the original body's art and no backdrop; with the
        theme on the screen's backdrop 1x frame and the screen marked"""
        if wlib.exe_load():
            pytest.skip("no exe image")
        name_va = 0x2e000000
        n = 0
        for screen in screens:
            k = screens.index(screen)
            sheet = port_export.WIDE[screen]["bg"]
            via = 1 if screen == "world" else 0
            got = {}
            for pilot in (None, 0, 1):
                with title_state(wlib, pilot) as st:
                    nb = st.map(name_va, bytearray(0x100))
                    raw = sheet.replace("/", "\\").encode() + b"\0"
                    C.memmove(C.addressof(nb), raw, len(raw))
                    buf = (C.c_uint32 * (800 * 600))()
                    st.map(WIDE_VA, buf)
                    assert st.call("uvwide.drawBg", [name_va, via]).eax == 1, (screen, pilot)
                    got[pilot] = (root_of(st), C.c_int32(st.call("uvwide.shown", []).eax).value)
                    if pilot == 1:
                        assert st.call("uvwide.frame", [k, WIDE_VA]).eax == 1
                        frame = np.frombuffer(bytes(buf)[:800 * 600 * 2], "<u2").reshape(600, 800).copy()
            art = got[None][0]
            assert (got[0][0] == art).all() and got[None][1] == -1 and got[0][1] == -1, f"{screen}: theme off is not the art"
            assert got[1][1] == k and (got[1][0] == frame).all(), f"{screen}: theme on is not the backdrop (shown {got[1][1]})"
            assert (frame != art).any()
            n += 1
            print(f"[{n}/{len(screens)}] {screen} ({sheet}, {'fillRegion' if via else 'blitThunk'}) off=art on=backdrop PASS", flush=True)

    return {k: v for k, v in locals().items() if k.startswith("test_") or k in ("wlib", "wide_static")}
