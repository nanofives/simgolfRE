"""Roadmap S7.1, the live run core (portsim/sim/live.h, port/test/live.py; notes log/roadmap/S7/1_notes.md).

EQUIVALENCE: the trace host (a corpus trace answering every request of the live core on demand) gives sg_run_boot's records
(T + 203 sim hashes per frame head) over the same trace, and the boot gate's verdict against the capture is unchanged; the
recording the live core writes replays (sg_run_boot) to the same records. The full gate over every boot scenario is
`~/bin/py -3.12 -m port.test.live equiv --all --jobs 3` (test_equivalence_all, slow); a fast set runs here.

ROUND TRIP: scripted sessions (live.py SCRIPTS: input decided at run time by the script host) write recordings that
trace_format parses and validates (virtual clock rule off) and sg_run_boot replays to the live run's own records, the same on
x64 and Win32 (records and recording bytes).

C ABI: a host made of ctypes callbacks (sg_live_run) gives the script host's records for the same script.

Negative controls (live.recorder, live.cpp): a clock answer left out of the recording (RED: the replay stops at the boot), two
dispatches swapped in the recording (RED: state diverges at record 31), one cursor read answered x + 1 (RED in the equivalence
gate: g_cursorTileX at record 767 of cov_k2_build). Equivalent mutants found on the way (BLIND, recorded in the notes): the
first two dispatches swapped (the title click's onPoint and press: same menu choice), a cursor read answered one read late
where the cursor holds still within the frame, and x + 1 at 26 other probed reads (no tile column boundary within a pixel).
Roadmap S7.9: an inflated dispatch recorded without its CURSOR record (control 10) is RED on the replay's count of deliveries
at another cursor count; the frame records and the drawing calls are blind to it on ui_tour / ui_tour_v (9_notes.md section 1).
"""
from __future__ import annotations

import ctypes as C

import pytest

from port.test import boot as BOOT
from port.test import lib as L
from port.test import live as LV

pytestmark = pytest.mark.port

FAST = ("sandbox_basic", "cov_l_point", "cov_a_flags", "cov_b_title", "cov_k2_simfoto", "cov_k2_build")
MIN_RECORDS = {"sandbox_play": 300, "report": 100, "modal_wait": 80, "bio_type": 600, "load_input": 80}


def _need(scen):
    if not (BOOT.TRACES / f"{scen}.trace").exists() or BOOT.expected_records(scen)[1] is None:
        pytest.skip(f"no trace or capture for {scen}")


@pytest.mark.parametrize("scen", FAST)
def test_equivalence(portlib, scen):
    _need(scen)
    r = LV.equivalence(scen, portlib)
    assert r.status == "PASS", r
    assert r.live_vs_boot == ""
    assert r.extra["live_capture_status"] == r.boot_status
    assert r.replay_vs_live == "", r.replay_vs_live
    assert r.violations == 0


@pytest.mark.slow
def test_equivalence_all(portlib):
    """every boot scenario, one process per scenario (roadmap S7.9: on Win32 that is one runner per scenario; a single runner
    over all of them died near 2 GB, U-1681)"""
    import json
    names = BOOT.scenarios()
    rows = LV.run_parallel("equiv", names, jobs=3, verbose=False)
    assert sorted(r["scenario"] for r in rows) == sorted(names)
    bad = [(r["scenario"], r["status"], str(r.get("detail", ""))[:200]) for r in rows if r["status"] not in ("PASS", "NO_FRAMES")]
    assert not bad, bad
    for r in rows:
        extra = json.loads(r["extra"]) if isinstance(r.get("extra"), str) and r["extra"] else (r.get("extra") or {})
        assert extra.get("live_capture_status", r["boot_status"]) == r["boot_status"], r["scenario"]


@pytest.mark.parametrize("name", sorted(LV.SCRIPTS))
def test_roundtrip(portlib, name):
    r = LV.roundtrip(name, portlib)
    assert r.parse_ok and not r.validate, r.validate
    assert r.status == "PASS", r
    assert r.records >= MIN_RECORDS[name]
    assert r.violations == 0
    st = r.extra["stats"]
    assert r.extra["live_detail"].startswith("live: host")   # the script ran to its end
    if name == "report":
        assert st["modal_calls"] >= 6     # the title menu and location screen (4) plus both report screens' loops
    if name == "modal_wait":
        assert st["paced_calls"] >= 40    # the paced pumpAndRedraw wait declined pass after pass
    if name == "bio_type":                # roadmap S7.7: the script's keys, focus keys among them, recorded and replayed
        assert st["dispatches"] >= 61
        assert r.extra["focus_msgs"] == 11     # the script's 11 focus keys, each a MSG record with no HANDLER after it
    if name == "load_input":
        assert r.extra["load_dispatches"] == 2  # 'z' and 'x' given at Load pumps: dispatches followed by the palette QPC


