"""Roadmap S7.4: the app's own recordings in the boot gate (notes log/roadmap/S7/4_notes.md).

Every app session (port/app, `simgolf_port.exe --install <dir>`) writes trace.trace (schema v1, the live core's recording) and
framehash.bin (per frame head: u32 T, u32 record, u64 sgh64 of every sim object). The gate runs sg_run_boot over each stored
recording and compares every record (T and the 203 sim hashes) with the session's own framehash.bin: a recording of real play
must replay to the same simulation.

Recordings live in port/corpus/app/<name>/ (local, gitignored with the rest of port/corpus): the session directory as the app
wrote it (trace.trace, framehash.bin / .json, session.json, input.jsonl, input_dispatch.tsv, dispatches.tsv, body_calls.tsv,
presents.tsv, shown.tsv, frames.tsv, waits.tsv, gfx_log.txt, back.555), plus posted.json (what the driver posted, in order,
with the input_dispatch seq each message got) and script.json (the script, its options and the app build).

    # make a recording: the app driven only by posted window messages (port/app/tools/app_drive.py), --clock-origin and
    # --keys message (log/roadmap/S7/4_notes.md section 2: what the cross-check against the original needs)
    ~/bin/py -3.12 -m port.test.app_replay record <script> [--name N] [--exe <simgolf_port.exe>]
    # the gate: every recording (or the named ones), x64 or Win32 (SG_BACKEND / --backend), TSV one row per recording
    ~/bin/py -3.12 -m port.test.app_replay gate [names...] [--backend win32] [--out <tsv>]
    ~/bin/py -3.12 -m port.test.app_replay list

Scripts (SCRIPTS): `hole` (title -> Sandbox -> Monterey, the inputs the corpus never recorded: Shift / Ctrl clicks, a double
click, the wheel, Alt+X, Shift+A; then the Build Course palette, a tee, a green, `H` opens hole 1), `saveload` (Monterey, the save key
`S` + Enter writes a save into the app's data overlay, `L` loads it back through the browser), `report` (Monterey, the report
keys F1..F4 open their modal screens, Esc closes each).
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

ROOT = pathlib.Path(__file__).resolve().parents[2]
CORPUS = ROOT / "port" / "corpus" / "app"
INSTALL = ROOT / "original"
EXE_CANDIDATES = [ROOT / "port" / "build" / "S7_4" / "app" / "Release" / "simgolf_port.exe",
                  ROOT / "port" / "build" / "S7_6_app" / "Release" / "simgolf_port.exe"]
# the original's first main-thread timeGetTime (seedFromTime) under scenario_det.js without an audio device: g_seedTime
# 0x0051b380 = 0x00100000 (re/analysis/data/scenario_baseline.md, batch b2); the app's clock starts there (--clock-origin)
CLOCK_ORIGIN = 0x00100000

WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN, WM_SYSKEYUP = 0x100, 0x101, 0x102, 0x104, 0x105
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK = 0x200, 0x201, 0x202, 0x203
WM_RBUTTONDOWN, WM_RBUTTONUP, WM_MOUSEWHEEL = 0x204, 0x205, 0x20A
MK_LBUTTON, MK_RBUTTON, MK_SHIFT, MK_CONTROL = 0x1, 0x2, 0x4, 0x8
VK_RETURN, VK_SHIFT, VK_CONTROL, VK_MENU, VK_ESCAPE = 0x0D, 0x10, 0x11, 0x12, 0x1B


# ------------------------------------------------------------------------------------------------ recordings on disk
def recordings() -> list[str]:
    if not CORPUS.is_dir():
        return []
    return sorted(p.name for p in CORPUS.iterdir() if (p / "trace.trace").exists() and (p / "framehash.bin").exists())


def read_framehash(raw: bytes, nsim: int) -> list[tuple[int, list[int]]]:
    """framehash.bin (the captures' layout): [(T, [sim hashes])]"""
    step = 8 + 8 * nsim
    if len(raw) % step:
        raise ValueError(f"framehash.bin: {len(raw)} bytes is not a multiple of {step}")
    out = []
    for o in range(0, len(raw), step):
        t, _k = struct.unpack_from("<II", raw, o)
        out.append((t, list(struct.unpack_from(f"<{nsim}Q", raw, o + 8))))
    return out


def replay(lib, trace: bytes, max_records: int) -> dict:
    """sg_run_boot over a recording, as port/test/test_app_live.py's replay does (the install as game files, GDI text)"""
    from port.test import live as LV
    st, done = LV.boot_state(lib, None)
    try:
        code, n, ts, hs = st.run_boot(trace, max_records)
    finally:
        done()
    return {"code": code, "records": n, "T": list(ts), "hashes": hs}


def compare(expected: list[tuple[int, list[int]]], got: dict, names: list[str]) -> dict:
    """first record whose T or any sim hash differs; the record counts must be equal too"""
    n = got["records"]
    nsim = len(names)
    first = None
    for k in range(min(n, len(expected))):
        t, hs = expected[k]
        if got["T"][k] != t:
            first = {"record": k, "what": "T", "expected": t, "got": got["T"][k]}
            break
        gh = got["hashes"][k]
        bad = [names[i] for i in range(nsim) if gh[i] != hs[i]]
        if bad:
            first = {"record": k, "what": "sim", "objects": bad[:8], "n_objects": len(bad)}
            break
    status = "PASS" if first is None and n == len(expected) and got["code"] in (0, None) else "FAIL"
    if first is None and n != len(expected):
        first = {"record": min(n, len(expected)), "what": "records", "expected": len(expected), "got": n}
    return {"status": status, "records": n, "expected": len(expected), "code": got["code"], "first": first}


def gate_one(lib, name: str, trace: bytes | None = None, framehash: bytes | None = None) -> dict:
    d = CORPUS / name
    names = [o[0] if isinstance(o, (tuple, list)) else o for o in lib.objects("sim")]
    trace = trace if trace is not None else (d / "trace.trace").read_bytes()
    exp = read_framehash(framehash if framehash is not None else (d / "framehash.bin").read_bytes(), len(names))
    got = replay(lib, trace, len(exp) + 1)
    r = compare(exp, got, names)
    r["name"] = name
    return r


def cmd_gate(a) -> int:
    from port.test import lib as L
    if a.backend:
        os.environ["SG_BACKEND"] = a.backend
    lib = L.load(os.environ.get("SG_PORTSIM_DLL") if L.backend() != "win32" else None)
    names = a.names or recordings()
    rows = []
    for n in names:
        t0 = time.time()
        r = gate_one(lib, n)
        r["seconds"] = round(time.time() - t0, 1)
        rows.append(r)
        print(f"{n:24s} {r['status']:5s} records {r['records']}/{r['expected']} code {r['code']} first {r['first']} "
              f"({r['seconds']} s)", flush=True)
    if a.out:
        with open(a.out, "w", newline="\n") as f:
            f.write("name\tbackend\tstatus\trecords\texpected\tcode\tfirst\n")
            for r in rows:
                f.write(f"{r['name']}\t{L.backend()}\t{r['status']}\t{r['records']}\t{r['expected']}\t{r['code']}\t"
                        f"{json.dumps(r['first'])}\n")
    return 0 if rows and all(r["status"] == "PASS" for r in rows) else 1


# ------------------------------------------------------------------------------------------------ driving the app
def _rows(p: pathlib.Path) -> int:
    if not p.exists():
        return 0
    return max(0, len([l for l in p.read_text().splitlines() if l.strip()]) - 1)


def _frames(rec: pathlib.Path) -> int:
    p = rec / "frames.tsv"
    return len([l for l in p.read_text().splitlines() if l.strip()]) if p.exists() else 0


class Driver:
    """posted window messages only (app_drive.App.post), waits measured in the session's own frame heads (frames.tsv) or
    in quiet presents (shown.tsv) while a modal screen waits"""

    def __init__(self, app, data: pathlib.Path):
        self.app = app
        self.data = data  # the app's --data directory (its saves overlay)
        self.rec = app.record
        self.tags: list[str] = []

    def post(self, msg, wp, lp, tag=""):
        self.app.post(msg, wp & 0xFFFFFFFF, lp & 0xFFFFFFFF)
        self.tags.append(tag)

    def lp(self, x, y):
        return ((y & 0xFFFF) << 16) | (x & 0xFFFF)

    def frames(self) -> int:
        return _frames(self.rec)

    def wait_frames(self, n: int, timeout: float = 120.0):
        """n more frame heads"""
        goal = self.frames() + n
        end = time.time() + timeout
        while self.frames() < goal:
            if time.time() > end:
                raise TimeoutError(f"frame {goal} never came (at {self.frames()})")
            if self.app.proc.poll() is not None:
                raise RuntimeError(f"app exited {self.app.proc.returncode}")
            time.sleep(0.02)

    def wait_quiet(self, quiet: float = 1.5, timeout: float = 60.0):
        """until no new present for `quiet` seconds (a modal screen waiting for input)"""
        p = self.rec / "shown.tsv"
        last, t_last, end = -1, time.time(), time.time() + timeout
        while time.time() < end:
            n = _rows(p)
            if n != last:
                last, t_last = n, time.time()
            elif time.time() - t_last >= quiet:
                return n
            time.sleep(0.05)
        return last

    def move(self, x, y, wp=0):
        self.post(WM_MOUSEMOVE, wp, self.lp(x, y), f"move {x} {y}")

    def click(self, x, y, right=False, mk=0, tag="click"):
        down, up, b = (WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON) if right else (WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON)
        self.post(down, b | mk, self.lp(x, y), f"{tag} {x} {y} down")
        self.post(up, mk, self.lp(x, y), f"{tag} {x} {y} up")

    def key(self, vk, tag="key", sys_key=False, alt=False, hold=0.25):
        """WM_KEYDOWN, `hold` seconds, WM_KEYUP, with the scan code (the app's TranslateMessage adds a character key's WM_CHAR
        right after the key-down, as a real key press and the original's pumps do; posting both at once would queue the
        WM_CHAR after the key-up)"""
        self.key_down(vk, tag, sys_key, alt)
        time.sleep(hold)
        self.key_up(vk, tag, sys_key, alt)

    def key_down(self, vk, tag="key", sys_key=False, alt=False):
        import ctypes
        scan = ctypes.windll.user32.MapVirtualKeyW(vk, 0) & 0xFF
        self.post(WM_SYSKEYDOWN if sys_key else WM_KEYDOWN, vk, ((1 << 29) if alt else 0) | (scan << 16) | 1,
                  f"{tag} {vk:#x} down")

    def key_up(self, vk, tag="key", sys_key=False, alt=False):
        import ctypes
        scan = ctypes.windll.user32.MapVirtualKeyW(vk, 0) & 0xFF
        self.post(WM_SYSKEYUP if sys_key else WM_KEYUP, vk,
                  ((1 << 29) if alt else 0) | (1 << 31) | (1 << 30) | (scan << 16) | 1, f"{tag} {vk:#x} up")

    def char(self, ch: str, tag="char"):
        """WM_CHAR alone (scenario.py _cova_char's rule: an uppercase key for mainLoop's H1 key switch; a posted WM_KEYDOWN
        would add TranslateMessage's lowercase character from the real keyboard state)"""
        self.post(WM_CHAR, ord(ch), 1, f"{tag} {ch!r}")

    def wheel(self, x, y, delta=120, tag="wheel"):
        """WM_MOUSEWHEEL: lParam in SCREEN coordinates (the cross-check maps it back to the client point it came from)"""
        import ctypes
        import ctypes.wintypes as W
        pt = W.POINT(x, y)
        ctypes.windll.user32.ClientToScreen(self.app.hwnd, ctypes.byref(pt))
        self.post(WM_MOUSEWHEEL, (delta & 0xFFFF) << 16, self.lp(pt.x, pt.y), f"{tag} {x} {y} {delta}")


def _to_monterey(d: Driver):
    d.wait_quiet(1.5, 120)                         # the title menu waits
    d.move(130, 445)
    d.wait_quiet(1.0, 20)
    d.click(130, 445, tag="Sandbox")
    d.wait_quiet(1.5, 60)                          # the location screen
    d.move(270, 32)
    d.wait_quiet(1.0, 20)
    d.click(270, 32, tag="Monterey")
    d.wait_frames(40, 180)                         # the world load, then frames
    d.key(VK_ESCAPE, tag="welcome Esc")
    d.wait_frames(15)


def script_hole(d: Driver):
    _to_monterey(d)
    # the inputs no corpus trace holds (U-1611..U-1614), each in its own frames: Shift click, Ctrl click, a double click,
    # the wheel, Alt+X
    d.move(400, 300)
    d.wait_frames(4)
    d.key_down(VK_SHIFT, tag="Shift")
    d.wait_frames(4)
    d.click(400, 300, mk=MK_SHIFT, tag="Shift click")
    d.wait_frames(4)
    d.key_up(VK_SHIFT, tag="Shift")
    d.wait_frames(4)
    d.move(430, 320)
    d.wait_frames(4)
    d.key_down(VK_CONTROL, tag="Ctrl")
    d.wait_frames(4)
    d.click(430, 320, mk=MK_CONTROL, tag="Ctrl click")
    d.wait_frames(4)
    d.key_up(VK_CONTROL, tag="Ctrl")
    d.wait_frames(4)
    d.move(380, 290)
    d.wait_frames(4)
    d.post(WM_LBUTTONDOWN, MK_LBUTTON, d.lp(380, 290), "dbl down")
    d.post(WM_LBUTTONUP, 0, d.lp(380, 290), "dbl up")
    d.post(WM_LBUTTONDBLCLK, MK_LBUTTON, d.lp(380, 290), "dbl dblclk")
    d.post(WM_LBUTTONUP, 0, d.lp(380, 290), "dbl up2")
    d.wait_frames(8)
    d.wheel(380, 290, 120)
    d.wait_frames(4)
    d.wheel(380, 290, -120)
    d.wait_frames(4)
    d.key_down(VK_MENU, tag="Alt", sys_key=True, alt=True)
    d.wait_frames(4)
    d.key(ord("X"), tag="Alt+X", sys_key=True, alt=True)
    d.wait_frames(4)
    d.key_up(VK_MENU, tag="Alt")                   # a released Alt after a combination comes as WM_KEYUP
    d.wait_frames(8)
    # Shift + a character key: the key-down's code and the WM_CHAR's code with the Shift bit (TranslateMessage makes the
    # character from the real keyboard state, so it is 'a' in the app and in the original alike)
    d.key_down(VK_SHIFT, tag="Shift")
    d.wait_frames(4)
    d.key(ord("A"), tag="Shift+A")
    d.wait_frames(4)
    d.key_up(VK_SHIFT, tag="Shift")
    d.wait_frames(8)
    # one hole (scenario.build_first_hole's clicks)
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
    d.key(VK_ESCAPE, tag="Esc"); d.wait_frames(60)


def script_saveload(d: Driver):
    _to_monterey(d)
    d.wait_frames(20)
    d.char("S", tag="save key")
    d.wait_quiet(1.5, 30)                          # saveGameDialog's textInputBox waits
    d.key(VK_RETURN, tag="accept the proposed name")
    d.wait_frames(30)
    d.key(VK_ESCAPE, tag="Esc the save message")
    d.wait_frames(40)
    d.char("L", tag="load key")
    d.wait_quiet(1.5, 30)                          # loadGameBrowserScreen waits
    row = _save_row(d.data)                        # the new save's row in the list
    d.move(400, 121 + 16 * row); d.wait_quiet(1.0, 10)
    d.click(400, 121 + 16 * row, tag=f"row {row}"); d.wait_quiet(1.0, 10)
    d.move(632, 553); d.wait_quiet(1.0, 10)
    d.click(632, 553, tag="OK")
    d.wait_frames(60, 180)
    d.key(VK_ESCAPE, tag="Esc"); d.wait_frames(40)


def script_report(d: Driver):
    _to_monterey(d)
    d.wait_frames(20)
    for vk in (0x70, 0x71, 0x72, 0x73):            # F1..F4: report screens (mainLoop's report byte g_ui_569498)
        d.key(vk, tag=f"F{vk - 0x6F}")
        d.wait_quiet(2.0, 30)
        d.key(VK_ESCAPE, tag="Esc report")
        d.wait_frames(20)
    d.wait_frames(30)


SCRIPTS = {"hole": script_hole, "saveload": script_saveload, "report": script_report}


def _save_row(data: pathlib.Path) -> int:
    """the row the browser lists the app's new save at: &AutoSave1/2 first, then every save by name (scenario.py
    _cova_load_row's rule), the install's and the overlay's together"""
    names = {p.name for p in (INSTALL / "Saved Games").glob("*.sve")}
    over = data / "files" / "Saved Games"
    new = sorted(p.name for p in over.glob("*.sve")) if over.is_dir() else []
    allnames = names | set(new)
    auto = sorted(n for n in allnames if n.startswith("&"))
    rest = sorted((n for n in allnames if not n.startswith("&")), key=str.upper)
    order = auto + rest
    # the save key's file: not an autosave, not While_Browsing.sve (the browser saves the running game under that name when
    # it opens, so loading it would change nothing)
    cand = [n for n in new if not n.startswith("&") and n.lower() != "while_browsing.sve"]
    return order.index(cand[-1]) if cand else -1


def find_exe(exe: str | None) -> pathlib.Path:
    for p in ([pathlib.Path(exe)] if exe else []) + ([pathlib.Path(os.environ["SG_APP_EXE"])] if os.environ.get("SG_APP_EXE")
                                                     else []) + EXE_CANDIDATES:
        if p.exists():
            return p
    raise SystemExit("no simgolf_port.exe (pass --exe or set SG_APP_EXE)")


def record(script: str, name: str, exe: pathlib.Path, extra=(), keep_tmp=False) -> pathlib.Path:
    sys.path.insert(0, str(ROOT / "port" / "app" / "tools"))
    import app_drive
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="s7_4_"))
    rec, data = tmp / "rec", tmp / "data"
    args = ["--install", str(INSTALL), "--data", str(data), "--record-dir", str(rec), "--no-activate", "--dpi", "96",
            "--cursor", "message", "--keys", "message", "--clock-origin", str(CLOCK_ORIGIN), *extra]
    app = app_drive.launch(exe, args, cwd=str(ROOT))
    d = Driver(app, data)
    err = ""
    try:
        SCRIPTS[script](d)
    except Exception as e:  # noqa: BLE001 - the session is kept for diagnosis
        err = f"{type(e).__name__}: {e}"
    finally:
        rc = app.close(timeout=120)
    out = CORPUS / name
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(rec, out)
    events = [json.loads(l) for l in (rec / "input.jsonl").read_text().splitlines() if l.strip()] \
        if (rec / "input.jsonl").exists() else []
    seqs, k = [], 0
    for e in events:  # app_drive.matched's walk: each posted message's event seq (the others are TranslateMessage's)
        if k < len(app.posted) and (e["msg"], e["wp"], e["lp"]) == app.posted[k]:
            seqs.append(e["seq"])
            k += 1
    posted = [{"msg": m, "wp": w, "lp": l, "tag": d.tags[i] if i < len(d.tags) else "", "seq": seqs[i] if i < len(seqs) else -1}
              for i, (m, w, l) in enumerate(app.posted)]
    (out / "posted.json").write_text(json.dumps(posted, indent=0), newline="\n")
    (out / "script.json").write_text(json.dumps({"script": script, "args": args, "exe": str(exe), "rc": rc, "error": err,
                                                 "exe_mtime": exe.stat().st_mtime, "matched": k, "posted": len(app.posted),
                                                 "recorded": time.strftime("%Y-%m-%d %H:%M:%S")}, indent=1), newline="\n")
    if data.is_dir():
        shutil.copytree(data, out / "data")
    if not keep_tmp:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f"record {script} -> {out}: rc {rc} error {err!r} posted {len(app.posted)} matched {k}")
    return out


def cmd_record(a) -> int:
    out = record(a.script, a.name or a.script, find_exe(a.exe))
    s = json.loads((out / "session.json").read_text())
    print(json.dumps(s.get("session", {}), indent=None)[:600])
    return 0 if not json.loads((out / "script.json").read_text())["error"] else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("record")
    p.add_argument("script", choices=sorted(SCRIPTS))
    p.add_argument("--name")
    p.add_argument("--exe")
    p.set_defaults(fn=cmd_record)
    p = sp.add_parser("gate")
    p.add_argument("names", nargs="*")
    p.add_argument("--backend", choices=["x64", "win32"])
    p.add_argument("--out")
    p.set_defaults(fn=cmd_gate)
    p = sp.add_parser("list")
    p.set_defaults(fn=lambda a: print("\n".join(recordings())) or 0)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
