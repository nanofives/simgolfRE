"""Roadmap S7.5: the in-game frame in the Windows app (port/app: the course drawn by LiveTerrain into the gfx root surface,
port/src/sim/terrain_live.cpp + gfx_terrain.cpp, presented with the S2 / S3 2D bodies), judged on the app executable itself.
Notes: log/roadmap/S7/5_notes.md.

GATE (re/roadmap/S7_steps.tsv row S7.5):
  (1) an in-game frame presented by the app equals the gfx back buffer of the port at the same record: an app session driven
      only by posted window messages (port/app/tools/app_drive.py; title -> Sandbox -> Monterey, then camera moves, zoom and
      a hole built) with LiveTerrain attached (`--terrain live`, the default); every present equals the snapshot a replay of
      the session's own recording (sg_run_boot with gfx and LiveTerrain attached, gfx.snapshotAt at the present's recorded
      position) takes, and the in-game presents hold terrain pixels (terrain passes ran, the view region is not blank);
  (2) framegate's in-game checkpoints that PASS under S3 also PASS when produced through the app's live path from the same
      trace: `simgolf_port.exe --live-trace <scenario>` (the live core over the corpus trace, LiveTracePresenter: presents
      where a session makes them) with gfx.snapshotAt at each checkpoint's ordinal; each in-game checkpoint's snapshot is
      compared with the original's frame by framegate's regions (PASS = 0 px), and every in-game snapshot of the judged
      scenarios equals framegate's own snapshot (sg_run_boot with LiveTerrain) in every pixel and the app's present that
      showed it;
  (3) the frame period under in-game load: 87 ms (the original's H1 wait: frame head + 87 ms in waitTicks(1, 0) steps of 10
      ms) within one quantum and the host's timer resolution, over the in-game frames of (1), terrain passes included.

Controls (RED): the terrain body family's pass skipped in the app only (`--negctl reg:gfx.family.terrain=3`): the presents
differ from the replay's snapshots; the same switch on the --live-trace path: the in-game snapshots differ from framegate's;
pacing off in game (`--negctl pacing-off`): periods below 87 ms; `--terrain headless`: the in-game view region holds no terrain.

    cd port/build/S7_5_head && SG_APP_EXE=<simgolf_port.exe> SG_PORTSIM_DLL=<portsim_c.dll> \\
        ~/bin/py -3.12 -m pytest port/test/test_app_ingame.py -p no:cacheprovider -v
"""
from __future__ import annotations

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
for _p in (str(ROOT), str(ROOT / "re" / "tools"), str(ROOT / "port" / "app" / "tools")):
    if _p not in sys.path:
        sys.path.insert(0, _p)
APP = ROOT / "port" / "app"
BUILD = ROOT / "port" / "build" / "S7_5_app"
INSTALL = ROOT / "original"
W, H = 800, 600
QUANTUM = 10                # one waitTicks(1, 0) step (pacing.h)
JITTER = 2                  # timeGetTime / Sleep granularity at timeBeginPeriod(1)
FRAME_KINDS = "rw"          # S7.8: a frame's own presents: at Window::refreshFull ('r', the original's present point) or the H1 wait ('w')
SNAP_CHUNK = 60             # gfx.snapshotAt requests per replay run (each snapshot is 960,000 bytes in memory)
# the scenarios whose in-game checkpoints the app's live path is judged on (2): the ones framegate passes in game under S3
# (5_notes.md section 2 lists the census rows) and two with the course view on screen at every in-game checkpoint
LIVE_TRACE_SCENARIOS = ("cov_a_keys", "cov_k1_routing", "sandbox_basic", "course_season")
# S7.8 (S7.5 wrote 5_app_ingame_facts.json). Roadmap S3.12 (U-1398): SG_APP_FACTS_DIR=<dir> writes the facts file there instead, so
# a mirror's run (whose log/ is a junction to the shared one) leaves the tracked file alone; S3.10's private copy of this module
# had hard-coded log/roadmap/S3/10_app_ingame_facts_app.json
FACTS = pathlib.Path(os.environ.get("SG_APP_FACTS_DIR") or ROOT / "log" / "roadmap" / "S7") / "8_app_ingame_facts.json"


