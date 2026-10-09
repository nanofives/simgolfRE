# Batch c3ch: jgld.dll - the top of the zlib 1.0.2 inflate chain and libpng 1.0.5's chunk reader
# (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr). Reimplementations and the instruction
# citations: shim/src/re/c3ch.cpp; branch tables and the Purposes: log/c3/c3ch_purpose.md; fixtures:
# re/frida/js/fixtures.d/c3ch.js; the byte tables those embed: log/c3/c3ch_gen.py.
#
# THE ZLIB THREE. Every z_stream these bodies are given is the fixture's and carries c3cb's slot
# allocator at +0x20 and recording deallocator at +0x24, so no allocation and no free touches the process
# heap, the fixture decides which allocation of a call returns NULL (that is how every Z_MEM_ERROR arm is
# reached) and the order of the allocations and frees is compared state (ctl->i and ctl->freeLog).
#
# THE PNG FOUR. Every png_struct is the fixture's and carries a 35-byte __cdecl reader of ours in the
# read_data_fn slot at +0x50 (the slot png_set_read_fn 0x10078cb0 writes) plus a one-byte `ret` in the
# warning slot at +0x44. The reader's cursor and call counter are compared state, so the sequence of
# lengths each arm asked for is evidence.
#
# WHAT IS NOT COMPARED. The pool the slot allocator hands huft tables and blens arrays out of is left out
# of the state for inflate_blocks and inflate: those tables are huft_build's own output, which huft_build's
# own key compares entry by entry, and what proves the decode here is the bytes in the output buffer and
# the window plus the stream's counters. huft_build's key DOES compare the first four tables of every
# vector (0x480 bytes each), which is where the exop/bits/base bytes and the 0xcc padding show up.
#
# Hooked callees to switch off when re-running on the main build (SIMGOLF_HOOKS_OFF, all `jgld.dll:<rva>`):
#   huft_build       9f250                                   inflate_trees_free (c3bw)
#   inflate_blocks   9f030,9f2f0,a00e0,9e710,9ef00,9f250,9f370,a0130,9e370,9e7a0
#                                                            inflate_trees_fixed / inflate_codes_new /
#                                                            inflate_codes_free / inflate_trees_bits /
#                                                            inflate_trees_dynamic / inflate_trees_free
#                                                            (c3cb), inflate_codes (c3bg),
#                                                            inflate_flush (c3bb), adler32 (c3ax),
#                                                            huft_build (this batch)
#   inflate          9ce80,9cc70 + everything inflate_blocks reaches
#   png_read_data    none
#   png_crc_read     78c40 (this batch), 787d0 (png_calculate_crc, c3ao), 9ca70 (crc32, c3ax)
#   png_crc_error    78c40 (this batch)
#   png_crc_finish   794b0,78c40,79620 (this batch), 787d0, 9ca70, 79220 (png_format_buffer, c3bb)
#
# Leaf rule: none of the seven is a leaf (every one of them calls at least one function that has a
# hooks.csv row), so the >= 10 vectors rule does not apply; the counts are 17, 17 + 33, 34, 12, 12, 12
# and 16 anyway.

_N_HUFT = 17
_N_BLOCKS = 17
_N_ENTRY = 33
_N_INFLATE = 34
_N_PNG = 12
_N_CRCFIN = 16

# inflate_blocks_entry: the parm slice holds a 258-dword blens array per vector and, for vector 12, an
# inflate_codes_state at +0x490. Only the first 19 entries of blens are ever written (the BTREE case
# stores through border[]), so comparing 0x60 bytes of each slice plus that one codes state keeps the
# compared region small enough to hash 18 times per arm.
_ENTRY_PARM = ([("$ieParm", i * 0x500, 0x60) for i in range(_N_ENTRY)]
               + [("$ieParm", 12 * 0x500 + 0x490, 0x20)])


