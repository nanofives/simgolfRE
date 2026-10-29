"""Unnamed retail bodies reached through a paired table slot (S10.11): body kind by S10.9's shape.kind, size, every
(table, slot, demo name) that reaches it. Usage: slotpool.py <vtpair tsv> <out tsv>"""
import sys, csv, pathlib, collections
import capstone
T = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(T)); sys.path.insert(0, str(T.parents[3] / 're' / 'tools'))
import idmap, shape

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def body(img, va, n):
    rva = va - img.base
    out = []
    for i in md.disasm(img.code(rva, n), va):
        out.append('%x %s %s' % (i.address, i.mnemonic, i.op_str))
    return out


def main(vtp, out):
    ret = idmap.Image(idmap.ROOT / 'original' / 'sound.dll')
    size = {int(r['entry'], 16) + 0x10000000: int(r['size'])
            for r in csv.DictReader(open(idmap.ROOT / 're' / 'functions_ghidra_sound.dll.tsv'), delimiter='\t')}
    reach = collections.defaultdict(list)
    for r in csv.DictReader(open(vtp), delimiter='\t'):
        if r['names_file'] == '-':
            reach[int(r['retail_body'], 16)].append((r['retail_table'], r['slot'], r['demo_name']))
    kinds = collections.Counter()
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['retail_va', 'size', 'kind', 'detail', 'demo_names', 'slots'])
        for va in sorted(reach):
            n = size.get(va)
            k, det = shape.kind(body(ret, va, n)) if n else ('NOT_A_FUNCTION', '')
            kinds[k] += 1
            dn = sorted({x[2] for x in reach[va]})
            w.writerow(['%#x' % va, n or 0, k, det, ';'.join(dn), ';'.join('%s/%s' % x[:2] for x in reach[va])])
    print('bodies', len(reach), dict(kinds))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
