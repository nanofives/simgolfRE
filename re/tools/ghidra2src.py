"""Turn Ghidra decompilations of DEBUG-build functions (/Od: every C variable lives on the stack and code follows
the source literally) into VC6-compilable C++ and score each with match.py.

The output is raw (Ghidra's offsets and casts, `*(int *)((int)this + 0x14)`): in /Od that compiles to the same
instructions as a member access, so a 100% result is byte-level evidence like any other match; readable names
come later. What it does per function:
  * drops the /GZ stack fill loop and the `__chkesp()` calls (the compiler re-emits both); `X(...); v = __chkesp();`
    becomes `v = X(...);` (Ghidra mistakes the call's eax, kept through __chkesp, for its result)
  * replaces the `this` stack slot (`local_8 = this;`) by `this`, removes Ghidra's junk stores of return addresses
  * inlines register temporaries (iVar1, uVar2...) used once: in /Od a named variable would have a stack slot
  * declares locals in stack order (VC6 /Od gives the first declared variable the highest slot)
  * declares callees from Ghidra's stored prototypes (log/sigs_<module>.tsv, ghidra/scripts/DumpSignatures.java);
    thiscall callees through a wrapper struct; globals by the access size the instructions use
  * compiles, and fixes what VC6 rejects: casts for '=' / argument / return conversions, forward declarations of
    unknown types; then scores with match.compare.

    py -3.12 re/tools/ghidra2src.py --module jgld.dll --decomp log/jgld/decomp_0.c [--only 0x10002650] [--out log/g2s]
Writes <out>/<addr>.cpp and <out>/results.tsv (addr, accuracy or failure stage, note)."""
import argparse, collections, pathlib, re, subprocess, sys, tempfile

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

HEADER = """#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);
"""
FLAGS = {"jgld.dll": "/Od /ZI /GZ", "Terrain.dll": "/Od /ZI /GZ /GX /MTd", "golf_clean.exe": "/O2", "sound.dll": "/O2"}
SYM = re.compile(r"\b(_?DAT_[0-9a-f]{8}|PTR_\w+_[0-9a-f]{8}|PTR_[0-9a-f]{8}|[su]_\w*_[0-9a-f]{8}|LAB_[0-9a-f]{8})\b")
CALLEE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "do", "else", "case"}
TEMP = re.compile(r"^(?:[a-zA-Z]+Var\d+|extraout_\w+|in_\w+|unaff_\w+)$")


def split_args(s, start):
    """s[start] == '(' ; returns (list of (a, b) arg spans, index of the closing paren)."""
    depth, spans, a = 0, [], start + 1
    for i in range(start, len(s)):
        c = s[i]
        if c in "([":
            depth += 1
        elif c in ")]":
            depth -= 1
            if depth == 0:
                if s[a:i].strip():
                    spans.append((a, i))
                return spans, i
        elif c == "," and depth == 1:
            spans.append((a, i))
            a = i + 1
    return spans, None


LABELS = {}                                 # named data labels (g_flags...) -> address, from the same dump


def load_sigs(module):
    sigs = {}
    for line in (ROOT / "log" / f"sigs_{module}.tsv").read_text().splitlines():
        if line.startswith("LBL\t"):
            _, addr, name = line.split("\t")[:3]
            if re.match(r"^[0-9a-f]{8}$", addr) and re.match(r"^[A-Za-z_]\w*$", name.strip()):
                LABELS[name.strip()] = int(addr, 16)
            continue
        if line.startswith("SIG\t"):
            line = line[4:]
        entry, name, conv, proto = line.split("\t", 3)
        sigs[name] = (int(entry, 16), conv, proto.strip())
        if ":" in name:                     # FID_conflict:_memcpy is printed FID_conflict__memcpy
            sigs[name.replace(":", "_")] = (int(entry, 16), conv, proto.strip().replace(name, name.replace(":", "_")))
    return sigs


def parse_proto(proto):
    """'RET CONV NAME(PARAMS)' -> (ret, conv, name, [param strings])."""
    m = re.match(r"^(.*?)\s*\b(__cdecl|__stdcall|__thiscall|__fastcall)?\s*(\w+)\s*\((.*)\)\s*$", proto)
    if not m:
        return None
    ret, conv, name, params = m.groups()
    params = [p.strip() for p in params.split(",") if p.strip() and p.strip() != "void"]
    return ret.strip(), conv or "__cdecl", name, params


def ctype(t):
    t = t.strip()
    if t == "undefined":
        return "int"
    return t


