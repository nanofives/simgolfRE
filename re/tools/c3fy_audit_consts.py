r"""c3fy audit (b), batch c3fyc: every immediate, global address and frame offset written in shim/src/re/c3fy.cpp
against the instructions of mainLoop 0x0040f5c0 near the addresses the line cites.

For each code line with citations, the window is [min(cites) - 0x40, max(cites) + 0x20] of the listing
(log/c3/alt/c3fy_norm.asm). From the window: every immediate operand, every absolute address `[0x..]`, every
displacement `[reg (*k) + 0x..]`, every frame offset `[F+0x..]`. From the line: every literal outside FA/FD/FU/FW/FUW/FB/
FUB (`value`), and every frame offset inside them (`frame`). Checks:
  GLOBAL   a literal >= 0x00400000 (code / data address) with no window address or displacement within 0x40 below it
           ... or above it by at most 0x40 (struct field arithmetic `base + k`), nor an immediate equal to it
  VALUE    a literal 0x10 <= v < 0x00400000 that is neither an immediate of the window (v, -v, v - 1, v + 1, ~v,
           the 8/16-bit truncations), nor a displacement, nor a frame offset, nor a power of two whose log2 is a shift
           count in the window, nor a multiplier a `lea`/`imul` chain produces (3, 5, 9 and their products up to 0x400)
  FRAME    an F-slot offset of the line that no instruction of the window addresses (`[F+X]`, `lea r, [F+X]`), with the
           same widened window
Literals below 0x10 are not checked (indices, shifts and flags the compiler folds). Every report is a reading aid: the
compiler folds, hoists and merges, so each one was checked by hand (log/c3/c3fyc_notes.txt).

    py -3.12 re/tools/c3fy_audit_consts.py [--out log/c3/c3fyc_audit_consts.txt] [--pad 0x40]
"""
from __future__ import annotations

import argparse
import collections
import re
import sys

import c3fy_audit_common as A

LIT = re.compile(r"(?<![\w.])(0[xX][0-9a-fA-F]+|\d+)[uUlL]*(?![\w.])")
FSLOT = re.compile(r"\bF(?:D|U|W|UW|B|UB|A)\((0[xX][0-9a-fA-F]+|\d+)")
ADDR_IN_OP = re.compile(r"\[(?:[a-z]{2,3}(?:\*\d)?\s*\+\s*)*(?:[a-z]{2,3}(?:\*\d)?\s*[+-]\s*)?(-?0x[0-9a-f]+)\]")


def window_facts(ins: list[A.Ins]):
    imms, addrs, frames, shifts = set(), set(), set(), set()
    for i in ins:
        ops = i.ops
        for m in re.finditer(r"\[F\+(?:[a-z]{3}\*\d\+)?0x([0-9a-f]+)\]", ops):
            frames.add(int(m.group(1), 16))
        for m in re.finditer(r"\[([^\]]*)\]", ops):
            inner = m.group(1)
            if inner.startswith("F+"):
                continue
            for d in re.findall(r"(?:^|[+-]\s*)(0x[0-9a-f]+)", inner):
                v = int(d, 16)
                addrs.add(v)
                neg = re.search(r"-\s*" + d, inner)
                if neg:
                    addrs.add((-v) & 0xFFFFFFFF)
        outside = re.sub(r"\[[^\]]*\]", "", ops)
        for d in re.findall(r"(?<![\w])(-?0x[0-9a-f]+|-?\d+)(?![\w])", outside):
            v = int(d, 0) & 0xFFFFFFFF
            imms.add(v)
            if i.mn in ("shl", "sar", "shr", "sal"):
                shifts.add(v)
            if i.mn in ("call", "jmp") or i.mn.startswith("j"):
                addrs.add(v)
    return imms, addrs, frames, shifts


MULTS = set()
for a in (1, 2, 3, 4, 5, 8, 9):
    for b in (1, 2, 3, 4, 5, 8, 9):
        for c in (1, 2, 3, 4, 5, 8, 9):
            MULTS.add(a * b * c)


