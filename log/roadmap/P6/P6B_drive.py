"""P6B: app runs for item 1 (closing) and item 2 (the world-load delay). Posted messages only, from the tree root:

    ~/bin/py -3.12 log/roadmap/P6/P6B_drive.py quit  [--answer quit|save|wait] [--close-prompt on|off] [--out <dir>]
    ~/bin/py -3.12 log/roadmap/P6/P6B_drive.py title [--close-prompt on|off] [--out <dir>]
    ~/bin/py -3.12 log/roadmap/P6/P6B_drive.py load  [--extra "<app args>"] [--out <dir>]

quit: Monterey, the close button (WM_SYSCOMMAND SC_CLOSE), then the prompt's answer by keys (Down x n, Enter: option 0 `Wait`,
1 save then quit, 2 quit); reports whether and when the process ends, the session's stop reason and the last shown frame.
load: Monterey; reports the wall time from the Monterey click to the first in-game frame head and every present in between
(presents.tsv / shown.tsv rows with their arrival time, polled).
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys
import time

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT / "log" / "roadmap" / "P5"))
import P5B_drive as P  # noqa: E402

AR = P.AR
VK_DOWN, VK_RETURN, VK_ESCAPE = 0x28, 0x0D, 0x1B


def lines(p: pathlib.Path) -> int:
    try:
        return len(p.read_text().splitlines())
    except OSError:
        return 0


def cmd_quit(a) -> int:
    app, d, tmp, rec = P.launch(["--close-prompt", a.close_prompt])
    res = {"answer": a.answer}
    try:
        AR._to_monterey(d)
        d.post(P.WM_SYSCOMMAND, P.SC_CLOSE, 0, "close")
        res["alive_after_close"] = P.alive_after(app, 3.0)
        d.wait_quiet(1.5, 30)
        n = {"wait": 1, "save": 2, "quit": 3}[a.answer]
        for i in range(n):
            d.key(VK_DOWN, tag=f"Down {i}")
            time.sleep(0.4)
        t0 = time.time()
        try:  # the window may be gone before the key-up is posted (the app closed)
            d.key(VK_RETURN, tag="Enter")
            if a.answer == "save":  # saveGameDialog: Enter keeps the offered name
                time.sleep(2.0)
                d.key(VK_RETURN, tag="save Enter")
        except OSError as e:
            res["post_failed"] = str(e)
        try:
            rc = app.proc.wait(a.wait)
        except Exception:  # noqa: BLE001
            rc = None
        res["rc"] = rc
        res["exit_s"] = round(time.time() - t0, 2) if rc is not None else None
    finally:
        if app.proc.poll() is None:
            res["still_running_after_s"] = a.wait
            res["forced_close_rc"] = app.close(60)
    P.keep(rec, a.out)
    s = json.loads((rec / "session.json").read_text())
    res["session"] = {k: s["session"].get(k) for k in ("stop", "records", "close_prompt", "close_flag_writes", "close_at_menu",
                                                      "close_cancelled", "close_exit")}
    saves = sorted(str(p.relative_to(tmp)) for p in (tmp / "data").rglob("*") if p.is_file() and "recordings" not in p.parts)
    res["data_files"] = saves
    res["gate"] = P.final_state(rec)["gate"]  # the recording replayed through sg_run_boot (every frame record equal)
    res["replay"] = replay_files(rec, tmp)
    print(json.dumps(res, indent=1))
    return 0


def replay_files(rec: pathlib.Path, tmp: pathlib.Path) -> dict:
    """the title session's recording through sg_run_boot to its end: its status and whether the files the replay wrote
    (top10.sve, Default.pro) equal the app's"""
    from port.test import lib as L
    from port.test import live as LV
    lib = L.load(__import__("os").environ.get("SG_PORTSIM_DLL"))
    session = json.loads((rec / "session.json").read_text())
    AR.apply_switches(lib, AR.session_switches(session))
    st, done = LV.boot_state(lib, None)
    out = {}
    try:
        code, n, ts, hs = st.run_boot((rec / "trace.trace").read_bytes(), int(session["session"]["records"]) + 1)  # + 1: on to the trace's end
        out["code"], out["records"] = code, n
        for name in ("top10.sve", "Themes/Default.pro"):
            try:
                b = st.files_read(name)
            except Exception as e:  # noqa: BLE001
                b = None
                out[name + "_err"] = str(e)
            app = (tmp / "data" / "files" / name)
            out[name] = (b is not None and app.exists() and bytes(b) == app.read_bytes())
    finally:
        done()
        lib.defect_reset()
    return out


