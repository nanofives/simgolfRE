"""The exe's sound layer (step S4.10): its path-1 keys replayed without the game, and the AudioSink command stream of the
segments. Write-up: re/analysis/data/audio_stream.md.

ExeKeyRun is S4.1's KeyRun (port/test/sound_emu.py) with the user's golf_clean.exe as the image (base 0x00400000, no
relocations), so the fixture VM rebuilds an exe key's fixture and the ORIGINAL exe function runs in Unicorn over it.
"""
from __future__ import annotations

import csv
import functools
import pathlib
import sys


def _root() -> pathlib.Path:
    for p in pathlib.Path(__file__).resolve().parents:
        if (p / "re" / "tools").is_dir() and (p / "port" / "test" / "sound_emu.py").exists():
            return p
    raise RuntimeError("SimGolf root not found")


ROOT = _root()
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "port" / "test"))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import sound_emu as E  # noqa: E402

EXE = ROOT / "original" / "golf_clean.exe"
EXE_BASE = 0x00400000
SCOPE = ROOT / "re" / "port" / "scope.tsv"
HOOKS_CSV = ROOT / "hooks.csv"


# ---- the exe's backend-audio rows and their keys ----------------------------------------------------------------------
@functools.lru_cache(maxsize=1)
def audio_rows() -> list[dict]:
    """re/port/scope.tsv rows with port_module backend-audio, with hooks.csv's level / file."""
    lv = {}
    with HOOKS_CSV.open(newline="") as f:
        for r in csv.DictReader(f):
            if r["module"] in ("", "golf_clean.exe"):
                lv[int(r["addr"], 16)] = r
    out = []
    with SCOPE.open(newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["port_module"] != "backend-audio":
                continue
            a = int(r["addr"], 16)
            h = lv.get(a, {})
            out.append({"addr": a, "name": r["name"], "size": int(r["size"]), "level": h.get("confidence", r["hooks_level"]),
                        "file": h.get("file", "")})
    return out


# backend-audio rows another module ports and judges (their owners' gates: port/test/test_tick.py playHabitatSound,
# test_bridge.py playSoundAt's path-1 replay, test_video.py's emulator oracle); this module calls them by name
OTHER_OWNERS = {"playHabitatSound": "tick", "playSoundAt": "bridge", "playAudioFile": "video"}


def exe_keys(levels=("C3", "C4"), others=False) -> list[str]:
    """Every registry key of the exe's backend-audio rows at the given levels (others: also the rows OTHER_OWNERS ports)."""
    hooks, _ = E.registry()
    addrs = {r["addr"] for r in audio_rows() if r["level"] in levels and (others or r["name"] not in OTHER_OWNERS)}
    return sorted((k for k, v in hooks.items()
                   if (v.get("module") or "golf_clean.exe").lower() == "golf_clean.exe" and v["addr"] in addrs),
                  key=lambda k: (hooks[k]["addr"], k))


# ---- the original exe in the fixture VM -------------------------------------------------------------------------------
@functools.lru_cache(maxsize=1)
def exe_image() -> E.Image:
    return E.load_image(EXE_BASE, path=EXE)


# Per layout, NativeCallbacks made before the fixture runs: the fixture's own callbacks then sit at other trap addresses
# in each layout (the VM hands them out from one counter), so a region holding a callback pointer reads as a run address
# (S4.1's census otherwise compares it with the live CSV, where Frida's callbacks had other addresses: U-1497).
TRAP_SHIFT = {"A": 0, "B": 7, "C": 13}
# fixvm.js calls a fixture without its object as `this` (diff_hook.js calls DIFF_FIXTURES[name]() as a method); c3cc's
# closeHookC fixtures call this._c3ccCloseC (U-1497): every fixture function is bound to DIFF_FIXTURES first.
BIND_FIXTURES = ("Object.keys(DIFF_FIXTURES).forEach(function (k) { const f = DIFF_FIXTURES[k];"
                 " if (typeof f === 'function') DIFF_FIXTURES[k] = f.bind(DIFF_FIXTURES); }); 0")
# The emulator's TEB holds StackBase (+4) and StackLimit (+8) but not DeallocationStack (+0xe0c), which framemask.js reads
# for the stack's low end (unlinkSound's fixture): the bottom of the emulator's stack, as the live TEB holds the reservation's
TEB_DEALLOC = 0xE0C


# ---- custom keys: the rows with no C3 key (C2 forwarders and helpers), over log/roadmap/S4/10_fixtures.js ---------------
CUSTOM_FIXTURES = ROOT / "log" / "roadmap" / "S4" / "10_fixtures.js"
# (hooks.csv name, VA, stack argument count after `this`, ret)
_C2_ROWS = [
    ("Snd::isPlaying", 0x484440, 0), ("Snd::linkIntoList", 0x484450, 0), ("Snd::getNext", 0x4844b0, 0),
    ("Snd::getPrev", 0x4844c0, 0), ("Snd::getMode", 0x4844d0, 0), ("Snd::fwdVoice1c", 0x484590, 0),
    ("Snd::fwdVoice18", 0x4845b0, 1), ("Snd::fwdVoice38", 0x484600, 0), ("Snd::setVolumeBase", 0x484620, 1),
    ("Snd::setLoop", 0x484690, 1), ("Snd::setField38Voice54", 0x484700, 1), ("Snd::applyField38Voice28", 0x484730, 0),
    ("Snd::applyField38Voice30", 0x484770, 0), ("Snd::fwdVoice58", 0x4847d0, 0), ("Snd::fwdVoice74", 0x4847e0, 0),
    ("Snd::fwdVoice60", 0x484870, 0), ("Snd::fwdVoice98", 0x484920, 1), ("Snd::fwdVoice94", 0x4849e0, 1),
    ("Snd::fwdVoiceCC", 0x484b00, 0), ("Snd::getLengthMs", 0x484b20, 0), ("Snd::fwdVoiceF0", 0x484bf0, 2),
    ("Snd::setFlags", 0x484f80, 1), ("Snd::fwdVoiceB8", 0x485040, 1), ("Snd::fwdVoiceBC", 0x485060, 0),
    ("Snd::fwdVoice3c", 0x485080, 0), ("Snd::fwdDev48", 0x485090, 0), ("Snd::setField60", 0x4850f0, 1),
    ("Snd::fwdDev54", 0x4851b0, 3), ("Snd::fwdDev55", 0x4851e0, 1), ("Snd::fwdDev56", 0x485200, 1),
    ("Snd::fwdDev57", 0x485220, 1), ("SndBase::setField38", 0x485290, 1), ("VoiceRx::fwdDev37", 0x485390, 0),
    ("VoiceRx::start", 0x485450, 3), ("VoiceRx::callDev34", 0x485500, 0), ("VoiceRx::callDev8", 0x485520, 0),
    ("VoiceRx::fwdDev5", 0x485530, 0), ("VoiceRx::fwdDev35", 0x485550, 1), ("VoiceRx::fwdDev36", 0x485570, 0),
    ("Snd::fwdDev49", 0x4850b0, 3),
]
# the null-device paths of fwdVoice34 / D0 / D4 leave the caller's eax and fwdDev58's clears only al (U-1496): those rows
# are judged on the device path only (records with a device) and their keys compare state, not eax, on the null path
_C2_KEEP_EAX = [("Snd::fwdVoice34", 0x484790, 3), ("Snd::fwdVoiceD0", 0x484a00, 1), ("Snd::fwdVoiceD4", 0x484a20, 1),
                ("Snd::fwdDev58", 0x485240, 0)]
_ARGV = [0, 1, 0x7f, 0x80, 0xffffffff, 0x12345, 0x3f, 0xffffffc0, 2, 0x4b0, 0xfffffb50, 0x400]
_STATE = [("$sblk", 0, 12 * 0x80), ("$rec", 0, 0x40), ("$scr", 0, 0x40), ("$gl", 0, 0x64), ("$md", 0, 0x1e4)]


def _c2_vec(i, n, name):
    vals = [_ARGV[(i + 3 * k) % len(_ARGV)] for k in range(n)]
    if name == "Snd::fwdDev49":
        vals = ["$scr", 0x10 + i, i % 2]
    return tuple([f"$s{i}"] + vals)


CUSTOM: dict = {}
# the list rows run on records linked from g_soundHead (prev +0x48 / next +0x4c), where a swapped link shows
_LINKED = {"Snd::getNext", "Snd::getPrev", "Snd::linkIntoList"}
for _name, _va, _n in _C2_ROWS:
    for _fx in ("ready", "noready") if _name == "VoiceRx::start" else ("ready",):
        CUSTOM[f"s410:{_name}" + ("" if _fx == "ready" else "@noready")] = dict(
            module="golf_clean.exe", addr=_va, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * _n,
            fixture=f"s410_snd_{_fx}" if _name not in _LINKED else "s410_nodelist", state=list(_STATE),
            vectors=[_c2_vec(i, _n, _name) for i in range(12)])
for _name, _va, _n in _C2_KEEP_EAX:
    CUSTOM[f"s410:{_name}"] = dict(
        module="golf_clean.exe", addr=_va, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * _n,
        fixture="s410_snd_ready", state=list(_STATE), vectors=[_c2_vec(i, _n, _name) for i in range(12) if i % 4 != 3])
for _name, _va, _off in (("SndGroup_isDisabled", 0x487770, 0), ("SndGroup_isMuted", 0x4877a0, 0)):
    CUSTOM[f"s410:{_name}"] = dict(module="golf_clean.exe", addr=_va, abi="thiscall", ret="uint32",
                                   args=["pointer", "uint32"], fixture="s410_snd_ready", state=list(_STATE),
                                   vectors=[("$md", g) for g in list(range(17)) + [0x20, 0xffffffff, 0x1234]])
# the `mov eax, 0xb; ret N` stubs of the Snd tables (stack arguments after `this`)
for _name, _va, _n in (("returnB_484540", 0x484540, 1), ("returnB_485120", 0x485120, 0),
                       ("returnBPop12_485130", 0x485130, 3), ("returnBPop4_485380", 0x485380, 1)):
    CUSTOM[f"s410:{_name}"] = dict(
        module="golf_clean.exe", addr=_va, abi="thiscall", ret="uint32", args=["pointer"] + ["uint32"] * _n,
        fixture="s410_snd_ready", state=list(_STATE), vectors=[_c2_vec(i, _n, _name) for i in range(12)])
CUSTOM["s410:NodeList::playChildSuspendingSounds"] = dict(
    module="golf_clean.exe", addr=0x487500, abi="thiscall", ret="uint32", args=["pointer", "uint32"],
    fixture="s410_nodelist", state=list(_STATE) + [("$nl", 0, 0x20), ("$nl0", 0, 0x20)],
    vectors=[("$nl", v) for v in _ARGV[:10]] + [("$nl0", 5), ("$nl0", 0)])
# the exit-time destructors (cdecl, no arguments) over the exe's own g_sounds and device lists
_EXIT_STATE = [(0x80d840, 0, 0x7e90), (0x83ad58, 0, 0x20), (0x83ad80, 0, 0x1e4), (0x83af98, 0, 0x20), (0x83af64, 0, 0x64)]
for _name, _va, _fxs in (("gSoundsDtor_448190", 0x448190, ("plain", "live")), ("gMusicDeviceDtor_483e00", 0x483e00, ("plain",)),
                         ("gSfxDeviceDtor_483e40", 0x483e40, ("plain",)), ("gVoiceDeviceDtor_483e80", 0x483e80, ("plain",))):
    for _fx in _fxs:
        CUSTOM[f"s410:{_name}" + ("" if _fx == "plain" else "@live")] = dict(
            module="golf_clean.exe", addr=_va, abi="cdecl", ret="void" if _name.startswith("gSounds") else "uint32",
            args=[], fixture=f"s410_exit_{_fx}", state=list(_EXIT_STATE) + ([("$rec", 0, 0x40)] if _fx == "live" else []),
            vectors=[()])


def custom_names() -> dict:
    return {k: (k.split(":", 1)[1].split("@")[0]) for k in CUSTOM}


def _install_custom():
    hooks, batch = E.registry()
    for k, s in CUSTOM.items():
        hooks.setdefault(k, dict(s))
        batch.setdefault(k, "s410")


class ExeKeyRun(E.KeyRun):
    """KeyRun over golf_clean.exe: no DllMain (the exe's CRT start-up is not run; a fixture writes what its key needs)."""

    def __init__(self, vm, key, layout="A", **kw):
        if key in CUSTOM:
            _install_custom()
        kw.setdefault("init", False)
        super().__init__(vm, key, layout, **kw)
        self.img = exe_image()
        self.spec["addr"] -= EXE_BASE        # KeyRun calls img.base + addr; exe registry entries hold the VA

    def setup(self):
        m = super().setup()
        import struct
        m.write(E.TEB + TEB_DEALLOC, struct.pack("<I", E.STACK_BASE))
        lay = self.layout if isinstance(self.layout, str) else "A"
        k = TRAP_SHIFT.get(lay, 0)
        src = BIND_FIXTURES
        if self.key in CUSTOM:
            src = CUSTOM_FIXTURES.read_text(encoding="utf-8") + "\n;" + src
        if k:
            src = (f"for (let i = 0; i < {k}; i++) __diffKeepAlive.push(new NativeCallback(function () {{}}, 'void', []));"
                   + src)
        r = self.vm.request({"op": "eval", "src": src})
        if "error" in r:
            raise E.EmuError("fixture", "audio_stream setup: " + r["error"])
        return m


# ---- the port as the second arm (the oracle) --------------------------------------------------------------------------
import ctypes as C  # noqa: E402
import dataclasses  # noqa: E402
import hashlib  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import struct  # noqa: E402

from port.test import lib as L  # noqa: E402

PAGE = 0x1000
DATA_VA = 0x004C1000                      # golf_clean.exe .data = the GameState (port/test/segments.py DATA_VA)
SND_BASE_ORACLE = E.LAYOUTS["A"]["base"]  # the audio core's sound.dll base in an exe key (nothing is mapped there)
FPU_BOX = 0xFEAF0000                      # audio.h kSndFpuBox
HOST_GAP = 0x400
FRAME_LIMIT = 0x700
SETUP_CLAIMS, SETUP_SCRIPT, SETUP_CALLBACK, SETUP_RESET = 1, 2, 4, 8
ABI_NAME = {0: "default", 1: "stdcall", 2: "thiscall", 3: "fastcall"}
STACK_PAGES = range(E.STACK_BASE >> 12, (E.STACK_INIT - 0x8000) >> 12)
MIRROR = (pathlib.Path(os.environ["SG_S4_MIRROR"]) if os.environ.get("SG_S4_MIRROR")
          else ROOT / "port" / "build" / "S4_10_mirror")
BUNDLE_DIR = (pathlib.Path(os.environ["SG_EXE_BUNDLES"]) if os.environ.get("SG_EXE_BUNDLES")
              else ROOT / "port" / "corpus" / "audio_exe")
GOLDEN = (pathlib.Path(os.environ["SG_EXE_GOLDEN"]) if os.environ.get("SG_EXE_GOLDEN")
          else ROOT / "port" / "test" / "golden_audio_exe.json")


class SgSndCall(C.Structure):
    _fields_ = [("va", C.c_uint32), ("rva", C.c_uint32), ("kind", C.c_int32), ("abi", C.c_int32), ("nargs", C.c_int32),
                ("want_st0", C.c_int32), ("args", C.c_uint32 * 16), ("eax", C.c_uint32), ("edx", C.c_uint32),
                ("st0", C.c_double), ("has_st0", C.c_int32), ("status", C.c_int32)]


HOSTFN = C.CFUNCTYPE(C.c_int, C.c_void_p, C.POINTER(SgSndCall))


def mirror_dll() -> pathlib.Path:
    if os.environ.get("SG_PORTSIM_DLL"):
        return pathlib.Path(os.environ["SG_PORTSIM_DLL"])
    for sub in ("x64_all", "x64"):        # 10_mirror.py --modules all --tag _all (every S4 module), else its default build
        m = MIRROR / sub / "Release" / "portsim_c.dll"
        if m.exists():
            return m
    return L.B.dll_path("x64")


def load_lib():
    lib = L.load(mirror_dll(), rebuild=False)
    if not hasattr(lib, "_snd"):
        f = lib.dll.sg_snd_set_host
        f.restype = C.c_int
        f.argtypes = [C.c_void_p, HOSTFN, C.c_void_p]
        lib._snd = True
    return lib


# the x87 state (the image format of portsim::x87::Unit, audio.h)
CC_MASK, TOP_MASK = 0x4700, 0x3800
X87_IMAGE = 4 + 8 + 80


def fpu_image(uc) -> bytes:
    from unicorn.x86_const import UC_X86_REG_FP0, UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG
    out = b"SX87" + struct.pack("<HHHH", uc.reg_read(UC_X86_REG_FPCW) & 0xFFFF, uc.reg_read(UC_X86_REG_FPSW) & 0xFFFF,
                                uc.reg_read(UC_X86_REG_FPTAG) & 0xFFFF, 0)
    for i in range(8):
        m, e = uc.reg_read(UC_X86_REG_FP0 + i)
        out += struct.pack("<QH", m & (2 ** 64 - 1), e & 0xFFFF)
    return out


def set_fpu(uc, img: bytes):
    from unicorn.x86_const import UC_X86_REG_FP0, UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG
    cw, sw, tag, _ = struct.unpack_from("<HHHH", img, 4)
    uc.reg_write(UC_X86_REG_FPCW, cw)
    uc.reg_write(UC_X86_REG_FPSW, sw)
    uc.reg_write(UC_X86_REG_FPTAG, tag)
    for i in range(8):
        m, e = struct.unpack_from("<QH", img, 12 + 10 * i)
        uc.reg_write(UC_X86_REG_FP0 + i, (m, e))


def x87_view(img: bytes) -> dict:
    cw, sw, tag, _ = struct.unpack_from("<HHHH", img, 4)
    regs = {i: struct.unpack_from("<QH", img, 12 + 10 * i) for i in range(8) if (tag >> (2 * i)) & 3 != 3}
    return {"cw": cw | 0x40, "top": (sw & TOP_MASK) >> 11, "cc": sw & CC_MASK, "regs": regs}


def x87_diff(port, emu) -> str:
    if not port or not emu:
        return "" if bool(port) == bool(emu) else "missing x87 image"
    a, b = x87_view(port), x87_view(emu)
    out = [f"{k} port 0x{a[k]:x} emu 0x{b[k]:x}" for k in ("cw", "top", "cc") if a[k] != b[k]]
    if set(a["regs"]) != set(b["regs"]):
        out.append(f"in use port {sorted(a['regs'])} emu {sorted(b['regs'])}")
    out += [f"R{i} differs" for i in sorted(set(a["regs"]) & set(b["regs"])) if a["regs"][i] != b["regs"][i]]
    return "; ".join(out)


def x87_digest(img):
    if not img:
        return None
    v = x87_view(img)
    return hashlib.sha256(json.dumps([v["cw"], v["top"], v["cc"], sorted(v["regs"].items())]).encode()).hexdigest()[:20]


def digest(ret_text, state_hex) -> str:
    return hashlib.sha256(f"{ret_text}|{state_hex}".encode()).hexdigest()[:20]


def slots_of(args: list) -> list:
    out = []
    for a in args:
        t, v = a["t"], a["v"]
        if t == "double":
            out += list(struct.unpack("<II", struct.pack("<d", float(v))))
        elif t in E._WIDE:
            q = int(v) & (2 ** 64 - 1)
            out += [q & 0xFFFFFFFF, q >> 32]
        else:
            out.append(int(v) & 0xFFFFFFFF)
    return out


def entry_esp(spec, slots) -> int:
    nreg = {"thiscall": 1, "fastcall": 2}.get(spec.get("abi"), 0)
    return ((E.STACK_INIT - 0x400) & ~0xF) - 4 * max(0, len(slots) - nreg) - 4


def parse_kv(b) -> dict:
    out = {}
    for line in (b or b"").decode(errors="replace").splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            out[k] = v
    return out


def diff_writes(old: bytes, new: bytes, base: int):
    """the runs of bytes where `new` differs from `old`, as (va, bytes); numpy finds the runs (a byte loop over the 3.6 MB
    GameState per host call made shutdownSoundSystem's keys, thousands of host calls, run for hours)"""
    if old == new:
        return []
    import numpy as np
    mo, mn, n, P = memoryview(old), memoryview(new), len(old), 0x1000
    out, cur = [], None   # cur: [start, end] of a run that may continue into the next page
    for p in range(0, n, P):
        q = min(n, p + P)
        if mo[p:q] == mn[p:q]:
            continue
        a = np.frombuffer(old, dtype=np.uint8, count=q - p, offset=p)
        b = np.frombuffer(new, dtype=np.uint8, count=q - p, offset=p)
        d = np.concatenate(([0], (a != b).view(np.int8), [0]))
        edges = np.flatnonzero(np.diff(d))
        for s, e in zip(edges[0::2], edges[1::2]):
            s, e = p + int(s), p + int(e)
            if cur and cur[1] == s:
                cur[1] = e
            else:
                if cur:
                    out.append(cur)
                cur = [s, e]
    if cur:
        out.append(cur)
    return [(base + s, new[s:e]) for s, e in out]


class ExeSpace:
    """The port context's address space for an exe key: the emulator's .data bytes INTO the GameState (at DATA_VA), every
    other emulator page (but the stack's bottom) mapped as contiguous runs; headers / .text / .rdata read-only."""

    def __init__(self, st):
        self.st = st
        self.runs = []
        self.gs_lo, self.gs_hi = DATA_VA, DATA_VA + st.size

    def load(self, m):
        st = self.st
        st.unmap_all()
        self.runs = []
        st.set_data_base(DATA_VA)
        st.clear()
        self.add({p: m.read(p << 12, PAGE) for p in sorted(m.mapped) if p not in STACK_PAGES})

    def _ro(self, va):
        return EXE_BASE <= va < DATA_VA

    def add(self, pages: dict):
        pieces = []
        for p in sorted(pages):
            lo, b = p << 12, pages[p]
            hi = lo + PAGE
            if lo < self.gs_hi and hi > self.gs_lo:     # the part inside the GameState goes into the state
                a, z = max(lo, self.gs_lo), min(hi, self.gs_hi)
                self.st.write(a - self.gs_lo, b[a - lo:z - lo])
                if lo < a:
                    pieces.append((lo, b[:a - lo]))
                if z < hi:
                    pieces.append((z, b[z - lo:]))
            else:
                pieces.append((lo, b))
        runs = []
        for base, b in pieces:
            if runs and runs[-1][0] + len(runs[-1][1]) == base and self._ro(base) == self._ro(runs[-1][0]):
                runs[-1] = (runs[-1][0], runs[-1][1] + b)
            else:
                runs.append((base, b))
        for base, b in runs:
            buf = (C.c_uint8 * len(b)).from_buffer_copy(b)
            self.st.map(base, buf, readonly=self._ro(base))
            self.runs.append((base, len(b), buf))

    def mapped_pages(self) -> set:
        s = {p for base, size, _ in self.runs for p in range(base >> 12, (base + size + PAGE - 1) >> 12)}
        return s | set(range(self.gs_lo >> 12, (self.gs_hi + PAGE - 1) >> 12))

    def to_emu(self, m):
        """port -> emulator (the state and every writable run)"""
        gs = self.st.image()
        cur = m.read(self.gs_lo, len(gs))
        if cur != gs:
            for va, b in diff_writes(cur, gs, self.gs_lo):
                m.write(va, b)
        for base, size, buf in self.runs:
            if self._ro(base):
                continue
            b = C.string_at(C.addressof(buf), size)
            if m.read(base, size) != b:
                m.write(base, b)

    def from_emu(self, m, record=False):
        """emulator -> port; the byte-level writes (record) and the pages the emulator gained (mapped here too)"""
        writes = []
        gs = self.st.image()
        cur = m.read(self.gs_lo, len(gs))
        if cur != gs:
            if record:
                writes += diff_writes(gs, cur, self.gs_lo)
            self.st.load_image(cur)
        for base, size, buf in self.runs:
            if self._ro(base):
                continue
            cur = m.read(base, size)
            old = C.string_at(C.addressof(buf), size)
            if cur != old:
                if record:
                    writes += diff_writes(old, cur, base)
                C.memmove(C.addressof(buf), cur, size)
        have = self.mapped_pages()
        new = {p: m.read(p << 12, PAGE) for p in sorted(m.mapped) if p not in have and p not in STACK_PAGES}
        if new:
            self.add(new)
        return writes, new


@dataclasses.dataclass
class VecRec:
    args: list
    esp: int
    x87_in: object
    x87_exp: object = None
    x87_port: object = None
    x87_diff: str = ""
    report: dict = dataclasses.field(default_factory=dict)
    exe_report: dict = dataclasses.field(default_factory=dict)
    script: list = dataclasses.field(default_factory=list)
    gs: object = None
    runs: list = dataclasses.field(default_factory=list)
    claims: list = dataclasses.field(default_factory=list)
    error: object = None
    obj: object = None


_ARITY = {}


def arities(lib) -> dict:
    if id(lib) not in _ARITY:
        _ARITY[id(lib)] = {n: f["arity"] for n, f in lib.functions().items()}
    return _ARITY[id(lib)]


class LiveExePort:
    """ExeKeyRun's second arm: the registered port function over the emulator's memory, the emulator as the live host."""

    def __init__(self, lib, key: str, name: str, record: bool = False):
        self.lib, self.key, self.name, self.record = lib, key, name, record
        self.st = lib.state()
        self.space = ExeSpace(self.st)
        self.records = []
        self.err = None
        self._cb = HOSTFN(self._host)
        self.run = None
        self.cur = None

    def close(self):
        self.st.close()

    def _host(self, user, pc):
        c = pc.contents
        try:
            m = self.run.m
            self.space.to_emu(m)
            box = m.read(FPU_BOX, X87_IMAGE)
            if box[:4] == b"SX87":
                set_fpu(m.uc, box)
            args = [{"t": "uint32", "v": int(c.args[i])} for i in range(c.nargs)]
            r = m.native_call(int(c.va), ABI_NAME[c.abi], args, "double" if c.want_st0 else "uint32")
            m.write(FPU_BOX, fpu_image(m.uc))
            writes, new = self.space.from_emu(m, self.record)
            c.eax, c.edx = r["eax"] & 0xFFFFFFFF, r["edx"] & 0xFFFFFFFF
            fb = 0
            if c.want_st0 and "fbits" in r:
                fbits = bytes.fromhex(r["fbits"])
                c.st0 = struct.unpack("<d", fbits)[0] if len(fbits) == 8 else struct.unpack("<f", fbits)[0]
                c.has_st0 = 1
                fb = struct.unpack("<Q", struct.pack("<d", c.st0))[0]
            if self.record:
                self.cur.script.append({"kind": c.kind, "va": c.va, "abi": c.abi, "nargs": c.nargs, "want": c.want_st0,
                                        "args": [int(c.args[i]) for i in range(c.nargs)], "eax": c.eax, "edx": c.edx,
                                        "has_st0": c.has_st0, "st0": fb,
                                        "writes": [(va, b.hex()) for va, b in writes],
                                        "maps": [(p << 12, b.hex()) for p, b in new.items()]})
            return 0
        except Exception as e:  # noqa: BLE001
            self.err = f"host: {type(e).__name__}: {e}"
            return 1

    def __call__(self, run, args):
        self.run = run
        m = run.m
        if (FPU_BOX >> 12) not in m.mapped:
            m._map(FPU_BOX, PAGE)
            m.write(FPU_BOX, bytes(PAGE))
        slots = slots_of(args)
        esp = entry_esp(run.spec, slots)
        # a registry key may pass a slot the function never reads (loadSoundTable's dummy, shutdownSoundSystem's ecx):
        # the port function takes its own arity, the original's extra slot stays on the emulator's side unread
        arity = arities(self.lib).get(self.name)
        if arity is not None and len(slots) > arity:
            slots = slots[:arity]
        img_in, img_exp = getattr(run, "_x87_in", None), getattr(run, "_x87_out", None)
        st = self.st
        self.space.load(m)
        claims = sorted(int(t) for t in m.redirects)
        self.cur = VecRec(slots, esp, img_in, img_exp, claims=claims)
        self.cur.obj = run._req({"op": "curargs"}).get("obj")
        if self.record:
            self.cur.gs = st.image()
            self.cur.runs = [(b, C.string_at(C.addressof(buf), n)) for b, n, buf in self.space.runs]
        assert self.lib.dll.sg_snd_set_host(st.ctx, self._cb, None) == 0
        st.files_put("$audio/claims", b"".join(struct.pack("<I", v) for v in claims))
        st.call("audio_core.setup", [SND_BASE_ORACLE, SETUP_CLAIMS | SETUP_CALLBACK | SETUP_RESET])
        st.call("audio_exe.setup", [1])
        st.files_put("$audio/x87", img_in or b"")
        st.call("audio_core.vector", [esp, esp - FRAME_LIMIT])
        self.err = None
        saved_depth, saved_esp = m.depth, m.uc.reg_read(E.UC_X86_REG_ESP)
        m.depth += 1
        m.uc.reg_write(E.UC_X86_REG_ESP, esp - HOST_GAP)
        out = None
        try:
            res = st.call(self.name, slots)
            out = {"eax": res.eax, "edx": res.edx}
        except L.SgError as e:
            self.cur.error = f"{e}{'; ' + self.err if self.err else ''}"
        finally:
            m.depth = saved_depth
            m.uc.reg_write(E.UC_X86_REG_ESP, saved_esp)
        st.call("audio_core.finish", [])
        st.call("audio_exe.report", [])
        self.cur.report = parse_kv(st.files_read("$audio/out"))
        self.cur.exe_report = parse_kv(st.files_read("$audio_exe/out"))
        self.cur.x87_port = st.files_read("$audio/x87out")
        self.cur.x87_diff = x87_diff(self.cur.x87_port, img_exp) if img_exp else ""
        if self.cur.error is None and self.err:
            self.cur.error = self.err
        self.space.to_emu(m)
        if img_exp:
            set_fpu(m.uc, img_exp)
        self.records.append(self.cur)
        if out is None:
            raise E.EmuError("fault", "port: " + str(self.cur.error))
        return out


# the original arm with the FPU it found and left recorded, when the port is the second arm
_base_arm = ExeKeyRun._arm
_base_setup = ExeKeyRun.setup


def _arm(self):
    if self.second_arm is None:
        return _base_arm(self)
    self._x87_in = fpu_image(self.m.uc)
    try:
        return _base_arm(self)
    finally:
        self._x87_out = fpu_image(self.m.uc)


def _setup(self):
    m = _base_setup(self)
    if self.second_arm is not None:          # the emulator's FPU starts as a Windows thread's: empty stack (S4.2b)
        from unicorn.x86_const import UC_X86_REG_FPSW, UC_X86_REG_FPTAG
        m.uc.reg_write(UC_X86_REG_FPTAG, 0xFFFF)
        m.uc.reg_write(UC_X86_REG_FPSW, m.uc.reg_read(UC_X86_REG_FPSW) & ~TOP_MASK & 0xFFFF)
    return m


ExeKeyRun._arm = _arm
ExeKeyRun.setup = _setup


@functools.lru_cache(maxsize=1)
def _names_by_addr() -> dict:
    out = {}
    with HOOKS_CSV.open(newline="") as f:
        for r in csv.DictReader(f):
            if r["module"] in ("", "golf_clean.exe"):
                out[int(r["addr"], 16)] = r["name"]
    return out


def port_name(key: str) -> str:
    hooks, _ = E.registry()
    return _names_by_addr()[hooks[key]["addr"]]


CRT_HOST = {"operator_new", "operatorDelete_4a4ffc"}


@dataclasses.dataclass
class Verdict:
    key: str
    name: str
    vectors: int
    match: int
    match_bc: int
    errors: int
    fallbacks: int
    exe_fallbacks: int
    tier: str
    first: str
    golden: list
    x87_exit: list
    fallback_names: str = ""
    verdict: str = "RED"


def oracle_key(vm, lib, key, layouts=("A", "B", "C"), record=False):
    """(Verdict, layout A's VecRecs or None, layout A's state regions)"""
    import sound_census as SC
    if key in CUSTOM:
        _install_custom()
    hooks, batch = E.registry()
    spec = hooks[key]
    name = port_name(key)
    res, ports = {}, {}
    for lay in layouts:
        lp = LiveExePort(lib, key, name, record and lay == "A")
        try:
            res[lay] = ExeKeyRun(vm, key, lay, second_arm=lp).run()
        except E.EmuError as e:
            res[lay] = e
        finally:
            lp.close()
        ports[lay] = lp
    a = res["A"]
    if isinstance(a, Exception) or a.fixture_error:
        return Verdict(key, name, len(spec["vectors"]), 0, 0, len(spec["vectors"]), 0, 0, "SETUP_ERR",
                       str(a if isinstance(a, Exception) else a.fixture_error)[:200], [], []), None, []
    first, match, match_bc, errs = "", 0, 0, 0
    golden = []
    recs_a = ports["A"].records
    for i, row in enumerate(a.rows):
        rec = recs_a[i] if i < len(recs_a) else None
        xd = bool(rec is not None and rec.x87_diff)
        golden.append(digest(row.orig, row.state_hex))
        if row.error:
            errs += 1
            first = first or f"v{i} A: {row.error[:200]}"
        elif row.match and not xd:
            match += 1
        elif not first:
            first = (f"v{i} A: x87 {rec.x87_diff}" if xd and row.match else
                     f"v{i} A: ret emu {row.orig} port {row.re}" + ("" if row.state_hex == row.state_re_hex
                                                                     else " (regions differ)"))
    for lay in layouts[1:]:
        rb = res[lay]
        if isinstance(rb, Exception):
            first = first or f"{lay}: {rb}"
            continue
        recs = ports[lay].records
        for i, row in enumerate(rb.rows):
            xd = i < len(recs) and bool(recs[i].x87_diff)
            if row.match and not xd and not row.error:
                match_bc += 1
            elif not first:
                first = f"v{i} {lay}: " + (row.error[:200] if row.error else f"ret emu {row.orig} port {row.re}")
    allrecs = [r for lp in ports.values() for r in lp.records]
    fb = sum(int(r.report.get("fallback", 0) or 0) for r in allrecs)
    names = sorted({n for r in allrecs for n in (r.exe_report.get("fallback_names") or "").split()})
    # the exe's CRT allocator is the replay host's heap by design (exe_new / exe_delete, U-1495): not a pending row
    efb = sum(sum(1 for n in (r.exe_report.get("fallback_names") or "").split() if n not in CRT_HOST) for r in allrecs)
    if key in CUSTOM:
        tier = "EMU_ONLY"                    # no registry key, no live CSV: judged against the emulated original only
    elif {"B", "C"} <= set(res) and not any(isinstance(res[x], Exception) for x in ("B", "C")):
        tier = SC.compare(key, spec, batch.get(key), res, E.read_csv(spec, key))["row"]["tier"]
    else:
        tier = "A_ONLY"
    v = Verdict(key, name, len(a.rows), match, match_bc, errs, fb, efb, tier, first, golden,
                [x87_digest(r.x87_exp) for r in recs_a], " ".join(names))
    allm = v.vectors > 0 and v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1)
    if allm and tier in ("EXACT", "EQUAL_PI", "EMU_ONLY"):
        v.verdict = "GREEN_PENDING" if efb else "GREEN"
    return v, (recs_a if record else None), [list(x) for x in a.regions]


