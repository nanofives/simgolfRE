"""S11.3 hand check of dead / trivial verdicts, independent of endstate.py's code: scan the FILE's section bytes for
(a) E8/E9 rel32 and Jcc rel32 encodings landing on the function, (b) the absolute VA as a dword anywhere in the file,
(c) the Ghidra caller column, then print the disassembly (capstone) for the reader. Not a re-run of endstate.py.

    ~/bin/py -3.12 log/roadmap/S11/3_handcheck.py > log/roadmap/S11/3_handcheck.log
"""
import csv, pathlib, struct, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[3]
SAMPLE = [("golf_clean.exe", "004a7470"), ("golf_clean.exe", "004a9039"), ("golf_clean.exe", "00402180"),
          ("golf_clean.exe", "00488cb0"), ("Terrain.dll", "00021b70"), ("Terrain.dll", "0002a9e0"),
          ("Terrain.dll", "0001a200"), ("jgld.dll", "0007f110"), ("jgld.dll", "00089a40"),
          ("sound.dll", "0001c730"), ("sound.dll", "00042505"), ("golf_clean.exe", "004a682f")]
rows = {(r["module"], r["addr"]): r for r in csv.DictReader(open(ROOT / "re/roadmap/S11_c2_triage.tsv"), delimiter="\t")}
for mod, addr in SAMPLE:
    pe = pefile.PE(str(ROOT / "original" / mod), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    va = int(addr, 16) + (0 if mod == "golf_clean.exe" else base)
    r = rows[(mod, addr)]
    print(f"=== {mod} {addr} {r['name']} size {r['size']} verdict {r['endstate']} hooks callers {r['callers']}")
    hits = []
    for s in pe.sections:
        data = s.get_data()
        sva = base + s.VirtualAddress
        exe = bool(s.Characteristics & 0x20000000)
        for i in range(len(data) - 4):
            b = data[i]
            if exe and b in (0xE8, 0xE9) and i + 5 <= len(data) and sva + i + 5 + struct.unpack_from("<i", data, i + 1)[0] == va:
                hits.append(f"rel32 {b:02x} at 0x{sva + i:08x}")
            if exe and b == 0x0F and 0x80 <= data[i + 1] <= 0x8F and i + 6 <= len(data) and \
                    sva + i + 6 + struct.unpack_from("<i", data, i + 2)[0] == va:
                hits.append(f"jcc rel32 at 0x{sva + i:08x}")
            if data[i:i + 4] == struct.pack("<I", va):
                hits.append(f"dword {s.Name.rstrip(bytes(1)).decode()} 0x{sva + i:08x}")
    ep = base + pe.OPTIONAL_HEADER.AddressOfEntryPoint
    print(f"  byte scan hits: {hits or 'none'}; PE entry point 0x{ep:08x}{' = THIS FUNCTION' if ep == va else ''}")
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    size = min(int(r["size"] or 32), 40)
    try:
        code = pe.get_data(va - base, size)
        for ins in md.disasm(code, va):
            print(f"    0x{ins.address:08x}  {ins.mnemonic} {ins.op_str}")
    except Exception as e:
        print("  disasm failed", e)
