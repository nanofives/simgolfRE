"""S3.28: compare the cached census scenarios (28_out/census_split.tsv.d/*.json) with the wave 8 census, row by row (interim)."""
import csv, json, pathlib
H = pathlib.Path(__file__).resolve().parent
ref = {(r['scenario'], r['checkpoint']): r for r in csv.DictReader(open(H / 'w8_out/census_split.tsv'), delimiter='\t')}
n = moved = 0
for p in sorted((H / '28_out/census_split.tsv.d').glob('*.json')):
    for r in json.loads(p.read_text())['rows']:
        k = (r['scenario'], r['checkpoint']); a = ref[k]; n += 1
        f = lambda x: (x['status'], int(x.get('total_px') or 0), int(x.get('beyond_px') or 0), x.get('split_status'), int(x.get('twod_px') or 0), int(x.get('terrain_origin_px') or 0))
        if f(a) != f(r):
            moved += 1
            print(k, f(a), '->', f(r))
print(n, 'rows,', moved, 'moved')
