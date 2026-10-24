"""D8 / D40: placeHabitat 0x00405e30 / addHabitatSlot 0x00405970.
Arms: (a) slots 0..126 used, 127 free: the cluster loop's test reads the byte after the table (0x005736c2 = ball record 0 +0x12);
      (b) the same with that byte 0xff (hang arm);  (c) a free slot below the first one (completes);  (d) the table full."""
import plib as P

HAB, HS = 0x572cb0, 0x14


def build(scen, used, ball_byte=None, flags5a00=0x20):
    im = P.Img(P.start_image(scen, "00"))
    for s in range(128):
        im.w(HAB + s * HS + 0x12, 0 if s in used else 0xFF, 1)
    im.w(0x5a5a00, im.r(0x5a5a00, 4) | flags5a00, 4)
    im.w(0x5a34e0, 0, 1)                  # course type 0 -> allowed mask bit 1
    if ball_byte is not None:
        im.w(0x5736c2, ball_byte, 1)
    return im


def run(label, im, budget=3_000_000, rng=None):
    log = []
    # script: rand(3)=1, rand(3)=1 (x,y offset 0), rand(8)=0, rand(0x80)=127 or lowest free as the arm asks, rand(9)=1, then rand(2)=c
    vals = rng or [1, 1, 0, 127, 1, 0] + [1] * 40
    r = P.emu(0x405e30, [10, 10, 0x04], im.done(), max_insns=budget, hooks=[P.script_random(vals, log)])
    P.show(r, label, 6)
    print("   rng calls:", len(log), log[:12])
    return r


for scen in ("sandbox_basic", "course_season"):
    print("=====", scen)
    used = set(range(127))
    run("(a) 0..126 used, 127 free, byte after table 0x00", build(scen, used, ball_byte=0x00))
    run("(b) 0..126 used, 127 free, byte after table 0xff", build(scen, used, ball_byte=0xFF))
    # (b2) hang check by budget: larger budget gives the same state
    r1 = run("(b2) as (b), 10x the budget", build(scen, used, ball_byte=0xFF), budget=30_000_000)
    # (c) slot 5 is free below 127 and the picked slot is 5: first claim = 5, test reads slot 6's flag (used) -> ends
    used_c = set(range(128)) - {5, 6}
    run("(c) 5 and 6 free, picked 5", build(scen, used_c), rng=[1, 1, 0, 5, 1, 0] + [1] * 40)
