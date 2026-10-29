"""cv_debug.py: our own reader of CodeView debug information embedded in a PE image (S10.11, 2026-10-29).

Formats (identified from the bytes of the IMAGE_DEBUG_DIRECTORY entry of type 2, IMAGE_DEBUG_TYPE_CODEVIEW):
  NB09 / NB11  the whole CodeView block is in the file (VC 4-6 linker /debugtype:cv /pdb:none). A signature, then
               lfoDirectory; the subsection directory (OMFDirHeader 16 bytes + OMFDirEntry 12 bytes each) lists
               sstModule 0x120, sstAlignSym 0x125, sstGlobalSym 0x129, sstGlobalPub 0x12a, sstGlobalTypes 0x12b,
               sstSegMap 0x12d, sstStaticSym 0x134 ... NB11 symbol and type records carry 32-bit type indices
               (S_GPROC32 0x100b, LF_CLASS 0x1004 ...); NB09 records carry 16-bit ones (S_GPROC32 0x0205, LF_CLASS 0x0004).
  NB10 / RSDS  a reference to an external .pdb: only the path is reported (use re/tools/smsg_pdb.py for MSF 7 PDBs).
Nothing is executed; the file is read as bytes. Usage:
  cv_debug.py info <pe>            format, subsections, record counts
  cv_debug.py procs <pe>           rva, size, kind, module, name (TSV)
  cv_debug.py publics <pe>         rva, name (TSV)
  cv_debug.py classes <pe> [name]  class / struct layouts: size, bases, vtable slots, members with offsets
  cv_debug.py json <pe> <out>      everything above as one JSON file
"""
import json, struct, sys

SST = {0x120: 'sstModule', 0x121: 'sstTypes', 0x122: 'sstPublic', 0x123: 'sstPublicSym', 0x124: 'sstSymbols',
       0x125: 'sstAlignSym', 0x126: 'sstSrcLnSeg', 0x127: 'sstSrcModule', 0x128: 'sstLibraries',
       0x129: 'sstGlobalSym', 0x12a: 'sstGlobalPub', 0x12b: 'sstGlobalTypes', 0x12c: 'sstMPC', 0x12d: 'sstSegMap',
       0x12e: 'sstSegName', 0x12f: 'sstPreComp', 0x133: 'sstFileIndex', 0x134: 'sstStaticSym'}

# symbol kinds: (NB11 32-bit type index form, NB09 16-bit form)
S_LPROC32 = (0x100a, 0x0204)
S_GPROC32 = (0x100b, 0x0205)
S_PUB32 = (0x1009, 0x0203)
S_LDATA32 = (0x1007, 0x0201)
S_GDATA32 = (0x1008, 0x0202)
S_UDT = (0x1003, 0x0004)
S_THUNK32 = (0x0206, 0x0206)
S_OBJNAME = 0x0009


def numeric(b, p):
    """CodeView numeric leaf at p: (value, next position)."""
    v, = struct.unpack_from('<H', b, p)
    if v < 0x8000:
        return v, p + 2
    fmt = {0x8000: '<b', 0x8001: '<h', 0x8002: '<H', 0x8003: '<i', 0x8004: '<I', 0x8009: '<q', 0x800a: '<Q'}.get(v)
    if fmt is None:
        raise ValueError('numeric leaf %#x at %#x' % (v, p))
    x, = struct.unpack_from(fmt, b, p + 2)
    return x, p + 2 + struct.calcsize(fmt)


def pstr(b, p):
    """Length-prefixed (Pascal) name as VC 4-6 write it: (text, next position)."""
    n = b[p]
    return b[p + 1:p + 1 + n].decode('latin-1'), p + 1 + n


def pe_sections(d):
    pe, = struct.unpack_from('<I', d, 0x3c)
    nsec, = struct.unpack_from('<H', d, pe + 6)
    opt, = struct.unpack_from('<H', d, pe + 20)
    secs = []
    for i in range(nsec):
        name, vsz, va, rsz, raw = struct.unpack_from('<8sIIII', d, pe + 24 + opt + i * 40)
        secs.append({'name': name.rstrip(b'\0').decode('latin-1'), 'va': va, 'vsize': vsz, 'raw': raw, 'rsize': rsz})
    return pe, opt, secs


