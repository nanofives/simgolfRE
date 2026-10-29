"""Minimal read-only PDB 7.0 reader: info, DBI modules, procedures, publics, globals, UDT counts.
Treats the file as untrusted data: bounds-checked reads only."""
import struct, sys, collections, json, os

class PDB:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        magic = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0"
        assert self.d[:32] == magic, "not MSF 7.00"
        self.bs, _, self.nblocks, ndirb, _, bmap = struct.unpack_from("<6I", self.d, 32)
        nb = (ndirb + self.bs - 1) // self.bs
        dirblocks = struct.unpack_from("<%dI" % nb, self.d, bmap * self.bs)
        dd = b"".join(self.blk(b) for b in dirblocks)[:ndirb]
        n = struct.unpack_from("<I", dd, 0)[0]
        sizes = struct.unpack_from("<%dI" % n, dd, 4)
        pos = 4 + 4 * n
        self.streams = []
        for s in sizes:
            if s == 0xffffffff:
                self.streams.append(b""); continue
            k = (s + self.bs - 1) // self.bs
            bl = struct.unpack_from("<%dI" % k, dd, pos); pos += 4 * k
            self.streams.append(b"".join(self.blk(b) for b in bl)[:s])
    def blk(self, i):
        if i >= self.nblocks: raise ValueError("block out of range")
        return self.d[i * self.bs:(i + 1) * self.bs]

def cstr(b, o):
    e = b.index(b"\0", o)
    return b[o:e].decode("latin-1"), e + 1

def numleaf(b, o):
    v = struct.unpack_from("<H", b, o)[0]
    if v < 0x8000: return v, o + 2
    sz = {0x8000: 1, 0x8001: 2, 0x8002: 2, 0x8003: 4, 0x8004: 4, 0x8009: 8, 0x800a: 8}.get(v, 0)
    fmt = {1: "<b", 2: "<h", 4: "<i", 8: "<q"}[sz] if sz else None
    return (struct.unpack_from(fmt, b, o + 2)[0] if fmt else 0), o + 2 + sz

