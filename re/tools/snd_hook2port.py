r"""Convert sound.dll hook sources (shim/src/re/c3*.cpp) into audio-core port code (step S4.2, for S4.3..S4.9).

The C3 reimplementations call the original's code through the loaded module and dereference host pointers. The port runs
over the context's 32-bit address space (portsim/sim/audio.h), so the converter rewrites, for one function or a list:

  memory      fat<T>(p, off) / fld / fatc / At / F (any `template <typename T> T& NAME(void*, int)` of the file),
              *reinterpret_cast<T*>(e), &fat<T>(p, off)      -> ld<T>(ctx, p + off), st<T>(ctx, p + off, v), (p + off);
              compound assignments (+= |= ...) become a load and a store
  pointers    void* / char* / T* variables, parameters and returns -> u32 virtual addresses; pointer casts are dropped
  globals     PFX_sound(rva)  -> snd_va(ctx, rva) (a named SoundState member or snd_rva constant when one starts there);
              PFX_g<T>(K)     -> a load / store at snd_va(ctx, K)
  calls       PFX_fn<Type>(rva)(...)                -> snd_call(ctx, rva, abi, {...}).eax (abi from the file's typedef:
                                                       __fastcall (void*, void*, ...) = thiscall, its dummy edx dropped)
              reinterpret_cast<Type>(PFX_slot(o, k))(...), PFX_vslot<Type>(o, k)(...), PFX_vfn<Type>(o, k)(...)
                                                    -> snd_vcall(ctx, o, k, abi, {...}).eax
              PFX_imp<Type>(SLOT)(...)              -> snd_import(ctx, SLOT, abi, {...}).eax
              PFX_delete(p) / PFX_new(n)            -> snd_delete(ctx, p) / snd_new(ctx, n)
              PFX_th0(rva, o) .. PFX_th6(rva, o, ...) -> snd_call(ctx, rva, Thiscall, {o, ...}).eax
              PFX_com1..3(o, k, ...)                -> snd_vcall(ctx, o, k, Stdcall, {...}).eax
              strlen(s) / memcpy(d, s, n)           -> vstrlen(ctx.mem, s) / rep_movs(ctx.mem, d, s, n)
              helpers of the same file              -> converted too, `ctx` passed first
  comments    kept; every VA 0x1000xxxx (and 0x100xxxxx) becomes the RVA (VA - 0x10000000), as port code must not hold
              an address of the DLL's image range (port/test/test_static.py)
  macros      a detour made by a function-like #define of the file (C3CN_SOUND_PUSH(AppendNodeB)) is expanded first

and emits, per function: the body `<name>_body(Ctx &ctx, ...)`, an adapter with the original's slots (from the registry
key: abi, ret, args), its negative-control switch, and the FuncDef row. Anything it cannot rewrite is kept and marked
`/*TODO(convert): ...*/` and listed at the end: x87 helpers, inline asm, arrays of host pointers, typed pointer
arithmetic. The output is a starting point the worker edits and judges with port/test/sound_replay.py.

    ~/bin/py -3.12 re/tools/snd_hook2port.py --rva 0x31ac0 [--rva ...] [--out file]      # by rva (sound.dll SG_HOOKs)
    ~/bin/py -3.12 re/tools/snd_hook2port.py --step S4.3 [--out file]                    # every row of a step
    ~/bin/py -3.12 re/tools/snd_hook2port.py --file shim/src/re/c3dj.cpp --detour FindUnlink_re
"""
from __future__ import annotations

import argparse
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SHIM = ROOT / "shim" / "src" / "re"
SOUNDSTATE = ROOT / "port" / "include" / "portsim" / "sim" / "soundstate.h"
FUNCTIONS = ROOT / "re" / "roadmap" / "S4_functions.tsv"
DATA_RVA = 0x63000          # soundstate.h kSoundDataRva (checked against the header at load)

TYPES = {
    "unsigned": "uint32_t", "unsigned int": "uint32_t", "uint32_t": "uint32_t", "DWORD": "uint32_t", "UINT": "uint32_t",
    "int": "int32_t", "int32_t": "int32_t", "long": "int32_t", "LONG": "int32_t", "unsigned long": "uint32_t",
    "unsigned char": "uint8_t", "uint8_t": "uint8_t", "BYTE": "uint8_t", "char": "int8_t", "signed char": "int8_t",
    "int8_t": "int8_t", "unsigned short": "uint16_t", "uint16_t": "uint16_t", "WORD": "uint16_t", "short": "int16_t",
    "int16_t": "int16_t", "float": "float", "double": "double", "bool": "uint8_t", "BOOL": "int32_t",
    "unsigned __int64": "uint64_t", "__int64": "int64_t", "uint64_t": "uint64_t", "int64_t": "int64_t",
}
ABI = {"__cdecl": "SndAbi::Cdecl", "__stdcall": "SndAbi::Stdcall", "WINAPI": "SndAbi::Stdcall", "__fastcall": "SndAbi::Fastcall",
       "__thiscall": "SndAbi::Thiscall"}


# ------------------------------------------------------------------------------------------------ text helpers
def match_paren(s: str, i: int) -> int:
    """index of the bracket closing the one at s[i] ( '(' '[' '{' '<' ), skipping strings and chars"""
    open_, close = s[i], {"(": ")", "[": "]", "{": "}", "<": ">"}[s[i]]
    depth, j = 0, i
    while j < len(s):
        c = s[j]
        if c in "\"'":
            q = c
            j += 1
            while j < len(s) and s[j] != q:
                j += 2 if s[j] == "\\" else 1
        elif c == open_:
            depth += 1
        elif c == close:
            depth -= 1
            if depth == 0:
                return j
        j += 1
    raise ValueError(f"unbalanced {open_} at {i}: {s[i:i + 60]!r}")