def find_codeview(d):
    """(signature, file offset, size) of the IMAGE_DEBUG_TYPE_CODEVIEW entry, or None."""
    pe, opt, secs = pe_sections(d)
    magic, = struct.unpack_from('<H', d, pe + 24)
    ddir = pe + 24 + (96 if magic == 0x10b else 112) + 6 * 8
    rva, size = struct.unpack_from('<II', d, ddir)
    if not rva:
        return None
    off = next(s['raw'] + rva - s['va'] for s in secs if s['va'] <= rva < s['va'] + max(s['vsize'], s['rsize']))
    for i in range(size // 28):
        _c, _t, _ma, _mi, typ, sz, _a, ptr = struct.unpack_from('<IIHHIIII', d, off + 28 * i)
        if typ == 2:
            return d[ptr:ptr + 4].decode('latin-1'), ptr, sz
    return None


class CvImage:
    """The CodeView block of one PE file (NB09 / NB11), parsed into procs, publics, data, UDTs and types."""

    def __init__(self, data):
        self.d = data
        self.pe, _o, self.secs = pe_sections(data)
        cv = find_codeview(data)
        if cv is None:
            raise ValueError('no CodeView debug directory entry')
        self.sig, self.base, self.size = cv
        if self.sig in ('NB10', 'RSDS'):
            self.pdb = data[self.base + (16 if self.sig == 'NB10' else 24):self.base + self.size].split(b'\0')[0]
            raise ValueError('%s: external PDB %r' % (self.sig, self.pdb))
        if self.sig not in ('NB09', 'NB11'):
            raise ValueError('unknown CodeView signature %r' % self.sig)
        self.wide = self.sig == 'NB11'          # 32-bit type indices in symbol and type records
        self.ti = 4 if self.wide else 2
        lfo, = struct.unpack_from('<I', data, self.base + 4)
        cbh, cbe, n = struct.unpack_from('<HHI', data, self.base + lfo)
        self.dir = []
        for i in range(n):
            sst, imod, so, cb = struct.unpack_from('<HHII', data, self.base + lfo + cbh + i * cbe)
            self.dir.append((sst, imod, self.base + so, cb))
        self.modules, self.segmap = {}, []
        self.procs, self.publics, self.datas, self.udts, self.types = [], [], [], [], {}
        self.thunks = []
        self._read_modules()
        self._read_segmap()
        for sst, imod, so, cb in self.dir:
            if sst == 0x125:
                self._read_symbols(so + 4, so + cb, imod)
            elif sst in (0x129, 0x12a, 0x134):
                cbsym, = struct.unpack_from('<I', data, so + 4)
                self._read_symbols(so + 16, so + 16 + cbsym, None)
            elif sst == 0x12b:
                self._read_types(so, cb)

    def _read_modules(self):
        for sst, imod, so, cb in self.dir:
            if sst != 0x120:
                continue
            _ovl, _lib, nseg = struct.unpack_from('<HHH', self.d, so)
            name, _p = pstr(self.d, so + 8 + 12 * nseg)
            self.modules[imod] = name

    def _read_segmap(self):
        for sst, imod, so, cb in self.dir:
            if sst != 0x12d:
                continue
            n, _nl = struct.unpack_from('<HH', self.d, so)
            for i in range(n):
                fl, ovl, grp, frame, _a, _b, off, cbs = struct.unpack_from('<HHHHHHII', self.d, so + 4 + 20 * i)
                self.segmap.append((frame, off))

    def rva(self, seg, off):
        """Logical segment seg (1-based) + offset -> RVA, through sstSegMap and the PE section table."""
        frame, soff = self.segmap[seg - 1] if 0 < seg <= len(self.segmap) else (seg, 0)
        if not 0 < frame <= len(self.secs):
            return None
        return self.secs[frame - 1]['va'] + soff + off

    def _read_symbols(self, p, end, imod):
        d, w = self.d, self.wide
        while p + 4 <= end:
            ln, kind = struct.unpack_from('<HH', d, p)
            if ln < 2:
                break
            q = p + 4
            if kind in (S_LPROC32[0], S_GPROC32[0]) and w or kind in (S_LPROC32[1], S_GPROC32[1]) and not w:
                _par, _end, _nxt, size, dbs, dbe = struct.unpack_from('<6I', d, q)
                if w:
                    ti, off, seg, flags = struct.unpack_from('<IIHB', d, q + 24); r = q + 35
                else:
                    off, seg, ti, flags = struct.unpack_from('<IHHB', d, q + 24); r = q + 33
                name, _r = pstr(d, r)
                self.procs.append({'rva': self.rva(seg, off), 'size': size, 'debug_start': dbs, 'debug_end': dbe,
                                   'type': ti, 'kind': 'L' if kind in S_LPROC32 else 'G', 'flags': flags,
                                   'module': self.modules.get(imod, ''), 'name': name})
            elif kind in (S_PUB32[0], S_LDATA32[0], S_GDATA32[0], S_UDT[0]) and w or \
                    kind in (S_PUB32[1], S_LDATA32[1], S_GDATA32[1], S_UDT[1]) and not w:
                if kind in S_UDT:
                    ti, = struct.unpack_from('<I' if w else '<H', d, q)
                    name, _r = pstr(d, q + self.ti)
                    self.udts.append({'type': ti, 'name': name})
                else:
                    if w:
                        ti, off, seg = struct.unpack_from('<IIH', d, q); r = q + 10
                    else:
                        off, seg, ti = struct.unpack_from('<IHH', d, q); r = q + 8
                    name, _r = pstr(d, r)
                    row = {'rva': self.rva(seg, off), 'type': ti, 'name': name, 'module': self.modules.get(imod, '')}
                    (self.publics if kind in S_PUB32 else self.datas).append(row)
            elif kind == S_THUNK32[0]:
                _par, _end, _nxt, off, seg, tlen, _ord = struct.unpack_from('<IIIIHHB', d, q)
                name, _r = pstr(d, q + 19)
                self.thunks.append({'rva': self.rva(seg, off), 'size': tlen, 'name': name})
            p += 2 + ln

    # leaf kinds: (NB11 form, NB09 form)
    LF = {'pointer': (0x1002, 0x0002), 'array': (0x1003, 0x0003), 'class': (0x1004, 0x0004),
          'struct': (0x1005, 0x0005), 'union': (0x1006, 0x0006), 'enum': (0x1007, 0x0007),
          'procedure': (0x1008, 0x0008), 'mfunction': (0x1009, 0x0009), 'arglist': (0x1201, 0x0201),
          'fieldlist': (0x1203, 0x0204), 'methodlist': (0x1206, 0x0207), 'bitfield': (0x1205, 0x0206),
          'modifier': (0x1001, 0x0001)}
    LFM = {'bclass': (0x1400, 0x0400), 'vbclass': (0x1401, 0x0401), 'ivbclass': (0x1402, 0x0402),
           'enumerate': (0x0403, 0x0403), 'index': (0x1404, 0x0405), 'member': (0x1405, 0x0406),
           'stmember': (0x1406, 0x0407), 'method': (0x1407, 0x0408), 'nesttype': (0x1408, 0x0409),
           'vfunctab': (0x1409, 0x040a), 'onemethod': (0x140b, 0x040c)}

    def _leaf(self, kind, table):
        col = 0 if self.wide else 1
        for k, v in table.items():
            if v[col] == kind:
                return k
        return None

    def _read_types(self, so, cb):
        n, = struct.unpack_from('<I', self.d, so + 4)
        offs = struct.unpack_from('<%dI' % n, self.d, so + 8)
        start = so + 8 + 4 * n
        for i, o in enumerate(offs):
            ln, = struct.unpack_from('<H', self.d, start + o)
            self.types[0x1000 + i] = self.d[start + o + 2:start + o + 2 + ln]

    def _ti(self, b, p):
        return struct.unpack_from('<I' if self.wide else '<H', b, p)[0], p + self.ti

    def leaf(self, ti):
        b = self.types.get(ti)
        return (None, b) if b is None else (self._leaf(struct.unpack_from('<H', b, 0)[0], self.LF), b)

    def aggregate(self, ti):
        """LF_CLASS / LF_STRUCTURE / LF_UNION -> dict(name, size, count, property, field, vshape)."""
        kind, b = self.leaf(ti)
        if kind not in ('class', 'struct', 'union'):
            return None
        if kind == 'union' and self.wide:
            count, prop, field = struct.unpack_from('<HHI', b, 2); p = 10; vshape = 0
        elif kind == 'union':
            count, field, prop = struct.unpack_from('<HHH', b, 2); p = 8; vshape = 0
        elif self.wide:
            count, prop, field, derived, vshape = struct.unpack_from('<HHIII', b, 2); p = 18
        else:
            count, field, prop, derived, vshape = struct.unpack_from('<HHHHH', b, 2); p = 12
        size, p = numeric(b, p)
        name, _p = pstr(b, p)
        return {'kind': kind, 'name': name, 'size': size, 'count': count, 'property': prop, 'field': field,
                'forward': bool(prop & 0x80), 'vshape': vshape}

    def fields(self, ti):
        """Members of an LF_FIELDLIST (following LF_INDEX continuations) as dicts with 'leaf'."""
        out, seen = [], set()
        while ti and ti not in seen:
            seen.add(ti)
            kind, b = self.leaf(ti)
            if kind != 'fieldlist':
                break
            p, ti = 2, 0
            while p + 2 <= len(b):
                if b[p] >= 0xf0:                      # LF_PAD bytes
                    p += b[p] & 0x0f or 1
                    continue
                lk, = struct.unpack_from('<H', b, p)
                k = self._leaf(lk, self.LFM)
                p += 2
                if k is None:
                    raise ValueError('field leaf %#x' % lk)
                f = {'leaf': k}
                if k == 'index':
                    p += 2 if self.wide else 0
                    ti, p = self._ti(b, p)
                    continue
                if k in ('vfunctab', 'nesttype'):
                    p += 2 if self.wide else 0
                    f['type'], p = self._ti(b, p)
                    if k == 'nesttype':
                        f['name'], p = pstr(b, p)
                elif k == 'enumerate':
                    f['attr'], = struct.unpack_from('<H', b, p)
                    f['value'], p = numeric(b, p + 2)
                    f['name'], p = pstr(b, p)
                elif k == 'method':
                    f['count'], = struct.unpack_from('<H', b, p)
                    f['mlist'], p = self._ti(b, p + 2)
                    f['name'], p = pstr(b, p)
                else:
                    if self.wide:
                        f['attr'], = struct.unpack_from('<H', b, p); f['type'], p = self._ti(b, p + 2)
                    else:
                        f['type'], f['attr'] = struct.unpack_from('<HH', b, p); p += 4
                    if k in ('vbclass', 'ivbclass'):
                        f['vbptr_type'], p = self._ti(b, p)
                        f['vbpoff'], p = numeric(b, p)
                        f['vbind'], p = numeric(b, p)
                    elif k in ('bclass', 'member'):
                        f['offset'], p = numeric(b, p)
                    if k == 'onemethod' and (f['attr'] >> 2) & 7 in (4, 6):
                        f['vbaseoff'], = struct.unpack_from('<I', b, p); p += 4
                    if k in ('member', 'stmember', 'onemethod'):
                        f['name'], p = pstr(b, p)
                out.append(f)
        return out

    def methodlist(self, ti):
        kind, b = self.leaf(ti)
        out, p = [], 2
        while kind == 'methodlist' and p + 4 <= len(b):
            if self.wide:
                attr, _pad, t = struct.unpack_from('<HHI', b, p); p += 8
            else:
                attr, t = struct.unpack_from('<HH', b, p); p += 4
            e = {'attr': attr, 'type': t}
            if (attr >> 2) & 7 in (4, 6):
                e['vbaseoff'], = struct.unpack_from('<I', b, p); p += 4
            out.append(e)
        return out

    def layout(self, ti):
        """One class layout: size, bases, data members (offset, name, type text), methods (with vtable offsets of
        introducing virtuals: mprop 4 = intro virtual, 6 = pure intro)."""
        a = self.aggregate(ti)
        if a is None:
            return None
        a = dict(a, ti=ti, bases=[], members=[], statics=[], methods=[], vfptr=False)
        for f in self.fields(a['field']) if a['field'] else []:
            k = f['leaf']
            if k == 'bclass':
                a['bases'].append({'name': self.type_name(f['type']), 'offset': f['offset']})
            elif k in ('vbclass', 'ivbclass'):
                a['bases'].append({'name': self.type_name(f['type']), 'virtual': True})
            elif k == 'member':
                a['members'].append({'offset': f['offset'], 'name': f['name'], 'type': self.type_name(f['type'])})
            elif k == 'stmember':
                a['statics'].append({'name': f['name'], 'type': self.type_name(f['type'])})
            elif k == 'vfunctab':
                a['vfptr'] = True
            elif k == 'onemethod':
                a['methods'].append(self._method(f['name'], f))
            elif k == 'method':
                for e in self.methodlist(f['mlist']):
                    a['methods'].append(self._method(f['name'], e))
        return a

    def _method(self, name, e):
        mprop = (e['attr'] >> 2) & 7
        m = {'name': name, 'mprop': ['vanilla', 'virtual', 'static', 'friend', 'intro', 'pure', 'pureintro'][mprop]
             if mprop < 7 else str(mprop), 'type': e['type']}
        if 'vbaseoff' in e:
            m['vtable_offset'] = e['vbaseoff']
        return m

    PRIM = {0x03: 'void', 0x10: 'char', 0x20: 'uchar', 0x11: 'short', 0x21: 'ushort', 0x12: 'long', 0x22: 'ulong',
            0x13: 'int64', 0x23: 'uint64', 0x30: 'bool', 0x40: 'float', 0x41: 'double', 0x42: 'long double',
            0x68: 'int8', 0x69: 'uint8', 0x70: 'rchar', 0x71: 'wchar', 0x72: 'int16', 0x73: 'uint16',
            0x74: 'int', 0x75: 'uint'}

    def type_name(self, ti, depth=0):
        """Readable text for a type index (primitives, pointers, arrays, aggregates, enums)."""
        if ti < 0x1000:
            base = self.PRIM.get(ti & 0xff, 'prim%#x' % (ti & 0xff))
            return base + ('*' if ti & 0x700 else '')
        if depth > 8:
            return '?'
        kind, b = self.leaf(ti)
        if kind in ('class', 'struct', 'union'):
            return self.aggregate(ti)['name']
        if kind == 'enum':
            p = 2 + 2 + 2 + 2 * self.ti
            return 'enum ' + pstr(b, p)[0]
        if kind == 'pointer':
            if self.wide:
                t, _attr = struct.unpack_from('<II', b, 2)
            else:
                _attr, t = struct.unpack_from('<HH', b, 2)
            return self.type_name(t, depth + 1) + '*'
        if kind == 'modifier':
            t = struct.unpack_from('<I' if self.wide else '<H', b, 2 if self.wide else 4)[0]
            return self.type_name(t, depth + 1)
        if kind == 'array':
            el, = struct.unpack_from('<I' if self.wide else '<H', b, 2)
            size, _p = numeric(b, 2 + 2 * self.ti)
            return '%s[%#x bytes]' % (self.type_name(el, depth + 1), size)
        if kind == 'bitfield':
            return 'bitfield'
        if kind in ('procedure', 'mfunction'):
            return 'function'
        return 'type%#x' % ti

    def classes(self):
        """Every defined (not forward) aggregate, by type index order."""
        out = []
        for ti in sorted(self.types):
            a = self.aggregate(ti)
            if a and not a['forward']:
                out.append(self.layout(ti))
        return out


def info(cv):
    import collections
    c = collections.Counter(SST.get(s, hex(s)) for s, _m, _o, _c in cv.dir)
    print('format', cv.sig, 'at file offset %#x, %d bytes' % (cv.base, cv.size))
    print('subsections', dict(c))
    print('modules', len(cv.modules), 'procs', len(cv.procs), 'publics', len(cv.publics), 'data', len(cv.datas),
          'udts', len(cv.udts), 'thunks', len(cv.thunks), 'types', len(cv.types))


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    cmd, path = argv[0], argv[1]
    cv = CvImage(open(path, 'rb').read())
    if cmd == 'info':
        info(cv)
    elif cmd == 'procs':
        print('rva\tsize\tkind\tmodule\tname')
        for p in sorted(cv.procs, key=lambda r: r['rva'] or 0):
            print('%#x\t%d\t%s\t%s\t%s' % (p['rva'] or 0, p['size'], p['kind'], p['module'], p['name']))
    elif cmd == 'publics':
        print('rva\tname')
        for p in sorted(cv.publics, key=lambda r: r['rva'] or 0):
            print('%#x\t%s' % (p['rva'] or 0, p['name']))
    elif cmd == 'classes':
        for a in cv.classes():
            if len(argv) > 2 and a['name'] != argv[2]:
                continue
            print('%s %s size %#x%s' % (a['kind'], a['name'], a['size'], ' vfptr' if a['vfptr'] else ''))
            for b in a['bases']:
                print('  base %s %s' % (b['name'], '+%#x' % b['offset'] if 'offset' in b else 'virtual'))
            for m in a['members']:
                print('  +%#x %s %s' % (m['offset'], m['name'], m['type']))
            for m in a['methods']:
                if 'vtable_offset' in m:
                    print('  vtbl +%#x %s (%s)' % (m['vtable_offset'], m['name'], m['mprop']))
    elif cmd == 'json':
        out = {'format': cv.sig, 'modules': cv.modules, 'procs': cv.procs, 'publics': cv.publics, 'data': cv.datas,
               'udts': cv.udts, 'thunks': cv.thunks, 'classes': cv.classes()}
        with open(argv[2], 'w') as f:
            json.dump(out, f, indent=0)
        print('wrote', argv[2])
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
