import plib as P
import re
# find calls to evalPlacementArea 0x40db90 within generateRandomCourse
for a, f, m, o in P._all_insns():
    if m == "call" and o.strip() in ("0x40db90",):
        print(hex(a), hex(f))
