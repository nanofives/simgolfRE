"""S3.16: GameState symbol + offset of exe data VAs (from port/include/gamestate_addresses.inc).  16_sym.py <va>..."""
import re, sys, bisect, pathlib
t = pathlib.Path("port/build/S3_16/port/include/gamestate_addresses.inc").read_text()
rows = sorted((int(v, 16), s) for s, v in re.findall(r"SG_ADDRESS\((\w+), (0x[0-9a-fA-F]+)\)", t))
vas = [r[0] for r in rows]
for a in sys.argv[1:]:
    v = int(a, 16)
    i = bisect.bisect_right(vas, v) - 1
    print(a, rows[i][1], hex(v - rows[i][0]))
