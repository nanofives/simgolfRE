"""S10.11 rename proposals: retail functions instruction-identical to a demo procedure whose names row differs from the
demo's symbol beyond C decoration. Port column P when the old name occurs in port/src, port/test, shim/src or re/frida.
Usage: renames.py <exact_names tsv> <out tsv>"""
import sys, csv, pathlib, subprocess
ROOT = pathlib.Path(__file__).resolve().parents[4]
KEEP = {'$$$00001': 'a local label of chkstk.obj, not a name; keep _alloca_probe',
        '_stricmp': '_strcmpi and _stricmp are two publics of one CRT body (stricmp.obj); keep'}


def occurs(name):
    for d in ('port/src', 'port/test', 'shim/src', 're/frida'):
        r = subprocess.run(['git', 'grep', '-l', '-F', '-w', name, '--', d], cwd=ROOT, capture_output=True, text=True)
        if r.stdout.strip():
            return True
    return False


def main(src, out):
    rows = list(csv.DictReader(open(src), delimiter='\t'))
    n = 0
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['file', 'addr', 'old', 'new', 'evidence', 'port'])
        for r in rows:
            old, new = r['names_name'], r['demo_name']
            if old.lstrip('_') == new.lstrip('_') or new in KEEP:
                continue
            ev = ('instruction-identical (relocations masked) to the demo sound.dll procedure %s at demo RVA %s, module '
                  '.\\Debug\\*.obj of the LPC-10 codec (log/roadmap/S10/11_idmap.tsv, re/tools/cv_debug.py); the demo symbol '
                  'is the C name of the routine' % (new, r['demo_rva']))
            if new == 'voicin_':
                ev = ev.replace('instruction-identical', 'code instruction-identical up to the switch table after its last instruction (EXACT_CODE)')
            w.writerow([r['names_file'], r['retail_va'], old, new, ev, 'P' if occurs(old) else '-'])
            n += 1
    print('renames', n)


if __name__ == '__main__':
    main(*sys.argv[1:3])
