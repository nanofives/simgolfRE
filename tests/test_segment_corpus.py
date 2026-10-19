"""re/tools/segment_corpus.py, the pure parts (phase 2.4 segment corpus, 2026-10-18).

Static (no game), on synthetic mini-segments built with trace_format.Writer and numpy frame records:
  - slice_trace cuts a stand-alone trace at a frame-head clock call and an end checkpoint: valid by trace_format.validate,
    ordinals / frames / polls rebased to 0, t0 / q0 equal to the slice's first clock values, the deliveries kept with
    rebased positions, a FRAME of the end poll's own clock call included, and an error for a start value no frame has;
  - input_summary counts deliveries, messages, handler entries, takes, frames, modal time, pause and RNG columns;
  - classify: TIME-FORWARD without input, UI-ONLY / COMMAND from the counterfactual, '?' without one;
  - frame_window / compare_frames (first divergent record and objects, count mismatch), compare_objhash per class;
  - select_segments (prefix skipped, min-frames ends, captures inside a segment skipped, even subset), prefix_index;
  - mask_runs / runs_mask, pack_frames / unpack_frames round trip;
  - write_manifests on a temporary run / corpus tree: rows, status, sha256 and bytes of every corpus file.
The runtime proof (extract + two restarts of a short sandbox_basic segment) is tests/test_segment_corpus_runtime.py.
"""
import hashlib
import json
import struct
import sys

import numpy as np
import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

sys.path.insert(0, str(ROOT / "re" / "tools"))
import segment_corpus as SC  # noqa: E402
import trace_format as TF  # noqa: E402

META = {"schema": 1, "scenario": "synthetic", "frame_ra": TF.FRAME_RA,
        "slots": [["golf_clean.exe", "tgt", 0x1ba21c], ["jgld.dll", "qpc", 0x1000]], "snapshot": [], "spec": "x"}
OTHER_RA = 0x0041E690


def snap(flags=0, date=0, rng=0):
    return b"\0" * (TF.SNAP_LEN - 12) + struct.pack("<III", flags, date, rng)


def build_trace(poll_in_frame_call=False):
    """A boot, then 6 frames of 99 tgt calls (+2 QPC calls each), polls every frame, checkpoints a (frame 1), b (frame
    3), c (frame 5); a click delivered in frame 4 (cursor, post, MSG, HANDLER) and an H1 take; frame 4 is paused."""
    w = TF.Writer(META)
    w.clock(0x00467116, 0, 1, 10)                     # boot calls
    for f in range(6):
        if poll_in_frame_call and f == 5:
            t = w.clock(TF.FRAME_RA, 0, 1, 1)
            w.poll("c")
            w.buf.write(TF.rec(TF.FRAME, 0, struct.pack("<III", w.frames, w.n, t) + snap(0, 100 + f, 7 * f)))
            w.frames += 1
        else:
            w.frame(snap(4 if f == 4 else 0, 100 + f, 7 * f))
        w.clock(OTHER_RA, 0, 1, 98)
        w.clock(0x8100A40E, 1, 2, 2)
        if f == 4:
            w.deliver("cursor", 100, 200)
            w.deliver("post", 1, 0, 0x201, 1, 0x00c80064)
            w.raw(TF.MSG, 0, "<III", 0x201, 1, 0x00c80064)
            w.raw(TF.HANDLER, 2, "<II", 100, 200)
            w.raw(TF.TAKE, 1, "<II", 0x0041C0D7, 0x50)
        if not (poll_in_frame_call and f == 5):
            w.poll({1: "a", 3: "b", 5: "c"}.get(f), final=(f == 5))
        elif f == 5:
            w.poll(None, final=True)
    return TF.parse(w.end())


def frame_values(tr):
    return [r.f["t"] for r in tr.records if r.type == TF.FRAME]


