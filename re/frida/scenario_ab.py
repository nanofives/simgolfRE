r"""Canonical-scenario ON/OFF A/B (C4 evidence, re/CONFIDENCE.md): run one scenario of re/tools/scenario.py on the same
build with a hook set OFF and ON, under a deterministic main thread (re/frida/js/scenario_det.js), and compare the
observable game state at fixed checkpoints: the .sve globals (re/tools/sve.py FIELDS and both tail blocks), the RNG
seed 0x00822d9c, and the frame.

    # 1. determinism: a RECORD run (input timed by screen waits, schedule saved) and a REPLAY run, both hooks OFF
    py -3.12 re/frida/scenario_ab.py determinism --scenario sandbox_basic
    # 2. A/B: replay the recorded schedule with hooks OFF and with the named hooks ON (all other hooks OFF)
    py -3.12 re/frida/scenario_ab.py ab --scenario sandbox_basic --on golf_clean.exe:0x45c1a0 jgld.dll:0x11dd0 ...
    #    --on all: every registered hook ON; --negctl <dll>: the ON arm runs a control build (never writes log/diff)
    # 3. spawn-time reach (opt-in, round 36 s36c): the same commands with --spawn-reach arm the reach breakpoints before
    #    the exe entry runs (static initializers and early boot count as reached); outputs under <scenario>_spawn
    py -3.12 re/frida/scenario_ab.py determinism --scenario sandbox_basic --spawn-reach
    py -3.12 re/frida/scenario_ab.py ab --scenario sandbox_basic --spawn-reach --on all
    # 4. finer than a checkpoint (opt-in, batch c3fy2): --trace va:n,... reads those bytes at EVERY poll (100 virtual ms),
    #    --watch va:size,... (<= 4) logs every main-thread write that changes them with the writing pc and caller chain;
    #    both go to <run>/trace.json, and `tracecmp` prints the first poll where two runs differ and both watch logs there
    py -3.12 re/frida/scenario_ab.py ab --scenario course_season --on 0x40f5c0 --trace 575eb8:4 --watch 575eb8:4
    py -3.12 re/frida/scenario_ab.py tracecmp --scenario course_season --a ab_set1_off --b ab_set1_on
    # 5. extended oracle (opt-in, batch c3fyl): --call-log va:n[:ra/ra][:sK][:r][:xK],... logs main-thread calls of exe
    #    functions (arguments, string, return value) and --extra-regions va:n,... dumps more memory at each checkpoint;
    #    both are compared between the arms; `extcmp --a <run dir> --b <run dir>` compares two finished runs
    py -3.12 re/frida/scenario_ab.py ab --scenario cov_k1_magazine_d2900 --on 0x40f5c0 --call-log 4481b0:5

Every run uses the install copy instances\c3ft (--instance) and SIMGOLF_ALLOW_MULTI=1, so other sessions' games are
never touched. The ON set is written to the copy's simgolf_shim.ini [hooks] section (every registered hook not in the set
= 0; the registered list is read from the shim log of an OFF boot), because SIMGOLF_HOOKS_OFF cannot list ~2,900 hooks in
one environment variable (32,767 characters). The ini key is the address without its module, so a DLL RVA another module
shares is switched off through SIMGOLF_HOOKS_OFF=<module>:<rva> instead.

Output: log/c3/alt/c3ft/<scenario>/<arm>/ (checkpoint dumps, frames, poll log, summary.json) and, for an `ab` run,
log/diff/<addr>_<name>.scenario.csv per function in the ON set (DLL: <rva>_<stem>_<name>.scenario.csv, the scheme of
diff_hook.py), with one row per checkpoint, the install witness row and a VERDICT row (GREEN / RED / UNREACHED).
"""
from __future__ import annotations

import argparse
import csv
import ctypes
import datetime
import hashlib
import json
import os
import pathlib
import queue
import re
import shutil
import struct
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
INSTANCE_DEFAULT = ROOT / "instances" / "c3ft"


def _early_instance():
    """SIMGOLF_GAME_DIR must be set before game.py is imported (it reads it at import)."""
    if "--instance" in sys.argv:
        return pathlib.Path(sys.argv[sys.argv.index("--instance") + 1]).resolve()
    return INSTANCE_DEFAULT


INSTANCE = _early_instance()
os.environ["SIMGOLF_GAME_DIR"] = str(INSTANCE)
os.environ["SIMGOLF_ALLOW_MULTI"] = "1"
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(pathlib.Path(__file__).parent))

import imgcmp  # noqa: E402
import numpy as np  # noqa: E402
import pefile  # noqa: E402
import sve  # noqa: E402
from game import GAME_H, GAME_W, Game, JS_DIR  # noqa: E402
from PIL import Image  # noqa: E402

OUT_ROOT = ROOT / "log" / "c3" / "alt" / "c3ft"
DIFF_DIR = ROOT / "log" / "diff"
GOLDEN = ROOT / "tests" / "golden"
HOOKS_CSV = ROOT / "hooks.csv"

# ---------------------------------------------------------------- clock / slot configuration
STEP_MS = 1                      # virtual ms per main-thread timeGetTime call
T0 = 0x00100000                  # virtual timeGetTime at boot
POLL_MS = 100                    # virtual ms between polls
REARM_MS = int(os.environ.get("C3FT_REARM_MS", "1000"))   # reach breakpoints re-armed once per this many virtual ms:
                                 # a reach count is the number of such intervals with >= 1 entry of the function
SLOT_SPECS = {                   # import slots the agent redirects (module -> {'DLL!Name': kind})
    "golf_clean.exe": {"WINMM.dll!timeGetTime": "tgt", "USER32.dll!SetTimer": "SetTimer", "USER32.dll!KillTimer": "KillTimer",
                       "WINMM.dll!timeSetEvent": "timeSetEvent", "WINMM.dll!timeKillEvent": "timeKillEvent"},
    "jgld.dll": {"KERNEL32.dll!QueryPerformanceCounter": "qpc"},
    "sound.dll": {"WINMM.dll!timeGetTime": "tgt"},
    "binkw32.dll": {"WINMM.dll!timeGetTime": "tgt", "KERNEL32.dll!QueryPerformanceCounter": "qpc"},
}
# AUDIO STAND-IN (batch b2): with C3FT_AUDIO=none (the default) sound.dll's DirectSound enumeration / creation and
# binkw32's waveOutOpen get the answers of a PC with no audio output (scenario_det.js, re/analysis/data/
# scenario_baseline.md), so the boot's timeline no longer depends on whether a device (the TV's HDMI endpoint) is
# present; C3FT_AUDIO=live leaves the real APIs (the timeline then follows the device, as before 2026-10-17).
AUDIO_SLOTS = {"sound.dll": {"DSOUND.dll!#2": "audio:dsenum", "ole32.dll!CoCreateInstance": "audio:cocreate"},
               "binkw32.dll": {"WINMM.dll!waveOutOpen": "audio:waveOutOpen"}}


def frame_mode() -> str:
    """b5: where the checkpoint frame comes from: `backbuffer` (default) or `window` (the capture used before b5)."""
    m = os.environ.get("C3FT_FRAME", "backbuffer")
    if m not in ("backbuffer", "window"):
        raise SystemExit(f"C3FT_FRAME={m}: backbuffer or window")
    return m


def stack_seed() -> str:
    """b5: zero-fill the stack below courseActionMenu's entry (scenario_det.js STACK SEED): `zero` (default); `probe`
    (C3FT_STACK_SEED=probe) only records the three uninitialised words natively; C3FT_STACK_SEED=0 returns "" and leaves
    the thunk 0x00470a00 untouched (the agent's key is then absent)."""
    m = os.environ.get("C3FT_STACK_SEED", "1")
    if m not in ("0", "1", "probe"):
        raise SystemExit(f"C3FT_STACK_SEED={m}: 1, probe or 0")
    return {"0": "", "1": "zero", "probe": "probe"}[m]


