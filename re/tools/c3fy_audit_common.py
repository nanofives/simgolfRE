r"""Shared parsing for the c3fy audits (re/tools/c3fy_audit_*.py, batch c3fyc 2026-10-16): mainLoop 0x0040f5c0's
listing with frame-normalised operands and the hand transcription shim/src/re/c3fy.cpp.

Listing: log/c3/alt/c3fy_norm.asm (log/c3/alt/c3fy_espnorm.py: every `[esp+X]` rewritten as `[F+X']`, F = esp after the
prologue's `push edi` 0x0040f5e1; the second column is the push depth before the instruction, from a CFG walk). Lines:
`<va>  <depth>  <mnemonic> <operands>` (the prologue lines carry no depth).

Source: every line of c3fy.cpp with the addresses it cites (`0x00xxxxxx` in its `//` comment, inside the body range). A
code line without a citation inherits the citations of the comment-only lines directly above it (the transcription
writes `// callee(args)  0x00420e45` above the call it describes).
"""
from __future__ import annotations

import bisect
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))

LO, HI = 0x0040F5C0, 0x00421616
LISTING = ROOT / "log" / "c3" / "alt" / "c3fy_norm.asm"
SOURCE = ROOT / "shim" / "src" / "re" / "c3fy.cpp"

_LINE = re.compile(r"^([0-9a-f]{8})\s+(?:([+-]?[0-9a-f]+)\s+)?(\S+)\s*(.*)$")


class Ins:
    __slots__ = ("va", "depth", "mn", "ops", "text", "k")

    def __init__(self, va, depth, mn, ops, k):
        self.va, self.depth, self.mn, self.ops, self.k = va, depth, mn, ops, k
        self.text = f"{mn} {ops}".strip()

    def __repr__(self):
        return f"{self.va:08x} {self.text}"


def load_listing(path: pathlib.Path = LISTING) -> list[Ins]:
    out = []
    for line in path.read_text().splitlines():
        m = _LINE.match(line.strip())
        if not m:
            continue
        va, depth, mn, ops = m.groups()
        # `rep movsd ...` / `repne scasb ...`: keep the prefix in the mnemonic
        if mn in ("rep", "repne", "repe") and ops:
            p, _, rest = ops.partition(" ")
            mn, ops = f"{mn} {p}", rest
        d = None
        if depth is not None:
            d = int(depth, 16) if not depth.startswith("-") else -int(depth[1:], 16)
        if d is not None and "[esp + " in ops:
            # c3fy_espnorm.py left the 5 indexed esp operands (`[esp + esi*2 + 0x192]`) as they were: F + (X - depth)
            ops = re.sub(r"\[esp \+ ([a-z]{3}(?:\*\d)?) \+ 0x([0-9a-f]+)\]",
                         lambda m: f"[F+{m.group(1)}+{int(m.group(2), 16) - d:#x}]", ops)
        out.append(Ins(int(va, 16), d, mn, ops.strip(), len(out)))
    return out


class Listing:
    def __init__(self, path: pathlib.Path = LISTING):
        self.ins = load_listing(path)
        self.by_va = {i.va: i for i in self.ins}
        self.vas = [i.va for i in self.ins]

    def at_or_after(self, va: int) -> Ins | None:
        k = bisect.bisect_left(self.vas, va)
        return self.ins[k] if k < len(self.ins) else None

    def containing(self, va: int) -> Ins | None:
        k = bisect.bisect_right(self.vas, va) - 1
        return self.ins[k] if k >= 0 else None

    def window(self, lo: int, hi: int) -> list[Ins]:
        a = bisect.bisect_left(self.vas, lo)
        b = bisect.bisect_right(self.vas, hi)
        return self.ins[a:b]


# ------------------------------------------------------------------ the binary (callee conventions, tables)
_pe = None


def pe():
    global _pe
    if _pe is None:
        import xref
        _pe = xref.load_pe("golf_clean.exe")
    return _pe


def read(va: int, n: int) -> bytes:
    p, base = pe()
    return p.get_data(va - base, n)


def u32(va: int) -> int:
    return struct.unpack("<I", read(va, 4))[0]


_retcache: dict[int, int | None] = {}


def ret_pop(va: int) -> int | None:
    """Bytes the callee pops (`ret N`), following a leading `jmp` thunk; None when no `ret` is found in 0x4000 bytes."""
    if va in _retcache:
        return _retcache[va]
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    res, seen = None, 0
    for i in md.disasm(read(va, 0x4000), va):
        if i.mnemonic == "jmp" and i.op_str.startswith("0x"):
            # a leading thunk or a tail jump (e.g. 0x00483016 `jmp 0x483070`): the callee returns through the target
            _retcache[va] = None
            res = ret_pop(int(i.op_str, 16))
            break
        if i.mnemonic == "ret":
            res = int(i.op_str, 16) if i.op_str else 0
            break
        seen += 1
    _retcache[va] = res
    return res