def _sources():
    out = [p for p in APP.rglob("*") if p.suffix in (".cpp", ".h", ".txt")]
    for d in ("src", "include"):
        out += [p for p in (ROOT / "port" / d).rglob("*") if p.suffix in (".cpp", ".h", ".inc")]
    return out


def _paths() -> tuple[pathlib.Path, pathlib.Path]:
    """the app and the C ABI of the same build: $SG_APP_EXE + $SG_PORTSIM_DLL, else the standalone build in BUILD"""
    if os.environ.get("SG_APP_EXE") and os.environ.get("SG_PORTSIM_DLL"):
        return pathlib.Path(os.environ["SG_APP_EXE"]), pathlib.Path(os.environ["SG_PORTSIM_DLL"])
    return BUILD / "Release" / "simgolf_port.exe", BUILD / "portsim" / "Release" / "portsim_c.dll"


@pytest.fixture(scope="module")
def exe():
    if not (INSTALL / "golf_clean.exe").exists():
        pytest.skip("no install (original/golf_clean.exe)")
    exe, dll = _paths()
    if not (os.environ.get("SG_APP_EXE") and os.environ.get("SG_PORTSIM_DLL")):
        if not B.toolchain_ok():
            pytest.skip("no CMake / Visual Studio 2022 Build Tools")
        newest = max(s.stat().st_mtime for s in _sources())
        if not exe.exists() or not dll.exists() or min(exe.stat().st_mtime, dll.stat().st_mtime) < newest:
            cm = B.cmake_exe()
            subprocess.run([cm, "-S", str(APP), "-B", str(BUILD), "-A", "x64", "-DPORTAPP_SHELL=ON"], check=True,
                           capture_output=True)
            r = subprocess.run([cm, "--build", str(BUILD), "--config", "Release", "--target", "simgolf_port", "portsim_c", "--",
                                "/m"], capture_output=True, text=True)
            assert r.returncode == 0, r.stdout[-3000:]
    assert exe.exists() and dll.exists(), (exe, dll)
    os.environ["SG_PORTSIM_DLL"] = str(dll)  # replays and framegate run the same build
    return exe


def _lib():
    from port.test import lib as L
    return L.load(os.environ["SG_PORTSIM_DLL"])


def _rgb(raw: bytes) -> np.ndarray:
    import imgcmp
    return imgcmp.rgb555_to_rgb(np.frombuffer(raw, "<u2").reshape(H, W))


def _rows(p: pathlib.Path) -> list[list[str]]:
    if not p.exists():
        return []
    return [l.split("\t") for l in p.read_text().splitlines()[1:] if l.strip()]


# ------------------------------------------------------------------------------------------------ (2) the live path over a trace
def checkpoint_plan(scenario: str) -> dict:
    """framegate.run_scenario's checkpoint ordinals (the trace ordinal of each checkpoint's clock call), context and record
    budget, from the same inputs (the corpus trace, the det_b summary)"""
    import trace_format as TF
    from port.test import boot as BT
    from port.test import framegate as FG
    from port.test import gfx as GX
    tr = TF.read(BT.TRACES / f"{scenario}.trace")
    tgt, k = {}, 0
    for ordinal, _ra, _slot, kind, value in TF.clock_calls(tr):
        if kind == 1:  # timeGetTime (framegate's rule)
            tgt[k] = (ordinal, value)
            k += 1
    summ = json.loads((GX.REFS / scenario / "det_b" / "summary.json").read_text())
    rows = []
    for c in FG.scenario_checkpoints(scenario):
        st = summ["checkpoints"][c]["status"]
        if st["tgt"] in tgt and tgt[st["tgt"]][1] == st["T"]:
            o = tgt[st["tgt"]][0]
            rows.append(dict(FG.classify(tr, o), checkpoint=c, ordinal=o))
    rows.sort(key=lambda r: r["ordinal"])
    frames_pos = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    no_frames = not (BT.expected_records(scenario)[1] or [])
    max_records = (sum(1 for p in frames_pos if p <= rows[-1]["ordinal"]) + 2) if rows else 1
    to_end = no_frames or max_records > len(frames_pos)
    return {"rows": rows, "max_records": -1 if to_end else max_records}


