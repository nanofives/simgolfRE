"""S4.10, U-1415 part 2: for each thunk of 10_thunk_survey.tsv whose target is not ported, whether anything references the
thunk or its target: a `call` / `jmp` rel32 decoded inside a Ghidra function range, or an aligned dword in .rdata / .data
equal to its VA (a vtable or a stored pointer). Adds the column `refs` and prints the counts per class.

    ~/bin/py -3.12 log/roadmap/S4/10_thunk_refs.py
"""
import collections
import csv
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "test"))
import sound_emu as E  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402

data, relocs, secs, exps, entry, ib = E._raw(str(E.SOUND_DLL))
md = Cs(CS_ARCH_X86, CS_MODE_32)
code_refs = collections.Counter()
for r in csv.DictReader(open(ROOT / "re" / "functions_ghidra_sound.dll.tsv"), delimiter="\t"):
    for g in (r.get("ranges") or "").split(";"):
        if not g:
            continue
        lo, hi = (int(x, 16) for x in g.split("-"))
        if hi - lo <= 5 and data[lo] == 0xE9:      # a thunk itself
            continue
        for i in md.disasm(bytes(data[lo:hi]), lo):
            if i.mnemonic in ("call", "jmp") or i.mnemonic.startswith("j"):
                if i.op_str.startswith("0x"):
                    code_refs[int(i.op_str, 16)] += 1
data_refs = collections.Counter()
for va, vs, prot, name in secs:
    if name in (".rdata", ".data"):
        for a in range(va, va + vs - 3, 4):
            v = struct.unpack_from("<I", data, a)[0] - ib
            if 0 < v < len(data):
                data_refs[v] += 1
exp_rvas = set(exps.values()) if isinstance(exps, dict) else set()
p = ROOT / "log" / "roadmap" / "S4" / "10_thunk_survey.tsv"
rows = list(csv.DictReader(open(p), delimiter="\t"))
cls = collections.Counter()
for r in rows:
    th, t = int(r["thunk_rva"], 16), int(r["target_rva"], 16)
    n = code_refs[th] + code_refs[t] + data_refs[th] + data_refs[t] + (1 if th in exp_rvas or t in exp_rvas else 0)
    r["refs"] = str(n)
    cls[(r["class"], "referenced" if n else "unreferenced")] += 1
with open(p, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter="\t")
    w.writeheader()
    w.writerows(rows)
for k, v in sorted(cls.items()):
    print(k, v)
kind = collections.Counter()
for r in rows:
    if r["class"] == "NO_ROW" and r["refs"] != "0":
        th, t = int(r["thunk_rva"], 16), int(r["target_rva"], 16)
        kind["code" if code_refs[th] + code_refs[t] else "data only"] += 1
print("NO_ROW referenced by:", dict(kind))
print("NO_ROW with a code reference:", " ".join(r["target_rva"] for r in rows if r["class"] == "NO_ROW" and r["refs"] != "0"
      and code_refs[int(r["thunk_rva"], 16)] + code_refs[int(r["target_rva"], 16)]))