def test_slice_trace_valid_and_rebased():
    tr = build_trace()
    assert TF.validate(tr) == []
    ts = frame_values(tr)
    data, info = SC.slice_trace(tr, ts[2], "c")
    st = TF.parse(data)
    assert TF.validate(st) == []
    assert st.t0 == ts[2] and info["t0"] == ts[2]
    first = st.records[0].f
    assert st.records[0].type == TF.CLOCKRUN and first["first"] == 0 and first["count"] == 1 and first["ra"] == TF.FRAME_RA
    frames = [r.f for r in st.records if r.type == TF.FRAME]
    assert [f["frame"] for f in frames] == [0, 1, 2, 3]       # frames 2..5 of the source
    assert [f["t"] for f in frames] == ts[2:]
    polls = [r.f for r in st.records if r.type == TF.POLL]
    assert polls[0]["index"] == 0 and polls[-1]["checkpoint"] == "c" and polls[-1]["final"]
    # QPC: two calls per frame before the cut (frames 0, 1) -> q0 advanced by 4 steps
    assert st.q0 == tr.q0 + 4 * tr.qstep
    calls = list(TF.clock_calls(st))
    assert calls[0][4] == ts[2] and calls[0][0] == 0
    assert info["first_ordinal"] == 10 + 2 * 101 and st.meta["slice"]["first_frame"] == 2
    kinds = [r.f["kind"] for r in st.records if r.type == TF.DELIVER]
    assert kinds == ["cursor", "post"]


def test_slice_includes_frame_of_end_poll_call():
    tr = build_trace(poll_in_frame_call=True)
    assert TF.validate(tr) == []
    ts = frame_values(tr)
    st = TF.parse(SC.slice_trace(tr, ts[3], "c")[0])
    assert TF.validate(st) == []
    assert frame_values(st) == ts[3:]                       # frame 5 follows the POLL inside the same clock call
    assert st.records[-2].type == TF.FRAME and st.records[-1].type == TF.END


def test_slice_errors():
    tr = build_trace()
    with pytest.raises(ValueError):
        SC.slice_trace(tr, 0x12345, "c")
    with pytest.raises(ValueError):
        SC.slice_trace(tr, frame_values(tr)[4], "a")          # end checkpoint before the start


def test_input_summary():
    tr = build_trace()
    ts = frame_values(tr)
    s = SC.input_summary(TF.parse(SC.slice_trace(tr, ts[2], "c")[0]))
    assert s["deliver"] == {"cursor": 1, "post": 1}
    assert s["msg"] == {"0x201": 1} and s["handler"] == {"rootWin_onPoint": 1} and s["take"] == [[1, 0x50]]
    assert s["inputs"] == 5 and s["frames"] == 4 and s["paused_frames"] == 1
    assert s["intervals"] == {"99": 3} and s["modal_ms"] == 0 and s["date"] == [102, 105] and s["rng_changes"] == 3
    q = SC.input_summary(TF.parse(SC.slice_trace(tr, ts[0], "b")[0]))
    assert q["inputs"] == 0 and q["frames"] == 4


def test_classify():
    assert SC.classify({"inputs": 0}, None)[0] == "TIME-FORWARD"
    assert SC.classify({"inputs": 3}, None)[0] == "?"
    same = {"frames": {"same": True, "first_divergent": None}, "end_sim": {"same": True, "differ": {}}}
    assert SC.classify({"inputs": 3}, same)[0] == "UI-ONLY"
    div = {"frames": {"same": False, "first_divergent": {"record": 4, "T": [1, 1], "objects": ["g_tiles"]}},
           "end_sim": {"same": False, "differ": {"sim": ["g_tiles"]}}}
    c, why = SC.classify({"inputs": 3}, div)
    assert c == "COMMAND" and "g_tiles" in why
    end_only = {"frames": {"same": True, "first_divergent": None}, "end_sim": {"same": False, "differ": {"sim": ["g_x"]}}}
    assert SC.classify({"inputs": 1}, end_only)[0] == "COMMAND"
    assert SC.classify({"inputs": 1}, {"error": "game died"})[0] == "?"


def frames(n_rec=10, n_obj=3, t0=1000, dt=99):
    T = np.array([t0 + dt * i for i in range(n_rec)], np.uint32)
    I = np.arange(n_rec, dtype=np.uint32) + 50
    H = np.arange(n_rec * n_obj, dtype=np.uint64).reshape(n_rec, n_obj)
    return T, I, H


