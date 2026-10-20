r"""Gate (d) of phase 3.8 (stream 38f, re/PHASE38_PLAN.md section 3): a recorded segment driven through the SEMANTIC
command kinds of phase 3.7b (portsim/sim/commands.h, 0x100..0x106) instead of raw root-handler calls.

    ~/bin/py -3.12 -m port.test.semantic translate --segment championship/00 [--segment ...] [--build 38f]
    ~/bin/py -3.12 -m port.test.semantic run --segment championship/00 [--mode semantic|hybrid|raw] [--build 38f]

TRANSLATION. Every dispatch of the segment's trace (port/test/trace_commands.py: one batch of HANDLER records per pump
call) is classified from what the port's raw run does with it. A press whose click reaches one of the seven click
handlers that have a semantic kind (H2 of mainLoop calls them with the click position) is translated into that kind,
with arg[0] = the handler's own hit test evaluated on the state the handler was called with (SG_TICK_DUMP pre-state of
the call, the hit-test function called through sg_call):

    clickGolferPanel      0x100 GolferPanelAction   hitGolferPanel435f00
    handleEmployeeAction  0x101 EmployeeAction      hitEmployee436b00
    clickEmployeeView     0x102 EmployeeViewSelect  hitEmployeeSlot435570
    clickBuildTool433d30  0x103 BuildToolSelect     hitBuildTool433c60
    pickBuilding          0x104 BuildingSelect      hitBuildingTool
    clickAmenityPanel     0x105 AmenitySelect       pickAmenity434980
    clickSlotPanel        0x106 SlotPanelSelect     hitTestSlot432f90

The click's mouse-down (the onPoint batch at the same point before it) belongs to the same gesture. Every other batch
(keys: onKey / onChar / onChar0; presses no click handler took; their mouse-downs) has NO semantic translation and is
reported as a GAP with what it is.

RUNS (sg_run_frames with the integrator module's batch overrides, portsim/sim/integrator.h: the pump call that took a
recorded batch applies the replacement instead):
  semantic  every recorded batch replaced: translated presses by their semantic command, everything else dropped, so no
            RootHandler command reaches the port (the gate as the plan states it)
  hybrid    only the translated gestures replaced (press -> semantic command, its mouse-down dropped); gaps keep their
            raw records: checks the translations themselves
  raw       no override (the segment as the corpus runs it)
Each run is compared with the segment's frames.bin and end checkpoint like port/test/segments.py.
"""
from __future__ import annotations

import argparse
import ctypes as C
import dataclasses
import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))

import trace_format as TF  # noqa: E402

from port.test import integrator as I  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import segments as S  # noqa: E402
from port.test import trace_commands as TC  # noqa: E402

STRADDLERS = {  # click handler -> (semantic kind, its hit test)
    "clickGolferPanel": (0x100, "hitGolferPanel435f00"),
    "handleEmployeeAction": (0x101, "hitEmployee436b00"),
    "clickEmployeeView": (0x102, "hitEmployeeSlot435570"),
    "clickBuildTool433d30": (0x103, "hitBuildTool433c60"),
    "pickBuilding": (0x104, "hitBuildingTool"),
    "clickAmenityPanel": (0x105, "pickAmenity434980"),
    "clickSlotPanel": (0x106, "hitTestSlot432f90"),
}
KIND_NAMES = {0x100: "GolferPanelAction", 0x101: "EmployeeAction", 0x102: "EmployeeViewSelect", 0x103: "BuildToolSelect",
              0x104: "BuildingSelect", 0x105: "AmenitySelect", 0x106: "SlotPanelSelect"}
PRESS = (1, 10)          # onPress1 / onPress1b: a left click (the click handlers read the left click's position)
OUT = ROOT / "log" / "p38" / "38f"
BLOCK_VA = 0x60000000    # where the override table is mapped while integrator.setBatchOverrides copies it


def seg_of(name: str) -> S.Segment:
    scen, k = name.split("/")
    return S.load_segment(S.SEG_DIR / scen / f"{int(k):02d}")


# ------------------------------------------------------------------------------------------- the raw run's facts
def callee_counts(lib, seg: S.Segment) -> dict:
    """{callee: calls} of a raw run of the segment (sg_tick_callee_counts)."""
    f = lib.dll.sg_tick_callee_counts
    f.argtypes = [C.c_char_p, C.c_int32, C.c_int32]
    f(C.create_string_buffer(16), 16, 1)
    I.run_from(lib, seg.name, seg, seg.trace, seg.records, None)
    buf = C.create_string_buffer(1 << 20)
    f(buf, 1 << 20, 0)
    out = {}
    for line in buf.value.decode().splitlines():
        if "=" in line:
            k, v = line.rsplit("=", 1)
            out[k] = int(v)
    return out