@pytest.mark.parametrize("name", sorted(LV.SCRIPTS))
def test_roundtrip_backends_identical(lib64, lib32, name):
    kind, data = LV.script_data(name)
    a = LV.run_live(lib64, LV.SCRIPT_FILES.get(name), kind, data, LV.TF.FRAME_RA, 20000)
    b = LV.run_live(lib32, LV.SCRIPT_FILES.get(name), kind, data, LV.TF.FRAME_RA, 20000)
    assert (a.code, a.records) == (b.code, b.records)
    assert a.T == b.T and a.hashes == b.hashes
    assert a.trace == b.trace


# (what, run, mode, the expected verdict)
NEGCTL = [("a clock answer left out (seedFromTime's)", "roundtrip", "modal_wait", 1),
          ("dispatches 4 and 5 swapped ('.' and Esc)", "roundtrip", "modal_wait", 2 + 16 * 4),
          ("dispatches 6 and 7 swapped", "roundtrip", "modal_wait", 2 + 16 * 6),
          ("cursor read 2655 answered x + 1", "equiv", "cov_k2_build", 4 + 16 * 2655),
          # roadmap S7.7 (U-1604): the two script sessions over S1's input paths
          ("focus keys left out of the recording (bio_type)", "roundtrip", "bio_type", 8),
          ("the load dispatch's palette QPC left out (load_input)", "roundtrip", "load_input", 7)]


@pytest.mark.parametrize("what,run,name,mode", NEGCTL, ids=[n[0] for n in NEGCTL])
def test_negctl_red(portlib, what, run, name, mode):
    if run == "equiv":
        _need(name)
        r = LV.equivalence(name, portlib, max_records=770, negctl=mode, replay=False)
        assert r.status == "FAIL", f"{what}: {r}"
        assert "g_cursorTileX" in r.live_vs_boot
    else:
        r = LV.roundtrip(name, portlib, negctl=mode)
        assert r.status == "FAIL", f"{what}: {r}"


# ------------------------------------------------------------------------------------------------ roadmap S7.6: idle passes
def test_idle_passes_replay(lib64):
    """U-1601: the title menu's hover loop declines pass after pass while only the cursor moves; the replay of the recording
    shows the same back buffer at every mark (gfx.snapshotAt), makes the same drawing calls (the same passes) and the same
    frame records."""
    r = LV.idle_frames(lib=lib64)
    assert r.status == "PASS", r
    assert r.idle_declines >= 20 and len(r.marks) == 6
    assert r.distinct >= 3          # the hover moved: highlighted Sandbox, no item, another item
    assert r.draws_live.get("Window::refreshFull", 0) > 20


@pytest.mark.parametrize("n", [0, 10, 20])
def test_idle_pass_left_out_red(lib64, n):
    """live.recorder what 5: the cursor read after the n-th idle pass is not recorded; the replay makes one pass fewer (its
    drawing calls differ). The snapshots by position alone are blind to it (each one follows its own record)."""
    r = LV.idle_frames(lib=lib64, negctl=5 + 16 * n)
    assert r.status == "FAIL" and "drawing calls" in r.detail, r


def test_dropped_batch_red(lib64):
    """live.recorder what 6: the 2nd dispatch the host gives (the Sandbox click's press) is applied but not recorded: the
    replay stays on the title menu."""
    r = LV.roundtrip("sandbox_play", lib64, negctl=6 + 16 * 1)
    assert r.status == "FAIL", r


STUCK = ("ui_tour", "ui_tour_v")   # roadmap S7.9: cov_a_keys lost its stuck delivery after b3d61e41 (9_notes.md section 1)


@pytest.mark.parametrize("scen", STUCK)
def test_stuck_delivery_recorded_inflated(lib64, scen):
    """U-1600: a modal loop that pumps twice at one cursor count takes its dispatch at the second call through TraceInput's
    `stuck` rule; the recording gives that dispatch one CURSOR record in front of it, so it replays there: the live run equals
    sg_run_boot over the corpus trace, the replay of the recording equals the live run, and the replay made exactly as many
    deliveries at another cursor count as the live run inflated (each one taken at the call the live run gave it)."""
    _need(scen)
    r = LV.stuck_replay(scen, lib64)
    assert r.status == "PASS", r.detail
    assert r.inflated >= 1 and r.replay_stuck == r.inflated


