# Batch c3cb: jgld.dll non-render, the zlib 1.0.2 inflate chain (module-aware hooks; addr is the RVA,
# VA = 0x10000000 + addr). Reimplementations and the instruction citations: shim/src/re/c3cb.cpp; branch
# tables and the Purposes: log/c3/c3cb_purpose.md; fixtures: re/frida/js/fixtures.d/c3cb.js.
#
# Every z_stream these bodies are given is the fixture's, and it carries a SLOT allocator and a RECORDING
# deallocator of ours at +0x20 and +0x24. zlib reaches the heap only through those two pointers, so no
# allocation and no free of this batch touches the process heap, the fixture decides which allocation of a
# call returns NULL (that is how every Z_MEM_ERROR arm is reached), and the order of the allocations and
# of the frees is part of the compared state (ctl->i and ctl->freeLog, both inside the ctl regions below).
#
# State regions per key: the z_streams, the ctls, and the parm slice holding every structure a body
# writes (block states, internal_states, code-length arrays, the bl/bd/tl/td out-dwords). The pool the
# allocator hands out for huft tables and sliding windows is NOT a region: no body here writes a window
# byte (they move the window/read/write/end pointers, which live in the block state) and the huft tables
# are huft_build's own deterministic output, which it never reads back; the evidence about them is the
# pointers in the parm slice and the free log.
#
# Hooked callees to switch off when re-running (SIMGOLF_HOOKS_OFF, all of them `jgld.dll:<rva>`):
#   inflate_trees_fixed      9f210                 falloc (this batch, through the pointer it stores)
#   inflate_trees_bits       9f250                 inflate_trees_free (c3bw)
#   inflate_trees_dynamic    9f250                 inflate_trees_free (c3bw)
#   inflate_blocks_reset     9f250,a00e0,9e370     inflate_trees_free (c3bw), inflate_codes_free (this
#                                                  batch), adler32 (c3ax, called through s->checkfn)
#   inflate_blocks_new       9cc70,9e370           inflate_blocks_reset (this batch), adler32 (c3ax)
#   inflate_blocks_free      9cc70,9f250,a00e0,9e370
#   inflateReset             9cc70,9f250,a00e0,9e370
#   inflateEnd               9e290,9cc70,9f250,a00e0,9e370
#   inflateInit2_            9be70,9cd90,9bdd0,9cc70,9e370
#   inflateInit_             9bf10,9be70,9cd90,9bdd0,9cc70,9e370
#   falloc, inflate_codes_new, inflate_codes_free: none (their only call is through the caller's own
#   function pointer, or none at all).
#
# Leaf rule (>= 10 vectors for a function with no named callee): falloc 0x9f210 (12, it calls nothing at
# all, not even __chkesp), inflate_codes_new 0x9f2f0 (12, only the caller's zalloc) and
# inflate_codes_free 0xa00e0 (12, only the caller's zfree). Every other function here has a callee in
# hooks.csv.

_N_FALLOC = 12

_FIXED = [("$fixedMod", 0, 0x10A4), ("$fixedOut", 0, 10 * 0x20)]


def _st(z, ctl, parm, n, parm_stride):
    """z_streams at a 0x40 stride, ctls at 0x100, and the parm slices, for n vectors."""
    return [(z, 0, n * 0x40), (ctl, 0, n * 0x100), (parm, 0, n * parm_stride)]


