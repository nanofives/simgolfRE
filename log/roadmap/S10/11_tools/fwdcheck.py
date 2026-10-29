"""For each unnamed forwarder candidate (S10.11): the classes whose demo vtable record declares the same method name
at the slot the retail body calls, and the paired retail body in that slot. Usage: fwdcheck.py <demo dll> <vtpair tsv>"""
import sys, csv, pathlib, re
import capstone
T = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(T)); sys.path.insert(0, str(T.parents[3] / 're' / 'tools'))
import idmap, cv_debug, layouts

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
CANDS = [0x1002e060, 0x1002a4b0, 0x1002a4e0, 0x1002c1c0, 0x1002c1f0, 0x1002b870, 0x1002b8a0, 0x10029420, 0x1001c340,
         0x10031a30]


def main(demo_path, vtp):
    ret = idmap.Image(idmap.ROOT / 'original' / 'sound.dll')
    cv = cv_debug.CvImage(open(demo_path, 'rb').read())
    lay = layouts.Layouts(cv)
    rows = list(csv.DictReader(open(vtp), delimiter='\t'))
    by_body = {}
    slot_of = {}
    for r in rows:
        by_body.setdefault(int(r['retail_body'], 16), r)
        cls = re.match(r'\?\?_7(.+?)@@6B', r['demo_symbol'])
        if cls:
            slot_of[(cls.group(1), int(r['slot']) * 4)] = r
    for c in CANDS:
        r = by_body[c]
        meth = r['demo_name'].rsplit('::', 1)[1]
        calls = [(i.address, i.op_str) for i in md.disasm(ret.code(c - ret.base, 200), c)
                 if i.mnemonic in ('call', 'jmp') and '[' in i.op_str]
        print('== %#x %s  calls %s' % (c, r['demo_name'], calls[:3]))
        for at, op in calls[:2]:
            m = re.search(r'\+ (0x[0-9a-f]+)\]', op)
            s = int(m.group(1), 16) if m else 0
            for cls, a in sorted(lay.by.items()):
                if lay.vslots(cls).get(s) == meth:
                    t = slot_of.get((cls, s))
                    print('   slot %#x = %s::%s  retail body %s (%s %s, agree-table %s)' % (
                        s, cls, meth, t['retail_body'] if t else '-', t['names_file'] if t else '-',
                        t['names_name'] if t else '-', t['retail_table'] if t else '-'))


if __name__ == '__main__':
    main(*sys.argv[1:3])