# ---- bundles and the default replay (no emulator, no Node) -------------------------------------------------------------
# A bundle (port/corpus/audio_exe/<key>.json.z, local) holds per vector of layout A the port's starting memory as page
# deltas (GameState pages by their virtual address, the other mapped runs), the argument slots, entry esp, x87 entry image,
# the claims and the recorded host calls (audio_core's "$audio/script" format). The default replay runs the port from it
# with the script host and checks the return text and the regions against the golden digests pinned from the emulated
# original (port/test/golden_audio_exe.json).
import base64  # noqa: E402
import zlib  # noqa: E402


def _safe(key: str) -> str:
    return key.replace("::", "_").replace("<", "(").replace(">", ")").replace("*", "P")


def rec_pages(rec) -> dict:
    """{page index: (bytes, readonly)} of a recorded vector's port memory"""
    out = {}
    gs = rec.gs
    for k in range(0, len(gs), PAGE):
        out[(DATA_VA + k) >> 12] = (gs[k:k + PAGE], False, "gs")
    for base, b in rec.runs:
        for k in range(0, len(b), PAGE):
            va = base + k
            out[(va >> 12, va & 0xFFF)] = (b[k:k + PAGE], EXE_BASE <= va < DATA_VA, "run")
    return out


def save_bundle(key: str, recs: list, regions: list):
    BUNDLE_DIR.mkdir(parents=True, exist_ok=True)
    prev = {}
    vecs = []
    for r in recs:
        pages = rec_pages(r)
        delta = {}
        for k, (b, ro, kind) in pages.items():
            if prev.get(k) != b:
                delta[json.dumps(k)] = [base64.b64encode(b).decode(), ro, kind]
        prev = {k: v[0] for k, v in pages.items()}
        vecs.append({"args": r.args, "esp": r.esp, "x87": (r.x87_in or b"").hex(), "claims": r.claims, "obj": r.obj,
                     "script": r.script, "error": r.error, "delta": delta, "keys": [json.dumps(k) for k in pages]})
    blob = json.dumps({"key": key, "format": 1, "regions": regions, "vectors": vecs}).encode()
    (BUNDLE_DIR / f"{_safe(key)}.json.z").write_bytes(zlib.compress(blob, 6))