def app_live_trace(exe, scenario: str, plan: dict, extra=()) -> dict:
    """one `simgolf_port.exe --live-trace` run with the scenario's install files: session.json, the presents, the snapshots"""
    from port.test import segment_files
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_5_lt_"))
    try:
        args = [str(exe), "--install", str(INSTALL), "--data", str(tmp / "data"), "--record-dir", str(tmp / "rec"),
                "--live-trace", str(ROOT / "port" / "corpus" / "traces" / f"{scenario}.trace"),
                "--snapshots", ",".join(str(r["ordinal"]) for r in plan["rows"])]
        if plan["max_records"] > 0:
            args += ["--max-records", str(plan["max_records"])]
        for i, (name, data) in enumerate(segment_files.files_for(scenario).items()):
            p = tmp / "puts" / f"{i}.bin"
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
            args += ["--put", f"{name}={p}"]
        t0 = time.time()
        r = subprocess.run(args + list(extra), cwd=str(ROOT), capture_output=True, text=True, timeout=3600)
        rec = tmp / "rec"
        out = {"rc": r.returncode, "stdout": r.stdout[-2000:], "stderr": r.stderr[-2000:], "seconds": round(time.time() - t0, 1),
               "session": json.loads((rec / "session.json").read_text()) if (rec / "session.json").exists() else {},
               "presents": [{"index": int(x[0]), "kind": x[1], "frame": int(x[2]), "snaps": int(x[3]), "hash": int(x[4], 16)}
                            for x in _rows(rec / "presents.tsv")],
               "gfx_log": (rec / "gfx_log.txt").read_text() if (rec / "gfx_log.txt").exists() else "",
               "snapshots": {}}
        for k, row in enumerate(plan["rows"]):
            p = rec / f"snapshot_{k}.555"
            out["snapshots"][row["checkpoint"]] = p.read_bytes() if p.exists() else None
        return out
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def framegate_frames(scenario: str, checkpoints: list[str]) -> tuple[list[dict], dict]:
    """framegate's own run (sg_run_boot, gfx + LiveTerrain): its rows and its port frame per checkpoint (RGB)"""
    from PIL import Image

    from port.test import framegate as FG
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_5_fg_"))
    try:
        rows = FG.run_scenario(scenario, checkpoints, lib=_lib(), images=tmp, probe=False)
        imgs = {}
        for c in checkpoints:
            p = tmp / f"{scenario}.{c}.port.png"
            imgs[c] = np.asarray(Image.open(p).convert("RGB")) if p.exists() else None
        return rows, imgs
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def judge_live_trace(exe, scenario: str, extra=()) -> list[dict]:
    """per in-game checkpoint: the app path's verdict against the original (framegate's regions), its pixels against
    framegate's own frame, and the present that showed it"""
    from port.test import framegate as FG
    from port.test import gfx as GX
    lib = _lib()
    plan = checkpoint_plan(scenario)
    app = app_live_trace(exe, scenario, plan, extra)
    glog = FG.parse_gfx_log(app["gfx_log"])
    fg_rows, fg_imgs = framegate_frames(scenario, [r["checkpoint"] for r in plan["rows"]])
    fg_by = {r["checkpoint"]: r for r in fg_rows}
    out = []
    for k, row in enumerate(plan["rows"]):
        c = row["checkpoint"]
        kind = FG.screen_kind(row["context"], row["frame"], row["wait_ticks"], glog["handled"])
        raw = app["snapshots"].get(c)
        res = {"scenario": scenario, "checkpoint": c, "kind": kind, "context": row["context"],
               "framegate_status": fg_by.get(c, {}).get("status", "?"), "app_rc": app["rc"],
               "app_stop": app["session"].get("stop"), "app_code": app["session"].get("code")}
        if raw is None or len(raw) != W * H * 2:
            res.update(status="NO_SNAPSHOT")
            out.append(res)
            continue
        img = _rgb(raw)
        ref = GX.reference(scenario, c)
        rn = FG.region_numbers(img, ref, kind)
        res["app_px"] = sum(v["px"] for v in rn.values())
        fgi = fg_imgs.get(c)
        res["px_vs_framegate"] = None if fgi is None else int((img != fgi).any(axis=2).sum())
        # the present that showed it: the last present made before the snapshot appeared
        h = lib.hash_bytes(raw)
        before = [p for p in app["presents"] if p["snaps"] <= k]
        res["shown_by_present"] = bool(before) and before[-1]["hash"] == h
        res["present_kind"] = before[-1]["kind"] if before else ""
        res["status"] = "PASS" if res["app_px"] == 0 else "DIFF"
        out.append(res)
    return out


