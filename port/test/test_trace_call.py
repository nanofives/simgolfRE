"""Roadmap S3.30 (decision G24, U-0681, U-1115): the trace's DELIVER kind for an INJECTED CALL.

golfer_events (log/c3/s36a_run.py sc_golfer_events) calls the landmark notice (re/tools/scenario.py LANDMARK_NOTICE_FN, the
port's registered announceLandmark) on the game thread from inside a main-thread clock call (log/c3/s36a_agent_ext.js action
`call`). trace_rec.js wrote that action as DELIVER kind 0 with no fields, which re/tools/trace_format.py rejects, so the
scenario had no corpus trace and its 10 census checkpoints read NOT_RUN. Kind 7 `call` (pos, addr, argc, a0, a1) carries it:

  * re/tools/trace_format.py decodes / writes / validates it and replay_plan hands it to trace_rec.js's replay;
  * re/frida/js/trace_rec.js writes it (before the call runs);
  * port/src/sim/tick.cpp parse_trace / TraceInput apply it in the frame's trace-ordered list of harness items (with the
    DELIVER w32 writes): the registered port function of that address (kInjectedCalls), called with the recorded arguments;
  * port/src/sim/live.cpp records it again (LiveWriteLog::called), so a live recording over the trace replays the same.

Negative controls (tick.frame): 20 the call dropped, 21 its last argument + 1, 22 run at its frame's start instead of its
position. The static tests need nothing; the port tests need port/corpus/traces/golfer_events.trace (local, recorded on
instances\\s3_30) and log/p2/p24/golfer_events/c_a/framehash.bin.
"""
from __future__ import annotations

import pathlib
import re
import struct
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF  # noqa: E402

LANDMARK = 0x004722C0          # re/tools/scenario.py LANDMARK_NOTICE_FN (hooks.csv announceLandmark)
SCEN = "golfer_events"
TRACE = ROOT / "port" / "corpus" / "traces" / f"{SCEN}.trace"


def _writer():
    w = TF.Writer({"scenario": "synthetic", "frame_ra": TF.FRAME_RA, "slots": [["golf_clean.exe", "tgt", 0]]})
    w.clock(0x00401000, 0, 1, 3)
    w.frame()
    return w


# ---------------------------------------------------------------- format (no library)
@pytest.mark.static
def test_call_round_trip_and_replay_plan():
    w = _writer()
    w.clock(0x00401000, 0, 1, 2)
    w.call(LANDMARK, 7, -1)
    w.poll("x")
    w.poll(None, final=True)
    tr = TF.parse(w.end())
    assert TF.validate(tr) == []
    calls = [r for r in tr.records if r.type == TF.DELIVER and r.sub == 7]
    assert len(calls) == 1
    f = calls[0].f
    assert f == {"pos": 6, "kind": "call", "addr": LANDMARK, "argc": 2, "args": [7, 0xFFFFFFFF]}
    assert TF.summary(tr)["deliveries"] == {"call": 1}
    _plan, deliveries, _polls = TF.replay_plan(tr)
    act = [d for d in deliveries if d["k"] == "act"]
    # log/c3/s36a_agent_ext.js's action: signed int arguments a0 / a1
    assert act == [{"pos": 6, "k": "act", "a": {"k": "call", "addr": LANDMARK, "argc": 2, "a0": 7, "a1": -1}, "cls": -1}]
    assert "addr=0x004722c0" in TF.fmt_record(calls[0])


@pytest.mark.static
@pytest.mark.parametrize("argc,slots,why", [(3, (1, 2), "argc 3"), (1, (5, 9), "past argc")])
def test_call_record_rules(argc, slots, why):
    w = _writer()
    w.buf.write(TF.rec(TF.DELIVER, 7, struct.pack("<IIIII", w.n, LANDMARK, argc, *slots)))
    w.poll(None, final=True)
    with pytest.raises(TF.TraceError, match=why):
        TF.parse(w.end())


@pytest.mark.static
def test_writer_refuses_three_arguments():
    with pytest.raises(TF.TraceError):
        _writer().call(LANDMARK, 1, 2, 3)


