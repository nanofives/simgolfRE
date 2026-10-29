"""Facts for the unnamed retail bodies of paired slots (S10.11): retail listing, the demo member at every
[this + d] access, and for `call/jmp [reg + s]` after loading [this + d] the method the demo layout of d's pointee
type declares at vtable offset s. Usage: candfacts.py <demo dll> <slotfeat tsv> <out txt>"""
import sys, csv, pathlib, re
import capstone
T = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(T)); sys.path.insert(0, str(T.parents[3] / 're' / 'tools'))
import idmap, cv_debug, layouts, fieldprop

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def main(demo_path, sft, out):
    ret = idmap.Image(idmap.ROOT / 'original' / 'sound.dll')
    cv = cv_debug.CvImage(open(demo_path, 'rb').read())
    lay = layouts.Layouts(cv)
    rsize = {int(r['entry'], 16) + 0x10000000: int(r['size'])
             for r in csv.DictReader(open(idmap.ROOT / 're' / 'functions_ghidra_sound.dll.tsv'), delimiter='\t')}
    seen = set()
    with open(out, 'w') as f:
        for r in csv.DictReader(open(sft), delimiter='\t'):
            rb = int(r['retail_body'], 16)
            if r['names_file'] != '-' or r['agree'] == 'EMPTY_BOTH' or rb in seen or rb not in rsize:
                continue
            seen.add(rb)
            cls = r['demo_name'].rsplit('::', 1)[0]
            sub = re.match(r'\?\?_7.+@@6B(.+)@@@$', r['demo_symbol'])
            lcls = sub.group(1) if sub else cls
            f.write('== %#x %d bytes  slot %s of %s (%s, retail table %s installer %s)  demo %s %s  agree %s\n' % (
                rb, rsize[rb], r['slot'], r['demo_symbol'], r['demo_table'], r['retail_table'], r['installer'],
                r['demo_body'], r['demo_name'], r['agree']))
            for d, at, rw, sz in fieldprop.this_accesses(ret, rb, rsize[rb]):
                m = lay.deep4(lcls, d)
                f.write('   %#x %s this+%#x -> %s\n' % (at, rw, d, '%s %s (in %s)' % (m[1], m[2], m[3]) if m else '-'))
                if m and m[2].endswith('*'):
                    pt = m[2].rstrip('*')
                    vs = lay.vslots(pt)
                    f.write('      %s vtable: %s\n' % (pt, ' '.join('+%#x=%s' % kv for kv in sorted(vs.items()))[:900]))
            for i in md.disasm(ret.code(rb - ret.base, rsize[rb]), rb):
                f.write('      %x %s %s\n' % (i.address, i.mnemonic, i.op_str))
    print('wrote', out, len(seen))


if __name__ == '__main__':
    main(*sys.argv[1:4])
