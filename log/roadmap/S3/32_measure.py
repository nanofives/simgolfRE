"""S3.32 measurement: static objects in .data that hold constant image words (.rdata / .text) in EVERY segment start image
while the port's boot leaves them as the file's initial data (0 in the bss).

    ~/bin/py -3.12 log/roadmap/S3/32_measure.py static            # segment images vs the file's initialised .data
    ~/bin/py -3.12 log/roadmap/S3/32_measure.py boot <scenario>... # + the port's boot state (SG_PORTSIM_DLL) at record 0

Writes log/roadmap/S3/32_logs/const_words.tsv (static) and boot_<scenario>.tsv. Run from the repo root; the tree is
port/build/S3_32 (its corpus junctions)."""
from __future__ import annotations

import collections
import json
import os
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
TREE = ROOT / "port" / "build" / os.environ.get("S3_32_TREE", "S3_32")
OUT = ROOT / "log" / "roadmap" / "S3" / "32_logs"
EXE = ROOT / "original" / "golf_clean.exe"
DATA_VA = 0x004C1000


def sections():
    import pefile
    pe = pefile.PE(str(EXE), fast_load=True)
    out = {}
    for s in pe.sections:
        n = s.Name.rstrip(b"\0").decode()
        out[n] = (0x400000 + s.VirtualAddress, s.Misc_VirtualSize, s.PointerToRawData, s.SizeOfRawData)
    return out, EXE.read_bytes()


def objects():
    rows = []
    for m in re.finditer(r"^SG_OBJECT\((\w+), 0x([0-9a-f]+), 0x([0-9a-f]+), (\w+), (\w+), ([\w:+]+), (\d)\)",
                         (TREE / "port" / "include" / "gamestate_meta.inc").read_text(), re.M):
        rows.append((int(m.group(2), 16), int(m.group(3), 16), m.group(1), m.group(4), m.group(5)))
    rows.sort()
    return rows


def obj_at(rows, off):
    lo, hi = 0, len(rows)
    while lo < hi:
        mid = (lo + hi) // 2
        if rows[mid][0] <= off:
            lo = mid + 1
        else:
            hi = mid
    if lo and rows[lo - 1][0] <= off < rows[lo - 1][0] + rows[lo - 1][1]:
        return rows[lo - 1]
    return None


def segment_images():
    segs = sorted((TREE / "port" / "corpus" / "segments").glob("*/*/start.data.bin"))
    return segs


def static():
    secs, exe = sections()
    va, vsize, raw, rsize = secs[".data"]
    size = None
    init = bytearray(exe[raw:raw + rsize])
    segs = segment_images()
    imgs = [p.read_bytes() for p in segs]
    size = len(imgs[0])
    assert all(len(b) == size for b in imgs)
    init += bytes(size - len(init))
    rd = secs[".rdata"]
    tx = secs[".text"]
    def kind(v):
        if rd[0] <= v < rd[0] + rd[1]:
            return "rdata"
        if tx[0] <= v < tx[0] + tx[1]:
            return "text"
        return None
    rows = objects()
    import numpy as np
    A = np.frombuffer(b"".join(imgs), dtype="<u4").reshape(len(imgs), size // 4)
    I = np.frombuffer(bytes(init), dtype="<u4")
    const = np.all(A == A[0], axis=0)
    cand = np.nonzero(const & (A[0] != I))[0]
    out = []
    for w in cand:
        v = int(A[0][w])
        k = kind(v)
        if not k:
            continue
        off = int(w) * 4
        o = obj_at(rows, off)
        out.append((off, DATA_VA + off, v, k, int(I[w]), o[2] if o else "-", (off - o[0]) if o else -1,
                    o[3] if o else "gap", o[4] if o else "-"))
    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / "const_words.tsv", "w") as f:
        f.write("gs_off\tva\tvalue\tsection\tfile_init\tobject\tobj_off\tclass\tpointer\n")
        for r in out:
            f.write(f"0x{r[0]:06x}\t0x{r[1]:08x}\t0x{r[2]:08x}\t{r[3]}\t0x{r[4]:08x}\t{r[5]}\t0x{r[6]:x}\t{r[7]}\t{r[8]}\n")
    by = collections.Counter((r[5], r[7], r[3]) for r in out)
    print(f"{len(imgs)} segment images, {len(out)} constant image words not in the file's initial data")
    for (o, c, k), n in sorted(by.items(), key=lambda x: -x[1]):
        print(f"  {n:5d}  {k:5s}  {c:8s}  {o}")
    return out


