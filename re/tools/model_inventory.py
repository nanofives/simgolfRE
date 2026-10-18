"""Phase-1 stream A: inventory of every data address golf_clean.exe's game logic references, grouped into clusters,
classified, and assigned to one data-model stream (re/PHASE1_PLAN.md section 3).

    py -3.12 re/tools/model_inventory.py                 # writes re/model/inventory_*.tsv and re/model/ownership.tsv
    py -3.12 re/tools/model_inventory.py --summary       # also prints the counts the report quotes

Inputs: re/functions_ghidra.tsv (the repaired index: every range of every function whose entry is in 0x401000..
0x4a4ecf, mainLoop's whole body 0x0040f5c0..0x00421615 included), the anchored image original/golf_clean.exe,
hooks.csv and re/names/*.tsv (subsystem tags), re/names/*_globals.tsv (names), re/tools/sve.py (FIELDS, tails),
log/xref_golf_clean.exe.json (call graph), re/analysis/sim/mainLoop_structure.md (phase ranges, copied below).

KEEP RULE (an address in the data range 0x4ba000..0x842178 becomes an inventory row when at least one reference is):
  mem      a memory operand displacement (any form: absolute, base + disp, index * scale + disp, lea, call / jmp [m]);
  deref    an immediate loaded into a register (mov / lea / add) that a later instruction of the same straight-line
           range uses as a memory base or index (register tracking: re/tools/model_scan.py);
  addbase  an immediate added to an index register that is then dereferenced (`shl eax, 8; add eax, 0x5794b8`);
  thiscall an immediate in ecx at a `call` (a global object passed as `this`);
  cmpend   `cmp reg, imm` where reg holds a tracked pointer or was used as a memory base / index in the previous 64
           instructions of the range (a loop end: the address is kept but marked `end_marker` when nothing else
           references it, and it does not take part in cluster formation);
  value    the immediate is passed or stored as a value (push, mov reg, mov [m], mov [esp+k]) and is not a single bit
           (a power of two is a flag constant, e.g. 0x800000 in `test dword [0x59e7b8], 0x800000`).
An immediate whose only uses are arithmetic or bit operations (and / or / xor / test / sub / imul with the immediate)
or a compare against a register with no pointer evidence is DROPPED; the count is printed and kept in the TSV
(column kept = 0).
"""
import argparse, bisect, collections, csv, glob, json, math, pathlib, re, struct, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import model_scan as ms  # noqa: E402
import sve  # noqa: E402
import xref  # noqa: E402

OUT = ROOT / "re" / "model"
GAP = 0x40
MOD = "golf_clean.exe"

# Typed tables already established (re/analysis/data/golfer_record.md, record_tables.md): (name, base, stride, count)
KNOWN = [
    ("g_golfers", 0x005794B8, 0x100, 156, "re/analysis/data/golfer_record.md"),
    ("g_walkers", 0x00585850, 0x4C, 64, "re/analysis/data/record_tables.md"),
    ("g_golferStats", 0x0056AE70, 0xB8, 32, "re/analysis/data/record_tables.md"),
    ("g_golferTypeState", 0x005849E0, 0x2C, 84, "re/analysis/data/record_tables.md"),
    ("g_storyDefs", 0x0053A454, 0x32, 100, "re/analysis/data/record_tables.md"),
]

# mainLoop 0x0040f5c0 phases (re/analysis/sim/mainLoop_structure.md: Top-level state machine, FRAME body table)
PHASES = [
    (0x0040F5C0, 0x0040F9FC, "SETUP"), (0x0040F9FC, 0x0040FA29, "SESSION"), (0x0040FA29, 0x0040FAC8, "QUIT"),
    (0x0040FAC8, 0x0040FC24, "SESSION"), (0x0040FC24, 0x00410604, "PRE"), (0x00410604, 0x0041234E, "P1"),
    (0x0041234E, 0x0041353D, "P2"), (0x0041353D, 0x00415847, "P3"), (0x00415847, 0x004175F2, "P4"),
    (0x004175F2, 0x00418BC7, "P5"), (0x00418BC7, 0x00419D06, "P6"), (0x00419D06, 0x0041C0C5, "P7"),
    (0x0041C0C5, 0x0041ECBB, "H1"), (0x0041ECBB, 0x00420D83, "H2"), (0x00420D83, 0x00420E0A, "P7"),
    (0x00420E0A, 0x00420E35, "H2"), (0x00420E35, 0x00421615, "MENU"),
]
MAINLOOP, GAMEMAIN, ENTRY = 0x0040F5C0, 0x0045BAF0, 0x004A682F

# subsystem tag (hooks.csv / re/names) -> data-model stream (re/PHASE1_PLAN.md section 3)
TAG_STREAM = {"golfer": "B", "sim": "B", "economy": "B", "ai": "B",
              "terrain": "C", "course": "C",
              "render": "D", "video": "D", "audio": "D",
              "ui": "E", "input": "E", "frontend": "E", "net": "E"}
NO_VOTE = {"boot", "util", "save", "crt"}   # static initializers, generic helpers, the serializer and the CRT
                                            # (eh-vector array constructors, heap, string helpers) touch every kind
                                            # of data, so they do not vote (listed in the non_voting column)
# Objects re/PHASE1_PLAN.md section 3 names for a stream, with their extent: (lo, hi exclusive, stream, what, classes
# the claim applies to or None for any). Extents are the .sve field sizes (re/tools/sve.py FIELDS) where the plan's
# address is a field start, else the single dword the plan names.
PLAN_CLAIMS = [
    (0x005722E8, 0x005722E8 + 0x9C4, "C", "tile type grid 0x5722e8 (C)", None),
    (0x0056988C, 0x0056988C + 0x9C4, "C", "tile byte grid 0x56988c (C, tile grids)", None),
    (0x0053CAF0, 0x0053CAF0 + 5000, "C", "tile flags 0x53caf0 (C)", None),
    (0x0058BCB8, 0x0058BCB8 + 0x1000, "C", "placed objects 0x58bcb8 (C)", None),
    (0x00838C1C, 0x00838C1C + 0x169, "C", "height grid 0x838c1c (C)", None),
    (0x00575AB0, 0x00575AB0 + 0x28A0, "C", "hole records 0x575cb8 x 0x208 (C; .sve field 0x575ab0)", None),
    (0x004C2BA0, 0x004C2BA8, "C", "camera tile 0x4c2ba0 (C)", None),
    (0x004C2844, 0x004C2848, "C", "zoom 0x4c2844 (C)", None),
    (0x005685F4, 0x005685F8, "C", "camera rotation 0x5685f4 (C)", None),
    (0x00571FD8, 0x00571FD8 + 0x30E, "B", "course records 0x571ff4 x 0x2e (B; .sve field 0x571fd8)", None),
    (0x005A4998, 0x005A4998 + 0xA29, "B", "level table 0x5a4998 (B)", None),
    (0x00822D9C, 0x00822DA0, "B", "RNG seed 0x822d9c (B)", None),
    (0x00834170, 0x00834174, "B", "date g_date 0x834170 (B)", None),
    (0x0059FC60, 0x0059FC60 + 0x3880, "B", "story pairs 0x59fc60 (B, stories)", None),
    (0x0082415C, 0x0082515C, "D", "sprite queue g_spriteSortKey (D)", None),
    (0x0082815C, 0x0082915C, "D", "sprite queue g_spriteFlags (D)", None),
    (0x0082C160, 0x0082C164, "D", "sprite queue head (D)", None),
    (0x00830164, 0x00831164, "D", "sprite queue g_spriteX (D)", None),
    (0x00831164, 0x00832164, "D", "sprite queue g_spriteY (D)", None),
    (0x008361F4, 0x008371F4, "D", "sprite queue next (D)", None),
    (0x00838200, 0x00838204, "D", "sprite queue count (D)", None),
    (0x0082415C, 0x00838204, "D", "sprite queue parallel arrays 0x82xxxx-0x83xxxx (D)",
     ("scalar array", "scalar", "buffer")),
    (0x0080D840, 0x0080D840 + 219 * 0x6C, "D", "sound table 0x80d840 x 0x6c (D)", None),
    (0x0083AD50, 0x0083AD54, "D", "display object 0x83ad50 (D)", None),
    (0x00822C74, 0x00822C78, "D", "colour table 0x822c74 (D)", None),
    (0x008400B8, 0x008400BC, "E", "net session 0x8400b8 (E)", None),
    (0x004E4000, 0x004E6000, "F", "option / flag words 0x4e4xxx-0x4e5xxx (F)", ("scalar", "flag word", "unknown")),
]
STREAM_DESC = {"B": "simulation state", "C": "course / terrain / world", "D": "rendering and audio",
               "E": "C++ object layouts (vtables, global objects, UI toolkit statics)",
               "F": "initialised constant tables, strings, configuration words", "X": "unassigned"}