def split_top(s: str, sep: str = ",") -> list[str]:
    out, depth, cur, j = [], 0, "", 0
    while j < len(s):
        c = s[j]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "<" and re.match(r"[A-Za-z_]\w*\s*$", cur.split()[-1] if cur.split() else "") and depth >= 0:
            # template argument list of an identifier: keep it whole
            try:
                k = match_paren(s, j)
                if re.fullmatch(r"<[\w\s\*:,&<>]*>", s[j:k + 1]):
                    cur += s[j:k + 1]
                    j = k + 1
                    continue
            except ValueError:
                pass
        if c == sep and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += c
        j += 1
    if cur.strip():
        out.append(cur.strip())
    return out


def ptype(t: str) -> str:
    """the port type of a hook type: any pointer -> u32"""
    t = re.sub(r"\bconst\b|\bvolatile\b", "", t).strip()
    t = re.sub(r"\s+", " ", t)
    if "*" in t or t in ("HANDLE", "HWND", "LPVOID", "PVOID", "LPCSTR", "LPSTR", "HMODULE", "uintptr_t"):
        return "u32"
    return TYPES.get(t, t)


def va2rva(text: str) -> str:
    """VAs of the DLL image (0x1000xxxx..0x101fffff) -> RVAs, so no image-range literal remains"""
    def f(m):
        v = int(m.group(1), 16)
        if 0x10000000 <= v < 0x10200000:
            return "0x%x" % (v - 0x10000000)
        return m.group(0)
    return re.sub(r"\b0x([0-9a-fA-F]{8})\b", f, text)


# ------------------------------------------------------------------------------------------------ the source file
class Source:
    def __init__(self, path: pathlib.Path):
        self.path = path
        raw = path.read_text(encoding="utf-8", errors="replace")
        self.text = raw.replace("\\\r\n", " ").replace("\\\n", " ")
        self.accessors = set(re.findall(r"template\s*<\s*typename\s+T\s*>\s*(?:static\s+)?(?:inline\s+)?T\s*&\s*(\w+)\s*\(",
                                        self.text)) | {"fat", "fld", "fatc", "At"}
        self.globals = set(re.findall(r"template\s*<\s*typename\s+T\s*>\s*(?:static\s+)?(?:inline\s+)?T\s*&\s*(\w+_g)\s*\(",
                                      self.text))
        self.typedefs = {}
        for m in re.finditer(r"typedef\s+([\w\s\*]+?)\s*\(\s*(__\w+|WINAPI)\s*\*\s*(\w+)\s*\)\s*\(([^)]*)\)\s*;", self.text):
            ret, conv, name, params = m.group(1).strip(), m.group(2), m.group(3), m.group(4)
            ps = [p.strip() for p in params.split(",") if p.strip() and p.strip() != "void"]
            this_emul = conv == "__fastcall" and len(ps) >= 2 and "*" in ps[0] and ps[1].replace(" ", "") in ("void*", "void*edx")
            self.typedefs[name] = dict(ret=ret, abi="SndAbi::Thiscall" if this_emul else ABI.get(conv, "SndAbi::Cdecl"),
                                       drop_edx=this_emul, params=ps)
        self.consts = dict(re.findall(r"const\s+unsigned\s+(k\w+)\s*=\s*(0x[0-9a-fA-F]+)\s*;", self.text))
        self.macros = {}
        for m in re.finditer(r"^\s*#define\s+(\w+)\(([^)]*)\)\s*(.*)$", self.text, re.M):
            self.macros[m.group(1)] = ([p.strip() for p in m.group(2).split(",")], m.group(3))
        self.functions = self._functions(self.text)
        self.roles = self._roles()

    def _roles(self) -> dict:
        """module-access wrappers of the file by body shape: name -> (role, extra)
        sound  (rva) -> base + rva            soundva (va) -> base + va - image base     base () -> the module handle
        fn     <F>(rva) -> code at rva        imp <F>(slot) -> the IAT slot's value       g <T>(rva) -> a module global
        vslot  <F>(obj, off) -> [[obj]+off]   slot (obj, off) -> [[obj]+off]              fnptr () -> reinterpret_cast<T>(k)"""
        roles = {}
        for name, (_, sig, body) in self.functions.items():
            if 'GetModuleHandleA("sound.dll")' in body:
                params = sig[sig.index("(") + 1:sig.rindex(")")].strip()
                roles[name] = ("sound", None) if params and params != "void" else ("base", None)
        for _ in range(3):
            for name, (start, sig, body) in self.functions.items():
                if name in roles:
                    continue
                b = re.sub(r"\s+", " ", body)
                ref = lambda role: "|".join(re.escape(n) for n, (r, _) in roles.items() if r == role) or "@@"  # noqa: E731
                snd, base, sva = ref("sound"), ref("base"), ref("soundva")
                templ = "template" in self.text[max(0, start - 60):start]
                if re.search(r"return (?:reinterpret_cast<[^>]*>\()?(?:" + base + r")\(\)\)? \+ \(\w+ - 0x10000000\)", b):
                    roles[name] = ("soundva", None)
                elif "()" in sig.replace(" ", "") and "&" in sig and \
                        re.search(r"return \*reinterpret_cast<([\w ]+?)\s*\*>\((?:" + snd + r")\((0x[0-9a-fA-F]+)\)\)", b):
                    mm = re.search(r"return \*reinterpret_cast<([\w ]+?)\s*\*>\((?:" + snd + r")\((0x[0-9a-fA-F]+)\)\)", b)
                    roles[name] = ("gref", (mm.group(1).strip(), mm.group(2)))
                elif (templ or "template" in sig) and re.search(r"return \*reinterpret_cast<\w+\s*\*>\((?:" + snd + r")\(\w+\)\)", b):
                    roles[name] = ("g" if re.search(r"\bT\s*&", sig) else "imp", None)
                elif re.search(r"return reinterpret_cast<\w+>\((?:" + snd + r")\(\w+\)\)", b):
                    roles[name] = ("fn", None)
                elif re.search(r"return \*reinterpret_cast<\w+\s*\*>\(\*reinterpret_cast<char\s*\*\*>\(\w+\) \+ \w+\)", b):
                    roles[name] = ("vslot", None)
                elif re.search(r"return \w+<void\s*\*>\(\w+<void\s*\*>\(\w+, 0\), \w+\)", b):
                    roles[name] = ("slot", None)
                else:
                    m = re.search(r"return reinterpret_cast<(\w+)>\((?:" + sva + "|" + snd + r")\((0x[0-9a-fA-F]+)\)\)", b)
                    if m and "()" in sig.replace(" ", ""):
                        k = int(m.group(2), 16)
                        roles[name] = ("fnptr", (m.group(1), k - 0x10000000 if k >= 0x10000000 else k))
        return roles

    @staticmethod
    def _functions(text: str) -> dict:
        """name -> (start, signature text, body text with braces) for every function definition at file scope"""
        out = {}
        for m in re.finditer(r"(^|\n)([ \t]*(?:static\s+)?(?:inline\s+)?[\w:\s\*&<>]+?\b(\w+)\s*\(([^;{}()]*(?:\([^)]*\)[^;{}()]*)*)\)\s*)\{",
                             text):
            name = m.group(3)
            if name in ("if", "for", "while", "switch", "return", "sizeof"):
                continue
            b = m.end() - 1
            try:
                e = match_paren(text, b)
            except ValueError:
                continue
            out.setdefault(name, (m.start(2), m.group(2).strip(), text[b:e + 1]))
        return out

    def expand_macro_for(self, detour: str) -> str | None:
        """the text a function-like macro invocation produces when it defines `detour`"""
        for mname, (params, body) in self.macros.items():
            for inv in re.finditer(r"\b" + mname + r"\s*\(", self.text):
                if self.text[max(0, inv.start() - 9):inv.start()].endswith("#define "):
                    continue
                i = inv.end() - 1
                try:
                    j = match_paren(self.text, i)
                except ValueError:
                    continue
                args = split_top(self.text[i + 1:j])
                if len(args) != len(params):
                    continue
                t = body
                for p, a in zip(params, args):
                    t = re.sub(r"\b" + re.escape(p) + r"\s*##\s*", a, t)
                    t = re.sub(r"##\s*" + re.escape(p) + r"\b", a, t)
                    t = re.sub(r"\b" + re.escape(p) + r"\b", a, t)
                t = t.replace("##", "")
                t = re.sub(r";\s*", ";\n", t)
                if re.search(r"\b" + re.escape(detour) + r"\s*\(", t):
                    return t
        return None


