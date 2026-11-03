"""P8B: the app with --wide and a world layer, driven by posted messages (as log/roadmap/P7/P7_drive.py). Run from the P8B tree:

    ~/bin/py -3.12 log/roadmap/P8/P8B_drive.py [--dpi 96] [--render-scale native|1] [--scale auto] [--wide-edge on|off] [--out <dir>]

Title menu -> Sandbox -> Monterey -> Esc on the welcome window -> capture `ingame`. Then the edge-scroll checks, all with
`--cursor message` (the cursor answer is the last mouse message's logical point):
  1. cursor parked in the middle of the left side column (logical x < 0): 12 frames, capture `side_park`; with --wide-edge on the
     view must not scroll (H1 gets x clamped to 1), with off it scrolls (H1 sees x <= 0)
  2. cursor at client x = 0 (the window's own left edge): 12 frames, capture `edge_left`: the view scrolls
The view is compared between captures over the content rectangle's rows 60..400 (no HUD there in the sandbox start view).
The session's recording is replayed through sg_run_boot (app_replay.gate_one: every frame record equal).
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import sys
import tempfile

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
from port.test import app_replay as AR  # noqa: E402

EXE = ROOT / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"
VK_ESCAPE = 0x1B


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dpi", default="96")
    ap.add_argument("--render-scale", default="native")
    ap.add_argument("--scale", default="auto")
    ap.add_argument("--wide-edge", default="on")
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "P8" / "P8B_live"))
    ap.add_argument("--profile", default="fixed")
    ap.add_argument("--resize", default="", help="client sizes to step through after the edge checks, e.g. 1400x600,900x600")
    ap.add_argument("--negctl", default="")
    ap.add_argument("--tag", default="")
    ap.add_argument("--click-pause", action="store_true", help="click the radial menu's pause button at its HUD-anchored place")
    a = ap.parse_args()
    import app_drive
    import numpy as np
    from PIL import Image
    for k in ("SG_APP_P5B", "SG_APP_WIDE", "SG_APP_P6B"):
        os.environ.pop(k, None)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="p8b_"))
    rec, data = tmp / "rec", tmp / "data"
    args = ["--install", str(AR.INSTALL), "--data", str(data), "--record-dir", str(rec), "--no-activate", "--dpi", a.dpi,
            "--cursor", "message", "--keys", "message", "--clock-origin", str(AR.CLOCK_ORIGIN), "--profile", a.profile,
            "--wide", "--render-scale", a.render_scale, "--scale", a.scale, "--wide-edge", a.wide_edge, "--audio", "off"]
    if a.negctl:
        args += ["--negctl", a.negctl]
    app = app_drive.launch(EXE, args, cwd=str(ROOT))
    d = AR.Driver(app, data)
    cw, ch = app.client_size()
    vw = ch * 800 // 600
    ox = (cw - vw) // 2

    def pt(x, y):  # the client point that shows the 800-space point (x, y)
        return ox + (2 * x + 1) * vw // 1600, (2 * y + 1) * ch // 1200

    res = {"client": [cw, ch], "content_w": vw, "origin_x": ox, "args": args}
    shots = {}
    try:
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        d.wait_quiet(1.5, 120)
        d.move(*pt(130, 445))
        d.wait_quiet(1.0, 20)
        d.click(*pt(130, 445), tag="Sandbox")
        d.wait_quiet(1.5, 60)
        d.move(*pt(270, 32))
        d.wait_quiet(1.0, 20)
        d.click(*pt(270, 32), tag="Monterey")
        d.wait_frames(40, 180)
        d.key(VK_ESCAPE, tag="welcome Esc")
        d.move(*pt(400, 250))
        d.wait_frames(15)
        shots["ingame"] = app.capture()
        d.move(ox // 2, ch // 2)          # the middle of the left side column
        d.wait_frames(12)
        shots["side_park"] = app.capture()
        d.move(0, ch // 2)                # the window's own left edge
        d.wait_frames(12)
        shots["edge_left"] = app.capture()
        d.move(*pt(400, 250))
        d.wait_frames(6)
        if a.click_pause:  # the pause button (800-space 105, 567) of the bottom band, anchored to the left edge: client x - origin_x
            px, py = pt(105, 567)
            res["pause_click_client"] = [px - ox, py]
            d.move(px - ox, py)
            d.wait_frames(3)
            d.click(px - ox, py, tag="pause (moved)")
            d.wait_frames(8)
            shots["paused"] = app.capture()
        import ctypes as C
        from ctypes import wintypes as W
        u32 = C.windll.user32
        for sz in [s for s in a.resize.split(",") if s]:
            want_w, want_h = (int(v) for v in sz.split("x"))
            wr, cr = W.RECT(), W.RECT()
            u32.GetWindowRect(app.hwnd, C.byref(wr))
            u32.GetClientRect(app.hwnd, C.byref(cr))
            fw = (wr.right - wr.left) - (cr.right - cr.left)
            fh = (wr.bottom - wr.top) - (cr.bottom - cr.top)
            u32.SetWindowPos(app.hwnd, 0, 0, 0, want_w + fw, want_h + fh, 0x0002 | 0x0004 | 0x0010)  # NOMOVE NOZORDER NOACTIVATE
            d.wait_frames(10)
            shots[f"resize_{sz}"] = app.capture()
            res.setdefault("resized_clients", []).append(list(app.client_size()))
    finally:
        res["close_rc"] = app.close(60)
    tag = f"dpi{a.dpi}_{a.render_scale}_{a.scale}_edge{a.wide_edge}" + (f"_{a.tag}" if a.tag else "")
    for k, img in shots.items():
        Image.fromarray(img).save(out / f"{k}_{tag}.png")

    def view(img):
        return img[60 * ch // 600:400 * ch // 600, ox:ox + vw].astype(int)

    if all(k in shots for k in ("ingame", "side_park", "edge_left")):
        res["side_park_changed_px"] = int((np.abs(view(shots["side_park"]) - view(shots["ingame"])).max(axis=2) > 24).sum())
        res["edge_left_changed_px"] = int((np.abs(view(shots["edge_left"]) - view(shots["side_park"])).max(axis=2) > 24).sum())
        res["view_px"] = int(view(shots["ingame"]).shape[0] * view(shots["ingame"]).shape[1])
        side = shots["ingame"][:, :ox]
        res["ingame_side_nonblack_px"] = int((side.reshape(-1, 3).max(axis=1) > 0).sum())
        res["ingame_side_px"] = int(side.shape[0] * side.shape[1])
    session = json.loads((rec / "session.json").read_text())
    res["p8b"] = session.get("p8b")
    res["session_p8b"] = session.get("session", {}).get("p8b")
    res["records"] = session.get("session", {}).get("records")
    from port.test import lib as L
    lib = L.load(os.environ.get("SG_PORTSIM_DLL"))
    g = AR.gate_one(lib, "p8b", trace=(rec / "trace.trace").read_bytes(), framehash=(rec / "framehash.bin").read_bytes(),
                    session=session)
    res["gate"] = {k: g[k] for k in g if k in ("status", "records", "expected", "first", "profile")}
    dst = out / f"rec_{tag}"
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(rec, dst, ignore=shutil.ignore_patterns("trace.trace"))
    (out / f"result_{tag}.json").write_text(json.dumps(res, indent=1, default=str))
    print(json.dumps(res, indent=1, default=str))
    shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
