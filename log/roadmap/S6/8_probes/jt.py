import plib as P, struct
for k in range(16):
    v = struct.unpack_from("<I", P.IMG.data, 0x472244 + 4*k - 0x400000)[0]
    print(k, hex(v))