# ------------------------------------------------------------------------------------------------ (1) + (3) an in-game session
VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN = 0x25, 0x26, 0x27, 0x28


def script_ingame(d):
    """title -> Sandbox -> Monterey (app_replay's walk), then in-game load: the camera moved with the arrow keys (each a
    terrain strip render, 4 tiles), the zoom keys, and one hole built (app_replay.script_hole's clicks)"""
    from port.test import app_replay as AR
    AR._to_monterey(d)
    for vk in (VK_LEFT, VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN, VK_DOWN, VK_RIGHT):
        d.key(vk, tag=f"camera {vk:#x}")
        d.wait_frames(10)
    for ch in ("X", "Z", "X", "Z"):
        d.char(ch, tag=f"zoom {ch}")
        d.wait_frames(12)
    d.move(45, 470); d.wait_frames(3)
    d.click(45, 470, tag="Build Course palette"); d.wait_frames(12)
    d.move(300, 527); d.wait_frames(3)
    d.click(300, 527, tag="tee tool"); d.wait_frames(8)
    d.move(300, 200); d.wait_frames(3)
    d.click(300, 200, tag="tee"); d.wait_frames(12)
    d.move(365, 527); d.wait_frames(3)
    d.click(365, 527, tag="green tool"); d.wait_frames(8)
    d.move(520, 330); d.wait_frames(3)
    d.click(520, 330, tag="green"); d.wait_frames(20)
    d.move(300, 350); d.wait_frames(3)
    d.click(300, 350, right=True, tag="drop tool"); d.wait_frames(10)
    d.key(ord("H"), tag="open hole"); d.wait_frames(30)
    d.key(AR.VK_ESCAPE, tag="Esc"); d.wait_frames(60)


class Session:
    """one windowed app run driven by `script(driver)` (posted messages only), closed; then its recording"""

    def __init__(self, exe, script, extra=()):
        import app_drive

        from port.test import app_replay as AR
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_5_"))
        self.rec = self.tmp / "rec"
        args = ["--install", str(INSTALL), "--data", str(self.tmp / "data"), "--record-dir", str(self.rec), "--no-activate",
                "--dpi", "96", "--cursor", "message", "--keys", "message", "--clock-origin", str(AR.CLOCK_ORIGIN), *extra]
        self.app = app_drive.launch(exe, args, cwd=str(ROOT))
        self.error = ""
        try:
            script(AR.Driver(self.app, self.tmp / "data"))
        except Exception as e:  # noqa: BLE001 - reported by the tests
            self.error = f"{type(e).__name__}: {e}"
        finally:
            self.rc = self.app.close(timeout=120)
        self.session = json.loads((self.rec / "session.json").read_text())
        self.presents = [{"index": int(r[0]), "mark": int(r[1]), "pos": int(r[2]), "kind": r[3], "frame": int(r[4]),
                          "hash": int(r[6], 16)} for r in _rows(self.rec / "presents.tsv")]
        self.shown = [(int(r[0]), int(r[1], 16)) for r in _rows(self.rec / "shown.tsv")]
        self.heads = [tuple(int(x) for x in l.split("\t")) for l in (self.rec / "frames.tsv").read_text().splitlines()
                      if l.strip()]
        self.waits = [[int(x) for x in l.split("\t")] for l in (self.rec / "waits.tsv").read_text().splitlines() if l.strip()]
        self.facts: dict = {}

    def records(self):
        nsim = len(_lib().objects("sim"))
        raw = (self.rec / "framehash.bin").read_bytes()
        step = 8 + 8 * nsim
        return [(struct.unpack_from("<I", raw, o)[0], list(struct.unpack_from(f"<{nsim}Q", raw, o + 8)))
                for o in range(0, len(raw), step)]

    def cleanup(self):
        if os.environ.get("S7_5_KEEP"):  # development: keep the session directory
            print("kept", self.tmp)
            return
        shutil.rmtree(self.tmp, ignore_errors=True)


