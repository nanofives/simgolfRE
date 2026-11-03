"""P5B: a short app run per item (log/roadmap/P5/P5B_notes.md). Posted messages only, as port/test/app_replay.py drives the app.

    ~/bin/py -3.12 log/roadmap/P5/P5B_drive.py ingame  [--zoom-keys all|numpad|off] [--out <dir>]
    ~/bin/py -3.12 log/roadmap/P5/P5B_drive.py title   [--out <dir>]
    ~/bin/py -3.12 log/roadmap/P5/P5B_drive.py edge    [--edge-guard on|off] [--out <dir>]

ingame: Monterey, '-' '-' '+' (numpad), then the main keyboard '-' (OEM_MINUS), the close button (WM_SYSCOMMAND SC_CLOSE), Esc
on the prompt, the close button again (a new prompt), and a second press while it is open (the window closes). The session's
recording is replayed through sg_run_boot (app_replay.gate_one: every frame record equal) and the final state's g_zoomScale,
g_cash_div100, unk_0059b734 and g_flags are read from the replay.
title: the close button on the title menu: the process must end at once.
edge: the title menu with the window never the foreground window (--no-activate) and the cursor from the system: every answer of
the recording must lie in 1..798 x 1..598 with the guard on.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import struct
import sys
import tempfile
import time

ROOT = pathlib.Path.cwd()  # run from the tree root (log/ is a junction into the shared tree)
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import app_replay as AR  # noqa: E402

WM_SYSCOMMAND, SC_CLOSE = 0x112, 0xF060
VK_ADD, VK_SUBTRACT, VK_OEM_MINUS, VK_OEM_PLUS, VK_ESCAPE = 0x6B, 0x6D, 0xBD, 0xBB, 0x1B
EXE = ROOT / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"


def gs_off(name: str) -> int:
    import re
    t = (ROOT / "port" / "include" / "gamestate.h").read_text()
    m = re.search(r"offsetof\(GameState, " + re.escape(name) + r"\) == (0x[0-9a-f]+)", t)
    return int(m.group(1), 16)


def launch(extra, cursor="message"):
    import app_drive
    env_p5b = os.environ.pop("SG_APP_P5B", None)
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="p5b_"))
    rec, data = tmp / "rec", tmp / "data"
    args = ["--install", str(AR.INSTALL), "--data", str(data), "--record-dir", str(rec), "--no-activate", "--dpi", "96",
            "--cursor", cursor, "--keys", "message", "--clock-origin", str(AR.CLOCK_ORIGIN), "--profile", "fixed", *extra]
    app = app_drive.launch(EXE, args, cwd=str(ROOT))
    if env_p5b is not None:
        os.environ["SG_APP_P5B"] = env_p5b
    return app, AR.Driver(app, data), tmp, rec


def alive_after(app, s: float) -> bool:
    end = time.time() + s
    while time.time() < end:
        if app.proc.poll() is not None:
            return False
        time.sleep(0.05)
    return True


def keep(rec: pathlib.Path, out: pathlib.Path | None):
    if out:
        if out.exists():
            shutil.rmtree(out)
        shutil.copytree(rec, out)


def final_state(rec: pathlib.Path) -> dict:
    """the session replayed through sg_run_boot: the gate verdict and the final state's words"""
    from port.test import lib as L
    from port.test import live as LV
    lib = L.load(os.environ.get("SG_PORTSIM_DLL"))
    session = json.loads((rec / "session.json").read_text())
    trace, fh = (rec / "trace.trace").read_bytes(), (rec / "framehash.bin").read_bytes()
    names = [o[0] if isinstance(o, (tuple, list)) else o for o in lib.objects("sim")]
    exp = AR.read_framehash(fh, len(names))
    AR.apply_switches(lib, AR.session_switches(session))
    st, done = LV.boot_state(lib, None)
    try:
        code, n, ts, hs = st.run_boot(trace, len(exp) + 1)
        words = {k: struct.unpack("<i", st.read(gs_off(k), 4))[0]
                 for k in ("g_zoomScale", "g_cash_div100", "unk_0059b734", "g_flags", "g_difficulty")}
    finally:
        done()
        lib.defect_reset()
    verdict = AR.compare(exp, {"code": code, "records": n, "T": list(ts), "hashes": hs}, names)
    return {"gate": verdict, "words": words, "session": session}


