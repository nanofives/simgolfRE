"""Roadmap S7.6: the app on the live core (port/app/host.h: the simulation on its own thread through portsim::run_live, real
window input, real-time pacing, presents, idle passes, every session recorded), judged on its own standalone build of the app
and the C ABI (`cmake -S port/app -DPORTAPP_SHELL=ON`, port/build/S7_6_app). Notes: log/roadmap/S7/6_notes.md.

ONE MAIN SESSION (module fixture `main`), driven only by real window messages posted by port/app/tools/app_drive.py:
  - the app starts with `--install <dir>` (plus a temporary data directory, 96 dpi, the cursor taken from mouse messages);
    the title menu is the first modal present and the window's client pixels equal the original's sandbox_basic `menu`
    checkpoint (log/c3/alt/refs, gfx.reference);
  - title idle: process CPU time over 10 s with no input (blocking pumps sleep);
  - hover: three cursor moves and nothing else; each one gives a new present (the hover follows the cursor), and the client
    pixels equal the presented frame under the presenter's mapping;
  - Sandbox (130, 445), Monterey (270, 32), then frames until FRAMES frame heads, with g_flags bit 17 set from live frame
    SLOW[0] to SLOW[1] (--flag17: a harness write the recording carries);
  - the window is closed: the session ends at the next frame head.
Then, from the session directory: periods between frame heads (87 ms, 125 with bit 17, at most one 10 ms waitTicks quantum
over); every frame the window took equals the back buffer of its present (shown.tsv against presents.tsv); sg_run_boot over
the session's trace.trace gives the session's framehash.bin in every record; the replay with gfx.snapshotAt at every present's
recorded position gives the presented frame (U-1601: the idle passes replay, and the gfx backend's drawing-call counts of the
replay equal the session's, so the replay made the same passes).

Controls (each its own short session): pacing off (`--negctl pacing-off`: frame periods below 87 ms), a dropped input batch in
the recording (`recorder:<6 + 16 n>`: the replay diverges), a stale present (`present-late`: the window shows the previous
present's frame), an idle pass left out of the recording (`recorder:<5 + 16 n>`: the replay makes one pass fewer).

    cd port/build/S7_6_head && ~/bin/py -3.12 -m pytest port/test/test_app_live.py -p no:cacheprovider -v
"""
from __future__ import annotations

import ctypes as C
import ctypes.wintypes as W
import json
import os
import pathlib
import shutil
import statistics
import struct
import subprocess
import sys
import tempfile
import time

import numpy as np
import pytest

from port.test import build as B

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
APP = ROOT / "port" / "app"
BUILD = ROOT / "port" / "build" / "S7_6_app"
EXE = BUILD / "Release" / "simgolf_port.exe"
DLL = BUILD / "portsim" / "Release" / "portsim_c.dll"
INSTALL = ROOT / "original"
FRAMES = 420                # frame heads of the main session
SLOW = (250, 310)           # live frames with g_flags bit 17 (125 ms frames)
HOVER = ((650, 445), (130, 445), (400, 445))   # another item, Sandbox, no item (the start frame hovers no item)
QUANTUM = 10                # one waitTicks(1, 0) step (pacing.h)
JITTER = 2                  # timeGetTime / Sleep granularity at timeBeginPeriod(1)
SNAP_CHUNK = 100            # gfx.snapshotAt requests per replay run (each snapshot is 960,000 bytes in memory)


def _sources():
    out = [p for p in APP.rglob("*") if p.suffix in (".cpp", ".h", ".txt")]
    for d in ("src", "include"):
        out += [p for p in (ROOT / "port" / d).rglob("*") if p.suffix in (".cpp", ".h", ".inc")]
    return out


@pytest.fixture(scope="module")
def exe():
    if not B.toolchain_ok():
        pytest.skip("no CMake / Visual Studio 2022 Build Tools")
    if not (INSTALL / "golf_clean.exe").exists():
        pytest.skip("no install (original/golf_clean.exe)")
    newest = max(s.stat().st_mtime for s in _sources())
    if not EXE.exists() or not DLL.exists() or min(EXE.stat().st_mtime, DLL.stat().st_mtime) < newest:
        cm = B.cmake_exe()
        subprocess.run([cm, "-S", str(APP), "-B", str(BUILD), "-A", "x64", "-DPORTAPP_SHELL=ON"], check=True,
                       capture_output=True)
        r = subprocess.run([cm, "--build", str(BUILD), "--config", "Release", "--target", "simgolf_port", "portsim_c", "--",
                            "/m"], capture_output=True, text=True)
        assert r.returncode == 0, r.stdout[-3000:]
    os.environ["SG_PORTSIM_DLL"] = str(DLL)  # the replay runs the same build
    return EXE


