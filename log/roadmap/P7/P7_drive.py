"""P7: the app with --wide, driven by posted messages (as port/test/app_replay.py drives it). Run from a tree root:

    ~/bin/py -3.12 log/roadmap/P7/P7_drive.py [--dpi 96] [--fill soft|clamp] [--scale nearest|smooth|auto] [--out <dir>]

The window opens at 1067 x 600 logical pixels (16:9) scaled by --dpi. The script waits for the title menu, captures the client
area, clicks Sandbox at the client point that shows the 800-space point (130, 445), captures the location screen, clicks
Monterey (270, 32), waits for the frame loop, presses Esc on the welcome window and captures the course view (stage 1: black
side bands in game). The client points are computed from the window's own viewport (content rectangle at full height, centred),
so a click that lands proves the input mapping. The session's recording is then replayed through sg_run_boot (app_replay's
gate: every frame record equal) to show the session is an ordinary one.
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
    ap.add_argument("--fill", default="soft")
    ap.add_argument("--scale", default="auto")
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "P7" / "live"))
    a = ap.parse_args()
    import app_drive
    from PIL import Image
    for k in ("SG_APP_P5B", "SG_APP_WIDE"):
        os.environ.pop(k, None)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="p7_"))
    rec, data = tmp / "rec", tmp / "data"
    args = ["--install", str(AR.INSTALL), "--data", str(data), "--record-dir", str(rec), "--no-activate", "--dpi", a.dpi,
            "--cursor", "message", "--keys", "message", "--clock-origin", str(AR.CLOCK_ORIGIN), "--profile", "fixed",
            "--wide", "--wide-fill", a.fill, "--scale", a.scale]
    app = app_drive.launch(EXE, args, cwd=str(ROOT))
    d = AR.Driver(app, data)
    cw, ch = app.client_size()
    # the content rectangle (present.cpp fit_viewport): full height when the client is wider than 4:3
    vw = ch * 800 // 600
    ox = (cw - vw) // 2

    def pt(x, y):  # the client point that shows the 800-space point (x, y)
        return ox + (2 * x + 1) * vw // 1600, (2 * y + 1) * ch // 1200

    res = {"client": [cw, ch], "content_w": vw, "origin_x": ox, "args": args}
    shots = {}
    try:
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        d.wait_quiet(1.5, 120)
        shots["title"] = app.capture()
        d.move(*pt(130, 445))
        d.wait_quiet(1.0, 20)
        d.click(*pt(130, 445), tag="Sandbox")
        d.wait_quiet(1.5, 60)
        shots["location"] = app.capture()
        d.move(*pt(270, 32))
        d.wait_quiet(1.0, 20)
        d.click(*pt(270, 32), tag="Monterey")
        d.wait_frames(40, 180)
        d.key(VK_ESCAPE, tag="welcome Esc")
        d.wait_frames(15)
        shots["ingame"] = app.capture()
    finally:
        res["close_rc"] = app.close(60)
    tag = f"dpi{a.dpi}_{a.fill}_{a.scale}"
    for k, img in shots.items():
        Image.fromarray(img).save(out / f"{k}_{tag}.png")
    # band facts: the side bands' pixels per screen (non-black count) and the content rectangle untouched is checked by the selftest
    import numpy as np
    for k, img in shots.items():
        bands = np.concatenate([img[:, :ox], img[:, ox + vw:]], axis=1)
        res[f"{k}_band_nonblack_px"] = int((bands.reshape(-1, 3).max(axis=1) > 0).sum())
        res[f"{k}_band_px"] = int(bands.shape[0] * bands.shape[1])
    session = json.loads((rec / "session.json").read_text())
    res["p7"] = session.get("p7")
    res["presents_band"] = session.get("session", {}).get("presents_band")
    res["records"] = session.get("session", {}).get("records")
    from port.test import lib as L
    lib = L.load(os.environ.get("SG_PORTSIM_DLL"))
    g = AR.gate_one(lib, "p7", trace=(rec / "trace.trace").read_bytes(), framehash=(rec / "framehash.bin").read_bytes(),
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
