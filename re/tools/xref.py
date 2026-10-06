"""Offline cross-references for naming work: call graph and string references of every function in a module,
built from the Ghidra function list (re/functions_ghidra*.tsv) and the original bytes (capstone), cached in
log/xref_<module>.json. Strings are shown on screen only; files under re/ cite them by address.

    py -3.12 re/tools/xref.py golf_clean.exe 0x0040afa0          # one function: callers, callees, strings, match
    py -3.12 re/tools/xref.py golf_clean.exe --range 0x401000 0x40ffff   # one line per function
    py -3.12 re/tools/xref.py golf_clean.exe --string 0x4d3904   # functions that reference a string address
    py -3.12 re/tools/xref.py golf_clean.exe --rebuild

Names come from the function list, overridden by re/names/*.tsv (module, address, name, ...) and by the decorated
symbol of a `// MATCH:` line in re/match/*.cpp when the function list still has FUN_."""
import argparse, glob, json, pathlib, re, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402


def load_pe(mod):
    pe = pefile.PE(str(ROOT / "original" / mod), fast_load=True)
    return pe, pe.OPTIONAL_HEADER.ImageBase


def read_cstr(pe, base, va, maxlen=200):
    try:
        data = pe.get_data(va - base, maxlen)
    except Exception:
        return None
    end = data.find(b"\0")
    if end < 4:
        return None
    s = data[:end]
    if all(32 <= c < 127 or c in (9, 10, 13) for c in s):
        return s.decode("latin-1")
    return None


def functions(mod):
    """[(va, name, size)] from the Ghidra list, sizes capped like match.ghidra_size."""
    rows = [l.split("\t") for l in (ROOT / "re" / match.TSV[mod]).read_text().splitlines()[1:]]
    base = match.image_base(mod)
    out = []
    for r in rows:
        key = int(r[0], 16)
        va = key if mod == "golf_clean.exe" else key + base
        size = match.ghidra_size(mod, va) or int(r[2])
        out.append((va, r[1], size))
    return sorted(out)


def build(mod):
    pe, base = load_pe(mod)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    fns = functions(mod)
    entries = {va for va, _, _ in fns}
    img_lo, img_hi = base, base + pe.OPTIONAL_HEADER.SizeOfImage
    idx = {}
    for va, name, size in fns:
        try:
            code = pe.get_data(va - base, size)
        except Exception:
            continue
        calls, strs, data = set(), set(), set()
        for ins in md.disasm(code, va):
            if ins.mnemonic in ("call", "jmp") and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                t = ins.operands[0].imm & 0xFFFFFFFF
                if t in entries and (ins.mnemonic == "call" or not (va <= t < va + size)):
                    calls.add(t)
                continue
            for op in ins.operands:
                v = None
                if op.type == capstone.x86.X86_OP_IMM:
                    v = op.imm & 0xFFFFFFFF
                elif op.type == capstone.x86.X86_OP_MEM and not op.mem.base and not op.mem.index:
                    v = op.mem.disp & 0xFFFFFFFF
                if v is None or not (img_lo <= v < img_hi) or v in entries:
                    continue
                if read_cstr(pe, base, v) is not None:
                    strs.add(v)
                else:
                    data.add(v)
        idx[va] = {"name": name, "size": size, "calls": sorted(calls), "strings": sorted(strs), "data": sorted(data)}
    # debug DLLs call through incremental-linking thunks (a 5-byte `jmp body`): attribute calls to the body
    thunk = {}
    for va, name, size in fns:
        try:
            b = pe.get_data(va - base, 5)
        except Exception:
            continue
        if size <= 5 and b[0] == 0xE9:
            t = (va + 5 + int.from_bytes(b[1:5], "little", signed=True)) & 0xFFFFFFFF
            if t in entries:
                thunk[va] = t
    for f in idx.values():
        f["calls"] = sorted({thunk.get(t, t) for t in f["calls"]})
    for va in thunk:
        idx.pop(va, None)
    callers = {}
    for va, f in idx.items():
        for t in f["calls"]:
            callers.setdefault(t, []).append(va)
    for va in idx:
        idx[va]["callers"] = sorted(callers.get(va, []))
    path = ROOT / "log" / f"xref_{mod}.json"
    path.parent.mkdir(exist_ok=True)
    path.write_text(json.dumps({str(k): v for k, v in idx.items()}))
    return idx