def soundstate_names() -> tuple[dict, dict]:
    """(.data offset -> SoundState member, rva -> snd_rva constant) from the generated header"""
    t = SOUNDSTATE.read_text(encoding="utf-8")
    m = re.search(r"kSoundDataRva = (0x[0-9a-f]+)", t)
    assert m and int(m.group(1), 16) == DATA_RVA, "soundstate.h .data rva"
    members = {int(o, 16): n for n, o in re.findall(r"offsetof\(SoundState, (\w+)\) == (0x[0-9a-f]+)", t)}
    rvas = {int(v, 16): n for n, v in re.findall(r"constexpr uint32_t (\w+) = (0x[0-9a-f]+);", t.split("namespace snd_rva")[1]
                                                                                             .split("}  // namespace snd_rva")[0])}
    return members, rvas


# ------------------------------------------------------------------------------------------------ expression rewriting
class Conv:
    def __init__(self, src: Source, helpers: set[str]):
        self.src = src
        self.helpers = helpers
        self.todo: list[str] = []
        self.members, self.rvas = soundstate_names()
        acc = "|".join(sorted(map(re.escape, src.accessors), key=len, reverse=True))
        self.re_acc = re.compile(r"(&\s*)?\b(" + acc + r")\s*<")
        self.prefix = sorted({m.group(1) for m in re.finditer(r"\b(c3\w+?)_sound\s*\(", src.text)}) or ["c3"]
        self.ptrs: dict = {}            # typed pointer variables of the function being converted: name -> (T, size)

    ELEM = {"char": ("int8_t", 1), "signed char": ("int8_t", 1), "unsigned char": ("uint8_t", 1), "uint8_t": ("uint8_t", 1),
            "short": ("int16_t", 2), "unsigned short": ("uint16_t", 2), "uint16_t": ("uint16_t", 2), "int16_t": ("int16_t", 2),
            "unsigned": ("uint32_t", 4), "unsigned int": ("uint32_t", 4), "uint32_t": ("uint32_t", 4), "int": ("int32_t", 4),
            "int32_t": ("int32_t", 4), "float": ("float", 4), "double": ("double", 8)}

    def scan_pointers(self, sig: str, body: str):
        """typed pointer variables (parameters and declarations) whose elements the function indexes"""
        self.ptrs = {}
        for m in re.finditer(r"(?:\bconst\s+)?\b(unsigned\s+char|signed\s+char|unsigned\s+short|unsigned\s+int|char|short|unsigned|"
                             r"int|uint8_t|uint16_t|uint32_t|int16_t|int32_t|float|double)\s*\*\s*(\w+)", sig + " " + body):
            t = re.sub(r"\s+", " ", m.group(1))
            if re.search(r"\b" + re.escape(m.group(2)) + r"\s*\[", body):
                self.ptrs[m.group(2)] = self.ELEM[t]

    def snd_addr(self, rva_text: str) -> str:
        rva_text = self.src.consts.get(rva_text, rva_text)
        try:
            v = int(rva_text, 16) if rva_text.lower().startswith("0x") else int(rva_text)
        except ValueError:
            return f"snd_va(ctx, {rva_text})"
        if v >= DATA_RVA and (v - DATA_RVA) in self.members:
            return f"snd_obj(ctx, &SoundState::{self.members[v - DATA_RVA]}).va()"
        if v in self.rvas:
            return f"snd_va(ctx, snd_rva::{self.rvas[v]})"
        return f"snd_va(ctx, 0x{v:x})"

    def todo_mark(self, what: str, text: str) -> str:
        self.todo.append(f"{what}: {text.strip()[:100]}")
        return f"/*TODO(convert): {what}*/ {text}"

    # one pass over a statement / expression; returns the rewritten text
    def expr(self, s: str) -> str:
        s = self._calls(s)
        s = self._memory(s)
        s = self._casts(s)
        s = self._misc(s)
        return s

    def _call_args(self, s: str, j: int) -> tuple[list[str], int]:
        """the argument list of a call whose '(' is at s[j]: (args rewritten, index after ')')"""
        k = match_paren(s, j)
        return [self.expr(a) for a in split_top(s[j + 1:k])], k + 1

    def _typed_call(self, td: str, args: list[str]) -> tuple[str, list[str], str]:
        info = self.src.typedefs.get(td)
        if info is None:
            return "SndAbi::Cdecl", args, "/*TODO(convert): typedef " + td + "*/"
        a = list(args)
        if info["drop_edx"] and len(a) >= 2:
            a = [a[0]] + a[2:]
        ret = info["ret"].strip()
        suffix = "" if ret == "void" else (".st0" if ret in ("float", "double") else ".eax")
        return info["abi"], a, suffix

    def _calls(self, s: str) -> str:
        out, i = "", 0
        pfx = "|".join(map(re.escape, self.prefix))

        def role(r):
            return "|".join(re.escape(n) for n, (x, _) in self.src.roles.items() if x == r) or "@@none@@"
        pat = re.compile(
            r"\b(?P<fn>" + role("fn") + r")\s*<\s*(?P<t1>\w+)\s*>\s*\(" +
            r"|\breinterpret_cast\s*<\s*(?P<t2>\w+)\s*>\s*\(\s*(?P<slotfn>" + role("slot") + r")\s*\(" +
            r"|\b(?P<vs>" + role("vslot") + r")\s*<\s*(?P<t3>\w+)\s*>\s*\(" +
            r"|\b(?P<imp>" + role("imp") + r")\s*<\s*(?P<t4>\w+)\s*>\s*\(" +
            r"|\b(?P<th>(?:" + pfx + r")_th(?P<thn>\d))\s*\(" +
            r"|\b(?P<com>(?:" + pfx + r")_com(?P<comn>\d))\s*\(" +
            r"|\b(?P<del>(?:" + pfx + r")_delete)\s*\(" +
            r"|\b(?P<new>(?:" + pfx + r")_new)\s*\(" +
            r"|\b(?P<snd>" + role("sound") + r")\s*\(" +
            r"|\b(?P<sva>" + role("soundva") + r")\s*\(" +
            r"|\b(?P<base>" + role("base") + r")\s*\(\s*\)" +
            r"|\b(?P<fnptr>" + role("fnptr") + r")\s*\(\s*\)\s*\(" +
            r"|\b(?P<g>" + role("g") + r")\s*<\s*(?P<t5>[^>]+)\s*>\s*\(" +
            r"|\b(?P<gref>" + role("gref") + r")\s*\(\s*\)" +
            r"|\b(?P<strlen>strlen)\s*\(|\b(?P<memcpy>memcpy)\s*\(" +
            r"|\b(?P<helper>" + ("|".join(map(re.escape, self.helpers)) or "@@none@@") + r")\s*\(")
        while True:
            m = pat.search(s, i)
            if not m:
                return out + s[i:]
            out += s[i:m.start()]
            g = m.groupdict()
            j = m.end() - 1
            if g["fn"]:
                k = match_paren(s, j)
                rva = s[j + 1:k].strip()
                if k + 1 < len(s) and s[k + 1] == "(":
                    args, e = self._call_args(s, k + 1)
                    abi, a, suf = self._typed_call(g["t1"], args)
                    out += f"snd_call(ctx, {self.src.consts.get(rva, rva)}, {abi}, {{{', '.join(a)}}}){suf}"
                    i = e
                else:
                    out += self.snd_addr(rva)
                    i = k + 1
            elif g["slotfn"]:
                k = match_paren(s, j)                       # PFX_slot( o, off )
                o, off = split_top(s[j + 1:k])
                k2 = match_paren(s, m.start() + s[m.start():].index("("))   # the cast's (
                if k2 + 1 < len(s) and s[k2 + 1] == "(":
                    args, e = self._call_args(s, k2 + 1)
                    abi, a, suf = self._typed_call(g["t2"], args)
                    out += f"snd_vcall(ctx, {self.expr(o)}, {off}, {abi}, {{{', '.join(a[1:])}}}){suf}"
                    i = e
                else:
                    out += self.todo_mark("slot address", s[m.start():k2 + 1])
                    i = k2 + 1
            elif g["vs"]:
                k = match_paren(s, j)
                o, off = split_top(s[j + 1:k])
                args, e = self._call_args(s, k + 1)
                abi, a, suf = self._typed_call(g["t3"], args)
                out += f"snd_vcall(ctx, {self.expr(o)}, {off}, {abi}, {{{', '.join(a[1:])}}}){suf}"
                i = e
            elif g["imp"]:
                k = match_paren(s, j)
                slot = s[j + 1:k].strip()
                args, e = self._call_args(s, k + 1)
                abi, a, suf = self._typed_call(g["t4"], args)
                if abi == "SndAbi::Cdecl" and "/*TODO" in suf:
                    abi, suf = "SndAbi::Stdcall", ".eax"
                out += f"snd_import(ctx, {slot}, {abi}, {{{', '.join(a)}}}){suf}"
                i = e
            elif g["th"]:
                args, e = self._call_args(s, j)
                out += f"snd_call(ctx, {args[0]}, SndAbi::Thiscall, {{{', '.join(args[1:])}}}).eax"
                i = e
            elif g["com"]:
                args, e = self._call_args(s, j)
                out += f"snd_vcall(ctx, {args[0]}, {args[1]}, SndAbi::Stdcall, {{{', '.join(args[2:])}}}).eax"
                i = e
            elif g["del"]:
                args, e = self._call_args(s, j)
                out += f"snd_delete(ctx, {args[0]})"
                i = e
            elif g["new"]:
                args, e = self._call_args(s, j)
                out += f"snd_new(ctx, {args[0]})"
                i = e
            elif g["snd"]:
                k = match_paren(s, j)
                out += self.snd_addr(s[j + 1:k].strip())
                i = k + 1
            elif g["sva"]:
                k = match_paren(s, j)
                v = s[j + 1:k].strip()
                try:
                    out += self.snd_addr(hex(int(v, 16) - 0x10000000))
                except ValueError:
                    out += f"snd_va(ctx, {self.expr(v)} - 0x10000000)"
                i = k + 1
            elif g["base"]:
                out += "snd_base(ctx)"
                i = m.end()
            elif g["gref"]:
                t, k = self.src.roles[g["gref"]][1]
                out += f"SNDG<{ptype(t)}>({self.snd_addr(k)})"
                i = m.end()
            elif g["fnptr"]:
                td, rva = self.src.roles[g["fnptr"]][1]
                args, e = self._call_args(s, j)
                abi, a, suf = self._typed_call(td, args)
                out += f"snd_call(ctx, 0x{rva:x}, {abi}, {{{', '.join(a)}}}){suf}"
                i = e
            elif g["g"]:
                k = match_paren(s, j)
                out += f"SNDG<{ptype(g['t5'])}>({self.snd_addr(s[j + 1:k].strip())})"   # resolved by _memory
                i = k + 1
            elif g["strlen"]:
                args, e = self._call_args(s, j)
                out += f"vstrlen(ctx.mem, {args[0]})"
                i = e
            elif g["memcpy"]:
                args, e = self._call_args(s, j)
                out += f"rep_movs(ctx.mem, {', '.join(args)})"
                i = e
            elif g["helper"]:
                args, e = self._call_args(s, j)
                out += f"{g['helper']}(ctx{''.join(', ' + a for a in args)})"
                i = e

    def _memory(self, s: str) -> str:
        """accessor calls and *reinterpret_cast<T*>(e) -> ld / st"""
        # normalise both forms to MEM<T>(addr) first, then decide load / store by what follows
        def norm(text):
            res, i = "", 0
            while True:
                m1 = self.re_acc.search(text, i)
                m2 = re.compile(r"\*\s*reinterpret_cast\s*<\s*([^>]*?)\s*\*\s*>\s*\(").search(text, i)
                m3 = re.compile(r"\bSNDG<([^>]+)>\(").search(text, i)
                ms = [m for m in (m1, m2, m3) if m]
                if not ms:
                    return res + text[i:]
                m = min(ms, key=lambda x: x.start())
                res += text[i:m.start()]
                if m is m1:
                    lt = m.end() - 1
                    gt = match_paren(text, lt)
                    t = ptype(text[lt + 1:gt])
                    p = gt + 1
                    while text[p] == " ":
                        p += 1
                    k = match_paren(text, p)
                    parts = split_top(text[p + 1:k])
                    base, off = norm(parts[0]), norm(parts[1]) if len(parts) > 1 else "0"
                    addr = base if off.strip() in ("0", "0x0", "0x00") else f"{base} + {off}"
                    prev = (res + text[i:m.start()]).rstrip()
                    unary = m.group(1) and not (prev and (prev[-1].isalnum() or prev[-1] in ")]_&"))
                    if unary:                                      # &fat<T>(b, o): the address itself
                        res += f"({addr})"
                    else:
                        res += (m.group(1) or "") + f"MEM<{t}>({addr})"
                    i = k + 1
                elif m is m2:
                    raw = m.group(1).strip()
                    j = m.end() - 1
                    k = match_paren(text, j)
                    inner = norm(text[j + 1:k])
                    if raw in self.src.typedefs:                   # a function pointer read from memory
                        res += f"FNPTR<{raw}>({inner})"
                    elif inner.strip().startswith("&"):              # bits of a local seen as another type
                        res += f"PUNM<{ptype(raw)}>({inner.strip()[1:].strip()})"
                    else:
                        res += f"MEM<{ptype(raw) if '*' not in raw else 'u32'}>({inner})"
                    i = k + 1
                else:
                    j = m.end() - 1
                    k = match_paren(text, j)
                    res += f"MEM<{m.group(1)}>({norm(text[j + 1:k])})"
                    i = k + 1
        s = self._index(s)
        s = norm(s)
        s = self._stage2(s)
        return self._fnptr_calls(s)

    def _index(self, s: str) -> str:
        """p[i] of a typed pointer variable -> MEM<T>(p + i * size)"""
        if not self.ptrs:
            return s
        pat = re.compile(r"\b(" + "|".join(map(re.escape, self.ptrs)) + r")\s*\[")
        out, i = "", 0
        while True:
            m = pat.search(s, i)
            if not m:
                return out + s[i:]
            j = m.end() - 1
            k = match_paren(s, j)
            t, size = self.ptrs[m.group(1)]
            idx = self._index(s[j + 1:k])
            off = idx if size == 1 else f"({idx}) * {size}"
            out += s[i:m.start()] + f"MEM<{t}>({m.group(1)} + {off})"
            i = k + 1

    def _fnptr_calls(self, s: str) -> str:
        """(FNPTR<TD>(a))(args) / FNPTR<TD>(a)(args) -> snd_call_va(ctx, ld<u32>(ctx, a), abi, {args})"""
        pat = re.compile(r"\(?\s*FNPTR<(\w+)>\(")
        out, i = "", 0
        while True:
            m = pat.search(s, i)
            if not m:
                return out + s[i:]
            j = m.end() - 1
            k = match_paren(s, j)
            addr = s[j + 1:k]
            e = k + 1
            wrapped = m.group(0).startswith("(")
            if wrapped:
                while e < len(s) and s[e] == " ":
                    e += 1
                if e < len(s) and s[e] == ")":
                    e += 1
            while e < len(s) and s[e] == " ":
                e += 1
            if e < len(s) and s[e] == "(":
                q = match_paren(s, e)
                args = split_top(s[e + 1:q])
                abi, a, suf = self._typed_call(m.group(1), args)
                out += s[i:m.start()] + f"snd_call_va(ctx, ld<u32>(ctx, {addr}), {abi}, {{{', '.join(a)}}}){suf}"
                i = q + 1
            else:
                out += s[i:m.start()] + self.todo_mark("function pointer value", f"ld<u32>(ctx, {addr})")
                i = e

    def _stage2(self, s: str) -> str:
        # stores: MEM<T>(a) OP= rhs;  (statement level); every other MEM is a load
        out, i = "", 0
        pat = re.compile(r"(MEM|PUNM)<([^>]+)>\(")
        while True:
            m = pat.search(s, i)
            if not m:
                out += s[i:]
                break
            j = m.end() - 1
            k = match_paren(s, j)
            pun = m.group(1) == "PUNM"
            t, addr = m.group(2), self._stage2(s[j + 1:k])
            rest = s[k + 1:]
            am = re.match(r"\s*(\+|-|\||&|\^|<<|>>|\*)?=(?!=)\s*", rest)
            before = s[:m.start()].rstrip()
            stmt_start = (not before) or before[-1] in ";{}" or before.endswith(")") and re.search(r"\b(if|else|for|while)\b", before[-40:]) \
                or before.endswith("else")
            if am and stmt_start:
                e = k + 1 + am.end()
                depth, q = 0, e
                while q < len(s) and not (depth == 0 and s[q] == ";"):
                    depth += s[q] in "([{"
                    depth -= s[q] in ")]}"
                    q += 1
                rhs = self._stage2(s[e:q].strip())
                op = am.group(1)
                val = rhs if not op else f"ld<{t}>(ctx, {addr}) {op} {self._paren(rhs)}"
                if op:
                    val = f"static_cast<{t}>({val})"
                if pun:
                    val = val.replace(f"ld<{t}>(ctx, {addr})", f"pun<{t}>({addr})")
                    out += s[i:m.start()] + f"pun_store<{t}>({addr}, {val})"
                else:
                    out += s[i:m.start()] + f"st<{t}>(ctx, {addr}, {val})"
                i = q
            else:
                out += s[i:m.start()] + (f"pun<{t}>({addr})" if pun else f"ld<{t}>(ctx, {addr})")
                i = k + 1
        return out

    @staticmethod
    def _paren(e: str) -> str:
        return e if re.fullmatch(r"[\w\.]+|0x[0-9a-fA-F]+u?", e.strip()) else f"({e})"

    def _casts(self, s: str) -> str:
        # pointer casts disappear (everything is a u32 address); other casts keep with port types
        def f(m):
            j = m.end() - 1
            k = match_paren(s2[0], j)
            inner = s2[0][j + 1:k]
            return None, j, k, inner
        s2 = [s]
        out, i = "", 0
        pat = re.compile(r"\b(static_cast|reinterpret_cast)\s*<\s*([^<>]*?)\s*>\s*\(")
        while True:
            m = pat.search(s, i)
            if not m:
                return out + s[i:]
            j = m.end() - 1
            k = match_paren(s, j)
            inner = self._casts(s[j + 1:k])
            t = m.group(2)
            if "*" in t or t in ("uintptr_t", "unsigned", "uint32_t") and m.group(1) == "reinterpret_cast":
                out += s[i:m.start()] + f"({inner})" if not re.fullmatch(r"[\w\.]+", inner.strip()) else s[i:m.start()] + inner.strip()
            else:
                out += s[i:m.start()] + f"static_cast<{ptype(t)}>({inner})"
            i = k + 1

    def _misc(self, s: str) -> str:
        s = re.sub(r"\(\s*(?:const\s+)?(?:void|char|unsigned char|unsigned)\s*\*\s*\)", "", s)     # C-style pointer casts
        s = re.sub(r"\b(?:const\s+)?(?:void|char|unsigned char|uint8_t|unsigned|int|short|float|[A-Z]\w*)\s*\*\s*(?=\w)", "u32 ", s)
        s = re.sub(r"\bnullptr\b|\bNULL\b", "0", s)
        if "__asm" in s:
            s = self.todo_mark("inline asm", s)
        if re.search(r"\bx87|\bfld\b|fstp|_controlfp|c3\w*_x87", s):
            s = self.todo_mark("x87", s)
        return s