def load_bundle(key: str):
    p = BUNDLE_DIR / f"{_safe(key)}.json.z"
    return json.loads(zlib.decompress(p.read_bytes())) if p.exists() else None


def script_blob(entries: list) -> bytes:
    out = bytearray(b"SGAS" + struct.pack("<II", 1, len(entries)))
    for e in entries:
        out += struct.pack("<5I", e["kind"], e["va"], e["abi"], e["nargs"], e["want"])
        out += b"".join(struct.pack("<I", a) for a in e["args"])
        out += struct.pack("<3I", e["eax"], e["edx"], e["has_st0"]) + struct.pack("<Q", e["st0"])
        out += struct.pack("<I", len(e["writes"]))
        for va, hx in e["writes"]:
            b = bytes.fromhex(hx)
            out += struct.pack("<II", va, len(b)) + b
        out += struct.pack("<I", len(e["maps"]))
        for va, hx in e["maps"]:
            b = bytes.fromhex(hx)
            out += struct.pack("<II", va, len(b)) + b
    return bytes(out)


def load_golden() -> dict:
    return json.loads(GOLDEN.read_text()) if GOLDEN.exists() else {}


def golden_entry(v, regions) -> dict:
    return {"name": v.name, "vectors": v.vectors, "digests": v.golden, "tier": v.tier, "regions": regions,
            "x87_exit": v.x87_exit, "exe_fallbacks": v.exe_fallbacks, "fallback_names": v.fallback_names}