def _judged(presents):
    """roadmap S7.9 (U-1685): gfx.snapshotAt resolves one clock ordinal, so of two presents the app made at one recording
    position (no clock read between their refreshes) a replay can show only the last. Since the gfx refresh hook (U-1670) the
    boot makes one such pair: showTitleScreen's refresh, then the first loading screen's. Returns (the last present at each
    position: judged, the others)."""
    last = {}
    for p in presents:
        last[p["pos"]] = p
    return [p for p in presents if last[p["pos"]] is p], [p for p in presents if last[p["pos"]] is not p]


def replay(s: Session, positions: list[int], terrain: bool = True) -> dict:
    """sg_run_boot over the session's recording with gfx attached and (terrain) LiveTerrain at the app's address 0: the
    records and the sgh64 of the gfx snapshot at each recording position; the last snapshot's raw bytes"""
    from port.test import live as LV
    from port.test import terrainlive as TL
    lib = _lib()
    trace = (s.rec / "trace.trace").read_bytes()
    nrec = len(s.records())
    chunks = [positions[i:i + SNAP_CHUNK] for i in range(0, len(positions), SNAP_CHUNK)] or [[]]
    hashes, out, last = [], {}, None
    for ci, chunk in enumerate(chunks):
        st, done = LV.boot_state(lib, None)
        try:
            st.call("gfx.attach", [])
            for p in chunk:
                st.call("gfx.snapshotAt", [p])
            if terrain:
                assert TL.live_attach(lib, st, 0) == 0
            code, n, ts, hs = st.run_boot(trace, nrec + 1)
            for k in range(len(chunk)):
                raw = st.files_read(f"$gfx/snapshot_{k}.555")
                hashes.append(lib.hash_bytes(raw) if raw else None)
                if raw:
                    last = raw
            if ci == 0:
                out.update(code=code, records=n, T=ts, hashes=hs)
            if terrain:
                TL.live_detach(lib, st)
            st.call("gfx.detach", [])
        finally:
            done()
    out["snapshots"] = hashes
    out["last"] = last
    return out


def _periods(heads, lo, hi):
    by = {f: t for f, t, _fl in heads}
    return [by[k + 1] - by[k] for k in range(lo, hi) if k in by and k + 1 in by]


def _ingame_from(s: Session) -> int:
    """the first live frame judged in game: the title menu, the location screen and the world load run in live frame 0
    (the boot step: modal loops only, no frame head), so every frame head is in game; the first 5 are left out (the world
    load's present and the welcome message). Independent of waits.tsv, which a pacing-off control does not write"""
    return (s.heads[0][0] + 5) if s.heads else 0


def _stats(d):
    return {"n": len(d), "min": min(d), "max": max(d), "median": statistics.median(d), "mean": round(sum(d) / len(d), 2),
            "within_quantum": round(sum(1 for x in d if x <= 87 + QUANTUM + JITTER) / len(d), 4)} if d else {}