def _dump_child(segname: str, callee: str, out_dir: str, dll: str):
    os.environ["SG_TICK_DUMP"] = f"{callee};{out_dir}"
    lib = L.load(dll)
    seg = seg_of(segname)
    I.run_from(lib, seg.name, seg, seg.trace, seg.records, None)


def straddler_calls(spec: I.LibSpec, seg: S.Segment, callee: str) -> list[dict]:
    """Every call of `callee` in a raw run of the segment: its arguments and its hit test's value on the state it was called
    with (one child process per callee: SG_TICK_DUMP is read once per process)."""
    kind, hit = STRADDLERS[callee]
    with tempfile.TemporaryDirectory(prefix="sem38f_") as td:
        subprocess.run([sys.executable, "-m", "port.test.semantic", "_dump", seg.name, callee, td, spec.dll],
                       cwd=str(ROOT), check=True)
        lib = L.load(spec.dll)
        out = []
        k = 0
        while (pathlib.Path(td) / f"{callee}_{k}.pre.bin").exists():
            pre = (pathlib.Path(td) / f"{callee}_{k}.pre.bin").read_bytes()
            args = [int(x) for x in (pathlib.Path(td) / f"{callee}_{k}.args").read_text().split()]
            with lib.state() as st:
                st.set_data_base(S.DATA_VA)
                st.load_image(pre)
                v = st.call(hit, args[:2]).eax
            v = v - (1 << 32) if v & 0x80000000 else v
            out.append({"callee": callee, "k": k, "x": _s32(args[0]), "y": _s32(args[1]), "kind": kind, "hit": hit,
                        "arg0": v})
            k += 1
    return out


def _s32(v: int) -> int:
    return v - (1 << 32) if v & 0x80000000 else v


# ------------------------------------------------------------------------------------------- translation
@dataclasses.dataclass
class Row:
    segment: str
    batch: int
    frame: int
    context: str
    pos: int
    handlers: str
    args: str
    category: str
    translation: str          # "" = gap
    semantic: dict | None = None


def describe(b: TC.Batch) -> tuple[str, str]:
    hs = [c.handler for c in b.commands]
    names = "+".join(TF.HANDLERS[h].replace("rootWin_", "") for h in hs)
    args = " ".join(f"({c.a0},{c.a1})" for c in b.commands)
    return names, args


def gap_category(b: TC.Batch) -> str:
    hs = {c.handler for c in b.commands}
    if hs & {7, 8, 9}:
        keys = sorted({c.a0 for c in b.commands if c.handler in (7, 8)})
        return "key " + ",".join(f"0x{k:02x}" for k in keys) if keys else "key release (onChar0)"
    if hs & set(PRESS) or hs & {4, 11}:
        return "press no semantic click handler took"
    if hs & {2, 5}:
        return "mouse-down"
    if hs & {3, 6}:
        return "mouse-up"
    return "other"


