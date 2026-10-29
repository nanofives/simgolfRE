"""Read-only reader of SMSG-decomp's release PDBs (third_party/smsg/release/*.pdb, MSF 7.00), roadmap S10.

Promoted from log/roadmap/S10/pdb/pdbread.py (same parsing, bounds-checked reads; the files are untrusted data).
What S10 needs from a PDB: every procedure (S_GPROC32 / S_LPROC32) with its RVA, length, undecorated name and
the DBI module (object file) and library it came from, plus the publics (RVA -> decorated name).

    ~/bin/py -3.12 re/tools/smsg_pdb.py procs Terrain [--obj Tile.obj]      # rva size kind name module lib
    ~/bin/py -3.12 re/tools/smsg_pdb.py publics golf
    ~/bin/py -3.12 re/tools/smsg_pdb.py modules jgld

Module keys: golf, jgld, sound, Terrain (the PDB file stems). `module` is the DBI module name (the .obj path as the
linker saw it, e.g. `.\\Release\\FlatFrame.obj`); `lib` is the archive it came from (or the .obj itself).
"""
from __future__ import annotations

import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PDB_DIR = ROOT / "third_party" / "smsg" / "release"
# our module name (re/tools/match.py, hooks.csv) for each PDB
OUR_MODULE = {"golf": "golf_clean.exe", "jgld": "jgld.dll", "sound": "sound.dll", "Terrain": "Terrain.dll"}
IMAGE_BASE = {"golf": 0x00400000, "jgld": 0x10000000, "sound": 0x10000000, "Terrain": 0x10000000}


class Msf:
    """MSF 7.00 container: `streams[i]` is the bytes of stream i."""

    MAGIC = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0"

    def __init__(self, path: pathlib.Path):
        self.d = path.read_bytes()
        if self.d[:32] != self.MAGIC:
            raise ValueError(f"{path}: not an MSF 7.00 PDB")
        self.bs, _, self.nblocks, ndirb, _, bmap = struct.unpack_from("<6I", self.d, 32)
        nb = (ndirb + self.bs - 1) // self.bs
        dirblocks = struct.unpack_from("<%dI" % nb, self.d, bmap * self.bs)
        dd = b"".join(self._blk(b) for b in dirblocks)[:ndirb]
        n = struct.unpack_from("<I", dd, 0)[0]
        sizes = struct.unpack_from("<%dI" % n, dd, 4)
        pos = 4 + 4 * n
        self.streams = []
        for s in sizes:
            if s == 0xFFFFFFFF:
                self.streams.append(b"")
                continue
            k = (s + self.bs - 1) // self.bs
            bl = struct.unpack_from("<%dI" % k, dd, pos)
            pos += 4 * k
            self.streams.append(b"".join(self._blk(b) for b in bl)[:s])

    def _blk(self, i: int) -> bytes:
        if i >= self.nblocks:
            raise ValueError("block out of range")
        return self.d[i * self.bs:(i + 1) * self.bs]


def _cstr(b: bytes, o: int) -> tuple[str, int]:
    e = b.index(b"\0", o)
    return b[o:e].decode("latin-1"), e + 1


