"""Static survey of golf_clean.exe's network layer (roadmap S9.1).

Re-derives, from the anchored binary alone, the facts re/analysis/net/NET_MODEL.md states:
the import directory, the DirectPlay GUIDs and their references, the LoadLibraryA sites, every
reference to the net globals (read / write, value stored), and every call through the
IDirectPlay2A pointer g_netInterface with its slot name from the DX8 SDK's dplay.h.

    ~/bin/py -3.12 re/tools/net_survey.py report      # print the derived facts
    ~/bin/py -3.12 re/tools/net_survey.py check       # compare with re/port/net_*.tsv (exit 1 on a difference)

No game run, no Ghidra; capstone over original/golf_clean.exe and re/functions_ghidra.tsv.
"""
import re
import struct
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / 'original' / 'golf_clean.exe'
FUNCS = ROOT / 're' / 'functions_ghidra.tsv'
DPLAY_H = ROOT / 'tools' / 'dxsdk8' / 'include' / 'dplay.h'
DPLOBBY_H = ROOT / 'tools' / 'dxsdk8' / 'include' / 'dplobby.h'
BASE = 0x400000
NET_LO, NET_HI = 0x00497b00, 0x0049bf70     # gfxFlushA .. end of netListRemove
G_NET_INTERFACE = 0x008400b0                 # IDirectPlay2A *
G_NET_SESSION = 0x008400b8                   # the Jackal net session object
NET_GLOBALS = (0x0083ff6c, 0x0083ff9c, 0x0083ffa8, 0x008400ac, G_NET_INTERFACE,
               G_NET_SESSION, 0x008400bc, 0x008400c0)


class Image:
    def __init__(self, path=EXE):
        self.pe = pefile.PE(str(path))
        self.img = self.pe.get_memory_mapped_image()
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.iat = {}
        for d in self.pe.DIRECTORY_ENTRY_IMPORT:
            for i in d.imports:
                self.iat[i.address] = d.dll.decode() + '!' + (i.name or b'').decode()
        self.sections = {s.Name.rstrip(b'\0').decode(): s for s in self.pe.sections}

    def u32(self, va):
        return struct.unpack_from('<I', self.img, va - BASE)[0]

    def cstr(self, va, limit=128):
        r = va - BASE
        if not (0 <= r < len(self.img)):
            return None
        e = r
        while e < len(self.img) and 0x20 <= self.img[e] < 0x7f and e - r < limit:
            e += 1
        return self.img[r:e].decode() if e > r and self.img[e] == 0 else None

    def disasm(self, lo, hi):
        return self.md.disasm(self.img[lo - BASE:hi - BASE], lo)

    def dword_hits(self, value):
        pat = re.escape(struct.pack('<I', value))
        return [m.start() + BASE for m in re.finditer(pat, self.img)]


def load_functions(path=FUNCS):
    """[(lo, hi, entry, name)] for every range of every listed function."""
    out = []
    for line in open(path, encoding='utf-8'):
        p = line.rstrip('\n').split('\t')
        if p[0] == 'entry':
            continue
        for r in p[5].split(';'):
            a, b = r.split('-')
            out.append((int(a, 16), int(b, 16), int(p[0], 16), p[1]))
    return sorted(out)


def function_of(funcs, va):
    for lo, hi, ent, name in funcs:
        if lo <= va < hi:
            return ent, name
    return None, None


def interface_methods(header=DPLAY_H, iface='IDirectPlay2'):
    """Method names of a COM interface in vtable order, read from the SDK header."""
    txt = open(header, errors='replace').read()
    m = re.search(r'DECLARE_INTERFACE_\(\s*%s\s*,\s*IUnknown\s*\)(.*?)\};' % iface, txt, re.S)
    if not m:
        raise SystemExit('interface %s not found in %s' % (iface, header))
    return re.findall(r'STDMETHOD_?\((?:\w+\s*,\s*)?(\w+)\)', m.group(1))



X86 = capstone.x86


