"""Minimal Microsoft cabinet (MSCF) reader: stored and MSZIP folders. Usage: mscf.py <file> <offset hex> <outdir>.
Reads untrusted bytes only; writes the member files under <outdir> (path components sanitised)."""
import sys, os, struct, zlib

def parse(d, base):
    assert d[base:base + 4] == b'MSCF'
    cb, coff_files = struct.unpack_from('<I4xI', d, base + 8)
    minor, major, nfold, nfile, flags = struct.unpack_from('<BBHHH', d, base + 24)
    p = base + 36
    rsv_fold = rsv_data = 0
    if flags & 4:
        rsv_hdr, rsv_fold, rsv_data = struct.unpack_from('<HBB', d, p)
        p += 4 + rsv_hdr
    if flags & 1:
        p = d.index(b'\0', p) + 1; p = d.index(b'\0', p) + 1
    if flags & 2:
        p = d.index(b'\0', p) + 1; p = d.index(b'\0', p) + 1
    folders = []
    for i in range(nfold):
        start, ndata, comp = struct.unpack_from('<IHH', d, p)
        folders.append((start, ndata, comp)); p += 8 + rsv_fold
    files = []
    p = base + coff_files
    for i in range(nfile):
        size, off, ifold, date, time, attr = struct.unpack_from('<IIHHHH', d, p)
        e = d.index(b'\0', p + 16)
        files.append((d[p + 16:e].decode('latin-1'), size, off, ifold)); p = e + 1
    return cb, folders, files, rsv_data

def folder_bytes(d, base, folder, rsv_data):
    start, ndata, comp = folder
    p = base + start
    out = bytearray(); zd = None
    for i in range(ndata):
        csum, cbytes, ubytes = struct.unpack_from('<IHH', d, p)
        blk = d[p + 8 + rsv_data:p + 8 + rsv_data + cbytes]
        p += 8 + rsv_data + cbytes
        if comp & 0xf == 0:
            out += blk
        elif comp & 0xf == 1:
            assert blk[:2] == b'CK', 'bad MSZIP block'
            z = zlib.decompressobj(-15, zdict=bytes(out[-32768:])) if out else zlib.decompressobj(-15)
            u = z.decompress(blk[2:]) + z.flush()
            assert len(u) == ubytes, (len(u), ubytes)
            out += u
        else:
            raise SystemExit('compression %#x not supported' % comp)
    return bytes(out)

def safe(name):
    parts = [x for x in name.replace('/', '\\').split('\\') if x not in ('', '.', '..')]
    return os.path.join(*[''.join(c for c in x if c not in ':*?"<>|') for x in parts])

def main(path, off, outdir):
    d = open(path, 'rb').read()
    base = int(off, 16)
    cb, folders, files, rsv = parse(d, base)
    print('cabinet bytes', cb, 'folders', folders, 'files', len(files))
    data = [folder_bytes(d, base, f, rsv) for f in folders]
    for name, size, foff, ifold in files:
        dst = os.path.join(outdir, safe(name))
        os.makedirs(os.path.dirname(dst) or outdir, exist_ok=True)
        open(dst, 'wb').write(data[ifold][foff:foff + size])
        print(size, name)

if __name__ == '__main__':
    main(*sys.argv[1:4])
