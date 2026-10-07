# C3 batch c3r (2026-10-07): util leaves with a static caller but no reach in the test scenarios, reimplemented in
# shim/src/re/c3r.cpp. Format documented at the top of re/frida/hooks_registry.py; fixtures in
# re/frida/js/fixtures.d/c3r.js. Which side of every jcc each vector takes is listed in log/c3/c3r_purpose.md.

_I32 = [-0x80000000, -0x7FFFFFFF, -1000, -65, -64, -63, -9, -8, -7, -2, -1, 0, 1, 2, 7, 8, 9, 63, 64, 65, 1000,
        0x7FFFFFFE, 0x7FFFFFFF]

# boundIndex 0x00463170: (i, n) pairs on both sides of the signed `jl`, equal values, and sign/overflow edges.
_BOUND = [(i, n) for i in (-0x80000000, -5, -1, 0, 1, 7, 15, 16, 0x7FFFFFFF)
          for n in (-0x80000000, -1, 0, 1, 7, 16, 0x7FFFFFFF)]

# jdiv_round_up 0x004b04c0 / jround_up 0x004b04d0: divisors of both signs; dividends 0, exact multiples, remainders,
# negatives and sums that wrap at 32 bits. b = 0 and (a + b - 1, b) = (INT_MIN, -1) raise #DE in idiv and are excluded.
_DIVS = [1, 2, 3, 7, 8, 16, 64, 1000, 0x7FFFFFFF, -1, -3, -8]
_DIVIDENDS = [0, 1, 2, 7, 8, 9, 15, 16, 17, 100, 1023, -1, -7, -8, -9, -1000, 0x7FFFFFFF, 0x7FFFFFF0, -0x80000000]


def _safe(a, b):
    c = (a + b - 1 + 0x80000000) % 0x100000000 - 0x80000000
    return b != 0 and not (c == -0x80000000 and b == -1)


_ROUND = [(a, b) for a in _DIVIDENDS for b in _DIVS if _safe(a, b)]

# reverseFindChar 0x004935b0: (base, p, ch) over the 64-byte buffer $o0..$o63 of fixture c3r_rfind (layout there).
_RFIND = [
    (0, "$o20", 0x2F),            # base NULL                 -> NULL (0x004935ba)
    ("$o0", 0, 0x2F),             # p NULL                    -> NULL (0x004935c1)
    ("$o10", "$o10", 0x2F),       # p == base                 -> NULL (0x004935c9)
    ("$o0", "$o20", 0x2F),        # nearest '/' below p        -> o17
    ("$o0", "$o16", 0x2F),        # next one down              -> o3
    ("$o0", "$o17", 0x2F),        # match at p itself          -> o17
    ("$o0", "$o2", 0x2F),         # range o2..o1 has none      -> NULL
    ("$o0", "$o20", 0x5A),        # 'Z' only at base o0, never compared -> NULL
    ("$o40", "$o50", 0x51),       # 'Q' only at base o40       -> NULL
    ("$o39", "$o50", 0x51),       # 'Q' at base+1              -> o40
    ("$o0", "$o50", 0x7A),        # 'z' absent                 -> NULL after 50 compares
    ("$o0", "$o63", 0x00),        # NUL at o63                 -> o63
    ("$o0", "$o40", 0x151),       # upper bits of ch ignored   -> o40
    ("$o0", "$o40", -0xAF),       # 0xffffff51: low byte 'Q'   -> o40
    ("$o4", "$o16", 0x2E),        # '.' at o9                  -> o9
    ("$o9", "$o16", 0x2E),        # '.' only at base o9        -> NULL
]

# swapInts 0x00493580: pairs of cells $c0..$c7 (values in fixture c3r_swap); a == b leaves the cell unchanged.
_SWAP = [("$c0", "$c1"), ("$c1", "$c0"), ("$c2", "$c2"), ("$c3", "$c7"), ("$c5", "$c5"), ("$c0", "$c4"),
         ("$c6", "$c7"), ("$c4", "$c5"), ("$c7", "$c3"), ("$c0", "$c0"), ("$c2", "$c6"), ("$c1", "$c5")]

# HashTable::bucketEmpty 0x00487770 / bucketSet 0x004877a0: every in-range bucket and indexes above 15 (unsigned).
_BUCKETS = [(("$t", i)) for i in list(range(16)) + [16, 17, 31, 0xFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFF0, 0xFFFFFFFF]]