def translate(spec: I.LibSpec, segname: str) -> list[Row]:
    lib = I.load_lib(spec)
    seg = seg_of(segname)
    bs = TC.batches(TF.parse(seg.trace))
    counts = callee_counts(lib, seg)
    calls = []
    for callee in STRADDLERS:
        if counts.get(callee):
            calls += straddler_calls(spec, seg, callee)
    # match calls (in run order per callee; across callees by frame order of the press batches) to press batches
    taken: dict[int, dict] = {}
    for c in calls:
        for i, b in enumerate(bs):
            if i in taken:
                continue
            if any(x.handler in PRESS and x.a0 == c["x"] and x.a1 == c["y"] for x in b.commands):
                taken[i] = c
                break
        else:
            c["unmatched"] = True
    downs: dict[int, int] = {}       # mouse-down batch -> its press batch
    for i in taken:
        p = next(x for x in bs[i].commands if x.handler in PRESS)
        for j in range(i - 1, -1, -1):
            if any(x.handler == 2 and x.a0 == p.a0 and x.a1 == p.a1 for x in bs[j].commands):
                if j not in taken and j not in downs:
                    downs[j] = i
                break
            if any(x.handler in PRESS or x.handler == 2 for x in bs[j].commands):
                break
    rows = []
    for i, b in enumerate(bs):
        names, args = describe(b)
        pos = b.commands[0].pos
        if i in taken:
            c = taken[i]
            sem = {"kind": c["kind"], "arg": [c["arg0"]] + [0] * 7}
            extra = [x for x in b.commands if x.handler not in PRESS and x.handler != 3]
            cat = f"press -> {c['callee']}"
            tr = f"{KIND_NAMES[c['kind']]}({c['arg0']}) [{c['hit']}({c['x']},{c['y']})]"
            if extra:
                cat += f" (+{len(extra)} other records in the batch: gap)"
            rows.append(Row(seg.name, i, b.frame, b.context, pos, names, args, cat, tr, sem))
        elif i in downs:
            rows.append(Row(seg.name, i, b.frame, b.context, pos, names, args,
                            f"mouse-down of the press in batch {downs[i]}", "dropped (part of that gesture)"))
        else:
            rows.append(Row(seg.name, i, b.frame, b.context, pos, names, args, gap_category(b), ""))
    unmatched = [c for c in calls if c.get("unmatched")]
    if unmatched:
        print(f"{seg.name}: {len(unmatched)} click-handler calls matched no recorded press: {unmatched[:3]}", flush=True)
    return rows


# ------------------------------------------------------------------------------------------- runs
def pack_overrides(table: list[tuple[int, list[dict]]]) -> bytes:
    out = bytearray()
    for pos, cmds in table:
        out += struct.pack("<II", pos, len(cmds))
        for c in cmds:
            a = (c.get("arg") or []) + [0] * 8
            out += struct.pack("<IIiiIII8iI", c["kind"], c.get("handler", 0), c.get("a0", 0), c.get("a1", 0),
                               c.get("async_ctrl", 0), c.get("async_shift", 0), c.get("t", 0), *a[:8], 0)
    return bytes(out)


def overrides_for(rows: list[Row], mode: str) -> list[tuple[int, list[dict]]]:
    tab = []
    for r in rows:
        if r.semantic is not None:
            tab.append((r.pos, [r.semantic]))
        elif r.translation.startswith("dropped"):
            tab.append((r.pos, []))
        elif mode == "semantic":
            tab.append((r.pos, []))
    return tab if mode != "raw" else []


def run_mode(spec: I.LibSpec, segname: str, rows: list[Row], mode: str) -> dict:
    lib = I.load_lib(spec)
    seg = seg_of(segname)
    table = overrides_for(rows, mode)
    blob = pack_overrides(table)
    applied = {}

    def prepare(st):
        if not table:
            return
        buf = (C.c_uint8 * max(16, len(blob))).from_buffer_copy(blob + b"\0" * max(0, 16 - len(blob)))
        st.map(BLOCK_VA, buf, readonly=True)
        n = st.call("integrator.setBatchOverrides", [BLOCK_VA, len(table)]).eax
        st.unmap_all()
        if n != len(table):
            raise RuntimeError(f"setBatchOverrides took {n} of {len(table)}")

    def after(st, res):
        applied["n"] = st.call("integrator.appliedCount", []).eax
        st.call("integrator.clearBatchOverrides", [])

    end = list(seg.checkpoints.values())[-1]["hashes"] if seg.checkpoints else None
    res = I.run_from(lib, f"{seg.name}:{mode}", seg, seg.trace, seg.records, end, prepare=prepare, after=after)
    d = dataclasses.asdict(res)
    sem = sum(1 for _, c in table if c)
    d.update(mode=mode, overrides=len(table), semantic_commands=sem, raw_batches_left=len(rows) - len(table),
             applied=applied.get("n"),
             root_handler_commands_left=sum(len(r.handlers.split("+")) for r in rows
                                            if r.pos not in {p for p, _ in table}))
    return d


def state_after(spec: I.LibSpec, segname: str, rows: list[Row], mode: str, frames: int) -> bytes:
    """The GameState image after `frames` frames of a run in `mode` (no comparison)."""
    lib = I.load_lib(spec)
    seg = seg_of(segname)
    table = overrides_for(rows, mode)
    blob = pack_overrides(table)
    with lib.state() as st:
        st.set_data_base(S.DATA_VA)
        if S.INSTALL.is_dir():
            st.files_root(S.INSTALL)
        st.load_image(seg.start)
        if table:
            buf = (C.c_uint8 * max(16, len(blob))).from_buffer_copy(blob + b"\0" * max(0, 16 - len(blob)))
            st.map(BLOCK_VA, buf, readonly=True)
            st.call("integrator.setBatchOverrides", [BLOCK_VA, len(table)])
            st.unmap_all()
        st.run_frames(seg.trace, seg.queued, frames)
        img = st.image()
        st.call("integrator.clearBatchOverrides", [])
    return img


