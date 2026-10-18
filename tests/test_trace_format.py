"""Clock / input trace format and tools on synthetic traces (phase 2.3, 2026-10-18; re/model/trace_schema.md).

Static (no game): a trace built with trace_format.Writer parses back, validates, summarises and diffs as the schema says;
each validity rule rejects a trace that breaks it; replay_plan expands the clock runs and lists deliveries and polls in
order; trace_tool.py's offline commands work on a file; trace_rec.js stays opt-in (scenario_det.js and scenario_ab.py do
not load it) and its record layouts match trace_format's.
"""
import json
import re
import struct
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF  # noqa: E402

META = {"schema": 1, "scenario": "synthetic", "frame_ra": TF.FRAME_RA, "spec": "re/model/trace_schema.md",
        "snapshot": [[va, n] for va, n in TF.SNAPSHOT],
        "slots": [["golf_clean.exe", "tgt", 0xBA21C], ["jgld.dll", "qpc", 0x12C414]]}


def snap(date=0, flags=0, rng=0):
    s = bytearray(TF.SNAP_LEN)
    struct.pack_into("<III", s, TF.SNAP_LEN - 12, flags, date, rng)
    return bytes(s)


def build(frames=5, extra=None) -> bytes:
    """A small but complete trace: boot clock calls, a first poll with deliveries, frames with the frame wait's pattern,
    a key delivery and its consumption, a checkpoint and the final poll."""
    w = TF.Writer(META)
    w.clock(0x00467116, 0, 1, 1)                 # seedFromTime
    w.clock(0x8106A414, 1, 2, 3)                 # Palette::update QPC
    w.poll()
    for vk in (1, 2, 3):
        w.deliver("vkey", vk, 1)
    for i in range(frames):
        w.frame(snap(date=i))
        w.clock(0x8106A414, 1, 2, 10)
        w.raw(TF.CURSOR, 1, "<ii", 270, 32)
        w.buf.write(TF.rec(TF.KBSTATE, 1, struct.pack("<IIHBB", w.n, 0x004321E1, 1, 0x90, 1)))
        w.clock(0x0041E641, 0, 1, 1)
        w.clock(0x0041E661, 0, 1, 1)
        for _ in range(8):
            w.clock(0x0045BFB3, 0, 1, 1)
            w.clock(0x0045BFB7, 0, 1, 1)
            w.clock(0x0045BFE3, 0, 1, 9)
            w.clock(0x0041E690, 0, 1, 1)
        if i == 1:
            w.deliver("vkey", 0x1B, 2)
            w.deliver("post", 1, 0, 0x100, 0x1B, 1)
        if i == 2:
            w.raw(TF.MSG, 0, "<III", 0x100, 0x1B, 1)
            w.raw(TF.HANDLER, 7, "<II", 0x1B, 0x1B)
            w.raw(TF.TAKE, 1, "<II", 0x0041C0D7, 0x1B)
        if i == 3:
            w.poll("cp1")
    if extra:
        extra(w)
    w.poll(final=True)
    return w.end()


def test_roundtrip_and_validate():
    data = build()
    tr = TF.parse(data)
    assert TF.validate(tr) == []
    assert tr.meta == META and (tr.step, tr.t0, tr.qstep, tr.q0) == (1, 0x00100000, 10000, 0x10000000)
    assert tr.records[-1].type == TF.END
    # the virtual clock values: the i-th timeGetTime call returns t0 + i
    tg = [v for _o, _ra, _s, k, v in TF.clock_calls(tr) if k == 1]
    assert tg == [0x00100000 + i for i in range(len(tg))]


def test_summary_numbers():
    s = TF.summary(TF.parse(build(frames=5)))
    assert s["frames"] == 5 and s["checkpoints"] == ["cp1"]
    # 1 frame head + 2 + 8 x 12 = 99 timeGetTime calls and 10 QPC calls per frame, as measured on the original
    assert s["clock_calls_per_frame"]["median"] == 109
    assert s["virtual_ms_per_frame"]["median"] == 99
    assert s["date_step_per_frame"]["median"] == 1
    assert s["handlers"] == {"rootWin_onKey": 1} and s["deliveries"] == {"vkey": 4, "post": 1}


def test_diff_finds_the_first_change():
    a = TF.parse(build())
    assert TF.diff(a, TF.parse(build()))["same"]
    data = bytearray(build())
    tr = TF.parse(bytes(data))
    key = next(r for r in tr.records if r.type == TF.DELIVER and r.sub == 3)
    struct.pack_into("<I", data, key.offset + 4 + 16, 0x1C)       # post wParam 0x1b -> 0x1c
    d = TF.diff(a, TF.parse(bytes(data)))
    assert not d["same"] and d["where"] == "record" and d["frame"] == 1


@pytest.mark.parametrize("breaker, rule", [
    ("gap", "first"), ("kind", "slot 1 is qpc"), ("value", "virtual clock"), ("pos", "past the"), ("frame", "FRAME"),
    ("final", "final polls"), ("end", "END counts"),
])
def test_each_rule_rejects(breaker, rule):
    data = bytearray(build())
    tr = TF.parse(bytes(data))
    runs = [r for r in tr.records if r.type == TF.CLOCKRUN]
    if breaker == "gap":
        r = runs[5]
        struct.pack_into("<I", data, r.offset + 4, r.f["first"] + 1)
    elif breaker == "kind":
        r = next(r for r in runs if r.f["slot"] == 1)
        data[r.offset + 1] = 1
    elif breaker == "value":
        r = runs[7]
        struct.pack_into("<Q", data, r.offset + 4 + 16, r.f["v0"] + 1)
    elif breaker == "pos":
        r = next(r for r in tr.records if r.type == TF.CURSOR)
        struct.pack_into("<I", data, r.offset + 4, 10 ** 7)
    elif breaker == "frame":
        r = [r for r in tr.records if r.type == TF.FRAME][2]
        struct.pack_into("<I", data, r.offset + 4, 7)
    elif breaker == "final":
        r = [r for r in tr.records if r.type == TF.POLL][-1]
        data[r.offset + 1] = 0
    elif breaker == "end":
        r = tr.records[-1]
        struct.pack_into("<I", data, r.offset + 4 + 4, 99)
    errs = TF.validate(TF.parse(bytes(data)))
    assert any(rule in e for e in errs), errs


