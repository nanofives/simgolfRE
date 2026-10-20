"""Phase 3.8 integrator (stream 38f): chained segment runs (gate c), the command-API driver (gate d), the divergence
ledger (gate e), the no-input speed trace (gate f) and the Win32 / x64 identity of a segment run. Tools: port/test/
integrator.py, port/test/semantic.py; notes log/p38/38f_notes.md."""
from __future__ import annotations

import pathlib

import pytest

from port.test import integrator as I
from port.test import lib as L
from port.test import segments as S

need_segments = pytest.mark.skipif(not S.SEG_DIR.exists(), reason="segment corpus not present (port/corpus/segments, local)")
need_traces = pytest.mark.skipif(not I.TRACES.exists(), reason="trace corpus not present (port/corpus/traces, local)")
need_install = pytest.mark.skipif(not S.INSTALL.is_dir(), reason="the user's install (original/) is not present")


# ------------------------------------------------------------------------------------------- static
@pytest.mark.static
def test_course_season_is_one_chain():
    """The manifest's 10 course_season segments touch: one chain, frames 167 .. 6,294 (re/port/segments_manifest.tsv)."""
    ch = I.chains_of("course_season")
    assert len(ch) == 1 and len(ch[0]) == 10
    first, last = ch[0][0], ch[0][-1]
    assert int(first["start_frame"]) == 167 and int(last["start_frame"]) + int(last["frames"]) == 6294


@pytest.mark.static
def test_chains_break_where_windows_do_not_touch():
    rows = [{"scenario": "x", "segment": "0", "start_checkpoint": "a", "end_checkpoint": "b", "start_frame": "10",
             "frames": "20", "status": "accepted", "class": "COMMAND"},
            {"scenario": "x", "segment": "1", "start_checkpoint": "b", "end_checkpoint": "c", "start_frame": "31",
             "frames": "20", "status": "accepted", "class": "COMMAND"},
            {"scenario": "x", "segment": "2", "start_checkpoint": "d", "end_checkpoint": "e", "start_frame": "60",
             "frames": "20", "status": "accepted", "class": "COMMAND"}]
    orig = I.accepted_rows
    I.accepted_rows = lambda cls="all": rows
    try:
        ch = I.chains_of("x")
    finally:
        I.accepted_rows = orig
    assert [[int(r["segment"]) for r in c] for c in ch] == [[0, 1], [2]]


@pytest.mark.static
def test_ledger_reads_the_stop_reason():
    d = "missing: Text_put; stopped in showPopup < demolishObject; input: 3 dispatches delivered"
    assert I._missing_name(d) == "Text_put"
    assert I._stop_path(d) == "showPopup < demolishObject"
    assert I._missing_name("end checkpoint at site 312244") == ""


@pytest.mark.static
def test_ledger_flags_a_row_without_cause(tmp_path):
    seg = tmp_path / "segs.tsv"
    I.write_tsv(seg, [{"name": "a/00", "status": "FAIL", "frames_run": 3, "frames_expected": 9, "record": "4",
                       "objects": "g_rng", "run_detail": "", "detail": ""},
                      {"name": "b/00", "status": "FAIL", "frames_run": 1, "frames_expected": 9, "record": "",
                       "objects": "", "run_detail": "missing: Text_put; stopped in showPopup", "detail": ""},
                      {"name": "c/00", "status": "PASS", "frames_run": 9, "frames_expected": 9}])
    rows = [r for r in I.ledger_rows(seg, None, None) if r["kind"] == "segment"]
    by = {r["name"]: r for r in rows}
    assert set(by) == {"a/00", "b/00"}
    assert by["a/00"]["explained"] == "NO" and "record 4" in by["a/00"]["stop"]
    assert by["b/00"]["explained"] == "yes" and by["b/00"]["owner"] == "38a"


@pytest.mark.static
def test_writes_of_the_final_poll_are_dropped():
    """The original dumps a checkpoint before it applies that poll's actions: a w32 write recorded at the final POLL's clock
    position leaves the slice, an earlier one stays."""
    import trace_format as TF
    w = TF.Writer({"frame_ra": TF.FRAME_RA})
    w.frame()
    w.deliver("w32", 0x004C2BA4, 7)
    w.clock(TF.FRAME_RA + 0x4C384, 4, 1, 5)
    w.frame()
    w.clock(TF.FRAME_RA + 0x4C384, 4, 1, 3)
    w.deliver("w32", 0x004C2BA4, 54)
    w.deliver("vkey", 37, 2)
    w.poll("cp", final=True)
    data = w.end()
    out, n = I.drop_final_poll_writes(data)
    assert n == 1
    recs = TF.parse(out).records
    writes = [r.f["v"] for r in recs if r.type == TF.DELIVER and r.sub == 4]
    assert writes == [7] and any(r.type == TF.DELIVER and r.sub == 2 for r in recs)
    assert not TF.validate(TF.parse(out))
    assert I.drop_final_poll_writes(out) == (out, 0)


@pytest.mark.static
def test_a_menu_exit_at_a_boundary_is_a_break():
    row = {"status": "FAIL", "run_detail": I.MENU_EXIT + "; input: 3 dispatches", "boundaries": "1:914,2:2947,3:3309",
           "record": "3309"}
    assert I.menu_break(row) == 3
    assert I.menu_break({**row, "record": "3300"}) is None          # inside a segment: a real failure
    assert I.menu_break({**row, "run_detail": "missing: Text_put"}) is None