def back_buffer_image(meta: dict, bits) -> "Image.Image | None":
    """jgld's back buffer as the 800x600 RGB picture the window shows: RGB555 (BI_RGB, or BI_BITFIELDS 0x7c00/0x3e0/0x1f)
    or RGB565 (BI_BITFIELDS 0xf800/0x7e0/0x1f), channels widened by (v << 3) | (v >> 2) (6-bit green: (v << 2) | (v >> 4)).
    Rows are top-down: GetObjectA reports biHeight +600 for this DIB, but its first row in memory is the top of the
    window (measured: the unflipped rows equal the window capture, the flipped ones differ in 418,146 px). None when
    there is no such buffer."""
    if not meta.get("ok") or bits is None or meta.get("bpp") != 16 or (meta.get("w"), abs(meta.get("h") or 0)) != (GAME_W, GAME_H):
        return None
    comp, masks = meta.get("comp"), tuple(meta.get("masks") or ())
    if comp == 0 or (comp == 3 and masks == (0x7C00, 0x3E0, 0x1F)):
        fmt = 555
    elif comp == 3 and masks == (0xF800, 0x7E0, 0x1F):
        fmt = 565
    else:
        return None
    wb = meta["wb"]
    v = np.frombuffer(bits, np.uint16, count=(wb // 2) * GAME_H).reshape(GAME_H, wb // 2)[:, :GAME_W].astype(np.uint16)
    w5 = lambda x: ((x << 3) | (x >> 2)).astype(np.uint8)
    if fmt == 555:
        r, g, b = w5((v >> 10) & 31), w5((v >> 5) & 31), w5(v & 31)
    else:
        r, g, b = w5((v >> 11) & 31), (((v >> 5) & 63) << 2 | ((v >> 5) & 63) >> 4).astype(np.uint8), w5(v & 31)
    return Image.fromarray(np.ascontiguousarray(np.stack([r, g, b], axis=2)), "RGB")


def audio_mode() -> str:
    m = os.environ.get("C3FT_AUDIO", "none")
    if m not in ("none", "live"):
        raise SystemExit(f"C3FT_AUDIO={m}: none or live")
    return m


EXTRA_REGIONS = [(0x00822D9C, 4, "rng_seed"), (0x0051B380, 4, "seed_time")]
UNOBSERVED_MODULES = {"sound.dll"}   # modules whose effects no checkpoint observes: a SAME run gives them UNOBSERVED


# ---------------------------------------------------------------- opt-in trace / write watch (batch c3fy2, 2026-10-16)
def parse_spans(spec: str | None) -> list[list[int]]:
    """`va:n,va:n` (hex va, n decimal or 0x) -> [[va, n], ...]; None or '' -> []."""
    out = []
    for part in (spec or "").split(","):
        part = part.strip()
        if not part:
            continue
        va, _, n = part.partition(":")
        out.append([int(va, 16), int(n or "4", 0)])
    return out


def trace_spec() -> list[list[int]]:
    """--trace / C3FT_TRACE: regions dumped at every poll (scenario_det.js cfg.trace). Default: none."""
    return parse_spans(os.environ.get("C3FT_TRACE"))


def watch_spec() -> list[dict]:
    """--watch / C3FT_WATCH: main-thread hardware write watchpoints (scenario_det.js cfg.watch), size 1/2/4, <= 4."""
    w = [{"va": va, "size": n} for va, n in parse_spans(os.environ.get("C3FT_WATCH"))]
    if len(w) > 4 or any(x["size"] not in (1, 2, 4) for x in w):
        raise SystemExit("--watch: at most 4 addresses, size 1, 2 or 4")
    return w


# ---------------------------------------------------------------- opt-in extended oracle (batch c3fyl, 2026-10-17)
def call_log_spec() -> list[dict]:
    """--call-log / C3FT_CALL_LOG: `va:n[:ra/ra...][:sK][:r][:xK],...` (hex va and return addresses, n decimal) ->
    scenario_det.js cfg.callLog: every main-thread entry of the exe function va is logged with its return address and n
    stack arguments, only for the listed return addresses when given, plus the C string at argument K with `sK` and the
    return value (eax) with `r`; `xK` keeps argument K out of the comparison (a heap or stack address). Default: none."""
    out = []
    for part in (os.environ.get("C3FT_CALL_LOG") or "").split(","):
        part = part.strip()
        if not part:
            continue
        f = part.split(":")
        e = {"va": int(f[0], 16), "n": int(f[1]) if len(f) > 1 and f[1] else 0}
        for x in f[2:]:
            if x.startswith("s"):
                e["s"] = int(x[1:])
            elif x == "r":
                e["r"] = 1
            elif x.startswith("x"):
                e.setdefault("x", []).append(int(x[1:]))
            elif x:
                e["ra"] = [int(r, 16) for r in x.split("/")]
        if not 0x401000 <= e["va"] < 0x4b0000 or e["n"] > 16 or (e.get("s") is not None and not 0 <= e["s"] < max(e["n"], 1)):
            raise SystemExit(f"--call-log {part}: va in golf_clean.exe .text, n <= 16, sK with K < n")
        out.append(e)
    return out


def extra_regions_spec() -> list[list[int]]:
    """--extra-regions / C3FT_EXTRA_REGIONS: `va:n,...` read at every checkpoint into <name>.extra.bin, outside the
    default dump blob. Default: none."""
    return parse_spans(os.environ.get("C3FT_EXTRA_REGIONS"))


def _calls_by_checkpoint(run_dir: pathlib.Path) -> dict | None:
    p = run_dir / "calls.json"
    if not p.exists():
        return None
    d = json.loads(p.read_text())
    out: dict = {}
    masks = {e["va"]: e.get("x", []) for e in (d.get("spec") or [])}
    for row in d["rows"]:
        r = list(row[1:])
        for k in masks.get(r[1], []):        # `xK`: argument K logged but not compared (a heap or stack pointer)
            if 3 + k < len(r):
                r[3 + k] = None
        # the return address is logged but not compared: in the ON arm a call the hooked function makes returns into its
        # reimplementation (shim code, laid out per build and load address), in the OFF arm into golf_clean.exe, so two
        # arms are compared by the calls made (time, function, arguments, string, return value), not by where from
        if len(r) > 2:
            r[2] = None
        out.setdefault(row[0], []).append(r)
    return {"spec": d.get("spec"), "dropped": d.get("dropped", 0), "by_cp": out}


def compare_ext(a_dir: pathlib.Path, b_dir: pathlib.Path) -> dict | None:
    """The extended oracle of two runs made with the same --call-log / --extra-regions: per checkpoint, the calls logged
    since the previous checkpoint (T, va, return address, arguments, string) and the extra region bytes. None when
    neither run has extended data (the default)."""
    ca, cb = _calls_by_checkpoint(a_dir), _calls_by_checkpoint(b_dir)
    sa = json.loads((a_dir / "summary.json").read_text())
    sb = json.loads((b_dir / "summary.json").read_text())
    xa, xb = sa.get("extra_regions"), sb.get("extra_regions")
    if ca is None and cb is None and not xa and not xb:
        return None
    out = {"call_spec": [ca and ca["spec"], cb and cb["spec"]], "extra_spec": [xa, xb], "checkpoints": {}}
    if (ca is None) != (cb is None) or (ca and ca["spec"] != cb["spec"]) or xa != xb:
        out["incomparable"] = "the two runs logged different calls / regions"
        return out
    for name in sa["checkpoints"]:
        if name not in sb["checkpoints"]:
            continue
        r = {}
        if ca is not None:
            la, lb = ca["by_cp"].get(name, []), cb["by_cp"].get(name, [])
            first = next((i for i, (x, y) in enumerate(zip(la, lb)) if x != y), None)
            if first is None and len(la) != len(lb):
                first = min(len(la), len(lb))
            r["calls"] = [len(la), len(lb)]
            if first is not None:
                r["calls_first_diff"] = {"index": first, "a": la[first] if first < len(la) else None,
                                         "b": lb[first] if first < len(lb) else None}
        if xa:
            ba, bb = (d / f"{name}.extra.bin" for d in (a_dir, b_dir))
            if ba.exists() and bb.exists():
                pa, pb = ba.read_bytes(), bb.read_bytes()
                diffs, off = [], 0
                for va, n in xa:
                    idx = [i for i in range(n) if pa[off + i] != pb[off + i]]
                    if idx:
                        diffs.append({"va": f"0x{va:08x}", "bytes": len(idx), "first": f"0x{va + idx[0]:08x}"})
                    off += n
                r["extra_differ"] = diffs
        r["same"] = "calls_first_diff" not in r and not r.get("extra_differ")
        out["checkpoints"][name] = r
    out["dropped"] = [ca and ca["dropped"], cb and cb["dropped"]]
    out["same"] = all(r["same"] for r in out["checkpoints"].values())
    return out


def print_compare_ext(ext: dict | None):
    if ext is None:
        return
    if ext.get("incomparable"):
        print(f"  extended oracle: INCOMPARABLE ({ext['incomparable']})")
        return
    for name, r in ext["checkpoints"].items():
        s = "SAME" if r["same"] else "DIFF"
        det = []
        if "calls" in r:
            det.append(f"calls {r['calls'][0]}/{r['calls'][1]}")
        if r.get("calls_first_diff"):
            det.append(f"first call diff #{r['calls_first_diff']['index']}: {r['calls_first_diff']['a']} / "
                       f"{r['calls_first_diff']['b']}")
        if r.get("extra_differ"):
            det.append("extra " + ", ".join(f"{d['va']}({d['bytes']}@{d['first']})" for d in r["extra_differ"][:6]))
        print(f"  ext {name}: {s}  " + "  ".join(det))
    print(f"  extended oracle (call log / extra regions): {'SAME' if ext['same'] else 'DIFF'}"
          + (f", dropped rows {ext['dropped']}" if any(ext["dropped"]) else ""))


def slot_config() -> list[dict]:
    out = []
    real_timers = os.environ.get("C3FT_REAL_TIMERS") == "1"
    audio = audio_mode() == "none"
    for mod, want in SLOT_SPECS.items():
        if real_timers:
            want = {k: v for k, v in want.items() if v in ("tgt", "qpc")}
        if audio:
            want = {**want, **AUDIO_SLOTS.get(mod, {})}
        pe = pefile.PE(str(ROOT / "original" / mod), fast_load=True)
        pe.parse_data_directories()
        base = pe.OPTIONAL_HEADER.ImageBase
        found = set()
        for d in pe.DIRECTORY_ENTRY_IMPORT:
            for i in d.imports:
                key = f"{d.dll.decode()}!{i.name.decode() if i.name else f'#{i.ordinal}'}"
                k2 = next((k for k in want if k.lower() == key.lower()), None)
                if k2:
                    out.append({"module": mod, "rva": i.address - base, "kind": want[k2]})
                    found.add(k2)
        missing = set(want) - found
        if missing:
            raise SystemExit(f"{mod}: imports not found {missing}")
    return out


def regions() -> list[tuple[int, int, str]]:
    regs = [(va, n, sve.NAMES.get(va, f"g_{va:08X}")) for va, n in sve.FIELDS]
    for _bit, extra in (sve.TAIL_A, sve.TAIL_B):
        regs += [(va, n, f"tail_{sve.NAMES.get(va, f'g_{va:08X}')}") for va, n in extra]
    regs += EXTRA_REGIONS
    return regs


def qpc_freq() -> int:
    f = ctypes.c_longlong()
    ctypes.windll.kernel32.QueryPerformanceFrequency(ctypes.byref(f))
    return f.value


# ---------------------------------------------------------------- hook sets
def registered_hooks(log_path: pathlib.Path) -> list[tuple[str, str, int]]:
    """(name, module, rva) of every SG_HOOK the shim registered, from its log (`re-hook <name> <module>+<rva>: ...`)."""
    out = []
    for line in log_path.read_text(errors="replace").splitlines():
        m = re.search(r"re-hook (.*) (\S+)\+([0-9a-fA-F]{8}): (OFF|ON|\S.*)$", line)
        if m and (m.group(4) in ("OFF", "ON") or True):
            key = (m.group(1), m.group(2), int(m.group(3), 16))
            if key not in out:
                out.append(key)
    return out


def parse_target(t: str) -> tuple[str, int]:
    if ":" in t:
        mod, a = t.split(":", 1)
    else:
        mod, a = "golf_clean.exe", t
    return mod, int(a, 16)


def write_ini_hooks(off: list[tuple[str, int]]):
    """Replace the [hooks] section of the instance's simgolf_shim.ini with `<rva>=0` for every hook in `off`."""
    ini = INSTANCE / "simgolf_shim.ini"
    text = ini.read_text()
    text = re.sub(r"\n\[hooks\][^\[]*", "\n", text)
    if off:
        keys = sorted({f"{a:08x}" for _m, a in off})
        text = text.rstrip("\n") + "\n\n[hooks]\n" + "".join(f"{k}=0\n" for k in keys)
    ini.write_text(text)


def hooks_csv_rows() -> dict[tuple[str, int], dict]:
    rows = {}
    with HOOKS_CSV.open(newline="") as f:
        for r in csv.DictReader(f):
            rows[((r.get("module") or "golf_clean.exe").lower(), int(r["addr"], 16))] = r
    return rows


# ---------------------------------------------------------------- the lockstep driver
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP = 0x200, 0x201, 0x202, 0x204, 0x205
WM_KEYDOWN, WM_KEYUP, WM_CHAR = 0x100, 0x101, 0x102


class Ctx:
    """What a scenario script sees at each poll. Times are virtual ms."""

    def __init__(self, run: "Run"):
        self.run = run
        self.T = 0
        self.actions: list[dict] = []
        self.dump_name: str | None = None
        self.wait_index = 0

    # ---- input (actions run on the game's main thread at T + dt)
    def act(self, dt, **a):
        a["dt"] = int(dt)
        self.actions.append(a)

    def lp(self, x, y):
        """Logical (800x600) coordinates. game.py's Game._lp scales them to physical pixels because Windows rescales a
        message posted from the DPI-aware harness process to the DPI-unaware game window; these messages are posted by
        the game's own main thread, so nothing rescales them (physical ones landed off-screen at 250 %: the listener's
        hit test 0x0047f700 in its button handler 0x00495aa0 found no window and the click was dropped)."""
        return ((int(y) & 0xFFFF) << 16) | (int(x) & 0xFFFF)

    def move(self, x, y, dt=0):
        self.act(dt, k="cursor", x=int(x), y=int(y), tag=f"cursor {x},{y}")
        self.act(dt, k="post", msg=WM_MOUSEMOVE, wp=0, lp=self.lp(x, y))

    def click(self, x, y, right=False, hold=150, settle=350):
        lp = self.lp(x, y)
        vk = 0x02 if right else 0x01
        down, up, mk = (WM_RBUTTONDOWN, WM_RBUTTONUP, 2) if right else (WM_LBUTTONDOWN, WM_LBUTTONUP, 1)
        self.move(x, y)
        self.act(settle, k="post", msg=WM_MOUSEMOVE, wp=0, lp=lp)
        self.act(settle, k="vkey", vk=vk, st=2, tag=f"{'r' if right else 'l'}down {x},{y}")
        self.act(settle, k="post", msg=down, wp=mk, lp=lp)
        self.act(settle + hold, k="vkey", vk=vk, st=1)
        self.act(settle + hold, k="post", msg=up, wp=0, lp=lp)
        yield from self.sleep(settle + hold + 100)

    def key(self, vk, hold=150, char=None):
        if char is None and (0x30 <= vk <= 0x39 or 0x41 <= vk <= 0x5A):
            char = chr(vk).lower()
        self.act(0, k="vkey", vk=vk, st=2, tag=f"key {vk:#x}")
        self.act(0, k="post", msg=WM_KEYDOWN, wp=vk, lp=1)
        if char:
            self.act(0, k="post", msg=WM_CHAR, wp=ord(char), lp=1)
        self.act(hold, k="vkey", vk=vk, st=1)
        self.act(hold, k="post", msg=WM_KEYUP, wp=vk, lp=0xC0000001)
        yield from self.sleep(hold + 50)

    def write32(self, addr, value, tag=""):
        self.act(0, k="w32", addr=addr, v=value, tag=tag)

    # ---- time
    def sleep(self, ms):
        end = self.T + ms
        while self.T < end:
            yield

    def wait_screen(self, ref, crop=None, threshold=imgcmp.DEFAULT_THRESHOLD, timeout=120_000, name=""):
        """RECORD: poll the frame until it matches `ref`; the T it matched at goes into the schedule.
        REPLAY: wait until the recorded T (no screenshots), so input lands at the same virtual time in every arm."""
        key = f"{self.wait_index}:{name}"
        self.wait_index += 1
        if self.run.mode == "replay":
            t = self.run.schedule["waits"][key]
            while self.T < t:
                yield
            return
        t0 = self.T
        while True:
            if not self.run.g.hwnd():          # boot: no window yet
                yield
                continue
            img = self.run.g.shot()
            if crop:
                img = img.crop(crop)
            if imgcmp.matches(img, ref, threshold):
                self.run.schedule["waits"][key] = self.T
                return
            if self.T - t0 > timeout:
                raise TimeoutError(f"screen {name} never matched within {timeout} virtual ms")
            yield

    def checkpoint(self, name):
        self.dump_name = name
        yield


class Run:
    """One boot of the game under scenario_det.js, driven through one scenario script."""

    def __init__(self, label, scenario_name, mode, env, reach, witness, schedule=None, out_dir=None, spawn_reach=False):
        self.spawn_reach = spawn_reach       # --spawn-reach: breakpoints armed before the exe entry (scenario_det.js)
        self.label = label
        self.scenario_name = scenario_name
        self.mode = mode                     # 'record' | 'replay'
        self.env = env
        self.reach = reach                   # [{module, rva, arm}]
        self.witness = witness               # [{module, rva}]
        self.schedule = schedule or {"waits": {}}
        self.out = out_dir
        self.q: queue.Queue = queue.Queue()
        self.checkpoints: dict[str, dict] = {}
        self.polls = 0
        self.errors: list = []
        self.agent_log: list = []
        self.trace: list = []
        self.debug_frames = int(os.environ.get("C3FT_DEBUG_FRAMES", "0"))
        self.regs = regions()
        self.trace_spec = trace_spec()       # opt-in (--trace): [[va, n]] read at every poll
        self.watch_spec = watch_spec()       # opt-in (--watch): [{va, size}] write watchpoints
        self.trace_rows: list = []           # [T, hex] per poll
        self.watch_rows: list = []           # agent watch log entries
        self.watch_status = None
        self.clock_log = int(os.environ.get("C3FT_CLOCK_LOG", "0") or 0)   # opt-in (--clock-log N, batch b2)
        self.clock_rows: list = []           # [checkpoint, index, kind, slot module, caller] per recorded clock call
        self.msg_log = int(os.environ.get("C3FT_MSG_LOG", "0") or 0)       # opt-in (--msg-log N, batch b2)
        self.msg_rows: list = []             # [checkpoint, T, msg, wParam, lParam] per windowProc entry
        self.call_log = call_log_spec()      # opt-in (--call-log, batch c3fyl)
        self.call_rows: list = []            # [checkpoint, T, va, return address, args..., string]
        self.extra_regs = extra_regions_spec()   # opt-in (--extra-regions, batch c3fyl)

    def _on_message(self, m, data):
        if m.get("type") == "send":
            p = m["payload"]
            if isinstance(p, dict) and p.get("type") in ("poll", "dump", "ready", "error", "full", "extra", "bb"):
                self.q.put((p, data))
        elif m.get("type") == "error":
            self.q.put(({"type": "error", "where": "script", "msg": m.get("description", "") + " " + str(m.get("stack", ""))}, None))

    def agent_source(self) -> pathlib.Path:
        dbg_reach = [{"module": "golf_clean.exe", "rva": int(x, 16), "arm": "off"}
                     for x in os.environ.get("C3FT_DEBUG_REACH", "").split(",") if x]
        cfg = {"step": STEP_MS, "qstep": max(qpc_freq() // 1000, 1) * STEP_MS, "t0": T0, "q0": 0x10000000,
               "slots": slot_config(), "regions": [[va, n] for va, n, _ in self.regs], "witness": self.witness,
               "reach": self.reach + dbg_reach, "firstPoll": T0 + POLL_MS, "rearmMs": REARM_MS, "debugBt": bool(os.environ.get("C3FT_DEBUG_BT")),
               "debugMsg": bool(os.environ.get("C3FT_DEBUG_MSG"))}
        if self.spawn_reach:                 # the key is absent in the default mode, so its agent source is unchanged
            cfg["spawnReach"] = True
        if self.trace_spec:                  # opt-in, absent by default (c3fy2)
            cfg["trace"] = self.trace_spec
        if self.watch_spec:
            cfg["watch"] = self.watch_spec
        if self.clock_log:                   # opt-in (b2): the first N main-thread clock calls with their callers
            cfg["clockLog"] = self.clock_log
        if self.msg_log:                     # opt-in (b2): jgld windowProc entries on the main thread
            cfg["msgLog"] = self.msg_log
        if self.call_log:                    # opt-in (c3fyl): main-thread call log of exe functions
            cfg["callLog"] = self.call_log
        if self.extra_regs:                  # opt-in (c3fyl): regions dumped apart from the default blob
            cfg["extraRegions"] = self.extra_regs
        if audio_mode() == "none":           # b2: the device-less audio answers (default)
            cfg["audio"] = "none"
        if stack_seed():                     # b5: courseActionMenu's uninitialised frame words read 0 (default)
            cfg["stackSeed"] = stack_seed()
        src ="const SCENARIO_CFG = " + json.dumps(cfg) + ";\n" + (JS_DIR / "scenario_det.js").read_text()
        self.out.mkdir(parents=True, exist_ok=True)
        p = self.out / "agent.js"
        p.write_text(src)
        return p

    def post(self, sc, **kw):
        sc.post({"type": "go", **kw})

    def wait_msg(self, kinds, timeout=180):
        while True:
            try:
                p, data = self.q.get(timeout=timeout)
            except queue.Empty:
                where = None
                if self.g.alive:
                    try:
                        where = self.g.scripts[-1].exports_sync.main_where()
                    except Exception as e:  # noqa: BLE001
                        where = f"main_where failed: {e}"
                raise TimeoutError(f"{self.label}: no message from the agent in {timeout} s (game alive={self.g.alive}); "
                                   f"main thread: {where}")
            if p["type"] == "error":
                self.errors.append(p)
                print(f"  [{self.label}] agent error: {p.get('where')}: {p.get('msg', '')[:400]}", flush=True)
                continue
            if p["type"] in kinds:
                return p, data

    def take_dump(self, sc, name):
        self.post(sc, dump=True, hwnd=self.hwnd_val())
        p, data = self.wait_msg(("dump",))
        img, shot_info = self.checkpoint_frame(sc, name)
        img.save(self.out / f"{name}.png")
        if os.environ.get("C3FT_FULLDUMP") == "1":
            # opt-in (batch b2): the exe's whole .data section at this checkpoint, for diffs outside the compared regions
            self.post(sc, full=True, hwnd=self.hwnd_val())
            _pf, full = self.wait_msg(("full",))
            (self.out / f"{name}.full.bin").write_bytes(full)
        if self.extra_regs:
            # opt-in (--extra-regions, batch c3fyl): the extra regions, apart from the default blob
            self.post(sc, extra=True, hwnd=self.hwnd_val())
            _px, xdata = self.wait_msg(("extra",))
            (self.out / f"{name}.extra.bin").write_bytes(xdata)
        if p.get("cl") is not None:
            # opt-in (--call-log, batch c3fyl): the calls logged since the previous checkpoint
            self.call_rows += [[name] + r for r in p["cl"]["rows"]]
            (self.out / "calls.json").write_text(json.dumps({"spec": self.call_log, "dropped": p["cl"]["dropped"],
                                                             "rows": self.call_rows}))
        (self.out / f"{name}.bin").write_bytes(data)
        if p.get("wm") is not None:
            self.msg_rows += [[name] + e for e in p["wm"]]
            (self.out / "wndmsg.json").write_text(json.dumps({"limit": self.msg_log, "rows": self.msg_rows}))
        if p.get("clk"):
            self.clock_rows += [[name] + e for e in p["clk"]["entries"]]
            (self.out / "clock.json").write_text(json.dumps({"limit": self.clock_log, "total": p["clk"]["total"],
                                                             "rows": self.clock_rows}))
        self.checkpoints[name] = {"T": p["st"]["T"], "status": p["st"], "witness": p["witness"], "reach": p["reach"],
                                  "frame_sha": hashlib.sha256(img.tobytes()).hexdigest()}
        if shot_info is not None:
            self.checkpoints[name]["shot"] = shot_info
        print(f"  [{self.label}] checkpoint {name} T={p['st']['T'] - T0} tgt={p['st']['tgt']} "
              f"date=0x{self.region_u32(data, 0x00834170):08x} seed=0x{self.region_u32(data, 0x00822D9C):08x}", flush=True)

    def checkpoint_frame(self, sc, name):
        """The checkpoint's frame (batch b5). Default C3FT_FRAME=backbuffer: jgld's back buffer, the DIB section the game
        draws into and Display::present 0x10066470 BitBlts to the window, read on the blocked main thread (scenario_det.js
        backBuffer) and converted from RGB555 by (v << 3) | (v >> 2) per channel, which is the window capture's value for
        every pixel while the window is current (0 px differ in the 12 checkpoints compared). The window is captured too:
        when it differs, the window picture is kept as <name>.window.png and the count goes into the checkpoint's "shot"
        entry (window_px_differ). The location screen of cov_b_career_phx showed a window older than the back buffer in
        1 of 6 boots for 90 virtual s while the game presented the full rect every frame, the stale picture coming from
        outside the game (re/analysis/data/scenario_baseline.md section 11). C3FT_FRAME=window: the window capture, as
        before b5. Falls back to the window, with the reason in "shot", when there is no 800x600 RGB555 / 565 back buffer."""
        if frame_mode() == "window":
            return self.stable_shot(name)
        self.post(sc, bb=True, hwnd=self.hwnd_val())
        pb, bits = self.wait_msg(("bb",))
        meta = pb.get("meta") or {}
        img = back_buffer_image(meta, bits)
        if img is None:
            win, info = self.stable_shot(name)
            return win, {**(info or {}), "source": "window", "why": meta.get("why") or f"back buffer {meta}"}
        win = self.g.shot()
        a, b = np.asarray(img), np.asarray(win.convert("RGB"))
        px = int((a != b).any(axis=2).sum()) if a.shape == b.shape else -1
        if px:
            win.save(self.out / f"{name}.window.png")
        return img, {"source": "backbuffer", "window_px_differ": px}

    def stable_shot(self, name):
        """The checkpoint's frame. C3FT_SHOT_SETTLE=<ms> (batch b2): capture again every <ms> while the main thread is
        blocked in the dump, until two consecutive captures are identical (at most 40), and record how many it took and
        whether the first capture differed from the kept one (saved as <name>.first.png). Default: one capture."""
        settle = int(os.environ.get("C3FT_SHOT_SETTLE", "0") or 0)
        img = self.g.shot()
        if not settle:
            return img, None
        first, prev, n = img, img, 1
        t0 = time.time()
        while n < 40:
            time.sleep(settle / 1000)
            cur = self.g.shot()
            n += 1
            if cur.tobytes() == prev.tobytes():
                break
            prev = cur
        changed = first.tobytes() != prev.tobytes()
        if changed:
            first.save(self.out / f"{name}.first.png")
        return prev, {"captures": n, "first_differs": changed, "ms": round((time.time() - t0) * 1000)}

    def region_u32(self, blob, va):
        off = 0
        for rva, n, _ in self.regs:
            if rva == va:
                return struct.unpack_from("<I", blob, off)[0]
            off += n
        return None

    def hwnd_val(self):
        h = self.g.hwnd()
        return int(h) if h else 0

    def go(self, script_fn, extra_env=None):
        js = self.agent_source()
        env = {"SIMGOLF_SKIP_INTRO": "1", **self.env, **(extra_env or {})}
        if audio_mode() == "none":
            # the device-less path FreeLibrary's sound.dll; game.py pins it (keepsound.js) only when this PC has no audio
            # output, so pin it on every PC (b2)
            os.environ["SIMGOLF_KEEP_SOUND"] = "1"
        t_start = time.time()
        self.slots = None
        self.failure = None
        try:
            self._go(script_fn, env, js, t_start)
        except Exception as e:
            self.failure = f"{type(e).__name__}: {e}"
            raise
        finally:
            self.wall = time.time() - t_start
            (self.out / "poll_log.json").write_text(json.dumps({"agent": self.agent_log, "trace": self.trace}))
            summary = {"label": self.label, "mode": self.mode, "env": self.env, "audio": audio_mode(), "frame": frame_mode(),
                       "stack_seed": stack_seed(), "polls": self.polls,
                       "wall_s": round(self.wall, 1), "slots": self.slots, "checkpoints": self.checkpoints,
                       "errors": self.errors, "schedule": self.schedule, "failure": self.failure}
            if self.extra_regs:              # opt-in (c3fyl): absent by default, so default summaries are unchanged
                summary["extra_regions"] = self.extra_regs
            (self.out / "summary.json").write_text(json.dumps(summary, indent=1))
            if self.trace_spec or self.watch_spec:
                (self.out / "trace.json").write_text(json.dumps({"trace_regions": self.trace_spec, "rows": self.trace_rows,
                                                                 "watch": self.watch_spec, "watch_log": self.watch_rows,
                                                                 "watch_status": self.watch_status}))
        return self

    def _go(self, script_fn, env, js, t_start):
        with Game(env=env, extra_js=[str(js)], on_message=self._on_message) as g:
            self.g = g
            sc = g.scripts[-1]
            p, _ = self.wait_msg(("ready",), timeout=60)
            self.slots = p["slots"]
            ctx = Ctx(self)
            gen = script_fn(ctx)
            # first poll: every virtual key forced up, so the real keyboard / mouse buttons are never read
            first = True
            while True:
                p, _ = self.wait_msg(("poll",))
                self.polls += 1
                ctx.T = p["st"]["T"] - T0
                self.agent_log += p.get("log", [])
                if "tr" in p:
                    self.trace_rows.append([ctx.T, p["tr"]])
                if "wl" in p:
                    self.watch_rows += p["wl"]
                    self.watch_status = p.get("ws")
                self.trace.append([ctx.T, p["st"]["qpc"], p["st"]["forwarded"], round(time.time() - t_start, 3)]
                                  + ([p["bt"], p.get("reach")] if p.get("bt") else [])
                                  + ([p["st"]["msgs"], p["st"].get("dbg")] if p["st"].get("msgs") is not None else []))
                if self.debug_frames and g.hwnd() and self.polls % self.debug_frames == 0:
                    (self.out / "dbg").mkdir(exist_ok=True)
                    g.shot(self.out / "dbg" / f"{ctx.T:08d}.png")
                ctx.actions = []
                if first and os.environ.get("C3FT_NO_VKEYS") != "1":
                    for vk in range(1, 255):
                        ctx.act(0, k="vkey", vk=vk, st=1)
                    first = False
                ctx.dump_name = None
                try:
                    next(gen)
                    finished = False
                except StopIteration:
                    finished = True
                if ctx.dump_name:
                    self.take_dump(sc, ctx.dump_name)
                if finished:
                    self.post(sc, actions=ctx.actions, hwnd=self.hwnd_val())
                    break
                self.post(sc, actions=ctx.actions, next=POLL_MS, hwnd=self.hwnd_val())
                if not g.alive:
                    raise RuntimeError(f"{self.label}: game died: {g.detach_reason}")
            self.final_status = p["st"]


# ---------------------------------------------------------------- scenarios (scenario.py steps, frame-locked)
def sc_sandbox_basic(c: Ctx, season_s=40, every_s=5):
    """menu -> Sandbox Mode -> Monterey -> in game (re/tools/scenario.py to_sandbox), then `season_s` virtual seconds
    with a checkpoint every `every_s`."""
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")
    yield from c.click(130, 445)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "location.png"), name="location")
    yield from c.click(270, 32)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=(15, 535, 140, 595), threshold=30, name="hud")
    yield from c.sleep(2000)
    yield from c.checkpoint("sandbox_00")
    for i in range(1, season_s // every_s + 1):
        yield from c.sleep(every_s * 1000)
        yield from c.checkpoint(f"sandbox_{i * every_s:02d}")


def sc_course_season(c: Ctx, season_s=600, every_s=60):
    """scenario.py course_season: sandbox, build_first_hole, then a season with Esc every 2 s (popups hold game time).
    scenario.py runs 240 s at SIMGOLF_TIMEWARP=32 (~6 game years); here the clock is the virtual one, under which the date
    0x00834170 advances ~10 units per virtual second in the sandbox (1024 units = one month: 0x16 -> 0x1b6 in 41 s), so
    600 virtual seconds is ~6 game months."""
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.click(130, 445)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "location.png"), name="location")
    yield from c.click(270, 32)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=(15, 535, 140, 595), threshold=30, name="hud")
    yield from c.sleep(2000)
    yield from c.checkpoint("sandbox")
    # build_first_hole (scenario.py): golf icon, tee, green, drop the tool, open the hole with H
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.click(300, 527); yield from c.sleep(800)
    yield from c.click(300, 200); yield from c.sleep(1200)
    yield from c.click(365, 527); yield from c.sleep(800)
    yield from c.click(520, 330); yield from c.sleep(2000)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.key(0x48); yield from c.sleep(3000)
    yield from c.checkpoint("hole_open")
    t = 0
    while t < season_s * 1000:
        yield from c.key(0x1B)
        yield from c.sleep(2000 - 200)
        t += 2000
        if t % (every_s * 1000) == 0:
            yield from c.checkpoint(f"season_{t // 1000:03d}")


