"""S10.11: 11_fields.tsv -> markdown table for 11_merge.md, with our names for the retail functions and our class
prefix (S10.9 classmap) where one exists. Usage: fields_md.py <fields tsv> <out md part>"""
import sys, csv, pathlib
T = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(T))
import pool, classmap

ROOT = T.parents[3]


def main(src, out):
    nm = pool.named()
    for r in csv.DictReader(open(ROOT / 're' / 'names' / 'demo_sound_1.tsv', encoding='utf-8'), delimiter='\t'):
        nm[int(r['addr'], 16) - 0x10000000] = ('demo_sound_1.tsv', r['name'])
    cm = classmap.classmap('sound')
    if isinstance(cm, tuple):
        cm = cm[0]
    rows = list(csv.DictReader(open(src), delimiter='\t'))
    with open(out, 'w', encoding='utf-8') as f:
        f.write('| demo class | our prefix | offset | member | type (demo) | declared in | retail instruction | r/w | '
                'width | retail function (our name) |\n|---|---|---|---|---|---|---|---|---|---|\n')
        for r in rows:
            fn = int(r['retail_function'], 16)
            our = nm.get(fn - 0x10000000, ('-', 'unnamed'))[1]
            f.write('| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s %s |\n' % (
                r['class'], cm.get(r['class'], '-'), r['offset'], r['member'], r['type'].replace('|', '/'),
                r['declared_in'], r['first_access'], r['access'], r['size'], r['retail_function'], our))
    print('rows', len(rows))


if __name__ == '__main__':
    main(*sys.argv[1:3])