def cmd_diff(a) -> int:
    """Every sim-class byte that differs between the raw run and a `mode` run after `frames` frames (first 40)."""
    spec = I.LibSpec.from_args(a)
    src = pathlib.Path(a.translation or OUT / "semantic_translation.json")
    allrows = [Row(**r) for r in json.loads(src.read_text())]
    sg = a.segment[0]
    rows = [r for r in allrows if r.segment == seg_of(sg).name]
    lib = I.load_lib(spec)
    ra = state_after(spec, sg, rows, "raw", a.frames)
    rb = state_after(spec, sg, rows, a.mode[0], a.frames)
    n = 0
    for cls in ("sim", "hidden", "ui"):
        for name, off, size in lib.objects(cls):
            x, y = ra[off:off + size], rb[off:off + size]
            if x == y:
                continue
            for k in range(0, size, 4):
                if x[k:k + 4] != y[k:k + 4]:
                    print(f"{cls:6s} {name}+0x{k:x}: raw {x[k:k + 4].hex()} {a.mode[0]} {y[k:k + 4].hex()}")
                    n += 1
                    if n >= 40:
                        return 0
    if n == 0:
        print("no sim / hidden / ui byte differs")
    return 0


# ------------------------------------------------------------------------------------------- input actions (38i)
# Phase 3.8 stream 38i (log/p38/38i_notes.md, portsim/sim/semantic.h): EVERY recorded dispatch translated into the input
# action kinds 0x200.. (Key, PointerDown, Click, TileClick), which the port lowers to the root-handler calls their input
# makes, so mainLoop H1 / H2 and the modal screens react as to the recorded input. Per batch (one pump call's handler
# records, port/test/trace_commands.py):
#   onChar(vk) + onKey(ch, vk) [+ onKey(ch2, vk) ...]  -> Key(the action whose binding is (vk, last ch)), else Key(Char)
#   onKey(0, vk) alone                                   -> Key(the non-character key vk), else Key(Char, 0, vk)
#   onKey(ch, code) alone (a WM_CHAR with no key-down: jgld passes its last key-down's code)
#                                                        -> Key(Char, ch, vk 0, code); --named-chars: Key(the action of ch)
#   onChar0                                              -> nothing (the release writes only g_char83821c, never read)
#   onPoint / onPoint2                                   -> PointerDown(x, y, 1 / 2)
#   onPress1 + onUnpoint / onPress2 + onUnpoint2         -> Click(x, y, 1 / 2, modifiers, target)
#   anything else                                        -> a GAP with what it is (the batch is dropped in a semantic run)
# A Key from a modal dispatch with a printable character is written Key(Char) (text typed into a screen), the same
# lowering as its named action. The Click TARGET of a frame-body dispatch is where H2's routing sends the click on the
# port's own state at the dispatch point: a PROBE run (every target Any, the semantic module's click log on) records
# it, the FINAL run states it and the port checks it (CommandRefused when it does not hold). Modal-dispatch clicks keep
# target Any (the screen's own hit tests decide).
KIND_KEY, KIND_DOWN, KIND_CLICK, KIND_TILE = 0x200, 0x201, 0x202, 0x203
TARGET_NAMES = {0: "Any", 1: "World", 2: "Panel", 3: "Hud", 4: "Swing", 5: "Place", 6: "GolferAction", 7: "TerrainEdit"}
SEMANTIC_H = ROOT / "port" / "include" / "portsim" / "sim" / "semantic.h"
OUT38I = ROOT / "log" / "p38" / "38i"
BODY_CONTEXTS = ("frame-head", "frame-palette")
NAV_CHARS = {0x08, 0x09, 0x0D, 0x1B, 0x20}   # keys named in every context (the modal screens read them by code too)


