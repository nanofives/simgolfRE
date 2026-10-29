"""Explore the NB11 block of a PE: subsection directory, symbol record kinds. Reads only."""
import sys, struct, collections

def main(path, cvoff):
    d = open(path, 'rb').read()
    base = int(cvoff, 16)
    sig, lfo = struct.unpack_from('<4sI', d, base)
    print(sig, hex(lfo))
    cbh, cbe, n, nxt, fl = struct.unpack_from('<HHIII', d, base + lfo)
    print('dir', cbh, cbe, n, nxt, fl)
    kinds = collections.Counter()
    ents = []
    for i in range(n):
        sst, imod, so, cb = struct.unpack_from('<HHII', d, base + lfo + cbh + i * cbe)
        ents.append((sst, imod, so, cb)); kinds[sst] += 1
    print({hex(k): v for k, v in kinds.items()})
    for e in ents:
        if e[0] not in (0x120, 0x125, 0x127):
            print('sst', hex(e[0]), e[1], hex(e[2]), e[3])
    rec = collections.Counter()
    for sst, imod, so, cb in ents:
        if sst not in (0x125, 0x129, 0x12a, 0x134):
            continue
        p = base + so + (4 if sst == 0x125 else 16)
        end = base + so + cb
        while p + 4 <= end:
            ln, kind = struct.unpack_from('<HH', d, p)
            if ln < 2:
                break
            rec[(sst, kind)] += 1
            p += 2 + ln
    for k, v in sorted(rec.items()):
        print(hex(k[0]), hex(k[1]), v)

main(sys.argv[1], sys.argv[2])
