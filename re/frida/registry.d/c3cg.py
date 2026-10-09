# C3 batch c3cg (round 20, 2026-10-09) of golf_clean.exe: nine IJG libjpeg 6a method pointers plus two vtable
# methods. Reimplementations in shim/src/re/c3cg.cpp, fixtures in re/frida/js/fixtures.d/c3cg.js, Purposes and
# per-jcc branch coverage in log/c3/c3cg_purpose.md.
#
# Hooked callees (the verifier should re-run these keys with SIMGOLF_HOOKS_OFF so the original arm uses
# original callees; none of them is in this batch):
#   write_any_marker     -> 004afbf0 emit_marker, 004afc10 emit_2bytes, 004afbb0 emit_byte (C3, c3af)
#   write_file_header    -> 004afbf0 emit_marker (C3, c3af), 004afc80 emit_jfif_app0 and
#                           004afd20 emit_adobe_app14 (C3, c3be)
#   write_frame_header   -> 004afec0 emit_dqt and 004affa0 emit_sof (C3, c3aa), which call
#                           004afbb0 / 004afbf0 / 004afc10 (C3, c3af)
#   write_tables_only    -> 004afbf0 emit_marker (C3, c3af), 004afec0 emit_dqt and 004b0120 emit_dht (C3, c3aa)
#   fullsize_downsample  -> 004b04f0 jcopy_sample_rows and 004b4390 expand_right_edge (C3, c3w)
#   request_virt_sarray / request_virt_barray -> 004aef70 alloc_small (C2, hooked by no batch)
# rgb_gray_convert, grayscale_convert, Window::hitTestBorder and Snd::flags call nothing that is hooked.
# The pointers the fixtures install (error_exit at err+0, emit_message at err+4, empty_output_buffer at
# dest+0xc and the sound sub-object's vtable slot +0x70) are the fixture's own callbacks, not hooks.

# ---- write_any_marker ------------------------------------------------------------------------------------
_WAM_STATE = [("$c", 0, 0x180), ("$err", 0, 0x84), ("$dest", 0, 0x20), ("$buf", 0, 0x400), ("$rec", 0, 0x40)]

# The payload blocks $p0..$p6 are 1, 2, 5, 16, 64, 200 and 256 bytes long.
_WAM_VECTORS = [
    ("$c", 0xE0, "$p0", 0),          # datalen 0: marker and length only (the je at 0x004afb8f)
    ("$c", 0xE1, "$p0", 1),
    ("$c", 0xE2, "$p1", 2),
    ("$c", 0xE3, "$p2", 5),
    ("$c", 0xFE, "$p3", 16),
    ("$c", 0xD8, "$p4", 64),
    ("$c", 0x01, "$p5", 200),
    ("$c", 0xEE, "$p6", 256),
    ("$c", 0xE0, "$p0", 0xFFFE),     # above 0xfffd: the ja at 0x004afb6c, nothing is emitted
    ("$c", 0xE0, "$p0", 0xFFFF),
    ("$c", 0xE0, "$p0", 0x10000),
    ("$c", 0xE0, "$p0", 0xFFFFFFFF),
]

# ---- write_file_header / write_frame_header / write_tables_only ------------------------------------------
_SHARED_DEST = [("$err", 0, 0x84), ("$dest", 0, 0x20), ("$buf", 0, 0x400), ("$rec", 0, 0x40)]


def _cinfos(n):
    return [(f"$c{i}", 0, 0x180) for i in range(n)]


# write_tables_only: which quant / DC / AC slots each of the eight compress objects installs (the same table
# the fixture uses, so the state lists exactly the allocated blocks).
_WTO_ROWS = [
    ([0, 1, 2, 3], [0, 1, 2, 3], [0, 1, 2, 3]),
    ([], [], []),
    ([0, 2], [1], [3]),
    ([0, 1, 2, 3], [0, 1, 2, 3], [0, 1, 2, 3]),
    ([3], [0, 1, 2, 3], []),
    ([], [], [0, 1, 2, 3]),
    ([1], [2], [2]),
    ([], [], []),
]
_WTO_TABLES = ([(f"$q{i}_{s}", 0, 0x84) for i, r in enumerate(_WTO_ROWS) for s in r[0]]
               + [(f"$d{i}_{s}", 0, 0x118) for i, r in enumerate(_WTO_ROWS) for s in r[1]]
               + [(f"$a{i}_{s}", 0, 0x118) for i, r in enumerate(_WTO_ROWS) for s in r[2]])