@dataclasses.dataclass
class Replay:
    key: str
    vectors: int
    passed: int
    first: str


def replay_key(lib, key: str, negctl: tuple | None = None) -> Replay:
    """The bundle of `key` replayed through the script host; each vector's digest must equal the golden one."""
    g = load_golden().get(key)
    b = load_bundle(key)
    if g is None or b is None:
        return Replay(key, 0, 0, "no golden entry" if g is None else "no bundle")
    if key in CUSTOM:
        _install_custom()
    hooks, _ = E.registry()
    spec = hooks[key]
    name = g["name"]
    regions = [tuple(x) for x in b["regions"]]
    style = E.csv_double_style(spec, key)
    passed, first = 0, ""
    pages: dict = {}
    if negctl:
        lib.negctl(negctl[0], negctl[1])
    try:
        for i, v in enumerate(b["vectors"]):
            for k, (b64, ro, kind) in v["delta"].items():
                pages[k] = (base64.b64decode(b64), ro, kind)
            keep = set(v["keys"])
            for k in list(pages):
                if k not in keep:
                    del pages[k]
            with lib.state() as st:
                st.set_data_base(DATA_VA)
                runs, bufs = [], []
                items = []
                for k, (data, ro, kind) in pages.items():
                    kk = json.loads(k)
                    if kind == "gs":
                        st.write((kk << 12) - DATA_VA, data)
                    else:
                        items.append(((kk[0] << 12) | kk[1], data, ro))
                items.sort()
                for va, data, ro in items:
                    if runs and runs[-1][0] + len(runs[-1][1]) == va and runs[-1][2] == ro:
                        runs[-1] = (runs[-1][0], runs[-1][1] + data, ro)
                    else:
                        runs.append((va, data, ro))
                for va, data, ro in runs:
                    buf = (C.c_uint8 * len(data)).from_buffer_copy(data)
                    st.map(va, buf, readonly=ro)
                    bufs.append((va, len(data), buf))
                st.files_put("$audio/claims", b"".join(struct.pack("<I", x) for x in v["claims"]))
                st.files_put("$audio/script", script_blob(v["script"]))
                st.call("audio_core.setup", [SND_BASE_ORACLE, SETUP_CLAIMS | SETUP_SCRIPT | SETUP_RESET])
                st.call("audio_exe.setup", [1])
                st.files_put("$audio/x87", bytes.fromhex(v["x87"]))
                st.call("audio_core.vector", [v["esp"], v["esp"] - FRAME_LIMIT])
                try:
                    res = st.call(name, v["args"])
                    r = {"eax": res.eax, "edx": res.edx}
                except L.SgError as e:
                    first = first or f"v{i}: {e}"
                    continue
                fx = {"obj": {"p": v["obj"]}} if v.get("obj") is not None else {}
                text = (E.format_double(E._from_json_float(E._json_float(res.st0)), style)
                        if spec["ret"] in ("double", "float") else E.format_ret(spec["ret"], r, fx))

                def read(va, n):
                    if DATA_VA <= va and va + n <= DATA_VA + st.size:
                        return st.read(va - DATA_VA, n)
                    for base, size, buf in bufs:
                        if base <= va and va + n <= base + size:
                            return C.string_at(C.addressof(buf) + va - base, n)
                    raise KeyError(f"0x{va:08x}+{n} not in the bundle's memory")

                hexs = "|".join(read(a, n).hex() for a, n in regions) if regions else None
                if digest(text, hexs) == g["digests"][i]:
                    passed += 1
                elif not first:
                    first = f"v{i}: digest differs (ret {text})"
    finally:
        if negctl:
            lib.negctl_reset()
    return Replay(key, len(b["vectors"]), passed, first)


