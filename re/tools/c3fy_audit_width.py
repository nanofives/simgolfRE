r"""c3fy audit (a), batch c3fyc: operand width and signedness of the loads, stores and compares written in
shim/src/re/c3fy.cpp against mainLoop 0x0040f5c0's instructions.

Part 1, accesses. Every width macro with a literal base, G8 / GU8 / G16 / GU16 / G32 / GU32 (global) and FB / FUB / FW /
FUW / FD / FU (frame slot), on a line with citations, is paired with the instructions of the window [min(cites) - 0x40,
max(cites) + 0x20] whose memory operand has that base (`[0xBASE]`, `[reg + 0xBASE]`, `[reg*k + 0xBASE]`, `[F+0xBASE]`,
`[F+reg*k+0xBASE]`). Reports:
  WIDTH  no instruction of the window touches the base with the macro's width, but one touches it with another width
  SIGN   the macro is signed (G8 / G16 / FB / FW) and every load of the base in the window is `movzx`, or unsigned
         (GU8 / GU16 / FUB / FUW) and every load is `movsx`; and for `cmp byte|word ptr [base], imm` followed by a signed
         jcc (jl / jg / jle / jge) under an unsigned macro, or an unsigned jcc (jb / ja / jbe / jae) under a signed macro
  NONE   no instruction of the wider window [-0x200, +0x100] touches the base (citations out of place)
Part 2, compares. Every signed (jl / jle / jg / jge / setl..) and unsigned (jb / jbe / ja / jae / setb..) conditional of
the listing, paired with the source lines citing it or the compare before it: the source condition is unsigned when it
holds a `(u32)` / `(unsigned)` cast, a `u32` / `unsigned` / `DWORD` variable of the enclosing function, a GU32 / FU access
or a literal with a `u` suffix; else signed. Reports CMPSIGN on disagreement. GU8 / GU16 / FUB / FUW promote to int and
count as neutral (a zero-extended byte compares the same either way).

    py -3.12 re/tools/c3fy_audit_width.py [--out log/c3/c3fyc_audit_width.txt]
"""
from __future__ import annotations

import argparse
import collections
import re
import sys

import c3fy_audit_common as A

MACRO = re.compile(r"\b(GU8|GU16|GU32|G8|G16|G32|FUB|FUW|FB|FW|FD|FU)\(\s*\(?\s*(?:\(u32\)\s*)?(0x[0-9a-fA-F]+|\d+)\b")
WIDTH = {"G8": 1, "GU8": 1, "FB": 1, "FUB": 1, "G16": 2, "GU16": 2, "FW": 2, "FUW": 2, "G32": 4, "GU32": 4, "FD": 4, "FU": 4}
SIGNED = {"G8", "G16", "FB", "FW"}
UNSIGNED_SMALL = {"GU8", "GU16", "FUB", "FUW"}
PTRW = {"byte": 1, "word": 2, "dword": 4}
SIGNED_J = {"jl", "jle", "jg", "jge", "setl", "setle", "setg", "setge"}
UNSIGNED_J = {"jb", "jbe", "ja", "jae", "setb", "setbe", "seta", "setae", "adc", "sbb"}
REG8 = {"al", "ah", "bl", "bh", "cl", "ch", "dl", "dh"}
REG16 = {"ax", "bx", "cx", "dx", "si", "di", "bp", "sp"}


def mem_operands(i: A.Ins):
    """[(base, frame?, width or None)] of the instruction's memory operands."""
    out = []
    for m in re.finditer(r"(?:(byte|word|dword|qword) ptr )?\[([^\]]*)\]", i.ops):
        w = PTRW.get(m.group(1)) if m.group(1) else None
        inner = m.group(2)
        frame = inner.startswith("F+")
        d = re.findall(r"0x([0-9a-f]+)$", inner)
        if not d:
            continue
        base = int(d[0], 16)
        if w is None and i.mn == "lea":
            continue
        if w is None:
            # register operand gives the width (`mov al, [..]`)
            first = i.ops.split(",")[0].strip()
            w = 1 if first in REG8 else 2 if first in REG16 else 4
        out.append((base, frame, w))
    return out


def next_jcc(L: A.Listing, k: int):
    for q in range(k + 1, min(k + 4, len(L.ins))):
        mn = L.ins[q].mn
        if mn.startswith("j") and mn != "jmp" or mn.startswith("set"):
            return mn
        if mn in ("call", "jmp", "ret", "cmp", "test"):
            return None
    return None