def _pass_frames(s: Session, lo: int) -> list[tuple[int, int, int]]:
    """(live frame, terrain passes in it, work ms) for the in-game frame waits (waits.tsv columns 0, 6 delta, 1)"""
    return [(s.waits[i][0], s.waits[i][6] - s.waits[i - 1][6], s.waits[i][1]) for i in range(1, len(s.waits))
            if s.waits[i][0] >= lo and len(s.waits[i]) >= 8]


def _save_facts(key: str, value):
    if FACTS.parent.is_dir():
        old = json.loads(FACTS.read_text()) if FACTS.exists() else {}
        old[key] = value
        FACTS.write_text(json.dumps(old, indent=1))


@pytest.fixture(scope="module")
def ingame(exe):
    s = Session(exe, script_ingame)
    lo = _ingame_from(s)
    pf = _pass_frames(s, lo)
    s.facts = {"session": s.session.get("session"), "error": s.error, "ingame_from": lo,
               "periods": _stats(_periods(s.heads, lo, len(s.heads))), "work_ms": _stats([w for (_f, _n, w) in pf]),
               "work_ms_frames_with_terrain_passes": _stats([w for (_f, n, w) in pf if n > 0]),
               "frames_with_terrain_passes": sum(1 for (_f, n, _w) in pf if n > 0),
               "terrain_passes": s.waits[-1][6] if s.waits else 0, "terrain_pixels": s.waits[-1][7] if s.waits else 0,
               "kinds": {k: sum(1 for p in s.presents if p["kind"] == k) for k in "rmwp"}, "presents": len(s.presents)}
    yield s
    _save_facts("ingame_session", s.facts)
    s.cleanup()


def test_ingame_session_ran(ingame):
    ss = ingame.session["session"]
    assert ingame.error == "" and ingame.rc == 0 and ingame.session["error"] == "", (ingame.error, ingame.session)
    assert ss["code"] == 0 and ss["stop"] == "host" and ss["violations"] == 0, ss
    lo = _ingame_from(ingame)
    assert lo < len(ingame.heads) - 300, (lo, len(ingame.heads))          # >= 300 in-game frames
    # the terrain was drawn in game: passes ran on many frames (each camera move, the zoom, the edits)
    assert ingame.facts["frames_with_terrain_passes"] >= 8, _pass_frames(ingame, lo)


def test_recording_replays(ingame):
    """sg_run_boot over the session's trace gives the session's framehash in every record (LiveTerrain attached on both
    sides: the simulation is the one without it, S3.2)"""
    rec = ingame.records()
    r = replay(ingame, [])
    assert r["code"] == 0 and r["records"] == len(rec)
    bad = [k for k in range(len(rec)) if rec[k][0] != r["T"][k] or rec[k][1] != r["hashes"][k]]
    assert not bad, bad[:5]


def test_ingame_presents_equal_the_port_back_buffer(ingame):
    """gate (1): every present (title, location, in game) = the replay's gfx back buffer at its recorded position, LiveTerrain
    attached in both; and every frame the window took is its present's frame"""
    judged, shared = _judged(ingame.presents)
    assert [p["index"] for p in shared] == [0], shared      # the boot's title present only (U-1685)
    r = replay(ingame, [p["pos"] for p in judged])
    bad = [p["index"] for p, h in zip(judged, r["snapshots"]) if h != p["hash"]]
    assert not bad, (len(bad), bad[:10])
    lo = _ingame_from(ingame)
    # a present is made only when the back buffer changed: the frames of an idle course view present nothing new
    assert sum(1 for p in ingame.presents if p["kind"] in FRAME_KINDS and p["frame"] >= lo) >= 50
    by_index = {p["index"]: p["hash"] for p in ingame.presents}
    assert ingame.shown and not [(i, h) for i, h in ingame.shown if by_index.get(i) != h]