def oracle_main(argv=None) -> int:
    """oracle over the keys; --update records the bundles of GREEN keys and pins their goldens"""
    import argparse
    import time
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys")
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--layouts", default="ABC")
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    keys = a.keys.split(",") if a.keys else exe_keys() + sorted(CUSTOM)
    lib = load_lib()
    gold = load_golden()
    rows, t0 = [], time.time()
    with E.FixVM() as vm:
        for n, k in enumerate(keys, 1):
            v, recs, regions = oracle_key(vm, lib, k, tuple(a.layouts), record=a.update)
            rows.append(v)
            print(f"[{n}/{len(keys)}] {k} {v.verdict} match {v.match}/{v.vectors} bc {v.match_bc} err {v.errors} "
                  f"fb {v.fallbacks} exe_fb {v.exe_fallbacks} {v.fallback_names} tier {v.tier} {v.first}", flush=True)
            if a.update and recs is not None and v.verdict.startswith("GREEN"):
                save_bundle(k, recs, regions)
                gold[k] = golden_entry(v, regions)
    if a.update:
        GOLDEN.write_text(json.dumps(gold, indent=1, sort_keys=True) + "\n")
    if a.out:
        with open(a.out, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["key", "name", "verdict", "vectors", "match", "match_bc", "errors", "fallbacks", "exe_fallbacks",
                        "fallback_names", "tier", "first"])
            for v in rows:
                w.writerow([v.key, v.name, v.verdict, v.vectors, v.match, v.match_bc, v.errors, v.fallbacks,
                            v.exe_fallbacks, v.fallback_names, v.tier, v.first])
    c = collections.Counter(v.verdict for v in rows)
    print(f"keys {len(rows)} vectors {sum(v.vectors for v in rows)} in {time.time() - t0:.0f} s: {dict(c)}")
    return 1 if c.get("RED") else 0


