"""I1 (S6 merge): check sg_profile_set / sg_profile_get (MERGE.md item 5) on a built portsim_c.dll (x64, ctypes).
Usage: ~/bin/py -3.12 log/roadmap/merges/S6-1/abi_check.py <portsim_c.dll> <defect_profiles.tsv>"""
import ctypes as C
import sys

dll = C.CDLL(sys.argv[1])
dll.sg_profile_set.argtypes = [C.c_char_p, C.POINTER(C.c_char_p), C.POINTER(C.c_int32), C.c_int32]
dll.sg_profile_get.restype = C.c_char_p
dll.sg_defect_get.argtypes = [C.c_char_p]
dll.sg_defect_set.argtypes = [C.c_char_p, C.c_int32]
members = [l.split("\t")[:2] for l in open(sys.argv[2], encoding="utf-8").read().splitlines()
           if l and not l.startswith("#") and not l.startswith("switch\t")]
names = [m[0].encode() for m in members]
vals = [int(m[1]) for m in members]
n = dll.sg_defect_count()
fails = 0


def check(what, cond):
    global fails
    print(("OK   " if cond else "FAIL ") + what)
    fails += 0 if cond else 1


def arr(xs, t):
    return (t * len(xs))(*xs)


check(f"{n} switches, fresh library reads original", dll.sg_profile_get() == b"original")
check("fixed with the 6 table members -> SG_OK", dll.sg_profile_set(b"fixed", arr(names, C.c_char_p), arr(vals, C.c_int32), len(names)) == 0)
check("get -> fixed", dll.sg_profile_get() == b"fixed")
check("every member at its table value", all(dll.sg_defect_get(k) == v for k, v in zip(names, vals)))
check("sg_defect_set directly -> custom", dll.sg_defect_set(names[0], 1) == 0 and dll.sg_profile_get() == b"custom")
# unknown switch: nothing changes (names[1] stays 0 = still the fixed value)
bad = names + [b"noSuch.switch"]
check("unknown switch -> SG_E_ARG", dll.sg_profile_set(b"fixed", arr(bad, C.c_char_p), arr(vals + [0], C.c_int32), len(bad)) < 0)
check("  ... and nothing changed", dll.sg_defect_get(names[1]) == 0 and dll.sg_defect_get(names[0]) == 1)
check("unknown profile -> SG_E_ARG", dll.sg_profile_set(b"other", None, None, 0) < 0)
check("original with members -> SG_E_ARG", dll.sg_profile_set(b"original", arr(names, C.c_char_p), arr(vals, C.c_int32), 1) < 0)
check("NULL list with n > 0 -> SG_E_ARG", dll.sg_profile_set(b"fixed", None, None, 2) < 0)
check("original -> SG_OK, every switch 1", dll.sg_profile_set(b"original", None, None, 0) == 0
      and all(dll.sg_defect_get(k) == 1 for k in names) and dll.sg_profile_get() == b"original")
check("fixed with 0 members reads original", dll.sg_profile_set(b"fixed", None, None, 0) == 0 and dll.sg_profile_get() == b"original")
print("FAILED" if fails else "ALL OK", fails)
sys.exit(1 if fails else 0)