def test_ingame_presents_hold_the_terrain(ingame):
    """the last in-game present is LiveTerrain's drawing: the replay without LiveTerrain (headless terrain) gives another
    frame at that position, differing in the course view"""
    last_w = [p for p in ingame.presents if p["kind"] in FRAME_KINDS][-1]
    with_t = replay(ingame, [last_w["pos"]])
    without = replay(ingame, [last_w["pos"]], terrain=False)
    assert with_t["snapshots"][0] == last_w["hash"]
    a, b = _rgb(with_t["last"]), _rgb(without["last"])
    view = int((a[0:450] != b[0:450]).any(axis=2).sum())            # the rows above the toolbar
    ingame.facts["view_px_live_vs_headless"] = view
    assert view > 50_000, view


def _frame_periods(s: Session, lo: int) -> list[dict]:
    """per in-game live frame k: its period (frame head k + 1 - frame head k), its frame wait row (None: the frame made no
    H1 wait, the original's control flow: a build click's frame, U-0651), work ms, waitTicks steps, terrain passes"""
    by = {f: t for f, t, _fl in s.heads}
    waits = {w[0]: w for w in s.waits}
    prev = {s.waits[i][0]: s.waits[i - 1][6] for i in range(1, len(s.waits)) if len(s.waits[i]) >= 8}
    out = []
    for k in sorted(by):
        if k < lo or k + 1 not in by:
            continue
        w = waits.get(k)
        out.append({"frame": k, "period": by[k + 1] - by[k], "waited": w is not None, "work": w[1] if w else None,
                    "ticks": w[4] if w else None, "passes": (w[6] - prev.get(k, w[6])) if w and len(w) >= 8 else 0})
    return out


def test_pacing_under_ingame_load(ingame):
    """gate (3), the original's frame rule in game, terrain passes included: a frame that waited and whose work fits the 87 ms
    holds 87 ms + at most one 10 ms waitTicks quantum + the timer resolution; a frame whose work passed 87 ms makes no
    waitTicks step and is not slept (period = its work + the head's own reads); the frames that made no H1 wait are the
    build clicks' frames (the original's control flow)"""
    lo = _ingame_from(ingame)
    fp = _frame_periods(ingame, lo)
    fits = [r for r in fp if r["waited"] and r["work"] < 87]
    over = [r for r in fp if r["waited"] and r["work"] >= 87]
    nowait = [r for r in fp if not r["waited"]]
    ingame.facts.update(pacing_fits=_stats([r["period"] for r in fits]), pacing_over=[(r["frame"], r["work"], r["period"],
                        r["passes"]) for r in over], pacing_nowait=[(r["frame"], r["period"]) for r in nowait])
    assert len(fits) >= 300, len(fits)
    assert min(r["period"] for r in fits) >= 87, min(r["period"] for r in fits)
    within = sum(1 for r in fits if r["period"] <= 87 + QUANTUM + JITTER) / len(fits)
    assert within >= 0.95, (within, sorted(r["period"] for r in fits)[-20:])
    assert statistics.median(r["period"] for r in fits) <= 87 + QUANTUM + JITTER
    assert all(r["ticks"] == 0 and r["period"] <= r["work"] + 2 * QUANTUM for r in over), over
    assert all(r["passes"] > 0 for r in over), over          # only terrain-pass frames overrun
    assert len(nowait) <= 8, nowait                          # the five build clicks of the script


# roadmap S3.12: U-1640 resolved by S3 wave 6 (S3.10 workers, S3.10b state rebuild, the LiveTerrain log switch): zoom frames 41..49 ms
# at 84..97 % load (log/roadmap/S3/12_notes.md), so the xfail of S7.5 is gone
def test_terrain_pass_frames_fit_the_frame(ingame):
    """gate (3) strictly: no in-game frame overruns 87 ms, the ones that rasterise terrain passes included"""
    over = [r for r in _frame_periods(ingame, _ingame_from(ingame)) if r["waited"] and r["work"] >= 87]
    assert not over, [(r["frame"], r["work"], r["passes"]) for r in over]


