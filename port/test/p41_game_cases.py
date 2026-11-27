"""P41G: the in-game element scenes (portsim/sim/gfx_uvgame.h, port/src/sim/gfx_uvgame.cpp, port/src/sim/uivec_game_<area>.inc), imported
by test_p41_uivec.py so they run in its process with its fixtures (portlib, the install, _root).

  * GAME PARITY: every exported element state (port_game.py records) rendered by the DLL (uvgame.render, at the record's own sheet place)
    equals sdf.render of the same approved scene (the scene file's items, not the exported tables), at 1x and 2.7x, over black and
    white: mean absolute difference <= 1.0 level and no channel over 12. Keep items are left out here (the sheet not read). One line
    per state `[N/M] area/id.state xS STATUS`. Control uvgame 2 (the bevel's normal upside down) reads RED.
  * KEEP ITEMS: a state with keep_art, its sheet read (uvgame.prepare), equals hp.render_state's rule (the sheet's own pixels, nearest,
    edge coverage clip(0.5 - d s), opaque pixels only) after the layers.
  * GAME EXPORT: port/src/sim/uivec_game_<area>.inc is what port_game.py writes now.
  * STATES (P41S, re/tools/uivec/states.py, states_port.py): a state exported as patches of its element's base is compared above with
    states.apply of its alteration (in the base's coordinates, the record's shift + origin), and states.apply itself equals the approved
    scene within 1 level (max abs) at 1x and 2.7x over black and white (test_game_states_apply_equals_approved). Control uvgame 8 (the
    patches ignored: every patched state drawn as its element's base) reads RED on the states that alter their base.
  * GAME OFF IS THE ORIGINAL: after loadInterfaceArt's cuts, every sprite that takes records drawn by Image_draw and by
    Surface_stretchBlit with the theme off draws exactly what a backend with no theme draws (the art); with the theme on the root
    outside the run's clip is unchanged and inside it is the run's 1x render over the root as it was (RGB555, within one level), and
    not the art. Controls: uvgame 3 (the hook off) draws the art with the theme on; uvgame 1 (one pixel to the right) reads RED.
"""
from __future__ import annotations

import ctypes as C
import importlib
import os
import subprocess
import sys

import numpy as np
import pytest

GAME_VA = 0x30000000
GAME_CAP = 1 << 25   # words: a full 800 x 600 screen at 2.7x is 14 M floats
TEXT_VA = 0x2e000000
MAD_MAX, PX_MAX = 1.0, 12.0


