"""S6.5: the `fixed`-profile switches of replaceInText (D46) and generateRandomCourse (D52b1, D52b2).

Switch 1 (default) = what the port does today (the original's behaviour); 0 = the fixed behaviour. Facts about the
original: log/roadmap/S6/6_decisions.tsv rows D46, D52b1, D52b2 (Unicorn probes in log/roadmap/S6/6_probes/).
Run as one file per process: `pytest port/test/test_defects_gen.py`.
"""
from __future__ import annotations

import ctypes as C

import pytest

from port.test.test_events import _new_state, _set_text, _text, need_exe
from port.test.test_terrain_gen import _fixed_image, _put, port_call
import struct

pytestmark = pytest.mark.port

TAIL = "replaceInText.tailLimit"
SEARCH = "generateRandomCourse.searchBound"


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl("replaceInText", 0)
    yield
    portlib.defect_reset()
    portlib.negctl("replaceInText", 0)


def test_switches_registered_default_on(portlib):
    d = portlib.defects()
    for name, ref in ((TAIL, "model_defects#46"), (SEARCH, "model_defects#52")):
        assert name in d and d[name]["ref"] == ref and d[name]["on"] == 1


# ------------------------------------------------------------------------------------------------ D46
def _replace(lib, tail_len: int) -> bytes:
    """Text `ab[N]` + filler so that the tail from the match has tail_len characters; replace [N] with Bob."""
    text = b"ab[N]" + b"x" * (tail_len - 3)
    with _new_state(lib) as st:
        kb = (C.c_uint8 * 64)()
        rb = (C.c_uint8 * 64)()
        st.map(0x40000000, kb)
        st.map(0x40010000, rb)
        C.memmove(kb, b"[N]".ljust(64, b"\0"), 64)
        C.memmove(rb, b"Bob".ljust(64, b"\0"), 64)
        _set_text(st, lib, text)
        st.call("replaceInText", [0x40000000, 0x40010000])
        return _full_text(st, lib)


def _full_text(st, lib):
    from port.test.test_events import _off
    b = st.read(_off(lib, "g_textBuffer"), 0x1000)
    return b[:b.index(0)]


@need_exe
@pytest.mark.parametrize("tail_len", [0x10, 0x1FF])
def test_d46_completing_tails_equal_with_both_settings(portlib, tail_len):
    want = b"abBob" + b"x" * (tail_len - 3)
    assert _replace(portlib, tail_len) == want
    portlib.defect(TAIL, False)
    assert _replace(portlib, tail_len) == want


@need_exe
@pytest.mark.parametrize("tail_len", [0x200, 0x204, 0x300])
def test_d46_overflowing_tail(portlib, tail_len):
    """1 = the port copies the whole tail (as today); 0 = the replacement is refused and the text is untouched."""
    original = b"ab[N]" + b"x" * (tail_len - 3)
    assert _replace(portlib, tail_len) == b"abBob" + b"x" * (tail_len - 3)
    portlib.defect(TAIL, False)
    assert _replace(portlib, tail_len) == original


@need_exe
def test_d46_wrong_body_reads_red_on_the_completing_control(portlib):
    portlib.defect(TAIL, False)
    portlib.negctl("replaceInText", 1)  # the saved tail skips key's length minus one
    assert _replace(portlib, 0x1FF) != b"abBob" + b"x" * (0x1FF - 3)


# ------------------------------------------------------------------------------------------------ D52b2 and D52b1
@need_exe
def test_d52b2_clubhouse_window_without_a_spot(portlib):
    img = _fixed_image(portlib)
    _put(portlib, img, "g_tileFlags", struct.pack("<H", 0x8000) * 2500)
    eax, out = port_call(portlib, img, 3)
    assert out is None and eax == ("error", -10)  # NoReturn: the original never leaves the search
    portlib.defect(SEARCH, False)
    eax, out = port_call(portlib, img, 3)
    assert out is not None and not isinstance(eax, tuple), "the fixed search takes the window centre and finishes"


@need_exe
def test_d52b1_type_0x11_window_without_a_cell(portlib):
    img = _fixed_image(portlib, n=3, loc=1, setting=0, ctype=0, difficulty=3, relief=0)
    eax, out = port_call(portlib, img, 3)
    assert out is None and eax == ("error", -10)
    portlib.defect(SEARCH, False)
    eax, out = port_call(portlib, img, 3)
    assert out is not None and not isinstance(eax, tuple)
    # the same location with the clubhouse arm only (location 3) completes with the switch at 1: the -10 above is D52b1
    portlib.defect_reset()
    other = _fixed_image(portlib, n=3, loc=3, setting=0, ctype=0, difficulty=3, relief=0)
    eax, out = port_call(portlib, other, 3)
    assert out is not None and not isinstance(eax, tuple)


@need_exe
@pytest.mark.parametrize("kw", [dict(loc=1, setting=1, ctype=3, difficulty=7, relief=0),
                                dict(loc=1, setting=2, ctype=0, difficulty=0, relief=0),
                                dict(loc=3, setting=0, ctype=0, difficulty=1, relief=1)])
def test_completing_controls_equal_with_both_settings(portlib, kw):
    img = _fixed_image(portlib, n=3, **kw)
    on = port_call(portlib, img, 3)
    assert not isinstance(on[0], tuple), "the control must complete with the switch at 1"
    portlib.defect(SEARCH, False)
    off = port_call(portlib, img, 3)
    assert off == on


@need_exe
def test_wrong_body_reads_red_on_the_search_control(portlib):
    img = _fixed_image(portlib, n=3, loc=3, setting=0, ctype=0, difficulty=1, relief=1)
    portlib.defect(SEARCH, False)
    good = port_call(portlib, img, 3)
    portlib.negctl("generateRandomCourse", 1)  # camera y = column - 3
    try:
        assert port_call(portlib, img, 3) != good
    finally:
        portlib.negctl("generateRandomCourse", 0)