def boot(scens):
    sys.path.insert(0, str(TREE))
    from port.test import boot as B
    rows = {int(l.split("\t")[0], 16): l.rstrip("\n").split("\t") for l in
            open(OUT / "const_words.tsv").readlines()[1:]}
    for sc in scens:
        got = {}
        def after(st, res):
            img = st.image()
            for off in rows:
                got[off] = struct.unpack_from("<I", img, off)[0]
        r = B.run(sc, max_records=1, after=after)
        diff = [(off, rows[off]) for off in rows if got.get(off) != int(rows[off][2], 16)]
        with open(OUT / f"boot_{sc}.tsv", "w") as f:
            f.write("gs_off\tva\tvalue\tport\tobject\tobj_off\tclass\n")
            for off, r_ in diff:
                f.write(f"{r_[0]}\t{r_[1]}\t{r_[2]}\t0x{got.get(off, 0):08x}\t{r_[5]}\t{r_[6]}\t{r_[7]}\n")
        by = collections.Counter((r_[5], r_[7]) for _, r_ in diff)
        print(f"{sc}: boot {r.status} records {r.records}; {len(diff)} of {len(rows)} words differ from every segment image")
        for (o, c), n in sorted(by.items(), key=lambda x: -x[1]):
            print(f"  {n:5d}  {c:8s}  {o}")


SURFACE_CTOR = 0x00473AB0      # Surface_ctor (re/names/exe_2.tsv)
VEC_CTOR = 0x004A55D2          # ??_L, eh vector constructor iterator (ptr, size, count, ctor, dtor)
XC_A, XC_Z = 0x004C1000, 0x004C1188   # __cinit 0x004a50db: push 0x4c1188; push 0x4c1000; call _initterm 0x004a51e1
GAME_END = 0x004A4ED0          # game code 0x401000..0x4a4ecf (PORT PIVOT survey range); the static CRT follows