# ------------------------------------------------------------------------------------------------ functions
def convert_signature(sig: str, name_map: dict) -> tuple[str, list[tuple[str, str]], str]:
    """(port return type, [(type, name)] without the thiscall dummy, new name)"""
    m = re.match(r"(?:static\s+)?(?:inline\s+)?(.*?)\b(?:(__fastcall|__cdecl|__stdcall|WINAPI)\s+)?(\w+)\s*\((.*)\)\s*$", sig, re.S)
    ret, conv, name, params = m.group(1).strip(), m.group(2), m.group(3), m.group(4)
    ps = [p.strip() for p in split_top(params) if p.strip() and p.strip() != "void"]
    out = []
    for k, p in enumerate(ps):
        pm = re.match(r"(.*?)(\w+)\s*$", p)
        if pm and pm.group(1).strip():
            t, n = pm.group(1).strip(), pm.group(2)
        else:
            t, n = p, f"unused{k}"
        if conv == "__fastcall" and k == 1 and t.replace(" ", "") == "void*" and (not pm or not pm.group(1).strip() or n.startswith("unused")):
            continue                                       # the dummy edx of the thiscall emulation
        out.append((ptype(t), n))
    return ptype(ret) if ret != "void" else "void", out, name_map.get(name, name)


def convert_body(cv: Conv, body: str) -> str:
    """the body's statements rewritten, comments kept (VA -> RVA)"""
    lines = body.split("\n")
    out = []
    for ln in lines:
        code, sep, com = ln.partition("//")
        code = cv.expr(code) if code.strip() else code
        out.append((code.rstrip() + ("  //" + va2rva(com) if sep else "")).rstrip())
    return "\n".join(out)