@pytest.mark.parametrize("scen,n", [("ui_tour", 0), ("ui_tour", 3), ("ui_tour_v", 0), ("ui_tour_v", 3)])
def test_stuck_cursor_record_left_out_red(lib64, scen, n):
    """live.recorder control 10: the n-th inflated dispatch is recorded without its CURSOR record, so the replay delivers it at
    the loop's earlier call at the same cursor count (one stuck delivery fewer: RED). The frame records are blind to it: the
    two calls fall in one pass of a screen loop on these traces (replay_vs_live stays empty)."""
    _need(scen)
    r = LV.stuck_replay(scen, lib64, negctl=10 + 16 * n)
    assert r.status == "FAIL" and r.replay_stuck == r.inflated - 1, r
    assert r.replay_vs_live == ""


def test_stuck_recording_backends_identical(lib64, lib32):
    """the Win32 live run over ui_tour writes the x64 run's recording byte for byte (so it takes the same stuck deliveries)"""
    _need("ui_tour")
    a = LV.stuck_replay("ui_tour", lib64)
    b = LV.stuck_replay("ui_tour", lib32)
    assert a.status == "PASS" and b.status == "PASS", (a.detail, b.detail)
    assert (a.records, a.trace) == (b.records, b.trace)


# ------------------------------------------------------------------------------------------------ roadmap S7.6: U-1604
def test_load_pump_and_focus_keys_recorded(lib64):
    """S1's input paths through the live core: cov_r_flic_input's key is taken by loadFlic's Load pump, cov_r_bio's bio keys by
    the focus child (RootHandler 12 / 13). The trace host gives sg_run_boot's records, and its recording, where a load dispatch
    is followed by a palette QPC record and each focus key is a MSG record with no HANDLER after it, replays to them."""
    for scen in ("cov_r_flic_input", "cov_r_bio"):
        _need(scen)
        r = LV.equivalence(scen, lib64)
        assert r.status == "PASS" and r.replay_vs_live == "" and r.violations == 0, (scen, r)
    data = (BOOT.TRACES / "cov_r_bio.trace").read_bytes()
    lv = LV.run_live(lib64, "cov_r_bio", LV.KIND_TRACE, data, LV.frame_ra_of(data), 5000)
    tr = LV.TF.parse(lv.trace)
    msgs = [r for r in tr.records if r.type == LV.TF.MSG]
    corpus = [r for r in LV.TF.parse(data).records if r.type == LV.TF.MSG and r.f["msg"] in (0x100, 0x102)]
    assert len(msgs) >= 25 and corpus, (len(msgs), len(corpus))
    assert not LV.TF.validate(tr, virtual_clock=False)


@pytest.mark.parametrize("scen,mode", [("cov_r_flic_input", 7), ("cov_r_bio", 8)], ids=["load marker left out",
                                                                                       "focus keys left out"])
def test_u1604_controls_red(lib64, scen, mode):
    _need(scen)
    r = LV.equivalence(scen, lib64, negctl=mode)
    assert r.status == "PASS" and r.replay_vs_live != "", r     # the live run is unchanged, its recording no longer replays


# ------------------------------------------------------------------------------------------------ the C callback ABI
class SgLivePump(C.Structure):
    _fields_ = [("kind", C.c_uint32), ("body", C.c_int32), ("may_deliver", C.c_int32), ("must_deliver", C.c_int32),
                ("frame", C.c_uint32), ("cursor_reads", C.c_uint32), ("pace_site", C.c_int32), ("calls", C.c_uint32),
                ("idle_ok", C.c_int32)]


class SgLiveCmd(C.Structure):
    _fields_ = [("handler", C.c_uint32), ("a0", C.c_int32), ("a1", C.c_int32), ("async_ctrl", C.c_uint32),
                ("async_shift", C.c_uint32), ("t", C.c_uint32)]


FRAME_HEAD = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_uint32, C.POINTER(C.c_uint32))
CLOCK = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_int32, C.c_int32, C.POINTER(C.c_uint32))
CURSOR = C.CFUNCTYPE(None, C.c_void_p, C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int32))
ASYNC = C.CFUNCTYPE(C.c_uint32, C.c_void_p, C.c_int32)
PUMP = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.POINTER(SgLivePump), C.POINTER(SgLiveCmd), C.c_int32, C.POINTER(C.c_int32))


