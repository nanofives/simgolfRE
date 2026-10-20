"""Raw input of a recorded trace as portsim Commands (phase 3.7b; portsim/sim/commands.h).

Every HANDLER record of a trace (re/model/trace_schema.md, type 0x09) is one root-window handler call of the original: it
becomes exactly one Command{RootHandler, handler, a0, a1}, with
  * async_ctrl / async_shift: the GetAsyncKeyState(VK_CONTROL / VK_SHIFT) results (KEYSTATE records, api 2) the press
    handlers 1 / 4 read inside themselves (ra 0x004655c9 / 0x004655dd .. 0x00465700),
  * t: the timeGetTime value the handler's deadline tail read (the CLOCKRUN inside handlers 2 / 3 / 7).
Commands are grouped into BATCHES, one per dispatch (one pump call that delivered messages): consecutive handler records
with no clock call between them other than the handlers' own. Each batch carries the frame it was dispatched in (the last
FRAME record before it) and the CONTEXT: the call site of the last clock read outside the handlers before it, which tells
which pump dispatched it (time_model.md section 7): the frame's H1 pump (frame head 0x0040fc2f / jgld Palette::update
QPC), pumpAndRedraw 0x0045c0cc / 0x0045c0e5, waitTicks 0x0045bfb3 / 0x0045bfb7 / 0x0045bfe3, a screen's own loop.
This is the mapping a trace-driven InputSource needs; it uses the recorded facts only.
"""
from __future__ import annotations

import dataclasses
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF  # noqa: E402

HANDLER_LO, HANDLER_HI = 0x00465580, 0x004658A0     # rootWin_onPress1 .. past rootWin_onPress2b
UIKEY_LO, UIKEY_HI = 0x00495910, 0x00495A00        # uiKeyEvent (GetKeyState for every dispatched key)
DISPATCH_LO, DISPATCH_HI = 0x0047C740, 0x0047C900  # Window::dispatchAccel (MapVirtualKeyA)
CONTEXTS = {0x0040FC2F: "frame-head", 0x0045C0CC: "pumpAndRedraw", 0x0045C0E5: "pumpAndRedraw",
            0x0045BFB3: "waitTicks", 0x0045BFB7: "waitTicks", 0x0045BFE3: "waitTicks",
            0x8106A414: "frame-palette"}   # jgld.dll Palette::update QPC (0x1006a40e): P1..P4, right before H1


@dataclasses.dataclass
class RawCmd:
    handler: int
    a0: int
    a1: int
    async_ctrl: int = 0
    async_shift: int = 0
    t: int = 0
    pos: int = 0

    def args(self):
        return [self.handler, self.a0 & 0xFFFFFFFF, self.a1 & 0xFFFFFFFF, self.async_ctrl, self.async_shift, self.t]


@dataclasses.dataclass
class Batch:
    frame: int
    context_ra: int
    commands: list

    @property
    def context(self) -> str:
        if self.context_ra in CONTEXTS:
            return CONTEXTS[self.context_ra]
        return "other:" + hex(self.context_ra)


def _inside_handler(ra: int) -> bool:
    return HANDLER_LO <= ra < HANDLER_HI or UIKEY_LO <= ra < UIKEY_HI or DISPATCH_LO <= ra < DISPATCH_HI


def batches(tr: TF.Trace) -> list[Batch]:
    out: list[Batch] = []
    frame, ctx, cur, open_batch = -1, 0, None, False
    for r in tr.records:
        f = r.f
        if r.type == TF.FRAME:
            frame, ctx, open_batch = f["frame"], TF.FRAME_RA, False
        elif r.type == TF.CLOCKRUN:
            if _inside_handler(f["ra"]):
                if cur is not None and cur.commands and cur.commands[-1].handler in (2, 3, 7) and f["kind"] == 1:
                    cur.commands[-1].t = f["v0"] & 0xFFFFFFFF
            else:
                ctx, open_batch = f["ra"], False
        elif r.type == TF.KEYSTATE:
            if f["api"] == 2 and HANDLER_LO <= f["ra"] < HANDLER_HI and cur is not None and cur.commands:
                c = cur.commands[-1]
                if f["vk"] == 0x11:
                    c.async_ctrl = f["result"] & 0xFFFF
                elif f["vk"] == 0x10:
                    c.async_shift = f["result"] & 0xFFFF
        elif r.type == TF.HANDLER:
            if not open_batch:
                cur = Batch(frame, ctx, [])
                out.append(cur)
                open_batch = True
            a0 = f["a0"] - (1 << 32) if f["a0"] & 0x80000000 else f["a0"]
            a1 = f["a1"] - (1 << 32) if f["a1"] & 0x80000000 else f["a1"]
            cur.commands.append(RawCmd(f["handler"], a0, a1, pos=f["pos"]))
    return out


def summary(tr: TF.Trace) -> dict:
    bs = batches(tr)
    by = {}
    for b in bs:
        by[b.context] = by.get(b.context, 0) + 1
    return {"batches": len(bs), "commands": sum(len(b.commands) for b in bs), "by_context": by,
            "handlers": {h: sum(1 for b in bs for c in b.commands if c.handler == h) for h in range(1, 12)}}
