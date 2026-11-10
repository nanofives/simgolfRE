"""The live run core's harness (roadmap S7.1, portsim/sim/live.h; notes log/roadmap/S7/1_notes.md).

A LIVE RUN asks a host for every outside value as the port reads it (frame-head clock, clock reads, cursor, GetAsyncKeyState,
pump batches) instead of reading a trace ahead, and records the answers as a schema v1 trace. This file runs the built-in
hosts through the C ABI (sg_live_run_builtin; the Win32 backend through the runner's op 40) and the two gates of
re/roadmap/S7_steps.tsv row S7.1:

  EQUIVALENCE: the trace host (a corpus trace answering every request) gives the per-frame T + 203 sim hashes of sg_run_boot
  over the same trace, for every boot-gate scenario; and the recording it writes replays (sg_run_boot) to the same records.

      ~/bin/py -3.12 -m port.test.live equiv <scenario>... | --all [--jobs 3] [--out log/roadmap/S7/1_equiv_x64.tsv]

  ROUND TRIP: a scripted session (input decided at run time by the script host) writes a recording that parse_trace accepts,
  and sg_run_boot over it gives the live run's own records.

      ~/bin/py -3.12 -m port.test.live roundtrip <script name>... | --all [--out ...]

Backend: SG_BACKEND=win32 (or --backend win32) runs the Win32 build through port/test/runner32.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import ctypes as C
import json
import os
import pathlib
import re
import struct
import subprocess
import sys
from dataclasses import dataclass, field

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import boot as BOOT  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import segment_files  # noqa: E402
from port.test import segments as SG  # noqa: E402
import trace_format as TF  # noqa: E402

OP_LIVE = 40   # runner32 op: live run with a built-in host (S7.7: 28..32 are S3.2's LiveTerrain ops)
KIND_TRACE, KIND_SCRIPT = 1, 2
NEGCTL = "live.recorder"   # live.cpp's switch: what + 16 * n (1 drop a clock answer, 2 swap dispatches, 3 skip a cursor answer)
#   ... 10 (roadmap S7.9) the n-th inflated dispatch recorded without its CURSOR record


@dataclass
class LiveRun:
    code: int
    records: int
    T: list
    hashes: list
    trace: bytes
    stats: dict
    detail: str


STAT_NAMES = ("violations", "first_violation", "asks", "reasks", "clocks", "cursors", "asyncs", "dispatches", "writes",
              "body_calls", "modal_calls", "paced_calls")


def _sig(dll):
    P, U32, I32, SZ = C.c_void_p, C.c_uint32, C.c_int32, C.c_size_t
    dll.sg_live_run_builtin.restype = C.c_int
    dll.sg_live_run_builtin.argtypes = [P, I32, C.c_char_p, SZ, U32, I32, C.POINTER(U32), C.POINTER(C.c_uint64), C.POINTER(I32)]
    dll.sg_live_recording.restype = C.c_int
    dll.sg_live_recording.argtypes = [P, C.c_char_p, SZ, C.POINTER(SZ)]
    dll.sg_live_stats.restype = C.c_int
    dll.sg_live_stats.argtypes = [P, C.POINTER(C.c_int64)]
    dll.sg_run_frames_detail.restype = C.c_char_p
    dll.sg_run_frames_detail.argtypes = [P]


def run_builtin(st, kind: int, data: bytes, frame_ra: int, max_records: int) -> LiveRun:
    """One live run with a built-in host on the state `st` (lib.State or lib32.State32)."""
    lib = st.lib
    nsim = len(lib.objects("sim"))
    if hasattr(lib, "runner"):   # Win32: the runner's op 40
        body = struct.pack("<iIiI", kind, frame_ra & 0xFFFFFFFF, max_records, len(data)) + bytes(data)
        code, r = st._tx(OP_LIVE, body)
        if not r.d:
            return LiveRun(code, 0, [], [], b"", {}, "")
        n = r.i32()
        ts = list(struct.unpack(f"<{n}I", r.raw(4 * n)))
        hs = struct.unpack(f"<{n * nsim}Q", r.raw(8 * n * nsim))
        rl = r.u32()
        rec = bytes(r.raw(rl))
        stats = dict(zip(STAT_NAMES, struct.unpack("<12q", r.raw(96))))
        detail = r.s()
        return LiveRun(code, n, ts, [list(hs[k * nsim:(k + 1) * nsim]) for k in range(n)], rec, stats, detail)
    dll = lib.dll
    if not getattr(dll, "_live_sig", False):
        _sig(dll)
        dll._live_sig = True
    cap = max(1, max_records)
    out_t = (C.c_uint32 * cap)()
    out_h = (C.c_uint64 * (cap * nsim))()
    done = C.c_int32(0)
    code = dll.sg_live_run_builtin(st.ctx, kind, bytes(data), len(data), frame_ra & 0xFFFFFFFF, max_records, out_t, out_h,
                                   C.byref(done))
    n = done.value
    ln = C.c_size_t(0)
    dll.sg_live_recording(st.ctx, None, 0, C.byref(ln))
    buf = C.create_string_buffer(max(1, ln.value))
    dll.sg_live_recording(st.ctx, buf, ln.value, C.byref(ln))
    stv = (C.c_int64 * 12)()
    dll.sg_live_stats(st.ctx, stv)
    detail = (dll.sg_run_frames_detail(st.ctx) or b"").decode("utf-8", "replace")
    stats = dict(zip(STAT_NAMES, list(stv)))
    if hasattr(dll, "sg_live_stats_ext"):   # roadmap S7.6 (x64 only)
        _m, ext = _marks_and_ext(st)
        stats["idle_declines"], stats["inflated"] = ext
    return LiveRun(code, n, list(out_t[:n]), [list(out_h[k * nsim:(k + 1) * nsim]) for k in range(n)], buf.raw[:ln.value],
                   stats, detail)


def saves_order_of(session: dict | None) -> list[str] | None:
    """P23B fix 3: the listing order a recording's session.json carries (`saves_newest_first`, `saves_order`: the saved games
    ranked when the session began, newest first), None for a recording made with the option off (name order)"""
    if not session or not session.get("saves_newest_first"):
        return None
    return [str(n) for n in session.get("saves_order") or []]


def boot_state(lib, scenario: str | None, saves_order: list[str] | None = None):
    """A fresh state set up as port/test/boot.py sets one up: the exe's initialised data at the original data base, the
    install (and the scenario's own files) as game files, the GDI text provider. Returns (state, cleanup)."""
    why = lib.exe_load()
    if why:
        raise RuntimeError(f"exe: {why}")
    st = lib.state()
    st.__enter__()
    st.set_data_base(SG.DATA_VA)
    st.fill_from_exe(True)
    if SG.INSTALL.is_dir():
        st.files_root(SG.INSTALL)
    if scenario:
        for fname, data in segment_files.files_for(scenario).items():
            st.files_put(fname, data)
    if saves_order is not None:   # after the fixture files: they stay unranked, as in the app
        st.files_saves_order(saves_order)
    gdi = lib.has("gditext.attach") and os.environ.get("SG_TEXT_METRICS", "gdi") == "gdi"
    if gdi:
        st.call("gditext.attach", [])

    def cleanup():
        try:
            if gdi:
                st.call("gditext.detach", [])
        finally:
            st.__exit__(None, None, None)
    return st, cleanup


def run_boot(lib, scenario: str | None, trace: bytes, max_records: int):
    st, done = boot_state(lib, scenario)
    try:
        code, n, ts, hs = st.run_boot(trace, max_records)
        detail = SG.SegmentRunner(lib)._detail(st) if hasattr(lib, "dll") else ""
        return code, n, ts, hs, detail
    finally:
        done()


def run_live(lib, scenario: str | None, kind: int, data: bytes, frame_ra: int, max_records: int) -> LiveRun:
    st, done = boot_state(lib, scenario)
    try:
        return run_builtin(st, kind, data, frame_ra, max_records)
    finally:
        done()


def first_diff(ta, ha, tb, hb, names):
    """(record, [objects]) of the first record where two runs differ in T or a sim hash, or None"""
    for k in range(min(len(ta), len(tb))):
        bad = [names[j] for j in range(len(names)) if ha[k][j] != hb[k][j]]
        if bad or ta[k] != tb[k]:
            return k, bad[:12], ta[k], tb[k]
    if len(ta) != len(tb):
        return min(len(ta), len(tb)), ["(record count)"], None, None
    return None


def frame_ra_of(trace: bytes) -> int:
    return int(TF.parse(trace, decode_fields=False).meta.get("frame_ra", 0))


# ------------------------------------------------------------------------------------------------ equivalence gate
def capture_status(scenario, code, done, n, ts, hs, exp, names) -> str:
    """port/test/boot.py run()'s verdict for these records against the capture (PASS / KNOWN / FAIL), same rules"""
    known = BOOT.KNOWN.get(scenario, [])
    seen = False
    for k in range(done):
        bad = [names[j] for j in range(len(names)) if hs[k][j] != exp[k][1][j]]
        rng = next((r for r in known if r[0] <= k < r[1] and bad and set(bad) <= set(r[2]) and ts[k] == exp[k][0]), None)
        if rng:
            seen = True
            continue
        if bad or ts[k] != exp[k][0]:
            return "FAIL"
    if code != L.SG_OK or done < n:
        return "FAIL"
    return "KNOWN" if seen else "PASS"


@dataclass
class EquivResult:
    scenario: str
    status: str            # PASS / FAIL / MISSING / NO_FRAMES (the boot gate has no record to compare)
    records: int = 0
    expected: int = 0
    boot_status: str = ""  # port/test/boot.py's status of the same trace (sg_run_boot against the capture)
    live_vs_boot: str = ""  # first difference between the live run and sg_run_boot ("" = none)
    replay_vs_live: str = ""  # first difference between sg_run_boot over the live recording and the live run
    violations: int = 0
    detail: str = ""
    extra: dict = field(default_factory=dict)


def equivalence(scenario: str, lib=None, max_records: int | None = None, negctl: int = 0, replay: bool = True) -> EquivResult:
    lib = lib or L.load()
    tr = BOOT.TRACES / f"{scenario}.trace"
    symbols, exp = BOOT.expected_records(scenario)
    if exp is None or not tr.exists():
        return EquivResult(scenario, "MISSING", detail="no trace or no capture frame hashes")
    names = [n for n, _, _ in lib.objects("sim")]
    n = len(exp) if max_records is None else min(max_records, len(exp))
    res = EquivResult(scenario, "FAIL", expected=n)
    if not exp:
        res.status = res.boot_status = "NO_FRAMES"
        res.detail = "the capture has no frame record (boot.py does not run it)"
        return res
    data = tr.read_bytes()
    code_b, nb, tsb, hsb, _ = run_boot(lib, scenario, data, n)
    if negctl:
        lib.negctl(NEGCTL, negctl)
    try:
        lv = run_live(lib, scenario, KIND_TRACE, data, frame_ra_of(data), n)
    finally:
        if negctl:
            lib.negctl(NEGCTL, 0)
    res.records = lv.records
    res.violations = lv.stats.get("violations", 0)
    res.boot_status = capture_status(scenario, code_b, nb, n, tsb, hsb, exp, names)
    res.extra = {"live_code": lv.code, "boot_code": code_b, "stats": lv.stats, "live_detail": lv.detail[:300]}
    res.extra["live_capture_status"] = capture_status(scenario, lv.code, lv.records, n, lv.T, lv.hashes, exp, names)
    d = first_diff(lv.T, lv.hashes, tsb, hsb, names)
    res.live_vs_boot = "" if d is None else json.dumps({"record": d[0], "objects": d[1], "T_live": d[2], "T_boot": d[3]})
    if replay and lv.trace:
        code_r, nr, tsr, hsr, rd = run_boot(lib, scenario, lv.trace, lv.records)
        d2 = first_diff(tsr, hsr, lv.T, lv.hashes, names)
        res.replay_vs_live = "" if d2 is None else json.dumps({"record": d2[0], "objects": d2[1], "T_replay": d2[2],
                                                               "T_live": d2[3], "code": code_r})
        res.extra["replay_code"] = code_r
    ok = not res.live_vs_boot and lv.records == nb
    res.status = "PASS" if ok else "FAIL"
    if not ok:
        res.detail = res.live_vs_boot or f"live {lv.records} records, sg_run_boot {nb}"
    return res


# ------------------------------------------------------------------------------------------------ scripted sessions
# The scripted sessions of the round-trip gate (live.cpp SCRIPT HOST syntax). Positions are root-window client coordinates
# taken from the corpus recordings' HANDLER records: the title menu's Sandbox hotspot (130, 445), the location screen's
# Monterey (270, 32), the build palette's golf icon (45, 470) and its slots (300, 527) / (365, 527) (cov_a_keys).
SCRIPTS = {
    # title -> Sandbox -> Monterey, then about 350 frames of play: Esc, the build palette, two palette slots placed, a right
    # click drops the tool, 'h', Esc, the pause key P twice, a hover point
    "sandbox_play": """
click 130 445
click 270 32
wait_frames 25
key 27
wait_frames 20
click 45 470
wait_frames 20
click 300 527
wait_frames 15
click 300 200
wait_frames 15
click 365 527
wait_frames 15
click 520 330
wait_frames 25
rclick 300 350
wait_frames 12
key 72 104
wait_frames 30
key 27
wait_frames 20
cursor 400 300
wait_frames 40
key 80 80
wait_frames 30
key 80 80
wait_frames 60
""",
    # title -> Sandbox -> Monterey, then the report keys F1 and F2 (rootWin_onChar's report switch, cov_a_keys2 opens a
    # report with F1): each waits for the report's modal loop, which takes the next input (Esc closes it)
    "report": """
click 130 445
click 270 32
wait_frames 30
vkey 112
wait_modal
key 27
wait_frames 40
vkey 113
wait_modal
key 27
wait_frames 40
""",
    # a paced modal wait: in the sandbox the '.' key reaches pumpAndRedraw (cov_a_keys frame 1853: onKey(46, 27), then
    # pumpAndRedraw's start read); the script lets 40 passes of the wait go by (declines), then Esc ends it
    "modal_wait": """
click 130 445
click 270 32
wait_frames 30
batch 7:46:27
wait_modal
wait_passes 40
key 27
wait_frames 30
batch 7:47:27
wait_modal
wait_passes 15
click 400 300
wait_frames 30
""",
    # roadmap S7.7 (U-1604): the character editor's bio EditBox, as a PREFIX session (SCRIPT_PREFIX: cov_r_bio's corpus trace
    # answers until the bio loop's first dispatch, which holds the keys its EditBox took; the script answers from that pump
    # call on). The keys as the original routes them (log/roadmap/S7/7_notes.md section 3): printable characters as WM_CHAR
    # the focus child takes (RootHandler 13), an edit key's key-down taken by it (12) with its key-up at the root's onChar0
    # (9); Back's WM_CHAR 8 and Enter's WM_CHAR 13 reach the root's onKey (7, a1 = the last key-down's virtual key, as
    # cov_r_bio's HANDLER records carry it); the second batch ends on a focus key (resolved by the record after it). Enter
    # ends the bio loop (gDirtyFlagB = 0x0d); the cursor rests on the editor's check mark for three idle passes (its hit
    # test reads the cursor before the pump), whose click goes back to the course (`idle 1`: the release wait's second
    # pumpMessages, an idle_ok call, finds nothing)
    "bio_type": """
batch 13:72:0 13:105:0 13:32:0 13:71:0 12:8:0 7:8:8 9:0:0
batch 12:37:0 9:0:0 13:88:0
batch 12:36:0 9:0:0 13:65:0 12:35:0 9:0:0
batch 13:33:0 7:13:35
wait_modal
cursor 753 213
idle 3
click 753 213
idle 1
wait_frames 40
""",
    # roadmap S7.7 (U-1604): input during a world load. Title -> Sandbox, then the Monterey click: its release starts the
    # world load (loadWorldAssets' loadFlic calls, whose doAllTasks is PumpKind::Load); `wait_load` gives the next
    # step at the first Load call the core lets deliver: 'z' (zoom in) as WM_CHAR alone (cov_r_flic_input's key, the root's
    # onKey 7 with a1 0), which mainLoop's first frame takes and which changes the camera cache; a second `wait_load` gives
    # 'x' (zoom out) at a later Load call of the same world load (two load dispatches in one load, U-1664)
    "load_input": """
click 130 445
click 270 32
wait_load
batch 7:122:0
wait_load
batch 7:120:0
wait_frames 60
key 80 80
wait_frames 30
""",
}


# the install files a script session runs with besides the user's install (segment_files.files_for of a corpus scenario):
# bio_type loads mood5.sve from the title menu's saved-game list, as cov_r_bio does (roadmap S7.7)
SCRIPT_FILES = {"bio_type": "cov_r_bio"}
# scripts that start where a corpus trace's first focus-key dispatch would be given (KIND_PREFIX, live.cpp PREFIX HOST)
SCRIPT_PREFIX = {"bio_type": "cov_r_bio"}
KIND_PREFIX = 3


def script_data(name: str) -> tuple[int, bytes]:
    """(host kind, data) of a script session: the script text, or u32 trace length + the prefix trace + the text."""
    text = SCRIPTS[name].encode("ascii")
    pre = SCRIPT_PREFIX.get(name)
    if not pre:
        return KIND_SCRIPT, text
    tr = (BOOT.TRACES / f"{pre}.trace").read_bytes()
    return KIND_PREFIX, struct.pack("<I", len(tr)) + tr + text


def scripts() -> list[str]:
    return sorted(SCRIPTS)


# ------------------------------------------------------------------------------------------------ idle passes (roadmap S7.6)
# The title menu's hover loop (tick_menu.inc: pumpMessages, View_toLocal's cursor read, the hover redraw) as a player who only
# moves the mouse: `idle N` declines N idle_ok calls of the loop, `mark` keeps the back buffer of that call ($live/mark_k.555)
# and its snapshot position (sg_live_marks). The cursor points are on the Sandbox hotspot (130, 445), between hotspots and on
# the corpus's other title clicks; then the session goes on to the sandbox as sandbox_play does.
IDLE_SCRIPT = """
cursor 130 445
idle 4
mark
cursor 400 445
idle 4
mark
cursor 650 445
idle 3
mark
cursor 130 300
idle 5
mark
cursor 700 120
idle 2
mark
cursor 130 445
idle 3
mark
click 130 445
click 270 32
wait_frames 30
"""


@dataclass
class IdleFrames:
    status: str
    records: int = 0
    marks: list = field(default_factory=list)          # snapshot positions
    live: list = field(default_factory=list)           # sha256 of each mark's back buffer in the live run
    replay: list = field(default_factory=list)         # sha256 of each gfx.snapshotAt(mark) in the replay
    distinct: int = 0                                  # distinct live frames among the marks
    idle_declines: int = 0
    inflated: int = 0
    draws_live: dict = field(default_factory=dict)     # gfx.log `handled` counts: one per drawing call the passes made
    draws_replay: dict = field(default_factory=dict)
    replay_vs_live: str = ""
    detail: str = ""


def gfx_handled(st) -> dict:
    """the gfx backend's `handled` counts (gfx.log): how many times each drawing call ran (a modal pass redraws, so the
    counts follow the number of passes a loop made)"""
    st.call("gfx.log", [])
    txt = (st.files_read("$gfx/log.txt") or b"").decode("utf-8", "replace")
    out = {}
    for line in txt.splitlines():
        f = line.split("\t")
        if len(f) == 3 and f[0] == "handled":
            out[f[1]] = int(f[2])
    return out


def _marks_and_ext(st):
    dll = st.lib.dll
    dll.sg_live_marks.restype = C.c_int32
    dll.sg_live_marks.argtypes = [C.c_void_p, C.POINTER(C.c_uint32), C.c_int32]
    dll.sg_live_stats_ext.restype = C.c_int
    dll.sg_live_stats_ext.argtypes = [C.c_void_p, C.POINTER(C.c_int64)]
    n = dll.sg_live_marks(st.ctx, None, 0)
    buf = (C.c_uint32 * max(1, n))()
    dll.sg_live_marks(st.ctx, buf, n)
    ext = (C.c_int64 * 2)()
    dll.sg_live_stats_ext(st.ctx, ext)
    return list(buf[:n]), list(ext)


def idle_frames(script: str = IDLE_SCRIPT, lib=None, negctl: int = 0, max_records: int = 5000) -> IdleFrames:
    """The script host with the gfx backend attached; then sg_run_boot over the recording with gfx.snapshotAt at every mark:
    each replay snapshot must equal the live back buffer of its mark, and the replay's records the live run's (x64 only:
    sg_live_marks is not served by the Win32 runner)."""
    import hashlib
    lib = lib or L.load()
    names = [n for n, _, _ in lib.objects("sim")]
    st, done = boot_state(lib, None)
    try:
        st.call("gfx.attach", [])
        if negctl:
            lib.negctl(NEGCTL, negctl)
        try:
            lv = run_builtin(st, KIND_SCRIPT, script.encode("ascii"), TF.FRAME_RA, max_records)
        finally:
            if negctl:
                lib.negctl(NEGCTL, 0)
        marks, ext = _marks_and_ext(st)
        live = [st.files_read(f"$live/mark_{k}.555") for k in range(len(marks))]
        draws_live = gfx_handled(st)
        st.call("gfx.detach", [])
    finally:
        done()
    res = IdleFrames("FAIL", records=lv.records, marks=marks, idle_declines=ext[0], inflated=ext[1])
    res.live = [hashlib.sha256(b).hexdigest()[:16] if b else "" for b in live]
    res.distinct = len(set(res.live))
    st, done = boot_state(lib, None)
    try:
        st.call("gfx.attach", [])
        for p in marks:
            st.call("gfx.snapshotAt", [p])
        code, n, ts, hs = st.run_boot(lv.trace, lv.records)
        rep = [st.files_read(f"$gfx/snapshot_{k}.555") for k in range(len(marks))]
        res.draws_replay = gfx_handled(st)
        st.call("gfx.detach", [])
    finally:
        done()
    res.draws_live = draws_live
    res.replay = [hashlib.sha256(b).hexdigest()[:16] if b else "" for b in rep]
    d = first_diff(ts, hs, lv.T, lv.hashes, names)
    res.replay_vs_live = "" if d is None and n == lv.records else json.dumps({"diff": d, "replay": n, "live": lv.records,
                                                                              "code": code})
    bad = [k for k in range(len(marks)) if not res.live[k] or res.live[k] != res.replay[k]]
    draws = {k: (res.draws_live.get(k), res.draws_replay.get(k)) for k in set(res.draws_live) | set(res.draws_replay)
             if res.draws_live.get(k) != res.draws_replay.get(k)}
    ok = not bad and not draws and not res.replay_vs_live and len(marks) > 0 and lv.records > 0
    res.status = "PASS" if ok else "FAIL"
    if bad:
        res.detail = f"marks whose replay snapshot differs from the live frame: {bad}"
    elif draws:
        res.detail = f"drawing calls (live, replay) differ: {sorted(draws.items())[:6]}"
    elif res.replay_vs_live:
        res.detail = res.replay_vs_live
    return res


@dataclass
class StuckReplay:
    scenario: str
    status: str
    records: int = 0
    inflated: int = -1          # live dispatches recorded with a CURSOR record in front (sg_live_stats_ext, x64 only)
    replay_stuck: int = -1      # replay deliveries at another cursor count than recorded (run_frames_detail, x64 only)
    live_vs_boot: str = ""      # first difference between the live run and sg_run_boot over the corpus trace
    replay_vs_live: str = ""
    trace: bytes = b""
    detail: str = ""


# roadmap S7.9: scenarios whose corpus trace still takes the live core's `stuck` (inflated) delivery on HEAD 78ffb06d
# (log/roadmap/S7/9_notes.md section 1; cov_a_keys' one is gone since b3d61e41)
STUCK_SCENARIOS = ("ui_tour", "ui_tour_v", "cov_r_s2_screens")
_STUCK_RE = re.compile(r"(\d+) at another cursor count")


def stuck_replay(scenario: str, lib=None, negctl: int = 0, max_records: int | None = None) -> StuckReplay:
    """roadmap S7.9 (U-1600): the trace host over a corpus trace, then sg_run_boot over the live recording. A dispatch the
    live run gave at a modal loop's second call at one cursor count is recorded with a CURSOR record in front of it, so the
    replay's TraceInput refuses it at the earlier call and delivers it at that second call through its `stuck` rule. PASS:
    the replay's frame records equal the live run's and (x64) the replay made exactly `inflated` stuck deliveries, so every
    such dispatch was taken at the call the live run gave it. The frame records alone are blind to the call: on ui_tour the
    two calls fall in one pass of a screen loop (live.recorder control 10 changes no record and no drawing call)."""
    lib = lib or L.load()
    names = [n for n, _, _ in lib.objects("sim")]
    _syms, exp = BOOT.expected_records(scenario)
    data = (BOOT.TRACES / f"{scenario}.trace").read_bytes()
    n = len(exp) if max_records is None else min(max_records, len(exp))
    if negctl:
        lib.negctl(NEGCTL, negctl)
    try:
        lv = run_live(lib, scenario, KIND_TRACE, data, frame_ra_of(data), n)
    finally:
        if negctl:
            lib.negctl(NEGCTL, 0)
    res = StuckReplay(scenario, "FAIL", records=lv.records, inflated=int(lv.stats.get("inflated", -1)), trace=lv.trace)
    _cb, _nb, tsb, hsb, _db = run_boot(lib, scenario, data, n)          # sg_run_boot over the corpus trace
    db = first_diff(lv.T, lv.hashes, tsb, hsb, names)
    res.live_vs_boot = "" if db is None else json.dumps({"record": db[0], "objects": db[1]})
    code, nr, ts, hs, det = run_boot(lib, scenario, lv.trace, lv.records)
    m = _STUCK_RE.search(det or "")
    res.replay_stuck = int(m.group(1)) if m else -1
    d = first_diff(ts, hs, lv.T, lv.hashes, names)
    res.replay_vs_live = "" if d is None and nr == lv.records else json.dumps({"diff": d, "replay": nr, "live": lv.records,
                                                                               "code": code})
    # (Win32: neither count is served, both read -1 and only the records are judged)
    ok = (not res.replay_vs_live and not res.live_vs_boot and lv.records == n and lv.code == L.SG_OK
          and lv.stats.get("violations", 0) == 0 and res.replay_stuck == res.inflated)
    res.status = "PASS" if ok else "FAIL"
    if not ok:
        res.detail = (res.live_vs_boot or res.replay_vs_live or f"live code {lv.code} records {lv.records} of {n}, violations "
                      f"{lv.stats.get('violations')}, inflated {res.inflated}, replay stuck deliveries {res.replay_stuck}")
    return res


@dataclass
class RoundTrip:
    script: str
    status: str
    records: int = 0
    dispatches: int = 0
    violations: int = 0
    parse_ok: bool = False
    validate: list = field(default_factory=list)
    replay_vs_live: str = ""
    detail: str = ""
    extra: dict = field(default_factory=dict)


PALETTE_QPC_RA = 0x8106A414   # jgld's Palette::update QPC return as a trace writes it (module 1, tick.cpp kPaletteQpcRa)


def _handler_site(ra: int) -> bool:
    site = (ra - TF.FRAME_RA) & 0xFFFFFFFF
    return 0x55951 <= site < 0x55C71 or 0x85CE1 <= site < 0x85DD1 or 0x6CB11 <= site < 0x6CCD1   # tick.cpp in_handler_code


def input_paths(trace: bytes) -> dict:
    """roadmap S7.7 (U-1604): S1's two input paths in a recording, by parse_trace's rules (tick.cpp): `focus_msgs` = key MSG
    records (0x100 / 0x102) no HANDLER follows before the next MSG / clock / FRAME record (RootHandler 12 / 13), and
    `load_dispatches` = modal dispatches whose next clock record is the palette QPC (`load`: loadFlic's Load pump takes them)."""
    recs = TF.parse(trace).records
    focus = load = 0
    pend = False
    open_batch = batch_body = False
    ctx_ra = 0
    for r in recs:
        if r.type == TF.MSG:
            focus += pend
            pend = r.f["msg"] in (0x100, 0x102)
        elif r.type == TF.HANDLER:
            pend = False
            if not open_batch:
                open_batch, batch_body = True, ctx_ra in (TF.FRAME_RA, PALETTE_QPC_RA)
        elif r.type == TF.CLOCKRUN and _handler_site(r.f["ra"]):
            pass                        # a root handler's own clock read (tick.cpp in_handler_code): the batch goes on
        elif r.type in (TF.CLOCKRUN, TF.FRAME):
            focus += pend
            pend = False
            if r.type == TF.CLOCKRUN:
                if open_batch and not batch_body and r.f["kind"] == 2 and r.f["ra"] == PALETTE_QPC_RA:
                    load += 1
                ctx_ra = r.f["ra"]
            open_batch = False
    return {"focus_msgs": focus + pend, "load_dispatches": load}


def roundtrip(name: str, lib=None, negctl: int = 0, max_records: int = 20000, keep: pathlib.Path | None = None) -> RoundTrip:
    lib = lib or L.load()
    kind, data = script_data(name)
    names = [n for n, _, _ in lib.objects("sim")]
    if negctl:
        lib.negctl(NEGCTL, negctl)
    try:
        lv = run_live(lib, SCRIPT_FILES.get(name), kind, data, TF.FRAME_RA, max_records)
    finally:
        if negctl:
            lib.negctl(NEGCTL, 0)
    res = RoundTrip(name, "FAIL", records=lv.records, dispatches=lv.stats.get("dispatches", 0),
                    violations=lv.stats.get("violations", 0))
    res.extra = {"live_code": lv.code, "stats": lv.stats, "live_detail": lv.detail[:400], "bytes": len(lv.trace)}
    if keep:
        keep.parent.mkdir(parents=True, exist_ok=True)
        keep.write_bytes(lv.trace)
    try:
        tr = TF.parse(lv.trace)
        res.parse_ok = True
        res.validate = TF.validate(tr, virtual_clock=False)
        res.extra.update(input_paths(lv.trace))
    except TF.TraceError as e:
        res.detail = f"trace_format: {e}"
        return res
    code_r, nr, tsr, hsr, rd = run_boot(lib, SCRIPT_FILES.get(name), lv.trace, lv.records)
    res.extra["replay_code"] = code_r
    res.extra["replay_detail"] = rd[:400]
    d = first_diff(tsr, hsr, lv.T, lv.hashes, names)
    res.replay_vs_live = "" if d is None else json.dumps({"record": d[0], "objects": d[1], "T_replay": d[2], "T_live": d[3]})
    ok = not res.replay_vs_live and nr == lv.records and lv.records > 0 and not res.validate
    res.status = "PASS" if ok else "FAIL"
    if not ok:
        res.detail = res.replay_vs_live or (f"validate: {res.validate[:3]}" if res.validate else
                                            f"replay {nr} records, live {lv.records}")
    return res


# ------------------------------------------------------------------------------------------------ CLI
def run_parallel(what: str, names, jobs: int, negctl: int = 0, verbose: bool = True) -> list[dict]:
    """One process per scenario (or script), `jobs` at a time; the rows as `--out -` prints them. Each process loads its
    own library, so on the Win32 backend every scenario gets a fresh runner (roadmap S7.9: one runner over all 70
    scenarios kept 16 MB per freed context and died near 1,765 MB before U-1681 was fixed; 9_notes.md section 2)."""
    names = list(names)
    cmds = {s: [sys.executable, "-m", "port.test.live", what, s, "--out", "-", "--negctl", str(negctl)] for s in names}
    rows = []
    with cf.ThreadPoolExecutor(max(1, jobs)) as ex:
        futs = {ex.submit(subprocess.run, c, capture_output=True, text=True, cwd=str(ROOT)): s for s, c in cmds.items()}
        for i, f in enumerate(cf.as_completed(futs), 1):
            s = futs[f]
            out = f.result().stdout.strip().splitlines()
            row = json.loads(out[-1]) if out and out[-1].startswith("{") else {
                "scenario" if what == "equiv" else "script": s, "status": "ERROR", "detail": f.result().stderr[-300:]}
            rows.append(row)
            if verbose:
                print(f"[{i}/{len(names)}] {s} {row['status']} {row.get('records', '')} {str(row.get('detail', ''))[:150]}",
                      flush=True)
    return rows


def _row(obj) -> dict:
    return {k: (json.dumps(v) if isinstance(v, (dict, list)) else v) for k, v in obj.__dict__.items()}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what", choices=["equiv", "roundtrip"])
    ap.add_argument("names", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--negctl", type=int, default=0)
    ap.add_argument("--backend", choices=["x64", "win32"])
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    if a.backend:
        os.environ["SG_BACKEND"] = a.backend
    names = a.names or (BOOT.scenarios() if a.what == "equiv" else scripts())
    rows = []
    if a.jobs > 1:
        rows = run_parallel(a.what, names, a.jobs, a.negctl)
    else:
        lib = L.load()
        for i, s in enumerate(names, 1):
            r = equivalence(s, lib, negctl=a.negctl) if a.what == "equiv" else roundtrip(s, lib, negctl=a.negctl)
            row = _row(r)
            rows.append(row)
            if a.out == "-":
                print(json.dumps(row))
            else:
                print(f"[{i}/{len(names)}] {s} {row['status']} {row.get('records', '')} {str(row.get('detail', ''))[:150]}",
                      flush=True)
    if a.out and a.out != "-":
        p = pathlib.Path(a.out)
        p.parent.mkdir(parents=True, exist_ok=True)
        keys = list(rows[0].keys()) if rows else []
        with p.open("w", encoding="utf-8") as f:
            f.write("\t".join(keys) + "\n")
            for r in sorted(rows, key=lambda r: str(r.get(keys[0]))):
                f.write("\t".join(str(r.get(k, "")).replace("\t", " ") for k in keys) + "\n")
        counts = {}
        for r in rows:
            counts[r["status"]] = counts.get(r["status"], 0) + 1
        print(json.dumps({"counts": counts}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