SCENARIOS = {"sandbox_basic": sc_sandbox_basic, "course_season": sc_course_season}


# ---------------------------------------------------------------- comparison
def load_blob(run_dir: pathlib.Path, name: str) -> bytes:
    return (run_dir / f"{name}.bin").read_bytes()


def compare_runs(a_dir: pathlib.Path, b_dir: pathlib.Path) -> dict:
    """Per checkpoint: the regions whose bytes differ (name, differing bytes) and the frame's differing pixels."""
    sa = json.loads((a_dir / "summary.json").read_text())
    sb = json.loads((b_dir / "summary.json").read_text())
    regs = regions()
    out = {}
    for name in sa["checkpoints"]:
        if name not in sb["checkpoints"]:
            out[name] = {"missing": True}
            continue
        ba, bb = load_blob(a_dir, name), load_blob(b_dir, name)
        diffs = []
        off = 0
        for va, n, rname in regs:
            xa, xb = ba[off:off + n], bb[off:off + n]
            if xa != xb:
                na = np.frombuffer(xa, np.uint8)
                nb = np.frombuffer(xb, np.uint8)
                idx = np.nonzero(na != nb)[0]
                diffs.append({"region": rname, "va": f"0x{va:08x}", "bytes": int(len(idx)), "first": f"0x{va + int(idx[0]):08x}"})
            off += n
        ia = np.asarray(Image.open(a_dir / f"{name}.png").convert("RGB"), np.int16)
        ib = np.asarray(Image.open(b_dir / f"{name}.png").convert("RGB"), np.int16)
        px = int((np.abs(ia - ib).sum(axis=2) > 0).sum())
        out[name] = {"T_a": sa["checkpoints"][name]["T"], "T_b": sb["checkpoints"][name]["T"],
                     "state_hash_a": hashlib.sha256(ba).hexdigest()[:16], "state_hash_b": hashlib.sha256(bb).hexdigest()[:16],
                     "regions_differ": diffs, "frame_px_differ": px,
                     "frame_sha_a": sa["checkpoints"][name]["frame_sha"][:16], "frame_sha_b": sb["checkpoints"][name]["frame_sha"][:16]}
    return out


