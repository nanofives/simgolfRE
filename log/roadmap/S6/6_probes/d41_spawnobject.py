"""D41: spawnObject 0x004011e0 cell search (0x00401264..0x0040129d) with every cell of the 4 x 4 corner of the bank's 6-wide map occupied."""
import plib as P

BANK, BS = 0x4e6d70, 0x74          # occupancy +0x50 of g_recordBanks (0x4e6d20)
ANIM = 0x56d1b8                      # animal records, stride 0x3c, free flag word at +0x20 (0x56d1d8) == -1


def build(scen, lot, occupied_cells):
    im = P.Img(P.start_image(scen, "00"))
    for si in range(4):
        for cx in range(4):
            im.w(BANK + lot * BS + si * 6 + cx, 0 if (si, cx) in occupied_cells else 0xFF, 1)
    # an animal slot must be free (word at 0x56d1d8 == -1)
    im.w(0x56d1d8, 0xFFFF, 2)
    return im


full = {(a, b) for a in range(4) for b in range(4)}
for scen in ("sandbox_basic", "course_season"):
    print("=====", scen)
    for lot in (0, 3):
        log = []
        r = P.emu(0x4011e0, [lot, 1, 0, 0, 0], build(scen, lot, full).done(), max_insns=2_000_000,
                  hooks=[P.script_random(lambda n, k: k % 4, log)])
        P.show(r, f"lot {lot}: 16 of 16 cells occupied", 4)
        print("   rng draws:", len(log), "animal record claimed:", [hex(a) for a in sorted(r.writes) if 0x56d1b8 <= a < 0x56d1f8][:8])
        log = []
        r = P.emu(0x4011e0, [lot, 1, 0, 0, 0], build(scen, lot, full - {(2, 3)}).done(), max_insns=2_000_000,
                  hooks=[P.script_random(lambda n, k: [0, 0, 2, 3][k] if k < 4 else 0, log)])
        P.show(r, f"lot {lot}: cell (2,3) free (control)", 4)
        print("   rng draws:", len(log))