# write_frame_header: every compress object owns all four quantisation tables (emit_dqt writes sent_table).
_WFRH_TABLES = [(f"$q{i}_{s}", 0, 0x84) for i in range(12) for s in range(4)]

# ---- fullsize_downsample ---------------------------------------------------------------------------------
_FDS_STATE = [(f"$ob{i}", 0, 8 * 0x80) for i in range(10)]
_FDS_VECTORS = [(f"$c{i}", f"$k{i}", f"$i{i}", f"$o{i}") for i in range(10)]

# ---- the two colour converters ---------------------------------------------------------------------------
_CONV_STATE = [("$outBuf", 0, 8 * 0x40)]
# (cinfo, input_buf, output_buf, output_row, num_rows). $c0..$c3 have image_width 0, 1, 5 and 8.
_CONV_VECTORS = [
    ("$c3", "$inRows", "$image", 0, 1),
    ("$c3", "$inRows", "$image", 0, 2),
    ("$c3", "$inRows", "$image", 2, 3),
    ("$c3", "$inRows", "$image", 5, 1),
    ("$c3", "$inRows", "$image", 0, 0),       # the js at 0x004b4ecb / 0x004b50e5: nothing is written
    ("$c3", "$inRows", "$image", 0, -1),
    ("$c3", "$inRows", "$image", 0, -7),
    ("$c2", "$inRows", "$image", 0, 1),
    ("$c2", "$inRows", "$image", 1, 4),
    ("$c2", "$inRows", "$image", 4, 2),
    ("$c1", "$inRows", "$image", 0, 3),
    ("$c1", "$inRows", "$image", 7, 1),
    ("$c0", "$inRows", "$image", 0, 4),       # image_width 0: the jbe at 0x004b4f0a / 0x004b5114
    ("$c0", "$inRows", "$image", 3, 1),
    ("$c3", "$inRows", "$image", 0, 8),
]

# ---- request_virt_sarray / request_virt_barray -----------------------------------------------------------
_RVA_STATE = [("$c", 0, 0x180), ("$err", 0, 0x84), ("$mem", 0, 0x60),
              ("$pool0", 0, 0x400), ("$pool1", 0, 0x400)]
# (cinfo, pool_id, pre_zero, samplesperrow/blocksperrow, numrows, maxaccess)
_RVA_VECTORS = [
    ("$c", 1, 0, 8, 16, 2),
    ("$c", 1, 1, 100, 200, 4),
    ("$c", 1, 0, 0, 0, 0),
    ("$c", 1, 1, 0xFFFFFFFF, 0x7FFFFFFF, 1),
    ("$c", 1, -5, 37, 41, 37),
    ("$c", 1, 0, 1, 1, 1),
    ("$c", 1, 2, 0x1000, 0x10, 0x10),
    ("$c", 0, 0, 8, 16, 2),                   # pool_id != 1: the ERREXIT at 0x004af2e1 / 0x004af351
    ("$c", 0, 1, 64, 64, 8),
    ("$c", 0, 0, 0xDEADBEEF, 3, 9),
]

# ---- Window::hitTestBorder -------------------------------------------------------------------------------
# $w0 resizable, width 8, rect (0,0,100,60) -> far edges 92 and 52, flag dword 0.
# $w1 resizable, width 4, rect (10,20,110,80) -> far edges 96 and 56, flag dword 1.
# $w2 not resizable, flag dword 0x10, limit +0x180 = 30.  $w3 not resizable, flag dword 0x4000000, +0x1ec = 25.
# $w4 not resizable, flag dword 0.  $w5 resizable, width 0, rect (0,0,10,10), flag dword 0x4000000, +0x1ec = 5.
# $w6 resizable, width 50, rect (0,0,20,20) (both far edges negative), flag dword 0x11.
_HIT_VECTORS = [
    ("$w0", 0, 0), ("$w0", 0, 55), ("$w0", 95, 0), ("$w0", 95, 55),
    ("$w0", 3, 30), ("$w0", 50, 2), ("$w0", 95, 30), ("$w0", 50, 55),
    ("$w0", 50, 30), ("$w0", 8, 8), ("$w0", 92, 52), ("$w0", 93, 52), ("$w0", 92, 53),
    ("$w0", -0x80000000, 0x7FFFFFFF), ("$w0", 0x7FFFFFFF, -0x80000000),
    ("$w1", 0, 0), ("$w1", 3, 57), ("$w1", 97, 1), ("$w1", 97, 57), ("$w1", 50, 30),
    ("$w2", 0, 0), ("$w2", 0, 40), ("$w2", -1, 29),
    ("$w3", 5, 10), ("$w3", 5, 30), ("$w3", 5, 25),
    ("$w4", 1, 1), ("$w4", -5, -5),
    ("$w5", 0, 0), ("$w5", -1, -1), ("$w5", 11, 11), ("$w5", 11, 0), ("$w5", 0, 11),
    ("$w6", 0, 0), ("$w6", 100, 100),
]