def function_scopes(src: list[A.SrcLine]):
    """line index -> set of unsigned identifier names declared in its enclosing top-level function."""
    scopes = {}
    cur_unsigned, depth = set(), 0
    for k, ln in enumerate(src):
        code = ln.code
        if depth == 0:
            cur_unsigned = set()
            # parameters
            m = re.match(r"\s*(?:static\s+)?(?:__declspec\([^)]*\)\s*)?[\w*]+\s+(?:__\w+\s+)?\w+\(([^)]*)\)\s*\{?", code)
            if m:
                for p in m.group(1).split(","):
                    pm = re.match(r"\s*(?:const\s+)?(u32|unsigned|DWORD|unsigned int|u16|u8)\s+\*?\s*(\w+)", p)
                    if pm and pm.group(1) in ("u32", "unsigned", "DWORD", "unsigned int"):
                        cur_unsigned.add(pm.group(2))
        for dm in re.finditer(r"\b(?:const\s+)?(u32|unsigned|DWORD)\s+(\w+)\s*(?:=|;|,)", code):
            cur_unsigned.add(dm.group(2))
        depth += code.count("{") - code.count("}")
        scopes[k] = cur_unsigned
    return scopes


FUNC_START = re.compile(r"^(?:static\s+|__declspec\([^)]*\)\s*)*(?:void|int|u32|u8|u16|bool|unsigned|DWORD)\s+(?:__\w+\s+)?\w+\(")
DECL = r"\b(?:const\s+)?(u32|unsigned int|unsigned|DWORD|int|short|s8|u8|u16|char|bool|long long)\s+\*?\s*{name}\b"


def var_types(src: list[A.SrcLine], k: int, names: set[str]) -> dict[str, str]:
    """type of each name from its nearest declaration above line k inside the enclosing function (or its parameters)."""
    out = {}
    j = k
    while j >= 0 and names - set(out):
        text = src[j].code
        for n in names - set(out):
            m = re.search(DECL.format(name=re.escape(n)), text)
            if m:
                out[n] = m.group(1)
        if FUNC_START.match(src[j].text):
            break
        j -= 1
    return out


def cond_unsigned2(src, k, text: str) -> bool:
    t = text
    if re.search(r"\((?:u32|unsigned|DWORD|unsigned int)\)", t) or "static_cast<u32>" in t or "static_cast<unsigned" in t:
        return True
    if re.search(r"\b(GU32|FU)\(", t):
        return True
    if re.search(r"\b0x[0-9a-fA-F]+[uU]\b|\b\d+[uU]\b", t):
        return True
    names = set(re.findall(r"\b[a-z_]\w*\b", t)) - {"if", "for", "while", "return", "goto", "int", "u32", "else", "break",
                                                       "continue", "do", "const", "static_cast", "reinterpret_cast"}
    ty = var_types(src, k, names)
    return any(v in ("u32", "unsigned", "unsigned int", "DWORD") for v in ty.values())


