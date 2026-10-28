"""LiveTerrain: the port's exe-side Terrain calls and the Terrain.dll state they build, against the original on pinned scenarios
(phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 14; harness port/test/terrainlive.py).

    ~/bin/py -3.12 -m pytest port/test/test_terrain_live.py -q
"""
from __future__ import annotations

import pytest

from port.test import terrainlive as TL

pytestmark = pytest.mark.port

SCENARIOS = TL.pinned()
_RUNS: dict = {}
# deliberately wrong bodies the pinned runs must see: 22 the exported resetTerrain does nothing (the course switch, a call-log and
# state divergence), 14 rand()'s increment (the drawn variations in the log), 12 elevateCorner's centre min for max (state only)
NEGCTL = {22: "calls", 14: "calls", 12: "state"}
NEGCTL_SCENARIO = "sandbox_basic"


@pytest.fixture(scope="module")
def lib():
    from port.test import lib as L
    lb = L.load()
    if getattr(lb, "dll", None) is not None:
        TL.bind(lb.dll)
    return lb


def _run(lib, scenario):
    if scenario not in _RUNS:
        _RUNS[scenario] = TL.run(lib, scenario)
    return _RUNS[scenario]


def _need(scenario):
    if scenario == "none":
        pytest.skip("no pinned terrain recording (re/tools/terrain_calls.py --scenario <name>; python -m port.test.terraincorpus export)")


@pytest.mark.parametrize("scenario", SCENARIOS or ["none"])
def test_boot_still_passes(lib, scenario):
    """With the port's own Terrain.dll state attached, the boot gate's sim hashes stay the original's."""
    _need(scenario)
    r = _run(lib, scenario)
    assert r.boot.status in ("PASS", "KNOWN"), (r.boot.status, r.boot.detail)


@pytest.mark.parametrize("scenario", SCENARIOS or ["none"])
def test_calls_equal_the_original(lib, scenario):
    _need(scenario)
    r = _run(lib, scenario)
    assert r.calls_equal, f"{len(r.orig)} / {len(r.port)} calls, first difference {r.first_call_diff}"


def _checkpoints(s):
    root = TL.TC.CORPUS / ("pin_" + s)
    names = TL.TC.snaps(root)
    until = TL.json.loads((root / "index.json").read_text()).get("valid_until")
    return names[:names.index(until) + 1] if until else names


KEYS = [(s, n) for s in SCENARIOS for n in _checkpoints(s)]


@pytest.mark.parametrize("scenario,checkpoint", KEYS or [("none", "none")])
def test_state_equals_the_original(lib, scenario, checkpoint):
    _need(scenario)
    d = _run(lib, scenario).states[checkpoint]
    assert not any(d.values()), d


@pytest.mark.parametrize("mode", sorted(NEGCTL))
def test_negctl(lib, mode):
    if getattr(lib, "dll", None) is None:
        pytest.skip("sg_ts_negctl is an x64 ctypes entry (the Win32 runner has no op for it)")
    if NEGCTL_SCENARIO not in SCENARIOS:
        pytest.skip(f"no pinned recording of {NEGCTL_SCENARIO}")
    lib.dll.sg_ts_negctl(mode)
    try:
        r = TL.run(lib, NEGCTL_SCENARIO)
    finally:
        lib.dll.sg_ts_negctl(0)
    if NEGCTL[mode] == "calls":
        assert not r.calls_equal, f"mode {mode} leaves the call log equal"
    assert any(any(d.values()) for d in r.states.values()) or not r.calls_equal, f"mode {mode} reads GREEN"


# ------------------------------------------------------------------------------------------------ the course view (roadmap S3.2)
# Recordings of the original's root surface around the last terrain frame before each checkpoint (and, in the s32p_ ones, the last
# frame with a draw pass: <checkpoint>.pass), re/tools/terrain_calls.py --scenario <s> --pass-frames, in log/p5/terrain/<label>/frames
# (local data, like the GL corpora). The port boots the same trace with the graphics backend and LiveTerrain attached; every copy
# (the frame's exit, each pass's pre and post) must meet the G2 criterion over the pixels the original's frame drew.
FRAME_RECORDINGS = {"s32_sandbox": "sandbox_basic", "s32_course_season": "course_season", "s32_cov_b_holes18": "cov_b_holes18",
                    "s32_cov_b_career_types": "cov_b_career_types", "s32p_cov_l_camera": "cov_l_camera",
                    "s32p_cov_b_paths": "cov_b_paths", "s32p_cov_k1_terrain": "cov_k1_terrain", "s32p_cov_k3_camera": "cov_k3_camera",
                    "s32q_cov_b_holes18": "cov_b_holes18", "s32q_cov_k1_paths": "cov_k1_paths",
                    "s32t_cov_b_s34_hawaii": "cov_b_s34_hawaii"}
# Tropical (course type 2): the stock game dies on every tropical course (U-1330), so the study scenario cov_b_s34_hawaii was recorded
# with S3.4's Tree alias in the instance, its trace beside the frames (log/roadmap/S3/2_tools/tropical_rec.sh); the port runs that trace
# with run_trace (no capture to compare sim hashes with: the boot gate never sees it)
TRACE_RUNS = {"s32t_cov_b_s34_hawaii"}
# Compatibility switches per recording (roadmap S3.4c): the alias copies stand for flic.treeAliasNames = 0 (the underscore names over
# the stock install); at the default 1 the port faults at the first Tree Cerc load as the stock game does (test below)
SWITCHES = {"s32t_cov_b_s34_hawaii": {"flic.treeAliasNames": False}}
_REC: dict = {}
_PORT: dict = {}


def _rec(label):
    if label not in _REC:
        _REC[label] = TL.recorded_frames(label)
    return _REC[label]


