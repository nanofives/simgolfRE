"""Probe the demo installer's bytes: PE sections, overlay, embedded signatures. Reads only."""
import sys, re, struct

def main(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    sec = pe + 24 + opt
    end = 0
    for i in range(nsec):
        name, vsz, va, rsz, raw = struct.unpack_from('<8sIIII', d, sec + i * 40)
        print('sec', name.rstrip(b'\0'), hex(va), hex(vsz), hex(raw), hex(rsz))
        end = max(end, raw + rsz)
    print('overlay at', hex(end), 'size', len(d) - end)
    print('overlay head', d[end:end + 64])
    for m in re.finditer(rb'[\x20-\x7e]{6,}', d[:0x40000]):
        s = m.group()
        if any(k in s.lower() for k in (b'setup', b'cab', b'install', b'.dll', b'copyright', b'version')):
            print(hex(m.start()), s[:100])
    for pat in (b'MSCF', b'ISc(', b'\x13\x5d\x65\x8c', b'MZ'):
        hits = [m.start() for m in re.finditer(re.escape(pat), d)]
        print(pat, len(hits), [hex(h) for h in hits[:12]])

main(sys.argv[1])
