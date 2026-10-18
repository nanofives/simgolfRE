"""Shared access scanner for the phase-1 model tools (model_inventory.py, model_check.py).

Decodes every range of every function of golf_clean.exe (re/functions_ghidra.tsv, via xref.functions) with capstone
and returns one Ref per data reference: a memory operand whose displacement lies in the data range, an immediate in
the data range, or an access derived from a register that was loaded with a data address (kind `reg`).

Register tracking is straight-line, in decode order inside one range (no control flow is followed): it resets at
`ret` / `jmp`, a `call` clobbers eax / ecx / edx. Per register it knows one of
  ('anchor', addr, ref_id, idx_mult)  the register holds the data address addr (plus an index scaled by idx_mult when
                                      the address was added to an index register: `shl eax, 8; add eax, 0x5794b8`);
  ('index', var, mult)                the register holds mult * v for a value v loaded or computed in this block;
  ('loaded', addr, depth)             the register holds the dword at data address addr (depth 1) or the dword that
                                      dword points to (depth 2);
  ('const', k)                        a small constant (for `rep stos/movs` counts and `imul` by a register).
Nothing here decides what an address is; model_inventory.py applies the keep rule and the classifier on the Refs.
"""
import collections, pathlib, sys

import capstone
from capstone import x86

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import xref  # noqa: E402

MOD = "golf_clean.exe"
TEXT = (0x401000, 0x4BA000)
LOGIC = (0x401000, 0x4A4ED0)                      # game-logic functions (the port surveys' range)
RDATA = (0x4BA000, 0x4C1000)                      # .rdata (virtual size 0x6098, file 0x7000)
IAT = (0x4BA000, 0x4BA264)                        # import address table: KERNEL32 0x4ba000.. binkw32 ..0x4ba260
INIT = (0x4C1000, 0x4E7000)                       # .data with file bytes (raw size 0x26000)
BSS = (0x4E7000, 0x842178)                        # .data past the raw size (virtual size 0x381174)
DATA = (0x4BA000, 0x842178)

SIGNED_J = {"jl", "jg", "jle", "jge", "js", "jns", "setl", "setg", "setle", "setge", "cmovl", "cmovg", "cmovle", "cmovge"}
UNSIGNED_J = {"jb", "ja", "jbe", "jae", "setb", "seta", "setbe", "setae", "cmovb", "cmova", "cmovbe", "cmovae"}
BITOPS = {"and", "or", "xor", "test", "bt", "bts", "btr", "btc"}
X87_FLOAT = {"fld", "fst", "fstp", "fadd", "fsub", "fsubr", "fmul", "fdiv", "fdivr", "fcom", "fcomp"}
X87_INT = {"fild", "fist", "fistp", "fiadd", "fisub", "fisubr", "fimul", "fidiv", "fidivr", "ficom", "ficomp"}
LOOP_SPAN = 0x800                                  # a backward branch spanning more is not an inner loop (mainLoop's
                                                   # frame loop spans 73 KB)
STRING_OPS = {"movsd", "movsb", "movsw", "stosd", "stosb", "stosw", "scasb", "scasd", "cmpsb", "cmpsd", "lodsb", "lodsd"}
CLOBBER = (x86.X86_REG_EAX, x86.X86_REG_ECX, x86.X86_REG_EDX)
REG32 = {x86.X86_REG_AL: x86.X86_REG_EAX, x86.X86_REG_AH: x86.X86_REG_EAX, x86.X86_REG_AX: x86.X86_REG_EAX,
         x86.X86_REG_BL: x86.X86_REG_EBX, x86.X86_REG_BH: x86.X86_REG_EBX, x86.X86_REG_BX: x86.X86_REG_EBX,
         x86.X86_REG_CL: x86.X86_REG_ECX, x86.X86_REG_CH: x86.X86_REG_ECX, x86.X86_REG_CX: x86.X86_REG_ECX,
         x86.X86_REG_DL: x86.X86_REG_EDX, x86.X86_REG_DH: x86.X86_REG_EDX, x86.X86_REG_DX: x86.X86_REG_EDX,
         x86.X86_REG_SI: x86.X86_REG_ESI, x86.X86_REG_DI: x86.X86_REG_EDI, x86.X86_REG_BP: x86.X86_REG_EBP,
         x86.X86_REG_SP: x86.X86_REG_ESP}