def print_compare(cmp: dict):
    for name, r in cmp.items():
        if r.get("missing"):
            print(f"  {name}: missing in B")
            continue
        regs = ", ".join(f"{d['region']}({d['bytes']}@{d['first']})" for d in r["regions_differ"][:8])
        more = "" if len(r["regions_differ"]) <= 8 else f" +{len(r['regions_differ']) - 8} more"
        print(f"  {name}: T {r['T_a'] - T0}/{r['T_b'] - T0}  state {'SAME' if not r['regions_differ'] else 'DIFF ' + regs + more}"
              f"  frame {'SAME' if r['frame_px_differ'] == 0 else str(r['frame_px_differ']) + ' px'}")


# ---------------------------------------------------------------- commands
def arm_env_off():
    return {"SIMGOLF_HOOKS_OFF": "all"}


def run_dir(scenario, label):
    return OUT_ROOT / scenario / label


def scen_key(a) -> str:
    """The name a run's outputs are filed under: the scenario, plus `_spawn` with --spawn-reach, so a spawn-mode run has its
    own folder (log/c3/alt/c3ft/<scenario>_spawn/), its own schedule.json and its own CSV names
    (<addr>_<name>_<scenario>_spawn.scenario.csv) and never overwrites a default-mode result."""
    return a.scenario + ("_spawn" if getattr(a, "spawn_reach", False) else "")


