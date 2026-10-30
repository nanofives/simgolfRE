"""S3.16: rows of framegate TSVs (glob) whose view or bottom_panel region has beyond-one-level px, against I14's census row."""
import csv, glob, sys
old = {(r['scenario'], r['checkpoint']): r for r in csv.DictReader(open('log/roadmap/merges/I14/census.tsv'), delimiter='\t')}
def reg(r):
    d = {}
    for p in (r.get('regions') or '').split(';'):
        if '=' in p:
            k, v = p.split('='); d[k] = v
    return d
thr = int(sys.argv[2]) if len(sys.argv) > 2 else 1
for f in sorted(glob.glob(sys.argv[1])):
    for r in csv.DictReader(open(f), delimiter='\t'):
        o = old.get((r['scenario'], r['checkpoint']), {})
        rv, ov = reg(r), reg(o)
        if not rv:
            print(r['scenario'], r['checkpoint'], r['status'], r['detail'][:80]); continue
        v = rv.get('view', '0/0/0/0'); b = rv.get('bottom_panel', '0/0/0/0')
        if int(v.split('/')[3]) >= thr or int(b.split('/')[3]) >= thr or (o and o.get('status') != r['status']):
            print(r['scenario'], r['checkpoint'], o.get('status'), '->', r['status'], 'view', ov.get('view'), '->', v, 'bp', b, r['first_unsupported_drawing'][:40])
