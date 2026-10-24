"""D11: spawnWalker 0x00402970 with a full walker table (64 live walkers, flags byte +0x12 nonzero)."""
import plib as P

WALK, STRIDE = 0x585850, 0x4c
for scen, k in (("sandbox_basic", "00"), ("course_season", "00")):
    base = P.start_image(scen, k)
    # control: first free slot found
    im = P.Img(base)
    for i in range(0, 5):
        im.w(WALK + i * STRIDE + 0x12, 1, 1)
    P.show(P.emu(0x402970, [3], im.done()), f"{scen} 5 live (control)")

    im = P.Img(base)
    for i in range(64):
        im.w(WALK + i * STRIDE + 0x12, 1, 1)
    r = P.emu(0x402970, [3], im.done())
    P.show(r, f"{scen} 64 live, byte after table as in the image")
    print("   returned slot (eax) =", hex(r.eax), " flags byte at slot 64 (0x586b62) before:", hex(im.r(WALK + 64 * STRIDE + 0x12, 1)))
    print("   bytes written outside g_walkers:", len([a for a in r.writes if not (WALK <= a < WALK + 0x1300)]))
    lo = [a for a in r.writes if not (WALK <= a < WALK + 0x1300)]
    if lo:
        print("   range:", hex(min(lo)), hex(max(lo)))

    # worst case for 'ends': every following byte at stride 0x4c nonzero up to the end of the image
    im = P.Img(base)
    for i in range(64):
        im.w(WALK + i * STRIDE + 0x12, 1, 1)
    a = WALK + 0x12 + 64 * STRIDE
    n = 0
    while a < P.D0 + P.DN:
        im.w(a, 1, 1)
        a += STRIDE
        n += 1
    r = P.emu(0x402970, [3], im.done())
    P.show(r, f"{scen} 64 live and every stride-0x4c byte to the end of .data nonzero ({n} slots)")