def test_frame_window_and_compare():
    T, I, H = frames()
    w = SC.frame_window(T, int(T[2]), int(T[6]) + 1)
    assert list(w) == [2, 3, 4, 5, 6]
    assert list(SC.frame_window(T, int(T[2]), int(T[6]))) == [2, 3, 4, 5]
    sym = ["a", "b", "c"]
    assert SC.compare_frames(T, H, T.copy(), H.copy(), sym)["same"]
    H2 = H.copy()
    H2[5, 1] ^= 1
    r = SC.compare_frames(T, H, T, H2, sym)
    assert not r["same"] and r["first_divergent"]["record"] == 5 and r["first_divergent"]["objects"] == ["b"]
    r = SC.compare_frames(T, H, T[:-1], H[:-1], sym)
    assert not r["same"] and r["first_divergent"]["reason"] == "record count differs"
    T3 = T.copy()
    T3[3] += 1
    assert SC.compare_frames(T, H, T3, H, sym)["first_divergent"]["record"] == 3


def test_compare_objhash():
    e = {"g_a": ["sim", "1"], "g_b": ["ui", "2"], "g_c": ["sim", "3"]}
    g = {"g_a": ["sim", "1"], "g_b": ["ui", "9"], "g_c": ["sim", "4"]}
    assert SC.compare_objhash(e, g)["differ"] == {"sim": ["g_c"]}
    assert SC.compare_objhash(e, g, None)["differ"] == {"sim": ["g_c"], "ui": ["g_b"]}
    assert SC.compare_objhash(e, dict(e))["same"]


def test_select_segments_and_prefix():
    T = np.array([1000 + 99 * i for i in range(200)], np.uint32)    # 200 frames, T 1000 .. 20701
    caps = [{"name": "menu", "T": 900, "ret": 899, "seen": 1500},
            {"name": "p", "T": 1100, "ret": 1099, "seen": 60}] + [
        {"name": f"c{k}", "T": 2000 + 2000 * k + 50, "ret": 2000 + 2000 * k + 49, "seen": 50} for k in range(8)]
    cps = [{"name": c["name"], "T": c["T"] - 40} for c in caps] + [{"name": "end", "T": 20000}]
    assert SC.prefix_index(caps) == 1
    assert SC.prefix_index([{"name": "m", "seen": 9999}]) is None
    segs = SC.select_segments(caps[1:], cps, T, min_frames=10, max_segments=20)
    # c0 -> c1 has about 20 frames: every capture starts one segment, ends at the next checkpoint
    assert [s["start"] for s in segs] == [f"c{k}" for k in range(8)]
    assert [s["end"] for s in segs] == [f"c{k}" for k in range(1, 8)] + ["end"]
    assert all(s["frames"] >= 10 for s in segs) and [s["id"] for s in segs] == list(range(8))
    long = SC.select_segments(caps[1:], cps, T, min_frames=30, max_segments=20)
    # a 30-frame minimum spans two checkpoints, and the capture inside is skipped
    assert [s["start"] for s in long] == ["c0", "c2", "c4", "c6"] and [s["end"] for s in long] == ["c2", "c4", "c6", "end"]
    few = SC.select_segments(caps[1:], cps, T, min_frames=10, max_segments=3)
    assert [s["start"] for s in few] == ["c0", "c4", "c7"] and [s["id"] for s in few] == [0, 1, 2]


def test_masks_and_frame_pack():
    m = np.zeros(32, bool)
    m[4:8] = True
    m[20:21] = True
    assert SC.mask_runs(m) == [[4, 4], [20, 1]]
    assert (SC.runs_mask(SC.mask_runs(m), 32) == m).all()
    T, I, H = frames(5, 4)
    T2, I2, H2 = SC.unpack_frames(SC.pack_frames(T, I, H), 4)
    assert (T2 == T).all() and (I2 == I).all() and (H2 == H).all()
    with pytest.raises(ValueError):
        SC.unpack_frames(b"\0" * 7, 4)