# ---- setFocusItem ----------------------------------------------------------------------------------------
# The eight synthetic views, their 32 list nodes (the _cb_item fixture clears a node's item from the
# notification callback), the global 0x0083ab98 and the notification recorder.
_FOCUS_STATE = ([(f"$v{i}", 0, 0x180) for i in range(8)]
                + [(f"$nd{i}_{k}", 0, 0x20) for i in range(8) for k in range(4)]
                + [(0x0083AB98, 0, 4), ("$rec", 0, 0x40)])
_FOCUS_VECTORS = [
    ("$v0", 0),            # null item: the je at 0x0047ae8c
    ("$v0", "$it0_0"),     # already focused: the je at 0x0047aeb9
    ("$v0", "$it0_1"),     # found at index 1, both notifications
    ("$v0", "$it0_2"),     # found at index 2
    ("$v0", "$it0_3"),     # found at index 3
    ("$v0", "$it1_0"),     # an item of another view: the walk runs out (0x0047af13)
    ("$v1", "$it1_0"),     # found at index 0 with the cursor starting at node 2
    ("$v1", "$it1_2"),     # already focused
    ("$v1", "$it1_3"),
    ("$v2", "$it2_0"),     # child count 1: the jle at 0x0047af27 skips the lose-focus call
    ("$v3", "$it3_1"),     # the cursor node's item is null, so there is no previous item (0x0047af1e)
    ("$v4", "$it4_0"),     # child count 0: the je at 0x0047ae9a
    ("$v5", "$it5_1"),     # child count -1: passes 0x0047ae9a, stops at 0x0047aeed
    ("$v6", "$it6_0"),     # null list head: the je at 0x0047aecd
    ("$v7", "$it7_0"),     # child count 2, found at index 0
    ("$v7", "$it7_2"),     # child count 2, the walk runs out
]
# The six vectors that actually reach the lose-focus notification, used by the two callback fixtures.
_FOCUS_CB_VECTORS = [("$v0", "$it0_1"), ("$v0", "$it0_2"), ("$v0", "$it0_3"),
                     ("$v1", "$it1_0"), ("$v1", "$it1_3"), ("$v7", "$it7_0")]