@pytest.mark.static
@need_segments
@need_traces
@pytest.mark.parametrize("scenario", ["course_season", "golfer_stories", "championship"])
def test_chain_slices_equal_the_segment_traces(scenario):
    """slice_trace of the whole trace from a segment's start to its end checkpoint is its trace.trace byte for byte: the
    chained runs' slices come from the corpus's own cutting function."""
    import segment_corpus as SC
    for r in [r for r in I.accepted_rows() if r["scenario"] == scenario]:
        seg = S.load_segment(S.segment_dir(r))
        data, _ = SC.slice_trace(I.full_trace(scenario), seg.start_json["start"]["frame_value"], r["end_checkpoint"],
                                 {"scenario": scenario, "segment": int(r["segment"])})
        assert data == seg.trace, seg.name


@pytest.mark.static
@need_segments
def test_synthetic_speed_trace_has_no_input():
    import trace_format as TF
    seg = S.load_segment(S.SEG_DIR / "course_season" / "00")
    data, info = I.synthetic_trace(seg.trace, seg.start_json["start"]["frame_value"], 50)
    tr = TF.parse(data)
    assert not TF.validate(tr)
    assert not [r for r in tr.records if r.type in I._input_types()]
    ts = [r.f["t"] for r in tr.records if r.type == TF.FRAME]
    assert len(ts) == 51 and ts[0] == seg.start_json["start"]["frame_value"]
    assert {b - a for a, b in zip(ts, ts[1:])} == {info["tgt_calls_per_frame"]}


# ------------------------------------------------------------------------------------------- runs
def _spec(portlib) -> I.LibSpec:
    return I.LibSpec(backend="x64", dll=str(portlib.path))


@pytest.mark.port
@need_segments
@need_traces
@need_install
def test_golfer_stories_chain_passes(portlib):
    """golfer_stories' three segments as one run from segment 0's image: every frame record and all three checkpoints."""
    ch = I.chains_of("golfer_stories")[0]
    plan = I.chain_plan(ch)
    for cp, _k in plan["checkpoints"]:
        r = I._run_chain_checkpoint(_spec(portlib), "golfer_stories", [int(x["segment"]) for x in ch], cp, False)
        assert r["status"] == "PASS", f"{cp}: {r['status']} {r['frames_run']}/{r['frames_expected']} {r['detail']}"
    assert r["frames_expected"] == r["frames_in_chain"] == 1915


@pytest.mark.port
@need_segments
@need_traces
@need_install
def test_chain_reads_red_on_a_wrong_frame(portlib):
    """Negative control: tick.frame mode 1 (one extra draw of the game's generator per frame) fails the chain at its
    first frame record."""
    ch = I.chains_of("golfer_stories")[0]
    portlib.negctl("tick.frame", 1)
    try:
        r = I._run_chain_checkpoint(_spec(portlib), "golfer_stories", [int(x["segment"]) for x in ch], "holes_built", False)
    finally:
        portlib.negctl("tick.frame", 0)
    assert r["status"] == "FAIL" and r["record"] == "1"


@pytest.mark.port
@need_segments
@need_install
def test_no_input_speed_run_is_deterministic(portlib):
    """200 no-input frames from course_season/00 twice: the same digest (T and every sim hash per frame)."""
    a = I._speed(_spec(portlib), "course_season/00", 200)
    b = I._speed(_spec(portlib), "course_season/00", 200)
    assert a["frames_run"] == 200 and a["code"] == 0
    assert a["digest"] == b["digest"]


def _has_overrides(portlib) -> bool:
    return portlib.has("integrator.setBatchOverrides")


@pytest.mark.port
@need_segments
@need_install
def test_semantic_golfer_panel_click_misses_the_frame_effect(portlib):
    """Gate d's finding (log/p38/38f_notes.md): replacing championship/00's first golfer-panel click by
    GolferPanelAction(hitGolferPanel435f00) keeps the panel action but not mainLoop H2's own press effects (the camera tile
    cache reset at ML+0xedf1, the return to the frame head after the handler): the run diverges at the next frame head,
    in g_cachedCameraTileX. Integrator negative control 1 (replacements ignored) gives the raw result back."""
    if not _has_overrides(portlib):
        pytest.skip("the integrator module is not in this build")
    from port.test import semantic as SEM
    spec = _spec(portlib)
    rows = SEM.translate(spec, "championship/00")
    first = next(r for r in rows if r.semantic)
    assert first.semantic["kind"] == 0x100 and first.semantic["arg"][0] == 4
    only = [r if (r.batch <= first.batch) else SEM.Row(**{**r.__dict__, "semantic": None, "translation": ""})
            for r in rows]
    d = SEM.run_mode(spec, "championship/00", only, "hybrid")
    if d["applied"] == 0:
        pytest.skip("this build has the integrator module but not the tick.cpp hook (log/p38/38f_tick.patch)")
    assert d["status"] == "FAIL" and d["record"] == str(first.frame + 1) and "g_cachedCameraTileX" in d["objects"]
    portlib.negctl("integrator.setBatchOverrides", 1)
    try:
        d2 = SEM.run_mode(spec, "championship/00", only, "hybrid")
    finally:
        portlib.negctl("integrator.setBatchOverrides", 0)
    assert d2["status"] == "PASS"


@pytest.mark.x87
@need_segments
@need_install
def test_segment_digest_equal_on_win32(lib64, lib32):
    """course_season/00 gives the same per-frame digest on the Win32 backend as on x64 (the whole gate:
    `integrator segments --backend win32` + `integrator compare`)."""
    seg = S.load_segment(S.SEG_DIR / "course_season" / "00")
    end = list(seg.checkpoints.values())[-1]["hashes"]
    a = I.run_from(lib64, seg.name, seg, seg.trace, seg.records, end)
    b = I.run_from(lib32, seg.name, seg, seg.trace, seg.records, end)
    assert a.status == b.status == "PASS" and a.digest == b.digest
