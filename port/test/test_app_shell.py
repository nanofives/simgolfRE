"""Roadmap S7.3: the Windows app shell (port/app: window, presentation, input capture, saves overlay, pacing) judged on its own
build of the app (simgolf_port.exe) and of the C ABI (portsim_c.dll), both from one standalone CMake tree
(`cmake -S port/app -DPORTAPP_SHELL=ON`, port/build/S7_3_app).

  1. PRESENT: `--present-trace sandbox_basic --snapshot-at <menu's clock ordinal>` runs the port from process start over the
     corpus trace with the gfx backend and the GDI text provider attached, then shows the back buffer. The frame equals the
     original's `menu` checkpoint (log/c3/alt/refs/sandbox_basic/det_b/menu.png) and port/test/gfx.py's run of the same
     checkpoint (0 px), its record 0 equals the capture's 203 sim hashes, and the window's CLIENT pixels, read from the window
     DC by port/app/tools/app_drive.py, equal the frame scaled with the presenter's mapping at 100 %, 150 % and the monitor's
     own dpi (at 100 % they are the reference pixels themselves).
  2. INPUT: app_drive.py posts real window messages (moves, left / right clicks, keys, a character) to the window; every
     posted message is logged in order with the lParam it was posted with, the app's steps equal an offline re-translation
     by input_map (`--translate`, answers replayed), and each click's handler call carries the logical point Python computes
     from the posted client point (floor(x * 800 / w), floor(y * 600 / h)).
  3. SAVES: the cov_r_quit boot run (title -> two sessions -> exit, which writes top10.sve, Themes/Default.pro and
     &QuitSave.sve) through the app writes those files under its data directory with the original's compared bytes, the
     install's tree digest is unchanged, `--cat` reads back what was written, and a data directory inside the install is
     refused without creating it.
  4. PACING and the overlay / presenter units: `--selftest` (37 rows, "[k/n] name PASS").
  5. No original binary in the app's module list (EnumProcessModulesEx).

Controls (all RED): --negctl present-stale (the first, black frame stays), present-swap (RGB555 bytes swapped), input-bypass
(commands built from the raw message), and the selftest's pacing-no-bit17 / pacing-signed / pacing-quantum, saves-install /
saves-noreadback / saves-inside, present-stale / present-swap.

    ~/bin/py -3.12 -m pytest port/test/test_app_shell.py -p no:cacheprovider
"""
from __future__ import annotations

import hashlib
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile

import numpy as np
import pytest

from port.test import build as B

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
APP = ROOT / "port" / "app"
BUILD = ROOT / "port" / "build" / "S7_3_app"
EXE = BUILD / "Release" / "simgolf_port.exe"
DLL = BUILD / "portsim" / "Release" / "portsim_c.dll"
INSTALL = ROOT / "original"
SCEN, CKPT = "sandbox_basic", "menu"
ORIGINAL_MODULES = {"golf_clean.exe", "golf.exe", "golf_v100_clean.exe", "jgld.dll", "jgl.dll", "terrain.dll", "sound.dll",
                    "binkw32.dll", "drvmgt.dll"}


def _sources():
    out = [p for p in APP.rglob("*") if p.suffix in (".cpp", ".h", ".txt")]
    for d in ("src", "include"):
        out += [p for p in (ROOT / "port" / d).rglob("*") if p.suffix in (".cpp", ".h", ".inc")]
    return out


@pytest.fixture(scope="session")
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
    os.environ["SG_PORTSIM_DLL"] = str(DLL)  # the Python side (gfx.py / boot.py) loads the same build
    return EXE


@pytest.fixture()
def tmp(request):
    d = pathlib.Path(tempfile.mkdtemp(prefix="s7_3_"))
    yield d
    shutil.rmtree(d, ignore_errors=True)


def _menu_args():
    from port.test import boot, gfx
    import trace_format as TF
    p = gfx.checkpoint_ordinal(SCEN, CKPT)
    tr = TF.read(boot.TRACES / f"{SCEN}.trace")
    frames = sum(1 for r in tr.records if r.type == TF.FRAME and r.f["pos"] <= p)
    return ["--present-trace", SCEN, "--snapshot-at", str(p), "--max-records", str(frames + 1)]


def _base(tmp, extra=()):
    return ["--install", str(INSTALL), "--data", str(tmp / "data"), "--record-dir", str(tmp / "rec"), *extra]


def _run(exe, args, check=True):
    r = subprocess.run([str(exe), *args], capture_output=True, text=True, cwd=str(ROOT), timeout=900)
    if check:
        assert r.returncode == 0, (r.returncode, r.stdout[-2000:], r.stderr[-2000:])
    return r


def _frame(rec: pathlib.Path) -> np.ndarray:
    import imgcmp
    words = np.frombuffer((rec / "back.555").read_bytes(), "<u2").reshape(600, 800)
    return imgcmp.rgb555_to_rgb(words)