# ---- the segments' AudioSink command stream (gate b) -------------------------------------------------------------------
STREAM_DIR = (pathlib.Path(os.environ["SG_AUDIO_STREAMS"]) if os.environ.get("SG_AUDIO_STREAMS")
              else ROOT / "port" / "corpus" / "audio_stream")


def frame_lines(raw: str, records: list) -> list:
    """The sink's lines with each frame marker `import portsim!frame <t_head>` written as `frame <record> <t_head>`:
    record k is the frame whose head clock is records[k]'s T (record 0 = the segment's start frame; -1 = not a record)."""
    by_t = {}
    for k, r in enumerate(records):
        by_t.setdefault(int(r[0]), k)
    out = []
    for line in raw.splitlines():
        if line.startswith("import portsim!frame "):
            t = int(line.split()[2])
            out.append(f"frame {by_t.get(t, -1)} {t}")
        elif line.startswith("import golf_clean.exe!"):
            out.append("req " + line[len("import golf_clean.exe!"):])
        else:
            out.append(line)
    return out


def _stream_one(spec, name: str) -> dict:
    from port.test import integrator as I
    from port.test import segments as S
    lib = I.load_lib(spec)
    scen, k = name.split("/")
    seg = S.load_segment(S.SEG_DIR / scen / k)
    end = list(seg.checkpoints.values())[-1]["hashes"] if seg.checkpoints else None
    box = {}

    def prepare(st):
        st.call("audio_exe.useSink", [0])

    def after(st, res):
        st.call("audio_exe.streamLog", [])
        st.call("audio_exe.report", [])
        box["raw"] = (st.files_read("$audio_exe/stream") or b"").decode()
        box["report"] = parse_kv(st.files_read("$audio_exe/out"))

    try:
        res = I.run_from(lib, name, seg, seg.trace, seg.records, end, prepare=prepare, after=after)
        row = dataclasses.asdict(res)
    except Exception as e:  # noqa: BLE001
        row = dataclasses.asdict(I.RunResult(name, "ERROR", detail=repr(e)[:300]))
    lines = frame_lines(box.get("raw", ""), seg.records)
    STREAM_DIR.mkdir(parents=True, exist_ok=True)
    (STREAM_DIR / (name.replace("/", "__") + ".txt")).write_text("\n".join(lines) + ("\n" if lines else ""))
    reqs = [ln for ln in lines if ln.startswith("req ")]
    others = [ln for ln in lines if not ln.startswith(("req ", "frame "))]
    row["requests"] = len(reqs)
    row["frames_with_requests"] = sum(1 for ln in lines if ln.startswith("frame "))
    row["unmatched_frames"] = sum(1 for ln in lines if ln.startswith("frame -1 "))
    row["other_commands"] = len(others)
    row["by_name"] = " ".join(f"{n}:{c}" for n, c in sorted(collections.Counter(r.split()[1] for r in reqs).items()))
    row["stream_sha"] = hashlib.sha256("\n".join(lines).encode()).hexdigest()[:16]
    row["exe_requests_counted"] = box.get("report", {}).get("requests", "")
    return row


