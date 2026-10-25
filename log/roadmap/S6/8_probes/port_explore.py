"""S6.8 exploration: find fixed images where the port's loop A (location 4) / loop B (5, 6, 8, 12, 13, 15) is the only search
that cannot finish. Run with SG_PORTSIM_DLL set: ~/bin/py -3.12 log/roadmap/S6/8_probes/port_explore.py"""
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT))
from port.test import lib as L                                       # noqa: E402
from port.test.test_terrain_gen import _fixed_image, _put, _i32, port_call, objs  # noqa: E402

lib = L.load()
PLACE = "generateRandomCourse.placementBound"
o = objs(lib)
hole19 = o["g_holes"][0] + 19 * 0x208


def run(img, n=3, bound=1):
    lib.defect_reset()
    if bound == 0:
        lib.defect(PLACE, False)
    return port_call(lib, img, n)


def blocked_image(loc, kw, keep):
    """flags 0x8080 everywhere except the cells in `keep` (set of (x, y))"""
    img = _fixed_image(lib, n=3, loc=loc, **kw)
    flags = bytearray(struct.pack("<H", 0x8080) * 2500)
    for x, y in keep:
        struct.pack_into("<H", flags, 2 * (x * 50 + y), 0)
    _put(lib, img, "g_tileFlags", bytes(flags))
    return img


for loc in (4, 5, 6, 8, 12, 13, 15):
    for kw in (dict(setting=0, ctype=0, difficulty=1, relief=1), dict(setting=1, ctype=3, difficulty=3, relief=0)):
        img = _fixed_image(lib, n=3, loc=loc, **kw)
        eax, out = run(img)
        if out is None:
            print(loc, kw, "plain image does not complete", eax)
            continue
        cx, cy = _i32(out, hole19 + 8), _i32(out, hole19 + 0xC)
        res = {}
        for side in (2, 3, 4, 5, 6):
            keep = {(cx + i, cy + j) for i in range(side) for j in range(side)}
            b = blocked_image(loc, kw, keep)
            e1, o1 = run(b, bound=1)
            e0, o0 = run(b, bound=0)
            # same image, location 3 (no eval-gated feature search): does it complete at 1?
            c = blocked_image(3, kw, keep)
            e3, o3 = run(c, bound=1)
            res[side] = (e1 if o1 is None else "ok", e0 if o0 is None else "ok", e3 if o3 is None else "ok")
        print(loc, kw, (cx, cy), res)
