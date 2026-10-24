"""D7: stampCourseTiles 0x0040e000 free-slot scan (0x0040e2f1..0x0040e30a) with the 256-record placed-object table full."""
import plib as P

TAB, REC, N = 0x58bcb8, 0x10, 0x100
for scen, k in (("sandbox_basic", "00"), ("course_season", "00")):
    base = P.start_image(scen, k)
    # control: table with 3 live records -> returns record index 3
    im = P.Img(base)
    for i in range(256):
        im.w(TAB + i * REC, 0xFFFF, 2)
    for i in range(3):
        im.w(TAB + i * REC, 0, 2)
    r = P.emu(0x40e000, [10, 10, 0, 0], im.done())
    P.show(r, f"{scen} control, 3 live records, kind 0", 4)
    # full table
    im = P.Img(base)
    for i in range(256):
        im.w(TAB + i * REC, 0, 2)
    data = im.done()
    # where the scan stops: first word == 0xffff at stride 0x10 from the first byte past the table
    a = TAB + N * REC
    while a < P.D0 + P.DN and int.from_bytes(data[a - P.D0:a - P.D0 + 2], "little") != 0xFFFF:
        a += REC
    print(f"  {scen}: scan past the table ends at {a:#x} (index {(a - TAB) // REC}), symbols there:")
    P.pr_syms(a, a + 2)
    r = P.emu(0x40e000, [10, 10, 0, 0], data)
    P.show(r, f"{scen} full table, kind 0", 4)
    print("   writes into the placed-object table:", sum(1 for x in r.writes if TAB <= x < TAB + N * REC),
          " writes outside table and tile grids:", [hex(x) for x in sorted(r.writes) if not (0x53caf0 <= x < 0x53caf0 + 0x2800 or 0x569880 <= x < 0x56a300 or 0x572000 <= x < 0x572f00 or 0x53bba0 <= x < 0x53c600 or 0x5a4998 <= x < 0x5a5800)][:10])
    # worst case: no 0xffff word anywhere at stride 0x10 past the table
    bb = bytearray(data)
    a = TAB + N * REC
    while a < P.D0 + P.DN:
        if int.from_bytes(bb[a - P.D0:a - P.D0 + 2], "little") == 0xFFFF:
            bb[a - P.D0] = 0
        a += REC
    r = P.emu(0x40e000, [10, 10, 0, 0], bytes(bb))
    P.show(r, f"{scen} full table and no 0xffff word at stride 0x10 to the end of .data", 2)
print("image: IMAGE_END", hex(P.E.IMAGE_END), "data end", hex(P.D0 + P.DN))