def make(sdf, uivec_dir, install, root_of, over555):
    need_install = pytest.mark.skipif(not (install / "golf_clean.exe").exists(), reason="no install (original/)")
    sys.path.insert(0, str(uivec_dir))
    port_game = importlib.import_module("port_game")
    # the area modules register their shape kinds with sdf.EXTRA
    port_game.load_area_kinds()

    @pytest.fixture(scope="module")
    def glib(portlib):
        if not portlib.has("uvgame.render"):
            pytest.skip("the library has no gfx_uvgame module")
        return portlib

    @pytest.fixture(scope="module")
    def game_records(glib):
        """[(index, area, id, state, items, keep)] in the DLL's order, each matched to its port_game record"""
        py = {}
        for area in port_game.EXPORTED:   # (P41S: as exported: a patched state's shift and states.apply's items)
            for eid, st, sheet, cut, shift, items, extra, how in port_game.export_records(area):
                py[(area, eid, st)] = (sheet, cut, shift, items, extra.get("textures") or [], how)
        out = []
        with glib.state() as s:
            buf = (C.c_uint8 * 1024)()
            s.map(TEXT_VA, buf)
            n = s.call("uvgame.count", []).eax
            for i in range(n):
                ln = s.call("uvgame.info", [i, TEXT_VA]).eax
                f = bytes(buf)[:ln].decode().split("\t")
                area, eid, st, sheet = f[:4]
                x, y, w, h, sx, sy, nitems, keep = (int(v) for v in f[4:12])
                psheet, pcut, pshift, items, tex, how = py[(area, eid, st)]
                assert (psheet, [int(round(v)) for v in pcut], tuple(pshift)) == (sheet, [x, y, w, h], (sx, sy)), (i, area, eid, st)
                out.append((i, area, eid, st, items, bool(keep), (sheet, x, y, w, h, sx, sy), tex, how))
        assert len(out) == len(py)
        only = [a for a in os.environ.get("P41G_AREAS", "").split(",") if a]   # development: the areas to judge (default all)
        return [r for r in out if not only or r[1] in only]

    def _outbuf(s):
        """the state's output buffer at GAME_VA (mapped once per state)"""
        buf = getattr(s, "_p41g_out", None)
        if buf is None:
            buf = (C.c_uint32 * GAME_CAP)()
            s.map(GAME_VA, buf)
            s._p41g_out = buf
        return buf

    def _tbuf(s, off):
        attr = f"_p41g_t{off:x}"
        tb = getattr(s, attr, None)
        if tb is None:
            tb = (C.c_uint8 * 4096)()
            s.map(TEXT_VA + off, tb)
            setattr(s, attr, tb)
        return tb

    def g_render(lib, i, scale, st=None, keep=0):
        def run(s):
            buf = _outbuf(s)
            n = s.call("uvgame.render", [i, int(round(scale * 1000)), GAME_VA, GAME_CAP, keep]).eax
            assert n > 4, (i, scale)
            w, h, x0, y0 = (C.c_int32 * 4).from_buffer(buf)[:]
            return np.frombuffer(bytes(buf)[16:16 + 16 * w * h], dtype="<f4").reshape(h, w, 4).astype(np.float64), x0, y0
        if st is not None:
            return run(st)
        with lib.state() as s:
            return run(s)

    def layer_items(items):
        return [it for it in items if "layers" in it]

    def g_compare(prem, x0, y0, items, scale, keep=None, ref_fn=None, shift=(0, 0)):
        """keep(ref, x0, y0, scale, eps): the keep items over ref with the nearest sample taken at the point + eps (a sample point on
        a sheet pixel's edge, 2.7 x k = an integer + 0.5, picks either side by rounding: both are accepted, per pixel)"""
        h, w = prem.shape[:2]
        mads, worst = [], 0.0
        for bg in (0.0, 255.0):
            ref = np.full((h, w, 3), bg)
            if ref_fn is None:   # (the DLL renders at sheet coordinates: scene = sheet - shift, marks' crop pixels)
                sdf.render(layer_items(items), ref, scale, origin=(x0 - shift[0], y0 - shift[1]))
            got = prem[..., :3] + (1.0 - prem[..., 3:4]) * bg
            if keep is not None:
                best, err = None, None
                for e in ((-1e-6, -1e-6), (-1e-6, 1e-6), (1e-6, -1e-6), (1e-6, 1e-6)):
                    r = ref.copy()
                    if ref_fn is not None:
                        ref_fn(r, x0, y0, scale, e)
                    else:
                        keep(r, x0, y0, scale, e)
                    er = np.abs(got - r).max(axis=2)
                    if best is None:
                        best, err = r, er
                    else:
                        m = er < err
                        best = np.where(m[..., None], r, best)
                        err = np.minimum(er, err)
                ref = best
            touched = (prem[..., 3] > 0) | (np.abs(ref - bg).max(axis=2) > 0)
            d = np.abs(got - ref)[touched]
            mads.append(float(d.mean()) if d.size else 0.0)
            worst = max(worst, float(d.max()) if d.size else 0.0)
        return max(mads), worst

    @pytest.mark.parametrize("scale", [1.0, 2.7])
    def test_game_render_equals_sdf(glib, game_records, scale):
        bad = []
        n = len(game_records)
        with glib.state() as s:
            for k, (i, area, eid, st, items, keep, rec, _t, _h) in enumerate(game_records):
                prem, x0, y0 = g_render(glib, i, scale, s)
                mad, worst = g_compare(prem, x0, y0, items, scale, shift=rec[5:7])
                ok = mad <= MAD_MAX and worst <= PX_MAX
                print(f"[{k + 1}/{n}] {area}/{eid}.{st} x{scale} {'PASS' if ok else 'FAIL'} mad {mad:.2e} max {worst:.2e}", flush=True)
                if not ok:
                    bad.append((area, eid, st, mad, worst))
        assert not bad, bad

    def test_game_negctl_bevel_reads_red(glib, game_records):
        # the states whose layers carry a bevel paint
        picks = [r for r in game_records if any(isinstance(L.get("fill"), dict) and "bevel" in L["fill"]
                                                for it in layer_items(r[4]) for L in it["layers"])][:12]
        assert picks
        red = 0
        with glib.state() as s:
            glib.negctl("uvgame.render", 2)
            try:
                for (i, area, eid, st, items, keep, rec, _t, _h) in picks:
                    prem, x0, y0 = g_render(glib, i, 1.0, s)
                    mad, worst = g_compare(prem, x0, y0, items, 1.0, shift=rec[5:7])
                    red += mad > MAD_MAX or worst > PX_MAX
            finally:
                glib.negctl_reset()
        print(f"uvgame 2: {red}/{len(picks)} bevel states RED", flush=True)
        assert red >= len(picks) // 2

    def _patched_pairs():
        """[(area, id, state, approved items, exported items, rect in the base's coordinates, origin)] of the states exported as
        patches of their element's base (P41S)"""
        states = importlib.import_module("states")
        out = []
        for area in port_game.EXPORTED:
            for a, b in zip(port_game.records(area), port_game.export_records(area)):
                if b[7] != "patched":
                    continue
                eid, st, sheet, cut, shift, items, extra = a
                o = (b[4][0] - shift[0], b[4][1] - shift[1])
                approved = states._moved_items(items, list(o)) if o != (0, 0) else items
                rect = [cut[0] - b[4][0], cut[1] - b[4][1], cut[2], cut[3]]
                out.append((area, eid, st, approved, b[5], rect, o))
        return out

    def test_game_states_apply_equals_approved():
        """states.apply of every patched state equals its approved scene within 1 level (max abs, black and white, 1x and 2.7x):
        the conversion changes nothing visible"""
        states = importlib.import_module("states")
        pairs = _patched_pairs()
        assert pairs
        bad = []
        for k, (area, eid, st, approved, ref, rect, o) in enumerate(pairs):
            m = states.check(layer_items(ref), layer_items(approved), rect)
            ok = max(m) <= states.TOL_LEVELS
            print(f"[{k + 1}/{len(pairs)}] {area}/{eid}.{st} origin {o} max {m[0]:.3f} / {m[1]:.3f} {'PASS' if ok else 'FAIL'}", flush=True)
            if not ok:
                bad.append((area, eid, st, m))
        assert not bad, bad

    def test_game_negctl_states_read_red(glib, game_records):
        """control uvgame 8: the patches ignored (every patched state drawn as its element's base) reads RED on the states whose
        alteration changes the drawing"""
        st_mod = importlib.import_module("states_port")
        conv = {a: st_mod.load(a) for a in port_game.EXPORTED}
        picks = []
        for r in game_records:
            if r[8] != "patched":
                continue
            e = conv[r[1]].get(r[2])
            ref = e["states"][r[3]]
            if ref.get("status") == "exact" and e["variants"][ref["variant"]]["states"].get(r[3]):
                picks.append(r)
        assert picks
        red = 0
        with glib.state() as s:
            glib.negctl("uvgame.render", 8)
            try:
                for (i, area, eid, st, items, keep, rec, _t, _h) in picks:
                    prem, x0, y0 = g_render(glib, i, 1.0, s)
                    mad, worst = g_compare(prem, x0, y0, items, 1.0, shift=rec[5:7])
                    red += mad > MAD_MAX or worst > PX_MAX
            finally:
                glib.negctl_reset()
        print(f"uvgame 8: {red}/{len(picks)} patched states RED", flush=True)
        assert red >= (3 * len(picks)) // 4

    @need_install
    def test_game_keep_items(glib, game_records):
        from PIL import Image
        if glib.exe_load():
            pytest.skip("no exe image")
        # (a record whose only non-layer items are pictures is judged by test_game_pictures: its keep flag is any item nlayers < 0)
        keeps = [r for r in game_records if r[5] and (r[7] or any("keep_art" in it for it in r[4]))]
        tex_under = port_game.TEX_UNDER
        assert keeps, "no state with keep_art"
        sheets = {}
        bad = []
        with glib.state() as s:
            s.fill_from_exe()
            s.files_root(install)
            for k, (i, area, eid, st, items, keep, (sheet, cx, cy, cw, ch, shx, shy), tex, _h) in enumerate(keeps):
                assert s.call("uvgame.prepare", [i]).eax == 1, (eid, st, sheet)
                if sheet not in sheets:
                    p = next(q for q in install.rglob("*.pcx") if str(q.relative_to(install)).replace("\\", "/").lower() == sheet)
                    im = Image.open(p)
                    sheets[sheet] = (np.asarray(im), np.asarray(im.convert("RGB")).astype(float))
                idx, rgb = sheets[sheet]
                kshape = {"t": "union", "of": [it["keep_art"] for it in items if "keep_art" in it]}

                def keep_fn(ref, x0, y0, scale, eps, kshape=kshape, idx=idx, rgb=rgb, shx=shx, shy=shy):
                    if not kshape["of"]:
                        return
                    h, w = ref.shape[:2]
                    jj, ii = np.mgrid[0:h, 0:w]
                    X = (ii + 0.5) / scale + x0
                    Y = (jj + 0.5) / scale + y0
                    kk = np.clip(0.5 - sdf.sd(kshape, X, Y) * scale, 0, 1)
                    u = np.floor(X + shx + eps[0]).astype(int)
                    v = np.floor(Y + shy + eps[1]).astype(int)
                    ok = (u >= 0) & (v >= 0) & (u < idx.shape[1]) & (v < idx.shape[0])
                    u, v = np.clip(u, 0, idx.shape[1] - 1), np.clip(v, 0, idx.shape[0] - 1)
                    kk = np.where(ok & (idx[v, u] < 0xFE), kk, 0.0)[..., None]
                    ref[:] = ref * (1 - kk) + rgb[v, u] * kk
                def ref_fn(ref, x0, y0, scale, eps, items=items, tex=tex, eid=eid, keep_fn=keep_fn, idx=idx, rgb=rgb):
                    """layers, the textures at their place (pages.py render_tex), then the keep_art (hp.py render_state)"""
                    n_under = tex_under.get(eid, len(items))

                    def paste(t):
                        keep_fn2 = {"t": "union", "of": [t["clip"]]}
                        h, w = ref.shape[:2]
                        jj, ii = np.mgrid[0:h, 0:w]
                        X = (ii + 0.5) / scale + x0
                        Y = (jj + 0.5) / scale + y0
                        kk = np.clip(0.5 - sdf.sd(keep_fn2, X, Y) * scale, 0, 1)
                        u = np.floor(X + eps[0]).astype(int)
                        v = np.floor(Y + eps[1]).astype(int)
                        ok = (u >= 0) & (v >= 0) & (u < idx.shape[1]) & (v < idx.shape[0])
                        u, v = np.clip(u, 0, idx.shape[1] - 1), np.clip(v, 0, idx.shape[0] - 1)
                        kk = np.where(ok & (idx[v, u] < 0xFE), kk, 0.0)[..., None]
                        ref[:] = ref * (1 - kk) + rgb[v, u] * kk
                    for j, it in enumerate(items):
                        if tex and j == n_under:
                            for t in tex:
                                paste(t)
                        if "layers" in it:
                            sdf.render([it], ref, scale, origin=(x0, y0))
                    if tex and n_under >= len(items):
                        for t in tex:
                            paste(t)
                    keep_fn(ref, x0, y0, scale, eps)
                for scale in (1.0, 2.7):
                    prem, x0, y0 = g_render(glib, i, scale, s, keep=1)
                    mad, worst = g_compare(prem, x0, y0, items, scale, keep_fn, ref_fn)
                    ok = mad <= MAD_MAX and worst <= PX_MAX
                    print(f"[{k + 1}/{len(keeps)}] keep {area}/{eid}.{st} x{scale} {'PASS' if ok else 'FAIL'} mad {mad:.2e} max {worst:.2e}",
                          flush=True)
                    if not ok:
                        bad.append((eid, st, scale, mad, worst))
        assert not bad, bad

    def test_game_include_is_the_exporters_output():
        r = subprocess.run([sys.executable, str(uivec_dir / "port_game.py"), "--check"], capture_output=True, text=True)
        assert r.returncode == 0, r.stdout + r.stderr

    def _hud_state(lib, pilot, negctl=0):
        s = lib.state()
        s.fill_from_exe()
        s.files_root(install)
        s.call("gfx.attach", [])
        nb = s.map(TEXT_VA, bytearray(0x100))
        raw = b"loadInterfaceArt.images\0"
        C.memmove(C.addressof(nb), raw, len(raw))
        assert s.call("gfx.call", [TEXT_VA] + [0] * 11).eax == 1
        if pilot is not None:
            s.call("holestats.uiPilot", [pilot])
        return s

    def _sprites(s):
        buf = _outbuf(s)
        n = s.call("uvgame.sprites", [GAME_VA, 1 << 20]).eax
        out = []
        for ln in bytes(buf)[:n].decode().splitlines():
            key, x, y, left, top, cuts = ln.split()
            out.append((int(key), int(left), int(top), cuts))
        return out

    def _run_render(s, text):
        tb = _tbuf(s, 0x10000)
        raw = text.encode() + b"\0"
        C.memmove(C.addressof(tb), raw, len(raw))
        buf = _outbuf(s)
        n = s.call("uvgame.runRender", [TEXT_VA + 0x10000, 1000, GAME_VA, GAME_CAP]).eax
        assert n > 4, text
        w, h, x0, y0 = (C.c_int32 * 4).from_buffer(buf)[:]
        return np.frombuffer(bytes(buf)[16:16 + 16 * w * h], dtype="<f4").reshape(h, w, 4).astype(np.float64), x0, y0

    def _sprite_run(s, key, x, y):
        tb = _tbuf(s, 0x20000)
        C.memset(tb, 0, 4096)
        n = s.call("uvgame.spriteRun", [key, x & 0xFFFFFFFF, y & 0xFFFFFFFF, TEXT_VA + 0x20000]).eax
        return bytes(tb)[:n].decode()

    @need_install
    @pytest.mark.parametrize("via", [0, 1])
    def test_game_option_off_makes_the_original_calls(glib, via):
        if glib.exe_load():
            pytest.skip("no exe image")
        with _hud_state(glib, None) as s:
            sprites = _sprites(s)
        assert sprites, "loadInterfaceArt cut no sprite that takes records"
        # each sprite drawn with its crop corner at (40, 40) on a fresh root of each state (one state per theme, drawn in turn)
        X, Y = 40, 40
        states = {p: _hud_state(glib, p) for p in (None, 0, 1)}
        ctl = _hud_state(glib, 1)
        try:
            ok_on = 0
            for k, (key, left, top, cuts) in enumerate(sprites):
                got = {}
                for p, s in list(states.items()) + [("ctl3", ctl)]:
                    if p == "ctl3":
                        glib.negctl("uvgame.render", 3)
                    before = root_of(s)
                    assert s.call("uvgame.drawSprite", [key, X, Y, via]).eax == 1, (key, p)
                    got[p] = (before, root_of(s))
                    if p == "ctl3":
                        glib.negctl_reset()
                b0, art = got[None]
                assert (got[0][1] == art).all(), f"sprite {key} ({cuts}): the theme off does not draw the original's art"
                assert (got["ctl3"][1] == art).all() or (got["ctl3"][0] != b0).any(), f"sprite {key}: uvgame 3 does not draw the art"
                b1, on = got[1]
                # where the kept pixel (0, 0) lands: Image_draw adds the trims, Surface_stretchBlit does not
                kx, ky = (X, Y) if via == 1 else (X + left, Y + top)
                prem, x0, y0 = _run_render(states[1], _sprite_run(states[1], key, kx, ky))
                h, w = prem.shape[:2]
                outside = np.ones(on.shape, bool)
                outside[max(0, y0):y0 + h, max(0, x0):x0 + w] = False
                assert (on[outside] == b1[outside]).all(), f"sprite {key}: the run changed pixels outside its bounds"
                want = b1.copy()
                ys, xs = slice(max(0, y0), y0 + h), slice(max(0, x0), x0 + w)
                sub = prem[ys.start - y0:ys.stop - y0, xs.start - x0:xs.stop - x0]
                want[ys, xs] = over555(sub, b1[ys, xs])
                diff = np.zeros(on.shape, int)
                for sh in (10, 5, 0):
                    diff = np.maximum(diff, np.abs(((on >> sh) & 31).astype(int) - ((want >> sh) & 31).astype(int)))
                assert diff.max() <= 1, f"sprite {key} ({cuts}): the root is not the run's 1x render (max {diff.max()} levels)"
                if (on != art).any():
                    ok_on += 1
                print(f"[{k + 1}/{len(sprites)}] sprite {key} {cuts} via {via} off=art on=scene PASS", flush=True)
            assert ok_on == len(sprites)
        finally:
            for s in list(states.values()) + [ctl]:
                s.close()

    # install78 (log/roadmap/P41/install78/notes.md): the speed button (gfx_hud.cpp b_radial_menu) from its approved scenes
    # hud-speed-<1x|2x|4x> (records on the pseudo sheet @root, screen pixels): theme off and control uvgame 10 draw nothing through
    # it (the caller keeps the button made from the pause's pixels); theme on draws the record of that speed and state at no offset,
    # the root = the run's 1x render over what was there, and every speed and state draws differently
    @need_install
    def test_game_speed_button(glib, game_records):
        if glib.exe_load():
            pytest.skip("no exe image")
        idx = {(r[2], r[3]): r[0] for r in game_records if r[2].startswith("hud-speed-")}
        assert len(idx) == 6, sorted(idx)
        roots = {}
        for k, (speed, over) in enumerate([(sp, ov) for sp in (1, 2, 4) for ov in (0, 1)]):
            rid, st = f"hud-speed-{speed}x", "hover" if over else "idle"
            with _hud_state(glib, None) as s:
                b = root_of(s)
                assert s.call("uvgame.drawSpeed", [speed, over]).eax == 0, (speed, over, "theme off drew")
                assert (root_of(s) == b).all()
            with _hud_state(glib, 1) as s:
                glib.negctl("uvgame.render", 10)
                try:
                    b = root_of(s)
                    assert s.call("uvgame.drawSpeed", [speed, over]).eax == 0, (speed, over, "uvgame 10 drew")
                    assert (root_of(s) == b).all()
                finally:
                    glib.negctl_reset()
            with _hud_state(glib, 1) as s:
                b1 = root_of(s)
                assert s.call("uvgame.drawSpeed", [speed, over]).eax == 1, (speed, over)
                on = root_of(s)
                prem, x0, y0 = _run_render(s, f"uvg:{idx[(rid, st)]} 0 0")
            h, w = prem.shape[:2]
            outside = np.ones(on.shape, bool)
            outside[y0:y0 + h, x0:x0 + w] = False
            assert (on[outside] == b1[outside]).all(), (rid, st, "pixels changed outside the record's bounds")
            want = b1.copy()
            want[y0:y0 + h, x0:x0 + w] = over555(prem, b1[y0:y0 + h, x0:x0 + w])
            diff = np.zeros(on.shape, int)
            for sh in (10, 5, 0):
                diff = np.maximum(diff, np.abs(((on >> sh) & 31).astype(int) - ((want >> sh) & 31).astype(int)))
            assert diff.max() <= 1, (rid, st, int(diff.max()))
            assert 80 <= x0 <= 110 and 570 <= y0 <= 590, (rid, st, x0, y0)   # where gfx_hud.cpp draws the button (105, 590)
            roots[(speed, over)] = on
            print(f"[{k + 1}/6] {rid} {st}: off and uvgame 10 draw nothing, on = the record at ({x0}, {y0}) PASS", flush=True)
        keys = list(roots)
        for a in range(len(keys)):
            for b in range(a + 1, len(keys)):
                assert (roots[keys[a]] != roots[keys[b]]).any(), (keys[a], keys[b], "draw the same")

    @need_install
    def test_game_negctl_placement_reads_red(glib):
        if glib.exe_load():
            pytest.skip("no exe image")
        s = _hud_state(glib, 1)
        try:
            key, left, top, cuts = _sprites(s)[0]
            before = root_of(s)
            s.call("uvgame.drawSprite", [key, 40, 40, 0])
            good = root_of(s)
        finally:
            s.close()
        s = _hud_state(glib, 1)
        try:
            glib.negctl("uvgame.render", 1)
            s.call("uvgame.drawSprite", [key, 40, 40, 0])
            moved = root_of(s)
            glib.negctl_reset()
        finally:
            s.close()
        print(f"uvgame 1: {(moved != good).sum()} root pixels differ", flush=True)
        assert (moved != good).sum() > 20

    # P41G round 6: PICTURE items (wide.py's format, port_game.py add_pic): the record rendered by the DLL with its pictures (keep 1)
    # against sdf.render of its layers with the picture laid over as wide.composite_picture lays it, lifted by the area's own
    # reference (areas/reports_slabs/fix6_sga.py lift: ground_diff, solid, clip, feather) against the items beneath at 1x
    def _fix6_lift():
        import importlib
        d = str(uivec_dir / "areas" / "reports_slabs")
        saved = sys.modules.pop("common", None)
        sys.path.insert(0, d)
        try:
            for m in ("common", "fix6_sga"):
                sys.modules.pop(m, None)
            return importlib.import_module("fix6_sga").lift
        finally:
            sys.path.remove(d)
            sys.modules.pop("common", None)
            if saved is not None:
                sys.modules["common"] = saved

    def _picture_ref(items, x0, y0, scale, bg, h, w):
        from PIL import Image
        lift = _fix6_lift()
        ref = np.full((h, w, 3), float(bg))
        k = 0
        for n, it in enumerate(items):
            if "picture" not in it:
                continue
            G = np.zeros((600, 800, 3))
            sdf.render([q for q in items[:n] if "layers" in q], G, 1.0)
            rgb, al = lift(it["picture"], G)
            pics = (k, it, rgb, al)
            k += 1
        sdf.render(layer_items(items), ref, scale, origin=(x0, y0))
        _, it, rgb, al = pics
        px, py, pw, ph = it["picture"]["rect"]
        ax, ay = it["picture"].get("at", [px, py])
        W, H = int(round(pw * scale)), int(round(ph * scale))
        pm = np.dstack([rgb * al[..., None], al * 255.0]).astype(np.float32)
        big = np.dstack([np.asarray(Image.fromarray(pm[..., c], "F").resize((W, H), Image.BILINEAR)) for c in range(4)])
        i0, j0 = int(round((ax - x0) * scale)), int(round((ay - y0) * scale))
        a0, b0, a1, b1 = max(0, -i0), max(0, -j0), min(W, w - i0), min(H, h - j0)
        sub = big[b0:b1, a0:a1]
        A = sub[..., 3:4] / 255.0
        dst = ref[j0 + b0:j0 + b1, i0 + a0:i0 + a1]
        dst[:] = dst * (1 - A) + sub[..., :3]
        return ref, (j0 + b0, j0 + b1, i0 + a0, i0 + a1)

    def _picture_records(game_records):
        return [r for r in game_records if any("picture" in it for it in r[4])]

    @need_install
    @pytest.mark.parametrize("scale", [1.0, 2.7])
    def test_game_pictures(glib, game_records, scale):
        if glib.exe_load():
            pytest.skip("no exe image")
        recs = _picture_records(game_records)
        assert recs, "no record with a picture item"
        bad = []
        with glib.state() as s:
            s.fill_from_exe()
            s.files_root(install)
            for (i, area, eid, st, items, keep, rec, _t, _h) in recs:
                assert s.call("uvgame.prepare", [i]).eax == 1, (eid, st)
                prem, x0, y0 = g_render(glib, i, scale, s, keep=1)
                h, w = prem.shape[:2]
                worst, mads, inside = 0.0, [], 0.0
                for bg in (0.0, 255.0):
                    ref, (ja, jb, ia, ib) = _picture_ref(items, x0, y0, scale, bg, h, w)
                    got = prem[..., :3] + (1.0 - prem[..., 3:4]) * bg
                    d = np.abs(got - ref)
                    worst = max(worst, float(d.max()))
                    mads.append(float(d.mean()))
                    inside = max(inside, float(d[ja:jb, ia:ib].max()))
                ok = max(mads) <= MAD_MAX and worst <= PX_MAX
                print(f"picture {area}/{eid}.{st} x{scale}: mad {max(mads):.2e} max {worst:.2e} (picture rect max {inside:.2e}) "
                      f"{'PASS' if ok else 'FAIL'}", flush=True)
                if not ok:
                    bad.append((eid, st, max(mads), worst))
        assert not bad, bad

    @need_install
    def test_game_negctl_picture_fields_read_red(glib, game_records):
        if glib.exe_load():
            pytest.skip("no exe image")
        i, area, eid, st, items, keep, rec, _t, _h = _picture_records(game_records)[0]
        glib.negctl("uvgame.render", 6)
        try:
            with glib.state() as s:
                s.fill_from_exe()
                s.files_root(install)
                assert s.call("uvgame.prepare", [i]).eax == 1
                prem, x0, y0 = g_render(glib, i, 1.0, s, keep=1)
        finally:
            glib.negctl_reset()
        h, w = prem.shape[:2]
        ref, (ja, jb, ia, ib) = _picture_ref(items, x0, y0, 1.0, 0.0, h, w)
        worst = float(np.abs(prem[..., :3] - ref)[ja:jb, ia:ib].max())
        print(f"uvgame 6: picture rect max {worst:.1f} levels", flush=True)
        assert worst > PX_MAX

    return {k: v for k, v in locals().items() if k.startswith("test_") or k in ("glib", "game_records")}
