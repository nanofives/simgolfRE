r"""c3fy audit (c), batch c3fyc: every call of mainLoop 0x0040f5c0 against shim/src/re/c3fy.cpp.

For each `call` of the listing: the callee, the arguments on the stack at the call (each push's operand, a `push reg`
resolved backwards inside its straight-line run to `mov reg, imm` / `lea reg, [F+X]` / a load), ecx for a callee that
pops (`ret N`), and the cleanup after it. For each call in the C source (CC / TC, the shorthand wrappers expanded, the
IAT casts `reinterpret_cast<..WINAPI..>(GU32(slot))`): the callee, the arguments (literal, FA / FD / FW / global load,
or `expr`), and the convention (CC = cdecl, TC = thiscall through fastcall, WINAPI = stdcall).

A C call is paired with the original call to the same callee nearest to the addresses its line cites (+-0x80). Checks:
  CALLEE    the line cites a `call` whose target differs and no call to the C callee is near
  CONV      CC to a callee that pops (`ret N`, N > 0), or TC / WINAPI with 4 * (stack args) != N
  NARGS     cdecl: stack args of the C call != the original's (from the cleanup `add esp, X` / `pop ecx` right after the
            call, when that cleanup belongs to this call alone), or for callee-pops N / 4
  ARG       a C argument that is a literal / FA / FD / FW / global differs from the classified original push at the
            same position (only when both sides classify)
  SELF      TC: the literal / FA self differs from the resolved ecx
  UNMATCHED an original direct call no C call is paired with (wrappers inlined in the source excepted)
  UNCITED   a C call whose line cites no address

    py -3.12 re/tools/c3fy_audit_calls.py [--out log/c3/c3fyc_audit_calls.txt]
"""
from __future__ import annotations

import argparse
import collections
import re
import sys

import c3fy_audit_common as A

WRAPPERS_SKIP = {"CC", "TC", "G", "StrCpy", "StrCat"}


def classify_operand(op: str) -> tuple[str, int] | None:
    op = op.strip()
    if re.fullmatch(r"-?0x[0-9a-f]+|-?\d+", op):
        return ("imm", int(op, 0) & 0xFFFFFFFF)
    m = re.fullmatch(r"dword ptr \[F\+0x([0-9a-f]+)\]", op)
    if m:
        return ("FD", int(m.group(1), 16))
    m = re.fullmatch(r"dword ptr \[0x([0-9a-f]+)\]", op)
    if m:
        return ("G32", int(m.group(1), 16))
    m = re.fullmatch(r"dword ptr \[F\+[a-z]{3}\*\d\+0x([0-9a-f]+)\]", op)
    if m:
        return ("FDi", int(m.group(1), 16))
    return None


REGS = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"}
SUB = {"eax": ("ax", "al", "ah"), "ebx": ("bx", "bl", "bh"), "ecx": ("cx", "cl", "ch"), "edx": ("dx", "dl", "dh"),
       "esi": ("si",), "edi": ("di",), "ebp": ("bp",)}


def writes_reg(ins: A.Ins, reg: str) -> bool:
    ops = ins.ops
    first = ops.split(",")[0].strip() if ops else ""
    names = (reg,) + SUB.get(reg, ())
    if ins.mn in ("call",):
        return reg in ("eax", "ecx", "edx")
    if ins.mn in ("cdq",) and reg == "edx":
        return True
    if ins.mn.startswith("rep") or ins.mn in ("movsd", "stosd", "scasb"):
        return reg in ("ecx", "edi", "esi", "eax")
    if ins.mn in ("pop", "inc", "dec", "neg", "not", "setl", "sete", "setne", "setg", "setge", "setle", "setb",
                  "seta", "setae", "setbe", "sets", "setns") or ins.mn.startswith("set"):
        return first in names
    if ins.mn in ("push", "cmp", "test", "jmp") or ins.mn.startswith("j"):
        return False
    if ins.mn in ("xchg",):
        return any(x.strip() in names for x in ops.split(","))
    if ins.mn in ("imul", "mul", "div", "idiv") and len(ops.split(",")) == 1:
        return reg in ("eax", "edx")
    return first in names