def section(va):
    if IAT[0] <= va < IAT[1]:
        return "idata"
    if RDATA[0] <= va < RDATA[1]:
        return "rdata"
    if INIT[0] <= va < INIT[1]:
        return "init"
    if BSS[0] <= va < BSS[1]:
        return "bss"
    return ""


def in_data(v):
    return DATA[0] <= v < DATA[1]


_CACHE = {}


def scan_cached(lo=LOGIC[0], hi=LOGIC[1]):
    """(functions, refs) of one full scan of the functions with entries in [lo, hi), cached per process; callers
    must not change the Refs"""
    if (lo, hi) not in _CACHE:
        sc = Scanner(lo=lo, hi=hi)
        _CACHE[(lo, hi)] = (sc.fns, sc.scan())
    return _CACHE[(lo, hi)]


class Steps:
    """loop steps of one straight-line range: steps[reg] = [(ins address, K)], loops = [(target, branch address)]"""

    def __init__(self):
        self.steps = collections.defaultdict(list)
        self.loops = []

    def add(self, reg, addr, k):
        self.steps[reg].append((addr, k))

    def at(self, reg, addr):
        """the step K of reg when exactly one K lies in a loop that also contains addr"""
        st = self.steps.get(reg)
        if not st:
            return None
        loops = [(t, j) for t, j in self.loops if t <= addr <= j]
        ks = {k for a, k in st for t, j in loops if t <= a <= j}
        return ks.pop() if len(ks) == 1 else None


class Ref:
    __slots__ = ("id", "fn", "ins", "addr", "kind", "mnem", "text", "width", "x87", "acc", "scale", "base", "index",
                 "sign", "bitop", "strides", "evid", "dest", "block", "jcc", "bound", "rep")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))
        if self.strides is None:
            self.strides = []
        if self.evid is None:
            self.evid = set()

    def row(self):
        return {k: getattr(self, k) for k in self.__slots__}


def r32(r):
    return REG32.get(r, r)


