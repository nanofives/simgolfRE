# C3 batch c3af (revisit, 2026-10-08) of golf_clean.exe. Functions offered in earlier rounds and still C2, left for
# time/scope rather than a hard reason. IJG libjpeg 6a helpers that write through a destination or memory manager
# reached via a function pointer (emit_byte/emit_2bytes/emit_marker, jpeg_alloc_quant_table/jpeg_alloc_huff_table,
# jpeg_stdio_dest, jinit_forward_dct) plus the live walker-spawn writer spawnWalker. Reimplemented in
# shim/src/re/c3af.cpp; fixtures in re/frida/js/fixtures.d/c3af.js; per-jcc branch coverage in log/c3/c3af_purpose.md.
#
# Cross-batch / same-batch callees:
#  - emit_marker and emit_2bytes call emit_byte (0x004afbb0), which IS in this batch. The verifier should re-run both
#    with SIMGOLF_HOOKS_OFF=004afbb0 so the original arm uses the original emit_byte.
#  - spawnWalker calls Random::range (0x0045c1e0), which is C3 in c3f and NOT in this batch; both A/B arms advance the
#    same restored RNG seed 0x00822d9c, so no SIMGOLF_HOOKS_OFF is needed (like golf_course heightBlend -> clamp).

# A byte value set and a 16-bit value set for the emit helpers.
_VALS = [0, 1, 2, 0x7f, 0x80, 0xa5, 0xfe, 0xff, 0x100, 0x1ff, 0x55, 0xcc]            # low byte is what lands
_VALS16 = [0x0000, 0x0001, 0x1234, 0x7fff, 0x8000, 0xabcd, 0xff00, 0x00ff, 0xffff, 0x5a5a, 0x1200, 0x00c3]
_MARKS = [0xc0, 0xc4, 0xd8, 0xd9, 0xda, 0xdb, 0xe0, 0xee, 0xfe, 0x01, 0x00, 0xff]     # JPEG marker codes

_DEST_STATE = [("$buf", 0, 0x20), ("$dest", 0, 8)]

