"""What is left at C2, and why (2026-10-11): every C2 row by module, reachability (recorded caller / stored-pointer
installer / neither) and the drop reason earlier batch notes give next to its address (log/c3/round24_classify.py
categories), plus held rows (re/c3_hold.tsv) and size buckets. Writes log/c3/c2_survey.tsv (one row per function).

    py -3.12 log/c3/c2_survey.py
"""
import collections, csv, re, sys
sys.path.insert(0, 're/tools')
exec(open('log/c3/round26_candidates.py').read().split("X = 'golf_clean.exe'")[0])   # C2 (holds excluded), SIZE, cat
import pointer_callers as pc

for r in csv.DictReader(open('re/functions_ghidra_Terrain.dll.tsv'), delimiter='\t'):
    e = int(r['entry'], 16); e = e - 0x10000000 if e >= 0x10000000 else e
    SIZE[('Terrain.dll', e)] = int(r['size'])
HOLD = {(l.split('\t')[0], l.split('\t')[1].lower()): l.split('\t')[2].strip() for l in open('re/c3_hold.tsv')
        if l.strip() and not l.startswith('module')}
ALL_C2 = [r for r in csv.DictReader(open('hooks.csv')) if r['confidence'] == 'C2']


def reach(r):
    if (r['callers'] or 'none') != 'none':
        return 'caller'
    m = pc.mod(mod(r))
    return 'installer' if any(i for *_, i in pc.references(m, pc.norm(m, r['addr']))) else 'none'


def bucket(n):
    return 'tiny <=32' if n <= 32 else 'small <=128' if n <= 128 else 'medium <=512' if n <= 512 else 'large >512'


rows = []
for r in ALL_C2:
    key = (mod(r), r['addr'].lower())
    size = SIZE.get((mod(r), int(r['addr'], 16)), 0)
    held = HOLD.get(key)
    rows.append(dict(module=mod(r), addr=r['addr'], name=r['name'], subsystem=r['subsystem'], size=size,
                     reach=reach(r), category='held' if held else cat(r), held=held or '',
                     note=(note(r)[0] if note(r) else '')[:200]))
with open('log/c3/c2_survey.tsv', 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter='\t')
    w.writeheader(); w.writerows(rows)

print(f'{len(rows)} C2 rows')
by_mod = collections.Counter(r['module'] for r in rows)
for m in sorted(by_mod):
    mr = [r for r in rows if r['module'] == m]
    print(f'\n== {m}: {len(mr)}')
    print('  reach:    ', dict(collections.Counter(r['reach'] for r in mr)))
    reachable = [r for r in mr if r['reach'] != 'none']
    print('  reachable by reason:', dict(collections.Counter(r['category'] for r in reachable).most_common()))
    print('  reachable by size:  ', dict(collections.Counter(bucket(r['size']) for r in reachable).most_common()))
    print('  reachable by subsystem:', dict(collections.Counter(r['subsystem'] for r in reachable).most_common(8)))
