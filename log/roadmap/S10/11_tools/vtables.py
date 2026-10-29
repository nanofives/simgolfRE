"""Vtable-shaped runs in a PE's .rdata (S10.11): consecutive relocated dwords pointing into .text whose first address
some code operand holds (an installer). Incremental-link thunks (E9 jmp) are followed to the body.
runs(img) -> [(table_rva, [body_rva per slot], [installer instruction rva])] in address order."""
import struct, collections


def sec(img, name):
    for s in img.pe.sections:
        if s.Name.rstrip(b'\0').decode() == name:
            return s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)
    raise KeyError(name)


def dword(img, rva):
    b = img.pe.get_data(rva, 4)
    return struct.unpack('<I', b)[0] if len(b) == 4 else None


def follow(img, rva, lo, hi):
    for _ in range(4):
        b = img.pe.get_data(rva, 5)
        if len(b) == 5 and b[0] == 0xE9:
            t = (rva + 5 + struct.unpack('<i', b[1:])[0]) & 0xFFFFFFFF
            if lo <= t < hi:
                rva = t
                continue
        break
    return rva


def runs(img):
    tlo, thi = sec(img, '.text')
    rlo, rhi = sec(img, '.rdata')
    base = img.base
    code_refs = collections.defaultdict(list)       # value rva -> instruction-ish sites in .text holding it
    for r in img.rel:
        if tlo <= r < thi:
            v = dword(img, r)
            if v is not None and base <= v < base + img.size:
                code_refs[v - base].append(r)
    out, cur = [], None
    for r in img.rel:
        if not rlo <= r < rhi:
            continue
        v = dword(img, r)
        into_code = v is not None and base + tlo <= v < base + thi
        if into_code and cur and r == cur[0] + 4 * len(cur[1]) and r not in code_refs:
            cur[1].append(follow(img, v - base, tlo, thi))
            continue
        if cur:
            out.append(cur); cur = None
        if into_code and r in code_refs:
            cur = (r, [follow(img, v - base, tlo, thi)], code_refs[r])
    if cur:
        out.append(cur)
    return out