class Body:
    """One function's instructions (all ranges) with its intra-function jump sources."""

    def __init__(self, im, ranges):
        self.ins = [i for a, b in ranges for i in im.disasm(a, b)]
        self.at = {i.address: k for k, i in enumerate(self.ins)}
        self.src = {}
        for k, i in enumerate(self.ins):
            if i.group(X86.X86_GRP_JUMP) and i.operands and i.operands[0].type == X86.X86_OP_IMM:
                self.src.setdefault(i.operands[0].imm, []).append(k)

    def defs(self, k, reg, depth=4, seen=None):
        """Instructions that last wrote `reg` before index k, following jump sources backwards."""
        seen = set() if seen is None else seen
        out = []
        while k > 0 and depth >= 0:
            if k in seen:
                return out
            seen.add(k)
            for s in self.src.get(self.ins[k].address, []):
                out += self.defs(s, reg, depth - 1, seen)
            prev = self.ins[k - 1]
            if prev.mnemonic in ('jmp', 'ret') or prev.mnemonic == 'call' and reg in (
                    X86.X86_REG_EAX, X86.X86_REG_ECX, X86.X86_REG_EDX):
                return out
            k -= 1
            if reg in self.ins[k].regs_access()[1]:
                return out + [self.ins[k]]
        return out


def _is_load(ins, disp=None):
    if ins.mnemonic != 'mov' or len(ins.operands) != 2:
        return False
    d, s = ins.operands
    if d.type != X86.X86_OP_REG or s.type != X86.X86_OP_MEM or s.mem.index:
        return False
    if disp is not None:                     # mov R, [absolute]
        return s.mem.base == 0 and (s.mem.disp & 0xffffffff) == disp
    return s.mem.base != 0 and s.mem.disp == 0   # mov R, [R2]


def function_bodies(im, funcs, keep):
    """{(entry, name): Body} of the kept functions; bodies are cached on the Image."""
    cache = im.__dict__.setdefault('_bodies', {})
    by = {}
    for a, b, ent, name in funcs:
        if keep(ent):
            by.setdefault((ent, name), []).append((a, b))
    out = {}
    for k, v in by.items():
        if k not in cache:
            cache[k] = Body(im, v)
        out[k] = cache[k]
    return out


