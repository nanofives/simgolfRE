"""S11.1: library-object identity of sound.dll strcpy 0x1004a010 at its own 7-byte extent and over the code it runs.

Reads strcat.obj out of VC6 SP5 LIBCMT.LIB (the library S10.7 matched sound.dll's CRT against), takes the .text section
that holds `_strcpy`, and compares, byte for byte, (1) the 7 bytes of strcpy's entry (our function extent, re/match
boundaries: strcpy's entry falls into the copy loop it shares with strcat) and (2) every byte from `_strcpy` to the end of
that section, against sound.dll at RVA 0x4a010. Prints the obj's relocations inside the span (the comparison is only
meaningful on bytes no relocation patches) and the targets of every branch in the strcpy path, which must stay inside
the compared span. Writes log/roadmap/S11/1_strcpy_lib.csv in match.py's CSV layout (16-byte rows, VERDICT line).
Run: ~/bin/py -3.12 log/roadmap/S11/1_strcpy_lib.py
"""
import pathlib
import struct
import sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[3]
LIB = ROOT / "tools" / "vc6sp5" / "VC98" / "lib" / "LIBCMT.LIB"
DLL = ROOT / "original" / "sound.dll"
RVA = 0x4A010
EXTENT = 7
OUT = ROOT / "log" / "roadmap" / "S11" / "1_strcpy_lib.csv"


def members(blob: bytes):
    """(name, data) for every member of a COFF archive (`!<arch>`), long names resolved through the `//` member."""
    assert blob[:8] == b"!<arch>\n"
    pos, longnames = 8, b""
    while pos + 60 <= len(blob):
        hdr = blob[pos:pos + 60]
        name = hdr[:16].decode("latin-1").rstrip()
        size = int(hdr[48:58].decode().strip())
        data = blob[pos + 60:pos + 60 + size]
        if name == "//":
            longnames = data
        elif name.startswith("/") and name[1:].isdigit():
            off = int(name[1:])
            end = longnames.find(b"\0", off)
            end2 = longnames.find(b"\n", off)
            end = min(e for e in (end, end2) if e >= 0) if (end >= 0 or end2 >= 0) else len(longnames)
            name = longnames[off:end].decode("latin-1").rstrip("/")
        yield name, data
        pos += 60 + size + (size & 1)


def coff(data: bytes):
    nsec = struct.unpack_from("<H", data, 2)[0]
    symptr, nsym = struct.unpack_from("<II", data, 8)
    opt = struct.unpack_from("<H", data, 16)[0]
    strtab = symptr + nsym * 18
    secs = []
    for i in range(nsec):
        o = 20 + opt + i * 40
        name = data[o:o + 8].rstrip(b"\0").decode("latin-1")
        size, rawptr, relptr, _, nrel = struct.unpack_from("<IIIIH", data, o + 16)
        raw = data[rawptr:rawptr + size] if rawptr else b"\0" * size
        rels = [struct.unpack_from("<IIH", data, relptr + k * 10) for k in range(nrel)]
        secs.append({"name": name, "raw": raw, "rels": rels})
    syms, k = [], 0
    while k < nsym:
        o = symptr + k * 18
        if data[o:o + 4] == b"\0\0\0\0":
            soff = struct.unpack_from("<I", data, o + 4)[0]
            end = data.find(b"\0", strtab + soff)
            name = data[strtab + soff:end].decode("latin-1")
        else:
            name = data[o:o + 8].rstrip(b"\0").decode("latin-1")
        value, secno, typ, cls, naux = struct.unpack_from("<IhHBB", data, o + 8)
        syms.append({"name": name, "value": value, "sec": secno, "type": typ, "cls": cls})
        k += 1 + naux
    return secs, syms


def main() -> int:
    obj = next(d for n, d in members(LIB.read_bytes()) if n.lower().endswith("strcat.obj"))
    secs, syms = coff(obj)
    s = next(x for x in syms if x["name"] == "_strcpy" and x["sec"] > 0)
    sec = secs[s["sec"] - 1]
    lib = sec["raw"][s["value"]:]
    pe = pefile.PE(str(DLL), fast_load=True)
    orig = pe.get_data(RVA, len(lib))
    rels_in = [r for r in sec["rels"] if r[0] >= s["value"]]
    print(f"strcat.obj section {s['sec']} {sec['name']}: _strcpy at +{s['value']:#x}, span {len(lib):#x} bytes, "
          f"relocations in span: {len(rels_in)}")
    entry_eq = lib[:EXTENT] == orig[:EXTENT]
    print(f"entry {EXTENT} bytes: lib {lib[:EXTENT].hex()} sound.dll {orig[:EXTENT].hex()} equal={entry_eq}")
    # branches of the strcpy path: decode linearly from the entry over the whole span, list every jump target
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    va = pe.OPTIONAL_HEADER.ImageBase + RVA
    out_of_span = []
    for ins in md.disasm(orig, va):
        if ins.mnemonic.startswith("j") and ins.op_str.startswith("0x"):
            t = int(ins.op_str, 16)
            if not (va <= t < va + len(lib)):
                out_of_span.append((ins.address, t))
    first = next(md.disasm(orig[5:7], va + 5))
    print(f"strcpy's jump: {first.address:#x} {first.mnemonic} {first.op_str}; branch targets outside the span: "
          f"{[(hex(a), hex(t)) for a, t in out_of_span]}")
    rows, same = [], 0
    for off in range(0, len(lib), 16):
        a, b = lib[off:off + 16], orig[off:off + 16]
        ok = a == b
        same += ok
        rows.append(f"{off // 16},bytes +{off:#x}: {a.hex()},bytes +{off:#x}: {b.hex()},{ok}")
    n = len(rows)
    green = entry_eq and same == n and not rels_in and not out_of_span
    with OUT.open("w", newline="\n") as f:
        f.write("instruction,library_object,original,match\n")
        f.write("\n".join(rows) + "\n")
        f.write(f"meta,\"library=tools/vc6sp5/VC98/lib/LIBCMT.LIB member=strcat.obj (library object, no compilation; "
                f"entry {EXTENT} bytes = our extent, rows = _strcpy to the end of its section, which holds the shared copy "
                f"loop strcpy jumps into; relocations in span {len(rels_in)})\",symbol=_strcpy,module=sound.dll\n")
        f.write(f"VERDICT,{'GREEN' if green else 'RED'},{n},{100.0 * same / n:.1f}%\n")
    print(f"rows {same}/{n} equal -> {'GREEN' if green else 'RED'}; wrote {OUT.relative_to(ROOT)}")
    return 0 if green else 1


if __name__ == "__main__":
    sys.exit(main())