def value_ok(v: int, imms, addrs, frames, shifts) -> bool:
    cands = {v, (-v) & 0xFFFFFFFF, (v - 1) & 0xFFFFFFFF, (v + 1) & 0xFFFFFFFF, (~v) & 0xFFFFFFFF, v & 0xFF, v & 0xFFFF}
    if any(x in imms or x in addrs for x in cands):
        return True
    if v in frames:
        return True
    if v & (v - 1) == 0 and v.bit_length() - 1 in shifts:
        return True
    if v in MULTS:
        return True
    # sign-extended byte / word immediates
    for x in imms:
        if (x & 0xFF) == (v & 0xFF) and v < 0x100:
            return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(A.ROOT / "log" / "c3" / "c3fyc_audit_consts.txt"))
    ap.add_argument("--pad", default="0x40")
    a = ap.parse_args()
    pad = int(a.pad, 0)
    L = A.Listing()
    src = A.load_source()
    rep = collections.defaultdict(list)
    st = collections.Counter()
    for ln in src:
        code = ln.code
        if not code.strip() or not ln.cites or re.match(r"\s*(#|template|static inline)", code):
            continue
        lo, hi = min(ln.cites) - pad, max(ln.cites) + 0x20
        win = L.window(lo, hi)
        imms, addrs, frames, shifts = window_facts(win)
        # frame slots
        fspans = []
        for m in FSLOT.finditer(code):
            fo = int(m.group(1), 0)
            fspans.append((m.start(), m.end()))
            st["frame_checked"] += 1
            if fo not in frames:
                big = L.window(min(ln.cites) - 0x200, max(ln.cites) + 0x100)
                if fo not in window_facts(big)[2]:
                    rep["FRAME"].append(f"line {ln.n}: F+{fo:#x} not addressed near {[hex(c) for c in ln.cites][:4]}: "
                                        f"{code.strip()[:150]}")
        for m in LIT.finditer(code):
            if any(s <= m.start() < e for s, e in fspans):
                continue
            v = int(m.group(1), 0) & 0xFFFFFFFF
            before, after = code[:m.start()].rstrip(), code[m.end():].lstrip()
            arith = (before.endswith("*") or after.startswith("*") or before.endswith("/") or before.endswith("%")
                     or before.endswith(">>") or before.endswith("<<"))
            mask = v in (0xFF, 0xFFFF, 0xFFFF0000, 0xFFFFFF00, 0xFFFFFFFF) or (before.endswith("&") and v > 0xFFF)
            if arith or mask:
                st["arith_or_mask"] += 1
                if v < 0x00400000:
                    big = L.window(min(ln.cites) - 0x200, max(ln.cites) + 0x100)
                    if any(i.mn in ("lea", "shl", "sar", "shr", "imul", "idiv", "div", "and", "movzx", "movsx", "cdq")
                           or i.ops.startswith(("cx,", "ax,", "dx,", "si,", "di,", "bx,"))
                           for i in big):
                        continue

            # a negative literal written `-0x10`
            if v < 0x10:
                continue
            if v >= 0x00400000 and v < 0x01000000:
                st["global_checked"] += 1
                ok = v in imms or any(0 <= v - d <= 0x40 or 0 <= d - v <= 0x40 for d in addrs)
                if not ok:
                    big = L.window(min(ln.cites) - 0x200, max(ln.cites) + 0x100)
                    bi, ba, _, _ = window_facts(big)
                    tag = "GLOBAL" if not (v in bi or any(abs(v - d) <= 0x40 for d in ba)) else "GLOBAL~"
                    rep[tag].append(f"line {ln.n}: {v:#010x} not in the window {lo:#x}..{hi:#x}: {code.strip()[:150]}")
            else:
                st["value_checked"] += 1
                if not value_ok(v, imms, addrs, frames, shifts):
                    big = L.window(min(ln.cites) - 0x200, max(ln.cites) + 0x100)
                    bi, ba, bf, bs = window_facts(big)
                    tag = "VALUE~" if value_ok(v, bi, ba, bf, bs) else "VALUE"
                    rep[tag].append(f"line {ln.n}: {v:#x} not an immediate of the window {lo:#x}..{hi:#x}: "
                                    f"{code.strip()[:150]}")
    head = f"c3fy audit (b) constants: {dict(st)}"
    lines = [head]
    for k in ("GLOBAL", "VALUE", "FRAME", "GLOBAL~", "VALUE~"):
        lines.append(f"== {k}: {len(rep[k])}")
        lines += rep[k]
        lines.append("")
    A.pathlib.Path(a.out).write_text("\n".join(lines) + "\n")
    print(head)
    print("\n".join(x for x in lines if x.startswith("==")))


if __name__ == "__main__":
    sys.exit(main())