def cmd_title(a) -> int:
    app, d, tmp, rec = P.launch(["--close-prompt", a.close_prompt])
    res = {}
    try:
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        d.wait_quiet(1.5, 120)
        t0 = time.time()
        d.post(P.WM_SYSCOMMAND, P.SC_CLOSE, 0, "close on the title menu")
        try:
            res["rc"] = app.proc.wait(a.wait)
        except Exception:  # noqa: BLE001
            res["rc"] = None
        res["exit_s"] = round(time.time() - t0, 2)
    finally:
        if app.proc.poll() is None:
            res["forced_close_rc"] = app.close(60)
    P.keep(rec, a.out)
    s = json.loads((rec / "session.json").read_text())
    res["session"] = {k: s["session"].get(k) for k in ("stop", "records", "close_at_menu", "close_exit")}
    res["data_files"] = sorted(str(p.relative_to(tmp)) for p in (tmp / "data").rglob("*") if p.is_file() and "recordings" not in p.parts)
    res["replay"] = replay_files(rec, tmp)
    print(json.dumps(res, indent=1))
    return 0


def cmd_load(a) -> int:
    app, d, tmp, rec = P.launch(a.extra.split() if a.extra else [])
    res = {}
    try:
        AR.wait_shown_hash(rec, AR.title_menu_hash(), 120)
        d.wait_quiet(1.5, 120)
        d.move(130, 445)
        d.wait_quiet(1.0, 20)
        d.click(130, 445, tag="Sandbox")
        d.wait_quiet(1.5, 60)
        d.move(270, 32)
        d.wait_quiet(1.0, 20)
        shown0, frames0 = lines(rec / "shown.tsv"), lines(rec / "frames.tsv")
        t0 = time.time()
        d.click(270, 32, tag="Monterey")
        events = []
        last_s, last_f = shown0, frames0
        shot = None
        while time.time() - t0 < 120:
            if shot is None and time.time() - t0 >= 0.4:  # the window 0.4 s into the load
                shot = app.capture()
                res["capture_s"] = round(time.time() - t0, 3)
            s, f = lines(rec / "shown.tsv"), lines(rec / "frames.tsv")
            now = round(time.time() - t0, 3)
            if s != last_s:
                events.append((now, "shown", s - shown0))
                last_s = s
            if f != last_f:
                events.append((now, "frame", f - frames0))
                last_f = f
                if f - frames0 >= 3:
                    break
            time.sleep(0.005)
        res["events"] = events
        res["first_frame_s"] = next((e[0] for e in events if e[1] == "frame"), None)
        res["shown_before_first_frame"] = [e for e in events if e[1] == "shown" and (res["first_frame_s"] is None or e[0] <= res["first_frame_s"])]
        if shot is not None:
            from PIL import Image
            Image.fromarray(shot).save(tmp / "loading.png")
    finally:
        app.close(60)
    P.keep(rec, a.out)
    if a.out and (tmp / "loading.png").exists():
        import shutil
        shutil.copy(tmp / "loading.png", a.out / "loading.png")
    print(json.dumps(res, indent=1))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    q = sub.add_parser("quit")
    q.add_argument("--answer", default="quit", choices=("quit", "save", "wait"))
    q.add_argument("--close-prompt", default="on")
    q.add_argument("--wait", type=float, default=20.0)
    q.add_argument("--out", type=pathlib.Path)
    q.set_defaults(fn=cmd_quit)
    t = sub.add_parser("title")
    t.add_argument("--close-prompt", default="on")
    t.add_argument("--wait", type=float, default=20.0)
    t.add_argument("--out", type=pathlib.Path)
    t.set_defaults(fn=cmd_title)
    l = sub.add_parser("load")
    l.add_argument("--extra", default="")
    l.add_argument("--out", type=pathlib.Path)
    l.set_defaults(fn=cmd_load)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