# ------------------------------------------------------------------------------------------------ session helpers
def _rows(p: pathlib.Path) -> list[list[str]]:
    if not p.exists():
        return []
    return [l.split("\t") for l in p.read_text().splitlines()[1:] if l.strip()]


def _frames_tsv(rec: pathlib.Path) -> list[tuple[int, int, int]]:
    p = rec / "frames.tsv"
    if not p.exists():
        return []
    return [tuple(int(x) for x in l.split("\t")) for l in p.read_text().splitlines() if l.strip()]


def _wait_quiet(p: pathlib.Path, quiet: float, timeout: float, minimum: int = 1) -> int:
    """until the table at p has at least `minimum` rows and no new row for `quiet` seconds"""
    end = time.time() + timeout
    last, t_last = -1, time.time()
    while time.time() < end:
        n = len(_rows(p))
        if n != last:
            last, t_last = n, time.time()
        elif n >= minimum and time.time() - t_last >= quiet:
            return n
        time.sleep(0.05)
    return last


def _wait_rows(p: pathlib.Path, n: int, timeout: float) -> int:
    end = time.time() + timeout
    while time.time() < end and len(_rows(p)) < n:
        time.sleep(0.05)
    return len(_rows(p))


def _wait_frames(rec: pathlib.Path, n: int, timeout: float) -> int:
    end = time.time() + timeout
    while time.time() < end and len(_frames_tsv(rec)) < n:
        time.sleep(0.2)
    return len(_frames_tsv(rec))


def _cpu_seconds(pid: int) -> float:
    k = C.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.restype = W.HANDLE
    k.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    k.GetProcessTimes.argtypes = [W.HANDLE] + [C.POINTER(W.FILETIME)] * 4
    k.CloseHandle.argtypes = [W.HANDLE]
    h = k.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    ft = [W.FILETIME() for _ in range(4)]
    assert k.GetProcessTimes(h, *[C.byref(f) for f in ft])
    k.CloseHandle(h)

    def v(f):
        return ((f.dwHighDateTime << 32) | f.dwLowDateTime) / 1e7
    return v(ft[2]) + v(ft[3])