@pytest.mark.static
def test_recorder_encodes_call_and_flags_unknown_actions():
    js = (ROOT / "re" / "frida" / "js" / "trace_rec.js").read_text(encoding="utf-8")
    m = re.search(r"const TRD = \{(.*?)\};", js, re.S)
    assert dict(re.findall(r"(\w+): (\d+)", m.group(1)))["call"] == "7"
    assert "a.k === 'call'" in js and "no DELIVER kind for action" in js
    # the port maps the call by its offset from the frame-head return address (no image literal in port code)
    tick = (ROOT / "port" / "src" / "sim" / "tick.cpp").read_text(encoding="utf-8")
    m = re.search(r"kInjectedCalls\[\] = \{\{(0x[0-9a-f]+), \"(\w+)\"\}\}", tick)
    assert m and int(m.group(1), 16) == LANDMARK - TF.FRAME_RA and m.group(2) == "announceLandmark"


# ---------------------------------------------------------------- the port over the golfer_events trace
def _need_trace():
    from port.test import boot as B
    if not TRACE.exists() or B.expected_records(SCEN)[1] is None:
        pytest.skip("port/corpus/traces/golfer_events.trace or its capture frame hashes are not on disk")
    return B


def _call_frame(tr):
    """The frame record index the injected call's effects first show in (the FRAME after the DELIVER call)."""
    k = -1
    for r in tr.records:
        if r.type == TF.FRAME:
            k = r.f["frame"]
        elif r.type == TF.DELIVER and r.sub == 7:
            return k + 1
    return None


@pytest.mark.port
def test_golfer_events_trace_has_one_call():
    _need_trace()
    tr = TF.read(TRACE)
    assert TF.validate(tr) == []
    calls = [r.f for r in tr.records if r.type == TF.DELIVER and r.sub == 7]
    assert len(calls) == 1 and calls[0]["addr"] == LANDMARK and calls[0]["argc"] == 2


@pytest.mark.port
def test_golfer_events_boot_passes():
    B = _need_trace()
    r = B.run(SCEN)
    assert r.status == "PASS", (r.status, r.detail, r.run_detail)


@pytest.mark.port
@pytest.mark.parametrize("mode", [20, 21, 22])
def test_call_controls_red(mode):
    """20 dropped, 21 wrong argument, 22 wrong position: each diverges at the first record after the call (or later only when
    the state the wrong body leaves differs later; measured: all three at the call's frame)."""
    B = _need_trace()
    from port.test import lib as L
    lib = L.load()
    call_frame = _call_frame(TF.read(TRACE))
    lib.negctl("tick.frame", mode)
    try:
        r = B.run(SCEN, lib=lib)
    finally:
        lib.negctl("tick.frame", 0)
    assert r.status == "FAIL" and r.first_divergence, (mode, r.status, r.detail)
    assert r.first_divergence["record"] >= call_frame - (1 if mode == 22 else 0), (mode, r.first_divergence, call_frame)


@pytest.mark.port
def test_unknown_call_address_stops_the_run(tmp_path, monkeypatch):
    """A call to an address kInjectedCalls does not hold stops the run with MissingResource at the call, not a silent skip."""
    B = _need_trace()
    data = bytearray(TRACE.read_bytes())
    tr = TF.parse(bytes(data))
    rec = next(r for r in tr.records if r.type == TF.DELIVER and r.sub == 7)
    struct.pack_into("<I", data, rec.offset + 4 + 4, LANDMARK + 0x10)
    (tmp_path / f"{SCEN}.trace").write_bytes(bytes(data))
    monkeypatch.setattr(B, "TRACES", tmp_path)
    r = B.run(SCEN)
    assert r.status == "FAIL" and r.first_divergence is None, (r.status, r.first_divergence)
    # x64 gives the MissingResource text (sg_run_frames_detail); the Win32 runner returns no detail text (U-1100), only the code
    # SG_E_MISSING (-11) with every record before the stop equal
    if r.run_detail:
        assert "kInjectedCalls" in r.run_detail, (r.detail, r.run_detail)
    else:
        assert "returned -11" in r.detail, r.detail
    assert r.records == _call_frame(tr) - 1 or r.records == _call_frame(tr), (r.records, _call_frame(tr))


@pytest.mark.port
def test_live_core_records_the_call():
    """The live run core over the trace equals sg_run_boot, and sg_run_boot over the live core's own recording (which must
    hold the DELIVER call again) equals the live run."""
    _need_trace()
    from port.test import live as LV
    r = LV.equivalence(SCEN)
    assert r.status == "PASS", (r.status, r.detail, r.live_vs_boot, r.replay_vs_live, r.extra)
