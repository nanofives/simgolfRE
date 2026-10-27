"""Roadmap S7.4: the app's own recordings in the boot gate (port/test/app_replay.py; notes log/roadmap/S7/4_notes.md).

Each stored app session (port/corpus/app/<name>: trace.trace + framehash.bin, recorded by `app_replay.py record`) replays
through sg_run_boot to the session's own framehash in every record (T and the 203 sim hashes), on x64 and on Win32. The
three recordings the gate names: `hole` (title -> Sandbox -> Monterey, Shift / Ctrl clicks, a double click, the wheel,
Alt+X, then one hole built and opened), `saveload` (a save written into the app's overlay and loaded back through the
browser) and `report` (the F1..F4 report screens, modal).

Controls: one byte of a recording's input changed (the tee click's x, 300 -> 364, in its HANDLER record) must make the replay
diverge at or after the tee's frame; one framehash record changed (a byte of one sim hash, then a T) must fail at exactly that
record.

    cd <tree> && ~/bin/py -3.12 -m pytest port/test/test_app_replay.py -p no:cacheprovider -v
    (SG_PORTSIM_DLL picks the x64 DLL; the Win32 runner is port/build/Win32's, as for every Win32 test)
"""
from __future__ import annotations

import json
import os
import pathlib
import struct
import sys

import pytest

from port.test import app_replay as AR

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
REQUIRED = ("hole", "saveload", "report")


def _have(name: str) -> bool:
    return name in AR.recordings()


@pytest.fixture(scope="module")
def x64():
    from port.test import lib as L
    if L.backend() == "win32":
        L.B.stale("x64") and L.B.build("x64", quiet=True)
        return L.Lib(L.B.dll_path("x64"))
    return L.load(os.environ.get("SG_PORTSIM_DLL"))


@pytest.mark.parametrize("name", REQUIRED)
def test_recording_replays_x64(x64, name):
    if not _have(name):
        pytest.skip(f"no recording port/corpus/app/{name} (make it: app_replay.py record {name})")
    r = AR.gate_one(x64, name)
    assert r["status"] == "PASS", r
    assert r["records"] >= 150, r


@pytest.mark.parametrize("name", REQUIRED)
def test_recording_replays_win32(lib32, name):
    if not _have(name):
        pytest.skip(f"no recording port/corpus/app/{name}")
    r = AR.gate_one(lib32, name)
    assert r["status"] == "PASS", r


def test_backends_identical(x64, lib32):
    names = [n for n in REQUIRED if _have(n)]
    if not names:
        pytest.skip("no recordings")
    for n in names:
        tr = (AR.CORPUS / n / "trace.trace").read_bytes()
        nrec = len(AR.read_framehash((AR.CORPUS / n / "framehash.bin").read_bytes(), 203))
        a, b = AR.replay(x64, tr, nrec + 1), AR.replay(lib32, tr, nrec + 1)
        assert a["records"] == b["records"] and a["T"] == b["T"] and a["hashes"] == b["hashes"], n


def test_recordings_cover_the_inputs():
    """what the recordings must hold: the unmeasured message classes (U-1611..U-1614) in `hole`, a save file and a load in
    `saveload`, modal dispatches in live frames in `report`"""
    if not _have("hole"):
        pytest.skip("no recording hole")
    msgs = {json.loads(l)["msg"] for l in (AR.CORPUS / "hole" / "input.jsonl").read_text().splitlines() if l.strip()}
    for m in (0x203, 0x20A, 0x104, 0x105, 0x106):     # double click, wheel, WM_SYSKEYDOWN / UP, WM_SYSCHAR
        assert m in msgs, hex(m)
    ev = [json.loads(l) for l in (AR.CORPUS / "hole" / "input.jsonl").read_text().splitlines() if l.strip()]

    def async_down(e, vk):  # window.h HostAnswer as event_json writes it: [api, a, b, result, x, y]; api 2 GetAsyncKeyState
        return any(a[0] == 2 and a[1] == vk and a[3] & 0x8000 for a in e.get("ans", []))
    shift = [e for e in ev if e["msg"] == 0x202 and async_down(e, 0x10)]
    ctrl = [e for e in ev if e["msg"] == 0x202 and async_down(e, 0x11)]
    assert shift and ctrl, (len(shift), len(ctrl))
    if _have("saveload"):
        saves = list((AR.CORPUS / "saveload" / "data").rglob("*.sve"))
        assert any(not p.name.startswith("&") and p.name.lower() != "while_browsing.sve" for p in saves), saves
    if _have("report"):
        rows = [l.split("\t") for l in (AR.CORPUS / "report" / "dispatches.tsv").read_text().splitlines()[1:]]
        assert any(int(r[1]) > 0 and r[2] == "0" for r in rows), "no modal dispatch in a live frame"


