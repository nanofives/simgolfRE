# C3 batch c3w (2026-10-07): util/libjpeg leaves with static callers the test scenarios never reach, reimplemented
# in shim/src/re/c3w.cpp. Fixtures in re/frida/js/fixtures.d/c3w.js. Which side of every jcc each vector takes is in
# log/c3/c3w_purpose.md. Keys match the hooks.csv names. Keys are merged into HOOKS by hooks_registry.py.

# jpeg_quality_scaling (q): q<1 -> 5000; 1..49 -> 5000/q; 50..100 -> (100-q)*2; >100 -> 0.
_QUAL = [(v,) for v in (-0x80000000, -5, -1, 0, 1, 2, 5, 10, 25, 49, 50, 51, 75, 99, 100, 101, 150, 1000, 0x7FFFFFFF)]

# directionOf (dx, dy): |dx|>|dy| -> dx<=0?6:2; else dy<=0?0:4. Grid covers both axes, the |dx|==|dy| tie, zero.
_DIR = [(x, y) for x in (-100, -5, -1, 0, 1, 5, 100) for y in (-100, -5, -1, 0, 1, 5, 100)]
_DIR += [(-0x80000000, 1), (0x7FFFFFFF, -1), (3, 3), (-3, 3), (7, -7), (-8, 8)]

# swapTableEntry (a, b): swaps two 0x100-byte golfer records (base 0x005794b8) through scratch 0x00582cb8.
_SWAP = [(0, 1), (1, 0), (2, 5), (5, 2), (0, 3), (3, 4), (1, 4), (4, 1), (0, 0), (2, 2), (3, 5), (5, 0)]
_SWAP_STATE = [(0x005794B8, 0, 6 * 0x100), (0x00582CB8, 0, 0x100)]

# sanitizeFileName (s): reads one of ten strings laid out in one buffer; trims trailing spaces in place.
_NAMES = [("$n%d" % i,) for i in range(10)]

# jcopy_sample_rows (src, src_row, dst, dst_row, num_rows, num_bytes): copies num_rows row pointers of num_bytes.
_JCOPY = [("$src", 0, "$dst", 0, 4, 64), ("$src", 1, "$dst", 0, 2, 32), ("$src", 0, "$dst", 2, 1, 1),
          ("$src", 0, "$dst", 0, 0, 64), ("$src", 2, "$dst", 1, 2, 7), ("$src", 0, "$dst", 0, 1, 64),
          ("$src", 1, "$dst", 1, 3, 16), ("$src", 0, "$dst", 3, 1, 5), ("$src", 2, "$dst", 0, 2, 33),
          ("$src", 3, "$dst", 3, 1, 64), ("$src", 0, "$dst", 0, 4, 1), ("$src", 1, "$dst", 2, 2, 60)]

# expand_right_edge (image_data, num_rows, input_cols, output_cols): pads each row from input_cols with its last pixel.
_EXPAND = [("$img", 2, 10, 20), ("$img", 1, 5, 8), ("$img", 2, 10, 10), ("$img", 2, 20, 10), ("$img", 0, 10, 20),
           ("$img", 3, 1, 4), ("$img", 4, 30, 64), ("$img", 4, 1, 64), ("$img", 1, 63, 64), ("$img", 2, 16, 48),
           ("$img", 3, 8, 8), ("$img", 4, 32, 33)]

# jpeg_suppress_tables (cinfo, suppress): writes suppress into every non-NULL quant (+0x80) and huff (+0x114) table.
_SUPPRESS = [("$cinfo", s) for s in (0, 1, 2, -1, 0x55, 0x100, 0x7FFFFFFF, 3, 4, 5, -2, 0x1234)]
_SUPPRESS_STATE = [("$tbls", 0, 6 * 0x120)]

HOOKS = {
    "jpeg_quality_scaling": dict(module="golf_clean.exe", addr=0x004AE700, abi="default", ret="int", args=["int"],
                                 vectors=_QUAL),
    "directionOf": dict(module="golf_clean.exe", addr=0x0042E7A0, abi="default", ret="int", args=["int", "int"],
                        vectors=_DIR),
    "findByteInRange": dict(module="golf_clean.exe", addr=0x004935F0, abi="default", ret="pointer", args=["pointer"],
                            fixture="c3w_bytes",
                            vectors=[("$s0",), ("$s1",), ("$s2",), ("$s3",), ("$s4",), ("$s5",), ("$s6",), ("$s7",),
                                     ("$s8",), ("$s9",), ("$s10",), (0,)]),
    "swapTableEntry": dict(module="golf_clean.exe", addr=0x0045DE30, abi="default", ret="void", args=["int", "int"],
                           fixture="c3w_golfers", state=_SWAP_STATE, vectors=_SWAP),
    "sanitizeFileName": dict(module="golf_clean.exe", addr=0x00405AC0, abi="default", ret="bool", args=["pointer"],
                             fixture="c3w_names", state=[("$buf", 0, 0x200)], vectors=_NAMES),
    "jcopy_sample_rows": dict(module="golf_clean.exe", addr=0x004B04F0, abi="default", ret="void",
                              args=["pointer", "int", "pointer", "int", "int", "uint32"], fixture="c3w_rows",
                              state=[("$dbufs", 0, 4 * 64)], vectors=_JCOPY),
    "expand_right_edge": dict(module="golf_clean.exe", addr=0x004B4390, abi="default", ret="void",
                              args=["pointer", "int", "int", "int"], fixture="c3w_edge",
                              state=[("$bufs", 0, 4 * 64)], vectors=_EXPAND),
    "jpeg_suppress_tables": dict(module="golf_clean.exe", addr=0x004AE220, abi="default", ret="void",
                                 args=["pointer", "int"], fixture="c3w_jpeg", state=_SUPPRESS_STATE,
                                 vectors=_SUPPRESS),
}