def _fit(cw, ch):
    """present.cpp fit_viewport: the 4:3 content rectangle (ox, oy, w, h)"""
    if cw * 600 >= ch * 800:
        w = ch * 800 // 600
        return (cw - w) // 2, 0, w, ch
    h = cw * 600 // 800
    return 0, (ch - h) // 2, cw, h


def _scaled(frame: np.ndarray, cw: int, ch: int) -> np.ndarray:
    """The client image the presenter must show: logical pixel (floor((x - ox) * 800 / w), floor((y - oy) * 600 / h))"""
    ox, oy, w, h = _fit(cw, ch)
    out = np.zeros((ch, cw, 3), np.uint8)
    xs = (np.arange(w) * 800) // w
    ys = (np.arange(h) * 600) // h
    out[oy:oy + h, ox:ox + w] = frame[ys][:, xs]
    return out


def _monitor_dpi() -> int:
    import ctypes
    ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    return int(ctypes.windll.user32.GetDpiForSystem())


# ------------------------------------------------------------------------------------------------ 4. unit tests
def test_selftest_all_rows_pass(exe):
    r = _run(exe, ["--selftest"])
    rows = [l for l in r.stdout.splitlines() if l.startswith("[")]
    assert len(rows) == 37 and all(l.endswith(" PASS") for l in rows), r.stdout


SELFTEST_CONTROLS = {
    "pacing-no-bit17": {"pacing.deadline_125_with_bit17", "pacing.frame_125ms_reads_135"},
    "pacing-signed": {"pacing.wrap_unsigned_no_wait"},
    "pacing-quantum": {"pacing.frame_87ms_reads_99", "pacing.waitTicks1_10ms_11_reads"},
    "saves-install": {"saves.write_lands_in_data_dir", "saves.install_unchanged"},
    "saves-noreadback": {"saves.read_back", "saves.shadow_install_file"},
    "saves-inside": {"saves.refuse_data_inside_install", "saves.refuse_data_equal_install"},
    "present-stale": {"present.mapping_800x600", "present.mapping_2000x1500"},
    "present-swap": {"present.mapping_800x600", "present.mapping_1200x900"},
}


@pytest.mark.parametrize("negctl", sorted(SELFTEST_CONTROLS))
def test_selftest_controls_read_red(exe, negctl):
    r = _run(exe, ["--selftest", "--negctl", negctl], check=False)
    failed = {l.split()[1] for l in r.stdout.splitlines() if l.startswith("[") and l.endswith(" FAIL")}
    assert r.returncode != 0 and SELFTEST_CONTROLS[negctl] <= failed, (negctl, failed)


# ------------------------------------------------------------------------------------------------ 1. present, headless
def test_present_trace_equals_reference_and_port(exe, tmp):
    import imgcmp
    from port.test import boot, gfx
    _run(exe, _base(tmp, _menu_args() + ["--no-window"]))
    rec = tmp / "rec"
    app = _frame(rec)
    assert imgcmp.exact_diff(app, gfx.reference(SCEN, CKPT))["total"] == 0
    run = gfx.render(SCEN, [CKPT])
    assert run.frames[CKPT] is not None and int((run.frames[CKPT] != app).any(axis=2).sum()) == 0
    syms, exp = boot.expected_records(SCEN)
    fh = (rec / "framehash.bin").read_bytes()
    assert json.loads((rec / "framehash.json").read_text())["symbols"] == syms
    n = len(syms)
    assert len(fh) == 8 + 8 * n
    assert struct.unpack_from("<I", fh, 0)[0] == exp[0][0]
    assert list(struct.unpack_from("<%dQ" % n, fh, 8)) == exp[0][1]
    s = json.loads((rec / "session.json").read_text())
    assert s["code"] == 0 and s["records"] == 1 and s["error"] == ""


# ------------------------------------------------------------------------------------------------ 1 + 2 + 5. the window
def _window_run(exe, tmp, dpi, negctl=None, script=True):
    import app_drive
    args = _base(tmp, _menu_args() + ["--no-activate"] + (["--dpi", str(dpi)] if dpi else []) +
                 (["--negctl", negctl] if negctl else []))
    app = app_drive.launch(exe, args, cwd=str(ROOT))
    try:
        img = app.capture()
        mods = app.modules()
        if script:
            app_drive.default_script(app)
        events = app.wait_events(len(app.posted)) if script else []
    finally:
        rc = app.close()
    return app, img, mods, events, rc


def _dpis():
    native = _monitor_dpi()
    return sorted({96, 144, native})