# ------------------------------------------------------------------------------------------------ negative controls
def _handler_payload_offset(trace: bytes, pred) -> tuple[int, dict]:
    import trace_format as TF
    tr = TF.parse(trace)
    recs = tr.records
    for i, r in enumerate(recs):
        if r.name == "HANDLER" and pred(r.f):
            end = recs[i + 1].offset if i + 1 < len(recs) else len(trace)
            return end - len(r.payload), r.f
    raise AssertionError("no such HANDLER record")


def _frame_of(trace: bytes, pos_offset: int) -> int:
    """the live frame a trace byte offset lies in (FRAME records before it)"""
    import trace_format as TF
    return sum(1 for r in TF.parse(trace).records if r.name == "FRAME" and r.offset < pos_offset)


def test_control_input_byte_changed(x64):
    """the tee click (rootWin_onPress1 at (300, 200)): its x's low byte 0x2c -> 0x6c (300 -> 364)"""
    if not _have("hole"):
        pytest.skip("no recording hole")
    tr = bytearray((AR.CORPUS / "hole" / "trace.trace").read_bytes())
    off, f = _handler_payload_offset(bytes(tr), lambda f: f["handler"] == 1 and f["a0"] == 300 and f["a1"] == 200)
    assert struct.unpack_from("<i", tr, off + 4)[0] == 300
    tr[off + 4] = 0x6C
    assert struct.unpack_from("<i", tr, off + 4)[0] == 364
    frame = _frame_of(bytes(tr), off)
    r = AR.gate_one(x64, "hole", trace=bytes(tr))
    assert r["status"] == "FAIL", r
    assert r["first"]["record"] >= frame - 1, (r["first"], frame)   # not before the click (record k = head of frame k + 1)
    assert r["first"]["what"] == "sim", r["first"]


@pytest.mark.parametrize("what", ["hash", "T"])
def test_control_framehash_record_changed(x64, what):
    if not _have("hole"):
        pytest.skip("no recording hole")
    fh = bytearray((AR.CORPUS / "hole" / "framehash.bin").read_bytes())
    step = 8 + 8 * 203
    k = 200
    o = k * step + (8 + 8 * 57 if what == "hash" else 0)
    fh[o] ^= 0x01
    r = AR.gate_one(x64, "hole", framehash=bytes(fh))
    assert r["status"] == "FAIL", r
    assert r["first"]["record"] == k, r["first"]
    assert r["first"]["what"] == ("sim" if what == "hash" else "T"), r["first"]


@pytest.mark.parametrize("name", REQUIRED)
def test_h1_dispatches_hold_one_mouse_message(name):
    """the original's H1 (pumpMessages: WM_MOUSEMOVE drained, then ONE PeekMessage(0x200..0x20a) through display slot +0x13c,
    jgld 0x1006b7e0) dispatches at most one mouse-class message per frame and never a key; the app's host keeps that rule
    (host.cpp), so a recording's H1 dispatch carries the commands of exactly one mouse button / wheel message"""
    if not _have(name):
        pytest.skip(f"no recording {name}")
    d = AR.CORPUS / name
    ev = {int(l.split("\t")[0]): [int(x) for x in l.split("\t")[1:]]
          for l in (d / "input_dispatch.tsv").read_text().splitlines()[1:] if l.strip()}
    n_h1 = 0
    for l in (d / "dispatches.tsv").read_text().splitlines()[1:]:
        r = [int(x) for x in l.split("\t")]
        if not (r[2] == 1 and r[3] == 1):
            continue
        n_h1 += 1
        msgs = [ev[s][0] for s in range(r[10], r[11] + 1) if s in ev and ev[s][4] > 0]
        assert len(msgs) == 1 and 0x200 <= msgs[0] <= 0x20A, (r, [hex(m) for m in msgs])
    assert n_h1 or name != "hole", name   # hole clicks in-game (P6 press, H1 release); the others click in modal screens