def ctors():
    """Every Surface_ctor construction the C++ initializer table reaches: `mov ecx, imm32; call Surface_ctor` and
    ??_L(imm32, size, count, Surface_ctor, dtor), followed through direct calls / jmps into game code (depth 4)."""
    import capstone
    import pefile
    pe = pefile.PE(str(EXE), fast_load=True)
    img = pe.get_memory_mapped_image()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    d32 = lambda va: struct.unpack_from("<I", img, va - 0x400000)[0]
    found = {}      # array va -> (count, slot, site)
    nested = {}     # Surface sub-object va in element 0 -> (count, stride, slot, ??_L site, Surface_ctor site)
    starts = {int(l.split("\t")[0], 16) for l in open(ROOT / 're' / 'functions_ghidra.tsv').readlines()[1:]}
    seen = set()
    other_ctor_calls = collections.Counter()

    def walk(fn, slot, depth, this=None):
        if (fn, this) in seen or depth > 4 or not (0x401000 <= fn < GAME_END):
            return
        seen.add((fn, this))
        pushes, regs = [], {"ecx": this}
        for i in md.disasm(img[fn - 0x400000: fn - 0x400000 + 0x400], fn):
            m, op = i.mnemonic, i.op_str
            mm = re.fullmatch(r"(e[a-z]{2}), (0x[0-9a-f]+|[0-9]+)", op)
            if m == "push" and re.fullmatch(r"0x[0-9a-f]+|[0-9]+", op):
                pushes.append(int(op, 0))
            elif m == "push" and re.fullmatch(r"e[a-z]{2}", op):
                pushes.append(regs.get(op))
            elif m == "push":
                pushes.append(None)
            elif m == "mov" and mm:
                regs[mm.group(1)] = int(mm.group(2), 0)
            elif m == "mov" and re.fullmatch(r"e[a-z]{2}, e[a-z]{2}", op):
                a, b = op.split(", ")
                regs[a] = regs.get(b)
            elif m == "lea" and re.fullmatch(r"e[a-z]{2}, \[e[a-z]{2} \+ (0x[0-9a-f]+|[0-9]+)\]", op):
                a, rest = op.split(", ")
                b, k = rest[1:-1].split(" + ")
                regs[a] = None if regs.get(b) is None else regs[b] + int(k, 0)
            elif m in ("call", "jmp") and op.startswith("0x"):
                t = int(op, 16)
                ecx = regs.get("ecx")
                if t == SURFACE_CTOR:
                    if ecx is None:
                        other_ctor_calls[(slot, i.address)] += 1
                    else:
                        found[ecx] = (1, slot, i.address)
                elif t == VEC_CTOR and len(pushes) >= 5 and pushes[-4] == SURFACE_CTOR and None in (pushes[-1], pushes[-3]):
                    other_ctor_calls[(slot, i.address)] += 1
                elif t == VEC_CTOR and len(pushes) >= 5 and pushes[-4] == SURFACE_CTOR:
                    found[pushes[-1]] = (pushes[-3], slot, i.address)
                    assert pushes[-2] == 0x2C, hex(i.address)
                elif t == VEC_CTOR and len(pushes) >= 5 and None not in pushes[-4:]:
                    # ??_L(base, size, count, ctor, dtor) with another element constructor: its Surface_ctor calls on
                    # element 0, repeated at every element (stride `size`)
                    base, size, n, ector = pushes[-1], pushes[-2], pushes[-3], pushes[-4]
                    before = set(found)
                    walk(ector, slot, depth + 1, base)
                    for va in sorted(set(found) - before):
                        cnt, _, site = found.pop(va)
                        assert cnt == 1, hex(va)
                        nested[va] = (n, size, slot, i.address, site)
                elif m == "call" or (m == "jmp" and (t in starts or not (fn <= t < fn + 0x400))):
                    walk(t, slot, depth + 1, ecx)
                if m == "call":
                    pushes, regs = [], {"esi": regs.get("esi"), "edi": regs.get("edi"), "ebx": regs.get("ebx")}
                if m == "jmp" and (t in starts or not (fn <= t < fn + 0x400)):
                    return
            elif m == "ret":
                return
            elif m not in ("push", "nop", "test", "cmp") and op.split(",")[0].strip() in regs:
                regs[op.split(",")[0].strip()] = None

    for slot in range(XC_A + 4, XC_Z, 4):
        if d32(slot):
            walk(d32(slot), slot, 0)
    rows = objects()
    out = []
    for va in sorted(set(found) | set(nested)):
        if va in found:
            (n, slot, site), stride = found[va], 0x2C
        else:
            n, stride, slot, site, _ = nested[va]
        o = obj_at(rows, va - DATA_VA)
        out.append((va, n, slot, site, o, stride))
    with open(OUT / "static_surfaces.tsv", "w") as f:
        f.write("va\tcount\tstride\tinit_slot\tsite\tobject\tobj_off\tobj_size\tclass\n")
        for va, n, slot, site, o, stride in out:
            f.write(f"0x{va:08x}\t{n}\t0x{stride:x}\t0x{slot:08x}\t0x{site:08x}\t{o[2] if o else '-'}\t"
                    f"0x{(va - DATA_VA - o[0]) if o else 0:x}\t0x{o[1] if o else 0:x}\t{o[3] if o else 'gap'}\n")
    print(f"{len(out)} statically constructed Surface objects / arrays, {sum(r[1] for r in out)} elements; "
          f"Surface_ctor calls without a constant ecx: {len(other_ctor_calls)}")
    for va, n, slot, site, o, stride in out:
        exact = o and o[0] == va - DATA_VA and o[1] == n * 0x2C
        print(f"  0x{va:08x} x{n:3d} /0x{stride:x}  slot 0x{slot:08x} site 0x{site:08x}  {o[2] if o else '-'} "
              f"{'(exact)' if exact else '(+0x%x of 0x%x)' % ((va - DATA_VA - o[0]) if o else 0, o[1] if o else 0)}")
    for (slot, site), c in other_ctor_calls.items():
        print(f"  no constant this: slot 0x{slot:08x} site 0x{site:08x}")
    return out


