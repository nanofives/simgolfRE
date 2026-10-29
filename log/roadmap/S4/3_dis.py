"""S4.3: disassemble the S4.3 rows of sound.dll (original/sound.dll, image base 0x10000000) into log/roadmap/S4/3_dis/<rva>_<name>.asm
and print per function: size, frame bytes (sub esp), locals passed by address (lea r,[ebp-x] ... push r), call targets."""
import csv, pathlib, re, sys
import pefile, capstone
ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = ROOT / "log/roadmap/S4/3_dis"
pe = pefile.PE(str(ROOT / "original/sound.dll"))
img = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
rows = [r for r in csv.DictReader(open(ROOT / "re/roadmap/S4_functions.tsv"), delimiter="\t") if r["step"] == "S4.3"]
fl = {}
for r in csv.DictReader(open(ROOT / "re/functions_ghidra_sound.dll.tsv"), delimiter="\t"):
    fl[int(r["entry"], 16)] = r
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
for r in rows:
    rva = int(r["rva"], 16)
    va = base + rva
    f = fl.get(va) or fl.get(rva)
    size = int(f["size"]) if f else 0x400
    code = img[rva:rva + size]
    lines, frame, leas, calls = [], 0, set(), []
    last_lea = {}
    for ins in md.disasm(code, va):
        lines.append(f"{ins.address:08x}  {ins.mnemonic} {ins.op_str}")
        if ins.mnemonic == "sub" and ins.op_str.startswith("esp,") and not frame:
            frame = int(ins.op_str.split(",")[1], 0)
        m = re.match(r"(\w+), (?:dword ptr )?\[ebp - (0x[0-9a-f]+|\d+)\]", ins.op_str)
        if ins.mnemonic == "lea" and m:
            last_lea[m.group(1)] = int(m.group(2), 0)
        if ins.mnemonic == "push" and ins.op_str in last_lea:
            leas.add(last_lea[ins.op_str])
        if ins.mnemonic == "call":
            calls.append(ins.op_str)
    (OUT / f"{rva:05x}_{r['name']}.asm").write_text("\n".join(lines) + "\n")
    print(f"{r['rva']} {r['name']:28s} size {size:5d} frame {frame:#6x} locals-by-addr {sorted(hex(x) for x in leas)} calls {len(calls)}")
