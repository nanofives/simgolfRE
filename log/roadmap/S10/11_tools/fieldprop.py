"""Field proposals for sound.dll classes (S10.11). Source: paired slot bodies whose demo / retail features are EQUAL
(slotfeat.py). In the retail body, `this` is ecx at entry and any register a `mov r, ecx` copies it to; a memory
operand [this + d] (d != 0) is a field access when the demo layout of the method's class has a member starting at d.
Usage: fieldprop.py <demo dll> <slotfeat tsv> <out tsv>"""
import sys, csv, pathlib, collections, re
import capstone
T = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(T)); sys.path.insert(0, str(T.parents[3] / 're' / 'tools'))
import idmap, cv_debug, layouts

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
X = capstone.x86
SAVED = {'esi', 'edi', 'ebx', 'ebp'}


def this_accesses(img, va, n):
    """[(disp, instruction va, 'r' | 'w', size)] of [this + disp] operands."""
    this, out = {'ecx'}, []
    for ins in md.disasm(img.code(va - img.base, n), va):
        ops = ins.operands
        for i, op in enumerate(ops):
            if op.type == X.X86_OP_MEM and op.mem.base and not op.mem.index \
                    and ins.reg_name(op.mem.base) in this and 0 < op.mem.disp < 0x10000 and ins.mnemonic != 'lea':
                w = i == 0 and ins.mnemonic not in ('cmp', 'test', 'push', 'call', 'jmp') and len(ops) > 1
                out.append((op.mem.disp, ins.address, 'w' if w else 'r', op.size))
        if ins.mnemonic == 'call':
            this &= SAVED
        if ops and ops[0].type == X.X86_OP_REG:
            dst = ins.reg_name(ops[0].reg)
            if ins.mnemonic == 'mov' and len(ops) == 2 and ops[1].type == X.X86_OP_REG and ins.reg_name(ops[1].reg) in this:
                this.add(dst)
            elif ins.mnemonic not in ('cmp', 'test', 'push'):
                this.discard(dst)
        if ins.mnemonic in ('ret', 'jmp'):
            break
    return out


def main(demo_path, sft, out):
    ret = idmap.Image(idmap.ROOT / 'original' / 'sound.dll')
    cv = cv_debug.CvImage(open(demo_path, 'rb').read())
    lay = layouts.Layouts(cv)
    rsize = {int(r['entry'], 16) + 0x10000000: int(r['size'])
             for r in csv.DictReader(open(idmap.ROOT / 're' / 'functions_ghidra_sound.dll.tsv'), delimiter='\t')}
    props = collections.defaultdict(list)
    miss = collections.Counter()
    done = set()
    for r in csv.DictReader(open(sft), delimiter='\t'):
        if r['agree'] != 'EQUAL' or '::' not in r['demo_name']:
            continue
        rb = int(r['retail_body'], 16)
        cls = r['demo_name'].rsplit('::', 1)[0]
        sub = re.match(r'\?\?_7.+@@6B(.+)@@@$', r['demo_symbol'])
        if sub:                      # a secondary vtable: `this` is the subobject of the class named after 6B
            cls = sub.group(1)
        if (rb, cls) in done or rb not in rsize or cls not in lay.by:
            continue
        done.add((rb, cls))
        for d, at, rw, sz in this_accesses(ret, rb, rsize[rb]):
            m = lay.deep4(cls, d)
            if m is None:
                miss[cls] += 1
                continue
            props[(cls, d)].append((at, rw, sz, rb, r['demo_name'], m))
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['class', 'offset', 'member', 'type', 'declared_in', 'first_access', 'access', 'size', 'retail_function',
                    'demo_method', 'accesses'])
        for (cls, d), v in sorted(props.items()):
            at, rw, sz, rb, dn, m = min(v)
            w.writerow([cls, '%#x' % d, m[1], m[2], m[3], '%#x' % at, rw, sz, '%#x' % rb, dn, len(v)])
    print('fields', len(props), 'classes', len({c for c, _ in props}), 'unmatched accesses', sum(miss.values()))


if __name__ == '__main__':
    main(*sys.argv[1:4])
