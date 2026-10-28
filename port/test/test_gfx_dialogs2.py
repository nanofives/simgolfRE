"""Roadmap S2.11: the dialogs drawn over the course, part 2: the TOURNAMENT RESULTS strip (showTournamentResults), the bulletin
board (showBulletinBoard), and narrateShot's SimFoto frame with the message ball (drawWindow with a golfer, drawMessageBuffer).

Bodies: family `results` (port/src/sim/gfx_results.cpp: the marker results.begin), family `simfoto` (gfx_simfoto.cpp:
loadInterfaceArt.ballArt, the marker simfoto.begin, the probe of drawWindow's golfer arm), family `text` (gfx_text.cpp, roadmap
S3.7: drawWindow.frame with the golfer arm and drawStringRightAt, the only bodies of those names since roadmap S3.13; S2.11's
drawWindow.golfer and results' drawStringRightAt were folded into them, controls moved), family `bulletin` (gfx_bulletin.cpp:
loadInterfaceArt.bulletinArt, showBulletinBoard.board / .event / .photo / .fixed); the screens' calls are in tick_panels.inc,
commands.cpp / commands_layout.inc, shots.cpp (narrateShot), dialogs.cpp (showBulletinBoard) and menu.cpp. Notes:
log/roadmap/S2/11_notes.md.

The course under a dialog is not drawn yet (roadmap S3), so each dialog is judged in its own rectangle, measured on the reference
frame (log/c3/alt/refs/<scenario>/det_b/<checkpoint>.png) by log/roadmap/S2/11_tools/rects.py:
  - the SimFoto frame: the bounding box of the 4-connected region of its border colour 0x7fff after a 5 x 5 opening (which
    removes white label text touching the border and keeps the border whole: a solid 16-pixel ring, so the box
    is the frame's outer rectangle);
  - the message ball's frame: the outline colour RGB (206, 206, 231) of drawFrameR3's border pieces gives the top and bottom
    lines; the frame's rectangle is (x0 - 10, top, x1 + 7, bottom + 2) with x0 / x1 the lines' extent, the insets of the outline
    inside drawFrameR3's rectangle (measured once on tour_0180, whose drawWindow arguments (0x96, 0xef, -2) and H 0x60 give the
    frame (170, 275) .. (585, 386): the outline lies at x 180..578, rows 275 and 384);
  - the results strip and the bulletin board: the whole frame (the strip is modal, nothing under it is redrawn after the marker;
    the board's image covers the whole screen).
HELD pixels: the family's probe (negctl modes >= 0x10000, a measurement) fills the rectangle a dialog covers with a probe
pattern when it starts (the whole root for the results strip); a pixel is judged when three probe runs (0x0000, 0x7fff, a
pseudo-random pattern) and the run without a probe agree on it. The ball's square is painted by the probe after the head
drawThoughtBubble draws there (Surface_drawScaled of a heads cut, roadmap S3.6; its palette composeGolferSprite's, S3.5; drawn
since roadmap S3.13 and judged by port/test/test_gfx_text.py's frames), so the whole square is held here. The gate: every
judged pixel of every rectangle equals the reference.
"""
from __future__ import annotations

import numpy as np
import pytest

from port.test import gfx as GX
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

FULL = (0, 0, 799, 599)
# (x0, y0, x1, y1) inclusive, measured on the reference frames (module docstring; log/roadmap/S2/11_rects_measure.log)
CASES = {
    "cov_b_shots": {"results": [FULL], "results_r2": [FULL]},
    "cov_b_shots2": {"results": [FULL], "results_r2": [FULL], "results_r3": [FULL]},
    "championship": {"tour_0180": [(505, 102, 657, 254), (170, 275, 585, 386)]},   # SimFoto, the ball's frame
    "ui_tour": {"menus": [(531, 291, 754, 474), (260, 26, 675, 168)]},
    "cov_k1_animals": {"bought": [(395, 235, 617, 418), (260, 26, 675, 168)]},
    "cov_b_inject2": {"debug_overlay": [(376, 241, 599, 423), (260, 26, 675, 168)]},
    "cov_k2_simfoto": {"bottom": [(288, 484, 511, 599)], "right": [(684, 208, 799, 391)]},
    "cov_a_golfer": {"button_2": [(215, 191, 517, 454)]},
    "cov_r_s2_screens": {"bulletin": [FULL]},
}
# the family whose probe holds the pixels a scenario's dialogs leave to what lies under them (None: no probe needed)
PROBE_FAMILY = {"cov_b_shots": "gfx.family.results", "cov_b_shots2": "gfx.family.results", "cov_r_s2_screens": None}
DEFAULT_PROBE = "gfx.family.simfoto"
PROBES = (0x10000, 0x17FFF, 0x20001)   # black, white, a pseudo-random pattern
# not rendered: championship tournament_end has no snapshot (sg_run_boot returns -11 at the boot's end, U-1120: roadmap S3.3)
NOT_RUN = {("championship", "tournament_end"): "U-1120 (S3.3): sg_run_boot returns -11 at the end of the championship boot"}


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has("gfx.family.simfoto"):
        pytest.skip("the library has no simfoto family")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _have(scen):
    return all((GX.REFS / scen / "det_b" / f"{c}.png").exists() for c in CASES[scen]) and SG.INSTALL.is_dir()