def key_table() -> dict:
    """{id: (name, vk, ch)} from semantic.h's SG_SEMANTIC_KEYS rows (the one source of the bindings)."""
    import re
    out = {}
    pat = r"SG_KEY\((\d+), (\w+), (0x[0-9a-fA-F]+), ('.'|0x[0-9a-fA-F]+|0)\)"
    for m in re.finditer(pat, SEMANTIC_H.read_text()):
        ch = m.group(4)
        ch = ord(ch[1]) if ch.startswith("'") else int(ch, 0)
        out[int(m.group(1))] = (m.group(2), int(m.group(3), 16), ch)
    return out


@dataclasses.dataclass
class XRow:
    segment: str
    batch: int
    frame: int
    context: str
    pos: int
    handlers: str
    args: str
    category: str
    translation: str
    gap: str = ""
    commands: list = dataclasses.field(default_factory=list)   # [{"kind": k, "arg": [8 ints]}]


def _mods(c) -> int | None:
    m = 0
    for bit, v in ((1, c.async_ctrl), (2, c.async_shift)):
        hb = (v >> 8) & 0xFF
        if hb not in (0, 0x80):
            return None
        if hb:
            m |= bit
    return m


def _char_text(ch: int) -> str:
    return repr(chr(ch)) if 0x20 < ch < 0x7F else f"0x{ch:02x}"


def _key_cmd(keys: dict, vk: int, ch: int, modal: bool) -> tuple[dict, str]:
    by = {(v, c): i for i, (_, v, c) in keys.items()}
    named = by.get((vk, ch))
    if named is not None and not (modal and 0x20 < ch < 0x7F and ch not in NAV_CHARS):
        return {"kind": KIND_KEY, "arg": [named, 0, 0, 0, 0, 0, 0, 0]}, f"Key({keys[named][0]})"
    return {"kind": KIND_KEY, "arg": [0, ch, vk, 0, 0, 0, 0, 0]}, f"Key(Char {_char_text(ch)} vk 0x{vk:02x})"


def translate_batch(b: TC.Batch, keys: dict, named_chars: bool = False) -> tuple[list, list, list, str]:
    """(commands, texts, gaps, category) for one recorded dispatch."""
    modal = b.context not in BODY_CONTEXTS
    cs = b.commands
    cmds, texts, gaps, cats = [], [], [], []
    by_ch = {c: i for i, (_, v, c) in keys.items() if c}
    i = 0
    while i < len(cs):
        c = cs[i]
        h = c.handler
        if h == 9:
            texts.append("release (no command)")
            cats.append("key-release")
            i += 1
        elif h == 8:
            vk = c.a0
            j = i + 1
            chs = []
            while j < len(cs) and cs[j].handler == 7 and cs[j].a1 == vk:
                chs.append(cs[j].a0)
                j += 1
            if not chs or c.a1 != 0:
                gaps.append(f"onChar({c.a0},{c.a1}) with no WM_CHAR of its key")
                i += 1
                continue
            cmd, t = _key_cmd(keys, vk, chs[-1], modal)
            cmds.append(cmd)
            texts.append(t)
            cats.append("key")
            i = j
        elif h == 7:
            ch, code = c.a0, c.a1
            if ch == 0:
                cmd, t = _key_cmd(keys, code, 0, modal)
                cmds.append(cmd)
                texts.append(t)
                cats.append("key")
            elif named_chars and ch in by_ch:
                k = by_ch[ch]
                cmds.append({"kind": KIND_KEY, "arg": [k, 0, 0, 0, 0, 0, 0, 0]})
                texts.append(f"Key({keys[k][0]}) [typed with no key-down, code 0x{code:02x}]")
                cats.append("key-char-named")
            else:
                cmds.append({"kind": KIND_KEY, "arg": [0, ch, 0, code, 0, 0, 0, 0]})
                act = f", H1: {keys[by_ch[ch]][0]}" if ch in by_ch else ""
                texts.append(f"Key(Char {_char_text(ch)} with no key-down, code 0x{code:02x}{act})")
                cats.append("key-char")
            i += 1
        elif h in (2, 5):
            btn = 1 if h == 2 else 2
            cmds.append({"kind": KIND_DOWN, "arg": [c.a0, c.a1, btn, 0, 0, 0, 0, 0]})
            texts.append(f"PointerDown({c.a0},{c.a1},{'L' if btn == 1 else 'R'})")
            cats.append("pointer-down")
            i += 1
        elif h in (1, 4):
            btn = 1 if h == 1 else 2
            up = 3 if h == 1 else 6
            m = _mods(c)
            if i + 1 >= len(cs) or cs[i + 1].handler != up or m is None:
                gaps.append(f"press {h}({c.a0},{c.a1}) without its unpoint or with key state "
                            f"0x{c.async_ctrl:x}/0x{c.async_shift:x}")
                i += 1
                continue
            cmds.append({"kind": KIND_CLICK, "arg": [c.a0, c.a1, btn, m, 0, -1, -1, 0], "modal": modal})
            mt = f",mods={m}" if m else ""
            texts.append(f"Click({c.a0},{c.a1},{'L' if btn == 1 else 'R'}{mt})")
            cats.append("click-ui" if modal else "click")
            i += 2
        else:
            gaps.append(f"{TF.HANDLERS.get(h, h)}({c.a0},{c.a1}) has no semantic kind")
            i += 1
    cat = "+".join(dict.fromkeys(cats)) if cats else "gap"
    return cmds, texts, gaps, cat


