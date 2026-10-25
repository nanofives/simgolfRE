"""S6.8: run the ORIGINAL generateRandomCourse 0x00470a60 in Unicorn over the exact images test_defects_gen_w2.py gives the port
(port/test/test_terrain_gen.py _fixed_image + every flag word 0x8080 except the 4 x 4 clubhouse block the plain image chose)
and compare with the port at placementBound 1 and 0.
Run with SG_PORTSIM_DLL set: ~/bin/py -3.12 log/roadmap/S6/8_probes/d52b3_port_images.py [budget]"""
import pathlib
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(HERE))
import plib as P                                                    # noqa: E402
from port.test import lib as L                                      # noqa: E402
from port.test.test_terrain_gen import _fixed_image, _put, _i32, port_call, objs  # noqa: E402

lib = L.load()
PLACE = "generateRandomCourse.placementBound"
o = objs(lib)
hole19 = o["g_holes"][0] + 19 * 0x208
KW = dict(setting=0, ctype=0, difficulty=1, relief=1)
budget = int(sys.argv[1]) if len(sys.argv) > 1 else 300_000_000
LOOPS = {4: (0x47172e, 0x47176f)}
LOOPS.update({c: (0x4718a4, 0x4718e7) for c in (5, 6, 8, 12, 13, 15)})


def image(loc, blocked):
    img = _fixed_image(lib, n=3, loc=loc, **KW)
    if blocked:
        plain = _fixed_image(lib, n=3, loc=4, **KW)
        _, out = port_call(lib, plain, 3)
        cx, cy = _i32(out, hole19 + 8), _i32(out, hole19 + 0xC)
        flags = bytearray(struct.pack("<H", 0x8080) * 2500)
        for i in range(4):
            for j in range(4):
                struct.pack_into("<H", flags, 2 * ((cx + i) * 50 + cy + j), 0)
        _put(lib, img, "g_tileFlags", bytes(flags))
    return bytes(img)


def port(img, bound):
    lib.defect_reset()
    if bound == 0:
        lib.defect(PLACE, False)
    r = port_call(lib, img, 3)
    lib.defect_reset()
    return r


def counter(addr, st):
    def install(uc):
        from unicorn import UC_HOOK_CODE

        def on(u, a, s, _):
            st["n"] += 1
        uc.hook_add(UC_HOOK_CODE, on, None, addr, addr)
    return install


assert len(image(4, False)) == P.DN
LOCS = [int(v) for v in sys.argv[2].split(",")] if len(sys.argv) > 2 else [3, 4, 5, 6, 8, 12, 13, 15]
MODES = [m == "blocked" for m in sys.argv[3].split(",")] if len(sys.argv) > 3 else [False, True]
for loc in LOCS:
    for blocked in MODES:
        img = image(loc, blocked)
        st = {"n": 0}
        entry = LOOPS.get(loc, (0, 0))[0]
        r = P.emu(0x470a60, [3], img, max_insns=budget, hooks=[counter(entry, st)] if entry else None)
        p1, p0 = port(img, 1), port(img, 0)
        line = f"loc {loc:2} {'blocked' if blocked else 'plain  '}: original {r.status} eip={r.eip:#x} loop iterations {st['n']:,}"
        if r.status == "completed":
            final = bytearray(img)
            for va, (_, nb) in r.writes.items():
                final[va - P.D0] = nb
            eq1 = p1[1] is not None and p1[0] == r.eax and bytes(p1[1]) == bytes(final)
            eq0 = p0[1] is not None and p0[0] == r.eax and bytes(p0[1]) == bytes(final)
            line += f" eax={r.eax:#x} | port@1 equal: {eq1} | port@0 equal: {eq0}"
        else:
            line += f" | port@1 {p1[0] if p1[1] is None else 'completed'} | port@0 {'completed' if p0[1] is not None else p0[0]}"
        print(line, flush=True)
