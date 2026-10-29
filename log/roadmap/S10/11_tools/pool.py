"""Retail sound.dll functions with no names row in any re/names/*.tsv (S10.11 pool). Usage: pool.py <idmap tsv> <out>"""
import sys, csv, glob, pathlib, collections
ROOT = pathlib.Path(__file__).resolve().parents[4]


def named():
    """{rva: (file, name)} for every sound.dll row in re/names (function files, not *_globals)."""
    out = {}
    for f in sorted(glob.glob(str(ROOT / 're' / 'names' / '*.tsv'))):
        if f.endswith('_globals.tsv') or 'withdrawn' in f or pathlib.Path(f).name.startswith('demo_sound_'):
            continue
        for r in csv.DictReader(open(f, encoding='utf-8'), delimiter='\t'):
            if r.get('module') != 'sound.dll':
                continue
            a = int(r['addr'], 16)
            out[a - 0x10000000 if a >= 0x10000000 else a] = (pathlib.Path(f).name, r['name'])
    return out


def main(idmap, out):
    nm = named()
    fn = {int(r['entry'], 16): r for r in csv.DictReader(open(ROOT / 're' / 'functions_ghidra_sound.dll.tsv'), delimiter='\t')}
    pool = [a for a in fn if a not in nm and int(fn[a]['size']) > 5]
    print('master functions', len(fn), 'named rows', len(nm), 'unnamed (> 5 bytes)', len(pool))
    ex = collections.Counter()
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['demo_rva', 'demo_name', 'retail_va', 'names_file', 'names_name'])
        for r in csv.DictReader(open(idmap), delimiter='\t'):
            if r['verdict'] not in ('EXACT', 'EXACT_CODE'):
                continue
            a = int(r['retail_va'], 16) - 0x10000000
            hit = nm.get(a)
            ex['named' if hit else 'unnamed'] += 1
            w.writerow([r['demo_rva'], r['name'], r['retail_va'], hit[0] if hit else '-', hit[1] if hit else '-'])
    print('exact matches by names-row presence', dict(ex))
    with open(out.replace('.tsv', '_pool.txt'), 'w') as f:
        for a in sorted(pool):
            f.write('%#x\t%s\t%s\n' % (0x10000000 + a, fn[a]['size'], fn[a]['name']))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