def render(lib, scen, mode=0, name="gfx.attach"):
    return GX.render(scen, list(CASES[scen]), lib, negctl=mode, negctl_name=name)


def measure(lib, scen, normal=None) -> dict:
    """{checkpoint: [ {rect, px, opaque, fail, held, held_differ} ]}: the run without a probe against the reference, the
    pixels the probe runs agree on judged."""
    normal = normal or render(lib, scen)
    fam = PROBE_FAMILY.get(scen, DEFAULT_PROBE)
    probes = [render(lib, scen, m, fam) for m in PROBES] if fam else []
    out = {}
    for c, rects in CASES[scen].items():
        img = normal.frames.get(c)
        if img is None or any(p.frames.get(c) is None for p in probes):
            out[c] = None
            continue
        ref = GX.reference(scen, c)
        rows = []
        for (x0, y0, x1, y1) in rects:
            sl = np.s_[y0:y1 + 1, x0:x1 + 1]
            opaque = np.ones(img[sl].shape[:2], bool)
            for p in probes:
                opaque &= (p.frames[c][sl] == img[sl]).all(2)
            differ = (img[sl] != ref[sl]).any(2)
            rows.append({"rect": (x0, y0, x1, y1), "px": int(opaque.size), "opaque": int(opaque.sum()),
                         "fail": int((opaque & differ).sum()), "held": int((~opaque).sum()),
                         "held_differ": int((~opaque & differ).sum())})
        out[c] = rows
    return out


def rect_diff(run, scen, c) -> int:
    """pixels of all the checkpoint's rectangles that differ from the reference"""
    img, ref = run.frames.get(c), GX.reference(scen, c)
    if img is None:
        return -1
    return int(sum((img[y0:y1 + 1, x0:x1 + 1] != ref[y0:y1 + 1, x0:x1 + 1]).any(2).sum() for x0, y0, x1, y1 in CASES[scen][c]))


_RUNS: dict = {}


def _normal(lib, scen):
    if scen not in _RUNS:
        _RUNS[scen] = render(lib, scen)
    return _RUNS[scen]


_MEAS: dict = {}


@pytest.fixture(scope="module")
def measured(glib):
    def get(scen):
        if not _have(scen) or not glib.has("gditext.attach"):
            pytest.skip("no pinned reference / install / host text provider")
        if scen not in _MEAS:
            _MEAS[scen] = measure(glib, scen, _normal(glib, scen))
        return _MEAS[scen]
    return get


# ------------------------------------------------------------------------------------------------ the frame gate
ALL = [(s, c) for s in CASES for c in CASES[s]]


@pytest.mark.parametrize("scen,chk", ALL)
def test_dialog_rectangles_equal_the_original_outside_held_pixels(measured, scen, chk):
    rows = measured(scen)[chk]
    assert rows is not None, "no snapshot"
    for r in rows:
        assert r["fail"] == 0, r
        assert r["opaque"] > 0, r


def test_bulletin_board_judges_every_pixel(measured):
    (row,) = measured("cov_r_s2_screens")["bulletin"]
    assert row["held"] == 0 and row["fail"] == 0, row


def test_tournament_end_not_judged_yet(glib):
    """championship tournament_end: on this step's base the boot ends with -11 before the snapshot (U-1120, fixed by roadmap
    S3.3 in b3d61e41). Once a snapshot exists it is not judged here: its screen (the results strip in the layout this checkpoint
    reaches) has no measured rectangle yet (U-1218)"""
    if not _have("championship"):
        pytest.skip("no reference")
    run = GX.render("championship", ["tour_0180", "tournament_end"], glib)
    if run.frames.get("tournament_end") is not None:
        pytest.skip("tournament_end has a snapshot (U-1120 fixed): not judged yet, U-1218")
    assert run.frames.get("tournament_end") is None


def test_bodies_handled(glib):
    if not _have("ui_tour"):
        pytest.skip("no reference")
    run = _normal(glib, "ui_tour")
    for name in ("loadInterfaceArt.ballArt", "drawWindow.frame", "simfoto.begin", "loadInterfaceArt.bulletinArt"):
        assert f"handled\t{name}\t" in run.log, name
    # drawWindow's golfer arm (the text family's drawWindow.frame since roadmap S3.13) draws the head itself, so no part of it
    # is reported; that the arm ran on ui_tour is read by the controls text 12 / 13 below (RED on ui_tour menus)
    for bad in ("loadInterfaceArt.ballArt", "loadInterfaceArt.bulletinArt", "drawWindow", "drawThoughtBubble",
                "gfx_text: no body", "bulletin: no body"):
        assert not any(l.startswith("unsupported\t" + bad) for l in run.log.splitlines()), bad


