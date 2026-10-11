"""Evidence for the two C2 end states (decided 2026-10-11): `dead` and `trivial`.

A C2 row whose function nothing can reach, or whose body has nothing a test could observe, cannot reach C3 under the
rubric (re/CONFIDENCE.md: no caller exemption; an A/B with one possible result is not evidence). Instead of leaving
them in the work pool forever, `re_classify.py mark <addr> --status dead|trivial` records the end state in hooks.csv's
`status` column (the level stays C2) after this module's checks pass, with one CHANGELOG line; `unmark` reverses it.

dead: all of
  - no direct reference: no E8/E9 rel32, EB rel8 or 0F 8x rel32 encoding anywhere in the module's executable sections
    lands on the body or on one of its incremental-link thunks (a byte scan, not a disassembly, so it over-reports
    references and errs toward NOT dead);
  - no stored pointer: no dword equal to the body or a thunk anywhere in the mapped image (re/tools/pointer_callers.py
    references(); an uninstalled table slot also counts as a reference);
  - not exported (PE export table, body or thunk).
trivial: all of
  - the body is at most 64 bytes (re/functions_ghidra*.tsv size) and is one range;
  - no call except to the module's __chkesp (the /GZ frame check of the debug DLLs);
  - no memory write except to the stack frame ([ebp-x], [esp+x], push, rep stos with edi = lea [ebp-x]);
  - eax at every `ret` is an immediate, zero, or never written (void).

    py -3.12 re/tools/endstate.py <module> 0x<addr> [--status dead|trivial]   # prints each check
    py -3.12 re/tools/endstate.py --survey                                    # counts over the C2 rows
"""
import argparse, csv, pathlib, re, struct, sys

import capstone

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import pointer_callers as pc  # noqa: E402

TRIVIAL_MAX = 64
_REL = {}


def _rel_targets(m):
    """{target: [source, ...]} for every call/jmp/jcc encoding at any byte offset of the executable sections (byte scan)."""
    if m.name in _REL:
        return _REL[m.name]
    out = {}
    img, base = m.img, m.base
    for lo, hi in m.text:
        o0, o1 = lo - base, hi - base
        data = img[o0:o1]
        for i in range(len(data) - 1):
            b = data[i]
            t = None
            if b in (0xE8, 0xE9) and i + 5 <= len(data):
                t = lo + i + 5 + struct.unpack_from("<i", data, i + 1)[0]
            elif b == 0xEB:
                t = lo + i + 2 + struct.unpack_from("<b", data, i + 1)[0]
            elif b == 0x0F and 0x80 <= data[i + 1] <= 0x8F and i + 6 <= len(data):
                t = lo + i + 6 + struct.unpack_from("<i", data, i + 2)[0]
            if t is not None:
                out.setdefault(t, []).append(lo + i)
    _REL[m.name] = out
    return out


_STARTS = {}


def _real_source(m, src, target):
    """A byte-scan hit is a reference when it lies outside every known function (conservative) or when decoding the
    owning function range from its start gives an instruction at `src` that branches to `target`."""
    i = pc.bisect.bisect_right(m._starts, src) - 1
    if i < 0 or not (m.ranges[i][0] <= src < m.ranges[i][1]):
        return True
    lo, hi, _ = m.ranges[i]
    if (lo, hi) not in _STARTS:
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        md.skipdata = True
        _STARTS[(lo, hi)] = {ins.address: ins for ins in md.disasm(m.img[lo - m.base:hi - m.base], lo)}
    ins = _STARTS[(lo, hi)].get(src)
    return bool(ins and ins.mnemonic in ("call", "jmp") or (ins and ins.mnemonic.startswith("j"))) and \
        ins.op_str.startswith("0x") and int(ins.op_str, 16) == target


