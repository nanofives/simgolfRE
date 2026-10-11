"""Check every A/B registry entry's calling convention and argument count against the original's `ret N`.

Round 28 (c3dx): panelSubSlot60 returns with `ret 4`, but its detour and registry entry declared no argument; its own A/B
was GREEN (the harness's call frame absorbed the 4 bytes) while a C3 caller crashed with the hook on. The same mismatch in
the registry means the A/B calls the original with the wrong frame. For each entry this decodes the original over its
Ghidra extent (re/functions_ghidra*.tsv ranges; a 5-byte `jmp` thunk is followed) and collects the immediates of its `ret`
instructions, then expects:
    default / mscdecl (cdecl)   ret 0
    stdcall                     ret 4 * len(args)
    thiscall                    ret 4 * (len(args) - 1)     (this in ecx)
    fastcall                    ret 4 * max(0, len(args) - 2)
A function with no `ret` in its extent (noreturn, tail jumps only) is reported as `noret`, not as a mismatch.

    py -3.12 re/tools/check_ret_args.py            # every entry of the enabled batches; exit 1 on a mismatch
    py -3.12 re/tools/check_ret_args.py --all      # also prints the entries that agree
"""
import csv, pathlib, struct, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "frida"))
import hooks_registry as h  # noqa: E402

FILES = {"golf_clean.exe": "re/functions_ghidra.tsv", "Terrain.dll": "re/functions_ghidra_Terrain.dll.tsv",
         "jgld.dll": "re/functions_ghidra_jgld.dll.tsv", "sound.dll": "re/functions_ghidra_sound.dll.tsv"}
_MOD = {}


def module(name):
    if name not in _MOD:
        pe = pefile.PE(str(ROOT / "original" / name), fast_load=True)
        base = pe.OPTIONAL_HEADER.ImageBase
        ranges = {}
        for r in csv.DictReader(open(ROOT / FILES[name]), delimiter="\t"):
            e = int(r["entry"], 16)
            e = e if name == "golf_clean.exe" or e >= 0x10000000 else e + base
            rs = []
            for rg in (r.get("ranges") or "").split(";"):
                if "-" in rg:
                    lo, hi = (int(x, 16) for x in rg.split("-"))
                    if name != "golf_clean.exe" and lo < 0x10000000:
                        lo, hi = lo + base, hi + base
                    rs.append((lo, hi))
            ranges[e] = rs or [(e, e + int(r["size"]))]
        _MOD[name] = (base, pe.get_memory_mapped_image(), ranges)
    return _MOD[name]


def rets(name, va, depth=0):
    base, img, ranges = module(name)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    if img[va - base] == 0xE9 and depth < 3:                    # incremental-link thunk: follow it
        return rets(name, va + 5 + struct.unpack("<i", img[va - base + 1:va - base + 5])[0], depth + 1)
    out, tails = set(), set()
    rs = ranges.get(va, [(va, va + 0x400)])
    inside = lambda t: any(lo <= t < hi for lo, hi in rs)
    for lo, hi in rs:
        for ins in md.disasm(img[lo - base:hi - base], lo):
            if ins.mnemonic == "ret":
                out.add(int(ins.op_str, 16) if ins.op_str else 0)
            elif ins.mnemonic == "jmp" and ins.op_str.startswith("0x") and not inside(int(ins.op_str, 16)):
                tails.add(int(ins.op_str, 16))      # a tail call: its target's `ret` pops this function's frame
    if not out and depth < 3:
        for t in tails:
            out |= rets(name, t, depth + 1)
    return out


def expected(abi, nargs):
    abi = abi or "default"
    if abi in ("default", "mscdecl"):
        return 0
    if abi == "stdcall":
        return 4 * nargs
    if abi == "thiscall":
        return 4 * max(0, nargs - 1)
    if abi == "fastcall":
        return 4 * max(0, nargs - 2)
    return None


def main():
    show_all = "--all" in sys.argv
    bad = 0
    seen = set()
    for key, v in sorted(h.HOOKS.items()):
        mod = v.get("module") or "golf_clean.exe"
        base = module(mod)[0]
        va = v["addr"] if mod == "golf_clean.exe" or v["addr"] >= 0x10000000 else v["addr"] + base
        exp = expected(v.get("abi"), len(v.get("args") or []))
        got = rets(mod, va)
        tag = (mod, va, v.get("abi"), len(v.get("args") or []))
        if tag in seen and not show_all:
            continue
        seen.add(tag)
        if not got:
            status = "noret"
        elif exp is None:
            status = "unknown-abi"
        elif exp in got:
            # several values only appear through followed tail jumps into shared bodies (sound strcpy 0x1004a010 jumps
            # into strcat's loop, round 21); the declared convention is consistent when its value is among them
            status = "ok"
        else:
            status = "MISMATCH"
            bad += 1
        if status != "ok" or show_all:
            print(f"{status:9} {mod}:{va:08x} {key:40} abi={v.get('abi') or 'default'} args={len(v.get('args') or [])} "
                  f"expects ret {exp}, original has ret {sorted(got)}")
    print(f"{len(seen)} (module, addr, abi, argc) combinations checked, {bad} mismatches")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