HOOKS = {
    # write_any_marker 0x004afb60: both sides of the unsigned length test at 0x004afb6c and both sides of the
    # zero-length test at 0x004afb8f, with payloads of 1 to 256 bytes. The roomy key keeps free_in_buffer at
    # 0x400 so emit_byte 0x004afbb0 never refills; the _full key leaves three free bytes, so the destination
    # manager's empty_output_buffer runs many times per vector and the refills are counted in the recorder.
    # 12 + 8 vectors.
    "write_any_marker": dict(module="golf_clean.exe", addr=0x004AFB60, abi="default", ret="void",
                             args=["pointer", "int", "pointer", "uint32"], fixture="c3cg_wam",
                             state=_WAM_STATE, vectors=_WAM_VECTORS),
    "write_any_marker_full": dict(module="golf_clean.exe", addr=0x004AFB60, abi="default", ret="void",
                                  args=["pointer", "int", "pointer", "uint32"], fixture="c3cg_wam_full",
                                  state=_WAM_STATE, vectors=_WAM_VECTORS[:8]),

    # write_file_header 0x004afc40: all four combinations of write_JFIF_header (cinfo+0xc8, the je at
    # 0x004afc5b) and write_Adobe_marker (cinfo+0xd4, the je at 0x004afc6e) over twelve compress objects whose
    # density fields and colour space differ, so the emitted bytes differ per vector. 12 vectors.
    "write_file_header": dict(module="golf_clean.exe", addr=0x004AFC40, abi="default", ret="void",
                              args=["pointer"], fixture="c3cg_wfh",
                              state=_cinfos(12) + _SHARED_DEST,
                              vectors=[(f"$c{i}",) for i in range(12)]),

    # write_frame_header 0x004afdc0: num_components 0 to 4 (both sides of 0x004afdd4 and 0x004afe1b), 8-bit and
    # 16-bit quantisation tables (both sides of the prec test at 0x004afe33 and the TRACEMS at 0x004afe3f),
    # dc_tbl_no / ac_tbl_no at and above the limit 1 (both sides of 0x004afe23 and 0x004afe27), a
    # data_precision other than 8 (0x004afe0a) and every one of the four markers 0xc9, 0xc2, 0xc0 and 0xc1.
    # 12 vectors.
    "write_frame_header": dict(module="golf_clean.exe", addr=0x004AFDC0, abi="default", ret="void",
                               args=["pointer"], fixture="c3cg_wfrh",
                               state=_cinfos(12) + _WFRH_TABLES + _SHARED_DEST,
                               vectors=[(f"$c{i}",) for i in range(12)]),

    # write_tables_only 0x004b0320: both sides of the arith_code test at 0x004b035a and of all three table-slot
    # tests (0x004b033d, 0x004b0366, 0x004b0377), from "every slot filled" to "none at all". The emitted bytes
    # and every table's sent_table flag are compared. 8 vectors.
    "write_tables_only": dict(module="golf_clean.exe", addr=0x004B0320, abi="default", ret="void",
                              args=["pointer"], fixture="c3cg_wto",
                              state=_cinfos(8) + _WTO_TABLES + _SHARED_DEST,
                              vectors=[(f"$c{i}",) for i in range(8)]),

    # fullsize_downsample 0x004b43f0: ten (max_v_samp_factor, image_width, width_in_blocks) sets. The padded
    # width width_in_blocks * 8 is below, equal to and above image_width, so expand_right_edge 0x004b4390 pads
    # nothing, one column and many; the rows beyond max_v_samp_factor keep their fill, which is part of the
    # compared state. The function has no branch of its own. 10 vectors.
    "fullsize_downsample": dict(module="golf_clean.exe", addr=0x004B43F0, abi="default", ret="void",
                                args=["pointer", "pointer", "pointer", "pointer"], fixture="c3cg_fds",
                                state=_FDS_STATE, vectors=_FDS_VECTORS),

    # rgb_gray_convert 0x004b4eb0: both sides of the row test at 0x004b4ecb (num_rows 0, -1 and -7 write
    # nothing) and of the column test at 0x004b4f0a (image_width 0), with image_width 1, 5 and 8, num_rows 1 to
    # 8 and output_row 0 to 7, so every written row is at a different place in the output component. Leaf,
    # 15 vectors.
    "rgb_gray_convert": dict(module="golf_clean.exe", addr=0x004B4EB0, abi="default", ret="void",
                             args=["pointer", "pointer", "pointer", "uint32", "int"], fixture="c3cg_rgb",
                             state=_CONV_STATE, vectors=_CONV_VECTORS),

    # grayscale_convert 0x004b50d0: the same grid against the four input strides 1, 2, 3 and 4 that $c0..$c3
    # carry in input_components (cinfo+0x20), with both sides of 0x004b50e5 and 0x004b5114. Leaf, 15 vectors.
    "grayscale_convert": dict(module="golf_clean.exe", addr=0x004B50D0, abi="default", ret="void",
                              args=["pointer", "pointer", "pointer", "uint32", "int"], fixture="c3cg_gray",
                              state=_CONV_STATE, vectors=_CONV_VECTORS),

    # request_virt_sarray 0x004af2d0: seven vectors on pool_id 1 (the je at 0x004af2e1 taken, no message) and
    # three on pool_id 0 (not taken: message code 0xc with the pool id, recorded by the returning error_exit,
    # and the block then carved out of small_list[0]). The control block the function fills, the pool it came
    # from and mem->virt_sarray_list are all compared, and the returned pointer is the block. 10 vectors.
    "request_virt_sarray": dict(module="golf_clean.exe", addr=0x004AF2D0, abi="default", ret="pointer",
                                args=["pointer", "int", "int", "uint32", "uint32", "uint32"],
                                fixture="c3cg_rva", state=_RVA_STATE, vectors=_RVA_VECTORS),

    # request_virt_barray 0x004af340: the same against mem->virt_barray_list (mem+0x44). 10 vectors.
    "request_virt_barray": dict(module="golf_clean.exe", addr=0x004AF340, abi="default", ret="pointer",
                                args=["pointer", "int", "int", "uint32", "uint32", "uint32"],
                                fixture="c3cg_rva", state=_RVA_STATE, vectors=_RVA_VECTORS),

    # Window::hitTestBorder 0x0047ed30: all ten results (0, 2, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11) over
    # seven synthetic windows, including the exact far-edge coordinates (the jle at 0x0047ed97, 0x0047eded and
    # 0x0047ee0c keep a point ON the edge inside, the jge at 0x0047ed51 and 0x0047edcf keep a point AT the
    # border width inside), the INT_MIN / INT_MAX pair an unsigned compare would classify the other way, a
    # window with border width 0, one whose far edges are negative, and all three flag-dword arms of the second
    # half (0x0047ee21, 0x0047ee30, 0x0047ee4a). Leaf, 35 vectors, no state (the body writes nothing).
    "Window_hitTestBorder": dict(module="golf_clean.exe", addr=0x0047ED30, abi="thiscall", ret="int",
                                 args=["pointer", "int", "int"], fixture="c3cg_hit", vectors=_HIT_VECTORS),

    # Snd::flags 0x00484ff0 is NOT a c3cg function: shim/src/re/c3f.cpp has hooked it since round 2 and its
    # key `Snd::flags` in re/frida/registry.d/c3f.py is GREEN over 16 vectors
    # (log/diff/00484ff0_Snd_flags.path1.csv, 2026-10-06). That fixture leaves the sub-object pointer at +0x40
    # NULL in every vector, so the virtual call at 0x00484ffe has never run in an A/B. This key adds that arm:
    # four of its fifteen objects carry a sub-object built on our own vtable whose slot +0x70 is a thiscall
    # callback returning a value with bits above the low word (which is how the run shows that the ORs at
    # 0x00485008 onwards touch only AL and AH), plus a flag dword whose high bytes are set and whose low byte
    # is zero. It runs against C3F'S body, not one of ours.
    # VERIFIER: promote 0x00484ff0 through c3f (`--file shim/src/re/c3f.cpp`), NOT through c3cg. The row is
    # held at C2 only by the anti-island rule (`callers none`), which round 20's stored-pointer installers
    # (Snd::ctorDerived 0x00484820, SndMusic::dtor 0x004848a0) now satisfy.
    "Snd_flags_device": dict(module="golf_clean.exe", addr=0x00484FF0, abi="thiscall", ret="int",
                             args=["pointer"], fixture="c3cg_snd", state=[("$rec", 0, 0x40)],
                             vectors=[(f"$o{i}",) for i in range(15)]),

    # setFocusItem 0x0047ae80: every guard (null item, zero child count, already-focused item, null list head,
    # negative child count), the walk finding the item at index 0, 1, 2 and 3 and running out, and both sides
    # of the two conditions that gate the lose-focus notification (no previous item at 0x0047af1e, a child
    # count of exactly 1 at 0x0047af27). Leaf, 16 vectors. Compared state: the eight views, the 32 list nodes,
    # the global 0x0083ab98 and the notification recorder.
    "setFocusItem": dict(module="golf_clean.exe", addr=0x0047AE80, abi="thiscall", ret="void",
                         args=["pointer", "pointer"], fixture="c3cg_focus",
                         state=_FOCUS_STATE, vectors=_FOCUS_VECTORS),

    # The two re-reads after the lose-focus notification (0x0047af3b on the list head, 0x0047af48 on the
    # cursor node's item) can only fail if that notification changes the list, so each key gives the callback
    # one of those two side effects: _cb_head clears the view's +0x138, _cb_item clears the cursor node's +4.
    # 6 vectors each (the ones that reach the notification).
    "setFocusItem_cb_head": dict(module="golf_clean.exe", addr=0x0047AE80, abi="thiscall", ret="void",
                                 args=["pointer", "pointer"], fixture="c3cg_focus_cb_head",
                                 state=_FOCUS_STATE, vectors=_FOCUS_CB_VECTORS),
    "setFocusItem_cb_item": dict(module="golf_clean.exe", addr=0x0047AE80, abi="thiscall", ret="void",
                                 args=["pointer", "pointer"], fixture="c3cg_focus_cb_item",
                                 state=_FOCUS_STATE, vectors=_FOCUS_CB_VECTORS),
}