def collect(src: Source, detour: str):
    """(definitions text [(name, sig, body)], helper names) for the detour and the file-scope helpers it reaches"""
    text = src.text
    funcs = dict(src.functions)
    if detour not in funcs:
        t = src.expand_macro_for(detour)
        if t is None:
            raise SystemExit(f"{src.path.name}: no definition of {detour}")
        funcs.update(Source._functions(t))
    order, seen = [], set()

    def visit(n):
        if n in seen or n not in funcs:
            return
        seen.add(n)
        _, sig, body = funcs[n]
        for callee in re.findall(r"\b(\w+)\s*\(", body):
            if callee in funcs and callee != n and callee not in src.roles and \
                    not callee.endswith(("_sound", "_fn", "_delete", "_new", "_slot")):
                if re.match(r"c3\w+_(th\d|com\d|imp|vslot|vfn|g)$", callee):
                    continue
                visit(callee)
        order.append(n)
    visit(detour)
    return [(n, funcs[n][1], funcs[n][2]) for n in order], {n for n in order if n != detour}


def registry_spec(rva: int):
    sys.path.insert(0, str(ROOT / "port" / "test"))
    import sound_emu as E
    hooks, _ = E.registry()
    for k, v in sorted(hooks.items()):
        if (v.get("module") or "").lower() == "sound.dll" and v["addr"] == rva:
            return k, v
    return None, None