def hx(v):
    return f"0x{v:08x}"


# ---------------------------------------------------------------------------------------------------- inputs
def load_names():
    """data names: re/names/*_globals.tsv rows of golf_clean.exe -> {va: (name, type, file)}"""
    out = {}
    for f in sorted(glob.glob(str(ROOT / "re" / "names" / "*_globals.tsv"))):
        for row in csv.reader(open(f, encoding="utf-8", errors="replace"), delimiter="\t"):
            if len(row) > 2 and row[0] == MOD:
                try:
                    out[int(row[1], 16)] = (row[2], row[3] if len(row) > 3 else "", pathlib.Path(f).name)
                except ValueError:
                    pass
    return out


def load_tags():
    """function entry -> subsystem tag, from hooks.csv (golf_clean.exe rows) and re/names/*.tsv (names rows win)"""
    tags = {}
    with open(ROOT / "hooks.csv", encoding="utf-8", errors="replace") as fh:
        for r in csv.DictReader(fh):
            if r["module"] == MOD and r["subsystem"]:
                tags[int(r["addr"], 16)] = r["subsystem"]
    for f in sorted(glob.glob(str(ROOT / "re" / "names" / "*.tsv"))):
        if f.endswith("_globals.tsv") or f.endswith("withdrawn.tsv"):
            continue
        for row in csv.reader(open(f, encoding="utf-8", errors="replace"), delimiter="\t"):
            if len(row) > 3 and row[0] == MOD and row[3] and not row[3].startswith(" "):
                try:
                    tags[int(row[1], 16)] = row[3]
                except ValueError:
                    pass
    return tags


def sve_map():
    """[(va, size, field VA, tail)] of the .sve layout (both tails included)"""
    out = [(va, size, va, "") for va, size in sve.FIELDS]
    for tag, (bit, extra) in (("tailA", sve.TAIL_A), ("tailB", sve.TAIL_B)):
        out += [(va, size, va, tag) for va, size in extra]
    return sorted(out)


def import_names(pe):
    pe.parse_data_directories()
    out = {}
    for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        for i in e.imports:
            out[i.address] = f"{e.dll.decode()}!{(i.name or b'#%d' % i.ordinal).decode()}"
    return out


# ---------------------------------------------------------------------------------------------------- content view
class Image:
    def __init__(self, pe, base):
        self.pe, self.base = pe, base
        self.img = pe.get_memory_mapped_image()
        self.entries = {f[0] for f in xref.functions(MOD)}
        self.targets = set()                   # referenced data addresses (set by Inventory after build)
        self._kind = {}

    def u32(self, va):
        o = va - self.base
        return int.from_bytes(self.img[o:o + 4], "little") if 0 <= o <= len(self.img) - 4 else None

    def cstr(self, va, maxlen=256, minlen=2):
        """printable run of >= minlen chars ending in NUL at va (tab / CR / LF allowed), else None"""
        o = va - self.base
        if not (0 <= o < len(self.img)):
            return None
        data = self.img[o:o + maxlen]
        end = data.find(b"\0")
        if end < minlen:
            return None
        s = data[:end]
        if all(32 <= c < 127 or c in (9, 10, 13) for c in s):
            return s.decode("latin-1")
        return None

    def has_file_bytes(self, va):
        return ms.RDATA[0] <= va < ms.RDATA[1] or ms.INIT[0] <= va < ms.INIT[1]

    def is_ptr(self, d):
        """a dword is read as a pointer when it is a function entry (code) or a data address that is 4-aligned, is
        referenced by code, or starts a string of >= 2 characters. A 3-character string aliases a pointer value
        (`Fun\\0` = 0x006e7546, `1st\\0` = 0x00747331), and these tests keep such strings as text."""
        if ms.TEXT[0] <= d < ms.TEXT[1]:
            return "code" if d in self.entries else None
        if ms.in_data(d) and (d % 4 == 0 or d in self.targets or
                              (self.has_file_bytes(d) and self.cstr(d) is not None)):
            return "sptr" if self.cstr(d) is not None else "dptr"
        return None

    def kind(self, va):
        k = self._kind.get(va)
        if k is None:
            k = self._kind[va] = self._kind_of(va)
        return k

    def _kind_of(self, va):
        """file-content view of the dword at va (init / rdata only): code / sptr / dptr (see is_ptr), else
        ascii:<len> for a NUL-terminated printable run of >= 2 characters, zero, int:<v>, f32:<v>, raw:<hex>"""
        if not self.has_file_bytes(va):
            return "bss" if ms.BSS[0] <= va < ms.BSS[1] else ""
        d = self.u32(va)
        if d is None:
            return ""
        p = self.is_ptr(d)
        if p:
            return p
        s = self.cstr(va)
        if s is not None and len(s) >= 2:
            return f"ascii:{len(s)}"
        if d == 0:
            return "zero"
        sd = d - (1 << 32) if d & 0x80000000 else d
        if -0x10000 < sd < 0x10000:
            return f"int:{sd}"
        f = struct.unpack("<f", struct.pack("<I", d))[0]
        if math.isfinite(f) and 1e-6 < abs(f) < 1e7:
            return f"f32:{f:g}"
        return f"raw:{d:08x}"


# ---------------------------------------------------------------------------------------------------- call graph
def call_graph():
    idx = xref.load(MOD)
    return {va: f["calls"] for va, f in idx.items()}, idx


def reach_map(calls, fns):
    """function entry -> (set of mainLoop phases through which it is reached, min depth below mainLoop, boot flag)"""
    import capstone
    pe, base = xref.load_pe(MOD)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    entries = {f[0] for f in fns}
    lo, hi = MAINLOOP, 0x00421615
    phase_of = lambda a: next((p for l, h, p in PHASES if l <= a < h), "")
    seeds = collections.defaultdict(set)
    for ins in md.disasm(pe.get_data(lo - base, hi - lo), lo):
        if ins.mnemonic == "call" and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
            t = ins.operands[0].imm & 0xFFFFFFFF
            if t in entries:
                seeds[t].add(phase_of(ins.address))
    phases = collections.defaultdict(set)
    depth = {}
    phases[MAINLOOP] = {p for _, _, p in PHASES}
    depth[MAINLOOP] = 0
    for t, ps in seeds.items():
        stack = [(t, 1)]
        seen = set()
        while stack:
            v, d = stack.pop()
            if v in seen or v == MAINLOOP:
                continue
            seen.add(v)
            phases[v] |= ps
            depth[v] = min(depth.get(v, 99), d)
            for c in calls.get(v, []):
                stack.append((c, d + 1))
    boot = set()
    stack = [ENTRY]
    while stack:
        v = stack.pop()
        if v in boot or v == MAINLOOP:
            continue
        boot.add(v)
        stack += calls.get(v, [])
    return phases, depth, boot, phase_of


