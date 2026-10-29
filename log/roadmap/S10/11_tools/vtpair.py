"""Pair demo and retail sound.dll pointer tables (S10.11). Both sides' tables come from vtables.runs (relocated
code-pointer runs whose start an instruction holds). Pairing: longest common subsequence of the slot-count
sequences in .rdata order, then leftover tables whose slot count is unique on both sides. A demo table is named by
the CV public / data symbol at its address. Usage: vtpair.py <demo dll> <out tsv>
Output: one row per slot of a paired table with the demo body's CV procedure and the retail body."""
import sys, csv, pathlib, collections
T = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(T)); sys.path.insert(0, str(T.parents[3] / 're' / 'tools'))
import idmap, vtables, pool, cv_debug


def lcs(a, b):
    n, m = len(a), len(b)
    L = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n - 1, -1, -1):
        for j in range(m - 1, -1, -1):
            L[i][j] = L[i + 1][j + 1] + 1 if a[i] == b[j] else max(L[i + 1][j], L[i][j + 1])
    i = j = 0; out = []
    while i < n and j < m:
        if a[i] == b[j]:
            out.append((i, j)); i += 1; j += 1
        elif L[i + 1][j] >= L[i][j + 1]:
            i += 1
        else:
            j += 1
    return out


def pair(dr, rr):
    pairs = lcs([len(x[1]) for x in dr], [len(x[1]) for x in rr])
    used_d = {i for i, _ in pairs}; used_r = {j for _, j in pairs}
    cd = collections.Counter(len(dr[i][1]) for i in range(len(dr)) if i not in used_d)
    cr = collections.Counter(len(rr[j][1]) for j in range(len(rr)) if j not in used_r)
    for i in range(len(dr)):
        n = len(dr[i][1])
        if i not in used_d and cd[n] == 1 and cr[n] == 1:
            j = next(j for j in range(len(rr)) if j not in used_r and len(rr[j][1]) == n)
            pairs.append((i, j)); used_r.add(j)
    return sorted(pairs)


def proc_at(cv):
    by = {}
    for p in cv.procs:
        if p['rva']:
            by[p['rva']] = p['name']
    return by


def main(demo_path, out):
    demo = idmap.Image(demo_path); ret = idmap.Image(idmap.ROOT / 'original' / 'sound.dll')
    cv = cv_debug.CvImage(open(demo_path, 'rb').read())
    sym = {x['rva']: x['name'] for x in cv.publics + cv.datas if x['rva']}
    names = proc_at(cv)
    nm = pool.named()
    dr, rr = vtables.runs(demo), vtables.runs(ret)
    prs = pair(dr, rr)
    print('demo tables', len(dr), 'retail tables', len(rr), 'paired', len(prs))
    agree = collections.Counter()
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['retail_table', 'installer', 'demo_table', 'demo_symbol', 'slot', 'retail_body', 'demo_body',
                    'demo_name', 'names_file', 'names_name'])
        for i, j in prs:
            d, r = dr[i], rr[j]
            for k, (db, rb) in enumerate(zip(d[1], r[1])):
                hit = nm.get(rb)
                dn = names.get(db, '?')
                w.writerow(['%#x' % (ret.base + r[0]), '%#x' % (ret.base + r[2][0]), '%#x' % (demo.base + d[0]),
                            sym.get(d[0], '-'), k, '%#x' % (ret.base + rb), '%#x' % (demo.base + db), dn,
                            hit[0] if hit else '-', hit[1] if hit else '-'])
                if hit:
                    a = dn.split('::')[-1].lower() == hit[1].split('::')[-1].lower()
                    agree['method part equal' if a else 'differs'] += 1
                else:
                    agree['no names row'] += 1
    for i in range(len(dr)):
        if i not in {p[0] for p in prs}:
            print('unpaired demo %#x %d %s' % (demo.base + dr[i][0], len(dr[i][1]), sym.get(dr[i][0], '-')))
    for j in range(len(rr)):
        if j not in {p[1] for p in prs}:
            print('unpaired retail %#x %d' % (ret.base + rr[j][0], len(rr[j][1])))
    print(dict(agree))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