# ------------------------------------------------------------------ the source
_CITE = re.compile(r"0x([0-9a-fA-F]{6,8})\b")


class SrcLine:
    __slots__ = ("n", "text", "code", "comment", "cites", "own")

    def __init__(self, n, text):
        self.n, self.text = n, text
        code, sep, com = text.partition("//")
        self.code, self.comment = code, com if sep else ""
        self.own = [c for c in (int(x, 16) for x in _CITE.findall(self.comment)) if LO <= c < HI]
        self.cites = list(self.own)


def load_source(path: pathlib.Path = SOURCE) -> list[SrcLine]:
    lines = [SrcLine(n, t) for n, t in enumerate(path.read_text(encoding="utf-8", errors="replace").split("\n"), 1)]
    for k, ln in enumerate(lines):
        if ln.code.strip() and not ln.own:
            # inherit from comment-only lines directly above (up to 4)
            j = k - 1
            inh = []
            while j >= 0 and k - j <= 4 and not lines[j].code.strip() and lines[j].comment:
                inh = lines[j].own + inh
                j -= 1
            ln.cites = inh
    return lines


def split_args(s: str, start: int) -> tuple[list[str], int]:
    """s[start] == '(' -> (top-level comma-separated arguments, index after the matching ')')."""
    depth, cur, out, i = 0, "", [], start
    while i < len(s):
        ch = s[i]
        if ch == "(":
            depth += 1
            if depth > 1:
                cur += ch
        elif ch == ")":
            depth -= 1
            if depth == 0:
                if cur.strip():
                    out.append(cur.strip())
                return out, i + 1
            cur += ch
        elif ch == "," and depth == 1:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
        i += 1
    return out, i


def strip_casts(e: str) -> str:
    e = e.strip()
    while True:
        m = re.match(r"^\((?:int|u32|unsigned|short|u16|u8|s8|char|DWORD)\)\s*(.*)$", e)
        if not m:
            break
        e = m.group(1).strip()
    while e.startswith("(") and e.endswith(")"):
        inner, end = split_args(e, 0)
        if end != len(e) or len(inner) != 1:
            break
        e = inner[0]
    return e


def int_literal(e: str) -> int | None:
    e = strip_casts(e)
    m = re.fullmatch(r"(-)?\s*(0[xX][0-9a-fA-F]+|\d+)[uUlL]*", e)
    if not m:
        return None
    v = int(m.group(2), 0)
    if m.group(1):
        v = -v
    return v & 0xFFFFFFFF


def body_range_lines(src: list[SrcLine]) -> list[SrcLine]:
    """Lines of real code (skip the naked entry's asm and the negctl selector)."""
    return [l for l in src if l.code.strip()]


# ------------------------------------------------------------------ control flow over the listing
CFG_JSON = ROOT / "log" / "c3" / "alt" / "c3fy_cfg.json"


def tables() -> dict[int, dict]:
    """switch tables of the body (log/c3/alt/c3fy_cfg.py): table va -> {site, n, targets}."""
    import json
    d = json.loads(CFG_JSON.read_text())
    return {int(k, 16): v for k, v in d["tables"].items()}


def is_uncond_end(i: Ins) -> bool:
    return i.mn in ("jmp", "ret") or (i.mn == "call" and False)


def preds(L: Listing) -> dict[int, list[int]]:
    """instruction va -> predecessor instruction vas (fallthrough, direct jumps, switch table sites)."""
    out: dict[int, list[int]] = {}
    for k, i in enumerate(L.ins):
        if k + 1 < len(L.ins) and not is_uncond_end(i):
            out.setdefault(L.ins[k + 1].va, []).append(i.va)
        if (i.mn == "jmp" or i.mn.startswith("j")) and re.fullmatch(r"0x[0-9a-f]+", i.ops):
            out.setdefault(int(i.ops, 16), []).append(i.va)
    for t in tables().values():
        for tg in t["targets"]:
            out.setdefault(tg, []).append(t["site"])
    return out


def back_path(L: Listing, P: dict[int, list[int]], k: int, hints: list[int], limit: int = 80) -> list[Ins]:
    """Instructions executed before L.ins[k], newest first, along one path: at a join the predecessor nearest to a hint
    address (the citations of the C statement and the statements before it) is taken; fallthrough when no hint is
    nearer."""
    out = []
    va = L.ins[k].va
    seen = set()
    while len(out) < limit:
        ps = P.get(va, [])
        if not ps:
            break
        if len(ps) == 1:
            pv = ps[0]
        else:
            if hints:
                pv = min(ps, key=lambda p: min(abs(p - h) for h in hints))
            else:
                pv = max(ps)
        if pv in seen:
            break
        seen.add(pv)
        out.append(L.by_va[pv])
        va = pv
    return out
