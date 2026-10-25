"""S6.7: loadGolfPro.negativeWindowBound (D42c, decision G3). With g_difficulty 5 or 6 the pro draw's acceptance bound
tries / (4 - difficulty) + rep + cash falls without end, so the original never ends (probe log/roadmap/S6/6_probes/d42_golfpro.py).
Default 1 = the original: the port raises NoReturn once the bound is under every value v the roster can give; 0 = the draw ends
with pick 0, the arm a repeated pick already takes. Per case: (1) ON the probe state raises NoReturn, (2) OFF it returns,
(3) completing controls give the same whole-image result ON and OFF, (4) a wrong bound (negctl mode 3) reads RED.
Notes: log/roadmap/S6/7_notes.md."""
from __future__ import annotations

import hashlib
from pathlib import Path

import pytest

from port.test import lib as L
from port.test.test_golfers import S, VL

pytestmark = pytest.mark.port

SWITCH = "loadGolfPro.negativeWindowBound"
SEGMENTS = Path(__file__).resolve().parents[1] / "corpus" / "segments"
FULL = set(range(1, 100))


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl_reset()
    yield
    portlib.defect_reset()
    portlib.negctl_reset()


def image_hash(lib, st):
    h = hashlib.sha256()
    for n, o, size in lib.objects():
        h.update(st.read(o, size))
    return h.hexdigest()


def _image(scenario):
    p = SEGMENTS / scenario / "00" / "start.data.bin"
    if not p.exists():
        pytest.skip(f"no segment corpus: {p}")
    return p.read_bytes()


def _call(lib, setup, defect, mode=0, divide_guard=1):
    lib.defect(SWITCH, defect)
    lib.defect("loadGolfPro.divideAndRejectGuard", divide_guard)
    lib.negctl("loadGolfPro", mode)
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        setup(st)
        try:
            res = ("ok", st.call("loadGolfPro", [0]).eax)
        except L.SgError as e:
            res = e.code
        return res, image_hash(lib, st)


def _probe(scenario, difficulty, filled, skill=10):
    """The probe's state: a segment start image, pro records 1..99 filled (look_hi +0x20, skill +0x36), g_difficulty."""
    img = _image(scenario)

    def setup(st):
        st.write(0, img)
        o = _OFF
        for k in range(100):
            ok = k in filled
            st.write(o["g_golfPros"] + k * 0x38 + 0x20, bytes([0x00 if ok else 0xFF]))
            st.write(o["g_golfPros"] + k * 0x38 + 0x36, (skill if ok else 0).to_bytes(2, "little"))
        st.write(o["g_difficulty"], (difficulty & 0xFFFFFFFF).to_bytes(4, "little"))
    return setup


_OFF = {}


@pytest.fixture(autouse=True)
def _symbols(portlib):
    if not _OFF:
        for n, o, size in portlib.objects():
            _OFF[n] = o


def _synthetic(difficulty, filled, holes=(1,), purse=0, cash=0, seed=777):
    """wave 1's state (zero image) with the open holes, g_tournamentPurse and g_cash_div100 chosen."""
    def setup(st):
        s = S.__new__(S)
        s.st, s.o = st, _OFF
        for h in holes:
            s.w("g_holes", 0x208 * h, "b", 1)
        s.w("g_difficulty", 0, "i", difficulty)
        s.w("g_rng", 0, "I", seed)
        s.w("g_tournamentPurse", 0, "i", purse)
        s.w("g_cash_div100", 0, "i", cash)
        for k in range(100):
            s.w("g_golfPros", k * 0x38 + 0x20, "b", 0 if k in filled else -1)
            s.w("g_golfPros", k * 0x38 + 0x36, "h", 10 if k in filled else 0)
    return setup


def test_switch_exists_and_default_on(portlib):
    d = portlib.defects()
    assert SWITCH in d and d[SWITCH]["on"] == 1


# ------------------------------------------------------------------------- (1) + (2) the probe's hang states
@pytest.mark.parametrize("difficulty", [5, 6])
@pytest.mark.parametrize("filled", [FULL, {1}], ids=["all", "rec1"])
def test_probe_course_season_hangs_on_returns_off(portlib, difficulty, filled):
    st_on = _call(portlib, _probe("course_season", difficulty, filled), 1)
    assert st_on[0] == L.SG_E_NO_RETURN
    res, _ = _call(portlib, _probe("course_season", difficulty, filled), 0)
    assert res[0] == "ok", res