def _exports(m):
    import pefile
    pe = pefile.PE(str(ROOT / "original" / m.name), fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    if not hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        return set()
    return {m.base + s.address for s in pe.DIRECTORY_ENTRY_EXPORT.symbols}


def dead_checks(module, addr):
    m = pc.mod(module)
    va = pc.norm(m, addr)
    thunks = m.thunks.get(va, [])
    targets = [va] + thunks
    rel = _rel_targets(m)
    # a real hit on a thunk is a reference; the body is always hit by its own thunks' `jmp`, which does not count
    direct = []
    for t in targets:
        srcs = [s for s in rel.get(t, []) if not (t == va and s in thunks) and _real_source(m, s, t)]
        if srcs:
            direct.append(t)
    refs = pc.references(m, va)
    exp = sorted(t for t in targets if t in _exports(m))
    return [
        ("no direct call/jmp/jcc lands on the body or a thunk", not direct, ", ".join(f"0x{t:08x}" for t in direct)),
        ("no stored pointer to the body or a thunk", not refs, f"{len(refs)} reference(s)" if refs else ""),
        ("not exported", not exp, ", ".join(f"0x{t:08x}" for t in exp)),
    ]


STACK = re.compile(r"\[(ebp|esp)( [-+] (0x[0-9a-f]+|\d+))?\]")
# a trivial body may only contain these: the /GZ frame (push/mov/sub/lea/rep stosd/cmp/call __chkesp), stack moves,
# immediates and the return; anything else (a branch, fldcw/fnstsw, in/out, a string op other than the fill) disqualifies
ALLOWED = {"push", "pop", "mov", "lea", "sub", "add", "xor", "cmp", "ret", "call", "rep stosd", "nop", "int3"}


def trivial_checks(module, addr):
    m = pc.mod(module)
    va = pc.norm(m, addr)
    if m.img[va - m.base] == 0xE9:                                  # a thunk: judge its body
        va = va + 5 + struct.unpack_from("<i", m.img, va - m.base + 1)[0]
    ranges = [(lo, hi) for lo, hi, e in m.ranges if e == va]
    size = sum(hi - lo for lo, hi in ranges)
    chkesp = {e for e, n in m.entries.items() if n.endswith("__chkesp")}
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    calls, writes, eax_ok, rets, others = [], [], True, 0, []
    eax_state = "unset"                                             # unset | imm | other
    for lo, hi in ranges:
        for ins in md.disasm(m.img[lo - m.base:hi - m.base], lo):
            mn, op = ins.mnemonic, ins.op_str
            if mn not in ALLOWED:                                   # branches, FPU state (fldcw/fnstsw), I/O...
                others.append(f"0x{ins.address:08x} {mn} {op}")
            if mn == "call":
                t = int(op, 16) if op.startswith("0x") else None
                tt = t + 5 + struct.unpack_from("<i", m.img, t - m.base + 1)[0] if t and m.img[t - m.base] == 0xE9 else t
                if tt not in chkesp:
                    calls.append(f"0x{ins.address:08x} call {op}")
            elif mn.startswith("rep stos"):
                pass                                                # the /GZ fill: edi = lea [ebp-x] (checked below)
            elif mn in ("mov", "add", "sub", "and", "or", "xor", "inc", "dec", "pop", "xchg") and op.split(",")[0].startswith(
                    ("dword ptr [", "word ptr [", "byte ptr [")):
                dst = op.split(",")[0]
                if not STACK.search(dst):
                    writes.append(f"0x{ins.address:08x} {mn} {op}")
            if mn == "lea" and op.startswith("edi") and not STACK.search(op):
                writes.append(f"0x{ins.address:08x} {mn} {op} (rep stos base not on the stack)")
            # eax tracking (straight-line: these bodies have no branches worth following)
            if op.startswith("eax,") or op == "eax":
                if mn == "xor" and op == "eax, eax":
                    eax_state = "imm"
                elif mn == "mov" and re.fullmatch(r"eax, (0x[0-9a-f]+|\d+)", op):
                    eax_state = "imm"
                elif mn in ("mov", "add", "sub", "and", "or", "xor", "lea", "pop", "inc", "dec", "movzx", "movsx",
                            "imul", "shl", "shr", "sar", "neg", "not"):
                    eax_state = "other"
            if mn == "rep stosd":
                pass
            if mn == "ret":
                rets += 1
                if eax_state == "other":
                    eax_ok = False
    return [
        (f"body <= {TRIVIAL_MAX} bytes in one range", bool(ranges) and len(ranges) == 1 and size <= TRIVIAL_MAX,
         f"{size} bytes, {len(ranges)} range(s)"),
        ("no call except __chkesp", not calls, "; ".join(calls)),
        ("no memory write outside the stack frame", not writes, "; ".join(writes)),
        ("eax at ret is an immediate or unset", eax_ok and rets > 0, f"{rets} ret(s)"),
        ("only frame, stack, immediate and return instructions (no branch, FPU, I/O)", not others, "; ".join(others)),
    ]


def run(module, addr, status):
    checks = dead_checks(module, addr) if status == "dead" else trivial_checks(module, addr)
    return all(ok for _, ok, _ in checks), checks


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("module", nargs="?")
    ap.add_argument("addr", nargs="?")
    ap.add_argument("--status", choices=("dead", "trivial"))
    ap.add_argument("--survey", action="store_true")
    a = ap.parse_args()
    if a.survey:
        rows = [r for r in csv.DictReader(open(ROOT / "hooks.csv")) if r["confidence"] == "C2" and r["status"] == "transcribed"]
        from collections import Counter
        c = Counter()
        for r in rows:
            mod = r["module"] or "golf_clean.exe"
            if (r["callers"] or "none") == "none" and run(mod, r["addr"], "dead")[0]:
                c[(mod, "dead")] += 1
            elif run(mod, r["addr"], "trivial")[0]:
                c[(mod, "trivial")] += 1
        for k in sorted(c):
            print(f"{k[0]:15} {k[1]:8} {c[k]}")
        return 0
    for st in ([a.status] if a.status else ["dead", "trivial"]):
        ok, checks = run(a.module, a.addr, st)
        print(f"{st}: {'PASS' if ok else 'FAIL'}")
        for name, good, detail in checks:
            print(f"  [{'PASS' if good else 'FAIL'}] {name}" + (f" ({detail})" if detail else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
