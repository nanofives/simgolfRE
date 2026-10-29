"""usage: counts_table.py <pdb> <source-path-substring>  -> markdown table of the latest run per source"""
import csv, sys
pdb, sub = sys.argv[1], sys.argv[2]
last = {}
for r in csv.DictReader(open('log/roadmap/S10/runs/summary.tsv'), delimiter='\t'):
    if r['pdb'] == pdb and sub in r['source']:
        last[r['source']] = r
k = ['procs', 'mapped', 'new_100', 'already_100', 'folded', 'miss', 'nosym', 'error']
print('| source | procs | mapped | new 100 | already | folded | miss | nosym | error |')
print('|---|---|---|---|---|---|---|---|---|')
t = dict.fromkeys(k, 0)
for s, r in last.items():
    print('| ' + s.split('/')[-1] + ' | ' + ' | '.join(r[x] for x in k) + ' |')
    for x in k: t[x] += int(r[x])
print('| **' + str(len(last)) + ' files** | ' + ' | '.join('**%d**' % t[x] for x in k) + ' |')
