"""padfit.py <src> <out> <off1> <off2> ...: replace PAD0..PADn so that the k-th _RPTF0 line sits at base+off_k
(base = the function's opening brace line). PADk precedes the RPTs it shifts; RPTs are matched in order."""
import sys, re
src, out = sys.argv[1], sys.argv[2]
offs = [int(o, 16) for o in sys.argv[3:]]
t = open(src).read().split('\n')
pads = sorted([(i, l.strip()) for i, l in enumerate(t) if re.fullmatch(r'PAD\d+', l.strip())])
counts = {p: 0 for _, p in pads}
def render():
    r = []
    for l in t:
        if re.fullmatch(r'PAD\d+', l.strip()):
            r += [''] * counts[l.strip()]
        else:
            r.append(l)
    return r
for _ in range(50):
    r = render()
    base = [i for i, l in enumerate(r) if l == '{'][-1]
    rpt = [i - base for i, l in enumerate(r) if '_RPTF0' in l]
    assert len(rpt) == len(offs), (rpt, offs)
    # fix the first mismatch using the last PAD before that RPT
    bad = [k for k in range(len(offs)) if rpt[k] != offs[k]]
    if not bad:
        break
    k = bad[0]
    rline = [i for i, l in enumerate(r) if '_RPTF0' in l][k]
    # pads in source order located before this RPT in rendered output: find by mapping
    # count PAD markers preceding the k-th RPT in t
    tk = [i for i, l in enumerate(t) if '_RPTF0' in l][k]
    cand = [p for i, p in pads if i < tk]
    p = cand[-1]
    counts[p] += offs[k] - rpt[k]
    assert counts[p] >= 0, ('need fewer lines', p, counts[p])
open(out, 'w').write('\n'.join(render()))
print(counts)
