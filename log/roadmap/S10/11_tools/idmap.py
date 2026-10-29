"""Demo sound.dll (debug, NB11) -> retail original/sound.dll by code identity (S10.11).
Each demo proc's bytes [rva, rva + CV length) and each retail function (Ghidra ranges + SMSG PDB extents) are
disassembled and normalised: operands carrying a base relocation -> <addr>, call rel32 -> <fn>, a jmp leaving the
body -> <fn>, branches relative to the start. Equal key sequences = exact identity. Usage: idmap.py <demo dll> <out tsv>"""
import sys, csv, hashlib, pathlib, struct, collections
import capstone, pefile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[4] / 're' / 'tools'))
import cv_debug

ROOT = pathlib.Path(__file__).resolve().parents[4]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True


class Image:
    def __init__(self, path):
        self.pe = pefile.PE(str(path), fast_load=True)
        self.pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_BASERELOC']])
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.size = self.pe.OPTIONAL_HEADER.SizeOfImage
        self.rel = sorted(e.rva for b in getattr(self.pe, 'DIRECTORY_ENTRY_BASERELOC', []) for e in b.entries if e.type)
        self.relset = set(self.rel)

    def code(self, rva, n):
        return self.pe.get_data(rva, n)

    def norm(self, rva, n):
        code = self.code(rva, n)
        if len(code) < n or n <= 0:
            return None
        keys, end, start = [], rva, rva
        for ins in md.disasm(code, rva):
            end = ins.address + ins.size
            relo = any(a in self.relset for a in range(ins.address, end))
            b0 = ins.bytes[0]
            if b0 == 0xE8:
                keys.append('call <fn>'); continue
            if b0 in (0xE9, 0xEB, 0xE0, 0xE1, 0xE2, 0xE3) or 0x70 <= b0 < 0x80 or (b0 == 0x0F and 0x80 <= ins.bytes[1] < 0x90):
                tgt = ins.operands[0].imm if ins.operands else 0
                if b0 == 0xE9 and not start <= tgt < start + n:
                    keys.append('jmp <fn>'); continue
                keys.append('%s %+d' % (ins.mnemonic, tgt - start)); continue
            ops = []
            for op in ins.operands:
                if op.type == capstone.x86.X86_OP_IMM:
                    v = op.imm & 0xFFFFFFFF
                    ops.append('<addr>' if relo and self.base <= v < self.base + self.size else hex(v))
                elif op.type == capstone.x86.X86_OP_MEM:
                    m = op.mem; disp = m.disp & 0xFFFFFFFF
                    dd = '<addr>' if relo and self.base <= disp < self.base + self.size else hex(disp)
                    ops.append('%d[%s+%s*%d+%s]' % (op.size, ins.reg_name(m.base) if m.base else '',
                                                   ins.reg_name(m.index) if m.index else '', m.scale, dd))
                else:
                    ops.append(ins.reg_name(op.reg))
            keys.append(ins.mnemonic + ' ' + ','.join(ops))
        if end != rva + n:
            return None                      # undecodable bytes inside the extent
        return hashlib.sha1('\n'.join(keys).encode()).hexdigest(), len(keys)


def retail_extents():
    """{rva: set of sizes}: Ghidra master ranges (first range of each function) and SMSG PDB procedure lengths."""
    ext = collections.defaultdict(set)
    for r in csv.DictReader(open(ROOT / 're' / 'functions_ghidra_sound.dll.tsv'), delimiter='\t'):
        a = int(r['entry'], 16)
        lo, hi = r['ranges'].split(';')[0].split('-')
        ext[a].add(int(hi, 16) - int(lo, 16))
        ext[a].add(int(r['size']))
    for line in open(ROOT / 'log' / 'roadmap' / 'S10' / 'pdb' / 'sound.procs.tsv'):
        f = line.rstrip('\n').split('\t')
        ext[int(f[0], 16)].add(int(f[1]))
    return ext


def tail_cuts(img, p):
    """Cut points c (4-aligned from the end) such that every dword in [c, rva + size) of the demo proc is relocated."""
    end, cuts, c = p['rva'] + p['size'], set(), p['rva'] + p['size'] - 4
    while c >= p['rva'] and c in img.relset:
        cuts.add(c - p['rva']); c -= 4
    if cuts:      # an index byte table can sit between the code and the dword table: allow cuts up to 256 bytes lower
        lo = min(cuts)
        cuts |= set(range(max(1, lo - 256), lo))
    return cuts if end else set()


def size_class(n):
    for lim, name in ((16, '<=16'), (64, '17-64'), (256, '65-256'), (1024, '257-1024')):
        if n <= lim:
            return name
    return '>1024'


def main(demo_path, out):
    demo = Image(demo_path)
    retail = Image(ROOT / 'original' / 'sound.dll')
    cv = cv_debug.CvImage(open(demo_path, 'rb').read())
    index = collections.defaultdict(list)
    ext = retail_extents()
    for a, sizes in ext.items():
        for n in sorted(sizes):
            h = retail.norm(a, n) if n > 0 else None
            if h:
                index[(n, h[0])].append(a)
    rows, tally = [], collections.Counter()
    for p in sorted(cv.procs, key=lambda r: r['rva'] or 0):
        h = demo.norm(p['rva'], p['size']) if p['rva'] and p['size'] else None
        hits = sorted(set(index.get((p['size'], h[0]), []))) if h else []
        verdict = 'EXACT' if len(hits) == 1 else 'AMBIGUOUS' if hits else 'NONE' if h else 'UNDECODED'
        if not hits:
            # an inline switch table after the code: every dword from the cut to the CV length carries a relocation
            for cut in sorted({n for a, s in ext.items() for n in s if 0 < n < p['size']} & tail_cuts(demo, p), reverse=True):
                h2 = demo.norm(p['rva'], cut)
                hits = sorted(set(index.get((cut, h2[0]), []))) if h2 else []
                if len(hits) == 1:
                    verdict = 'EXACT_CODE'          # identical code; the trailing jump table differs only by relocation
                    break
        tally[(size_class(p['size']), verdict)] += 1
        rows.append((p['rva'], p['size'], h[1] if h else 0, verdict, ';'.join('%#x' % (retail.base + x) for x in hits),
                     p['module'], p['name']))
    with open(out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['demo_rva', 'size', 'ins', 'verdict', 'retail_va', 'module', 'name'])
        for r in rows:
            w.writerow(['%#x' % r[0]] + list(r[1:]))
    for cls in ('<=16', '17-64', '65-256', '257-1024', '>1024'):
        print(cls, {v: tally[(cls, v)] for v in ('EXACT', 'EXACT_CODE', 'AMBIGUOUS', 'NONE', 'UNDECODED')})
    print('total', collections.Counter(r[3] for r in rows))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