def analyse(path):
    p = PDB(path)
    out = {"file": os.path.basename(path), "streams": len(p.streams)}
    info = p.streams[1]
    ver, sig, age = struct.unpack_from("<3I", info, 0)
    out["info"] = {"version": ver, "signature": hex(sig), "age": age, "guid": info[12:28].hex()}
    dbi = p.streams[3]
    (vsig, vhdr, dage, gsi, build, psi, dllv, symrec, rbld, modsz, sczsz, smsz, srcsz, tsmsz, mfc, dbgsz, ecsz,
     flags, machine) = struct.unpack_from("<iIIHHHHHHiiiiiIiiHH", dbi, 0)
    out["dbi"] = {"version": vhdr, "build": hex(build), "machine": hex(machine)}
    # modules
    mods = []
    o = 64; end = 64 + modsz
    while o < end:
        sc_sec, = struct.unpack_from("<H", dbi, o + 4)
        sc_off, sc_size = struct.unpack_from("<iI", dbi, o + 8)
        mflags, symstream, symbytes, c11, c13, nsrc = struct.unpack_from("<HHIIIH", dbi, o + 32)
        name, q = cstr(dbi, o + 64)
        obj, q = cstr(dbi, q)
        mods.append({"name": name, "obj": obj, "symstream": symstream, "symbytes": symbytes, "c11": c11, "c13": c13, "nsrc": nsrc})
        o = (q + 3) & ~3
    out["modules"] = len(mods)
    # section headers from optional debug header
    dbgo = 64 + modsz + sczsz + smsz + srcsz + tsmsz + ecsz
    dbgidx = struct.unpack_from("<%dH" % (dbgsz // 2), dbi, dbgo)
    secs = []
    if len(dbgidx) > 5 and dbgidx[5] != 0xffff:
        sh = p.streams[dbgidx[5]]
        for i in range(len(sh) // 40):
            nm = sh[i * 40:i * 40 + 8].rstrip(b"\0").decode("latin-1")
            va = struct.unpack_from("<I", sh, i * 40 + 12)[0]
            secs.append((nm, va))
    out["sections"] = secs
    def rva(seg, off):
        return secs[seg - 1][1] + off if 0 < seg <= len(secs) else None
    # module symbols
    kinds = collections.Counter(); procs = []; locals_ = 0
    for m in mods:
        if m["symstream"] == 0xffff: continue
        s = p.streams[m["symstream"]]
        o = 4
        while o + 4 <= m["symbytes"]:
            ln, kind = struct.unpack_from("<HH", s, o)
            if ln < 2: break
            kinds[kind] += 1
            if kind in (0x1110, 0x110f):  # S_GPROC32 / S_LPROC32
                ln_ = struct.unpack_from("<I", s, o + 4 + 12)[0]
                typ, off, seg = struct.unpack_from("<IIH", s, o + 4 + 24)
                nm, _ = cstr(s, o + 4 + 35)
                procs.append((rva(seg, off), ln_, nm, m["obj"]))
            if kind in (0x1111, 0x110b, 0x1006, 0x113e, 0x1012):  # regrel / bprel / local
                locals_ += 1
            o += 2 + ln
    out["module_symbol_kinds"] = {hex(k): v for k, v in kinds.most_common(12)}
    out["procedures"] = len(procs)
    out["locals_and_params"] = locals_
    out["modules_with_lines_c13"] = sum(1 for m in mods if m["c13"])
    out["modules_with_lines_c11"] = sum(1 for m in mods if m["c11"])
    # publics / globals in symbol record stream
    sr = p.streams[symrec]
    gk = collections.Counter(); pubs = []
    o = 0
    while o + 4 <= len(sr):
        ln, kind = struct.unpack_from("<HH", sr, o)
        if ln < 2: break
        gk[kind] += 1
        if kind == 0x110e:
            fl, off, seg = struct.unpack_from("<IIH", sr, o + 4)
            nm, _ = cstr(sr, o + 14)
            pubs.append((rva(seg, off), nm))
        o += 2 + ln
    out["symrec_kinds"] = {hex(k): v for k, v in gk.most_common(10)}
    out["publics"] = len(pubs)
    # types
    tpi = p.streams[2]
    hsz, tib, tie, trb = struct.unpack_from("<4I", tpi, 4)
    o = hsz; tk = collections.Counter(); udts = []
    while o + 4 <= hsz + trb:
        ln, kind = struct.unpack_from("<HH", tpi, o)
        tk[kind] += 1
        if kind in (0x1504, 0x1505):
            cnt, props = struct.unpack_from("<HH", tpi, o + 4)
            size, q = numleaf(tpi, o + 4 + 16)
            nm, _ = cstr(tpi, q)
            if not props & 0x80:
                udts.append((nm, size, cnt))
        elif kind == 0x1506:
            cnt, props = struct.unpack_from("<HH", tpi, o + 4)
            size, q = numleaf(tpi, o + 4 + 8)
            nm, _ = cstr(tpi, q)
            if not props & 0x80:
                udts.append((nm, size, cnt))
        o += 2 + ln
    out["types"] = tie - tib
    out["type_kinds"] = {hex(k): v for k, v in tk.most_common(8)}
    out["udt_definitions"] = len(udts)
    return out, procs, pubs, mods, udts

if __name__ == "__main__":
    outdir = sys.argv[1]
    for path in sys.argv[2:]:
        out, procs, pubs, mods, udts = analyse(path)
        b = os.path.basename(path).split(".")[0]
        json.dump(out, open(os.path.join(outdir, b + ".pdbinfo.json"), "w"), indent=1)
        with open(os.path.join(outdir, b + ".procs.tsv"), "w", encoding="utf-8") as f:
            for r, ln, nm, obj in sorted(procs, key=lambda x: (x[0] or 0)):
                f.write("%s\t%d\t%s\t%s\n" % (hex(r) if r is not None else "-", ln, nm, obj))
        with open(os.path.join(outdir, b + ".publics.tsv"), "w", encoding="utf-8") as f:
            for r, nm in sorted(pubs, key=lambda x: (x[0] or 0)):
                f.write("%s\t%s\n" % (hex(r) if r is not None else "-", nm))
        with open(os.path.join(outdir, b + ".modules.tsv"), "w", encoding="utf-8") as f:
            for m in mods:
                f.write("%s\t%s\t%d\t%d\n" % (m["name"], m["obj"], m["symbytes"], m["c13"] + m["c11"]))
        with open(os.path.join(outdir, b + ".udts.tsv"), "w", encoding="utf-8") as f:
            for nm, size, cnt in sorted(set(udts)):
                f.write("%s\t%d\t%d\n" % (nm, size, cnt))
        print(json.dumps({k: v for k, v in out.items() if k != "sections"}))
        print("  sections", out["sections"])