def xlate(segname: str, named_chars: bool = False) -> list[XRow]:
    keys = key_table()
    seg = seg_of(segname)
    rows = []
    for i, b in enumerate(TC.batches(TF.parse(seg.trace))):
        names, args = describe(b)
        cmds, texts, gaps, cat = translate_batch(b, keys, named_chars)
        rows.append(XRow(seg.name, i, b.frame, b.context, b.commands[0].pos, names, args, cat, "; ".join(texts),
                         "; ".join(gaps), cmds))
    return rows


def _overrides(rows: list[XRow]) -> list[tuple[int, list[dict]]]:
    return [(r.pos, [{"kind": c["kind"], "arg": c["arg"]} for c in r.commands]) for r in rows]


def run_input(spec: I.LibSpec, segname: str, rows: list[XRow], log: bool = True,
              negctl: dict | None = None) -> tuple[dict, list]:
    """sg_run_frames of the segment with EVERY recorded batch replaced by its translation (no RootHandler command reaches
    the port from the trace); returns the RunResult dict and the semantic module's click log
    [(x, y, button, target, a, b, tile_x, tile_y, stated, kind)]."""
    lib = I.load_lib(spec)
    seg = seg_of(segname)
    table = _overrides(rows)
    blob = pack_overrides(table)
    got = {}

    def prepare(st):
        for name, mode in (negctl or {}).items():
            lib.negctl(name, mode)
        buf = (C.c_uint8 * max(16, len(blob))).from_buffer_copy(blob + b"\0" * max(0, 16 - len(blob)))
        st.map(BLOCK_VA, buf, readonly=True)
        n = st.call("integrator.setBatchOverrides", [BLOCK_VA, len(table)]).eax
        st.unmap_all()
        if n != len(table):
            raise RuntimeError(f"setBatchOverrides took {n} of {len(table)}")
        st.call("semantic.logClear", [])
        st.call("semantic.logOn", [1 if log else 0])

    def after(st, res):
        got["applied"] = st.call("integrator.appliedCount", []).eax
        n = st.call("semantic.logCount", []).eax
        got["log"] = [tuple(_s32(st.call("semantic.logGet", [k, f]).eax) for f in range(10)) for k in range(n)]
        st.call("semantic.logOn", [0])
        st.call("semantic.logClear", [])
        st.call("integrator.clearBatchOverrides", [])
        if negctl:
            lib.negctl_reset()

    end = list(seg.checkpoints.values())[-1]["hashes"] if seg.checkpoints else None
    res = I.run_from(lib, segname, seg, seg.trace, seg.records, end, prepare=prepare, after=after)
    d = dataclasses.asdict(res)
    d.update(batches=len(rows), applied=got.get("applied"), commands=sum(len(r.commands) for r in rows),
             gaps=sum(1 for r in rows if r.gap), root_handler_commands=0)
    return d, got.get("log", [])


def assign_targets(rows: list[XRow], log: list) -> int:
    """States the probe run's routes as the targets of the frame-body clicks (in delivery order); returns how many clicks
    got no log record (a probe that stopped before them)."""
    k = 0
    missing = 0
    for r in rows:
        for c in r.commands:
            if c["kind"] != KIND_CLICK:
                continue
            if k >= len(log):
                missing += 1
                continue
            x, y, btn, tgt, a, b, tx, ty, _, _ = log[k]
            k += 1
            if (x, y, btn) != tuple(c["arg"][:3]):
                raise RuntimeError(f"{r.segment} batch {r.batch}: click log out of step ({x},{y},{btn}) vs {c['arg'][:3]}")
            if c.get("modal"):
                continue
            arg = list(c["arg"])
            arg[4] = tgt
            arg[5], arg[6] = -1, -1
            label = TARGET_NAMES.get(tgt, str(tgt))
            if tgt in (1, 4):
                arg[5], arg[6] = tx, ty
                label += f" tile ({tx},{ty})"
            elif tgt == 2:
                arg[5], arg[6] = a, b
                label += f" {KIND_NAMES.get(a, hex(a))} control {b}"
            elif tgt in (5, 6, 7):
                arg[5] = a
                label += f" {a}"
            c["arg"] = arg
            r.translation = r.translation.replace(f"Click({x},{y},", f"Click[{label}]({x},{y},", 1)
    return missing