@pytest.mark.parametrize("dpi", _dpis())
def test_window_presents_and_translates(exe, tmp, dpi):
    import app_drive
    app, img, mods, events, rc = _window_run(exe, tmp, dpi)
    assert rc == 0
    cw, ch = app.ready["client"]
    assert (cw, ch) == ((800 * dpi + 48) // 96, (600 * dpi + 48) // 96) and app.ready["dpi"] == dpi
    frame = _frame(tmp / "rec")
    want = _scaled(frame, cw, ch)
    assert img.shape == want.shape and int((img != want).any(axis=2).sum()) == 0, "client pixels differ from the frame"
    if dpi == 96:
        from port.test import gfx
        assert int((img != gfx.reference(SCEN, CKPT)).any(axis=2).sum()) == 0
    # 5. no original binary loaded
    inst = str(INSTALL.resolve()).lower()
    bad = [m for m in mods if m.lower().startswith(inst) or pathlib.Path(m).name.lower() in ORIGINAL_MODULES]
    assert not bad, bad
    # 2. input: every posted message in order, the offline translation equal, the click points independent of the app
    assert app_drive.matched(app.posted, events) == len(app.posted), (len(app.posted), len(events))
    r = _run(exe, ["--translate", str(tmp / "rec" / "input.jsonl"), "--out", str(tmp / "offline.jsonl")])
    assert "mismatches 0" in r.stdout, r.stdout
    ox, oy, w, h = _fit(cw, ch)
    clicks = 0
    for e in events:
        if e["msg"] in (0x201, 0x202, 0x204, 0x205):
            x, y = e["lp"] & 0xFFFF, e["lp"] >> 16
            lx, ly = ((x - ox) * 800) // w, ((y - oy) * 600) // h
            handler = {0x201: 2, 0x202: 1, 0x204: 5, 0x205: 4}[e["msg"]]
            assert [handler, lx, ly] in [c[:3] for c in e["cmds"]], (e, lx, ly)
            clicks += 1
        if e["msg"] == 0x101:
            assert [c[0] for c in e["cmds"]] == [9]
        if e["msg"] == 0x100:
            assert len(e["cmds"]) == 1 and e["cmds"][0][0] in (7, 8)
    assert clicks == 10


# ------------------------------------------------------------------------------------------------ window controls
def test_control_present_stale_reads_red(exe, tmp):
    app, img, *_ = _window_run(exe, tmp, 96, "present-stale", script=False)
    assert int((img != _scaled(_frame(tmp / "rec"), *app.ready["client"])).any(axis=2).sum()) > 0


def test_control_present_swap_reads_red(exe, tmp):
    from port.test import gfx
    app, img, *_ = _window_run(exe, tmp, 96, "present-swap", script=False)
    assert int((img != gfx.reference(SCEN, CKPT)).any(axis=2).sum()) > 0


def test_control_input_bypass_reads_red(exe, tmp):
    import app_drive
    app, img, mods, events, rc = _window_run(exe, tmp, 144, "input-bypass")
    r = _run(exe, ["--translate", str(tmp / "rec" / "input.jsonl")], check=False)
    assert r.returncode != 0 and "mismatches 0" not in r.stdout, r.stdout


# ------------------------------------------------------------------------------------------------ 3. saves
def _tree_digest(root: pathlib.Path) -> str:
    h = hashlib.sha256()
    for p in sorted(root.rglob("*")):
        if p.is_file():
            h.update(str(p.relative_to(root)).encode())
            h.update(hashlib.sha256(p.read_bytes()).digest())
    return h.hexdigest()


def test_saves_quit_run_writes_only_the_data_dir(exe, tmp):
    from port.test import segment_files, sve_refs
    end, start = sve_refs.load("cov_r_quit", "end"), sve_refs.load("cov_r_quit", "run_a")
    if end is None or start is None:
        pytest.skip("no collected cov_r_quit files (~/bin/py -3.12 -m port.test.sve_refs collect cov_r_quit)")
    want = {k: v for k, v in end.items() if start.get(k) != v or k == "top10.sve"}
    puts = []
    for name, data in segment_files.files_for("cov_r_quit").items():
        f = tmp / "put" / name.replace("/", "__")
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_bytes(data)
        puts += ["--put", f"{name}={f}"]
    before = _tree_digest(INSTALL)
    _run(exe, _base(tmp, ["--present-trace", "cov_r_quit", "--no-window", *puts]))
    assert _tree_digest(INSTALL) == before, "the install changed"
    files = tmp / "data" / "files"
    assert {"top10.sve", "Themes/Default.pro", "&QuitSave.sve"} <= set(want), sorted(want)
    for k, v in want.items():
        disk = files / sve_refs.logical(k)
        assert disk.is_file(), f"{k} not written under the data directory"
        got = disk.read_bytes()
        assert sve_refs.compared_bytes(k, got) == sve_refs.compared_bytes(k, v), k
        out = tmp / ("cat_" + k.replace("/", "__"))
        _run(exe, ["--install", str(INSTALL), "--data", str(tmp / "data"), "--cat", sve_refs.logical(k), str(out)])
        assert out.read_bytes() == got, f"{k} not read back"
    s = json.loads((tmp / "rec" / "session.json").read_text())
    assert s["saves_refused"] == 0 and "mainLoop returned" in s["detail"], s["detail"][:300]


def test_saves_refuse_a_data_dir_inside_the_install(exe):
    target = INSTALL / "s7_3_refused_data"
    assert not target.exists()
    r = _run(exe, ["--install", str(INSTALL), "--data", str(target), "--list", "Saved Games/*.sve"], check=False)
    assert r.returncode == 3 and "refused" in r.stderr and not target.exists()