STREAM_COLS = ["name", "status", "frames_run", "frames_expected", "code", "record", "objects", "noncausal", "digest",
               "seconds", "requests", "frames_with_requests", "unmatched_frames", "other_commands", "exe_requests_counted",
               "by_name", "stream_sha", "run_detail", "detail"]


def stream_main(argv=None) -> int:
    import argparse
    import concurrent.futures as CF
    import collections as _c  # noqa: F401
    import time
    from port.test import integrator as I
    ap = argparse.ArgumentParser()
    ap.add_argument("--backend", choices=["x64", "win32"], default=None)
    ap.add_argument("--dll")
    ap.add_argument("--dll32")
    ap.add_argument("--runner32")
    ap.add_argument("--build")
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--only", default="")
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    spec = I.LibSpec.from_args(a)
    names = [I.seg_name(r) for r in I.accepted_rows("all")]
    if a.only:
        want = set(a.only.split(","))
        names = [n for n in names if n in want or n.split("/")[0] in want]
    print(f"library: {spec.describe()}; {len(names)} segments", flush=True)
    rows, t0 = [], time.time()
    with CF.ProcessPoolExecutor(a.jobs) as ex:
        futs = {ex.submit(_stream_one, spec, n): n for n in names}
        for i, f in enumerate(CF.as_completed(futs), 1):
            try:
                r = f.result()
            except Exception as e:  # noqa: BLE001
                r = {"name": futs[f], "status": "ERROR", "detail": repr(e)[:300]}
            rows.append(r)
            print(f"[{i}/{len(names)}] {r['name']} {r['status']} {r.get('frames_run')}/{r.get('frames_expected')} "
                  f"requests {r.get('requests')} other {r.get('other_commands')} unmatched {r.get('unmatched_frames')} "
                  f"digest={r.get('digest')} | {str(r.get('run_detail', ''))[:100]}", flush=True)
    rows.sort(key=lambda r: r["name"])
    I.write_tsv(pathlib.Path(a.out), rows, STREAM_COLS)
    st = collections.Counter(r["status"] for r in rows)
    print(json.dumps({"counts": dict(st), "requests": sum(int(r.get("requests") or 0) for r in rows),
                      "other_commands": sum(int(r.get("other_commands") or 0) for r in rows),
                      "wall_s": round(time.time() - t0, 1)}), flush=True)
    return 0


