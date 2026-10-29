"""InstallShield 6 cabinet reader (ISc( version 0x0100600c): list files from <hdr>, extract one by name.
Usage: iscab.py <dir with data1.hdr> list | get <name substring> <outdir>. Layout per unshield's header.c (v6 file
descriptors of 0x57 bytes, 64-bit sizes); compressed files are chunks of u16 length + raw deflate."""
import sys, os, struct, zlib

def files(hdr):
    d = open(hdr, 'rb').read()
    sig, ver, volinfo, cdo, cds = struct.unpack_from('<4sIIII', d, 0)
    assert sig == b'ISc(', sig
    c = cdo
    fto, = struct.unpack_from('<I', d, c + 0xc)
    nd, = struct.unpack_from('<I', d, c + 0x1c)
    nf, fto2 = struct.unpack_from('<II', d, c + 0x28)
    ft = c + fto
    dirs = []
    for i in range(nd):
        o, = struct.unpack_from('<I', d, ft + 4 * i)
        dirs.append(d[ft + o:d.index(b'\0', ft + o)].decode('latin-1'))
    out = []
    for i in range(nf):
        p = ft + fto2 + i * 0x57
        flags, xs, cs, off = struct.unpack_from('<HQQQ', d, p)
        md5 = d[p + 26:p + 42]
        nameo, diri = struct.unpack_from('<IH', d, p + 58)
        vol, = struct.unpack_from('<H', d, p + 85)
        name = d[ft + nameo:d.index(b'\0', ft + nameo)].decode('latin-1')
        out.append(dict(i=i, flags=flags, xs=xs, cs=cs, off=off, md5=md5.hex(), dir=dirs[diri] if diri < nd else '?',
                        name=name, vol=vol))
    return out

def extract(dirpath, f):
    cab = os.path.join(dirpath, 'data%d.cab' % f['vol'])
    d = open(cab, 'rb').read()
    raw = d[f['off']:f['off'] + f['cs']]
    if f['flags'] & 1:
        raise SystemExit('split file not supported: %r' % f)
    if not f['flags'] & 4:
        return raw
    out = bytearray(); p = 0
    while p < len(raw):
        n, = struct.unpack_from('<H', raw, p); p += 2
        z = zlib.decompressobj(-15)
        out += z.decompress(raw[p:p + n]) + z.flush(); p += n
    assert len(out) == f['xs'], (len(out), f['xs'])
    return bytes(out)

def main(argv):
    dirpath = argv[0]
    fs = files(os.path.join(dirpath, 'data1.hdr'))
    if argv[1] == 'list':
        for f in fs:
            print(f['i'], f['vol'], hex(f['flags']), f['xs'], f['cs'], f['dir'], f['name'], f['md5'])
        return
    for f in fs:
        if argv[2].lower() in f['name'].lower():
            data = extract(dirpath, f)
            dst = os.path.join(argv[3], '%d_%s' % (f['i'], os.path.basename(f['name'])))
            os.makedirs(argv[3], exist_ok=True)
            open(dst, 'wb').write(data)
            print('wrote', dst, len(data), f['dir'])

if __name__ == '__main__':
    main(sys.argv[1:])
