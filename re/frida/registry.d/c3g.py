"""c3g batch: ui text builders, window-corner sizing, widget-table copies, message-ticker start and the golfer-panel
hit test of golf_clean.exe. One registry entry per branch group; a global-gated function gets one entry per fixture
variant. diff_hook/re_classify key off the address, so a key is only a label (the CSV file name). shim/src/re/c3g.cpp."""

# startMessage (0x0040cb00) writers: the busy/direction/arg/length/sound/delay globals, the two RNG outputs
# (0x0056d1a8 contiguous with 0x0056d1ac), the RNG seed (0x00822d9c, so both arms draw the same numbers) and the
# 64-byte message copy at 0x005a6d40.
_MSG_STATE = [(0x0053DF54, 0, 4), (0x00569498, 0, 1), (0x005A34EC, 0, 4), (0x005A7144, 0, 4), (0x004C2E08, 0, 4),
              (0x005694A4, 0, 4), (0x0056D1A8, 0, 8), (0x00822D9C, 0, 4), (0x005A6D40, 0, 64)]
_MSG_VEC = [(100, -5, 7), (200, 0, 1), (300, 3, 2), (-1, -10, 0), (5, 1, -1), (0, -1, 9)]

# text buffer (0x0051a068) + the itoa scratch (0x0058a528); appendCents also stores into 0x00569628, appendDate
# into the year word 0x005a6d3c.
_TEXT = [(0x0051A068, 0, 256), (0x0058A528, 0, 32)]

