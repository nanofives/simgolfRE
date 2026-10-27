"""Roadmap S7.8 (log/roadmap/S7/8_notes.md): where the Windows app presents (U-1642) and what a long play session keeps in
memory (U-1643).

U-1642. The original's back buffer reaches its window only through jgld's Display::present (display vtable slot +0xa4, BitBlt of
the back buffer clipped to a rect), and the exe calls that slot in one place: the end of drawTooltip, which Window::refreshFull
reaches through Window::refreshRect (the cursor, tooltip and WM_PAINT paths reach it too, none of them drawn by the port). The
slot P6 calls (+0x9c) is Display::setScale. The port's back buffer changes only inside the Window::refreshFull body of the gfx
backend, so the app now presents where tick runs that drop ('r', LiveHost::present_point), as the original does, and keeps the
frame-wait ('w') and modal-pump ('m' / 'p') presents for anything drawn elsewhere. Gate: over four corpus traces no present is
left for the frame wait (every in-frame change is presented at its refreshFull), every checkpoint snapshot equals the last
present made before it, and the count of 'r' presents a later 'r' replaced before any w / m / p point (frames S7.5's points
never showed) is recorded. Control: `--negctl present-point-off` (S7.5's points only) leaves the frame-wait presents.

U-1643. LiveTerrain logs one line per Terrain call and never drops one; the app drops the lines at every frame end
(host.h TerrainLogBound). The larger growth was the live core's own: every frame record and recorder event held until the
end; the app now streams them (host.h RecordStreams: framehash.bin per record, trace.trace spilled by the core). Gate:
course_season_6y over the app's live path (51,092 records), framehash.bin equal to the original's capture byte for byte, the
terrain log empty at the end, private bytes after frame 2,000 within GROWTH_LIMIT; the control (`--in-memory --negctl
terrain-log-keep`, S7.5's way) grows past ten times the limit (RED) and gives the same records and the same recording.

Run with SG_APP_EXE / SG_PORTSIM_DLL set to one build (test_app_ingame.py's `exe` fixture). The long runs take about ten
minutes each and are marked slow.
"""
from __future__ import annotations

import hashlib
import json
import pathlib
import shutil
import subprocess
import tempfile
import time

import pytest

from port.test.test_app_ingame import INSTALL, ROOT, _lib, app_live_trace, checkpoint_plan, exe  # noqa: F401 (fixture)

pytestmark = pytest.mark.port

PRESENT_SCENARIOS = ("sandbox_basic", "course_season", "cov_a_keys", "cov_k1_routing")
LONG = "course_season_6y"
OUT = ROOT / "log" / "roadmap" / "S7" / "8_long"
FACTS = ROOT / "log" / "roadmap" / "S7" / "8_present_facts.json"


def _facts_update(key: str, value) -> None:
    if not FACTS.parent.is_dir():
        return
    d = json.loads(FACTS.read_text()) if FACTS.exists() else {}
    d[key] = value
    FACTS.write_text(json.dumps(d, indent=1))


# ------------------------------------------------------------------------------------------------ U-1642: the present point
@pytest.fixture(scope="module")
def present_runs(exe):  # noqa: F811
    return {sc: app_live_trace(exe, sc, checkpoint_plan(sc)) for sc in PRESENT_SCENARIOS}


def test_presents_at_the_original_point(present_runs):
    """every back-buffer change of the frame code is presented at its Window::refreshFull: no frame-wait present is left"""
    facts = {}
    for sc, run in present_runs.items():
        s = run["session"]
        assert run["rc"] == 0 and s.get("code") == 0, (sc, run["stderr"])
        k = s["presents_by_kind"]
        facts[sc] = {"presents_by_kind": k, "r_superseded": s["r_superseded"], "records": s["records"],
                     "presents": s["presents"]}
        assert k["r"] > 0, (sc, k)
        assert k["w"] == 0, (sc, k)
    _facts_update("present_point", facts)


def test_every_checkpoint_is_shown_by_a_present(present_runs):
    """the replay's snapshot at each checkpoint equals the last present the app made before it"""
    lib = _lib()
    for sc, run in present_runs.items():
        n = 0
        for k, (c, raw) in enumerate(run["snapshots"].items()):
            if raw is None:
                continue
            before = [p for p in run["presents"] if p["snaps"] <= k]
            assert before and before[-1]["hash"] == lib.hash_bytes(raw), (sc, c, before[-1]["kind"] if before else None)
            n += 1
        assert n >= 1, sc


def test_control_present_point_off(exe):  # noqa: F811
    """S7.5's present points only: the frame's changes are left for the frame wait (RED for the gate above)"""
    run = app_live_trace(exe, "sandbox_basic", checkpoint_plan("sandbox_basic"), extra=["--negctl", "present-point-off"])
    k = run["session"]["presents_by_kind"]
    assert k["r"] == 0 and k["w"] > 0, k     # RED: test_presents_at_the_original_point asserts w == 0


# ------------------------------------------------------------------------------------------------ U-1643: a long session
GROWTH_LIMIT = 4 << 20   # private bytes a bounded run may gain after frame 2,000 (measured: 28,672 over 49,000 frames)