class Func:
    def __init__(self, module, addr, text, sigs, pe, base):
        self.module, self.addr, self.text, self.sigs, self.pe, self.base = module, addr, text, sigs, pe, base
        self.notes = []
        self.ret_this = False
        self.scopy = False
        self.structs = set()

    # ---------------------------------------------------------------- parse
    def parse(self):
        lines = self.text.splitlines()
        o = lines.index("{")
        hdr = " ".join(l.strip() for l in lines[1:o] if l.strip())
        self.proto = parse_proto(hdr)
        if not self.proto:
            raise ValueError("header")
        self.fix_signature()
        c = len(lines) - 1 - lines[::-1].index("}")
        body = lines[o + 1:c]
        # join continuation lines (Ghidra wraps long statements)
        joined = []
        for l in body:
            if joined and not re.search(r"[;{}:]\s*$", joined[-1]) and joined[-1].strip() and not re.match(r"^\s*(else|do)\s*$", joined[-1]):
                joined[-1] = joined[-1].rstrip() + " " + l.strip()
            else:
                joined.append(l)
        # declarations: leading lines "TYPE NAME;" / "TYPE NAME [N];"
        self.decls, i = {}, 0
        while i < len(joined) and (not joined[i].strip() or re.match(r"^\s+[\w\s\*]+?\s\**\w+(\s*\[\d+\])?;$", joined[i])):
            m = re.match(r"^\s+([\w\s\*]+?)\s(\**)(\w+)(\s*\[\d+\])?;$", joined[i])
            if m:
                self.decls[m[3]] = (m[1].strip() + (" " + m[2] if m[2] else ""), (m[4] or "").strip())
            i += 1
        self.stmts = [l for l in joined[i:] if l.strip()]

    _ret_cache = {}

    def ret_pop(self, va):
        """Bytes popped by the function at va (`ret N`), following one incremental-linking `jmp` thunk;
        None when it cannot be read."""
        key = (self.module, va)
        if key in Func._ret_cache:
            return Func._ret_cache[key]
        out = None
        try:
            md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
            first = next(md.disasm(self.pe.get_data(va - self.base, 8), va), None)
            if first is not None and first.mnemonic == "jmp" and first.op_str.startswith("0x"):
                va = int(first.op_str, 16)
            size = match.ghidra_size(self.module, va)
            if size:
                rets = [i for i in md.disasm(self.pe.get_data(va - self.base, size), va) if i.mnemonic == "ret"]
                if rets:
                    out = max(int(i.op_str, 0) if i.op_str else 0 for i in rets)
        except Exception:
            out = None
        Func._ret_cache[key] = out
        return out

    def fix_signature(self):
        """Debug builds only: the prologue stores ecx to [ebp-4] in a thiscall, and `ret N` pops N/4 stack
        parameters. When Ghidra's prototype disagrees (unused parameters of virtual stubs are lost), rebuild it."""
        release = FLAGS.get(self.module, "").find("/Od") < 0
        size = match.ghidra_size(self.module, self.addr) or 0
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        ins = list(md.disasm(self.pe.get_data(self.addr - self.base, size), self.addr))
        if not ins:
            return
        head = [f"{i.mnemonic} {i.op_str}" for i in ins[:14]]
        this = "mov dword ptr [ebp - 4], ecx" in head
        rets = [i for i in ins if i.mnemonic == "ret"]
        n = int(rets[-1].op_str, 0) // 4 if rets and rets[-1].op_str else 0
        ret, conv, name, params = self.proto
        if release:
            # no frame to read `this` from: only the stack parameter count is known (`ret N`)
            if n and conv == "__cdecl":
                params = (params + [f"int pad_{k + 1}" for k in range(len(params), n)])[:n]
                self.proto = (ret, "__stdcall", name, params)
            elif conv in ("__thiscall", "__fastcall") and len(params) - 1 != n and (conv == "__thiscall" or n):
                stack = (params[1:] + [f"int pad_{k + 1}" for k in range(len(params) - 1, n)])[:n]
                first = "void *this" if conv == "__thiscall" else params[0]
                self.proto = (ret, "__thiscall", name, [first if conv == "__thiscall" else "void *this"] + stack)
                if conv == "__fastcall":
                    self.fast_this = params[0]
            elif conv == "__stdcall" and len(params) != n:
                params = (params + [f"int pad_{k + 1}" for k in range(len(params), n)])[:n]
                self.proto = (ret, conv, name, params)
            return
        if this:
            if conv == "__fastcall" and len(params) == 1 and n == 0:
                return
            stack = params[1:] if conv in ("__thiscall", "__fastcall") else params
            if conv == "__thiscall" and len(stack) == n:
                return
            stack = (stack + [f"int param_{k + 1}" for k in range(len(stack), n)])[:n]
            self.proto = (ret, "__thiscall", name, ["void *this"] + stack)
            self.notes.append("signature rebuilt from the prologue/ret")
        elif conv == "__stdcall" and len(params) != n:
            params = (params + [f"int param_{k + 1}" for k in range(len(params), n)])[:n]
            self.proto = (ret, conv, name, params)

    def is_temp(self, v):
        """Register temporaries: Ghidra's xVarN, and declared names that are not stack slots or parameters (Ghidra
        names a register value after the API parameter it feeds, e.g. `h`, `hdc`). /Od keeps every C variable on
        the stack, so these are intermediate values of one expression."""
        return bool(TEMP.match(v)) or (v in self.decls and not re.match(r"^(local_|[a-zA-Z]*Stack_|param_|this$)", v))

    # ---------------------------------------------------------------- clean
    def clean(self):
        s = self.stmts
        # 1. stack fill: "puVar = local_x;" + for loop of 4 lines
        for k in range(len(s) - 4):
            m = re.match(r"^\s+(\w+) = (\w+);$", s[k])
            if m and "0xcccccccc" in s[k + 2] and s[k + 1].strip().startswith("for ("):
                fill_ptr, fill_buf = m[1], m[2]
                it = re.match(r"^\s+for \((\w+) =", s[k + 1])[1]
                del s[k:k + 5]
                for v in (fill_ptr, fill_buf, it):
                    if not any(re.search(rf"\b{v}\b", x) for x in s):
                        self.decls.pop(v, None)
                break
        # 2. __chkesp: "CALL(...);" then "X = __chkesp();" -> "X = CALL(...);"
        out = []
        for l in s:
            m = re.match(r"^(\s+)(.+?) = (\([^()]*\))?__chkesp\(\);$", l)
            if m and out and out[-1].rstrip().endswith(");") and not re.match(r"^\s+(if|while|for|switch|return)\b", out[-1])                     and not re.match(r"^\s+[^()]*?[^=!<>]=[^=]", out[-1]):
                out[-1] = f"{m[1]}{m[2]} = {m[3] or ''}{out[-1].strip()[:-1]};"
                continue
            if re.match(r"^\s+__chkesp\(\);$", l):
                continue
            if "__chkesp" in l:
                raise ValueError("chkesp inside expression")
            out.append(l)
        s = out
        # 3. this slot
        ret, conv, name, params = self.proto
        self.this_param = None
        if conv == "__thiscall":
            self.this_param = "this"
        elif conv == "__fastcall" and len(params) == 1:
            self.this_param = None          # defined as __fastcall: param_1 stays a parameter
        slot = None
        for l in s:
            m = re.match(r"^\s+(local_\w+) = (this|param_1);$", l)
            if m and (m[2] == "this" or conv == "__fastcall"):
                slot, src = m[1], m[2]
                break
        if slot:
            s = [l for l in s if not re.match(rf"^\s+{slot} = (this|param_1);$", l)]
            s = [l for l in s if not re.match(rf"^\s+{slot} = (\([^)]*\))?0x[0-9a-f]{{8}};$", l)]
            if any(re.search(rf"\b{slot}\b", l) for l in s):
                t = self.decls.get(slot, ("void *", ""))[0]
                rep = f"(({t})this)" if src == "this" else src
                s = [re.sub(rf"\b{slot}\b", rep, l) for l in s]
            self.decls.pop(slot, None)
        # junk return-address stores into any other local that is never read again
        keep = []
        for k, l in enumerate(s):
            m = re.match(r"^\s+(local_\w+|[a-zA-Z]*Stack_\w+) = (?:\([^)]*\))?0x([0-9a-f]{8});$", l)
            if (m and 0 <= int(m[2], 16) - self.base < 0x200000
                    and not any(re.search(rf"\b{m[1]}\b", x) for x in s[k + 1:])):
                self.decls.pop(m[1], None)
                continue
            keep.append(l)
        s = keep
        # 4. inline register temporaries used once
        changed = True
        while changed:
            changed = False
            for k, l in enumerate(s):
                m = re.match(r"^\s+(\w+) = (.+);$", l)
                if not m or not self.is_temp(m[1]):
                    continue
                v, expr = m[1], m[2]
                rest = s[k + 1:]
                uses = [j for j, x in enumerate(rest) if re.search(rf"\b{v}\b", x)]
                if sum(len(re.findall(rf"\b{v}\b", rest[j])) for j in uses) != 1:
                    continue
                if sum(1 for x in s if re.match(rf"^\s+{v} = ", x)) != 1:
                    continue
                j = k + 1 + uses[0]
                s[j] = re.sub(rf"\b{v}\b", lambda _: f"({expr})" if re.search(r"[^\w\.\[\]]", expr) else expr, s[j], count=1)
                del s[k]
                self.decls.pop(v, None)
                changed = True
                break
        # a temporary assigned in several branches and read only by `return v;`: each assignment is a return
        for v in sorted({m[1] for x in s for m in [re.match(r"^\s+(\w+) = .+;$", x)] if m and self.is_temp(m[1])}):
            reads = [x for x in s if re.search(rf"\b{v}\b", x) and not re.match(rf"^\s+{v} = ", x)]
            if len(reads) == 1 and re.match(rf"^\s+return {v};$", reads[0]):
                s = [re.sub(rf"^(\s+){v} = (.+);$", r"\1return \2;", x) for x in s if x is not reads[0]]
                self.decls.pop(v, None)
        for v in list(self.decls):
            if self.is_temp(v) and not any(re.search(rf"\b{v}\b", x) for x in s):
                self.decls.pop(v)
        # null pointer constants compare against ints as well as pointers
        s = [re.sub(r"\((?:[\w ]+?) \*+\)0x0\b", "0", x) for x in s]
        # partial variables: x._1_2_ is the 2 bytes at offset 1 of x
        part = {1: "unsigned char", 2: "unsigned short", 4: "unsigned int", 8: "unsigned __int64"}
        s = [re.sub(r"\b(\w+)\._(\d+)_(\d+)_",
                    lambda m: f"(*({part.get(int(m[3]), 'unsigned int')} *)((char *)&{m[1]} + {m[2]}))", x) for x in s]
        # a case label right before `default:` or `}` needs a statement
        out = []
        for k, x in enumerate(s):
            out.append(x)
            if re.match(r"^\s*(case .+|default):\s*$", x) and k + 1 < len(s) and re.match(r"^\s*(default:|case |\})", s[k + 1]):
                out.append(x[:len(x) - len(x.lstrip())] + "  ;")
        s = out
        if any(re.search(r"\b(extraout_|in_|unaff_)\w+", x) for x in s):
            raise ValueError("register artefact (extraout/in/unaff)")
        if re.search(r"\b(CONCAT\d+|SUB\d+|ZEXT\d+|SEXT\d+|CARRY\d|SBORROW\d|POPCOUNT|__ftol)\b", "\n".join(s)):
            raise ValueError("Ghidra pseudo-op (CONCAT/SUB/ZEXT/__ftol...)")
        self.stmts = s

    # ---------------------------------------------------------------- declarations
    def data_sizes(self):
        """{address: (size, is_fpu)} for absolute memory operands in the original function."""
        size = match.ghidra_size(self.module, self.addr) or 0
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        md.detail = True
        out = {}
        for ins in md.disasm(self.pe.get_data(self.addr - self.base, size), self.addr):
            for op in ins.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                    out.setdefault(op.mem.disp & 0xffffffff, (op.size, ins.mnemonic.startswith("f")))
        return out

    def struct_copies(self):
        """Runs of n >= 2 consecutive 4-byte stores dst+4i = src+4i become one struct assignment
        `*(S_n *)(dst) = *(S_n *)(src);` (debug code loads both pointers once for a struct copy)."""
        def ref(e):
            e = e.strip()
            m = re.match(r"^\*\([\w\s]+\*\)\(\(int\)(.+?) \+ (0x[0-9a-f]+|\d+)\)$", e)
            if m:
                return m[1], int(m[2], 0)
            m = re.match(r"^\*\([\w\s]+\*\)(\(?[\w\.]+\)?)$", e) or re.match(r"^\*(\w+)$", e)
            if m:
                return m[1], 0
            m = re.match(r"^(\w+)\[(\d+)\]$", e)
            if m:
                return m[1], 4 * int(m[2])
            return None
        s, out, k, changed = self.stmts, [], 0, False
        while k < len(s):
            run = []
            while k + len(run) < len(s):
                m = re.match(r"^(\s+)(.+?) = (.+);$", s[k + len(run)])
                if not m:
                    break
                d, r = ref(m[2]), ref(m[3])
                if not d or not r or d[1] != r[1] or d[1] != 4 * len(run):
                    break
                if run and (d[0] != run[0][0] or r[0] != run[0][1]):
                    break
                run.append((d[0], r[0], m[1]))
            if len(run) >= 2:
                n = len(run)
                self.structs.add(n)
                out.append(f"{run[0][2]}*(S_{n} *)({run[0][0]}) = *(S_{n} *)({run[0][1]});")
                k += n
                changed = True
            else:
                out.append(s[k])
                k += 1
        self.stmts = out
        return changed

    def build(self):
        ret, conv, name, params = self.proto
        body = "\n".join(self.stmts)
        decl = [f"struct S_{n} {{ int a[{n}]; }};" for n in sorted(self.structs)]
        fwd = []
        # integer constants inside the image are addresses of globals (Ghidra prints `FUN_x(0x821308)` for `&obj`)
        lo = self.base + self.pe.sections[0].VirtualAddress
        hi = self.base + self.pe.OPTIONAL_HEADER.SizeOfImage
        body = re.sub(r"(?<![\w&])0x([0-9a-f]{6,8})\b",
                      lambda m: f"&DAT_{int(m[1], 16):08x}" if lo <= int(m[1], 16) < hi else m[0], body)
        # globals
        sizes = None
        for sym in sorted(set(SYM.findall(body))):
            a = int(sym[-8:], 16)
            if sym.startswith(("DAT_", "_DAT_")):
                if a - self.base < 0x1000:              # inside the PE header: a constant that looks like an address
                    body = re.sub(rf"&{sym}\b", f"0x{a:08x}", body)
                    if not re.search(rf"\b{sym}\b", body):
                        continue
                sizes = sizes if sizes is not None else self.data_sizes()
                sz, fpu = sizes.get(a, (4, False))
                t = {1: "char", 2: "short", 4: "float" if fpu else "int", 8: "double" if fpu else "__int64"}.get(sz, "int")
                decl.append(f"extern {t} {sym};")
                body = re.sub(rf"&{sym}\b", f"((char *)&{sym})", body)
            elif sym.startswith("PTR_"):
                decl.append(f"extern void *{sym};")
            elif sym.startswith("s_"):
                new = f"s_{sym[-8:]}"
                decl.append(f"extern char {new}[];")
                body = re.sub(rf"\b{sym}\b", new, body)
            elif sym.startswith("u_"):
                new = f"u_{sym[-8:]}"
                decl.append(f"extern wchar_t {new}[];")
                body = re.sub(rf"\b{sym}\b", new, body)
            elif sym.startswith("LAB_"):
                decl.append(f"void {sym}();")
        # named data labels (g_flags...): sized like DAT_ by the instruction that touches them
        for lbl in sorted(set(re.findall(r"\b[A-Za-z_]\w*\b", body)) & set(LABELS)):
            if lbl in self.sigs or lbl.startswith(("DAT_", "_DAT_", "PTR_", "LAB_", "s_", "u_")):
                continue
            sizes = sizes if sizes is not None else self.data_sizes()
            sz, fpu = sizes.get(LABELS[lbl], (4, False))
            t = {1: "char", 2: "short", 4: "float" if fpu else "int", 8: "double" if fpu else "__int64"}.get(sz, "int")
            decl.append(f"extern {t} {lbl};")
            body = re.sub(rf"&{lbl}\b", f"((char *)&{lbl})", body)
        # absolute low addresses Ghidra names by type and address: pcRam00000004 -> (*(char **)0x4)
        ram = {"pc": "char *", "pu": "unsigned int *", "pi": "int *", "ppc": "char **", "pp": "void **", "u": "unsigned int",
               "i": "int", "c": "char", "b": "unsigned char", "s": "short", "us": "unsigned short", "f": "float",
               "pv": "void *", "uc": "unsigned char", "puc": "unsigned char *", "ps": "short *", "pf": "float *"}
        def ramvar(m):
            if m[1] not in ram:
                raise ValueError(f"Ram variable {m[0]}")
            return f"(*({ram[m[1]]} *)0x{int(m[2], 16):x})"
        body = re.sub(r"\b([a-z]+)Ram([0-9a-f]{8})\b", ramvar, body)
        # callees
        wrappers = {}
        for cal in sorted(set(CALLEE.findall(body)) - KEYWORDS):
            if cal not in self.sigs or cal == name:
                continue
            p = parse_proto(self.sigs[cal][2])
            if not p:
                continue
            r, cv, nm, ps = p
            ps = [re.sub(r"\bparam_\d+\b|\bthis\b", "", x).strip() or x for x in ps]
            r = ctype(r)
            # the callee's own `ret` settles who pops the stack: Ghidra's stored convention is often wrong
            pops = self.ret_pop(self.sigs[cal][0])
            if pops is not None and not any("..." in x for x in ps):
                stack = len(ps) - (1 if cv in ("__thiscall", "__fastcall") else 0) - (1 if cv == "__fastcall" and len(ps) > 1 else 0)
                if pops == 0 and stack > 0 and cv in ("__stdcall", "__thiscall"):
                    cv = "__cdecl"          # a thiscall `this` becomes the first stack argument
                elif pops > 0 and cv == "__cdecl":
                    cv = "__stdcall"
            if cv == "__thiscall":
                cls = f"T_{nm}"
                decl.append(f"struct {cls} {{ {r} {nm}({', '.join(ctype(x) for x in ps[1:])}); }};")
                wrappers[nm] = cls
            else:
                decl.append(f"{r} {cv} {nm}({', '.join(ctype(x) for x in ps)});")
        # virtual calls: (**(code **)(*X + OFF))(args) -> ((VT_k *)(X))->fN(args)  (Ghidra drops the `this`)
        vk = 0
        pos = 0
        while True:
            m = re.compile(r"\(\*\*\(code \*\*\)").search(body, pos)
            if not m:
                break
            # (**(code **)E)(args): E is `*OBJ` or `(*OBJ + OFF)` for a virtual call; anything else is a plain
            # function pointer and is left as it is
            spans0, gclose = split_args(body, m.start())
            if gclose is None or gclose + 1 >= len(body) or body[gclose + 1] != "(":
                raise ValueError("vcall shape")
            e = body[m.end():gclose].strip()
            while e.startswith("(") and split_args(e, 0)[1] == len(e) - 1:
                e = e[1:-1].strip()
            mi = re.match(r"^\*(?:\((?:int|undefined4|uint) \*\))?(.+?)(?: \+ (0x[0-9a-f]+|\d+))?$", e)
            if not e.startswith("*") or not mi:
                pos = m.end()
                continue
            obj, off = mi[1].strip(), int(mi[2] or "0", 0)
            if off % 4:
                raise ValueError("vcall offset")
            spans, cend = split_args(body, gclose + 1)
            args = [body[x:y].strip() for x, y in spans]
            slot = off // 4
            vk += 1
            fs = " ".join(f"virtual int f{k}();" for k in range(slot))
            decl.append(f"struct VT_{vk} {{ {fs} virtual int f{slot}({', '.join('int' for _ in args)}); }};")
            call = f"((VT_{vk} *)({obj}))->f{slot}({', '.join(f'(int)({x})' for x in args)})"
            body = body[:m.start()] + call + body[cend + 1:]
        # rewrite thiscall callee calls: NAME(a0, a1..) -> ((T_NAME *)(a0))->NAME(a1..)
        for nm, cls in wrappers.items():
            out, pos = [], 0
            for m in re.finditer(rf"(?<![\w>.]){nm}\s*\(", body):
                if m.start() < pos:
                    continue
                spans, close = split_args(body, m.end() - 1)
                if close is None or not spans:
                    raise ValueError(f"thiscall call without this: {nm}")
                a0 = body[spans[0][0]:spans[0][1]].strip()
                rest = ", ".join(body[x:y].strip() for x, y in spans[1:])
                out.append(body[pos:m.start()] + f"(({cls} *)({a0}))->{nm}({rest})")
                pos = close + 1
            body = "".join(out) + body[pos:]
        # locals in stack order: first declared = highest slot (local_8 first)
        def slot(v):
            m = re.search(r"_([0-9a-f]+)$", v)
            return int(m[1], 16) if m and not TEMP.match(v) else -1
        locs = sorted(self.decls.items(), key=lambda kv: slot(kv[0]))
        # Ghidra often stretches an array over the slot above it (the saved `this`): clip each array to the gap
        # up to the next variable (or the this slot / ebp)
        top = -4 if conv in ("__thiscall", "__fastcall") else 0
        offs = sorted(-(slot(v) - 4) for v, _ in locs if slot(v) > 0)
        fixed = []
        for v, (t, arr) in locs:
            if arr and slot(v) > 0:
                off = -(slot(v) - 4)
                above = [o for o in offs if o > off] + [top]
                lim = min(above)
                es = 1 if re.match(r"^(char|undefined1?|byte|uchar|sbyte|bool)$", t) else (
                    2 if re.match(r"^(short|ushort|undefined2|word|wchar_t)$", t) else (
                    8 if re.match(r"^(double|longlong|ulonglong|undefined8|__int64)$", t) else 4))
                n = int(arr.strip("[] "))
                if lim > off and (lim - off) // es < n:
                    arr = f"[{(lim - off) // es}]"
            fixed.append((v, (t, arr)))
        ldecl = [f"    {t} {v}{arr};" for v, (t, arr) in fixed]
        # definition
        fname = f"FUN_{self.addr:08x}"
        rt = ctype(ret) if ret != "undefined" else ("int" if re.search(r"return [^;]", body) else "void")
        if self.ret_this and rt == "void":
            me = self.ret_this
            if me == "this":
                me = "this" if conv == "__thiscall" else "param_1"
                rt = "void *"
            else:
                rt = "int"
            body = re.sub(r"\breturn;", f"return {me};", body)
            if not re.search(r"return [^;]+;\s*$", body):
                body += f"\n  return {me};"
        ps = [ctype(x) for x in params]
        if getattr(self, "fast_this", None):
            ft = re.sub(r"\bparam_1\b", "", self.fast_this).strip()
            body = re.sub(r"\bparam_1\b", f"(({ft})this)", body)
        if conv != "__thiscall" and any(re.search(r"\bthis\b", x) for x in ps):
            ps = [re.sub(r"\bthis\b", "pThis", x) for x in ps]
            body = re.sub(r"\bthis\b", "pThis", body)
        if conv == "__thiscall":
            cls = f"C_{fname}"
            psig = ", ".join(ps[1:])
            head = f"struct {cls} {{ {rt} {fname}({psig}); }};\n"
            sig = f"{rt} {cls}::{fname}({psig})"
        else:
            head = ""
            sig = f"{rt} {conv} {fname}({', '.join(ps)})"
        self.decl = decl
        self.lines = ([HEADER] + fwd + decl + [head, f"// MATCH: {self.module} 0x{self.addr:08x} {fname}", sig, "{"]
                      + ldecl + body.splitlines() + ["}"])

    def source(self):
        return f"// FLAGS {self.module}: {FLAGS[self.module]}\n" + "\n".join(self.lines) + "\n"