def test_parse_rejects_bad_files():
    data = build()
    with pytest.raises(TF.TraceError):
        TF.parse(b"XXTRACE\0" + data[8:])
    with pytest.raises(TF.TraceError):
        TF.parse(data[:-3])                                  # truncated END
    bad = bytearray(data)
    tr = TF.parse(data)
    bad[tr.records[3].offset] = 0x55                         # unknown record type
    with pytest.raises(TF.TraceError):
        TF.parse(bytes(bad))


def test_replay_plan():
    tr = TF.parse(build(frames=3))
    plan, deliveries, polls = TF.replay_plan(tr)
    n = struct.unpack_from("<I", plan)[0]
    assert n == sum(r.f["count"] for r in tr.records if r.type == TF.CLOCKRUN) and len(plan) == 4 + 12 * n
    ras = struct.unpack_from(f"<{n}I", plan, 4)
    vlo = struct.unpack_from(f"<{n}I", plan, 4 + 4 * n)
    assert ras[0] == 0x00467116 and vlo[0] == 0x00100000
    assert ras[1] == 0                                       # a DLL site (0x81......) is not checked natively
    assert vlo[1:4] == (0x10000000, 0x10000000 + 10000, 0x10000000 + 20000)
    assert [d["k"] for d in deliveries[:4]] == ["poll", "act", "act", "act"]
    assert deliveries[0]["t"] == 0x00100001                  # D+0 after the one timeGetTime call before it
    post = next(d for d in deliveries if d["k"] == "act" and d["a"]["k"] == "post")
    assert post["a"] == {"k": "post", "msg": 0x100, "wp": 0x1B, "lp": 1} and post["cls"] == 1
    assert polls == [None, "<final>"]                        # cp1 is beyond frame 2


def test_cli_offline(tmp_path):
    p = tmp_path / "a.trace"
    p.write_bytes(build())
    q = tmp_path / "b.trace"
    q.write_bytes(build(frames=6))
    tool = [sys.executable, str(ROOT / "re" / "tools" / "trace_tool.py")]
    r = subprocess.run(tool + ["validate", str(p)], capture_output=True, text=True)
    assert r.returncode == 0 and "OK" in r.stdout
    r = subprocess.run(tool + ["summary", str(p), "--json"], capture_output=True, text=True)
    assert r.returncode == 0 and json.loads(r.stdout)["frames"] == 5
    r = subprocess.run(tool + ["dump", str(p), "--types", "FRAME", "--limit", "2"], capture_output=True, text=True)
    assert r.returncode == 0 and r.stdout.count("FRAME") == 2
    r = subprocess.run(tool + ["diff", str(p), str(p)], capture_output=True, text=True)
    assert r.returncode == 0 and "byte-identical" in r.stdout
    r = subprocess.run(tool + ["diff", str(p), str(q)], capture_output=True, text=True)
    assert r.returncode == 1


def test_recorder_is_opt_in_and_layouts_agree():
    det = (ROOT / "re" / "frida" / "js" / "scenario_det.js").read_text(encoding="utf-8")
    ab = (ROOT / "re" / "frida" / "scenario_ab.py").read_text(encoding="utf-8")
    assert "trace_rec" not in det and "TRACE_CFG" not in det and "trace_rec" not in ab
    js = (ROOT / "re" / "frida" / "js" / "trace_rec.js").read_text(encoding="utf-8")
    # record type numbers
    m = re.search(r"const TRT = \{(.*?)\};", js, re.S)
    jt = dict((k, int(v, 0)) for k, v in re.findall(r"(\w+): (0x[0-9a-f]+|\d+)", m.group(1)))
    assert jt == {v: k for k, v in TF.TYPE_NAMES.items()}
    # delivery kinds
    m = re.search(r"const TRD = \{(.*?)\};", js, re.S)
    jd = dict((k, int(v)) for k, v in re.findall(r"(\w+): (\d+)", m.group(1)))
    assert jd == {v: k for k, v in TF.DELIVER_KINDS.items()}
    # FRAME snapshot regions
    m = re.search(r"const TR_SNAP = \[(.*?)\];", js, re.S)
    snapjs = [(int(a, 16), int(b, 0)) for a, b in re.findall(r"\[(0x[0-9a-f]+), (0x[0-9a-f]+|\d+)\]", m.group(1))]
    assert snapjs == TF.SNAPSHOT
    # root-window handler table
    m = re.search(r"const H = \[(.*?)\];", js, re.S)
    hjs = {int(i): int(va, 16) for i, va, _n in re.findall(r"\[(\d+), (0x[0-9a-f]+), (\d)\]", m.group(1))}
    assert hjs == TF.HANDLER_VA


def test_port_schema_copy_is_current():
    copy = ROOT / "port" / "schemas" / "trace.md"
    if not copy.exists():
        pytest.skip("port/schemas/trace.md not present")
    body = copy.read_text(encoding="utf-8").split("\n", 1)[1]
    assert body == (ROOT / "re" / "model" / "trace_schema.md").read_text(encoding="utf-8")