def _long_run(exe, variant: str, extra=()) -> dict:  # noqa: F811
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_8_long_"))
    try:
        args = [str(exe), "--install", str(INSTALL), "--data", str(tmp / "data"), "--record-dir", str(tmp / "rec"),
                "--live-trace", str(ROOT / "port" / "corpus" / "traces" / f"{LONG}.trace"), "--mem-every", "1000"]
        t0 = time.time()
        r = subprocess.run(args + list(extra), cwd=str(ROOT), capture_output=True, text=True, timeout=5400)
        rec = tmp / "rec"
        out = {"rc": r.returncode, "stderr": r.stderr[-2000:], "seconds": round(time.time() - t0, 1),
               "session": json.loads((rec / "session.json").read_text()),
               "memory": [[int(x) for x in l.split("\t")] for l in (rec / "memory.tsv").read_text().splitlines()[1:] if l],
               "framehash": (rec / "framehash.bin").read_bytes(),
               "trace_sha": hashlib.sha256((rec / "trace.trace").read_bytes()).hexdigest()}
        if OUT.parent.is_dir():
            dst = OUT / variant
            dst.mkdir(parents=True, exist_ok=True)
            for f in ("session.json", "memory.tsv"):
                shutil.copyfile(rec / f, dst / f)
        return out
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def _trace_run(exe, scenario: str, extra=()) -> tuple[str, str, dict]:  # noqa: F811
    """one --live-trace run: (sha256 of trace.trace, of framehash.bin, session.json)"""
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_8_tr_"))
    try:
        args = [str(exe), "--install", str(INSTALL), "--data", str(tmp / "data"), "--record-dir", str(tmp / "rec"),
                "--live-trace", str(ROOT / "port" / "corpus" / "traces" / f"{scenario}.trace")]
        subprocess.run(args + list(extra), cwd=str(ROOT), capture_output=True, text=True, timeout=3600)
        rec = tmp / "rec"
        sha = lambda f: hashlib.sha256((rec / f).read_bytes()).hexdigest() if (rec / f).exists() else ""  # noqa: E731
        return sha("trace.trace"), sha("framehash.bin"), json.loads((rec / "session.json").read_text())
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@pytest.mark.slow
def test_spilled_recording_equals_the_in_memory_one(exe):  # noqa: F811
    """course_season: the recording spilled to disk as the session goes (45,801 events in 4,096-event spills) is the in-memory
    recording byte for byte, and so is framehash.bin; the control (live.recorder 9: each spill drops its last event) is RED"""
    a = _trace_run(exe, "course_season")
    b = _trace_run(exe, "course_season", ["--in-memory"])
    c = _trace_run(exe, "course_season", ["--negctl", "reg:live.recorder=9"])
    assert a[2]["streamed"] and not b[2]["streamed"] and a[2]["spilled_events"] > 0, (a[2], b[2])
    assert a[0] == b[0] and a[1] == b[1]
    assert c[0] != b[0] and c[1] == b[1]      # RED: the spilled recording lost events (the frame records are the core's)


@pytest.fixture(scope="module")
def long_bound(exe):  # noqa: F811
    return _long_run(exe, "bound")


@pytest.fixture(scope="module")
def long_keep(exe):  # noqa: F811
    """the control: S7.5's way (records and recording held until the end) and every terrain log line kept"""
    return _long_run(exe, "keep", ["--in-memory", "--negctl", "terrain-log-keep"])


def _growth(mem: list[list[int]]) -> int:
    """private bytes at the end minus at the first sample from frame 2,000 on (the world is loaded by then)"""
    after = [m for m in mem if m[0] >= 2000]
    return after[-1][3] - after[0][3]


@pytest.mark.slow
def test_long_session_equals_the_capture(long_bound):
    """>= 20,000 frames over the app's live path: framehash.bin (every record's T and 203 sim hashes) is the original's
    capture byte for byte"""
    ref = (ROOT / "log" / "p2" / "p24" / LONG / "c_a" / "framehash.bin").read_bytes()
    assert long_bound["rc"] == 0 and long_bound["session"]["code"] == 0, long_bound["stderr"]
    assert long_bound["session"]["records"] >= 20_000 and long_bound["session"]["streamed"]
    assert long_bound["framehash"] == ref


@pytest.mark.slow
def test_long_session_terrain_log_bounded(long_bound):
    s = long_bound["session"]
    assert s["terrain_log_end"] == 0, s
    assert all(m[4] == 0 for m in long_bound["memory"]), long_bound["memory"][:3]
    assert s["terrain_log_max"] <= s["terrain_calls"], s


@pytest.mark.slow
def test_long_session_memory_bounded(long_bound, long_keep):
    """the streaming run's private bytes stay flat after the world load; the control's grow with every frame (RED); both runs
    give the same frame records and the same recording"""
    gb, gk = _growth(long_bound["memory"]), _growth(long_keep["memory"])
    ks = long_keep["session"]
    _facts_update("long_memory", {"growth_bound": gb, "growth_keep": gk, "records": long_bound["session"]["records"],
                                  "bound_seconds": long_bound["seconds"], "keep_seconds": long_keep["seconds"],
                                  "bound_memory": long_bound["session"]["memory"], "keep_memory": ks["memory"],
                                  "terrain_calls": long_bound["session"]["terrain_calls"],
                                  "keep_terrain_log_end": ks["terrain_log_end"], "trace_bytes": long_bound["session"]["trace_bytes"],
                                  "spilled_events": long_bound["session"]["spilled_events"]})
    assert gb < GROWTH_LIMIT, gb
    assert gk > 10 * GROWTH_LIMIT, gk                                    # RED for the bound above
    assert ks["terrain_log_end"] == ks["terrain_calls"], ks              # the control kept every line
    assert long_bound["framehash"] == long_keep["framehash"]
    assert long_bound["trace_sha"] == long_keep["trace_sha"]             # the spilled recording = the in-memory one