# ---------------------------------------------------------------- compile / fix loop
ERR = re.compile(r"\((\d+)\) : error (C\d+): (.*)")


def wrap_rhs(line, typ):
    m = re.match(r"^(\s*)(.*?[^=!<>+\-*/%&|^])=(?!=)(.*);\s*$", line)
    if not m:
        return None
    return f"{m[1]}{m[2]}= ({typ})({m[3].strip()});"


def wrap_arg(line, fn, n, typ):
    for m in re.finditer(rf"\b{re.escape(fn)}\s*\(", line):
        spans, close = split_args(line, m.end() - 1)
        if close is None or n > len(spans):
            continue
        a, b = spans[n - 1]
        return line[:a] + f"({typ})({line[a:b].strip()})" + line[b:]
    return None


def atom_end(s, i):
    """End index (exclusive) of the operand starting at s[i] (casts, unary *, &, -, calls, indexing)."""
    while i < len(s) and s[i] == " ":
        i += 1
    while i < len(s) and s[i] in "*&-!~":
        i += 1
    if i < len(s) and s[i] == "(":
        _, c = split_args(s, i)
        i = c + 1
        if i < len(s) and (s[i].isalnum() or s[i] in "_*(&"):      # it was a cast: the operand follows
            return atom_end(s, i)
    else:
        while i < len(s) and (s[i].isalnum() or s[i] in "_."):
            i += 1
    while i < len(s) and s[i] in "([":
        if s[i] == "(":
            _, c = split_args(s, i)
        else:
            depth, c = 0, i
            for c in range(i, len(s)):
                depth += s[c] == "["
                depth -= s[c] == "]"
                if depth == 0:
                    break
        i = c + 1
    return i