HOOKS = {
    # emit_byte: leaf writer through cinfo->dest. Common path (free_in_buffer large, no callback); vary the byte.
    "emit_byte": dict(module="golf_clean.exe", addr=0x004afbb0, abi="default", ret="void",
                      args=["pointer", "int"], fixture="c3af_dest", state=_DEST_STATE,
                      vectors=[("$cinfo", v) for v in _VALS]),
    # emit_byte full-buffer branch: free_in_buffer == 1 so --free == 0 (0x004afbcc) calls empty_output_buffer, which
    # returns 1 (the ERREXIT side at 0x004afbd9 needs it to return 0, which libjpeg's sink never does: not covered).
    "emit_byte_full": dict(module="golf_clean.exe", addr=0x004afbb0, abi="default", ret="void",
                           args=["pointer", "int"], fixture="c3af_dest_full", state=_DEST_STATE,
                           vectors=[("$cinfo", v) for v in (0, 0x40, 0xa5, 0xff)]),
    # emit_marker: writes 0xFF then the marker (calls emit_byte x2). Run with SIMGOLF_HOOKS_OFF=004afbb0.
    "emit_marker": dict(module="golf_clean.exe", addr=0x004afbf0, abi="default", ret="void",
                        args=["pointer", "int"], fixture="c3af_dest", state=_DEST_STATE,
                        vectors=[("$cinfo", m) for m in _MARKS]),
    # emit_2bytes: writes the 16-bit value MSB first (calls emit_byte x2). Run with SIMGOLF_HOOKS_OFF=004afbb0.
    "emit_2bytes": dict(module="golf_clean.exe", addr=0x004afc10, abi="default", ret="void",
                        args=["pointer", "int"], fixture="c3af_dest", state=_DEST_STATE,
                        vectors=[("$cinfo", v) for v in _VALS16]),
    # jpeg_alloc_quant_table: alloc 0x84 then clear sent_table at +0x80. Leaf; ten cinfos -> ten distinct buffers.
    "jpeg_alloc_quant_table": dict(module="golf_clean.exe", addr=0x004afab0, abi="default", ret="pointer",
                                   args=["pointer"], fixture="c3af_quant",
                                   state=[(f"$buf{i}", 0, 0x84) for i in range(10)],
                                   vectors=[(f"$c{i}",) for i in range(10)]),
    # jpeg_alloc_huff_table: alloc 0x118 then clear sent_table at +0x114. Leaf; ten cinfos.
    "jpeg_alloc_huff_table": dict(module="golf_clean.exe", addr=0x004afad0, abi="default", ret="pointer",
                                  args=["pointer"], fixture="c3af_huff",
                                  state=[(f"$buf{i}", 0, 0x118) for i in range(10)],
                                  vectors=[(f"$c{i}",) for i in range(10)]),
    # jpeg_stdio_dest, dest != NULL: rewrite the three method pointers + the outfile at dest+0x14; vary outfile.
    "jpeg_stdio_dest": dict(module="golf_clean.exe", addr=0x004ae4d0, abi="default", ret="void",
                            args=["pointer", "pointer"], fixture="c3af_stdio_preset",
                            state=[("$dest", 0, 0x1c)],
                            vectors=[("$cinfo", f) for f in (0, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555,
                                                             0x6666, 0x7777, 0x8888, 0x9999)]),
    # jpeg_stdio_dest, dest == NULL: alloc branch (0x004ae4da taken), store the manager at cinfo+0x14.
    "jpeg_stdio_dest_alloc": dict(module="golf_clean.exe", addr=0x004ae4d0, abi="default", ret="void",
                                  args=["pointer", "pointer"], fixture="c3af_stdio_alloc",
                                  state=[("$cinfo", 0x14, 4), ("$buf", 0, 0x1c)],
                                  vectors=[("$cinfo", f) for f in (0, 0xaaaa, 0xbbbb, 0xcccc)]),
    # jinit_forward_dct: alloc 0x30, set start_pass, branch on the DCT method at cinfo+0xbc (0/1/2), zero the divisor
    # cache. Leaf; twelve cinfos cycling methods 0/1/2.
    "jinit_forward_dct": dict(module="golf_clean.exe", addr=0x004b34b0, abi="default", ret="void",
                              args=["pointer"], fixture="c3af_fdct",
                              state=[(f"$buf{i}", 0, 0x30) for i in range(12)] +
                                    [(f"$c{i}", 0x160, 4) for i in range(12)],
                              vectors=[(f"$c{i}",) for i in range(12)]),
    # spawnWalker: live writer. State = the walker table (slots 0..3), the RNG seed, and the two globals it reads.
    # slot0 fixture leaves slot 0 free (returns 0); occ fixture marks slots 0..2 taken (returns 3, exercises the
    # search loop 0x00402984). Vary the type byte written at rec+0x13.
    "spawnWalker": dict(module="golf_clean.exe", addr=0x00402970, abi="default", ret="int", args=["int"],
                        fixture="c3af_walker0",
                        state=[(0x00585850, 0, 0x130), (0x00822D9C, 0, 64),
                               (0x0058BCBA, 0, 4), (0x00575CB9, 0, 1)],
                        vectors=[(t,) for t in (0, 1, 2, 3, 7, 0xb, 0x10, 0x2a, 0x7f, 0xaa, 0xfe, 0xff)]),
    "spawnWalker_occ": dict(module="golf_clean.exe", addr=0x00402970, abi="default", ret="int", args=["int"],
                            fixture="c3af_walker3",
                            state=[(0x00585850, 0, 0x130), (0x00822D9C, 0, 64),
                                   (0x0058BCBA, 0, 4), (0x00575CB9, 0, 1)],
                            vectors=[(t,) for t in (0, 1, 2, 3, 7, 0xb, 0x10, 0x2a, 0x7f, 0xaa, 0xfe, 0xff)]),
}