def gate_segment(spec: I.LibSpec, segname: str, named_chars: bool = False, negctl: dict | None = None) -> dict:
    """Gate (d) for one segment: translate, probe run (targets unstated), final run (targets stated and checked)."""
    rows = xlate(segname, named_chars)
    probe, log = run_input(spec, segname, rows, log=True)
    missing = assign_targets(rows, log)
    final, _ = run_input(spec, segname, rows, log=False, negctl=negctl)
    cats = {}
    for r in rows:
        cats[r.category] = cats.get(r.category, 0) + 1
    return {"name": segname, "probe": probe["status"], "status": final["status"], "frames_run": final["frames_run"],
            "frames_expected": final["frames_expected"], "record": final["record"], "objects": final["objects"],
            "noncausal": final["noncausal"], "batches": len(rows), "commands": final["commands"], "gaps": final["gaps"],
            "applied": final["applied"], "clicks_unrouted": missing, "categories": json.dumps(cats, sort_keys=True),
            "digest": final["digest"], "detail": final["detail"], "run_detail": final["run_detail"],
            "seconds": round(probe["seconds"] + final["seconds"], 2), "rows": rows}


GATE_COLS = ["name", "status", "probe", "frames_run", "frames_expected", "record", "objects", "noncausal", "batches",
             "commands", "gaps", "applied", "clicks_unrouted", "categories", "digest", "seconds", "detail", "run_detail"]
XLATE_COLS = ["segment", "batch", "frame", "context", "pos", "handlers", "args", "category", "translation", "gap"]


def _gate_worker(args):
    spec, name, named_chars, negctl = args
    d = gate_segment(spec, name, named_chars, negctl)
    d["rows"] = [dataclasses.asdict(r) for r in d["rows"]]
    return d


def cmd_xlate(a) -> int:
    allrows = []
    for sg in a.segment:
        rows = xlate(sg, a.named_chars)
        allrows += rows
        gaps = [r for r in rows if r.gap]
        print(f"{sg}: {len(rows)} batches, {sum(len(r.commands) for r in rows)} semantic commands, {len(gaps)} gaps",
              flush=True)
    out = pathlib.Path(a.out or OUT38I / "xlate.tsv")
    I.write_tsv(out, [dataclasses.asdict(r) for r in allrows], XLATE_COLS)
    print(f"-> {out}")
    return 0


def cmd_gate(a) -> int:
    import concurrent.futures as CF
    import time
    spec = I.LibSpec.from_args(a)
    names = [I.seg_name(r) for r in I.accepted_rows("COMMAND")] if a.all else a.segment
    negctl = {k: int(v) for k, v in (x.split("=") for x in a.negctl)} if a.negctl else None
    out = pathlib.Path(a.out or OUT38I / "gate.tsv")
    print(f"library: {spec.describe()}; {len(names)} segments -> {out}", flush=True)
    t0 = time.time()
    res, xrows = [], []
    with CF.ProcessPoolExecutor(a.jobs) as ex:
        futs = {ex.submit(_gate_worker, (spec, n, a.named_chars, negctl)): n for n in names}
        for i, f in enumerate(CF.as_completed(futs), 1):
            try:
                d = f.result()
            except Exception as e:  # noqa: BLE001
                d = {"name": futs[f], "status": "ERROR", "detail": repr(e)[:300], "rows": []}
            xrows += d.pop("rows")
            res.append(d)
            print(f"[{i}/{len(names)}] {d['name']} {d['status']} probe={d.get('probe')} "
                  f"{d.get('frames_run')}/{d.get('frames_expected')} rec={d.get('record')} "
                  f"gaps={d.get('gaps')} cmds={d.get('commands')} {str(d.get('objects', ''))[:60]} "
                  f"{d.get('seconds')}s | {str(d.get('run_detail', ''))[:100]}", flush=True)
    res.sort(key=lambda d: d["name"])
    xrows.sort(key=lambda r: (r["segment"], r["batch"]))
    I.write_tsv(out, res, GATE_COLS)
    I.write_tsv(out.with_name(out.stem + "_xlate.tsv"), xrows, XLATE_COLS)
    counts = {}
    for d in res:
        counts[d["status"]] = counts.get(d["status"], 0) + 1
    print(json.dumps({"counts": counts, "wall_s": round(time.time() - t0, 1)}), flush=True)
    return 0


