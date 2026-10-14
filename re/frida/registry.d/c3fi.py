# C3 batch c3fi (round 34, 2026-10-13) of golf_clean.exe: rows round 33 offered and left, under 2 KB
# (log/c3/c3fi_candidates.tsv). Reimplementations: shim/src/re/c3fi.cpp. Fixtures: re/frida/js/fixtures.d/c3fi.js.
# Purposes and per-jcc tables: log/c3/c3fi_purpose.md. Notes, controls and drops: log/c3/c3fi_notes.txt.
#
# Every key freezes the game's first thread and stands every callee not under test in with a recording stub
# (re/frida/js/framemask.js FM.stand, A/B thread only): each call is logged with `this` and its arguments in $fmlog,
# which is compared state. A vector's first argument is a row getter `$rN` that rebuilds that row's objects and globals
# before diff_hook snapshots the state; further arguments are `$aN_K`.

EXE = "golf_clean.exe"
FM = [("$fmlog", 0, 0x4000)]


def _h(addr, fixture, vectors, state, args, ret="int", abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, vectors=vectors,
                state=FM + list(state))


def _rows(n):
    return [(f"$r{i}",) for i in range(n)]


def _argrows(n, nargs):
    return [(f"$r{i}",) + tuple(f"$a{i}_{k}" for k in range(1, nargs)) for i in range(n)]


HOOKS = {}

# unlinkSound 0x004843e0 (node): the node block and the two list globals 0x0083af64 / 0x0083af68.
HOOKS["unlinkSound"] = _h(0x004843E0, "c3fi_us", _rows(13), [("$obj", 0, 0x400), (0x0083AF64, 0, 8)], ["pointer"],
                          ret="uint32")

# Window::setSize 0x0047b4e0 (thiscall: w, h, keep, a4): the window block; the display global holds the guarded fixture.
HOOKS["Window::setSize"] = _h(0x0047B4E0, "c3fi_ss", _argrows(13, 5), [("$obj", 0, 0x400), (0x0083AD50, 0, 4)],
                              ["pointer", "int", "int", "int", "int"], ret="uint32", abi="thiscall")

# Cache482f60::dtor 0x004835f0 (thiscall on the virtual base = full + 0x20): the whole block (object, nodes, data).
HOOKS["Cache482f60::dtor"] = _h(0x004835F0, "c3fi_cd", _rows(13), [("$obj", 0, 0x1000)], ["pointer"], ret="uint32",
                                abi="thiscall")

# Dialog::viewDtor 0x00491500 (thiscall, this = block + 0x1fc): the whole block.
HOOKS["Dialog::viewDtor"] = _h(0x00491500, "c3fi_vd", _rows(10), [("$obj", 0, 0x1000)], ["pointer"], ret="uint32",
                               abi="thiscall")

# Dialog::dtor 0x00491710 (thiscall): the dialog block and [0x00839650].
HOOKS["Dialog::dtor"] = _h(0x00491710, "c3fi_dd", _rows(10), [("$obj", 0, 0x2400), (0x00839650, 0, 4)], ["pointer"],
                           ret="uint32", abi="thiscall")

# netPoll 0x0049b7b0 (thiscall): the session block (object, buffer, scripted queue), the interface global and the
# name record 0x0083ff9c the interface's slot +0x54 fills.
HOOKS["netPoll"] = _h(0x0049B7B0, "c3fi_np", _rows(17), [("$obj", 0, 0x1800), (0x008400B0, 0, 4), (0x0083FF9C, 0, 0x10)],
                      ["pointer"], ret="uint32", abi="thiscall")

# Flic::createStream 0x00482b90 (thiscall: src, frames): the stream block (stream, src, header, palettes, array block)
# and the palette global 0x0083ad0c.
HOOKS["Flic::createStream"] = _h(0x00482B90, "c3fi_fcs", _argrows(15, 3), [("$obj", 0, 0x1000), (0x0083AD0C, 0, 4)],
                                 ["pointer", "pointer", "uint32"], ret="uint32", abi="thiscall")

# placeHabitat 0x00405e30 (x, y, type): the scripted generator block, the tile type grid, the flag words, the habitat
# slots (from 0x00572ca8, eight bytes before slot 0, so a store through slot -1 is seen), the text buffer, the two flags.
HOOKS["placeHabitat"] = _h(0x00405E30, "c3fi_ph", _argrows(13, 3),
                           [("$obj", 0, 0x200), (0x005722E8, 0, 0x9C4), (0x0053CAF0, 0, 0x1388), (0x00572CA8, 0, 0xA10),
                            (0x0051A068, 0, 0x40), (0x005A5A00, 0, 4), (0x005A34E0, 0, 4)],
                           ["int", "int", "int"], ret="uint32")

# Reader::open 0x00487fb0 (thiscall: path, section): the reader block (reader, FILE, lines, argument strings).
HOOKS["Reader::open"] = _h(0x00487FB0, "c3fi_ro", _argrows(14, 3), [("$obj", 0, 0x1000)],
                           ["pointer", "pointer", "pointer"], ret="uint32", abi="thiscall")

# scrollbarCreate 0x00496330 (thiscall: x, y, w, h, parent, page, flags): the scrollbar block and [0x0083ff60].
HOOKS["scrollbarCreate"] = _h(0x00496330, "c3fi_sc", _argrows(18, 8), [("$obj", 0, 0x1400), (0x0083FF60, 0, 4)],
                              ["pointer", "int", "int", "int", "int", "pointer", "uint32", "uint32"], ret="uint32",
                              abi="thiscall")

# ListBox::build 0x0048a5d0 (thiscall: parent, x, y, style): the list box block (direct fields, V fields, vbtable).
HOOKS["ListBox::build"] = _h(0x0048A5D0, "c3fi_lb", _argrows(13, 5), [("$obj", 0, 0x1000)],
                             ["pointer", "pointer", "int", "int", "uint32"], ret="uint32", abi="thiscall")

# panelBuild4a3880 0x004a3880 (thiscall: parent, x, y, style): the panel block (fields, item ring, vbtable).
HOOKS["panelBuild4a3880"] = _h(0x004A3880, "c3fi_pb", _argrows(13, 5), [("$obj", 0, 0x1000)],
                               ["pointer", "pointer", "int", "int", "uint32"], ret="uint32", abi="thiscall")

# showMainRadialMenu 0x00432ba0 (cdecl: x, y): the text buffer, the blink / hover counters, the hover spot, the display.
HOOKS["showMainRadialMenu"] = _h(0x00432BA0, "c3fi_rm", _argrows(21, 2),
                                 [(0x0051A068, 0, 0x40), (0x005A9F5C, 0, 4), (0x005A9F68, 0, 4), (0x005AA6B8, 0, 4),
                                  (0x0083AD50, 0, 4)], ["int", "int"], ret="uint32")

# showLoadingScreen 0x00406250 (cdecl: percent): the text buffer (the frame objects are named by $fmlog's tokens).
HOOKS["showLoadingScreen"] = _h(0x00406250, "c3fi_ls", _rows(14), [(0x0051A068, 0, 0x40)], ["int"], ret="uint32")

# netSendShutdown 0x00497d10 (thiscall): the session block, the interface global, the KERNEL32 import log.
HOOKS["netSendShutdown"] = _h(0x00497D10, "c3fi_sd", _rows(10),
                              [("$obj", 0, 0x1000), (0x008400B0, 0, 4), ("$implog", 0, 0x1000)], ["pointer"],
                              ret="uint32", abi="thiscall")