def atom_start(s, i):
    """Start index of the operand that ends right before s[i]."""
    j = i
    while j > 0 and s[j - 1] == " ":
        j -= 1
    k = j
    while True:
        if k > 0 and s[k - 1] in ")]":
            depth, m = 0, k - 1
            close, opn = s[k - 1], "(" if s[k - 1] == ")" else "["
            for m in range(k - 1, -1, -1):
                depth += s[m] == close
                depth -= s[m] == opn
                if depth == 0:
                    break
            k = m
        elif k > 0 and (s[k - 1].isalnum() or s[k - 1] in "_."):
            while k > 0 and (s[k - 1].isalnum() or s[k - 1] in "_."):
                k -= 1
        else:
            break
    while k > 0 and s[k - 1] in "*&-!~":
        k -= 1
    return k


def cast_compare(line, op, typ):
    """Cast both operands of the first `a OP b` comparison on the line that is not yet cast."""
    for m in re.finditer(rf" {re.escape(op)} ", line):
        a0 = atom_start(line, m.start())
        b1 = atom_end(line, m.end())
        left, right = line[a0:m.start()].strip(), line[m.end():b1].strip()
        if left.startswith(f"({typ})") and right.startswith(f"({typ})"):
            continue
        return line[:a0] + f"({typ})({left}) {op} ({typ})({right})" + line[b1:]
    return None