def spawn_reach_report(dirs: list[pathlib.Path]) -> dict:
    """--spawn-reach: per run, the targets entered before the first poll (boot hits), and whether two runs agree on every
    target's state and counts (main thread and other threads, boot and total)."""
    out = {}
    for d in dirs:
        s = json.loads((d / "summary.json").read_text())
        last = list(s["checkpoints"])[-1]
        out[d.name] = {f"{x[0]}:{x[1]:x}": x[2:] for x in s["checkpoints"][last]["reach"]}
    names = list(out)
    rep = {"runs": names}
    for n in names:
        r = out[n]
        rep[n] = {"targets": len(r), "armed_at_spawn": sum(1 for v in r.values() if len(v) > 5 and v[5] == "spawn"),
                  "boot_main": sum(1 for v in r.values() if len(v) > 3 and (v[3] or 0) > 0),
                  "total_main": sum(1 for v in r.values() if (v[1] or 0) > 0)}
    if len(names) == 2:
        a, b = out[names[0]], out[names[1]]
        diff = {k: [a.get(k), b.get(k)] for k in sorted(set(a) | set(b)) if a.get(k) != b.get(k)}
        main_diff = {k: v for k, v in diff.items()
                     if not v[0] or not v[1] or v[0][1] != v[1][1] or v[0][3] != v[1][3] or v[0][0] != v[1][0]}
        rep["differ"] = diff
        rep["differ_main_thread"] = main_diff
    return rep