def _scaled(frame: np.ndarray, cw: int, ch: int) -> np.ndarray:
    """the client image the presenter must show for an 800 x 600 RGB frame (present.cpp fit_viewport + floor mapping)"""
    if cw * 600 >= ch * 800:
        w, h = ch * 800 // 600, ch
        ox, oy = (cw - w) // 2, 0
    else:
        w, h = cw, cw * 600 // 800
        ox, oy = 0, (ch - h) // 2
    out = np.zeros((ch, cw, 3), np.uint8)
    out[oy:oy + h, ox:ox + w] = frame[(np.arange(h) * 600) // h][:, (np.arange(w) * 800) // w]
    return out


def _rgb(raw: bytes) -> np.ndarray:
    import imgcmp
    return imgcmp.rgb555_to_rgb(np.frombuffer(raw, "<u2").reshape(600, 800))


class Session:
    """one app run: launched, driven by `steps(app, session)`, closed; then its directory"""

    def __init__(self, exe, steps, extra=(), cursor="message"):
        import app_drive
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_6_"))
        self.rec = self.tmp / "rec"
        args = ["--install", str(INSTALL), "--data", str(self.tmp / "data"), "--record-dir", str(self.rec), "--no-activate",
                "--dpi", "96", "--cursor", cursor, *extra]
        self.facts: dict = {}
        self.app = app_drive.launch(exe, args, cwd=str(ROOT))
        try:
            steps(self.app, self)
        finally:
            self.rc = self.app.close(timeout=60)
        self.session = json.loads((self.rec / "session.json").read_text())
        self.presents = [{"index": int(r[0]), "mark": int(r[1]), "pos": int(r[2]), "kind": r[3], "frame": int(r[4]),
                          "hash": int(r[6], 16)} for r in _rows(self.rec / "presents.tsv")]
        self.shown = [(int(r[0]), int(r[1], 16)) for r in _rows(self.rec / "shown.tsv")]
        self.heads = _frames_tsv(self.rec)

    def shown_hash(self) -> int | None:
        return self.shown[-1][1] if self.shown else None

    def records(self):
        """framehash.bin: [(T, [sim hashes])] (the captures' layout)"""
        from port.test import lib as L
        nsim = len(L.load(str(DLL)).objects("sim"))
        raw = (self.rec / "framehash.bin").read_bytes()
        step = 8 + 8 * nsim
        out = []
        for o in range(0, len(raw), step):
            t, _k = struct.unpack_from("<II", raw, o)
            out.append((t, list(struct.unpack_from(f"<{nsim}Q", raw, o + 8))))
        return out

    def cleanup(self):
        shutil.rmtree(self.tmp, ignore_errors=True)


def _to_title(app, s, title_frame=True):
    """roadmap S3.14 (U-1396): until the window shows the title menu's frame (past the boot's loading screens), then quiet.
    title_frame=False for the present-late control, whose window shows each frame only at the next present"""
    from port.test import app_replay as AR
    if title_frame:
        AR.wait_shown_hash(s.rec, AR.title_menu_hash(), 120)
    s.facts["title_rows"] = _wait_quiet(s.rec / "shown.tsv", 1.5, 120)


def _to_game(app, s, frames):
    app.click(130, 445)                                     # Sandbox Mode
    _wait_quiet(s.rec / "shown.tsv", 1.5, 60)
    app.click(270, 32)                                      # Monterey
    s.facts["frames_seen"] = _wait_frames(s.rec, frames, 30 + frames * 0.2)


def _live_terrain(lib, st):
    """roadmap S7.5: the app draws the course with LiveTerrain at address 0 (main.cpp --terrain live, the default), so a replay
    that rebuilds its frames attaches the same (gfx.attach, gfx.snapshotAt, then the terrain, as port/test/framegate.py)"""
    from port.test import terrainlive as TL
    assert TL.live_attach(lib, st, 0) == 0


def replay(s: Session, snapshots: list[int] | None = None, draws: bool = False):
    """sg_run_boot over the session's recording (x64, the app's own build): the records, the gfx snapshots at the given
    recording positions (sgh64 of each) and, with draws, the gfx backend's `handled` counts"""
    from port.test import lib as L
    from port.test import live as LV
    lib = L.load(str(DLL))
    trace = (s.rec / "trace.trace").read_bytes()
    nrec = len(s.records())
    snaps = list(snapshots or [])
    chunks = [snaps[i:i + SNAP_CHUNK] for i in range(0, len(snaps), SNAP_CHUNK)] or [[]]
    hashes: list[int | None] = []
    out = {}
    for ci, chunk in enumerate(chunks):
        st, done = LV.boot_state(lib, None)
        try:
            st.call("gfx.attach", [])
            for p in chunk:
                st.call("gfx.snapshotAt", [p])
            _live_terrain(lib, st)
            code, n, ts, hs = st.run_boot(trace, nrec + 1)
            for k in range(len(chunk)):
                raw = st.files_read(f"$gfx/snapshot_{k}.555")
                hashes.append(lib.hash_bytes(raw) if raw else None)
            if ci == 0:
                out.update(code=code, records=n, T=ts, hashes=hs)
                if draws:
                    out["draws"] = LV.gfx_handled(st)
            st.call("gfx.detach", [])
        finally:
            done()
    out["snapshots"] = hashes
    return out


def _judged(presents):
    """roadmap S7.9 (U-1685): gfx.snapshotAt resolves one clock ordinal, so of two presents the app made at one recording
    position (no clock read between their refreshes) a replay can show only the last. Since the gfx refresh hook (U-1670) the
    boot makes one such pair: showTitleScreen's refresh, then the first loading screen's. Returns (the last present at each
    position: judged, the others)."""
    last = {}
    for p in presents:
        last[p["pos"]] = p
    return [p for p in presents if last[p["pos"]] is p], [p for p in presents if last[p["pos"]] is not p]


def _live_draws(s: Session) -> dict:
    out = {}
    for line in (s.rec / "gfx_log.txt").read_text().splitlines():
        f = line.split("\t")
        if len(f) == 3 and f[0] == "handled":
            out[f[1]] = int(f[2])
    return out


def _periods(heads, lo, hi):
    """frame-head periods T[k + 1] - T[k] for live frames k in [lo, hi)"""
    by = {f: t for f, t, _fl in heads}
    return [by[k + 1] - by[k] for k in range(lo, hi) if k in by and k + 1 in by]


# ------------------------------------------------------------------------------------------------ the main session
@pytest.fixture(scope="module")
def main(exe):
    from port.test import gfx

    def steps(app, s):
        _to_title(app, s)
        s.facts["title_img"] = app.capture()
        s.facts["modules"] = app.modules()
        s.facts["title_shown"] = _rows(s.rec / "shown.tsv")[-1][1]
        c0, w0 = _cpu_seconds(app.proc.pid), time.time()
        time.sleep(10)
        c1, w1 = _cpu_seconds(app.proc.pid), time.time()
        s.facts["idle_cpu"] = 100.0 * (c1 - c0) / (w1 - w0)
        s.facts["hover"] = []
        for x, y in HOVER:
            n0 = len(_rows(s.rec / "shown.tsv"))
            app.move(x, y)
            _wait_rows(s.rec / "shown.tsv", n0 + 1, 10)
            _wait_quiet(s.rec / "shown.tsv", 1.0, 10)
            rows = _rows(s.rec / "shown.tsv")
            s.facts["hover"].append((x, y, len(rows) - n0, rows[-1][1], app.capture()))
        _to_game(app, s, FRAMES)

    s = Session(exe, steps, extra=["--flag17", f"{SLOW[0]}:{SLOW[1]}"])
    s.reference = gfx.reference("sandbox_basic", "menu")
    yield s
    # the measured facts (log/roadmap/S7/6_notes.md cites them)
    normal = _periods(s.heads, 5, SLOW[0] - 1) + _periods(s.heads, SLOW[1] + 1, len(s.heads))
    slow = _periods(s.heads, SLOW[0] + 1, SLOW[1] - 1)

    def stats(d):
        return {"n": len(d), "min": min(d), "max": max(d), "median": statistics.median(d), "mean": round(sum(d) / len(d), 2),
                "within_quantum": round(sum(1 for x in d if x <= 87 + QUANTUM + JITTER) / len(d), 4)} if d else {}
    waits = [[int(x) for x in l.split("\t")] for l in (s.rec / "waits.tsv").read_text().splitlines() if l.strip()] \
        if (s.rec / "waits.tsv").exists() else []
    facts = {"idle_cpu_pct": round(s.facts.get("idle_cpu", -1), 2), "normal": stats(normal), "slow": stats(slow),
             "work_ms": stats([w[1] for w in waits]), "wait_end_ms": stats([w[3] for w in waits]),
             "kinds": {k: sum(1 for p in s.presents if p["kind"] == k) for k in "rmwp"}, "shown": len(s.shown),
             "hover_new_presents": [h[2] for h in s.facts.get("hover", [])], "session": s.session["session"]}
    # S7.8 (S7.6 wrote 6_app_live_facts.json); roadmap S3.12 (U-1398): SG_APP_FACTS_DIR=<dir> writes it there instead
    out = pathlib.Path(os.environ.get("SG_APP_FACTS_DIR") or ROOT / "log" / "roadmap" / "S7") / "8_app_live_facts.json"
    if out.parent.is_dir():
        out.write_text(json.dumps(facts, indent=1))
    s.cleanup()


def test_session_ran(main):
    ss = main.session["session"]
    assert main.rc == 0 and main.session["error"] == "", main.session
    assert ss["code"] == 0 and ss["stop"] == "host", ss
    assert ss["violations"] == 0, ss
    assert ss["records"] >= FRAMES - 1 and len(main.heads) >= FRAMES, (ss["records"], len(main.heads))
    assert ss["dispatches"] >= 4                       # the two clicks, press and release each
    assert main.session["input_events"] >= len(HOVER) + 4
    # no original binary was loaded (S7.3's check, on the live path)
    names = {"golf_clean.exe", "golf.exe", "golf_v100_clean.exe", "jgld.dll", "jgl.dll", "terrain.dll", "sound.dll",
             "binkw32.dll", "drvmgt.dll"}
    inst = str(INSTALL.resolve()).lower()
    bad = [m for m in main.facts["modules"] if m.lower().startswith(inst) or pathlib.Path(m).name.lower() in names]
    assert main.facts["modules"] and not bad, bad


def test_title_menu_is_the_original_frame(main):
    """the first screen that waits for input is the title menu: client pixels = the original's `menu` checkpoint"""
    img = main.facts["title_img"]
    assert img.shape == main.reference.shape
    assert int((img != main.reference).any(axis=2).sum()) == 0


def test_title_idle_cpu(main):
    """a blocking pump sleeps on input: under 5 % of one core over 10 s with no input on the title menu"""
    assert main.facts["idle_cpu"] < 5.0, main.facts["idle_cpu"]


def test_hover_follows_the_cursor(main):
    """only cursor moves: each one gives a new present (the hover redraw of an idle pass), the hover frames differ from one
    another, and the client shows exactly the presented frame"""
    hv = main.facts["hover"]
    assert all(n >= 1 for (_x, _y, n, _h, _img) in hv), [(x, y, n) for x, y, n, _h, _i in hv]
    hashes = [int(h, 16) for (_x, _y, _n, h, _i) in hv]
    assert len(set(hashes)) == len(hv)
    # S7.8: the menu loop's pass presents at its Window::refreshFull ('r'), before the pump ('m') finds nothing new
    assert all(next(p["kind"] for p in main.presents if p["hash"] == h) in ("r", "m") for h in hashes)
    # the presented frames, rebuilt by a replay of the recording (gfx.snapshotAt at their positions; _hover_frames checks
    # each snapshot's sgh64 against the present's), against the client pixels captured at each point
    for (x, y, _n, _h, img), fr in zip(hv, _hover_frames(main, hashes)):
        want = _scaled(fr, img.shape[1], img.shape[0])
        assert int((img != want).any(axis=2).sum()) == 0, (x, y)


def _hover_frames(s: Session, hashes: list[int]) -> list[np.ndarray]:
    """the RGB frames of the presents with these hashes, from a replay of the session (snapshots at their positions)"""
    from port.test import lib as L
    from port.test import live as LV
    lib = L.load(str(DLL))
    pos = [next(p["pos"] for p in s.presents if p["hash"] == h) for h in hashes]
    order = sorted(range(len(pos)), key=lambda i: pos[i])
    st, done = LV.boot_state(lib, None)
    try:
        st.call("gfx.attach", [])
        for i in order:
            st.call("gfx.snapshotAt", [pos[i]])
        _live_terrain(lib, st)
        st.run_boot((s.rec / "trace.trace").read_bytes(), len(s.records()) + 1)
        raws = [st.files_read(f"$gfx/snapshot_{k}.555") for k in range(len(order))]
        st.call("gfx.detach", [])
    finally:
        done()
    out: list[np.ndarray | None] = [None] * len(pos)
    for k, i in enumerate(order):
        assert raws[k] is not None and lib.hash_bytes(raws[k]) == hashes[i]
        out[i] = _rgb(raws[k])
    return out


def test_shown_frames_are_the_presents(main):
    """every frame the window thread took is the back buffer the sim thread presented (index and sgh64)"""
    by_index = {p["index"]: p["hash"] for p in main.presents}
    assert main.shown and len(main.presents) >= len(main.shown)
    bad = [(i, h) for i, h in main.shown if by_index.get(i) != h]
    assert not bad, bad[:5]
    kinds = {p["kind"] for p in main.presents}
    assert "r" in kinds and kinds <= set("rwmp"), kinds  # S7.8: 'r' = Window::refreshFull, the original's present point


def test_pacing_held(main):
    """frame heads held by the original's frame wait: 87 ms (125 with g_flags bit 17) + at most one waitTicks quantum, over
    >= 300 frames (the first 5 after the world load and the two switch frames left out)"""
    normal = _periods(main.heads, 5, SLOW[0] - 1) + _periods(main.heads, SLOW[1] + 1, len(main.heads))
    slow = _periods(main.heads, SLOW[0] + 1, SLOW[1] - 1)
    assert len(normal) >= 300, len(normal)
    assert min(normal) >= 87, min(normal)
    assert statistics.median(normal) <= 87 + QUANTUM + JITTER, statistics.median(normal)
    within = sum(1 for d in normal if d <= 87 + QUANTUM + JITTER) / len(normal)
    assert within >= 0.90, (within, sorted(normal)[-20:])
    assert len(slow) >= 50 and min(slow) >= 125, (len(slow), min(slow) if slow else None)
    assert statistics.median(slow) <= 125 + QUANTUM + JITTER, statistics.median(slow)
    flags = {f: fl for f, _t, fl in main.heads}
    assert flags[SLOW[0]] & (1 << 17) and not flags[SLOW[1]] & (1 << 17)


def test_recording_replays(main):
    """sg_run_boot over the session's trace gives the session's framehash in every record (T + 203 sim hashes)"""
    rec = main.records()
    r = replay(main)
    assert r["code"] == 0, r["code"]
    assert r["records"] == len(rec)
    bad = [k for k in range(len(rec)) if rec[k][0] != r["T"][k] or rec[k][1] != r["hashes"][k]]
    assert not bad, bad[:5]


def test_presents_replay(main):
    """every present's frame is the replay's gfx back buffer at that present's recorded position, and the replay makes the
    session's drawing calls (U-1601: the idle passes are recorded, so the replay takes the same passes)"""
    judged, shared = _judged(main.presents)
    assert [p["index"] for p in shared] == [0], shared      # the boot's title present only (U-1685)
    r = replay(main, [p["pos"] for p in judged], draws=True)
    bad = [p["index"] for p, h in zip(judged, r["snapshots"]) if h != p["hash"]]
    assert not bad, (len(bad), bad[:10])
    assert r["draws"] == _live_draws(main)
    assert main.session["session"]["idle_declines"] >= len(HOVER)


# ------------------------------------------------------------------------------------------------ controls (RED)
def test_control_pacing_off(exe):
    s = Session(exe, lambda app, s: (_to_title(app, s), _to_game(app, s, 80)), extra=["--negctl", "pacing-off"])
    try:
        d = _periods(s.heads, 5, len(s.heads))
        assert len(d) >= 40
        assert statistics.median(d) < 87, statistics.median(d)   # RED: the frame wait is gone
    finally:
        s.cleanup()


def test_control_dropped_batch(exe):
    """the 4th dispatch the app gives (Monterey's release, onPress1 + onUnpoint) applied but left out of the recording: the
    replay stays on the location screen. (Leaving out the 3rd, its press, is an equivalent mutant: the location screen takes
    the click from the release, 6_notes.md.)"""
    s = Session(exe, lambda app, s: (_to_title(app, s), _to_game(app, s, 40)), extra=["--negctl", f"recorder:{6 + 16 * 3}"])
    try:
        rec = s.records()
        assert len(rec) >= 30
        r = replay(s)
        same = r["records"] == len(rec) and all(rec[k][0] == r["T"][k] and rec[k][1] == r["hashes"][k] for k in range(len(rec)))
        assert not same     # RED
    finally:
        s.cleanup()


def test_control_stale_present(exe):
    """present-late: the window shows the previous present's frame; shown.tsv and the client pixels disagree with the
    presents"""
    def steps(app, s):
        _to_title(app, s, title_frame=False)
        app.move(*HOVER[0])
        _wait_quiet(s.rec / "shown.tsv", 1.0, 10, minimum=2)
        s.facts["img"] = app.capture()
        _to_game(app, s, 20)
    s = Session(exe, steps, extra=["--negctl", "present-late"])
    try:
        by_index = {p["index"]: p["hash"] for p in s.presents}
        bad = [(i, h) for i, h in s.shown if by_index.get(i) != h]
        assert bad      # RED
    finally:
        s.cleanup()


def test_control_idle_pass_left_out(exe):
    """the cursor read after the 2nd idle pass is not recorded: the replay makes one pass fewer (its drawing-call counts
    differ from the session's)"""
    def steps(app, s):
        _to_title(app, s)
        for x, y in HOVER:
            n0 = len(_rows(s.rec / "shown.tsv"))
            app.move(x, y)
            _wait_rows(s.rec / "shown.tsv", n0 + 1, 10)
        _to_game(app, s, 30)
    s = Session(exe, steps, extra=["--negctl", f"recorder:{5 + 16 * 1}"])
    try:
        r = replay(s, draws=True)
        assert s.session["session"]["idle_declines"] >= 2
        assert r["draws"] != _live_draws(s)     # RED
    finally:
        s.cleanup()


def test_plain_install_start_reaches_title(exe):
    """`simgolf_port.exe --install <dir>` (the system cursor, the default data directory replaced by a temporary one): the
    session reaches a modal present (the title menu) and closes cleanly at a pump"""
    import app_drive
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_6_plain_"))
    try:
        app = app_drive.launch(exe, ["--install", str(INSTALL), "--data", str(tmp / "data"), "--no-activate"], cwd=str(ROOT))
        rec = app.record
        try:
            n = _wait_quiet(rec / "shown.tsv", 1.5, 120)
        finally:
            rc = app.close(timeout=60)
        s = json.loads((rec / "session.json").read_text())
        assert rc == 0 and n >= 1 and s["session"]["stop"] == "host" and s["session"]["code"] == 0, s
        assert s["session"]["modal_calls"] >= 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
