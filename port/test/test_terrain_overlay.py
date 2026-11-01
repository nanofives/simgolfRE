"""Terrain::drawLine's antialiased overlay line (roadmap S3.8; port/include/portsim/sim/terrain_overlay.h, notes
log/roadmap/S3/8_notes.md) against Windows' generic OpenGL.

PROBES: synthetic scenes of the original's exact drawLine call sequence over the context state Terrain::initGL leaves (GL_LINE_SMOOTH,
GL_NICEST), run through the REAL software GL (re/tools/glexact/glreplay.exe, which writes the DIB it drew; port/test/glprobe.py's
method) and through the port's overlay.rasterLine; compared pixel by pixel on the 5-bit channels. Each scene's counts are pinned
(the port's residual against the real GL, U-1370); the negative controls of overlay.rasterLine must break G2 on a scene that
exercises them. Without glreplay.exe (a local build, re/tools/glexact/build.bat) the probe tests skip.

FRAMES (opt-in, SG_S38_FRAMES=1, about a minute): framegate boots of pinned scenarios with the overlay on and with negative control 10
(every overlay body draws nothing); the pixels the overlay changes are judged against the original's checkpoint frames. The full
census numbers are in log/roadmap/S3/8_linegate.tsv (log/roadmap/S3/8_census.sh).

    ~/bin/py -3.12 -m pytest port/test/test_terrain_overlay.py -q
"""
from __future__ import annotations

import ctypes as C
import os
import pathlib
import struct
import tempfile

import numpy as np
import pytest

from port.test import glprobe as P
from port.test import lib as L

pytestmark = pytest.mark.port

W, H = 800, 600
BUF = 0x40000000
INIT = ["glHint 0xc50,0x1101", "glEnable 0xb20", "glHint 0xc52,0x1102", "glEnable 0xde1", "glEnable 0xb50", "glFrontFace 0x900",
        "glEnable 0xb44", "glViewport 0x0,0x0,0x320,0x258"]
G2_ONE_PCT, G2_BEYOND = 1.5, 70


# ------------------------------------------------------------------------------------------------ scenes
def _fbits(x: float) -> str:
    return "0x%x" % struct.unpack("<I", struct.pack("<f", x))[0]


def _f32(v) -> float:
    return float(np.float32(v))


def draw_line_calls(x1, y1, x2, y2, colour, width, alpha, flip=True) -> list[str]:
    """Terrain::drawLine's GL calls (re/analysis/terrain/000048a0_TerrainDll_Terrain_drawLine.md) in the recorder's line form"""
    ch = [_f32(np.float32(v) / np.float32(255.0)) for v in ((colour >> 7) & 0xF8, (colour >> 2) & 0xF8, (colour << 3) & 0xF8)]
    o = [P.db(0), P.db(W), P.db(0), P.db(H)] if flip else [P.db(0), P.db(W), P.db(H), P.db(0)]
    return ["glMatrixMode 0x1701", "glPushMatrix", "glLoadIdentity", "glOrtho " + ",".join(o + [P.db(-1), P.db(1)]),
            "glMatrixMode 0x1700", "glPushMatrix", "glLoadIdentity", "glDisable 0xb50", "glDisable 0xde1", "glEnable 0xbe2",
            "glBlendFunc 0x302,0x303", "glLineWidth " + _fbits(float(width)),
            "glColor4f " + ",".join(_fbits(v) for v in ch + [_f32(np.float32(alpha) / np.float32(10.0))]),
            "glBegin 0x1", "glVertex2i 0x%x,0x%x" % (x1 & 0xFFFFFFFF, y1 & 0xFFFFFFFF),
            "glVertex2i 0x%x,0x%x" % (x2 & 0xFFFFFFFF, y2 & 0xFFFFFFFF), "glEnd",
            "glEnable 0xb50", "glEnable 0xde1", "glDisable 0xbe2", "glPopMatrix", "glMatrixMode 0x1701", "glPopMatrix",
            "glMatrixMode 0x1700", "glFlush"]