def load(mod, rebuild=False):
    path = ROOT / "log" / f"xref_{mod}.json"
    if rebuild or not path.exists():
        return build(mod)
    return {int(k): v for k, v in json.loads(path.read_text()).items()}


def names(mod):
    """Current best name per address: re/names/*.tsv, then MATCH symbols, then the function list."""
    out = {}
    base = match.image_base(mod)
    for f in sorted(glob.glob(str(ROOT / "re" / "match" / "*.cpp"))):
        for m, a, sym in re.findall(r"//\s*MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)\s+(\S+)", pathlib.Path(f).read_text(errors="replace")):
            if m == mod:
                out.setdefault(int(a, 16), ("match", sym, pathlib.Path(f).name))
    for f in sorted(glob.glob(str(ROOT / "re" / "names" / "*.tsv"))):
        for line in pathlib.Path(f).read_text().splitlines()[1:]:
            c = line.split("\t")
            if len(c) >= 3 and c[0] == mod:
                a = int(c[1], 16)
                va = a if mod == "golf_clean.exe" or a >= base else a + base
                out[va] = ("names", c[2], pathlib.Path(f).name)
    return out


def label(va, idx, nm):
    n = nm.get(va)
    if n and n[0] == "names":
        return n[1]
    base_name = idx.get(va, {}).get("name", "?")
    if base_name.startswith("FUN_") and n:
        return f"{base_name} [{n[1]}]"
    return base_name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("module")
    ap.add_argument("addr", nargs="?")
    ap.add_argument("--range", nargs=2)
    ap.add_argument("--string")
    ap.add_argument("--rebuild", action="store_true")
    a = ap.parse_args()
    mod = a.module
    idx = load(mod, a.rebuild)
    nm = names(mod)
    pe, base = load_pe(mod)
    if a.string:
        s = int(a.string, 16)
        print(repr(read_cstr(pe, base, s)))
        for va, f in sorted(idx.items()):
            if s in f["strings"]:
                print(f"  0x{va:08x} {label(va, idx, nm)}")
        return
    if a.range:
        lo, hi = (int(x, 16) for x in a.range)
        for va, f in sorted(idx.items()):
            if lo <= va <= hi:
                m = "M" if va in nm and nm[va][0] == "match" or (va in nm and nm[va][0] == "names") else " "
                ss = ", ".join(repr(read_cstr(pe, base, s))[:40] for s in f["strings"][:2])
                print(f"{m} 0x{va:08x} {f['size']:5} in{len(f['callers']):3} out{len(f['calls']):3} {label(va, idx, nm)[:50]:50} {ss}")
        return
    va = int(a.addr, 16)
    f = idx.get(va)
    if not f:
        raise SystemExit(f"0x{va:08x} is not a function entry in {mod}")
    print(f"0x{va:08x} {label(va, idx, nm)}  size {f['size']}")
    if va in nm and nm[va][0] == "match":
        print(f"  matched 100%: {nm[va][1]} in re/match/{nm[va][2]}")
    print("  callers: " + ", ".join(f"0x{c:08x} {label(c, idx, nm)}" for c in f["callers"]) if f["callers"] else "  callers: none")
    print("  callees: " + ", ".join(f"0x{c:08x} {label(c, idx, nm)}" for c in f["calls"]) if f["calls"] else "  callees: none")
    for s in f["strings"]:
        print(f"  string 0x{s:08x}: {read_cstr(pe, base, s)!r}")
    if f["data"]:
        print("  data: " + " ".join(f"0x{d:08x}" for d in f["data"][:30]))


if __name__ == "__main__":
    main()