def cmd_determinism(a):
    key = scen_key(a)
    sched_path = OUT_ROOT / key / "schedule.json"
    write_ini_hooks([])
    fn = SCENARIOS[a.scenario]
    reach, spawn = [], getattr(a, "spawn_reach", False)
    if spawn:
        # spawn mode: both OFF runs arm a breakpoint on the original entry of every registered hook before the exe entry,
        # so the check covers the breakpoints' own effect on the timeline and the reach counts of the two runs are compared
        reg = registered_hooks(INSTANCE / "simgolf_shim.log")
        if not reg:
            raise SystemExit(f"no `re-hook` lines in {INSTANCE / 'simgolf_shim.log'}: boot the instance once first")
        reach = [{"module": m, "rva": r, "arm": "off"} for _n, m, r in reg]
        (OUT_ROOT / key).mkdir(parents=True, exist_ok=True)
    if a.reuse_schedule and sched_path.exists():
        sched = json.loads(sched_path.read_text())
        ra = Run("off_a", a.scenario, "replay", arm_env_off(), reach, [], schedule=sched, out_dir=run_dir(key, a.label_a),
                 spawn_reach=spawn).go(fn)
    else:
        ra = Run("off_a", a.scenario, "record", arm_env_off(), reach, [], out_dir=run_dir(key, a.label_a),
                 spawn_reach=spawn).go(fn)
        sched_path.write_text(json.dumps(ra.schedule, indent=1))
    rb = Run("off_b", a.scenario, "replay", arm_env_off(), reach, [], schedule=ra.schedule,
             out_dir=run_dir(key, a.label_b), spawn_reach=spawn).go(fn)
    cmp = compare_runs(ra.out, rb.out)
    (OUT_ROOT / key / f"determinism_{a.label_a}_vs_{a.label_b}.json").write_text(json.dumps(cmp, indent=1))
    print(f"determinism {key}: {a.label_a} vs {a.label_b}")
    print_compare(cmp)
    ext = compare_ext(ra.out, rb.out)        # None by default (no --call-log / --extra-regions)
    if ext is not None:
        (OUT_ROOT / key / f"determinism_{a.label_a}_vs_{a.label_b}_ext.json").write_text(json.dumps(ext, indent=1))
        print_compare_ext(ext)
    same = all(not r.get("missing") and not r["regions_differ"] and r["frame_px_differ"] == 0 for r in cmp.values())
    if spawn:
        rep = spawn_reach_report([ra.out, rb.out])
        (OUT_ROOT / key / f"reach_{a.label_a}_vs_{a.label_b}.json").write_text(json.dumps(rep, indent=1))
        for n in rep["runs"]:
            print(f"  reach {n}: {rep[n]}")
        print(f"  reach differs for {len(rep['differ'])} targets, on the main thread for {len(rep['differ_main_thread'])}")
    print("DETERMINISTIC" if same else "NOT DETERMINISTIC")
    return 0 if same else 1


def resolve_on_set(targets: list[str], reg: list[tuple[str, str, int]]) -> list[tuple[str, str, int]]:
    if targets == ["all"]:
        return list(reg)
    out = []
    for t in targets:
        mod, a = parse_target(t)
        hit = [r for r in reg if r[1].lower() == mod.lower() and r[2] == a]
        if not hit:
            raise SystemExit(f"{t}: no SG_HOOK registered at {mod}+{a:08x} (shim log)")
        out += hit
    return out


def csv_stem(name: str) -> str:
    return name.replace("::", "_").replace("<", "(").replace(">", ")").replace("*", "P").replace(" ", "_")


def scenario_csv_path(module: str, rva: int, name: str, out_dir: pathlib.Path, scenario: str) -> pathlib.Path:
    """<addr>_[<stem>_]<name>_<scenario>.scenario.csv: the scenario is part of the name so two scenarios' results for one
    function never overwrite each other (re_classify's glob <addr>_*.scenario.csv still finds them)."""
    tag = "" if module.lower() == "golf_clean.exe" else module.rsplit(".", 1)[0] + "_"
    return out_dir / f"{rva:08x}_{tag}{csv_stem(name)}_{scenario}.scenario.csv"


def cmd_ab(a):
    key = scen_key(a)
    spawn = getattr(a, "spawn_reach", False)
    sched_path = OUT_ROOT / key / "schedule.json"
    if not sched_path.exists():
        raise SystemExit(f"no recorded schedule {sched_path}: run `determinism --scenario {a.scenario}"
                         f"{' --spawn-reach' if spawn else ''}` first")
    sched = json.loads(sched_path.read_text())
    fn = SCENARIOS[a.scenario]
    log_path = INSTANCE / "simgolf_shim.log"
    # the registered hook list: from the shim log of an OFF boot (the determinism runs leave one)
    reg = registered_hooks(log_path)
    if not reg:
        raise SystemExit(f"no `re-hook` lines in {log_path}: boot the instance once (determinism) first")
    on = resolve_on_set(a.on, reg)
    on_keys = {(m.lower(), r) for _n, m, r in on}
    off_list = [(m, r) for _n, m, r in reg if (m.lower(), r) not in on_keys]
    # ini keys carry no module: an RVA shared by an ON hook and an OFF hook of another module stays out of the ini and
    # the OFF one goes into SIMGOLF_HOOKS_OFF as module:rva
    on_rvas = {r for _m, r in on_keys}
    ini_off = [(m, r) for m, r in off_list if r not in on_rvas]
    env_off = [f"{m}:{r:x}" for m, r in off_list if r in on_rvas]
    reach_on = [{"module": m, "rva": r, "arm": "on"} for _n, m, r in on]
    reach_off = [{"module": m, "rva": r, "arm": "off"} for _n, m, r in on]
    witness = [{"module": m, "rva": r} for _n, m, r in on]
    tag = a.label or ("all" if a.on == ["all"] else f"set{len(on)}")
    if a.negctl:
        tag += "_negctl"
    # OFF arm
    if not a.skip_off:
        write_ini_hooks([])
        off_run = Run("off", a.scenario, "replay", arm_env_off(), reach_off, witness, schedule=sched,
                      out_dir=run_dir(key, f"ab_{tag}_off"), spawn_reach=spawn).go(fn)
        off_dir = off_run.out
    else:
        off_dir = run_dir(key, a.skip_off)
    # ON arm
    dll_backup = None
    try:
        if a.negctl:
            dll_backup = INSTANCE / "winmm.dll.c3ft_main"
            if not dll_backup.exists():
                shutil.copy2(INSTANCE / "winmm.dll", dll_backup)
            shutil.copy2(a.negctl, INSTANCE / "winmm.dll")
        write_ini_hooks(ini_off)
        env = {"SIMGOLF_HOOKS_OFF": ",".join(env_off)} if env_off else {}
        env.update(dict(kv.split("=", 1) for kv in (a.env or [])))
        on_run = Run("on", a.scenario, "replay", env, reach_on, witness, schedule=sched,
                     out_dir=run_dir(key, f"ab_{tag}_on"), spawn_reach=spawn)
        try:
            on_run.go(fn)
        except Exception:  # noqa: BLE001 - a hang or a crash of the ON arm is a result (RED), not a tool error
            print(f"  ON arm stopped: {on_run.failure}", flush=True)
    finally:
        write_ini_hooks([])
        if dll_backup is not None:
            # the killed game can hold winmm.dll for a few seconds: retry, and never leave the control DLL in place
            for i in range(60):
                try:
                    shutil.copy2(dll_backup, INSTANCE / "winmm.dll")
                    break
                except PermissionError:
                    time.sleep(1)
            else:
                raise SystemExit(f"could not restore {INSTANCE / 'winmm.dll'} from {dll_backup}: restore it by hand")
    return write_results(a, on, tag, off_dir, on_run.out)