def fix(lines, errors, signed=False):
    """Apply one fix per error line; returns True when something changed."""
    changed = False
    done = set()
    for ln, code, msg in errors:
        i = ln - 2                      # line 1 is the FLAGS comment
        if i in done or not (0 <= i < len(lines)):
            continue
        L = lines[i]
        new = None
        m = re.match(r"'=' : cannot convert from '.*' to '(.*)'", msg)
        if code == "C2440" and m:
            new = wrap_rhs(L, m[1])
        m = re.match(r"'return' : cannot convert from '.*' to '(.*)'", msg)
        if code == "C2440" and m and new is None:
            new = re.sub(r"return (.*);", lambda x: f"return ({m[1]})({x[1]});", L)
        m = re.match(r"'([=!<>]=?)' : no conversion from '(.*)' to '(.*)'", msg)
        if code == "C2446" and m:
            rel = m[1] in ("<", ">", "<=", ">=")
            ptr = "*" in m[2] or "*" in m[3]
            new = cast_compare(L, m[1], "unsigned int" if rel and ptr and not signed else "int")
        m = re.match(r"'(?:\w+::)?(\w+)' : cannot convert parameter (\d+) from '.*' to '(.*)'", msg)
        if code == "C2664" and m:
            new = wrap_arg(L, m[1], int(m[2]), m[3])
        m = re.match(r"syntax error : identifier '(\w+)'", msg) or re.match(r"'(\w+)' : missing storage-class or type specifiers", msg)
        m = re.match(r"'(\w+)' : function does not take \d+ parameters", msg)
        if code == "C2660" and m:
            for j, D in enumerate(lines):
                if re.match(rf"^\w[\w\s\*]*__cdecl {m[1]}\(.*\);$", D):
                    lines[j] = re.sub(r"\(.*\);$", "(...);", D)
                    return True
        if code in ("C2061", "C2501") and m:
            lines.insert(1, f"struct {m[1]};")
            return True
        m = re.match(r"'(\w+)' : undeclared identifier", msg)
        if code == "C2065" and m and re.search(rf"\b{m[1]}\s*\(", L):
            lines.insert(1, f"int __cdecl {m[1]}(...);")
            return True
        if code == "C2065" and m and m[1].startswith(("FUN_", "LAB_", "thunk_FUN_")):
            lines.insert(1, f"void {m[1]}();")       # a function used as a value (callback, vtable entry)
            return True
        if code == "C2065" and m and re.match(r"^[A-Z_][A-Z0-9_]*$|^[A-Z]\w*$", m[1]) and not m[1].startswith(("FUN_", "DAT_")):
            lines.insert(1, f"struct {m[1]};")
            return True
        if new and new != L:
            lines[i] = new
            done.add(i)
            changed = True
    return changed