# ---------------------------------------------------------------------------------------------------- inventory
class Inventory:
    def __init__(self):
        self.pe, self.base = xref.load_pe(MOD)
        self.im = Image(self.pe, self.base)
        self.names = load_names()
        self.tags_raw = load_tags()
        self.imports = import_names(self.pe)
        self.sve = sve_map()
        self.sve_starts = [s[0] for s in self.sve]
        self.fns, self.refs = ms.scan_cached()
        self.fn_name = {}
        nm = xref.names(MOD)
        for va, name, size, rngs in self.fns:
            n = nm.get(va)
            self.fn_name[va] = n[1] if n and n[0] == "names" else (n[1] if n and name.startswith("FUN_") else name)
        self.calls, self.xidx = call_graph()
        self.tags, self.tag_src = self.propagate_tags()
        self.phases, self.depth, self.boot, self.phase_of = reach_map(self.calls, self.fns)

    # ---- tags
    def propagate_tags(self):
        """own tag; else the majority tag of direct callers, else of callees, else the two nearest tagged neighbours
        by address when they agree (MSVC keeps one source file's functions together)"""
        tags, src = {}, {}
        for va, *_ in self.fns:
            if va in self.tags_raw:
                tags[va], src[va] = self.tags_raw[va], "own"
        callers = collections.defaultdict(list)
        for v, cs in self.calls.items():
            for c in cs:
                callers[c].append(v)
        order = sorted(va for va, *_ in xref.functions(MOD))
        for va, *_ in self.fns:
            if va in tags:
                continue
            for label, peers in (("callers", callers.get(va, [])), ("callees", self.calls.get(va, []))):
                c = collections.Counter(self.tags_raw[p] for p in peers if p in self.tags_raw)
                if c:
                    top = c.most_common(2)
                    if len(top) == 1 or top[0][1] > top[1][1]:
                        tags[va], src[va] = top[0][0], label
                        break
            if va in tags:
                continue
            i = bisect.bisect_left(order, va)
            prv = next((self.tags_raw[order[j]] for j in range(i - 1, -1, -1) if order[j] in self.tags_raw), None)
            nxt = next((self.tags_raw[order[j]] for j in range(i + 1, len(order)) if order[j] in self.tags_raw), None)
            if prv and prv == nxt:
                tags[va], src[va] = prv, "neighbours"
        return tags, src

    # ---- per-address aggregation
    def build(self):
        lit = collections.defaultdict(list)
        derived = collections.defaultdict(list)
        ptruse = collections.defaultdict(list)
        for r in self.refs:
            if r.kind == "reg":
                derived[r.addr].append(r)
            elif r.kind == "ptruse":
                ptruse[r.addr].append(r)
            else:
                lit[r.addr].append(r)
        self.lit, self.derived, self.ptruse = lit, derived, ptruse
        rows = {}
        dropped_refs = 0
        for a, rs in lit.items():
            ev = set().union(*(r.evid for r in rs))
            keep_ev = ev & {"mem", "deref", "addbase", "thiscall", "cmpend"}
            value = ev & {"push", "movreg", "store", "stackarg", "field", "field0"}
            single_bit = a & (a - 1) == 0
            if keep_ev or (value and not single_bit):
                kept, why = 1, ",".join(sorted(keep_ev | ({"value"} if value and not single_bit else set())))
            else:
                kept = 0
                why = "single-bit value" if value else ("arithmetic only" if ev & {"arith"} else "compare only")
                dropped_refs += len(rs)
            marker = kept and not (ev & {"mem", "deref", "addbase", "thiscall"}) and "cmpend" in ev and not (value)
            rows[a] = dict(va=a, kept=kept, why=why, role="end_marker" if marker else ("member" if kept else ""),
                           refs=rs, ev=ev)
        self.rows = rows
        self.im.targets = {a for a, r in rows.items() if r["kept"]}
        self.im._kind.clear()
        self.dropped_refs = dropped_refs
        return rows

    def row_stats(self, a):
        row = self.rows[a]
        rs = row["refs"] + self.derived.get(a, [])
        w = collections.Counter()
        for r in rs:
            if not r.width:
                continue
            key = "x87" if r.x87 else str(r.width)
            for c in r.acc or "":
                w[c + key] += 1
        sign = collections.Counter(r.sign for r in rs if r.sign)
        strides = collections.Counter()
        for r in rs:
            for s, src in r.strides:
                if s and s > 0:
                    strides[s] += 1
        fns = sorted({r.fn for r in rs})
        ptr = collections.Counter()
        for r in self.ptruse.get(a, []):
            for e in r.evid:
                ptr[e] += 1
        for r in row["refs"]:
            if "thiscall" in r.evid:
                ptr["thiscall"] += 1
        blocks = sorted({r.block[1] for r in row["refs"] if r.block})
        return dict(widths=w, sign=sign, strides=strides, fns=fns, ptr=ptr, blocks=blocks,
                    addr_of=sum(1 for r in row["refs"] if r.kind.startswith("lea") or r.kind == "imm"),
                    idx=sum(1 for r in rs if r.base or r.index), bitops=sum(1 for r in rs if r.bitop),
                    nder=len(self.derived.get(a, [])))

    # ---- object starts known before clustering
    def detect_tables(self):
        """vtables and vbtables in .rdata: a kept address that code stores into an object field (`mov [reg + k], imm`)
        or loads / pushes as a value, whose dword is a code pointer (vtable: slots = the run of code pointers) or a
        small integer stored into a field (vbtable: the run of small integers, at most 16). A run stops at the next
        such start."""
        cand = [a for a, r in self.rows.items() if r["kept"] and ms.section(a) == "rdata"
                and r["ev"] & {"field", "field0", "movreg", "push", "store"}]
        starts = set()
        for a in cand:
            k = self.im.kind(a)
            if k == "code" or (self.rows[a]["ev"] & {"field", "field0"} and k.split(":")[0] in ("int", "zero")):
                starts.add(a)
        self.vt = {}
        for a in sorted(starts):
            if self.im.kind(a) == "code":
                n = 0
                while self.im.kind(a + 4 * n) == "code" and (n == 0 or a + 4 * n not in starts):
                    n += 1
                self.vt[a] = ("vtable", n)
            else:
                n = 0
                while n < 16 and self.im.kind(a + 4 * n).split(":")[0] in ("int", "zero") and \
                        (n == 0 or a + 4 * n not in starts):
                    n += 1
                self.vt[a] = ("vbtable", n)
        # stride votes per address (literal and derived references), used to keep record fields together
        self.addr_strides = collections.defaultdict(set)
        for a, row in self.rows.items():
            for r in row["refs"] + self.derived.get(a, []):
                for st, src in r.strides:
                    if st and st > 0:
                        self.addr_strides[a].add(st)

    def cclass(self, a):
        k = self.im.kind(a).split(":")[0]
        return k if k in ("code", "sptr", "dptr", "ascii") else "other"

    # ---- clusters
    def breaks(self):
        """addresses where a new object starts: section edges, the typed tables of KNOWN, every .sve field, every
        vtable / vbtable start and end, and every named global outside KNOWN"""
        b = set()
        for lo, hi in (ms.IAT, ms.RDATA, ms.INIT, ms.BSS):
            b.add(lo)
            b.add(hi)
        for name, base, stride, count, _ in KNOWN:
            b.add(base)
            b.add(base + stride * count)
        for va, size, _, _ in self.sve:
            b.add(va)
            b.add(va + size)
        for a, (k, n) in self.vt.items():
            b.add(a)
            b.add(a + 4 * max(n, 1))
        for a in self.names:
            if not any(base <= a < base + stride * count for _, base, stride, count, _ in KNOWN):
                b.add(a)
        for lo, hi, stream, what, classes in PLAN_CLAIMS:
            if hi - lo <= 0x10000:
                b.add(lo)
                b.add(hi)
        for a, r in self.rows.items():
            if "thiscall" in r["ev"]:
                b.add(a)                    # a global object's `this`
        return sorted(b)

    def indexed(self, a):
        """(has an index-register access, largest stride seen) for a member"""
        rs = self.rows[a]["refs"] + self.derived.get(a, [])
        return any(r.index or r.base for r in rs), max(self.addr_strides.get(a, {0}) or {0})

    def index_break(self, a, b):
        """a member that code only reaches at its fixed address, followed by one that is the displacement of an
        indexed access with no stride above 4 (a scalar array base), starts a new object"""
        ia, _ = self.indexed(a)
        ib, sb = self.indexed(b)
        return not ia and ib and 0 < sb <= 4

    def content_break(self, a, b):
        """in file-initialised data, a change of content kind between two neighbouring members that are not record
        fields (no stride vote above 4) starts a new object: an int table next to a string-pointer table"""
        if not (self.im.has_file_bytes(a) and self.im.has_file_bytes(b)):
            return False
        common = self.addr_strides.get(a, set()) & self.addr_strides.get(b, set())
        if any(s > 4 for s in common):
            return False
        return self.cclass(a) != self.cclass(b)

    def cluster(self):
        self.detect_tables()
        members = sorted(a for a, r in self.rows.items() if r["kept"] and r["role"] == "member")
        brk = self.breaks()
        clusters, cur = [], []
        self.break_reason = collections.Counter()
        for a in members:
            if cur:
                crossed = bisect.bisect_right(brk, cur[-1]) != bisect.bisect_right(brk, a)
                if a - cur[-1] > GAP:
                    self.break_reason["gap"] += 1
                elif crossed:
                    self.break_reason["object start"] += 1
                elif self.content_break(cur[-1], a):
                    self.break_reason["content"] += 1
                    crossed = True
                elif self.index_break(cur[-1], a):
                    self.break_reason["scalar before array"] += 1
                    crossed = True
                if a - cur[-1] > GAP or crossed:
                    clusters.append(cur)
                    cur = []
            cur.append(a)
        if cur:
            clusters.append(cur)
        self.clusters = [dict(id=i, addrs=c, markers=[]) for i, c in enumerate(clusters)]
        starts = [c["addrs"][0] for c in self.clusters]
        self.cl_of = {}
        for c in self.clusters:
            for a in c["addrs"]:
                self.cl_of[a] = c["id"]
        # end markers: attached to the cluster whose span contains them, else the nearest cluster below
        for a, r in self.rows.items():
            if r["role"] != "end_marker":
                continue
            i = max(0, bisect.bisect_right(starts, a) - 1)
            self.clusters[i]["markers"].append(a)
            self.cl_of[a] = self.clusters[i]["id"]
        self.der_of = collections.defaultdict(list)
        for r in self.refs:
            if r.kind == "reg" and r.dest is not None and r.dest in self.cl_of:
                self.der_of[self.cl_of[r.dest]].append(r)
        # survey-compatible count (gap only, no forced breaks, markers included)
        allk = sorted(a for a, r in self.rows.items() if r["kept"])
        n, prev = 0, None
        for a in allk:
            if prev is None or a - prev > GAP:
                n += 1
            prev = a
        self.survey_clusters = n
        return self.clusters

    # ---- per-cluster features and class
    def features(self, c):
        addrs = c["addrs"]
        lo, hi = addrs[0], addrs[-1]
        rs = [r for a in addrs for r in self.rows[a]["refs"]]
        der = self.der_of.get(c["id"], [])
        allr = rs + der
        maxw = max([r.width for r in allr if r.width] or [1])
        span = hi - lo + max([r.width for r in self.rows[hi]["refs"] if r.width] or [1])
        if lo in self.vt:
            span = max(span, 4 * self.vt[lo][1])
        widths = collections.Counter()
        reads = writes = 0
        for r in allr:
            if r.width:
                widths["x87" if r.x87 else r.width] += 1
            reads += "r" in (r.acc or "")
            writes += "w" in (r.acc or "")
        ev = set().union(*(r.evid for r in rs))
        votes = collections.Counter()
        voters = collections.defaultdict(set)
        for r in allr:
            for s, src in r.strides:
                if s and s > 0:
                    voters[s].add(r.ins)
        for s, ins in voters.items():
            votes[s] = len(ins)
        idx_refs = sum(1 for r in allr if r.base or r.index)
        bitops = sum(1 for r in allr if r.bitop)
        jmp_idx = any(r.kind.startswith("jmp_") and r.index for r in rs)
        call_idx = any(r.kind.startswith("call_") and r.index for r in rs)
        fields0 = [a for a in addrs if self.rows[a]["ev"] & {"field", "field0"}]
        thiscall = [a for a in addrs if "thiscall" in self.rows[a]["ev"]]
        ptrv = collections.Counter()
        for a in addrs:
            for r in self.ptruse.get(a, []):
                ptrv[next(iter(r.evid))] += 1
        content = collections.Counter()
        if self.im.has_file_bytes(lo):
            for v in range(lo - lo % 4, lo + span, 4):
                k = self.im.kind(v)
                content[k.split(":")[0]] += 1
        member_content = collections.Counter(self.im.kind(a).split(":")[0] for a in addrs)
        blocks = [r.block for r in rs if r.block]
        fns = sorted({r.fn for r in allr})
        addr_of = sum(1 for r in rs if r.kind.startswith("lea") or r.kind == "imm")
        return dict(lo=lo, hi=hi, span=span, n=len(addrs), refs=len(rs), der=len(der), widths=widths, maxw=maxw,
                    addr_of=addr_of,
                    reads=reads, writes=writes, ev=ev, votes=votes, idx=idx_refs, bitops=bitops, jmp_idx=jmp_idx,
                    call_idx=call_idx, fields0=fields0, thiscall=thiscall, ptrv=ptrv, content=content,
                    member_content=member_content, blocks=blocks, fns=fns, sec=ms.section(lo),
                    allr=allr)

    def run_len(self, a, kinds):
        """dwords from a whose content kind is in kinds, stopping at the next address code references"""
        n = 0
        while self.im.kind(a + 4 * n).split(":")[0] in kinds and (n == 0 or a + 4 * n not in self.rows):
            n += 1
        return n

    def classify(self, c, f, blind=False):
        """-> (class, detail, confidence, stride, count, count_src). blind=True ignores KNOWN (validation)."""
        sec = f["sec"]
        votes = f["votes"]
        stride = count = None
        count_src = ""
        known = self.covered(c)
        top = [s for s, _ in votes.most_common()]
        # a stride vote larger than every access width points at records; equal to the width at a scalar array
        rec_votes = {s: v for s, v in votes.items() if s > f["maxw"]}
        if sec == "idata":
            names = [self.imports.get(a, "?") for a in c["addrs"]]
            return ("import_table", f"{len(names)} slots: " + " ".join(n.split("!")[1] for n in names[:6])
                    + (" ..." if len(names) > 6 else ""), "high", 4, len(c["addrs"]), "import directory")
        if f["jmp_idx"] and f["member_content"].get("code"):
            n = self.run_len(f["lo"], ("code",))
            return ("jump table", f"{n} code targets", "high", 4, n, "file content")
        if f["lo"] in self.vt:
            k, n = self.vt[f["lo"]]
            ins = [hx(a) for a in c["addrs"] if a != f["lo"]]
            det = f"{k} {n} slots" + (f", slot addresses also referenced: {' '.join(ins[:4])}" if ins else "")
            conf = "high" if (k == "vtable" and n >= 2) or (k == "vbtable" and f["fields0"]) else "medium"
            return ("vtable", det, conf, 4, n, "file content")
        if sec in ("rdata", "init") and f["writes"] == 0:
            mc = f["member_content"]
            if mc.get("ascii", 0) == f["n"]:
                return ("string pool", f"{f['n']} strings", "high", None, f["n"], "file content")
            if mc.get("sptr", 0) and mc.get("sptr", 0) + mc.get("ascii", 0) >= f["n"] * 0.8:
                n = self.run_len(f["lo"], ("sptr",)) if self.im.kind(f["lo"]) == "sptr" else mc["sptr"]
                conf = "high" if f["idx"] else "medium"
                return ("string table", f"{n} string pointers from {hx(f['lo'])}", conf, 4, n, "file content")
            if (mc.get("code", 0) + mc.get("dptr", 0) >= f["n"] * 0.8) and (f["call_idx"] or f["idx"]):
                kinds = ("code",) if mc.get("code") else ("dptr", "sptr")
                n = self.run_len(f["lo"], kinds)
                return ("pointer table", f"{n} {'code' if kinds == ('code',) else 'data'} pointers", "high" if
                        f["call_idx"] else "medium", 4, n, "file content")
            if mc.get("ascii", 0) >= f["n"] * 0.8:
                return ("string pool", f"{mc['ascii']} of {f['n']} strings", "medium", None, mc["ascii"],
                        "file content")
        if known and not blind:
            name, base, kstride, kcount, _ = known
            return ("record array", f"{name} {kcount} x {hx(kstride)[2:].lstrip('0')}", "high", kstride, kcount,
                    "covered_by")
        mem = sum(f["widths"].values())
        mixed = len({w for w in f["widths"]}) > 1
        if sec in ("rdata", "init") and f["writes"] == 0:
            if f["idx"] or rec_votes or votes:
                s = (pick_stride(rec_votes) if rec_votes else
                     (top[0] if top else f["maxw"]))
                n = max(1, f["span"] // s) if s else None
                conf = "high" if votes.get(s, 0) >= 2 else "medium"
                return ("const lookup table", f"element {s} bytes, read width {self.width_str(f)}", conf, s, n,
                        "span / stride")
            if mem:
                return ("scalar", f"const: {f['n']} fixed-address read-only {'values' if f['n'] > 1 else 'value'}, "
                        f"widths {self.width_str(f)}", "medium" if not mixed else "low", None, f["n"], "")
            if all(self.im.cstr(a, minlen=0) is not None for a in c["addrs"]):
                return ("string pool", f"{f['n']} short strings (0-1 characters, or not decodable as longer text)",
                        "medium", None, f["n"], "file content")
            return ("unknown", "read-only, passed by address only, no string / pointer content", "none", None, None,
                    "")
        # mutable data (bss, or init data the code writes)
        if f["thiscall"] and len(f["thiscall"]) >= 1 and not rec_votes:
            return ("object", "global object instance: `this` at " + " ".join(hx(a) for a in f["thiscall"][:4]),
                    "high" if len(f["thiscall"]) and f["idx"] == 0 else "medium", None, len(f["thiscall"]), "")
        # a char buffer: at least 2/3 of the accesses are bytes, the byte stride is not outvoted by a record stride,
        # and the address is passed as a value at least once per 4 accesses (g_textBuffer 0x51a068: strcpy / sprintf)
        if mem and f["widths"].get(1, 0) * 3 >= 2 * mem and \
                (not votes or votes.get(1, 0) > max(rec_votes.values() or [0])) and f["addr_of"] * 2 >= mem:
            return ("buffer", f"char buffer: {f['widths'].get(1, 0)} of {mem} accesses are bytes, address passed "
                    f"{f['addr_of']} times", "medium", 1, f["span"], "span")
        if rec_votes:
            s = pick_stride(rec_votes)
            count, count_src = self.count_of(c, f, s)
            fields = {(a - f["lo"]) % s for a in c["addrs"]}
            conf = "high" if rec_votes[s] >= 2 and (len(fields) >= 2 or count_src in ("block", "end marker", "sve")) \
                else "medium" if rec_votes[s] >= 2 or len(fields) >= 2 else "low"
            if s <= 8 and len(fields) == 1:
                return ("scalar array", f"element {s} bytes", conf, s, count, count_src)
            return ("record array", f"stride {hx(s)}, {len(fields)} field offsets", conf, s, count, count_src)
        if votes:
            s = top[0]
            count, count_src = self.count_of(c, f, s)
            return ("scalar array", f"element {s} bytes", "high" if votes[s] >= 2 else "medium", s, count, count_src)
        if f["idx"] and f["n"] == 1:
            count, count_src = self.count_of(c, f, f["maxw"])
            return ("scalar array", f"indexed, width {f['maxw']}, no stride evidence", "low", f["maxw"], count,
                    count_src)
        if f["bitops"] and f["bitops"] * 2 >= f["reads"] and mem:
            return ("flag word", f"{f['bitops']} of {f['reads']} reads are bit tests / masks", "high" if f["n"] <= 2
                    else "medium", None, f["n"], "")
        addr_only = mem == 0 or (f["ev"] & {"push", "movreg"} and mem <= f["refs"] // 4)
        if (addr_only and (f["blocks"] or f["span"] > 0x40 or f["ev"] & {"push"})) or \
                (f["widths"] and set(f["widths"]) <= {1} and f["idx"]):
            if f["blocks"] or (mem and f["idx"]):
                return ("buffer", "passed by address" + (f", block {hex(max(b[1] for b in f['blocks']))}"
                                                         if f["blocks"] else ", byte-indexed"),
                        "medium", 1, f["span"], "span")
            return ("buffer", "address passed to callees only, no direct access: the layout is the callees'", "low",
                    1, f["span"], "span")
        if mem:
            return ("scalar",f"{f['n']} fixed-address {'scalars' if f['n'] > 1 else 'scalar'}, widths "
                    f"{self.width_str(f)}" + (" (mixed)" if mixed else ""), "high" if f["n"] == 1 and not mixed
                    else "medium" if not mixed else "low", None, f["n"], "")
        return ("unknown", "no width, no index, no content evidence", "none", None, None, "")

    def width_str(self, f):
        return ",".join(f"{k}x{v}" for k, v in sorted(f["widths"].items(), key=lambda kv: str(kv[0])))

    def covered(self, c):
        for k in KNOWN:
            name, base, stride, count, _ = k
            if base <= c["addrs"][0] and c["addrs"][-1] < base + stride * count:
                return k
        return None

    def count_of(self, c, f, s):
        """element count: .sve field size, rep block size, end-marker compare, else span (a lower bound)"""
        lo = f["lo"]
        for va, size, fv, tail in self.sve:
            if va <= lo < va + size and va + size > f["hi"]:
                if (va - lo) % s == 0 or size % s == 0:
                    return size // s, "sve"
        for b in f["blocks"]:
            if b[1] % s == 0 and b[0] <= lo:
                return b[1] // s, "block"
        best = None
        for m in c["markers"]:
            for a in c["addrs"]:
                if (m - a) % s == 0 and m > a:
                    n = (m - a) // s
                    best = n if best is None else max(best, n)
        if best:
            return best, "end marker"
        return max(1, -(-f["span"] // s)), "span"

    # ---- records addressed at several offsets (the golfer-record lesson)
    def record_bases(self, c, f, s):
        disp = collections.Counter()
        for r in f["allr"]:
            if any(st == s for st, _ in r.strides):
                disp[r.addr] += 1
        blocks = [b[0] for b in f["blocks"]]
        if not disp:
            return None
        base = min(list(disp) + blocks)
        fields = sorted({(d - base) % s for d in disp})
        named = [(a, self.names[a][0]) for a in range(f["lo"], f["hi"] + 1) if a in self.names]
        notes = [f"{n} {hx(a)} is field +{(a - base) % s:#x} of record {(a - base) // s} counted from the lowest "
                 f"indexed displacement" if a > base else
                 f"{n} {hx(a)} lies below every indexed displacement with stride {s:#x}" for a, n in named if a != base]
        return dict(base=base, fields=fields, disps=sorted(disp), blocks=blocks, notes=notes)

    # ---- ownership
    def votes_of(self, f):
        v = collections.Counter()
        nov = collections.Counter()
        for fn in f["fns"]:
            t = self.tags.get(fn)
            if t is None:
                nov["untagged"] += 1
            elif t in NO_VOTE:
                nov[t] += 1
            elif t in TAG_STREAM:
                v[TAG_STREAM[t]] += 1
            else:
                nov[t] += 1
        return v, nov

    def vote(self, f):
        """stream by the referencing functions' tags: most functions; a tie is broken by the number of references"""
        v, nov = self.votes_of(f)
        if not v:
            return None, "no voting caller: " + ",".join(f"{k}:{n}" for k, n in sorted(nov.items())), v, nov
        ranked = sorted(v.items(), key=lambda kv: (-kv[1], kv[0]))
        top = ranked[0][0]
        if len(ranked) > 1 and ranked[0][1] == ranked[1][1]:
            w = collections.Counter()
            for r in f["allr"]:
                t = self.tags.get(r.fn)
                if t in TAG_STREAM:
                    w[TAG_STREAM[t]] += 1
            tied = [s for s, n in ranked if n == ranked[0][1]]
            top = max(tied, key=lambda s: (w[s], s))
            return top, "tie on functions, broken by references " + " ".join(f"{s}:{w[s]}" for s in tied), v, nov
        return top, "vote", v, nov

    def claim(self, c, f, cls):
        for lo, hi, stream, what, classes in PLAN_CLAIMS:
            if lo <= f["lo"] and f["hi"] < hi and (not classes or cls in classes):
                return stream, what
        return None

    def owner(self, c, f, cls):
        """-> (stream, rule, votes, non-voting, vote stream). Order: KNOWN tables (B), the plan's named objects
        (PLAN_CLAIMS), the data kind (vtable / object -> E, import table / read-only file data -> F), then the vote;
        written .data words that the UI vote would give to E go to F (configuration words with file defaults)."""
        vs, vrule, v, nov = self.vote(f)
        sec = f["sec"]
        known = self.covered(c)
        if known:
            return "B", "covered_by " + known[0], v, nov, vs
        cl = self.claim(c, f, cls)
        if cl:
            return cl[0], "plan: " + cl[1], v, nov, vs
        if cls == "vtable":
            return "E", "vtable", v, nov, vs
        if cls == "object":
            return "E", "global object instance", v, nov, vs
        if cls == "import_table":
            return "F", "import address table (rdata)", v, nov, vs
        if sec in ("rdata", "init") and f["writes"] == 0:
            return "F", "read-only file data", v, nov, vs
        if vs is None:
            if sec == "init":
                return "F", "written .data, " + vrule, v, nov, vs
            return "X", vrule, v, nov, vs
        if vs == "X":
            vrule += " (CRT callers)"
        if sec == "init" and vs == "E" and cls in ("scalar", "flag word"):
            return "F", "file-initialised option / state word (UI vote)", v, nov, vs
        return vs, vrule, v, nov, vs


def pick_stride(votes):
    """the record stride: the largest stride whose vote (distinct instructions) is at least a quarter of the top vote
    and at least 2. Records hold sub-arrays with their own strides (g_holes: 0x208 records with 8-byte pairs inside,
    which get slightly more votes), so the most-voted stride is not always the record's."""
    top = max(votes.values())
    cands = [s for s, v in votes.items() if v >= max(2, top / 4)]
    return max(cands) if cands else max(votes, key=lambda s: (votes[s], s))


def fmt_counter(c, hexkeys=False):
    items = sorted(c.items(), key=lambda kv: (-kv[1], str(kv[0])))
    return ",".join(f"{hx(k)[2:].lstrip('0') if hexkeys else k}:{n}" for k, n in items)


def run(summary=False, out=OUT):
    inv = Inventory()
    rows = inv.build()
    clusters = inv.cluster()
    out = pathlib.Path(out)
    out.mkdir(parents=True, exist_ok=True)
    feats = {}
    classes = {}
    owners = {}
    recs = []
    for c in clusters:
        f = inv.features(c)
        feats[c["id"]] = f
        cls = inv.classify(c, f)
        classes[c["id"]] = cls
        owners[c["id"]] = inv.owner(c, f, cls[0])
    # parts of one table: neighbouring array clusters with the same stride S (> 4) whose gap is at most 2 * S are
    # fields, columns or biased bases of one table: `[hole * 0x208 + 0x5758a8]` reaches the .sve block 0x575ab0
    # for hole 1 (1-based index), `[i * 0x160 + 0x562368 + 0x2c * k]` addresses eight objects per row (the `this` of Surface_blit). Every
    # part takes the owner of the group's anchor (a KNOWN table, else the part with the most references).
    inv.bias_of = {}
    groups, cur = [], None
    for c in clusters:
        cls = classes[c["id"]]
        s = cls[3] if cls[0] in ("record array", "scalar array") else None
        if s is None or s <= 4:
            cur = None
            continue
        f = feats[c["id"]]
        if cur and cur["s"] == s and f["lo"] - cur["hi"] <= 2 * s:
            cur["ids"].append(c["id"])
            cur["hi"] = f["lo"] + f["span"]
        else:
            cur = dict(s=s, hi=f["lo"] + f["span"], ids=[c["id"]])
            groups.append(cur)
    for g in groups:
        if len(g["ids"]) < 2:
            continue
        anchor = max(g["ids"], key=lambda i: (bool(inv.covered(inv.clusters[i])), feats[i]["refs"] + feats[i]["der"]))
        for i in g["ids"]:
            if i == anchor or inv.covered(inv.clusters[i]):
                continue
            inv.bias_of[i] = anchor
            st = owners[anchor][0]
            owners[i] = (st, f"part of the table at cluster {anchor} ({hx(feats[anchor]['lo'])}, stride "
                             f"{g['s']:#x})", *owners[i][2:])
    for c in clusters:
        cls = classes[c["id"]]
        f = feats[c["id"]]
        if cls[0] == "record array" and cls[3]:
            rb = inv.record_bases(c, f, cls[3])
            if rb:
                recs.append((c, f, cls, rb))

    def fn_list(fns, n=40):
        lst = [f"{inv.fn_name.get(v, hx(v))}@{v:x}" for v in fns]
        return ";".join(lst[:n]) + (f";+{len(lst) - n}" if len(lst) > n else "")

    def reach_of(fns):
        ph = set()
        boot = indirect = 0
        dmin = None
        for v in fns:
            if v in inv.phases:
                ph |= inv.phases[v]
                d = inv.depth.get(v)
                dmin = d if dmin is None else min(dmin, d)
            elif v in inv.boot:
                boot += 1
            else:
                indirect += 1
        order = [p for _, _, p in PHASES]
        s = ",".join(sorted(ph, key=order.index))
        extra = []
        if boot:
            extra.append(f"boot:{boot}")
        if indirect:
            extra.append(f"indirect:{indirect}")
        return (s + (f" d{dmin}" if dmin is not None else "") + (" " if s and extra else "") + " ".join(extra)).strip()

    def sve_of(a):
        i = bisect.bisect_right(inv.sve_starts, a) - 1
        if i >= 0:
            va, size, fv, tail = inv.sve[i]
            if va <= a < va + size:
                return f"{hx(fv)}+{hx(a - va)[2:].lstrip('0') or '0'}{'/' + tail if tail else ''}"
        return ""

    def name_of(a):
        if a in inv.names:
            return inv.names[a][0]
        for name, base, stride, count, _ in KNOWN:
            if base <= a < base + stride * count:
                return f"{name}[{(a - base) // stride}]+{hx((a - base) % stride)[2:].lstrip('0') or '0'}"
        if a in inv.imports:
            return inv.imports[a]
        return sve.NAMES.get(a, "")

    # ---- addresses
    with open(out / "inventory_addresses.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["va", "section", "cluster", "kept", "keep_rule", "role", "refs", "functions", "r1", "w1", "r2",
                    "w2", "r4", "w4", "r8", "w8", "rx87", "wx87", "addr_of", "indexed", "derived", "sign", "bitops",
                    "stride_votes", "pointer_use", "block", "evidence", "subsystems", "reach", "name", "sve",
                    "content", "function_list"])
        for a in sorted(rows):
            row = rows[a]
            st = inv.row_stats(a)
            wd = st["widths"]
            tagc = collections.Counter(inv.tags.get(v, "untagged") for v in st["fns"])
            w.writerow([hx(a), ms.section(a), inv.cl_of.get(a, ""), row["kept"], row["why"], row["role"],
                        len(row["refs"]), len(st["fns"])] + [wd.get(k, 0) for k in
                                                             ("r1", "w1", "r2", "w2", "r4", "w4", "r8", "w8", "rx87", "wx87")] +
                       [st["addr_of"], st["idx"], st["nder"], fmt_counter(st["sign"]), st["bitops"],
                        fmt_counter(st["strides"], True), fmt_counter(st["ptr"]),
                        ",".join(hex(b) for b in st["blocks"]), ",".join(sorted(row["ev"])), fmt_counter(tagc),
                        reach_of(st["fns"]), name_of(a), sve_of(a), inv.im.kind(a) if not a in inv.imports else
                        "import", fn_list(st["fns"])])
    # ---- clusters
    with open(out / "inventory_clusters.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["cluster", "lo", "hi", "span", "section", "addresses", "end_markers", "refs", "derived",
                    "functions", "reads", "writes", "widths", "indexed", "stride_votes", "bitops", "pointer_use",
                    "content", "class", "detail", "confidence", "stride", "count", "count_source", "covered_by",
                    "sve", "names", "subsystems", "reach", "part_of", "function_list"])
        for c in clusters:
            f = feats[c["id"]]
            cls = classes[c["id"]]
            known = inv.covered(c)
            names = [f"{inv.names[a][0]}@{a:x}" for a in range(f["lo"], f["lo"] + f["span"]) if a in inv.names]
            sv = sorted({sve_of(a).split("+")[0] for a in c["addrs"] if sve_of(a)})
            tagc = collections.Counter(inv.tags.get(v, "untagged") for v in f["fns"])
            ptr = collections.Counter(f["ptrv"])
            if f["thiscall"]:
                ptr["thiscall"] += len(f["thiscall"])
            w.writerow([c["id"], hx(f["lo"]), hx(f["hi"]), hex(f["span"]), f["sec"], f["n"], len(c["markers"]),
                        f["refs"], f["der"], len(f["fns"]), f["reads"], f["writes"], inv.width_str(f), f["idx"],
                        fmt_counter(f["votes"], True), f["bitops"], fmt_counter(ptr), fmt_counter(f["content"]),
                        cls[0], cls[1], cls[2], hex(cls[3]) if cls[3] else "", cls[4] if cls[4] is not None else "",
                        cls[5], known[0] if known else "", ";".join(sv), ";".join(names[:12]), fmt_counter(tagc),
                        reach_of(f["fns"]), inv.bias_of.get(c["id"], ""), fn_list(f["fns"], 25)])
    # ---- records addressed at several offsets
    with open(out / "inventory_records.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["cluster", "lo", "hi", "stride", "count", "inferred_base", "field_offsets", "indexed_displacements",
                    "block_bases", "named_not_base", "covered_by", "part_of"])
        for c, f, cls, rb in recs:
            known = inv.covered(c)
            w.writerow([c["id"], hx(f["lo"]), hx(f["hi"]), hex(cls[3]), cls[4], hx(rb["base"]),
                        ",".join(hex(x) for x in rb["fields"]), ",".join(hex(x) for x in rb["disps"][:40]),
                        ",".join(hex(x) for x in rb["blocks"]), "; ".join(rb["notes"]), known[0] if known else "",
                        inv.bias_of.get(c["id"], "")])
    # ---- ownership
    hard = []
    with open(out / "ownership.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["cluster", "lo", "hi", "section", "class", "stream", "rule", "vote_stream", "votes", "non_voting",
                    "margin", "addresses", "functions"])
        for c in clusters:
            f = feats[c["id"]]
            stream, rule, v, nov, vs = owners[c["id"]]
            ranked = sorted(v.values(), reverse=True)
            margin = (ranked[0] - (ranked[1] if len(ranked) > 1 else 0)) if ranked else ""
            if rule.startswith("vote") or rule.startswith("tie"):
                if len(ranked) > 1:
                    hard.append((ranked[0] - ranked[1], -sum(ranked), c["id"]))
            w.writerow([c["id"], hx(f["lo"]), hx(f["hi"]), f["sec"], classes[c["id"]][0], stream, rule, vs or "",
                        fmt_counter(v), fmt_counter(nov), margin, f["n"], len(f["fns"])])
    inv.hard = sorted(hard)[:20]
    inv.feats, inv.classes, inv.owners, inv.recs = feats, classes, owners, recs
    inv.validation = validate(inv)
    with open(out / "inventory_validation.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["item", "va", "expected", "cluster", "got_class", "got_stride", "got_count", "confidence", "ok",
                    "source"])
        for v in inv.validation:
            w.writerow(v)
    # ---- vtables and vbtables for the class-layout stream (E)
    inv.vtable_rows = vtable_rows(inv)
    with open(out / "vtables.tsv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["va", "kind", "slots", "class", "class_evidence", "installers", "slot_targets", "cluster"])
        for row in inv.vtable_rows:
            w.writerow(row)
    if summary:
        print_summary(inv)
    return inv


def vtable_rows(inv):
    """one row per vtable / vbtable start: the functions that install it (store it into an object field, or load /
    push it), the class named by those functions (the `X` of `X::ctor`, `X::dtor`, ... when every installer that
    has a `::` name agrees, else every distinct one), and the slot targets"""
    rows = []
    installers = collections.defaultdict(set)
    for a in inv.vt:
        for r in inv.rows[a]["refs"]:
            installers[a].add(r.fn)
    for a, (kind, n) in sorted(inv.vt.items()):
        names = sorted({inv.fn_name.get(f, hx(f)) for f in installers[a]})
        classes = collections.Counter()
        for nm in names:
            m = re.match(r"([A-Za-z]\w*?)(?:::\w+|_(?:ctor|dtor|deleteDtor|scalarDtor|deletingDtor)\w*)$", nm)
            if m:
                classes[m.group(1)] += 1
        cls = "/".join(k for k, _ in classes.most_common()) if classes else ""
        targets = []
        if kind == "vtable":
            for i in range(n):
                t = inv.im.u32(a + 4 * i)
                targets.append(inv.fn_name.get(t, hx(t)))
        else:
            for i in range(n):
                k = inv.im.kind(a + 4 * i)
                targets.append(k.split(":")[1] if ":" in k else "0")
        rows.append([hx(a), kind, n, cls, "installer names" if cls else "", ";".join(names),
                     ";".join(f"{i}:{t}" for i, t in enumerate(targets)), inv.cl_of.get(a, "")])
    return rows


# Known objects the classifier is checked against (class, stride or None), each with its source. Record tables are
# classified BLIND (KNOWN is ignored), so they test the classifier, not the covered_by shortcut.
VALIDATION = [
    ("g_golfers", 0x005794B8, "record array", 0x100, "golfer_record.md"),
    ("g_walkers", 0x00585850, "record array", 0x4C, "record_tables.md"),
    ("g_golferStats", 0x0056AE70, "record array", 0xB8, "record_tables.md"),
    ("g_golferTypeState", 0x005849E0, "record array", 0x2C, "record_tables.md"),
    ("g_storyDefs", 0x0053A454, "record array", 0x32, "record_tables.md"),
    ("g_holes", 0x00575AB0, "record array", 0x208, "exe_*_globals.tsv"),
    ("golfer_types", 0x004D60A8, "record array", 0x230, "sve.py NAMES (0x4d60a8 + type*0x230)"),
    ("g_placed_objects", 0x0058BCB8, "record array", 0x10, "exe_0_globals.tsv"),
    ("g_animals", 0x0056D1D8, "record array", 0x3C, "exe_*_globals.tsv"),
    ("g_recordBanks", 0x004E6D20, "record array", 0x74, "exe_0_globals.tsv"),
    ("g_storyPairs", 0x0059FC60, "record array", 0x388, "exe_*_globals.tsv"),
    ("g_typeAttr", 0x00578372, "record array", 0x30, "exe_*_globals.tsv"),
    ("g_spriteSortKey", 0x0082415C, "scalar array", 4, "exe_*_globals.tsv int[]"),
    ("g_spriteFlags", 0x0082815C, "scalar array", 4, "exe_*_globals.tsv int[]"),
    ("g_spriteX", 0x00830164, "scalar array", 4, "exe_*_globals.tsv int[]"),
    ("g_spriteY", 0x00831164, "scalar array", 4, "exe_*_globals.tsv int[]"),
    ("g_spriteQueueNext", 0x008361F4, "scalar array", 4, "exe_*_globals.tsv int[]"),
    ("g_pathQueueX", 0x005A7404, "scalar array", 2, "exe_*_globals.tsv short[0x400]"),
    ("g_pathQueueY", 0x005A8028, "scalar array", 2, "exe_*_globals.tsv short[0x400]"),
    ("g_shotStartDate", 0x0056962C, "scalar array", 4, "exe_*_globals.tsv int[152]"),
    ("rootWin vtable", 0x004BA2FC, "vtable", None, "CLAUDE.md round 30 (slots 29..39)"),
    ("ListBox vtable A", 0x004BB3CC, "vtable", None, "function_boundaries.md"),
    ("ListBox vtable B", 0x004BBA3C, "vtable", None, "function_boundaries.md"),
    ("NodeListC vtable", 0x004BB014, "vtable", None, "CLAUDE.md round 19 (closeHookC slot 2)"),
    ("Snd vtable", 0x004BACF8, "vtable", None, "CLAUDE.md round 20 (Snd::flags slot 28)"),
    ("month names", 0x004C2908, "string table", None, "sve.py MONTHS (8 pointers)"),
    ("course types", 0x004C3078, "string table", None, "sve.py COURSE_TYPES (4 pointers)"),
    ("golfer preferences", 0x004C2858, "string table", None, "golfer_record.md +0x19 (3 strings)"),
    ("g_date", 0x00834170, "scalar", None, "sve.py NAMES"),
    ("g_cash_div100", 0x00571FD4, "scalar", None, "exe_*_globals.tsv int"),
    ("gCameraTileX", 0x004C2BA0, "scalar", None, "exe_*_globals.tsv int"),
    ("g_zoomScale", 0x004C2844, "scalar", None, "exe_*_globals.tsv int"),
    ("g_screenWidth", 0x00822C8C, "scalar", None, "exe_*_globals.tsv int"),
    ("g_flags", 0x0059E7B8, "flag word", None, "sve.py NAMES (gates the tails), mainLoop_structure.md"),
    ("g_screenMgrA", 0x0083ACB0, "object", None, "exe_*_globals.tsv ScreenMgr"),
    ("g_sectionReader", 0x0083B000, "object", None, "exe_*_globals.tsv Reader"),
    ("g_loadImage", 0x00839348, "object", None, "exe_*_globals.tsv Image"),
    ("g_textBuffer", 0x0051A068, "buffer", None, "exe_*_globals.tsv char[]"),
    ("g_pathScratch", 0x0083C004, "buffer", None, "exe_*_globals.tsv char[256]"),
    ("g_itoaBuf4e9a70", 0x004E9A70, "buffer", None, "exe_*_globals.tsv char[]"),
]


def expected_from_type(t):
    """classes compatible with a declared type in re/names/*_globals.tsv (hold-out check), None when the type text
    does not say (e.g. `camera rotation (0,2,4,6)`)"""
    tl = t.lower().strip()
    if re.match(r"(screenmgr|reader|image|list|sounddevice)\b", tl) or re.search(r"gfx surface( object)?$", tl):
        return {"object"}
    if re.search(r"\brecords?\b|stride|struct|entries|table", tl) and not re.search(r"\*|\bptr\b", tl):
        return {"record array", "scalar array", "const lookup table"}
    if "[" in tl:
        if re.match(r"(unsigned )?char|byte", tl):
            return {"buffer", "scalar array", "record array"}
        if re.match(r"[\w ]+\*\s*\[", tl):
            return {"scalar array", "pointer table", "record array"}
        return {"scalar array", "record array"}
    if re.search(r"\*|\bptr\b|handle|hmodule", tl):
        return {"scalar", "flag word"}
    if re.match(r"(int|unsigned|uint|ushort|short|uint16_t)\b", tl):
        return {"scalar", "flag word"}
    if tl.startswith("rect"):
        return {"scalar", "record array", "object"}
    if "buffer" in tl:
        return {"buffer", "scalar array"}
    return None


def holdout(inv, tuned):
    out = []
    for va, (name, typ, fname) in sorted(inv.names.items()):
        if fname.startswith("model_"):
            continue    # phase 1 stream outputs (re/names/model_*_globals.tsv) are derived from this classifier's own work, not a hold-out
        if va in tuned or any(base <= va < base + stride * count for _, base, stride, count, _ in KNOWN):
            continue
        exp = expected_from_type(typ)
        cid = inv.cl_of.get(va)
        if exp is None or cid is None:
            out.append([name, hx(va), typ[:40], cid if cid is not None else "", "", "", "", "", "",
                        "skipped holdout: " + ("type not mapped" if exp is None else "not referenced")])
            continue
        cls = inv.classify(inv.clusters[cid], inv.feats[cid], blind=True)
        ok = cls[0] in exp
        out.append([name, hx(va), "|".join(sorted(exp)), cid, cls[0], hex(cls[3]) if cls[3] else "",
                    cls[4] if cls[4] is not None else "", cls[2], int(ok), "holdout " + fname])
    return out


def validate(inv):
    out = []
    for item, va, exp, stride, src in VALIDATION:
        cid = inv.cl_of.get(va)
        if cid is None:
            # the address itself may be unreferenced: take the cluster whose span holds it
            cid = next((c["id"] for c in inv.clusters if inv.feats[c["id"]]["lo"] <= va <
                        inv.feats[c["id"]]["lo"] + max(inv.feats[c["id"]]["span"], 1)), None)
        if cid is None:
            out.append([item, hx(va), exp + (f" {hex(stride)}" if stride else ""), "", "not in a cluster", "", "", "",
                        0, src])
            continue
        c = inv.clusters[cid]
        cls = inv.classify(c, inv.feats[cid], blind=True)
        ok = cls[0] == exp and (stride is None or cls[3] == stride)
        out.append([item, hx(va), exp + (f" {hex(stride)}" if stride else ""), cid, cls[0],
                    hex(cls[3]) if cls[3] else "", cls[4] if cls[4] is not None else "", cls[2], int(ok),
                    "tuned " + src])
    return out + holdout(inv, {v[1] for v in VALIDATION})


def print_summary(inv):
    rows = inv.rows
    kept = [a for a, r in rows.items() if r["kept"]]
    print(f"functions scanned {len(inv.fns)}; references {len(inv.refs)} "
          f"(literal {sum(len(r['refs']) for r in rows.values())}, derived {sum(len(v) for v in inv.derived.values())},"
          f" pointer-use {sum(len(v) for v in inv.ptruse.values())})")
    print(f"literal data addresses {len(rows)}: kept {len(kept)}, dropped {len(rows) - len(kept)} "
          f"({inv.dropped_refs} references)")
    print("  dropped:", collections.Counter(r["why"] for r in rows.values() if not r["kept"]))
    print("  kept by:", collections.Counter(r["why"] for r in rows.values() if r["kept"]).most_common(12))
    print(f"  end markers {sum(1 for r in rows.values() if r['role'] == 'end_marker')}")
    print("  by section:", collections.Counter(ms.section(a) for a in kept))
    print(f"clusters {len(inv.clusters)} (survey-compatible gap-only count {inv.survey_clusters}); "
          f"splits besides gaps: {dict(inv.break_reason)}; vtables {sum(1 for k, n in inv.vt.values() if k == 'vtable')}"
          f", vbtables {sum(1 for k, n in inv.vt.values() if k == 'vbtable')}")
    print("  by section:", collections.Counter(inv.feats[c['id']]['sec'] for c in inv.clusters))
    cc = collections.Counter((inv.classes[c['id']][0], inv.classes[c['id']][2]) for c in inv.clusters)
    for (k, conf), n in sorted(cc.items()):
        print(f"  {k:20} {conf:7} {n}")
    cov = collections.Counter(inv.covered(c)[0] for c in inv.clusters if inv.covered(c))
    print("  covered_by:", dict(cov))
    sh = collections.defaultdict(lambda: [0, 0, set()])
    for c in inv.clusters:
        s = inv.owners[c["id"]][0]
        sh[s][0] += 1
        sh[s][1] += len(c["addrs"]) + len(c["markers"])
        sh[s][2] |= set(inv.feats[c["id"]]["fns"])
    for s in sorted(sh):
        print(f"  stream {s}: clusters {sh[s][0]}, addresses {sh[s][1]}, functions {len(sh[s][2])}")
    rules = collections.Counter(inv.owners[c["id"]][1].split(" ")[0].rstrip(":") for c in inv.clusters)
    print("  rules:", dict(rules))
    dis = [c["id"] for c in inv.clusters if inv.owners[c["id"]][4] and inv.owners[c["id"]][0] != inv.owners[c["id"]][4]]
    print(f"  clusters whose stream differs from their vote: {len(dis)}",
          collections.Counter((inv.owners[i][1].split(":")[0].split(" ")[0], inv.owners[i][4], inv.owners[i][0])
                              for i in dis).most_common(12))
    print("hardest ties (margin, cluster):")
    for m, tot, cid in inv.hard:
        f = inv.feats[cid]
        st, rule, v, nov, vs = inv.owners[cid]
        print(f"  {cid} {hx(f['lo'])}-{hx(f['hi'])} {inv.classes[cid][0]} -> {st} votes {fmt_counter(v)} margin {m}")
    for kind in ("tuned", "holdout"):
        rows = [v for v in inv.validation if v[9].startswith(kind + " ")]
        ok = sum(v[8] for v in rows)
        print(f"validation {kind}: {ok} of {len(rows)} correct")
        for v in rows:
            if not v[8]:
                print("  WRONG", v)
    print("validation skipped:", sum(1 for v in inv.validation if v[9].startswith("skipped")))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--summary", action="store_true")
    a = ap.parse_args()
    run(summary=a.summary)


if __name__ == "__main__":
    main()
