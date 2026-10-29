"""Feature agreement of paired slot bodies (S10.11). Features of a body: the displacements of memory operands whose
base register is not esp / ebp (object fields and vtable slots), and the imports it calls. A demo (debug /Od) body
and the retail (/O2) body of the same method should touch the same fields and imports.
Usage: slotfeat.py <demo dll> <vtpair tsv> <out tsv>"""
import sys, csv, pathlib, collections
import capstone
T = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(T)); sys.path.insert(0, str(T.parents[3] / 're' / 'tools'))
import idmap, cv_debug

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
X86 = capstone.x86


def imports(img):
    import pefile
    img.pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
    out = {}
    for e in getattr(img.pe, 'DIRECTORY_ENTRY_IMPORT', []):
        for i in e.imports:
            out[i.address] = (i.name or b'#%d' % i.ordinal).decode()
    return out


def feats(img, imp, va, n):
    disps, calls, first = set(), [], None
    for ins in md.disasm(img.code(va - img.base, n), va):
        for op in ins.operands:
            if op.type != X86.X86_OP_MEM:
                continue
            m = op.mem; d = m.disp & 0xFFFFFFFF
            if not m.base and not m.index and d in imp:
                calls.append(imp[d])
            elif m.base and ins.reg_name(m.base) not in ('esp', 'ebp') and not m.index and d < 0x10000:
                disps.add(d)
                if first is None:
                    first = (d, ins.address)
    return disps, sorted(set(calls)), first


def main(demo_path, vtp, out):
    demo = idmap.Image(demo_path); ret = idmap.Image(idmap.ROOT / 'original' / 'sound.dll')
    cv = cv_debug.CvImage(open(demo_path, 'rb').read())
    dsize = {p['rva'] + demo.base: p['size'] for p in cv.procs if p['rva']}
    rsize = {int(r['entry'], 16) + 0x10000000: int(r['size'])
             for r in csv.DictReader(open(idmap.ROOT / 're' / 'functions_ghidra_sound.dll.tsv'), delimiter='\t')}
    di, ri = imports(demo), imports(ret)
    tally = collections.Counter()
    rows = list(csv.DictReader(open(vtp), delimiter='\t'))
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(list(rows[0].keys()) + ['agree', 'demo_fields', 'retail_fields', 'demo_imports', 'retail_imports'])
        for r in rows:
            db, rb = int(r['demo_body'], 16), int(r['retail_body'], 16)
            if db not in dsize or rb not in rsize:
                ag = 'NO_EXTENT'; a = b = (set(), [], None)
            else:
                a, b = feats(demo, di, db, dsize[db]), feats(ret, ri, rb, rsize[rb])
                if not a[0] and not b[0] and not a[1] and not b[1]:
                    ag = 'EMPTY_BOTH'
                elif a[0] == b[0] and a[1] == b[1]:
                    ag = 'EQUAL'
                elif b[0] <= a[0] and set(b[1]) <= set(a[1]) and (b[0] or b[1]):
                    ag = 'RETAIL_SUBSET'
                elif a[0] & b[0] or set(a[1]) & set(b[1]):
                    ag = 'OVERLAP'
                else:
                    ag = 'DISJOINT'
            tally[ag] += 1
            w.writerow(list(r.values()) + [ag, ' '.join('%#x' % x for x in sorted(a[0])),
                                           ' '.join('%#x' % x for x in sorted(b[0])), ' '.join(a[1]), ' '.join(b[1])])
    print(dict(tally))


if __name__ == '__main__':
    main(*sys.argv[1:4])
