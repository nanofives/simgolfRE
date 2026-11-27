"""P41 feedback 1, item 8 (log/roadmap/P41/ITEM8_BRIEF.md, notes log/roadmap/P41/feedback1/item8_notes.md): the build palette's
building thumbnails drawn from the world art (portsim/sim/gfx_bthumb.h, port/src/sim/gfx_bthumb.cpp).

  * TABLE: port/src/sim/gfx_bthumb_table.inc is what log/roadmap/P41/feedback1/item8/gen_table.py writes from fit_all.tsv and
    parts.py: every course type x slot x level except the driving range (slot 4, kept as art), each with 1..6 parts, a positive
    scale, and the palette tag buildSceneSprites draws the object with (level 0: the case's tag; level 1: type + 0x90).
  * DLL: the library's bthumb.entries equals the table's entry count; bthumb.prepared is 0 in a fresh state (nothing composed
    before a theme-on draw).
The in-game pictures (theme off LEFT, on RIGHT at 1334 x 750, 1920 x 1080, 4K) are judged by eye: log/roadmap/P41/feedback1/item8/.
"""
from __future__ import annotations

import pathlib
import re
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
ITEM8 = ROOT / "log" / "roadmap" / "P41" / "feedback1" / "item8"
INC = ROOT / "port" / "src" / "sim" / "gfx_bthumb_table.inc"
ROW = re.compile(r"^\s*\{(\d), (\d), (\d), ([\d.]+)f, (-?[\d.]+)f, (-?[\d.]+)f, (0x[0-9a-f]+), (\d), \{(.*)\}\},", re.M)


def _rows():
    return [m.groups() for m in ROW.finditer(INC.read_text())]


@pytest.mark.static
def test_table_covers_every_slot_but_the_driving_range():
    rows = _rows()
    keys = {(int(t), int(s), int(v)) for t, s, v, *_ in rows}
    want = {(t, s, v) for t in range(4) for s in range(9) if s != 4 for v in (0, 1)}
    assert keys == want and len(rows) == len(want)
    tag0 = {0: 0xb1, 1: 0x10, 2: 0x0e, 3: 0x1c, 5: 0x17, 6: 0x1b, 7: 0x11, 8: 0x12}
    for t, s, v, scale, ox, oy, tag, n, parts in rows:
        s, v = int(s), int(v)
        assert 0.1 < float(scale) < 1.5
        assert 1 <= int(n) <= 6 and len(re.findall(r"\{0x", parts)) == int(n)
        want_tag = tag0[s] if (v == 0 or s == 0) else s + 6 + 0x90
        assert int(tag, 16) == want_tag, (t, s, v)


@pytest.mark.static
def test_table_is_the_generator_output():
    if not (ITEM8 / "fit_all.tsv").exists():
        pytest.skip("the fits (log/roadmap/P41/feedback1/item8/fit_all.tsv) are not in this checkout")
    before = INC.read_text()
    sys.path.insert(0, str(ITEM8))
    try:
        import gen_table  # noqa: WPS433
        gen_table.main()
        assert INC.read_text() == before
    finally:
        INC.write_text(before)


@pytest.mark.port
def test_dll_entries(portlib):
    if not portlib.has("bthumb.entries"):
        pytest.skip("the library has no gfx_bthumb module")
    with portlib.state() as s:
        assert s.call("bthumb.entries", []).eax == len(_rows())
        assert s.call("bthumb.prepared", []).eax == 0