def test_results_bodies_handled(glib):
    if not _have("cov_b_shots"):
        pytest.skip("no reference")
    run = _normal(glib, "cov_b_shots")
    for name in ("results.begin", "drawStringRightAt"):
        assert f"handled\t{name}\t" in run.log, name
    assert not any(l.startswith("unsupported\tdrawStringRightAt") for l in run.log.splitlines())


# ------------------------------------------------------------------------------------------------ controls on the frames
# (family, mode, scenario, checkpoint, what): RED = the checkpoint's rectangles differ from the reference in more pixels than
# the run without the control
FRAME_NEGCTL = [
    ("gfx.family.text", 15, "cov_b_shots", "results", "drawStringRightAt ends one pixel right of x - run (was results 1)"),
    ("gfx.family.results", 2, "cov_b_shots", "results", "the rows one pixel further apart (the band heights + 1)"),
    ("gfx.family.text", 12, "ui_tour", "menus", "the message text one pixel lower (was simfoto 2)"),
    ("gfx.family.text", 13, "ui_tour", "menus", "the ball's frame one pixel to the left (was simfoto 3)"),
    ("gfx.family.text", 14, "championship", "tour_0180", "the text offset for a window under 0x60 ignored (was simfoto 4)"),
    ("gfx.family.bulletin", 1, "cov_r_s2_screens", "bulletin", "the board image one pixel lower"),
    ("gfx.family.bulletin", 2, "cov_r_s2_screens", "bulletin", "the trophy one pixel to the right"),
    ("gfx.family.bulletin", 3, "cov_r_s2_screens", "bulletin", "the notes' x without the s / 2 term"),
    ("gfx.family.bulletin", 4, "cov_r_s2_screens", "bulletin", "the TableRow cuts read the w / h one entry later"),
]


@pytest.mark.parametrize("fam,mode,scen,chk,what", FRAME_NEGCTL)
def test_frames_negctl_red(glib, fam, mode, scen, chk, what):
    if not _have(scen):
        pytest.skip("no reference")
    base = rect_diff(_normal(glib, scen), scen, chk)
    run = GX.render(scen, [chk], glib, negctl=mode, negctl_name=fam)
    assert rect_diff(run, scen, chk) > base, (fam, mode, what)


# ------------------------------------------------------------------------------------------------ the ball's cut
# The ball's square is held on every frame (the probe paints it after the head), so the cut (simfoto mode 1: one row lower) is
# read on the pixels of the square that equal the reference in the run without a probe: the ring the head leaves visible. tour_0180's ball lies at (128, 243)
# .. 0x8c x 0x8c (drawWindow(0x96, 0xef, .., -2): ex - 0x32, ey - 0x28 with ex = 0x96 + 0x1c, ey = 0xef + 0x2c).
BALL = (128, 243, 128 + 0x8C - 1, 243 + 0x8C - 1)


def _ball_equal(run) -> int:
    img, ref = run.frames.get("tour_0180"), GX.reference("championship", "tour_0180")
    x0, y0, x1, y1 = BALL
    return int((img[y0:y1 + 1, x0:x1 + 1] == ref[y0:y1 + 1, x0:x1 + 1]).all(2).sum())


def test_ball_cut_negctl_red(glib):
    if not _have("championship"):
        pytest.skip("no reference")
    good = _ball_equal(_normal(glib, "championship"))
    bad = _ball_equal(GX.render("championship", ["tour_0180"], glib, negctl=1, negctl_name="gfx.family.simfoto"))
    # the ring outside the head equals the reference with the right cut (most of the square's 19,600 pixels outside the
    # head); one row lower moves every ring pixel
    assert good > bad and good > 5000, (good, bad)


def main(argv=None) -> int:
    """print the gate table for every case: ~/bin/py -3.12 -m port.test.test_gfx_dialogs2 [out.tsv]"""
    import sys
    argv = sys.argv[1:] if argv is None else argv
    lib = L.load()
    if not lib.exe_loaded():
        lib.exe_load()
    lines = ["scenario\tcheckpoint\trect\tpx\tjudged\tfail\theld\theld_differ"]
    for scen in CASES:
        if not _have(scen):
            continue
        m = measure(lib, scen)
        for c, rows in m.items():
            if rows is None:
                lines.append(f"{scen}\t{c}\tNO_SNAPSHOT\t\t\t\t\t")
                continue
            for r in rows:
                lines.append(f"{scen}\t{c}\t{r['rect']}\t{r['px']}\t{r['opaque']}\t{r['fail']}\t{r['held']}\t{r['held_differ']}")
        print("\n".join(lines[-len(m) * 2:]), flush=True)
    txt = "\n".join(lines) + "\n"
    if argv:
        open(argv[0], "w", encoding="utf-8").write(txt)
    print(txt)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