class Pdb:
    """procs: [{rva, size, kind 'G'|'L', name, module, lib}] sorted by rva; publics: [(rva, decorated)];
    modules: [{module, lib, symbytes}]. RVAs of symbols outside every section are None."""

    def __init__(self, key: str, path: pathlib.Path | None = None):
        self.key = key
        msf = Msf(path or PDB_DIR / f"{key}.pdb")
        dbi = msf.streams[3]
        (_, _, _, _, _, _, _, symrec, _, modsz, sczsz, smsz, srcsz, tsmsz, _, dbgsz, ecsz,
         _, _) = struct.unpack_from("<iIIHHHHHHiiiiiIiiHH", dbi, 0)
        self.modules, raw = [], []
        o, end = 64, 64 + modsz
        while o < end:
            symstream, symbytes = struct.unpack_from("<HI", dbi, o + 34)
            name, q = _cstr(dbi, o + 64)
            lib, q = _cstr(dbi, q)
            self.modules.append({"module": name, "lib": lib, "symbytes": symbytes})
            raw.append((symstream, symbytes, name, lib))
            o = (q + 3) & ~3
        dbgo = 64 + modsz + sczsz + smsz + srcsz + tsmsz + ecsz
        dbgidx = struct.unpack_from("<%dH" % (dbgsz // 2), dbi, dbgo)
        self.sections = []
        if len(dbgidx) > 5 and dbgidx[5] != 0xFFFF:
            sh = msf.streams[dbgidx[5]]
            for i in range(len(sh) // 40):
                nm = sh[i * 40:i * 40 + 8].rstrip(b"\0").decode("latin-1")
                self.sections.append((nm, struct.unpack_from("<I", sh, i * 40 + 12)[0]))
        self.procs = []
        for symstream, symbytes, name, lib in raw:
            if symstream == 0xFFFF:
                continue
            s, o = msf.streams[symstream], 4
            while o + 4 <= symbytes:
                ln, kind = struct.unpack_from("<HH", s, o)
                if ln < 2:
                    break
                if kind in (0x1110, 0x110F):            # S_GPROC32 / S_LPROC32
                    size = struct.unpack_from("<I", s, o + 16)[0]
                    off, seg = struct.unpack_from("<IH", s, o + 32)
                    nm, _ = _cstr(s, o + 39)
                    self.procs.append({"rva": self._rva(seg, off), "size": size, "kind": "G" if kind == 0x1110 else "L",
                                       "name": nm, "module": name, "lib": lib})
                o += 2 + ln
        self.procs.sort(key=lambda p: (p["rva"] is None, p["rva"] or 0))
        self.publics = []
        sr, o = msf.streams[symrec], 0
        while o + 4 <= len(sr):
            ln, kind = struct.unpack_from("<HH", sr, o)
            if ln < 2:
                break
            if kind == 0x110E:                          # S_PUB32
                off, seg = struct.unpack_from("<IH", sr, o + 8)
                self.publics.append((self._rva(seg, off), _cstr(sr, o + 14)[0]))
            o += 2 + ln
        self.publics.sort(key=lambda p: (p[0] is None, p[0] or 0))

    def _rva(self, seg: int, off: int) -> int | None:
        return self.sections[seg - 1][1] + off if 0 < seg <= len(self.sections) else None

    def public_names(self) -> dict[int, list[str]]:
        """rva -> decorated public names (several when the linker folded identical functions)."""
        out: dict[int, list[str]] = {}
        for rva, nm in self.publics:
            if rva is not None:
                out.setdefault(rva, []).append(nm)
        return out

    def procs_of(self, obj_name: str) -> list[dict]:
        """Procedures whose DBI module's file name is `obj_name` (case-insensitive, e.g. `Tile.obj`)."""
        want = obj_name.lower()
        return [p for p in self.procs if p["rva"] is not None
                and p["module"].replace("/", "\\").rsplit("\\", 1)[-1].lower() == want]


_CACHE: dict[str, Pdb] = {}


def load(key: str) -> Pdb:
    if key not in _CACHE:
        _CACHE[key] = Pdb(key)
    return _CACHE[key]


def main(argv: list[str]) -> int:
    import argparse
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what", choices=["procs", "publics", "modules"])
    ap.add_argument("pdb", choices=sorted(OUR_MODULE))
    ap.add_argument("--obj", default="", help="only procedures of this object file (procs)")
    a = ap.parse_args(argv)
    p = load(a.pdb)
    out = sys.stdout
    if a.what == "procs":
        rows = p.procs_of(a.obj) if a.obj else p.procs
        for r in rows:
            rva = "-" if r["rva"] is None else f"0x{r['rva']:x}"
            out.write(f"{rva}\t{r['size']}\t{r['kind']}\t{r['name']}\t{r['module']}\t{r['lib']}\n")
    elif a.what == "publics":
        for rva, nm in p.publics:
            out.write(f"{'-' if rva is None else hex(rva)}\t{nm}\n")
    else:
        for m in p.modules:
            out.write(f"{m['module']}\t{m['lib']}\t{m['symbytes']}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