def _copies(label):
    out = []
    for c, e in sorted(_rec(label).items(), key=lambda kv: kv[1]["meta"]["frame"]):
        for what in sorted(k for k in e if k not in ("meta", "entry")):
            out.append((label, c, what))
    return out


FRAME_KEYS = [k for lb in FRAME_RECORDINGS for k in _copies(lb)]


def _port(lib, label, negctl=None, defects="recording"):
    defects = SWITCHES.get(label) if defects == "recording" else defects
    key = (label, negctl, getattr(lib, "dll", None) is None, repr(defects))
    if key not in _PORT:
        rec = _rec(label)
        last = max(rec, key=lambda c: rec[c]["meta"]["frame"])
        frames = sorted({e["meta"]["frame"] for e in rec.values()})
        tr = TL.ROOT / "log" / "p5" / "terrain" / label / "trace.trace" if label in TRACE_RUNS else None
        _PORT[key] = TL.port_frames(lib, FRAME_RECORDINGS[label], frames, last.split(".")[0], negctl=negctl, trace=tr,
                                    defects=defects)
    return _PORT[key]


def _judge(rec, got, c, what):
    e = rec[c]
    p = got.get((e["meta"]["frame"], what.replace("pass", "p")))
    if p is None:
        return None
    return TL.g2(e[what], p, e["entry"])


@pytest.mark.parametrize("label,checkpoint,what", FRAME_KEYS or [("none", "none", "none")])
def test_course_view_equals_the_original(lib, label, checkpoint, what):
    """The port's root surface after the terrain pass (or the frame) is within G2 of the original's."""
    if label == "none":
        pytest.skip("no --pass-frames recording in log/p5/terrain")
    got = _port(lib, label)
    assert got["boot"].status in ("PASS", "KNOWN", "RAN"), (got["boot"].status, got["boot"].detail)
    r = _judge(_rec(label), got, checkpoint, what)
    assert r is not None, f"the port kept no copy of frame {_rec(label)[checkpoint]['meta']['frame']} {what}"
    assert r["pass"], r


def test_hawaii_at_the_default_switch_faults_like_the_stock_game(lib):
    """The tropical recording's trace at flic.treeAliasNames = 1 (the original profile): the boot stops with SG_E_FAULT at the first
    Tree Cerc load, as the stock game dies there (U-1330, log/roadmap/S3/4_rec_hawaii_stock.log); at 0 it runs (the course view
    tests above)."""
    from port.test import lib as L
    label = "s32t_cov_b_s34_hawaii"
    if not _rec(label):
        pytest.skip(f"no recording {label}")
    got = _port(lib, label, defects=None)
    assert got["boot"].status == "ERROR" and f"sg_run_boot {L.SG_E_FAULT} " in got["boot"].detail,         (got["boot"].status, got["boot"].detail)
    assert _port(lib, label)["boot"].status in ("PASS", "KNOWN", "RAN")


def test_course_view_draws(lib):
    """The terrain family handled passes and drew pixels, and no terrain body reported an unsupported case."""
    label = next((lb for lb in FRAME_RECORDINGS if _rec(lb)), None)
    if label is None:
        pytest.skip("no recording")
    got = _port(lib, label)
    assert got["passes"] > 0
    bad = [ln for ln in got["log"].splitlines() if ln.startswith("unsupported\tterrain")]
    assert not bad, bad


# negative controls of the terrain family (gfx_terrain.cpp): each must break G2 on the recording that exercises it
TERRAIN_NEGCTL = {1: ("s32_cov_b_holes18", "wrong zoom: the rasteriser misses setZoomLevel's projection"),
                  2: ("s32_cov_b_career_types", "wrong theme texture set: loadNewCourseType's reload not rasterised"),
                  3: ("s32_sandbox", "pass skipped"),
                  4: ("s32q_cov_b_holes18", "renderTerrainStrip's copy without its shift"),
                  5: ("s32_course_season", "Window::paint leaves the root surface")}


@pytest.mark.parametrize("mode", sorted(TERRAIN_NEGCTL))
def test_terrain_negctl_reads_red(lib, mode):
    label, why = TERRAIN_NEGCTL[mode]
    if not _rec(label):
        pytest.skip(f"no recording {label}")
    got = _port(lib, label, negctl=("gfx.family.terrain", mode))
    res = [_judge(_rec(label), got, c, w) for _lb, c, w in _copies(label)]
    red = [r for r in res if r is None or not r["pass"]]
    assert red, f"mode {mode} ({why}) reads GREEN on all {len(res)} copies of {label}"


def test_x64_equals_win32_terrain_pixels(lib, request):
    """LiveTerrain attaches on the Win32 backend (the runner's sg_ts_live ops) and gives the same terrain pixels as x64.
    SG_PORTSIM_DLL32 / SG_PORTSIM_RUNNER32 name a Win32 build outside port/build/Win32 (a private mirror)."""
    import os
    from port.test import lib as L
    label = "s32_sandbox"
    if not _rec(label):
        pytest.skip("no recording")
    if os.environ.get("SG_PORTSIM_DLL32"):
        from port.test import integrator as I
        l32 = I.load_lib(I.LibSpec(backend="win32", dll32=os.environ["SG_PORTSIM_DLL32"],
                                   runner32=os.environ.get("SG_PORTSIM_RUNNER32")))
    else:
        l32 = request.getfixturevalue("lib32")
    l64 = lib if getattr(lib, "dll", None) is not None else L.load()
    a, b = _port(l64, label), _port(l32, label)
    assert b["passes"] == a["passes"] > 0
    keys = sorted(k for k in a if isinstance(k, tuple))
    assert keys and keys == sorted(k for k in b if isinstance(k, tuple))
    for k in keys:
        assert (a[k] == b[k]).all(), k