def test_write_manifests(tmp_path, monkeypatch):
    out, corpus = tmp_path / "runs", tmp_path / "corpus"
    monkeypatch.setattr(SC, "OUT", out)
    monkeypatch.setattr(SC, "CORPUS", corpus)
    monkeypatch.setattr(SC, "ROOT", tmp_path)
    monkeypatch.setattr(SC, "MANIFEST_JSON", tmp_path / "corpus" / "segments.json")
    monkeypatch.setattr(SC, "MANIFEST_TSV", tmp_path / "re" / "segments_manifest.tsv")
    sd = out / "scen"
    sd.mkdir(parents=True)
    segs = [{"id": i, "start": f"s{i}", "end": f"s{i + 1}", "start_frame_index": 10 * i} for i in range(3)]
    (sd / "plan.json").write_text(json.dumps({"segments": segs}))
    rows = [{"id": 0, "start": "s0", "end": "s1", "frames": 5, "accepted": True, "class": "TIME-FORWARD", "reasons": [],
             "class_reason": "no input"},
            {"id": 1, "start": "s1", "end": "s2", "frames": 5, "accepted": True, "class": "UI-ONLY", "reasons": [],
             "class_reason": "no sim effect"},
            {"id": 2, "start": "s2", "end": "s3", "frames": 5, "accepted": False, "class": "COMMAND",
             "reasons": ["r1: first divergent frame record 3"], "class_reason": "x"}]
    (sd / "verify.json").write_text(json.dumps({"segments": rows}))
    d = corpus / "scen" / "00"
    d.mkdir(parents=True)
    (d / "start.data.bin").write_bytes(b"abc")
    (d / "trace.trace").write_bytes(b"defg")
    assert SC.write_manifests() == 0
    man = json.loads((tmp_path / "corpus" / "segments.json").read_text())["segments"]
    assert [e["status"] for e in man] == ["accepted", "excluded", "rejected"]
    assert man[0]["files"]["start.data.bin"] == {"sha256": hashlib.sha256(b"abc").hexdigest(), "bytes": 3}
    assert man[0]["dir"] == "corpus/scen/00" and man[1]["dir"] is None
    lines = (tmp_path / "re" / "segments_manifest.tsv").read_text().splitlines()
    head = lines[0].split("\t")
    assert head[:8] == ["scenario", "segment", "start_checkpoint", "end_checkpoint", "start_frame", "frames", "class",
                        "status"]
    r0 = dict(zip(head, lines[1].split("\t")))
    assert r0["sha256_trace_trace"] == hashlib.sha256(b"defg").hexdigest() and r0["bytes"] == "7"
    assert r0["sha256_frames_bin"] == "-"
    assert dict(zip(head, lines[3].split("\t")))["reason"].startswith("r1: first divergent")


def test_noise_bitmap():
    m = np.zeros(22, bool)
    m[5] = True                      # dword 1
    m[20] = True                     # dword 5 (partial last dword)
    b = SC.noise_bitmap(m)
    assert b == bytes([0b00100010])
    back = SC.bitmap_mask(b, 22)
    assert list(np.nonzero(back)[0]) == [4, 5, 6, 7, 20, 21]


def test_queued_posts():
    w = TF.Writer(META)
    w.clock(0x00467116, 0, 1, 5)
    w.deliver("post", 1, 0, 0x100, 0x26, 1)            # KEYDOWN posted, dispatched only after the next frame head
    w.deliver("post", 1, 0, 0x200, 0, 0x00100010)      # a mouse move that a peek removes without a MSG record
    w.frame()
    w.raw(TF.MSG, 0, "<III", 0x100, 0x26, 1)
    w.clock(OTHER_RA, 0, 1, 98)
    w.deliver("post", 1, 0, 0x101, 0x26, 0xC0000001)
    w.frame()
    w.raw(TF.MSG, 0, "<III", 0x102, 0x1B, 1)           # a WM_CHAR from TranslateMessage: matches nothing
    w.raw(TF.MSG, 0, "<III", 0x101, 0x26, 0xC0000001)
    w.clock(OTHER_RA, 0, 1, 98)
    w.frame()
    w.poll(None, final=True)
    tr = TF.parse(w.end())
    fr = [i for i, r in enumerate(tr.records) if r.type == TF.FRAME]
    q0 = SC.queued_posts(tr, fr[0])
    assert [(e["msg"], e["wp"]) for e in q0] == [(0x100, 0x26), (0x200, 0)]
    q1 = SC.queued_posts(tr, fr[1])                    # the KEYDOWN was dispatched; the mouse move is still queued
    assert [(e["msg"], e["wp"]) for e in q1] == [(0x200, 0), (0x101, 0x26)]
    assert SC.queued_posts(tr, fr[2]) == []            # the KEYUP's dispatch drops the older, never-dispatched move


def test_prefix_skips_queued():
    caps = [{"name": "a", "seen": 50, "queued": [{"msg": 0x100}]}, {"name": "b", "seen": 50, "queued": []}]
    assert SC.prefix_index(caps) == 1