SETRET = {"void": None, "pointer": "set_u32", "uint32": "set_u32", "int": "set_i32", "uint8": "set_u32", "int8": "set_i32",
          "uint16": "set_u32", "int16": "set_i32", "bool": "set_u32", "float": "set_double", "double": "set_double"}


def convert_function(path: pathlib.Path, detour: str, port_name: str, rva: int | None,
                     emitted: set | None = None) -> tuple[str, list[str]]:
    """`emitted`: (file, helper) pairs already written by an earlier function of the same output (written once)"""
    src = Source(path)
    defs, helpers = collect(src, detour)
    body_name = re.sub(r"\W", "_", port_name) + "_body"
    names = {detour: body_name}
    cv = Conv(src, set(helpers))
    out = [f"// ---- {port_name}" + (f" (rva 0x{rva:x})" if rva else "") + f": converted from {path.name} {detour}"]
    emitted = set() if emitted is None else emitted
    for n, sig, body in defs:
        if n != detour:
            if (path.name, n) in emitted:
                continue
            emitted.add((path.name, n))
        # the definition's comment block above it (citations), VAs rewritten
        start = src.text.find(sig)
        pre = src.text[:start].rstrip("\n ").split("\n")
        com = []
        for ln in reversed(pre):
            if ln.strip().startswith("//"):
                com.append(va2rva(ln.strip()))
            else:
                break
        ret, params, newname = convert_signature(sig, names)
        ps = ", ".join(["Ctx &ctx"] + [f"{t} {nm}" for t, nm in params])
        out += list(reversed(com))
        cv.scan_pointers(sig, body)
        out.append(f"{ret} {newname}({ps}) " + convert_body(cv, body))
    # the file's named constants (IAT slots, global rvas) the converted text uses, declared once per output
    used = set(re.findall(r"\b(k[A-Z]\w*)\b", "\n".join(out)))
    decl = []
    for k in sorted(used):
        if k in src.consts and ("const", k) not in emitted:
            emitted.add(("const", k))
            decl.append(f"constexpr uint32_t {k} = 0x{int(src.consts[k], 16):x};  // {path.name}")
    out[1:1] = decl
    # adapter + registry row from the registry key
    key, spec = registry_spec(rva) if rva else (None, None)
    neg = re.sub(r"\W", "_", port_name)
    out.append(f"int g_negctl_{neg} = 0;   // TODO(convert): give the body a wrong-result mode read through SG_NEGCTL")
    if spec:
        args = [f"a[{i}]" for i in range(len(spec["args"]))]
        r = SETRET.get(spec["ret"], "set_u32")
        call = f"{body_name}(ctx{''.join(', ' + a for a in args)})"
        line = f"r.{r}({call});" if r else f"{call};"
        out.append(f"void a_{neg}(Ctx &ctx, const uint32_t *a, Ret &r) {{ {line} }}")
        out.append(f'// FuncDef: {{"{port_name}", "{spec["abi"]}", "{spec["ret"]}", {len(spec["args"])}, 0, a_{neg}, &g_negctl_{neg}}},'
                   f"   // registry key {key}")
    else:
        out.append(f"// TODO(convert): no registry key for rva {rva}: write the adapter by hand")
    return "\n".join(out) + "\n", cv.todo