COLOURS = [0x0, 0x7FFF, 0x6318, 0x23E8, 0x7F6F, 0x7F9C, 0x6090, 0x80000000, 0x80007FF0, 0x800003FF]


def scene(name: str):
    """(lines, pre): lines (x1, y1, x2, y2, colour, width, alpha, flip) in call order, pre the DIB before them"""
    if name == "rand":       # long lines anywhere, crossing the edges, every width, colour and alpha the game passes
        rng = np.random.default_rng(5)
        ln = []
        for _ in range(150):
            x1, y1 = rng.integers(0, 800), rng.integers(0, 600)
            x2, y2 = x1 + rng.integers(-150, 150), y1 + rng.integers(-150, 150)
            ln.append((int(x1), int(y1), int(x2), int(y2), int(rng.choice(COLOURS[:7])), int(rng.choice([1, 2, 3, 5, 10])),
                       int(rng.choice([3, 4, 5, 7, 10])), True))
        return ln, rng.integers(0, 32768, size=(H, W)).astype("<u2")
    if name == "widths":     # widths 1..10 at nine angles, white on black at alpha 1
        ln = []
        for k, (w, ang) in enumerate((w, a) for w in range(1, 11) for a in (0, 15, 30, 40, 45, 50, 60, 75, 90)):
            cx, cy, r = 40 + (k % 10) * 78, 40 + (k // 10) * 62, 24
            c, s = np.cos(np.radians(ang)), np.sin(np.radians(ang))
            ln.append((int(round(cx - r * c)), int(round(cy - r * s)), int(round(cx + r * c)), int(round(cy + r * s)), 0x7FFF, w, 10, True))
        return ln, np.zeros((H, W), dtype="<u2")
    if name == "short":      # zero-length and 1..3 pixel lines; widths above the clamp (12..30 draw as 10)
        ln = []
        k = 0
        for w in (1, 2, 3, 10, 12, 15, 20, 30):
            for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1), (2, 1), (3, 0), (0, 2)):
                cx, cy = 30 + (k % 14) * 55, 30 + (k // 14) * 70
                k += 1
                ln.append((cx, cy, cx + dx, cy + dy, 0x7FFF, w, 10, True))
        return ln, np.zeros((H, W), dtype="<u2")
    if name == "flip":       # the other projection (flip byte clear: glOrtho(0, W, H, 0)), alphas 0 and above 10 (clamped)
        rng = np.random.default_rng(23)
        ln = []
        for _ in range(60):
            x1, y1 = rng.integers(-50, 850), rng.integers(-50, 650)
            x2, y2 = x1 + rng.integers(-120, 120), y1 + rng.integers(-120, 120)
            ln.append((int(x1), int(y1), int(x2), int(y2), int(rng.choice(COLOURS)), int(rng.choice([1, 2, 3, 10])),
                       int(rng.choice([0, 5, 10, 15, 30])), bool(rng.integers(0, 2))))
        return ln, rng.integers(0, 32768, size=(H, W)).astype("<u2")
    raise KeyError(name)


SCENES = ["rand", "widths", "short", "flip"]
# S3.8's model (line rule 0, control 11) against the real GL per scene: (pixels the real GL changed, pixels differing, of them one
# level, beyond one level) (measured 2026-10-25 on x64 and Win32; the differences are the footprint-edge pixels of U-1370, which the
# span rule of S3.24 removes, and one-level rounding)
PINNED: dict[str, tuple[int, int, int, int]] = {
    "rand": (74466, 108, 98, 10), "widths": (27689, 88, 48, 40), "short": (744, 1, 0, 1), "flip": (18261, 18, 16, 2)}


# ------------------------------------------------------------------------------------------------ runs
_REAL: dict = {}


def real_frame(name: str) -> np.ndarray:
    if name in _REAL:
        return _REAL[name]
    if not P.REPLAY.exists():
        pytest.skip("re/tools/glexact/glreplay.exe not built (re/tools/glexact/build.bat)")
    lines, pre = scene(name)
    sc = P.Scene()
    sc.state += INIT
    for ln in lines:
        sc.pass_ += draw_line_calls(*ln)
    d = sc.write("s38t_" + name)
    (d / "snaps" / "s1" / "dib_pre.bin").write_bytes(pre.astype("<u2").tobytes())
    _REAL[name] = P.real_frame(d).reshape(H, W)
    return _REAL[name]


def port_frame(lib, name: str, mode: int = 0, context: bool = True, rule: int | None = None) -> np.ndarray:
    """the port's frame of a scene; rule = the overlay line rule for the call (None: the build's default)"""
    lines, pre = scene(name)
    prev = lib.negctl("overlay.rasterLine", mode)
    try:
        with lib.state() as st:
            if rule is not None:
                old_rule = st.call("overlay.setRule", [rule]).eax
            buf = (C.c_uint16 * (W * H))(*pre.reshape(-1).tolist())
            st.map(BUF, buf)
            wbits = 0
            for x1, y1, x2, y2, c, w, a, flip in lines:
                r = st.call("overlay.rasterLine", [BUF, W, H, x1, y1, x2, y2, c, w, a, (1 if flip else 0) | (0 if context else 2), wbits])
                wbits = r.edx
            if rule is not None:
                st.call("overlay.setRule", [old_rule])
            return np.frombuffer(bytes(buf), dtype="<u2").reshape(H, W).copy()
    finally:
        lib.negctl("overlay.rasterLine", prev or 0)


def compare(real: np.ndarray, pre: np.ndarray, port: np.ndarray) -> tuple[int, int, int, int]:
    r5 = np.stack([(real >> s) & 31 for s in (10, 5, 0)]).astype(int)
    p5 = np.stack([(port >> s) & 31 for s in (10, 5, 0)]).astype(int)
    d = np.abs(r5 - p5).max(axis=0)
    return int((real != pre).sum()), int((d > 0).sum()), int((d == 1).sum()), int((d > 1).sum())


def g2(changed: int, one: int, beyond: int) -> bool:
    return one <= G2_ONE_PCT / 100.0 * changed and beyond <= G2_BEYOND


@pytest.fixture(scope="module")
def lib():
    lb = L.load()
    if not lb.has("overlay.rasterLine"):
        pytest.skip("this build has no module terrain_overlay")
    return lb


# ------------------------------------------------------------------------------------------------ probes
@pytest.mark.parametrize("name", SCENES)
def test_probe_meets_g2_and_pinned(lib, name):
    real = real_frame(name)
    _lines, pre = scene(name)
    got = compare(real, pre, port_frame(lib, name))
    changed, differ, one, beyond = got
    assert changed > 0
    assert g2(changed, one, beyond), got
    assert got == PINNED_SPAN[name], (got, PINNED_SPAN[name])     # the build's default line rule is the span rule (S3.24)


@pytest.mark.parametrize("name", SCENES)
def test_old_rule_keeps_s38_counts(lib, name):
    """rule 0 (and control 11) is S3.8's model: its pinned counts are unchanged"""
    real = real_frame(name)
    _lines, pre = scene(name)
    assert compare(real, pre, port_frame(lib, name, rule=OLD_RULE)) == PINNED[name]
    assert compare(real, pre, port_frame(lib, name, mode=11)) == PINNED[name]


# ------------------------------------------------------------------------------------------------ the span rule (roadmap S3.24)
# overlay.setRule: 0 = S3.8's model (PINNED above); 2 = the per-column span: N = floor(w * L / |major|) + 2 pixels per column of
# the major axis, the pixel centres whose minor offset s = o * sign (x-major: sign(dx) * sign(dy), y-major: sign(dx)) lies in
# [-N/2, N/2) (log/roadmap/S3/24_notes.md); 6 = the same span without a tie decision (|o| <= N/2 + 0.002). Counts per scene (same
# columns as PINNED); measured with the C++ body (log/roadmap/S3/24_scenes.py's Python model gives rand 8 for rule 2: it does not clip
# the ends to the viewport before the span test)
SPAN_RULE, NOTIE_RULE, OLD_RULE = 2, 6, 0
PINNED_SPAN: dict[str, tuple[int, int, int, int]] = {
    "rand": (74466, 7, 6, 1), "widths": (27689, 12, 6, 6), "short": (744, 0, 0, 0), "flip": (18261, 5, 3, 2)}
PINNED_NOTIE: dict[str, tuple[int, int, int, int]] = {
    "rand": (74466, 9, 8, 1), "widths": (27689, 12, 6, 6), "short": (744, 0, 0, 0), "flip": (18261, 5, 3, 2)}


def wrong(real: np.ndarray, port: np.ndarray) -> np.ndarray:
    r5 = np.stack([(real >> s) & 31 for s in (10, 5, 0)]).astype(int)
    p5 = np.stack([(port >> s) & 31 for s in (10, 5, 0)]).astype(int)
    return (r5 != p5).any(axis=0)


@pytest.mark.parametrize("rule,pinned", [(SPAN_RULE, PINNED_SPAN), (NOTIE_RULE, PINNED_NOTIE)])
@pytest.mark.parametrize("name", SCENES)
def test_span_rule_pinned(lib, name, rule, pinned):
    real = real_frame(name)
    _lines, pre = scene(name)
    assert compare(real, pre, port_frame(lib, name, rule=rule)) == pinned[name]


@pytest.mark.parametrize("rule", [SPAN_RULE, NOTIE_RULE])
@pytest.mark.parametrize("name", SCENES)
def test_span_rule_fewer_and_none_new(lib, name, rule):
    """every pixel the span rule gets wrong was wrong under S3.8's model too, and it gets fewer wrong"""
    real = real_frame(name)
    old, new = wrong(real, port_frame(lib, name, rule=OLD_RULE)), wrong(real, port_frame(lib, name, rule=rule))
    assert not (new & ~old).any(), int((new & ~old).sum())
    assert new.sum() < old.sum() or old.sum() == 0


def test_span_rule_control_old_rule_reads_red(lib):
    """control 11 (S3.8's footprint back) breaks the default's pinned counts on every scene with a cut"""
    for name in ("rand", "widths", "flip"):
        real = real_frame(name)
        _lines, pre = scene(name)
        assert compare(real, pre, port_frame(lib, name, mode=11)) != PINNED_SPAN[name]
        assert compare(real, pre, port_frame(lib, name, rule=OLD_RULE)) != PINNED_SPAN[name]


# control mode -> the scene that must break G2 with it
CONTROLS = {1: "rand", 2: "rand", 3: "rand", 4: "flip", 5: "rand", 8: "short"}


@pytest.mark.parametrize("mode", sorted(CONTROLS))
def test_negctl_reads_red(lib, mode):
    name = CONTROLS[mode]
    real = real_frame(name)
    _lines, pre = scene(name)
    changed, differ, one, beyond = compare(real, pre, port_frame(lib, name, mode))
    if mode == 5:
        # no snap: with the span rule the scene stays 0.04 % inside G2 (1,085 one-level px of 74,466), so the control reads RED
        # on its distance from the default's pinned count instead (more than 100 times the default's differing pixels)
        assert differ > 100 * PINNED_SPAN[name][1], (mode, changed, differ, one, beyond)
        return
    assert not g2(changed, one, beyond), (mode, changed, one, beyond)


def test_no_context_draws_nothing(lib):
    """Without a current GL context every drawLine call is ignored (the location screen's drawPanelBackdrop bar, U-1071)"""
    _lines, pre = scene("rand")
    assert (port_frame(lib, "rand", context=False) == pre).all()
    assert (port_frame(lib, "rand", mode=9, context=False) != pre).any()   # control 9: drawn without a context


def test_line_width_outlasts_the_call(lib):
    """glLineWidth(w <= 0) is GL_INVALID_VALUE: the previous width stays (it is returned in edx as float bits)"""
    with lib.state() as st:
        buf = (C.c_uint16 * (W * H))()
        st.map(BUF, buf)
        ten = struct.unpack("<I", struct.pack("<f", 10.0))[0]
        r = st.call("overlay.rasterLine", [BUF, W, H, 100, 100, 200, 100, 0x7FFF, 0, 10, 1, ten])
        r10 = st.call("overlay.rasterLine", [BUF, W, H, 100, 100, 200, 100, 0x7FFF, 10, 10, 1, 0])   # the same pixels again
        assert r.edx == ten and r.eax == r10.eax > 1000   # width 0 draws what width 10 draws
        r = st.call("overlay.rasterLine", [BUF, W, H, 100, 300, 200, 300, 0x7FFF, 3, 10, 1, ten])
        assert r.edx == struct.unpack("<I", struct.pack("<f", 3.0))[0]


# ------------------------------------------------------------------------------------------------ frames (opt-in)
FRAME_CASES = [("cov_a_keys", "holes"), ("cov_a_keys", "k_dlg_R"), ("cov_k1_routing", "trees"), ("cov_a_keys", "k_render_dot")]


FRAMES_ON = os.environ.get("SG_S38_FRAMES") == "1"
_FRAMES: dict = {}


def frame(lib, scenario: str, checkpoint: str, mode: int) -> np.ndarray:
    """the port's checkpoint frame (5-bit channels) with overlay control `mode` (0 = the correct overlay)"""
    key = (scenario, checkpoint, mode)
    if key not in _FRAMES:
        from PIL import Image
        from port.test import framegate as FG
        d = pathlib.Path(tempfile.mkdtemp(prefix=f"s38_{mode}_"))
        FG.run_scenario(scenario, [checkpoint], lib=lib, negctl=("overlay.rasterLine", mode) if mode else None, images=d, probe=False)
        _FRAMES[key] = np.asarray(Image.open(d / f"{scenario}.{checkpoint}.port.png").convert("RGB")).astype(int) >> 3
    return _FRAMES[key]


def judge_frame(lib, scenario: str, checkpoint: str, mode: int) -> tuple[int, int, int]:
    """the pixels the overlay touches (with the correct body or control `mode`, against control 10 = nothing drawn), judged
    against the original's checkpoint frame: (pixels, one level, beyond)"""
    from port.test import gfx as GX
    off, on, m = frame(lib, scenario, checkpoint, 10), frame(lib, scenario, checkpoint, 0), frame(lib, scenario, checkpoint, mode)
    ref = GX.reference(scenario, checkpoint).astype(int) >> 3
    line = (on != off).any(axis=2) | (m != off).any(axis=2)
    d = np.abs(m - ref).max(axis=2)
    return int(line.sum()), int(((d == 1) & line).sum()), int(((d > 1) & line).sum())


@pytest.mark.skipif(not FRAMES_ON, reason="opt-in: SG_S38_FRAMES=1 (pinned boots, a few minutes)")
@pytest.mark.parametrize("scenario,checkpoint", FRAME_CASES)
def test_frame_line_pixels(lib, scenario, checkpoint):
    """The pixels the overlay draws in a pinned checkpoint frame (overlay on against control 10) meet G2 against the original"""
    n, one, beyond = judge_frame(lib, scenario, checkpoint, 0)
    assert n > 0
    assert g2(n, one, beyond), (n, one, beyond)


# frame controls: 6 lineTo keeps the old line point (the debug view's path chains; on k_dlg_R it is an equivalent mutant: its holes
# have no waypoint, one lineTo each), 7 drawCircle's segments
# 3 wide (the debug view's range rings)
FRAME_CONTROLS = {6: ("cov_a_keys", "k_render_dot"), 7: ("cov_a_keys", "k_render_dot")}


@pytest.mark.skipif(not FRAMES_ON, reason="opt-in: SG_S38_FRAMES=1 (pinned boots, a few minutes)")
@pytest.mark.parametrize("mode", sorted(FRAME_CONTROLS))
def test_frame_negctl_reads_red(lib, mode):
    n, one, beyond = judge_frame(lib, *FRAME_CONTROLS[mode], mode)
    assert not g2(n, one, beyond), (mode, n, one, beyond)
