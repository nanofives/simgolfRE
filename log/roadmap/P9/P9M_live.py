"""P9 merge check: the merged app live at 1920 x 1080 (title, location screen, world load, in game).
From the repo root: ~/bin/py -3.12 log/roadmap/P9/P9M_live.py [--out log/roadmap/P8/P8M_runs/banner_wide2]
Title menu -> Sandbox -> Monterey (clicks through the wide view's content rectangle), captures every 0.15 s from the click."""
import argparse, json, os, pathlib, sys, tempfile, time
ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
from port.test import app_replay as AR  # noqa: E402

EXE = ROOT / "port" / "build" / "P9M" / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "P9" / "P9M_runs" / "live1920"))
    a = ap.parse_args()
    import app_drive
    from PIL import Image
    for k in ("SG_APP_P5B", "SG_APP_WIDE", "SG_APP_P6B", "SG_APP_P8B"):
        os.environ.pop(k, None)
    out = pathlib.Path(a.out); out.mkdir(parents=True, exist_ok=True)
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="p8m_"))
    rec, data = tmp / "rec", tmp / "data"
    args = ["--install", str(AR.INSTALL), "--data", str(data), "--record-dir", str(rec), "--no-activate", "--dpi", "96",
            "--cursor", "message", "--keys", "message", "--clock-origin", str(AR.CLOCK_ORIGIN), "--profile", "play",
            "--wide", "--audio", "off", "--resizable", "on"]
    app = app_drive.launch(EXE, args, cwd=str(ROOT))
    d = AR.Driver(app, data)
    import ctypes as C
    from ctypes import wintypes as W
    r = W.RECT(0, 0, 1920, 1080)
    C.windll.user32.AdjustWindowRect(C.byref(r), 0x00CF0000, False)
    C.windll.user32.SetWindowPos(app.hwnd, 0, 0, 0, r.right - r.left, r.bottom - r.top, 0x16)
    time.sleep(1.0)
    cw, ch = app.client_size()
    vw = ch * 800 // 600
    ox = (cw - vw) // 2

    def pt(x, y):
        return ox + (2 * x + 1) * vw // 1600, (2 * y + 1) * ch // 1200

    res = {"client": [cw, ch]}
    try:
        time.sleep(8)  # P9A: the play title menu lights no item until hovered, so its frame differs from the pinned hash
        d.wait_quiet(1.5, 120)
        d.move(*pt(130, 445)); d.wait_quiet(1.0, 20)
        d.click(*pt(130, 445), tag="Sandbox"); d.wait_quiet(1.5, 60)
        d.move(*pt(270, 32)); d.wait_quiet(1.0, 20)
        Image.fromarray(app.capture()).save(out / "location.png")
        t0 = time.time(); d.click(*pt(270, 32), tag="Monterey")
        k = 0; nxt = 0.0
        while time.time() - t0 < 20 and k < 14:
            now = time.time() - t0
            if now >= nxt:
                Image.fromarray(app.capture()).save(out / f"load_{k:02d}.png"); res[f"t{k:02d}"] = round(now, 3); k += 1; nxt = now + 0.15
            time.sleep(0.01)
        d.wait_frames(20, 120)
        Image.fromarray(app.capture()).save(out / "ingame.png")
    finally:
        app.close(60)
    (out / "res.json").write_text(json.dumps(res, indent=1)); print(json.dumps(res, indent=1))


if __name__ == "__main__":
    main()