class Scanner:
    def __init__(self, lo=LOGIC[0], hi=LOGIC[1], window=None):
        """lo/hi select functions by entry. window=(wlo, whi) keeps only references whose address lies inside it
        (model_check scans a range plus margins); None keeps every data address."""
        self.pe, self.base = xref.load_pe(MOD)
        self.img = self.pe.get_memory_mapped_image()
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.fns = [f for f in xref.functions(MOD) if lo <= f[0] < hi]
        self.window = window
        self.refs = []
        self.reg_used_as_base = collections.defaultdict(list)     # (fn) -> [(ins index, reg)]
        self._var = 0
        self._ctx = {}
        self._bound = {}

    def keep(self, v):
        if not in_data(v):
            return False
        return self.window is None or self.window[0] <= v < self.window[1]

    def u32(self, va):
        o = va - self.base
        if 0 <= o <= len(self.img) - 4:
            return int.from_bytes(self.img[o:o + 4], "little")
        return None

    def new_var(self):
        self._var += 1
        return ("index", self._var, 1)

    def add(self, **kw):
        for k, v in self._ctx.items():
            kw.setdefault(k, v)
        r = Ref(id=len(self.refs), **kw)
        self.refs.append(r)
        return r

    def scan(self):
        for va, name, size, rngs in self.fns:
            for rlo, rhi in rngs:
                try:
                    code = self.pe.get_data(rlo - self.base, rhi - rlo)
                except Exception:
                    continue
                insns = list(self.md.disasm(code, rlo))
                self._scan_block(va, insns)
        return self.refs

    # ------------------------------------------------------------------ one straight-line range
    def _scan_block(self, fn, insns):
        st = {}                                            # reg -> state tuple
        self._fresh = set()                                # regs written by mov / lea and not read since
        self._bound = {}                                   # reg -> max value on the fall-through of cmp + ja/jae/jg/jge
        # loop steps: `add r, K` / `inc r` / `lea r, [r + K]` that lie inside a backward branch's span together with
        # the access (see Steps.at); a step anywhere else in a large function says nothing about the access
        steps = Steps()
        if insns:
            lo_r, hi_r = insns[0].address, insns[-1].address + insns[-1].size
        for ins in insns:
            ops = ins.operands
            m = ins.mnemonic
            if (m.startswith("j") or m.startswith("loop")) and ops and ops[0].type == x86.X86_OP_IMM:
                t = ops[0].imm & 0xFFFFFFFF
                if lo_r <= t <= ins.address and ins.address - t <= LOOP_SPAN:
                    steps.loops.append((t, ins.address))
            if m in ("inc", "dec") and ops and ops[0].type == x86.X86_OP_REG:
                steps.add(r32(ops[0].reg), ins.address, 1)
            elif m in ("add", "sub") and len(ops) == 2 and ops[0].type == x86.X86_OP_REG and ops[1].type == x86.X86_OP_IMM:
                k = ops[1].imm
                if 0 < k < 0x10000:
                    steps.add(r32(ops[0].reg), ins.address, k)
            elif m == "lea" and len(ops) == 2 and ops[1].type == x86.X86_OP_MEM and ops[0].type == x86.X86_OP_REG:
                mm = ops[1].mem
                if r32(mm.base) == r32(ops[0].reg) and not mm.index and 0 < mm.disp < 0x10000:
                    steps.add(r32(ops[0].reg), ins.address, mm.disp)
        recent_bases = collections.deque(maxlen=64)        # regs used as a memory base / index in the last 64 insns
        for i, ins in enumerate(insns):
            nxt = insns[i + 1] if i + 1 < len(insns) else None
            nxt2 = insns[i + 2] if i + 2 < len(insns) else None
            self._one(fn, ins, nxt, nxt2, st, steps, recent_bases)
            for op in ins.operands:
                if op.type == x86.X86_OP_MEM:
                    for rg in (op.mem.base, op.mem.index):
                        if rg:
                            recent_bases.append(r32(rg))

    def _sign_of(self, ins, nxt, nxt2, dst_reg=None):
        m = ins.mnemonic
        if m.startswith("movsx"):
            return "s"
        if m.startswith("movzx"):
            return "u"
        if m in ("cmp", "test") and nxt is not None:
            if nxt.mnemonic in SIGNED_J:
                return "s"
            if nxt.mnemonic in UNSIGNED_J:
                return "u"
        if dst_reg is not None and nxt is not None and nxt.mnemonic in ("cmp", "test") and nxt.operands \
                and nxt.operands[0].type == x86.X86_OP_REG and r32(nxt.operands[0].reg) == dst_reg and nxt2 is not None:
            if nxt2.mnemonic in SIGNED_J:
                return "s"
            if nxt2.mnemonic in UNSIGNED_J:
                return "u"
        return ""

    def _bit_of(self, ins, nxt, dst_reg=None):
        if ins.mnemonic in BITOPS and len(ins.operands) == 2 and ins.operands[1].type == x86.X86_OP_IMM:
            return True
        if ins.mnemonic in ("bt", "bts", "btr", "btc"):
            return True
        if dst_reg is not None and nxt is not None and nxt.mnemonic in BITOPS and len(nxt.operands) == 2 \
                and nxt.operands[0].type == x86.X86_OP_REG and r32(nxt.operands[0].reg) == dst_reg \
                and nxt.operands[1].type == x86.X86_OP_IMM:
            return True
        return False

    def _mult(self, st, reg):
        s = st.get(r32(reg)) if reg else None
        return s if s and s[0] == "index" else None

    def _one(self, fn, ins, nxt, nxt2, st, steps, recent_bases):
        m = ins.mnemonic
        ops = ins.operands
        try:
            reads, written = ins.regs_access()
        except Exception:
            reads, written = [], []
        if m != "call":
            for rd in reads:
                self._fresh.discard(r32(rd))
        text = f"{m} {ins.op_str}"
        is_branch = m == "call" or m == "jmp" or m.startswith("j") or m.startswith("loop")
        self._ctx = {"jcc": nxt.mnemonic if m in ("cmp", "test") and nxt is not None else None,
                     "rep": m.startswith("rep") or m in STRING_OPS}
        dst = r32(ops[0].reg) if ops and ops[0].type == x86.X86_OP_REG else None
        x87f = m in X87_FLOAT
        handled_dst = False

        # --- memory operands
        for oi, op in enumerate(ops):
            if op.type != x86.X86_OP_MEM:
                continue
            mm = op.mem
            if mm.segment in (x86.X86_REG_FS, x86.X86_REG_GS):
                continue
            d = mm.disp & 0xFFFFFFFF
            breg, ireg = r32(mm.base) if mm.base else 0, r32(mm.index) if mm.index else 0
            acc = ("r" if op.access & capstone.CS_AC_READ else "") + ("w" if op.access & capstone.CS_AC_WRITE else "")
            if m == "lea":
                acc = ""
            load_dst = dst if (oi == 1 and dst is not None and m in ("mov", "movzx", "movsx")) else None
            if in_data(d):
                strides = []
                for rg, sc in ((breg, 1), (ireg, mm.scale)):
                    if not rg:
                        continue
                    s = st.get(rg)
                    mult = s[2] * sc if s and s[0] == "index" else sc
                    if mult > 1:
                        strides.append((mult, "scale"))
                    else:
                        k = steps.at(rg, ins.address)
                        if k:
                            strides.append((k, "step"))
                if not breg and not ireg:
                    kind = "abs"
                else:
                    kind = "idx" if ireg else "base"
                if m == "lea":
                    kind = "lea_" + kind
                elif m in ("call", "jmp"):
                    kind = m + "_" + kind
                bound = None
                if ireg and not breg and ireg in self._bound:
                    bound = self._bound[ireg] * mm.scale
                elif breg and not ireg and breg in self._bound and (st.get(breg) or ("",))[0] == "index":
                    bound = self._bound[breg]
                if self.keep(d):
                    self.add(fn=fn, ins=ins.address, addr=d, kind=kind, mnem=m, text=text, width=op.size,
                             x87=x87f, acc=acc, scale=mm.scale, base=breg, index=ireg,
                             sign=self._sign_of(ins, nxt, nxt2, load_dst), bitop=self._bit_of(ins, nxt, load_dst),
                             strides=strides, evid={"mem"}, bound=bound)
                # lea of a data address into a register: anchor (pointer arithmetic follows)
                if m == "lea" and dst is not None:
                    im = None
                    if breg or ireg:
                        ms = [st.get(breg) if breg else None, st.get(ireg) if ireg else None]
                        mults = [(s[2] * (1 if k == 0 else mm.scale)) for k, s in enumerate(ms) if s and s[0] == "index"]
                        im = mults[0] if len(mults) == 1 else None
                    st[dst] = ("anchor", d, self.refs[-1].id if self.keep(d) else None, im)
                    handled_dst = True
                elif load_dst is not None and not breg and not ireg and op.size == 4:
                    st[load_dst] = ("loaded", d, 1)
                    handled_dst = True
            else:
                # a derived access through an anchored / loaded register
                for rg in (breg, ireg):
                    s = st.get(rg) if rg else None
                    if not s:
                        continue
                    if s[0] == "anchor":
                        a = (s[1] + (mm.disp if rg == breg else 0)) & 0xFFFFFFFF
                        if s[2] is not None:
                            self.refs[s[2]].evid.add("deref")
                        strides = [(s[3], "anchor")] if s[3] else []
                        other = ireg if rg == breg else breg
                        so = st.get(other) if other else None
                        if so and so[0] == "index" and other == ireg:
                            strides.append((so[2] * mm.scale, "scale"))
                        elif other == ireg and ireg and mm.scale > 1:
                            strides.append((mm.scale, "scale"))
                        k = steps.at(rg, ins.address) if not s[3] else None
                        if k:
                            strides.append((k, "step"))
                        if self.keep(a) and m != "lea":
                            self.add(fn=fn, ins=ins.address, addr=a, kind="reg", mnem=m, text=text, width=op.size,
                                     x87=x87f, acc=acc, scale=mm.scale, base=breg, index=ireg,
                                     sign=self._sign_of(ins, nxt, nxt2, load_dst),
                                     bitop=self._bit_of(ins, nxt, load_dst), strides=strides, evid={"reg"},
                                     dest=s[1])
                        if m == "lea" and dst is not None and rg == breg and not ireg:
                            st[dst] = ("anchor", a, s[2], s[3])
                            handled_dst = True
                        elif m in ("movs", "movsd", "movsb", "movsw", "stosd", "stosb", "stosw", "stos") \
                                or m.startswith("rep"):
                            pass
                    elif s[0] == "loaded":
                        # the dword at s[1] is used as a pointer (depth 1) or as a vtable (depth 2)
                        tag = "ptrval" if s[2] == 1 else ("vtblval" if m in ("call", "jmp") else "ptrval2")
                        self.add(fn=fn, ins=ins.address, addr=s[1], kind="ptruse", mnem=m, text=text, width=0,
                                 x87=False, acc="", scale=mm.scale, base=breg, index=ireg, sign="", bitop=False,
                                 strides=[], evid={tag}) if self.keep(s[1]) else None
                        if load_dst is not None and rg == breg and s[2] == 1 and mm.disp == 0 and not ireg:
                            st[load_dst] = ("loaded", s[1], 2)
                            handled_dst = True
        # rep stos / movs with an anchored edi / esi: the block size when ecx is a known constant
        if m.startswith("rep") and ("stos" in m or "movs" in m):
            c = st.get(x86.X86_REG_ECX)
            unit = 4 if m.endswith("d") else (2 if m.endswith("w") else 1)
            for rg in (x86.X86_REG_EDI, x86.X86_REG_ESI):
                s = st.get(rg)
                if s and s[0] == "anchor" and s[2] is not None:
                    self.refs[s[2]].evid.add("deref")
                    if c and c[0] == "const":
                        self.refs[s[2]].block = (s[1], c[1] * unit, m)
        # --- immediates
        for oi, op in enumerate(ops):
            if op.type != x86.X86_OP_IMM:
                continue
            v = op.imm & 0xFFFFFFFF
            if is_branch or not in_data(v):
                continue
            ev = set()
            if m == "push":
                ev.add("push")
            elif m == "mov" and ops[0].type == x86.X86_OP_MEM:
                ev.add("store")
            elif m == "cmp":
                o0 = ops[0]
                if o0.type == x86.X86_OP_REG:
                    s = st.get(r32(o0.reg))
                    if (s and s[0] == "anchor") or r32(o0.reg) in recent_bases:
                        ev.add("cmpend")
                    else:
                        ev.add("cmp")
                else:
                    ev.add("cmp")
            elif m == "add" and dst is not None:
                ev.add("add")
            elif m == "mov" and dst is not None:
                ev.add("movreg")
            else:
                ev.add("arith")
            dest = None
            if "cmpend" in ev:
                # the loop pointer's start: the immediate the compared register was loaded with, else its current
                # tracked address; None when the register is only known as a recent memory base
                s = st.get(r32(ops[0].reg))
                if s and s[0] == "anchor":
                    dest = self.refs[s[2]].addr if s[2] is not None else s[1]
            if "store" in ev:
                mm = ops[0].mem
                if not mm.base and not mm.index:
                    dest = mm.disp & 0xFFFFFFFF
                elif mm.base and r32(mm.base) not in (x86.X86_REG_ESP, x86.X86_REG_EBP):
                    ev.add("field")
                    if mm.disp == 0 and not mm.index:
                        ev.add("field0")
                else:
                    ev.add("stackarg")
            rf = self.add(fn=fn, ins=ins.address, addr=v, kind="imm", mnem=m, text=text, width=0, x87=False, acc="",
                          scale=0, base=0, index=0, sign="", bitop=False, strides=[], evid=ev, dest=dest) \
                if self.keep(v) else None
            if dst is not None and m == "mov" and oi == 1:
                st[dst] = ("anchor", v, rf.id if rf else None, None)
                handled_dst = True
            elif dst is not None and m == "add" and oi == 1:
                s = st.get(dst)
                st[dst] = ("anchor", v, rf.id if rf else None, s[2] if s and s[0] == "index" else None)
                if rf is not None and s and s[0] == "index":
                    rf.evid.add("addbase")
                handled_dst = True
        # thiscall on a global object: `mov ecx, imm` ... `call`
        if m == "call" and x86.X86_REG_ECX in self._fresh:
            s = st.get(x86.X86_REG_ECX)
            if s and s[0] == "anchor" and s[2] is not None:
                self.refs[s[2]].evid.add("thiscall")
            elif s and s[0] == "loaded" and s[2] == 1 and self.keep(s[1]):
                self.add(fn=fn, ins=ins.address, addr=s[1], kind="ptruse", mnem=m, text=text, width=0, x87=False,
                         acc="", scale=0, base=0, index=0, sign="", bitop=False, strides=[], evid={"thisptrval"})
        # --- register state updates
        if handled_dst:
            pass
        elif dst is not None:
            src = ops[1] if len(ops) > 1 else None
            if m in ("mov", "movzx", "movsx") and src is not None:
                if src.type == x86.X86_OP_REG:
                    st[dst] = st.get(r32(src.reg))
                elif src.type == x86.X86_OP_IMM:
                    st[dst] = ("const", src.imm) if abs(src.imm) < 0x100000 else None
                else:
                    st[dst] = self.new_var()
            elif m == "xor" and src is not None and src.type == x86.X86_OP_REG and r32(src.reg) == dst:
                st[dst] = self.new_var()
            # Any value computed from non-pointer registers becomes a fresh index variable (multiplier 1), so the
            # multiplier applied after combining two loaded values is still seen: golferStats is indexed by
            # `lea eax, [edx + ecx*4]` (cls + 4 * prefs), `lea ecx, [eax + eax*2]; shl ecx, 3; sub ecx, eax` (x 23),
            # then `[ecx*8 + 0x56ae7c]` (x 8): stride 0xb8.
            elif m == "shl" and src is not None and src.type == x86.X86_OP_IMM:
                s = st.get(dst)
                st[dst] = ("index", s[1], s[2] << src.imm) if s and s[0] == "index" else self._fresh_mult(1 << src.imm)
            elif m == "imul" and len(ops) == 3 and ops[2].type == x86.X86_OP_IMM:
                s = st.get(r32(ops[1].reg)) if ops[1].type == x86.X86_OP_REG else None
                st[dst] = ("index", s[1], s[2] * ops[2].imm) if s and s[0] == "index" else self._fresh_mult(ops[2].imm)
            elif m == "imul" and len(ops) == 2 and src.type == x86.X86_OP_IMM:
                s = st.get(dst)
                st[dst] = ("index", s[1], s[2] * src.imm) if s and s[0] == "index" else self._fresh_mult(src.imm)
            elif m == "lea" and src is not None and src.type == x86.X86_OP_MEM:
                mm = src.mem
                sb = st.get(r32(mm.base)) if mm.base else None
                si = st.get(r32(mm.index)) if mm.index else None
                if sb and sb[0] == "anchor" and not mm.index:
                    st[dst] = ("anchor", (sb[1] + mm.disp) & 0xFFFFFFFF, sb[2], sb[3])
                elif (sb and sb[0] == "anchor") or (si and si[0] == "anchor"):
                    st[dst] = None
                else:
                    vs = {s[1] for s in (sb, si) if s and s[0] == "index"}
                    both_index = (not mm.base or (sb and sb[0] == "index")) and \
                        (not mm.index or (si and si[0] == "index"))
                    if both_index and len(vs) == 1:
                        mult = (sb[2] if sb else 0) + (si[2] * mm.scale if si else 0)
                        st[dst] = ("index", vs.pop(), mult)
                    else:
                        st[dst] = self.new_var()
            elif m in ("add", "sub") and src is not None and src.type == x86.X86_OP_REG:
                a, b = st.get(dst), st.get(r32(src.reg))
                if a and b and a[0] == "index" and b[0] == "index" and a[1] == b[1]:
                    st[dst] = ("index", a[1], a[2] + b[2] if m == "add" else a[2] - b[2])
                elif m == "add" and a and b and {a[0], b[0]} == {"anchor", "index"}:
                    an, ix = (a, b) if a[0] == "anchor" else (b, a)
                    st[dst] = ("anchor", an[1], an[2], ix[2])
                elif (a and a[0] == "anchor") or (b and b[0] == "anchor"):
                    st[dst] = None
                else:
                    st[dst] = self.new_var()
            elif m in ("add", "sub") and src is not None and src.type == x86.X86_OP_IMM:
                s = st.get(dst)
                if s and s[0] == "anchor":
                    st[dst] = ("anchor", (s[1] + (src.imm if m == "add" else -src.imm)) & 0xFFFFFFFF, s[2], s[3])
                elif s and s[0] == "index":
                    pass
                else:
                    st[dst] = self.new_var()
            elif m in ("inc", "dec"):
                s = st.get(dst)
                if s and s[0] == "anchor":
                    st[dst] = ("anchor", (s[1] + (1 if m == "inc" else -1)) & 0xFFFFFFFF, s[2], s[3])
                elif not (s and s[0] == "index"):
                    st[dst] = self.new_var()
            elif m in ("cmp", "test", "push", "bt"):
                pass
            elif r32(dst) in (x86.X86_REG_ESP,):
                st.pop(dst, None)
            else:
                st[dst] = self.new_var()
        # a constant upper bound on the fall-through of `cmp reg, imm` + ja / jae / jg / jge
        for wr in written:
            self._bound.pop(r32(wr), None)
        if m == "cmp" and len(ops) == 2 and ops[0].type == x86.X86_OP_REG and ops[1].type == x86.X86_OP_IMM                 and nxt is not None and nxt.mnemonic in ("ja", "jae", "jg", "jge") and 0 <= ops[1].imm < 0x1000000:
            self._bound[r32(ops[0].reg)] = ops[1].imm - (1 if nxt.mnemonic in ("jae", "jge") else 0)
        # implicit writes (cdq, div, mul, xchg ...): a new value in every other register written
        for wr in written:
            self._fresh.discard(r32(wr))
        if dst is not None and m in ("mov", "lea"):
            self._fresh.add(dst)
        for wr in written:
            w = r32(wr)
            if w == dst:
                continue
            if w in (x86.X86_REG_ESP, x86.X86_REG_EIP) or w == 0:
                continue
            if w in (x86.X86_REG_EAX, x86.X86_REG_EBX, x86.X86_REG_ECX, x86.X86_REG_EDX, x86.X86_REG_ESI,
                     x86.X86_REG_EDI, x86.X86_REG_EBP) and m not in ("call",) and not m.startswith("rep") \
                    and m not in ("movsd", "movsb", "movsw", "stosd", "stosb", "stosw", "scasb", "cmpsb", "lodsb"):
                st[w] = self.new_var()
            else:
                st.pop(w, None)
        if m == "call":
            for r in CLOBBER:
                st.pop(r, None)
                self._fresh.discard(r)
            st[x86.X86_REG_EAX] = self.new_var()
        elif m in ("ret", "jmp") or m.startswith("ret"):
            st.clear()

    def _fresh_mult(self, k):
        self._var += 1
        return ("index", self._var, k)