HOOKS = {
    # huft_build 0x0009e7a0: sixteen code-length sets, one per outcome and per table shape. Vectors 0-3,
    # 7, 10-15 return Z_OK, 4 and 5 Z_DATA_ERROR (the final test and the in-loop one), 6 Z_BUF_ERROR, and
    # 8 and 9 Z_MEM_ERROR with h == 0 and h != 0. The compared state is the out words (*m and *t), the
    # z_streams, the control blocks and the first four tables each vector builds.
    "huft_build": dict(module="jgld.dll", addr=0x0009E7A0, abi="default", ret="int",
                       args=["pointer", "uint32", "uint32", "pointer", "pointer",
                             "pointer", "pointer", "pointer"],
                       fixture="c3ch_huft",
                       state=[("$hbZ", 0, _N_HUFT * 0x40), ("$hbCtl", 0, _N_HUFT * 0x100),
                              ("$hbOut", 0, _N_HUFT * 0x10), ("$hbHot", 0, _N_HUFT * 4 * 0x120)],
                       vectors=[("$hbB%d" % i, "$hbN%d" % i, "$hbS%d" % i, "$hbD%d" % i, "$hbE%d" % i,
                                 "$hbT%d" % i, "$hbM%d" % i, "$hbZs%d" % i) for i in range(_N_HUFT)]),

    # inflate_blocks 0x0009ce80 entering in mode TYPE: sixteen whole deflate blocks (stored, fixed,
    # dynamic, the minimal dynamic one, block type 3, a non-final stored block followed by a fixed one)
    # and the allocation failures each can hit. The compared state is the z_streams, the control blocks,
    # the block states and the per-vector window and output buffer.
    "inflate_blocks": dict(module="jgld.dll", addr=0x0009CE80, abi="default", ret="int",
                           args=["pointer", "pointer", "int"], fixture="c3ch_blocks",
                           state=[("$ibZ", 0, _N_BLOCKS * 0x40), ("$ibCtl", 0, _N_BLOCKS * 0x100),
                                  ("$ibS", 0, _N_BLOCKS * 0x40), ("$ibBuf", 0, _N_BLOCKS * 0x280)],
                           vectors=[("$ibSt%d" % i, "$ibZs%d" % i, "$ibR%d" % i)
                                    for i in range(_N_BLOCKS)]),

    # The same body entered in each of the other modes, so every slot of the ten-entry jump table at
    # 0x1009e258 and the default arm at 0x1009cf11 run: LENS 1 (three shapes), STORED 2 (two), TABLE 3
    # (two), BTREE 4 (three), DTREE 5 (two), CODES 6, DRY 7 (two), DONE 8, BAD 9 and mode 10.
    "inflate_blocks_entry": dict(module="jgld.dll", addr=0x0009CE80, abi="default", ret="int",
                                 args=["pointer", "pointer", "int"], fixture="c3ch_blocks_entry",
                                 state=[("$ieZ", 0, _N_ENTRY * 0x40), ("$ieCtl", 0, _N_ENTRY * 0x100),
                                        ("$ieS", 0, _N_ENTRY * 0x40), ("$ieBuf", 0, _N_ENTRY * 0x100)]
                                       + _ENTRY_PARM,
                                 vectors=[("$ieSt%d" % i, "$ieZs%d" % i, 0) for i in range(_N_ENTRY)]),

    # inflate 0x0009c0f0: the four guards at the top (vectors 0-3), every slot of the fourteen-entry
    # jump table at 0x1009c79d and the default arm (vector 27). Vectors 17, 18 and 19 run a whole
    # deflate block through inflate_blocks; the rest consume the header, dictionary-id and check bytes
    # one at a time straight out of the z_stream.
    "inflate": dict(module="jgld.dll", addr=0x0009C0F0, abi="default", ret="int",
                    args=["pointer", "int"], fixture="c3ch_inflate",
                    state=[("$ifZ", 0, _N_INFLATE * 0x40), ("$ifCtl", 0, _N_INFLATE * 0x100),
                           ("$ifSt", 0, _N_INFLATE * 0x20), ("$ifS", 0, _N_INFLATE * 0x40),
                           ("$ifBuf", 0, _N_INFLATE * 0xC0)],
                    vectors=[("$ifZs%d" % i, "$ifF%d" % i) for i in range(_N_INFLATE)]),

    # png_read_data 0x00078c40: twelve forwards through the reader in the png_struct's +0x50 slot, with
    # lengths from 0 to 0x3f. The compared state is the destination bytes, the reader's cursor and its
    # call counter. The null-read_data_fn arm at 0x10078c81 is not taken by any vector: it calls
    # png_error 0x10078fe0, which never returns.
    "png_read_data": dict(module="jgld.dll", addr=0x00078C40, abi="default", ret="void",
                          args=["pointer", "pointer", "uint32"], fixture="c3ch_readdata",
                          state=[("$rdPng", 0, _N_PNG * 0x200), ("$rdDst", 0, _N_PNG * 0x40),
                                 ("$rdCtl", 0, _N_PNG * 0x10)],
                          vectors=[("$rdP%d" % i, "$rdD%d" % i, "$rdL%d" % i) for i in range(_N_PNG)]),

    # png_crc_read 0x000794b0: the same twelve reads followed by png_calculate_crc 0x100787d0, whose own
    # two arms the chunk names and flag words below take both ways. The compared state adds the running
    # crc at png_struct+0x110.
    "png_crc_read": dict(module="jgld.dll", addr=0x000794B0, abi="default", ret="void",
                         args=["pointer", "pointer", "uint32"], fixture="c3ch_crcread",
                         state=[("$crPng", 0, _N_PNG * 0x200), ("$crDst", 0, _N_PNG * 0x40),
                                ("$crCtl", 0, _N_PNG * 0x10)],
                         vectors=[("$crP%d" % i, "$crD%d" % i, "$crL%d" % i) for i in range(_N_PNG)]),

    # png_crc_error 0x00079620: twelve chunks covering both sides of all four of its tests. The four CRC
    # bytes are read on every path, so the reader's cursor advances by four in every vector even where
    # need_crc is 0 and nothing is compared.
    "png_crc_error": dict(module="jgld.dll", addr=0x00079620, abi="default", ret="int",
                          args=["pointer"], fixture="c3ch_crcerror",
                          state=[("$cePng", 0, _N_PNG * 0x200), ("$ceCtl", 0, _N_PNG * 0x10)],
                          vectors=[("$ceP%d" % i,) for i in range(_N_PNG)]),

    # png_crc_finish 0x00079510: sixteen chunks whose skip counts and zbuf sizes take both sides of the
    # loop at 0x1007954b and of the remainder test at 0x10079573, and whose crc bytes - the crc32 of
    # exactly the bytes the reader will deliver - take both sides of 0x1007959d. Every disagreeing vector
    # reaches png_chunk_warning 0x10079250, never png_chunk_error 0x100790a0 (which ends in longjmp).
    "png_crc_finish": dict(module="jgld.dll", addr=0x00079510, abi="default", ret="int",
                           args=["pointer", "uint32"], fixture="c3ch_crcfinish",
                           state=[("$cfPng", 0, _N_CRCFIN * 0x200), ("$cfZbuf", 0, _N_CRCFIN * 0x40),
                                  ("$cfCtl", 0, _N_CRCFIN * 0x10)],
                           vectors=[("$cfP%d" % i, "$cfSkip%d" % i) for i in range(_N_CRCFIN)]),
}