def detour_of(path: pathlib.Path, rva: int) -> str | None:
    t = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'SG_HOOK\("sound\.dll",\s*0x0*%x\s*,\s*[^,]+,\s*(\w+)\s*,' % rva, t, re.I)
    return m.group(1) if m else None


def rows_for(step: str | None, rvas: list[int]) -> list[dict]:
    with FUNCTIONS.open(newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    if step:
        return [r for r in rows if r["step"] == step and r["hook_source"]]
    return [r for r in rows if int(r["rva"], 16) in rvas and r["hook_source"]]


TU_HEAD = """// generated by re/tools/snd_hook2port.py --tu (a compile check of the conversion, not a module)
#include <cstdint>
#include <cstring>
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/audio.h"
namespace portsim {
namespace {
using u32 = uint32_t;
// the bits of a local seen as another type (the hook's *reinterpret_cast<T*>(&x)): a copy, never a value conversion
template <class T, class S> T pun(const S &s) { static_assert(sizeof(T) == sizeof(S), "pun"); T t; std::memcpy(&t, &s, sizeof t); return t; }
template <class T, class S> void pun_store(S &s, T v) { static_assert(sizeof(T) == sizeof(S), "pun"); std::memcpy(&s, &v, sizeof v); }
#if defined(_MSC_VER)
#pragma warning(disable : 4100 4189 4505)
#endif
"""
TU_TAIL = """}  // namespace
}  // namespace portsim
"""


VCVARS = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat")


def compile_check(tu: pathlib.Path) -> tuple[int, str]:
    """cl /c of a --tu output against the port headers (MSVC x64, /W4 like the port): (exit code, compiler output)"""
    import subprocess
    import tempfile
    inc = [ROOT / "port" / "include", ROOT / "re" / "model" / "include"]
    with tempfile.TemporaryDirectory() as d:
        bat = pathlib.Path(d) / "cc.bat"
        bat.write_text(f'@call "{VCVARS}" >nul\r\ncl /nologo /c /std:c++17 /W4 /permissive- /EHsc '
                       + " ".join(f'/I"{p}"' for p in inc) + f' /Fo"{d}\\tu.obj" "{tu}"\r\n')
        r = subprocess.run(["cmd", "/c", str(bat)], capture_output=True, text=True)
        return r.returncode, r.stdout + r.stderr


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--rva", action="append", default=[])
    ap.add_argument("--step")
    ap.add_argument("--file")
    ap.add_argument("--detour")
    ap.add_argument("--name")
    ap.add_argument("--out")
    ap.add_argument("--compile", action="store_true", help="with --tu --out: compile the result (cl /c /W4) and report")
    ap.add_argument("--tu", action="store_true", help="wrap the output as a translation unit (includes, namespace) for a compile check")
    ap.add_argument("--module", help="emit a test-only port module of that name (FuncDef table + ModuleDef)")
    ap.add_argument("--prefix", default="conv:", help="--module: registered name = prefix + hooks.csv name")
    a = ap.parse_args(argv)
    chunks, todos = [], []
    emitted = set()
    if a.file:
        text, td = convert_function(pathlib.Path(a.file), a.detour, a.name or a.detour, None)
        chunks.append(text)
        todos += td
    else:
        for r in rows_for(a.step, [int(x, 16) for x in a.rva]):
            rva = int(r["rva"], 16)
            path = SHIM / r["hook_source"]
            d = detour_of(path, rva)
            if d is None:
                chunks.append(f"// TODO(convert): {r['name']} rva 0x{rva:x}: no SG_HOOK in {path.name}\n")
                continue
            try:
                text, td = convert_function(path, d, r["name"], rva, emitted)
            except Exception as e:  # noqa: BLE001
                text, td = f"// TODO(convert): {r['name']} rva 0x{rva:x}: {type(e).__name__}: {e}\n", [str(e)]
            chunks.append(text)
            todos += [f"{r['name']}: {x}" for x in td]
    res = "\n".join(chunks) + "\n// TODO list\n" + "".join(f"//   {t}\n" for t in todos)
    if a.tu or a.module:
        res = TU_HEAD + res
        if a.module:
            # a test-only port module of the converted functions, registered as <prefix><hooks.csv name>
            rows = re.findall(r'^// FuncDef: \{"([^"]+)"(.*?)\},', res, re.M)
            res += "const FuncDef kConvFuncs[] = {\n" + "".join(f'    {{"{a.prefix}{n}"{rest}}},\n' for n, rest in rows) + "};\n"
            res += TU_TAIL + (f"extern const ModuleDef kModule_{a.module} = {{\"{a.module}\", kConvFuncs, "
                              f"sizeof(kConvFuncs) / sizeof(kConvFuncs[0])}};\n}}  // namespace portsim\n")
            res = res.replace("}  // namespace\n}  // namespace portsim\nextern", "}  // namespace\nextern")
        else:
            res += TU_TAIL
    if a.out:
        pathlib.Path(a.out).write_text(res, encoding="utf-8")
        print(f"wrote {a.out}: {len(chunks)} functions, {len(todos)} TODO")
        if a.compile:
            rc, txt = compile_check(pathlib.Path(a.out).resolve())
            errs = [ln for ln in txt.splitlines() if ": error" in ln or ": warning" in ln]
            print("\n".join(errs[:80]))
            print(f"compile: exit {rc}, {sum(': error' in e for e in errs)} errors, {sum(': warning' in e for e in errs)} warnings")
            return rc
    else:
        print(res)
    return 0


if __name__ == "__main__":
    sys.exit(main())