import collections  # noqa: E402

def replay_main(argv=None) -> int:
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys")
    ap.add_argument("--negctl", help="fn:mode")
    a = ap.parse_args(argv)
    lib = load_lib()
    keys = a.keys.split(",") if a.keys else sorted(k for k in load_golden() if not k.startswith("$"))
    neg = None
    if a.negctl:
        f, m = a.negctl.rsplit(":", 1)
        neg = (f, int(m))
    bad = 0
    for n, k in enumerate(keys, 1):
        r = replay_key(lib, k, neg)
        ok = r.vectors > 0 and r.passed == r.vectors
        bad += not ok
        print(f"[{n}/{len(keys)}] {k} {'PASS' if ok else 'FAIL'} {r.passed}/{r.vectors} {r.first}", flush=True)
    print(f"keys {len(keys)} fail {bad}")
    return 1 if bad else 0


# ---- the device-present path (gate c) ----------------------------------------------------------------------------------
def device_run(lib, plays=300, ticks=40, neg=None, segment="sandbox_basic/00"):
    """audio_exe.deviceFixture on a segment start image, then initSound, loadSoundTable, playSound for the first `plays`
    playable slots, `ticks` multimedia timer ticks and shutdownSoundSystem, all into the DeviceSink with no host:
    (each step's eax, the module's report, the stream lines)"""
    from port.test import segments as S
    scen, k = segment.split("/")
    seg = S.load_segment(S.SEG_DIR / scen / k)
    if neg:
        lib.negctl(*neg)
    try:
        with lib.state() as st:
            st.set_data_base(DATA_VA)
            st.files_root(S.INSTALL)
            st.load_image(seg.start)
            st.call("audio_exe.deviceFixture", [1])
            out = [st.call("audio_exe.deviceStep", [s, n]).eax
                   for s, n in ((1, 0), (2, 0), (3, plays), (4, ticks), (5, 0))]
            st.call("audio_exe.report", [])
            st.call("audio_exe.streamLog", [])
            rep = parse_kv(st.files_read("$audio_exe/out"))
            lines = (st.files_read("$audio_exe/stream") or b"").decode().splitlines()
    finally:
        if neg:
            lib.negctl_reset()
    return out, rep, lines


NL = chr(10)


def device_sha(lines) -> str:
    return hashlib.sha256(NL.join(lines).encode()).hexdigest()


def device_main(argv=None) -> int:
    """the device-present run; --update pins its stream digest as the golden entry `$device`"""
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--out")
    a = ap.parse_args(argv)
    out, rep, lines = device_run(load_lib())
    sha = device_sha(lines)
    print(f"steps {[hex(x) for x in out]} report {rep} lines {len(lines)} sha {sha}")
    if a.out:
        pathlib.Path(a.out).write_text(NL.join(lines) + NL)
    if a.update:
        gold = load_golden()
        gold["$device"] = {"stream_sha256": sha, "lines": len(lines), "steps": out}
        GOLDEN.write_text(json.dumps(gold, indent=1, sort_keys=True) + NL)
    return 0


if __name__ == "__main__":
    CMDS = {"oracle": oracle_main, "stream": stream_main, "replay": replay_main, "device": device_main}
    cmd, rest = (sys.argv[1], sys.argv[2:]) if len(sys.argv) > 1 and sys.argv[1] in CMDS else ("stream", sys.argv[1:])
    sys.exit(CMDS[cmd](rest))