@pytest.mark.parametrize("difficulty", [5, 6])
@pytest.mark.parametrize("holes", [(1,), (2,), (1, 2, 3)])
@pytest.mark.parametrize("purse", [0, 50, 5000])
def test_synthetic_hang_on_returns_off(portlib, difficulty, holes, purse):
    """At d = 5 (4 - d = -1) golfer 0 (hole 1) needs R - t/4 <= v <= R - t, empty for every t >= 1, and golfers 2+ need
    v == R - t (v = 0 here, R = rep = purse / 10: R = 0 never). At d = 6 the bound is R - t/2. Which states hang was checked
    against the original in Unicorn (log/roadmap/S6/7_emu_check.log: 11 hang = 11 NoReturn, the other 7 pass the draw)."""
    on = _call(portlib, _synthetic(difficulty, FULL, holes, purse), 1)
    off = _call(portlib, _synthetic(difficulty, FULL, holes, purse), 0)
    if on[0] == L.SG_E_NO_RETURN:
        assert off[0][0] == "ok", off
    else:
        assert on == off, "a completing state must not change"


# ------------------------------------------------------------------------- (3) completing controls
@pytest.mark.parametrize("difficulty", [-1, 0, 1, 2, 3])
@pytest.mark.parametrize("scenario", ["course_season", "sandbox_basic"])
def test_probe_completing_controls_equal(portlib, scenario, difficulty):
    a = _call(portlib, _probe(scenario, difficulty, FULL), 1)
    b = _call(portlib, _probe(scenario, difficulty, FULL), 0)
    assert a == b


@pytest.mark.parametrize("difficulty", [5, 6])
def test_sandbox_no_holes_completes_equal(portlib, difficulty):
    """sandbox_basic's start image has no open hole: the original completes at d = 5 / 6 (probe log)."""
    a = _call(portlib, _probe("sandbox_basic", difficulty, FULL), 1)
    b = _call(portlib, _probe("sandbox_basic", difficulty, FULL), 0)
    assert a == b and a[0][0] == "ok"


@pytest.mark.parametrize("difficulty", [0, 1, 2, 3, -1])
def test_wave1_controls_equal(portlib, difficulty):
    a = _call(portlib, _synthetic(difficulty, FULL), 1)
    b = _call(portlib, _synthetic(difficulty, FULL), 0)
    assert a == b and a[0][0] == "ok"


@pytest.mark.parametrize("difficulty", [5, 6])
def test_d5_completing_window_equal(portlib, difficulty):
    """hole 2 only (golfers 2 and 3, slack = tries): with v = 0 the draw is accepted at t = R (here R = rep = 5) when
    4 - d = -1, so the original completes at d = 5; d = 6 (4 - d = -2) accepts where t / 2 and t bracket R."""
    a = _call(portlib, _synthetic(difficulty, FULL, holes=(2,), purse=50), 1)
    b = _call(portlib, _synthetic(difficulty, FULL, holes=(2,), purse=50), 0)
    assert a == b and a[0][0] == "ok", a[0]


def test_menu_call_unchanged(portlib):
    """the title menu's loadGolfPro(0) on the zero state (no open hole): identical at 1 and 0."""
    def setup(st):
        pass
    a, b = _call(portlib, setup, 1), _call(portlib, setup, 0)
    assert a == b and a[0][0] == "ok"


def test_wave1_switch_independent(portlib):
    """d = 4 still faults with wave 1's switch on, whatever this switch says (4 - d == 0 is not a negative window)."""
    assert _call(portlib, _synthetic(4, FULL), 0)[0] == L.SG_E_DIVIDE


# ------------------------------------------------------------------------- (4) wrong body
def test_wrong_bound_reads_red(portlib):
    good = _call(portlib, _synthetic(5, FULL, holes=(2,), purse=50), 0)
    assert good[0][0] == "ok"
    assert _call(portlib, _synthetic(5, FULL, holes=(2,), purse=50), 0, mode=3) != good