HOOKS = {
    # falloc 0x0009f210: twelve counters of our own. Each vector subtracts n from the counter and returns
    # fixed_mem + counter*8; the last one subtracts more than the counter holds, so it wraps (the
    # original has no guard, and nothing dereferences the pointer). The third argument is passed four
    # different values to show the body never reads it.
    "falloc": dict(module="jgld.dll", addr=0x0009F210, abi="default", ret="pointer",
                   args=["pointer", "uint32", "uint32"], fixture="c3cb_falloc",
                   state=[("$faBlk", 0, _N_FALLOC * 0x10)],
                   vectors=[("$fa0", 129, 8), ("$fa1", 33, 8), ("$fa2", 128, 8), ("$fa3", 143, 8),
                            ("$fa4", 97, 8), ("$fa5", 24, 8), ("$fa6", 9, 0), ("$fa7", 1, 8),
                            ("$fa8", 0, 8), ("$fa9", 530, 0x10), ("$fa10", 64, 4), ("$fa11", 20, 8)]),

    # inflate_trees_fixed 0x0009f030 with fixed_built 0 (the fixture clears it): ten calls that each run
    # the whole build - 288 literal lengths, huft_build, 30 distance lengths, huft_build - and then copy
    # the four results out. Every vector writes a different slice of the out block, so the (return,
    # state) pairs differ even though the module block comes out identical each time.
    "inflate_trees_fixed": dict(module="jgld.dll", addr=0x0009F030, abi="default", ret="int",
                                args=["pointer", "pointer", "pointer", "pointer"],
                                fixture="c3cb_fixed_unbuilt", state=_FIXED,
                                vectors=[("$fx%dbl" % i, "$fx%dbd" % i, "$fx%dtl" % i, "$fx%dtd" % i)
                                         for i in range(10)]),

    # The other side of the guard at 0x1009f04e: fixed_built is 1 and the four module values carry
    # markers, so the body is only the four copies and nothing is built.
    "inflate_trees_fixed_built": dict(module="jgld.dll", addr=0x0009F030, abi="default", ret="int",
                                      args=["pointer", "pointer", "pointer", "pointer"],
                                      fixture="c3cb_fixed_built", state=_FIXED,
                                      vectors=[("$fb%dbl" % i, "$fb%dbd" % i, "$fb%dtl" % i,
                                                "$fb%dtd" % i) for i in range(10)]),

    # inflate_trees_bits 0x0009e710: eleven 19-entry length arrays, one per huft_build outcome. Vectors
    # 0-3, 9 and 10 return Z_OK, 4 and 5 take the Z_DATA_ERROR arm at 0x1009e74b, 6 and 7 the
    # Z_BUF_ERROR arm at 0x1009e75d (which frees *tb and rewrites r), and 8 gets Z_MEM_ERROR from a
    # starved allocator and falls past both compares.
    "inflate_trees_bits": dict(module="jgld.dll", addr=0x0009E710, abi="default", ret="int",
                               args=["pointer", "pointer", "pointer", "pointer"],
                               fixture="c3cb_trees_bits",
                               state=_st("$tbZ", "$tbCtl", "$tbParm", 11, 0x80),
                               vectors=[("$tbC%d" % i, "$tbB%d" % i, "$tbT%d" % i, "$tbS%d" % i)
                                        for i in range(11)]),

    # inflate_trees_dynamic 0x0009ef00: the same outcomes for each of the two builds. Vectors 1, 2 and 5
    # fail the literal build and return at 0x1009ef87 without freeing *tl; vectors 3, 4 and 6 fail the
    # distance build and run the shared tail at 0x1009f001, which frees *tl as well; the rest return 0.
    "inflate_trees_dynamic": dict(module="jgld.dll", addr=0x0009EF00, abi="default", ret="int",
                                  args=["uint32", "uint32", "pointer", "pointer", "pointer",
                                        "pointer", "pointer", "pointer"],
                                  fixture="c3cb_trees_dyn",
                                  state=_st("$tdZ", "$tdCtl", "$tdParm", 11, 0x100),
                                  vectors=[("$tdNl%d" % i, "$tdNd%d" % i, "$tdC%d" % i, "$tdBl%d" % i,
                                            "$tdBd%d" % i, "$tdTl%d" % i, "$tdTd%d" % i, "$tdS%d" % i)
                                           for i in range(11)]),

    # inflate_codes_new 0x0009f2f0: twelve allocations, three of them starved so the guard at 0x1009f328
    # is taken both ways. Four vectors pass bl and bd above 255, which is what the byte stores at
    # 0x1009f33d and 0x1009f346 turn into the low byte only. Leaf: twelve vectors.
    "inflate_codes_new": dict(module="jgld.dll", addr=0x0009F2F0, abi="default", ret="pointer",
                              args=["uint32", "uint32", "pointer", "pointer", "pointer"],
                              fixture="c3cb_codes_new",
                              state=_st("$cnZ", "$cnCtl", "$cnParm", 12, 0x100),
                              vectors=[("$cnBl%d" % i, "$cnBd%d" % i, "$cnTl%d" % i, "$cnTd%d" % i,
                                        "$cnS%d" % i) for i in range(12)]),

    # inflate_codes_free 0x000a00e0: one ZFREE and no branch. Twelve different pointers, one of them
    # NULL (vector 5), each landing in its own free log. Leaf: twelve vectors.
    "inflate_codes_free": dict(module="jgld.dll", addr=0x000A00E0, abi="default", ret="void",
                               args=["pointer", "pointer"], fixture="c3cb_codes_free",
                               state=_st("$cfZ", "$cfCtl", "$cfParm", 12, 0x80),
                               vectors=[("$cfP%d" % i, "$cfS%d" % i) for i in range(12)]),

    # inflate_blocks_reset 0x0009cc70: twelve block states. Vectors 1, 3, 5, 7, 9 and 11 carry adler32 as
    # the check function (both sides of 0x1009cc8b and 0x1009cd45); 2, 3, 8 and 10 are in mode BTREE or
    # DTREE (the ZFREE at 0x1009ccbf); 4, 5 and 9 are in mode CODES (the three releases at 0x1009ccdf,
    # 0x1009ccf2 and 0x1009cd05); 0, 1, 6, 7 and 11 are in modes that take neither arm.
    "inflate_blocks_reset": dict(module="jgld.dll", addr=0x0009CC70, abi="default", ret="void",
                                 args=["pointer", "pointer", "pointer"], fixture="c3cb_blocks_reset",
                                 state=_st("$brZ", "$brCtl", "$brParm", 12, 0x400),
                                 vectors=[("$brS%d" % i, "$brZs%d" % i, "$brC%d" % i)
                                          for i in range(12)]),

    # inflate_blocks_new 0x0009cd90: twelve calls. Vectors 6, 7 and 8 starve the first allocation (the
    # function returns the NULL it got at 0x1009cdce), 9, 10 and 11 starve the window (it gives the block
    # state back through ZFREE at 0x1009ce14 and returns 0), the other six succeed with window sizes from
    # 8 to 1024 and with and without adler32.
    "inflate_blocks_new": dict(module="jgld.dll", addr=0x0009CD90, abi="default", ret="pointer",
                               args=["pointer", "pointer", "uint32"], fixture="c3cb_blocks_new",
                               state=_st("$bnZ", "$bnCtl", "$bnParm", 12, 0x100),
                               vectors=[("$bnS%d" % i, "$bnF%d" % i, "$bnW%d" % i)
                                        for i in range(12)]),

    # inflate_blocks_free 0x0009e290: no branch of its own. Twelve block states in the reset fixture's
    # shapes, so the recorded free order - what the reset released, then the window, then the state -
    # differs from vector to vector.
    "inflate_blocks_free": dict(module="jgld.dll", addr=0x0009E290, abi="default", ret="int",
                                args=["pointer", "pointer", "pointer"], fixture="c3cb_blocks_free",
                                state=_st("$bfZ", "$bfCtl", "$bfParm", 12, 0x400),
                                vectors=[("$bfS%d" % i, "$bfZs%d" % i, "$bfC%d" % i)
                                         for i in range(12)]),

    # inflateReset 0x0009bdd0: vector 0 passes a null stream and vectors 1 and 11 a stream with a null
    # state, so both tests of the shared Z_STREAM_ERROR arm are taken; the rest differ in nowrap (mode
    # BLOCKS 7 against METHOD 0) and in the block state the reset reaches.
    "inflateReset": dict(module="jgld.dll", addr=0x0009BDD0, abi="default", ret="int",
                         args=["pointer"], fixture="c3cb_inflate_reset",
                         state=_st("$irZ", "$irCtl", "$irParm", 12, 0x400),
                         vectors=[("$irS%d" % i,) for i in range(12)]),

    # inflateEnd 0x0009be70: vector 0 a null stream, 1 a null state, 2 and 11 a null zfree (the three
    # tests of the shared arm at 0x1009bea0); 3 and 8 have no block state, so the call at 0x1009bec5 is
    # skipped; the rest free the whole chain.
    "inflateEnd": dict(module="jgld.dll", addr=0x0009BE70, abi="default", ret="int",
                       args=["pointer"], fixture="c3cb_inflate_end",
                       state=_st("$ieZ", "$ieCtl", "$ieParm", 12, 0x400),
                       vectors=[("$ieS%d" % i,) for i in range(12)]),

    # inflateInit2_ 0x0009bf10: sixteen vectors over every guard but the zalloc one (its own key below).
    # 0 a null version, 1 a wrong version byte, 2 a wrong stream_size, 3 a null stream, 4 a null zfree
    # (zcfree installed, then success), 5 a starved state allocation, 6 and 15 a negative w (nowrap), 7 w
    # below 8, 8 w above 15, 9 a negative w above 15 in magnitude, 12 and 13 the two ways
    # inflate_blocks_new can fail (which run inflateEnd at 0x1009c075), and 10, 11 and 14 plain successes
    # with w = 8, 15 and 9.
    "inflateInit2_": dict(module="jgld.dll", addr=0x0009BF10, abi="default", ret="int",
                          args=["pointer", "int", "pointer", "int"], fixture="c3cb_init2",
                          state=_st("$i2Z", "$i2Ctl", "$i2Parm", 16, 0x100),
                          vectors=[("$i2S%d" % i, "$i2W%d" % i, "$i2V%d" % i, "$i2Sz%d" % i)
                                   for i in range(16)]),

    # The arm at 0x1009bf6c: a stream with no allocator gets zcalloc 0x1009e680 and a cleared opaque (and
    # zcfree 0x1009e6d0 for its missing deallocator), and everything below allocates through the C
    # runtime. The two arms therefore get different blocks by construction, so the regions here are the
    # z_stream MINUS the state pointer at +0x1c: what is compared is the two installed pointers, the
    # cleared opaque, msg, total_in, total_out and adler, plus the return value. The structures' contents
    # are compared by the fourteen vectors of the main key that allocate from our own slots.
    "inflateInit2__zcalloc": dict(module="jgld.dll", addr=0x0009BF10, abi="default", ret="int",
                                  args=["pointer", "int", "pointer", "int"],
                                  fixture="c3cb_init2_zcalloc",
                                  state=[("$zcBlk", 0x00, 0x1C), ("$zcBlk", 0x20, 0x20),
                                         ("$zcBlk", 0x40, 0x1C), ("$zcBlk", 0x60, 0x20)],
                                  vectors=[("$zcS0", 8, "$zcVer", 0x38),
                                           ("$zcS1", 9, "$zcVer", 0x38)]),

    # inflateInit_ 0x0009c0b0: no conditional jump of its own, so the twelve vectors are chosen for what
    # inflateInit2_ returns through it - Z_VERSION_ERROR (0, 1, 2, 9), Z_STREAM_ERROR (3), Z_MEM_ERROR
    # from each of the three allocations (4, 10, 5, 6) and Z_OK with the full 32 KiB window (7, 8, 11).
    "inflateInit_": dict(module="jgld.dll", addr=0x0009C0B0, abi="default", ret="int",
                         args=["pointer", "pointer", "int"], fixture="c3cb_init",
                         state=_st("$i1Z", "$i1Ctl", "$i1Parm", 12, 0x100),
                         vectors=[("$i1S%d" % i, "$i1V%d" % i, "$i1Sz%d" % i) for i in range(12)]),
}