def cond_unsigned(text: str, unsigned_names: set[str]) -> bool | None:
    t = text
    if re.search(r"\((?:u32|unsigned|DWORD|unsigned int)\)", t) or "static_cast<u32>" in t or "static_cast<unsigned" in t:
        return True
    if re.search(r"\b(GU32|FU)\(", t):
        return True
    if re.search(r"\b0x[0-9a-fA-F]+[uU]\b|\b\d+[uU]\b", t):
        return True
    names = set(re.findall(r"\b[A-Za-z_]\w*\b", t))
    if names & unsigned_names:
        return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(A.ROOT / "log" / "c3" / "c3fyc_audit_width.txt"))
    a = ap.parse_args()
    L = A.Listing()
    src = A.load_source()
    rep = collections.defaultdict(list)
    st = collections.Counter()
    # ---------------- part 1
    for ln in src:
        code = ln.code
        if not code.strip() or not ln.cites or re.match(r"\s*(#|template|static inline)", code):
            continue
        lo, hi = min(ln.cites) - 0x40, max(ln.cites) + 0x20
        win = L.window(lo, hi)
        for m in MACRO.finditer(code):
            mac, base = m.group(1), int(m.group(2), 0)
            frame = mac.startswith("F")
            if not frame and base < 0x400000:
                continue
            st["access"] += 1
            hits = [(i, w) for i in win for (b, fr, w) in mem_operands(i) if b == base and fr == frame]
            if not hits:
                big = L.window(min(ln.cites) - 0x200, max(ln.cites) + 0x100)
                hits = [(i, w) for i in big for (b, fr, w) in mem_operands(i) if b == base and fr == frame]
                if not hits:
                    rep["NONE"].append(f"line {ln.n}: {mac}({base:#x}) not addressed near {[hex(c) for c in ln.cites][:3]}: "
                                       f"{code.strip()[:140]}")
                    continue
            want = WIDTH[mac]
            widths = {w for _i, w in hits}
            if want not in widths:
                rep["WIDTH"].append(f"line {ln.n}: {mac}({base:#x}) is {want} byte(s), the window accesses it with "
                                    f"{sorted(widths)}: {', '.join(repr(i) for i, _ in hits[:3])} | {code.strip()[:120]}")
                continue
            st["width_ok"] += 1
            loads = [i for i, w in hits if w == want and i.mn in ("movsx", "movzx")]
            if want < 4 and loads:
                kinds = {i.mn for i in loads}
                if mac in SIGNED and kinds == {"movzx"}:
                    rep["SIGN"].append(f"line {ln.n}: {mac} (signed) but the window loads {base:#x} with movzx: "
                                       f"{loads[0]!r} | {code.strip()[:120]}")
                elif mac in UNSIGNED_SMALL and kinds == {"movsx"}:
                    rep["SIGN"].append(f"line {ln.n}: {mac} (unsigned) but the window loads {base:#x} with movsx: "
                                       f"{loads[0]!r} | {code.strip()[:120]}")
                else:
                    st["sign_ok"] += 1
            for i, w in hits:
                if i.mn == "cmp" and w == want and want < 4 and re.search(r",\s*-?0x[0-9a-f]+$|,\s*-?\d+$", i.ops):
                    j = next_jcc(L, i.k)
                    imm = int(i.ops.split(",")[1], 0)
                    if j in SIGNED_J and mac in UNSIGNED_SMALL:
                        rep["SIGN"].append(f"line {ln.n}: {mac} (unsigned) compared with a signed {j} after {i!r} | "
                                           f"{code.strip()[:120]}")
                    elif j in UNSIGNED_J and mac in SIGNED:
                        rep["SIGN"].append(f"line {ln.n}: {mac} (signed) compared with an unsigned {j} after {i!r} | "
                                           f"{code.strip()[:120]}")
                    elif j:
                        st["cmp_mem_ok"] += 1
    # ---------------- part 2
    scopes = function_scopes(src)
    by_cite = collections.defaultdict(list)
    for k, ln in enumerate(src):
        if ln.code.strip():
            for c in ln.cites:
                by_cite[c].append(k)
    for i in L.ins:
        kind = "s" if i.mn in SIGNED_J else "u" if i.mn in UNSIGNED_J and i.mn not in ("adc", "sbb") else None
        if kind is None:
            continue
        st["jcc"] += 1
        prev = L.ins[i.k - 1] if i.k else None
        lines = by_cite.get(i.va, []) or (by_cite.get(prev.va, []) if prev else [])
        if not lines:
            st["jcc_uncited"] += 1
            continue
        # skip switch bounds (`cmp r, N; ja default` before `jmp [r*4 + table]`)
        nxt = L.ins[i.k + 1] if i.k + 1 < len(L.ins) else None
        if nxt is not None and nxt.mn == "jmp" and "*4 +" in nxt.ops:
            st["jcc_switch_bound"] += 1
            continue
        k = lines[0]
        text = src[k].code
        has_cmp = re.search(r"[<>]=?|!=|==", text)
        if not has_cmp:
            st["jcc_no_relop"] += 1
            continue
        u = cond_unsigned2(src, k, text)
        # pointer walks: both sides addresses below 0x80000000 compare the same signed or unsigned
        lits = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{6,8})", text)]
        if lits and all(0x00400000 <= v < 0x01000000 for v in lits) and re.search(r"[<>]", text) and                 re.search(r"(?:p|rec|row|c|s|esi|ebx|edi|eax|ecx|edx|edx_tab|w2)\s*[<>]=?\s*0x", text):
            st["jcc_pointer_walk"] += 1
            continue
        st["jcc_checked"] += 1
        # a compare against 0 with jl / jge (sign test) is signed by construction; jb / jae after `cmp x, 0` never occur
        if kind == "u" and not u:
            rep["CMPSIGN"].append(f"{i.va:#010x} {i.text} after {prev.text if prev else ''}: source line {src[k].n} reads "
                                  f"signed: {text.strip()[:140]}")
        elif kind == "s" and u:
            rep["CMPSIGN"].append(f"{i.va:#010x} {i.text} after {prev.text if prev else ''}: source line {src[k].n} reads "
                                  f"unsigned: {text.strip()[:140]}")
    head = f"c3fy audit (a) width / signedness: {dict(st)}"
    lines = [head]
    for k in ("WIDTH", "SIGN", "CMPSIGN", "NONE"):
        lines.append(f"== {k}: {len(rep[k])}")
        lines += rep[k]
        lines.append("")
    A.pathlib.Path(a.out).write_text("\n".join(lines) + "\n")
    print(head)
    print("\n".join(x for x in lines if x.startswith("==")))


if __name__ == "__main__":
    sys.exit(main())