def cmd_ingame(a) -> int:
    app, d, tmp, rec = launch(["--zoom-keys", a.zoom_keys])
    res = {"zoom_keys": a.zoom_keys}
    try:
        AR._to_monterey(d)
        for vk, tag in ((VK_SUBTRACT, "num -"), (VK_SUBTRACT, "num -"), (VK_ADD, "num +"), (VK_OEM_MINUS, "main -")):
            d.key(vk, tag=tag)
            d.wait_frames(8)
        d.post(WM_SYSCOMMAND, SC_CLOSE, 0, "close 1")
        res["alive_after_close_1"] = alive_after(app, 4.0)
        d.wait_quiet(1.5, 30)
        d.key(VK_ESCAPE, tag="prompt Esc")
        d.wait_frames(10, 60)
        res["alive_after_esc"] = app.proc.poll() is None
        d.post(WM_SYSCOMMAND, SC_CLOSE, 0, "close 2")
        res["alive_after_close_2"] = alive_after(app, 4.0)
        d.wait_quiet(1.5, 30)
        shutil.copy(rec / "shown.tsv", tmp / "shown_at_prompt.tsv")
        d.post(WM_SYSCOMMAND, SC_CLOSE, 0, "close 3 (during the prompt)")
        t0 = time.time()
        try:
            rc = app.proc.wait(30)
        except Exception:  # noqa: BLE001
            rc = None
        res["exit_after_close_3_s"] = round(time.time() - t0, 2)
        res["rc"] = rc
    finally:
        if app.proc.poll() is None:
            res["forced_close_rc"] = app.close(60)
    keep(rec, a.out)
    f = final_state(rec)
    s = f["session"]
    res["session"] = {k: s["session"].get(k) for k in ("stop", "records", "close_prompt", "close_flag_writes", "close_at_menu",
                                                      "close_cancelled", "edge_guard", "guarded_cursor_reads")}
    res["p5b"] = s.get("p5b")
    res["gate"] = f["gate"]
    res["final"] = f["words"]
    print(json.dumps(res, indent=1))
    return 0


def cmd_title(a) -> int:
    app, d, tmp, rec = launch([])
    res = {}
    try:
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        d.wait_quiet(1.5, 120)
        t0 = time.time()
        d.post(WM_SYSCOMMAND, SC_CLOSE, 0, "close on the title menu")
        try:
            res["rc"] = app.proc.wait(30)
        except Exception:  # noqa: BLE001
            res["rc"] = None
        res["exit_s"] = round(time.time() - t0, 2)
    finally:
        if app.proc.poll() is None:
            res["forced_close_rc"] = app.close(60)
    keep(rec, a.out)
    s = json.loads((rec / "session.json").read_text())
    res["stop"] = s["session"]["stop"]
    res["records"] = s["session"]["records"]
    print(json.dumps(res, indent=1))
    return 0


def cmd_edge(a) -> int:
    import trace_format as TF  # re/tools
    app, d, tmp, rec = launch(["--edge-guard", a.edge_guard], cursor="system")
    res = {"edge_guard": a.edge_guard}
    try:  # the title menu reads the cursor every pass (its hover); a menu cannot be driven with the system cursor unmoved
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        time.sleep(4.0)
    finally:
        app.close(60)
    import ctypes
    import ctypes.wintypes as W
    pt = W.POINT()
    ctypes.windll.user32.GetCursorPos(ctypes.byref(pt))
    res["system_cursor_screen"] = (pt.x, pt.y)
    keep(rec, a.out)
    s = json.loads((rec / "session.json").read_text())
    res["guarded_cursor_reads"] = s["session"].get("guarded_cursor_reads")
    res["cursors"] = s["session"].get("cursors")
    tr = TF.parse((rec / "trace.trace").read_bytes())
    pts = [(r.f["x"], r.f["y"]) for r in tr.records if r.type == TF.CURSOR]
    res["cursor_records"] = len(pts)
    res["outside_1_798"] = sum(1 for x, y in pts if not (1 <= x <= 798 and 1 <= y <= 598))
    res["distinct"] = sorted(set(pts))[:10]
    print(json.dumps(res, indent=1))
    return 0