# ------------------------------------------------------------------------------------------------ (2) the live path, judged
@pytest.fixture(scope="module")
def judged(exe):
    rows = []
    for sc in LIVE_TRACE_SCENARIOS:
        rows += judge_live_trace(exe, sc)
    _save_facts("live_trace", rows)
    return rows


def test_live_path_ran(judged):
    assert judged and all(r["app_rc"] == 0 and r["app_code"] == 0 for r in judged), [r for r in judged if r["app_rc"]][:3]


def test_ingame_framegate_pass_rows_pass_on_the_app_path(judged):
    """gate (2): every in-game checkpoint framegate passes passes through the app's live path (0 px against the original)"""
    fg_pass = [r for r in judged if r["kind"] == "ingame" and r["framegate_status"] == "PASS"]
    assert len(fg_pass) >= 3, [(r["scenario"], r["checkpoint"]) for r in fg_pass]
    bad = [(r["scenario"], r["checkpoint"], r.get("app_px")) for r in fg_pass if r["status"] != "PASS"]
    assert not bad, bad


def test_app_path_equals_framegate_and_its_present(judged):
    """every checkpoint of the judged scenarios: the app path's frame = framegate's frame in every pixel, and = the present
    the app made just before the snapshot (what the window showed there)"""
    bad = [(r["scenario"], r["checkpoint"], r.get("px_vs_framegate"), r.get("shown_by_present")) for r in judged
           if r["status"] == "NO_SNAPSHOT" or r["px_vs_framegate"] != 0 or not r["shown_by_present"]]
    assert not bad, bad
    assert sum(1 for r in judged if r["kind"] == "ingame") >= 20


# ------------------------------------------------------------------------------------------------ controls (RED)
def _short(d):
    from port.test import app_replay as AR
    AR._to_monterey(d)
    for vk in (VK_LEFT, VK_UP):
        d.key(vk, tag="camera")
        d.wait_frames(20)


def test_control_terrain_pass_skipped_in_the_app(exe):
    """the terrain family's passes skipped in the app only (gfx_terrain.cpp negctl 3): in-game presents differ from the
    replay's back buffer"""
    s = Session(exe, _short, extra=["--negctl", "reg:gfx.family.terrain=3"])
    try:
        ws = [p for p in s.presents if p["kind"] in FRAME_KINDS and p["frame"] >= _ingame_from(s) - 5]
        r = replay(s, [p["pos"] for p in ws])
        assert ws and [p for p, h in zip(ws, r["snapshots"]) if h != p["hash"]]      # RED
    finally:
        s.cleanup()


def test_control_terrain_pass_skipped_on_the_live_path(exe):
    """the same switch on --live-trace: sandbox_basic's in-game snapshots differ from framegate's"""
    rows = judge_live_trace(exe, "sandbox_basic", extra=["--negctl", "reg:gfx.family.terrain=3"])
    ing = [r for r in rows if r["kind"] == "ingame"]
    assert ing and all((r.get("px_vs_framegate") or 0) > 0 for r in ing), ing    # RED


def test_control_pacing_off_in_game(exe):
    s = Session(exe, _short, extra=["--negctl", "pacing-off"])
    try:
        per = _periods(s.heads, _ingame_from(s), len(s.heads))
        assert len(per) >= 20 and statistics.median(per) < 87, (len(per), statistics.median(per) if per else None)  # RED
    finally:
        s.cleanup()


def test_control_headless_terrain_in_the_app(exe):
    """`--terrain headless`: no terrain pass in the session (waits.tsv), and the in-game present is not the LiveTerrain
    replay's frame"""
    s = Session(exe, _short, extra=["--terrain", "headless"])
    try:
        assert s.waits and all(w[6] == 0 for w in s.waits)      # no terrain drawn
        last_w = [p for p in s.presents if p["kind"] in FRAME_KINDS][-1]
        assert replay(s, [last_w["pos"]])["snapshots"][0] != last_w["hash"]   # RED
    finally:
        s.cleanup()