def resolve_reg(L: A.Listing, k: int, reg: str, limit: int = 24):
    """Backward from instruction index k (exclusive) to the instruction that last wrote `reg`; stops at a jump target
    boundary only by distance (the listing is in address order; c3fy's citations keep calls local)."""
    j = k - 1
    while j >= 0 and k - j <= limit:
        i = L.ins[j]
        if i.mn.startswith("j") or i.mn == "ret":
            return None
        if writes_reg(i, reg):
            parts = [p.strip() for p in i.ops.split(",", 1)]
            if i.mn == "mov" and len(parts) == 2 and parts[0] == reg:
                c = classify_operand(parts[1])
                if c:
                    return c
                if parts[1] in REGS:
                    return resolve_reg(L, j, parts[1], limit - (k - j))
                return ("load", parts[1])
            if i.mn == "lea" and len(parts) == 2 and parts[0] == reg:
                m = re.fullmatch(r"\[F\+0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("FA", int(m.group(1), 16))
                return ("lea", parts[1])
            if i.mn in ("movsx", "movzx") and len(parts) == 2 and parts[0] == reg:
                m = re.fullmatch(r"(word|byte) ptr \[F\+0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("F" + ("W" if m.group(1) == "word" else "B") + ("s" if i.mn == "movsx" else "z"),
                            int(m.group(2), 16))
                m = re.fullmatch(r"(word|byte) ptr \[F\+[a-z]{3}\*\d\+0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("F" + ("W" if m.group(1) == "word" else "B") + ("s" if i.mn == "movsx" else "z") + "i",
                            int(m.group(2), 16))
                m = re.fullmatch(r"(word|byte) ptr \[0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("G" + ("16" if m.group(1) == "word" else "8") + ("s" if i.mn == "movsx" else "z"),
                            int(m.group(2), 16))
                return ("ext", parts[1])
            if i.mn == "xor" and parts[0] == reg and len(parts) == 2 and parts[1] == reg:
                return ("imm", 0)
            return ("expr", i.text)
        j -= 1
    return None


def original_calls(L: A.Listing):
    """[{va, k, target | None, iat, ind, pop, cleanup, depth}] for every `call` of the listing."""
    out = []
    for k, i in enumerate(L.ins):
        if i.mn != "call" or i.depth is None:
            continue
        tgt = int(i.ops, 16) if re.fullmatch(r"0x[0-9a-f]+", i.ops) else None
        m = re.fullmatch(r"dword ptr \[0x([0-9a-f]+)\]", i.ops)
        iat = int(m.group(1), 16) if m else None
        cleanup = None
        nxt = L.ins[k + 1] if k + 1 < len(L.ins) else None
        if nxt is not None and nxt.mn == "add" and nxt.ops.startswith("esp,"):
            cleanup = int(nxt.ops.split(",")[1], 0)
        elif nxt is not None and nxt.mn == "pop" and nxt.ops == "ecx":
            cleanup = 4
        out.append({"va": i.va, "k": k, "target": tgt, "iat": iat, "ind": None if (tgt or iat) else i.ops,
                    "pop": A.ret_pop(tgt) if tgt else None, "cleanup": cleanup, "depth": i.depth})
    return out


def resolve_on_path(path: list, j: int, reg: str):
    """path = newest-first instructions before a call; resolve the value of `reg` as read by path[j] (exclusive)."""
    for q in range(j + 1, min(len(path), j + 30)):
        i = path[q]
        if writes_reg(i, reg):
            parts = [x.strip() for x in i.ops.split(",", 1)]
            if i.mn == "mov" and len(parts) == 2 and parts[0] == reg:
                c = classify_operand(parts[1])
                if c:
                    return c
                if parts[1] in REGS:
                    return resolve_on_path(path, q, parts[1])
                return ("load", parts[1])
            if i.mn == "lea" and len(parts) == 2 and parts[0] == reg:
                m = re.fullmatch(r"\[F\+0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("FA", int(m.group(1), 16))
                return ("lea", parts[1])
            if i.mn in ("movsx", "movzx") and len(parts) == 2 and parts[0] == reg:
                m = re.fullmatch(r"(word|byte) ptr \[F\+0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("F" + ("W" if m.group(1) == "word" else "B") + ("s" if i.mn == "movsx" else "z"),
                            int(m.group(2), 16))
                m = re.fullmatch(r"(word|byte) ptr \[F\+[a-z]{3}\*\d\+0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("F" + ("W" if m.group(1) == "word" else "B") + ("s" if i.mn == "movsx" else "z") + "i",
                            int(m.group(2), 16))
                m = re.fullmatch(r"(word|byte) ptr \[0x([0-9a-f]+)\]", parts[1])
                if m:
                    return ("G" + ("16" if m.group(1) == "word" else "8") + ("s" if i.mn == "movsx" else "z"),
                            int(m.group(2), 16))
                return ("ext", parts[1])
            if i.mn == "xor" and len(parts) == 2 and parts[0] == reg and parts[1] == reg:
                return ("imm", 0)
            return ("expr", i.text)
    return None


def call_args(L, path: list, call: A.Ins, n: int):
    """Arguments 0..n-1 of `call` along `path`: [(push va, operand, class)] and the stack-arg count the depths give
    (pushes on top of the lowest depth since the previous call along the path)."""
    D = call.depth
    want = {D - 4 * p: p for p in range(n)}
    got = {}
    base = D
    for j, i in enumerate(path):
        if i.depth is None:
            break
        if i.mn == "call":
            break
        base = min(base, i.depth)
        if i.mn == "push" and (i.depth + 4) in want and want[i.depth + 4] not in got:
            c = classify_operand(i.ops)
            if c is None and i.ops in REGS:
                c = resolve_on_path(path, j, i.ops)
            got[want[i.depth + 4]] = (i.va, i.ops, c)
    # depth right after the previous call (the path element just newer than it) also bounds the base
    return [got.get(p) for p in range(n)], (D - base) // 4


# ------------------------------------------------------------------ source side
def parse_wrappers(src_text: str) -> dict[str, dict]:
    """static inline helpers whose body is one CC / TC call: name -> {params, kind, target, args (param exprs)}."""
    out = {}
    for m in re.finditer(r"static (?:inline )?\w+\s+(\w+)\(([^)]*)\)\s*\{([^{}]*?)\}", src_text):
        name, params, body = m.groups()
        calls = re.findall(r"\b(CC|TC)\(", body)
        if len(calls) != 1:
            continue
        pnames = [p.strip().split()[-1] for p in params.split(",") if p.strip() and p.strip() != "void"]
        k = body.index(calls[0] + "(")
        args, _ = A.split_args(body, k + 2)
        out[name] = {"params": pnames, "kind": calls[0], "target": A.int_literal(args[0]), "args": args[1:]}
    return out


def subst(expr: str, mapping: dict[str, str]) -> str:
    def rep(m):
        return mapping.get(m.group(0), m.group(0))
    return re.sub(r"\b\w+\b", rep, expr)


def classify_c(e: str):
    e0 = A.strip_casts(e)
    v = A.int_literal(e0)
    if v is not None:
        return ("imm", v)
    if e0 == "kBuf":
        return ("imm", 0x0051A068)
    m = re.fullmatch(r"FA\((0x[0-9a-fA-F]+|\d+)\)", e0)
    if m:
        return ("FA", int(m.group(1), 0))
    m = re.fullmatch(r"F(D|U)\((0x[0-9a-fA-F]+|\d+)\)", e0)
    if m:
        return ("FD", int(m.group(2), 0))
    m = re.fullmatch(r"F(W|UW|B|UB)\((0x[0-9a-fA-F]+|\d+)\)", e0)
    if m:
        w = "W" if "W" in m.group(1) else "B"
        return ("F" + w + ("z" if m.group(1).startswith("U") else "s"), int(m.group(2), 0))
    m = re.fullmatch(r"G(32|U32)\((0x[0-9a-fA-F]+)\)", e0)
    if m:
        return ("G32", int(m.group(2), 16))
    m = re.fullmatch(r"G(16|U16|8|U8)\((0x[0-9a-fA-F]+)\)", e0)
    if m:
        g = m.group(1)
        return ("G" + g.lstrip("U") + ("z" if g.startswith("U") else "s"), int(m.group(2), 16))
    m = re.fullmatch(r"F(D|U|W|UW|B|UB)\((0x[0-9a-fA-F]+)\s*\+\s*(?:\d\s*\*\s*\w+|\w+\s*\*\s*\d|\w+)\)", e0)
    if m:
        g = m.group(1)
        if g in ("D", "U"):
            return ("FDi", int(m.group(2), 16))
        w = "W" if "W" in g else "B"
        return ("F" + w + ("z" if g.startswith("U") else "s") + "i", int(m.group(2), 16))
    if e0 == "Root()":
        return ("G32", 0x004C1570)
    return None




def joined_code(src: list[A.SrcLine]) -> tuple[str, list[int]]:
    """The code parts of every line joined with spaces, and the offset each line starts at (for multi-line calls)."""
    parts, offs, pos = [], [], 0
    for ln in src:
        offs.append(pos)
        parts.append(ln.code + " ")
        pos += len(ln.code) + 1
    return "".join(parts), offs


WRAPDEF = re.compile(r"\s*static (?:inline )?\w+\s+(\w+)\(")


def source_calls(src: list[A.SrcLine], wrappers: dict[str, dict]):
    import bisect
    text, offs = joined_code(src)
    out = []
    names = "|".join(sorted(set(wrappers) | {"CC", "TC"}, key=len, reverse=True))
    pat = re.compile(r"\b(" + names + r")\(")
    for m in pat.finditer(text):
        li = bisect.bisect_right(offs, m.start()) - 1
        ln = src[li]
        if re.match(r"\s*(?:template|#define)", ln.code):
            continue
        wd = WRAPDEF.match(ln.code)
        if wd and wd.group(1) in wrappers:
            continue                      # a wrapper's own definition line
        if wd and m.start() - offs[li] < ln.code.index("(") + 1 and m.group(1) == wd.group(1):
            continue
        nm = m.group(1)
        args, end = A.split_args(text, m.end() - 1)
        if nm in ("CC", "TC"):
            if not args:
                continue
            tgt = A.int_literal(args[0])
            kind, cargs = nm, args[1:]
        else:
            w = wrappers[nm]
            mp = dict(zip(w["params"], args))
            kind, tgt, cargs = w["kind"], w["target"], [subst(x, mp) for x in w["args"]]
        if tgt is None:
            continue
        self_, stack = (cargs[0], cargs[1:]) if kind == "TC" else (None, cargs)
        last = bisect.bisect_right(offs, end) - 1
        cites = []
        for q in range(li, last + 1):
            cites += src[q].cites
        prev = []
        q = li - 1
        while q >= 0 and len(prev) < 3:
            if src[q].code.strip() and src[q].cites:
                prev.append(src[q].cites)
            q -= 1
        out.append({"line": ln.n, "cites": cites, "prev": [c for p in prev for c in p], "kind": kind, "via": nm,
                    "target": tgt, "self": self_, "args": stack, "text": " ".join(text[m.start():end].split())[:150]})
    iat = re.compile(r"reinterpret_cast<[^>]*\((WINAPI|__stdcall|__cdecl)\*\)[^>]*>\(GU?32\((0x[0-9a-fA-F]+)\)\)\(")
    for m in iat.finditer(text):
        li = bisect.bisect_right(offs, m.start()) - 1
        ln = src[li]
        if "static inline" in ln.code:
            continue
        args, end = A.split_args(text, m.end() - 1)
        out.append({"line": ln.n, "cites": ln.cites, "prev": [], "kind": "IAT" if m.group(1) != "__cdecl" else "ICC",
                    "via": "iat", "target": ("iat", int(m.group(2), 16)), "self": None, "args": args,
                    "text": " ".join(text[m.start():end].split())[:150]})
    return out


def missing_any(oargs):
    return any(x is None for x in oargs)


def fmt(c):
    if c is None:
        return "?"
    return f"{c[0]} {c[1] if isinstance(c[1], str) else hex(c[1])}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(A.ROOT / "log" / "c3" / "c3fyc_audit_calls.txt"))
    a = ap.parse_args()
    L = A.Listing()
    P = A.preds(L)
    src = A.load_source()
    text = A.SOURCE.read_text(encoding="utf-8", errors="replace")
    wrappers = parse_wrappers(text)
    oc = original_calls(L)
    by_target = collections.defaultdict(list)
    for c in oc:
        key = c["target"] if c["target"] is not None else ("iat", c["iat"]) if c["iat"] else ("ind", c["ind"])
        by_target[key].append(c)
    sc = source_calls(src, wrappers)
    rep = collections.defaultdict(list)
    paired = collections.defaultdict(list)
    stats = collections.Counter()
    for s in sc:
        stats["c_calls"] += 1
        key = s["target"]
        if not s["cites"]:
            rep["UNCITED"].append(f"line {s['line']}: {s['text']}")
            continue
        lo, hi = min(s["cites"]) - 0x80, max(s["cites"]) + 0x80
        cands = [c for c in by_target.get(key, []) if lo <= c["va"] <= hi]
        kt = key if isinstance(key, tuple) else hex(key)
        if not cands:
            cited_calls = [c for c in oc if c["va"] in s["cites"]]
            if cited_calls:
                cc = cited_calls[0]
                rep["CALLEE"].append(f"line {s['line']}: C -> {kt}, cited call {cc['va']:#010x} -> "
                                     f"{hex(cc['target']) if cc['target'] else cc['iat'] and hex(cc['iat'])}: {s['text']}")
            else:
                rep["NOPAIR"].append(f"line {s['line']} cites {[hex(x) for x in s['cites']]}: no call to {kt} within "
                                     f"0x80: {s['text']}")
            continue
        cands.sort(key=lambda c: (c["va"] not in s["cites"], min(abs(c["va"] - x) for x in s["cites"])))
        c = cands[0]
        paired[c["va"]].append(s["line"])
        stats["paired"] += 1
        n_c = len(s["args"])
        call = L.ins[c["k"]]
        hints = s["prev"] or [x for x in s["cites"] if x != c["va"]]
        path = A.back_path(L, P, c["k"], hints)
        oargs, n_depth = call_args(L, path, call, n_c)
        if s["kind"] == "CC":
            if c["pop"]:
                rep["CONV"].append(f"line {s['line']} / {c['va']:#010x}: CC (cdecl) to {kt} which pops {c['pop']} "
                                   f"(ret N): {s['text']}")
        elif s["kind"] in ("TC", "IAT"):
            if c["pop"] is not None and c["pop"] != 4 * n_c:
                rep["CONV"].append(f"line {s['line']} / {c['va']:#010x}: {s['kind']} with {n_c} stack args, callee "
                                   f"ret {c['pop']}: {s['text']}")
            if c["cleanup"] is not None:
                rep["CONV"].append(f"line {s['line']} / {c['va']:#010x}: {s['kind']} but the original cleans "
                                   f"{c['cleanup']} after the call: {s['text']}")
        # cdecl cleanup within the next 4 instructions (VC6 merges the cleanups of consecutive calls, and an argument
        # pushed before an inner call is popped by the outer call's cleanup)
        x_clean = None
        for q in range(c["k"] + 1, min(c["k"] + 5, len(L.ins))):
            iq = L.ins[q]
            if iq.mn == "add" and iq.ops.startswith("esp,"):
                x_clean = int(iq.ops.split(",")[1], 0)
                break
            if iq.mn == "pop" and iq.ops == "ecx":
                x_clean = 4
                break
            if iq.mn in ("call", "push", "ret") or iq.mn.startswith("j"):
                break
        if n_depth != n_c and not (s["kind"] == "CC" and x_clean is not None and 4 * n_c == x_clean) and                 not (s["kind"] == "CC" and n_depth < n_c and x_clean is not None and 4 * n_c <= x_clean
                     and not missing_any(oargs)):
            rep["NARGS?"].append(f"line {s['line']} / {c['va']:#010x}: C {n_c} stack args, the original's depth gives "
                                 f"{n_depth} (pushes since the previous call on the path): {s['text']}")
        else:
            stats["nargs_ok"] += 1
        missing = [p for p, x in enumerate(oargs) if x is None]
        if missing:
            rep["ARGPOS?"].append(f"line {s['line']} / {c['va']:#010x}: no push found on the path for arg(s) {missing}: "
                                  f"{s['text']}")
        for p, ce in enumerate(s["args"]):
            cc = classify_c(ce)
            if p >= len(oargs) or oargs[p] is None:
                continue
            ova, oop, ocl = oargs[p]
            if cc is None or ocl is None or ocl[0] in ("expr", "load", "lea", "ext"):
                stats["arg_unresolved"] += 1
                continue
            stats["arg_checked"] += 1
            if cc != ocl:
                rep["ARG"].append(f"line {s['line']} / {c['va']:#010x} arg {p}: C `{ce}` = {fmt(cc)}, original push "
                                  f"{ova:#010x} `{oop}` = {fmt(ocl)}: {s['text']}")
        if s["kind"] == "TC" and s["self"] is not None:
            cs = classify_c(s["self"])
            ecx = resolve_on_path([call] + path, 0, "ecx")
            if cs and ecx and ecx[0] in ("imm", "FA", "G32", "FD"):
                stats["self_checked"] += 1
                if cs != ecx:
                    rep["SELF"].append(f"line {s['line']} / {c['va']:#010x}: C self `{s['self']}` = {fmt(cs)}, original "
                                       f"ecx {fmt(ecx)}: {s['text']}")
            else:
                stats["self_unresolved"] += 1
    for c in oc:
        if c["va"] not in paired:
            if c["target"] in (0x004A6070,):
                continue
            t = hex(c["target"]) if c["target"] else (("IAT " + hex(c["iat"])) if c["iat"] else c["ind"])
            rep["UNMATCHED"].append(f"{c['va']:#010x} call {t} (pop {c['pop']}, cleanup {c['cleanup']})")
        else:
            stats["orig_calls_paired"] += 1
    stats["orig_calls"] = len(oc)
    head = f"c3fy audit (c) calls: {dict(stats)}"
    lines = [head]
    for k in ("CALLEE", "CONV", "NARGS?", "ARGPOS?", "ARG", "SELF", "NOPAIR", "UNCITED", "UNMATCHED"):
        lines.append(f"== {k}: {len(rep[k])}")
        lines += rep[k]
        lines.append("")
    A.pathlib.Path(a.out).write_text("\n".join(lines) + "\n")
    print(head)
    print("\n".join(x for x in lines if x.startswith("==")))


if __name__ == "__main__":
    sys.exit(main())