def compile_errors(src):
    env = {"PATH": str(match.VC / "bin"), "INCLUDE": str(match.VC / "include"), "LIB": str(match.VC / "lib"),
           "SystemRoot": r"C:\Windows", "TEMP": tempfile.gettempdir(), "TMP": tempfile.gettempdir()}
    out = pathlib.Path(tempfile.mkdtemp(prefix="g2s_")) / "x.obj"
    flags = re.search(r"// FLAGS \S+: (.*)", src.read_text())[1]
    r = subprocess.run([str(match.VC / "bin" / "cl.exe"), "/nologo", "/c", "/Gy", *flags.split(), f"/Fo{out}", f"/Fd{out.parent}\\", f"/Tp{src}"],
                       capture_output=True, text=True, env=env)
    return [(int(m[1]), m[2], m[3]) for m in ERR.finditer(r.stdout)], r.stdout


def process(module, addr, text, sigs, pe, base, outdir):
    """Try the literal translation, then variants: return `this` (Ghidra drops it from destructors and some
    setters), field-by-field copies as struct assignments, and both."""
    best = None
    tried_note = ""
    for ret_this, scopy in ((False, False), (True, False), (False, True), (True, True)):
        if ret_this:
            if not tried_note.startswith("ret:"):
                continue
            ret_this = tried_note[4:]
        r = process1(module, addr, text, sigs, pe, base, outdir, ret_this, scopy)
        if r is None:                       # variant changes nothing
            continue
        if not ret_this and not scopy:
            tried_note = r[1]
        if r[0][0].isdigit() and (best is None or not best[0][0].isdigit() or float(r[0]) > float(best[0])):
            best = r
            (outdir / f"{addr:08x}.best.cpp").write_text((outdir / f"{addr:08x}.cpp").read_text())
        elif best is None:
            best = r
        if best[0] == "100.0":
            break
    if best and best[0][0].isdigit() and float(best[0]) < 100 and "(unsigned int)(" in (outdir / f"{addr:08x}.best.cpp").read_text():
        r = process1(module, addr, text, sigs, pe, base, outdir, False, False, signed=True)
        if r and r[0][0].isdigit() and float(r[0]) > float(best[0]):
            best = r
            (outdir / f"{addr:08x}.best.cpp").write_text((outdir / f"{addr:08x}.cpp").read_text())
    if best and best[0][0].isdigit() and 80 <= float(best[0]) < 100:
        bf = outdir / f"{addr:08x}.best.cpp"
        subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "match_permute.py"), str(bf), f"0x{addr:08x}",
                        "--write", "--rounds", "3"], capture_output=True, text=True, timeout=180)
        try:
            acc = match.compare(bf, "", False, quiet=True)[0][4]
            if 100 * acc > float(best[0]):
                best = (f"{100 * acc:.1f}", "permuted")
        except SystemExit:
            pass
    return best