def interface_calls(im, funcs, keep=lambda e: NET_LO <= e < NET_HI or e in GATES):
    """Calls through g_netInterface's vtable: [(site, function, slot_offset, method, args)].

    `call [V + d]` counts when every def of V is `mov V, [I]` and every def of I is
    `mov I, [g_netInterface]` (defs followed back through jump sources). `args` are the operands
    pushed since the previous call in address order, first argument (the interface) first.
    """
    methods = interface_methods()
    out = []
    for (ent, name), body in function_bodies(im, funcs, keep).items():
        last_call = 0
        for k, ins in enumerate(body.ins):
            if ins.mnemonic != 'call':
                continue
            pushes = [i.op_str for i in body.ins[last_call:k] if i.mnemonic == 'push']
            last_call = k + 1
            op = ins.operands[0]
            if op.type != X86.X86_OP_MEM or op.mem.index or not op.mem.base:
                continue
            vd = body.defs(k, op.mem.base)
            if not vd or not all(_is_load(v) for v in vd):
                continue
            ok = True
            for v in vd:
                idefs = body.defs(body.at[v.address], v.operands[1].mem.base)
                if not idefs or not all(_is_load(i, disp=G_NET_INTERFACE) for i in idefs) \
                        or v.operands[1].mem.disp != 0:
                    ok = False
            if ok:
                d = op.mem.disp
                out.append((ins.address, name, d, methods[d // 4], list(reversed(pushes))))
    return sorted(out)


GATES = (0x0047ee70, 0x00495d20)    # Window::onCommand, uiIdleNetPoll
SENDERS = {0x00497fc0: 'netService', 0x00499140: 'netSendMessages'}
SENDER_ARGS = ('data', 'size', 'dest', 'type', 'flags')
DATA_LO, DATA_HI = 0x004c1000, 0x004c1000 + 0x381174


def pushed_args(body, k, n):
    """The n operands pushed before call index k, first argument first, walking back along
    fall-through and, at a block entered only by jumps, the first jump source."""
    out, seen = [], set()
    while k > 0 and len(out) < n and k not in seen:
        seen.add(k)
        prev = body.ins[k - 1]
        if prev.mnemonic in ('jmp', 'ret'):
            srcs = body.src.get(body.ins[k].address, [])
            if not srcs:
                break
            k = srcs[0]
            continue
        k -= 1
        if body.ins[k].mnemonic == 'push':
            out.append(body.ins[k].op_str)
        elif body.ins[k].mnemonic == 'call':
            break
    return out


def sender_calls(im, funcs):
    """Every direct call of netService / netSendMessages: [(site, caller, sender, args dict)]."""
    out = []
    for (ent, name), body in function_bodies(im, funcs, lambda e: 0x401000 <= e < 0x4b9430).items():
        for k, ins in enumerate(body.ins):
            if ins.mnemonic == 'call' and ins.operands[0].type == X86.X86_OP_IMM \
                    and ins.operands[0].imm in SENDERS:
                args = dict(zip(SENDER_ARGS, pushed_args(body, k, 5)))
                out.append((ins.address, name, SENDERS[ins.operands[0].imm], args))
    return sorted(out)


def net_function_set(im, funcs):
    """Entries of the net layer: the range, the gates, and every function that references a
    net global or makes an interface call."""
    keep = set(GATES)
    for a, b, ent, name in funcs:
        if NET_LO <= ent < NET_HI:
            keep.add(ent)
    for va in NET_GLOBALS:
        for site in im.dword_hits(va):
            ent, _ = function_of(funcs, site)
            if ent is not None and ent < 0x4b9430:
                keep.add(ent)
    return keep


def global_refs(im, funcs, entries):
    """{va: [(site, function, 'r'|'w'|'addr', stored operand or '')]} for absolute .data
    references of the given functions (memory operands with no base / index, and immediates)."""
    out = {}
    names = {}
    for a, b, ent, name in funcs:
        names[ent] = name
    for (ent, name), body in function_bodies(im, funcs, lambda e: e in entries).items():
        for ins in body.ins:
            for n, op in enumerate(ins.operands):
                if op.type == X86.X86_OP_MEM and not op.mem.base and not op.mem.index:
                    va = op.mem.disp & 0xffffffff
                    kind = 'w' if op.access & capstone.CS_AC_WRITE else 'r'
                elif op.type == X86.X86_OP_IMM:
                    va, kind = op.imm & 0xffffffff, 'addr'
                else:
                    continue
                if not DATA_LO <= va < DATA_HI:
                    continue
                stored = ''
                if kind == 'w' and len(ins.operands) == 2 and n == 0:
                    src = ins.operands[1]
                    stored = hex(src.imm & 0xffffffff) if src.type == X86.X86_OP_IMM else ins.reg_name(src.reg)
                out.setdefault(va, []).append((ins.address, name, kind, stored))
    return out


def direct_callers(im, funcs, entries):
    """{entry: sorted caller entries} over direct `call` / `jmp` rel32 in every function."""
    out = {e: set() for e in entries}
    for (ent, name), body in function_bodies(im, funcs, lambda e: 0x401000 <= e < 0x4b9430).items():
        for ins in body.ins:
            if ins.mnemonic in ('call', 'jmp') and ins.operands and ins.operands[0].type == X86.X86_OP_IMM:
                t = ins.operands[0].imm
                if t in out and t != ent:
                    out[t].add(ent)
    return {e: sorted(v) for e, v in out.items()}


DPLAY_GUIDS = ('IID_IDirectPlay2A', 'DPAID_INet', 'DPSPGUID_TCPIP', 'DPSPGUID_MODEM', 'DPSPGUID_SERIAL')


def sdk_guids():
    pat = re.compile(r'DEFINE_GUID\(\s*(\w+)\s*,' + ','.join([r'\s*(0x[0-9a-fA-F]+|\d+)L?\s*'] * 11))
    out = {}
    for h in (DPLAY_H, DPLOBBY_H):
        for m in pat.finditer(open(h, errors='replace').read()):
            v = [int(x, 0) for x in m.groups()[1:]]
            out[m.group(1)] = struct.pack('<IHH8B', *v)
    return out


def facts(im, funcs):
    """The transport facts NET_MODEL.md section 1 states."""
    f = {'imports': [d.dll.decode() for d in im.pe.DIRECTORY_ENTRY_IMPORT]}
    raw = im.img.find(b'DPLAYX.dll\0')
    f['dplayx_string'] = raw + BASE if raw >= 0 else None
    f['dplayx_string_refs'] = len(im.dword_hits(f['dplayx_string'])) if raw >= 0 else 0
    g = {}
    for name, blob in sdk_guids().items():
        off = im.img.find(blob, 0xba000)
        if 0 <= off < 0xc1000:
            g[name] = (off + BASE, len(im.dword_hits(off + BASE)))
    f['guids'] = g
    ll = []
    for (ent, name), body in function_bodies(im, funcs, lambda e: 0x401000 <= e < 0x4b9430).items():
        for k, ins in enumerate(body.ins):
            op = ins.operands[0] if ins.operands else None
            if ins.mnemonic == 'call' and op is not None and op.type == X86.X86_OP_MEM \
                    and not op.mem.base and im.iat.get(op.mem.disp & 0xffffffff, '').endswith('!LoadLibraryA'):
                arg = (pushed_args(body, k, 1) or ['?'])[0]
                s = im.cstr(int(arg, 16)) if arg.startswith('0x') else None
                ll.append((ins.address, name, arg, s))
    f['loadlibrary'] = sorted(ll)
    data = im.sections['.data']
    f['data_raw_end'] = BASE + data.VirtualAddress + data.SizeOfRawData
    imps = {n for n in im.iat.values()}
    f['thread_or_event_imports'] = sorted(n for n in imps if re.search(
        r'!(CreateThread|CreateEvent\w*|_beginthread\w*|CoCreateInstance|WSA\w+|socket)$', n))
    return f


PORT = ROOT / 're' / 'port'
TSV = {k: PORT / ('net_%s.tsv' % k) for k in ('functions', 'state', 'messages')}
FN_COLS = ('addr', 'name', 'size', 'level', 'path1', 'port_module', 'reach', 'callers', 'dp_calls',
           'net_globals', 'role')
ST_COLS = ('va', 'symbol', 'gs_class', 'readers', 'writers', 'stored', 'meaning')
HAND = {'functions': ('role',), 'state': ('meaning',)}


def _tsv(path, key):
    rows = {}
    if Path(path).exists():
        lines = open(path, encoding='utf-8').read().splitlines()
        head = lines[0].split('\t')
        for line in lines[1:]:
            r = dict(zip(head, line.split('\t')))
            rows[r[key]] = r
    return rows


def gs_objects():
    """[(va, size, symbol, class)] from port/include/gamestate_meta.inc."""
    out = []
    pat = re.compile(r'SG_OBJECT\((\w+), (0x[0-9a-f]+), (0x[0-9a-f]+), (\w+),')
    for m in pat.finditer(open(ROOT / 'port' / 'include' / 'gamestate_meta.inc').read()):
        out.append((DATA_LO + int(m.group(2), 16), int(m.group(3), 16), m.group(1), m.group(4)))
    return out


def derive_functions(im, funcs):
    ents = net_function_set(im, funcs)
    sizes = {int(l.split('\t')[0], 16): l.split('\t') for l in open(FUNCS).read().splitlines()[1:]}
    hooks = {r['addr']: r for r in _csv_rows(ROOT / 'hooks.csv') if r['module'] == 'golf_clean.exe'}
    scope = _tsv(PORT / 'scope.tsv', 'addr')
    board = {r['addr']: r for r in _tsv_list(ROOT / 're' / 'scoreboard.tsv') if r['module'] == 'golf_clean.exe'}
    calls = {}
    for site, fname, d, meth, _ in interface_calls(im, funcs, keep=lambda e: e in ents):
        calls.setdefault(fname, []).append(meth)
    refs = global_refs(im, funcs, ents)
    callers = direct_callers(im, funcs, ents)
    rows = []
    for e in sorted(ents):
        a = '%08x' % e
        name = sizes[e][1]
        g = sorted('%08x' % va for va, lst in refs.items() if va in NET_GLOBALS and any(x[1] == name for x in lst))
        m = calls.get(name, [])
        rows.append({'addr': a, 'name': name, 'size': sizes[e][2],
                     'level': hooks.get(a, {}).get('confidence', '-'),
                     'path1': str(len(list((ROOT / 'log' / 'diff').glob(a + '_*.path1.csv')))),
                     'port_module': scope.get(a, {}).get('port_module', '-'),
                     'reach': board.get(a, {}).get('reach', '-'),
                     'callers': ';'.join('%08x' % c for c in callers[e]) or 'none',
                     'dp_calls': ';'.join('%s x%d' % (k, m.count(k)) for k in sorted(set(m))) or '-',
                     'net_globals': ';'.join(g) or '-'})
    return rows


def _csv_rows(path):
    import csv
    return list(csv.DictReader(open(path, encoding='utf-8', newline='')))


def _tsv_list(path):
    lines = open(path, encoding='utf-8').read().splitlines()
    head = lines[0].split('\t')
    return [dict(zip(head, l.split('\t'))) for l in lines[1:]]


def derive_state(im, funcs):
    ents = net_function_set(im, funcs)
    objs = gs_objects()
    rows = []
    for va, lst in sorted(global_refs(im, funcs, ents).items()):
        sym, cls = '-', '-'
        for o, size, s, c in objs:
            if o <= va < o + size:
                sym, cls = (s if o == va else '%s+0x%x' % (s, va - o)), c
        rd = sorted({x[1] for x in lst if x[2] in ('r', 'addr')})
        wr = sorted({x[1] for x in lst if x[2] == 'w'})
        st = sorted({x[3] for x in lst if x[2] == 'w'})
        rows.append({'va': '%08x' % va, 'symbol': sym, 'gs_class': cls,
                     'readers': ';'.join(rd) or '-', 'writers': ';'.join(wr) or '-',
                     'stored': ';'.join(st) or '-'})
    return rows


def _merge(kind, derived, key, cols):
    old = _tsv(TSV[kind], key)
    for r in derived:
        for h in HAND[kind]:
            r[h] = old.get(r[key], {}).get(h, '')
    return ['\t'.join(cols)] + ['\t'.join(r[c] for c in cols) for r in derived]


def compare(kind, derived, key, cols, path=None):
    """Differences between the derived rows and a TSV (hand columns ignored)."""
    old = _tsv(path or TSV[kind], key)
    out = []
    for r in derived:
        o = old.pop(r[key], None)
        if o is None:
            out.append('%s: missing row %s' % (kind, r[key]))
            continue
        for c in cols:
            if c not in HAND[kind] and o.get(c) != r[c]:
                out.append('%s %s: %s is %r, derived %r' % (kind, r[key], c, o.get(c), r[c]))
    out += ['%s: extra row %s' % (kind, k) for k in old]
    return out


MSG_COLS = ('id', 'channel', 'type', 'send_flags', 'payload', 'layout', 'senders', 'send_sites',
            'parser', 'parse_sites', 'evidence')


def check_messages(im, funcs, path=None):
    """Every derived sender call has a row with its site and type; every row's sites exist."""
    rows = _tsv(path or TSV['messages'], 'id')
    sites = {'%08x' % s: (caller, args) for s, caller, sender, args in sender_calls(im, funcs)}
    bodies = function_bodies(im, funcs, lambda e: NET_LO <= e < NET_HI or e in GATES)
    insn = {('%08x' % i.address): name for (ent, name), b in bodies.items() for i in b.ins}
    out, covered = [], set()
    for rid, r in rows.items():
        for s in filter(None, r.get('send_sites', '-').replace('-', '').split(';')):
            covered.add(s)
            if s not in sites:
                out.append('messages %s: send site %s is not a sender call' % (rid, s))
            elif sites[s][1].get('type') != r['type']:
                out.append('messages %s: site %s passes type %s, row says %s'
                           % (rid, s, sites[s][1].get('type'), r['type']))
        for s in filter(None, r.get('parse_sites', '-').replace('-', '').split(';')):
            if insn.get(s) != r['parser']:
                out.append('messages %s: parse site %s is not an instruction of %s' % (rid, s, r['parser']))
    out += ['messages: sender call %s (%s, type %s) has no row' % (s, c, a.get('type'))
            for s, (c, a) in sorted(sites.items()) if s not in covered]
    return out


def run_check(paths=None, im=None, funcs=None):
    paths = paths or {}
    im = im or Image()
    funcs = funcs or load_functions()
    out = compare('functions', derive_functions(im, funcs), 'addr', FN_COLS, paths.get('functions'))
    out += compare('state', derive_state(im, funcs), 'va', ST_COLS, paths.get('state'))
    out += check_messages(im, funcs, paths.get('messages'))
    return out


def main(argv):
    cmd = argv[1] if len(argv) > 1 else 'report'
    im, funcs = Image(), load_functions()
    if cmd == 'write':
        for kind, rows, key, cols in (('functions', derive_functions(im, funcs), 'addr', FN_COLS),
                                      ('state', derive_state(im, funcs), 'va', ST_COLS)):
            TSV[kind].write_text('\n'.join(_merge(kind, rows, key, cols)) + '\n', encoding='utf-8')
            print('wrote', TSV[kind])
        return 0
    if cmd == 'check':
        diffs = run_check()
        print('\n'.join(diffs) or 'net survey: all rows agree with the binary')
        return 1 if diffs else 0
    for k, v in facts(im, funcs).items():
        print(k, v)
    for c in interface_calls(im, funcs):
        print('iface %08x %-16s +0x%02x %-16s %s' % (c[0], c[1], c[2], c[3], ', '.join(c[4])))
    for s, caller, sender, args in sender_calls(im, funcs):
        print('send  %08x %-18s %-16s %s' % (s, caller, sender, args))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