HOOKS = {
    # appendNumber 0x0042dc00: n with thousands separators (recurses through its own address).
    "appendNumber": dict(module="golf_clean.exe", addr=0x0042DC00, abi="default", ret="void", args=["int"],
                         fixture="c3g_text_empty", state=_TEXT,
                         vectors=[(n,) for n in (0, 1, 9, 10, 99, 100, 101, 999, 1000, 1001, 1234, 10000, 12345,
                                                 999999, 1000000, -1, -99, -1000, -12345, 0x7FFFFFFF)]),
    # appendCents 0x0042dd50: amount in cents as "units.cc"; also stores the amount at 0x00569628.
    "appendCents": dict(module="golf_clean.exe", addr=0x0042DD50, abi="default", ret="void", args=["int"],
                        fixture="c3g_text_empty", state=_TEXT + [(0x00569628, 0, 4)],
                        vectors=[(n,) for n in (0, 1, 5, 9, 10, 99, 100, 101, 150, 999, 1000, 12345, 100000,
                                                -5, -9, -99, -100, -12345)]),
    # appendCents into a buffer ending in '+' (the negative branch rewrites the trailing '+' as '-').
    "appendCents_plus": dict(module="golf_clean.exe", addr=0x0042DD50, abi="default", ret="void", args=["int"],
                             fixture="c3g_text_plus", state=_TEXT + [(0x00569628, 0, 4)],
                             vectors=[(n,) for n in (-5, -100, -12345, 5, 100)]),
    # appendDate 0x0040d7b0: "<month> <year>" (8 months/year, year 0 = 2001); stores the year at 0x005a6d3c.
    "appendDate": dict(module="golf_clean.exe", addr=0x0040D7B0, abi="default", ret="void", args=["int"],
                       fixture="c3g_text_empty", state=_TEXT + [(0x005A6D3C, 0, 4)],
                       vectors=[(d,) for d in (0, 1024, 2048, 3072, 7168, 8192, 9216, 16384, 20480, 12345, 65536,
                                               100000)]),
    # appendHoleName 0x00407280: "Hole N" or a par-named string; appendString 0x0045b9f0 seeded to append nothing.
    "appendHoleName": dict(module="golf_clean.exe", addr=0x00407280, abi="default", ret="void", args=["int"],
                           fixture="c3g_holes", state=_TEXT, vectors=[(h,) for h in range(6)]),
    # appendCourseTitle 0x0040daa0: site name + a suffix chosen by bucketValue(holes-1) when full != -1. full is an
    # argument; two fixtures with different hole counts reach different bucketValue arms.
    "appendCourseTitle": dict(module="golf_clean.exe", addr=0x0040DAA0, abi="default", ret="void", args=["int"],
                              fixture="c3g_course_a", state=[(0x0051A068, 0, 256)],
                              vectors=[(-1,), (0,), (1,), (2,)]),
    "appendCourseTitle_b": dict(module="golf_clean.exe", addr=0x0040DAA0, abi="default", ret="void", args=["int"],
                                fixture="c3g_course_b", state=[(0x0051A068, 0, 256)],
                                vectors=[(-1,), (0,), (1,), (2,)]),
    # Window::calcSizeFromCorners 0x00481760: max() over four corner objects into +0x1a4/+0x1a8. Four window
    # objects (two with flag bit 0x10, two without) with different corner sizes; each window's +0x1a4 is a region.
    "Window_calcSizeFromCorners": dict(module="golf_clean.exe", addr=0x00481760, abi="thiscall", ret="void",
                                       args=["pointer"], fixture="c3g_calcsize",
                                       state=[("$o0", 0x1A4, 8), ("$o1", 0x1A4, 8), ("$o2", 0x1A4, 8),
                                              ("$o3", 0x1A4, 8)],
                                       vectors=[("$o0",), ("$o1",), ("$o2",), ("$o3",)]),
    # resetWidgetTable 0x00495eb0: reordered copy of the 0x0083fe78 template into the object (+4..+0x98). Two source
    # patterns so the copied bytes differ between the keys.
    "resetWidgetTable": dict(module="golf_clean.exe", addr=0x00495EB0, abi="thiscall", ret="void", args=["pointer"],
                             fixture="c3g_widget_a", state=[("$obj", 4, 0x98)], vectors=[("$obj",)]),
    "resetWidgetTable_b": dict(module="golf_clean.exe", addr=0x00495EB0, abi="thiscall", ret="void", args=["pointer"],
                               fixture="c3g_widget_b", state=[("$obj", 4, 0x98)], vectors=[("$obj",)]),
    # initWidgetTable 0x00495d30: constructor; vtable 0x004baa14 at +0 then the same copy (+0..+0x98). ret is this
    # (not compared); declared void.
    "initWidgetTable": dict(module="golf_clean.exe", addr=0x00495D30, abi="thiscall", ret="void", args=["pointer"],
                            fixture="c3g_widget_a", state=[("$obj", 0, 0x9C)], vectors=[("$obj",)]),
    "initWidgetTable_b": dict(module="golf_clean.exe", addr=0x00495D30, abi="thiscall", ret="void", args=["pointer"],
                              fixture="c3g_widget_b", state=[("$obj", 0, 0x9C)], vectors=[("$obj",)]),
    # startMessage 0x0040cb00: refused (0) while a message runs/pends with prio<=0, or in mode 3; otherwise 1 and
    # writes the ticker globals. Three fixtures: idle (proceeds), busy (direction byte 1), mode 3 (always refused).
    "startMessage": dict(module="golf_clean.exe", addr=0x0040CB00, abi="default", ret="int", args=["int", "int", "int"],
                         fixture="c3g_msg_idle", state=_MSG_STATE, vectors=_MSG_VEC),
    "startMessage_busy": dict(module="golf_clean.exe", addr=0x0040CB00, abi="default", ret="int",
                              args=["int", "int", "int"], fixture="c3g_msg_busy", state=_MSG_STATE, vectors=_MSG_VEC),
    "startMessage_mode3": dict(module="golf_clean.exe", addr=0x0040CB00, abi="default", ret="int",
                               args=["int", "int", "int"], fixture="c3g_msg_mode3", state=_MSG_STATE, vectors=_MSG_VEC),
    # hitGolferPanel435f00 0x00435f00: pure predicate, index of the panel hotspot under (x, y) or -1. mode0 (4..8
    # inactive) and mode3 (all 11 hotspots).
    "hitGolferPanel435f00": dict(module="golf_clean.exe", addr=0x00435F00, abi="default", ret="int",
                                 args=["int", "int"], fixture="c3g_panel_mode0",
                                 vectors=[(0x111, 0x22F), (0x139, 0x207), (0x139, 0x224), (0x139, 0x244),
                                          (0x11E, 0x1EA), (0xE7, 0x21C), (0x19E, 0x1F9), (0, 0), (0x400, 0x400)]),
    "hitGolferPanel435f00_mode3": dict(module="golf_clean.exe", addr=0x00435F00, abi="default", ret="int",
                                       args=["int", "int"], fixture="c3g_panel_mode3",
                                       vectors=[(0x111, 0x22F), (0x139, 0x207), (0x139, 0x224), (0x139, 0x244),
                                                (0x19E, 0x1F9), (0x1ED, 0x1F9), (0x23C, 0x1F9), (0x28B, 0x1F9),
                                                (0x2DA, 0x1F9), (0x11E, 0x1EA), (0xE7, 0x21C), (0, 0)]),
}