# overlayCharAt 0x00456bb0: cells whose tile type (fixture c3r_overlay) spans all 32 seeded type entries, plus
# (0,0) and (1,1), seeded with the negative types -1 and -2.
_OVERLAY = [(x, y) for x in range(10) for y in (0, 1, 2, 5)] + [(49, 49), (25, 13), (48, 0)]

HOOKS = {
    "boundIndex": dict(module="golf_clean.exe", addr=0x00463170, abi="default", ret="int", args=["int", "int"],
                       vectors=_BOUND),
    "jdiv_round_up": dict(module="golf_clean.exe", addr=0x004B04C0, abi="default", ret="int", args=["int", "int"],
                          vectors=_ROUND),
    "jround_up": dict(module="golf_clean.exe", addr=0x004B04D0, abi="default", ret="int", args=["int", "int"],
                      vectors=_ROUND),
    "reverseFindChar": dict(module="golf_clean.exe", addr=0x004935B0, abi="default", ret="pointer",
                            args=["pointer", "pointer", "int"], fixture="c3r_rfind", vectors=_RFIND),
    "swapInts": dict(module="golf_clean.exe", addr=0x00493580, abi="default", ret="void", args=["pointer", "pointer"],
                     fixture="c3r_swap", state=[("$obj", 0, 0x20)], vectors=_SWAP),
    "appendNewline": dict(module="golf_clean.exe", addr=0x004925F0, abi="default", ret="void", args=["pointer"],
                          fixture="c3r_nl", state=[("$obj", 0, 12 * 0x40)], vectors=[("$s%d" % i,) for i in range(12)]),
    # list iterators: $i0..$i11 (0x18 bytes each, in $obj) over the 6-node ring of fixture c3r_iter.
    "listIterValue": dict(module="golf_clean.exe", addr=0x00402160, abi="fastcall", ret="uint32", args=["pointer"],
                          fixture="c3r_iter", vectors=[("$i%d" % i,) for i in range(12)]),
    "listIterNext": dict(module="golf_clean.exe", addr=0x00402130, abi="fastcall", ret="uint32", args=["pointer"],
                         fixture="c3r_iter", state=[("$obj", 0, 12 * 0x18), ("$nodes", 0, 6 * 0x10)],
                         vectors=[("$i%d" % i,) for i in range(12)]),
    # readListNode: holders $h0 (null node) and $h1..$h3; out cells $f / $s (state) or NULL.
    "readListNode": dict(module="golf_clean.exe", addr=0x004A4EA0, abi="thiscall", ret="uint32",
                         args=["pointer", "pointer", "pointer"], fixture="c3r_node", state=[("$cells", 0, 8)],
                         vectors=[(h, f, s) for h in ("$h0", "$h1", "$h2", "$h3")
                                  for f, s in (("$f", "$s"), ("$f", 0), (0, "$s"), (0, 0))]
                         + [("$h1", "$f", "$f"), ("$h2", "$s", "$s"), ("$h3", "$s", "$f")]),
    "HashTable::bucketEmpty": dict(module="golf_clean.exe", addr=0x00487770, abi="thiscall", ret="uint8",
                                   args=["pointer", "uint32"], fixture="c3r_buckets", vectors=_BUCKETS),
    "HashTable::bucketSet": dict(module="golf_clean.exe", addr=0x004877A0, abi="thiscall", ret="uint8",
                                 args=["pointer", "uint32"], fixture="c3r_buckets", vectors=_BUCKETS),
    "overlayCharAt": dict(module="golf_clean.exe", addr=0x00456BB0, abi="default", ret="int8", args=["int", "int"],
                          fixture="c3r_overlay", vectors=_OVERLAY),
    # libjpeg: cinfo objects $a0..$a11 (memory manager always set) and $d0..$d11 (NULL for d0, d3, d6, d9) in $obj;
    # the fake memory managers' free_pool / self_destruct callbacks log into $rec.
    "jpeg_abort": dict(module="golf_clean.exe", addr=0x004AFA60, abi="default", ret="void", args=["pointer"],
                       fixture="c3r_jpeg", state=[("$rec", 0, 0x10), ("$obj", 0, 24 * 0x20)],
                       vectors=[("$a%d" % i,) for i in range(12)]),
    "jpeg_destroy": dict(module="golf_clean.exe", addr=0x004AFA90, abi="default", ret="void", args=["pointer"],
                         fixture="c3r_jpeg", state=[("$rec", 0, 0x10), ("$obj", 0, 24 * 0x20)],
                         vectors=[("$d%d" % i,) for i in range(12)]),
}