def conv_of(f):
    return f.proto[1] if getattr(f, "proto", None) else ""


def process1(module, addr, text, sigs, pe, base, outdir, ret_this, scopy=False, signed=False):
    f = Func(module, addr, text, sigs, pe, base)
    f.ret_this = ret_this
    f.scopy = scopy
    path = outdir / f"{addr:08x}.cpp"
    try:
        f.parse()
        f.clean()
        if scopy and not f.struct_copies():
            return None
        f.build()
    except (ValueError, StopIteration) as e:
        return "translate", str(e)[:80]
    lines = f.source().splitlines()[1:]
    head = f.source().splitlines()[0]
    for _ in range(25):
        path.write_text(head + "\n" + "\n".join(lines) + "\n")
        errs, raw = compile_errors(path)
        if not errs:
            break
        if not fix(lines, errs, signed):
            return "compile", errs[0][1] + " " + errs[0][2][:70]
    else:
        return "compile", "fix loop limit"
    try:
        res = match.compare(path, "", False, quiet=True)
    except SystemExit as e:
        return "score", str(e)[:80]
    acc = res[0][4]
    note = ""
    if acc < 1:
        a, b = res[0][5], res[0][6]
        def before_pop(seq):
            ix = [k for k, x in enumerate(seq) if x[0] == "pop edi"]
            return seq[ix[-1] - 1][0] if ix and ix[-1] > 0 else ""
        o, m_ = before_pop(b), before_pop(a)
        if o != m_:
            if o == "mov eax, dword ptr [ebp - 4]" and conv_of(f) in ("__thiscall", "__fastcall"):
                note = "ret:this"
            elif o == "xor eax, eax":
                note = "ret:0"
            elif re.match(r"^mov eax, (0x[0-9a-f]+|\d+)$", o):
                note = "ret:" + o.split(", ")[1]
            else:
                mm = re.match(r"^mov eax, dword ptr \[ebp - (0x[0-9a-f]+)\]$", o)
                if mm:
                    note = f"ret:local_{int(mm[1], 16) + 4:x}"
    return f"{100 * acc:.1f}", note


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", required=True)
    ap.add_argument("--decomp", nargs="+", required=True)
    ap.add_argument("--only", default="")
    ap.add_argument("--out", default="log/g2s")
    a = ap.parse_args()
    outdir = ROOT / a.out / a.module
    outdir.mkdir(parents=True, exist_ok=True)
    sigs = load_sigs(a.module)
    pe = pefile.PE(str(ROOT / "original" / a.module))
    base = match.image_base(a.module)
    blocks = []
    for d in a.decomp:
        for b in pathlib.Path(d).read_text().split("// ===== ")[1:]:
            addr = int(b.split()[0], 16)
            blocks.append((addr, b))
    only = {int(x, 16) for x in a.only.split(",") if x}
    res_path = outdir / "results.tsv"
    def read_results():
        out = {}
        if res_path.exists():
            for l in res_path.read_text().splitlines():
                k, v, n = (l.split("\t") + ["", ""])[:3]
                out[k] = (v, n)
        return out
    mine = {}
    stats = collections.Counter()
    for addr, b in blocks:
        if only and addr not in only:
            continue
        r, note = process(a.module, addr, b, sigs, pe, base, outdir)
        mine[f"0x{addr:08x}"] = (r, note)
        stats["100" if r == "100.0" else (r if not r[0].isdigit() else "partial")] += 1
        print(f"0x{addr:08x}\t{r}\t{note}", flush=True)
    merged = read_results()              # re-read: another run on the same module may have written meanwhile
    merged.update(mine)
    res_path.write_text("\n".join(f"{k}\t{v}\t{n}" for k, (v, n) in sorted(merged.items())) + "\n")
    print(dict(stats))


if __name__ == "__main__":
    main()