def arm_conditions_differ(off_sum: dict, on_sum: dict) -> str:
    """'' when the two arms booted on the same audio path, else what differs (batch b2). An arm ran the device-less path
    when it had the audio stand-in (summary.json "audio": "none", since b2) or when its sound timer thread never called
    timeGetTime (first checkpoint's `forwarded` == 0: no device, as on LIVING-PC with the TV off); otherwise the sound
    engine ran. With a device the boot makes one more main-thread clock call before seedFromTime 0x00467110, so
    seed_time 0x0051b380 and the RNG differ from the first checkpoint on, whatever the hooks do."""
    def path(sm):
        cps = sm.get("checkpoints") or {}
        fwd = next(iter(cps.values()))["status"]["forwarded"] if cps else None
        if sm.get("audio") == "none" or fwd == 0:
            return "device-less"
        return "sound engine live" if fwd else None
    pa, pb = path(off_sum), path(on_sum)
    if pa and pb and pa != pb:
        return f"audio path {pa} / {pb}"
    return ""


def write_results(a, on, tag, off_dir: pathlib.Path, on_dir: pathlib.Path) -> int:
    """Compare two finished arms (their run folders) and write one scenario CSV per function of the ON set."""
    key = scen_key(a)
    spawn = getattr(a, "spawn_reach", False)
    on_summary = json.loads((on_dir / "summary.json").read_text())
    failure = on_summary.get("failure")
    cmp = compare_runs(off_dir, on_dir)
    (OUT_ROOT / key / f"ab_{tag}.json").write_text(json.dumps(cmp, indent=1))
    print(f"A/B {key} [{tag}]: OFF vs ON ({len(on)} hooks ON)")
    print_compare(cmp)
    ext = compare_ext(off_dir, on_dir)       # opt-in extended oracle (c3fyl); None by default
    if ext is not None:
        (OUT_ROOT / key / f"ab_{tag}_ext.json").write_text(json.dumps(ext, indent=1))
        print_compare_ext(ext)
    state_same = all(not r.get("missing") and not r["regions_differ"] for r in cmp.values())
    frames_same = all(not r.get("missing") and r["frame_px_differ"] == 0 for r in cmp.values())
    # per-function CSVs
    off_sum = json.loads((off_dir / "summary.json").read_text())
    stale = arm_conditions_differ(off_sum, on_summary)
    if stale:
        print(f"  INCOMPARABLE ARMS: {stale}: a difference here compares two boot timelines, not the hook set "
              f"(re/analysis/data/scenario_baseline.md); every verdict is INCOMPARABLE", flush=True)
    on_sum = on_summary["checkpoints"]
    on_last = list(on_sum)[-1] if on_sum else None        # the ON arm's last checkpoint (none if it stopped before one)
    off_last = list(off_sum["checkpoints"])[-1]
    # reach entries: [module, rva, state, main-thread hits, other-thread hits] (hits = re-arm intervals with >= 1 entry)
    on_reach = {(x[0].lower(), x[1]): (x[2], x[3], x[4]) for x in on_sum[on_last]["reach"]} if on_last else {}
    off_reach = {(x[0].lower(), x[1]): (x[2], x[3], x[4]) for x in off_sum["checkpoints"][off_last]["reach"]}
    # witness entries: [module, rva, byte before any agent attach, live byte]. ON arm: the live byte (the agent attaches
    # its reach counter to the detour, never to the original entry). OFF arm: the byte before the agent's own counter.
    on_wit = {(w[0].lower(), w[1]): w[3] for w in on_sum[on_last]["witness"]} if on_last else {}
    off_wit = {(w[0].lower(), w[1]): w[2] for w in off_sum["checkpoints"][off_last]["witness"]}
    # --spawn-reach: [.., boot main hits, boot other hits, armed at 'spawn' | 'poll'] (hits before the first poll)
    on_boot = {(x[0].lower(), x[1]): x[5:8] for x in on_sum[on_last]["reach"]} if (spawn and on_last) else {}
    off_boot = {(x[0].lower(), x[1]): x[5:8] for x in off_sum["checkpoints"][off_last]["reach"]} if spawn else {}
    out_dir = OUT_ROOT / key / f"csv_{tag}"
    out_dir.mkdir(parents=True, exist_ok=True)
    rows_by_key = hooks_csv_rows()
    written = []
    for name, mod, rva in on:
        k = (mod.lower(), rva)
        wb = on_wit.get(k, -1)
        st_on, n_on, o_on = on_reach.get(k, ("not measured", None, None))
        st_off, n_off, o_off = off_reach.get(k, ("not measured", None, None))
        witness_ok = wb == 0xE9
        off_live = off_wit.get(k, -1) == 0xE9      # the hook was live in the OFF arm: no A/B at all
        reached = (n_on or 0) > 0                  # entered on the MAIN thread (the deterministic, compared timeline)
        if stale:
            verdict = "INCOMPARABLE"
        elif failure or not witness_ok or off_live:
            verdict = "RED"
        elif n_on is None or st_on != "armed":
            verdict = "UNMEASURED"
        elif not reached:
            # entered only by other threads (sound.dll's real-time timer thread): their effects are neither on the virtual
            # timeline nor in the compared state, so a SAME comparison says nothing about this function
            verdict = "OFFTHREAD" if (o_on or 0) > 0 else "UNREACHED"
        elif not (state_same and frames_same):
            verdict = "RED"
        elif mod.lower() in UNOBSERVED_MODULES:
            # sound.dll: its output is audio, which no checkpoint compares, and its main-thread entries follow the real-time
            # sound timer thread (3 sound.dll functions differed by one interval of main-thread reach between two SAME arms)
            verdict = "UNOBSERVED"
        else:
            verdict = "GREEN"
        hrow = rows_by_key.get(k, {})
        fname = hrow.get("name") or name
        path = scenario_csv_path(mod, rva, fname, out_dir, key)
        with path.open("w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["checkpoint", "off", "on", "match"])
            for cp, r in cmp.items():
                if r.get("missing"):
                    w.writerow([cp, "present", "missing", False])
                    continue
                same = not r["regions_differ"] and r["frame_px_differ"] == 0
                diff = ";".join(f"{d['region']}:{d['bytes']}" for d in r["regions_differ"])
                w.writerow([f"{cp} T={r['T_a'] - T0}", f"state={r['state_hash_a']} frame={r['frame_sha_a']}",
                            f"state={r['state_hash_b']} frame={r['frame_sha_b']}" + (f" differ={diff}" if diff else "")
                            + (f" px={r['frame_px_differ']}" if r["frame_px_differ"] else ""), same])
            w.writerow(["install_witness", f"0x{wb & 0xff:02x}" if wb >= 0 else str(wb), "0xe9", witness_ok])
            w.writerow(["meta", f"scenario={key} hooks_on={len(on)} tag={tag}",
                        f"reach_on={n_on} other_threads={o_on} ({st_on}) reach_off={n_off} other_threads={o_off} ({st_off}) "
                        f"off_byte=0x{off_wit.get(k, -1) & 0xff:02x}",
                        datetime.datetime.now().isoformat(timespec="seconds")])
            w.writerow(["meta", f"off={off_dir.relative_to(ROOT).as_posix()}", f"on={on_dir.relative_to(ROOT).as_posix()}",
                        f"clock=virtual step {STEP_MS} ms poll {POLL_MS} ms"])
            if spawn:
                bo, bf = on_boot.get(k, [None] * 3), off_boot.get(k, [None] * 3)
                w.writerow(["meta", "reach_armed=spawn (before the exe entry; DLLs loaded later at the polls)",
                            f"boot_on={bo[0]} boot_other_on={bo[1]} armed_on={bo[2]} "
                            f"boot_off={bf[0]} boot_other_off={bf[1]} armed_off={bf[2]}", ""])
            if failure:
                w.writerow(["meta", "on_arm_failure", failure[:600].replace("\n", " "), ""])
            if stale:
                w.writerow(["meta", "incomparable_arms", stale, ""])
            if ext is not None:
                # opt-in (c3fyl): reported beside the verdict, which stays the checkpoint dump's
                w.writerow(["meta", "extended_oracle", "INCOMPARABLE" if ext.get("incomparable") else
                            ("SAME" if ext["same"] else "DIFF"), "call log / extra regions"])
            w.writerow(["VERDICT", verdict, len(cmp), ""])
        written.append((verdict, path))
    from collections import Counter
    if len(written) <= 20:
        for v, p in written:
            print(f"  {v:10s} {p.relative_to(ROOT).as_posix()}")
    print(f"  verdicts: {dict(Counter(v for v, _ in written))} -> {out_dir.relative_to(ROOT).as_posix()}")
    (OUT_ROOT / key / f"verdicts_{tag}.tsv").write_text("".join(f"{v}\t{p.name}\n" for v, p in written))
    if a.write_evidence and not a.negctl:
        # evidence: GREEN CSVs only. The C4 gate (scripts/re_classify.py) reads every <addr>_[<stem>_]*.scenario.csv of the
        # row's own module and wants all of them GREEN with a witness, so a DLL CSV is never read for another module's
        # row at the same RVA (a hold-back list here covered that until the gate was fixed, 2026-10-15)
        copied = 0
        for verdict, path in written:
            if verdict == "GREEN":
                shutil.copy2(path, DIFF_DIR / path.name)
                copied += 1
        print(f"  evidence: {copied} GREEN CSVs copied to log/diff")
    print(f"state {'SAME' if state_same else 'DIFF'}, frames {'SAME' if frames_same else 'DIFF'}")
    return 0 if (state_same and frames_same) else 1


def cmd_csv(a):
    """Re-score two finished arms without booting (e.g. after a verdict rule changed)."""
    reg = registered_hooks(INSTANCE / "simgolf_shim.log")
    on = resolve_on_set(a.on, reg)
    a.negctl = None
    return write_results(a, on, a.label, run_dir(scen_key(a), a.off_dir), run_dir(scen_key(a), a.on_dir))


def cmd_tracecmp(a):
    """Compare two runs' trace.json (opt-in --trace / --watch): the first poll whose traced bytes differ, with the region,
    offset and both values, then each arm's watch log around that time."""
    da, db = run_dir(scen_key(a), a.a), run_dir(scen_key(a), a.b)
    ta, tb = (json.loads((d / "trace.json").read_text()) for d in (da, db))
    spans = ta["trace_regions"]
    if spans != tb["trace_regions"]:
        raise SystemExit("the two runs traced different regions")
    rb = {t: h for t, h in tb["rows"]}
    first = None
    for t, ha in ta["rows"]:
        hb = rb.get(t)
        if hb is None or hb == ha:
            continue
        ba, bb = bytes.fromhex(ha), bytes.fromhex(hb)
        off = 0
        diffs = []
        for va, n in spans:
            for i in range(n):
                if ba[off + i] != bb[off + i]:
                    diffs.append(f"0x{va + i:08x} (region 0x{va:08x}+0x{i:x}): {ba[off + i]:02x} / {bb[off + i]:02x}")
            off += n
        first = t
        print(f"first differing poll T={t} ({a.a} / {a.b}):")
        for x in diffs[:40]:
            print("  " + x)
        break
    if first is None:
        common = len(set(t for t, _ in ta["rows"]) & set(rb))
        print(f"traces identical over {common} common polls ({len(ta['rows'])} / {len(tb['rows'])} polls)")
    lo = (first if first is not None else 0) - a.window
    hi = (first if first is not None else 1 << 40) + a.window
    for name, t in ((a.a, ta), (a.b, tb)):
        rows = [r for r in t.get("watch_log") or [] if lo <= r[0] - T0 <= hi]
        print(f"watch log {name} ({len(t.get('watch_log') or [])} entries, status {t.get('watch_status')}), "
              f"T in [{lo}, {hi}]:")
        for r in rows[:a.max_rows]:
            print(f"  T={r[0] - T0} " + " ".join(str(x) for x in r[1:]))
    return 0 if first is None else 1


SPAWN_HELP = ("arm the reach breakpoints when the agent loads, before the spawned process's main thread reaches the exe "
              "entry, so static initializers (__cinit 0x004a50db) and boot code count as reached; outputs go to "
              "<scenario>_spawn (folder, schedule, CSV names). Default: armed at the first poll")


TRACE_HELP = ("opt-in: `va:n,...` regions read at EVERY poll (100 virtual ms) into <run>/trace.json; compare two runs with "
              "`tracecmp`. Same as env C3FT_TRACE. Default: none (agent and outputs unchanged)")
WATCH_HELP = ("opt-in: `va:size,...` (<= 4, size 1/2/4) main-thread hardware write watchpoints, armed at the first poll; "
              "each write that changes the value is logged with the writing pc and caller chain into <run>/trace.json. "
              "Same as env C3FT_WATCH")

CLOCK_HELP = ("opt-in: record the first N main-thread timeGetTime / QPC calls with their caller (module+rva) into "
              "<run>/clock.json, grouped by the checkpoint whose dump read them (batch b2). Same as env C3FT_CLOCK_LOG")
CALL_HELP = ("opt-in extended oracle (batch c3fyl): `va:n[:ra/ra][:sK][:r][:xK],...` log every main-thread entry of these exe "
             "functions (return address, n stack arguments, the C string at argument K, the return value) into <run>/calls.json and compare "
             "them per checkpoint between arms. Same as env C3FT_CALL_LOG")
EXTRA_HELP = ("opt-in extended oracle (batch c3fyl): `va:n,...` regions dumped at every checkpoint into <name>.extra.bin "
              "(apart from the default blob) and compared between arms. Same as env C3FT_EXTRA_REGIONS")


def cmd_extcmp(a):
    """Compare two finished runs (absolute or repo-relative folders), default dump and extended oracle."""
    da, db = (pathlib.Path(x) if pathlib.Path(x).is_absolute() else ROOT / x for x in (a.a, a.b))
    cmp = compare_runs(da, db)
    print_compare(cmp)
    ext = compare_ext(da, db)
    print_compare_ext(ext)
    if a.out:
        pathlib.Path(a.out).write_text(json.dumps({"default": cmp, "ext": ext}, indent=1))
    same = all(not r.get("missing") and not r["regions_differ"] and r["frame_px_differ"] == 0 for r in cmp.values())
    return 0 if same and (ext is None or ext.get("same")) else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--instance", default=str(INSTANCE_DEFAULT))
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("determinism")
    p.add_argument("--scenario", default="sandbox_basic", choices=sorted(SCENARIOS))
    p.add_argument("--label-a", default="det_a")
    p.add_argument("--label-b", default="det_b")
    p.add_argument("--reuse-schedule", action="store_true", help="replay the saved schedule in both runs")
    p.add_argument("--spawn-reach", action="store_true", help=SPAWN_HELP + "; both OFF runs arm every registered hook's "
                   "original entry and their reach counts are compared")
    p.add_argument("--trace", help=TRACE_HELP)
    p.add_argument("--watch", help=WATCH_HELP)
    p.add_argument("--clock-log", help=CLOCK_HELP)
    p.add_argument("--call-log", help=CALL_HELP)
    p.add_argument("--extra-regions", help=EXTRA_HELP)
    p.set_defaults(fn=cmd_determinism)
    p = sp.add_parser("ab")
    p.add_argument("--scenario", default="sandbox_basic", choices=sorted(SCENARIOS))
    p.add_argument("--on", nargs="+", required=True, help="module:0xaddr targets (exe: bare VA) or `all`")
    p.add_argument("--negctl", help="control winmm.dll for the ON arm (results never go to log/diff)")
    p.add_argument("--label")
    p.add_argument("--env", nargs="*", help="extra env for the ON arm, KEY=VALUE")
    p.add_argument("--skip-off", help="reuse an earlier OFF run directory name instead of running the OFF arm")
    p.add_argument("--write-evidence", action="store_true", help="write the per-function CSVs into log/diff")
    p.add_argument("--spawn-reach", action="store_true", help=SPAWN_HELP)
    p.add_argument("--trace", help=TRACE_HELP)
    p.add_argument("--watch", help=WATCH_HELP)
    p.add_argument("--clock-log", help=CLOCK_HELP)
    p.add_argument("--call-log", help=CALL_HELP)
    p.add_argument("--extra-regions", help=EXTRA_HELP)
    p.set_defaults(fn=cmd_ab)
    p = sp.add_parser("csv", help="re-score two finished arms (run folder names under log/c3/alt/c3ft/<scenario>/)")
    p.add_argument("--scenario", default="sandbox_basic", choices=sorted(SCENARIOS))
    p.add_argument("--on", nargs="+", required=True)
    p.add_argument("--off-dir", required=True)
    p.add_argument("--on-dir", required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--write-evidence", action="store_true")
    p.add_argument("--spawn-reach", action="store_true", help="the arms are spawn-mode runs (folder <scenario>_spawn)")
    p.set_defaults(fn=cmd_csv)
    p = sp.add_parser("tracecmp", help="compare two runs' trace.json (made with --trace / --watch)")
    p.add_argument("--scenario", default="sandbox_basic", choices=sorted(SCENARIOS))
    p.add_argument("--a", required=True, help="run folder name under log/c3/alt/c3ft/<scenario>/")
    p.add_argument("--b", required=True)
    p.add_argument("--window", type=int, default=2000, help="virtual ms of watch log printed around the first difference")
    p.add_argument("--max-rows", type=int, default=60)
    p.add_argument("--spawn-reach", action="store_true")
    p.set_defaults(fn=cmd_tracecmp)
    p = sp.add_parser("extcmp", help="compare two finished run folders, default dump and extended oracle (c3fyl)")
    p.add_argument("--a", required=True)
    p.add_argument("--b", required=True)
    p.add_argument("--out", help="write both comparisons as JSON")
    p.set_defaults(fn=cmd_extcmp)
    a = ap.parse_args()
    for flag, var in (("trace", "C3FT_TRACE"), ("watch", "C3FT_WATCH"), ("clock_log", "C3FT_CLOCK_LOG"),
                      ("call_log", "C3FT_CALL_LOG"), ("extra_regions", "C3FT_EXTRA_REGIONS")):
        if getattr(a, flag, None):
            os.environ[var] = getattr(a, flag)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
