"""P8A: the loading banner. From the P8A tree root:
    ~/bin/py -3.12 log/roadmap/P8/P8A_drive.py load --dpi 96 --out log/roadmap/P8/P8A_runs/dpi96 [--overlay on|off]
Monterey from the sandbox location screen; window captures every 0.15 s from the click to the 3rd frame head."""
import argparse, json, pathlib, sys, time
ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT / "log" / "roadmap" / "P5"))
import P5B_drive as P  # noqa: E402
P.EXE = ROOT / "port" / "build" / "P8A_x64" / "app" / "Release" / "simgolf_port.exe"
AR = P.AR

def lines(p):
    try: return len(p.read_text().splitlines())
    except OSError: return 0

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("cmd"); ap.add_argument("--dpi", default="96")
    ap.add_argument("--overlay", default="on"); ap.add_argument("--resize", default=""); ap.add_argument("--out", type=pathlib.Path, required=True)
    a = ap.parse_args()
    import os
    os.environ.pop("SG_APP_P6B", None)
    app, d, tmp, rec = P.launch(["--loading-overlay", a.overlay, "--dpi", a.dpi] + (["--resizable", "on", "--scale", "smooth"] if a.resize else []))
    # launch fixes --dpi 96; a second --dpi later on the command line wins
    res = {}
    a.out.mkdir(parents=True, exist_ok=True)
    try:
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        d.wait_quiet(1.5, 120); d.move(130, 445); d.wait_quiet(1.0, 20)
        d.click(130, 445, tag="Sandbox"); d.wait_quiet(1.5, 60)
        d.move(270, 32); d.wait_quiet(1.0, 20)
        frames0 = lines(rec / "frames.tsv")
        t0 = time.time(); d.click(270, 32, tag="Monterey")
        from PIL import Image
        if a.resize:
            import ctypes as C
            from ctypes import wintypes as W
            cw, ch = map(int, a.resize.split("x"))
            r = W.RECT(0, 0, cw, ch)
            C.windll.user32.AdjustWindowRect(C.byref(r), 0x00CF0000, False)
            C.windll.user32.SetWindowPos(app.hwnd, 0, 0, 0, r.right - r.left, r.bottom - r.top, 0x16)
        k = 0
        nxt = 0.0
        while time.time() - t0 < 60:
            now = time.time() - t0
            if now >= nxt and k < 12:
                Image.fromarray(app.capture()).save(a.out / f"load_{k:02d}.png"); res[f"t{k:02d}"] = round(now, 3); k += 1; nxt = now + 0.15
            if lines(rec / "frames.tsv") - frames0 >= 3: break
            time.sleep(0.01)
        res["first_frame_s"] = round(time.time() - t0, 3)
        Image.fromarray(app.capture()).save(a.out / "frame.png")
        for i in range(6):   # the game's own message banners (welcome text, ticker) over the course
            d.wait_frames(12, 60)
            Image.fromarray(app.capture()).save(a.out / f"after_{i}.png")
    finally:
        app.close(60)
    (a.out / "res.json").write_text(json.dumps(res, indent=1)); print(json.dumps(res, indent=1))

if __name__ == "__main__": main()