def set_client(hwnd: int, cw: int, ch: int):
    """resize the window so its client area is cw x ch (the frame from AdjustWindowRectExForDpi of the window's own style)"""
    import ctypes
    import ctypes.wintypes as W
    u = ctypes.windll.user32
    u.GetWindowLongW.restype = ctypes.c_long
    style = u.GetWindowLongW(hwnd, -16) & 0xFFFFFFFF
    r = W.RECT(0, 0, cw, ch)
    u.AdjustWindowRectExForDpi(ctypes.byref(r), style, False, 0, u.GetDpiForWindow(hwnd))
    u.SetWindowPos(hwnd, None, 40, 40, r.right - r.left, r.bottom - r.top, 0x0004 | 0x0010)  # NOZORDER | NOACTIVATE


def cmd_resize(a) -> int:
    """item 4: a 1300 x 900 client (content 1200 x 900 at x 50: pillarbox, scale 1.5 = smooth under --scale auto), the
    title menu's Sandbox and the location screen's Monterey clicked at their SCALED client points (130, 445) * 1.5 + (50, 0)
    and (270, 32) * 1.5 + (50, 0): the world loads only when the clicks map back to the logical points; then maximised"""
    import numpy as np
    import PIL.Image as I
    app, d, tmp, rec = launch(["--scale", a.scale])
    res = {"scale": a.scale}
    out = a.out or (tmp / "out")
    out.mkdir(parents=True, exist_ok=True)
    try:
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        d.wait_quiet(1.5, 120)
        set_client(app.hwnd, 1300, 900)
        time.sleep(1.0)
        res["client_after_resize"] = app.client_size()
        img = app.capture()
        I.fromarray(img).save(out / "title_1300x900.png")
        res["bar_left_black"] = bool((img[:, :50] == 0).all())
        res["bar_right_black"] = bool((img[:, 1250:] == 0).all())
        sx = lambda x: 50 + (x * 1200) // 800  # noqa: E731
        sy = lambda y: (y * 900) // 600  # noqa: E731
        d.move(sx(130), sy(445))
        d.wait_quiet(1.0, 20)
        d.click(sx(130), sy(445), tag="Sandbox (scaled)")
        d.wait_quiet(1.5, 60)
        d.move(sx(270), sy(32))
        d.wait_quiet(1.0, 20)
        d.click(sx(270), sy(32), tag="Monterey (scaled)")
        d.wait_frames(40, 180)
        res["world_loaded"] = True
        d.key(0x1B, tag="welcome Esc")
        d.wait_frames(10)
        I.fromarray(app.capture()).save(out / "ingame_1300x900.png")
        import ctypes
        ctypes.windll.user32.ShowWindow(app.hwnd, 3)  # SW_MAXIMIZE
        time.sleep(1.5)
        d.wait_frames(5)
        res["client_maximised"] = app.client_size()
        I.fromarray(app.capture()).save(out / "ingame_maximised.png")
    except Exception as e:  # noqa: BLE001
        res["error"] = f"{type(e).__name__}: {e}"
    finally:
        app.close(60)
    keep(rec, (out / "rec") if a.out else None)
    print(json.dumps(res, indent=1))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("what", choices=["ingame", "title", "edge", "resize"])
    ap.add_argument("--zoom-keys", default="all")
    ap.add_argument("--edge-guard", default="on")
    ap.add_argument("--scale", default="auto")
    ap.add_argument("--out", type=pathlib.Path)
    a = ap.parse_args()
    return {"ingame": cmd_ingame, "title": cmd_title, "edge": cmd_edge, "resize": cmd_resize}[a.what](a)


if __name__ == "__main__":
    sys.exit(main())