def crosscheck():
    """static_surfaces.tsv (the elements the initializers construct) against const_words.tsv (the words every segment
    image holds): every constructed element's +0 must be Surface_ctor's vtable word in all 205 images."""
    A = set()
    for l in open(OUT / "static_surfaces.tsv").readlines()[1:]:
        f = l.split("\t")
        va, n, stride = int(f[0], 16), int(f[1]), int(f[2], 16)
        A.update(va + i * stride for i in range(n))
    rows = [l.rstrip("\n").split("\t") for l in open(OUT / "const_words.tsv")][1:]
    vt = collections.Counter(r[2] for r in rows if int(r[1], 16) in A)
    B = {int(r[1], 16): r for r in rows if r[2] == "0x004ba2d8"}
    print(f"constructed elements {len(A)}; segment-constant 0x004ba2d8 words {len(B)}")
    print(f"constructed elements whose +0 is 0x004ba2d8 in all 205 images: {len(A & set(B))}; other: {len(A - set(B))} "
          f"(values at constructed +0 words: {dict(vt)})")
    extra = sorted(set(B) - A)
    print(f"0x004ba2d8 words no static initializer stores: {len(extra)}")
    for k, v in collections.Counter(B[a][5] for a in extra).most_common():
        print(f"   {v:5d} {k}")


def neighbours(reach=0x400):
    """For every sim-class object, the constructed Surface vtable words within `reach` bytes after its end and before
    its start (the bytes an index past either end of the object reads)."""
    words = []
    for l in open(OUT / "static_surfaces.tsv").readlines()[1:]:
        f = l.split("\t")
        va, n, stride = int(f[0], 16), int(f[1]), int(f[2], 16)
        words += [va + i * stride - DATA_VA for i in range(n)]
    words.sort()
    import bisect
    rows = objects()
    out = []
    for off, size, name, cls, ptr in rows:
        if cls != "sim":
            continue
        end = off + size
        a = bisect.bisect_left(words, end)
        after = [w for w in words[a:a + 64] if w < end + reach]
        b = bisect.bisect_left(words, off - reach)
        before = [w for w in words[b:b + 64] if w < off]
        if after or before:
            out.append((name, off, size, after, before))
    with open(OUT / "sim_neighbours.tsv", "w") as f:
        f.write("object\tva\tsize\tfirst_after (bytes past end)\tn_after\tlast_before (bytes before start)\tn_before\n")
        for name, off, size, after, before in out:
            f.write(f"{name}\t0x{DATA_VA + off:08x}\t0x{size:x}\t{(after[0] - off - size) if after else '-'}\t{len(after)}\t"
                    f"{(off - before[-1]) if before else '-'}\t{len(before)}\n")
    print(f"sim objects with a constructed Surface vtable word within 0x{reach:x} bytes: {len(out)}")
    for name, off, size, after, before in out:
        print(f"  {name:28s} 0x{DATA_VA + off:08x} size 0x{size:5x}  after: "
              f"{'+%d (%d words)' % (after[0] - off - size, len(after)) if after else '-':18s} before: "
              f"{'-%d (%d words)' % (off - before[-1], len(before)) if before else '-'}")


if __name__ == "__main__":
    if sys.argv[1] == "static":
        static()
    elif sys.argv[1] == "ctors":
        ctors()
    elif sys.argv[1] == "crosscheck":
        crosscheck()
    elif sys.argv[1] == "neighbours":
        neighbours()
    else:
        boot(sys.argv[2:])