class SgLiveHost(C.Structure):
    _fields_ = [("user", C.c_void_p), ("frame_head", FRAME_HEAD), ("clock", CLOCK), ("cursor", CURSOR), ("async_key", ASYNC),
                ("pump", PUMP)]


class PyHost:
    """The script host's policy in Python for `click 130 445 / click 270 32 / wait_frames N` (live.cpp SCRIPT HOST): a click
    step sets the cursor when it runs and queues its two dispatches; the next step runs at the first pump or frame head after
    the queue is empty; every clock answer is the previous + 1 and a frame head is at least 99 after the last one; the
    script ends at the frame head that completes the wait (no record for it)."""

    def __init__(self, frames: int):
        self.now, self.last_head = 0x00100000, 0
        self.groups = [((130, 445), [[(2, 130, 445)], [(1, 130, 445), (3, 0, 0)]]),
                       ((270, 32), [[(2, 270, 32)], [(1, 270, 32), (3, 0, 0)]])]
        self.queue = []
        self.cx = self.cy = 0
        self.heads = 0
        self.wait_until = None
        self.frames = frames
        self.stopped = False
        self.advance()   # the script host's boot()

    def advance(self):
        if self.queue or self.stopped:
            return
        if self.groups:
            (self.cx, self.cy), self.queue = self.groups.pop(0)
            self.queue = list(self.queue)
        elif self.wait_until is None:
            self.wait_until = self.heads + self.frames
        elif self.heads >= self.wait_until:
            self.stopped = True

    def frame_head(self, _u, frame, t):
        self.heads += 1
        self.advance()
        if self.stopped:
            return 0
        self.now = max(self.now + 1, self.last_head + 99)
        self.last_head = self.now
        t[0] = self.now
        return 1

    def clock(self, _u, site, body, t):
        self.now += 1
        t[0] = self.now
        return 1

    def cursor(self, _u, modal, x, y):
        x[0], y[0] = self.cx, self.cy

    def async_key(self, _u, vk):
        return 0

    def pump(self, _u, p, out, cap, n):
        p = p[0]
        self.advance()
        if p.kind == 3:   # PumpKind::Load (U-1604): the script host declines a world load's pump
            return 0
        if self.queue and p.may_deliver:
            cmds = self.queue.pop(0)
            for i, (h, a0, a1) in enumerate(cmds):
                out[i] = SgLiveCmd(h, a0, a1, 0, 0, 0)
            n[0] = len(cmds)
            return 1
        return 2 if (p.must_deliver or p.idle_ok) else 0


def test_callback_host_equals_script_host(lib64):
    script = "click 130 445\nclick 270 32\nwait_frames 60\n"
    ref = LV.run_live(lib64, None, LV.KIND_SCRIPT, script.encode(), LV.TF.FRAME_RA, 1000)
    py = PyHost(60)
    host = SgLiveHost(None, FRAME_HEAD(py.frame_head), CLOCK(py.clock), CURSOR(py.cursor), ASYNC(py.async_key),
                      PUMP(py.pump))
    dll = lib64.dll
    dll.sg_live_run.restype = C.c_int
    dll.sg_live_run.argtypes = [C.c_void_p, C.POINTER(SgLiveHost), C.c_uint32, C.c_int32, C.POINTER(C.c_uint32),
                                C.POINTER(C.c_uint64), C.POINTER(C.c_int32)]
    nsim = len(lib64.objects("sim"))
    st, done = LV.boot_state(lib64, None)
    try:
        out_t = (C.c_uint32 * 1000)()
        out_h = (C.c_uint64 * (1000 * nsim))()
        recs = C.c_int32(0)
        code = dll.sg_live_run(st.ctx, C.byref(host), LV.TF.FRAME_RA, 1000, out_t, out_h, C.byref(recs))
        ln = C.c_size_t(0)
        dll.sg_live_recording(st.ctx, None, 0, C.byref(ln))
        buf = C.create_string_buffer(max(1, ln.value))
        dll.sg_live_recording(st.ctx, buf, ln.value, C.byref(ln))
    finally:
        done()
    n = recs.value
    # 59 since S1 (roadmap S7.7): the world load's Load pumps now reach the host, so both hosts start the 60-frame wait at a
    # Load call of the Monterey load instead of at the next frame head (one frame head earlier); the equality is the test
    assert code == L.SG_OK and n == ref.records and n >= 59
    assert list(out_t[:n]) == ref.T
    assert [list(out_h[k * nsim:(k + 1) * nsim]) for k in range(n)] == ref.hashes
    assert buf.raw[:ln.value] == ref.trace