# ------------------------------------------------------------------------------------------- commands
def cmd_translate(a) -> int:
    spec = I.LibSpec.from_args(a)
    allrows = []
    for sg in a.segment:
        rows = translate(spec, sg)
        allrows += rows
        tr = sum(1 for r in rows if r.semantic)
        gaps = [r for r in rows if not r.translation]
        print(f"{sg}: {len(rows)} batches, {tr} translated, {sum(1 for r in rows if r.translation.startswith('dropped'))} "
              f"mouse-downs of translated presses, {len(gaps)} gaps", flush=True)
    out = pathlib.Path(a.out or OUT / "semantic_translation.tsv")
    cols = ["segment", "batch", "frame", "context", "pos", "handlers", "args", "category", "translation"]
    I.write_tsv(out, [dataclasses.asdict(r) for r in allrows], cols)
    (out.with_suffix(".json")).write_text(json.dumps([dataclasses.asdict(r) for r in allrows], indent=0))
    print(f"-> {out}")
    return 0


def cmd_run(a) -> int:
    spec = I.LibSpec.from_args(a)
    src = pathlib.Path(a.translation or OUT / "semantic_translation.json")
    allrows = [Row(**r) for r in json.loads(src.read_text())] if src.exists() else []
    res = []
    for sg in a.segment:
        rows = [r for r in allrows if r.segment == seg_of(sg).name] or translate(spec, sg)
        for mode in a.mode:
            d = run_mode(spec, sg, rows, mode)
            res.append(d)
            print(f"{sg} {mode}: {d['status']} {d['frames_run']}/{d['frames_expected']} rec={d['record']} "
                  f"objects={d['objects'][:120]} semantic={d['semantic_commands']} overrides={d['overrides']} "
                  f"applied={d['applied']} raw_left={d['root_handler_commands_left']} | {d['run_detail'][:160]}", flush=True)
    out = pathlib.Path(a.out or OUT / "semantic_runs.tsv")
    cols = ["name", "mode", "status", "frames_run", "frames_expected", "record", "objects", "semantic_commands", "overrides",
            "applied", "root_handler_commands_left", "digest", "run_detail", "detail"]
    I.write_tsv(out, res, cols)
    print(f"-> {out}")
    return 0


def main(argv=None) -> int:
    if argv is None and len(sys.argv) > 1 and sys.argv[1] == "_dump":
        _dump_child(*sys.argv[2:6])
        return 0
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    for name in ("translate", "run", "diff", "xlate", "gate"):
        p = sp.add_parser(name)
        p.add_argument("--segment", nargs="+", default=["championship/00", "championship/01", "championship/02"])
        p.add_argument("--backend", choices=["x64", "win32"], default="x64")
        p.add_argument("--dll")
        p.add_argument("--dll32")
        p.add_argument("--runner32")
        p.add_argument("--build")
        p.add_argument("--out")
        if name in ("xlate", "gate"):
            p.add_argument("--named-chars", action="store_true",
                           help="a character typed with no key-down as the key action of that character")
        if name == "gate":
            p.add_argument("--all", action="store_true", help="every accepted COMMAND segment")
            p.add_argument("--jobs", type=int, default=1)
            p.add_argument("--negctl", nargs="*", help="name=mode negative controls for the final runs")
        if name in ("run", "diff"):
            p.add_argument("--mode", nargs="+", default=["raw", "hybrid", "semantic"])
            p.add_argument("--translation")
        if name == "diff":
            p.add_argument("--frames", type=int, required=True)
    a = ap.parse_args(argv)
    if not (a.dll or a.build):
        a.dll = os.environ.get("SG_PORTSIM_DLL") or str(L.B.dll_path("x64"))
    return {"translate": cmd_translate, "run": cmd_run, "diff": cmd_diff, "xlate": cmd_xlate,
            "gate": cmd_gate}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
